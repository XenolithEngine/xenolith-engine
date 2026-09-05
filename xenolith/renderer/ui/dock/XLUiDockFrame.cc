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

#include "XLUiDockFrame.h"
#include "XLUiLayoutSystem.h"
#include "XLUiStyleSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

bool DockFrame::init(const DockFrameParams &params, DockNodeHandle handle) {
	if (!Panel::init()) {
		return false;
	}

	_handle = handle;

	setType("dock-frame");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-dock-frame");
	registerStyleAppliers("dock-frame");

	// DockSystem writes position and content size directly, so the anchor is pinned once here and
	// `position` is simply the frame's bottom-left corner from then on
	setAnchorPoint(Anchor::BottomLeft);

	setComponent<DockFrameComponent>(DockFrameComponent{handle});

	// The strip-then-body arrangement is one flex run, built here rather than expected from a
	// stylesheet. It carries no SystemManagedLayout marker: the resolver only tears down layouts
	// it created itself, so this one survives.
	//
	// A stylesheet refines it only through a rule that ALSO declares `display: flex` - padding and
	// the gaps are read inside the resolver's flex branch, and a rule without `display` never
	// enters it. The DIRECTION stays the widget's own either way: it follows the tab bar's side,
	// and updateFlow re-asserts it after any such refinement.
	addSystem(Rc<LayoutSystem>::create());

	_tabBar = addChild(Rc<DockTabBar>::create(params.tabBarSide), ZOrder(1));
	// Two things are set here that look redundant and are not.
	//
	// `basis = FitContent` is what makes the strip self-size from its tabs, and it is the SAME
	// measurement DockSystem reads as the frame's floor - one source of truth for the strip.
	//
	// `order` fixes the FLOW order explicitly. Flow follows child order, child order follows
	// ZOrder, and the strip wants a higher ZOrder than the body - so without an explicit order the
	// strip would be laid out AFTER the body and a `Top` strip would come out at the bottom.
	LayoutSystem::setItem(_tabBar,
			FlexItemInfo{
				.grow = 0.0f,
				.shrink = 0.0f,
				.basis = FlexItemInfo::FitContent,
				.order = 0,
			});

	_body = addChild(Rc<Node>::create(), ZOrder(0));
	_body->setType("dock-frame-body");
	_body->setAnchorPoint(Anchor::BottomLeft);
	LayoutSystem::setItem(_body,
			FlexItemInfo{
				.grow = 1.0f,
				.shrink = 1.0f,
				.basis = 0.0f,
				.order = 1, // always after the strip in the flow; see the tab bar above
			});

	// The body needs a layout of its own, or the panel parked in it would keep whatever size it
	// was created with - which for a freshly built node is none at all. One stretched column over
	// a single child is the whole job: the panel fills the body, minus whatever padding CSS asks
	// for on `dock-frame-body`.
	_body->addSystem(Rc<LayoutSystem>::create(FlexLayoutInfo{
		.direction = FlexDirection::Column,
		.alignItems = FlexAlign::Stretch,
	}));

	setParams(params);
	return true;
}

void DockFrame::setParams(const DockFrameParams &params) {
	_params = params;
	// the frame's declared name is its CSS #id, so a stylesheet can address one parking place
	setName(_params.name);
	if (_tabBar) {
		_tabBar->setSide(_params.tabBarSide);
	}
	updateFlow();
}

void DockFrame::setCollapsed(bool value) {
	if (value == _collapsed) {
		return;
	}
	_collapsed = value;

	if (_collapsed) {
		addStyleClass("collapsed");
	} else {
		removeStyleClass("collapsed");
	}

	if (_body) {
		/* `displayNone`, and the distinction is the whole of it: `visibilityHidden` keeps the box,
		which would leave the strip sharing the frame with a full-width invisible body and the place
		exactly as wide as it was. The flex run collapses a display:none item outright, so the strip
		becomes the frame - which is what a shut rail IS. */
		_body->setOrUpdateComponent<VisibilityComponent>([&](NotNull<VisibilityComponent> vis) {
			if (vis->displayNone == _collapsed) {
				return false;
			}
			vis->displayNone = _collapsed;
			return true;
		});
	}
}

void DockFrame::updateFlow() {
	auto layout = getSystemByType<LayoutSystem>();
	if (!layout) {
		return;
	}

	// The strip comes FIRST in the flow for a Top or Left side and last otherwise. CSS `column`
	// runs top-down (the engine compensates for the Y-up axis), so Top is a plain column and
	// Bottom is the reversed one; the same for Left/Right and rows.
	FlexDirection direction = FlexDirection::Column;
	switch (_params.tabBarSide) {
	case DockTabBarSide::Top: direction = FlexDirection::Column; break;
	case DockTabBarSide::Bottom: direction = FlexDirection::ColumnReverse; break;
	case DockTabBarSide::Left: direction = FlexDirection::Row; break;
	case DockTabBarSide::Right: direction = FlexDirection::RowReverse; break;
	}

	layout->setInfo(FlexLayoutInfo{
		.direction = direction,
		.alignItems = FlexAlign::Stretch,
	});
}

Rect DockFrame::getTabBarRect() const {
	if (!_tabBar) {
		return Rect::ZERO;
	}
	return Rect(Vec2(_tabBar->getPosition().x, _tabBar->getPosition().y),
			_tabBar->getContentSize());
}

} // namespace stappler::xenolith::ui
