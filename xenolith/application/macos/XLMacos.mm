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

#import <AppKit/AppKit.h>

#include "XLMacos.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

uint32_t getMacosButtonNumber(core::InputMouseButton btn) {
	switch (btn) {
	case core::InputMouseButton::MouseLeft: return 0; break;
	case core::InputMouseButton::MouseRight: return 1; break;
	case core::InputMouseButton::MouseMiddle: return 2; break;
	default: break;
	}
	return toInt(btn) + 3 - toInt(core::InputMouseButton::Mouse8);
}

core::InputMouseButton getInputMouseButton(int32_t buttonNumber) {
	switch (buttonNumber) {
	case 0: return core::InputMouseButton::MouseLeft;
	case 1: return core::InputMouseButton::MouseRight;
	case 2: return core::InputMouseButton::MouseMiddle;
	default:
		return core::InputMouseButton(
				stappler::toInt(core::InputMouseButton::Mouse8) + (buttonNumber - 3));
	}
}

void createKeyTables(core::InputKeyCode keycodes[256],
		uint16_t scancodes[stappler::toInt(core::InputKeyCode::Max)]) {
	memset(keycodes, 0, sizeof(core::InputKeyCode) * 256);
	memset(scancodes, 0, sizeof(uint16_t) * stappler::toInt(core::InputKeyCode::Max));

	keycodes[0x1D] = core::InputKeyCode::_0;
	keycodes[0x12] = core::InputKeyCode::_1;
	keycodes[0x13] = core::InputKeyCode::_2;
	keycodes[0x14] = core::InputKeyCode::_3;
	keycodes[0x15] = core::InputKeyCode::_4;
	keycodes[0x17] = core::InputKeyCode::_5;
	keycodes[0x16] = core::InputKeyCode::_6;
	keycodes[0x1A] = core::InputKeyCode::_7;
	keycodes[0x1C] = core::InputKeyCode::_8;
	keycodes[0x19] = core::InputKeyCode::_9;
	keycodes[0x00] = core::InputKeyCode::A;
	keycodes[0x0B] = core::InputKeyCode::B;
	keycodes[0x08] = core::InputKeyCode::C;
	keycodes[0x02] = core::InputKeyCode::D;
	keycodes[0x0E] = core::InputKeyCode::E;
	keycodes[0x03] = core::InputKeyCode::F;
	keycodes[0x05] = core::InputKeyCode::G;
	keycodes[0x04] = core::InputKeyCode::H;
	keycodes[0x22] = core::InputKeyCode::I;
	keycodes[0x26] = core::InputKeyCode::J;
	keycodes[0x28] = core::InputKeyCode::K;
	keycodes[0x25] = core::InputKeyCode::L;
	keycodes[0x2E] = core::InputKeyCode::M;
	keycodes[0x2D] = core::InputKeyCode::N;
	keycodes[0x1F] = core::InputKeyCode::O;
	keycodes[0x23] = core::InputKeyCode::P;
	keycodes[0x0C] = core::InputKeyCode::Q;
	keycodes[0x0F] = core::InputKeyCode::R;
	keycodes[0x01] = core::InputKeyCode::S;
	keycodes[0x11] = core::InputKeyCode::T;
	keycodes[0x20] = core::InputKeyCode::U;
	keycodes[0x09] = core::InputKeyCode::V;
	keycodes[0x0D] = core::InputKeyCode::W;
	keycodes[0x07] = core::InputKeyCode::X;
	keycodes[0x10] = core::InputKeyCode::Y;
	keycodes[0x06] = core::InputKeyCode::Z;

	keycodes[0x27] = core::InputKeyCode::APOSTROPHE;
	keycodes[0x2A] = core::InputKeyCode::BACKSLASH;
	keycodes[0x2B] = core::InputKeyCode::COMMA;
	keycodes[0x18] = core::InputKeyCode::EQUAL;
	keycodes[0x32] = core::InputKeyCode::GRAVE_ACCENT;
	keycodes[0x21] = core::InputKeyCode::LEFT_BRACKET;
	keycodes[0x1B] = core::InputKeyCode::MINUS;
	keycodes[0x2F] = core::InputKeyCode::PERIOD;
	keycodes[0x1E] = core::InputKeyCode::RIGHT_BRACKET;
	keycodes[0x29] = core::InputKeyCode::SEMICOLON;
	keycodes[0x2C] = core::InputKeyCode::SLASH;
	keycodes[0x0A] = core::InputKeyCode::WORLD_1;

	keycodes[0x33] = core::InputKeyCode::BACKSPACE;
	keycodes[0x39] = core::InputKeyCode::CAPS_LOCK;
	keycodes[0x75] = core::InputKeyCode::DELETE;
	keycodes[0x7D] = core::InputKeyCode::DOWN;
	keycodes[0x77] = core::InputKeyCode::END;
	keycodes[0x24] = core::InputKeyCode::ENTER;
	keycodes[0x35] = core::InputKeyCode::ESCAPE;
	keycodes[0x7A] = core::InputKeyCode::F1;
	keycodes[0x78] = core::InputKeyCode::F2;
	keycodes[0x63] = core::InputKeyCode::F3;
	keycodes[0x76] = core::InputKeyCode::F4;
	keycodes[0x60] = core::InputKeyCode::F5;
	keycodes[0x61] = core::InputKeyCode::F6;
	keycodes[0x62] = core::InputKeyCode::F7;
	keycodes[0x64] = core::InputKeyCode::F8;
	keycodes[0x65] = core::InputKeyCode::F9;
	keycodes[0x6D] = core::InputKeyCode::F10;
	keycodes[0x67] = core::InputKeyCode::F11;
	keycodes[0x6F] = core::InputKeyCode::F12;
	keycodes[0x69] = core::InputKeyCode::PRINT_SCREEN;
	keycodes[0x6B] = core::InputKeyCode::F14;
	keycodes[0x71] = core::InputKeyCode::F15;
	keycodes[0x6A] = core::InputKeyCode::F16;
	keycodes[0x40] = core::InputKeyCode::F17;
	keycodes[0x4F] = core::InputKeyCode::F18;
	keycodes[0x50] = core::InputKeyCode::F19;
	keycodes[0x5A] = core::InputKeyCode::F20;
	keycodes[0x73] = core::InputKeyCode::HOME;
	keycodes[0x72] = core::InputKeyCode::INSERT;
	keycodes[0x7B] = core::InputKeyCode::LEFT;
	keycodes[0x3A] = core::InputKeyCode::LEFT_ALT;
	keycodes[0x3B] = core::InputKeyCode::LEFT_CONTROL;
	keycodes[0x38] = core::InputKeyCode::LEFT_SHIFT;
	keycodes[0x37] = core::InputKeyCode::LEFT_SUPER;
	keycodes[0x6E] = core::InputKeyCode::MENU;
	keycodes[0x47] = core::InputKeyCode::NUM_LOCK;
	keycodes[0x79] = core::InputKeyCode::PAGE_DOWN;
	keycodes[0x74] = core::InputKeyCode::PAGE_UP;
	keycodes[0x7C] = core::InputKeyCode::RIGHT;
	keycodes[0x3D] = core::InputKeyCode::RIGHT_ALT;
	keycodes[0x3E] = core::InputKeyCode::RIGHT_CONTROL;
	keycodes[0x3C] = core::InputKeyCode::RIGHT_SHIFT;
	keycodes[0x36] = core::InputKeyCode::RIGHT_SUPER;
	keycodes[0x31] = core::InputKeyCode::SPACE;
	keycodes[0x30] = core::InputKeyCode::TAB;
	keycodes[0x7E] = core::InputKeyCode::UP;

	keycodes[0x52] = core::InputKeyCode::KP_0;
	keycodes[0x53] = core::InputKeyCode::KP_1;
	keycodes[0x54] = core::InputKeyCode::KP_2;
	keycodes[0x55] = core::InputKeyCode::KP_3;
	keycodes[0x56] = core::InputKeyCode::KP_4;
	keycodes[0x57] = core::InputKeyCode::KP_5;
	keycodes[0x58] = core::InputKeyCode::KP_6;
	keycodes[0x59] = core::InputKeyCode::KP_7;
	keycodes[0x5B] = core::InputKeyCode::KP_8;
	keycodes[0x5C] = core::InputKeyCode::KP_9;
	keycodes[0x45] = core::InputKeyCode::KP_ADD;
	keycodes[0x41] = core::InputKeyCode::KP_DECIMAL;
	keycodes[0x4B] = core::InputKeyCode::KP_DIVIDE;
	keycodes[0x4C] = core::InputKeyCode::KP_ENTER;
	keycodes[0x51] = core::InputKeyCode::KP_EQUAL;
	keycodes[0x43] = core::InputKeyCode::KP_MULTIPLY;
	keycodes[0x4E] = core::InputKeyCode::KP_SUBTRACT;

	for (int scancode = 0; scancode < 256; scancode++) {
		// Store the reverse translation for faster key name lookup
		if (stappler::toInt(keycodes[scancode]) >= 0) {
			scancodes[stappler::toInt(keycodes[scancode])] = scancode;
		}
	}
}

core::InputModifier getInputModifiers(uint32_t flags) {
	core::InputModifier mods = core::InputModifier::None;

	if ((flags & NX_DEVICELSHIFTKEYMASK) != 0) {
		mods |= core::InputModifier::ShiftL;
	}
	if ((flags & NX_DEVICERSHIFTKEYMASK) != 0) {
		mods |= core::InputModifier::ShiftR;
	}
	if ((flags & NX_DEVICELCTLKEYMASK) != 0) {
		mods |= core::InputModifier::CtrlL;
	}
	if ((flags & NX_DEVICERCTLKEYMASK) != 0) {
		mods |= core::InputModifier::CtrlR;
	}
	if ((flags & NX_DEVICELALTKEYMASK) != 0) {
		mods |= core::InputModifier::AltL;
	}
	if ((flags & NX_DEVICERALTKEYMASK) != 0) {
		mods |= core::InputModifier::AltR;
	}
	if ((flags & NX_DEVICELCMDKEYMASK) != 0) {
		mods |= core::InputModifier::WinL;
	}
	if ((flags & NX_DEVICERCMDKEYMASK) != 0) {
		mods |= core::InputModifier::WinR;
	}

	if ((flags & NSEventModifierFlagCapsLock) != 0) {
		mods |= core::InputModifier::CapsLock;
	}
	if ((flags & NSEventModifierFlagShift) != 0) {
		mods |= core::InputModifier::Shift;
	}
	if ((flags & NSEventModifierFlagControl) != 0) {
		mods |= core::InputModifier::Ctrl;
	}
	if ((flags & NSEventModifierFlagOption) != 0) {
		mods |= core::InputModifier::Alt;
	}
	if ((flags & NSEventModifierFlagNumericPad) != 0) {
		mods |= core::InputModifier::NumLock;
	}
	if ((flags & NSEventModifierFlagCommand) != 0) {
		mods |= core::InputModifier::Mod3;
	}
	if ((flags & NSEventModifierFlagHelp) != 0) {
		mods |= core::InputModifier::Mod4;
	}
	if ((flags & NSEventModifierFlagFunction) != 0) {
		mods |= core::InputModifier::Mod5;
	}
	return mods;
}

} // namespace stappler::xenolith::platform
