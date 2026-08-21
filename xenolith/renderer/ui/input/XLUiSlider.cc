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

// What the handle measures when no rule gives it a size: a square as tall as the track is thick,
// which is grabbable at any track size and looks like a handle rather than like nothing.
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

	// Above the fill: the handle sits at the boundary between the filled and unfilled halves, and
	// the one drawn second is the one that reads as being on top of the other.
	_thumb = addChild(Rc<Panel>::create(), ZOrder(2));
	_thumb->setType("slider-thumb");
	_thumb->removeStyleClass("xl-ui-panel");
	_thumb->addStyleClass("xl-ui-slider-thumb");
	_thumb->registerStyleAppliers("slider-thumb");
	_thumb->setAnchorPoint(Anchor::Middle);

	/* THE HANDLE'S SIZE ARRIVES LATER THAN THE TRACK'S, and the geometry has to be redone when it
	does. The stylesheet reaches `slider-thumb` on a pass of its own, after this widget has already
	had - and used - a content size; without this the handle would be placed for whatever size it
	had at the time (the fallback square) and never moved again, so the travel the widget reports
	and the travel it drew would disagree. Cheaper than a per-frame tick and exact rather than
	nearly right. */
	_thumb->setContentSizeDirtyCallback([this] { updateGeometry(); });

	_listener = addSystem(Rc<InputListener>::create());

	// A press moves the value to where it landed. This is what a track is FOR - a slider that only
	// answered a drag of the handle would make every distant change a two-part gesture - and it has
	// to be its own recognizer, because a swipe is only recognized once the pointer has MOVED.
	_listener->addTapRecognizer([this](const GestureTap &tap) {
		if (!_enabled) {
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
			// threshold 0 with sendThreshold: a handle has to follow the pointer from the first
			// pixel rather than jump once the gesture has travelled the tap tolerance. Same
			// settings, and the same reason, as ui::DockSplitter's.
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

	// A key event carries the pointer's location, so the default filter would answer the arrows
	// only while the mouse hovers the track. A focused widget owns the keyboard wherever the
	// pointer is - the same seam, and the same reason, as ui::Select's.
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
	// Off until there is focus to lose - ui::Select and ui::TextInput do the same.
	_focusListener->setEnabled(false);

	/* The InteractiveComponent has to EXIST from the first frame rather than from the first call
	that changes something: a node without one reads as state 0 to the style resolver, and
	`:disabled` is "not :enabled" - so a slider nobody had touched matched `slider:disabled` while
	it was perfectly enabled. Same line, and same reason, as ui::Checkbox's. */
	applyControlEnabled(this, _enabled);

	return true;
}

bool Slider::setRange(double min, double max, double step) {
	// A range nobody can express is a programming mistake and not a value: refusing leaves the
	// widget saying what it said before, which is the only honest answer available here.
	if (!(step > 0.0) || !(max >= min)) {
		return false;
	}

	_min = min;
	_max = max;
	_step = step;

	// The author's max is KEPT even when it is not a whole number of steps from the minimum: the
	// last notch simply does not reach it, and getValueAt(getMaxIndex()) is what says so. Moving
	// the max, or adding a short last step, would put a number nobody wrote on the screen.
	_maxIndex = int64_t(sprt::floor((max - min) / step));
	if (_maxIndex < 0) {
		_maxIndex = 0;
	}

	// Silent: the scale changed, and a callback fired here would report a value change that
	// nobody asked for. An index that is no longer on the scale is clamped rather than kept -
	// reporting a notch that is not there would be worse.
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
	// Nearest notch, ties UPWARD. `floor(x + 0.5)` rather than round(): round() takes a tie away
	// from zero, which ties upward on a positive scale and downward on one that sits below zero -
	// so the same input would land differently depending on where the range happens to be.
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
	// The lock has the last word, and remembers what was asked for so unlocking can give it back.
	e = resolveEditLock(this, e);
	if (_enabled == e) {
		return;
	}
	_enabled = e;
	if (!_enabled) {
		// A control that stops being usable mid-gesture must not keep the pointer, and must not
		// keep the keyboard either.
		handleDragEnd();
		blur();
	}
	applyControlEnabled(this, _enabled);
	updateInteractiveState();
}

void Slider::focus() {
	if (_focused || !_enabled) {
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
		// Nothing measured yet, or a scale with a single notch: there is no position to read a
		// different answer out of.
		return _index;
	}

	const float thumb =
			_vertical ? _thumb->getContentSize().height : _thumb->getContentSize().width;
	const float along = _vertical ? location.y : location.x;

	// Measured from the handle's CENTRE, which is what the handle's position is: the travel runs
	// from `thumb/2` to `track - thumb/2`, so this is exactly the inverse of updateGeometry().
	const float fraction = sprt::clamp((along - thumb / 2.0f) / travel, 0.0f, 1.0f);
	return int64_t(sprt::floor(double(fraction) * double(_maxIndex) + 0.5));
}

bool Slider::step(int64_t delta) {
	const auto before = _index;
	setIndex(_index + delta);
	return _index != before;
}

bool Slider::handleKey(const GestureData &data) {
	if (!_focused || !_enabled || !data.input) {
		return false;
	}

	const auto &ev = data.input->data;
	if (ev.event != InputEventName::KeyPressed && ev.event != InputEventName::KeyRepeated) {
		return false;
	}

	const int64_t page = int64_t(_pageSteps);

	switch (ev.key.keycode) {
	/* ALONG THIS WIDGET'S OWN AXIS, and no other. On a horizontal track "up" names no direction,
	so answering it would be guessing - and a control that guesses takes the key away from whatever
	stood beside it and meant something by it. Home/End and the page keys are unambiguous either
	way and are answered by both. */
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
	if (!_enabled) {
		return false;
	}

	_dragging = true;
	focus();
	addStyleClass(s_draggingClass);
	_activeApplied = true;
	updateInteractiveState();

	// capture: the pointer goes on reaching this listener once it leaves the track, which it does
	// as soon as the gesture is anything but perfectly straight. Same mechanism ui::TextInput uses
	// for drag-selection and ui::DockSplitter for a divider.
	if (_listener) {
		_listener->setExclusive();
	}

	handleDragMove(location);
	return true;
}

void Slider::handleDragMove(const Vec2 &location) {
	if (!_dragging || !_enabled) {
		return;
	}
	// The ABSOLUTE position, not the accumulated delta: a slider has a point the pointer is
	// attached to, and reading the delta would let rounding drift away from it over a long drag.
	// The callback fires on every step the drag crosses - a value that arrived only on release
	// would give no live feedback, and grouping a drag into one history entry is the owner's job.
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

	// A LayoutSystem - from `display:flex`, or added by hand - owns the children's geometry, and
	// the placement below would be a second writer of the same positions. Same rule, and the same
	// line, as ui::Select's and ui::Button's.
	if (getSystemByType<LayoutSystem>()) {
		return;
	}

	const float width = _contentSize.width;
	const float height = _contentSize.height;
	if (width <= 0.0f || height <= 0.0f) {
		return;
	}

	// The handle's size is CSS's. With no rule for it there is still a handle: a square as thick as
	// the track, which is grabbable at any size a track can have.
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
		// UPWARD: the minimum is at the bottom. This is a level, not a scrollbar.
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

		// The fill runs to the handle's CENTRE rather than to its leading edge: it is the part of
		// the track BEHIND the handle, and stopping short of the centre leaves a gap that reads as
		// a rendering fault at every position but the ends.
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
		const bool hover = _hoverApplied && _enabled;
		if (hover != sprt::hasFlag(state->state, InteractiveState::Hover)) {
			dirty = state->handleHover(hover ? 1 : -1) || dirty;
		}
		const bool focus = _focusApplied && _enabled;
		if (focus != sprt::hasFlag(state->state, InteractiveState::Focus)) {
			dirty = state->handleFocus(focus ? 1 : -1) || dirty;
		}
		const bool active = _activeApplied && _enabled;
		if (active != sprt::hasFlag(state->state, InteractiveState::Active)) {
			dirty = state->handleActive(active ? 1 : -1) || dirty;
		}
		return dirty;
	});
}

} // namespace stappler::xenolith::ui
