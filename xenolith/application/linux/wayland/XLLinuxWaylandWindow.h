/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_APPLICATION_LINUX_WAYLAND_XLLINUXWAYLANDWINDOW_H_
#define XENOLITH_APPLICATION_LINUX_WAYLAND_XLLINUXWAYLANDWINDOW_H_

#include "XLContextInfo.h"
#include "XLLinuxWaylandLibrary.h"
#include "XLLinuxWaylandDisplay.h"
#include "platform/XLContextNativeWindow.h"
#include <libdecor-0/libdecor.h>

#if LINUX

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class LinuxContextController;

class SP_PUBLIC WaylandWindow : public NativeWindow {
public:
	static constexpr auto DecorWidth = 20;
	static constexpr auto DecorInset = 18;
	static constexpr auto DecorOffset = 6;
	static constexpr auto IconSize = DecorInset + DecorOffset - 2;

	struct PointerEvent {
		enum Event : uint32_t {
			None,
			Enter,
			Leave,
			Motion,
			Button,
			Axis,
			AxisSource,
			AxisStop,
			AxisDiscrete
		};

		Event event;
		union {
			struct {
				wl_fixed_t x;
				wl_fixed_t y;
			} enter;
			struct {
				uint32_t time;
				wl_fixed_t x;
				wl_fixed_t y;
			} motion;
			struct {
				uint32_t serial;
				uint32_t time;
				uint32_t button;
				uint32_t state;
			} button;
			struct {
				uint32_t time;
				uint32_t axis;
				float value;
			} axis;
			struct {
				uint32_t axis_source;
			} axisSource;
			struct {
				uint32_t time;
				uint32_t axis;
			} axisStop;
			struct {
				uint32_t axis;
				int32_t discrete120;
			} axisDiscrete;
			struct {
				uint32_t axis;
				uint32_t direction;
			} axisRelativeDirection;
		};
	};

	virtual ~WaylandWindow();

	WaylandWindow();

	bool init(NotNull<WaylandDisplay>, Rc<WindowInfo> &&, NotNull<LinuxContextController>);

	virtual void mapWindow() override;
	virtual void unmapWindow() override;
	virtual bool close() override;

	virtual void handleFramePresented(NotNull<core::PresentationFrame> frame) override;

	virtual core::FrameConstraints exportConstraints() const override;

	virtual core::SurfaceInfo getSurfaceOptions(const core::Device &dev,
			NotNull<core::Surface> surface) const override;

	virtual Extent2 getExtent() const override;

	virtual Rc<core::Surface> makeSurface(NotNull<core::Instance>) override;

	virtual core::PresentationOptions getPreferredOptions() const override;

	WaylandDisplay *getDisplay() const { return _display; }
	wl_surface *getSurface() const { return _surface; }

	void handleSurfaceEnter(wl_surface *surface, wl_output *output);
	void handleSurfaceLeave(wl_surface *surface, wl_output *output);

	void handleSurfaceConfigure(xdg_surface *, uint32_t serial);
	void handleToplevelConfigure(xdg_toplevel *xdg_toplevel, int32_t width, int32_t height,
			wl_array *states);
	void handleToplevelClose(xdg_toplevel *xdg_toplevel);
	void handleToplevelBounds(xdg_toplevel *xdg_toplevel, int32_t width, int32_t height);
	void handleToplevelCapabilities(xdg_toplevel *xdg_toplevel, wl_array *capabilities);
	void handleSurfaceFrameDone(wl_callback *wl_callback, uint32_t callback_data);

	void handleDecorConfigure(libdecor_frame *, libdecor_configuration *configuration);
	void handleDecorConfigure(zxdg_toplevel_decoration_v1 *decor, uint32_t mode);
	void handleDecorClose(libdecor_frame *);
	void handleDecorCommit(libdecor_frame *);

	void handlePointerEnter(wl_fixed_t surface_x, wl_fixed_t surface_y);
	void handlePointerLeave();
	void handlePointerMotion(uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y);
	void handlePointerButton(uint32_t serial, uint32_t time, uint32_t button, uint32_t state);
	void handlePointerAxis(uint32_t time, uint32_t axis, float value);
	void handlePointerAxisSource(uint32_t axis_source);
	void handlePointerAxisStop(uint32_t time, uint32_t axis);
	void handlePointerAxisDiscrete(uint32_t axis, int32_t discrete);
	void handlePointerAxisRelativeDirection(uint32_t axis, uint32_t direction);
	void handlePointerFrame();

	void handleKeyboardEnter(Vector<uint32_t> &&, uint32_t depressed, uint32_t latched,
			uint32_t locked);
	void handleKeyboardLeave();
	void handleKey(uint32_t time, uint32_t key, uint32_t state);
	void handleKeyModifiers(uint32_t depressed, uint32_t latched, uint32_t locked);
	void handleKeyRepeat();

	void notifyScreenChange();
	void motifyThemeChanged(const ThemeInfo &);

	void handleDecorationPress(WaylandDecoration *, uint32_t serial, uint32_t btn,
			bool released = false);

	void setPreferredScale(int32_t);
	void setPreferredTransform(uint32_t);

	virtual void dispatchPendingEvents() override;

	WindowCursor getCursor() const;
	bool isServerSideCursors() const { return _serverSideCursors; }

	virtual bool enableState(WindowState) override;
	virtual bool disableState(WindowState) override;

protected:
	virtual bool updateTextInput(const TextInputRequest &,
			TextInputFlags flags = TextInputFlags::RunIfDisabled) override;

	virtual void cancelTextInput() override;

	virtual Status setFullscreenState(FullscreenInfo &&) override;

	virtual void setCursor(WindowCursor) override;

	bool configureDecorations(Extent2 extent);

	bool initWithServerDecor();
	bool initWithLibdecor();
	bool initWithAppDecor();

	void cancelPointerEvents();

	Rc<WaylandDisplay> _display;
	Rc<WaylandLibrary> _wayland;

	wl_surface *_surface = nullptr;
	libdecor_frame *_clientDecor = nullptr;
	zxdg_toplevel_decoration_v1 *_serverDecor = nullptr;
	libdecor_configuration *_decorConfiguration = nullptr;

	wl_callback *_frameCallback = nullptr;
	xdg_surface *_xdgSurface = nullptr;
	xdg_toplevel *_toplevel = nullptr;
	Extent2 _currentExtent;
	Extent2 _commitedExtent;
	Extent2 _awaitingExtent;
	Extent2 _savedExtent;

	bool _started = false;
	bool _scheduleNext = false;
	bool _shouldClose = false;
	bool _surfaceDirty = false;
	bool _pointerInit = false;
	bool _serverSideCursors = false;
	bool _mapped = false;

	Set<WaylandOutput *> _activeOutputs;

	wl_fixed_t _surfaceFX = 0;
	wl_fixed_t _surfaceFY = 0;
	double _surfaceX = 0.0;
	double _surfaceY = 0.0;
	core::InputModifier _activeModifiers = core::InputModifier::None;
	Vector<PointerEvent> _pointerEvents;

	Vector<Rc<WaylandDecoration>> _decors;
	Rc<WaylandDecoration> _iconMaximized;

	uint32_t _configureSerial = 0;

	float _density = 0.0f;
	uint64_t _frameRate = 0;

	struct KeyData {
		uint32_t scancode;
		char32_t codepoint;
		uint64_t time;
		bool repeats;
		uint64_t lastRepeat = 0;
	};

	Map<uint32_t, KeyData> _keys;
	WindowCursor _cursor = WindowCursor::Default;

	uint32_t _buttonGripSerial = 0;
	WindowLayerFlags _buttonGripFlags = WindowLayerFlags::None;
	std::bitset<64> _buttons;
};

} // namespace stappler::xenolith::platform

#endif

#endif /* XENOLITH_APPLICATION_LINUX_WAYLAND_XLLINUXWAYLANDWINDOW_H_ */
