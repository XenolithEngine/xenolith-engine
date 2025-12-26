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

#include "XLLinuxWaylandWindow.h"
#include "SPCore.h"
#include "SPGeometry.h"
#include "SPStatus.h"
#include "XLContextInfo.h"
#include "XLCorePresentationEngine.h"
#include "XLLinuxWaylandProtocol.h"
#include "XlCoreMonitorInfo.h"
#include "linux/XLLinuxContextController.h"
#include "linux/thirdparty/wayland-protocols/xdg-decoration.h"
#include "linux/thirdparty/wayland-protocols/xdg-shell.h"
#include "XLAppWindow.h"
#include "platform/XLContextNativeWindow.h"
#include <wayland-client-protocol.h>

#if MODULE_XENOLITH_BACKEND_VK
#include "XLVkPresentationEngine.h" // IWYU pragma: keep
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

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

static libdecor_frame_interface s_libdecorFrameInterface {
	.configure = [](libdecor_frame *frame, libdecor_configuration *configuration, void *data) {
		reinterpret_cast<WaylandWindow *>(data)->handleDecorConfigure(frame, configuration);
	},
	.close = [](libdecor_frame *frame, void *data) {
		reinterpret_cast<WaylandWindow *>(data)->handleDecorClose(frame);
	},
	.commit = [](libdecor_frame *frame, void *data) {
		reinterpret_cast<WaylandWindow *>(data)->handleDecorCommit(frame);
	},
	.dismiss_popup = [](libdecor_frame *frame, const char *seat_name, void *user_data) {

	},
	/*.bounds = [](struct libdecor_frame *frame, int width, int height, void *user_data) {

	}*/
};

static zxdg_toplevel_decoration_v1_listener s_serverDecorationListener {
	.configure = [](void *data, zxdg_toplevel_decoration_v1 *decor, uint32_t mode) {
		reinterpret_cast<WaylandWindow *>(data)->handleDecorConfigure(decor, mode);
	},
};

// clang-format on

WaylandWindow::~WaylandWindow() {
	if (_frameCallback) {
		_wayland->wl_callback_destroy(_frameCallback);
		_frameCallback = nullptr;
	}

	if (_serverDecor) {
		_wayland->zxdg_toplevel_decoration_v1_destroy(_serverDecor);
		_serverDecor = nullptr;
	}

	if (_clientDecor) {
		_wayland->libdecor_frame_unref(_clientDecor);
		_clientDecor = nullptr;
	}

	_iconMaximized = nullptr;
	_decors.clear();
	if (_toplevel) {
		_wayland->xdg_toplevel_destroy(_toplevel);
		_toplevel = nullptr;
	}
	if (_xdgSurface) {
		_wayland->xdg_surface_destroy(_xdgSurface);
		_xdgSurface = nullptr;
	}
	if (_surface) {
		_display->destroySurface(this);
		_surface = nullptr;
	}
	_display = nullptr;
}

WaylandWindow::WaylandWindow() { }

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
		_wayland->wl_surface_set_user_data(_surface, this);
		_wayland->wl_surface_add_listener(_surface, &s_WaylandSurfaceListener, this);

		if (hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
			if (!initWithAppDecor()) {
				return false;
			}
		} else if (hasFlag(_info->capabilities, WindowCapabilities::ServerSideDecorations)
				&& hasFlag(_info->flags, WindowCreationFlags::PreferServerSideDecoration)) {
			if (!initWithServerDecor()) {
				return false;
			}
		} else if (hasFlag(_info->capabilities, WindowCapabilities::NativeDecorations)
				&& hasFlag(_info->flags, WindowCreationFlags::PreferNativeDecoration)) {
			if (!initWithLibdecor()) {
				return false;
			}
		} else {
			if (!initWithAppDecor()) {
				return false;
			}
		}
	}

	uint32_t rate = 60'000;
	for (auto &it : _display->outputs) { rate = std::max(rate, uint32_t(it->currentMode.rate)); }
	_frameRate = rate;

	if (_clientDecor) {
		_wayland->libdecor_frame_map(_clientDecor);
	}

	return true;
}

void WaylandWindow::mapWindow() {
	_mapped = true;
	_display->flush();
}

void WaylandWindow::unmapWindow() {
	if (_frameCallback) {
		_wayland->wl_callback_destroy(_frameCallback);
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

void WaylandWindow::handleFramePresented(NotNull<core::PresentationFrame> frame) {
	auto &c = frame->getFrameConstraints();

	auto newExtent = Extent2(c.extent.width, c.extent.height);
	if (_density != 0.0f) {
		newExtent.width /= _density;
		newExtent.height /= _density;
	}

	bool dirty = _commitedExtent.width != newExtent.width
			|| _commitedExtent.height != newExtent.height || _configureSerial != maxOf<uint32_t>()
			|| _decorConfiguration;

	if (!dirty) {
		for (auto &it : _decors) {
			if (it->dirty) {
				dirty = true;
				break;
			}
		}
	}

	if (!_frameCallback) {
		_frameCallback = _wayland->wl_surface_frame(_surface);
		_wayland->wl_callback_add_listener(_frameCallback, &s_WaylandSurfaceFrameListener, this);
		_wayland->wl_surface_commit(_surface);
	}

	if (!dirty) {
		return;
	}

	StringStream stream;
	stream << "commit: " << newExtent.width << " " << newExtent.height << ";";

	_commitedExtent = newExtent;

	bool surfacesDirty = configureDecorations(_commitedExtent);

	if (_configureSerial != maxOf<uint32_t>()
			&& (_awaitingExtent == Extent2(0, 0) || _awaitingExtent == _commitedExtent)) {
		if (_toplevel) {
			// TODO: configure it the right way
			_wayland->xdg_toplevel_set_min_size(_toplevel, DecorWidth * 2 + IconSize * 3,
					DecorWidth * 2 + DecorOffset + DecorInset);
		}

		if (_xdgSurface) {
			// Capture decoration

			auto pos = IVec2(0, 0);
			auto extent = _commitedExtent;
			if (!hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)
					&& !hasFlag(_info->state, WindowState::Fullscreen) && _serverDecor == nullptr) {
				extent.height += DecorInset + DecorOffset;
				pos.y -= DecorInset + DecorOffset;
			}

			_wayland->xdg_surface_ack_configure(_xdgSurface, _configureSerial);

			_wayland->xdg_surface_set_window_geometry(_xdgSurface, pos.x, pos.y, extent.width,
					extent.height);

			stream << " surface: " << extent.width << " " << extent.height;
			stream << " configure: " << _configureSerial << ";";
		}
		_configureSerial = maxOf<uint32_t>();
	}

	if (_decorConfiguration) {
		auto state = _wayland->libdecor_state_new(_commitedExtent.width, _commitedExtent.height);
		_wayland->libdecor_frame_commit(_clientDecor, state, _decorConfiguration);
		_decorConfiguration = nullptr;
	}

	if (!_frameCallback) {
		_frameCallback = _wayland->wl_surface_frame(_surface);
		_wayland->wl_callback_add_listener(_frameCallback, &s_WaylandSurfaceFrameListener, this);
	}

	if (_toplevel && _awaitingExtent != Extent2(0, 0)) {
		if (_awaitingExtent != _commitedExtent) {
			auto w = _awaitingExtent.width;
			auto h = _awaitingExtent.height;
			_awaitingExtent = Extent2(0, 0);
			handleToplevelConfigure(_toplevel, w, h, nullptr);
		} else {
			_awaitingExtent = Extent2(0, 0);
		}
	}

	if (surfacesDirty) {
		emitAppFrame();
		stream << " Surfaces Dirty;";
	}

	_wayland->wl_surface_commit(_surface);

	XL_WAYLAND_LOG(stream.str());
}

core::FrameConstraints WaylandWindow::exportConstraints() const {
	auto ret = NativeWindow::exportConstraints();

	ret.extent = _currentExtent;
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

core::SurfaceInfo WaylandWindow::getSurfaceOptions(const core::Device &dev,
		NotNull<core::Surface> surface) const {
	auto info = NativeWindow::getSurfaceOptions(dev, surface);
	info.currentExtent = _currentExtent;
	if (_density != 0.0f) {
		info.currentExtent.width *= _density;
		info.currentExtent.height *= _density;
	}
	return sp::move(info);
}

Extent2 WaylandWindow::getExtent() const { return _currentExtent; }

Rc<core::Surface> WaylandWindow::makeSurface(NotNull<core::Instance> cinstance) {
#if MODULE_XENOLITH_BACKEND_VK
	if (cinstance->getApi() != core::InstanceApi::Vulkan) {
		return nullptr;
	}

	auto instance = static_cast<vk::Instance *>(cinstance.get());

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkWaylandSurfaceCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR, nullptr,
		0, _display->display, _surface};
	if (instance->vkCreateWaylandSurfaceKHR(instance->getInstance(), &createInfo, nullptr, &surface)
			!= VK_SUCCESS) {
		return nullptr;
	}
	return Rc<vk::Surface>::create(instance, surface, this);
#else
	log::source().error("XcbWindow", "No available GAPI found for a surface");
	return nullptr;
#endif
}

core::PresentationOptions WaylandWindow::getPreferredOptions() const {
	core::PresentationOptions opts;
	opts.followDisplayLinkBarrier = true;
	return opts;
}

void WaylandWindow::handleSurfaceEnter(wl_surface *surface, wl_output *output) {
	if (!_wayland->ownsProxy(output)) {
		return;
	}

	auto out = (WaylandOutput *)_wayland->wl_output_get_user_data(output);
	if (out) {
		_activeOutputs.emplace(out);
		XL_WAYLAND_LOG("handleSurfaceEnter: output: ", out->description());
	}

	uint32_t rate = 60'000;
	for (auto &it : _activeOutputs) { rate = std::max(rate, uint32_t(it->currentMode.rate)); }

	if (rate != _frameRate) {
		_frameRate = rate;
		_controller->notifyWindowConstraintsChanged(this, core::UpdateConstraintsFlags::None);
	}
}

void WaylandWindow::handleSurfaceLeave(wl_surface *surface, wl_output *output) {
	if (!_wayland->ownsProxy(output)) {
		return;
	}

	auto out = (WaylandOutput *)_wayland->wl_output_get_user_data(output);
	if (out) {
		_activeOutputs.erase(out);
		XL_WAYLAND_LOG("handleSurfaceLeave: output: ", out->description());
	}

	uint32_t rate = 60'000;
	for (auto &it : _activeOutputs) { rate = std::max(rate, uint32_t(it->currentMode.rate)); }

	if (rate != _frameRate) {
		_frameRate = rate;
		_controller->notifyWindowConstraintsChanged(this, core::UpdateConstraintsFlags::None);
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
				_wayland->xdg_surface_set_window_geometry(_xdgSurface, 0, -headerHeight,
						_currentExtent.width, _currentExtent.height + headerHeight);
			} else if (_xdgSurface) {
				_wayland->xdg_surface_set_window_geometry(_xdgSurface, 0, 0, _currentExtent.width,
						_currentExtent.height);
			}
		}
	}
	_configureSerial = serial;
}

void WaylandWindow::handleToplevelConfigure(xdg_toplevel *xdg_toplevel, int32_t width,
		int32_t height, wl_array *states) {

	StringStream stream;
	stream << "handleToplevelConfigure" << (!states ? "(syntetic)" : "") << " width: " << width
		   << ", height: " << height << ";";

	bool hasModeSwitch = false;
	bool unfullscreen = false;

	if (states) {
		WindowState mask = WindowState::Maximized | WindowState::Fullscreen | WindowState::Resizing
				| WindowState::Focused | WindowState::Minimized | WindowState::TilingMask;
		WindowState state = WindowState::None;

		for (uint32_t *it = (uint32_t *)states->data;
				(const char *)it < ((const char *)states->data + states->size); ++it) {
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

		stream << state << " ";

		updateState(_configureSerial, (_info->state & ~mask) | state);
	}

	if (unfullscreen && !_activeOutputs.empty()) {
		auto extent = (*_activeOutputs.begin())->currentMode.size;
		if (extent.width == uint32_t(width) && extent.height == uint32_t(height)) {
			log::error("Wayland", "Unfullscreen failed, restore saved params");
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

	if (width && height && _commitedExtent.width && _commitedExtent.height) {
		if (!_clientDecor && !_serverDecor && !hasFlag(_info->state, WindowState::Fullscreen)
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
						core::UpdateConstraintsFlags::DeprecateSwapchain);

				stream << "surface: " << _currentExtent.width << " " << _currentExtent.height;
			} else {
				_awaitingExtent =
						Extent2(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
			}
		}
	}

	XL_WAYLAND_LOG(stream.str());

	emitAppFrame();

	if (!_started) {
		_controller->notifyWindowCreated(this);
		_started = true;
	}
}

void WaylandWindow::handleToplevelClose(xdg_toplevel *xdg_toplevel) {
	XL_WAYLAND_LOG("handleToplevelClose");
	_controller->notifyWindowClosed(this);
}

void WaylandWindow::handleToplevelBounds(xdg_toplevel *xdg_toplevel, int32_t width,
		int32_t height) {
	XL_WAYLAND_LOG("handleToplevelBounds: width: ", width, ", height: ", height);
}

void WaylandWindow::handleToplevelCapabilities(xdg_toplevel *xdg_toplevel, wl_array *capabilities) {
	WindowState capState =
			WindowState::AllowedClose | WindowState::AllowedMove | WindowState::AllowedResize;

	for (uint32_t *it = (uint32_t *)capabilities->data;
			(const char *)it < ((const char *)capabilities->data + capabilities->size); ++it) {
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
		_wayland->wl_callback_destroy(frame);
	} else {
		_wayland->wl_callback_destroy(frame);
		_frameCallback = nullptr;
		if (_appWindow) {
			_appWindow->update(core::PresentationUpdateFlags::DisplayLink);
		}
	}
}

void WaylandWindow::handleDecorConfigure(libdecor_frame *frame,
		libdecor_configuration *configuration) {

	int width = 0, height = 0;
	_wayland->libdecor_configuration_get_content_size(configuration, frame, &width, &height);

	StringStream stream;
	stream << "handleDecorConfigure: width: " << width << ", height: " << height << ";";

	libdecor_window_state wstate = LIBDECOR_WINDOW_STATE_NONE;
	if (_wayland->libdecor_configuration_get_window_state(configuration, &wstate)) {
		WindowState state = WindowState::None;

		if (hasFlag(wstate, LIBDECOR_WINDOW_STATE_ACTIVE)) {
			state |= WindowState::Focused;
		}
		if (hasFlag(wstate, LIBDECOR_WINDOW_STATE_MAXIMIZED)) {
			state |= WindowState::Maximized;
		}
		if (hasFlag(wstate, LIBDECOR_WINDOW_STATE_FULLSCREEN)) {
			state |= WindowState::Fullscreen;
		}
		if (hasFlag(wstate, LIBDECOR_WINDOW_STATE_TILED_LEFT)) {
			state |= WindowState::TiledLeft;
		}
		if (hasFlag(wstate, LIBDECOR_WINDOW_STATE_TILED_RIGHT)) {
			state |= WindowState::TiledRight;
		}
		if (hasFlag(wstate, LIBDECOR_WINDOW_STATE_TILED_TOP)) {
			state |= WindowState::TiledTop;
		}
		if (hasFlag(wstate, LIBDECOR_WINDOW_STATE_TILED_BOTTOM)) {
			state |= WindowState::TiledBottom;
		}
		if (hasFlag(wstate, LIBDECOR_WINDOW_STATE_SUSPENDED)) {
			state |= WindowState::Minimized;
		}

		if (state != _info->state) {
			updateState(0, state);
		}
	}

	_decorConfiguration = configuration;

	if (!_started) {
		_controller->notifyWindowCreated(this);
		_started = true;
	}

	if (width && height) {
		if (_currentExtent.width != static_cast<uint32_t>(width)
				|| _currentExtent.height != static_cast<uint32_t>(height)) {
			_currentExtent.width = static_cast<uint32_t>(width);
			_currentExtent.height = static_cast<uint32_t>(height);
			_controller->notifyWindowConstraintsChanged(this,
					core::UpdateConstraintsFlags::DeprecateSwapchain);

			stream << "surface: " << _currentExtent.width << " " << _currentExtent.height;
		}
	}

	XL_WAYLAND_LOG(stream.str());
}

void WaylandWindow::handleDecorConfigure(zxdg_toplevel_decoration_v1 *decor, uint32_t mode) {
	XL_WAYLAND_LOG("handleDecorConfigure:", mode);
}

void WaylandWindow::handleDecorClose(libdecor_frame *frame) {
	XL_WAYLAND_LOG("handleDecorClose");
	_controller->notifyWindowClosed(this);
}

void WaylandWindow::handleDecorCommit(libdecor_frame *frame) {
	XL_WAYLAND_LOG("handleDecorCommit");
	_configureSerial++;
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

		core::InputEventData event({
			maxOf<uint32_t>(),
			core::InputEventName::MouseMove,
			{{
				core::InputMouseButton::None,
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
		if (_buttons.test(toInt(core::InputMouseButton::MouseLeft)) && _buttons.count() == 1) {
			auto grip = _buttonGripFlags;
			switch (grip) {
			case WindowLayerFlags::MoveGrip:
				_wayland->xdg_toplevel_move(_toplevel, _display->seat->seat, _buttonGripSerial);
				break;
			case WindowLayerFlags::ResizeTopLeftGrip:
			case WindowLayerFlags::ResizeTopGrip:
			case WindowLayerFlags::ResizeTopRightGrip:
			case WindowLayerFlags::ResizeRightGrip:
			case WindowLayerFlags::ResizeBottomRightGrip:
			case WindowLayerFlags::ResizeBottomGrip:
			case WindowLayerFlags::ResizeBottomLeftGrip:
			case WindowLayerFlags::ResizeLeftGrip:
				_wayland->xdg_toplevel_resize(_toplevel, _display->seat->seat, _buttonGripSerial,
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

		_pendingEvents.emplace_back(core::InputEventData({
			maxOf<uint32_t>(),
			core::InputEventName::MouseMove,
			{{
				core::InputMouseButton::None,
				_activeModifiers,
				float(_surfaceX),
				float(_surfaceY),
			}},
		}));
	}
}

static core::InputMouseButton getWaylandButton(uint32_t button) {
	switch (button) {
	case BTN_LEFT: return core::InputMouseButton::MouseLeft; break;
	case BTN_RIGHT: return core::InputMouseButton::MouseRight; break;
	case BTN_MIDDLE: return core::InputMouseButton::MouseMiddle; break;
	default:
		return core::InputMouseButton(toInt(core::InputMouseButton::Mouse8) + (button - 0x113));
		break;
	}
	return core::InputMouseButton::None;
}

void WaylandWindow::handlePointerButton(uint32_t serial, uint32_t time, uint32_t button,
		uint32_t state) {
	if (!_pointerInit) {
		return;
	}

	auto btn = getWaylandButton(button);

	if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
		if (btn == core::InputMouseButton::MouseLeft) {
			// Capture current grip flags
			if (hasFlag(_currentLayerFlags, WindowLayerFlags::WindowMenuLeft)) {
				cancelPointerEvents();
				_wayland->xdg_toplevel_show_window_menu(_toplevel, _display->seat->seat, serial,
						wl_fixed_to_int(_surfaceFX), wl_fixed_to_int(_surfaceFY));
				return;
			} else {
				_buttonGripSerial = serial;
				_buttonGripFlags = _gripFlags;
			}
		} else if (btn == core::InputMouseButton::MouseRight) {
			if (hasFlag(_currentLayerFlags, WindowLayerFlags::WindowMenuRight)) {
				cancelPointerEvents();
				_wayland->xdg_toplevel_show_window_menu(_toplevel, _display->seat->seat, serial,
						wl_fixed_to_int(_surfaceFX), wl_fixed_to_int(_surfaceFY));
				return;
			}
		}
	} else {
		if (btn == core::InputMouseButton::MouseLeft) {
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
			_pendingEvents.emplace_back(core::InputEventData({
				button,
				core::InputEventName::Begin,
				{{
					btn,
					_activeModifiers,
					float(_surfaceX),
					float(_surfaceY),
				}},
			}));
		} else if (state == WL_POINTER_BUTTON_STATE_RELEASED && _buttons.test(toInt(btn))) {
			_buttons.reset(toInt(btn));
			_pendingEvents.emplace_back(core::InputEventData({
				button,
				core::InputEventName::End,
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
		core::InputMouseButton btn = core::InputMouseButton::None;
		switch (axis) {
		case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
			if (val < 0) {
				btn = core::InputMouseButton::MouseScrollUp;
			} else {
				btn = core::InputMouseButton::MouseScrollDown;
			}
			break;
		case WL_POINTER_AXIS_VERTICAL_SCROLL:
			if (val > 0) {
				btn = core::InputMouseButton::MouseScrollRight;
			} else {
				btn = core::InputMouseButton::MouseScrollLeft;
			}
			break;
		}

		core::InputEventData event({
			toInt(btn),
			core::InputEventName::Scroll,
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

	core::InputMouseButton axisBtn = core::InputMouseButton::None;
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

			_pendingEvents.emplace_back(core::InputEventData({
				maxOf<uint32_t>(),
				core::InputEventName::MouseMove,
				{{
					core::InputMouseButton::None,
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
					axisBtn = core::InputMouseButton::MouseScrollUp;
				} else {
					axisBtn = core::InputMouseButton::MouseScrollDown;
				}
				break;
			case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
				hasAxis = true;
				axisX += it.axis.value;
				if (it.axis.value > 0) {
					axisBtn = core::InputMouseButton::MouseScrollRight;
				} else {
					axisBtn = core::InputMouseButton::MouseScrollLeft;
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

		_pendingEvents.emplace_back(core::InputEventData({
			maxOf<uint32_t>(),
			core::InputEventName::MouseMove,
			{{
				core::InputMouseButton::None,
				_activeModifiers,
				float(_surfaceX),
				float(_surfaceY),
			}},
		}));
	}

	if (hasAxis) {
		auto &event = _pendingEvents.emplace_back(core::InputEventData({
			axisSource,
			core::InputEventName::Scroll,
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
				_pendingEvents.emplace_back(core::InputEventData({
					it.button.button,
					core::InputEventName::Begin,
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
				_pendingEvents.emplace_back(core::InputEventData({
					it.button.button,
					core::InputEventName::End,
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
		core::InputEventData event({
			n,
			core::InputEventName::KeyCanceled,
			{{
				core::InputMouseButton::None,
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
	core::InputEventData event({
		time,
		(state == WL_KEYBOARD_KEY_STATE_PRESSED) ? core::InputEventName::KeyPressed
												 : core::InputEventName::KeyReleased,
		{{
			core::InputMouseButton::None,
			_activeModifiers,
			float(_surfaceX),
			float(_surfaceY),
		}},
	});

	event.key.keycode = _display->seat->translateKey(scancode);
	event.key.compose = core::InputKeyComposeState::Nothing;
	event.key.keysym = scancode;
	event.key.keychar = 0;

	const xkb_keysym_t *keysyms = nullptr;
	const xkb_keycode_t keycode = scancode + 8;

	if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		char32_t codepoint = 0;
		if (_display->xkb && isTextInputEnabled()) {
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
								   sp::platform::clock(ClockType::Monotonic), false})
						  .first;

		if (_display->xkb
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

	_pendingEvents.emplace_back(sp::move(event));
}

void WaylandWindow::handleKeyModifiers(uint32_t depressed, uint32_t latched, uint32_t locked) {
	if (!_display->seat->state) {
		return;
	}

	_activeModifiers = core::InputModifier::None;
	if (_display->xkb->xkb_state_mod_index_is_active(_display->seat->state,
				_display->seat->keyState.controlIndex, XKB_STATE_MODS_EFFECTIVE)
			== 1) {
		_activeModifiers |= core::InputModifier::Ctrl;
	}

	if (_display->xkb->xkb_state_mod_index_is_active(_display->seat->state,
				_display->seat->keyState.altIndex, XKB_STATE_MODS_EFFECTIVE)
			== 1) {
		_activeModifiers |= core::InputModifier::Alt;
	}

	if (_display->xkb->xkb_state_mod_index_is_active(_display->seat->state,
				_display->seat->keyState.shiftIndex, XKB_STATE_MODS_EFFECTIVE)
			== 1) {
		_activeModifiers |= core::InputModifier::Shift;
	}

	if (_display->xkb->xkb_state_mod_index_is_active(_display->seat->state,
				_display->seat->keyState.superIndex, XKB_STATE_MODS_EFFECTIVE)
			== 1) {
		_activeModifiers |= core::InputModifier::Mod4;
	}

	if (_display->xkb->xkb_state_mod_index_is_active(_display->seat->state,
				_display->seat->keyState.capsLockIndex, XKB_STATE_MODS_EFFECTIVE)
			== 1) {
		_activeModifiers |= core::InputModifier::CapsLock;
	}

	if (_display->xkb->xkb_state_mod_index_is_active(_display->seat->state,
				_display->seat->keyState.numLockIndex, XKB_STATE_MODS_EFFECTIVE)
			== 1) {
		_activeModifiers |= core::InputModifier::NumLock;
	}
}

void WaylandWindow::handleKeyRepeat() {
	Vector<core::InputEventData> events;
	auto spawnRepeatEvent = [&, this](const KeyData &it) {
		core::InputEventData event({
			uint32_t(events.size() + 1),
			core::InputEventName::KeyRepeated,
			{{
				core::InputMouseButton::None,
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
	auto t = sp::platform::clock(ClockType::Monotonic);
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
			_wayland->xdg_toplevel_set_maximized(_toplevel);
			_iconMaximized->setAlternative(true);
		} else {
			_wayland->xdg_toplevel_unset_maximized(_toplevel);
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
	case WaylandDecorationName::IconMinimize:
		_wayland->xdg_toplevel_set_minimized(_toplevel);
		return;
	case WaylandDecorationName::HeaderCenter:
	case WaylandDecorationName::HeaderBottom:
	case WaylandDecorationName::HeaderLeft:
		if (btn == BTN_RIGHT) {
			_wayland->xdg_toplevel_show_window_menu(_toplevel, _display->seat->seat, serial,
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
		_wayland->xdg_toplevel_resize(_toplevel, _display->seat->seat, serial, edges);
		emitAppFrame();
	} else {
		_wayland->xdg_toplevel_move(_toplevel, _display->seat->seat, serial);
		emitAppFrame();
	}
}

void WaylandWindow::setPreferredScale(int32_t scale) {
	if (_density != float(scale)) {
		_density = float(scale);
		_wayland->wl_surface_set_buffer_scale(_surface, scale);
		_controller->notifyWindowConstraintsChanged(this, core::UpdateConstraintsFlags::None);
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
		_wayland->wl_surface_commit(_surface);
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
			_wayland->xdg_toplevel_set_minimized(_toplevel);
			return true;
		}
		break;
	case WindowState::Maximized:
		if (_toplevel) {
			_wayland->xdg_toplevel_set_maximized(_toplevel);
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
			_wayland->xdg_toplevel_unset_maximized(_toplevel);
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
				_wayland->xdg_toplevel_unset_fullscreen(_toplevel);
			} else if (_clientDecor) {
				_wayland->libdecor_frame_unset_fullscreen(_clientDecor);
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
					_wayland->xdg_toplevel_set_fullscreen(_toplevel, current->output);
				} else if (_clientDecor) {
					_wayland->libdecor_frame_set_fullscreen(_clientDecor, current->output);
				}

				// replace info with real data
				info.mode = core::ModeInfo{current->currentMode.size.width,
					current->currentMode.size.height, current->currentMode.rate};

				auto cfg = _controller->getDisplayConfigManager()->getCurrentConfig();
				for (auto &it : cfg->monitors) {
					if (it.id.name == current->name) {
						info.id = it.id;
						info.mode = it.getCurrent().mode;
						break;
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
				_wayland->xdg_toplevel_unset_fullscreen(_toplevel);
			} else if (_clientDecor) {
				_wayland->libdecor_frame_unset_fullscreen(_clientDecor);
			}
		}
		for (auto &it : _display->outputs) {
			if (it->name == info.id.name) {
				if (_toplevel) {
					_wayland->xdg_toplevel_set_fullscreen(_toplevel, it->output);
				} else if (_clientDecor) {
					_wayland->libdecor_frame_set_fullscreen(_clientDecor, it->output);
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
			break;
		case WaylandDecorationName::TopRightCorner:
			it->setGeometry(x + extent.width - inset, y + topOffet, cornerSize, cornerSize);
			break;
		case WaylandDecorationName::BottomLeftCorner:
			it->setGeometry(x - width, y + extent.height - inset, cornerSize, cornerSize);
			break;
		case WaylandDecorationName::BottomRightCorner:
			it->setGeometry(x + extent.width - inset, y + extent.height - inset, cornerSize,
					cornerSize);
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

bool WaylandWindow::initWithServerDecor() {
	// make server-size decorations
	_xdgSurface = _wayland->xdg_wm_base_get_xdg_surface(_display->xdgWmBase, _surface);

	_wayland->xdg_surface_add_listener(_xdgSurface, &s_XdgSurfaceListener, this);

	_toplevel = _wayland->xdg_surface_get_toplevel(_xdgSurface);

	_wayland->xdg_toplevel_set_title(_toplevel, _info->title.data());
	_wayland->xdg_toplevel_set_app_id(_toplevel, _info->id.data());
	_wayland->xdg_toplevel_add_listener(_toplevel, &s_XdgToplevelListener, this);

	_serverDecor = _wayland->zxdg_decoration_manager_v1_get_toplevel_decoration(
			_display->decorationManager, _toplevel);
	_wayland->zxdg_toplevel_decoration_v1_add_listener(_serverDecor, &s_serverDecorationListener,
			this);
	_wayland->zxdg_toplevel_decoration_v1_set_mode(_serverDecor,
			ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	_wayland->xdg_surface_set_window_geometry(_xdgSurface, 0, 0, _currentExtent.width,
			_currentExtent.height);
	_wayland->wl_surface_commit(_surface);
	_display->flush();
	return true;
}

bool WaylandWindow::initWithLibdecor() {
	// libdecor decorations
	_clientDecor =
			_wayland->libdecor_decorate(_display->decor, _surface, &s_libdecorFrameInterface, this);

	_wayland->libdecor_frame_set_title(_clientDecor, _info->title.data());
	_wayland->libdecor_frame_set_app_id(_clientDecor, _info->id.data());
	return true;
}

bool WaylandWindow::initWithAppDecor() {
	auto &theme = _controller->getThemeInfo();
	// application-based decorations
	_xdgSurface = _wayland->xdg_wm_base_get_xdg_surface(_display->xdgWmBase, _surface);

	_wayland->xdg_surface_add_listener(_xdgSurface, &s_XdgSurfaceListener, this);

	_toplevel = _wayland->xdg_surface_get_toplevel(_xdgSurface);
	_wayland->xdg_toplevel_set_title(_toplevel, _info->title.data());
	_wayland->xdg_toplevel_set_app_id(_toplevel, _info->id.data());
	_wayland->xdg_toplevel_add_listener(_toplevel, &s_XdgToplevelListener, this);

	if (!_display->viewporter) {
		log::source().error("WaylandWindow",
				"Viewporter interface should be available for the app decorations");
		return false;
	}

	ShadowBuffers buf;

	DecorationInfo info{
		&buf,
		Color::Grey_100,
		Color::Grey_300,
		Color::Grey_900,
		Color::Grey_700,
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
		log::source().error("WaylandWindow", "Fail to allocate decorations shared buffers");
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
	}

	return true;
}

void WaylandWindow::cancelPointerEvents() {
	for (uint32_t i = 0; i < 64; ++i) {
		if (_buttons.test(i)) {
			core::InputEventData event({
				i,
				core::InputEventName::Cancel,
				{{
					core::InputMouseButton(i),
					core::InputModifier::None,
					nan(),
					nan(),
				}},
			});
			_pendingEvents.emplace_back(sp::move(event));
		}
	}
	_buttons.reset();
	_buttonGripFlags = WindowLayerFlags::None;
}

} // namespace stappler::xenolith::platform
