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

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

ComponentId StyleManagedLayout::Id;

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
		return _iface.resolveString(v.stringId).str<memory::StandartInterface>();
	}
	return String();
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

	auto flags = SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents
			| SystemFlags::HandleComponents | SystemFlags::HandleAncestorComponents;
	if (_recursive) {
		flags |= SystemFlags::HandleChildNodeEvents;
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
	apply();
}

void StyleResolver::handleComponentsDirty() {
	apply(); //
}

void StyleResolver::handleReorderChildDirty() {
	// TODO: impplement actual recursion
	for (auto &it : _owner->getChildren()) { resolveForNode(it); }
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

	_nodesUpdated.clear();

	_interactiveMask = currentInteractiveMask;
	_sourceSystemId = currentSourceId;
	_sourceSystemVersion = currentSourceVersion;

	resolveForNode(_owner);
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

void StyleResolver::resolveForNode(Node *node) {
	auto style = resolveStyleForNode(node);
	if (!style.valid()) {
		return;
	}

	_nodesUpdated.emplace(_owner);

	if (_callback && _callback(node, style)) {
		return;
	}

	applyDefault(node, style);
}

void StyleResolver::applyDefault(Node *node, const ResolvedStyle &s) {
	using document::ParameterName;

	Size2 parentSize;
	if (auto p = node->getParent()) {
		parentSize = p->getContentSize();
	}
	// compile the views this applier reads broadly; scalars/strings are read on demand
	const auto font = s.font();
	const auto text = s.text();
	const auto block = s.block();
	const float fontSize = float(font.fontSize.get());

	auto computeMetric = [&](const document::Metric &m, float base) {
		return s.media().computeValueAuto(m, base, fontSize);
	};

	if (s.has(ParameterName::CssOpacity)) {
		node->setOpacity(float(text.opacity) / 255.0f);
	}

	auto label = dynamic_cast<Label *>(node);
	if (label) {
		if (s.has(ParameterName::CssColor)) {
			label->setColor(Color4F(Color4B(text.color, 255)), false);
		}
		if (s.has(ParameterName::CssFontSize) || s.has(ParameterName::CssFontSizeNumeric)) {
			label->setFontSize(font.fontSize);
		}
		if (s.has(ParameterName::CssFontFamily) && !font.fontFamily.empty()) {
			label->setFontFamily(font.fontFamily);
		}
		if (s.has(ParameterName::CssFontWeight)) {
			label->setFontWeight(font.fontWeight);
		}
		if (s.has(ParameterName::CssFontStyle)) {
			label->setFontStyle(font.fontStyle);
		}
		if (s.has(ParameterName::CssFontStretch)) {
			label->setFontStretch(font.fontStretch);
		}
		if (s.has(ParameterName::CssTextAlign)) {
			label->setAlignment(s.paragraph().textAlign);
		}
		if (s.has(ParameterName::CssWidth) && !block.width.isAuto()
				&& block.width.metric != document::Metric::Units::FitContent) {
			label->setWidth(computeMetric(block.width, parentSize.width));
		}
	} else {
		if (s.has(ParameterName::CssBackgroundColor)) {
			// Layer covers Button as well
			document::StyleValue styleColor;
			if (s.getValue(ParameterName::CssBackgroundColor, styleColor)) {


				node->setColor(Color4F(styleColor.color4), true);
			}
		}

		auto size = node->getContentSize();
		bool sizeDirty = false;
		// fit-content never writes a static size: it resolves through the
		// flex item mapping in applyLayout (basis / crossSize) instead
		if (s.has(ParameterName::CssWidth) && !block.width.isAuto()
				&& block.width.metric != document::Metric::Units::FitContent) {
			size.width = computeMetric(block.width, parentSize.width);
			sizeDirty = true;
		}
		if (s.has(ParameterName::CssHeight) && !block.height.isAuto()
				&& block.height.metric != document::Metric::Units::FitContent) {
			size.height = computeMetric(block.height, parentSize.height);
			sizeDirty = true;
		}
		if (sizeDirty) {
			node->setContentSize(size);
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
		const bool widthAuto = !s.has(ParameterName::CssWidth) || block.width.isAuto();
		const bool heightAuto = !s.has(ParameterName::CssHeight) || block.height.isAuto();

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
	// compile the block model once; flex/grid scalars are read on demand below
	const auto block = s.block();
	const float fontSize = float(s.font().fontSize.get());
	auto computeMetric = [&](const document::Metric &m, float base) {
		return s.media().computeValueAuto(m, base, fontSize);
	};

	// map the CSS padding-* onto a container Padding (percent against own width)
	auto fillPadding = [&](Padding &pad) {
		if (s.has(ParameterName::CssPaddingTop) && !block.paddingTop.isAuto()) {
			pad.top = computeMetric(block.paddingTop, ownSize.width);
		}
		if (s.has(ParameterName::CssPaddingRight) && !block.paddingRight.isAuto()) {
			pad.right = computeMetric(block.paddingRight, ownSize.width);
		}
		if (s.has(ParameterName::CssPaddingBottom) && !block.paddingBottom.isAuto()) {
			pad.bottom = computeMetric(block.paddingBottom, ownSize.width);
		}
		if (s.has(ParameterName::CssPaddingLeft) && !block.paddingLeft.isAuto()) {
			pad.left = computeMetric(block.paddingLeft, ownSize.width);
		}
	};
	// map the CSS margin-* onto an item Margin (percent against parent width)
	auto fillMargin = [&](Padding &m) {
		if (s.has(ParameterName::CssMarginTop) && !block.marginTop.isAuto()) {
			m.top = computeMetric(block.marginTop, parentSize.width);
		}
		if (s.has(ParameterName::CssMarginRight) && !block.marginRight.isAuto()) {
			m.right = computeMetric(block.marginRight, parentSize.width);
		}
		if (s.has(ParameterName::CssMarginBottom) && !block.marginBottom.isAuto()) {
			m.bottom = computeMetric(block.marginBottom, parentSize.width);
		}
		if (s.has(ParameterName::CssMarginLeft) && !block.marginLeft.isAuto()) {
			m.left = computeMetric(block.marginLeft, parentSize.width);
		}
	};

	const bool wantFlex = block.display == Display::Flex || block.display == Display::InlineFlex;
	const bool wantGrid = block.display == Display::Grid || block.display == Display::InlineGrid;

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
					&& block.width.metric == document::Metric::Units::FitContent;
			const bool heightFit = s.has(ParameterName::CssHeight)
					&& block.height.metric == document::Metric::Units::FitContent;
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
