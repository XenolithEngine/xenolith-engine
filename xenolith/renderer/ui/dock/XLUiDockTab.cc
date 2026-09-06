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

	// A tab has to size itself from its own title, and a ui::Button does not: like every button in
	// this kit it is arranged by a flex layout, which normally comes from `display: flex` in a
	// stylesheet. The dock cannot require an application to write that rule for a widget it did
	// not create, so the layout is built here - and with it the measurement protocol, through
	// which the strip and then the whole frame derive their floor from the actual titles.
	//
	// It carries NO SystemManagedLayout marker on purpose: the resolver only ever tears down a
	// layout it created itself, so this one survives untouched. A stylesheet can still refine it,
	// but only through a rule that ALSO declares `display: flex` - padding and the gaps are read
	// inside the resolver's flex branch, and a rule without `display` never enters it. So the
	// padding below is what an unstyled tab gets, not a placeholder a bare `dock-tab { padding }`
	// would override.
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

	// The title becomes the tab's HINT as well, because the title is the first thing a stylesheet
	// takes away: an icon rail is `dock-tab.vertical > label { display: none }` and what is left
	// on screen is a glyph with nothing to read. Declaring the hint HERE rather than leaving it to
	// the application is what makes the two kinds of strip interchangeable - a tab dragged from a
	// labelled strip into a rail must not arrive anonymous, and the application that dragged it
	// never touched either node.
	//
	// It costs a component and the scene's hover delay, and it shows nothing until a pointer comes
	// to rest, so the labelled strip pays no visible price for it.
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
		// Activation first, so a tap handler sees the panel it asked for already in front - a rail
		// that unfolds on this tap unfolds onto the right body.
		_host->activatePanel(_panelId);
		_host->handlePanelTapped(_panelId);
	}
	return true;
}

void DockTab::updatePanelDragOffer(DragOffer &, DockPanelPayload &payload) {
	// The source frame, so a drop can recognise the no-ops - dropping a frame's only panel back
	// into that same frame - without asking this node, which the drop may well destroy.
	payload.source = _frame;
}

} // namespace stappler::xenolith::ui
