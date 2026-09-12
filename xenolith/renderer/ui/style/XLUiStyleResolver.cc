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

// Bloom bits of a node's identity tokens; MUST use the same kinds (tag=0/class=1/id=2)
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

/* Custom properties in effect for one node, plus the strings that `var()` substitution had to
intern. Lives in the ResolvedStyle's pool.

`vars` maps a property name to its raw, unexpanded text; both views point into the string table
of whichever sheet declared it, which outlives the ResolvedStyle.

`strings` is empty unless substitution produced a STRING-valued parameter (`font-family:
var(--face)`). It cannot go through DocumentData::addString - that table never dedupes, so
re-resolving a node every frame would grow the sheet without bound. Instead it starts as a copy
of the nearest sheet's table, so ids already handed out stay valid, and the substituted strings
are appended to it. */
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

// Expand and parse the deferred `var()` declarations of one matched rule into `dst`, at the
// point in the cascade where the rule itself is being merged. A declaration whose expansion
// fails (undefined variable with no fallback, or a reference cycle) is dropped, exactly as CSS
// requires - it never reaches `dst`, so whatever a less specific rule set simply stands.
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

/* THE MATCH CACHE: which rules match ONE node, kept so the nodes under it do not each re-derive it.

A resolve walks the node's ancestor chain and asks, at every level, which of the sheets' rules match
that level's node. The studio's nodes sit eighteen levels deep, so that is eighteen questions per
node - and seventeen of them are about ancestors, asked again by every one of the five hundred nodes
in the subtree. The answers were identical every time: `collectMatches` is given the level's node,
that node's own index in the chain (which decides the sheets in scope) and the Bloom bits of that
node's own ancestors, so a level's match set is a property of THAT NODE ALONE. Measured at 3.6 ms of
a 6.0 ms style cost on a cold page, i.e. the largest single item left in the visit.

So it is cached per node, and the cache is checked against the CSS match stamp
(`Node::getStyleMatchId`, folded over the node and every ancestor above it) rather than against an
epoch: a global epoch would be cleared by every addChild of a page being built, which is exactly when
the cache has to work. A stamp covers one chain, so building a subtree invalidates that subtree and
nothing above it.

Entries keyed by a raw pointer are never read for a node that died: the stamp folds in a per-node id
that is unique for the life of the process, so an address the allocator hands out again cannot match
what the previous tenant stored. A node removed from its parent also bumps its parent's child-list
stamp, and the entries left behind are overwritten or dropped with the cache. They carry pointers
into the sheets (the rule, its string table, its media bits), which a sheet reload would dangle -
and cannot be read after one, because the sheet lives on the chain and its StyleSystemState version
is part of the stamp.

The bound is the crude one on purpose: a resolve needs eighteen entries and a page a few hundred, so
the map is cleared wholesale when it grows past a limit no real page reaches. Cost of being wrong:
one re-gather per level, the thing that used to happen every time. */
namespace {

struct MatchCacheEntry {
	uint64_t stamp = 0;
	Vector<document::StyleContainer::MatchedRule> matches;
};

/* Keyed by a POINTER, hence the spreading hasher - the same reason StyleResolver::_nodesUpdated
gives: node addresses share their alignment bits, and the default pointer hash leaves exactly those
low bits to choose the bucket. */
using MatchCacheMap = sprt::__malloc_unordered_map<const Node *, MatchCacheEntry,
		sprt::hash_spread<>, sprt::equal_to<void>>;

// thread_local rather than global: a process can run several app threads, each with its own scene
// graph, and nothing here is shared between them
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

	// A node-local declaration (StyleVariables) is a source of custom properties no sheet knows
	// about, so the variable table has to exist even when not one sheet in scope declares any.
	// Only the levels PASS 1 actually visits are checked, which is the same range gatherLevel
	// covers.
	if (!anyCustom) {
		for (size_t i = 0; i <= scopes.back().chainIndex; ++i) {
			if (chain[i]->getComponent<StyleVariables>()) {
				anyCustom = true;
				break;
			}
		}
	}

	// Gather every rule matching `levelNode` (simple + combinator/pseudo) from every scope
	// visible at `chainIndex`, across sheets, into one list sorted by CSS specificity (ties
	// broken by scope rank + source order) - i.e. in the order the cascade must apply them.
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

	// Build ONLY the raw merged parameter list plus the interpretation context here.
	// Nothing is compiled or extracted - each consumer reads what it needs from the
	// returned ResolvedStyle. The pool is owned by `ret` and freed with it.
	ret._pool = memory::pool::create(static_cast<memory::pool_t *>(nullptr));
	ret._media = &nearest.system->getMediaParameters();
	auto nearestStrings = nearest.system->getStyleSheet()->getStrings();

	memory::perform([&] {
		auto style = new (ret._pool) document::StyleList();

		// Both passes walk the SAME levels - the chain from the outermost sheet scope down to the
		// node - and at each level they need the SAME specificity-sorted rule list. Gathering it
		// twice was the single biggest item in a resolve (a lookup in every sheet in scope plus a
		// sort, about 0.44us, twice per chain level). Gather once here; both passes read it.
		//
		// In two steps, and the order matters: every level is put in the cache FIRST, and only then
		// are the pointers taken. An insert may rehash the map, and a pointer taken before one
		// would be left pointing at a moved entry.
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
		// the gather is prologue work now, not part of either pass
		account.styleChainNs += core::getAccountClock() - chainStart;
		const auto pass1Start = core::getAccountClock();
#endif

		// PASS 1 - custom properties only, every level, outermost first. They are always
		// inherited, and the whole cascade of them must be known before a single var() is
		// substituted: CSS resolves a variable to its computed value on the element, so a
		// variable declared by a MORE specific rule is visible to a use in a less specific one.
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

				// The node's own declarations go in after every rule that matched it: a
				// node-local property is the most specific source there is, the same standing an
				// inline style would have. The views point into the component, which outlives
				// this resolve - a ResolvedStyle is produced and consumed inside one apply().
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

		// PASS 2 - the parameters themselves, in cascade order. A rule's deferred var()
		// declarations are expanded right after its literal ones, so a substituted value takes
		// exactly the cascade position it was written at instead of winning by arriving last.
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

	// note: string parameters (font-family, background-image, grid tracks) resolve against
	// the NEAREST sheet's string table; with multiple sheets in scope, string values defined
	// by outer sheets may resolve incorrectly - documented v1 limitation. Strings produced by
	// var() substitution are appended to a private copy of that table (see VariableTable).
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

// individual property accessors: resolve exactly one parameter (mirroring the matching field of the
// compiled block) so a single-value read never expands a whole block. No allocation, one list scan.

document::FontSize ResolvedStyle::fontSize() const {
	document::FontSize ret(14); // FontSpecializationVector default
	document::StyleValue v;
	if (getValue(document::ParameterName::CssFontSize, v)) {
		ret = v.fontSize;
	}
	// font-size-increment scales the resolved size (mirrors StyleList::modifySize in compileFontStyle)
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
	// CSS initial value is `separate`, even though the document engine's compiled block model
	// defaults the other way
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
	// owner's PARENT resize (handleLayoutInParent) - percent metrics depend on the parent size
	auto flags = SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents
			| SystemFlags::HandleNodeEvents | SystemFlags::HandleComponents
			| SystemFlags::HandleAncestorComponents | SystemFlags::HandleLayoutChildren;

	if (_recursive) {
		// publish on the frame stack so every descendant delivers its content-size / layout-children
		// event back to this single resolver, which then resolves that descendant's style.
		// HandleChildComponents additionally catches a descendant's own components change (its
		// interactive :hover/:focus/:active flip), which the content-size cascade would miss
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
	// A descendant's content-size phase fired (delivered via the frame stack): its own size
	// changed, or - for nodes carrying NodeEventFlags::HandleParentContentSize (percent metrics
	// OR absolute insets) - an ancestor resized. Re-resolve unless the style is still fresh
	// (same version, resolved against the same parent/own sizes); equality-guarded writes make
	// re-resolution converge.
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
				// a node-local custom property changed: nothing moved and no rule started or
				// stopped matching, so this is the only signal that its style is stale
				|| mask.count(StyleVariables::Id.value) != 0) {
			resolveForNode(child);
		}
	}
}

// Force every descendant to re-run its content-size phase so each fires the frame-stack child event
// that makes the nearest recursive resolver re-resolve it. Needed when the stylesheet itself changes
// (CSS reload): a style-only change moves no geometry, so descendants would otherwise never signal
// and would keep their stale styles until some unrelated relayout happened to wake them.
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

	// ...and the one state that is NOT in that component. Without this the early return below
	// fires on a node whose only change was gaining or losing `:focus-within`.
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

	// the stylesheet source (id) or its version changed - i.e. the CSS was (re)loaded, not merely a
	// local interactive-state flip; the whole subtree's resolved styles are now potentially stale
	const bool sourceChanged =
			currentSourceId != _sourceSystemId || currentSourceVersion != _sourceSystemVersion;

	_nodesUpdated.clear();
	// the subtree is re-armed below when the source changed, so no digest needs to survive
	_nodeCustomProperties.clear();

	_interactiveMask = currentInteractiveMask;
	_sourceSystemId = currentSourceId;
	_sourceSystemVersion = currentSourceVersion;

	// resolve the owner's own style. A recursive resolver is pushed onto the frame stack only AFTER
	// its owner's phases, so the owner never delivers its events back to its own resolver - the owner
	// must be resolved directly here (descendants cascade via their frame-stack child events).
	resolveForNode(_owner);

	if (_recursive && sourceChanged) {
		// Sync-resolve every descendant now. markSubtreeComponentsDirty alone only arms a later
		// content-size phase; a layout pushed after the scene's enter pass (aux popup bodies) would
		// otherwise present its first frame with unresolved Inherited* styles on labels. Deduped
		// per node via _nodesUpdated inside resolveForNode.
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

/* ONE AXIS, described well enough to place a CSS Box Alignment keyword on it.

The three keyword families mean three different things, and collapsing them is what this struct
exists to stop:

  * `flex-start`/`flex-end` are FLEX-relative. The backend lays every line out in flow coordinates
    and mirrors the whole line when the axis is reversed, so these two map straight through and the
    backend's own reversal is the answer.
  * `start`/`end` (and `self-start`/`self-end`) are FLOW-relative - the writing mode's start, not
    the flex direction's. On a row they are the inline start, which under `direction: rtl` is the
    right edge; the backend also reverses an RTL row, and the two flips CANCEL. That is why
    `logicalStart()` below asks only about `reversed`: the direction has already been paid for.
  * `left`/`right` are PHYSICAL, and are the only family that has to ask the direction here. On an
    axis that is not the inline one CSS says they behave as `start`, which is what `inlineAxis`
    selects. */
struct AlignAxis {
	bool reversed = false; // the axis is laid out against its natural order
	bool inlineAxis = false; // this axis is the inline (horizontal) one
	bool rtl = false; // the inline direction is right-to-left

	// A flow-relative `start` sits at the axis's flow start unless the axis is reversed.
	bool logicalStart() const { return !reversed; }

	// A physical `left` sits at the flow start when the axis runs that way visually. On a
	// non-inline axis `left` degrades to `start`, per CSS Box Alignment.
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

/* Grid justify/align (content or items).

A grid track order is never "reversed" the way a flex line is, so the flow-relative keywords map
straight through and the backend mirrors the whole inline axis once, at projection. Only the
physical pair has to ask the direction, and only on the inline axis - `align-*` runs down the block
axis, where CSS says `left`/`right` behave as `start`. */
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

// One compiled border side -> the edge the table's collapse pass resolves. `border-*-width` is a
// Metric, so it has to be computed like any other length; `border-style: none` (the CSS initial
// value) yields an edge that loses every conflict.
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
	// the child-list version enters the key only when some sheet in scope actually uses a
	// structural pseudo-class; otherwise a sibling insertion would needlessly re-resolve the
	// whole child list (Node::markChildrenStructureDirty re-arms every sibling's phase)
	// The style system's version rides along, so that a sheet reload or a media flag flipped
	// between frames makes every node stale even when nothing moved.
	uint32_t sourceVersion = 0;
	node->findParentWithComponent<StyleSystemState>(
			[&](NotNull<Node>, NotNull<const StyleSystemState> state, uint32_t) {
		sourceVersion = state->version;
		return false;
	});

	return StyleFreshness{p ? p->getContentSize() : Size2::ZERO, node->getContentSize(),
		(p && _structuralSelectors) ? p->getChildrenVersion() : 0, sourceVersion};
}

// Does the resolved style depend on the parent's content size? Such nodes must re-resolve when
// an ancestor resizes — they opt into NodeEventFlags::HandleParentContentSize.
//
// Two cases:
// 1. Any Percent metric (width/height/margin/inset/…) — classic `%` against the parent box.
// 2. `position: absolute` with a non-auto inset (`top`/`right`/`bottom`/`left`), even in px —
//    placement is `y = parentHeight - top` (and left+right / top+bottom stretch the size), so a
//    first resolve against a zero-sized parent would stick forever. That is the installer
//    title-line bug: the bar's height arrives only after the flex pass writes MeasureComponent
//    into ContentSize, and without this bit the absolute child never sees the update.
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
	// Re-entrancy: applyDefault below mutates components and size, and that can come back here
	// for another node while this node's ResolvedStyle is still live. Queue it instead and drain
	// once the outer pass is done, so no node is ever half-applied when the next one starts.
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

	// Whether the sibling order matters is learned from the resolve rather than scanned up
	// front: apply() early-outs when the sheet lives on the resolver's own node, so it is not a
	// reliable place to look. Never cleared - a sheet that loses its structural selectors on
	// reload only costs a redundant freshness field.
	_structuralSelectors = _structuralSelectors || style.hasStructuralSelectors();

	// Custom properties are inherited and are substituted into values at resolve time, so when
	// a node's set changes - a class flip on it that brings in a different `--brand` - every
	// descendant's applied style is stale. The descendants get no event of their own (nothing
	// moved or resized), so re-arm the subtree by hand.
	if (auto hash = style.getCustomPropertiesHash()) {
		auto vit = _nodeCustomProperties.find(node);
		if (vit == _nodeCustomProperties.end()) {
			_nodeCustomProperties.emplace(node, hash);
		} else if (vit->second != hash) {
			vit->second = hash;
			// dropping the freshness entries is what makes the nudge land: nothing about the
			// descendants' geometry changed, so isNodeFresh() would otherwise swallow the
			// re-fired phase and they would keep the old variable's value
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

	// The reset command, delivered BEFORE any parameter and on EVERY pass (not only when
	// something changed). A style pass carries only the declarations that are present, so when a
	// rule stops matching - a class flip, an edited stylesheet - its properties just go missing
	// and nothing below would undo them. The applier answers by dropping whatever it took from
	// the previous pass - and no more than that: what the widget painted on itself is not the
	// pass's to take. The loop underneath then re-applies what is still declared.
	//
	// It is a pseudo-parameter: no CSS syntax produces it, so it never appears in `s` and an
	// applier only sees it if it listed CmdReset in its ParameterMask. Its value is unspecified.
	//
	// An applier may freely add or remove components here: a component change on the styled node
	// carries only that component's id, and handleChildComponentsDirty re-resolves a node it has
	// already seen only for NodeIdentity / InteractiveComponent / StyleSystemState - so the
	// remove-then-recreate this causes does not feed back into another resolve.
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

	// -xl-z-order: the node's ZOrder. Set it up front (before applyLayout): changing it marks the
	// parent's reorder dirty, and the reorder phase runs before handleLayoutChildren, so the flex/grid
	// LayoutSystem sees the children in the requested order. setLocalZOrder is equality-guarded.
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

	// Inheritable color/font/text properties -> data components on the node itself. Label and
	// other consumers accumulate them over the parent chain (see XLInheritedStyle.h), so any
	// descendant — styled or not — picks them up. Writes are equality-guarded; a component with
	// nothing defined is removed so consumers revert to their explicit values.
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

		// When a parent flex/grid container lays this node out, the LayoutSystem is the SOLE writer of
		// its ContentSize. Publishing the CSS-requested size here via setContentSize too would create a
		// cycle (style writes ContentSize <-> layout reads it as the natural size <-> layout writes it):
		// a re-resolve then re-imposes the CSS size over the laid-out size (the os-button double-height
		// bug). Instead hand the requested size to the layout as intrinsic INPUT in a MeasureComponent
		// (a per-axis value < 0 means "unspecified"); the layout reads it and owns ContentSize.
		// ...unless this node is out of the container's flow (`position: absolute`), in which case
		// no layout will ever read that component and the CSS size has to be committed directly.
		//
		// SystemManagedLayout is the same claim made without flex/grid parameters: a container that
		// places its children by its own rules (ui::DockSystem) is just as much the sole writer of
		// their ContentSize, and reads the MeasureComponent as an intrinsic hint the same way.
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
		// An absolutely positioned box is not a flex/grid item: it must not take space in its
		// container nor be moved by it, or the offsets computed below are overwritten by the
		// container's own placement on the very next layout pass. Tell the layout to skip it.
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

		// CSS over-constrained resolution: when the size is `auto` and both offsets on an
		// axis are given, the size stretches to fill the gap between them; when all three
		// (both offsets + explicit size) are set, the end offset (right/bottom) is ignored,
		// which the position math below already does by preferring left/top
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
		// Back in flow if the rule that took it out is gone - but only if it was OUR rule. An
		// overlay an application put out of the flow in code carries styleManaged == false and must
		// survive a style pass that matched nothing (see OutOfFlowComponent).
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

	/* THE COMPUTED DIRECTION OF THIS NODE, and the only place in the engine that turns an
	inline-axis declaration into a physical side.

	It has to be here because it is the only place that knows BOTH: the parser saw a declaration
	and no node, and the layout backend sees a node and no declaration. Note what this does NOT
	do - it never touches the physical properties. `padding-left` under `direction: rtl` is still
	the left edge, exactly as on the web; what follows the direction is the inline axis (in the
	backends) and the `*-inline-*` properties (here). */
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
		// The inline pair AFTER the physical sides, so a sheet declaring both gets the logical
		// one. CSS would decide that by source order; StyleList records none, and a cross-name
		// order index would cost the whole cascade a field. Documented in the css-engine skill.
		if (auto m = s.paddingInlineStart();
				s.has(ParameterName::CssPaddingInlineStart) && !m.isAuto()) {
			(rtl ? pad.right : pad.left) = computeMetric(m, ownSize.width);
		}
		if (auto m = s.paddingInlineEnd();
				s.has(ParameterName::CssPaddingInlineEnd) && !m.isAuto()) {
			(rtl ? pad.left : pad.right) = computeMetric(m, ownSize.width);
		}
	};
	// Map the four CSS border sides onto the edges the table's collapse pass resolves, touching only
	// the sides the sheet actually declares. Every other item mapping is guarded the same way, and
	// for the same reason: applyLayout runs for EVERY node on every style pass, so an unguarded
	// write would erase whatever an application set in code.
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

	// Map the CSS margin-* onto an item Margin (percent against parent width). `auto` is not a
	// length: it is recorded in the mask instead and resolved from the free space by the flex
	// engine (see FlexAutoMargin). `autoMask` is null for containers that cannot honour it.
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
		// See fillPadding: the inline pair resolves last and wins over the physical one.
		side(ParameterName::CssMarginInlineStart, s.marginInlineStart(),
				rtl ? mrg.right : mrg.left, rtl ? FlexAutoMargin::Right : FlexAutoMargin::Left);
		side(ParameterName::CssMarginInlineEnd, s.marginInlineEnd(), rtl ? mrg.left : mrg.right,
				rtl ? FlexAutoMargin::Left : FlexAutoMargin::Right);
	};

	// A system on this node already owns its children's geometry (SystemManagedLayout): a
	// stylesheet must neither add a second writer of it, nor reshape the layout that system built
	// for itself. Pretending the node asked for no container at all, with no layout to tear down,
	// disables the whole container block below. The per-item mapping after it still runs - this
	// node remains an item of whatever lays IT out.
	const bool systemManaged = node->getComponent<SystemManagedLayout>() != nullptr;

	const bool wantFlex =
			!systemManaged && (display == Display::Flex || display == Display::InlineFlex);
	const bool wantGrid =
			!systemManaged && (display == Display::Grid || display == Display::InlineGrid);
	const bool wantRow = !systemManaged && display == Display::TableRow;

	// `display: table` is the one container mapping that survives SystemManagedLayout. The reason is
	// ui::TableView: it owns its children's geometry (header + scroll), so it must carry that
	// marker, yet the column track list it lays its rows out with is CSS. Writing the parameter
	// component under the marker is safe - a component is not a second writer of anything; only a
	// SYSTEM would be, and none is added below. applyDefault already writes MeasureComponent under
	// the same marker on the same reasoning.
	const bool wantTable = display == Display::Table;

	// `overflow-x` / `overflow-y`: record the resolved pair and add or drop the ScrollSystem that
	// acts on it. Deliberately NOT gated on systemManaged - a dock or a TableView that asks for
	// clipping should get it. ScrollSystem writes no ContentSize of its own; without a LayoutSystem
	// to read an extent from it simply clips, which is the right answer for a widget that scrolls
	// itself.
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
			/* The axes stay as they were declared, and CSS's own "a visible axis computes to auto"
			rule is deliberately NOT applied here.

			That rule exists on the web because a clip is a box. It used to be enforced here for a
			harder reason - the only clip the engine had was an axis-aligned scissor RECT - and it
			cost more than it bought: an `overflow-y: auto` document also overflowed horizontally,
			and a flex container sized by its content on the overflowing axis then took the width of
			its widest unwrapped line, so nothing in it ever wrapped.

			A scissor is still one rectangle. It is now built per axis (ui::ScissorAxes): the axis
			nobody asked to clip is opened past any surface, leaving whatever an ancestor scissor
			imposed. So one axis really can scroll while the other flows. */
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
			// the table's own border is the outside participant of the collapse pass. Reuse the
			// cell mapping through a scratch TableCellInfo: the four edges are the same shape and
			// need the same "only what the sheet declares" guard.
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
		// only tear down layouts that WE added (marker present)
		if (layout && node->getComponent<StyleManagedLayout>()) {
			node->removeSystem(layout);
			node->removeComponent<FlexLayoutInfo>();
			node->removeComponent<GridLayoutInfo>();
			node->removeComponent<TableLayoutInfo>();
			// NOT TableColumnsComponent / TableBordersComponent: those are OUTPUTS, owned by the
			// table pass or by the widget that owns the rows. The style never wrote them and must
			// not delete them, or a virtualized row would lose the geometry it lays out with.
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

				/* The two axes as they stand AFTER `flex-direction` and `flex-wrap` are decided,
				which is why those two are assigned above rather than below: a container whose
				direction was set in code and whose `justify-content: left` comes from the sheet
				has to resolve that keyword against the direction actually in force. */
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
	// Table first, and keyed on the parent's TableColumnsComponent rather than on TableRowInfo: that
	// component is what a row carries in BOTH the static case (the table pass stamped it) and the
	// virtualized one (ui::TableView stamped it), so one branch covers a table that exists as a node
	// and one that does not.
	if (parent->getComponent<TableColumnsComponent>()) { // this node is a CELL
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
			// `vertical-align` is the table-cell spelling of the cross-axis alignment; it is also an
			// inherited text property, so only map it when this node declares one.
			if (s.has(ParameterName::CssVerticalAlign)) {
				switch (s.verticalAlign()) {
				case document::VerticalAlign::Top: next.alignSelf = GridAlign::Start; break;
				case document::VerticalAlign::Bottom: next.alignSelf = GridAlign::End; break;
				case document::VerticalAlign::Middle: next.alignSelf = GridAlign::Center; break;
				default: break; // baseline / sub / super have no table meaning here
				}
			}
			// Only touch a side the sheet actually declares. Writing all four unconditionally would
			// erase borders an application set in code on every style pass - and a style pass runs
			// for every node, including ones no rule matches.
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
	} else if (parent->getComponent<TableLayoutInfo>()) { // this node is a ROW
		node->setOrUpdateComponent<TableRowInfo>([&](NotNull<TableRowInfo> info) {
			TableRowInfo next = *info;
			// Guarded, like every other item mapping: a row whose sheet says nothing about height
			// keeps whatever it has, rather than being reset to Auto on every style pass.
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
			// min-/max-width/height on the MAIN axis map onto the item's own clamps, which the flex
			// algorithm already honours for both the base size and the flexed size. The cross axis
			// has no equivalent in FlexItemInfo, so those two are still ignored - which is why the
			// CSS reference tells you to size a flex item with basis/grow/shrink rather than
			// min-/max- on the cross axis.
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
				/* `align-self` sits on the CONTAINER's cross axis, so the axis has to come from
				the parent - this mapping runs on the item. A parent with no FlexLayoutInfo yet
				(the style pass reaches children in tree order, and a container written in code
				may be configured later) falls back to the default row, which is what the item
				would have got before this change. */
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
