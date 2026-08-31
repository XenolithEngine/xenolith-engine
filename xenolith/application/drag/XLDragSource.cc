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
	/* The node is leaving the scene with the button still down, and the drag SURVIVES it.

	This used to abort here, on the reasoning that a drop routinely removes its own source. But the
	source node going away is not the same event as the drag ending, and a list that scrolls under
	its own drag proves it: a virtualized row is unbuilt the moment it leaves the window, which
	killed every drag that reached the edge of a long list. A target disappearing mid-drag has
	always been ordinary here (see DragSystem::handleTargetGone); a source is no different - the
	session holds an Rc on this object and the payload is already a value.

	`_detached` is what keeps InputListener::handleExit below from undoing that: it cancels every
	recognizer, and the swipe's cancellation would otherwise arrive as a perfectly ordinary "the
	user let go". */
	_detached = _dragging;

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
	_detached = false;

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

	// The recognizer being torn down with the node, not the user letting go. Told apart by
	// _detached, because at this level the two arrive as the same call.
	if (_detached) {
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
