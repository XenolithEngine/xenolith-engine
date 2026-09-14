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

#include "XLUiSlider.h"
#include "XLUiLayoutSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

static constexpr StringView s_verticalClass = StringView("vertical");
static constexpr StringView s_draggingClass = StringView("dragging");

// The handle's size when no rule gives one: a square as tall as the track is thick.
static constexpr float s_defaultThumbRatio = 1.0f;

Slider::~Slider() { }

bool Slider::init() {
	if (!Panel::init()) {
		return false;
	}

	setType("slider");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-slider");
	// the same fill / outline / border-radius appliers Panel registers for itself, under "slider"
	registerStyleAppliers("slider");

	_fill = addChild(Rc<Panel>::create(), ZOrder(1));
	_fill->setType("slider-fill");
	_fill->removeStyleClass("xl-ui-panel");
	_fill->addStyleClass("xl-ui-slider-fill");
	_fill->registerStyleAppliers("slider-fill");
	_fill->setAnchorPoint(Anchor::BottomLeft);
	_fill->setPosition(Vec2::ZERO);

	// Drawn above the fill, at its boundary.
	_thumb = addChild(Rc<Panel>::create(), ZOrder(2));
	_thumb->setType("slider-thumb");
	_thumb->removeStyleClass("xl-ui-panel");
	_thumb->addStyleClass("xl-ui-slider-thumb");
	_thumb->registerStyleAppliers("slider-thumb");
	_thumb->setAnchorPoint(Anchor::Middle);

	/* The handle's CSS size arrives on a later style pass than the track's, so geometry is redone
	when it changes. */
	_thumb->setContentSizeDirtyCallback([this] { updateGeometry(); });

	_listener = addSystem(Rc<InputListener>::create());

	// A press moves the value to where it landed. A separate recognizer, because a swipe is only
	// recognized once the pointer has moved.
	_listener->addTapRecognizer([this](const GestureTap &tap) {
		if (!isEnabled()) {
			return false;
		}
		if (tap.event == GestureEvent::Activated) {
			focus();
			setIndex(indexForLocation(convertToNodeSpace(tap.location())));
		}
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}), 1});

	_listener->addMouseOverRecognizer([this](const GestureData &data) {
		switch (data.event) {
		case GestureEvent::Began: _hoverApplied = true; break;
		case GestureEvent::Ended:
		case GestureEvent::Cancelled: _hoverApplied = false; break;
		default: break;
		}
		updateInteractiveState();
		return true;
	}, false);

	_listener->addSwipeRecognizer(
			[this](const GestureSwipe &swipe) {
		switch (swipe.event) {
		case GestureEvent::Began: return handleDragBegin(swipe.secondTouch);
		case GestureEvent::Activated: handleDragMove(swipe.secondTouch); return true;
		case GestureEvent::Ended:
		case GestureEvent::Cancelled: handleDragEnd(); return true;
		}
		return false;
	},
			// threshold 0 with sendThreshold: the handle follows the pointer from the first pixel.
			InputSwipeInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}),
				0.0f, true});

	InputKeyMask keys;
	keys.set(toInt(InputKeyCode::LEFT));
	keys.set(toInt(InputKeyCode::RIGHT));
	keys.set(toInt(InputKeyCode::UP));
	keys.set(toInt(InputKeyCode::DOWN));
	keys.set(toInt(InputKeyCode::HOME));
	keys.set(toInt(InputKeyCode::END));
	keys.set(toInt(InputKeyCode::PAGE_UP));
	keys.set(toInt(InputKeyCode::PAGE_DOWN));
	_listener->addKeyRecognizer([this](const GestureData &data) { return handleKey(data); },
			InputKeyInfo{sp::move(keys)});

	// A key event carries the pointer location, so the default filter would answer only while the
	// mouse hovers the track; a focused widget takes keys wherever the pointer is.
	_listener->setTouchFilter(
			[this](const InputEvent &event, const InputListener::DefaultEventFilter &cb) {
		if (event.data.isKeyEvent()) {
			return _focused;
		}
		return cb(event);
	});

	// A tap outside gives focus up. Priority 1 puts it above the scene graph and its filter accepts
	// only points outside the widget, so it never competes with the tap above.
	_focusListener = addSystem(Rc<InputListener>::create());
	_focusListener->setPriority(1);
	_focusListener->addTapRecognizer([this](const GestureTap &) {
		blur();
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}), 1});
	_focusListener->setTouchFilter(
			[this](const InputEvent &event, const InputListener::DefaultEventFilter &) {
		return !isTouched(event.currentLocation, 0.0f);
	});
	// Off until there is focus to lose.
	_focusListener->setEnabled(false);

	/* The InteractiveComponent must exist from the start: without one the state reads as 0 and
	`slider:disabled` would match. */
	applyControlEnabled(this, true);

	return true;
}

bool Slider::setRange(double min, double max, double step) {
	// An invalid range is refused and the previous scale is kept.
	if (!(step > 0.0) || !(max >= min)) {
		return false;
	}

	_min = min;
	_max = max;
	_step = step;

	// The max is kept even when it is not a whole number of steps from the minimum; the last notch
	// then falls short of it.
	_maxIndex = int64_t(sprt::floor((max - min) / step));
	if (_maxIndex < 0) {
		_maxIndex = 0;
	}

	// Silent: a scale change is not a value change. An index off the new scale is clamped.
	if (_index > _maxIndex) {
		_index = _maxIndex;
	}

	updateGeometry();
	return true;
}

double Slider::getValueAt(int64_t index) const { return _min + _step * double(index); }

void Slider::setIndex(int64_t index, bool silent) {
	index = sprt::clamp(index, int64_t(0), _maxIndex);
	if (index == _index) {
		return;
	}
	_index = index;
	updateGeometry();
	if (!silent && _callback) {
		_callback(_index);
	}
}

void Slider::setValue(double value, bool silent) {
	if (_step <= 0.0) {
		return;
	}
	// Nearest notch, ties upward. `floor(x + 0.5)` rather than round(), which ties away from zero.
	const double raw = (value - _min) / _step;
	setIndex(int64_t(sprt::floor(raw + 0.5)), silent);
}

void Slider::setInteger(bool value) {
	if (_integer == value) {
		return;
	}
	_integer = value;
}

void Slider::setVertical(bool value) {
	if (_vertical == value) {
		return;
	}
	_vertical = value;
	if (_vertical) {
		addStyleClass(s_verticalClass);
	} else {
		removeStyleClass(s_verticalClass);
	}
	updateGeometry();
}

void Slider::setPageSteps(uint32_t value) { _pageSteps = value > 0 ? value : 1; }

void Slider::setEnabled(bool e) {
	// The edit lock overrides the request and remembers it for unlock.
	e = resolveEditLock(this, e);
	if (isEnabled() == e) {
		return;
	}
	applyControlEnabled(this, e);
	if (!e) {
		// A disabled control releases the pointer and the keyboard.
		handleDragEnd();
		blur();
	}
	updateInteractiveState();
}

void Slider::focus() {
	if (_focused || !isEnabled()) {
		return;
	}
	_focused = true;
	_focusApplied = true;
	if (_focusListener) {
		_focusListener->setEnabled(true);
	}
	updateInteractiveState();
	if (_focusCallback) {
		_focusCallback(true);
	}
}

void Slider::blur() {
	if (!_focused) {
		return;
	}
	_focused = false;
	_focusApplied = false;
	if (_focusListener) {
		_focusListener->setEnabled(false);
	}
	updateInteractiveState();
	if (_focusCallback) {
		_focusCallback(false);
	}
}

float Slider::getTravel() const {
	if (!_thumb) {
		return 0.0f;
	}
	const float track = _vertical ? _contentSize.height : _contentSize.width;
	const float thumb =
			_vertical ? _thumb->getContentSize().height : _thumb->getContentSize().width;
	return sprt::max(track - thumb, 0.0f);
}

int64_t Slider::indexForLocation(const Vec2 &location) const {
	const float travel = getTravel();
	if (travel <= 0.0f || _maxIndex <= 0) {
		// Nothing measured yet, or a scale with a single notch.
		return _index;
	}

	const float thumb =
			_vertical ? _thumb->getContentSize().height : _thumb->getContentSize().width;
	const float along = _vertical ? location.y : location.x;

	// Measured from the handle's centre: the travel runs from `thumb/2` to `track - thumb/2`, the
	// exact inverse of updateGeometry().
	const float fraction = sprt::clamp((along - thumb / 2.0f) / travel, 0.0f, 1.0f);
	return int64_t(sprt::floor(double(fraction) * double(_maxIndex) + 0.5));
}

bool Slider::step(int64_t delta) {
	const auto before = _index;
	setIndex(_index + delta);
	return _index != before;
}

bool Slider::handleKey(const GestureData &data) {
	if (!_focused || !isEnabled() || !data.input) {
		return false;
	}

	const auto &ev = data.input->data;
	if (ev.event != InputEventName::KeyPressed && ev.event != InputEventName::KeyRepeated) {
		return false;
	}

	const int64_t page = int64_t(_pageSteps);

	switch (ev.key.keycode) {
	/* Arrows only along the widget's own axis; Home/End and the page keys work on both. */
	case InputKeyCode::RIGHT: return !_vertical && step(1);
	case InputKeyCode::LEFT: return !_vertical && step(-1);
	case InputKeyCode::UP: return _vertical && step(1);
	case InputKeyCode::DOWN: return _vertical && step(-1);

	case InputKeyCode::HOME: return step(-_maxIndex);
	case InputKeyCode::END: return step(_maxIndex);

	case InputKeyCode::PAGE_UP: return step(page);
	case InputKeyCode::PAGE_DOWN: return step(-page);

	default: break;
	}
	return false;
}

bool Slider::handleDragBegin(const Vec2 &location) {
	if (!isEnabled()) {
		return false;
	}

	_dragging = true;
	focus();
	addStyleClass(s_draggingClass);
	_activeApplied = true;
	updateInteractiveState();

	// capture: the pointer keeps reaching this listener after it leaves the track.
	if (_listener) {
		_listener->setExclusive();
	}

	handleDragMove(location);
	return true;
}

void Slider::handleDragMove(const Vec2 &location) {
	if (!_dragging || !isEnabled()) {
		return;
	}
	// The absolute position, not the accumulated delta, so rounding does not drift. The callback
	// fires on every step the drag crosses.
	setIndex(indexForLocation(convertToNodeSpace(location)));
}

void Slider::handleDragEnd() {
	if (!_dragging) {
		return;
	}
	_dragging = false;
	removeStyleClass(s_draggingClass);
	_activeApplied = false;
	updateInteractiveState();
}

void Slider::handleContentSizeDirty() {
	Panel::handleContentSizeDirty();
	updateGeometry();
}

void Slider::updateGeometry() {
	if (!_fill || !_thumb || _inGeometry) {
		return;
	}

	// A LayoutSystem (from `display:flex` or added by hand) owns the children's geometry.
	if (getSystemByType<LayoutSystem>()) {
		return;
	}

	const float width = _contentSize.width;
	const float height = _contentSize.height;
	if (width <= 0.0f || height <= 0.0f) {
		return;
	}

	// The handle's size comes from CSS; with no rule it is a square as thick as the track.
	auto thumbSize = _thumb->getContentSize();
	if (thumbSize.width <= 0.0f || thumbSize.height <= 0.0f) {
		const float side = (_vertical ? width : height) * s_defaultThumbRatio;
		thumbSize = Size2(side, side);
		// Guarded: writing the handle's size dirties it, and its dirty callback is this function.
		_inGeometry = true;
		_thumb->setContentSize(thumbSize);
		_inGeometry = false;
	}

	const float fraction = _maxIndex > 0 ? float(double(_index) / double(_maxIndex)) : 0.0f;

	if (_vertical) {
		// Upward: the minimum is at the bottom.
		const float travel = sprt::max(height - thumbSize.height, 0.0f);
		const float center = thumbSize.height / 2.0f + travel * fraction;

		_fill->setAnchorPoint(Anchor::BottomLeft);
		_fill->setPosition(Vec2::ZERO);
		_fill->setContentSize(Size2(width, center));

		_thumb->setAnchorPoint(Anchor::Middle);
		_thumb->setPosition(Vec2(width / 2.0f, center));
	} else {
		const float travel = sprt::max(width - thumbSize.width, 0.0f);
		const float center = thumbSize.width / 2.0f + travel * fraction;

		// The fill runs to the handle's centre, so no gap shows between fill and handle.
		_fill->setAnchorPoint(Anchor::BottomLeft);
		_fill->setPosition(Vec2::ZERO);
		_fill->setContentSize(Size2(center, height));

		_thumb->setAnchorPoint(Anchor::Middle);
		_thumb->setPosition(Vec2(center, height / 2.0f));
	}
}

void Slider::updateInteractiveState() {
	setOrUpdateComponent<InteractiveComponent>([this](NotNull<InteractiveComponent> state) {
		// The Enabled bit and the `disabled` class are applyControlEnabled's, from setEnabled.
		bool dirty = false;
		// The counters are cumulative, so each flag is pushed on an edge and never twice.
		const bool hover = _hoverApplied && sprt::hasFlag(state->state, InteractiveState::Enabled);
		if (hover != sprt::hasFlag(state->state, InteractiveState::Hover)) {
			dirty = state->handleHover(hover ? 1 : -1) || dirty;
		}
		const bool focus = _focusApplied && sprt::hasFlag(state->state, InteractiveState::Enabled);
		if (focus != sprt::hasFlag(state->state, InteractiveState::Focus)) {
			dirty = state->handleFocus(focus ? 1 : -1) || dirty;
		}
		const bool active =
				_activeApplied && sprt::hasFlag(state->state, InteractiveState::Enabled);
		if (active != sprt::hasFlag(state->state, InteractiveState::Active)) {
			dirty = state->handleActive(active ? 1 : -1) || dirty;
		}
		return dirty;
	});
}

} // namespace stappler::xenolith::ui
