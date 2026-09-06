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

	// Its own flex run, built here rather than expected from a stylesheet. No SystemManagedLayout
	// marker: the resolver never tears down a layout it did not create, so this one survives.
	//
	// A stylesheet can still refine it, but only through a rule that ALSO declares `display: flex`
	// - padding, the gaps and the alignment are read inside the resolver's flex branch, and a rule
	// without `display` never enters it. `dock-tab-bar { display: flex; padding: 2px; }` works;
	// `dock-tab-bar { padding: 2px; }` alone is silently ignored.
	//
	// Only the DIRECTION is the widget's own business - it follows the strip's side - and setSide
	// re-asserts it after any such refinement.
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

// Re-run the content-size phase of everything below `node`. That is what makes the nearest
// recursive StyleResolver resolve those nodes again - the same thing the resolver does to itself
// when a stylesheet reloads, and for the same reason: a style-only change moves no geometry, so a
// descendant would otherwise never signal and would keep its stale style until some unrelated
// relayout happened to wake it.
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

	/* And re-arm what is INSIDE the tab.

	The class flipped on the tab, but the rules that make an icon rail what it is are written for
	its children - `dock-tab.vertical > label { display: none }` is the whole trick. A recursive
	resolver re-resolves a descendant when THAT descendant's own phase fires, and a label whose own
	identity did not change has no reason to fire one; without this a strip could turn on its side
	while its tabs kept the kind they were built with. */
	DockTabBar_restyleSubtree(tab);
}

void DockTabBar::setTabs(SpanView<DockTab *> tabs) {
	// take out whatever is no longer wanted, keep the rest: a tab that stays parked here must not
	// lose its node - and with it its hover state and any drag in flight - to a reorder
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
		// each tab is sized by its own content and neither grows nor shrinks: the strip is as long
		// as its tabs, which is what makes its measured size the frame's floor
		LayoutSystem::setItem(it,
				FlexItemInfo{
					.grow = 0.0f,
					.shrink = 0.0f,
					.basis = FlexItemInfo::FitContent,
				});
		// a tab that has just ARRIVED from another strip carries that strip's kind
		applyOrientation(it);
		z = ZOrder(z.get() + 1);
	}
}

size_t DockTabBar::indexForPosition(const Vec2 &point) const {
	// The slot a drop at `point` would land in: the first tab whose midpoint the point has not
	// reached yet. Comparing against midpoints rather than edges is what makes the caret flip over
	// exactly when the pointer passes the middle of a tab.
	const bool vertical = isVertical();
	for (size_t i = 0; i < _tabs.size(); ++i) {
		auto tab = _tabs[i];
		const auto pos = tab->getPosition();
		const auto size = tab->getContentSize();
		if (vertical) {
			// Y points up, so the strip runs from the TOP down: the first tab has the highest Y
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
