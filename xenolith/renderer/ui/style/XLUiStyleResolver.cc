/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#include "XLUiStyleResolver.h"
#include "XLFocusWithin.h"
#include "XLSelection.h"
#include "XLUiStyleSystem.h"
#include "XLUiLayoutSystem.h"
#include "XLUiScrollSystem.h"
#include "XLInteractiveComponent.h"
#include "XLInheritedStyle.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

ComponentId StyleManagedLayout::Id;
ComponentId SystemManagedLayout::Id;
ComponentId StyleManagedScroll::Id;

struct ApplierRegistryNode {
	StyleResolver::AttrApplier applier;
	StyleResolver::ParameterMask mask;
};

// All recursive StyleResolvers share one frame-stack tag, so a descendant's stack lookup resolves
// to the nearest ancestor recursive resolver (independent of the LayoutSystem tag)
uint64_t StyleResolver::SystemFrameTag = System::GetNextSystemId();

// Process-global type x attribute applier registry. Function-local static avoids static-init-order
// issues; widgets register from their own TU at startup. mem_std-backed (persists, no pool needed)
static HashMap<String, ApplierRegistryNode> &getTypeApplierRegistry() {
	static HashMap<String, ApplierRegistryNode> registry;
	return registry;
}

void StyleResolver::registerTypeApplier(StringView type, AttrApplier &&applier,
		sprt::bitset<toInt(document::ParameterName::Max)> &&params) {
	auto &reg = getTypeApplierRegistry();
	auto typeStr = type.str<Interface>();
	auto it = reg.find(typeStr);
	if (it == reg.end()) {
		reg.emplace(sp::move(typeStr),
				ApplierRegistryNode{
					sp::move(applier),
					sp::move(params),
				});
	} else {
		it->second = ApplierRegistryNode{
			sp::move(applier),
			sp::move(params),
		};
	}
}

StyleResolver::ParameterMask StyleResolver::makeParameterMask(
		sprt::initializer_list<document::ParameterName> &&params) {
	ParameterMask ret;
	for (auto &it : params) { ret.set(toInt(it)); }
	return ret;
}

namespace {

struct StyleScope {
	Node *owner = nullptr;
	StyleSystem *system = nullptr;
	size_t chainIndex = 0; // index in the node..root chain
	SpanView<bool> media;
};

// Bloom bits of a node's identity tokens; must use the same kinds (tag=0/class=1/id=2)
// as document::StyleContainer::addComplexSelector so the parse-side and match-side sets align
static uint64_t foldIdentityBits(const NodeIdentity *identity) {
	uint64_t bits = 0;
	if (identity) {
		if (!identity->type.empty()) {
			bits |= document::StyleContainer::selectorTokenBit(0, identity->type);
		}
		for (auto &cl : identity->classes) {
			bits |= document::StyleContainer::selectorTokenBit(1, cl);
		}
		if (!identity->name.empty()) {
			bits |= document::StyleContainer::selectorTokenBit(2, identity->name);
		}
	}
	return bits;
}

} // namespace

/* Custom properties in effect for one node; lives in the ResolvedStyle's pool. `vars` maps a name
to raw text (views into the declaring sheet or StyleVariables). `strings` copies the nearest
sheet's table and appends strings from substitution; DocumentData::addString never dedupes, so
using it would grow the sheet on every resolve. */
struct ResolvedStyle::VariableTable : memory::AllocPool {
	memory::PoolInterface::MapType<StringView, StringView> vars;
	memory::PoolInterface::VectorType<StringView> strings;
};

void ResolvedStyle::initVariables(SpanView<StringView> sheetStrings) {
	_variables = new (_pool) VariableTable();
	_variables->strings.assign(sheetStrings.begin(), sheetStrings.end());
}

document::StringId ResolvedStyle::internSubstitutedString(StringView str) {
	_variables->strings.emplace_back(str.pdup(_pool));
	return document::StringId(_variables->strings.size() - 1);
}

StringView ResolvedStyle::getCustomProperty(StringView name) const {
	if (!_variables) {
		return StringView();
	}
	auto it = _variables->vars.find(name);
	return (it == _variables->vars.end()) ? StringView() : it->second;
}

void ResolvedStyle::foreachCustomProperty(const Callback<void(StringView, StringView)> &cb) const {
	if (_variables) {
		for (auto &it : _variables->vars) { cb(it.first, it.second); }
	}
}

uint64_t ResolvedStyle::getCustomPropertiesHash() const {
	if (!_variables || _variables->vars.empty()) {
		return 0;
	}
	// FNV-1a over the whole set; the map is ordered, so the digest is stable
	uint64_t h = 1'469'598'103'934'665'603ull;
	auto fold = [&](StringView s) {
		for (size_t i = 0; i < s.size(); ++i) {
			h ^= static_cast<unsigned char>(s[i]);
			h *= 1'099'511'628'211ull;
		}
		h ^= 0xffu; // separator, so "ab"+"c" and "a"+"bc" differ
		h *= 1'099'511'628'211ull;
	};
	for (auto &it : _variables->vars) {
		fold(it.first);
		fold(it.second);
	}
	return h ? h : 1; // 0 is reserved for "no custom properties"
}

// Expand the deferred `var()` declarations of one matched rule into `dst` at the rule's cascade
// position. A failed expansion (undefined variable without fallback, or a cycle) is dropped.
void ResolvedStyle::expandPendingRule(document::StyleList &dst,
		const document::StyleContainer::MatchedRule &rule) {
	for (auto &p : rule.style->pending) {
		if (p.mediaQuery != document::MediaQueryIdNone && !rule.media.at(p.mediaQuery.get())) {
			continue;
		}

		auto name = rule.strings.at(p.nameText);
		mem_pool::String expanded;
		if (!document::expandCssVariables(rule.strings.at(p.rawValue), [&](StringView var) {
			return getCustomProperty(var);
		}, [&](StringView chunk) { expanded.append(chunk.data(), chunk.size()); })) {
			log::source().verbose("ui::StyleResolver", "Unresolved var() in '", name,
					"', declaration dropped");
			continue;
		}

		document::StyleContainer::readCssParameter(name, expanded,
				[&](document::StyleParameter &&param) {
			param.rule = p.rule;
			param.mediaQuery = document::MediaQueryIdNone;
			dst.set(param, true);
			return true;
		}, [&](const StringView &str) -> document::StringId {
			return internSubstitutedString(str);
		});
	}
}

/* Match cache: the rules matching one chain level depend only on that node and its ancestors, so
they are cached per node and shared by every descendant's resolve. An entry is valid while its
stamp matches the chain stamp (per-node unique id, components and child-list versions folded up
to the root), so reused addresses and sheet reloads never read a stale entry. The map is cleared
wholesale past MatchCacheLimit. */
namespace {

struct MatchCacheEntry {
	uint64_t stamp = 0;
	Vector<document::StyleContainer::MatchedRule> matches;
};

// pointer keys share alignment bits, hence the spreading hasher
using MatchCacheMap = sprt::__malloc_unordered_map<const Node *, MatchCacheEntry,
		sprt::hash_spread<>, sprt::equal_to<void>>;

// thread_local: each app thread has its own scene graph
static MatchCacheMap &getMatchCache() {
	static thread_local MatchCacheMap tl_cache;
	return tl_cache;
}

constexpr size_t MatchCacheLimit = 16'384;

// one level's three versions folded into the running chain stamp (odd multipliers, each version in
// its own round, so swapping two of them or moving one up the chain changes the result)
static inline uint64_t foldStyleMatchStamp(uint64_t acc, const Node *node) {
	acc = (acc ^ node->getStyleMatchId()) * 0x9E37'79B9'7F4A'7C15ull;
	acc = (acc ^ node->getComponentsVersion()) * 0xC2B2'AE3D'27D4'EB4Full;
	acc = (acc ^ node->getChildrenStyleVersion()) * 0x1656'67B1'9E37'79F9ull;
	return acc;
}

} // namespace

void StyleResolver::dropMatchCache() { getMatchCache().clear(); }

ResolvedStyle StyleResolver::resolveStyleForNode(NotNull<Node> node) {
	ResolvedStyle ret;
#if XL_FRAME_ACCOUNT
	auto &account = getVisitAccount();
	const auto chainStart = core::getAccountClock();
#endif

	// ancestor chain, node first
	Vector<Node *> chain;
	for (Node *p = node.get(); p != nullptr; p = p->getParent()) { chain.emplace_back(p); }

	// ancestor Bloom prefix: ancestorBitsFrom[i] = OR of identity tokens over chain[i..root].
	// A rule targeting chain[L] tests its ancestors chain[L+1..], i.e. ancestorBitsFrom[L+1].
	// CSS match stamp prefix, same shape and the same walk: stampFrom[i] folds chain[i] and every
	// ancestor above it, which is everything a match at level i can depend on (see the match cache)
	Vector<uint64_t> ancestorBitsFrom;
	Vector<uint64_t> stampFrom;
	ancestorBitsFrom.resize(chain.size() + 1, 0);
	stampFrom.resize(chain.size() + 1, 0);
	for (size_t i = chain.size(); i-- > 0;) {
		ancestorBitsFrom[i] =
				ancestorBitsFrom[i + 1] | foldIdentityBits(chain[i]->getComponent<NodeIdentity>());
		stampFrom[i] = foldStyleMatchStamp(stampFrom[i + 1], chain[i]);
	}

	// stylesheet scopes on the chain, nearest first
	Vector<StyleScope> scopes;
	for (size_t i = 0; i < chain.size(); ++i) {
		if (chain[i]->getComponent<StyleSystemState>()) {
			if (auto sys = chain[i]->getSystemByType<StyleSystem>()) {
				if (sys->getStyleSheet()) {
					scopes.emplace_back(StyleScope{chain[i], sys, i, sys->getMediaResolved()});
				}
			}
		}
	}

	if (scopes.empty()) {
		return ret;
	}

	auto &nearest = scopes.front();

	bool anyCustom = false;
	for (auto &scope : scopes) {
		auto sheet = scope.system->getStyleSheet();
		ret._structural = ret._structural || sheet->hasStructuralSelectors();
		anyCustom = anyCustom || sheet->hasCustomProperties();
	}

	// node-local StyleVariables need the variable table even when no sheet declares any; check
	// the same levels pass 1 visits
	if (!anyCustom) {
		for (size_t i = 0; i <= scopes.back().chainIndex; ++i) {
			if (chain[i]->getComponent<StyleVariables>()) {
				anyCustom = true;
				break;
			}
		}
	}

	// gather rules matching `levelNode` from every scope visible at `chainIndex`, sorted in
	// cascade order (specificity, then scope rank + source order)
	auto gatherLevel = [&](Vector<document::StyleContainer::MatchedRule> &matches, Node *levelNode,
							   size_t chainIndex) {
		uint64_t rank = 0; // outer sheets get a lower rank -> lose ties to nearer sheets
		for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
			if (it->chainIndex >= chainIndex) {
				// `:root` is scope-relative: for each sheet it means the node that owns it
				it->system->getStyleSheet()->collectMatches(matches, levelNode,
						ancestorBitsFrom[chainIndex + 1], rank << 32, it->media, it->owner);
				++rank;
			}
		}
		document::StyleContainer::sortMatchedRules(matches);
#if XL_FRAME_ACCOUNT
		++getVisitAccount().styleLevels;
#endif
	};

	// build only the raw merged parameter list plus context; the pool is owned by `ret`
	ret._pool = memory::pool::create(static_cast<memory::pool_t *>(nullptr));
	ret._media = &nearest.system->getMediaParameters();
	auto nearestStrings = nearest.system->getStyleSheet()->getStrings();

	memory::perform([&] {
		auto style = new (ret._pool) document::StyleList();

		// Gather each level's sorted matches once for both passes. Fill the cache for every level
		// first, then take pointers: an insert may rehash the map.
		const size_t outerLevel = scopes.back().chainIndex;
		auto &cache = getMatchCache();
		if (cache.size() > MatchCacheLimit) {
			cache.clear();
		}
		for (size_t i = outerLevel + 1; i-- > 0;) {
			auto it = cache.find(chain[i]);
			if (it != cache.end() && it->second.stamp == stampFrom[i]) {
#if XL_FRAME_ACCOUNT
				++account.styleLevelHits;
#endif
				continue;
			}
			if (it == cache.end()) {
				it = cache.emplace(chain[i], MatchCacheEntry()).first;
			}
			it->second.matches.clear();
			it->second.stamp = stampFrom[i];
			gatherLevel(it->second.matches, chain[i], i);
		}

		Vector<const Vector<document::StyleContainer::MatchedRule> *> levelMatches;
		levelMatches.resize(outerLevel + 1, nullptr);
		for (size_t i = outerLevel + 1; i-- > 0;) {
			levelMatches[i] = &cache.find(chain[i])->second.matches;
		}

#if XL_FRAME_ACCOUNT
		// the gather is accounted outside either pass
		account.styleChainNs += core::getAccountClock() - chainStart;
		const auto pass1Start = core::getAccountClock();
#endif

		// Pass 1: custom properties, every level, outermost first. All of them must be known
		// before any var() is substituted, since a more specific rule's variable is visible to a
		// less specific rule's use.
		if (anyCustom) {
			ret.initVariables(nearestStrings);
			auto declare = [&](StringView key, StringView value) {
				// merged in cascade order, so a later declaration overrides
				auto vit = ret._variables->vars.find(key);
				if (vit != ret._variables->vars.end()) {
					vit->second = value;
				} else {
					ret._variables->vars.emplace(key, value);
				}
			};
			auto collect = [&](size_t chainIndex) {
				Node *levelNode = chain[chainIndex];
				for (auto &m : *levelMatches[chainIndex]) {
					for (auto &c : m.style->custom) {
						if (c.mediaQuery != document::MediaQueryIdNone
								&& !m.media.at(c.mediaQuery.get())) {
							continue;
						}
						declare(m.strings.at(c.name), m.strings.at(c.value));
					}
				}

				// node-local declarations go last (most specific); the views point into the
				// component, which outlives this resolve
				if (auto vars = levelNode->getComponent<StyleVariables>()) {
					for (auto &it : vars->vars) { declare(it.first, it.second); }
				}
			};
			for (size_t i = outerLevel + 1; i-- > 0;) { collect(i); }
		}

#if XL_FRAME_ACCOUNT
		account.stylePass1Ns += core::getAccountClock() - pass1Start;
		const auto pass2Start = core::getAccountClock();
#endif

		// Pass 2: parameters in cascade order; a rule's deferred var() declarations expand right
		// after its literal ones, keeping their cascade position.
		auto resolveLevel = [&](document::StyleList &dst, size_t chainIndex, bool inherit) {
			for (auto &m : *levelMatches[chainIndex]) {
				dst.merge(*m.style, m.media, inherit);
				// a `width: var(--w)` on an ancestor is not inherited - only the variable is
				if (!inherit && !m.style->pending.empty()) {
					ret.expandPendingRule(dst, m);
				}
			}
		};

		// inheritable parameters cascade from the outermost styled ancestor down
		for (size_t i = outerLevel + 1; i-- > 1;) { resolveLevel(*style, i, true); }

		// the node's own matches (full, specificity-sorted) override inherited values
		resolveLevel(*style, 0, false);

		ret._style = style;
#if XL_FRAME_ACCOUNT
		account.stylePass2Ns += core::getAccountClock() - pass2Start;
#endif
	}, ret._pool);

	// string parameters resolve against the nearest sheet's string table, so strings defined by
	// outer sheets may resolve incorrectly (known limitation); var() strings use VariableTable
	ret._iface = document::SimpleStyleInterface(nearest.media,
			ret._variables ? SpanView<StringView>(ret._variables->strings) : nearestStrings, 1.0f,
			ret._media->fontScale);

	ret._valid = true;
	return ret;
}

ResolvedStyle::~ResolvedStyle() {
	if (_pool) {
		// _style is pool-allocated (AllocPool), freed with the pool; _iface is a plain value
		memory::pool::destroy(_pool);
		_pool = nullptr;
	}
}

ResolvedStyle::ResolvedStyle(ResolvedStyle &&o) noexcept
: _valid(o._valid)
, _structural(o._structural)
, _pool(o._pool)
, _style(o._style)
, _variables(o._variables)
, _iface(o._iface)
, _media(o._media) {
	o._valid = false;
	o._pool = nullptr;
	o._style = nullptr;
	o._variables = nullptr;
}

ResolvedStyle &ResolvedStyle::operator=(ResolvedStyle &&o) noexcept {
	if (this != &o) {
		if (_pool) {
			memory::pool::destroy(_pool);
		}
		_valid = o._valid;
		_structural = o._structural;
		_pool = o._pool;
		_style = o._style;
		_variables = o._variables;
		_iface = o._iface;
		_media = o._media;
		o._valid = false;
		o._pool = nullptr;
		o._style = nullptr;
		o._variables = nullptr;
	}
	return *this;
}

bool ResolvedStyle::getValue(document::ParameterName name, document::StyleValue &out) const {
	if (!_style) {
		return false;
	}
	// mirror StyleList::get(name, iface): last media-satisfied match wins. No allocation.
	bool found = false;
	for (auto &it : _style->data) {
		if (it.name == name
				&& (it.mediaQuery == document::MediaQueryIdNone
						|| _iface.resolveMediaQuery(it.mediaQuery))) {
			out = it.value;
			found = true;
		}
	}
	return found;
}

bool ResolvedStyle::has(document::ParameterName name) const {
	document::StyleValue tmp;
	return getValue(name, tmp);
}

String ResolvedStyle::getString(document::ParameterName name) const {
	document::StyleValue v;
	if (getValue(name, v)) {
		return _iface.resolveString(v.stringId).str<mem_std::Interface>();
	}
	return String();
}

void ResolvedStyle::foreach (const Callback<void(ParameterName, const StyleValue &)> &cb) const {
	for (auto &it : _style->data) {
		if ((it.mediaQuery == document::MediaQueryIdNone
					|| _iface.resolveMediaQuery(it.mediaQuery))) {
			cb(it.name, it.value);
		}
	}
}

// compiled views: compiled on demand within the owned pool

document::FontStyleParameters ResolvedStyle::font() const {
	document::FontStyleParameters ret;
	if (_style) {
		memory::perform([&] { ret = _style->compileFontStyle(&_iface); }, _pool);
	}
	return ret;
}

document::TextLayoutParameters ResolvedStyle::text() const {
	document::TextLayoutParameters ret;
	if (_style) {
		memory::perform([&] { ret = _style->compileTextLayout(&_iface); }, _pool);
	}
	return ret;
}

document::ParagraphLayoutParameters ResolvedStyle::paragraph() const {
	document::ParagraphLayoutParameters ret;
	if (_style) {
		memory::perform([&] { ret = _style->compileParagraphLayout(&_iface); }, _pool);
	}
	return ret;
}

document::BlockModelParameters ResolvedStyle::block() const {
	document::BlockModelParameters ret;
	if (_style) {
		memory::perform([&] { ret = _style->compileBlockModel(&_iface); }, _pool);
	}
	return ret;
}

document::BackgroundParameters ResolvedStyle::background() const {
	document::BackgroundParameters ret;
	if (_style) {
		memory::perform([&] { ret = _style->compileBackground(&_iface); }, _pool);
	}
	return ret;
}

document::OutlineParameters ResolvedStyle::outline() const {
	document::OutlineParameters ret;
	if (_style) {
		memory::perform([&] { ret = _style->compileOutline(&_iface); }, _pool);
	}
	return ret;
}

// individual property accessors: one list scan, no allocation

document::FontSize ResolvedStyle::fontSize() const {
	document::FontSize ret(14); // FontSpecializationVector default
	document::StyleValue v;
	if (getValue(document::ParameterName::CssFontSize, v)) {
		ret = v.fontSize;
	}
	// font-size-increment scales the resolved size (mirrors StyleList::modifySize)
	if (getValue(document::ParameterName::CssFontSizeIncrement, v)
			&& v.sizeValue.metric != document::Metric::Auto) {
		ret = ret.scale(v.sizeValue.value);
	}
	return ret;
}

document::FontStyle ResolvedStyle::fontStyle() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssFontStyle, v) ? v.fontStyle
															  : document::FontStyle::Normal;
}

document::FontWeight ResolvedStyle::fontWeight() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssFontWeight, v) ? v.fontWeight
															   : document::FontWeight::Normal;
}

document::FontStretch ResolvedStyle::fontStretch() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssFontStretch, v) ? v.fontStretch
																: document::FontStretch::Normal;
}

String ResolvedStyle::fontFamily() const {
	auto ret = getString(document::ParameterName::CssFontFamily);
	return ret.empty() ? String("default") : ret;
}

Color3B ResolvedStyle::color() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssColor, v) ? v.color : Color3B(0, 0, 0);
}

uint8_t ResolvedStyle::opacity() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssOpacity, v) ? v.opacity : uint8_t(255);
}

document::TextAlign ResolvedStyle::textAlign() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssTextAlign, v) ? v.textAlign
															  : document::TextAlign::Left;
}

document::TextDirection ResolvedStyle::direction() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssDirection, v) ? v.textDirection
															  : document::TextDirection::LeftToRight;
}

document::BidiMode ResolvedStyle::unicodeBidi() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssUnicodeBidi, v) ? v.bidiMode
																: document::BidiMode::Normal;
}

document::TextTransform ResolvedStyle::textTransform() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssTextTransform, v) ? v.textTransform
																  : document::TextTransform::None;
}

document::TextDecoration ResolvedStyle::textDecoration() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssTextDecoration, v) ? v.textDecoration
																   : document::TextDecoration::None;
}

document::WhiteSpace ResolvedStyle::whiteSpace() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssWhiteSpace, v) ? v.whiteSpace
															   : document::WhiteSpace::Normal;
}

document::Hyphens ResolvedStyle::hyphens() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssHyphens, v) ? v.hyphens : document::Hyphens::Manual;
}

document::VerticalAlign ResolvedStyle::verticalAlign() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssVerticalAlign, v)
			? v.verticalAlign
			: document::VerticalAlign::Baseline;
}

document::FontVariant ResolvedStyle::fontVariant() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssFontVariant, v) ? v.fontVariant
																: document::FontVariant::Normal;
}

document::Metric ResolvedStyle::lineHeight() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssLineHeight, v) ? v.sizeValue : document::Metric();
}

document::Display ResolvedStyle::display() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssDisplay, v) ? v.display
															: document::Display::Default;
}

document::Visibility ResolvedStyle::visibility() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssVisibility, v) ? v.visibility
															   : document::Visibility::Visible;
}

document::Overflow ResolvedStyle::overflowX() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssOverflowX, v) ? v.overflow
															  : document::Overflow::Visible;
}

document::Overflow ResolvedStyle::overflowY() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssOverflowY, v) ? v.overflow
															  : document::Overflow::Visible;
}

document::Metric ResolvedStyle::width() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssWidth, v) ? v.sizeValue : document::Metric();
}

document::Metric ResolvedStyle::height() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssHeight, v) ? v.sizeValue : document::Metric();
}

document::Metric ResolvedStyle::minWidth() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssMinWidth, v) ? v.sizeValue : document::Metric();
}

document::Metric ResolvedStyle::minHeight() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssMinHeight, v) ? v.sizeValue : document::Metric();
}

document::Metric ResolvedStyle::maxWidth() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssMaxWidth, v) ? v.sizeValue : document::Metric();
}

document::Metric ResolvedStyle::maxHeight() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssMaxHeight, v) ? v.sizeValue : document::Metric();
}

document::Metric ResolvedStyle::marginTop() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssMarginTop, v) ? v.sizeValue : document::Metric();
}

document::Metric ResolvedStyle::marginRight() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssMarginRight, v) ? v.sizeValue : document::Metric();
}

document::Metric ResolvedStyle::marginBottom() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssMarginBottom, v) ? v.sizeValue : document::Metric();
}

document::Metric ResolvedStyle::marginLeft() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssMarginLeft, v) ? v.sizeValue : document::Metric();
}

document::Metric ResolvedStyle::paddingTop() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssPaddingTop, v) ? v.sizeValue : document::Metric();
}

document::Metric ResolvedStyle::paddingRight() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssPaddingRight, v) ? v.sizeValue : document::Metric();
}

document::Metric ResolvedStyle::paddingBottom() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssPaddingBottom, v) ? v.sizeValue
																  : document::Metric();
}

document::Metric ResolvedStyle::paddingLeft() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssPaddingLeft, v) ? v.sizeValue : document::Metric();
}

document::Metric ResolvedStyle::paddingInlineStart() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssPaddingInlineStart, v) ? v.sizeValue : document::Metric();
}

document::Metric ResolvedStyle::paddingInlineEnd() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssPaddingInlineEnd, v) ? v.sizeValue : document::Metric();
}

document::Metric ResolvedStyle::marginInlineStart() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssMarginInlineStart, v) ? v.sizeValue : document::Metric();
}

document::Metric ResolvedStyle::marginInlineEnd() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssMarginInlineEnd, v) ? v.sizeValue : document::Metric();
}

document::Metric ResolvedStyle::insetInlineStart() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssInsetInlineStart, v) ? v.sizeValue : document::Metric();
}

document::Metric ResolvedStyle::insetInlineEnd() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssInsetInlineEnd, v) ? v.sizeValue : document::Metric();
}

// typed positioning / flex / grid accessors: read the raw value, else the CSS default

document::Position ResolvedStyle::position() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssPosition, v) ? v.position
															 : document::Position::Static;
}

document::Metric ResolvedStyle::top() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssTop, v) ? v.sizeValue : document::Metric();
}

document::Metric ResolvedStyle::right() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssRight, v) ? v.sizeValue : document::Metric();
}

document::Metric ResolvedStyle::bottom() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssBottom, v) ? v.sizeValue : document::Metric();
}

document::Metric ResolvedStyle::left() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssLeft, v) ? v.sizeValue : document::Metric();
}

Vec2 ResolvedStyle::anchorPoint() const {
	Vec2 ret;
	document::StyleValue v;
	if (getValue(document::ParameterName::CssXlAnchorPointX, v)) {
		ret.x = v.floatValue;
	}
	if (getValue(document::ParameterName::CssXlAnchorPointY, v)) {
		ret.y = v.floatValue;
	}
	return ret;
}

document::Metric ResolvedStyle::xlPositionX() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssXlPositionX, v) ? v.sizeValue : document::Metric();
}

document::Metric ResolvedStyle::xlPositionY() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssXlPositionY, v) ? v.sizeValue : document::Metric();
}

int32_t ResolvedStyle::xlZOrder() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssXlZOrder, v) ? v.intValue : 0;
}

document::FlexDirection ResolvedStyle::flexDirection() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssFlexDirection, v) ? v.flexDirection
																  : document::FlexDirection::Row;
}

document::FlexWrap ResolvedStyle::flexWrap() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssFlexWrap, v) ? v.flexWrap
															 : document::FlexWrap::NoWrap;
}

document::GridAutoFlow ResolvedStyle::gridAutoFlow() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssGridAutoFlow, v) ? v.gridAutoFlow
																 : document::GridAutoFlow::Row;
}

document::Align ResolvedStyle::justifyContent() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssJustifyContent, v) ? v.align
																   : document::Align::Auto;
}

document::Align ResolvedStyle::alignContent() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssAlignContent, v) ? v.align : document::Align::Auto;
}

document::Align ResolvedStyle::justifyItems() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssJustifyItems, v) ? v.align : document::Align::Auto;
}

document::Align ResolvedStyle::alignItems() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssAlignItems, v) ? v.align : document::Align::Auto;
}

document::Align ResolvedStyle::justifySelf() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssJustifySelf, v) ? v.align : document::Align::Auto;
}

document::Align ResolvedStyle::alignSelf() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssAlignSelf, v) ? v.align : document::Align::Auto;
}

float ResolvedStyle::flexGrow() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssFlexGrow, v) ? v.floatValue : 0.0f;
}

float ResolvedStyle::flexShrink() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssFlexShrink, v) ? v.floatValue : 1.0f;
}

document::Metric ResolvedStyle::flexBasis() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssFlexBasis, v) ? v.sizeValue : document::Metric();
}

int32_t ResolvedStyle::order() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssOrder, v) ? v.intValue : 0;
}

document::Metric ResolvedStyle::rowGap() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssRowGap, v) ? v.sizeValue : document::Metric();
}

document::Metric ResolvedStyle::columnGap() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssColumnGap, v) ? v.sizeValue : document::Metric();
}

String ResolvedStyle::gridTemplateColumns() const {
	return getString(document::ParameterName::CssGridTemplateColumns);
}
String ResolvedStyle::gridTemplateRows() const {
	return getString(document::ParameterName::CssGridTemplateRows);
}
String ResolvedStyle::gridAutoColumns() const {
	return getString(document::ParameterName::CssGridAutoColumns);
}
String ResolvedStyle::gridAutoRows() const {
	return getString(document::ParameterName::CssGridAutoRows);
}
String ResolvedStyle::gridColumnStart() const {
	return getString(document::ParameterName::CssGridColumnStart);
}
String ResolvedStyle::gridColumnEnd() const {
	return getString(document::ParameterName::CssGridColumnEnd);
}
String ResolvedStyle::gridRowStart() const {
	return getString(document::ParameterName::CssGridRowStart);
}
String ResolvedStyle::gridRowEnd() const {
	return getString(document::ParameterName::CssGridRowEnd);
}

document::TableLayout ResolvedStyle::tableLayout() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssTableLayout, v) ? v.tableLayout
																: document::TableLayout::Auto;
}
document::BorderCollapse ResolvedStyle::borderCollapse() const {
	document::StyleValue v;
	// CSS initial value is `separate`, unlike the document engine's compiled block model
	return getValue(document::ParameterName::CssBorderCollapse, v)
			? v.borderCollapse
			: document::BorderCollapse::Separate;
}
document::Metric ResolvedStyle::borderSpacingHorizontal() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssBorderSpacingHorizontal, v) ? v.sizeValue
																			: document::Metric();
}
document::Metric ResolvedStyle::borderSpacingVertical() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssBorderSpacingVertical, v) ? v.sizeValue
																		  : document::Metric();
}
uint32_t ResolvedStyle::columnSpan() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssXlColumnSpan, v) ? v.uintValue : 1;
}
uint32_t ResolvedStyle::rowSpan() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssXlRowSpan, v) ? v.uintValue : 1;
}

bool StyleResolver::init(bool recursive) {
	if (!System::init()) {
		return false;
	}

	_recursive = recursive;

	// HandleNodeEvents: react to the owner's own resize (handleContentSizeDirty) and to the
	// owner's parent resize (handleLayoutInParent) - percent metrics depend on the parent size
	auto flags = SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents
			| SystemFlags::HandleNodeEvents | SystemFlags::HandleComponents
			| SystemFlags::HandleAncestorComponents | SystemFlags::HandleLayoutChildren;

	if (_recursive) {
		// publish on the frame stack so descendants deliver their content-size / layout-children
		// events here; HandleChildComponents also catches their interactive state flips
		flags |= SystemFlags::HandleChildNodeEvents | SystemFlags::HandleChildComponents
				| SystemFlags::AddToFrameStack;
		setFrameTag(SystemFrameTag);
	}

	setSystemFlags(flags);
	return true;
}

bool StyleResolver::init(ApplyCallback &&cb, bool recursive) {
	if (!init()) {
		return false;
	}
	_callback = move(cb);
	return true;
}

void StyleResolver::handleAdded(Node *owner) { System::handleAdded(owner); }

void StyleResolver::handleEnter(Scene *scene) {
	System::handleEnter(scene);

	_interactiveMask = 0;
	_sourceSystemVersion = 0;
	_sourceSystemId = 0; // systemId starts from 1, update will be triggered if it exists

	// initial application: the ancestor chain is complete here, and the node may have
	// been added after the sheet owner's dirty frame
	_owner->markLayoutChildrenDirty();
}

void StyleResolver::handleComponentsDirty(const ComponentMask &mask) {
	System::handleComponentsDirty(mask);

	apply();
}

void StyleResolver::handleContentSizeDirty() {
	System::handleContentSizeDirty();

	// the owner itself was resized: own-size-relative paddings/gaps may need recompute
	resolveOwnerIfStale();
}

void StyleResolver::handleLayoutInParent(Node *parent) {
	System::handleLayoutInParent(parent);

	// the owner's parent was resized (or the owner was just attached): percent metrics
	// resolve against the parent size, so the applied style may be stale now
	resolveOwnerIfStale();
}

void StyleResolver::handleChildContentSizeDirty(Node *child) {
	// a descendant resized, or an ancestor did (nodes with HandleParentContentSize: percent
	// metrics or absolute insets); re-resolve unless still fresh
	if (_recursive && _owner && !isNodeFresh(child)) {
		resolveForNode(child);
	}
}

void StyleResolver::handleChildComponentsDirty(Node *child, const ComponentMask &mask) {
	if (!_recursive || !_owner) {
		return;
	}

	if (_recursive && _owner) {
		if (_nodesUpdated.find(child) == _nodesUpdated.end()
				|| mask.count(NodeIdentity::Id.value) != 0
				|| mask.count(InteractiveComponent::Id.value) != 0
				|| mask.count(FocusWithinComponent::Id.value) != 0
				|| mask.count(SelectionComponent::Id.value) != 0
				|| mask.count(StyleSystemState::Id.value) != 0
				// a node-local custom property changed: the only signal that its style is stale
				|| mask.count(StyleVariables::Id.value) != 0) {
			resolveForNode(child);
		}
	}
}

// re-arm every descendant's content-size phase so the recursive resolver re-resolves it after a
// CSS reload, which moves no geometry and so raises no event by itself
static void markSubtreeComponentsDirty(Node *node) {
	for (auto &child : node->getChildren()) {
		child->markContentSizeDirty();
		markSubtreeComponentsDirty(child);
	}
}

static void forEachInSubtree(Node *node, const Callback<void(Node *)> &cb) {
	for (auto &child : node->getChildren()) {
		cb(child);
		forEachInSubtree(child, cb);
	}
}

void StyleResolver::apply() {
	if (!_owner) {
		return;
	}

	uint32_t currentInteractiveMask = 0;
	uint64_t currentSourceId = 0;
	uint32_t currentSourceVersion = 0;

	if (!_owner->findParentWithComponent<StyleSystemState>(
				[&](NotNull<Node>, NotNull<const StyleSystemState> state, uint32_t) {
		currentSourceId = state->systemId;
		currentSourceVersion = state->version;
		return false; // stop recursion
	})) {
		return; // no style system in chain
	}

	if (auto ic = _owner->getComponent<InteractiveComponent>()) {
		currentInteractiveMask = toInt(ic->state);
	}

	// marker-component states, so a `:focus-within` or selection change is not skipped below
	if (hasFocusWithin(_owner)) {
		currentInteractiveMask |= toInt(InteractiveState::FocusWithin);
	}
	if (hasSelectionWithin(_owner)) {
		currentInteractiveMask |= toInt(InteractiveState::SelectionWithin);
	}
	if (isNodeSelected(_owner)) {
		currentInteractiveMask |= toInt(InteractiveState::Selected);
	}

	log::source().warn("StyleResolver", "apply ver=", currentSourceVersion, " was=",
			_sourceSystemVersion, " id=", currentSourceId, " wasid=", _sourceSystemId);
	if (currentInteractiveMask == _interactiveMask && currentSourceId == _sourceSystemId
			&& currentSourceVersion == _sourceSystemVersion) {
		return; // no changes to run style resolver
	}

	// the stylesheet source or version changed (not just interactive state): the subtree is stale
	const bool sourceChanged =
			currentSourceId != _sourceSystemId || currentSourceVersion != _sourceSystemVersion;

	_nodesUpdated.clear();
	// the subtree is re-armed below when the source changed, so no digest needs to survive
	_nodeCustomProperties.clear();

	_interactiveMask = currentInteractiveMask;
	_sourceSystemId = currentSourceId;
	_sourceSystemVersion = currentSourceVersion;

	// resolve the owner directly: the resolver joins the frame stack after the owner's phases, so
	// the owner's events never reach it
	resolveForNode(_owner);

	if (_recursive && sourceChanged) {
		// resolve descendants synchronously, so a layout pushed after the enter pass (popup bodies)
		// has no unstyled first frame; deduped via _nodesUpdated
		forEachInSubtree(_owner, [&](Node *child) { resolveForNode(child); });
	}
}

namespace {

using document::Align;

// document flex/grid enums -> ui layout enums

static FlexDirection toFlexDirection(document::FlexDirection d) {
	switch (d) {
	case document::FlexDirection::Row: return FlexDirection::Row;
	case document::FlexDirection::RowReverse: return FlexDirection::RowReverse;
	case document::FlexDirection::Column: return FlexDirection::Column;
	case document::FlexDirection::ColumnReverse: return FlexDirection::ColumnReverse;
	}
	return FlexDirection::Row;
}

static FlexWrap toFlexWrap(document::FlexWrap w) {
	switch (w) {
	case document::FlexWrap::NoWrap: return FlexWrap::NoWrap;
	case document::FlexWrap::Wrap: return FlexWrap::Wrap;
	case document::FlexWrap::WrapReverse: return FlexWrap::WrapReverse;
	}
	return FlexWrap::NoWrap;
}

static GridAutoFlow toGridAutoFlow(document::GridAutoFlow f) {
	switch (f) {
	case document::GridAutoFlow::Row: return GridAutoFlow::Row;
	case document::GridAutoFlow::Column: return GridAutoFlow::Column;
	case document::GridAutoFlow::RowDense: return GridAutoFlow::RowDense;
	case document::GridAutoFlow::ColumnDense: return GridAutoFlow::ColumnDense;
	}
	return GridAutoFlow::Row;
}

/* One axis, for placing a CSS Box Alignment keyword:
  * `flex-start`/`flex-end` are flex-relative and map straight through (the backend mirrors
    reversed lines);
  * `start`/`end` are flow-relative; the backend already reverses an RTL row, so only `reversed`
    matters;
  * `left`/`right` are physical and depend on direction; on a non-inline axis they act as
    `start`. */
struct AlignAxis {
	bool reversed = false; // the axis is laid out against its natural order
	bool inlineAxis = false; // this axis is the inline (horizontal) one
	bool rtl = false; // the inline direction is right-to-left

	// a flow-relative `start` sits at the axis's flow start unless the axis is reversed
	bool logicalStart() const { return !reversed; }

	// a physical `left` sits at the flow start when the axis runs that way visually
	bool physicalLeftIsStart() const { return inlineAxis ? !(reversed != rtl) : !reversed; }
};

// justify-content -> flex main-axis distribution
static FlexJustify toFlexJustify(Align a, const AlignAxis &ax) {
	switch (a) {
	case Align::FlexStart: return FlexJustify::FlexStart;
	case Align::FlexEnd: return FlexJustify::FlexEnd;
	case Align::Start:
	case Align::SelfStart:
		return ax.logicalStart() ? FlexJustify::FlexStart : FlexJustify::FlexEnd;
	case Align::End:
	case Align::SelfEnd: return ax.logicalStart() ? FlexJustify::FlexEnd : FlexJustify::FlexStart;
	case Align::Left:
		return ax.physicalLeftIsStart() ? FlexJustify::FlexStart : FlexJustify::FlexEnd;
	case Align::Right:
		return ax.physicalLeftIsStart() ? FlexJustify::FlexEnd : FlexJustify::FlexStart;
	case Align::Center: return FlexJustify::Center;
	case Align::SpaceBetween: return FlexJustify::SpaceBetween;
	case Align::SpaceAround: return FlexJustify::SpaceAround;
	case Align::SpaceEvenly: return FlexJustify::SpaceEvenly;
	default: return FlexJustify::FlexStart;
	}
}

// align-items / align-content; Normal/Auto/baseline fall back to Stretch
static FlexAlign toFlexAlignItems(Align a, const AlignAxis &ax) {
	switch (a) {
	case Align::FlexStart: return FlexAlign::FlexStart;
	case Align::FlexEnd: return FlexAlign::FlexEnd;
	case Align::Start:
	case Align::SelfStart: return ax.logicalStart() ? FlexAlign::FlexStart : FlexAlign::FlexEnd;
	case Align::End:
	case Align::SelfEnd: return ax.logicalStart() ? FlexAlign::FlexEnd : FlexAlign::FlexStart;
	case Align::Left: return ax.physicalLeftIsStart() ? FlexAlign::FlexStart : FlexAlign::FlexEnd;
	case Align::Right: return ax.physicalLeftIsStart() ? FlexAlign::FlexEnd : FlexAlign::FlexStart;
	case Align::Center: return FlexAlign::Center;
	case Align::SpaceBetween: return FlexAlign::SpaceBetween;
	case Align::SpaceAround: return FlexAlign::SpaceAround;
	default: return FlexAlign::Stretch;
	}
}

// align-self; Auto/Normal inherit the container's align-items
static FlexAlign toFlexAlignSelf(Align a, const AlignAxis &ax) {
	if (a == Align::Auto || a == Align::Normal) {
		return FlexAlign::Auto;
	}
	return toFlexAlignItems(a, ax);
}

/* Grid justify/align (content or items). Flow-relative keywords map straight through (the backend
mirrors the inline axis at projection); `left`/`right` depend on direction only on the inline
axis, and act as `start` on the block axis. */
static GridAlign toGridAlign(Align a, bool rtl, bool inlineAxis) {
	const bool leftIsStart = inlineAxis ? !rtl : true;
	switch (a) {
	case Align::Start:
	case Align::FlexStart:
	case Align::SelfStart: return GridAlign::Start;
	case Align::End:
	case Align::FlexEnd:
	case Align::SelfEnd: return GridAlign::End;
	case Align::Left: return leftIsStart ? GridAlign::Start : GridAlign::End;
	case Align::Right: return leftIsStart ? GridAlign::End : GridAlign::Start;
	case Align::Center: return GridAlign::Center;
	case Align::Stretch: return GridAlign::Stretch;
	case Align::SpaceBetween: return GridAlign::SpaceBetween;
	case Align::SpaceAround: return GridAlign::SpaceAround;
	case Align::SpaceEvenly: return GridAlign::SpaceEvenly;
	default: return GridAlign::Stretch;
	}
}

// grid justify-self / align-self; Auto/Normal inherit the container's items value
static GridAlign toGridAlignSelf(Align a, bool rtl, bool inlineAxis) {
	if (a == Align::Auto || a == Align::Normal) {
		return GridAlign::Auto;
	}
	return toGridAlign(a, rtl, inlineAxis);
}

// one compiled border side -> table collapse edge; `border-style: none` yields an edge that loses
// every conflict
static TableBorderEdge toTableBorder(const document::OutlineParameters::Params &p,
		const document::MediaParameters &media, float fontSize, float base) {
	TableBorderEdge edge;
	edge.style = p.style;
	edge.color = p.color;
	if (p.style != document::BorderStyle::None) {
		edge.width = sprt::max(media.computeValueAuto(p.width, base, fontSize), 0.0f);
	}
	return edge;
}

} // namespace

// the state the style was applied against - see StyleResolver::StyleFreshness
StyleResolver::StyleFreshness StyleResolver::makeStyleFreshness(Node *node) const {
	auto p = node->getParent();
	// the child-list version enters the key only when a sheet in scope uses structural
	// pseudo-classes, so sibling insertions don't re-resolve the whole child list.
	// Source versions of every scope on the chain are folded, the node's own first: the owner of a
	// StyleSystem is in its scope, and outer sheets' rules still match inside nested ones.
	uint32_t sourceVersion = 0;
	auto foldVersion = [&](uint32_t version) {
		sourceVersion = (sourceVersion ^ version) * 0x9E37'79B1u + 1;
	};
	if (auto own = node->getComponent<StyleSystemState>()) {
		foldVersion(own->version);
	}
	node->findParentWithComponent<StyleSystemState>(
			[&](NotNull<Node>, NotNull<const StyleSystemState> state, uint32_t) {
		foldVersion(state->version);
		return true;
	});

	return StyleFreshness{p ? p->getContentSize() : Size2::ZERO, node->getContentSize(),
		(p && _structuralSelectors) ? p->getChildrenVersion() : 0, sourceVersion};
}

// Does the resolved style depend on the parent's content size (then the node needs
// NodeEventFlags::HandleParentContentSize)? True for any percent metric, and for
// `position: absolute` with a non-auto inset, whose placement uses the parent size even in px.
static bool hasParentRelativeMetrics(const ResolvedStyle &s) {
	using document::ParameterName;

	if (s.position() == document::Position::Absolute) {
		// an undeclared inset resolves to a default-constructed Metric, i.e. Auto
		for (auto &inset : {s.top(), s.right(), s.bottom(), s.left()}) {
			if (!inset.isAuto()) {
				return true;
			}
		}
	}

	for (auto name : {ParameterName::CssWidth, ParameterName::CssHeight, ParameterName::CssTop,
			 ParameterName::CssRight, ParameterName::CssBottom, ParameterName::CssLeft,
			 ParameterName::CssXlPositionX, ParameterName::CssXlPositionY,
			 ParameterName::CssMarginTop, ParameterName::CssMarginRight,
			 ParameterName::CssMarginBottom, ParameterName::CssMarginLeft,
			 ParameterName::CssFlexBasis}) {
		document::StyleValue v;
		if (s.getValue(name, v) && v.sizeValue.metric == document::Metric::Units::Percent) {
			return true;
		}
	}
	return false;
}

bool StyleResolver::isNodeFresh(Node *node) const {
	auto it = _nodesUpdated.find(node);
	return it != _nodesUpdated.end() && it->second == makeStyleFreshness(node);
}

void StyleResolver::resolveOwnerIfStale() {
	if (_owner && !isNodeFresh(_owner)) {
		resolveForNode(_owner);
	}
}

void StyleResolver::resolveForNode(Node *node) {
	if (!node) {
		return;
	}
	// re-entrant call from applyDefault: queue the node and drain after the outer pass
	if (_inResolve) {
		for (auto *n : _pendingResolve) {
			if (n == node) {
				return;
			}
		}
		_pendingResolve.emplace_back(node);
		return;
	}

#if XL_FRAME_ACCOUNT
	auto &account = getVisitAccount();
	++account.styleResolves;
	const auto styleStart = core::getAccountClock();
	struct StyleClose {
		VisitAccount *a;
		uint64_t start;
		~StyleClose() { a->styleNs += core::getAccountClock() - start; }
	} styleClose{&account, styleStart};
#endif

	struct ResolveScope {
		StyleResolver *resolver;
		explicit ResolveScope(StyleResolver *r) : resolver(r) { resolver->_inResolve = true; }
		~ResolveScope() {
			resolver->_inResolve = false;
			auto pending = sp::move(resolver->_pendingResolve);
			resolver->_pendingResolve.clear();
			for (auto *n : pending) { resolver->resolveForNode(n); }
		}
	};
	ResolveScope scope(this);

	auto style = resolveStyleForNode(node);
	if (!style.valid()) {
		return;
	}

	// learned from the resolve (apply() may early-out); never cleared, which only costs a
	// redundant freshness field
	_structuralSelectors = _structuralSelectors || style.hasStructuralSelectors();

	// a changed custom property set makes every descendant stale without an event; re-arm them
	if (auto hash = style.getCustomPropertiesHash()) {
		auto vit = _nodeCustomProperties.find(node);
		if (vit == _nodeCustomProperties.end()) {
			_nodeCustomProperties.emplace(node, hash);
		} else if (vit->second != hash) {
			vit->second = hash;
			// drop freshness entries, or isNodeFresh() swallows the re-fired phase
			forEachInSubtree(node, [&](Node *child) { _nodesUpdated.erase(child); });
			markSubtreeComponentsDirty(node);
		}
	}

	// pre-mark to guard against re-entry while applying; the final key is recorded below
	// (applyDefault may change the node's own content size)
	auto it = _nodesUpdated.find(node);
	if (it == _nodesUpdated.end()) {
		it = _nodesUpdated.emplace(node, makeStyleFreshness(node)).first;
	}

	// a node whose style depends on the parent size must re-run its content-size phase when an
	// ancestor resizes, so its frame-stack event reaches this resolver again (the bit is never
	// cleared - a spurious phase re-run is cut off by the freshness check)
	if (hasParentRelativeMetrics(style)
			&& !hasFlag(node->getEventFlags(), NodeEventFlags::HandleParentContentSize)) {
		node->setEventFlags(node->getEventFlags() | NodeEventFlags::HandleParentContentSize);
	}

	if (_callback && _callback(node, style)) {
		it->second = makeStyleFreshness(node);
		return;
	}

	applyDefault(node, style);

	// re-find: applyDefault may have resolved other nodes and rehashed the map
	if (auto fit = _nodesUpdated.find(node); fit != _nodesUpdated.end()) {
		fit->second = makeStyleFreshness(node);
	}
}

void StyleResolver::applyTypeAttributes(Node *node, const ResolvedStyle &s,
		sprt::bitset<toInt(document::ParameterName::Max)> &handled) {
	auto &reg = getTypeApplierRegistry();
	auto it = reg.find(node->getType());
	if (it == reg.end() || !it->second.applier) {
		return;
	}

	// CmdReset goes first on every pass (only to appliers that list it; value unspecified): undo
	// the previous pass's styling, not the widget's own state, then the loop re-applies what is
	// declared. Adding/removing components here does not re-trigger a resolve, since
	// handleChildComponentsDirty filters by component id.
	if (it->second.mask.test(toInt(document::ParameterName::CmdReset))) {
		document::StyleValue val;
		if (it->second.applier(*this, node, s, document::ParameterName::CmdReset, val)) {
			handled.set(toInt(document::ParameterName::CmdReset));
		}
	}

	s.foreach ([&](document::ParameterName name, const document::StyleValue &val) {
		if (it->second.mask.test(toInt(name))) {
			if (it->second.applier(*this, node, s, name, val)) {
				handled.set(toInt(name));
			}
		}
	});
}

void StyleResolver::applyDefault(Node *node, const ResolvedStyle &s) {
	using document::ParameterName;

	Size2 parentSize;
	if (auto p = node->getParent()) {
		parentSize = p->getContentSize();
	}
	// read only the individual properties this applier needs (no whole-block compilation);
	// font-size is always needed as the em base for computeMetric
	const float fontSize = float(s.fontSize().get());
	const auto width = s.width();
	const auto height = s.height();

	auto computeMetric = [&](const document::Metric &m, float base) {
		return s.media().computeValueAuto(m, base, fontSize);
	};

	// type phase: attributes a registered per-type applier consumes are recorded in `handled`, so
	// the default mapping below skips them (a type overrides only the attributes it registered)
	sprt::bitset<toInt(document::ParameterName::Max)> handled;
	applyTypeAttributes(node, s, handled);
	auto def = [&](ParameterName name) { return s.has(name) && !handled.test(toInt(name)); };

	if (def(ParameterName::CssOpacity)) {
		node->setOpacity(float(s.opacity()) / 255.0f);
	}

	// -xl-z-order: set before applyLayout, so the reorder phase sorts children before the
	// LayoutSystem runs; setLocalZOrder is equality-guarded
	if (def(ParameterName::CssXlZOrder)) {
		node->setLocalZOrder(ZOrder(int16_t(s.xlZOrder())));
	}

	// display:none / visibility:hidden -> VisibilityComponent on the node; wrapVisit honors it
	// like setVisible(false) (whole subtree skipped), layout engines additionally collapse the
	// display:none box. The node's own explicit setVisible state is never touched.
	{
		VisibilityComponent v;
		if (def(ParameterName::CssDisplay) && s.display() == document::Display::None) {
			v.displayNone = true;
		}
		if (def(ParameterName::CssVisibility) && s.visibility() != document::Visibility::Visible) {
			v.visibilityHidden = true;
		}
		if (v.displayNone || v.visibilityHidden) {
			node->setOrUpdateComponent<VisibilityComponent>([&](NotNull<VisibilityComponent> c) {
				if (*c != v) {
					*c = v;
					return true;
				}
				return false;
			});
		} else {
			node->removeComponent<VisibilityComponent>();
		}
	}

	// inheritable color/font/text properties -> Inherited*Style components, accumulated by
	// consumers over the parent chain (XLInheritedStyle.h); an empty component is removed
	{
		InheritedColorStyle v;
		if (def(ParameterName::CssColor)) {
			v.color = s.color();
			v.defined |= InheritedColorStyle::DefinedColor;
		}
		// CSS `opacity` is not inherited; it is applied via Node::setOpacity above
		if (v.defined != 0) {
			node->setOrUpdateComponent<InheritedColorStyle>([&](NotNull<InheritedColorStyle> c) {
				if (*c != v) {
					*c = v;
					return true;
				}
				return false;
			});
		} else {
			node->removeComponent<InheritedColorStyle>();
		}
	}
	{
		InheritedFontStyle v;
		if (def(ParameterName::CssFontSize) || def(ParameterName::CssFontSizeNumeric)) {
			v.fontSize = s.fontSize(); // includes font-size-increment
			v.defined |= InheritedFontStyle::DefinedFontSize;
		}
		if (def(ParameterName::CssFontFamily)) {
			if (auto ff = s.fontFamily(); !ff.empty()) {
				v.fontFamily = sp::move(ff);
				v.defined |= InheritedFontStyle::DefinedFontFamily;
			}
		}
		if (def(ParameterName::CssFontWeight)) {
			v.fontWeight = s.fontWeight();
			v.defined |= InheritedFontStyle::DefinedFontWeight;
		}
		if (def(ParameterName::CssFontStyle)) {
			v.fontStyle = s.fontStyle();
			v.defined |= InheritedFontStyle::DefinedFontStyle;
		}
		if (def(ParameterName::CssFontStretch)) {
			v.fontStretch = s.fontStretch();
			v.defined |= InheritedFontStyle::DefinedFontStretch;
		}
		if (def(ParameterName::CssFontVariant)) {
			v.fontVariant = s.fontVariant();
			v.defined |= InheritedFontStyle::DefinedFontVariant;
		}
		if (v.defined != 0) {
			node->setOrUpdateComponent<InheritedFontStyle>([&](NotNull<InheritedFontStyle> c) {
				if (*c != v) {
					*c = v;
					return true;
				}
				return false;
			});
		} else {
			node->removeComponent<InheritedFontStyle>();
		}
	}
	{
		InheritedTextStyle v;
		if (def(ParameterName::CssTextAlign)) {
			v.textAlign = s.textAlign();
			v.defined |= InheritedTextStyle::DefinedTextAlign;
		}
		if (def(ParameterName::CssDirection)) {
			v.direction = s.direction();
			v.defined |= InheritedTextStyle::DefinedDirection;
		}
		if (def(ParameterName::CssUnicodeBidi)) {
			v.bidi = s.unicodeBidi();
			v.defined |= InheritedTextStyle::DefinedBidi;
		}
		if (def(ParameterName::CssTextTransform)) {
			v.textTransform = s.textTransform();
			v.defined |= InheritedTextStyle::DefinedTextTransform;
		}
		if (def(ParameterName::CssTextDecoration)) {
			v.textDecoration = s.textDecoration();
			v.defined |= InheritedTextStyle::DefinedTextDecoration;
		}
		if (def(ParameterName::CssWhiteSpace)) {
			v.whiteSpace = s.whiteSpace();
			v.defined |= InheritedTextStyle::DefinedWhiteSpace;
		}
		if (def(ParameterName::CssHyphens)) {
			v.hyphens = s.hyphens();
			v.defined |= InheritedTextStyle::DefinedHyphens;
		}
		if (def(ParameterName::CssVerticalAlign)) {
			v.verticalAlign = s.verticalAlign();
			v.defined |= InheritedTextStyle::DefinedVerticalAlign;
		}
		if (def(ParameterName::CssLineHeight)) {
			const auto lh = s.lineHeight();
			switch (lh.metric) {
			case document::Metric::Units::Auto: // unitless number ("line-height: 1.5")
			case document::Metric::Units::Em:
			case document::Metric::Units::Percent: // already stored as a /100 factor
				v.lineHeight = lh.value;
				v.lineHeightAbsolute = false;
				break;
			default:
				v.lineHeight = computeMetric(lh, fontSize);
				v.lineHeightAbsolute = true;
				break;
			}
			v.defined |= InheritedTextStyle::DefinedLineHeight;
		}
		if (v.defined != 0) {
			node->setOrUpdateComponent<InheritedTextStyle>([&](NotNull<InheritedTextStyle> c) {
				if (*c != v) {
					*c = v;
					return true;
				}
				return false;
			});
		} else {
			node->removeComponent<InheritedTextStyle>();
		}
	}

	auto label = dynamic_cast<Label *>(node);
	if (label) {
		// only non-inheritable geometry is pushed directly; inheritable color/font/text
		// properties flow through the Inherited*Style components above
		if (def(ParameterName::CssWidth) && !width.isAuto()
				&& width.metric != document::Metric::Units::FitContent) {
			label->setWidth(computeMetric(width, parentSize.width));
		}
	} else {
		if (def(ParameterName::CssBackgroundColor)) {
			// Layer covers Button as well
			document::StyleValue styleColor;
			if (s.getValue(ParameterName::CssBackgroundColor, styleColor)) {
				node->setColor(Color4F(styleColor.color4), true);
			}
		}

		// fit-content never writes a static size: it resolves through the
		// flex item mapping in applyLayout (basis / crossSize) instead
		const bool widthExplicit = def(ParameterName::CssWidth) && !width.isAuto()
				&& width.metric != document::Metric::Units::FitContent;
		const bool heightExplicit = def(ParameterName::CssHeight) && !height.isAuto()
				&& height.metric != document::Metric::Units::FitContent;

		// Under a flex/grid/SystemManagedLayout parent the layout is the sole writer of
		// ContentSize: pass the CSS size as MeasureComponent input (< 0 per axis = unspecified)
		// instead of setContentSize. Out-of-flow (`position: absolute`) nodes commit it directly.
		auto parent = node->getParent();
		const bool parentManagesSize = parent
				&& (parent->getComponent<FlexLayoutInfo>() || parent->getComponent<GridLayoutInfo>()
						|| parent->getComponent<SystemManagedLayout>())
				&& s.position() != document::Position::Absolute;

		if (parentManagesSize) {
			if (widthExplicit || heightExplicit) {
				const float w = widthExplicit ? computeMetric(width, parentSize.width) : -1.0f;
				const float h = heightExplicit ? computeMetric(height, parentSize.height) : -1.0f;
				node->setOrUpdateComponent<MeasureComponent>([&](NotNull<MeasureComponent> mc) {
					if (mc->normal != Size2(w, h)) {
						mc->normal = Size2(w, h);
						return true;
					}
					return false;
				});
			}
		} else {
			auto size = node->getContentSize();
			bool sizeDirty = false;
			if (widthExplicit) {
				size.width = computeMetric(width, parentSize.width);
				sizeDirty = true;
			}
			if (heightExplicit) {
				size.height = computeMetric(height, parentSize.height);
				sizeDirty = true;
			}
			if (sizeDirty) {
				node->setContentSize(size);
			}
		}
	}

	// flexbox / grid: add/remove/configure the LayoutSystem from `display`, and map
	// this node's flex/grid item properties (incl. margins) onto the parent
	// container's per-item component. Also maps padding onto the container.
	applyLayout(node, s);

	// positioning: `position: absolute` places the node via top/right/bottom/left
	// offsets against the parent; every other `position` value applies -xl-anchor-point
	if (s.position() == document::Position::Absolute) {
		// not a flex/grid item: the layout must skip it, or it overwrites the offsets below
		node->setComponent<OutOfFlowComponent>(OutOfFlowComponent{true});

		auto nodeSize = node->getContentSize();

		const auto left = s.left();
		const auto right = s.right();
		const auto top = s.top();
		const auto bottom = s.bottom();
		const bool hasLeft = s.has(ParameterName::CssLeft) && !left.isAuto();
		const bool hasRight = s.has(ParameterName::CssRight) && !right.isAuto();
		const bool hasTop = s.has(ParameterName::CssTop) && !top.isAuto();
		const bool hasBottom = s.has(ParameterName::CssBottom) && !bottom.isAuto();

		// auto size with both offsets on an axis stretches to fill; when over-constrained, the
		// right/bottom offset is ignored (the position math prefers left/top)
		const bool widthAuto = !s.has(ParameterName::CssWidth) || width.isAuto();
		const bool heightAuto = !s.has(ParameterName::CssHeight) || height.isAuto();

		bool sizeDirty = false;
		if (widthAuto && hasLeft && hasRight) {
			nodeSize.width = parentSize.width - computeMetric(left, parentSize.width)
					- computeMetric(right, parentSize.width);
			if (nodeSize.width < 0.0f) {
				nodeSize.width = 0.0f;
			}
			sizeDirty = true;
		}
		if (heightAuto && hasTop && hasBottom) {
			nodeSize.height = parentSize.height - computeMetric(top, parentSize.height)
					- computeMetric(bottom, parentSize.height);
			if (nodeSize.height < 0.0f) {
				nodeSize.height = 0.0f;
			}
			sizeDirty = true;
		}
		if (sizeDirty) {
			node->setContentSize(nodeSize);
		}

		// pin the node by its top-left corner (anchor 0,1) so CSS offsets map directly;
		// engine Y grows upward, so the top edge sits at parentHeight - top
		float x = node->getPosition().x;
		if (hasLeft) {
			x = computeMetric(left, parentSize.width);
		} else if (hasRight) {
			x = parentSize.width - computeMetric(right, parentSize.width) - nodeSize.width;
		}

		float y = node->getPosition().y;
		if (hasTop) {
			y = parentSize.height - computeMetric(top, parentSize.height);
		} else if (hasBottom) {
			y = computeMetric(bottom, parentSize.height) + nodeSize.height;
		}

		node->setAnchorPoint(Vec2(0.0f, 1.0f));
		node->setPosition(Vec2(x, y));
	} else {
		// back in flow only if the style put it out; code-set OutOfFlowComponent has
		// styleManaged == false and is kept
		if (auto c = node->getComponent<OutOfFlowComponent>(); c && c->styleManaged) {
			node->removeComponent<OutOfFlowComponent>();
		}

		if (s.has(ParameterName::CssXlAnchorPointX) || s.has(ParameterName::CssXlAnchorPointY)) {
			node->setAnchorPoint(s.anchorPoint());
		}

		// -xl-position: direct node position; percent values resolve against the parent
		if (s.has(ParameterName::CssXlPositionX) || s.has(ParameterName::CssXlPositionY)) {
			float x = node->getPosition().x;
			float y = node->getPosition().y;
			if (s.has(ParameterName::CssXlPositionX)) {
				x = computeMetric(s.xlPositionX(), parentSize.width);
			}
			if (s.has(ParameterName::CssXlPositionY)) {
				y = computeMetric(s.xlPositionY(), parentSize.height);
			}
			node->setPosition(Vec2(x, y));
		}
	}
}

void StyleResolver::applyLayout(Node *node, const ResolvedStyle &s) {
	using document::ParameterName;
	using document::Display;

	Size2 parentSize;
	if (auto p = node->getParent()) {
		parentSize = p->getContentSize();
	}
	const Size2 ownSize = node->getContentSize();
	// read only the individual properties this mapping needs (no whole-block compilation)
	const auto display = s.display();
	const auto width = s.width();
	const auto height = s.height();
	const float fontSize = float(s.fontSize().get());
	auto computeMetric = [&](const document::Metric &m, float base) {
		return s.media().computeValueAuto(m, base, fontSize);
	};

	// computed direction: maps `*-inline-*` properties to physical sides here; physical
	// properties (`padding-left`) are never flipped
	const bool rtl = s.direction() == document::TextDirection::RightToLeft;

	// map the CSS padding-* onto a container Padding (percent against own width)
	auto fillPadding = [&](Padding &pad) {
		if (auto m = s.paddingTop(); s.has(ParameterName::CssPaddingTop) && !m.isAuto()) {
			pad.top = computeMetric(m, ownSize.width);
		}
		if (auto m = s.paddingRight(); s.has(ParameterName::CssPaddingRight) && !m.isAuto()) {
			pad.right = computeMetric(m, ownSize.width);
		}
		if (auto m = s.paddingBottom(); s.has(ParameterName::CssPaddingBottom) && !m.isAuto()) {
			pad.bottom = computeMetric(m, ownSize.width);
		}
		if (auto m = s.paddingLeft(); s.has(ParameterName::CssPaddingLeft) && !m.isAuto()) {
			pad.left = computeMetric(m, ownSize.width);
		}
		// inline pair after the physical sides: when both are declared the logical one wins
		// (StyleList keeps no cross-name source order)
		if (auto m = s.paddingInlineStart();
				s.has(ParameterName::CssPaddingInlineStart) && !m.isAuto()) {
			(rtl ? pad.right : pad.left) = computeMetric(m, ownSize.width);
		}
		if (auto m = s.paddingInlineEnd();
				s.has(ParameterName::CssPaddingInlineEnd) && !m.isAuto()) {
			(rtl ? pad.left : pad.right) = computeMetric(m, ownSize.width);
		}
	};
	// map CSS border sides onto table collapse edges, only for declared sides, so values set in
	// code survive (applyLayout runs for every node on every pass)
	auto fillTableBorders = [&](const ResolvedStyle &st, TableCellInfo &cfg, float base) {
		const auto outline = st.outline();
		auto side = [&](ParameterName styleName, ParameterName widthName, ParameterName colorName,
							const document::OutlineParameters::Params &p, TableBorderEdge &out) {
			if (!st.has(styleName) && !st.has(widthName) && !st.has(colorName)) {
				return;
			}
			out = toTableBorder(p, st.media(), float(st.fontSize().get()), base);
		};
		side(ParameterName::CssBorderTopStyle, ParameterName::CssBorderTopWidth,
				ParameterName::CssBorderTopColor, outline.top, cfg.borderTop);
		side(ParameterName::CssBorderRightStyle, ParameterName::CssBorderRightWidth,
				ParameterName::CssBorderRightColor, outline.right, cfg.borderRight);
		side(ParameterName::CssBorderBottomStyle, ParameterName::CssBorderBottomWidth,
				ParameterName::CssBorderBottomColor, outline.bottom, cfg.borderBottom);
		side(ParameterName::CssBorderLeftStyle, ParameterName::CssBorderLeftWidth,
				ParameterName::CssBorderLeftColor, outline.left, cfg.borderLeft);
	};

	// map CSS margin-* onto an item Margin (percent against parent width); `auto` goes into
	// `autoMask` (FlexAutoMargin), which is null for containers that cannot honour it
	auto fillMargin = [&](Padding &mrg, FlexAutoMargin *autoMask) {
		auto side = [&](ParameterName name, const document::Metric &m, float &out,
							FlexAutoMargin flag) {
			if (!s.has(name)) {
				return;
			}
			if (m.isAuto()) {
				if (autoMask) {
					*autoMask |= flag;
					out = 0.0f;
				}
				return;
			}
			out = computeMetric(m, parentSize.width);
			if (autoMask) {
				*autoMask &= ~flag;
			}
		};
		side(ParameterName::CssMarginTop, s.marginTop(), mrg.top, FlexAutoMargin::Top);
		side(ParameterName::CssMarginRight, s.marginRight(), mrg.right, FlexAutoMargin::Right);
		side(ParameterName::CssMarginBottom, s.marginBottom(), mrg.bottom, FlexAutoMargin::Bottom);
		side(ParameterName::CssMarginLeft, s.marginLeft(), mrg.left, FlexAutoMargin::Left);
		// see fillPadding: the inline pair resolves last and wins over the physical one
		side(ParameterName::CssMarginInlineStart, s.marginInlineStart(),
				rtl ? mrg.right : mrg.left, rtl ? FlexAutoMargin::Right : FlexAutoMargin::Left);
		side(ParameterName::CssMarginInlineEnd, s.marginInlineEnd(), rtl ? mrg.left : mrg.right,
				rtl ? FlexAutoMargin::Left : FlexAutoMargin::Right);
	};

	// SystemManagedLayout: a system owns the children's geometry, so the container block below is
	// disabled; the per-item mapping for this node still runs
	const bool systemManaged = node->getComponent<SystemManagedLayout>() != nullptr;

	const bool wantFlex =
			!systemManaged && (display == Display::Flex || display == Display::InlineFlex);
	const bool wantGrid =
			!systemManaged && (display == Display::Grid || display == Display::InlineGrid);
	const bool wantRow = !systemManaged && display == Display::TableRow;

	// `display: table` survives SystemManagedLayout (ui::TableView takes its column tracks from
	// CSS): only the parameter component is written, no system is added
	const bool wantTable = display == Display::Table;

	// `overflow-x` / `overflow-y`: record the pair and add or drop the ScrollSystem. Not gated on
	// systemManaged; ScrollSystem writes no ContentSize and without a LayoutSystem only clips.
	{
		using document::Overflow;
		const bool declared =
				s.has(ParameterName::CssOverflowX) || s.has(ParameterName::CssOverflowY);
		if (declared) {
			OverflowComponent next;
			next.styleManaged = true;
			if (auto c = node->getComponent<OverflowComponent>()) {
				next = *c;
				next.styleManaged = true;
			}
			if (s.has(ParameterName::CssOverflowX)) {
				next.x = s.overflowX();
			}
			if (s.has(ParameterName::CssOverflowY)) {
				next.y = s.overflowY();
			}
			// axes stay as declared: CSS's "visible computes to auto" rule is not applied, since
			// the scissor is built per axis (ui::ScissorAxes)
			node->setOrUpdateComponent<OverflowComponent>([&](NotNull<OverflowComponent> c) {
				if (*c != next) {
					*c = next;
					return true;
				}
				return false;
			});
		} else if (auto c = node->getComponent<OverflowComponent>(); c && c->styleManaged) {
			node->removeComponent<OverflowComponent>();
		}

		auto ovf = node->getComponent<OverflowComponent>();
		auto scroll = node->getSystemByType<ScrollSystem>();
		if (ovf && (ovf->clipsX() || ovf->clipsY())) {
			if (!scroll) {
				scroll = node->addSystem(Rc<ScrollSystem>::create(ovf->x, ovf->y));
				node->setComponent<StyleManagedScroll>();
			} else {
				scroll->setOverflow(ovf->x, ovf->y);
			}
		} else if (scroll && node->getComponent<StyleManagedScroll>()) {
			node->removeSystem(scroll);
			node->removeComponent<StyleManagedScroll>();
		}
	}

	auto layout = systemManaged ? nullptr : node->getSystemByType<LayoutSystem>();

	if (wantTable) {
		node->setOrUpdateComponent<TableLayoutInfo>([&](NotNull<TableLayoutInfo> info) {
			TableLayoutInfo next = *info;
			if (s.has(ParameterName::CssGridTemplateColumns)) {
				next.columnTracks = parseGridTemplate(s.gridTemplateColumns());
			}
			if (s.has(ParameterName::CssGridAutoColumns)) {
				auto t = parseGridTemplate(s.gridAutoColumns());
				if (!t.empty()) {
					next.autoColumn = t.front();
				}
			}
			if (s.has(ParameterName::CssTableLayout)) {
				next.algorithm = s.tableLayout();
			}
			if (s.has(ParameterName::CssBorderCollapse)) {
				next.borderCollapse = s.borderCollapse();
			}
			if (auto m = s.borderSpacingHorizontal();
					s.has(ParameterName::CssBorderSpacingHorizontal) && !m.isAuto()) {
				next.borderSpacingH = computeMetric(m, ownSize.width);
			}
			if (auto m = s.borderSpacingVertical();
					s.has(ParameterName::CssBorderSpacingVertical) && !m.isAuto()) {
				next.borderSpacingV = computeMetric(m, ownSize.width);
			}
			if (s.has(ParameterName::CssJustifyItems)) {
				next.justifyItems = toGridAlign(s.justifyItems(), rtl, true);
			}
			if (s.has(ParameterName::CssAlignItems)) {
				next.alignItems = toGridAlign(s.alignItems(), rtl, false);
			}
			fillPadding(next.padding);
			// the table's own border takes part in the collapse pass; reuse the cell mapping
			// through a scratch TableCellInfo
			{
				TableCellInfo scratch;
				scratch.borderTop = next.borderTop;
				scratch.borderRight = next.borderRight;
				scratch.borderBottom = next.borderBottom;
				scratch.borderLeft = next.borderLeft;
				fillTableBorders(s, scratch, ownSize.width);
				next.borderTop = scratch.borderTop;
				next.borderRight = scratch.borderRight;
				next.borderBottom = scratch.borderBottom;
				next.borderLeft = scratch.borderLeft;
			}
			if (next != *info) {
				*info = next;
				return true;
			}
			return false;
		});
	}

	if (!wantFlex && !wantGrid && !wantTable && !wantRow) {
		// only tear down layouts the resolver added (marker present)
		if (layout && node->getComponent<StyleManagedLayout>()) {
			node->removeSystem(layout);
			node->removeComponent<FlexLayoutInfo>();
			node->removeComponent<GridLayoutInfo>();
			node->removeComponent<TableLayoutInfo>();
			// keep TableColumnsComponent / TableBordersComponent: outputs owned by the table pass
			// or the widget owning the rows
			node->removeComponent<StyleManagedLayout>();
		}
	} else if (systemManaged) {
		// a system owns this node's children; the parameter component above is all the style does
	} else {
		const LayoutMode mode = wantTable ? LayoutMode::Table
				: wantRow				  ? LayoutMode::TableRow
				: wantGrid				  ? LayoutMode::Grid
										  : LayoutMode::Flex;
		if (!layout) {
			layout = node->addSystem(Rc<LayoutSystem>::create());
			node->setComponent<StyleManagedLayout>();
		}
		if (layout->getMode() != mode) {
			layout->setMode(mode);
		}

		if (mode == LayoutMode::Flex) {
			if (node->getComponent<GridLayoutInfo>()) {
				node->removeComponent<GridLayoutInfo>();
			}
			node->setOrUpdateComponent<FlexLayoutInfo>([&](NotNull<FlexLayoutInfo> info) {
				FlexLayoutInfo next = *info;
				if (s.has(ParameterName::CssFlexDirection)) {
					next.direction = toFlexDirection(s.flexDirection());
				}
				if (s.has(ParameterName::CssFlexWrap)) {
					next.wrap = toFlexWrap(s.flexWrap());
				}

				// axes are derived after direction and wrap are final, including values set in code
				const bool isRowFlow = next.direction == FlexDirection::Row
						|| next.direction == FlexDirection::RowReverse;
				const bool flowReversed = next.direction == FlexDirection::RowReverse
						|| next.direction == FlexDirection::ColumnReverse;
				const AlignAxis mainAxis{flowReversed, isRowFlow, rtl};
				const AlignAxis crossAxis{next.wrap == FlexWrap::WrapReverse, !isRowFlow, rtl};

				if (s.has(ParameterName::CssJustifyContent)) {
					next.justifyContent = toFlexJustify(s.justifyContent(), mainAxis);
				}
				if (s.has(ParameterName::CssAlignItems)) {
					next.alignItems = toFlexAlignItems(s.alignItems(), crossAxis);
				}
				if (s.has(ParameterName::CssAlignContent)) {
					next.alignContent = toFlexAlignItems(s.alignContent(), crossAxis);
				}
				if (auto gap = s.columnGap(); s.has(ParameterName::CssColumnGap) && !gap.isAuto()) {
					next.columnGap = computeMetric(gap, ownSize.width);
				}
				if (auto gap = s.rowGap(); s.has(ParameterName::CssRowGap) && !gap.isAuto()) {
					next.rowGap = computeMetric(gap, ownSize.height);
				}
				fillPadding(next.padding);
				if (next != *info) {
					*info = next;
					return true;
				}
				return false;
			});
		} else {
			if (node->getComponent<FlexLayoutInfo>()) {
				node->removeComponent<FlexLayoutInfo>();
			}
			node->setOrUpdateComponent<GridLayoutInfo>([&](NotNull<GridLayoutInfo> info) {
				GridLayoutInfo next = *info;
				if (s.has(ParameterName::CssGridTemplateColumns)) {
					next.columnTracks = parseGridTemplate(s.gridTemplateColumns());
				}
				if (s.has(ParameterName::CssGridTemplateRows)) {
					next.rowTracks = parseGridTemplate(s.gridTemplateRows());
				}
				if (s.has(ParameterName::CssGridAutoColumns)) {
					auto t = parseGridTemplate(s.gridAutoColumns());
					if (!t.empty()) {
						next.autoColumn = t.front();
					}
				}
				if (s.has(ParameterName::CssGridAutoRows)) {
					auto t = parseGridTemplate(s.gridAutoRows());
					if (!t.empty()) {
						next.autoRow = t.front();
					}
				}
				if (s.has(ParameterName::CssGridAutoFlow)) {
					next.autoFlow = toGridAutoFlow(s.gridAutoFlow());
				}
				if (s.has(ParameterName::CssJustifyContent)) {
					next.justifyContent = toGridAlign(s.justifyContent(), rtl, true);
				}
				if (s.has(ParameterName::CssAlignContent)) {
					next.alignContent = toGridAlign(s.alignContent(), rtl, false);
				}
				if (s.has(ParameterName::CssJustifyItems)) {
					next.justifyItems = toGridAlign(s.justifyItems(), rtl, true);
				}
				if (s.has(ParameterName::CssAlignItems)) {
					next.alignItems = toGridAlign(s.alignItems(), rtl, false);
				}
				if (auto gap = s.columnGap(); s.has(ParameterName::CssColumnGap) && !gap.isAuto()) {
					next.columnGap = computeMetric(gap, ownSize.width);
				}
				if (auto gap = s.rowGap(); s.has(ParameterName::CssRowGap) && !gap.isAuto()) {
					next.rowGap = computeMetric(gap, ownSize.height);
				}
				fillPadding(next.padding);
				if (next != *info) {
					*info = next;
					return true;
				}
				return false;
			});
		}
	}

	// item config: map this node's flex/grid item properties onto its parent
	// container's per-item component (grid takes priority when both are present)
	auto parent = node->getParent();
	if (!parent) {
		return;
	}

	bool itemChanged = false;
	// table cells are keyed on the parent's TableColumnsComponent, which rows carry both in static
	// tables and in virtualized ui::TableView
	if (parent->getComponent<TableColumnsComponent>()) { // this node is a cell
		node->setOrUpdateComponent<TableCellInfo>([&](NotNull<TableCellInfo> info) {
			TableCellInfo next = *info;
			if (s.has(ParameterName::CssXlColumnSpan)) {
				next.columnSpan = sprt::max(s.columnSpan(), 1u);
			}
			if (s.has(ParameterName::CssXlRowSpan)) {
				next.rowSpan = sprt::max(s.rowSpan(), 1u);
			}
			if (s.has(ParameterName::CssJustifySelf)) {
				next.justifySelf = toGridAlignSelf(s.justifySelf(), rtl, true);
			}
			if (s.has(ParameterName::CssAlignSelf)) {
				next.alignSelf = toGridAlignSelf(s.alignSelf(), rtl, false);
			}
			// `vertical-align` is the cell's cross-axis alignment; mapped only when declared here
			if (s.has(ParameterName::CssVerticalAlign)) {
				switch (s.verticalAlign()) {
				case document::VerticalAlign::Top: next.alignSelf = GridAlign::Start; break;
				case document::VerticalAlign::Bottom: next.alignSelf = GridAlign::End; break;
				case document::VerticalAlign::Middle: next.alignSelf = GridAlign::Center; break;
				default: break; // baseline / sub / super have no table meaning here
				}
			}
			fillTableBorders(s, next, parentSize.width);
			// a cell has no auto-margin behaviour: an `auto` side resolves to zero
			fillMargin(next.margin, nullptr);
			if (next != *info) {
				*info = next;
				itemChanged = true;
				return true;
			}
			return false;
		});
	} else if (parent->getComponent<TableLayoutInfo>()) { // this node is a row
		node->setOrUpdateComponent<TableRowInfo>([&](NotNull<TableRowInfo> info) {
			TableRowInfo next = *info;
			// guarded: an undeclared height keeps its current value
			if (s.has(ParameterName::CssHeight)) {
				next.height =
						(!height.isAuto() && height.metric != document::Metric::Units::FitContent)
						? computeMetric(height, parentSize.height)
						: TableRowInfo::Auto;
			}
			if (s.has(ParameterName::CssOrder)) {
				next.order = s.order();
			}
			if (next != *info) {
				*info = next;
				itemChanged = true;
				return true;
			}
			return false;
		});
	} else if (parent->getComponent<GridLayoutInfo>()) {
		node->setOrUpdateComponent<GridItemInfo>([&](NotNull<GridItemInfo> info) {
			GridItemInfo next = *info;
			uint32_t a = 0, b = 0, c = 1;
			if (auto v = s.gridColumnStart(); s.has(ParameterName::CssGridColumnStart) && !v.empty()
					&& parseGridLine(v, a, b, c)) {
				next.gridColumnStart = a;
				if (c > 1) {
					next.columnSpan = c;
				}
			}
			a = 0, b = 0, c = 1;
			if (auto v = s.gridColumnEnd(); s.has(ParameterName::CssGridColumnEnd) && !v.empty()
					&& parseGridLine(v, a, b, c)) {
				next.gridColumnEnd = a; // a bare line number lands in `start`
				if (c > 1) {
					next.columnSpan = c;
				}
			}
			a = 0, b = 0, c = 1;
			if (auto v = s.gridRowStart(); s.has(ParameterName::CssGridRowStart) && !v.empty()
					&& parseGridLine(v, a, b, c)) {
				next.gridRowStart = a;
				if (c > 1) {
					next.rowSpan = c;
				}
			}
			a = 0, b = 0, c = 1;
			if (auto v = s.gridRowEnd(); s.has(ParameterName::CssGridRowEnd) && !v.empty()
					&& parseGridLine(v, a, b, c)) {
				next.gridRowEnd = a;
				if (c > 1) {
					next.rowSpan = c;
				}
			}
			if (s.has(ParameterName::CssJustifySelf)) {
				next.justifySelf = toGridAlignSelf(s.justifySelf(), rtl, true);
			}
			if (s.has(ParameterName::CssAlignSelf)) {
				next.alignSelf = toGridAlignSelf(s.alignSelf(), rtl, false);
			}
			if (s.has(ParameterName::CssOrder)) {
				next.order = s.order();
			}
			// grid has no auto-margin alignment yet: an `auto` side resolves to zero
			fillMargin(next.margin, nullptr);
			if (next != *info) {
				*info = next;
				itemChanged = true;
				return true;
			}
			return false;
		});
	} else if (auto flexInfo = parent->getComponent<FlexLayoutInfo>()) {
		const bool parentIsRow = flexInfo->direction == FlexDirection::Row
				|| flexInfo->direction == FlexDirection::RowReverse;
		node->setOrUpdateComponent<FlexItemInfo>([&](NotNull<FlexItemInfo> info) {
			FlexItemInfo next = *info;
			if (s.has(ParameterName::CssFlexGrow)) {
				next.grow = s.flexGrow();
			}
			if (s.has(ParameterName::CssFlexShrink)) {
				next.shrink = s.flexShrink();
			}
			if (s.has(ParameterName::CssFlexBasis)) {
				auto basis = s.flexBasis();
				if (basis.isAuto()) {
					next.basis = FlexItemInfo::Auto;
				} else if (basis.metric == document::Metric::Units::FitContent) {
					next.basis = FlexItemInfo::FitContent;
				} else {
					// clamp: negative lengths are invalid and would collide
					// with the Auto/FitContent sentinels
					next.basis = sprt::max(computeMetric(basis, parentSize.width), 0.0f);
				}
			}
			// width/height: fit-content projected onto the flow axes: the
			// cross axis lands in crossSize, the main axis becomes the basis
			// unless an explicit flex-basis was given
			const bool widthFit = s.has(ParameterName::CssWidth)
					&& width.metric == document::Metric::Units::FitContent;
			const bool heightFit = s.has(ParameterName::CssHeight)
					&& height.metric == document::Metric::Units::FitContent;
			if (parentIsRow ? heightFit : widthFit) {
				next.crossSize = FlexItemInfo::FitContent;
			}
			if ((parentIsRow ? widthFit : heightFit) && !s.has(ParameterName::CssFlexBasis)) {
				next.basis = FlexItemInfo::FitContent;
			}
			// min-/max- on the main axis map onto the item's clamps; cross-axis ones are ignored
			// (FlexItemInfo has no field for them)
			{
				const auto minMain = parentIsRow ? s.minWidth() : s.minHeight();
				const auto maxMain = parentIsRow ? s.maxWidth() : s.maxHeight();
				const auto minName =
						parentIsRow ? ParameterName::CssMinWidth : ParameterName::CssMinHeight;
				const auto maxName =
						parentIsRow ? ParameterName::CssMaxWidth : ParameterName::CssMaxHeight;
				const float base = parentIsRow ? parentSize.width : parentSize.height;

				if (s.has(minName) && !minMain.isAuto()) {
					next.minMain = sprt::max(computeMetric(minMain, base), 0.0f);
				}
				if (s.has(maxName)) {
					// `max-*: none` parses as auto here, and auto means "unbounded" for maxMain
					next.maxMain = maxMain.isAuto() ? FlexItemInfo::Auto
													: sprt::max(computeMetric(maxMain, base), 0.0f);
				}
			}
			if (s.has(ParameterName::CssAlignSelf)) {
				// `align-self` uses the container's cross axis; default row when the parent has
				// no FlexLayoutInfo
				AlignAxis selfAxis{false, false, rtl};
				if (auto parent = node->getParent()) {
					if (auto pinfo = parent->getComponent<FlexLayoutInfo>()) {
						const bool parentRow = pinfo->direction == FlexDirection::Row
								|| pinfo->direction == FlexDirection::RowReverse;
						selfAxis = AlignAxis{pinfo->wrap == FlexWrap::WrapReverse, !parentRow,
							rtl};
					}
				}
				next.alignSelf = toFlexAlignSelf(s.alignSelf(), selfAxis);
			}
			if (s.has(ParameterName::CssOrder)) {
				next.order = s.order();
			}
			fillMargin(next.margin, &next.autoMargin);
			if (next != *info) {
				*info = next;
				itemChanged = true;
				return true;
			}
			return false;
		});
	}

	if (itemChanged) {
		LayoutSystem::markItemDirty(node);
	}
}

} // namespace stappler::xenolith::ui
