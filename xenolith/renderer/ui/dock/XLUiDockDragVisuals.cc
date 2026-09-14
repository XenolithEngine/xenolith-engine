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

	// default size before any stylesheet; nothing else sizes a decorator
	setContentSize(DefaultSize);

	/* Default paint: an unstyled Panel is opaque white. Translucent so the drop indicator stays
	visible under the ghost; the alpha is in the fill, so icon and caption stay opaque. Any
	`dock-drag-ghost { background-color }` rule replaces it. */
	setPathColor(Color4B(0x26, 0x26, 0x2E, 0xC8), true);
	setBorderRadius(6.0f);

	// Own flex layout so the ghost looks like a tab without a stylesheet rule. Nothing sizes it; a
	// stylesheet sets `width`/`height` for a size other than the default.
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

	// Apply `width`/`height` from a `dock-drag-ghost` rule. The resolver only stores them as a
	// MeasureComponent hint, since the parent places its own children; a negative axis is unset.
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
