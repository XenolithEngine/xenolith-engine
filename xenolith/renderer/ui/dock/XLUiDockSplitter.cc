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

#include "XLUiDockSplitter.h"
#include "XLUiDockSystem.h"
#include "XLInteractiveComponent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

bool DockSplitter::init(NotNull<DockSystem> system, DockNodeHandle handle, DockAxis axis) {
	if (!Panel::init()) {
		return false;
	}

	_system = system;
	_handle = handle;
	_axis = axis;

	setType("dock-splitter");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-dock-splitter");
	addStyleClass(axis == DockAxis::Horizontal ? "vertical-bar" : "horizontal-bar");
	registerStyleAppliers("dock-splitter");

	setAnchorPoint(Anchor::BottomLeft);

	_listener = addSystem(Rc<InputListener>::create());
	_listener->setTouchPadding(GrabPadding);
	// a Horizontal split divides left from right, so the divider itself is a VERTICAL bar and the
	// cursor is the one for moving a column edge
	_listener->setCursor(
			axis == DockAxis::Horizontal ? WindowCursor::ResizeCol : WindowCursor::ResizeRow);

	_listener->addMouseOverRecognizer([this](const GestureData &data) {
		switch (data.event) {
		case GestureEvent::Began:
			setOrUpdateComponent<InteractiveComponent>(
					[](NotNull<InteractiveComponent> state) { return state->handleHover(1); });
			break;
		case GestureEvent::Activated: break;
		case GestureEvent::Ended:
		case GestureEvent::Cancelled:
			setOrUpdateComponent<InteractiveComponent>(
					[](NotNull<InteractiveComponent> state) { return state->handleHover(-1); });
			break;
		}
		return true;
	}, false);

	_listener->addSwipeRecognizer(
			[this](const GestureSwipe &swipe) {
		switch (swipe.event) {
		case GestureEvent::Began: return handleDragBegin();
		case GestureEvent::Activated: handleDrag(swipe.delta, swipe.density); return true;
		case GestureEvent::Ended:
		case GestureEvent::Cancelled: handleDragEnd(); return true;
		}
		return false;
	},
			// threshold 0 with sendThreshold: a divider has to follow the pointer from the first
			// pixel, not jump once the gesture has travelled the default tap tolerance
			InputSwipeInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}),
				0.0f, true});

	return true;
}

bool DockSplitter::handleDragBegin() {
	if (!_system || !_system->canResize(_handle)) {
		return false;
	}

	_dragging = true;
	// capture: the pointer keeps reaching this listener once it leaves the 6pt band, which it does
	// immediately. Same mechanism ui::TextInput uses for drag-selection.
	_listener->setExclusive();
	addStyleClass("dragging");
	setOrUpdateComponent<InteractiveComponent>(
			[](NotNull<InteractiveComponent> state) { return state->handleActive(1); });
	return true;
}

void DockSplitter::handleDrag(const Vec2 &delta, float density) {
	if (!_dragging || !_system) {
		return;
	}
	_system->updateSplitterDrag(_handle, (density > 0.0f) ? (delta / density) : delta);
}

void DockSplitter::handleDragEnd() {
	if (!_dragging) {
		return;
	}
	_dragging = false;
	removeStyleClass("dragging");
	setOrUpdateComponent<InteractiveComponent>(
			[](NotNull<InteractiveComponent> state) { return state->handleActive(-1); });
}

} // namespace stappler::xenolith::ui
