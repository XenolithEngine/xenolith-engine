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


// USB input of the Tau window (RT-A-WindowTauInput-1 of xenolith-tau): the events of
// dev.input@1 through __sprt_tau_input_*, part of the unity build after SPRTWinTauWindow.cc.

namespace sprt::window {

namespace {

constexpr uint64_t s_usbReopenUs = 1'000'000; // a missing provider is asked again
constexpr uint64_t s_usbRepeatDelayUs = 500'000;
constexpr uint64_t s_usbRepeatUs = 33'000;
constexpr uint32_t s_touchId = 1u << 16; // touch ids above those of the buttons

// HID usage page 7 (Keyboard/Keypad) from 0x04 to 0x73.
constexpr InputKeyCode s_hidKeys[] = {
	InputKeyCode::A, InputKeyCode::B, InputKeyCode::C, InputKeyCode::D, InputKeyCode::E, InputKeyCode::F,
	InputKeyCode::G, InputKeyCode::H, InputKeyCode::I, InputKeyCode::J, InputKeyCode::K, InputKeyCode::L,
	InputKeyCode::M, InputKeyCode::N, InputKeyCode::O, InputKeyCode::P, InputKeyCode::Q, InputKeyCode::R,
	InputKeyCode::S, InputKeyCode::T, InputKeyCode::U, InputKeyCode::V, InputKeyCode::W, InputKeyCode::X,
	InputKeyCode::Y, InputKeyCode::Z, // 0x1d
	InputKeyCode::_1, InputKeyCode::_2, InputKeyCode::_3, InputKeyCode::_4, InputKeyCode::_5, InputKeyCode::_6,
	InputKeyCode::_7, InputKeyCode::_8, InputKeyCode::_9, InputKeyCode::_0, // 0x27
	InputKeyCode::ENTER, InputKeyCode::ESCAPE, InputKeyCode::BACKSPACE, InputKeyCode::TAB, InputKeyCode::SPACE,
	InputKeyCode::MINUS, InputKeyCode::EQUAL, InputKeyCode::LEFT_BRACKET, InputKeyCode::RIGHT_BRACKET,
	InputKeyCode::BACKSLASH, InputKeyCode::BACKSLASH, // 0x32: non-US # and ~
	InputKeyCode::SEMICOLON, InputKeyCode::APOSTROPHE, InputKeyCode::GRAVE_ACCENT, InputKeyCode::COMMA,
	InputKeyCode::PERIOD, InputKeyCode::SLASH, InputKeyCode::CAPS_LOCK, // 0x39
	InputKeyCode::F1, InputKeyCode::F2, InputKeyCode::F3, InputKeyCode::F4, InputKeyCode::F5, InputKeyCode::F6,
	InputKeyCode::F7, InputKeyCode::F8, InputKeyCode::F9, InputKeyCode::F10, InputKeyCode::F11, InputKeyCode::F12, // 0x45
	InputKeyCode::PRINT_SCREEN, InputKeyCode::SCROLL_LOCK, InputKeyCode::PAUSE, InputKeyCode::INSERT, InputKeyCode::HOME,
	InputKeyCode::PAGE_UP, InputKeyCode::DELETE, InputKeyCode::END, InputKeyCode::PAGE_DOWN, InputKeyCode::RIGHT,
	InputKeyCode::LEFT, InputKeyCode::DOWN, InputKeyCode::UP, InputKeyCode::NUM_LOCK, // 0x53
	InputKeyCode::KP_DIVIDE, InputKeyCode::KP_MULTIPLY, InputKeyCode::KP_SUBTRACT, InputKeyCode::KP_ADD,
	InputKeyCode::KP_ENTER, InputKeyCode::KP_1, InputKeyCode::KP_2, InputKeyCode::KP_3, InputKeyCode::KP_4,
	InputKeyCode::KP_5, InputKeyCode::KP_6, InputKeyCode::KP_7, InputKeyCode::KP_8, InputKeyCode::KP_9,
	InputKeyCode::KP_0, InputKeyCode::KP_DECIMAL, // 0x63
	InputKeyCode::WORLD_2, InputKeyCode::MENU, InputKeyCode::Unknown, InputKeyCode::KP_EQUAL, // 0x67
	InputKeyCode::F13, InputKeyCode::F14, InputKeyCode::F15, InputKeyCode::F16, InputKeyCode::F17, InputKeyCode::F18,
	InputKeyCode::F19, InputKeyCode::F20, InputKeyCode::F21, InputKeyCode::F22, InputKeyCode::F23, InputKeyCode::F24, // 0x73
};

// The modifiers 0xe0..0xe7: left control, shift, alt, super, then the right ones.
constexpr InputKeyCode s_hidModifiers[] = {
	InputKeyCode::LEFT_CONTROL,
	InputKeyCode::LEFT_SHIFT,
	InputKeyCode::LEFT_ALT,
	InputKeyCode::LEFT_SUPER,
	InputKeyCode::RIGHT_CONTROL,
	InputKeyCode::RIGHT_SHIFT,
	InputKeyCode::RIGHT_ALT,
	InputKeyCode::RIGHT_SUPER,
};

constexpr InputModifier s_hidModifierBits[] = {
	InputModifier::Ctrl | InputModifier::CtrlL,
	InputModifier::Shift | InputModifier::ShiftL,
	InputModifier::Alt | InputModifier::AltL,
	InputModifier::Mod4 | InputModifier::Mod4L,
	InputModifier::Ctrl | InputModifier::CtrlR,
	InputModifier::Shift | InputModifier::ShiftR,
	InputModifier::Alt | InputModifier::AltR,
	InputModifier::Mod4 | InputModifier::Mod4R,
};

InputKeyCode hidKey(uint32_t usage) {
	if (usage >= 0x04 && usage < 0x04 + sizeof(s_hidKeys) / sizeof(s_hidKeys[0])) {
		return s_hidKeys[usage - 0x04];
	}
	if (usage >= 0xe0 && usage <= 0xe7) {
		return s_hidModifiers[usage - 0xe0];
	}
	return InputKeyCode::Unknown;
}

// Buttons 1, 2, 3 of HID: left, right, middle.
constexpr InputMouseButton s_buttons[] = {InputMouseButton::MouseLeft, InputMouseButton::MouseRight,
	InputMouseButton::MouseMiddle};

} // namespace

InputEventData TauWindow::usbPoint(InputEventName event, uint32_t id, InputMouseButton button, float x, float y) const {
	auto ev = InputEventData();
	ev.id = id;
	ev.event = event;
	ev.input.button = button;
	ev.input.modifiers = _modifiers;
	ev.input.x = x;
	ev.input.y = y;
	return ev;
}

void TauWindow::pollUsb(uint64_t now) {
	if (!_usbOpen) {
		if (_usbTriedUs && now - _usbTriedUs < s_usbReopenUs) {
			return;
		}
		_usbTriedUs = now;
		if (__sprt_tau_input_open(0) != 0) {
			return;
		}
		_usbOpen = true;
		_usbDeviceCount = 0;
	}
	__sprt_tau_input_event e;
	int st = 0;
	while ((st = __sprt_tau_input_next(&e)) > 0) {
		usbEvent(e, now);
	}
	if (st < 0) {
		// The provider is gone: what it held ends, and its successor is asked for at once.
		usbCancel();
		__sprt_tau_input_close();
		_usbOpen = false;
		_usbTriedUs = 0;
		return;
	}
	usbRepeat(now);
}

void TauWindow::usbEvent(const __sprt_tau_input_event &e, uint64_t now) {
	switch (e.kind) {
	case __SPRT_TAU_INPUT_EV_CONNECT: {
		struct __sprt_tau_input_device d;
		if (__sprt_tau_input_device(e.device, &d) == 0 && _usbDeviceCount < sizeof(_usbDevices) / sizeof(_usbDevices[0])) {
			_usbDevices[_usbDeviceCount++] = UsbDevice{d.id, d.kind, d.x_max, d.y_max};
		}
		break;
	}
	case __SPRT_TAU_INPUT_EV_DISCONNECT:
		for (size_t i = 0; i < _usbDeviceCount; ++i) {
			if (_usbDevices[i].id == e.device) {
				_usbDevices[i] = _usbDevices[--_usbDeviceCount];
				break;
			}
		}
		break;
	case __SPRT_TAU_INPUT_EV_TOUCH: {
		const UsbDevice *d = nullptr;
		for (size_t i = 0; i < _usbDeviceCount && !d; ++i) {
			d = _usbDevices[i].id == e.device ? &_usbDevices[i] : nullptr;
		}
		if (!d || !d->xMax || !d->yMax) {
			break;
		}
		float x = float(e.x) * _extent.width / float(d->xMax + 1);
		float y = float(_extent.height) - float(e.y) * _extent.height / float(d->yMax + 1);
		InputEventName name = e.phase == __SPRT_TAU_INPUT_DOWN ? InputEventName::Begin
				: e.phase == __SPRT_TAU_INPUT_UP			   ? InputEventName::End
															   : InputEventName::Move;
		size_t at = _contactCount;
		for (size_t i = 0; i < _contactCount; ++i) {
			at = _contacts[i] == e.contact ? i : at;
		}
		if (name == InputEventName::Begin && at == _contactCount
				&& _contactCount < sizeof(_contacts) / sizeof(_contacts[0])) {
			_contacts[_contactCount++] = e.contact;
		} else if (name == InputEventName::End && at < _contactCount) {
			_contacts[at] = _contacts[--_contactCount];
		} else if (at == _contactCount) {
			break; // a move or end of a contact that never began
		}
		auto ev = usbPoint(name, s_touchId + e.contact, InputMouseButton::Touch, x, y);
		ev.input.modifiers |= InputModifier::Touch;
		postEvent(ev);
		break;
	}
	case __SPRT_TAU_INPUT_EV_POINTER: usbPointer(e, false); break;
	case __SPRT_TAU_INPUT_EV_ABSOLUTE: usbPointer(e, true); break;
	case __SPRT_TAU_INPUT_EV_KEY: usbKey(e.value, e.phase == __SPRT_TAU_INPUT_DOWN, now); break;
	default: break;
	}
}

// A mouse moves the cursor by its deltas (HID y grows down), a tablet places it; the buttons
// that changed and the wheel act at the cursor.
void TauWindow::usbPointer(const __sprt_tau_input_event &e, bool absolute) {
	float w = float(_extent.width), h = float(_extent.height);
	if (!_cursorSet) {
		_cursorX = w / 2;
		_cursorY = h / 2;
		_cursorSet = true;
	}
	float x = _cursorX, y = _cursorY;
	if (absolute) {
		const UsbDevice *d = nullptr;
		for (size_t i = 0; i < _usbDeviceCount && !d; ++i) {
			d = _usbDevices[i].id == e.device ? &_usbDevices[i] : nullptr;
		}
		if (d && d->xMax && d->yMax) {
			x = float(e.x) * w / float(d->xMax + 1);
			y = h - float(e.y) * h / float(d->yMax + 1);
		}
	} else {
		x += float(e.x);
		y -= float(e.y);
	}
	x = x < 0.0f ? 0.0f : (x > w - 1.0f ? w - 1.0f : x);
	y = y < 0.0f ? 0.0f : (y > h - 1.0f ? h - 1.0f : y);
	if (x != _cursorX || y != _cursorY) {
		_cursorX = x;
		_cursorY = y;
		postEvent(usbPoint(InputEventName::MouseMove, Max<uint32_t>, InputMouseButton::None, x, y));
	}
	for (uint32_t b = 0; b < sizeof(s_buttons) / sizeof(s_buttons[0]); ++b) {
		bool now = (e.value >> b) & 1u, was = (_buttons >> b) & 1u;
		if (now != was) {
			postEvent(usbPoint(now ? InputEventName::Begin : InputEventName::End, b + 1, s_buttons[b], x, y));
		}
	}
	_buttons = e.value & 7u;
	if (e.wheel) {
		auto ev = usbPoint(InputEventName::Scroll, Max<uint32_t>,
				e.wheel > 0 ? InputMouseButton::MouseScrollUp : InputMouseButton::MouseScrollDown, x, y);
		ev.point.valueY = float(e.wheel) * InputScrollNotch;
		postEvent(ev);
	}
}

void TauWindow::usbKey(uint32_t usage, bool down, uint64_t now) {
	InputKeyCode code = hidKey(usage);
	if (code == InputKeyCode::Unknown) {
		return;
	}
	if (usage >= 0xe0 && usage <= 0xe7) {
		_modifiers = down ? (_modifiers | s_hidModifierBits[usage - 0xe0]) : (_modifiers & ~s_hidModifierBits[usage - 0xe0]);
	}
	size_t at = _usbKeyCount;
	for (size_t i = 0; i < _usbKeyCount; ++i) {
		at = _usbKeys[i].keycode == code ? i : at;
	}
	if (down && at == _usbKeyCount && _usbKeyCount < sizeof(_usbKeys) / sizeof(_usbKeys[0])) {
		_usbKeys[_usbKeyCount++] = UsbKey{code, now + s_usbRepeatDelayUs};
	} else if (!down && at < _usbKeyCount) {
		_usbKeys[at] = _usbKeys[--_usbKeyCount];
	}
	auto ev = InputEventData();
	ev.event = down ? InputEventName::KeyPressed : InputEventName::KeyReleased;
	ev.input.modifiers = _modifiers;
	ev.key.keycode = code;
	ev.key.keysym = usage;
	ev.key.keychar = 0;
	postEvent(ev);
}

void TauWindow::usbRepeat(uint64_t now) {
	for (size_t i = 0; i < _usbKeyCount; ++i) {
		UsbKey &k = _usbKeys[i];
		bool modifier = k.keycode >= InputKeyCode::LEFT_SHIFT && k.keycode <= InputKeyCode::RIGHT_SUPER;
		if (modifier || now < k.repeatUs) {
			continue;
		}
		k.repeatUs = now + s_usbRepeatUs;
		auto ev = InputEventData();
		ev.event = InputEventName::KeyRepeated;
		ev.input.modifiers = _modifiers;
		ev.key.keycode = k.keycode;
		ev.key.keysym = 0;
		ev.key.keychar = 0;
		postEvent(ev);
	}
}

void TauWindow::usbCancel() {
	for (size_t i = 0; i < _contactCount; ++i) {
		postEvent(usbPoint(InputEventName::Cancel, s_touchId + _contacts[i], InputMouseButton::Touch, NaN<float>,
				NaN<float>));
	}
	_contactCount = 0;
	for (uint32_t b = 0; b < sizeof(s_buttons) / sizeof(s_buttons[0]); ++b) {
		if ((_buttons >> b) & 1u) {
			postEvent(usbPoint(InputEventName::Cancel, b + 1, s_buttons[b], _cursorX, _cursorY));
		}
	}
	_buttons = 0;
	for (size_t i = 0; i < _usbKeyCount; ++i) {
		auto ev = InputEventData();
		ev.event = InputEventName::KeyCanceled;
		ev.key.keycode = _usbKeys[i].keycode;
		ev.key.keysym = 0;
		ev.key.keychar = 0;
		postEvent(ev);
	}
	_usbKeyCount = 0;
	_modifiers = InputModifier::None;
}

} // namespace sprt::window
