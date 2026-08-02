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
#include "XLUiStyleSystem.h"
#include "XLUiLayoutSystem.h"
#include "XLUiInteractiveComponent.h"
#include "XLInheritedStyle.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

ComponentId StyleManagedLayout::Id;

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

ResolvedStyle StyleResolver::resolveStyleForNode(NotNull<Node> node) {
	ResolvedStyle ret;

	// ancestor chain, node first
	Vector<Node *> chain;
	for (Node *p = node.get(); p != nullptr; p = p->getParent()) { chain.emplace_back(p); }

	// ancestor Bloom prefix: ancestorBitsFrom[i] = OR of identity tokens over chain[i..root].
	// A rule targeting chain[L] tests its ancestors chain[L+1..], i.e. ancestorBitsFrom[L+1].
	Vector<uint64_t> ancestorBitsFrom;
	ancestorBitsFrom.resize(chain.size() + 1, 0);
	for (size_t i = chain.size(); i-- > 0;) {
		ancestorBitsFrom[i] =
				ancestorBitsFrom[i + 1] | foldIdentityBits(chain[i]->getComponent<NodeIdentity>());
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

	// Resolve one cascade level: gather every matching rule (simple + combinator/pseudo) from
	// every scope visible at `chainIndex`, across sheets, into one list; sort by CSS specificity
	// (ties broken by scope rank + source order); then merge in that order so the most specific
	// / latest declaration wins. `inherit` restricts an ancestor level to inheritable params.
	auto resolveLevel = [&](document::StyleList &dst, Node *levelNode, size_t chainIndex,
								bool inherit) {
		Vector<document::StyleContainer::MatchedRule> matches;
		uint64_t rank = 0; // outer sheets get a lower rank -> lose ties to nearer sheets
		for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
			if (it->chainIndex >= chainIndex) {
				it->system->getStyleSheet()->collectMatches(matches, levelNode,
						ancestorBitsFrom[chainIndex + 1], rank << 32, it->media);
				++rank;
			}
		}
		document::StyleContainer::sortMatchedRules(matches);
		for (auto &m : matches) { dst.merge(*m.style, m.media, inherit); }
	};

	// Build ONLY the raw merged parameter list plus the interpretation context here.
	// Nothing is compiled or extracted - each consumer reads what it needs from the
	// returned ResolvedStyle. The pool is owned by `ret` and freed with it.
	ret._pool = memory::pool::create(static_cast<memory::pool_t *>(nullptr));
	ret._media = &nearest.system->getMediaParameters();
	memory::perform([&] {
		auto style = new (ret._pool) document::StyleList();

		// inheritable parameters cascade from the outermost styled ancestor down
		for (size_t i = scopes.back().chainIndex; i >= 1; --i) {
			resolveLevel(*style, chain[i], i, true);
		}

		// the node's own matches (full, specificity-sorted) override inherited values
		resolveLevel(*style, node.get(), 0, false);

		ret._style = style;
	}, ret._pool);

	// note: string parameters (font-family, background-image, grid tracks) resolve against
	// the NEAREST sheet's string table; with multiple sheets in scope, string values defined
	// by outer sheets may resolve incorrectly - documented v1 limitation
	ret._iface = document::SimpleStyleInterface(nearest.media,
			nearest.system->getStyleSheet()->getStrings(), 1.0f, ret._media->fontScale);

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
: _valid(o._valid), _pool(o._pool), _style(o._style), _iface(o._iface), _media(o._media) {
	o._valid = false;
	o._pool = nullptr;
	o._style = nullptr;
}

ResolvedStyle &ResolvedStyle::operator=(ResolvedStyle &&o) noexcept {
	if (this != &o) {
		if (_pool) {
			memory::pool::destroy(_pool);
		}
		_valid = o._valid;
		_pool = o._pool;
		_style = o._style;
		_iface = o._iface;
		_media = o._media;
		o._valid = false;
		o._pool = nullptr;
		o._style = nullptr;
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

document::Metric ResolvedStyle::width() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssWidth, v) ? v.sizeValue : document::Metric();
}

document::Metric ResolvedStyle::height() const {
	document::StyleValue v;
	return getValue(document::ParameterName::CssHeight, v) ? v.sizeValue : document::Metric();
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
	// changed, or - for percent-styled nodes carrying NodeEventFlags::HandleParentContentSize -
	// an ancestor resized. Re-resolve unless the style is still fresh (same version, resolved
	// against the same parent/own sizes); equality-guarded writes make re-resolution converge.
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
				|| mask.count(StyleSystemState::Id.value) != 0) {
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

	if (currentInteractiveMask == _interactiveMask && currentSourceId == _sourceSystemId
			&& currentSourceVersion == _sourceSystemVersion) {
		return; // no changes to run style resolver
	}

	// the stylesheet source (id) or its version changed - i.e. the CSS was (re)loaded, not merely a
	// local interactive-state flip; the whole subtree's resolved styles are now potentially stale
	const bool sourceChanged =
			currentSourceId != _sourceSystemId || currentSourceVersion != _sourceSystemVersion;

	_nodesUpdated.clear();

	_interactiveMask = currentInteractiveMask;
	_sourceSystemId = currentSourceId;
	_sourceSystemVersion = currentSourceVersion;

	// resolve the owner's own style. A recursive resolver is pushed onto the frame stack only AFTER
	// its owner's phases, so the owner never delivers its events back to its own resolver - the owner
	// must be resolved directly here (descendants cascade via their frame-stack child events).
	resolveForNode(_owner);

	if (_recursive && sourceChanged) {
		// nudge every descendant so it re-fires its child event and gets re-resolved this frame;
		// resolution is deduped per version via _nodesUpdated, so each descendant runs once
		markSubtreeComponentsDirty(_owner);
	}
}

namespace {

using document::Align;

// document flex/grid enums -> simpleui layout enums

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

// justify-content -> flex main-axis distribution
static FlexJustify toFlexJustify(Align a) {
	switch (a) {
	case Align::End:
	case Align::FlexEnd:
	case Align::Right: return FlexJustify::FlexEnd;
	case Align::Center: return FlexJustify::Center;
	case Align::SpaceBetween: return FlexJustify::SpaceBetween;
	case Align::SpaceAround: return FlexJustify::SpaceAround;
	case Align::SpaceEvenly: return FlexJustify::SpaceEvenly;
	default: return FlexJustify::FlexStart;
	}
}

// align-items / align-content; Normal/Auto/baseline fall back to Stretch
static FlexAlign toFlexAlignItems(Align a) {
	switch (a) {
	case Align::Start:
	case Align::FlexStart:
	case Align::SelfStart:
	case Align::Left: return FlexAlign::FlexStart;
	case Align::End:
	case Align::FlexEnd:
	case Align::SelfEnd:
	case Align::Right: return FlexAlign::FlexEnd;
	case Align::Center: return FlexAlign::Center;
	case Align::SpaceBetween: return FlexAlign::SpaceBetween;
	case Align::SpaceAround: return FlexAlign::SpaceAround;
	default: return FlexAlign::Stretch;
	}
}

// align-self; Auto/Normal inherit the container's align-items
static FlexAlign toFlexAlignSelf(Align a) {
	if (a == Align::Auto || a == Align::Normal) {
		return FlexAlign::Auto;
	}
	return toFlexAlignItems(a);
}

// grid justify/align (content or items)
static GridAlign toGridAlign(Align a) {
	switch (a) {
	case Align::Start:
	case Align::FlexStart:
	case Align::SelfStart:
	case Align::Left: return GridAlign::Start;
	case Align::End:
	case Align::FlexEnd:
	case Align::SelfEnd:
	case Align::Right: return GridAlign::End;
	case Align::Center: return GridAlign::Center;
	case Align::Stretch: return GridAlign::Stretch;
	case Align::SpaceBetween: return GridAlign::SpaceBetween;
	case Align::SpaceAround: return GridAlign::SpaceAround;
	case Align::SpaceEvenly: return GridAlign::SpaceEvenly;
	default: return GridAlign::Stretch;
	}
}

// grid justify-self / align-self; Auto/Normal inherit the container's items value
static GridAlign toGridAlignSelf(Align a) {
	if (a == Align::Auto || a == Align::Normal) {
		return GridAlign::Auto;
	}
	return toGridAlign(a);
}

} // namespace

// the sizes the style is applied against: percent metrics resolve vs the parent size,
// paddings/gaps vs the own size - together they form the style's freshness key
static Pair<Size2, Size2> makeStyleSizeKey(Node *node) {
	auto p = node->getParent();
	return pair(p ? p->getContentSize() : Size2::ZERO, node->getContentSize());
}

// does the resolved style contain a parent-size-relative (Percent) metric? Such nodes must
// re-resolve when an ancestor resizes - they opt into NodeEventFlags::HandleParentContentSize
static bool hasParentRelativeMetrics(const ResolvedStyle &s) {
	using document::ParameterName;

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
	return it != _nodesUpdated.end() && it->second == makeStyleSizeKey(node);
}

void StyleResolver::resolveOwnerIfStale() {
	if (_owner && !isNodeFresh(_owner)) {
		resolveForNode(_owner);
	}
}

void StyleResolver::resolveForNode(Node *node) {
	auto style = resolveStyleForNode(node);
	if (!style.valid()) {
		return;
	}

	// pre-mark to guard against re-entry while applying; the final key is recorded below
	// (applyDefault may change the node's own content size)
	auto it = _nodesUpdated.find(node);
	if (it == _nodesUpdated.end()) {
		it = _nodesUpdated.emplace(node, makeStyleSizeKey(node)).first;
	}

	// a node whose style depends on the parent size must re-run its content-size phase when an
	// ancestor resizes, so its frame-stack event reaches this resolver again (the bit is never
	// cleared - a spurious phase re-run is cut off by the freshness check)
	if (hasParentRelativeMetrics(style)
			&& !hasFlag(node->getEventFlags(), NodeEventFlags::HandleParentContentSize)) {
		node->setEventFlags(node->getEventFlags() | NodeEventFlags::HandleParentContentSize);
	}

	if (_callback && _callback(node, style)) {
		it->second = makeStyleSizeKey(node);
		return;
	}

	applyDefault(node, style);

	// re-find: applyDefault may have resolved other nodes and rehashed the map
	if (auto fit = _nodesUpdated.find(node); fit != _nodesUpdated.end()) {
		fit->second = makeStyleSizeKey(node);
	}
}

void StyleResolver::applyTypeAttributes(Node *node, const ResolvedStyle &s,
		sprt::bitset<toInt(document::ParameterName::Max)> &handled) {
	auto &reg = getTypeApplierRegistry();
	auto it = reg.find(node->getType());
	if (it == reg.end()) {
		return;
	}

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
		auto parent = node->getParent();
		const bool parentManagesSize = parent
				&& (parent->getComponent<FlexLayoutInfo>()
						|| parent->getComponent<GridLayoutInfo>());

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
	};
	// map the CSS margin-* onto an item Margin (percent against parent width)
	auto fillMargin = [&](Padding &mrg) {
		if (auto m = s.marginTop(); s.has(ParameterName::CssMarginTop) && !m.isAuto()) {
			mrg.top = computeMetric(m, parentSize.width);
		}
		if (auto m = s.marginRight(); s.has(ParameterName::CssMarginRight) && !m.isAuto()) {
			mrg.right = computeMetric(m, parentSize.width);
		}
		if (auto m = s.marginBottom(); s.has(ParameterName::CssMarginBottom) && !m.isAuto()) {
			mrg.bottom = computeMetric(m, parentSize.width);
		}
		if (auto m = s.marginLeft(); s.has(ParameterName::CssMarginLeft) && !m.isAuto()) {
			mrg.left = computeMetric(m, parentSize.width);
		}
	};

	const bool wantFlex = display == Display::Flex || display == Display::InlineFlex;
	const bool wantGrid = display == Display::Grid || display == Display::InlineGrid;

	auto layout = node->getSystemByType<LayoutSystem>();

	if (!wantFlex && !wantGrid) {
		// only tear down layouts that WE added (marker present)
		if (layout && node->getComponent<StyleManagedLayout>()) {
			node->removeSystem(layout);
			node->removeComponent<FlexLayoutInfo>();
			node->removeComponent<GridLayoutInfo>();
			node->removeComponent<StyleManagedLayout>();
		}
	} else {
		const LayoutMode mode = wantGrid ? LayoutMode::Grid : LayoutMode::Flex;
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
				if (s.has(ParameterName::CssJustifyContent)) {
					next.justifyContent = toFlexJustify(s.justifyContent());
				}
				if (s.has(ParameterName::CssAlignItems)) {
					next.alignItems = toFlexAlignItems(s.alignItems());
				}
				if (s.has(ParameterName::CssAlignContent)) {
					next.alignContent = toFlexAlignItems(s.alignContent());
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
					next.justifyContent = toGridAlign(s.justifyContent());
				}
				if (s.has(ParameterName::CssAlignContent)) {
					next.alignContent = toGridAlign(s.alignContent());
				}
				if (s.has(ParameterName::CssJustifyItems)) {
					next.justifyItems = toGridAlign(s.justifyItems());
				}
				if (s.has(ParameterName::CssAlignItems)) {
					next.alignItems = toGridAlign(s.alignItems());
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
	if (parent->getComponent<GridLayoutInfo>()) {
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
				next.justifySelf = toGridAlignSelf(s.justifySelf());
			}
			if (s.has(ParameterName::CssAlignSelf)) {
				next.alignSelf = toGridAlignSelf(s.alignSelf());
			}
			if (s.has(ParameterName::CssOrder)) {
				next.order = s.order();
			}
			fillMargin(next.margin);
			if (next != *info) {
				*info = next;
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
			if (s.has(ParameterName::CssAlignSelf)) {
				next.alignSelf = toFlexAlignSelf(s.alignSelf());
			}
			if (s.has(ParameterName::CssOrder)) {
				next.order = s.order();
			}
			fillMargin(next.margin);
			if (next != *info) {
				*info = next;
				return true;
			}
			return false;
		});
	}
}

} // namespace stappler::xenolith::ui
