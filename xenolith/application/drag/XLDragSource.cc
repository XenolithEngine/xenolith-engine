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
	/* The drag survives the source leaving the scene (e.g. a virtualized row unbuilt mid-drag): the
	session holds an Rc on this object. `_detached` keeps the recognizer cancellation in
	InputListener::handleExit from being taken as a release. */
	_detached = _dragging;

	InputListener::handleExit();
}

void DragSource::setOfferBuilder(OfferBuilder &&builder) { _builder = sp::move(builder); }

DragSession *DragSource::getSession() const {
	// The system outlives any one drag; its session may belong to another source
	return (_dragging && _drag) ? _drag->getSession() : nullptr;
}

bool DragSource::handleDragBegin(const GestureSwipe &swipe) {
	if (_dragging || !_owner || !_builder) {
		return false;
	}

	DragOffer offer;
	if (!_builder(offer)) {
		return false; // not draggable right now
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

	// The dispatcher freezes an event chain's listeners at Begin; without the capture the drag
	// stops receiving Move once the pointer leaves this node
	setExclusive();

	_drag->updateDrag(swipe.location(), swipe.input->data.getModifiers());
	return true;
}

void DragSource::handleDragMove(const GestureSwipe &swipe) {
	if (_dragging && _drag) {
		// a position, not an accumulated delta, so a burst of moves in one frame does not drift
		_drag->updateDrag(swipe.location(), swipe.input->data.getModifiers());
	}
}

void DragSource::handleDragEnd(bool cancelled) {
	if (!_dragging) {
		return;
	}

	// The recognizer torn down with the node, not a release
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
