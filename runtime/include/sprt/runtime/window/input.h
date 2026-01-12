/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_INPUT_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_INPUT_H_

#include <sprt/runtime/geom.h>
#include <sprt/runtime/window/window_info.h>

namespace sprt::window {

enum class InputFlags : uint32_t {
	None,
	TouchMouseInput = 1 << 0,
	KeyboardInput = 1 << 1,
	FocusInput = 1 << 2,
};

SPRT_DEFINE_ENUM_AS_MASK(InputFlags)

enum class InputMouseButton : uint32_t {
	None,
	MouseLeft,
	MouseMiddle,
	MouseRight,
	MouseScrollUp,
	MouseScrollDown,
	MouseScrollLeft,
	MouseScrollRight,
	Mouse8,
	Mouse9,
	Mouse10,
	Mouse11,
	Mouse12,
	Mouse13,
	Mouse14,
	Mouse15,

	Stilus1,
	Stilus2,
	Stilus3,
	Stilus4,

	Max,

	Touch = MouseLeft
};

enum class InputModifier : uint32_t {
	None = 0,
	Shift = 1 << 0,
	CapsLock = 1 << 1,
	Ctrl = 1 << 2,
	Alt = 1 << 3,
	NumLock = 1 << 4,
	Mod3 = 1 << 5,
	Mod4 = 1 << 6,
	Mod5 = 1 << 7,
	Button1 = 1 << 8,
	Button2 = 1 << 9,
	Button3 = 1 << 10,
	Button4 = 1 << 11,
	Button5 = 1 << 12,

	// Linux-only, experimental
	LayoutAlternative = 1 << 13,

	ShiftL = 1 << 14,
	ShiftR = 1 << 15,
	CtrlL = 1 << 16,
	CtrlR = 1 << 17,
	AltL = 1 << 18,
	AltR = 1 << 19,
	Mod3L = 1 << 20,
	Mod3R = 1 << 21,
	Mod4L = 1 << 22,
	Mod4R = 1 << 23,

	ScrollLock = 1 << 24,

	Command = Mod3, // MacOS Command
	Meta = Mod3, // Android Meta
	Function = Mod4, // Android Function
	Sym = Mod5, // Android Sym
	Win = Mod3,
	WinL = Mod3L,
	WinR = Mod3R,
	Menu = Alt, // VK_MENU
	MenuL = AltL, // VK_LMENU
	MenuR = AltR, // VK_RMENU

	// boolean value for switch event (background/focus)
	ValueFalse = None,
	ValueTrue = uint32_t(1) << uint32_t(31),
	Unmanaged = ValueTrue
};

SPRT_DEFINE_ENUM_AS_MASK(InputModifier)

/* Based on GLFW
 *
 * Designed to fit 128-bit bitmask for pressed key tracking
 * Undefined char is 0 instead of -1
 * Codepoints for BACKSPACE, TAB, ENTER, ESCAPE, DELETE matches ASCII positions
 *  - Platform can send this with or without keychar
 * Printable keys (and keypad nums) within [32-96]
 * APOSTROPHE moved from 39 to 43 to fit keypad nums in single block
 *  - expressions like KP_0 + 8 is more probable then direct casting InputKeyCode -> char
 * Key names based on QWERTY keyboard, but actually refers to physical key position rather then symbol
 * - actual physical key name defined in XKB convention
 * - e.g. InputKeyCode::S = AC02 key = A on QWERTY = O on Dworak
 * */
enum class InputKeyCode : uint16_t {
	Unknown = 0,

	KP_DECIMAL = 1, // "KPDL"
	KP_DIVIDE = 2, // "KPDV"
	KP_MULTIPLY = 3, // "KPMU"
	KP_SUBTRACT = 4, // "KPSU"
	KP_ADD = 5, // "KPAD"
	KP_ENTER = 6, // "KPEN"
	KP_EQUAL = 7, // "KPEQ"

	BACKSPACE = 8, // "BKSP"; ASCII-compatible
	TAB = 9, // "TAB"; ASCII-compatible
	ENTER = 10, // "RTRN"; ASCII-compatible

	RIGHT = 11, // "RGHT"
	LEFT = 12, // "LEFT"
	DOWN = 13, // "DOWN"
	UP = 14, // "UP"
	PAGE_UP = 15, // "PGUP"
	PAGE_DOWN = 16, // "PGDN"
	HOME = 17, // "HOME"
	END = 18, // "END"
	LEFT_SHIFT = 19, // "LFSH"
	LEFT_CONTROL = 20, // "LCTL"
	LEFT_ALT = 21, // "LALT"
	LEFT_SUPER = 22, // "LWIN"
	RIGHT_SHIFT = 23, // "RTSH"
	RIGHT_CONTROL = 24, // "RCTL"
	RIGHT_ALT = 25, // "RALT", "LVL3", MDSW"
	RIGHT_SUPER = 26, // "RWIN"

	ESCAPE = 27, // "ESC"; ASCII-compatible :

	INSERT = 28, // "INS"
	CAPS_LOCK = 29, // "CAPS"
	SCROLL_LOCK = 30, // "SCLK"
	NUM_LOCK = 31, // "NMLK"

	SPACE = 32, // "SPCE"

	KP_0 = 33, // "KP0"
	KP_1 = 34, // "KP1"
	KP_2 = 35, // "KP2"
	KP_3 = 36, // "KP3"
	KP_4 = 37, // "KP4"
	KP_5 = 38, // "KP5"
	KP_6 = 39, // "KP6"
	KP_7 = 40, // "KP7"
	KP_8 = 41, // "KP8"
	KP_9 = 42, // "KP9"

	APOSTROPHE = 43, // "AC11"; '\''
	COMMA = 44, // "AB08"; ','
	MINUS = 45, // "AE11"; '-'
	PERIOD = 46, // "AB09"; '.'
	SLASH = 47, // "AB10"; '/'
	_0 = 48, // "AE10"
	_1 = 49, // "AE01"
	_2 = 50, // "AE02"
	_3 = 51, // "AE03"
	_4 = 52, // "AE04"
	_5 = 53, // "AE05"
	_6 = 54, // "AE06"
	_7 = 55, // "AE07"
	_8 = 56, // "AE08"
	_9 = 57, // "AE09"
	SEMICOLON = 59, // "AC10"; ';'
	EQUAL = 61, // "AE12"; '='

	WORLD_1 = 62, // "LSGT"; non-US #1 :
	WORLD_2 = 63, // non-US #2

	A = 65, // "AC01"
	B = 66, // "AB05"
	C = 67, // "AB03"
	D = 68, // "AC03"
	E = 69, // "AD03"
	F = 70, // "AC04"
	G = 71, // "AC05"
	H = 72, // "AC06"
	I = 73, // "AD08"
	J = 74, // "AC07"
	K = 75, // "AC08"
	L = 76, // "AC09"
	M = 77, // "AB07"
	N = 78, // "AB06"
	O = 79, // "AD09"
	P = 80, // "AD10"
	Q = 81, // "AD01"
	R = 82, // "AD04"
	S = 83, // "AC02"
	T = 84, // "AD05"
	U = 85, // "AD07"
	V = 86, // "AB04"
	W = 87, // "AD02"
	X = 88, // "AB02"
	Y = 89, // "AD06"
	Z = 90, // "AB01"
	LEFT_BRACKET = 91, // "AD11"; '['
	BACKSLASH = 92, // "BKSL"; '\\'
	RIGHT_BRACKET = 93, // "AD12"; ']'
	GRAVE_ACCENT = 96, // "TLDE"; '`'

	/* Function keys */
	F1 = 97, // "FK01"
	F2 = 98, // "FK02"
	F3 = 99, // "FK03"
	F4 = 100, // "FK04"
	F5 = 101, // "FK05"
	F6 = 102, // "FK06"
	F7 = 103, // "FK07"
	F8 = 104, // "FK08"
	F9 = 105, // "FK09"
	F10 = 106, // "FK10"
	F11 = 107, // "FK11"
	F12 = 108, // "FK12"
	F13 = 109, // "FK13"
	F14 = 110, // "FK14"
	F15 = 111, // "FK15"
	F16 = 112, // "FK16"
	F17 = 113, // "FK17"
	F18 = 114, // "FK18"
	F19 = 115, // "FK19"
	F20 = 116, // "FK20"
	F21 = 117, // "FK21"
	F22 = 118, // "FK22"
	F23 = 119, // "FK23"
	F24 = 120, // "FK24"
	F25 = 121, // "FK25"

	MENU = 124, // "MENU"
	PRINT_SCREEN = 125, // "PRSC"
	PAUSE = 126, // "PAUS"
	DELETE = 127, // "DELE"; ASCII-compatible

	Max
};

enum class InputKeyComposeState : uint16_t {
	Nothing = 0,
	Composed,
	Composing,
	Disabled, // do not use this key event for text input processing
	Forced, // use, when key up should be processed
};

enum class InputEventName : uint32_t {
	None,
	Begin,
	Move,
	End,
	Cancel,
	MouseMove,
	Scroll,

	KeyPressed,
	KeyRepeated,
	KeyReleased,
	KeyCanceled,

	WindowState, // Window allowed actions or key constraints was changed
	Max,
};

enum class InputEventType {
	None,
	Input,
	Custom,
};

enum class InputEventDataType {
	None,
	Point,
	Key,
	Window
};

static constexpr struct {
	InputEventName name;
	InputEventType type;
	InputEventDataType dataType;
} InputEventInfo[toInt(InputEventName::Max)] = {
	{InputEventName::None, InputEventType::None, InputEventDataType::None},
	{InputEventName::Begin, InputEventType::Input, InputEventDataType::Point},
	{InputEventName::Move, InputEventType::Input, InputEventDataType::Point},
	{InputEventName::End, InputEventType::Input, InputEventDataType::Point},
	{InputEventName::Cancel, InputEventType::Input, InputEventDataType::Point},
	{InputEventName::MouseMove, InputEventType::Input, InputEventDataType::Point},
	{InputEventName::Scroll, InputEventType::Input, InputEventDataType::Point},

	{InputEventName::KeyPressed, InputEventType::Input, InputEventDataType::Key},
	{InputEventName::KeyRepeated, InputEventType::Input, InputEventDataType::Key},
	{InputEventName::KeyReleased, InputEventType::Input, InputEventDataType::Key},
	{InputEventName::KeyCanceled, InputEventType::Input, InputEventDataType::Key},

	{InputEventName::WindowState, InputEventType::Input, InputEventDataType::Window},
};

struct SPRT_API InputEventData {
	static InputEventData BoolEvent(InputEventName event, bool value) {
		return InputEventData{
			Max<uint32_t>,
			event,
			{{
				InputMouseButton::None,
				value ? InputModifier::ValueTrue : InputModifier::None,
			}},
		};
	}

	static InputEventData BoolEvent(InputEventName event, bool value, const Vec2 &pt) {
		return InputEventData{
			Max<uint32_t>,
			event,
			{{
				InputMouseButton::None,
				value ? InputModifier::ValueTrue : InputModifier::None,
				pt.x,
				pt.y,
			}},
		};
	}

	static InputEventData BoolEvent(InputEventName event, bool value, InputModifier mod,
			const Vec2 &pt) {
		return InputEventData{
			Max<uint32_t>,
			event,
			{{
				InputMouseButton::None,
				mod | (value ? InputModifier::ValueTrue : InputModifier::None),
				pt.x,
				pt.y,
			}},
		};
	}

	uint32_t id = Max<uint32_t>;
	InputEventName event = InputEventName::None;
	union {
		// default imput data
		struct {
			InputMouseButton button;
			InputModifier modifiers;
			float x;
			float y;
		} input = {InputMouseButton::None, InputModifier::None, NaN<float>, NaN<float>};

		// custom event data
		struct {
			uint32_t u0;
			uint32_t u1;
			float f0;
			float f1;
		} custom;
	};
	union {
		struct {
			float valueX = 0.0f;
			float valueY = 0.0f;
			float density = 1.0f;
		} point = {0.0f, 0.0f, 1.0f};
		struct {
			InputKeyCode keycode; // layout-independent key name
			InputKeyComposeState compose;
			uint32_t keysym; // OS-dependent keysym
			char32_t keychar; // unicode char for key
		} key;
		struct {
			WindowState state;
			WindowState changes;
		} window;
	};

	bool operator==(const uint32_t &i) const { return id == i; }
	bool operator!=(const uint32_t &i) const { return id != i; }

	bool hasLocation() const {
		if (hasInput()) {
			return !isnan(input.x) && !isnan(input.y);
		}
		return false;
	}

	bool getValue() const {
		if (hasInput()) {
			return (input.modifiers & InputModifier::ValueTrue) != InputModifier::None;
		}
		return false;
	}

	bool hasInput() const { return InputEventInfo[toInt(event)].type == InputEventType::Input; }

	bool isPointEvent() const {
		return InputEventInfo[toInt(event)].dataType == InputEventDataType::Point;
	}

	bool isKeyEvent() const {
		return InputEventInfo[toInt(event)].dataType == InputEventDataType::Key;
	}

	InputMouseButton getButton() const {
		if (hasInput()) {
			return input.button;
		}
		return InputMouseButton::None;
	}

	InputModifier getModifiers() const {
		if (hasInput()) {
			return input.modifiers;
		}
		return InputModifier::None;
	}

	Vec2 getLocation() const {
		if (hasLocation()) {
			return Vec2(input.x, input.y);
		}
		return Vec2();
	}

	bool operator==(const InputEventData &r) const {
		if (id != r.id || event != r.event) {
			return false;
		}
		if (hasInput()
				&& (input.button != r.input.button || input.modifiers != r.input.modifiers
						|| input.x != r.input.x || input.y != r.input.y)) {
			return false;
		}
		switch (event) {
		case InputEventName::Begin:
		case InputEventName::Move:
		case InputEventName::End:
		case InputEventName::Cancel:
		case InputEventName::MouseMove:
		case InputEventName::Scroll:
			// pointer event
			if (point.valueX != r.point.valueX || point.valueY != r.point.valueY
					|| point.density != r.point.density) {
				return false;
			}
			break;
		case InputEventName::KeyPressed:
		case InputEventName::KeyRepeated:
		case InputEventName::KeyReleased:
			// key event
			if (key.keycode != r.key.keycode || key.compose != r.key.compose
					|| key.keysym != r.key.keysym || key.keychar != r.key.keychar) {
				return false;
			}
			break;
		case InputEventName::WindowState:
			if (window.state != r.window.state || window.changes != r.window.changes) {
				return false;
			}
			break;
		default: break;
		}
		return true;
	}

	bool operator!=(const InputEventData &r) const { return !(*this == r); }

	bool operator<(const InputEventData &r) const {
		if (id < r.id) {
			return true;
		} else if (id > r.id) {
			return false;
		}

		if (toInt(event) < toInt(r.event)) {
			return true;
		} else if (toInt(event) > toInt(r.event)) {
			return false;
		}

		if (hasInput()) {
			if (toInt(input.button) < toInt(r.input.button)) {
				return true;
			} else if (toInt(input.button) > toInt(r.input.button)) {
				return false;
			}

			if (toInt(input.modifiers) < toInt(r.input.modifiers)) {
				return true;
			} else if (toInt(input.modifiers) > toInt(r.input.modifiers)) {
				return false;
			}
		}

		return false;
	}
};

SPRT_API StringView getInputKeyCodeName(InputKeyCode code);
SPRT_API StringView getInputKeyCodeKeyName(InputKeyCode code);
SPRT_API StringView getInputEventName(InputEventName name);
SPRT_API StringView getInputButtonName(InputMouseButton btn);
SPRT_API void getInputModifiersNames(const callback<void(StringView)> &out, InputModifier mod);

} // namespace sprt::window

namespace sprt {

inline const callback<void(StringView)> &operator<<(const callback<void(StringView)> &cb,
		window::InputModifier state) {
	getInputModifiersNames(cb, state);
	return cb;
}

inline const callback<void(StringView)> &operator<<(const callback<void(StringView)> &cb,
		window::InputMouseButton v) {
	cb << getInputButtonName(v);
	return cb;
}

inline const callback<void(StringView)> &operator<<(const callback<void(StringView)> &cb,
		window::InputEventName v) {
	cb << getInputEventName(v);
	return cb;
}

inline const callback<void(StringView)> &operator<<(const callback<void(StringView)> &cb,
		window::InputKeyCode v) {
	cb << getInputKeyCodeName(v);
	return cb;
}

} // namespace sprt

#endif
