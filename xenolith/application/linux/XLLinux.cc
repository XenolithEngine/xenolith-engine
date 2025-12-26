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

#include "XLLinux.h"
#include "SPCore.h"
#include "SPSpanView.h"
#include "XLContextInfo.h"

#include <X11/keysym.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

void _xl_null_fn() { }

static constexpr StringView s_cursorsDefault[] = {
	StringView("left_ptr"),
	StringView("top_left_arrow."),
	StringView("arrow"),
	StringView("default"),
};

static constexpr StringView s_cursorsCell[] = {
	StringView("cell"),
};

static constexpr StringView s_cursorsCrosshair[] = {
	StringView("crosshair"),
};

static constexpr StringView s_cursorsContextMenu[] = {
	StringView("context-menu"),
};

static constexpr StringView s_cursorsRightArrow[] = {
	StringView("right_ptr"),
};

static constexpr StringView s_cursorsText[] = {
	StringView("text"),
	StringView("xterm"),
};

static constexpr StringView s_cursorsVerticalText[] = {
	StringView("vertical-text"),
	StringView("xterm"),
};

static constexpr StringView s_cursorsPointer[] = {
	StringView("hand2"),
	StringView("hand"),
	StringView("pointer"),
};

static constexpr StringView s_cursorsGrab[] = {
	StringView("grab"),
	StringView("openhand"),
	StringView("hand1"),
};

static constexpr StringView s_cursorsGrabbed[] = {
	StringView("closedhand"),
	StringView("grabbing"),
	StringView("size_all"),
};

static constexpr StringView s_cursorsAllScroll[] = {
	StringView("all-scroll"),
};

static constexpr StringView s_cursorsZoomIn[] = {
	StringView("zoom-in"),
};

static constexpr StringView s_cursorsZoomOut[] = {
	StringView("zoom-out"),
};

static constexpr StringView s_cursorsTarget[] = {
	StringView("target"),
	StringView("icon"),
	StringView("draped_box"),
	StringView("dotbox"),
};

static constexpr StringView s_cursorsPencil[] = {
	StringView("pencil"),
	StringView("draft"),
};

static constexpr StringView s_cursorsHelp[] = {
	StringView("help"),
	StringView("question_arrow"),
	StringView("whats_this"),
};

static constexpr StringView s_cursorsProgress[] = {
	StringView("progress"),
	StringView("left_ptr_watch"),
	StringView("half-busy"),
};

static constexpr StringView s_cursorsWait[] = {
	StringView("wait"),
	StringView("watch"),
};

static constexpr StringView s_cursorsCopy[] = {
	StringView("copy"),
};

static constexpr StringView s_cursorsAlias[] = {
	StringView("alias"),
	StringView("dnd-link"),
};

static constexpr StringView s_cursorsNoDrop[] = {
	StringView("no-drop"),
	StringView("forbidden"),
};

static constexpr StringView s_cursorsNotAllowed[] = {
	StringView("not-allowed"),
	StringView("crossed_circle"),
};

static constexpr StringView s_cursorsMove[] = {
	StringView("move"),
};

static constexpr StringView s_cursorsResizeAll[] = {
	StringView("all-scroll"),
};

static constexpr StringView s_cursorsResizeTopLeft[] = {
	StringView("nw-resize"),
	StringView("top_left_corner"),
};

static constexpr StringView s_cursorsResizeTopRight[] = {
	StringView("ne-resize"),
	StringView("top_right_corner"),
};

static constexpr StringView s_cursorsResizeTop[] = {
	StringView("n-resize"),
	StringView("top_side"),
};

static constexpr StringView s_cursorsResizeLeft[] = {
	StringView("w-resize"),
	StringView("left_side"),
};

static constexpr StringView s_cursorsResizeRight[] = {
	StringView("e-resize"),
	StringView("right_side"),
};

static constexpr StringView s_cursorsResizeBottomLeft[] = {
	StringView("sw-resize"),
	StringView("bottom_left_corner"),
};

static constexpr StringView s_cursorsResizeBottomRight[] = {
	StringView("se-resize"),
	StringView("bottom_right_corner"),
};

static constexpr StringView s_cursorsResizeBottom[] = {
	StringView("s-resize"),
	StringView("bottom_side"),
};

static constexpr StringView s_cursorsResizeTopBottom[] = {
	StringView("ns-resize"),
	StringView("row-resize"),
	StringView("v_double_arrow"),
	StringView("sb_v_double_arrow"),
	StringView("split_v"),
	StringView("size-ver"),
	StringView("size_ver"),
	StringView("double_arrow"),
};

static constexpr StringView s_cursorsResizeLeftRight[] = {
	StringView("ew-resize"),
	StringView("col-resize"),
	StringView("h_double_arrow"),
	StringView("sb_h_double_arrow"),
	StringView("split_h"),
};

static constexpr StringView s_cursorssResizeTopRightBottomLeft[] = {
	StringView("nesw-resize"),
	StringView("fd_double_arrow"),
	StringView("size-bdiag"),
	StringView("size_bdiag"),
};

static constexpr StringView s_cursorssResizeTopLeftBottomRight[] = {
	StringView("nwse-resize"),
	StringView("bd_double_arrow"),
	StringView("size-fdiag"),
	StringView("size_fdiag"),
};

SpanView<StringView> getCursorNames(WindowCursor cursor) {
	switch (cursor) {
	case WindowCursor::Undefined: break;
	case WindowCursor::Default: return makeSpanView(s_cursorsDefault); break;
	case WindowCursor::ContextMenu: return makeSpanView(s_cursorsContextMenu); break;
	case WindowCursor::Help: return makeSpanView(s_cursorsHelp); break;
	case WindowCursor::Pointer: return makeSpanView(s_cursorsPointer); break;
	case WindowCursor::Progress: return makeSpanView(s_cursorsProgress); break;
	case WindowCursor::Wait: return makeSpanView(s_cursorsWait); break;
	case WindowCursor::Cell: return makeSpanView(s_cursorsCell); break;
	case WindowCursor::Crosshair: return makeSpanView(s_cursorsCrosshair); break;
	case WindowCursor::Text: return makeSpanView(s_cursorsText); break;
	case WindowCursor::VerticalText: return makeSpanView(s_cursorsVerticalText); break;
	case WindowCursor::Alias: return makeSpanView(s_cursorsAlias); break;
	case WindowCursor::Copy: return makeSpanView(s_cursorsCopy); break;
	case WindowCursor::Move: return makeSpanView(s_cursorsMove); break;
	case WindowCursor::NoDrop: return makeSpanView(s_cursorsNoDrop); break;
	case WindowCursor::NotAllowed: return makeSpanView(s_cursorsNotAllowed); break;
	case WindowCursor::Grab: return makeSpanView(s_cursorsGrab); break;
	case WindowCursor::Grabbing: return makeSpanView(s_cursorsGrabbed); break;
	case WindowCursor::AllScroll: return makeSpanView(s_cursorsAllScroll); break;
	case WindowCursor::ZoomIn: return makeSpanView(s_cursorsZoomIn); break;
	case WindowCursor::ZoomOut: return makeSpanView(s_cursorsZoomOut); break;
	case WindowCursor::RightPtr: return makeSpanView(s_cursorsRightArrow); break;
	case WindowCursor::Pencil: return makeSpanView(s_cursorsPencil); break;
	case WindowCursor::Target: return makeSpanView(s_cursorsTarget); break;
	case WindowCursor::ResizeTop: return makeSpanView(s_cursorsResizeTop); break;
	case WindowCursor::ResizeTopRight: return makeSpanView(s_cursorsResizeTopRight); break;
	case WindowCursor::ResizeRight: return makeSpanView(s_cursorsResizeRight); break;
	case WindowCursor::ResizeBottomRight: return makeSpanView(s_cursorsResizeBottomRight); break;
	case WindowCursor::ResizeBottom: return makeSpanView(s_cursorsResizeBottom); break;
	case WindowCursor::ResizeBottomLeft: return makeSpanView(s_cursorsResizeBottomLeft); break;
	case WindowCursor::ResizeLeft: return makeSpanView(s_cursorsResizeLeft); break;
	case WindowCursor::ResizeTopLeft: return makeSpanView(s_cursorsResizeTopLeft); break;
	case WindowCursor::ResizeTopBottom: return makeSpanView(s_cursorsResizeTopBottom); break;
	case WindowCursor::ResizeLeftRight: return makeSpanView(s_cursorsResizeLeftRight); break;
	case WindowCursor::ResizeTopLeftBottomRight:
		return makeSpanView(s_cursorssResizeTopLeftBottomRight);
		break;
	case WindowCursor::ResizeTopRightBottomLeft:
		return makeSpanView(s_cursorssResizeTopRightBottomLeft);
		break;
	case WindowCursor::ResizeAll: return makeSpanView(s_cursorsResizeAll); break;
	default: break;
	}
	return SpanView<StringView>();
}

core::InputKeyCode getKeysymCode(uint32_t sym) {
	// from GLFW: x11_init.c
	switch (sym) {
	case XK_KP_0: return core::InputKeyCode::KP_0;
	case XK_KP_1: return core::InputKeyCode::KP_1;
	case XK_KP_2: return core::InputKeyCode::KP_2;
	case XK_KP_3: return core::InputKeyCode::KP_3;
	case XK_KP_4: return core::InputKeyCode::KP_4;
	case XK_KP_5: return core::InputKeyCode::KP_5;
	case XK_KP_6: return core::InputKeyCode::KP_6;
	case XK_KP_7: return core::InputKeyCode::KP_7;
	case XK_KP_8: return core::InputKeyCode::KP_8;
	case XK_KP_9: return core::InputKeyCode::KP_9;
	case XK_KP_Separator:
	case XK_KP_Decimal: return core::InputKeyCode::KP_DECIMAL;
	case XK_Escape: return core::InputKeyCode::ESCAPE;
	case XK_Tab: return core::InputKeyCode::TAB;
	case XK_Shift_L: return core::InputKeyCode::LEFT_SHIFT;
	case XK_Shift_R: return core::InputKeyCode::RIGHT_SHIFT;
	case XK_Control_L: return core::InputKeyCode::LEFT_CONTROL;
	case XK_Control_R: return core::InputKeyCode::RIGHT_CONTROL;
	case XK_Meta_L:
	case XK_Alt_L: return core::InputKeyCode::LEFT_ALT;
	case XK_Mode_switch: // Mapped to Alt_R on many keyboards
	case XK_ISO_Level3_Shift: // AltGr on at least some machines
	case XK_Meta_R:
	case XK_Alt_R: return core::InputKeyCode::RIGHT_ALT;
	case XK_Super_L: return core::InputKeyCode::LEFT_SUPER;
	case XK_Super_R: return core::InputKeyCode::RIGHT_SUPER;
	case XK_Menu: return core::InputKeyCode::MENU;
	case XK_Num_Lock: return core::InputKeyCode::NUM_LOCK;
	case XK_Caps_Lock: return core::InputKeyCode::CAPS_LOCK;
	case XK_Print: return core::InputKeyCode::PRINT_SCREEN;
	case XK_Scroll_Lock: return core::InputKeyCode::SCROLL_LOCK;
	case XK_Pause: return core::InputKeyCode::PAUSE;
	case XK_Delete: return core::InputKeyCode::DELETE;
	case XK_BackSpace: return core::InputKeyCode::BACKSPACE;
	case XK_Return: return core::InputKeyCode::ENTER;
	case XK_Home: return core::InputKeyCode::HOME;
	case XK_End: return core::InputKeyCode::END;
	case XK_Page_Up: return core::InputKeyCode::PAGE_UP;
	case XK_Page_Down: return core::InputKeyCode::PAGE_DOWN;
	case XK_Insert: return core::InputKeyCode::INSERT;
	case XK_Left: return core::InputKeyCode::LEFT;
	case XK_Right: return core::InputKeyCode::RIGHT;
	case XK_Down: return core::InputKeyCode::DOWN;
	case XK_Up: return core::InputKeyCode::UP;
	case XK_F1: return core::InputKeyCode::F1;
	case XK_F2: return core::InputKeyCode::F2;
	case XK_F3: return core::InputKeyCode::F3;
	case XK_F4: return core::InputKeyCode::F4;
	case XK_F5: return core::InputKeyCode::F5;
	case XK_F6: return core::InputKeyCode::F6;
	case XK_F7: return core::InputKeyCode::F7;
	case XK_F8: return core::InputKeyCode::F8;
	case XK_F9: return core::InputKeyCode::F9;
	case XK_F10: return core::InputKeyCode::F10;
	case XK_F11: return core::InputKeyCode::F11;
	case XK_F12: return core::InputKeyCode::F12;
	case XK_F13: return core::InputKeyCode::F13;
	case XK_F14: return core::InputKeyCode::F14;
	case XK_F15: return core::InputKeyCode::F15;
	case XK_F16: return core::InputKeyCode::F16;
	case XK_F17: return core::InputKeyCode::F17;
	case XK_F18: return core::InputKeyCode::F18;
	case XK_F19: return core::InputKeyCode::F19;
	case XK_F20: return core::InputKeyCode::F20;
	case XK_F21: return core::InputKeyCode::F21;
	case XK_F22: return core::InputKeyCode::F22;
	case XK_F23: return core::InputKeyCode::F23;
	case XK_F24: return core::InputKeyCode::F24;
	case XK_F25:
		return core::InputKeyCode::F25;

		// Numeric keypad
	case XK_KP_Divide: return core::InputKeyCode::KP_DIVIDE;
	case XK_KP_Multiply: return core::InputKeyCode::KP_MULTIPLY;
	case XK_KP_Subtract: return core::InputKeyCode::KP_SUBTRACT;
	case XK_KP_Add:
		return core::InputKeyCode::KP_ADD;

		// These should have been detected in secondary keysym test above!
	case XK_KP_Insert: return core::InputKeyCode::KP_0;
	case XK_KP_End: return core::InputKeyCode::KP_1;
	case XK_KP_Down: return core::InputKeyCode::KP_2;
	case XK_KP_Page_Down: return core::InputKeyCode::KP_3;
	case XK_KP_Left: return core::InputKeyCode::KP_4;
	case XK_KP_Right: return core::InputKeyCode::KP_6;
	case XK_KP_Home: return core::InputKeyCode::KP_7;
	case XK_KP_Up: return core::InputKeyCode::KP_8;
	case XK_KP_Page_Up: return core::InputKeyCode::KP_9;
	case XK_KP_Delete: return core::InputKeyCode::KP_DECIMAL;
	case XK_KP_Equal: return core::InputKeyCode::KP_EQUAL;
	case XK_KP_Enter:
		return core::InputKeyCode::KP_ENTER;

		// Last resort: Check for printable keys (should not happen if the XKB
		// extension is available). This will give a layout dependent mapping
		// (which is wrong, and we may miss some keys, especially on non-US
		// keyboards), but it's better than nothing...
	case XK_a: return core::InputKeyCode::A;
	case XK_b: return core::InputKeyCode::B;
	case XK_c: return core::InputKeyCode::C;
	case XK_d: return core::InputKeyCode::D;
	case XK_e: return core::InputKeyCode::E;
	case XK_f: return core::InputKeyCode::F;
	case XK_g: return core::InputKeyCode::G;
	case XK_h: return core::InputKeyCode::H;
	case XK_i: return core::InputKeyCode::I;
	case XK_j: return core::InputKeyCode::J;
	case XK_k: return core::InputKeyCode::K;
	case XK_l: return core::InputKeyCode::L;
	case XK_m: return core::InputKeyCode::M;
	case XK_n: return core::InputKeyCode::N;
	case XK_o: return core::InputKeyCode::O;
	case XK_p: return core::InputKeyCode::P;
	case XK_q: return core::InputKeyCode::Q;
	case XK_r: return core::InputKeyCode::R;
	case XK_s: return core::InputKeyCode::S;
	case XK_t: return core::InputKeyCode::T;
	case XK_u: return core::InputKeyCode::U;
	case XK_v: return core::InputKeyCode::V;
	case XK_w: return core::InputKeyCode::W;
	case XK_x: return core::InputKeyCode::X;
	case XK_y: return core::InputKeyCode::Y;
	case XK_z: return core::InputKeyCode::Z;
	case XK_1: return core::InputKeyCode::_1;
	case XK_2: return core::InputKeyCode::_2;
	case XK_3: return core::InputKeyCode::_3;
	case XK_4: return core::InputKeyCode::_4;
	case XK_5: return core::InputKeyCode::_5;
	case XK_6: return core::InputKeyCode::_6;
	case XK_7: return core::InputKeyCode::_7;
	case XK_8: return core::InputKeyCode::_8;
	case XK_9: return core::InputKeyCode::_9;
	case XK_0: return core::InputKeyCode::_0;
	case XK_space: return core::InputKeyCode::SPACE;
	case XK_minus: return core::InputKeyCode::MINUS;
	case XK_equal: return core::InputKeyCode::EQUAL;
	case XK_bracketleft: return core::InputKeyCode::LEFT_BRACKET;
	case XK_bracketright: return core::InputKeyCode::RIGHT_BRACKET;
	case XK_backslash: return core::InputKeyCode::BACKSLASH;
	case XK_semicolon: return core::InputKeyCode::SEMICOLON;
	case XK_apostrophe: return core::InputKeyCode::APOSTROPHE;
	case XK_grave: return core::InputKeyCode::GRAVE_ACCENT;
	case XK_comma: return core::InputKeyCode::COMMA;
	case XK_period: return core::InputKeyCode::PERIOD;
	case XK_slash: return core::InputKeyCode::SLASH;
	case XK_less: return core::InputKeyCode::WORLD_1; // At least in some layouts...
	default: break;
	}
	return core::InputKeyCode::Unknown;
}

} // namespace stappler::xenolith::platform
