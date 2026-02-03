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

#define __SPRT_USE_STL 1
#define __SPRT_BUILD 1
#define UNICODE 1
#define _UNICODE 1

#include <sprt/runtime/config.h>
#include <sprt/c/bits/__sprt_def.h>

#if __SPRT_RUNTIME_CONFIG_HAVE_WINDOW && SPRT_WINDOWS

#include <sprt/runtime/stringview.h>

#include <private/window/windows/SPRTWinWindows.h>

#define NOMINMAX 1
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#undef __deallocate
#undef DELETE

namespace sprt::window {

const KeyCodes &KeyCodes::getInstance() {
	static KeyCodes s_instance;

	return s_instance;
}

KeyCodes::KeyCodes() {
	__constexpr_memset(keycodes, InputKeyCode(0), sizeof(keycodes));
	__constexpr_memset(scancodes, uint16_t(0), sizeof(scancodes));

	keycodes[0x00B] = InputKeyCode::_0;
	keycodes[0x002] = InputKeyCode::_1;
	keycodes[0x003] = InputKeyCode::_2;
	keycodes[0x004] = InputKeyCode::_3;
	keycodes[0x005] = InputKeyCode::_4;
	keycodes[0x006] = InputKeyCode::_5;
	keycodes[0x007] = InputKeyCode::_6;
	keycodes[0x008] = InputKeyCode::_7;
	keycodes[0x009] = InputKeyCode::_8;
	keycodes[0x00A] = InputKeyCode::_9;
	keycodes[0x01E] = InputKeyCode::A;
	keycodes[0x030] = InputKeyCode::B;
	keycodes[0x02E] = InputKeyCode::C;
	keycodes[0x020] = InputKeyCode::D;
	keycodes[0x012] = InputKeyCode::E;
	keycodes[0x021] = InputKeyCode::F;
	keycodes[0x022] = InputKeyCode::G;
	keycodes[0x023] = InputKeyCode::H;
	keycodes[0x017] = InputKeyCode::I;
	keycodes[0x024] = InputKeyCode::J;
	keycodes[0x025] = InputKeyCode::K;
	keycodes[0x026] = InputKeyCode::L;
	keycodes[0x032] = InputKeyCode::M;
	keycodes[0x031] = InputKeyCode::N;
	keycodes[0x018] = InputKeyCode::O;
	keycodes[0x019] = InputKeyCode::P;
	keycodes[0x010] = InputKeyCode::Q;
	keycodes[0x013] = InputKeyCode::R;
	keycodes[0x01F] = InputKeyCode::S;
	keycodes[0x014] = InputKeyCode::T;
	keycodes[0x016] = InputKeyCode::U;
	keycodes[0x02F] = InputKeyCode::V;
	keycodes[0x011] = InputKeyCode::W;
	keycodes[0x02D] = InputKeyCode::X;
	keycodes[0x015] = InputKeyCode::Y;
	keycodes[0x02C] = InputKeyCode::Z;

	keycodes[0x028] = InputKeyCode::APOSTROPHE;
	keycodes[0x02B] = InputKeyCode::BACKSLASH;
	keycodes[0x033] = InputKeyCode::COMMA;
	keycodes[0x00D] = InputKeyCode::EQUAL;
	keycodes[0x029] = InputKeyCode::GRAVE_ACCENT;
	keycodes[0x01A] = InputKeyCode::LEFT_BRACKET;
	keycodes[0x00C] = InputKeyCode::MINUS;
	keycodes[0x034] = InputKeyCode::PERIOD;
	keycodes[0x01B] = InputKeyCode::RIGHT_BRACKET;
	keycodes[0x027] = InputKeyCode::SEMICOLON;
	keycodes[0x035] = InputKeyCode::SLASH;
	keycodes[0x056] = InputKeyCode::WORLD_2;

	keycodes[0x00E] = InputKeyCode::BACKSPACE;
	keycodes[0x153] = InputKeyCode::DELETE;
	keycodes[0x14F] = InputKeyCode::END;
	keycodes[0x01C] = InputKeyCode::ENTER;
	keycodes[0x001] = InputKeyCode::ESCAPE;
	keycodes[0x147] = InputKeyCode::HOME;
	keycodes[0x152] = InputKeyCode::INSERT;
	keycodes[0x15D] = InputKeyCode::MENU;
	keycodes[0x151] = InputKeyCode::PAGE_DOWN;
	keycodes[0x149] = InputKeyCode::PAGE_UP;
	keycodes[0x045] = InputKeyCode::PAUSE;
	keycodes[0x039] = InputKeyCode::SPACE;
	keycodes[0x00F] = InputKeyCode::TAB;
	keycodes[0x03A] = InputKeyCode::CAPS_LOCK;
	keycodes[0x145] = InputKeyCode::NUM_LOCK;
	keycodes[0x046] = InputKeyCode::SCROLL_LOCK;
	keycodes[0x03B] = InputKeyCode::F1;
	keycodes[0x03C] = InputKeyCode::F2;
	keycodes[0x03D] = InputKeyCode::F3;
	keycodes[0x03E] = InputKeyCode::F4;
	keycodes[0x03F] = InputKeyCode::F5;
	keycodes[0x040] = InputKeyCode::F6;
	keycodes[0x041] = InputKeyCode::F7;
	keycodes[0x042] = InputKeyCode::F8;
	keycodes[0x043] = InputKeyCode::F9;
	keycodes[0x044] = InputKeyCode::F10;
	keycodes[0x057] = InputKeyCode::F11;
	keycodes[0x058] = InputKeyCode::F12;
	keycodes[0x064] = InputKeyCode::F13;
	keycodes[0x065] = InputKeyCode::F14;
	keycodes[0x066] = InputKeyCode::F15;
	keycodes[0x067] = InputKeyCode::F16;
	keycodes[0x068] = InputKeyCode::F17;
	keycodes[0x069] = InputKeyCode::F18;
	keycodes[0x06A] = InputKeyCode::F19;
	keycodes[0x06B] = InputKeyCode::F20;
	keycodes[0x06C] = InputKeyCode::F21;
	keycodes[0x06D] = InputKeyCode::F22;
	keycodes[0x06E] = InputKeyCode::F23;
	keycodes[0x076] = InputKeyCode::F24;
	keycodes[0x038] = InputKeyCode::LEFT_ALT;
	keycodes[0x01D] = InputKeyCode::LEFT_CONTROL;
	keycodes[0x02A] = InputKeyCode::LEFT_SHIFT;
	keycodes[0x15B] = InputKeyCode::LEFT_SUPER;
	keycodes[0x137] = InputKeyCode::PRINT_SCREEN;
	keycodes[0x138] = InputKeyCode::RIGHT_ALT;
	keycodes[0x11D] = InputKeyCode::RIGHT_CONTROL;
	keycodes[0x036] = InputKeyCode::RIGHT_SHIFT;
	keycodes[0x15C] = InputKeyCode::RIGHT_SUPER;
	keycodes[0x150] = InputKeyCode::DOWN;
	keycodes[0x14B] = InputKeyCode::LEFT;
	keycodes[0x14D] = InputKeyCode::RIGHT;
	keycodes[0x148] = InputKeyCode::UP;

	keycodes[0x052] = InputKeyCode::KP_0;
	keycodes[0x04F] = InputKeyCode::KP_1;
	keycodes[0x050] = InputKeyCode::KP_2;
	keycodes[0x051] = InputKeyCode::KP_3;
	keycodes[0x04B] = InputKeyCode::KP_4;
	keycodes[0x04C] = InputKeyCode::KP_5;
	keycodes[0x04D] = InputKeyCode::KP_6;
	keycodes[0x047] = InputKeyCode::KP_7;
	keycodes[0x048] = InputKeyCode::KP_8;
	keycodes[0x049] = InputKeyCode::KP_9;
	keycodes[0x04E] = InputKeyCode::KP_ADD;
	keycodes[0x053] = InputKeyCode::KP_DECIMAL;
	keycodes[0x135] = InputKeyCode::KP_DIVIDE;
	keycodes[0x11C] = InputKeyCode::KP_ENTER;
	keycodes[0x059] = InputKeyCode::KP_EQUAL;
	keycodes[0x037] = InputKeyCode::KP_MULTIPLY;
	keycodes[0x04A] = InputKeyCode::KP_SUBTRACT;

	for (auto scancode = 0; scancode < 512; scancode++) {
		if (keycodes[scancode] != InputKeyCode::Unknown) {
			scancodes[toInt(keycodes[scancode])] = scancode;
		}
	}
}

InputModifier KeyCodes::getKeyMods() {
	InputModifier mods = InputModifier::None;

	if (GetKeyState(VK_SHIFT) & 0x8000) {
		mods |= InputModifier::Shift;
	}
	if (GetKeyState(VK_RSHIFT) & 0x8000) {
		mods |= InputModifier::ShiftR;
	}
	if (GetKeyState(VK_LSHIFT) & 0x8000) {
		mods |= InputModifier::ShiftL;
	}
	if (GetKeyState(VK_CONTROL) & 0x8000) {
		mods |= InputModifier::Ctrl;
	}
	if (GetKeyState(VK_RCONTROL) & 0x8000) {
		mods |= InputModifier::CtrlR;
	}
	if (GetKeyState(VK_LCONTROL) & 0x8000) {
		mods |= InputModifier::CtrlL;
	}
	if (GetKeyState(VK_MENU) & 0x8000) {
		mods |= InputModifier::Menu;
	}
	if (GetKeyState(VK_RMENU) & 0x8000) {
		mods |= InputModifier::MenuR;
	}
	if (GetKeyState(VK_LMENU) & 0x8000) {
		mods |= InputModifier::MenuL;
	}
	if (GetKeyState(VK_LWIN) & 0x8000) {
		mods |= InputModifier::Win | InputModifier::WinL;
	}
	if (GetKeyState(VK_RWIN) & 0x8000) {
		mods |= InputModifier::Win | InputModifier::WinR;
	}
	if (GetKeyState(VK_CAPITAL) & 1) {
		mods |= InputModifier::CapsLock;
	}
	if (GetKeyState(VK_NUMLOCK) & 1) {
		mods |= InputModifier::NumLock;
	}
	if (GetKeyState(VK_SCROLL) & 1) {
		mods |= InputModifier::ScrollLock;
	}
	return mods;
}

} // namespace sprt::window

#include "SPRTWinWindowsContextController.cc"
#include "SPRTWinWindowsDisplayConfigManager.cc"
#include "SPRTWinWindowsMessageWindow.cc"
#include "SPRTWinWindowsWindow.cc"
#include "SPRTWinWindowsWindowClass.cc"

#endif
