/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "XLUiLayoutSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// Components acquire their unique ids statically during program startup.
ComponentId FlexLayoutInfo::Id;
ComponentId FlexItemInfo::Id;
ComponentId GridLayoutInfo::Id;
ComponentId GridItemInfo::Id;
ComponentId OutOfFlowComponent::Id;

// All LayoutSystems share one frame-stack tag, so a descendant's stack lookup resolves to the
// nearest ancestor container (back() of the tag's stack)
uint64_t LayoutSystem::SystemFrameTag = System::GetNextSystemId();

namespace {

// tolerance used when deciding whether the next item still fits on the line
static constexpr float FlexEpsilon = 0.01f;

// Per-item working set, computed in an abstract main/cross flow space and
// projected onto the node's actual position/size at the very end.
struct FlexItem {
	Node *node = nullptr;
	FlexItemInfo cfg;

	float baseMain = 0.0f; // resolved flex-basis (content box, no margins)
	float mainSize = 0.0f; // main size after flexing
	float crossSize = 0.0f; // cross size after alignment / stretching
	float naturalCross = 0.0f; // node's own cross size, used as the hypothetical size

	// the hypothetical cross size comes from measurement at the final main size
	bool fitCross = false;
	// at least one axis was measured -> the item gets handleLayoutApplied on commit
	bool measured = false;

	// margins projected onto the flow (start/end of main and cross axes)
	float mainMarginStart = 0.0f;
	float mainMarginEnd = 0.0f;
	float crossMarginStart = 0.0f;
	float crossMarginEnd = 0.0f;

	// `margin: auto` on the projected sides. An auto margin contributes nothing while sizes are
	// being resolved and is filled from the leftover space afterwards, so it is kept apart from
	// the fixed margins above rather than folded into them.
	bool mainMarginStartAuto = false;
	bool mainMarginEndAuto = false;
	bool crossMarginStartAuto = false;
	bool crossMarginEndAuto = false;

	// space handed to the main-axis auto margins once the free space is known
	float mainAutoStart = 0.0f;
	float mainAutoEnd = 0.0f;

	// margin-box start positions in the flow (0 == start of the content box / line)
	float mainStart = 0.0f;
	float crossStart = 0.0f;

	uint32_t mainAutoCount() const {
		return (mainMarginStartAuto ? 1u : 0u) + (mainMarginEndAuto ? 1u : 0u);
	}
	uint32_t crossAutoCount() const {
		return (crossMarginStartAuto ? 1u : 0u) + (crossMarginEndAuto ? 1u : 0u);
	}

	float outerMain() const {
		return mainMarginStart + mainAutoStart + mainSize + mainAutoEnd + mainMarginEnd;
	}
	float outerCross() const { return crossMarginStart + crossSize + crossMarginEnd; }
	float outerBaseMain() const { return mainMarginStart + baseMain + mainMarginEnd; }
	float outerNaturalCross() const { return crossMarginStart + naturalCross + crossMarginEnd; }
};

// Can this node answer the content-measurement protocol at all? Either a system opted into it
// (`SystemFlags::HandleMeasure` — Label's own, a flex container's, or one an application wrote),
// or the precomputed MeasureComponent fallback is present. Both are resolved by measureNode();
// this is only the cheap "is it worth asking" predicate, so it calls nothing.
static bool LayoutSystem_canMeasure(Node *node) {
	for (auto &it : node->getSystems()) {
		if (it->isEnabled() && hasFlag(it->getSystemFlags(), SystemFlags::HandleMeasure)) {
			return true;
		}
	}
	return node->getComponent<MeasureComponent>() != nullptr;
}

// Does the style give this node a definite size on the axis? That is what the resolver publishes
// in MeasureComponent::normal (a per-axis value < 0 means "unspecified"), and it is the CSS
// `width`/`height` of the item - which wins over content sizing.
static bool LayoutSystem_hasDefiniteSize(Node *node, bool horizontal) {
	if (auto mc = node->getComponent<MeasureComponent>()) {
		return (horizontal ? mc->normal.width : mc->normal.height) >= 0.0f;
	}
	return false;
}

// A run of items placed on a single line, plus its resolved cross extent.
struct FlexLine {
	size_t begin = 0;
	size_t end = 0; // exclusive
	float crossSize = 0.0f;
	float crossStart = 0.0f;

	size_t count() const { return end - begin; }
};

// Result of the shared collect/break/flex pass (steps 1-4 of the algorithm),
// consumed by both the placement pass and the measurement pass.
struct FlexPassOutput {
	Vector<FlexItem> items;
	Vector<FlexLine> lines;
	float usedMain = 0.0f; // widest line's outer main extent, gaps included
	float usedCross = 0.0f; // sum of line cross extents plus cross gaps
};

// ---- grid working structs -------------------------------------------------

// resolved half-open interval of 0-based track indices [start, end)
struct GridSpan {
	uint32_t start = 0;
	uint32_t end = 0;
	bool definite = false;

	uint32_t span() const { return end - start; }
};

struct GridItem {
	Node *node = nullptr;
	GridItemInfo cfg;

	GridSpan col;
	GridSpan row;

	float natW = 0.0f; // natural (max-content) size along X
	float natH = 0.0f; // natural size along Y

	// filled during positioning (top-left content-box coordinates)
	float boxX = 0.0f;
	float boxY = 0.0f;
	float boxW = 0.0f;
	float boxH = 0.0f;
};

// a resolved track after sizing
struct GridTrackSize {
	GridTrack def;
	float base = 0.0f; // resolved size in px
	float position = 0.0f; // start offset from the content-box start along its axis
};

// Read a leading number from `r` (advances r past it). Returns false if there is
// no number at the head.
static bool readNumber(StringView &r, float &out) {
	r.skipChars<StringView::WhiteSpace>();
	auto res = r.readFloat();
	if (!res.valid()) {
		return false;
	}
	out = res.get();
	return true;
}

// Parse one whitespace-delimited grid track token into `out`. Returns false for
// an unsupported token (named lines in brackets, minmax(), fit-content(), ...).
static bool parseGridTrackToken(StringView token, GridTrack &out) {
	token.trimChars<StringView::WhiteSpace>();
	if (token.empty()) {
		return false;
	}
	if (token == "auto" || token == "min-content" || token == "max-content") {
		out.type = GridTrack::Auto;
		out.value = 0.0f;
		return true;
	}
	StringView r(token);
	float num = 0.0f;
	if (!readNumber(r, num)) {
		return false; // named line "[...]" or unsupported function
	}
	r.trimChars<StringView::WhiteSpace>();
	if (r == "fr") {
		out.type = GridTrack::Fraction;
		out.value = num;
	} else if (r.is('%')) {
		out.type = GridTrack::Percent;
		out.value = num;
	} else { // "px", unit-less, or an unsupported length unit -> best-effort px
		out.type = GridTrack::Fixed;
		out.value = num;
	}
	return true;
}

} // namespace

// Parse a track list, expanding repeat(). Recursion depth is bounded by the
// nesting of repeat(), which CSS does not allow, so this is effectively flat.
Vector<GridTrack> parseGridTemplate(StringView input) {
	Vector<GridTrack> ret;
	StringView r(input);
	while (true) {
		r.skipChars<StringView::WhiteSpace>();
		if (r.empty()) {
			break;
		}
		if (r.starts_with("repeat(")) {
			r += 7; // strlen("repeat(")
			r.skipChars<StringView::WhiteSpace>();
			auto cres = r.readFloat();
			const uint32_t n = cres.valid() ? uint32_t(sprt::max(cres.get(), 0.0f)) : 0u;
			r.skipChars<StringView::WhiteSpace>();
			if (r.is(',')) {
				++r;
			}
			// inner track list up to the matching ')'
			const char *innerStart = r.data();
			int depth = 1;
			while (!r.empty() && depth > 0) {
				if (r.is('(')) {
					++depth;
				} else if (r.is(')')) {
					--depth;
					if (depth == 0) {
						break;
					}
				}
				++r;
			}
			StringView inner(innerStart, size_t(r.data() - innerStart));
			if (r.is(')')) {
				++r;
			}
			auto innerTracks = parseGridTemplate(inner);
			for (uint32_t i = 0; i < n; ++i) {
				for (auto &t : innerTracks) { ret.emplace_back(t); }
			}
			continue;
		}
		if (r.is('[')) {
			// named-line block: skip (unsupported)
			while (!r.empty() && !r.is(']')) { ++r; }
			if (r.is(']')) {
				++r;
			}
			continue;
		}
		// read a single whitespace-delimited token
		auto token = r.readUntil<StringView::WhiteSpace>();
		GridTrack track;
		if (parseGridTrackToken(token, track)) {
			ret.emplace_back(track);
		}
	}
	return ret;
}

// helper for parseGridLine: parse "span N" or "N" or "auto"/empty
static void parseLineToken(StringView token, uint32_t &line, uint32_t &span, bool &isSpan) {
	token.trimChars<StringView::WhiteSpace>();
	line = 0;
	span = 0;
	isSpan = false;
	if (token.empty() || token == "auto") {
		return;
	}
	if (token.starts_with("span")) {
		isSpan = true;
		StringView r = token;
		r += 4; // strlen("span")
		float n = 0.0f;
		if (readNumber(r, n)) {
			span = uint32_t(sprt::max(n, 1.0f));
		} else {
			span = 1;
		}
		return;
	}
	StringView r(token);
	float n = 0.0f;
	if (readNumber(r, n)) {
		line = uint32_t(sprt::max(n, 1.0f));
	}
}

bool parseGridLine(StringView input, uint32_t &start, uint32_t &end, uint32_t &span) {
	StringView r(input);
	r.trimChars<StringView::WhiteSpace>();
	if (r.empty()) {
		return false;
	}

	StringView left = r;
	StringView right;
	auto slash = r.find('/');
	if (slash < r.size()) {
		left = r.sub(0, slash);
		right = r.sub(slash + 1);
	}

	uint32_t lLine = 0, lSpan = 0, rLine = 0, rSpan = 0;
	bool lIsSpan = false, rIsSpan = false;
	parseLineToken(left, lLine, lSpan, lIsSpan);

	if (right.empty()) {
		// single value: start line or bare span
		if (lIsSpan) {
			start = 0;
			end = 0;
			span = lSpan;
		} else {
			start = lLine;
			end = 0;
			span = 1;
		}
		return true;
	}

	parseLineToken(right, rLine, rSpan, rIsSpan);
	if (lIsSpan) {
		// "span N / M"
		start = 0;
		end = rLine;
		span = sprt::max(lSpan, 1u);
	} else if (rIsSpan) {
		// "N / span M"
		start = lLine;
		end = 0;
		span = sprt::max(rSpan, 1u);
	} else {
		start = lLine;
		end = rLine;
		span = 1;
	}
	return true;
}

bool LayoutSystem::init() {
	if (!System::init()) {
		return false;
	}

	_systemPriority = LayoutDefaultPriority;

	// We lay out children in the layout-children phase, react to the container's own
	// layout-info component updates, descendant resize events (fit-content invalidation) and
	// answer the measurement protocol (fit-content of nested containers). AddToFrameStack +
	// the shared FrameTag publish us so descendants deliver their resize to the nearest container.
	setSystemFlags(SystemFlags::HandleLayoutChildren | SystemFlags::HandleComponents
			| SystemFlags::HandleSceneEvents | SystemFlags::HandleMeasure
			| SystemFlags::HandleChildNodeEvents | SystemFlags::HandleChildComponents
			| SystemFlags::AddToFrameStack);
	setFrameTag(SystemFrameTag);
	return true;
}

bool LayoutSystem::init(const FlexLayoutInfo &info) {
	if (!init()) {
		return false;
	}
	_mode = LayoutMode::Flex;
	_initialInfo = info;
	return true;
}

bool LayoutSystem::init(const GridLayoutInfo &info) {
	if (!init()) {
		return false;
	}
	_mode = LayoutMode::Grid;
	_initialGridInfo = info;
	return true;
}

void LayoutSystem::handleAdded(Node *owner) {
	System::handleAdded(owner);

	// Make sure the container always carries the matching container info
	// component, so the "parameters live on the parent node" contract holds even
	// if the caller never sets one explicitly.
	if (_mode == LayoutMode::Grid) {
		if (!owner->getComponent<GridLayoutInfo>()) {
			owner->setComponent<GridLayoutInfo>(_initialGridInfo);
		}
	} else {
		if (!owner->getComponent<FlexLayoutInfo>()) {
			owner->setComponent<FlexLayoutInfo>(_initialInfo);
		}
	}
}

void LayoutSystem::handleLayoutChildren() {
	System::handleLayoutChildren();
	apply();
}

void LayoutSystem::handleComponentsDirty(const ComponentMask &mask) {
	System::handleComponentsDirty(mask);
	_owner->markLayoutChildrenDirty(); // container params changed -> re-lay-out children
}

void LayoutSystem::setMode(LayoutMode mode) {
	if (_mode == mode) {
		return;
	}
	_mode = mode;
	if (_owner) {
		// ensure the container carries the component for the new mode
		if (_mode == LayoutMode::Grid) {
			if (!_owner->getComponent<GridLayoutInfo>()) {
				_owner->setComponent<GridLayoutInfo>(_initialGridInfo);
			}
		} else {
			if (!_owner->getComponent<FlexLayoutInfo>()) {
				_owner->setComponent<FlexLayoutInfo>(_initialInfo);
			}
		}
		apply();
	}
}

const FlexLayoutInfo *LayoutSystem::getInfo() const {
	if (!_owner) {
		return nullptr;
	}
	return _owner->getComponent<FlexLayoutInfo>();
}

void LayoutSystem::setInfo(const FlexLayoutInfo &info) {
	if (!_owner) {
		_initialInfo = info;
		return;
	}
	_owner->setComponent<FlexLayoutInfo>(info);
}

void LayoutSystem::updateInfo(const Callback<bool(FlexLayoutInfo &)> &cb) {
	if (!_owner) {
		cb(_initialInfo);
		return;
	}
	_owner->setOrUpdateComponent<FlexLayoutInfo>(
			[&](NotNull<FlexLayoutInfo> info) { return cb(*info); });
}

void LayoutSystem::setDirection(FlexDirection value) {
	updateInfo([&](FlexLayoutInfo &info) {
		if (info.direction == value) {
			return false;
		}
		info.direction = value;
		return true;
	});
}

void LayoutSystem::setWrap(FlexWrap value) {
	updateInfo([&](FlexLayoutInfo &info) {
		if (info.wrap == value) {
			return false;
		}
		info.wrap = value;
		return true;
	});
}

void LayoutSystem::setJustifyContent(FlexJustify value) {
	updateInfo([&](FlexLayoutInfo &info) {
		if (info.justifyContent == value) {
			return false;
		}
		info.justifyContent = value;
		return true;
	});
}

void LayoutSystem::setAlignItems(FlexAlign value) {
	updateInfo([&](FlexLayoutInfo &info) {
		if (info.alignItems == value) {
			return false;
		}
		info.alignItems = value;
		return true;
	});
}

void LayoutSystem::setAlignContent(FlexAlign value) {
	updateInfo([&](FlexLayoutInfo &info) {
		if (info.alignContent == value) {
			return false;
		}
		info.alignContent = value;
		return true;
	});
}

void LayoutSystem::setGap(float row, float column) {
	updateInfo([&](FlexLayoutInfo &info) {
		if (info.rowGap == row && info.columnGap == column) {
			return false;
		}
		info.rowGap = row;
		info.columnGap = column;
		return true;
	});
}

void LayoutSystem::setPadding(Padding value) {
	updateInfo([&](FlexLayoutInfo &info) {
		if (info.padding == value) {
			return false;
		}
		info.padding = value;
		return true;
	});
}

const FlexItemInfo *LayoutSystem::getItem(NotNull<Node> node) {
	return node->getComponent<FlexItemInfo>();
}

void LayoutSystem::setItem(NotNull<Node> node, const FlexItemInfo &info) {
	node->setComponent<FlexItemInfo>(info);
}

const GridLayoutInfo *LayoutSystem::getGridInfo() const {
	if (!_owner) {
		return nullptr;
	}
	return _owner->getComponent<GridLayoutInfo>();
}

void LayoutSystem::setGridInfo(const GridLayoutInfo &info) {
	if (!_owner) {
		_initialGridInfo = info;
		return;
	}
	_owner->setComponent<GridLayoutInfo>(info);
}

const GridItemInfo *LayoutSystem::getGridItem(NotNull<Node> node) {
	return node->getComponent<GridItemInfo>();
}

void LayoutSystem::setGridItem(NotNull<Node> node, const GridItemInfo &info) {
	node->setComponent<GridItemInfo>(info);
}

void LayoutSystem::apply() {
	if (!_owner || _inApply) {
		return;
	}
	_inApply = true;
	if (_mode == LayoutMode::Grid) {
		layoutGrid();
	} else {
		layoutFlex();
	}
	_inApply = false;
}

bool LayoutSystem::handleMeasure(const MeasureConstraints &c, Size2 &result) {
	if (!_owner || _mode == LayoutMode::Grid) {
		// grid measurement is not implemented yet: fall back to the node's size
		return false;
	}
	result = measure(c);
	return true;
}

// True when a layout engine computes this node's size FROM its content instead of taking the
// node's own content size as given: a fit-content flex item, or a grid item left to its content
// on either axis.
static bool LayoutSystem_isContentSized(Node *node) {
	if (auto item = node->getComponent<FlexItemInfo>()) {
		if (item->basis == FlexItemInfo::FitContent
				|| item->crossSize == FlexItemInfo::FitContent) {
			return true;
		}
	}
	if (auto item = node->getComponent<GridItemInfo>()) {
		return item->width == GridItemInfo::Auto || item->height == GridItemInfo::Auto;
	}
	return false;
}

// Re-arm every ancestor container whose measured size can still depend on this subtree.
//
// The content-size bubble carries a resize outwards only as long as each container's OWN
// ContentSize changes: the container node then delivers its own content-size event to the next
// ancestor through the frame stack. That chain breaks at a content-sized container - a
// fit-content item does NOT own its size, the container above it does, so re-laying out its
// children leaves its ContentSize untouched and the ancestor never learns that the size it
// measured from this subtree went stale (a label deep inside kept its old box, so the text
// overflowed it). Walk out explicitly instead, stopping at the first container whose size the
// content no longer determines.
static void LayoutSystem_invalidateMeasuredAncestors(Node *node) {
	while (LayoutSystem_isContentSized(node)) {
		auto parent = node->getParent();
		if (!parent) {
			return;
		}
		parent->markLayoutChildrenDirty();
		node = parent;
	}
}

void LayoutSystem::handleChildContentSizeDirty(Node *child) {
	if (_inApply || !_owner) {
		return;
	}

	if (child->getParent() == _owner) {
		// coalesced: any number of child changes per frame ends up as a single
		// re-layout through the layout-children phase on the next visit; convergence
		// is guaranteed by setContentSize's equal-size early-out.
		_owner->markLayoutChildrenDirty();
		LayoutSystem_invalidateMeasuredAncestors(_owner);
	}
}

void LayoutSystem::handleChildComponentsDirty(Node *child, const ComponentMask &mask) {
	if (_inApply || !_owner) {
		return;
	}

	// a direct child flipped between display:none and displayed (VisibilityComponent
	// written or removed) - the set of in-flow items changes, re-lay-out the container
	if (child->getParent() == _owner && mask.contains(VisibilityComponent::Id.value)) {
		_owner->markLayoutChildrenDirty();
		// the item set changed, so our own measured size did too
		LayoutSystem_invalidateMeasuredAncestors(_owner);
	}
}

Size2 LayoutSystem::measureNode(Node *node, const MeasureConstraints &c) {
	// copy the list - a handler may mutate the node's systems while we iterate
	auto span = node->getSystems();
	Vector<Rc<System>> tmpSystems(span.begin(), span.end());
	for (auto &it : tmpSystems) {
		if (it->isEnabled() && hasFlag(it->getSystemFlags(), SystemFlags::HandleMeasure)) {
			Size2 result;
			if (it->handleMeasure(c, result)) {
				return result;
			}
		}
	}
	// fallback for a node with no measuring system: its intrinsic size - an explicit per-axis size
	// from a MeasureComponent (published by the style resolver) merged over its current ContentSize
	// (an axis left unspecified, value < 0, keeps the current size). Mirrors intrinsicSize() above.
	Size2 cs = node->getContentSize();
	if (auto mc = node->getComponent<MeasureComponent>()) {
		const Size2 req = mc->measure(c);
		if (req.width >= 0.0f) {
			cs.width = req.width;
		}
		if (req.height >= 0.0f) {
			cs.height = req.height;
		}
	}
	return cs;
}

// Notify the node's measuring systems that a layout engine committed `size`
// to it (copy the list - handlers mutate node state, e.g. a label re-wraps)
static void dispatchLayoutApplied(Node *node, const Size2 &size) {
	auto span = node->getSystems();
	Vector<Rc<System>> tmpSystems(span.begin(), span.end());
	for (auto &it : tmpSystems) {
		if (it->isEnabled() && hasFlag(it->getSystemFlags(), SystemFlags::HandleMeasure)) {
			it->handleLayoutApplied(size);
		}
	}
}

// A child's intrinsic (style-requested) size - the INPUT to layout. An explicit per-axis size that
// the style resolver published in a MeasureComponent wins; an axis it left unspecified (value < 0)
// falls back to the child's current ContentSize. Keeping the requested size in a component rather
// than in ContentSize is what lets the LayoutSystem be the SOLE writer of a child's ContentSize,
// breaking the cycle where the style both writes ContentSize and has the layout read it back.
static Size2 intrinsicSize(Node *node) {
	Size2 cs = node->getContentSize();
	if (auto mc = node->getComponent<MeasureComponent>()) {
		if (mc->normal.width >= 0.0f) {
			cs.width = mc->normal.width;
		}
		if (mc->normal.height >= 0.0f) {
			cs.height = mc->normal.height;
		}
	}
	return cs;
}

// Steps 1-4 of the flex algorithm, shared by the placement pass (layoutFlex)
// and the measurement pass (LayoutSystem::measure): collect items and resolve
// their base sizes (measuring fit-content ones), sort by `order`, break into
// lines, resolve flexed main sizes, re-measure fit-content cross sizes and
// compute the line cross extents. With `forMeasure` grow/shrink are skipped
// (CSS content sizing ignores them) and nothing is committed to the nodes.
// contentMain / contentCross may be maxOf<float>() (unconstrained axis).
static void computeFlexLines(Node *owner, const FlexLayoutInfo &info, float contentMain,
		float contentCross, bool forMeasure, FlexPassOutput &out) {
	const bool isRow =
			info.direction == FlexDirection::Row || info.direction == FlexDirection::RowReverse;
	const float mainGap = isRow ? info.columnGap : info.rowGap;
	const float crossGap = isRow ? info.rowGap : info.columnGap;
	const bool boundedMain = contentMain != maxOf<float>();

	// 1. Collect in-flow items and project their parameters onto the flow axes.
	auto &items = out.items;
	for (auto &child : owner->getChildren()) {
		// collapsed when explicitly invisible or `display: none`; a `visibility: hidden`
		// child keeps its layout box (isDisplayed stays true)
		if (!child->isDisplayed()) {
			continue;
		}

		// `position: absolute` and friends: not an item at all, so it neither takes space nor
		// receives any (see OutOfFlowComponent)
		if (child->getComponent<OutOfFlowComponent>()) {
			continue;
		}

		FlexItem item;
		item.node = child;
		if (auto cfg = child->getComponent<FlexItemInfo>()) {
			item.cfg = *cfg;
		}

		const auto autoMargin = item.cfg.autoMargin;
		if (isRow) {
			// main is horizontal, cross is vertical (cross-start == top)
			item.mainMarginStart = item.cfg.margin.left;
			item.mainMarginEnd = item.cfg.margin.right;
			item.crossMarginStart = item.cfg.margin.top;
			item.crossMarginEnd = item.cfg.margin.bottom;
			item.mainMarginStartAuto = hasFlag(autoMargin, FlexAutoMargin::Left);
			item.mainMarginEndAuto = hasFlag(autoMargin, FlexAutoMargin::Right);
			item.crossMarginStartAuto = hasFlag(autoMargin, FlexAutoMargin::Top);
			item.crossMarginEndAuto = hasFlag(autoMargin, FlexAutoMargin::Bottom);
		} else {
			// main is vertical (main-start == top), cross is horizontal
			item.mainMarginStart = item.cfg.margin.top;
			item.mainMarginEnd = item.cfg.margin.bottom;
			item.crossMarginStart = item.cfg.margin.left;
			item.crossMarginEnd = item.cfg.margin.right;
			item.mainMarginStartAuto = hasFlag(autoMargin, FlexAutoMargin::Top);
			item.mainMarginEndAuto = hasFlag(autoMargin, FlexAutoMargin::Bottom);
			item.crossMarginStartAuto = hasFlag(autoMargin, FlexAutoMargin::Left);
			item.crossMarginEndAuto = hasFlag(autoMargin, FlexAutoMargin::Right);
		}

		// an auto margin is free space, not a distance: it contributes nothing until the sizes
		// are settled (CSS resolves auto margins to zero for intrinsic sizing as well)
		if (item.mainMarginStartAuto) {
			item.mainMarginStart = 0.0f;
		}
		if (item.mainMarginEndAuto) {
			item.mainMarginEnd = 0.0f;
		}
		if (item.crossMarginStartAuto) {
			item.crossMarginStart = 0.0f;
		}
		if (item.crossMarginEndAuto) {
			item.crossMarginEnd = 0.0f;
		}

		const Size2 cs = intrinsicSize(child);
		const float nodeMain = isRow ? cs.width : cs.height;
		const float nodeCross = isRow ? cs.height : cs.width;

		// Who gets asked for its content size. `flex-basis: fit-content` always measures; and so
		// does `flex-basis: auto` when the style gave the item no definite main size - that is
		// plain CSS ("auto" falls through to the size property, and an auto size falls through to
		// `content"). Which nodes can answer is not a fixed list: any node with a HandleMeasure
		// system or a MeasureComponent does, Label and nested flex containers being the two the
		// engine ships.
		const bool canMeasure = LayoutSystem_canMeasure(child);
		const bool measureMain = item.cfg.basis == FlexItemInfo::FitContent
				|| (item.cfg.basis == FlexItemInfo::Auto && canMeasure
						&& !LayoutSystem_hasDefiniteSize(child, isRow));

		// The cross size is re-measured at the final main size (a wrapped label: width -> height).
		// Same rule: an explicit fit-content, or an auto cross the node can answer for. A measured
		// main size also invalidates the stale contentSize-based cross, so it implies a re-measure.
		item.fitCross = item.cfg.crossSize == FlexItemInfo::FitContent
				|| (item.cfg.crossSize == FlexItemInfo::Auto
						&& (measureMain
								|| (canMeasure && !LayoutSystem_hasDefiniteSize(child, !isRow))));
		item.measured = item.fitCross || measureMain;

		if (measureMain) {
			// content sizing -> min(max-content, available main), clamped to [minMain, maxMain]
			MeasureConstraints mc;
			mc.mode = MeasureMode::MaxContent;
			if (contentCross != maxOf<float>()) {
				// bound the cross axis by the content box so nested containers
				// don't measure against infinite cross space
				(isRow ? mc.maxHeight : mc.maxWidth) = contentCross;
			}
			const Size2 m = LayoutSystem::measureNode(child, mc);
			float base = isRow ? m.width : m.height;
			if (boundedMain) {
				base = sprt::min(base,
						sprt::max(contentMain - item.mainMarginStart - item.mainMarginEnd, 0.0f));
			}
			base = sprt::max(base, item.cfg.minMain);
			if (item.cfg.maxMain >= 0.0f) {
				base = sprt::min(base, item.cfg.maxMain);
			}
			item.baseMain = sprt::max(base, 0.0f);
		} else {
			// an explicit flex-basis wins; otherwise the node's own size stands in for its
			// content (a node that cannot be measured has nothing better to offer)
			item.baseMain = (item.cfg.basis >= 0.0f) ? item.cfg.basis : nodeMain;
		}
		// hypothetical cross size used for line sizing and non-stretch alignment;
		// fit-content cross is re-measured after flexing, when the final main
		// size is known
		item.naturalCross = (item.cfg.crossSize >= 0.0f) ? item.cfg.crossSize : nodeCross;

		items.emplace_back(item);
	}

	if (items.empty()) {
		return;
	}

	// 2. Reorder by the CSS `order` property (stable, to keep document order
	// among equal values). Item counts in a UI are tiny, so an insertion sort
	// is both simple and stable.
	for (size_t i = 1; i < items.size(); ++i) {
		FlexItem key = items[i];
		size_t j = i;
		while (j > 0 && items[j - 1].cfg.order > key.cfg.order) {
			items[j] = items[j - 1];
			--j;
		}
		items[j] = key;
	}

	// 3. Break items into lines along the main axis.
	auto &lines = out.lines;
	{
		size_t i = 0;
		while (i < items.size()) {
			FlexLine line;
			line.begin = i;
			float used = 0.0f;
			size_t count = 0;
			while (i < items.size()) {
				const float outer = items[i].outerBaseMain();
				const float add = outer + (count > 0 ? mainGap : 0.0f);
				if (info.wrap != FlexWrap::NoWrap && boundedMain && count > 0
						&& used + add > contentMain + FlexEpsilon) {
					break;
				}
				used += add;
				++count;
				++i;
			}
			line.end = i;
			lines.emplace_back(line);
		}
	}

	// 4. Resolve flexible main sizes and the hypothetical cross size per line.
	for (auto &line : lines) {
		const size_t n = line.count();
		const float gapTotal = (n > 1) ? mainGap * static_cast<float>(n - 1) : 0.0f;

		if (forMeasure) {
			// content sizing ignores grow/shrink: keep the clamped base sizes
			for (size_t k = line.begin; k < line.end; ++k) {
				auto &item = items[k];
				float size = sprt::max(item.baseMain, item.cfg.minMain);
				if (item.cfg.maxMain >= 0.0f) {
					size = sprt::min(size, item.cfg.maxMain);
				}
				item.mainSize = sprt::max(size, 0.0f);
			}
		} else {
			// CSS "resolve the flexible lengths": distribute the free space, clamp each item to
			// its own [minMain, maxMain], and whenever a clamp actually moved an item, freeze it
			// there and share what is left among the others. Doing it in one pass instead would
			// drop the space a clamped item gave up: three items growing into 600px, one with
			// min-width 260 and one with max-width 120, must end at 260/120/220 - a single pass
			// leaves the third at its 200px share and the row 20px short.
			float sumOuterBase = 0.0f;
			for (size_t k = line.begin; k < line.end; ++k) {
				sumOuterBase += items[k].outerBaseMain();
			}
			const bool growing = (contentMain - sumOuterBase - gapTotal) > 0.0f;

			// an item that cannot flex in the needed direction is frozen from the start; its
			// baseMain was already clamped when it was resolved
			mem_std::Vector<uint8_t> frozen(line.count(), 0);
			for (size_t k = line.begin; k < line.end; ++k) {
				auto &item = items[k];
				item.mainSize = item.baseMain;
				if ((growing ? item.cfg.grow : item.cfg.shrink) <= 0.0f) {
					frozen[k - line.begin] = 1;
				}
			}

			// every pass freezes at least one item, so n + 1 of them is an exhaustive bound
			for (size_t pass = 0; pass <= n; ++pass) {
				float used = gapTotal;
				float sumFactor = 0.0f;
				for (size_t k = line.begin; k < line.end; ++k) {
					auto &item = items[k];
					if (frozen[k - line.begin]) {
						used += item.mainMarginStart + item.mainSize + item.mainMarginEnd;
					} else {
						used += item.outerBaseMain();
						sumFactor += growing ? item.cfg.grow : item.cfg.shrink * item.baseMain;
					}
				}
				if (sumFactor <= 0.0f) {
					break; // nothing left that can take the remainder
				}

				const float freeMain = contentMain - used;
				bool clampedAny = false;
				for (size_t k = line.begin; k < line.end; ++k) {
					auto &item = items[k];
					if (frozen[k - line.begin]) {
						continue;
					}
					const float factor = growing ? item.cfg.grow : item.cfg.shrink * item.baseMain;
					const float size = item.baseMain + (factor / sumFactor) * freeMain;

					float clamped = sprt::max(size, item.cfg.minMain);
					if (item.cfg.maxMain >= 0.0f) {
						clamped = sprt::min(clamped, item.cfg.maxMain);
					}
					clamped = sprt::max(clamped, 0.0f);

					item.mainSize = clamped;
					if (sprt::abs(clamped - size) > FlexEpsilon) {
						frozen[k - line.begin] = 1;
						clampedAny = true;
					}
				}
				if (!clampedAny) {
					break;
				}
			}
		}

		// re-measure fit-content cross sizes now that the final main size is
		// known (e.g. a wrapped label: width -> resulting height)
		for (size_t k = line.begin; k < line.end; ++k) {
			auto &item = items[k];
			if (item.fitCross) {
				MeasureConstraints mc;
				(isRow ? mc.maxWidth : mc.maxHeight) = item.mainSize;
				const Size2 m = LayoutSystem::measureNode(item.node, mc);
				item.naturalCross = isRow ? m.height : m.width;
			}
		}

		float maxCross = 0.0f;
		float usedMain = gapTotal;
		for (size_t k = line.begin; k < line.end; ++k) {
			maxCross = sprt::max(maxCross, items[k].outerNaturalCross());
			usedMain += items[k].outerMain();
		}
		line.crossSize = maxCross;
		out.usedMain = sprt::max(out.usedMain, usedMain);
	}

	float totalCross = 0.0f;
	for (auto &line : lines) { totalCross += line.crossSize; }
	out.usedCross = totalCross + crossGap * static_cast<float>(lines.size() - 1);
}

Size2 LayoutSystem::measure(const MeasureConstraints &c) {
	if (!_owner) {
		return Size2();
	}
	if (_mode == LayoutMode::Grid) {
		return _owner->getContentSize();
	}

	auto infoPtr = _owner->getComponent<FlexLayoutInfo>();
	const FlexLayoutInfo info = infoPtr ? *infoPtr : FlexLayoutInfo();

	const bool isRow =
			info.direction == FlexDirection::Row || info.direction == FlexDirection::RowReverse;

	const float padMain = isRow ? info.padding.horizontal() : info.padding.vertical();
	const float padCross = isRow ? info.padding.vertical() : info.padding.horizontal();

	// project the constraints onto the flow axes, minus padding; MaxContent
	// measures against an unconstrained main axis (single line, no wrap)
	float contentMain = maxOf<float>();
	float contentCross = maxOf<float>();
	const float maxMainAxis = isRow ? c.maxWidth : c.maxHeight;
	const float maxCrossAxis = isRow ? c.maxHeight : c.maxWidth;
	if (c.mode != MeasureMode::MaxContent && maxMainAxis != maxOf<float>()) {
		contentMain = sprt::max(maxMainAxis - padMain, 0.0f);
	}
	if (maxCrossAxis != maxOf<float>()) {
		contentCross = sprt::max(maxCrossAxis - padCross, 0.0f);
	}

	FlexPassOutput pass;
	computeFlexLines(_owner, info, contentMain, contentCross, true, pass);

	const float main = pass.usedMain + padMain;
	const float cross = pass.usedCross + padCross;
	return isRow ? Size2(main, cross) : Size2(cross, main);
}

void LayoutSystem::layoutFlex() {
	auto infoPtr = _owner->getComponent<FlexLayoutInfo>();
	const FlexLayoutInfo info = infoPtr ? *infoPtr : FlexLayoutInfo();

	const bool isRow =
			info.direction == FlexDirection::Row || info.direction == FlexDirection::RowReverse;
	const bool mainReverse = info.direction == FlexDirection::RowReverse
			|| info.direction == FlexDirection::ColumnReverse;
	const bool crossReverse = info.wrap == FlexWrap::WrapReverse;

	const Size2 containerSize = _owner->getContentSize();

	// content box: container size minus padding, projected onto main/cross axes
	float contentMain = (isRow ? containerSize.width - info.padding.horizontal()
							   : containerSize.height - info.padding.vertical());
	float contentCross = (isRow ? containerSize.height - info.padding.vertical()
								: containerSize.width - info.padding.horizontal());
	contentMain = sprt::max(contentMain, 0.0f);
	contentCross = sprt::max(contentCross, 0.0f);

	const float mainGap = isRow ? info.columnGap : info.rowGap;
	const float crossGap = isRow ? info.rowGap : info.columnGap;

	// 1-4. Collect and measure the items, break them into lines, resolve the
	// flexed main sizes and the hypothetical cross sizes (shared with the
	// measurement pass).
	FlexPassOutput pass;
	computeFlexLines(_owner, info, contentMain, contentCross, false, pass);
	auto &items = pass.items;
	auto &lines = pass.lines;

	if (items.empty()) {
		return;
	}

	// 5. Size and position the lines along the cross axis (align-content). A
	// single line always fills the whole content cross size.
	if (lines.size() == 1) {
		lines[0].crossSize = contentCross;
		lines[0].crossStart = 0.0f;
	} else {
		float totalCross = 0.0f;
		for (auto &line : lines) { totalCross += line.crossSize; }
		const float gapsTotal = crossGap * static_cast<float>(lines.size() - 1);
		const float freeCross = contentCross - totalCross - gapsTotal;

		float offset = 0.0f;
		float between = crossGap;
		const float count = static_cast<float>(lines.size());
		switch (info.alignContent) {
		case FlexAlign::FlexEnd: offset = freeCross; break;
		case FlexAlign::Center: offset = freeCross / 2.0f; break;
		case FlexAlign::SpaceBetween:
			between = crossGap + (lines.size() > 1 ? freeCross / (count - 1.0f) : 0.0f);
			break;
		case FlexAlign::SpaceAround: {
			const float space = freeCross / count;
			offset = space / 2.0f;
			between = crossGap + space;
			break;
		}
		case FlexAlign::Stretch:
			if (freeCross > 0.0f) {
				const float add = freeCross / count;
				for (auto &line : lines) { line.crossSize += add; }
			}
			break;
		default: break; // FlexStart / Auto
		}

		float pos = offset;
		for (auto &line : lines) {
			line.crossStart = pos;
			pos += line.crossSize + between;
		}
	}

	// 6. Distribute items along the main axis (justify-content) and align them
	// within their line (align-items / align-self).
	for (auto &line : lines) {
		const size_t n = line.count();

		float usedMain = 0.0f;
		uint32_t autoMainCount = 0;
		for (size_t k = line.begin; k < line.end; ++k) {
			usedMain += items[k].outerMain();
			autoMainCount += items[k].mainAutoCount();
		}
		const float gapTotal = (n > 1) ? mainGap * static_cast<float>(n - 1) : 0.0f;
		float freeMain = sprt::max(contentMain - usedMain - gapTotal, 0.0f);

		// Auto main margins are served first and take everything: they split the free space
		// equally, and `justify-content` is left with nothing to distribute (CSS 9.5). This is
		// what makes `margin-left: auto` push an item to the end and `margin: 0 auto` centre it.
		if (autoMainCount > 0 && freeMain > 0.0f) {
			const float share = freeMain / static_cast<float>(autoMainCount);
			for (size_t k = line.begin; k < line.end; ++k) {
				auto &item = items[k];
				if (item.mainMarginStartAuto) {
					item.mainAutoStart = share;
				}
				if (item.mainMarginEndAuto) {
					item.mainAutoEnd = share;
				}
			}
			freeMain = 0.0f;
		}

		float offset = 0.0f;
		float between = mainGap;
		const float count = static_cast<float>(n);
		switch (info.justifyContent) {
		case FlexJustify::FlexEnd: offset = freeMain; break;
		case FlexJustify::Center: offset = freeMain / 2.0f; break;
		case FlexJustify::SpaceBetween:
			between = mainGap + (n > 1 ? freeMain / (count - 1.0f) : 0.0f);
			break;
		case FlexJustify::SpaceAround: {
			const float space = (n > 0) ? freeMain / count : 0.0f;
			offset = space / 2.0f;
			between = mainGap + space;
			break;
		}
		case FlexJustify::SpaceEvenly: {
			const float space = freeMain / (count + 1.0f);
			offset = space;
			between = mainGap + space;
			break;
		}
		default: break; // FlexStart
		}

		float pos = offset;
		for (size_t k = line.begin; k < line.end; ++k) {
			auto &item = items[k];
			item.mainStart = pos;
			pos += item.outerMain() + between;

			FlexAlign align =
					(item.cfg.alignSelf == FlexAlign::Auto) ? info.alignItems : item.cfg.alignSelf;

			const uint32_t autoCross = item.crossAutoCount();

			const float availCross =
					sprt::max(line.crossSize - item.crossMarginStart - item.crossMarginEnd, 0.0f);
			// stretched items fill the line's cross extent; everyone else keeps their
			// hypothetical cross size. An auto cross margin outranks alignment, stretch
			// included - there would be no free space left for the margin to take otherwise.
			const float cross = (align == FlexAlign::Stretch && autoCross == 0) ? availCross
																				: item.naturalCross;
			item.crossSize = sprt::max(cross, 0.0f);

			float crossPos = 0.0f;
			const float outerCross = item.outerCross();
			if (autoCross > 0) {
				// the item's own auto margins split what is left of its line; one auto margin
				// pushes it to the opposite edge, two centre it
				const float freeCross = sprt::max(line.crossSize - outerCross, 0.0f);
				if (item.crossMarginStartAuto) {
					crossPos = (autoCross == 2) ? freeCross / 2.0f : freeCross;
				}
			} else {
				switch (align) {
				case FlexAlign::FlexEnd: crossPos = line.crossSize - outerCross; break;
				case FlexAlign::Center: crossPos = (line.crossSize - outerCross) / 2.0f; break;
				default: break; // FlexStart / Stretch / Auto
				}
			}
			item.crossStart = line.crossStart + crossPos;
		}
	}

	// 7. Project the flow coordinates onto the node's bottom-left coordinate
	// space and commit position + size to each child.
	for (auto &item : items) {
		float mainBox = item.mainStart;
		if (mainReverse) {
			mainBox = contentMain - (item.mainStart + item.outerMain());
		}
		const float mainPos = mainBox + item.mainMarginStart + item.mainAutoStart;

		float crossBox = item.crossStart;
		if (crossReverse) {
			crossBox = contentCross - (item.crossStart + item.outerCross());
		}
		const float crossPos = crossBox + item.crossMarginStart;

		float width = 0.0f;
		float height = 0.0f;
		Vec2 bottomLeft;
		if (isRow) {
			width = item.mainSize;
			height = item.crossSize;
			bottomLeft.x = info.padding.left + mainPos;
			// cross flows downward from the content-box top edge
			bottomLeft.y = containerSize.height - info.padding.top - crossPos - height;
		} else {
			width = item.crossSize;
			height = item.mainSize;
			bottomLeft.x = info.padding.left + crossPos;
			// main flows downward from the content-box top edge
			bottomLeft.y = containerSize.height - info.padding.top - mainPos - height;
		}

		const Size2 newSize(width, height);
		item.node->setContentSize(newSize);

		// honor the child's own anchor point: position is where the anchor sits
		const Vec2 anchor = item.node->getAnchorPoint();
		item.node->setPosition(bottomLeft + Vec2(anchor.x * width, anchor.y * height));

		if (item.measured) {
			// let the content adapt to the assigned box synchronously (e.g. a
			// label re-wraps to the committed width); the resulting child
			// notifications are suppressed by the _inApply guard
			dispatchLayoutApplied(item.node, newSize);
		}
	}
}

namespace {

// resolve a per-axis line/span placement into a 0-based half-open track interval
static GridSpan resolveGridSpan(uint32_t startLine, uint32_t endLine, uint32_t span) {
	GridSpan out;
	const uint32_t sp = sprt::max(span, 1u);
	if (startLine >= 1 && endLine >= 1) {
		uint32_t s = startLine - 1;
		uint32_t e = endLine - 1;
		if (e < s) {
			const uint32_t t = s;
			s = e;
			e = t;
		}
		if (e <= s) {
			e = s + 1;
		}
		out = {s, e, true};
	} else if (startLine >= 1) {
		const uint32_t s = startLine - 1;
		out = {s, s + sp, true};
	} else if (endLine >= 1) {
		uint32_t e = endLine - 1;
		if (e < 1) {
			e = 1;
		}
		const uint32_t s = (e > sp) ? (e - sp) : 0u;
		out = {s, sprt::max(e, s + 1), true};
	} else {
		out = {0, sp, false};
	}
	return out;
}

// distribution of leftover space along one axis (justify/align-content)
static void gridContentDistribution(GridAlign align, float freeSpace, size_t count, float &offset,
		float &between) {
	offset = 0.0f;
	between = 0.0f;
	if (freeSpace <= 0.0f || count == 0) {
		return;
	}
	const float n = static_cast<float>(count);
	switch (align) {
	case GridAlign::End: offset = freeSpace; break;
	case GridAlign::Center: offset = freeSpace / 2.0f; break;
	case GridAlign::SpaceBetween: between = (count > 1) ? freeSpace / (n - 1.0f) : 0.0f; break;
	case GridAlign::SpaceAround: {
		const float space = freeSpace / n;
		offset = space / 2.0f;
		between = space;
		break;
	}
	case GridAlign::SpaceEvenly: {
		const float space = freeSpace / (n + 1.0f);
		offset = space;
		between = space;
		break;
	}
	default: break; // Start / Stretch / Auto
	}
}

} // namespace

void LayoutSystem::layoutGrid() {
	auto infoPtr = _owner->getComponent<GridLayoutInfo>();
	const GridLayoutInfo info = infoPtr ? *infoPtr : GridLayoutInfo();

	const Size2 containerSize = _owner->getContentSize();
	const float contentW = sprt::max(containerSize.width - info.padding.horizontal(), 0.0f);
	const float contentH = sprt::max(containerSize.height - info.padding.vertical(), 0.0f);

	// 1. Collect items.
	Vector<GridItem> items;
	for (auto &child : _owner->getChildren()) {
		// collapsed when explicitly invisible or `display: none`; a `visibility: hidden`
		// child keeps its layout box (isDisplayed stays true)
		if (!child->isDisplayed()) {
			continue;
		}
		// out of the grid flow entirely, like an absolutely positioned box (OutOfFlowComponent)
		if (child->getComponent<OutOfFlowComponent>()) {
			continue;
		}
		GridItem item;
		item.node = child;
		if (auto cfg = child->getComponent<GridItemInfo>()) {
			item.cfg = *cfg;
		}
		const Size2 cs = intrinsicSize(child);
		item.natW = (item.cfg.width >= 0.0f) ? item.cfg.width : cs.width;
		item.natH = (item.cfg.height >= 0.0f) ? item.cfg.height : cs.height;
		item.col = resolveGridSpan(item.cfg.gridColumnStart, item.cfg.gridColumnEnd,
				item.cfg.columnSpan);
		item.row = resolveGridSpan(item.cfg.gridRowStart, item.cfg.gridRowEnd, item.cfg.rowSpan);
		items.emplace_back(item);
	}
	if (items.empty()) {
		return;
	}

	// stable-sort by order (insertion sort; tiny counts)
	for (size_t i = 1; i < items.size(); ++i) {
		GridItem key = items[i];
		size_t j = i;
		while (j > 0 && items[j - 1].cfg.order > key.cfg.order) {
			items[j] = items[j - 1];
			--j;
		}
		items[j] = key;
	}

	// 2. Placement. Work in minor (fixed, wrapping) / major (growing) axes.
	const bool rowFlow =
			info.autoFlow == GridAutoFlow::Row || info.autoFlow == GridAutoFlow::RowDense;
	const bool dense =
			info.autoFlow == GridAutoFlow::RowDense || info.autoFlow == GridAutoFlow::ColumnDense;

	const uint32_t minorBase = uint32_t(rowFlow ? info.columnTracks.size() : info.rowTracks.size());
	const uint32_t majorBase = uint32_t(rowFlow ? info.rowTracks.size() : info.columnTracks.size());

	// per-item minor / major spans (as pointers into col/row by flow)
	auto minorOf = [&](GridItem &it) -> GridSpan & { return rowFlow ? it.col : it.row; };
	auto majorOf = [&](GridItem &it) -> GridSpan & { return rowFlow ? it.row : it.col; };

	// finalize the minor track count (the wrap dimension)
	uint32_t W = minorBase;
	for (auto &it : items) {
		auto &mn = minorOf(it);
		W = sprt::max(W, mn.definite ? mn.end : mn.span());
	}
	W = sprt::max(W, 1u);

	uint32_t M = majorBase;
	Vector<uint8_t> occ;
	occ.resize(size_t(M) * W, 0);
	auto ensureMajor = [&](uint32_t m) {
		if (m > M) {
			occ.resize(size_t(m) * W, 0);
			M = m;
		}
	};
	auto isFree = [&](uint32_t maj, uint32_t majSpan, uint32_t mn, uint32_t mnSpan) -> bool {
		for (uint32_t a = maj; a < maj + majSpan; ++a) {
			for (uint32_t b = mn; b < mn + mnSpan; ++b) {
				if (occ[size_t(a) * W + b]) {
					return false;
				}
			}
		}
		return true;
	};
	auto mark = [&](uint32_t maj, uint32_t majSpan, uint32_t mn, uint32_t mnSpan) {
		for (uint32_t a = maj; a < maj + majSpan; ++a) {
			for (uint32_t b = mn; b < mn + mnSpan; ++b) { occ[size_t(a) * W + b] = 1; }
		}
	};
	auto commit = [&](GridItem &it, uint32_t maj, uint32_t mn) {
		auto &mnAxis = minorOf(it);
		auto &mjAxis = majorOf(it);
		// capture spans BEFORE mutating start (span() == end - start)
		const uint32_t mnSp = mnAxis.span();
		const uint32_t mjSp = mjAxis.span();
		mnAxis.start = mn;
		mnAxis.end = mn + mnSp;
		mjAxis.start = maj;
		mjAxis.end = maj + mjSp;
	};

	// Phase 1: items definite in both axes.
	for (auto &it : items) {
		auto &mn = minorOf(it);
		auto &mj = majorOf(it);
		if (mn.definite && mj.definite) {
			ensureMajor(mj.end);
			// clamp minor into the grid (definite W already covers it)
			mark(mj.start, mj.span(), mn.start, mn.span());
		}
	}

	// Phases 2 & 3: everything else in DOM/order sequence, with a shared cursor
	// for the fully-auto items.
	uint32_t curMajor = 0, curMinor = 0;
	for (auto &it : items) {
		auto &mn = minorOf(it);
		auto &mj = majorOf(it);
		if (mn.definite && mj.definite) {
			continue; // placed in phase 1
		}
		const uint32_t mnSpan = mn.span();
		const uint32_t mjSpan = mj.span();

		if (mn.definite && !mj.definite) {
			// minor-locked: scan the major axis for a free band
			uint32_t maj = 0;
			while (true) {
				ensureMajor(maj + mjSpan);
				if (isFree(maj, mjSpan, mn.start, mnSpan)) {
					break;
				}
				++maj;
			}
			mark(maj, mjSpan, mn.start, mnSpan);
			commit(it, maj, mn.start);
		} else if (!mn.definite && mj.definite) {
			// major-locked: scan the minor axis within the fixed major band
			ensureMajor(mj.end);
			bool placed = false;
			for (uint32_t b = 0; mnSpan <= W && b + mnSpan <= W; ++b) {
				if (isFree(mj.start, mjSpan, b, mnSpan)) {
					mark(mj.start, mjSpan, b, mnSpan);
					commit(it, mj.start, b);
					placed = true;
					break;
				}
			}
			if (!placed) {
				// overflow: pin to minor 0
				mark(mj.start, mjSpan, 0, mnSpan);
				commit(it, mj.start, 0);
			}
		} else {
			// fully auto: cursor packing
			if (dense) {
				curMajor = 0;
				curMinor = 0;
			}
			while (true) {
				if (curMinor + mnSpan > W) {
					curMinor = 0;
					++curMajor;
				}
				ensureMajor(curMajor + mjSpan);
				if (isFree(curMajor, mjSpan, curMinor, mnSpan)) {
					break;
				}
				++curMinor;
			}
			mark(curMajor, mjSpan, curMinor, mnSpan);
			commit(it, curMajor, curMinor);
			if (!dense) {
				curMinor += mnSpan;
			}
		}
	}

	const uint32_t colCount = rowFlow ? W : M;
	const uint32_t rowCount = rowFlow ? M : W;

	// 3. Track sizing (independent per axis).
	auto buildTracks = [&](uint32_t count, const Vector<GridTrack> &explicitTracks,
							   const GridTrack &autoDefault) {
		Vector<GridTrackSize> tracks;
		tracks.resize(count);
		for (uint32_t i = 0; i < count; ++i) {
			tracks[i].def = (i < explicitTracks.size()) ? explicitTracks[i] : autoDefault;
		}
		return tracks;
	};
	Vector<GridTrackSize> cols = buildTracks(colCount, info.columnTracks, info.autoColumn);
	Vector<GridTrackSize> rows = buildTracks(rowCount, info.rowTracks, info.autoRow);

	auto sizeAxis = [&](Vector<GridTrackSize> &tracks, float axisContent, float gap,
							bool isColumn) {
		const size_t n = tracks.size();
		if (n == 0) {
			return;
		}
		const float gapsTotal = gap * static_cast<float>(n > 0 ? n - 1 : 0);

		// base sizing for Fixed / Percent; Auto and Fraction start at 0
		for (auto &t : tracks) {
			switch (t.def.type) {
			case GridTrack::Fixed: t.base = sprt::max(t.def.value, 0.0f); break;
			case GridTrack::Percent:
				t.base = sprt::max(t.def.value / 100.0f * axisContent, 0.0f);
				break;
			default: t.base = 0.0f; break;
			}
		}

		// content sizing of Auto tracks: single-track items first
		for (auto &it : items) {
			const GridSpan &s = isColumn ? it.col : it.row;
			const float nat = isColumn ? it.natW : it.natH;
			if (s.span() == 1 && s.start < n && tracks[s.start].def.type == GridTrack::Auto) {
				tracks[s.start].base = sprt::max(tracks[s.start].base, nat);
			}
		}
		// spanning items: grow the Auto tracks they cover to cover the deficit
		for (auto &it : items) {
			const GridSpan &s = isColumn ? it.col : it.row;
			const float nat = isColumn ? it.natW : it.natH;
			if (s.span() <= 1 || s.end > n) {
				continue;
			}
			float covered = gap * static_cast<float>(s.span() - 1);
			uint32_t autoCount = 0;
			for (uint32_t t = s.start; t < s.end; ++t) {
				covered += tracks[t].base;
				if (tracks[t].def.type == GridTrack::Auto) {
					++autoCount;
				}
			}
			const float deficit = nat - covered;
			if (deficit > 0.0f && autoCount > 0) {
				const float add = deficit / static_cast<float>(autoCount);
				for (uint32_t t = s.start; t < s.end; ++t) {
					if (tracks[t].def.type == GridTrack::Auto) {
						tracks[t].base += add;
					}
				}
			}
		}

		// distribute positive free space across fr tracks
		float sumBase = 0.0f;
		float sumFr = 0.0f;
		for (auto &t : tracks) {
			sumBase += t.base;
			if (t.def.type == GridTrack::Fraction) {
				sumFr += sprt::max(t.def.value, 0.0f);
			}
		}
		const float freeSpace = axisContent - gapsTotal - sumBase;
		if (freeSpace > 0.0f && sumFr > 0.0f) {
			for (auto &t : tracks) {
				if (t.def.type == GridTrack::Fraction) {
					t.base = freeSpace * sprt::max(t.def.value, 0.0f) / sumFr;
				}
			}
		}
		for (auto &t : tracks) { t.base = sprt::max(t.base, 0.0f); }
	};
	sizeAxis(cols, contentW, info.columnGap, true);
	sizeAxis(rows, contentH, info.rowGap, false);

	// 4. Positioning: track offsets + content distribution.
	auto positionAxis = [&](Vector<GridTrackSize> &tracks, float axisContent, float gap,
								GridAlign contentAlign) {
		const size_t n = tracks.size();
		if (n == 0) {
			return;
		}
		float sumBase = 0.0f;
		for (auto &t : tracks) { sumBase += t.base; }
		const float gapsTotal = gap * static_cast<float>(n - 1);
		const float freeSpace = axisContent - sumBase - gapsTotal;

		float offset = 0.0f, between = 0.0f;
		gridContentDistribution(contentAlign, freeSpace, n, offset, between);

		float pos = offset;
		for (auto &t : tracks) {
			t.position = pos;
			pos += t.base + gap + between;
		}
	};
	positionAxis(cols, contentW, info.columnGap, info.justifyContent);
	positionAxis(rows, contentH, info.rowGap, info.alignContent);

	// 5. Per-item cell rect, self-alignment, and projection to bottom-left space.
	auto selfAlign = [](GridAlign a, GridAlign fallback, float cellStart, float cellSize,
							 float natural, float &outStart, float &outSize) {
		GridAlign use = (a == GridAlign::Auto) ? fallback : a;
		if (use == GridAlign::Stretch || use == GridAlign::Auto) {
			outStart = cellStart;
			outSize = cellSize;
			return;
		}
		outSize = natural;
		switch (use) {
		case GridAlign::End: outStart = cellStart + (cellSize - natural); break;
		case GridAlign::Center: outStart = cellStart + (cellSize - natural) / 2.0f; break;
		default: outStart = cellStart; break; // Start
		}
	};

	for (auto &it : items) {
		if (it.col.end == 0 || it.row.end == 0 || it.col.end > cols.size()
				|| it.row.end > rows.size()) {
			continue; // safety: unplaced / out of range
		}
		const float cellX = cols[it.col.start].position;
		const float cellRight = cols[it.col.end - 1].position + cols[it.col.end - 1].base;
		const float cellY = rows[it.row.start].position;
		const float cellBottom = rows[it.row.end - 1].position + rows[it.row.end - 1].base;

		// inset the cell by the item margin
		float availX = cellX + it.cfg.margin.left;
		float availW = sprt::max((cellRight - cellX) - it.cfg.margin.horizontal(), 0.0f);
		float availY = cellY + it.cfg.margin.top;
		float availH = sprt::max((cellBottom - cellY) - it.cfg.margin.vertical(), 0.0f);

		float x = availX, w = availW, y = availY, h = availH;
		selfAlign(it.cfg.justifySelf, info.justifyItems, availX, availW, it.natW, x, w);
		selfAlign(it.cfg.alignSelf, info.alignItems, availY, availH, it.natH, y, h);
		w = sprt::max(w, 0.0f);
		h = sprt::max(h, 0.0f);

		it.boxX = x;
		it.boxY = y;
		it.boxW = w;
		it.boxH = h;

		Vec2 bottomLeft;
		bottomLeft.x = info.padding.left + it.boxX;
		bottomLeft.y = containerSize.height - info.padding.top - it.boxY - it.boxH;

		it.node->setContentSize(Size2(it.boxW, it.boxH));
		const Vec2 anchor = it.node->getAnchorPoint();
		it.node->setPosition(bottomLeft + Vec2(anchor.x * it.boxW, anchor.y * it.boxH));
	}
}

} // namespace stappler::xenolith::ui
