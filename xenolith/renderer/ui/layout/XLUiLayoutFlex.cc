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

// CSS Flexible Box placement. A subunit of XLUi.scu.cpp - see XLUiLayoutInternal.h for what this
// shares with the grid and table backends.

#include "XLUiLayoutInternal.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

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

// The container's overflow axes projected onto the flow. Main/cross rather than x/y because
// everything past step 1 works in flow space.
struct FlexOverflow {
	bool main = false;
	bool cross = false;
};

} // namespace

// Steps 1-4 of the flex algorithm, shared by the placement pass (layoutFlex)
// and the measurement pass (LayoutSystem::measure): collect items and resolve
// their base sizes (measuring fit-content ones), sort by `order`, break into
// lines, resolve flexed main sizes, re-measure fit-content cross sizes and
// compute the line cross extents. With `forMeasure` grow/shrink are skipped
// (CSS content sizing ignores them) and nothing is committed to the nodes.
// contentMain / contentCross may be maxOf<float>() (unconstrained axis).
// `ovf` marks the axes the container is allowed to overflow (see LayoutSystem::setOverflowAxes).
static void computeFlexLines(Node *owner, const FlexLayoutInfo &info, float contentMain,
		float contentCross, bool forMeasure, FlexOverflow ovf, FlexPassOutput &out) {
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
			// On an overflow main axis the item keeps its measured content size: clamping it to
			// the available space is exactly what would make the content unreachable instead of
			// scrollable.
			if (boundedMain && !ovf.main) {
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
				// An overflow main axis never wraps: wrapping and scrolling the same axis are
				// contradictory requests, and the scroll wins (nowrap is what makes the line long
				// enough to have something to scroll).
				if (info.wrap != FlexWrap::NoWrap && boundedMain && !ovf.main && count > 0
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

		float sumOuterBase = 0.0f;
		for (size_t k = line.begin; k < line.end; ++k) { sumOuterBase += items[k].outerBaseMain(); }

		// An overflow main axis is freed ONLY when the content genuinely does not fit. While it
		// still fits, the container behaves like any other: grow distributes the slack and
		// justify-content places what is left. Freeing it unconditionally would break every
		// `overflow: auto` container that also holds a `flex-grow: 1` filler.
		const bool overflowing =
				ovf.main && boundedMain && (sumOuterBase + gapTotal) > contentMain + FlexEpsilon;

		if (forMeasure || overflowing) {
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
// Dry run of the flex pass: steps 1-4 with grow/shrink skipped (CSS content sizing), reporting the
// container's natural size. The mode dispatch and the null-owner guard belong to
// LayoutSystem::measure; by the time this runs the owner exists and the mode is Flex.
Size2 LayoutSystem::measureFlex(const MeasureConstraints &c) {
	LayoutSystem_settleChildren(_owner);

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
	// The measurement pass reports the natural size, which is what an overflow axis would be laid
	// out at anyway - so the overflow flags make no difference here and are left off.
	computeFlexLines(_owner, info, contentMain, contentCross, true, FlexOverflow(), pass);

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

	// The BOX: the container's own content box, minus padding, projected onto main/cross. Every
	// "available space" question - percentages, align-items: stretch, the wrap threshold - resolves
	// against this and never against the extent below. That is CSS, where percentages resolve
	// against the scrollport rather than against the scrollable area.
	float boxMain = (isRow ? containerSize.width - info.padding.horizontal()
						   : containerSize.height - info.padding.vertical());
	float boxCross = (isRow ? containerSize.height - info.padding.vertical()
							: containerSize.width - info.padding.horizontal());
	boxMain = sprt::max(boxMain, 0.0f);
	boxCross = sprt::max(boxCross, 0.0f);

	const float mainGap = isRow ? info.columnGap : info.rowGap;
	const float crossGap = isRow ? info.rowGap : info.columnGap;

	const FlexOverflow ovf{isRow ? _overflowX : _overflowY, isRow ? _overflowY : _overflowX};

	// 1-4. Collect and measure the items, break them into lines, resolve the
	// flexed main sizes and the hypothetical cross sizes (shared with the
	// measurement pass).
	FlexPassOutput pass;
	computeFlexLines(_owner, info, boxMain, boxCross, false, ovf, pass);
	auto &items = pass.items;
	auto &lines = pass.lines;

	if (items.empty()) {
		_placement.clear();
		// No content is no content: the padding is all there is. Reporting the container's own size
		// here would tell a caller the box was full when it is empty.
		_contentExtent = Size2(info.padding.horizontal(), info.padding.vertical());
		return;
	}

	// The extent the REST of the pass distributes space in. On an overflow axis it is the larger of
	// the box and what the content actually needed; everywhere else it is the box, unchanged - so
	// nothing about a non-overflowing container moves. With contentMain >= usedMain the free space
	// is zero when overflowing, which degrades justify-content, auto margins and align-content to
	// flex-start on their own, with no special-casing below.
	const float contentMain = ovf.main ? sprt::max(boxMain, pass.usedMain) : boxMain;
	const float contentCross = ovf.cross ? sprt::max(boxCross, pass.usedCross) : boxCross;

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
	_placement.clear();
	_placement.reserve(items.size());
	float extentMain = 0.0f;
	float extentCross = 0.0f;

	for (auto &item : items) {
		extentMain = sprt::max(extentMain, item.mainStart + item.outerMain());
		extentCross = sprt::max(extentCross, item.crossStart + item.outerCross());

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

		// cached UNSCROLLED, so setScrollOffset can re-place the children without re-flexing
		_placement.emplace_back(item.node, bottomLeft);

		// CSS scroll orientation is y-down and the engine's is y-up, so a positive vertical scroll
		// moves the content up. Zero unless an ancestor ScrollSystem set an offset.
		bottomLeft -= Vec2(_scrollOffset.x, -_scrollOffset.y);

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

	// What the content actually occupies, padding included on both sides. NOT floored at the box:
	// the scroll range floors at zero on its own, and a container that reported "at least my own
	// size" could never answer how much room its content left over - which is a question with real
	// callers (a strip that hands the leftover back to the window drag, for one).
	const float fullMain =
			extentMain + (isRow ? info.padding.horizontal() : info.padding.vertical());
	const float fullCross =
			extentCross + (isRow ? info.padding.vertical() : info.padding.horizontal());
	_contentExtent = isRow ? Size2(fullMain, fullCross) : Size2(fullCross, fullMain);
}

} // namespace stappler::xenolith::ui
