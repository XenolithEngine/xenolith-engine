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

#include "XLSimpleFlexLayout.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::simpleui {

// Components acquire their unique ids statically during program startup.
ComponentId FlexLayoutInfo::Id;
ComponentId FlexItemInfo::Id;

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

	// margins projected onto the flow (start/end of main and cross axes)
	float mainMarginStart = 0.0f;
	float mainMarginEnd = 0.0f;
	float crossMarginStart = 0.0f;
	float crossMarginEnd = 0.0f;

	// margin-box start positions in the flow (0 == start of the content box / line)
	float mainStart = 0.0f;
	float crossStart = 0.0f;

	float outerMain() const { return mainMarginStart + mainSize + mainMarginEnd; }
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

} // namespace

bool FlexLayout::init() {
	if (!System::init()) {
		return false;
	}

	// We need node geometry events (own content size, child reordering) and
	// component change events (the container's own FlexLayoutInfo updates).
	setSystemFlags(SystemFlags::HandleNodeEvents | SystemFlags::HandleComponents
			| SystemFlags::HandleSceneEvents);
	return true;
}

bool FlexLayout::init(const FlexLayoutInfo &info) {
	if (!init()) {
		return false;
	}
	_initialInfo = info;
	return true;
}

void FlexLayout::handleAdded(Node *owner) {
	System::handleAdded(owner);

	// Make sure the container always carries a FlexLayoutInfo component, so the
	// "parameters live on the parent node" contract holds even if the caller
	// never sets one explicitly.
	if (!owner->getComponent<FlexLayoutInfo>()) {
		owner->setComponent<FlexLayoutInfo>(_initialInfo);
	}
}

void FlexLayout::handleContentSizeDirty() {
	System::handleContentSizeDirty();
	apply();
}

void FlexLayout::handleComponentsDirty() {
	System::handleComponentsDirty();
	apply();
}

void FlexLayout::handleReorderChildDirty() {
	System::handleReorderChildDirty();
	apply();
}

const FlexLayoutInfo *FlexLayout::getInfo() const {
	if (!_owner) {
		return nullptr;
	}
	return _owner->getComponent<FlexLayoutInfo>();
}

void FlexLayout::setInfo(const FlexLayoutInfo &info) {
	if (!_owner) {
		_initialInfo = info;
		return;
	}
	_owner->setComponent<FlexLayoutInfo>(info);
}

void FlexLayout::updateInfo(const Callback<bool(FlexLayoutInfo &)> &cb) {
	if (!_owner) {
		cb(_initialInfo);
		return;
	}
	_owner->setOrUpdateComponent<FlexLayoutInfo>(
			[&](NotNull<FlexLayoutInfo> info) { return cb(*info); });
}

void FlexLayout::setDirection(FlexDirection value) {
	updateInfo([&](FlexLayoutInfo &info) {
		if (info.direction == value) {
			return false;
		}
		info.direction = value;
		return true;
	});
}

void FlexLayout::setWrap(FlexWrap value) {
	updateInfo([&](FlexLayoutInfo &info) {
		if (info.wrap == value) {
			return false;
		}
		info.wrap = value;
		return true;
	});
}

void FlexLayout::setJustifyContent(FlexJustify value) {
	updateInfo([&](FlexLayoutInfo &info) {
		if (info.justifyContent == value) {
			return false;
		}
		info.justifyContent = value;
		return true;
	});
}

void FlexLayout::setAlignItems(FlexAlign value) {
	updateInfo([&](FlexLayoutInfo &info) {
		if (info.alignItems == value) {
			return false;
		}
		info.alignItems = value;
		return true;
	});
}

void FlexLayout::setAlignContent(FlexAlign value) {
	updateInfo([&](FlexLayoutInfo &info) {
		if (info.alignContent == value) {
			return false;
		}
		info.alignContent = value;
		return true;
	});
}

void FlexLayout::setGap(float row, float column) {
	updateInfo([&](FlexLayoutInfo &info) {
		if (info.rowGap == row && info.columnGap == column) {
			return false;
		}
		info.rowGap = row;
		info.columnGap = column;
		return true;
	});
}

void FlexLayout::setPadding(Padding value) {
	updateInfo([&](FlexLayoutInfo &info) {
		if (info.padding == value) {
			return false;
		}
		info.padding = value;
		return true;
	});
}

const FlexItemInfo *FlexLayout::getItem(NotNull<Node> node) {
	return node->getComponent<FlexItemInfo>();
}

void FlexLayout::setItem(NotNull<Node> node, const FlexItemInfo &info) {
	node->setComponent<FlexItemInfo>(info);
}

void FlexLayout::apply() {
	if (!_owner) {
		return;
	}

	auto infoPtr = _owner->getComponent<FlexLayoutInfo>();
	const FlexLayoutInfo info = infoPtr ? *infoPtr : FlexLayoutInfo();

	const bool isRow = info.direction == FlexDirection::Row
			|| info.direction == FlexDirection::RowReverse;
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

	// 1. Collect in-flow items and project their parameters onto the flow axes.
	Vector<FlexItem> items;
	for (auto &child : _owner->getChildren()) {
		if (!child->isVisible()) {
			continue;
		}

		FlexItem item;
		item.node = child;
		if (auto cfg = child->getComponent<FlexItemInfo>()) {
			item.cfg = *cfg;
		}

		const Size2 cs = child->getContentSize();
		const float nodeMain = isRow ? cs.width : cs.height;
		const float nodeCross = isRow ? cs.height : cs.width;
		// flex-basis: an explicit value wins, otherwise fall back to the node's size
		item.baseMain = (item.cfg.basis >= 0.0f) ? item.cfg.basis : nodeMain;
		// hypothetical cross size used for line sizing and non-stretch alignment
		item.naturalCross = (item.cfg.crossSize >= 0.0f) ? item.cfg.crossSize : nodeCross;

		if (isRow) {
			// main is horizontal, cross is vertical (cross-start == top)
			item.mainMarginStart = item.cfg.margin.left;
			item.mainMarginEnd = item.cfg.margin.right;
			item.crossMarginStart = item.cfg.margin.top;
			item.crossMarginEnd = item.cfg.margin.bottom;
		} else {
			// main is vertical (main-start == top), cross is horizontal
			item.mainMarginStart = item.cfg.margin.top;
			item.mainMarginEnd = item.cfg.margin.bottom;
			item.crossMarginStart = item.cfg.margin.left;
			item.crossMarginEnd = item.cfg.margin.right;
		}

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
	Vector<FlexLine> lines;
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
				if (info.wrap != FlexWrap::NoWrap && count > 0
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
		float sumGrow = 0.0f;
		float sumShrinkScaled = 0.0f;
		for (size_t k = line.begin; k < line.end; ++k) {
			sumOuterBase += items[k].outerBaseMain();
			sumGrow += items[k].cfg.grow;
			sumShrinkScaled += items[k].cfg.shrink * items[k].baseMain;
		}

		const float freeMain = contentMain - sumOuterBase - gapTotal;

		for (size_t k = line.begin; k < line.end; ++k) {
			auto &item = items[k];
			float size = item.baseMain;
			if (freeMain > 0.0f && sumGrow > 0.0f) {
				size = item.baseMain + (item.cfg.grow / sumGrow) * freeMain;
			} else if (freeMain < 0.0f && sumShrinkScaled > 0.0f) {
				const float ratio = (item.cfg.shrink * item.baseMain) / sumShrinkScaled;
				size = item.baseMain + ratio * freeMain; // freeMain is negative
			}

			size = sprt::max(size, item.cfg.minMain);
			if (item.cfg.maxMain >= 0.0f) {
				size = sprt::min(size, item.cfg.maxMain);
			}
			item.mainSize = sprt::max(size, 0.0f);
		}

		float maxCross = 0.0f;
		for (size_t k = line.begin; k < line.end; ++k) {
			maxCross = sprt::max(maxCross, items[k].outerNaturalCross());
		}
		line.crossSize = maxCross;
	}

	// 5. Size and position the lines along the cross axis (align-content). A
	// single line always fills the whole content cross size.
	if (lines.size() == 1) {
		lines[0].crossSize = contentCross;
		lines[0].crossStart = 0.0f;
	} else {
		float totalCross = 0.0f;
		for (auto &line : lines) {
			totalCross += line.crossSize;
		}
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
				for (auto &line : lines) {
					line.crossSize += add;
				}
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
		for (size_t k = line.begin; k < line.end; ++k) {
			usedMain += items[k].outerMain();
		}
		const float gapTotal = (n > 1) ? mainGap * static_cast<float>(n - 1) : 0.0f;
		float freeMain = sprt::max(contentMain - usedMain - gapTotal, 0.0f);

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

			FlexAlign align = (item.cfg.alignSelf == FlexAlign::Auto) ? info.alignItems
																	  : item.cfg.alignSelf;

			const float availCross = sprt::max(
					line.crossSize - item.crossMarginStart - item.crossMarginEnd, 0.0f);
			// stretched items fill the line's cross extent; everyone else keeps
			// their hypothetical cross size
			const float cross = (align == FlexAlign::Stretch) ? availCross : item.naturalCross;
			item.crossSize = sprt::max(cross, 0.0f);

			float crossPos = 0.0f;
			const float outerCross = item.outerCross();
			switch (align) {
			case FlexAlign::FlexEnd: crossPos = line.crossSize - outerCross; break;
			case FlexAlign::Center: crossPos = (line.crossSize - outerCross) / 2.0f; break;
			default: break; // FlexStart / Stretch / Auto
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
		const float mainPos = mainBox + item.mainMarginStart;

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
		item.node->setPosition(
				bottomLeft + Vec2(anchor.x * width, anchor.y * height));
	}
}

} // namespace stappler::xenolith::simpleui
