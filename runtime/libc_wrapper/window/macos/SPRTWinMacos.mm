/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#define __SPRT_BUILD 1

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
#pragma clang diagnostic ignored "-Wdeprecated-anon-enum-enum-conversion"
#pragma clang diagnostic ignored "-Wavailability"

#import <AppKit/AppKit.h>

#pragma clang diagnostic pop

#include "private/window/macos/SPRTWinMacos.h"

namespace sprt::window {

uint32_t getMacosButtonNumber(InputMouseButton btn) {
	switch (btn) {
	case InputMouseButton::MouseLeft: return 0; break;
	case InputMouseButton::MouseRight: return 1; break;
	case InputMouseButton::MouseMiddle: return 2; break;
	default: break;
	}
	return toInt(btn) + 3 - toInt(InputMouseButton::Mouse8);
}

InputMouseButton getInputMouseButton(int32_t buttonNumber) {
	switch (buttonNumber) {
	case 0: return InputMouseButton::MouseLeft;
	case 1: return InputMouseButton::MouseRight;
	case 2: return InputMouseButton::MouseMiddle;
	default: return InputMouseButton(toInt(InputMouseButton::Mouse8) + (buttonNumber - 3));
	}
}

void createKeyTables(InputKeyCode keycodes[256], uint16_t scancodes[toInt(InputKeyCode::Max)]) {
	memset(keycodes, 0, sizeof(InputKeyCode) * 256);
	memset(scancodes, 0, sizeof(uint16_t) * toInt(InputKeyCode::Max));

	keycodes[0x1D] = InputKeyCode::_0;
	keycodes[0x12] = InputKeyCode::_1;
	keycodes[0x13] = InputKeyCode::_2;
	keycodes[0x14] = InputKeyCode::_3;
	keycodes[0x15] = InputKeyCode::_4;
	keycodes[0x17] = InputKeyCode::_5;
	keycodes[0x16] = InputKeyCode::_6;
	keycodes[0x1A] = InputKeyCode::_7;
	keycodes[0x1C] = InputKeyCode::_8;
	keycodes[0x19] = InputKeyCode::_9;
	keycodes[0x00] = InputKeyCode::A;
	keycodes[0x0B] = InputKeyCode::B;
	keycodes[0x08] = InputKeyCode::C;
	keycodes[0x02] = InputKeyCode::D;
	keycodes[0x0E] = InputKeyCode::E;
	keycodes[0x03] = InputKeyCode::F;
	keycodes[0x05] = InputKeyCode::G;
	keycodes[0x04] = InputKeyCode::H;
	keycodes[0x22] = InputKeyCode::I;
	keycodes[0x26] = InputKeyCode::J;
	keycodes[0x28] = InputKeyCode::K;
	keycodes[0x25] = InputKeyCode::L;
	keycodes[0x2E] = InputKeyCode::M;
	keycodes[0x2D] = InputKeyCode::N;
	keycodes[0x1F] = InputKeyCode::O;
	keycodes[0x23] = InputKeyCode::P;
	keycodes[0x0C] = InputKeyCode::Q;
	keycodes[0x0F] = InputKeyCode::R;
	keycodes[0x01] = InputKeyCode::S;
	keycodes[0x11] = InputKeyCode::T;
	keycodes[0x20] = InputKeyCode::U;
	keycodes[0x09] = InputKeyCode::V;
	keycodes[0x0D] = InputKeyCode::W;
	keycodes[0x07] = InputKeyCode::X;
	keycodes[0x10] = InputKeyCode::Y;
	keycodes[0x06] = InputKeyCode::Z;

	keycodes[0x27] = InputKeyCode::APOSTROPHE;
	keycodes[0x2A] = InputKeyCode::BACKSLASH;
	keycodes[0x2B] = InputKeyCode::COMMA;
	keycodes[0x18] = InputKeyCode::EQUAL;
	keycodes[0x32] = InputKeyCode::GRAVE_ACCENT;
	keycodes[0x21] = InputKeyCode::LEFT_BRACKET;
	keycodes[0x1B] = InputKeyCode::MINUS;
	keycodes[0x2F] = InputKeyCode::PERIOD;
	keycodes[0x1E] = InputKeyCode::RIGHT_BRACKET;
	keycodes[0x29] = InputKeyCode::SEMICOLON;
	keycodes[0x2C] = InputKeyCode::SLASH;
	keycodes[0x0A] = InputKeyCode::WORLD_1;

	keycodes[0x33] = InputKeyCode::BACKSPACE;
	keycodes[0x39] = InputKeyCode::CAPS_LOCK;
	keycodes[0x75] = InputKeyCode::DELETE;
	keycodes[0x7D] = InputKeyCode::DOWN;
	keycodes[0x77] = InputKeyCode::END;
	keycodes[0x24] = InputKeyCode::ENTER;
	keycodes[0x35] = InputKeyCode::ESCAPE;
	keycodes[0x7A] = InputKeyCode::F1;
	keycodes[0x78] = InputKeyCode::F2;
	keycodes[0x63] = InputKeyCode::F3;
	keycodes[0x76] = InputKeyCode::F4;
	keycodes[0x60] = InputKeyCode::F5;
	keycodes[0x61] = InputKeyCode::F6;
	keycodes[0x62] = InputKeyCode::F7;
	keycodes[0x64] = InputKeyCode::F8;
	keycodes[0x65] = InputKeyCode::F9;
	keycodes[0x6D] = InputKeyCode::F10;
	keycodes[0x67] = InputKeyCode::F11;
	keycodes[0x6F] = InputKeyCode::F12;
	keycodes[0x69] = InputKeyCode::PRINT_SCREEN;
	keycodes[0x6B] = InputKeyCode::F14;
	keycodes[0x71] = InputKeyCode::F15;
	keycodes[0x6A] = InputKeyCode::F16;
	keycodes[0x40] = InputKeyCode::F17;
	keycodes[0x4F] = InputKeyCode::F18;
	keycodes[0x50] = InputKeyCode::F19;
	keycodes[0x5A] = InputKeyCode::F20;
	keycodes[0x73] = InputKeyCode::HOME;
	keycodes[0x72] = InputKeyCode::INSERT;
	keycodes[0x7B] = InputKeyCode::LEFT;
	keycodes[0x3A] = InputKeyCode::LEFT_ALT;
	keycodes[0x3B] = InputKeyCode::LEFT_CONTROL;
	keycodes[0x38] = InputKeyCode::LEFT_SHIFT;
	keycodes[0x37] = InputKeyCode::LEFT_SUPER;
	keycodes[0x6E] = InputKeyCode::MENU;
	keycodes[0x47] = InputKeyCode::NUM_LOCK;
	keycodes[0x79] = InputKeyCode::PAGE_DOWN;
	keycodes[0x74] = InputKeyCode::PAGE_UP;
	keycodes[0x7C] = InputKeyCode::RIGHT;
	keycodes[0x3D] = InputKeyCode::RIGHT_ALT;
	keycodes[0x3E] = InputKeyCode::RIGHT_CONTROL;
	keycodes[0x3C] = InputKeyCode::RIGHT_SHIFT;
	keycodes[0x36] = InputKeyCode::RIGHT_SUPER;
	keycodes[0x31] = InputKeyCode::SPACE;
	keycodes[0x30] = InputKeyCode::TAB;
	keycodes[0x7E] = InputKeyCode::UP;

	keycodes[0x52] = InputKeyCode::KP_0;
	keycodes[0x53] = InputKeyCode::KP_1;
	keycodes[0x54] = InputKeyCode::KP_2;
	keycodes[0x55] = InputKeyCode::KP_3;
	keycodes[0x56] = InputKeyCode::KP_4;
	keycodes[0x57] = InputKeyCode::KP_5;
	keycodes[0x58] = InputKeyCode::KP_6;
	keycodes[0x59] = InputKeyCode::KP_7;
	keycodes[0x5B] = InputKeyCode::KP_8;
	keycodes[0x5C] = InputKeyCode::KP_9;
	keycodes[0x45] = InputKeyCode::KP_ADD;
	keycodes[0x41] = InputKeyCode::KP_DECIMAL;
	keycodes[0x4B] = InputKeyCode::KP_DIVIDE;
	keycodes[0x4C] = InputKeyCode::KP_ENTER;
	keycodes[0x51] = InputKeyCode::KP_EQUAL;
	keycodes[0x43] = InputKeyCode::KP_MULTIPLY;
	keycodes[0x4E] = InputKeyCode::KP_SUBTRACT;

	for (int scancode = 0; scancode < 256; scancode++) {
		// Store the reverse translation for faster key name lookup
		if (toInt(keycodes[scancode]) >= 0) {
			scancodes[toInt(keycodes[scancode])] = scancode;
		}
	}
}

InputModifier getInputModifiers(uint32_t flags) {
	InputModifier mods = InputModifier::None;

	if ((flags & NX_DEVICELSHIFTKEYMASK) != 0) {
		mods |= InputModifier::ShiftL;
	}
	if ((flags & NX_DEVICERSHIFTKEYMASK) != 0) {
		mods |= InputModifier::ShiftR;
	}
	if ((flags & NX_DEVICELCTLKEYMASK) != 0) {
		mods |= InputModifier::CtrlL;
	}
	if ((flags & NX_DEVICERCTLKEYMASK) != 0) {
		mods |= InputModifier::CtrlR;
	}
	if ((flags & NX_DEVICELALTKEYMASK) != 0) {
		mods |= InputModifier::AltL;
	}
	if ((flags & NX_DEVICERALTKEYMASK) != 0) {
		mods |= InputModifier::AltR;
	}
	if ((flags & NX_DEVICELCMDKEYMASK) != 0) {
		mods |= InputModifier::WinL;
	}
	if ((flags & NX_DEVICERCMDKEYMASK) != 0) {
		mods |= InputModifier::WinR;
	}

	if ((flags & NSEventModifierFlagCapsLock) != 0) {
		mods |= InputModifier::CapsLock;
	}
	if ((flags & NSEventModifierFlagShift) != 0) {
		mods |= InputModifier::Shift;
	}
	if ((flags & NSEventModifierFlagControl) != 0) {
		mods |= InputModifier::Ctrl;
	}
	if ((flags & NSEventModifierFlagOption) != 0) {
		mods |= InputModifier::Alt;
	}
	if ((flags & NSEventModifierFlagNumericPad) != 0) {
		mods |= InputModifier::NumLock;
	}
	if ((flags & NSEventModifierFlagCommand) != 0) {
		mods |= InputModifier::Mod3;
	}
	if ((flags & NSEventModifierFlagHelp) != 0) {
		mods |= InputModifier::Mod4;
	}
	if ((flags & NSEventModifierFlagFunction) != 0) {
		mods |= InputModifier::Mod5;
	}
	return mods;
}

} // namespace sprt::window
