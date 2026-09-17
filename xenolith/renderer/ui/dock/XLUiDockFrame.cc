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

	// Strip and body are one flex layout built here; no SystemManagedLayout, the resolver does not
	// remove layouts it did not create. CSS padding and gaps apply only with `display: flex`; the
	// direction follows the tab bar side and updateFlow re-asserts it.
	addSystem(Rc<LayoutSystem>::create());

	_tabBar = addChild(Rc<DockTabBar>::create(params.tabBarSide), ZOrder(1));
	// `basis = FitContent` sizes the strip from its tabs, the same measurement DockSystem uses as
	// the frame's floor. `order` is explicit because flow follows ZOrder and the strip's ZOrder is
	// higher than the body's.
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

	// Without its own layout the body would leave the parked panel at its creation size; a
	// stretched column makes the panel fill the body minus `dock-frame-body` padding.
	_body->addSystem(Rc<LayoutSystem>::create(FlexLayoutInfo{
		.direction = FlexDirection::Column,
		.alignItems = FlexAlign::Stretch,
	}));

	auto outline = Rc<Panel>::create();
	outline->setType("dock-frame-outline");
	outline->removeStyleClass("xl-ui-panel");
	registerStyleAppliers("dock-frame-outline");
	outline->setAnchorPoint(Anchor::BottomLeft);
	// own paint, so a style reset leaves it transparent instead of the Panel's white
	outline->setPathColor(Color4B(0, 0, 0, 0), true);
	// blended over the content, which may be drawn at the Solid level
	outline->setRenderingLevel(RenderingLevel::Transparent);
	outline->setVisible(false);
	// before parenting, so the flex layout never takes it as an item
	outline->setComponent<OutOfFlowComponent>(OutOfFlowComponent{false});
	_outline = addChild(outline, OutlineZOrder);

	setParams(params);
	return true;
}

void DockFrame::handleContentSizeDirty() {
	Panel::handleContentSizeDirty();
	if (_outline) {
		_outline->setPosition(Vec2::ZERO);
		_outline->setContentSize(_contentSize);
	}
}

void DockFrame::setCurrent(bool value) {
	if (value == _current) {
		return;
	}
	_current = value;

	if (_current) {
		addStyleClass("current");
	} else {
		removeStyleClass("current");
	}

	if (_outline) {
		if (_current) {
			_outline->addStyleClass("current");
		} else {
			_outline->removeStyleClass("current");
		}
		_outline->setVisible(_current);
	}
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
		// displayNone, not visibilityHidden: the flex layout drops the item entirely, so the strip
		// takes the whole frame.
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

	// The strip comes first in the flow for Top/Left, last otherwise. CSS `column` runs top-down
	// (the engine compensates for Y-up), so Bottom is ColumnReverse; the same for Left/Right with
	// rows.
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
