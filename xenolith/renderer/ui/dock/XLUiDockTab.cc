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

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

bool DockTab::init(NotNull<DockSystem> system, DockNodeHandle frame, StringView panelId) {
	if (!Button::init()) {
		return false;
	}

	_system = system;
	_frame = frame;
	_panelId = panelId.str<Interface>();

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

	// A drag pulls the panel out of its frame. It only begins after DragThreshold points of
	// travel, which is past the tap tolerance, so the tap recognizer on the same listener has
	// normally already given up by then - handleLeftTap still refuses while _dragging, belt and
	// braces, the same guard shape ui::TextInput uses around its drag-selection.
	_listener->addSwipeRecognizer(
			[this](const GestureSwipe &swipe) {
		switch (swipe.event) {
		case GestureEvent::Began: return handleDragBegin(swipe);
		case GestureEvent::Activated: handleDrag(swipe); return true;
		case GestureEvent::Ended: handleDragEnd(false); return true;
		case GestureEvent::Cancelled: handleDragEnd(true); return true;
		}
		return false;
	},
			InputSwipeInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}),
				DockSystem::DefaultDragThreshold, false});

	return true;
}

void DockTab::handleExit() {
	// The tab is leaving the scene for whatever reason - its frame collapsed, the layout was
	// restored, the whole dock was removed. A drag that outlived its own tab has nothing left to
	// commit, so it is aborted here rather than at each of those call sites.
	if (_dragging && _drag) {
		_drag->cancelDrag(this);
	}
	_dragging = false;
	_drag = nullptr;
	Button::handleExit();
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
			if (_system) {
				_system->closePanel(_panelId);
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
	if (_dragging) {
		return false; // this pointer belongs to a drag; a tap on release would be a second action
	}
	if (_system) {
		_system->activatePanel(_panelId);
	}
	return true;
}

bool DockTab::handleDragBegin(const GestureSwipe &swipe) {
	if (_dragging || !_system) {
		return false;
	}

	auto desc = _system->getPanelDescriptor(_panelId);
	if (!desc || !hasFlag(desc->flags, DockPanelFlags::Movable)) {
		return false;
	}

	auto drag = DragSystem::acquireForNode(this);
	if (!drag) {
		return false;
	}

	// The source frame travels with the panel: the drop needs it to recognise the moves that would
	// change nothing, and the tab node may not survive long enough to be asked
	auto payload = Rc<DockPanelPayload>::create();
	payload->panelId = _panelId;
	payload->source = _frame;

	// Only the three fields the ghost draws, copied out. Capturing the descriptor whole would drag
	// its `builder` along - a Function copy for something the ghost never calls
	DockPanelDescriptor ghost;
	ghost.id = desc->id;
	ghost.title = desc->title;
	ghost.icon = desc->icon;

	DragOffer offer;
	offer.local = payload;
	offer.localType = DockPanelPayload::TypeName.str<Interface>();
	offer.label = desc->title.empty() ? desc->id : desc->title;
	// A panel is MOVED between frames, never copied: there is one node with one identity, and the
	// dock keeps it alive across the move precisely so it is not rebuilt
	offer.allowedActions = DragActions::Move;
	offer.defaultAction = DragActions::Move;
	offer.decorator = [ghost = sp::move(ghost)]() -> Rc<Node> {
		return Rc<DockDragGhost>::create(ghost);
	};
	// Inside the dock root, not on the scene content: the ghost takes its look from
	// `dock-drag-ghost` in a stylesheet, and a StyleResolver only ever sees its own subtree
	offer.decoratorParent = _system->getOwner();

	if (!drag->beginDrag(sp::move(offer), Rc<Ref>(this), swipe.getId())) {
		return false;
	}

	_drag = drag;
	_dragging = true;

	// capture: the pointer immediately leaves the tab, and the recognizer has to keep delivering
	_listener->setExclusive();

	_drag->updateDrag(swipe.location(), swipe.input->data.getModifiers());
	return true;
}

void DockTab::handleDrag(const GestureSwipe &swipe) {
	if (_dragging && _drag) {
		_drag->updateDrag(swipe.location(), swipe.input->data.getModifiers());
	}
}

void DockTab::handleDragEnd(bool cancelled) {
	if (!_dragging) {
		return;
	}
	_dragging = false;

	auto drag = _drag;
	_drag = nullptr;
	if (!drag) {
		return;
	}

	if (cancelled) {
		drag->cancelDrag(this);
	} else {
		drag->commitDrag();
	}
}

} // namespace stappler::xenolith::ui
