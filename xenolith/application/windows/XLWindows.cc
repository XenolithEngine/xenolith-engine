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

#include "XLWindows.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

const KeyCodes &KeyCodes::getInstance() {
	static KeyCodes s_instance;

	return s_instance;
}

KeyCodes::KeyCodes() {
	memset(keycodes, 0, sizeof(keycodes));
	memset(scancodes, 0, sizeof(scancodes));

	keycodes[0x00B] = core::InputKeyCode::_0;
	keycodes[0x002] = core::InputKeyCode::_1;
	keycodes[0x003] = core::InputKeyCode::_2;
	keycodes[0x004] = core::InputKeyCode::_3;
	keycodes[0x005] = core::InputKeyCode::_4;
	keycodes[0x006] = core::InputKeyCode::_5;
	keycodes[0x007] = core::InputKeyCode::_6;
	keycodes[0x008] = core::InputKeyCode::_7;
	keycodes[0x009] = core::InputKeyCode::_8;
	keycodes[0x00A] = core::InputKeyCode::_9;
	keycodes[0x01E] = core::InputKeyCode::A;
	keycodes[0x030] = core::InputKeyCode::B;
	keycodes[0x02E] = core::InputKeyCode::C;
	keycodes[0x020] = core::InputKeyCode::D;
	keycodes[0x012] = core::InputKeyCode::E;
	keycodes[0x021] = core::InputKeyCode::F;
	keycodes[0x022] = core::InputKeyCode::G;
	keycodes[0x023] = core::InputKeyCode::H;
	keycodes[0x017] = core::InputKeyCode::I;
	keycodes[0x024] = core::InputKeyCode::J;
	keycodes[0x025] = core::InputKeyCode::K;
	keycodes[0x026] = core::InputKeyCode::L;
	keycodes[0x032] = core::InputKeyCode::M;
	keycodes[0x031] = core::InputKeyCode::N;
	keycodes[0x018] = core::InputKeyCode::O;
	keycodes[0x019] = core::InputKeyCode::P;
	keycodes[0x010] = core::InputKeyCode::Q;
	keycodes[0x013] = core::InputKeyCode::R;
	keycodes[0x01F] = core::InputKeyCode::S;
	keycodes[0x014] = core::InputKeyCode::T;
	keycodes[0x016] = core::InputKeyCode::U;
	keycodes[0x02F] = core::InputKeyCode::V;
	keycodes[0x011] = core::InputKeyCode::W;
	keycodes[0x02D] = core::InputKeyCode::X;
	keycodes[0x015] = core::InputKeyCode::Y;
	keycodes[0x02C] = core::InputKeyCode::Z;

	keycodes[0x028] = core::InputKeyCode::APOSTROPHE;
	keycodes[0x02B] = core::InputKeyCode::BACKSLASH;
	keycodes[0x033] = core::InputKeyCode::COMMA;
	keycodes[0x00D] = core::InputKeyCode::EQUAL;
	keycodes[0x029] = core::InputKeyCode::GRAVE_ACCENT;
	keycodes[0x01A] = core::InputKeyCode::LEFT_BRACKET;
	keycodes[0x00C] = core::InputKeyCode::MINUS;
	keycodes[0x034] = core::InputKeyCode::PERIOD;
	keycodes[0x01B] = core::InputKeyCode::RIGHT_BRACKET;
	keycodes[0x027] = core::InputKeyCode::SEMICOLON;
	keycodes[0x035] = core::InputKeyCode::SLASH;
	keycodes[0x056] = core::InputKeyCode::WORLD_2;

	keycodes[0x00E] = core::InputKeyCode::BACKSPACE;
	keycodes[0x153] = core::InputKeyCode::DELETE;
	keycodes[0x14F] = core::InputKeyCode::END;
	keycodes[0x01C] = core::InputKeyCode::ENTER;
	keycodes[0x001] = core::InputKeyCode::ESCAPE;
	keycodes[0x147] = core::InputKeyCode::HOME;
	keycodes[0x152] = core::InputKeyCode::INSERT;
	keycodes[0x15D] = core::InputKeyCode::MENU;
	keycodes[0x151] = core::InputKeyCode::PAGE_DOWN;
	keycodes[0x149] = core::InputKeyCode::PAGE_UP;
	keycodes[0x045] = core::InputKeyCode::PAUSE;
	keycodes[0x039] = core::InputKeyCode::SPACE;
	keycodes[0x00F] = core::InputKeyCode::TAB;
	keycodes[0x03A] = core::InputKeyCode::CAPS_LOCK;
	keycodes[0x145] = core::InputKeyCode::NUM_LOCK;
	keycodes[0x046] = core::InputKeyCode::SCROLL_LOCK;
	keycodes[0x03B] = core::InputKeyCode::F1;
	keycodes[0x03C] = core::InputKeyCode::F2;
	keycodes[0x03D] = core::InputKeyCode::F3;
	keycodes[0x03E] = core::InputKeyCode::F4;
	keycodes[0x03F] = core::InputKeyCode::F5;
	keycodes[0x040] = core::InputKeyCode::F6;
	keycodes[0x041] = core::InputKeyCode::F7;
	keycodes[0x042] = core::InputKeyCode::F8;
	keycodes[0x043] = core::InputKeyCode::F9;
	keycodes[0x044] = core::InputKeyCode::F10;
	keycodes[0x057] = core::InputKeyCode::F11;
	keycodes[0x058] = core::InputKeyCode::F12;
	keycodes[0x064] = core::InputKeyCode::F13;
	keycodes[0x065] = core::InputKeyCode::F14;
	keycodes[0x066] = core::InputKeyCode::F15;
	keycodes[0x067] = core::InputKeyCode::F16;
	keycodes[0x068] = core::InputKeyCode::F17;
	keycodes[0x069] = core::InputKeyCode::F18;
	keycodes[0x06A] = core::InputKeyCode::F19;
	keycodes[0x06B] = core::InputKeyCode::F20;
	keycodes[0x06C] = core::InputKeyCode::F21;
	keycodes[0x06D] = core::InputKeyCode::F22;
	keycodes[0x06E] = core::InputKeyCode::F23;
	keycodes[0x076] = core::InputKeyCode::F24;
	keycodes[0x038] = core::InputKeyCode::LEFT_ALT;
	keycodes[0x01D] = core::InputKeyCode::LEFT_CONTROL;
	keycodes[0x02A] = core::InputKeyCode::LEFT_SHIFT;
	keycodes[0x15B] = core::InputKeyCode::LEFT_SUPER;
	keycodes[0x137] = core::InputKeyCode::PRINT_SCREEN;
	keycodes[0x138] = core::InputKeyCode::RIGHT_ALT;
	keycodes[0x11D] = core::InputKeyCode::RIGHT_CONTROL;
	keycodes[0x036] = core::InputKeyCode::RIGHT_SHIFT;
	keycodes[0x15C] = core::InputKeyCode::RIGHT_SUPER;
	keycodes[0x150] = core::InputKeyCode::DOWN;
	keycodes[0x14B] = core::InputKeyCode::LEFT;
	keycodes[0x14D] = core::InputKeyCode::RIGHT;
	keycodes[0x148] = core::InputKeyCode::UP;

	keycodes[0x052] = core::InputKeyCode::KP_0;
	keycodes[0x04F] = core::InputKeyCode::KP_1;
	keycodes[0x050] = core::InputKeyCode::KP_2;
	keycodes[0x051] = core::InputKeyCode::KP_3;
	keycodes[0x04B] = core::InputKeyCode::KP_4;
	keycodes[0x04C] = core::InputKeyCode::KP_5;
	keycodes[0x04D] = core::InputKeyCode::KP_6;
	keycodes[0x047] = core::InputKeyCode::KP_7;
	keycodes[0x048] = core::InputKeyCode::KP_8;
	keycodes[0x049] = core::InputKeyCode::KP_9;
	keycodes[0x04E] = core::InputKeyCode::KP_ADD;
	keycodes[0x053] = core::InputKeyCode::KP_DECIMAL;
	keycodes[0x135] = core::InputKeyCode::KP_DIVIDE;
	keycodes[0x11C] = core::InputKeyCode::KP_ENTER;
	keycodes[0x059] = core::InputKeyCode::KP_EQUAL;
	keycodes[0x037] = core::InputKeyCode::KP_MULTIPLY;
	keycodes[0x04A] = core::InputKeyCode::KP_SUBTRACT;

	for (auto scancode = 0; scancode < 512; scancode++) {
		if (keycodes[scancode] != core::InputKeyCode::Unknown) {
			scancodes[toInt(keycodes[scancode])] = scancode;
		}
	}
}

core::InputModifier KeyCodes::getKeyMods() {
	core::InputModifier mods = core::InputModifier::None;

	if (GetKeyState(VK_SHIFT) & 0x8000) {
		mods |= core::InputModifier::Shift;
	}
	if (GetKeyState(VK_RSHIFT) & 0x8000) {
		mods |= core::InputModifier::ShiftR;
	}
	if (GetKeyState(VK_LSHIFT) & 0x8000) {
		mods |= core::InputModifier::ShiftL;
	}
	if (GetKeyState(VK_CONTROL) & 0x8000) {
		mods |= core::InputModifier::Ctrl;
	}
	if (GetKeyState(VK_RCONTROL) & 0x8000) {
		mods |= core::InputModifier::CtrlR;
	}
	if (GetKeyState(VK_LCONTROL) & 0x8000) {
		mods |= core::InputModifier::CtrlL;
	}
	if (GetKeyState(VK_MENU) & 0x8000) {
		mods |= core::InputModifier::Menu;
	}
	if (GetKeyState(VK_RMENU) & 0x8000) {
		mods |= core::InputModifier::MenuR;
	}
	if (GetKeyState(VK_LMENU) & 0x8000) {
		mods |= core::InputModifier::MenuL;
	}
	if (GetKeyState(VK_LWIN) & 0x8000) {
		mods |= core::InputModifier::Win | core::InputModifier::WinL;
	}
	if (GetKeyState(VK_RWIN) & 0x8000) {
		mods |= core::InputModifier::Win | core::InputModifier::WinR;
	}
	if (GetKeyState(VK_CAPITAL) & 1) {
		mods |= core::InputModifier::CapsLock;
	}
	if (GetKeyState(VK_NUMLOCK) & 1) {
		mods |= core::InputModifier::NumLock;
	}
	if (GetKeyState(VK_SCROLL) & 1) {
		mods |= core::InputModifier::ScrollLock;
	}
	return mods;
}

} // namespace stappler::xenolith::platform
