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

#ifndef XENOLITH_APPLICATION_LINUX_WAYLAND_XLLINUXWAYLANDDISPLAY_H_
#define XENOLITH_APPLICATION_LINUX_WAYLAND_XLLINUXWAYLANDDISPLAY_H_

#include "XLContextInfo.h"

#if LINUX

#include "XLLinuxWaylandSeat.h"
#include "platform/XLContextController.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class WaylandWindow;

struct WaylandDataDeviceManager;
struct WaylandShm;
struct WaylandOutput;
struct WaylandDecoration;

class DisplayConfigManager;
class WaylandKdeDisplayConfigManager;

enum class WaylandUpdateState {
	None,
	Updating,
	Done
};

struct SP_PUBLIC WaylandDisplay : Ref {
	virtual ~WaylandDisplay();

	bool init(NotNull<WaylandLibrary>, NotNull<XkbLibrary>, StringView = StringView());

	Rc<DisplayConfigManager> makeDisplayConfigManager(
			Function<void(NotNull<DisplayConfigManager>)> &&);

	wl_surface *createSurface(WaylandWindow *);
	void destroySurface(WaylandWindow *);

	wl_surface *createDecorationSurface(WaylandDecoration *);
	void destroyDecorationSurface(wl_surface *);

	bool ownsSurface(wl_surface *) const;
	bool isDecoration(wl_surface *) const;

	bool flush();
	bool poll();

	int getFd() const;

	void updateThemeInfo(const ThemeInfo &);

	void notifyScreenChange();

	Status readFromClipboard(Rc<ClipboardRequest> &&req);
	Status probeClipboard(Rc<ClipboardProbe> &&probe);
	Status writeToClipboard(Rc<ClipboardData> &&);

	bool isCursorSupported(WindowCursor, bool serverSide) const;
	WindowCapabilities getCapabilities() const;

	void handleClipboardChanged();

	Rc<WaylandLibrary> wayland;
	wl_display *display = nullptr;

	Rc<WaylandShm> shm;
	Rc<WaylandSeat> seat;
	Rc<XkbLibrary> xkb;
	Rc<WaylandDataDeviceManager> dataDeviceManager;
	Rc<WaylandKdeDisplayConfigManager> kdeDisplayConfigManager;
	Vector<Rc<WaylandOutput>> outputs;
	wl_compositor *compositor = nullptr;
	wl_subcompositor *subcompositor = nullptr;
	wp_viewporter *viewporter = nullptr;
	xdg_wm_base *xdgWmBase = nullptr;
	wp_cursor_shape_manager_v1 *cursorManager = nullptr;
	zxdg_decoration_manager_v1 *decorationManager = nullptr;
	libdecor *decor = nullptr;
	Set<wl_surface *> surfaces;
	Set<wl_surface *> decorations;
	Set<WaylandWindow *> windows;

	bool seatDirty = false;

	Function<void(SystemNotification)> systemNotification;
};

struct SP_PUBLIC WaylandShm : Ref {
	enum Format {
		ARGB = WL_SHM_FORMAT_ARGB8888,
		xRGB = WL_SHM_FORMAT_XRGB8888
	};

	virtual ~WaylandShm();

	bool init(const Rc<WaylandLibrary> &, wl_registry *, uint32_t name, uint32_t version);

	Rc<WaylandLibrary> wayland;
	uint32_t id = 0;
	wl_shm *shm = nullptr;
	uint32_t format;
};

struct SP_PUBLIC WaylandOutput : Ref {
	struct Geometry {
		IVec2 pos;
		geom::Extent2 mm;
		int32_t subpixel;
		int32_t transform;
		String make;
		String model;

		bool operator==(const Geometry &) const = default;
		bool operator!=(const Geometry &) const = default;
	};

	struct Mode {
		Extent2 size;
		uint32_t rate;
		uint32_t flags;

		bool operator==(const Mode &) const = default;
		bool operator!=(const Mode &) const = default;
	};

	virtual ~WaylandOutput();

	bool init(const Rc<WaylandLibrary> &, wl_registry *, uint32_t name, uint32_t version);

	String description() const;

	Rc<WaylandLibrary> wayland;
	uint32_t id;
	wl_output *output;
	Geometry geometry;
	Mode currentMode;
	Mode preferredMode;
	float scale = 0.0f;

	String name;
	String desc;

	Vector<Mode> newModes;
	Vector<Mode> availableModes;
	WaylandUpdateState state = WaylandUpdateState::None;
};

struct SP_PUBLIC WaylandDecoration : Ref {
	virtual ~WaylandDecoration();

	bool init(WaylandWindow *, Rc<WaylandBuffer> &&buffer, Rc<WaylandBuffer> &&active,
			WaylandDecorationName);
	bool init(WaylandWindow *, Rc<WaylandBuffer> &&buffer, WaylandDecorationName);

	void setAltBuffers(Rc<WaylandBuffer> &&b, Rc<WaylandBuffer> &&a);
	void handlePress(WaylandSeat *seat, uint32_t serial, uint32_t button, uint32_t state);
	void handleMotion(wl_fixed_t x, wl_fixed_t y);

	void onEnter();
	void onLeave();

	void setActive(bool);
	void setVisible(bool);
	void setAlternative(bool);
	void setGeometry(int32_t x, int32_t y, int32_t width, int32_t height);

	bool commit();

	bool isTouchable() const;

	void setLightTheme();
	void setDarkTheme();

	WaylandLibrary *wayland = nullptr;
	WaylandDisplay *display = nullptr;

	WaylandWindow *root = nullptr;
	WaylandDecorationName name = WaylandDecorationName::HeaderCenter;
	WindowCursor image = WindowCursor::Default;
	wl_surface *surface = nullptr;
	wl_subsurface *subsurface = nullptr;
	wp_viewport *viewport = nullptr;
	Rc<WaylandBuffer> buffer;
	Rc<WaylandBuffer> active;

	Rc<WaylandBuffer> altBuffer;
	Rc<WaylandBuffer> altActive;

	int32_t _x = 0;
	int32_t _y = 0;
	int32_t _width = 0;
	int32_t _height = 0;
	uint64_t lastTouch = 0;
	uint32_t serial = 0;
	wl_fixed_t pointerX;
	wl_fixed_t pointerY;

	bool visible = false;
	bool isActive = false;
	bool alternative = false;
	bool dirty = false;
	bool waitForMove = false;
};

} // namespace stappler::xenolith::platform

#endif

#endif // XENOLITH_APPLICATION_LINUX_WAYLAND_XLLINUXWAYLANDDISPLAY_H_
