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

#include "XLSimpleStyle.h"
#include "XLSimpleLayoutSystem.h"
#include "XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::simpleui {

ComponentId StyleSheetState::Id;

ComponentId StyleManagedLayout::Id;

bool StyleSheetSystem::init() {
	if (!System::init()) {
		return false;
	}
	setSystemFlags(SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents);
	return true;
}

bool StyleSheetSystem::init(Rc<StyleSheet> &&sheet) {
	if (!init()) {
		return false;
	}
	_sheet = move(sheet);
	return true;
}

bool StyleSheetSystem::init(StringView css) {
	if (!init()) {
		return false;
	}

	if (auto sheet = Rc<StyleSheet>::create(css)) {
		_sheet = move(sheet);
		return true;
	}

	return true;
}

bool StyleSheetSystem::init(const FileInfo &file) {
	if (!init()) {
		return false;
	}

	if (auto sheet = Rc<StyleSheet>::create(file)) {
		_sheet = move(sheet);
		return true;
	}

	return true;
}

void StyleSheetSystem::setStyleSheet(Rc<StyleSheet> &&sheet) {
	_sheet = move(sheet);
	_resolvedForVersion = maxOf<uint32_t>();
	invalidateStyles();
}

void StyleSheetSystem::setMediaParameters(const document::MediaParameters &media) {
	_media = media;
	_mediaExplicit = true;
	_resolvedForVersion = maxOf<uint32_t>();
	invalidateStyles();
}

SpanView<bool> StyleSheetSystem::getMediaResolved() {
	if (_sheet && _resolvedForVersion != _sheet->getVersion()) {
		_mediaResolved = _sheet->resolveMedia(_media);
		_resolvedForVersion = _sheet->getVersion();
	}
	return _mediaResolved;
}

void StyleSheetSystem::invalidateStyles() {
	if (_owner) {
		_owner->setOrUpdateComponent<StyleSheetState>([&](NotNull<StyleSheetState> state) {
			++state->version;
			return true;
		});
	}
}

void StyleSheetSystem::handleAdded(Node *owner) {
	System::handleAdded(owner);
	owner->setComponent<StyleSheetState>();
}

void StyleSheetSystem::handleRemoved() {
	if (_owner) {
		_owner->removeComponent<StyleSheetState>();
	}
	System::handleRemoved();
}

void StyleSheetSystem::handleEnter(Scene *scene) {
	System::handleEnter(scene);
	updateMedia();
}

void StyleSheetSystem::updateMedia() {
	if (_mediaExplicit || !_owner) {
		return;
	}

	if (auto dir = _owner->getDirector()) {
		auto &constraints = dir->getFrameConstraints();
		auto screen = constraints.getScreenSize();
		if (constraints.density > 0.0f) {
			_media.density = constraints.density;
			_media.dpi = int(92.0f * constraints.density);
			_media.surfaceSize = Size2(float(screen.width) / constraints.density,
					float(screen.height) / constraints.density);
			_resolvedForVersion = maxOf<uint32_t>();
		}
	}
}

bool ResolvedStyle::has(document::ParameterName name) const {
	for (auto &it : present) {
		if (it == toInt(name)) {
			return true;
		}
	}
	return false;
}

// parameters the v1 applier may consume; presence is recorded for each
static constexpr document::ParameterName s_trackedParameters[] = {
	document::ParameterName::CssOpacity,
	document::ParameterName::CssColor,
	document::ParameterName::CssBackgroundColor,
	document::ParameterName::CssFontSize,
	document::ParameterName::CssFontSizeNumeric,
	document::ParameterName::CssFontFamily,
	document::ParameterName::CssFontWeight,
	document::ParameterName::CssFontStyle,
	document::ParameterName::CssFontStretch,
	document::ParameterName::CssTextAlign,
	document::ParameterName::CssWidth,
	document::ParameterName::CssHeight,
	document::ParameterName::CssMarginTop,
	document::ParameterName::CssMarginRight,
	document::ParameterName::CssMarginBottom,
	document::ParameterName::CssMarginLeft,
	document::ParameterName::CssPaddingTop,
	document::ParameterName::CssPaddingRight,
	document::ParameterName::CssPaddingBottom,
	document::ParameterName::CssPaddingLeft,
	document::ParameterName::CssPosition,
	document::ParameterName::CssTop,
	document::ParameterName::CssRight,
	document::ParameterName::CssBottom,
	document::ParameterName::CssLeft,
	document::ParameterName::CssXlAnchorPointX,
	document::ParameterName::CssXlAnchorPointY,
	document::ParameterName::CssXlPositionX,
	document::ParameterName::CssXlPositionY,
	// flexbox / grid (drive the LayoutSystem)
	document::ParameterName::CssDisplay,
	document::ParameterName::CssFlexDirection,
	document::ParameterName::CssFlexWrap,
	document::ParameterName::CssOrder,
	document::ParameterName::CssFlexGrow,
	document::ParameterName::CssFlexShrink,
	document::ParameterName::CssFlexBasis,
	document::ParameterName::CssJustifyContent,
	document::ParameterName::CssAlignContent,
	document::ParameterName::CssJustifyItems,
	document::ParameterName::CssAlignItems,
	document::ParameterName::CssJustifySelf,
	document::ParameterName::CssAlignSelf,
	document::ParameterName::CssRowGap,
	document::ParameterName::CssColumnGap,
	document::ParameterName::CssGridAutoFlow,
	document::ParameterName::CssGridTemplateColumns,
	document::ParameterName::CssGridTemplateRows,
	document::ParameterName::CssGridAutoColumns,
	document::ParameterName::CssGridAutoRows,
	document::ParameterName::CssGridColumnStart,
	document::ParameterName::CssGridColumnEnd,
	document::ParameterName::CssGridRowStart,
	document::ParameterName::CssGridRowEnd,
};

namespace {

struct StyleScope {
	Node *owner = nullptr;
	StyleSheetSystem *system = nullptr;
	size_t chainIndex = 0; // index in the node..root chain
	SpanView<bool> media;
};

} // namespace

ResolvedStyle resolveStyleForNode(NotNull<Node> node) {
	ResolvedStyle ret;

	// ancestor chain, node first
	Vector<Node *> chain;
	for (Node *p = node.get(); p != nullptr; p = p->getParent()) { chain.emplace_back(p); }

	// stylesheet scopes on the chain, nearest first
	Vector<StyleScope> scopes;
	for (size_t i = 0; i < chain.size(); ++i) {
		if (chain[i]->getComponent<StyleSheetState>()) {
			if (auto sys = chain[i]->getSystemByType<StyleSheetSystem>()) {
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
	ret.media = nearest.system->getMediaParameters();

	// match identity against every scope visible from `chainIndex`, outer sheets
	// first so nearer sheets override
	auto resolveLevel = [&](document::StyleList &target, const NodeIdentity *identity,
								size_t chainIndex) {
		for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
			if (it->chainIndex >= chainIndex) {
				it->system->getStyleSheet()->resolveForIdentity(target, identity->type,
						identity->name, identity->classes, it->media);
			}
		}
	};

	auto pool = memory::pool::create(static_cast<memory::pool_t *>(nullptr));
	memory::perform([&] {
		document::StyleList style;

		// inheritable parameters cascade from the outermost styled ancestor down
		for (size_t i = scopes.back().chainIndex; i >= 1; --i) {
			if (auto identity = chain[i]->getComponent<NodeIdentity>()) {
				document::StyleList tmp;
				resolveLevel(tmp, identity, i);
				style.merge(tmp, true);
			}
		}

		// the node's own matches override inherited values
		if (auto identity = node->getComponent<NodeIdentity>()) {
			resolveLevel(style, identity, 0);
		}

		// note: string parameters (font-family, background-image) resolve against the
		// NEAREST sheet's string table; with multiple sheets in scope, string values
		// defined by outer sheets may resolve incorrectly - documented v1 limitation
		document::SimpleStyleInterface iface(nearest.media,
				nearest.system->getStyleSheet()->getStrings(), 1.0f, ret.media.fontScale);

		ret.font = style.compileFontStyle(&iface);
		ret.fontFamily = ret.font.fontFamily.str<memory::StandartInterface>();
		ret.font.fontFamily = StringView();
		ret.text = style.compileTextLayout(&iface);
		ret.paragraph = style.compileParagraphLayout(&iface);
		ret.block = style.compileBlockModel(&iface);
		ret.background = style.compileBackground(&iface);
		ret.backgroundImage = ret.background.backgroundImage.str<memory::StandartInterface>();
		ret.background.backgroundImage = StringView();

		// positioning parameters are not part of any compiled document structure;
		// pull them directly (last matching value wins, media-filtered)
		using document::ParameterName;
		auto getValue = [&](ParameterName name, document::StyleValue &out) -> bool {
			auto v = style.get(name, &iface);
			if (v.empty()) {
				return false;
			}
			out = v.back().value;
			return true;
		};
		document::StyleValue sv;
		if (getValue(ParameterName::CssPosition, sv)) {
			ret.position = sv.position;
		}
		if (getValue(ParameterName::CssTop, sv)) {
			ret.top = sv.sizeValue;
		}
		if (getValue(ParameterName::CssRight, sv)) {
			ret.right = sv.sizeValue;
		}
		if (getValue(ParameterName::CssBottom, sv)) {
			ret.bottom = sv.sizeValue;
		}
		if (getValue(ParameterName::CssLeft, sv)) {
			ret.left = sv.sizeValue;
		}
		if (getValue(ParameterName::CssXlAnchorPointX, sv)) {
			ret.anchorPoint.x = sv.floatValue;
		}
		if (getValue(ParameterName::CssXlAnchorPointY, sv)) {
			ret.anchorPoint.y = sv.floatValue;
		}
		if (getValue(ParameterName::CssXlPositionX, sv)) {
			ret.xlPositionX = sv.sizeValue;
		}
		if (getValue(ParameterName::CssXlPositionY, sv)) {
			ret.xlPositionY = sv.sizeValue;
		}

		// flexbox / grid parameters (also not part of any compiled struct)
		if (getValue(ParameterName::CssFlexDirection, sv)) {
			ret.flexDirection = sv.flexDirection;
		}
		if (getValue(ParameterName::CssFlexWrap, sv)) {
			ret.flexWrap = sv.flexWrap;
		}
		if (getValue(ParameterName::CssGridAutoFlow, sv)) {
			ret.gridAutoFlow = sv.gridAutoFlow;
		}
		if (getValue(ParameterName::CssJustifyContent, sv)) {
			ret.justifyContent = sv.align;
		}
		if (getValue(ParameterName::CssAlignContent, sv)) {
			ret.alignContent = sv.align;
		}
		if (getValue(ParameterName::CssJustifyItems, sv)) {
			ret.justifyItems = sv.align;
		}
		if (getValue(ParameterName::CssAlignItems, sv)) {
			ret.alignItems = sv.align;
		}
		if (getValue(ParameterName::CssJustifySelf, sv)) {
			ret.justifySelf = sv.align;
		}
		if (getValue(ParameterName::CssAlignSelf, sv)) {
			ret.alignSelf = sv.align;
		}
		if (getValue(ParameterName::CssFlexGrow, sv)) {
			ret.flexGrow = sv.floatValue;
		}
		if (getValue(ParameterName::CssFlexShrink, sv)) {
			ret.flexShrink = sv.floatValue;
		}
		if (getValue(ParameterName::CssFlexBasis, sv)) {
			ret.flexBasis = sv.sizeValue;
		}
		if (getValue(ParameterName::CssOrder, sv)) {
			ret.order = sv.intValue;
		}
		if (getValue(ParameterName::CssRowGap, sv)) {
			ret.rowGap = sv.sizeValue;
		}
		if (getValue(ParameterName::CssColumnGap, sv)) {
			ret.columnGap = sv.sizeValue;
		}

		// grid track / line strings live in the sheet's string table; copy them
		// into std memory (as with font-family / background-image)
		auto getString = [&](ParameterName name, String &out) {
			if (getValue(name, sv)) {
				out = iface.resolveString(sv.stringId).str<memory::StandartInterface>();
			}
		};
		getString(ParameterName::CssGridTemplateColumns, ret.gridTemplateColumns);
		getString(ParameterName::CssGridTemplateRows, ret.gridTemplateRows);
		getString(ParameterName::CssGridAutoColumns, ret.gridAutoColumns);
		getString(ParameterName::CssGridAutoRows, ret.gridAutoRows);
		getString(ParameterName::CssGridColumnStart, ret.gridColumnStart);
		getString(ParameterName::CssGridColumnEnd, ret.gridColumnEnd);
		getString(ParameterName::CssGridRowStart, ret.gridRowStart);
		getString(ParameterName::CssGridRowEnd, ret.gridRowEnd);

		for (auto &name : s_trackedParameters) {
			if (!style.get(name, &iface).empty()) {
				ret.present.emplace_back(toInt(name));
			}
		}

		ret.valid = true;
	}, pool);
	memory::pool::destroy(pool);

	return ret;
}

bool StyleApplier::init() {
	if (!System::init()) {
		return false;
	}
	setSystemFlags(SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents
			| SystemFlags::HandleComponents | SystemFlags::HandleAncestorComponents);
	return true;
}

bool StyleApplier::init(ApplyCallback &&cb) {
	if (!init()) {
		return false;
	}
	_callback = move(cb);
	return true;
}

void StyleApplier::handleAdded(Node *owner) { System::handleAdded(owner); }

void StyleApplier::handleEnter(Scene *scene) {
	System::handleEnter(scene);
	// initial application: the ancestor chain is complete here, and the node may have
	// been added after the sheet owner's dirty frame
	apply();
}

void StyleApplier::handleComponentsDirty() { apply(); }

void StyleApplier::apply() {
	if (!_owner) {
		return;
	}

	auto style = resolveStyleForNode(_owner);
	if (!style.valid) {
		return;
	}

	if (_callback && _callback(_owner, style)) {
		return;
	}

	applyDefault(_owner, style);
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

void StyleApplier::applyDefault(Node *node, const ResolvedStyle &s) {
	using document::ParameterName;

	Size2 parentSize;
	if (auto p = node->getParent()) {
		parentSize = p->getContentSize();
	}
	const float fontSize = float(s.font.fontSize.get());

	auto computeMetric = [&](const document::Metric &m, float base) {
		return s.media.computeValueAuto(m, base, fontSize);
	};

	if (s.has(ParameterName::CssOpacity)) {
		node->setOpacity(float(s.text.opacity) / 255.0f);
	}

	auto label = dynamic_cast<Label *>(node);
	if (label) {
		if (s.has(ParameterName::CssColor)) {
			label->setColor(Color4F(Color4B(s.text.color, 255)), false);
		}
		if (s.has(ParameterName::CssFontSize) || s.has(ParameterName::CssFontSizeNumeric)) {
			label->setFontSize(s.font.fontSize);
		}
		if (s.has(ParameterName::CssFontFamily) && !s.fontFamily.empty()) {
			label->setFontFamily(s.fontFamily);
		}
		if (s.has(ParameterName::CssFontWeight)) {
			label->setFontWeight(s.font.fontWeight);
		}
		if (s.has(ParameterName::CssFontStyle)) {
			label->setFontStyle(s.font.fontStyle);
		}
		if (s.has(ParameterName::CssFontStretch)) {
			label->setFontStretch(s.font.fontStretch);
		}
		if (s.has(ParameterName::CssTextAlign)) {
			label->setAlignment(s.paragraph.textAlign);
		}
		if (s.has(ParameterName::CssWidth) && !s.block.width.isAuto()) {
			label->setWidth(computeMetric(s.block.width, parentSize.width));
		}
	} else {
		if (s.has(ParameterName::CssBackgroundColor)) {
			// Layer covers Button as well
			if (auto layer = dynamic_cast<Layer *>(node)) {
				layer->setColor(Color4F(s.background.backgroundColor), true);
			}
		}

		auto size = node->getContentSize();
		bool sizeDirty = false;
		if (s.has(ParameterName::CssWidth) && !s.block.width.isAuto()) {
			size.width = computeMetric(s.block.width, parentSize.width);
			sizeDirty = true;
		}
		if (s.has(ParameterName::CssHeight) && !s.block.height.isAuto()) {
			size.height = computeMetric(s.block.height, parentSize.height);
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
	if (s.position == document::Position::Absolute) {
		auto nodeSize = node->getContentSize();

		const bool hasLeft = s.has(ParameterName::CssLeft) && !s.left.isAuto();
		const bool hasRight = s.has(ParameterName::CssRight) && !s.right.isAuto();
		const bool hasTop = s.has(ParameterName::CssTop) && !s.top.isAuto();
		const bool hasBottom = s.has(ParameterName::CssBottom) && !s.bottom.isAuto();

		// CSS over-constrained resolution: when the size is `auto` and both offsets on an
		// axis are given, the size stretches to fill the gap between them; when all three
		// (both offsets + explicit size) are set, the end offset (right/bottom) is ignored,
		// which the position math below already does by preferring left/top
		const bool widthAuto = !s.has(ParameterName::CssWidth) || s.block.width.isAuto();
		const bool heightAuto = !s.has(ParameterName::CssHeight) || s.block.height.isAuto();

		bool sizeDirty = false;
		if (widthAuto && hasLeft && hasRight) {
			nodeSize.width = parentSize.width - computeMetric(s.left, parentSize.width)
					- computeMetric(s.right, parentSize.width);
			if (nodeSize.width < 0.0f) {
				nodeSize.width = 0.0f;
			}
			sizeDirty = true;
		}
		if (heightAuto && hasTop && hasBottom) {
			nodeSize.height = parentSize.height - computeMetric(s.top, parentSize.height)
					- computeMetric(s.bottom, parentSize.height);
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
			x = computeMetric(s.left, parentSize.width);
		} else if (hasRight) {
			x = parentSize.width - computeMetric(s.right, parentSize.width) - nodeSize.width;
		}

		float y = node->getPosition().y;
		if (hasTop) {
			y = parentSize.height - computeMetric(s.top, parentSize.height);
		} else if (hasBottom) {
			y = computeMetric(s.bottom, parentSize.height) + nodeSize.height;
		}

		node->setAnchorPoint(Vec2(0.0f, 1.0f));
		node->setPosition(Vec2(x, y));
	} else {
		if (s.has(ParameterName::CssXlAnchorPointX) || s.has(ParameterName::CssXlAnchorPointY)) {
			node->setAnchorPoint(s.anchorPoint);
		}

		// -xl-position: direct node position; percent values resolve against the parent
		if (s.has(ParameterName::CssXlPositionX) || s.has(ParameterName::CssXlPositionY)) {
			float x = node->getPosition().x;
			float y = node->getPosition().y;
			if (s.has(ParameterName::CssXlPositionX)) {
				x = computeMetric(s.xlPositionX, parentSize.width);
			}
			if (s.has(ParameterName::CssXlPositionY)) {
				y = computeMetric(s.xlPositionY, parentSize.height);
			}
			node->setPosition(Vec2(x, y));
		}
	}
}

void StyleApplier::applyLayout(Node *node, const ResolvedStyle &s) {
	using document::ParameterName;
	using document::Display;

	Size2 parentSize;
	if (auto p = node->getParent()) {
		parentSize = p->getContentSize();
	}
	const Size2 ownSize = node->getContentSize();
	const float fontSize = float(s.font.fontSize.get());
	auto computeMetric = [&](const document::Metric &m, float base) {
		return s.media.computeValueAuto(m, base, fontSize);
	};

	// map the CSS padding-* onto a container Padding (percent against own width)
	auto fillPadding = [&](Padding &pad) {
		if (s.has(ParameterName::CssPaddingTop) && !s.block.paddingTop.isAuto()) {
			pad.top = computeMetric(s.block.paddingTop, ownSize.width);
		}
		if (s.has(ParameterName::CssPaddingRight) && !s.block.paddingRight.isAuto()) {
			pad.right = computeMetric(s.block.paddingRight, ownSize.width);
		}
		if (s.has(ParameterName::CssPaddingBottom) && !s.block.paddingBottom.isAuto()) {
			pad.bottom = computeMetric(s.block.paddingBottom, ownSize.width);
		}
		if (s.has(ParameterName::CssPaddingLeft) && !s.block.paddingLeft.isAuto()) {
			pad.left = computeMetric(s.block.paddingLeft, ownSize.width);
		}
	};
	// map the CSS margin-* onto an item Margin (percent against parent width)
	auto fillMargin = [&](Padding &m) {
		if (s.has(ParameterName::CssMarginTop) && !s.block.marginTop.isAuto()) {
			m.top = computeMetric(s.block.marginTop, parentSize.width);
		}
		if (s.has(ParameterName::CssMarginRight) && !s.block.marginRight.isAuto()) {
			m.right = computeMetric(s.block.marginRight, parentSize.width);
		}
		if (s.has(ParameterName::CssMarginBottom) && !s.block.marginBottom.isAuto()) {
			m.bottom = computeMetric(s.block.marginBottom, parentSize.width);
		}
		if (s.has(ParameterName::CssMarginLeft) && !s.block.marginLeft.isAuto()) {
			m.left = computeMetric(s.block.marginLeft, parentSize.width);
		}
	};

	const bool wantFlex =
			s.block.display == Display::Flex || s.block.display == Display::InlineFlex;
	const bool wantGrid =
			s.block.display == Display::Grid || s.block.display == Display::InlineGrid;

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
					next.direction = toFlexDirection(s.flexDirection);
				}
				if (s.has(ParameterName::CssFlexWrap)) {
					next.wrap = toFlexWrap(s.flexWrap);
				}
				if (s.has(ParameterName::CssJustifyContent)) {
					next.justifyContent = toFlexJustify(s.justifyContent);
				}
				if (s.has(ParameterName::CssAlignItems)) {
					next.alignItems = toFlexAlignItems(s.alignItems);
				}
				if (s.has(ParameterName::CssAlignContent)) {
					next.alignContent = toFlexAlignItems(s.alignContent);
				}
				if (s.has(ParameterName::CssColumnGap) && !s.columnGap.isAuto()) {
					next.columnGap = computeMetric(s.columnGap, ownSize.width);
				}
				if (s.has(ParameterName::CssRowGap) && !s.rowGap.isAuto()) {
					next.rowGap = computeMetric(s.rowGap, ownSize.height);
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
					next.columnTracks = parseGridTemplate(s.gridTemplateColumns);
				}
				if (s.has(ParameterName::CssGridTemplateRows)) {
					next.rowTracks = parseGridTemplate(s.gridTemplateRows);
				}
				if (s.has(ParameterName::CssGridAutoColumns)) {
					auto t = parseGridTemplate(s.gridAutoColumns);
					if (!t.empty()) {
						next.autoColumn = t.front();
					}
				}
				if (s.has(ParameterName::CssGridAutoRows)) {
					auto t = parseGridTemplate(s.gridAutoRows);
					if (!t.empty()) {
						next.autoRow = t.front();
					}
				}
				if (s.has(ParameterName::CssGridAutoFlow)) {
					next.autoFlow = toGridAutoFlow(s.gridAutoFlow);
				}
				if (s.has(ParameterName::CssJustifyContent)) {
					next.justifyContent = toGridAlign(s.justifyContent);
				}
				if (s.has(ParameterName::CssAlignContent)) {
					next.alignContent = toGridAlign(s.alignContent);
				}
				if (s.has(ParameterName::CssJustifyItems)) {
					next.justifyItems = toGridAlign(s.justifyItems);
				}
				if (s.has(ParameterName::CssAlignItems)) {
					next.alignItems = toGridAlign(s.alignItems);
				}
				if (s.has(ParameterName::CssColumnGap) && !s.columnGap.isAuto()) {
					next.columnGap = computeMetric(s.columnGap, ownSize.width);
				}
				if (s.has(ParameterName::CssRowGap) && !s.rowGap.isAuto()) {
					next.rowGap = computeMetric(s.rowGap, ownSize.height);
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
			if (s.has(ParameterName::CssGridColumnStart) && !s.gridColumnStart.empty()
					&& parseGridLine(s.gridColumnStart, a, b, c)) {
				next.gridColumnStart = a;
				if (c > 1) {
					next.columnSpan = c;
				}
			}
			a = 0, b = 0, c = 1;
			if (s.has(ParameterName::CssGridColumnEnd) && !s.gridColumnEnd.empty()
					&& parseGridLine(s.gridColumnEnd, a, b, c)) {
				next.gridColumnEnd = a; // a bare line number lands in `start`
				if (c > 1) {
					next.columnSpan = c;
				}
			}
			a = 0, b = 0, c = 1;
			if (s.has(ParameterName::CssGridRowStart) && !s.gridRowStart.empty()
					&& parseGridLine(s.gridRowStart, a, b, c)) {
				next.gridRowStart = a;
				if (c > 1) {
					next.rowSpan = c;
				}
			}
			a = 0, b = 0, c = 1;
			if (s.has(ParameterName::CssGridRowEnd) && !s.gridRowEnd.empty()
					&& parseGridLine(s.gridRowEnd, a, b, c)) {
				next.gridRowEnd = a;
				if (c > 1) {
					next.rowSpan = c;
				}
			}
			if (s.has(ParameterName::CssJustifySelf)) {
				next.justifySelf = toGridAlignSelf(s.justifySelf);
			}
			if (s.has(ParameterName::CssAlignSelf)) {
				next.alignSelf = toGridAlignSelf(s.alignSelf);
			}
			if (s.has(ParameterName::CssOrder)) {
				next.order = s.order;
			}
			fillMargin(next.margin);
			if (next != *info) {
				*info = next;
				return true;
			}
			return false;
		});
	} else if (parent->getComponent<FlexLayoutInfo>()) {
		node->setOrUpdateComponent<FlexItemInfo>([&](NotNull<FlexItemInfo> info) {
			FlexItemInfo next = *info;
			if (s.has(ParameterName::CssFlexGrow)) {
				next.grow = s.flexGrow;
			}
			if (s.has(ParameterName::CssFlexShrink)) {
				next.shrink = s.flexShrink;
			}
			if (s.has(ParameterName::CssFlexBasis)) {
				next.basis = s.flexBasis.isAuto() ? FlexItemInfo::Auto
												  : computeMetric(s.flexBasis, parentSize.width);
			}
			if (s.has(ParameterName::CssAlignSelf)) {
				next.alignSelf = toFlexAlignSelf(s.alignSelf);
			}
			if (s.has(ParameterName::CssOrder)) {
				next.order = s.order;
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

} // namespace stappler::xenolith::simpleui
