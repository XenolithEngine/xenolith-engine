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

#include "XLAndroidInput.h"
#include "XLAndroidActivity.h"
#include "XLContext.h"

#include <android/looper.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {
/* AKEYCODE_BACK mapped to ESC
 * AKEYCODE_FORWARD - ENTER
 * AKEYCODE_DPAD_* mapped to arrows, AKEYCODE_DPAD_CENTER to Enter
 * AKEYCODE_SYM - WORLD_1
 * AKEYCODE_SWITCH_CHARSET - WORLD_2
 * AKEYCODE_DEL - BACKSPACE
 *
 * === Unknown (can be used with platform-depended keycodes in event) ===
 * AKEYCODE_CALL
 * AKEYCODE_ENDCALL
 * AKEYCODE_STAR
 * AKEYCODE_POUND
 * AKEYCODE_VOLUME_UP
 * AKEYCODE_VOLUME_DOWN
 * AKEYCODE_POWER
 * AKEYCODE_CAMERA
 * AKEYCODE_CLEAR
 * AKEYCODE_EXPLORER
 * AKEYCODE_ENVELOPE
 * AKEYCODE_AT
 * AKEYCODE_NUM
 * AKEYCODE_HEADSETHOOK
 * AKEYCODE_FOCUS
 * AKEYCODE_PLUS
 * AKEYCODE_NOTIFICATION
 * AKEYCODE_SEARCH
 * AKEYCODE_MEDIA_PLAY_PAUSE
 * AKEYCODE_MEDIA_STOP
 * AKEYCODE_MEDIA_NEXT
 * AKEYCODE_MEDIA_PREVIOUS
 * AKEYCODE_MEDIA_REWIND
 * AKEYCODE_MEDIA_FAST_FORWARD
 * AKEYCODE_MUTE
 * AKEYCODE_PICTSYMBOLS
 * AKEYCODE_BUTTON_A
 * AKEYCODE_BUTTON_B
 * AKEYCODE_BUTTON_C
 * AKEYCODE_BUTTON_X
 * AKEYCODE_BUTTON_Y
 * AKEYCODE_BUTTON_Z
 * AKEYCODE_BUTTON_L1
 * AKEYCODE_BUTTON_R1
 * AKEYCODE_BUTTON_L2
 * AKEYCODE_BUTTON_R2
 * AKEYCODE_BUTTON_THUMBL
 * AKEYCODE_BUTTON_THUMBR
 * AKEYCODE_BUTTON_START
 * AKEYCODE_BUTTON_SELECT
 * AKEYCODE_BUTTON_MODE
 * AKEYCODE_FUNCTION
 * AKEYCODE_MEDIA_PLAY
 * AKEYCODE_MEDIA_PAUSE
 * AKEYCODE_MEDIA_CLOSE
 * AKEYCODE_MEDIA_EJECT
 * AKEYCODE_MEDIA_RECORD
 * AKEYCODE_NUMPAD_DOT
 * AKEYCODE_NUMPAD_COMMA
 * AKEYCODE_NUMPAD_LEFT_PAREN
 * AKEYCODE_NUMPAD_RIGHT_PAREN
 * AKEYCODE_VOLUME_MUTE
 * AKEYCODE_INFO
 * AKEYCODE_CHANNEL_UP
 * AKEYCODE_CHANNEL_DOWN
 * AKEYCODE_ZOOM_IN
 * AKEYCODE_ZOOM_OUT
 * AKEYCODE_TV
 * AKEYCODE_WINDOW
 * AKEYCODE_GUIDE
 * AKEYCODE_DVR
 * AKEYCODE_BOOKMARK
 * AKEYCODE_CAPTIONS
 * AKEYCODE_SETTINGS
 * AKEYCODE_TV_POWER
 * AKEYCODE_TV_INPUT
 * AKEYCODE_STB_POWER
 * AKEYCODE_STB_INPUT
 * AKEYCODE_AVR_POWER
 * AKEYCODE_AVR_INPUT
 * AKEYCODE_PROG_RED
 * AKEYCODE_PROG_GREEN
 * AKEYCODE_PROG_YELLOW
 * AKEYCODE_PROG_BLUE
 * AKEYCODE_LANGUAGE_SWITCH
 * AKEYCODE_MANNER_MODE
 * AKEYCODE_3D_MODE
 * AKEYCODE_CONTACTS
 * AKEYCODE_CALENDAR
 * AKEYCODE_MUSIC
 * AKEYCODE_CALCULATOR
 * AKEYCODE_ZENKAKU_HANKAKU
 * AKEYCODE_EISU
 * AKEYCODE_MUHENKAN
 * AKEYCODE_HENKAN
 * AKEYCODE_KATAKANA_HIRAGANA
 * AKEYCODE_YEN
 * AKEYCODE_RO
 * AKEYCODE_KANA
 * AKEYCODE_ASSIST
 * AKEYCODE_BRIGHTNESS_DOWN
 * AKEYCODE_BRIGHTNESS_UP
 * AKEYCODE_MEDIA_AUDIO_TRACK
 * AKEYCODE_SLEEP
 * AKEYCODE_WAKEUP
 * AKEYCODE_PAIRING
 * AKEYCODE_MEDIA_TOP_MENU
 * AKEYCODE_11
 * AKEYCODE_12
 * AKEYCODE_LAST_CHANNEL
 * AKEYCODE_TV_DATA_SERVICE
 * AKEYCODE_VOICE_ASSIST
 * AKEYCODE_TV_RADIO_SERVICE
 * AKEYCODE_TV_TELETEXT
 * AKEYCODE_TV_NUMBER_ENTRY
 * AKEYCODE_TV_TERRESTRIAL_ANALOG
 * AKEYCODE_TV_TERRESTRIAL_DIGITAL
 * AKEYCODE_TV_SATELLITE
 * AKEYCODE_TV_SATELLITE_BS
 * AKEYCODE_TV_SATELLITE_CS
 * AKEYCODE_TV_SATELLITE_SERVICE
 * AKEYCODE_TV_NETWORK
 * AKEYCODE_TV_ANTENNA_CABLE
 * AKEYCODE_TV_INPUT_HDMI_1
 * AKEYCODE_TV_INPUT_HDMI_2
 * AKEYCODE_TV_INPUT_HDMI_3
 * AKEYCODE_TV_INPUT_HDMI_4
 * AKEYCODE_TV_INPUT_COMPOSITE_1
 * AKEYCODE_TV_INPUT_COMPOSITE_2
 * AKEYCODE_TV_INPUT_COMPONENT_1
 * AKEYCODE_TV_INPUT_COMPONENT_2
 * AKEYCODE_TV_INPUT_VGA_1
 * AKEYCODE_TV_AUDIO_DESCRIPTION
 * AKEYCODE_TV_AUDIO_DESCRIPTION_MIX_UP
 * AKEYCODE_TV_AUDIO_DESCRIPTION_MIX_DOWN
 * AKEYCODE_TV_ZOOM_MODE
 * AKEYCODE_TV_CONTENTS_MENU
 * AKEYCODE_TV_MEDIA_CONTEXT_MENU
 * AKEYCODE_TV_TIMER_PROGRAMMING
 * AKEYCODE_HELP
 * AKEYCODE_NAVIGATE_PREVIOUS
 * AKEYCODE_NAVIGATE_NEXT
 * AKEYCODE_NAVIGATE_IN
 * AKEYCODE_NAVIGATE_OUT
 * AKEYCODE_STEM_PRIMARY
 * AKEYCODE_STEM_1
 * AKEYCODE_STEM_2
 * AKEYCODE_STEM_3
 * AKEYCODE_DPAD_UP_LEFT
 * AKEYCODE_DPAD_DOWN_LEFT
 * AKEYCODE_DPAD_UP_RIGHT
 * AKEYCODE_DPAD_DOWN_RIGHT
 * AKEYCODE_MEDIA_SKIP_FORWARD
 * AKEYCODE_MEDIA_SKIP_BACKWARD
 * AKEYCODE_MEDIA_STEP_FORWARD
 * AKEYCODE_MEDIA_STEP_BACKWARD
 * AKEYCODE_SOFT_SLEEP
 * AKEYCODE_CUT
 * AKEYCODE_COPY
 * AKEYCODE_PASTE
 * AKEYCODE_SYSTEM_NAVIGATION_UP
 * AKEYCODE_SYSTEM_NAVIGATION_DOWN
 * AKEYCODE_SYSTEM_NAVIGATION_LEFT
 * AKEYCODE_SYSTEM_NAVIGATION_RIGHT
 * AKEYCODE_ALL_APPS
 * AKEYCODE_REFRESH
 * AKEYCODE_THUMBS_UP
 * AKEYCODE_THUMBS_DOWN
 * AKEYCODE_PROFILE_SWITCH
 */

core::InputKeyCode InputQueue::KeyCodes[] = {
	core::InputKeyCode::Unknown, // AKEYCODE_UNKNOWN
	core::InputKeyCode::LEFT, // AKEYCODE_SOFT_LEFT
	core::InputKeyCode::RIGHT, // AKEYCODE_SOFT_RIGHT
	core::InputKeyCode::HOME, // AKEYCODE_HOME
	core::InputKeyCode::ESCAPE, // AKEYCODE_BACK
	core::InputKeyCode::Unknown, // AKEYCODE_CALL
	core::InputKeyCode::Unknown, // AKEYCODE_ENDCALL
	core::InputKeyCode::_0, // AKEYCODE_0
	core::InputKeyCode::_1, // AKEYCODE_1
	core::InputKeyCode::_2, // AKEYCODE_2
	core::InputKeyCode::_3, // AKEYCODE_3
	core::InputKeyCode::_4, // AKEYCODE_4
	core::InputKeyCode::_5, // AKEYCODE_5
	core::InputKeyCode::_6, // AKEYCODE_6
	core::InputKeyCode::_7, // AKEYCODE_7
	core::InputKeyCode::_8, // AKEYCODE_8
	core::InputKeyCode::_9, // AKEYCODE_9
	core::InputKeyCode::Unknown, // AKEYCODE_STAR
	core::InputKeyCode::Unknown, // AKEYCODE_POUND
	core::InputKeyCode::UP, // AKEYCODE_DPAD_UP
	core::InputKeyCode::DOWN, // AKEYCODE_DPAD_DOWN
	core::InputKeyCode::LEFT, // AKEYCODE_DPAD_LEFT
	core::InputKeyCode::RIGHT, // AKEYCODE_DPAD_RIGHT
	core::InputKeyCode::ENTER, // AKEYCODE_DPAD_CENTER
	core::InputKeyCode::Unknown, // AKEYCODE_VOLUME_UP
	core::InputKeyCode::Unknown, // AKEYCODE_VOLUME_DOWN
	core::InputKeyCode::Unknown, // AKEYCODE_POWER
	core::InputKeyCode::Unknown, // AKEYCODE_CAMERA
	core::InputKeyCode::Unknown, // AKEYCODE_CLEAR
	core::InputKeyCode::A, // AKEYCODE_A
	core::InputKeyCode::B, // AKEYCODE_B
	core::InputKeyCode::C, // AKEYCODE_C
	core::InputKeyCode::D, // AKEYCODE_D
	core::InputKeyCode::E, // AKEYCODE_E
	core::InputKeyCode::F, // AKEYCODE_F
	core::InputKeyCode::G, // AKEYCODE_G
	core::InputKeyCode::H, // AKEYCODE_H
	core::InputKeyCode::I, // AKEYCODE_I
	core::InputKeyCode::J, // AKEYCODE_J
	core::InputKeyCode::K, // AKEYCODE_K
	core::InputKeyCode::L, // AKEYCODE_L
	core::InputKeyCode::M, // AKEYCODE_M
	core::InputKeyCode::N, // AKEYCODE_N
	core::InputKeyCode::O, // AKEYCODE_O
	core::InputKeyCode::P, // AKEYCODE_P
	core::InputKeyCode::Q, // AKEYCODE_Q
	core::InputKeyCode::R, // AKEYCODE_R
	core::InputKeyCode::S, // AKEYCODE_S
	core::InputKeyCode::T, // AKEYCODE_T
	core::InputKeyCode::U, // AKEYCODE_U
	core::InputKeyCode::V, // AKEYCODE_V
	core::InputKeyCode::W, // AKEYCODE_W
	core::InputKeyCode::X, // AKEYCODE_X
	core::InputKeyCode::Y, // AKEYCODE_Y
	core::InputKeyCode::Z, // AKEYCODE_Z
	core::InputKeyCode::COMMA, // AKEYCODE_COMMA
	core::InputKeyCode::PERIOD, // AKEYCODE_PERIOD
	core::InputKeyCode::LEFT_ALT, // AKEYCODE_ALT_LEFT
	core::InputKeyCode::RIGHT_ALT, // AKEYCODE_ALT_RIGHT
	core::InputKeyCode::LEFT_SHIFT, // AKEYCODE_SHIFT_LEFT
	core::InputKeyCode::RIGHT_SHIFT, // AKEYCODE_SHIFT_RIGHT
	core::InputKeyCode::TAB, // AKEYCODE_TAB
	core::InputKeyCode::SPACE, // AKEYCODE_SPACE
	core::InputKeyCode::WORLD_1, // AKEYCODE_SYM
	core::InputKeyCode::Unknown, // AKEYCODE_EXPLORER
	core::InputKeyCode::Unknown, // AKEYCODE_ENVELOPE
	core::InputKeyCode::ENTER, // AKEYCODE_ENTER
	core::InputKeyCode::BACKSPACE, // AKEYCODE_DEL
	core::InputKeyCode::GRAVE_ACCENT, // AKEYCODE_GRAVE
	core::InputKeyCode::MINUS, // AKEYCODE_MINUS
	core::InputKeyCode::EQUAL, // AKEYCODE_EQUALS
	core::InputKeyCode::LEFT_BRACKET, // AKEYCODE_LEFT_BRACKET
	core::InputKeyCode::RIGHT_BRACKET, // AKEYCODE_RIGHT_BRACKET
	core::InputKeyCode::BACKSLASH, // AKEYCODE_BACKSLASH
	core::InputKeyCode::SEMICOLON, // AKEYCODE_SEMICOLON
	core::InputKeyCode::APOSTROPHE, // AKEYCODE_APOSTROPHE
	core::InputKeyCode::SLASH, // AKEYCODE_SLASH
	core::InputKeyCode::Unknown, // AKEYCODE_AT
	core::InputKeyCode::Unknown, // AKEYCODE_NUM
	core::InputKeyCode::Unknown, // AKEYCODE_HEADSETHOOK
	core::InputKeyCode::Unknown, // AKEYCODE_FOCUS
	core::InputKeyCode::Unknown, // AKEYCODE_PLUS
	core::InputKeyCode::MENU, // AKEYCODE_MENU
	core::InputKeyCode::Unknown, // AKEYCODE_NOTIFICATION
	core::InputKeyCode::Unknown, // AKEYCODE_SEARCH
	core::InputKeyCode::Unknown, // AKEYCODE_MEDIA_PLAY_PAUSE
	core::InputKeyCode::Unknown, // AKEYCODE_MEDIA_STOP
	core::InputKeyCode::Unknown, // AKEYCODE_MEDIA_NEXT
	core::InputKeyCode::Unknown, // AKEYCODE_MEDIA_PREVIOUS
	core::InputKeyCode::Unknown, // AKEYCODE_MEDIA_REWIND
	core::InputKeyCode::Unknown, // AKEYCODE_MEDIA_FAST_FORWARD
	core::InputKeyCode::Unknown, // AKEYCODE_MUTE
	core::InputKeyCode::PAGE_UP, // AKEYCODE_PAGE_UP
	core::InputKeyCode::PAGE_DOWN, // AKEYCODE_PAGE_DOWN
	core::InputKeyCode::Unknown, // AKEYCODE_PICTSYMBOLS
	core::InputKeyCode::WORLD_2, // AKEYCODE_SWITCH_CHARSET
	core::InputKeyCode::Unknown, // AKEYCODE_BUTTON_A
	core::InputKeyCode::Unknown, // AKEYCODE_BUTTON_B
	core::InputKeyCode::Unknown, // AKEYCODE_BUTTON_C
	core::InputKeyCode::Unknown, // AKEYCODE_BUTTON_X
	core::InputKeyCode::Unknown, // AKEYCODE_BUTTON_Y
	core::InputKeyCode::Unknown, // AKEYCODE_BUTTON_Z
	core::InputKeyCode::Unknown, // AKEYCODE_BUTTON_L1
	core::InputKeyCode::Unknown, // AKEYCODE_BUTTON_R1
	core::InputKeyCode::Unknown, // AKEYCODE_BUTTON_L2
	core::InputKeyCode::Unknown, // AKEYCODE_BUTTON_R2
	core::InputKeyCode::Unknown, // AKEYCODE_BUTTON_THUMBL
	core::InputKeyCode::Unknown, // AKEYCODE_BUTTON_THUMBR
	core::InputKeyCode::Unknown, // AKEYCODE_BUTTON_START
	core::InputKeyCode::Unknown, // AKEYCODE_BUTTON_SELECT
	core::InputKeyCode::Unknown, // AKEYCODE_BUTTON_MODE
	core::InputKeyCode::ESCAPE, // AKEYCODE_ESCAPE
	core::InputKeyCode::DELETE, // AKEYCODE_FORWARD_DEL
	core::InputKeyCode::LEFT_CONTROL, // AKEYCODE_CTRL_LEFT
	core::InputKeyCode::RIGHT_CONTROL, // AKEYCODE_CTRL_RIGHT
	core::InputKeyCode::CAPS_LOCK, // AKEYCODE_CAPS_LOCK
	core::InputKeyCode::SCROLL_LOCK, // AKEYCODE_SCROLL_LOCK
	core::InputKeyCode::LEFT_SUPER, // AKEYCODE_META_LEFT
	core::InputKeyCode::RIGHT_SUPER, // AKEYCODE_META_RIGHT
	core::InputKeyCode::Unknown, // AKEYCODE_FUNCTION
	core::InputKeyCode::PRINT_SCREEN, // AKEYCODE_SYSRQ
	core::InputKeyCode::PAUSE, // AKEYCODE_BREAK
	core::InputKeyCode::HOME, // AKEYCODE_MOVE_HOME
	core::InputKeyCode::END, // AKEYCODE_MOVE_END
	core::InputKeyCode::INSERT, // AKEYCODE_INSERT
	core::InputKeyCode::ENTER, // AKEYCODE_FORWARD
	core::InputKeyCode::Unknown, // AKEYCODE_MEDIA_PLAY
	core::InputKeyCode::Unknown, // AKEYCODE_MEDIA_PAUSE
	core::InputKeyCode::Unknown, // AKEYCODE_MEDIA_CLOSE
	core::InputKeyCode::Unknown, // AKEYCODE_MEDIA_EJECT
	core::InputKeyCode::Unknown, // AKEYCODE_MEDIA_RECORD
	core::InputKeyCode::F1, // AKEYCODE_F1
	core::InputKeyCode::F2, // AKEYCODE_F2
	core::InputKeyCode::F3, // AKEYCODE_F3
	core::InputKeyCode::F4, // AKEYCODE_F4
	core::InputKeyCode::F5, // AKEYCODE_F5
	core::InputKeyCode::F6, // AKEYCODE_F6
	core::InputKeyCode::F7, // AKEYCODE_F7
	core::InputKeyCode::F8, // AKEYCODE_F8
	core::InputKeyCode::F9, // AKEYCODE_F9
	core::InputKeyCode::F10, // AKEYCODE_F10
	core::InputKeyCode::F11, // AKEYCODE_F11
	core::InputKeyCode::F12, // AKEYCODE_F12
	core::InputKeyCode::NUM_LOCK, // AKEYCODE_NUM_LOCK
	core::InputKeyCode::KP_0, // AKEYCODE_NUMPAD_0
	core::InputKeyCode::KP_1, // AKEYCODE_NUMPAD_1
	core::InputKeyCode::KP_2, // AKEYCODE_NUMPAD_2
	core::InputKeyCode::KP_3, // AKEYCODE_NUMPAD_3
	core::InputKeyCode::KP_4, // AKEYCODE_NUMPAD_4
	core::InputKeyCode::KP_5, // AKEYCODE_NUMPAD_5
	core::InputKeyCode::KP_6, // AKEYCODE_NUMPAD_6
	core::InputKeyCode::KP_7, // AKEYCODE_NUMPAD_7
	core::InputKeyCode::KP_8, // AKEYCODE_NUMPAD_8
	core::InputKeyCode::KP_9, // AKEYCODE_NUMPAD_9
	core::InputKeyCode::KP_DIVIDE, // AKEYCODE_NUMPAD_DIVIDE
	core::InputKeyCode::KP_MULTIPLY, // AKEYCODE_NUMPAD_MULTIPLY
	core::InputKeyCode::KP_SUBTRACT, // AKEYCODE_NUMPAD_SUBTRACT
	core::InputKeyCode::KP_ADD, // AKEYCODE_NUMPAD_ADD
	core::InputKeyCode::Unknown, // AKEYCODE_NUMPAD_DOT
	core::InputKeyCode::Unknown, // AKEYCODE_NUMPAD_COMMA
	core::InputKeyCode::KP_ENTER, // AKEYCODE_NUMPAD_ENTER
	core::InputKeyCode::KP_EQUAL, // AKEYCODE_NUMPAD_EQUALS
	core::InputKeyCode::Unknown, // AKEYCODE_NUMPAD_LEFT_PAREN
	core::InputKeyCode::Unknown, // AKEYCODE_NUMPAD_RIGHT_PAREN
	core::InputKeyCode::Unknown, // AKEYCODE_VOLUME_MUTE
	core::InputKeyCode::Unknown, // AKEYCODE_INFO
	core::InputKeyCode::Unknown, // AKEYCODE_CHANNEL_UP
	core::InputKeyCode::Unknown, // AKEYCODE_CHANNEL_DOWN
	core::InputKeyCode::Unknown, // AKEYCODE_ZOOM_IN
	core::InputKeyCode::Unknown, // AKEYCODE_ZOOM_OUT
	core::InputKeyCode::Unknown, // AKEYCODE_TV
	core::InputKeyCode::Unknown, // AKEYCODE_WINDOW
	core::InputKeyCode::Unknown, // AKEYCODE_GUIDE
	core::InputKeyCode::Unknown, // AKEYCODE_DVR
	core::InputKeyCode::Unknown, // AKEYCODE_BOOKMARK
	core::InputKeyCode::Unknown, // AKEYCODE_CAPTIONS
	core::InputKeyCode::Unknown, // AKEYCODE_SETTINGS
	core::InputKeyCode::Unknown, // AKEYCODE_TV_POWER
	core::InputKeyCode::Unknown, // AKEYCODE_TV_INPUT
	core::InputKeyCode::Unknown, // AKEYCODE_STB_POWER
	core::InputKeyCode::Unknown, // AKEYCODE_STB_INPUT
	core::InputKeyCode::Unknown, // AKEYCODE_AVR_POWER
	core::InputKeyCode::Unknown, // AKEYCODE_AVR_INPUT
	core::InputKeyCode::Unknown, // AKEYCODE_PROG_RED
	core::InputKeyCode::Unknown, // AKEYCODE_PROG_GREEN
	core::InputKeyCode::Unknown, // AKEYCODE_PROG_YELLOW
	core::InputKeyCode::Unknown, // AKEYCODE_PROG_BLUE
	core::InputKeyCode::Unknown, // AKEYCODE_APP_SWITCH
	core::InputKeyCode::F1, // AKEYCODE_BUTTON_1
	core::InputKeyCode::F2, // AKEYCODE_BUTTON_2
	core::InputKeyCode::F3, // AKEYCODE_BUTTON_3
	core::InputKeyCode::F4, // AKEYCODE_BUTTON_4
	core::InputKeyCode::F5, // AKEYCODE_BUTTON_5
	core::InputKeyCode::F6, // AKEYCODE_BUTTON_6
	core::InputKeyCode::F7, // AKEYCODE_BUTTON_7
	core::InputKeyCode::F8, // AKEYCODE_BUTTON_8
	core::InputKeyCode::F9, // AKEYCODE_BUTTON_9
	core::InputKeyCode::F10, // AKEYCODE_BUTTON_10
	core::InputKeyCode::F11, // AKEYCODE_BUTTON_11
	core::InputKeyCode::F12, // AKEYCODE_BUTTON_12
	core::InputKeyCode::F13, // AKEYCODE_BUTTON_13
	core::InputKeyCode::F14, // AKEYCODE_BUTTON_14
	core::InputKeyCode::F15, // AKEYCODE_BUTTON_15
	core::InputKeyCode::F16, // AKEYCODE_BUTTON_16
	core::InputKeyCode::Unknown, // AKEYCODE_LANGUAGE_SWITCH
	core::InputKeyCode::Unknown, // AKEYCODE_MANNER_MODE
	core::InputKeyCode::Unknown, // AKEYCODE_3D_MODE
	core::InputKeyCode::Unknown, // AKEYCODE_CONTACTS
	core::InputKeyCode::Unknown, // AKEYCODE_CALENDAR
	core::InputKeyCode::Unknown, // AKEYCODE_MUSIC
	core::InputKeyCode::Unknown, // AKEYCODE_CALCULATOR
	core::InputKeyCode::Unknown, // AKEYCODE_ZENKAKU_HANKAKU
	core::InputKeyCode::Unknown, // AKEYCODE_EISU
	core::InputKeyCode::Unknown, // AKEYCODE_MUHENKAN
	core::InputKeyCode::Unknown, // AKEYCODE_HENKAN
	core::InputKeyCode::Unknown, // AKEYCODE_KATAKANA_HIRAGANA
	core::InputKeyCode::Unknown, // AKEYCODE_YEN
	core::InputKeyCode::Unknown, // AKEYCODE_RO
	core::InputKeyCode::Unknown, // AKEYCODE_KANA
	core::InputKeyCode::Unknown, // AKEYCODE_ASSIST
	core::InputKeyCode::Unknown, // AKEYCODE_BRIGHTNESS_DOWN
	core::InputKeyCode::Unknown, // AKEYCODE_BRIGHTNESS_UP
	core::InputKeyCode::Unknown, // AKEYCODE_MEDIA_AUDIO_TRACK
	core::InputKeyCode::Unknown, // AKEYCODE_SLEEP
	core::InputKeyCode::Unknown, // AKEYCODE_WAKEUP
	core::InputKeyCode::Unknown, // AKEYCODE_PAIRING
	core::InputKeyCode::Unknown, // AKEYCODE_MEDIA_TOP_MENU
	core::InputKeyCode::Unknown, // AKEYCODE_11
	core::InputKeyCode::Unknown, // AKEYCODE_12
	core::InputKeyCode::Unknown, // AKEYCODE_LAST_CHANNEL
	core::InputKeyCode::Unknown, // AKEYCODE_TV_DATA_SERVICE
	core::InputKeyCode::Unknown, // AKEYCODE_VOICE_ASSIST
	core::InputKeyCode::Unknown, // AKEYCODE_TV_RADIO_SERVICE
	core::InputKeyCode::Unknown, // AKEYCODE_TV_TELETEXT
	core::InputKeyCode::Unknown, // AKEYCODE_TV_NUMBER_ENTRY
	core::InputKeyCode::Unknown, // AKEYCODE_TV_TERRESTRIAL_ANALOG
	core::InputKeyCode::Unknown, // AKEYCODE_TV_TERRESTRIAL_DIGITAL
	core::InputKeyCode::Unknown, // AKEYCODE_TV_SATELLITE
	core::InputKeyCode::Unknown, // AKEYCODE_TV_SATELLITE_BS
	core::InputKeyCode::Unknown, // AKEYCODE_TV_SATELLITE_CS
	core::InputKeyCode::Unknown, // AKEYCODE_TV_SATELLITE_SERVICE
	core::InputKeyCode::Unknown, // AKEYCODE_TV_NETWORK
	core::InputKeyCode::Unknown, // AKEYCODE_TV_ANTENNA_CABLE
	core::InputKeyCode::Unknown, // AKEYCODE_TV_INPUT_HDMI_1
	core::InputKeyCode::Unknown, // AKEYCODE_TV_INPUT_HDMI_2
	core::InputKeyCode::Unknown, // AKEYCODE_TV_INPUT_HDMI_3
	core::InputKeyCode::Unknown, // AKEYCODE_TV_INPUT_HDMI_4
	core::InputKeyCode::Unknown, // AKEYCODE_TV_INPUT_COMPOSITE_1
	core::InputKeyCode::Unknown, // AKEYCODE_TV_INPUT_COMPOSITE_2
	core::InputKeyCode::Unknown, // AKEYCODE_TV_INPUT_COMPONENT_1
	core::InputKeyCode::Unknown, // AKEYCODE_TV_INPUT_COMPONENT_2
	core::InputKeyCode::Unknown, // AKEYCODE_TV_INPUT_VGA_1
	core::InputKeyCode::Unknown, // AKEYCODE_TV_AUDIO_DESCRIPTION
	core::InputKeyCode::Unknown, // AKEYCODE_TV_AUDIO_DESCRIPTION_MIX_UP
	core::InputKeyCode::Unknown, // AKEYCODE_TV_AUDIO_DESCRIPTION_MIX_DOWN
	core::InputKeyCode::Unknown, // AKEYCODE_TV_ZOOM_MODE
	core::InputKeyCode::Unknown, // AKEYCODE_TV_CONTENTS_MENU
	core::InputKeyCode::Unknown, // AKEYCODE_TV_MEDIA_CONTEXT_MENU
	core::InputKeyCode::Unknown, // AKEYCODE_TV_TIMER_PROGRAMMING
	core::InputKeyCode::F1, // AKEYCODE_HELP
	core::InputKeyCode::Unknown, // AKEYCODE_NAVIGATE_PREVIOUS
	core::InputKeyCode::Unknown, // AKEYCODE_NAVIGATE_NEXT
	core::InputKeyCode::Unknown, // AKEYCODE_NAVIGATE_IN
	core::InputKeyCode::Unknown, // AKEYCODE_NAVIGATE_OUT
	core::InputKeyCode::Unknown, // AKEYCODE_STEM_PRIMARY
	core::InputKeyCode::Unknown, // AKEYCODE_STEM_1
	core::InputKeyCode::Unknown, // AKEYCODE_STEM_2
	core::InputKeyCode::Unknown, // AKEYCODE_STEM_3
	core::InputKeyCode::Unknown, // AKEYCODE_DPAD_UP_LEFT
	core::InputKeyCode::Unknown, // AKEYCODE_DPAD_DOWN_LEFT
	core::InputKeyCode::Unknown, // AKEYCODE_DPAD_UP_RIGHT
	core::InputKeyCode::Unknown, // AKEYCODE_DPAD_DOWN_RIGHT
	core::InputKeyCode::Unknown, // AKEYCODE_MEDIA_SKIP_FORWARD
	core::InputKeyCode::Unknown, // AKEYCODE_MEDIA_SKIP_BACKWARD
	core::InputKeyCode::Unknown, // AKEYCODE_MEDIA_STEP_FORWARD
	core::InputKeyCode::Unknown, // AKEYCODE_MEDIA_STEP_BACKWARD
	core::InputKeyCode::Unknown, // AKEYCODE_SOFT_SLEEP
	core::InputKeyCode::Unknown, // AKEYCODE_CUT
	core::InputKeyCode::Unknown, // AKEYCODE_COPY
	core::InputKeyCode::Unknown, // AKEYCODE_PASTE
	core::InputKeyCode::Unknown, // AKEYCODE_SYSTEM_NAVIGATION_UP
	core::InputKeyCode::Unknown, // AKEYCODE_SYSTEM_NAVIGATION_DOWN
	core::InputKeyCode::Unknown, // AKEYCODE_SYSTEM_NAVIGATION_LEFT
	core::InputKeyCode::Unknown, // AKEYCODE_SYSTEM_NAVIGATION_RIGHT
	core::InputKeyCode::Unknown, // AKEYCODE_ALL_APPS
	core::InputKeyCode::Unknown, // AKEYCODE_REFRESH
	core::InputKeyCode::Unknown, // AKEYCODE_THUMBS_UP
	core::InputKeyCode::Unknown, // AKEYCODE_THUMBS_DOWN
	core::InputKeyCode::Unknown, // AKEYCODE_PROFILE_SWITCH
	core::InputKeyCode::Unknown,
	core::InputKeyCode::Unknown,
	core::InputKeyCode::Unknown,
	core::InputKeyCode::Unknown,
	core::InputKeyCode::Unknown,
	core::InputKeyCode::Unknown,
	core::InputKeyCode::Unknown,
	core::InputKeyCode::Unknown,
	core::InputKeyCode::Unknown,
	core::InputKeyCode::Unknown,
	core::InputKeyCode::Unknown,
	core::InputKeyCode::Unknown,
	core::InputKeyCode::Unknown,
};

static core::InputModifier InputQueue_getInputModifiers(int32_t modsFlags) {
	core::InputModifier mods = core::InputModifier::None;
	if (modsFlags != AMETA_NONE) {
		if ((modsFlags & AMETA_ALT_ON) != AMETA_NONE) {
			mods |= core::InputModifier::Alt;
		}
		if ((modsFlags & AMETA_ALT_LEFT_ON) != AMETA_NONE) {
			mods |= core::InputModifier::AltL;
		}
		if ((modsFlags & AMETA_ALT_RIGHT_ON) != AMETA_NONE) {
			mods |= core::InputModifier::AltR;
		}
		if ((modsFlags & AMETA_SHIFT_ON) != AMETA_NONE) {
			mods |= core::InputModifier::Shift;
		}
		if ((modsFlags & AMETA_SHIFT_LEFT_ON) != AMETA_NONE) {
			mods |= core::InputModifier::ShiftL;
		}
		if ((modsFlags & AMETA_SHIFT_RIGHT_ON) != AMETA_NONE) {
			mods |= core::InputModifier::ShiftR;
		}
		if ((modsFlags & AMETA_CTRL_ON) != AMETA_NONE) {
			mods |= core::InputModifier::Ctrl;
		}
		if ((modsFlags & AMETA_CTRL_LEFT_ON) != AMETA_NONE) {
			mods |= core::InputModifier::CtrlL;
		}
		if ((modsFlags & AMETA_CTRL_RIGHT_ON) != AMETA_NONE) {
			mods |= core::InputModifier::CtrlR;
		}
		if ((modsFlags & AMETA_META_ON) != AMETA_NONE) {
			mods |= core::InputModifier::Mod3;
		}
		if ((modsFlags & AMETA_META_LEFT_ON) != AMETA_NONE) {
			mods |= core::InputModifier::Mod3L;
		}
		if ((modsFlags & AMETA_META_RIGHT_ON) != AMETA_NONE) {
			mods |= core::InputModifier::Mod3R;
		}

		if ((modsFlags & AMETA_CAPS_LOCK_ON) != AMETA_NONE) {
			mods |= core::InputModifier::CapsLock;
		}
		if ((modsFlags & AMETA_NUM_LOCK_ON) != AMETA_NONE) {
			mods |= core::InputModifier::NumLock;
		}

		if ((modsFlags & AMETA_SCROLL_LOCK_ON) != AMETA_NONE) {
			mods |= core::InputModifier::ScrollLock;
		}
		if ((modsFlags & AMETA_SYM_ON) != AMETA_NONE) {
			mods |= core::InputModifier::Sym;
		}
		if ((modsFlags & AMETA_FUNCTION_ON) != AMETA_NONE) {
			mods |= core::InputModifier::Function;
		}
	}
	return mods;
}

static core::InputMouseButton InputQueue_getInputButton(int32_t button) {
	switch (button) {
	case AMOTION_EVENT_BUTTON_PRIMARY: return core::InputMouseButton::MouseLeft; break;
	case AMOTION_EVENT_BUTTON_SECONDARY: return core::InputMouseButton::MouseRight; break;
	case AMOTION_EVENT_BUTTON_TERTIARY: return core::InputMouseButton::MouseMiddle; break;
	case AMOTION_EVENT_BUTTON_BACK: return core::InputMouseButton::Mouse8; break;
	case AMOTION_EVENT_BUTTON_FORWARD: return core::InputMouseButton::Mouse9; break;
	case AMOTION_EVENT_BUTTON_STYLUS_PRIMARY: return core::InputMouseButton::Stilus1; break;
	case AMOTION_EVENT_BUTTON_STYLUS_SECONDARY: return core::InputMouseButton::Stilus2; break;
	}
	return core::InputMouseButton::None;
}

InputQueue::~InputQueue() {
	if (_queue) {
		AInputQueue_detachLooper(_queue);
		_queue = nullptr;
	}
}

bool InputQueue::init(AndroidActivity *a, AInputQueue *queue) {
	_activity = a;
	_queue = queue;

	AInputQueue_attachLooper(queue, ALooper_forThread(), 0, [](int fd, int events, void *data) {
		auto d = reinterpret_cast<InputQueue *>(data);
		return d->handleInputEventQueue(fd, events);
	}, this);

	_selfHandle = Dso(StringView(), DsoFlags::Self);
	if (_selfHandle) {
		_AMotionEvent_getActionButton = _selfHandle.sym<decltype(_AMotionEvent_getActionButton)>(
				"AMotionEvent_getActionButton");
	}

	return true;
}

int InputQueue::handleInputEventQueue(int fd, int events) {
	_activity->getContext()->performTemporary([&] {
		AInputEvent *event = NULL;
		while (AInputQueue_getEvent(_queue, &event) >= 0) {
			if (AInputQueue_preDispatchEvent(_queue, event)) {
				continue;
			}
			int32_t handled = handleInputEvent(event);
			AInputQueue_finishEvent(_queue, event, handled);
		}
	});
	return 1;
}

int InputQueue::handleInputEvent(AInputEvent *event) {
	auto type = AInputEvent_getType(event);
	// auto source = AInputEvent_getSource(event);
	switch (type) {
	case AINPUT_EVENT_TYPE_KEY: return handleKeyEvent(event); break;
	case AINPUT_EVENT_TYPE_MOTION: return handleMotionEvent(event); break;
	}
	return 0;
}

int InputQueue::handleKeyEvent(AInputEvent *event) {
	auto action = AKeyEvent_getAction(event);
	auto flags = AKeyEvent_getFlags(event);

	auto keyCode = AKeyEvent_getKeyCode(event);

	if (keyCode == AKEYCODE_BACK && !_backButtonHandlerEnabled) {
		return 0;
	}

	// auto scanCode = AKeyEvent_getScanCode(event);

	_activeModifiers = InputQueue_getInputModifiers(AKeyEvent_getMetaState(event));

	Vector<core::InputEventData> events;

	bool isCanceled = (flags & AKEY_EVENT_FLAG_CANCELED) != 0
			| (flags & AKEY_EVENT_FLAG_CANCELED_LONG_PRESS) != 0;

	switch (action) {
	case AKEY_EVENT_ACTION_DOWN: {
		auto &ev = events.emplace_back(core::InputEventData{
			uint32_t(keyCode),
			core::InputEventName::KeyPressed,
			{{
				core::InputMouseButton::Touch,
				_activeModifiers,
				_hoverLocation.x,
				_hoverLocation.y,
			}},
		});
		ev.key.keycode = KeyCodes[keyCode];
		ev.key.compose = core::InputKeyComposeState::Nothing;
		ev.key.keysym = keyCode;
		ev.key.keychar = 0;
		break;
	}
	case AKEY_EVENT_ACTION_UP: {
		auto &ev = events.emplace_back(core::InputEventData{
			uint32_t(keyCode),
			isCanceled ? core::InputEventName::KeyCanceled : core::InputEventName::KeyReleased,
			{{
				core::InputMouseButton::Touch,
				_activeModifiers,
				_hoverLocation.x,
				_hoverLocation.y,
			}},
		});
		ev.key.keycode = KeyCodes[keyCode];
		ev.key.compose = core::InputKeyComposeState::Nothing;
		ev.key.keysym = keyCode;
		ev.key.keychar = 0;
		break;
	}
	case AKEY_EVENT_ACTION_MULTIPLE: {
		auto &ev = events.emplace_back(core::InputEventData{
			uint32_t(keyCode),
			core::InputEventName::KeyRepeated,
			{{
				core::InputMouseButton::Touch,
				_activeModifiers,
				_hoverLocation.x,
				_hoverLocation.y,
			}},
		});
		ev.key.keycode = KeyCodes[keyCode];
		ev.key.compose = core::InputKeyComposeState::Nothing;
		ev.key.keysym = keyCode;
		ev.key.keychar = 0;
		break;
	}
	}
	if (!events.empty()) {
		_activity->notifyWindowInputEvents(sp::move(events));
		return 1;
	}
	return 0;
}

int InputQueue::handleMotionEvent(AInputEvent *event) {
	Vector<core::InputEventData> events;
	auto action = AMotionEvent_getAction(event);
	auto count = AMotionEvent_getPointerCount(event);
	auto btn = _AMotionEvent_getActionButton
			? InputQueue_getInputButton(_AMotionEvent_getActionButton(event))
			: core::InputMouseButton::MouseLeft;
	switch (action & AMOTION_EVENT_ACTION_MASK) {
	case AMOTION_EVENT_ACTION_DOWN:
		XL_ANDROID_LOG("Motion AMOTION_EVENT_ACTION_DOWN ", count, " ",
				AMotionEvent_getPointerId(event, 0), " ", 0);
		for (size_t i = 0; i < count; ++i) {
			auto &ev = events.emplace_back(core::InputEventData{
				uint32_t(AMotionEvent_getPointerId(event, i)),
				core::InputEventName::Begin,
				{{
					core::InputMouseButton::Touch,
					_activeModifiers,
					AMotionEvent_getX(event, i),
					AMotionEvent_getY(event, i),
				}},
			});
			ev.point.density = 1.0f;
		}
		break;
	case AMOTION_EVENT_ACTION_UP:
		XL_ANDROID_LOG("Motion AMOTION_EVENT_ACTION_UP ", count, " ",
				AMotionEvent_getPointerId(event, 0), " ", 0);
		for (size_t i = 0; i < count; ++i) {
			auto &ev = events.emplace_back(core::InputEventData{
				uint32_t(AMotionEvent_getPointerId(event, i)),
				core::InputEventName::End,
				{{
					core::InputMouseButton::Touch,
					_activeModifiers,
					AMotionEvent_getX(event, i),
					AMotionEvent_getY(event, i),
				}},
			});
			ev.point.density = 1.0f;
		}
		break;
	case AMOTION_EVENT_ACTION_MOVE:
		for (size_t i = 0; i < count; ++i) {
			if (AMotionEvent_getHistorySize(event) == 0
					|| AMotionEvent_getX(event, i)
									- AMotionEvent_getHistoricalX(event, i,
											AMotionEvent_getHistorySize(event) - 1)
							!= 0.0f
					|| AMotionEvent_getY(event, i)
									- AMotionEvent_getHistoricalY(event, i,
											AMotionEvent_getHistorySize(event) - 1)
							!= 0.0f) {
				auto &ev = events.emplace_back(core::InputEventData{
					uint32_t(AMotionEvent_getPointerId(event, i)),
					core::InputEventName::Move,
					{{
						core::InputMouseButton::Touch,
						_activeModifiers,
						AMotionEvent_getX(event, i),
						AMotionEvent_getY(event, i),
					}},
				});
				ev.point.density = 1.0f;
			}
		}
		break;
	case AMOTION_EVENT_ACTION_CANCEL:
		for (size_t i = 0; i < count; ++i) {
			auto &ev = events.emplace_back(core::InputEventData{
				uint32_t(AMotionEvent_getPointerId(event, i)),
				core::InputEventName::Cancel,
				{{
					core::InputMouseButton::Touch,
					_activeModifiers,
					AMotionEvent_getX(event, i),
					AMotionEvent_getY(event, i),
				}},
			});
			ev.point.density = 1.0f;
		}
		break;
	case AMOTION_EVENT_ACTION_OUTSIDE:
		XL_ANDROID_LOG("Motion AMOTION_EVENT_ACTION_OUTSIDE ", count, " ",
				AMotionEvent_getPointerId(event, 0));
		break;
	case AMOTION_EVENT_ACTION_POINTER_DOWN: {
		auto pointer = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> 8;
		XL_ANDROID_LOG("Motion AMOTION_EVENT_ACTION_POINTER_DOWN ", count, " ",
				AMotionEvent_getPointerId(event, pointer), " ", pointer);
		auto &ev = events.emplace_back(core::InputEventData{
			uint32_t(AMotionEvent_getPointerId(event, pointer)),
			core::InputEventName::Begin,
			{{
				core::InputMouseButton::Touch,
				_activeModifiers,
				AMotionEvent_getX(event, pointer),
				AMotionEvent_getY(event, pointer),
			}},
		});
		ev.point.density = 1.0f;
		break;
	}
	case AMOTION_EVENT_ACTION_POINTER_UP: {
		auto pointer = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> 8;
		XL_ANDROID_LOG("Motion AMOTION_EVENT_ACTION_POINTER_UP ", count, " ",
				AMotionEvent_getPointerId(event, pointer), " ", pointer);
		auto &ev = events.emplace_back(core::InputEventData{
			uint32_t(AMotionEvent_getPointerId(event, pointer)),
			core::InputEventName::End,
			{{
				core::InputMouseButton::Touch,
				_activeModifiers,
				AMotionEvent_getX(event, pointer),
				AMotionEvent_getY(event, pointer),
			}},
		});
		ev.point.density = 1.0f;
		break;
	}
	case AMOTION_EVENT_ACTION_HOVER_MOVE:
		for (size_t i = 0; i < count; ++i) {
			auto &ev = events.emplace_back(core::InputEventData{
				uint32_t(AMotionEvent_getPointerId(event, i)),
				core::InputEventName::MouseMove,
				{{
					core::InputMouseButton::Touch,
					_activeModifiers,
					AMotionEvent_getX(event, i),
					AMotionEvent_getY(event, i),
				}},
			});
			ev.point.density = 1.0f;
			_hoverLocation = Vec2(AMotionEvent_getX(event, i), AMotionEvent_getY(event, i));
		}
		break;
	case AMOTION_EVENT_ACTION_SCROLL:
		for (size_t i = 0; i < count; ++i) {
			events.emplace_back(core::InputEventData{
				uint32_t(AMotionEvent_getPointerId(event, i)),
				core::InputEventName::Scroll,
				{{
					core::InputMouseButton::None,
					_activeModifiers,
					AMotionEvent_getX(event, i),
					AMotionEvent_getY(event, i),
				}},
				{{
					AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HSCROLL, i),
					AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_VSCROLL, i),
					1.0f,
				}},
			});
		}
		XL_ANDROID_LOG("Motion AMOTION_EVENT_ACTION_SCROLL ", count, " ",
				AMotionEvent_getPointerId(event, 0));
		break;
	case AMOTION_EVENT_ACTION_HOVER_ENTER:
		_activity->notifyEnableState(WindowState::Pointer);
		XL_ANDROID_LOG("Motion AMOTION_EVENT_ACTION_HOVER_ENTER ", count, " ",
				AMotionEvent_getPointerId(event, 0));
		break;
	case AMOTION_EVENT_ACTION_HOVER_EXIT:
		_activity->notifyDisableState(WindowState::Pointer);
		XL_ANDROID_LOG("Motion AMOTION_EVENT_ACTION_HOVER_EXIT ", count, " ",
				AMotionEvent_getPointerId(event, 0));
		break;
	case AMOTION_EVENT_ACTION_BUTTON_PRESS:
		for (size_t i = 0; i < count; ++i) {
			events.emplace_back(core::InputEventData{
				uint32_t(AMotionEvent_getPointerId(event, i)),
				core::InputEventName::Begin,
				{{
					btn,
					_activeModifiers,
					AMotionEvent_getX(event, i),
					AMotionEvent_getY(event, i),
				}},
				{{
					0.0f,
					0.0f,
					1.0f,
				}},
			});
		}
		XL_ANDROID_LOG("Motion AMOTION_EVENT_ACTION_BUTTON_PRESS ", count, " ",
				AMotionEvent_getPointerId(event, 0));
		break;
	case AMOTION_EVENT_ACTION_BUTTON_RELEASE:
		for (size_t i = 0; i < count; ++i) {
			events.emplace_back(core::InputEventData{
				uint32_t(AMotionEvent_getPointerId(event, i)),
				core::InputEventName::End,
				{{
					btn,
					_activeModifiers,
					AMotionEvent_getX(event, i),
					AMotionEvent_getY(event, i),
				}},
				{{
					0.0f,
					0.0f,
					1.0f,
				}},
			});
		}
		XL_ANDROID_LOG("Motion AMOTION_EVENT_ACTION_BUTTON_RELEASE ", count, " ",
				AMotionEvent_getPointerId(event, 0));
		break;
	}
	if (!events.empty()) {
		_activity->notifyWindowInputEvents(sp::move(events));
		return 1;
	}
	return 0;
}

void InputQueue::setBackButtonHandlerEnabled(bool value) { _backButtonHandlerEnabled = value; }

void InputQueue::handleBackInvoked() {
	Vector<core::InputEventData> events;
	auto keyCode = AKEYCODE_BACK;

	do {
		auto &ev = events.emplace_back(core::InputEventData{
			uint32_t(keyCode),
			core::InputEventName::KeyPressed,
			{{
				core::InputMouseButton::Touch,
				_activeModifiers,
				_hoverLocation.x,
				_hoverLocation.y,
			}},
		});
		ev.key.keycode = KeyCodes[keyCode];
		ev.key.compose = core::InputKeyComposeState::Nothing;
		ev.key.keysym = keyCode;
		ev.key.keychar = 0;
	} while (0);

	do {
		auto &ev = events.emplace_back(core::InputEventData{
			uint32_t(keyCode),
			core::InputEventName::KeyReleased,
			{{
				core::InputMouseButton::Touch,
				_activeModifiers,
				_hoverLocation.x,
				_hoverLocation.y,
			}},
		});
		ev.key.keycode = KeyCodes[keyCode];
		ev.key.compose = core::InputKeyComposeState::Nothing;
		ev.key.keysym = keyCode;
		ev.key.keychar = 0;
	} while (0);

	_activity->notifyWindowInputEvents(sp::move(events));
}

} // namespace stappler::xenolith::platform
