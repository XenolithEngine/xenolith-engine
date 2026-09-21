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

#include "XLUiDockTabBar.h"
#include "XLUiLayoutSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

bool DockTabBar::init(DockTabBarSide side) {
	if (!Panel::init()) {
		return false;
	}

	setType("dock-tab-bar");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-dock-tab-bar");
	registerStyleAppliers("dock-tab-bar");

	setAnchorPoint(Anchor::BottomLeft);

	// Own flex layout; no SystemManagedLayout, the resolver does not remove layouts it did not
	// create. CSS padding, gaps and alignment apply only in a rule that also declares `display:
	// flex`. The direction follows the side and setSide re-asserts it.
	addSystem(Rc<LayoutSystem>::create());

	setSide(side);
	return true;
}

void DockTabBar::setSide(DockTabBarSide side) {
	_side = side;

	const bool vertical = isVertical();
	if (vertical) {
		removeStyleClass("horizontal");
		addStyleClass("vertical");
	} else {
		removeStyleClass("vertical");
		addStyleClass("horizontal");
	}

	if (auto layout = getSystemByType<LayoutSystem>()) {
		layout->setInfo(FlexLayoutInfo{
			.direction = vertical ? FlexDirection::Column : FlexDirection::Row,
			.wrap = FlexWrap::NoWrap,
			.alignItems = FlexAlign::Stretch,
		});
	}

	// the tabs already in the strip changed kind along with it; see applyOrientation
	for (auto &it : _tabs) { applyOrientation(it); }
}

// Mark every node below `node` content-size dirty so the recursive StyleResolver resolves them
// again; a style-only change moves no geometry and would not trigger it otherwise.
static void DockTabBar_restyleSubtree(Node *node) {
	for (auto &child : node->getChildren()) {
		child->markContentSizeDirty();
		DockTabBar_restyleSubtree(child);
	}
}

void DockTabBar::applyOrientation(DockTab *tab) const {
	const bool vertical = isVertical();
	const auto want = vertical ? StringView("vertical") : StringView("horizontal");
	if (tab->hasStyleClass(want)) {
		return; // already this kind: setTabs runs on every layout pass, and this must be a no-op
	}

	tab->removeStyleClass(vertical ? StringView("horizontal") : StringView("vertical"));
	tab->addStyleClass(want);

	// What a caption does with a width it cannot have is a property of the KIND of strip, not of
	// the tab, and a tab dragged across changes kind: see DockTab::setVerticalCaption.
	tab->setVerticalCaption(vertical);

	// Restyle the tab's children too: rules like `dock-tab.vertical > label` target them, and the
	// resolver does not revisit a child whose own identity did not change.
	DockTabBar_restyleSubtree(tab);
}

void DockTabBar::setTabs(SpanView<DockTab *> tabs) {
	// remove tabs no longer wanted, keep the rest: a kept tab must not lose its node (hover state,
	// a drag in flight) to a reorder
	for (auto &it : _tabs) {
		bool kept = false;
		for (auto &next : tabs) {
			if (next == it) {
				kept = true;
				break;
			}
		}
		if (!kept && it->getParent() == this) {
			it->removeFromParent(true);
		}
	}

	_tabs = Vector<DockTab *>(tabs.begin(), tabs.end());

	ZOrder z = ZOrder(1);
	for (auto &it : _tabs) {
		if (it->getParent() != this) {
			addChild(it, z);
		} else {
			reorderChild(it, z);
		}
		// tabs neither grow nor shrink, so the strip's measured size is the frame's floor
		LayoutSystem::setItem(it,
				FlexItemInfo{
					.grow = 0.0f,
					.shrink = 0.0f,
					.basis = FlexItemInfo::FitContent,
				});
		// a tab arriving from another strip carries that strip's orientation
		applyOrientation(it);
		z = ZOrder(z.get() + 1);
	}
}

size_t DockTabBar::indexForPosition(const Vec2 &point) const {
	// The slot a drop at `point` lands in: the first tab whose midpoint the point has not reached.
	const bool vertical = isVertical();
	for (size_t i = 0; i < _tabs.size(); ++i) {
		auto tab = _tabs[i];
		const auto pos = tab->getPosition();
		const auto size = tab->getContentSize();
		if (vertical) {
			// Y points up, so the strip runs top-down: the first tab has the highest Y
			const float middle = pos.y + size.height / 2.0f;
			if (point.y > middle) {
				return i;
			}
		} else {
			const float middle = pos.x + size.width / 2.0f;
			if (point.x < middle) {
				return i;
			}
		}
	}
	return _tabs.size();
}

Rect DockTabBar::caretRectForIndex(size_t index) const {
	static constexpr float CaretThickness = 2.0f;

	const auto size = getContentSize();
	const bool vertical = isVertical();

	if (_tabs.empty()) {
		return vertical ? Rect(0.0f, size.height - CaretThickness, size.width, CaretThickness)
						: Rect(0.0f, 0.0f, CaretThickness, size.height);
	}

	if (index >= _tabs.size()) {
		// after the last one: the trailing edge of the strip's content
		auto last = _tabs.back();
		return vertical
				? Rect(0.0f, last->getPosition().y - CaretThickness, size.width, CaretThickness)
				: Rect(last->getPosition().x + last->getContentSize().width, 0.0f, CaretThickness,
						  size.height);
	}

	auto tab = _tabs[index];
	return vertical ? Rect(0.0f, tab->getPosition().y + tab->getContentSize().height, size.width,
							  CaretThickness)
					: Rect(tab->getPosition().x, 0.0f, CaretThickness, size.height);
}

} // namespace stappler::xenolith::ui
