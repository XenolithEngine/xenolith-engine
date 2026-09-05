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

#include "XLUiPanelHandle.h"
#include "XLUiDockDragVisuals.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

bool PanelHandle::init(NotNull<PanelHost> host, StringView panelId) {
	if (!Button::init()) {
		return false;
	}

	_host = host;
	_panelId = panelId.str<Interface>();

	// A drag pulls the panel out of wherever it is parked. It only begins after DragThreshold points
	// of travel, which is past the tap tolerance, so the tap recognizer on the same listener has
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
				DragSystem::DefaultDragThreshold, false});

	return true;
}

void PanelHandle::handleExit() {
	// The handle is leaving the scene for whatever reason - its frame collapsed, the layout was
	// restored, the whole container was removed. A drag that outlived its own handle has nothing
	// left to commit, so it is aborted here rather than at each of those call sites.
	if (_dragging && _drag) {
		_drag->cancelDrag(this);
	}
	_dragging = false;
	_drag = nullptr;
	Button::handleExit();
}

bool PanelHandle::handleDragBegin(const GestureSwipe &swipe) {
	if (_dragging || !_host) {
		return false;
	}

	// Where the press STARTED. NOT GestureSwipe::firstTouch, whose name says otherwise: the
	// recognizer assigns it the CURRENT point on every event (XLGestureRecognizer.cc, renewEvent),
	// so by the time Began fires it is already a threshold's travel away from the press - which for
	// a grab point smaller than the threshold means it has left it. `originalLocation` is the press.
	if (swipe.input && !canBeginDragAt(swipe.input->originalLocation)) {
		return false;
	}

	auto desc = _host->getPanelDescriptor(_panelId);
	if (!desc || !hasFlag(desc->flags, DockPanelFlags::Movable)) {
		return false;
	}

	auto drag = DragSystem::acquireForNode(this);
	if (!drag) {
		return false;
	}

	// The origin travels with the panel: a drop needs it to recognise the moves that would change
	// nothing, and this node may not survive long enough to be asked.
	auto payload = Rc<DockPanelPayload>::create();
	payload->panelId = _panelId;
	payload->host = _host;
	payload->hostRef = _host->getPanelHostRef();

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
	// A panel is MOVED between containers, never copied: there is one node with one identity, and
	// the registry keeps it alive across the move precisely so it is not rebuilt
	offer.allowedActions = DragActions::Move;
	offer.defaultAction = DragActions::Move;
	offer.decorator = [ghost = sp::move(ghost)]() -> Rc<Node> {
		return Rc<DockDragGhost>::create(ghost);
	};
	// Inside the host's own subtree, not on the scene content: the ghost takes its look from
	// `dock-drag-ghost` in a stylesheet, and a StyleResolver only ever sees its own subtree
	offer.decoratorParent = _host->getPanelDecoratorParent();

	updatePanelDragOffer(offer, *payload);

	if (!drag->beginDrag(sp::move(offer), Rc<Ref>(this), swipe.getId())) {
		return false;
	}

	_drag = drag;
	_dragging = true;

	// capture: the pointer immediately leaves this node, and the recognizer has to keep delivering
	_listener->setExclusive();

	_drag->updateDrag(swipe.location(), swipe.input->data.getModifiers());
	return true;
}

void PanelHandle::handleDrag(const GestureSwipe &swipe) {
	if (_dragging && _drag) {
		_drag->updateDrag(swipe.location(), swipe.input->data.getModifiers());
	}
}

void PanelHandle::handleDragEnd(bool cancelled) {
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
