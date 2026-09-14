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

// isInlineRtl: the container asks for its own computed CSS `direction`.
#include "XLInheritedStyle.h"

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
	// a column's width to measure a height at; maxOf<float>() where there is none (see step 1)
	float wrapCross = maxOf<float>();

	// the hypothetical cross size comes from measurement at the final main size
	bool fitCross = false;
	// at least one axis was measured -> the item gets handleLayoutApplied on commit
	bool measured = false;

	// margins projected onto the flow (start/end of main and cross axes)
	float mainMarginStart = 0.0f;
	float mainMarginEnd = 0.0f;
	float crossMarginStart = 0.0f;
	float crossMarginEnd = 0.0f;

	// `margin: auto` on the projected sides: zero while sizes resolve, filled from the leftover
	// space afterwards
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

// The container's overflow axes projected onto the flow (main/cross).
struct FlexOverflow {
	bool main = false;
	bool cross = false;
};

} // namespace

// Steps 1-4 of the flex algorithm, shared by layoutFlex and measureFlex: collect and size items,
// sort by `order`, break into lines, flex main sizes, re-measure cross sizes, size the lines.
// `forMeasure` skips grow/shrink and commits nothing. contentMain / contentCross may be
// maxOf<float>() (unconstrained); `ovf` marks the overflow axes (LayoutSystem::setOverflowAxes).
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

		// out-of-flow children (`position: absolute`) are not items (see OutOfFlowComponent)
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

		// auto margins count as zero until the sizes are settled (as in CSS intrinsic sizing)
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

		// The main size is measured for `flex-basis: fit-content`, and for `flex-basis: auto`
		// without a definite main size when the node can measure (a HandleMeasure system or a
		// MeasureComponent).
		const bool canMeasure = LayoutSystem_canMeasure(child);
		const bool measureMain = item.cfg.basis == FlexItemInfo::FitContent
				|| (item.cfg.basis == FlexItemInfo::Auto && canMeasure
						&& !LayoutSystem_hasDefiniteSize(child, isRow));

		// The cross size is re-measured at the final main size under the same rule; a measured
		// main size makes the contentSize-based cross stale, so it implies a re-measure too.
		item.fitCross = item.cfg.crossSize == FlexItemInfo::FitContent
				|| (item.cfg.crossSize == FlexItemInfo::Auto
						&& (measureMain
								|| (canMeasure && !LayoutSystem_hasDefiniteSize(child, !isRow))));
		item.measured = item.fitCross || measureMain;

		/* The width a column gives this item, known before measuring: a column's main size is a
		height, which depends on the wrap width. Use the item's definite width, or the content box
		less cross margins; not on an overflow cross axis, where the box is no bound. */
		const bool boundedCross = contentCross != maxOf<float>();
		float wrapCross = maxOf<float>();
		if (!isRow && boundedCross && !ovf.cross) {
			if (item.cfg.crossSize >= 0.0f) {
				wrapCross = item.cfg.crossSize;
			} else if (LayoutSystem_hasDefiniteSize(child, !isRow)) {
				wrapCross = nodeCross;
			} else {
				wrapCross = sprt::max(contentCross - item.crossMarginStart - item.crossMarginEnd,
						0.0f);
			}
		}
		item.wrapCross = wrapCross;

		if (measureMain) {
			// content sizing -> min(max-content, available main), clamped to [minMain, maxMain]
			MeasureConstraints mc;
			mc.mode = MeasureMode::MaxContent;
			if (wrapCross != maxOf<float>()) {
				// a column's height, at the width the item will have (see above)
				mc.mode = MeasureMode::Normal;
				mc.maxWidth = wrapCross;
			} else if (boundedCross) {
				// bound the cross axis by the content box so nested containers
				// don't measure against infinite cross space
				(isRow ? mc.maxHeight : mc.maxWidth) = contentCross;
			}
			const Size2 m = LayoutSystem::measureNode(child, mc);
			float base = isRow ? m.width : m.height;
			// on an overflow main axis the item keeps its measured size, so it stays scrollable
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
			// an explicit flex-basis wins; otherwise the node's own size stands in for its content
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

	// 2. Reorder by the CSS `order` property; insertion sort keeps document order among equals.
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
				// an overflow main axis never wraps: scrolling wins over wrapping
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

		// An overflow main axis skips flexing only when the content does not fit; otherwise grow
		// still fills the slack (an `overflow: auto` container with a `flex-grow: 1` filler).
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
			// [minMain, maxMain], freeze the clamped ones and redistribute the rest among the
			// others, so the space a clamped item gave up is not lost.
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
				// a column item wraps at the same width its height was measured at
				if (item.wrapCross != maxOf<float>()) {
					mc.maxWidth = item.wrapCross;
				}
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
// Dry run of steps 1-4 without grow/shrink, returning the container's natural size. Called from
// LayoutSystem::measure with a non-null owner in Flex mode.
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
	// overflow flags do not change the natural size, so they are left off
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
	const bool flowReverse = info.direction == FlexDirection::RowReverse
			|| info.direction == FlexDirection::ColumnReverse;

	/* `direction: rtl` reverses the inline axis: xor with the flex reverse flags, since
	`row-reverse` inside rtl lays out left to right. For a column the inline axis is the cross. */
	const bool rtl = isInlineRtl(_owner);
	const bool mainReverse = flowReverse != (isRow && rtl);
	const bool crossReverse = (info.wrap == FlexWrap::WrapReverse) != (!isRow && rtl);

	const Size2 containerSize = _owner->getContentSize();

	// The content box minus padding, in main/cross. Available space (percentages, stretch, the
	// wrap threshold) resolves against it, not against the scrollable extent below.
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
		// an empty container's extent is its padding alone
		_contentExtent = Size2(info.padding.horizontal(), info.padding.vertical());
		return;
	}

	// The extent the rest of the pass distributes space in: the box, or on an overflow axis the
	// larger of the box and the content. Overflowing content leaves zero free space, so
	// justify-content, auto margins and align-content degrade to flex-start.
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

		// auto main margins split all the free space equally, leaving none to `justify-content`
		// (CSS 9.5)
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
			// stretched items fill the line's cross extent, others keep their hypothetical cross
			// size; an auto cross margin disables stretch
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

		// cached unscrolled, so setScrollOffset can re-place the children without re-flexing
		_placement.emplace_back(item.node, bottomLeft);

		// scroll offset is y-down (CSS), the engine is y-up; zero unless a ScrollSystem set it
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

	// What the content occupies, padding included; not floored at the box, so callers can
	// compute the leftover room (the scroll range floors at zero on its own).
	const float fullMain =
			extentMain + (isRow ? info.padding.horizontal() : info.padding.vertical());
	const float fullCross =
			extentCross + (isRow ? info.padding.vertical() : info.padding.horizontal());
	_contentExtent = isRow ? Size2(fullMain, fullCross) : Size2(fullCross, fullMain);
}

} // namespace stappler::xenolith::ui
