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

#include "XLUiDockDragVisuals.h"
#include "XLUiDockSystem.h"
#include "XLUiLayoutSystem.h"
#include "XLUiStyleSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

bool DockDragGhost::init(const DockPanelDescriptor &desc) {
	if (!Panel::init()) {
		return false;
	}

	setType("dock-drag-ghost");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-dock-drag-ghost");
	registerStyleAppliers("dock-drag-ghost");

	// under the pointer, not beside it
	setAnchorPoint(Anchor::Middle);

	setComponent<SystemManagedLayout>();
	addSystem(Rc<LayoutSystem>::create(FlexLayoutInfo{
		.direction = FlexDirection::Row,
		.alignItems = FlexAlign::Center,
	}));

	if (desc.icon != IconName::None) {
		_icon = addChild(Rc<basic2d::IconSprite>::create(desc.icon), ZOrder(1));
		_icon->setType("icon");
	}

	_label = addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	_label->setType("label");
	_label->setString(desc.title.empty() ? desc.id : desc.title);

	return true;
}

bool DockDropIndicator::init() {
	if (!Panel::init()) {
		return false;
	}

	setType("dock-drop-indicator");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-dock-drop-indicator");
	registerStyleAppliers("dock-drop-indicator");

	setAnchorPoint(Anchor::BottomLeft);
	setVisible(false);
	return true;
}

void DockDropIndicator::setZoneClass(StringView zone) {
	if (_zone == zone) {
		return;
	}
	if (!_zone.empty()) {
		removeStyleClass(_zone);
	}
	_zone = zone.str<Interface>();
	if (!_zone.empty()) {
		addStyleClass(_zone);
	}
}

void DockDropIndicator::setTarget(const DockDropTarget &target) {
	if (target.kind == DockDropTarget::Kind::None) {
		setVisible(false);
		return;
	}

	setVisible(true);
	setPosition(target.highlight.origin);
	setContentSize(target.highlight.size);

	switch (target.kind) {
	case DockDropTarget::Kind::TabStrip: setZoneClass("caret"); break;
	case DockDropTarget::Kind::Center: setZoneClass("center"); break;
	default: setZoneClass("split"); break;
	}
}

} // namespace stappler::xenolith::ui
