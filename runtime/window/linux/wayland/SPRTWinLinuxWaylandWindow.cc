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

#define __SPRT_BUILD 1

#include "SPRTWinLinuxWaylandWindow.h"
#include "SPRTWinLinuxWaylandLibrary.h"
#include "SPRTWinLinuxWaylandSeat.h"
#include "SPRTWinLinuxWaylandDisplay.h"
#include "SPRTWinLinuxWaylandSoftwareSurface.h"
#include "../SPRTWinLinuxController.h"
#include "../SPRTWinLinuxXkbLibrary.h"
#include "SPRTWinLinuxWaylandKeys.h"

#include <sprt/runtime/window/display_config.h>
#include <sprt/runtime/log.h>

#if MODULE_XENOLITH_BACKEND_VK
#include "XLVkPresentationEngine.h" // IWYU pragma: keep
#endif

namespace sprt::window {

// clang-format off

static struct wl_surface_listener s_WaylandSurfaceListener{
	.enter = [](void *data, wl_surface *surface, wl_output *output) {
		reinterpret_cast<WaylandWindow *>(data)->handleSurfaceEnter(surface, output);
	},
	.leave = [](void *data, wl_surface *surface, wl_output *output) {
		reinterpret_cast<WaylandWindow *>(data)->handleSurfaceLeave(surface, output);
	},
	.preferred_buffer_scale = [](void *data, struct wl_surface *wl_surface, int32_t factor) {
		XL_WAYLAND_LOG("setPreferredScale: ", factor);
		reinterpret_cast<WaylandWindow *>(data)->setPreferredScale(factor);
	},
	.preferred_buffer_transform = [](void *data, struct wl_surface *wl_surface, uint32_t transform) {
		XL_WAYLAND_LOG("setPreferredTransform: ", transform);
		reinterpret_cast<WaylandWindow *>(data)->setPreferredTransform(transform);
	}
};

static const wl_callback_listener s_WaylandSurfaceFrameListener{
	.done = [](void *data, wl_callback *wl_callback, uint32_t callback_data) {
		reinterpret_cast<WaylandWindow *>(data)->handleSurfaceFrameDone(wl_callback, callback_data);
	},
};

static xdg_surface_listener const s_XdgSurfaceListener{
	.configure = [](void *data, xdg_surface *xdg_surface, uint32_t serial) {
		reinterpret_cast<WaylandWindow *>(data)->handleSurfaceConfigure(xdg_surface, serial);
	},
};

static const xdg_toplevel_listener s_XdgToplevelListener{
	.configure = [](void *data, xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, wl_array *states) {
		reinterpret_cast<WaylandWindow *>(data)->handleToplevelConfigure(xdg_toplevel,
			width, height, states);
	},
	.close = [](void *data, struct xdg_toplevel *xdg_toplevel) {
		reinterpret_cast<WaylandWindow *>(data)->handleToplevelClose(xdg_toplevel);
	},
	.configure_bounds = [](void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height) {
		reinterpret_cast<WaylandWindow *>(data)->handleToplevelBounds(xdg_toplevel, width, height);
	},
	.wm_capabilities = [](void *data, struct xdg_toplevel *xdg_toplevel, struct wl_array *capabilities) {
		reinterpret_cast<WaylandWindow *>(data)->handleToplevelCapabilities(xdg_toplevel, capabilities);
	}
};

static const xdg_popup_listener s_XdgPopupListener{
	.configure = [](void *data, xdg_popup *popup, int32_t x, int32_t y, int32_t width,
						 int32_t height) {
		reinterpret_cast<WaylandWindow *>(data)->handlePopupConfigure(popup, x, y, width, height);
	},
	.popup_done = [](void *data, xdg_popup *popup) {
		reinterpret_cast<WaylandWindow *>(data)->handlePopupDone(popup);
	},
	.repositioned = [](void *, xdg_popup *, uint32_t) { },
};

static zxdg_toplevel_decoration_v1_listener s_serverDecorationListener {
	.configure = [](void *data, zxdg_toplevel_decoration_v1 *decor, uint32_t mode) {
		reinterpret_cast<WaylandWindow *>(data)->handleDecorConfigure(decor, mode);
	},
};

// clang-format on

WaylandWindow::~WaylandWindow() {
	if (_frameCallback) {
		wl_callback_destroy(_frameCallback);
		_frameCallback = nullptr;
	}

	if (_serverDecor) {
		zxdg_toplevel_decoration_v1_destroy(_serverDecor);
		_serverDecor = nullptr;
	}

	_iconMaximized = nullptr;
	_decors.clear();
	if (_toplevel) {
		xdg_toplevel_destroy(_toplevel);
		_toplevel = nullptr;
	}
	if (_popup) {
		xdg_popup_destroy(_popup);
		_popup = nullptr;
	}
	if (_xdgSurface) {
		xdg_surface_destroy(_xdgSurface);
		_xdgSurface = nullptr;
	}
	if (_surface) {
		_display->destroySurface(this);
		_surface = nullptr;
	}
	_display = nullptr;
}

WaylandWindow::WaylandWindow() {
	_controller = nullptr; //
}

bool WaylandWindow::init(NotNull<WaylandDisplay> display, Rc<WindowInfo> &&info,
		NotNull<LinuxContextController> c) {
	if (!NativeWindow::init(c, move(info), display->getCapabilities())) {
		return false;
	}

	_display = display;
	_wayland = _display->wayland;
	_controller = c;

	_currentExtent = Extent2(_info->rect.width, _info->rect.height);

	if (hasFlag(_info->capabilities, WindowCapabilities::ServerSideCursors)
			&& hasFlag(_info->flags, WindowCreationFlags::PreferServerSideCursors)) {
		_serverSideCursors = true;
	}

	_surface = _display->createSurface(this);
	if (_surface) {
		wl_surface_set_user_data(_surface, this);
		wl_surface_add_listener(_surface, &s_WaylandSurfaceListener, this);

		if (_info->type == WindowType::Popup || _info->type == WindowType::Tooltip) {
			if (!initPopup()) {
				return false;
			}
		} else if (hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
			if (!initWithAppDecor()) {
				return false;
			}
		} else if (hasFlag(_info->capabilities, WindowCapabilities::ServerSideDecorations)
				&& hasFlag(_info->flags, WindowCreationFlags::PreferServerSideDecoration)) {
			if (!initWithServerDecor()) {
				return false;
			}
		} else {
			if (!initWithAppDecor()) {
				return false;
			}
		}
	}

	uint32_t rate = 60'000;
	for (auto &it : _display->outputs) { rate = sprt::max(rate, uint32_t(it->currentMode.rate)); }
	_frameRate = rate;

	return true;
}

void WaylandWindow::mapWindow() {
	_mapped = true;
	_display->flush();
}

void WaylandWindow::unmapWindow() {
	if (_frameCallback) {
		wl_callback_destroy(_frameCallback);
		_frameCallback = nullptr;
	}
	_mapped = false;
}

bool WaylandWindow::close() {
	if (!_mapped) {
		return true;
	}

	if (!_shouldClose) {
		_shouldClose = true;
		if (!_controller->notifyWindowClosed(this)) {
			if (hasFlag(_info->state, WindowState::CloseGuard)) {
				updateState(_configureSerial, _info->state | WindowState::CloseRequest);
			}
			_shouldClose = false;
			return false;
		}
		return true;
	}
	return false;
}

void WaylandWindow::handleFrameReady(const PresentationFrameInfo &frame) {
	// Note that wl_surface_commit will be called by gAPI (vkPresentKHR) after this func;
	// We should not commit anything from here

	// Extract new windows's extent from rendered frame
	auto newExtent = Extent2(frame.constraints.extent.width, frame.constraints.extent.height);
	if (_density != 0.0f) {
		newExtent.width /= _density;
		newExtent.height /= _density;
	}

	// Check if we should update window's geometry
	bool windowGeometryDirty = _commitedExtent.width != newExtent.width
			|| _commitedExtent.height != newExtent.height || _configureSerial != Max<uint32_t>;

	if (!windowGeometryDirty) {
		// if some detoration's frame changed - general geometry is also dirty
		for (auto &it : _decors) {
			if (it->dirty) {
				windowGeometryDirty = true;
				break;
			}
		}
	}

	if (!_frameCallback) {
		// add callback for next frame
		_frameCallback = wl_surface_frame(_surface);
		wl_callback_add_listener(_frameCallback, &s_WaylandSurfaceFrameListener, this);
	}

	if (!windowGeometryDirty) {
		return;
	}

	String stream;
	stream += toString("handleFrameReady: ", frame.order, " commit: ", newExtent.width, " ",
			newExtent.height, ";");

	_commitedExtent = newExtent;

	// Update decoration's geometry to match new window extent
	configureDecorations(_commitedExtent);

	// There are two cases to be here
	// First - interactive resize (Resizing flag set), it this case - apply current frame's size for xdg_surface
	// Second - force-resize by WM to match some constraints, in this case, wait until PresentationEngine perform
	// frame, that extent's match _awaitingExtent
	if (_configureSerial != Max<uint32_t>
			&& (hasFlag(_info->state, WindowState::Resizing) || _commitedExtent == _awaitingExtent
					|| _awaitingExtent == Extent2())) {
		if (_toplevel) {
			// Re-assert min/max: decoration insets depend on _serverDecor, which is negotiated
			// asynchronously after init, so the constraints may need refreshing here.
			updateSizeConstraints();
		}

		if (_xdgSurface) {
			// Offset windows's extent by decoration's frames
			auto pos = IVec2(0, 0);
			auto extent = _commitedExtent;
			if (!hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)
					&& !hasFlag(_info->state, WindowState::Fullscreen) && _serverDecor == nullptr) {
				extent.height += DecorInset + DecorOffset;
				pos.y -= DecorInset + DecorOffset;
			}

			xdg_surface_set_window_geometry(_xdgSurface, pos.x, pos.y, extent.width, extent.height);

			stream += toString(" surface: ", extent.width, " ", extent.height);

			// If we have _configureSerial - confirm it
			if (_configureSerial != 0 && _configureSerial != Max<uint32_t>) {
				xdg_surface_ack_configure(_xdgSurface, _configureSerial);
				stream += toString("; configure: ", _configureSerial, ";");
			}
		}
		_configureSerial = Max<uint32_t>;
	}

	XL_WAYLAND_LOG(stream);
}

void WaylandWindow::handleFramePresented(const PresentationFrameInfo &frame) {
	// New frame was presented, so, wl_surface_commit was сalled before this func

	// ...but a commit only reaches the compositor once the connection is flushed, and nothing else
	// here is guaranteed to do it: the presentation engine does not necessarily return to the
	// looper's poll before the next frame. Without this the picture updates only when some
	// unrelated event happens to pump the socket. Mesa flushes for the Vulkan path; for a
	// host-rasterized frame it is ours to do, exactly as XcbWindow does.
	wl_display_flush(_display->display);

	// With interactive resize, we can miss some last resizing frames due async rendering;
	// It this case, _awaitingExtent (target extent for resize op) != _commitedExtent (last frame's extent)
	// We should then inform PresentationEngine that swapchain should be updated one more time, by calling
	// handleToplevelGeometry with new extent
	if (_awaitingExtent != Extent2() && _commitedExtent != _awaitingExtent
			&& !hasFlag(_info->state, WindowState::Resizing)) {
		handleToplevelGeometry(_toplevel, _awaitingExtent.width, _awaitingExtent.height, false,
				nullptr);
	}
}

void WaylandWindow::handleSwapchainUpdated(const FrameConstraints &c) { }

Rc<SoftwareSurface> WaylandWindow::makeSoftwareSurface() {
	return Rc<WaylandSoftwareSurface>::create(this);
}

FrameConstraints WaylandWindow::exportConstraints(uint64_t &serial) const {
	auto ret = NativeWindow::exportConstraints(serial);

	serial = _configureSerial;

	ret.extent = Extent3(_currentExtent.width, _currentExtent.height, 1);
	if (ret.density == 0.0f) {
		ret.density = 1.0f;
	}
	if (_density != 0.0f) {
		ret.density *= _density;
		ret.extent.width *= _density;
		ret.extent.height *= _density;
		ret.surfaceDensity = _density;
	}
	ret.frameInterval = 1'000'000'000 / _frameRate;
	return move(ret);
}

SurfaceInterfaceInfo WaylandWindow::getSurfaceInterfaceInfo() const {
	SurfaceInterfaceInfo ret;
	ret.backend = SurfaceBackend::Wayland;
	ret.wayland.surface = _surface;
	ret.wayland.display = _display->display;
	return ret;
}

SurfaceInfo WaylandWindow::getSurfaceOptions(SurfaceInfo &&info) const {
	info.currentExtent = _currentExtent;
	if (_density != 0.0f) {
		info.currentExtent.width *= _density;
		info.currentExtent.height *= _density;
	}
	return sprt::move(info);
}

Extent2 WaylandWindow::getExtent() const { return _currentExtent; }

PresentationOptions WaylandWindow::getPreferredOptions() const {
	PresentationOptions opts;
	opts.followDisplayLinkBarrier = true;
	opts.acquireImageWithoutFence = true;
	return opts;
}

void WaylandWindow::handleSurfaceEnter(wl_surface *surface, wl_output *output) {
	if (!_wayland->ownsProxy(output)) {
		return;
	}

	auto out = (WaylandOutput *)wl_output_get_user_data(output);
	if (out) {
		_activeOutputs.emplace(out);
		XL_WAYLAND_LOG("handleSurfaceEnter: output: ", out->description());
	}

	uint32_t rate = 60'000;
	for (auto &it : _activeOutputs) { rate = sprt::max(rate, uint32_t(it->currentMode.rate)); }

	if (rate != _frameRate) {
		_frameRate = rate;
		_controller->notifyWindowConstraintsChanged(this, UpdateConstraintsFlags::None);
	}
}

void WaylandWindow::handleSurfaceLeave(wl_surface *surface, wl_output *output) {
	if (!_wayland->ownsProxy(output)) {
		return;
	}

	auto out = (WaylandOutput *)wl_output_get_user_data(output);
	if (out) {
		_activeOutputs.erase(out);
		XL_WAYLAND_LOG("handleSurfaceLeave: output: ", out->description());
	}

	uint32_t rate = 60'000;
	for (auto &it : _activeOutputs) { rate = sprt::max(rate, uint32_t(it->currentMode.rate)); }

	if (rate != _frameRate) {
		_frameRate = rate;
		_controller->notifyWindowConstraintsChanged(this, UpdateConstraintsFlags::None);
	}
}

void WaylandWindow::handleSurfaceConfigure(xdg_surface *surface, uint32_t serial) {
	XL_WAYLAND_LOG("handleSurfaceConfigure: serial: ", serial);

	if (_configureSerial == 0 && _xdgSurface) {
		// initial config
		if (!hasFlag(_info->state, WindowState::Fullscreen)) {
			if (!_decors.empty() && _xdgSurface) {
				auto headerHeight = DecorInset + DecorOffset;
				if (hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
					headerHeight = 0;
				}

				configureDecorations(_currentExtent);
				xdg_surface_set_window_geometry(_xdgSurface, 0, -headerHeight, _currentExtent.width,
						_currentExtent.height + headerHeight);
			} else if (_xdgSurface) {
				xdg_surface_set_window_geometry(_xdgSurface, 0, 0, _currentExtent.width,
						_currentExtent.height);
			}
		}
	}
	_configureSerial = serial;

	if (!_started) {
		_controller->notifyWindowCreated(this);
		_started = true;
	}

	if (_toplevelDirty) {
		emitAppFrame();
		_toplevelDirty = false;
	}
}

void WaylandWindow::handleToplevelConfigure(xdg_toplevel *xdg_toplevel, int32_t width,
		int32_t height, wl_array *states) {

	String stream;
	stream = toString("handleToplevelConfigure", (!states ? "(syntetic)" : ""), " width: ", width,
			", height: ", height, ";");

	bool hasModeSwitch = false;
	bool unfullscreen = false;

	if (states) {
		WindowState mask = WindowState::Maximized | WindowState::Fullscreen | WindowState::Resizing
				| WindowState::Focused | WindowState::Minimized | WindowState::TilingMask;
		WindowState state = WindowState::None;

		for (size_t off = 0; off + sizeof(uint32_t) <= states->size; off += sizeof(uint32_t)) {
			uint32_t *it = (uint32_t *)((const char *)states->data + off);
			switch (*it) {
			case XDG_TOPLEVEL_STATE_MAXIMIZED: state |= WindowState::Maximized; break;
			case XDG_TOPLEVEL_STATE_FULLSCREEN: state |= WindowState::Fullscreen; break;
			case XDG_TOPLEVEL_STATE_RESIZING: state |= WindowState::Resizing; break;
			case XDG_TOPLEVEL_STATE_ACTIVATED: state |= WindowState::Focused; break;
			case XDG_TOPLEVEL_STATE_TILED_LEFT: state |= WindowState::TiledLeft; break;
			case XDG_TOPLEVEL_STATE_TILED_RIGHT: state |= WindowState::TiledRight; break;
			case XDG_TOPLEVEL_STATE_TILED_TOP: state |= WindowState::TiledTop; break;
			case XDG_TOPLEVEL_STATE_TILED_BOTTOM: state |= WindowState::TiledBottom;
			case XDG_TOPLEVEL_STATE_SUSPENDED: state |= WindowState::Minimized; break;
			case XDG_TOPLEVEL_STATE_CONSTRAINED_LEFT: state |= WindowState::ConstrainedLeft; break;
			case XDG_TOPLEVEL_STATE_CONSTRAINED_RIGHT:
				state |= WindowState::ConstrainedRight;
				break;
			case XDG_TOPLEVEL_STATE_CONSTRAINED_TOP: state |= WindowState::ConstrainedTop; break;
			case XDG_TOPLEVEL_STATE_CONSTRAINED_BOTTOM:
				state |= WindowState::ConstrainedBottom;
				break;
			}
		}

		if (hasFlag(state, WindowState::Maximized)
				!= hasFlag(_info->state, WindowState::Maximized)) {
			hasModeSwitch = true;
		}

		if (hasFlag(state, WindowState::Focused) != hasFlag(_info->state, WindowState::Focused)) {
			hasModeSwitch = true;
		}

		if (hasFlag(state, WindowState::Fullscreen)
				!= hasFlag(_info->state, WindowState::Fullscreen)) {
			hasModeSwitch = true;
			if (!hasFlag(state, WindowState::Fullscreen)) {
				unfullscreen = true;
			}
		}

		stream += toString(state, " ");

		updateState(_configureSerial, (_info->state & ~mask) | state);
	}

	if (unfullscreen && !_activeOutputs.empty()) {
		auto extent = (*_activeOutputs.begin())->currentMode.size;
		if (extent.width == uint32_t(width) && extent.height == uint32_t(height)) {
			oslog::vperror(__SPRT_LOCATION, "Wayland", "Unfullscreen failed, restore saved params");
			width = _savedExtent.width;
			height = _savedExtent.height;
		}
	}

	auto checkVisible = [&, this](WaylandDecorationName name) {
		switch (name) {
		case WaylandDecorationName::RightSide:
			if (hasFlag(_info->state,
						WindowState::Maximized | WindowState::Fullscreen
								| WindowState::TiledRight)) {
				return false;
			}
			break;
		case WaylandDecorationName::TopRightCorner:
			if (hasFlag(_info->state, WindowState::Maximized | WindowState::Fullscreen)
					|| hasFlagAll(_info->state, WindowState::TiledTopRight)) {
				return false;
			}
			break;
		case WaylandDecorationName::TopSide:
			if (hasFlag(_info->state,
						WindowState::Maximized | WindowState::Fullscreen | WindowState::TiledTop)) {
				return false;
			}
			break;
		case WaylandDecorationName::TopLeftCorner:
			if (hasFlag(_info->state, WindowState::Maximized | WindowState::Fullscreen)
					|| hasFlagAll(_info->state, WindowState::TiledTopLeft)) {
				return false;
			}
			break;
		case WaylandDecorationName::BottomRightCorner:
			if (hasFlag(_info->state, WindowState::Maximized | WindowState::Fullscreen)
					|| hasFlagAll(_info->state, WindowState::TiledBottomRight)) {
				return false;
			}
			break;
		case WaylandDecorationName::BottomSide:
			if (hasFlag(_info->state,
						WindowState::Maximized | WindowState::Fullscreen
								| WindowState::TiledBottom)) {
				return false;
			}
			break;
		case WaylandDecorationName::BottomLeftCorner:
			if (hasFlag(_info->state, WindowState::Maximized | WindowState::Fullscreen)
					|| hasFlagAll(_info->state, WindowState::TiledBottomLeft)) {
				return false;
			}
			break;
		case WaylandDecorationName::LeftSide:
			if (hasFlag(_info->state,
						WindowState::Maximized | WindowState::Fullscreen
								| WindowState::TiledLeft)) {
				return false;
			}
			break;
		case WaylandDecorationName::HeaderLeft:
		case WaylandDecorationName::HeaderRight:
		case WaylandDecorationName::HeaderCenter:
		case WaylandDecorationName::HeaderBottom:
			if (hasFlag(_info->state, WindowState::Fullscreen)) {
				return false;
			}
			break;

		default: break;
		}
		return true;
	};

	for (auto &it : _decors) {
		it->setActive(hasFlag(_info->state, WindowState::Focused));
		it->setVisible(checkVisible(it->name));
	}

	handleToplevelGeometry(xdg_toplevel, width, height, hasModeSwitch, &stream);

	XL_WAYLAND_LOG(stream);

	_toplevelDirty = true;
}

void WaylandWindow::handleToplevelGeometry(xdg_toplevel *xdg_toplevel, int32_t width,
		int32_t height, bool hasModeSwitch, String *stream) {
	if (width && height && _commitedExtent.width && _commitedExtent.height) {
		if (!_serverDecor && !hasFlag(_info->state, WindowState::Fullscreen)
				&& !hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
			height -= (DecorInset + DecorOffset);
		}

		if ((_currentExtent.width != static_cast<uint32_t>(width)
					|| _currentExtent.height != static_cast<uint32_t>(height))) {
			if (_currentExtent == _commitedExtent) {
				_currentExtent.width = static_cast<uint32_t>(width);
				_currentExtent.height = static_cast<uint32_t>(height);
				if (hasModeSwitch) {
					_awaitingExtent = _currentExtent;
				} else {
					_awaitingExtent = Extent2(0, 0);
				}

				_controller->notifyWindowConstraintsChanged(this,
						UpdateConstraintsFlags::DeprecateSwapchain);

				if (stream) {
					(*stream) +=
							toString("surface: ", _currentExtent.width, " ", _currentExtent.height);
				}
			} else {
				_awaitingExtent =
						Extent2(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
			}
		}
	}
}

void WaylandWindow::handleToplevelClose(xdg_toplevel *xdg_toplevel) {
	XL_WAYLAND_LOG("handleToplevelClose");
	_controller->notifyWindowClosed(this);
}

void WaylandWindow::handlePopupConfigure(xdg_popup *, int32_t x, int32_t y, int32_t width,
		int32_t height) {
	_info->rect = IRect(x, y, width, height);
	handleToplevelGeometry(nullptr, width, height, false, nullptr);
}

void WaylandWindow::handlePopupDone(xdg_popup *) {
	_controller->notifyWindowClosed(this, WindowCloseOptions::CloseInPlace);
}

void WaylandWindow::handleToplevelBounds(xdg_toplevel *xdg_toplevel, int32_t width,
		int32_t height) {
	XL_WAYLAND_LOG("handleToplevelBounds: width: ", width, ", height: ", height);
}

void WaylandWindow::handleToplevelCapabilities(xdg_toplevel *xdg_toplevel, wl_array *capabilities) {
	WindowState capState =
			WindowState::AllowedClose | WindowState::AllowedMove | WindowState::AllowedResize;

	for (size_t off = 0; off + sizeof(uint32_t) <= capabilities->size; off += sizeof(uint32_t)) {
		uint32_t *it = (uint32_t *)((const char *)capabilities->data + off);
		switch (*it) {
		case XDG_TOPLEVEL_WM_CAPABILITIES_WINDOW_MENU:
			capState |= WindowState::AllowedWindowMenu;
			break;
		case XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE:
			capState |= WindowState::AllowedMaximizeHorz | WindowState::AllowedMaximizeVert;
			break;
		case XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN:
			capState |= WindowState::AllowedFullscreen;
			break;
		case XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE:
			capState |= WindowState::AllowedMaximizeHorz | WindowState::AllowedMinimize;
			break;
		}
	}

	updateState(_configureSerial, (_info->state & ~WindowState::AllowedActionsMask) | capState);
}

void WaylandWindow::handleSurfaceFrameDone(wl_callback *frame, uint32_t data) {
	if (frame != _frameCallback) {
		wl_callback_destroy(frame);
	} else {
		wl_callback_destroy(frame);
		_frameCallback = nullptr;
		if (_appWindow) {
			_appWindow->update(PresentationUpdateFlags::DisplayLink);
		}
	}
}

void WaylandWindow::handleDecorConfigure(zxdg_toplevel_decoration_v1 *decor, uint32_t mode) {
	XL_WAYLAND_LOG("handleDecorConfigure:", mode);
}

void WaylandWindow::handlePointerEnter(wl_fixed_t surface_x, wl_fixed_t surface_y) {
	if (!_pointerInit || _display->seat->hasPointerFrames) {
		auto &ev = _pointerEvents.emplace_back(PointerEvent{PointerEvent::Enter});
		ev.enter.x = surface_x;
		ev.enter.y = surface_y;
	} else {
		float d = _density;
		if (d == 0.0f) {
			d = 1.0f;
		}

		_surfaceFX = surface_x;
		_surfaceFY = surface_y;

		_surfaceX = wl_fixed_to_double(surface_x) * d;
		_surfaceY = _currentExtent.height * d - wl_fixed_to_double(surface_y) * d;

		updateState(_configureSerial, _info->state | WindowState::Pointer);

		InputEventData event({
			Max<uint32_t>,
			InputEventName::MouseMove,
			{{
				InputMouseButton::None,
				_activeModifiers,
				float(_surfaceX),
				float(_surfaceY),
			}},
		});

		_pendingEvents.emplace_back(event);
	}

	XL_WAYLAND_LOG("handlePointerEnter: x: ", wl_fixed_to_int(surface_x),
			", y: ", wl_fixed_to_int(surface_y));
}

void WaylandWindow::handlePointerLeave() {
	if (!_pointerInit) {
		_pointerInit = true;
		if (!_display->seat->hasPointerFrames) {
			handlePointerFrame();
		}
	}

	handlePointerFrame(); // drop pending events
	updateState(_configureSerial, _info->state & ~WindowState::Pointer);
}

static uint32_t getWaylandGripEdge(WindowLayerFlags grip) {
	switch (grip) {
	case WindowLayerFlags::ResizeTopLeftGrip: return 5; break;
	case WindowLayerFlags::ResizeTopGrip: return 1; break;
	case WindowLayerFlags::ResizeTopRightGrip: return 9; break;
	case WindowLayerFlags::ResizeRightGrip: return 8; break;
	case WindowLayerFlags::ResizeBottomRightGrip: return 10; break;
	case WindowLayerFlags::ResizeBottomGrip: return 2; break;
	case WindowLayerFlags::ResizeBottomLeftGrip: return 6; break;
	case WindowLayerFlags::ResizeLeftGrip: return 4; break;
	default: break;
	}
	return 0;
}

void WaylandWindow::handlePointerMotion(uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
	// XL_WAYLAND_LOG("handlePointerMotion: x: ", wl_fixed_to_int(surface_x), ", y: ", wl_fixed_to_int(surface_y));

	if (_buttonGripFlags != WindowLayerFlags::None) {
		if (_buttons.test(toInt(InputMouseButton::MouseLeft)) && _buttons.count() == 1) {
			auto grip = _buttonGripFlags;
			switch (grip) {
			case WindowLayerFlags::MoveGrip:
				xdg_toplevel_move(_toplevel, _display->seat->seat, _buttonGripSerial);
				break;
			case WindowLayerFlags::ResizeTopLeftGrip:
			case WindowLayerFlags::ResizeTopGrip:
			case WindowLayerFlags::ResizeTopRightGrip:
			case WindowLayerFlags::ResizeRightGrip:
			case WindowLayerFlags::ResizeBottomRightGrip:
			case WindowLayerFlags::ResizeBottomGrip:
			case WindowLayerFlags::ResizeBottomLeftGrip:
			case WindowLayerFlags::ResizeLeftGrip:
				xdg_toplevel_resize(_toplevel, _display->seat->seat, _buttonGripSerial,
						getWaylandGripEdge(grip));
				break;
			default: break;
			}
			cancelPointerEvents();
			return;
		}
	}

	if (!_pointerInit) {
		_pointerInit = true;
		if (!_display->seat->hasPointerFrames) {
			handlePointerFrame();
		}
	}

	if (_display->seat->hasPointerFrames) {
		auto &ev = _pointerEvents.emplace_back(PointerEvent{PointerEvent::Motion});
		ev.motion.time = time;
		ev.motion.x = surface_x;
		ev.motion.y = surface_y;
	} else {
		_surfaceFX = surface_x;
		_surfaceFY = surface_y;

		_surfaceX = wl_fixed_to_double(surface_x);
		_surfaceY = _currentExtent.height - wl_fixed_to_double(surface_y);

		if (_density != 0.0f) {
			_surfaceX *= _density;
			_surfaceX *= _density;
		}

		_pendingEvents.emplace_back(InputEventData({
			Max<uint32_t>,
			InputEventName::MouseMove,
			{{
				InputMouseButton::None,
				_activeModifiers,
				float(_surfaceX),
				float(_surfaceY),
			}},
		}));
	}
}

static InputMouseButton getWaylandButton(uint32_t button) {
	switch (button) {
	case BTN_LEFT: return InputMouseButton::MouseLeft; break;
	case BTN_RIGHT: return InputMouseButton::MouseRight; break;
	case BTN_MIDDLE: return InputMouseButton::MouseMiddle; break;
	default: return InputMouseButton(toInt(InputMouseButton::Mouse8) + (button - 0x113)); break;
	}
	return InputMouseButton::None;
}

void WaylandWindow::handlePointerButton(uint32_t serial, uint32_t time, uint32_t button,
		uint32_t state) {
	// Only presses: xdg_popup.grab is validated against the implicit pointer grab, which starts on
	// press and ends when the last button is released. Handing the compositor a release serial
	// (the app opens menus on tap, so that is the newest event by then) gets the grab denied — the
	// popup then behaves like an ordinary surface: clicks in the parent are delivered to the
	// parent instead of dismissing the menu.
	if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
		_controller->notePointerSerial(serial);
	}
	if (!_pointerInit) {
		return;
	}

	auto btn = getWaylandButton(button);

	if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
		if (btn == InputMouseButton::MouseLeft) {
			// Capture current grip flags
			if (hasFlag(_currentLayerFlags, WindowLayerFlags::WindowMenuLeft)) {
				cancelPointerEvents();
				xdg_toplevel_show_window_menu(_toplevel, _display->seat->seat, serial,
						wl_fixed_to_int(_surfaceFX), wl_fixed_to_int(_surfaceFY));
				return;
			} else {
				_buttonGripSerial = serial;
				_buttonGripFlags = _gripFlags;
			}
		} else if (btn == InputMouseButton::MouseRight) {
			if (hasFlag(_currentLayerFlags, WindowLayerFlags::WindowMenuRight)) {
				cancelPointerEvents();
				xdg_toplevel_show_window_menu(_toplevel, _display->seat->seat, serial,
						wl_fixed_to_int(_surfaceFX), wl_fixed_to_int(_surfaceFY));
				return;
			}
		}
	} else {
		if (btn == InputMouseButton::MouseLeft) {
			_buttonGripFlags = WindowLayerFlags::None;
		}
	}

	XL_WAYLAND_LOG("handlePointerButton");
	if (_display->seat->hasPointerFrames) {
		auto &ev = _pointerEvents.emplace_back(PointerEvent{PointerEvent::Button});
		ev.button.serial = serial;
		ev.button.time = time;
		ev.button.button = button;
		ev.button.state = state;
	} else {
		if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
			_buttons.set(toInt(btn));
			_pendingEvents.emplace_back(InputEventData({
				button,
				InputEventName::Begin,
				{{
					btn,
					_activeModifiers,
					float(_surfaceX),
					float(_surfaceY),
				}},
			}));
		} else if (state == WL_POINTER_BUTTON_STATE_RELEASED && _buttons.test(toInt(btn))) {
			_buttons.reset(toInt(btn));
			_pendingEvents.emplace_back(InputEventData({
				button,
				InputEventName::End,
				{{
					btn,
					_activeModifiers,
					float(_surfaceX),
					float(_surfaceY),
				}},
			}));
		}
	}
}

void WaylandWindow::handlePointerAxis(uint32_t time, uint32_t axis, float val) {
	if (!_pointerInit) {
		return;
	}

	XL_WAYLAND_LOG("handlePointerAxis: ", time);

	if (_display->seat->hasPointerFrames) {
		auto &ev = _pointerEvents.emplace_back(PointerEvent{PointerEvent::Axis});
		ev.axis.time = time;
		ev.axis.axis = axis;
		ev.axis.value = val;
	} else {
		InputMouseButton btn = InputMouseButton::None;
		switch (axis) {
		case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
			if (val < 0) {
				btn = InputMouseButton::MouseScrollUp;
			} else {
				btn = InputMouseButton::MouseScrollDown;
			}
			break;
		case WL_POINTER_AXIS_VERTICAL_SCROLL:
			if (val > 0) {
				btn = InputMouseButton::MouseScrollRight;
			} else {
				btn = InputMouseButton::MouseScrollLeft;
			}
			break;
		}

		InputEventData event({
			toInt(btn),
			InputEventName::Scroll,
			{{
				btn,
				_activeModifiers,
				float(_surfaceX),
				float(_surfaceY),
			}},
		});

		switch (axis) {
		case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
			event.point.valueX = val;
			event.point.valueY = 0.0f;
			break;
		case WL_POINTER_AXIS_VERTICAL_SCROLL:
			event.point.valueX = 0.0f;
			event.point.valueY = -val;
			break;
		}

		_pendingEvents.emplace_back(event);
	}
}

void WaylandWindow::handlePointerAxisSource(uint32_t axis_source) {
	if (!_pointerInit) {
		return;
	}

	XL_WAYLAND_LOG("handlePointerAxisSource");

	auto &ev = _pointerEvents.emplace_back(PointerEvent{PointerEvent::AxisSource});
	ev.axisSource.axis_source = axis_source;
}

void WaylandWindow::handlePointerAxisStop(uint32_t time, uint32_t axis) {
	if (!_pointerInit) {
		return;
	}

	XL_WAYLAND_LOG("handlePointerAxisStop");

	auto &ev = _pointerEvents.emplace_back(PointerEvent{PointerEvent::AxisStop});
	ev.axisStop.time = time;
	ev.axisStop.axis = axis;
}

void WaylandWindow::handlePointerAxisDiscrete(uint32_t axis, int32_t discrete) {
	if (!_pointerInit) {
		return;
	}

	XL_WAYLAND_LOG("handlePointerAxisDiscrete");

	auto &ev = _pointerEvents.emplace_back(PointerEvent{PointerEvent::AxisDiscrete});
	ev.axisDiscrete.axis = axis;
	ev.axisDiscrete.discrete120 = discrete;
}

void WaylandWindow::handlePointerAxisRelativeDirection(uint32_t axis, uint32_t direction) {
	if (!_pointerInit) {
		return;
	}

	XL_WAYLAND_LOG("handlePointerAxisRelativeDirection");

	auto &ev = _pointerEvents.emplace_back(PointerEvent{PointerEvent::AxisDiscrete});
	ev.axisRelativeDirection.axis = axis;
	ev.axisRelativeDirection.direction = direction;
}

void WaylandWindow::handlePointerFrame() {
	if (!_pointerInit || _pointerEvents.empty()) {
		return;
	}

	bool positionChanged = false;
	double x = 0.0f;
	double y = 0.0f;

	InputMouseButton axisBtn = InputMouseButton::None;
	uint32_t axisSource = 0;
	bool hasAxis = false;
	double axisX = 0.0f;
	double axisY = 0.0f;

	float d = _density;
	if (d == 0.0f) {
		d = 1.0f;
	}

	float height = _currentExtent.height * d;

	for (auto &it : _pointerEvents) {
		switch (it.event) {
		case PointerEvent::None: break;
		case PointerEvent::Enter:
			positionChanged = true;
			x = wl_fixed_to_double(it.enter.x) * d;
			y = height - wl_fixed_to_double(it.enter.y) * d;

			updateState(_configureSerial, _info->state | WindowState::Pointer);

			_pendingEvents.emplace_back(InputEventData({
				Max<uint32_t>,
				InputEventName::MouseMove,
				{{
					InputMouseButton::None,
					_activeModifiers,
					static_cast<float>(x),
					static_cast<float>(y),
				}},
			}));
			break;
		case PointerEvent::Leave: break;
		case PointerEvent::Motion:
			positionChanged = true;

			_surfaceFX = it.motion.x;
			_surfaceFY = it.motion.y;

			x = wl_fixed_to_double(it.motion.x) * d;
			y = height - wl_fixed_to_double(it.motion.y) * d;
			break;
		case PointerEvent::Button: break;
		case PointerEvent::Axis:
			switch (it.axis.axis) {
			case WL_POINTER_AXIS_VERTICAL_SCROLL:
				hasAxis = true;
				axisY -= it.axis.value;
				if (it.axis.value < 0) {
					axisBtn = InputMouseButton::MouseScrollUp;
				} else {
					axisBtn = InputMouseButton::MouseScrollDown;
				}
				break;
			case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
				hasAxis = true;
				axisX += it.axis.value;
				if (it.axis.value > 0) {
					axisBtn = InputMouseButton::MouseScrollRight;
				} else {
					axisBtn = InputMouseButton::MouseScrollLeft;
				}
				break;
			default: break;
			}
			break;
		case PointerEvent::AxisSource: axisSource = it.axisSource.axis_source; break;
		case PointerEvent::AxisStop: break;
		case PointerEvent::AxisDiscrete: break;
		}
	}

	if (positionChanged) {
		_surfaceX = x;
		_surfaceY = y;

		_pendingEvents.emplace_back(InputEventData({
			Max<uint32_t>,
			InputEventName::MouseMove,
			{{
				InputMouseButton::None,
				_activeModifiers,
				float(_surfaceX),
				float(_surfaceY),
			}},
		}));
	}

	if (hasAxis) {
		auto &event = _pendingEvents.emplace_back(InputEventData({
			axisSource,
			InputEventName::Scroll,
			{{
				axisBtn,
				_activeModifiers,
				float(_surfaceX),
				float(height - _surfaceY),
			}},
		}));

		event.point.valueX = float(axisX);
		event.point.valueY = float(axisY);
		event.point.density = 1.0f;
	}

	for (auto &it : _pointerEvents) {
		switch (it.event) {
		case PointerEvent::None: break;
		case PointerEvent::Enter: break;
		case PointerEvent::Leave:
			updateState(_configureSerial, _info->state & ~WindowState::Pointer);
			break;
		case PointerEvent::Motion: break;
		case PointerEvent::Button:
			if (it.button.state == WL_POINTER_BUTTON_STATE_PRESSED) {
				_buttons.set(toInt(getWaylandButton(it.button.button)));
				_pendingEvents.emplace_back(InputEventData({
					it.button.button,
					InputEventName::Begin,
					{{
						getWaylandButton(it.button.button),
						_activeModifiers,
						float(_surfaceX),
						float(_surfaceY),
					}},
				}));
			} else if (it.button.state == WL_POINTER_BUTTON_STATE_RELEASED
					&& _buttons.test(toInt(getWaylandButton(it.button.button)))) {
				_buttons.reset(toInt(getWaylandButton(it.button.button)));
				_pendingEvents.emplace_back(InputEventData({
					it.button.button,
					InputEventName::End,
					{{
						getWaylandButton(it.button.button),
						_activeModifiers,
						float(_surfaceX),
						float(_surfaceY),
					}},
				}));
			}
			break;
		case PointerEvent::Axis: break;
		case PointerEvent::AxisSource: break;
		case PointerEvent::AxisStop: break;
		case PointerEvent::AxisDiscrete: break;
		}
	}

	_pointerEvents.clear();
}

void WaylandWindow::handleKeyboardEnter(Vector<uint32_t> &&keys, uint32_t depressed,
		uint32_t latched, uint32_t locked) {
	handleKeyModifiers(depressed, latched, locked);
	uint32_t n = 1;
	for (auto &it : keys) {
		handleKey(n, it, WL_KEYBOARD_KEY_STATE_PRESSED);
		++n;
	}
}

void WaylandWindow::handleKeyboardLeave() {
	uint32_t n = 1;
	for (auto &it : _keys) {
		InputEventData event({
			n,
			InputEventName::KeyCanceled,
			{{
				InputMouseButton::None,
				_activeModifiers,
				float(_surfaceX),
				float(_surfaceY),
			}},
		});

		event.key.keycode = _display->seat->translateKey(it.second.scancode);
		event.key.keysym = it.second.scancode;
		event.key.keychar = it.second.codepoint;

		_pendingEvents.emplace_back(move(event));

		++n;
	}
}

void WaylandWindow::handleKey(uint32_t time, uint32_t scancode, uint32_t state) {
	InputEventData event({
		time,
		(state == WL_KEYBOARD_KEY_STATE_PRESSED) ? InputEventName::KeyPressed
												 : InputEventName::KeyReleased,
		{{
			InputMouseButton::None,
			_activeModifiers,
			float(_surfaceX),
			float(_surfaceY),
		}},
	});

	event.key.keycode = _display->seat->translateKey(scancode);
	event.key.compose = InputKeyComposeState::Nothing;
	event.key.keysym = scancode;
	event.key.keychar = 0;

	const xkb_keysym_t *keysyms = nullptr;
	const xkb_keycode_t keycode = scancode + 8;

	const bool hasXkbState = _display->xkb && _display->seat->state;

	if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		char32_t codepoint = 0;
		if (hasXkbState && isTextInputEnabled()) {
			if (_display->xkb->xkb_state_key_get_syms(_display->seat->state, keycode, &keysyms)
					== 1) {
				const xkb_keysym_t keysym =
						_display->seat->composeSymbol(keysyms[0], event.key.compose);
				const uint32_t cp = _display->xkb->xkb_keysym_to_utf32(keysym);
				if (cp != 0 && keysym != XKB_KEY_NoSymbol) {
					codepoint = cp;
				}
			}
		}

		auto it = _keys.emplace(scancode,
							   KeyData{scancode, codepoint,
								   sprt::platform::clock(platform::ClockType::Monotonic), false})
						  .first;

		if (hasXkbState
				&& _display->xkb->xkb_keymap_key_repeats(
						_display->xkb->xkb_state_get_keymap(_display->seat->state), keycode)) {
			it->second.repeats = true;
		}
	} else {
		auto it = _keys.find(scancode);
		if (it == _keys.end()) {
			return;
		}

		event.key.keychar = it->second.codepoint;
		_keys.erase(it);
	}

	_pendingEvents.emplace_back(sprt::move(event));
}

void WaylandWindow::handleKeyModifiers(uint32_t depressed, uint32_t latched, uint32_t locked) {
	if (!_display->seat->state) {
		return;
	}

	_activeModifiers = InputModifier::None;
	if (_display->xkb->xkb_state_mod_index_is_active(_display->seat->state,
				_display->seat->keyState.controlIndex, XKB_STATE_MODS_EFFECTIVE)
			== 1) {
		_activeModifiers |= InputModifier::Ctrl;
	}

	if (_display->xkb->xkb_state_mod_index_is_active(_display->seat->state,
				_display->seat->keyState.altIndex, XKB_STATE_MODS_EFFECTIVE)
			== 1) {
		_activeModifiers |= InputModifier::Alt;
	}

	if (_display->xkb->xkb_state_mod_index_is_active(_display->seat->state,
				_display->seat->keyState.shiftIndex, XKB_STATE_MODS_EFFECTIVE)
			== 1) {
		_activeModifiers |= InputModifier::Shift;
	}

	if (_display->xkb->xkb_state_mod_index_is_active(_display->seat->state,
				_display->seat->keyState.superIndex, XKB_STATE_MODS_EFFECTIVE)
			== 1) {
		_activeModifiers |= InputModifier::Mod4;
	}

	if (_display->xkb->xkb_state_mod_index_is_active(_display->seat->state,
				_display->seat->keyState.capsLockIndex, XKB_STATE_MODS_EFFECTIVE)
			== 1) {
		_activeModifiers |= InputModifier::CapsLock;
	}

	if (_display->xkb->xkb_state_mod_index_is_active(_display->seat->state,
				_display->seat->keyState.numLockIndex, XKB_STATE_MODS_EFFECTIVE)
			== 1) {
		_activeModifiers |= InputModifier::NumLock;
	}
}

void WaylandWindow::handleKeyRepeat() {
	Vector<InputEventData> events;
	auto spawnRepeatEvent = [&, this](const KeyData &it) {
		InputEventData event({
			uint32_t(events.size() + 1),
			InputEventName::KeyRepeated,
			{{
				InputMouseButton::None,
				_activeModifiers,
				float(_surfaceX),
				float(_surfaceY),
			}},
		});

		event.key.keycode = _display->seat->translateKey(it.scancode);
		event.key.keysym = it.scancode;
		event.key.keychar = it.codepoint;

		events.emplace_back(move(event));
	};

	uint64_t repeatDelay = _display->seat->keyState.keyRepeatDelay;
	uint64_t repeatInterval = _display->seat->keyState.keyRepeatInterval;
	auto t = sprt::platform::clock(platform::ClockType::Monotonic);
	for (auto &it : _keys) {
		if (it.second.repeats) {
			if (!it.second.lastRepeat) {
				auto dt = t - it.second.time;
				if (dt > repeatDelay * 1'000) {
					dt -= repeatDelay * 1'000;
					it.second.lastRepeat = t - dt;
				}
			}
			if (it.second.lastRepeat) {
				auto dt = t - it.second.lastRepeat;
				while (dt > repeatInterval) {
					spawnRepeatEvent(it.second);
					dt -= repeatInterval;
					it.second.lastRepeat += repeatInterval;
				}
			}
		}
	}

	for (auto &it : events) { _pendingEvents.emplace_back(it); }
}

void WaylandWindow::notifyScreenChange() { XL_WAYLAND_LOG("notifyScreenChange"); }

void WaylandWindow::motifyThemeChanged(const ThemeInfo &theme) {
	if (theme.colorScheme == "dark" || theme.colorScheme == "prefer-dark") {
		for (auto &it : _decors) { it->setDarkTheme(); }
	} else {
		for (auto &it : _decors) { it->setLightTheme(); }
	}
	emitAppFrame();
}

void WaylandWindow::handleDecorationPress(WaylandDecoration *decor, uint32_t serial, uint32_t btn,
		bool released) {
	auto switchMaximized = [&, this] {
		if (!hasFlag(_info->state, WindowState::Maximized)) {
			xdg_toplevel_set_maximized(_toplevel);
			_iconMaximized->setAlternative(true);
		} else {
			xdg_toplevel_unset_maximized(_toplevel);
			_iconMaximized->setAlternative(false);
		}
	};
	switch (decor->name) {
	case WaylandDecorationName::IconClose:
		emitAppFrame();
		handleToplevelClose(_toplevel);
		return;
		break;
	case WaylandDecorationName::IconMaximize:
		switchMaximized();
		emitAppFrame();
		return;
		break;
	case WaylandDecorationName::IconMinimize: xdg_toplevel_set_minimized(_toplevel); return;
	case WaylandDecorationName::HeaderCenter:
	case WaylandDecorationName::HeaderBottom:
	case WaylandDecorationName::HeaderLeft:
		if (btn == BTN_RIGHT) {
			xdg_toplevel_show_window_menu(_toplevel, _display->seat->seat, serial,
					wl_fixed_to_int(decor->pointerX), wl_fixed_to_int(decor->pointerY));
			emitAppFrame();
		}
		break;
	default: break;
	}
	uint32_t edges = 0;
	switch (decor->image) {
	case WindowCursor::ResizeRight: edges = XDG_TOPLEVEL_RESIZE_EDGE_RIGHT; break;
	case WindowCursor::ResizeTopRight: edges = XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT; break;
	case WindowCursor::ResizeTop: edges = XDG_TOPLEVEL_RESIZE_EDGE_TOP; break;
	case WindowCursor::ResizeTopLeft: edges = XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT; break;
	case WindowCursor::ResizeBottomRight: edges = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT; break;
	case WindowCursor::ResizeBottom: edges = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM; break;
	case WindowCursor::ResizeBottomLeft: edges = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT; break;
	case WindowCursor::ResizeLeft: edges = XDG_TOPLEVEL_RESIZE_EDGE_LEFT; break;
	case WindowCursor::Default:
		if (released) {
			switchMaximized();
			emitAppFrame();
			return;
		}
		break;
	default: break;
	}

	if (edges != 0) {
		xdg_toplevel_resize(_toplevel, _display->seat->seat, serial, edges);
		emitAppFrame();
	} else {
		xdg_toplevel_move(_toplevel, _display->seat->seat, serial);
		emitAppFrame();
	}
}

void WaylandWindow::openWindowMenu(Vec2 pos) {
	if (!_toplevel || !_display || !_display->seat || !_display->seat->seat) {
		return;
	}

	int32_t x, y;
	if (pos.isValid()) {
		// pos: bottom-left origin -> surface-local top-left
		auto ext = getExtent();
		x = int32_t(pos.x);
		y = int32_t(int32_t(ext.height) - int32_t(pos.y));
	} else {
		x = wl_fixed_to_int(_surfaceFX);
		y = wl_fixed_to_int(_surfaceFY);
	}

	cancelPointerEvents();
	xdg_toplevel_show_window_menu(_toplevel, _display->seat->seat, _display->seat->serial, x, y);
}

void WaylandWindow::setPreferredScale(int32_t scale) {
	if (_density != float(scale)) {
		_density = float(scale);
		wl_surface_set_buffer_scale(_surface, scale);
		_controller->notifyWindowConstraintsChanged(this, UpdateConstraintsFlags::None);
	}
}

void WaylandWindow::setPreferredTransform(uint32_t) { }

void WaylandWindow::dispatchPendingEvents() {
	if (!_shouldClose) {
		if (!_keys.empty()) {
			handleKeyRepeat();
		}
	}

	if (_appWindow) {
		NativeWindow::dispatchPendingEvents();
	}

	bool surfacesDirty = false;
	for (auto &it : _decors) {
		if (it->commit()) {
			surfacesDirty = true;
		}
	}

	if (surfacesDirty) {
		wl_surface_commit(_surface);
	}
}

WindowCursor WaylandWindow::getCursor() const {
	if (_cursor == WindowCursor::Undefined) {
		return WindowCursor::Default;
	}
	return _cursor;
}

bool WaylandWindow::enableState(WindowState state) {
	if (NativeWindow::enableState(state)) {
		return true;
	}

	switch (state) {
	case WindowState::Minimized:
		if (_toplevel) {
			xdg_toplevel_set_minimized(_toplevel);
			return true;
		}
		break;
	case WindowState::Maximized:
		if (_toplevel) {
			xdg_toplevel_set_maximized(_toplevel);
			return true;
		}
		break;
	default: break;
	}

	return false;
}

bool WaylandWindow::disableState(WindowState state) {
	if (NativeWindow::disableState(state)) {
		return true;
	}

	switch (state) {
	case WindowState::Maximized:
		if (_toplevel) {
			xdg_toplevel_unset_maximized(_toplevel);
			return true;
		}
		break;
	default: break;
	}

	return false;
}

bool WaylandWindow::updateTextInput(const TextInputRequest &, TextInputFlags flags) { return true; }

void WaylandWindow::cancelTextInput() { }

Status WaylandWindow::setFullscreenState(FullscreenInfo &&info) {
	auto enable = info != FullscreenInfo::None;
	if (!enable) {
		if (hasFlag(_info->state, WindowState::Fullscreen)) {
			if (_toplevel) {
				xdg_toplevel_unset_fullscreen(_toplevel);
			}
			_info->fullscreen = move(info);
			return Status::Ok;
		} else {
			return Status::Declined;
		}
	} else {
		if (info == FullscreenInfo::Current) {
			if (!hasFlag(_info->state, WindowState::Fullscreen)) {
				_savedExtent = _currentExtent;
				auto current = (*_activeOutputs.begin());
				if (_toplevel) {
					xdg_toplevel_set_fullscreen(_toplevel, current->output);
				}

				// replace info with real data
				info.mode = ModeInfo{current->currentMode.size.width,
					current->currentMode.size.height, current->currentMode.rate};

				auto cfg = _controller->getDisplayConfigManager()->getCurrentConfig();
				if (cfg) {
					for (auto &it : cfg->monitors) {
						if (it.id.name == current->name) {
							info.id = it.id;
							info.mode = it.getCurrent().mode;
							break;
						}
					}
				}

				_info->fullscreen = move(info);
				return Status::Ok;
			}
			return Status::Declined;
		}

		if (!hasFlag(_info->state, WindowState::Fullscreen)) {
			_savedExtent = _currentExtent;
		}

		// find target output
		if (hasFlag(_info->state, WindowState::Fullscreen)) {
			if (_toplevel) {
				xdg_toplevel_unset_fullscreen(_toplevel);
			}
		}
		for (auto &it : _display->outputs) {
			if (it->name == info.id.name) {
				if (_toplevel) {
					xdg_toplevel_set_fullscreen(_toplevel, it->output);
				}
				_info->fullscreen = move(info);
				return Status::Ok;
				break;
			}
		}
		return Status::ErrorInvalidArguemnt;
	}
}

void WaylandWindow::setCursor(WindowCursor cursor) {
	_cursor = cursor;
	_display->seat->setCursor(_cursor, isServerSideCursors());
}

bool WaylandWindow::configureDecorations(Extent2 extent) {
	auto &theme = _controller->getThemeInfo();
	bool userDecor = hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations);
	int32_t x = 0;
	int32_t y = 0;
	int32_t width = userDecor ? static_cast<uint32_t>(theme.decorations.shadowWidth) : DecorWidth;
	int32_t inset = userDecor ? static_cast<uint32_t>(theme.decorations.borderRadius) : DecorInset;

	int32_t topOffet = -width - inset;
	int32_t topInset = 0;

	auto insetWidth = extent.width - inset * 2;
	auto insetHeight = extent.height - inset;
	auto cornerSize = width + inset;

	if (userDecor) {
		topOffet = -width;
		insetHeight = extent.height - inset * 2;
		topInset = inset;

		x = static_cast<uint32_t>(theme.decorations.shadowOffset.x);
		y = static_cast<uint32_t>(theme.decorations.shadowOffset.y);
	}

	// When window is docked to a tile or a monitor edge, its border matches that edge,
	// so the corner sprite's strip that protrudes past the window border must be hidden
	// (side sprites are hidden entirely by the WindowState handler).
	// Top corners extend above the header in fallback CSD mode, hence the DecorOffset term.
	const int32_t cropLeft = hasFlag(_info->state, WindowState::TiledLeft) ? width : 0;
	const int32_t cropRight = hasFlag(_info->state, WindowState::TiledRight) ? width : 0;
	const int32_t cropTop = hasFlag(_info->state, WindowState::TiledTop)
			? (userDecor ? width : width - DecorOffset)
			: 0;
	const int32_t cropBottom = hasFlag(_info->state, WindowState::TiledBottom) ? width : 0;

	for (auto &it : _decors) {
		switch (it->name) {
		case WaylandDecorationName::TopSide:
			it->setGeometry(x + inset, y + topOffet, insetWidth, width);
			break;
		case WaylandDecorationName::BottomSide:
			it->setGeometry(x + inset, y + extent.height, insetWidth, width);
			break;
		case WaylandDecorationName::LeftSide:
			it->setGeometry(x - width, y + topInset, width, insetHeight);
			break;
		case WaylandDecorationName::RightSide:
			it->setGeometry(x + extent.width, y + topInset, width, insetHeight);
			break;
		case WaylandDecorationName::TopLeftCorner:
			it->setGeometry(x - width, y + topOffet, cornerSize, cornerSize);
			it->setCrop(cropLeft, cropTop, 0, 0);
			break;
		case WaylandDecorationName::TopRightCorner:
			it->setGeometry(x + extent.width - inset, y + topOffet, cornerSize, cornerSize);
			it->setCrop(0, cropTop, cropRight, 0);
			break;
		case WaylandDecorationName::BottomLeftCorner:
			it->setGeometry(x - width, y + extent.height - inset, cornerSize, cornerSize);
			it->setCrop(cropLeft, 0, 0, cropBottom);
			break;
		case WaylandDecorationName::BottomRightCorner:
			it->setGeometry(x + extent.width - inset, y + extent.height - inset, cornerSize,
					cornerSize);
			it->setCrop(0, 0, cropRight, cropBottom);
			break;
		case WaylandDecorationName::HeaderLeft:
			it->setGeometry(x, y - inset - DecorOffset, inset, inset);
			break;
		case WaylandDecorationName::HeaderRight:
			it->setGeometry(x + extent.width - inset, y - inset - DecorOffset, inset, inset);
			break;
		case WaylandDecorationName::HeaderCenter:
			it->setGeometry(x + inset, y - inset - DecorOffset, extent.width - inset * 2, inset);
			break;
		case WaylandDecorationName::HeaderBottom:
			it->setGeometry(x, y - DecorOffset, extent.width, DecorOffset);
			break;
		case WaylandDecorationName::IconClose:
			it->setGeometry(x + extent.width - (IconSize + 4), y - IconSize, IconSize, IconSize);
			break;
		case WaylandDecorationName::IconMaximize:
			it->setGeometry(x + extent.width - (IconSize + 4) * 2, y - IconSize, IconSize,
					IconSize);
			break;
		case WaylandDecorationName::IconMinimize:
			it->setGeometry(x + extent.width - (IconSize + 4) * 3, y - IconSize, IconSize,
					IconSize);
			break;
		case WaylandDecorationName::RightShadowPanel:
			it->setGeometry(x + extent.width - inset, y + topInset, inset, insetHeight);
			break;
		case WaylandDecorationName::TopShadowPanel:
			it->setGeometry(x + inset, y - topOffet + inset, insetWidth, inset);
			break;
		case WaylandDecorationName::LeftShadowPanel:
			it->setGeometry(x + width + inset, y + topInset, inset, insetHeight);
			break;
		case WaylandDecorationName::BottomShadowPanel:
			it->setGeometry(x + inset, y + extent.height - inset, insetWidth, inset);
			break;
		default: break;
		}
	}

	bool surfacesDirty = false;
	for (auto &it : _decors) {
		if (it->commit()) {
			surfacesDirty = true;
		}
	}

	return surfacesDirty;
}

void WaylandWindow::updateSizeConstraints() {
	if (!_toplevel) {
		return;
	}

	// With client-side decorations the window geometry (what set_min/max_size constrain) is the
	// content plus the client-drawn title bar, so we add its insets and keep the decoration's own
	// minimum as a floor. xdg geometry is surface-logical, so no density scaling is needed.
	int32_t decorContentOffsetH = 0;
	int32_t decorMinW = 0;
	int32_t decorMinH = 0;
	if (!hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)
			&& !hasFlag(_info->state, WindowState::Fullscreen) && _serverDecor == nullptr) {
		decorContentOffsetH = DecorInset + DecorOffset;
		decorMinW = DecorWidth * 2 + IconSize * 3;
		decorMinH = DecorWidth * 2 + DecorOffset + DecorInset;
	}

	int32_t minW = decorMinW;
	int32_t minH = decorMinH;
	if (_info->minExtent.width != 0) {
		minW = sprt::max(minW, int32_t(_info->minExtent.width));
	}
	if (_info->minExtent.height != 0) {
		minH = sprt::max(minH, int32_t(_info->minExtent.height) + decorContentOffsetH);
	}
	xdg_toplevel_set_min_size(_toplevel, minW, minH);

	// 0 = no limit (xdg spec). Clamp above the minimum to avoid a min>max protocol error.
	int32_t maxW = 0;
	int32_t maxH = 0;
	if (_info->maxExtent.width != 0) {
		maxW = sprt::max(int32_t(_info->maxExtent.width), minW);
	}
	if (_info->maxExtent.height != 0) {
		maxH = sprt::max(int32_t(_info->maxExtent.height) + decorContentOffsetH, minH);
	}
	xdg_toplevel_set_max_size(_toplevel, maxW, maxH);
}

static uint32_t getPositionerAnchor(WindowAnchor anchor) {
	switch (anchor) {
	case WindowAnchor::None: return XDG_POSITIONER_ANCHOR_NONE;
	case WindowAnchor::Top: return XDG_POSITIONER_ANCHOR_TOP;
	case WindowAnchor::Bottom: return XDG_POSITIONER_ANCHOR_BOTTOM;
	case WindowAnchor::Left: return XDG_POSITIONER_ANCHOR_LEFT;
	case WindowAnchor::Right: return XDG_POSITIONER_ANCHOR_RIGHT;
	case WindowAnchor::TopLeft: return XDG_POSITIONER_ANCHOR_TOP_LEFT;
	case WindowAnchor::BottomLeft: return XDG_POSITIONER_ANCHOR_BOTTOM_LEFT;
	case WindowAnchor::TopRight: return XDG_POSITIONER_ANCHOR_TOP_RIGHT;
	case WindowAnchor::BottomRight: return XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT;
	}
	return XDG_POSITIONER_ANCHOR_NONE;
}

// WindowPlacement::gravity names the window corner that lands on the anchor point, so TopLeft
// means "expand down and right". xdg_positioner gravity names the side of the anchor point the
// surface is pushed towards, so its TOP_LEFT means "expand up and left" — the opposite corner.
static uint32_t getPositionerGravity(WindowAnchor gravity) {
	switch (gravity) {
	case WindowAnchor::None: return XDG_POSITIONER_GRAVITY_NONE;
	case WindowAnchor::Top: return XDG_POSITIONER_GRAVITY_BOTTOM;
	case WindowAnchor::Bottom: return XDG_POSITIONER_GRAVITY_TOP;
	case WindowAnchor::Left: return XDG_POSITIONER_GRAVITY_RIGHT;
	case WindowAnchor::Right: return XDG_POSITIONER_GRAVITY_LEFT;
	case WindowAnchor::TopLeft: return XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT;
	case WindowAnchor::BottomLeft: return XDG_POSITIONER_GRAVITY_TOP_RIGHT;
	case WindowAnchor::TopRight: return XDG_POSITIONER_GRAVITY_BOTTOM_LEFT;
	case WindowAnchor::BottomRight: return XDG_POSITIONER_GRAVITY_TOP_LEFT;
	}
	return XDG_POSITIONER_GRAVITY_NONE;
}

bool WaylandWindow::initPopup() {
	auto *parent = dynamic_cast<WaylandWindow *>(_controller->findWindow(_info->parent));
	if (!parent || !parent->_xdgSurface) {
		oslog::vperror(__SPRT_LOCATION, "WaylandWindow", "Popup parent is not available");
		return false;
	}

	_xdgSurface = xdg_wm_base_get_xdg_surface(_display->xdgWmBase, _surface);
	xdg_surface_add_listener(_xdgSurface, &s_XdgSurfaceListener, this);

	auto *positioner = xdg_wm_base_create_positioner(_display->xdgWmBase);
	const auto &placement = _info->placement;
	// xdg_positioner rejects a zero size or a zero-sized anchor rect with a protocol error, and a
	// protocol error takes down the whole wl_display connection. WindowPlacement allows a point
	// anchor (0x0 rect), so widen it to the smallest rect the protocol accepts.
	auto atLeastOne = [](int32_t v) { return v > 1 ? v : 1; };
	xdg_positioner_set_size(positioner, atLeastOne(int32_t(_currentExtent.width)),
			atLeastOne(int32_t(_currentExtent.height)));
	xdg_positioner_set_anchor_rect(positioner, placement.anchorRect.x, placement.anchorRect.y,
			atLeastOne(placement.anchorRect.width), atLeastOne(placement.anchorRect.height));
	xdg_positioner_set_anchor(positioner, getPositionerAnchor(placement.anchor));
	xdg_positioner_set_gravity(positioner, getPositionerGravity(placement.gravity));
	xdg_positioner_set_offset(positioner, placement.offset.x, placement.offset.y);

	uint32_t adjustment = XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_NONE;
	if (hasFlag(placement.adjustment, WindowPlacementAdjustment::SlideX)) {
		adjustment |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X;
	}
	if (hasFlag(placement.adjustment, WindowPlacementAdjustment::SlideY)) {
		adjustment |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y;
	}
	if (hasFlag(placement.adjustment, WindowPlacementAdjustment::FlipX)) {
		adjustment |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X;
	}
	if (hasFlag(placement.adjustment, WindowPlacementAdjustment::FlipY)) {
		adjustment |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y;
	}
	if (hasFlag(placement.adjustment, WindowPlacementAdjustment::ResizeX)) {
		adjustment |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X;
	}
	if (hasFlag(placement.adjustment, WindowPlacementAdjustment::ResizeY)) {
		adjustment |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y;
	}
	xdg_positioner_set_constraint_adjustment(positioner, adjustment);

	_popup = xdg_surface_get_popup(_xdgSurface, parent->_xdgSurface, positioner);
	xdg_positioner_destroy(positioner);
	if (!_popup) {
		return false;
	}
	xdg_popup_add_listener(_popup, &s_XdgPopupListener, this);

	if (_info->type == WindowType::Popup) {
		auto serial = _controller->getLastPointerSerial();
		if (serial != 0 && _display->seat) {
			xdg_popup_grab(_popup, _display->seat->seat, serial);
		}
	}

	xdg_surface_set_window_geometry(_xdgSurface, 0, 0, _currentExtent.width,
			_currentExtent.height);
	wl_surface_commit(_surface);
	_display->flush();
	return true;
}

// Transient-for on Wayland: xdg_toplevel.set_parent is the whole mechanism. There are no window
// roles here (no DIALOG, no UTILITY - see multi-window-research.md §2.1), so what a Dialog or a
// Utility palette IS gets emulated by "has a parent" plus the decoration choice; the compositor
// keeps such a toplevel above its parent, minimizes it with it, and keeps it out of the taskbar.
//
// Modality is NOT hinted here. That would be xdg_wm_dialog_v1, a staging protocol that is not in
// the scanner list; the engine's own _modalBlocks enforces the behaviour regardless, so its absence
// costs a compositor hint, not correctness.
void WaylandWindow::applyTransientParent() {
	if (_info->type == WindowType::Root || _info->parent.empty() || !_toplevel) {
		return;
	}
	auto parent = dynamic_cast<WaylandWindow *>(_controller->findWindow(_info->parent));
	if (parent && parent->_toplevel) {
		xdg_toplevel_set_parent(_toplevel, parent->_toplevel);
	}
}

bool WaylandWindow::initWithServerDecor() {
	// make server-size decorations
	_xdgSurface = xdg_wm_base_get_xdg_surface(_display->xdgWmBase, _surface);

	xdg_surface_add_listener(_xdgSurface, &s_XdgSurfaceListener, this);

	_toplevel = xdg_surface_get_toplevel(_xdgSurface);

	xdg_toplevel_set_title(_toplevel, _info->title.data());
	xdg_toplevel_set_app_id(_toplevel, _info->id.data());
	xdg_toplevel_add_listener(_toplevel, &s_XdgToplevelListener, this);
	applyTransientParent();
	updateSizeConstraints();

	_serverDecor = zxdg_decoration_manager_v1_get_toplevel_decoration(_display->decorationManager,
			_toplevel);
	zxdg_toplevel_decoration_v1_add_listener(_serverDecor, &s_serverDecorationListener, this);
	zxdg_toplevel_decoration_v1_set_mode(_serverDecor,
			ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	xdg_surface_set_window_geometry(_xdgSurface, 0, 0, _currentExtent.width, _currentExtent.height);
	wl_surface_commit(_surface);
	_display->flush();
	return true;
}

bool WaylandWindow::initWithAppDecor() {
	auto &theme = _controller->getThemeInfo();
	// application-based decorations
	_xdgSurface = xdg_wm_base_get_xdg_surface(_display->xdgWmBase, _surface);

	xdg_surface_add_listener(_xdgSurface, &s_XdgSurfaceListener, this);

	_toplevel = xdg_surface_get_toplevel(_xdgSurface);
	xdg_toplevel_set_title(_toplevel, _info->title.data());
	xdg_toplevel_set_app_id(_toplevel, _info->id.data());
	xdg_toplevel_add_listener(_toplevel, &s_XdgToplevelListener, this);
	applyTransientParent();
	updateSizeConstraints();

	if (!_display->viewporter) {
		oslog::vperror(__SPRT_LOCATION, "WaylandWindow",
				"Viewporter interface should be available for the app decorations");
		return false;
	}

	ShadowBuffers buf;

	/*{0xfa'fafa, 0x110, 0x4635'a5e7, "Grey50"},
	{0xf5'f5f5, 0x111, 0xf56a'7351, "Grey100"},
	{0xee'eeee, 0x112, 0xfb6c'bb5a, "Grey200"},
	{0xe0'e0e0, 0x113, 0xe16e'd123, "Grey300"},
	{0xbd'bdbd, 0x114, 0x6771'e2ac, "Grey400"},
	{0x9e'9e9e, 0x115, 0x6d74'2a95, "Grey500"},
	{0x75'7575, 0x116, 0x7376'729e, "Grey600"},
	{0x61'6161, 0x117, 0x7978'ba87, "Grey700"},
	{0x42'4242, 0x118, 0x5f53'c0c0, "Grey800"},
	{0x21'2121, 0x119, 0x6556'08c9, "Grey900"},
	*/

	WaylandDecorationInfo info{
		&buf,
		Color3B{0xf5, 0xf5, 0xf5},
		Color3B{0xe0, 0xe0, 0xe0},
		Color3B{0x21, 0x21, 0x21},
		Color3B{0x61, 0x61, 0x61},
		DecorWidth,
		DecorInset,
		24.0f / 256.0f,
		64.0f / 256.0f,
	};

	if (hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
		info.width = static_cast<uint32_t>(theme.decorations.shadowWidth);
		info.inset = static_cast<uint32_t>(theme.decorations.borderRadius);
		info.shadowMin = theme.decorations.shadowMinValue;
		info.shadowMax = theme.decorations.shadowMaxValue;
	}

	if (!allocateDecorations(_wayland, _display->shm->shm, info)) {
		oslog::vperror(__SPRT_LOCATION, "WaylandWindow",
				"Fail to allocate decorations shared buffers");
		return false;
	}

	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.top), move(buf.topActive),
			WaylandDecorationName::TopSide));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.bottom),
			move(buf.bottomActive), WaylandDecorationName::BottomSide));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.left), move(buf.leftActive),
			WaylandDecorationName::LeftSide));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.right), move(buf.rightActive),
			WaylandDecorationName::RightSide));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.topLeft),
			move(buf.topLeftActive), WaylandDecorationName::TopLeftCorner));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.topRight),
			move(buf.topRightActive), WaylandDecorationName::TopRightCorner));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.bottomLeft),
			move(buf.bottomLeftActive), WaylandDecorationName::BottomLeftCorner));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.bottomRight),
			move(buf.bottomRightActive), WaylandDecorationName::BottomRightCorner));

	if (!hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
		// Client-side, but not user-space decorations
		// Fallbk decorations
		auto hLeft = _decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.headerLeft),
				move(buf.headerLeftActive), WaylandDecorationName::HeaderLeft));
		hLeft->setAltBuffers(move(buf.headerDarkLeft), move(buf.headerDarkLeftActive));

		auto hRight =
				_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.headerRight),
						move(buf.headerRightActive), WaylandDecorationName::HeaderRight));
		hRight->setAltBuffers(move(buf.headerDarkRight), move(buf.headerDarkRightActive));

		auto hCenter = _decors.emplace_back(
				Rc<WaylandDecoration>::create(this, Rc<WaylandBuffer>(buf.headerLightCenter),
						Rc<WaylandBuffer>(buf.headerLightCenterActive),
						WaylandDecorationName::HeaderCenter));
		hCenter->setAltBuffers(Rc<WaylandBuffer>(buf.headerDarkCenter),
				Rc<WaylandBuffer>(buf.headerDarkCenterActive));

		auto hBottom = _decors.emplace_back(
				Rc<WaylandDecoration>::create(this, Rc<WaylandBuffer>(buf.headerLightCenter),
						Rc<WaylandBuffer>(buf.headerLightCenterActive),
						WaylandDecorationName::HeaderBottom));
		hBottom->setAltBuffers(Rc<WaylandBuffer>(buf.headerDarkCenter),
				Rc<WaylandBuffer>(buf.headerDarkCenterActive));

		_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.iconClose),
				move(buf.iconCloseActive), WaylandDecorationName::IconClose));
		_iconMaximized =
				_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.iconMaximize),
						move(buf.iconMaximizeActive), WaylandDecorationName::IconMaximize));
		_iconMaximized->setAltBuffers(move(buf.iconRestore), move(buf.iconRestoreActive));

		_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.iconMinimize),
				move(buf.iconMinimizeActive), WaylandDecorationName::IconMinimize));
	} else {
		// user-space extra panels

		_decors.emplace_back(Rc<WaylandDecoration>::create(this, Rc<WaylandBuffer>(buf.shadowPanel),
				Rc<WaylandBuffer>(buf.shadowPanelActive), WaylandDecorationName::RightShadowPanel));
		_decors.emplace_back(Rc<WaylandDecoration>::create(this, Rc<WaylandBuffer>(buf.shadowPanel),
				Rc<WaylandBuffer>(buf.shadowPanelActive), WaylandDecorationName::TopShadowPanel));
		_decors.emplace_back(Rc<WaylandDecoration>::create(this, Rc<WaylandBuffer>(buf.shadowPanel),
				Rc<WaylandBuffer>(buf.shadowPanelActive), WaylandDecorationName::LeftShadowPanel));
		_decors.emplace_back(Rc<WaylandDecoration>::create(this, Rc<WaylandBuffer>(buf.shadowPanel),
				Rc<WaylandBuffer>(buf.shadowPanelActive),
				WaylandDecorationName::BottomShadowPanel));
	}

	return true;
}

void WaylandWindow::cancelPointerEvents() {
	for (uint32_t i = 0; i < 64; ++i) {
		if (_buttons.test(i)) {
			InputEventData event({
				i,
				InputEventName::Cancel,
				{{
					InputMouseButton(i),
					InputModifier::None,
					NaN<float>,
					NaN<float>,
				}},
			});
			_pendingEvents.emplace_back(sprt::move(event));
		}
	}
	_buttons.reset();
	_buttonGripFlags = WindowLayerFlags::None;
}

} // namespace sprt::window
