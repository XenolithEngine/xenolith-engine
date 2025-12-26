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

#ifndef XENOLITH_APPLICATION_LINUX_WAYLAND_XLLINUXWAYLANDSEAT_H_
#define XENOLITH_APPLICATION_LINUX_WAYLAND_XLLINUXWAYLANDSEAT_H_

#include "XLContextInfo.h" // IWYU pragma: keep

#if LINUX

#include "XLLinuxWaylandLibrary.h"
#include "platform/XLContextController.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class WaylandWindow;

struct WaylandDisplay;
struct WaylandCursorTheme;
struct WaylandOutput;
struct WaylandDecoration;
struct WaylandDataDevice;

struct SP_PUBLIC WaylandSeat : Ref {
	virtual ~WaylandSeat();

	bool init(NotNull<WaylandLibrary>, NotNull<WaylandDisplay>, wl_registry *, uint32_t name,
			uint32_t version);

	void setCursor(WindowCursor, bool serverSide);

	void setCursors(StringView theme, int size);
	void tryUpdateCursor();

	void update();

	void clearWindow(WaylandWindow *);

	core::InputKeyCode translateKey(uint32_t scancode) const;
	xkb_keysym_t composeSymbol(xkb_keysym_t sym, core::InputKeyComposeState &compose) const;

	Rc<WaylandLibrary> wayland;
	WaylandDisplay *root;
	uint32_t id = 0;
	bool hasPointerFrames = false;
	bool serverSideCursor = false;
	wl_seat *seat = nullptr;
	String name;
	uint32_t capabilities = 0;
	wl_pointer *pointer = nullptr;
	wp_cursor_shape_device_v1 *cursorShape = nullptr;
	wl_keyboard *keyboard = nullptr;
	wl_touch *touch = nullptr;
	float pointerScale = 1.0f;
	wl_surface *pointerFocus = nullptr;
	uint32_t serial = 0;
	wl_surface *cursorSurface = nullptr;
	xkb_state *state = nullptr;
	xkb_compose_state *compose = nullptr;
	KeyState keyState;
	uint32_t doubleClickInterval = 500'000;

	Rc<WaylandCursorTheme> cursorTheme;
	Rc<WaylandDataDevice> dataDevice;
	WindowCursor cursorImage = WindowCursor::Undefined;

	Set<WaylandDecoration *> pointerDecorations;
	Set<WaylandOutput *> pointerOutputs;
	Set<WaylandWindow *> pointerViews;
	Set<WaylandWindow *> keyboardViews;
};

struct SP_PUBLIC WaylandCursorTheme : public Ref {
	virtual ~WaylandCursorTheme();

	bool init(WaylandDisplay *, StringView name, int size);

	void setCursor(WaylandSeat *);
	void setCursor(wl_pointer *, wl_surface *, uint32_t serial, WindowCursor img, int scale);

	bool hasCursor(WindowCursor) const;

	Rc<WaylandLibrary> wayland;
	wl_cursor_theme *cursorTheme = nullptr;
	int cursorSize = 24;
	String cursorName;
	Vector<wl_cursor *> cursors;
};

} // namespace stappler::xenolith::platform

#endif

#endif // XENOLITH_APPLICATION_LINUX_WAYLAND_XLLINUXWAYLANDSEAT_H_
