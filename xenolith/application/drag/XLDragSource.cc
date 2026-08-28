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

#include "XLDragSource.h"
#include "XLNode.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

bool DragSource::init(OfferBuilder &&builder, float threshold) {
	if (!InputListener::init(0)) {
		return false;
	}

	_builder = sp::move(builder);

	addSwipeRecognizer(
			[this](const GestureSwipe &swipe) {
		switch (swipe.event) {
		case GestureEvent::Began: return handleDragBegin(swipe);
		case GestureEvent::Activated: handleDragMove(swipe); return true;
		case GestureEvent::Ended: handleDragEnd(false); return true;
		case GestureEvent::Cancelled: handleDragEnd(true); return true;
		}
		return false;
	},
			InputSwipeInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}),
				threshold, false});

	return true;
}

void DragSource::handleExit() {
	// The node is leaving the scene with the button still down - which is the NORMAL case, since
	// the drop that ends the drag routinely removes the source. Aborting here rather than at each
	// of those call sites is what keeps this from being everybody's problem
	if (_dragging && _drag) {
		_drag->cancelDrag(this);
	}
	_dragging = false;
	_drag = nullptr;

	InputListener::handleExit();
}

void DragSource::setOfferBuilder(OfferBuilder &&builder) { _builder = sp::move(builder); }

DragSession *DragSource::getSession() const {
	// Guarded on _dragging as well as on the system: the system outlives any one drag, and its
	// session may by now belong to somebody else entirely.
	return (_dragging && _drag) ? _drag->getSession() : nullptr;
}

bool DragSource::handleDragBegin(const GestureSwipe &swipe) {
	if (_dragging || !_owner || !_builder) {
		return false;
	}

	DragOffer offer;
	if (!_builder(offer)) {
		return false; // not draggable right now; a plain refusal, not an error
	}

	auto drag = DragSystem::acquireForNode(_owner);
	if (!drag) {
		return false;
	}

	if (!drag->beginDrag(sp::move(offer), Rc<Ref>(this), swipe.getId())) {
		return false;
	}

	_drag = drag;
	_dragging = true;

	// The pointer is off this node by the next event, and the dispatcher freezes an event chain's
	// listener set at Begin - it can shrink, never re-target. Without the capture the drag stops
	// receiving Move as soon as it crosses its own edge
	setExclusive();

	_drag->updateDrag(swipe.location(), swipe.input->data.getModifiers());
	return true;
}

void DragSource::handleDragMove(const GestureSwipe &swipe) {
	if (_dragging && _drag) {
		// a POSITION, never an accumulated delta: that is what makes the drag a fixed point of
		// the pass and keeps a burst of moves in one frame from drifting
		_drag->updateDrag(swipe.location(), swipe.input->data.getModifiers());
	}
}

void DragSource::handleDragEnd(bool cancelled) {
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

} // namespace stappler::xenolith
