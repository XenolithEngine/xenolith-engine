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

	// left-aligned title, unlike Button's centered caption, as ui::MenuItem does for rows
	if (_label) {
		_label->setAlignment(font::TextAlign::Left);
	}

	// The drag begins after DragThreshold points, past the tap tolerance; handleLeftTap also
	// refuses while _dragging.
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
	// a drag that outlives its handle (frame collapsed, layout restored, container removed) is
	// aborted
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

	// Use where the press started: GestureSwipe::firstTouch is updated to the current point on
	// every event, so at Began it is already a threshold away and may be outside a small grip.
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

	// the origin lets a drop detect no-op moves; this node may not survive to be asked
	auto payload = Rc<DockPanelPayload>::create();
	payload->panelId = _panelId;
	payload->host = _host;
	payload->hostRef = _host->getPanelHostRef();

	// copy only what the ghost draws, not the descriptor's `builder`
	DockPanelDescriptor ghost;
	ghost.id = desc->id;
	ghost.title = desc->title;
	ghost.icon = desc->icon;

	DragOffer offer;
	offer.local = payload;
	offer.localType = DockPanelPayload::TypeName.str<Interface>();
	offer.label = desc->title.empty() ? desc->id : desc->title;
	// always a move: one node, kept alive by the registry across the move
	offer.allowedActions = DragActions::Move;
	offer.defaultAction = DragActions::Move;
	offer.decorator = [ghost = sp::move(ghost)]() -> Rc<Node> {
		return Rc<DockDragGhost>::create(ghost);
	};
	// inside the host's StyleResolver subtree, so the `dock-drag-ghost` rule applies
	offer.decoratorParent = _host->getPanelDecoratorParent();

	updatePanelDragOffer(offer, *payload);

	if (!drag->beginDrag(sp::move(offer), Rc<Ref>(this), swipe.getId())) {
		return false;
	}

	_drag = drag;
	_dragging = true;

	// capture: the pointer leaves this node immediately and the recognizer must keep delivering
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
