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

	// A box to draw on, before any stylesheet is consulted: see the class comment for why this is
	// the widget's job and not the layout's.
	setContentSize(DefaultSize);

	// Its own flex run, built here for the same reason DockTab builds one: the ghost is the tab's
	// icon-and-title pair again, and it must look like one without an application having written a
	// rule for a widget it did not create. NOTHING SIZES IT, though - the drag system only moves
	// its decorator - so a sheet that wants more than the default box gives it `width`/`height`.
	setComponent<SystemManagedLayout>();
	addSystem(Rc<LayoutSystem>::create(FlexLayoutInfo{
		.direction = FlexDirection::Row,
		.alignItems = FlexAlign::Center,
		.columnGap = 6.0f,
		.padding = Padding(6.0f, 10.0f),
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

void DockDragGhost::handleComponentsDirty(const ComponentMask &mask) {
	Panel::handleComponentsDirty(mask);

	// The size a `dock-drag-ghost { width: …; height: … }` rule asked for. It arrives as the
	// intrinsic HINT a layout would have read - the resolver refuses to commit a size on a node
	// whose parent places its own children - and this is the node that has to read it instead.
	// An axis the sheet said nothing about is negative and keeps whatever is there.
	if (auto measure = getComponent<MeasureComponent>()) {
		auto size = getContentSize();
		if (measure->normal.width > 0.0f) {
			size.width = measure->normal.width;
		}
		if (measure->normal.height > 0.0f) {
			size.height = measure->normal.height;
		}
		setContentSize(size);
	}
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
