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

#include "XLUiDockTab.h"
#include "XLUiDockSystem.h"
#include "XLUiLayoutSystem.h"
#include "XLUiTooltipSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

bool DockTab::init(NotNull<DockSystem> system, DockNodeHandle frame, StringView panelId) {
	if (!PanelHandle::init(system, panelId)) {
		return false;
	}

	_frame = frame;

	setType("dock-tab");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-dock-tab");
	registerStyleAppliers("dock-tab");

	setAnchorPoint(Anchor::BottomLeft);

	// The flex layout is built here so the tab sizes itself from its title without a stylesheet
	// rule; the strip and frame floors derive from that measurement. No SystemManagedLayout: the
	// resolver does not remove layouts it did not create. CSS padding and gaps apply only in a rule
	// that also declares `display: flex`.
	addSystem(Rc<LayoutSystem>::create(FlexLayoutInfo{
		.direction = FlexDirection::Row,
		.justifyContent = FlexJustify::Center,
		.alignItems = FlexAlign::Center,
		.columnGap = 6.0f,
		.padding = Padding(4.0f, 10.0f),
	}));

	return true;
}

void DockTab::setString(StringView value) {
	Button::setString(value);

	// The title is also the tooltip, so a tab whose label a stylesheet hides (an icon rail) stays
	// readable, including after being dragged in from a labelled strip.
	if (value.empty()) {
		removeTooltip(this);
	} else {
		setTooltip(this, value);
	}
}

void DockTab::setActive(bool value) {
	if (value == _active) {
		return;
	}
	_active = value;
	if (_active) {
		addStyleClass("active");
	} else {
		removeStyleClass("active");
	}
}

void DockTab::setClosable(bool value) {
	if (value && !_close) {
		_close = addChild(Rc<Button>::create([this] {
			if (_host) {
				_host->closePanel(_panelId);
			}
		}),
				ZOrder(3));
		_close->setType("dock-tab-close");
		_close->addStyleClass("xl-ui-dock-tab-close");
		_close->setIcon(IconName::Navigation_close_solid);
	} else if (!value && _close) {
		_close->removeFromParent(true);
		_close = nullptr;
	}
}

bool DockTab::handleLeftTap() {
	if (isDragging()) {
		return false; // this pointer belongs to a drag; a tap on release would be a second action
	}
	if (_host) {
		// activate first, so a tap handler already sees this panel in front
		_host->activatePanel(_panelId);
		_host->handlePanelTapped(_panelId);
	}
	return true;
}

void DockTab::updatePanelDragOffer(DragOffer &, DockPanelPayload &payload) {
	// the source frame lets a drop detect no-op moves without this node, which the drop may destroy
	payload.source = _frame;
}

} // namespace stappler::xenolith::ui
