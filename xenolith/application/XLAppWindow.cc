/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#include "XLAppWindow.h"
#include "SPCore.h"
#include "XLAppThread.h"
#include "XLContextInfo.h"
#include "XLCoreEnum.h"
#include "XLCorePresentationEngine.h"
#include "XLCoreDevice.h"
#include "XlCoreMonitorInfo.h"
#include "director/XLDirector.h"
#include "input/XLInputDispatcher.h"
#include "XLServerAppThread.h"

#if MODULE_XENOLITH_BACKEND_VK
#include "XLVkInstance.h"
#include "XLVkSwapchain.h"
#include "XLVkHeadlessPresentation.h"
#endif

#if MODULE_XENOLITH_BACKEND_WEBGPU
#include "XLWgpuInstance.h"
#include "XLWgpuPresentation.h"
#endif

#if MODULE_XENOLITH_BACKEND_MTL
#include "XLMtlInstance.h"
#include "XLMtlPresentation.h"
#endif

#if MODULE_XENOLITH_BACKEND_SOFT
#include "XLSoftInstance.h"
#include "XLSoftPresentation.h"
#include "XLSoftHeadlessPresentation.h"
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith {

XL_DECLARE_EVENT_CLASS(AppWindow, onWindowState);

AppWindow::~AppWindow() { log::source().info("AppWindow", "~AppWindow"); }

bool AppWindow::init(NotNull<Context> ctx, NotNull<ServerAppThread> app, NotNull<NativeWindow> w) {
	_context = ctx;
	_application = app;
	_window = w;
	_capabilities = _window->getInfo()->capabilities;
	_windowId = StringView(_window->getInfo()->id).str<String>();

	// Take the application's payload off the WindowInfo here, on the context thread. Taking is a
	// Rc move, which is legal on any thread; leaving it in place is not, because the WindowInfo
	// is destroyed here and the payload holds app-thread objects. It goes back to the app thread
	// in end().
	if (auto data = _window->takeAppData()) {
		_sceneInfo = static_cast<WindowSceneInfo *>(data.get());
		if (_sceneInfo) {
			_sceneInfo->setWindow(this);
		} else {
			log::source().error("AppWindow", "WindowInfo::appData is not a WindowSceneInfo");
		}
	}

	_presentationEngine = static_cast<core::Loop *>(_context->getGlLoop())
								  ->makePresentationEngine(this, w->getPreferredOptions());

	return _presentationEngine != nullptr;
}

void AppWindow::runWithQueue(const Rc<core::Queue> &queue) {
	// attachRenderQueue defers this onto the context thread, and an auxiliary window can be
	// dismissed before it gets there. Starting the engine on a window that is winding down
	// strands a swapchain that never presents and poisons the present path for later windows.
	if (!_window || !_presentationEngine || _inCloseRequest) {
		log::source().debug("WindowDiag", "runWithQueue skipped (dismissed) id=", _windowId);
		return;
	}

	if (!_presentationEngine->isRunning()) {
		// Every non-Root window maps before its first present; only Root defers.
		//
		// For Popup/Tooltip that has always been about behaviour: hit-testing and the dismiss
		// monitors need a placed window from the moment the menu exists. Dialog and Utility are here
		// for a harder reason - deferring their map is actively broken. With TWO decorated auxiliary
		// windows open at once, tearing either of them down poisons the present path (the surviving
		// windows start failing MaterialSwapchainPass and the process goes down). One such window
		// alone is fine, and four override-redirect popups are fine; it takes two windows on the
		// deferred path. Mapping them immediately avoids it entirely.
		//
		// NOTE: that is a containment, not a root-cause fix. The defect lives in the deferred path
		// itself (the map hangs off handleFrameReady, because scheduleNextImage is dropped without a
		// word while the swapchain does not exist yet - it is built asynchronously from the first WM
		// configure). Root still uses it, and is safe only because there is normally one of them;
		// the cost of deferring for Root is what it buys - no unpainted window at startup.
		const auto type = _window->getInfo()->type;
		if (type != sprt::window::WindowType::Root) {
			_window->mapWindow();
		} else {
			_mapOnFirstFrame = true;
		}

		_presentationEngine->run();
		_presentationEngine->scheduleNextImage(nullptr);
	}
}

void AppWindow::run() {
	auto c = _presentationEngine->getFrameConstraints();
	_application->performOnAppThread([this, c]() mutable {
		// _client is set by Director::init (during makeDirector below), before the initial scene
		// runs, so queue announcements reach the client.
		_director = _application->handleAppWindowCreated(this, c);
	}, this);
}

void AppWindow::update(core::PresentationUpdateFlags flags) {
	if (_presentationEngine) {
		_presentationEngine->update(flags);
	}
}

void AppWindow::releaseSceneInfo() {
	if (!_sceneInfo) {
		return;
	}
	// Destroyed on the app thread, never here: it holds scene-graph objects captured by the
	// opener. `this` is not captured — the window may be gone by the time this runs.
	_application->performOnAppThread([sceneInfo = move(_sceneInfo)]() mutable {
		sceneInfo->setWindow(nullptr);
		sceneInfo->fireClose();
		sceneInfo = nullptr;
	}, _application);
	_sceneInfo = nullptr;
}

void AppWindow::end() {
	if (!_presentationEngine) {
		// The engine never came up (or end() ran twice). The payload still has to go home: this is
		// the only path where a window can die without ever reaching the app thread below.
		releaseSceneInfo();
		synchronizeClose();
		return;
	}

	auto engine = move(_presentationEngine);
	_presentationEngine = nullptr;

	if (engine) {
		engine->end();
	}

	// Preserve final window capabilities
	// On Android, through capabilities we know if Director should be preserved
	if (_window) {
		_capabilities = _window->getInfo()->capabilities;
	}

	_application->performOnAppThread(
			[this, engine = move(engine), sceneInfo = move(_sceneInfo)]() mutable {
		_client = nullptr; // the Director (client endpoint) is being destroyed below
		_application->handleAppWindowDestroyed(this, sp::move(_director));
		if (sceneInfo) {
			// Every teardown route reaches here — own close, parent cascade, WM-side dismiss — so
			// this is the one place the opener's callback has to fire, and it fires with no id
			// lookup and no way to be missed.
			sceneInfo->setWindow(nullptr);
			sceneInfo->fireClose();
			sceneInfo = nullptr;
		}
		_context->performOnThread([this, engine = move(engine)]() mutable {
			if (_syncClose) {
				engine->synchronizeClose();
			}
			engine = nullptr;
			synchronizeClose();
		}, this);
		_director = nullptr;
	}, this);
}

void AppWindow::close(bool graceful) {
	if (_inCloseRequest) {
		return;
	}

	if (_context->getLooper()->isOnThisThread() && _presentationEngine
			&& _presentationEngine->getOptions().syncConstraintsUpdate) {
		_syncClose = true;
	}

	_inCloseRequest = true;

	// Auxiliary windows tear down through the very same path as every other window: the EndOfLife
	// handshake below is what makes the app thread let go before the engine goes away.
	_context->performOnThread([this, w = Rc<NativeWindow>(_window), graceful] {
		if (w) {
			if (!w->close()) {
				_application->performOnAppThread([this] {
					_context->performOnThread([this] { synchronizeClose(); }, this);
					_inCloseRequest = false;
				}, this);
				return;
			}
		}

		if (!graceful) {
			end();
		} else if (_presentationEngine) {
			_presentationEngine->updateConstraints(core::UpdateConstraintsFlags::EndOfLife,
					[this, w = Rc<NativeWindow>(w)](bool) {
				end();
				_window = nullptr;
			});
		} else {
			end();
			_window = nullptr;
		}
	}, this, true);

	if (_syncClose) {
		_context->getLooper()->run();
		_syncClose = true;
		_window = nullptr;
	}
}

void AppWindow::hide() {
	// Auxiliary windows are not pooled, so a dismiss is just a graceful close.
	close(true);
}

void AppWindow::setContentExtent(Extent2 extent) {
	_context->performOnThread([this, extent] {
		if (_window && _window->setContentExtent(extent)) {
			if (_presentationEngine) {
				_presentationEngine->updateConstraints(core::UpdateConstraintsFlags::WindowResized);
			}
		}
	}, this);
}

void AppWindow::handleInputEvents(Vector<InputEventData> &&events) {
	if (!_presentationEngine) {
		return;
	}

	if (_context->getLooper()->isOnThisThread()) {
		for (auto &it : events) {
			if (it.event == core::InputEventName::WindowState) {
				handleContextStateUpdate(it.window.state);
			}
		}
	}

	_application->performOnAppThread([this, events = sp::move(events)]() mutable {
		if (!_client) {
			return;
		}
		// Window-state bookkeeping owned by AppWindow (state mirror + app-side event),
		// then dispatch the whole batch to the client endpoint of the render session.
		for (auto &event : events) {
			if (event.event == InputEventName::WindowState) {
				_state = event.window.state;
				onWindowState(this,
						Value({
							pair("state", Value(toInt(event.window.state))),
							pair("changes", Value(toInt(event.window.changes))),
						}));
			}
		}
		_client->handleInputEvents(sp::move(events));
		setReadyForNextFrame();
	}, this, true);
}

void AppWindow::handleNativeInputEvents(Vector<InputEventData> &&events) {
	// Deliberately NOT handleInputEvents(): the point is to enter one level lower, at the native
	// window, so the events pass through NativeWindow::handleInputEvents - pointer bookkeeping and,
	// above all, the text-input processor's keyboard interception - before reaching the scene. The
	// native window then calls back into handleInputEvents() through the controller.
	_context->performOnThread([this, events = sp::move(events)]() mutable {
		if (_window) {
			_window->handleInputEvents(sp::move(events));
		}
	}, this);
}

void AppWindow::handleTextInput(const TextInputState &state) {
	if (!_presentationEngine) {
		return;
	}

	_application->performOnAppThread([this, state = state]() mutable {
		if (_client) {
			_client->handleTextInput(state);
		}
	}, this, true);
	setReadyForNextFrame();
}

const WindowInfo *AppWindow::getInfo() const {
	if (_window) {
		return _window->getInfo();
	}
	return nullptr;
}

sprt::window::SurfaceBackend AppWindow::getSurfaceBackend() const {
	if (_window) {
		return _window->getSurfaceInterfaceInfo().backend;
	}
	return sprt::window::SurfaceBackend::Surface;
}

core::ImageInfo AppWindow::getSwapchainImageInfo(const core::SwapchainConfig &cfg) const {
	core::ImageInfo swapchainImageInfo;
	swapchainImageInfo.format = cfg.imageFormat;
	swapchainImageInfo.flags = core::ImageFlags::None;
	swapchainImageInfo.imageType = core::ImageType::Image2D;
	swapchainImageInfo.extent = Extent3(cfg.extent.width, cfg.extent.height, 1);
	swapchainImageInfo.arrayLayers = core::ArrayLayers(1);
	swapchainImageInfo.usage = core::ImageUsage::ColorAttachment;
	if (cfg.transfer) {
		swapchainImageInfo.usage |= core::ImageUsage::TransferDst;
	}
	return swapchainImageInfo;
}

core::SurfaceInfo AppWindow::getSurfaceOptions(const core::Device &dev,
		NotNull<core::Surface> surface) const {
	if (_window) {
		auto ifaceInfo = _window->getSurfaceInterfaceInfo();
		auto info = surface->getSurfaceOptions(dev, ifaceInfo.fullscreenMode,
				ifaceInfo.fullscreenHandle);
		if (info.fullscreenHandle == ifaceInfo.fullscreenHandle
				&& info.fullscreenMode == ifaceInfo.fullscreenMode) {
			return _window->getSurfaceOptions(sp::move(info));
		} else {
			return _window->getSurfaceOptions(surface->getSurfaceOptions(dev,
					sprt::window::FullScreenExclusiveMode::Default, nullptr));
		}
	}
	return core::SurfaceInfo();
}

core::ImageViewInfo AppWindow::getSwapchainImageViewInfo(const core::ImageInfo &image) const {
	core::ImageViewInfo info;
	switch (image.imageType) {
	case core::ImageType::Image1D: info.type = core::ImageViewType::ImageView1D; break;
	case core::ImageType::Image2D: info.type = core::ImageViewType::ImageView2D; break;
	case core::ImageType::Image3D: info.type = core::ImageViewType::ImageView3D; break;
	}

	return image.getViewInfo(info);
}

core::SwapchainConfig AppWindow::selectConfig(const core::SurfaceInfo &cfg, bool fastMode) {
	auto c = _context->handleAppWindowSurfaceUpdate(this, cfg, fastMode);
	// preserve selected config for app thread
	_application->performOnAppThread([this, c, fastMode] {
		_appSwapchainConfig = c;
		if (fastMode && _appSwapchainConfig.presentModeFast != core::PresentMode::Unsupported) {
			_appSwapchainConfig.presentMode = _appSwapchainConfig.presentModeFast;
		}
	}, this);
	return c;
}

void AppWindow::acquireFrameData(NotNull<core::PresentationFrame> frame,
		Function<void(NotNull<core::PresentationFrame>)> &&cb) {
	// Tag the frame remote up front, on the presentation thread, so the engine tracks it for
	// connection-reset cleanup even while it is only awaiting the client's reply below. The render
	// client is only swapped via ServerAppThread::takeoverShared* (app thread), so this pointer read is
	// benign.
	if (_client && _client->isRemote()) {
		frame->markRemote();
	}

	_application->performOnAppThread(
			[this, frame = Rc<core::PresentationFrame>(frame), cb = sp::move(cb),
					req = Rc<core::FrameRequest>(frame->getRequest())]() mutable {
		auto proxy = Rc<core::LocalFrameRequestProxy>::create(req);
		if (_client && proxy) {
			uint64_t windowId = 0;

			auto objs = _application->getSharedObjects();
			if (objs) {
				windowId = objs->get(this);
			}

			_client->acquireFrame(windowId, proxy,
					[guard = Rc<AppWindow>(this), frame, cb = sp::move(cb)](bool success) mutable {
				guard->_context->performOnThread(
						[frame = move(frame), cb = sp::move(cb)]() mutable {
					cb(frame); //
				}, guard);
			});
		} else {
			// No client (or proxy): the window is mid-teardown or never got a Director. Dropping
			// the callback would leave the frame in _activeFrames forever and wedge EndOfLife —
			// invalidate it instead so the completion runs and the engine unwinds.
			log::source().debug("WindowDiag", "acquireFrameData dropped id=", _windowId);
			_context->performOnThread([frame = move(frame)]() mutable {
				if (frame) {
					frame->invalidate();
				}
			}, this);
		}
	},
			this);
}

void AppWindow::handleFrameReady(NotNull<core::PresentationFrame> frame) {
	_firstFrameCompleted = true;
	if (_mapOnFirstFrame && !_inCloseRequest && _window) {
		_mapOnFirstFrame = false;
		_window->mapWindow();
	}
	if (_window) {
		_window->handleFrameReady(frame->getInfo());
	}
}

void AppWindow::handleFramePresented(NotNull<core::PresentationFrame> frame) {
	if (_window) {
		_window->handleFramePresented(frame->getInfo());
	}
}

void AppWindow::handleSwapchainUpdated(const core::FrameConstraints &c) {
	if (_window) {
		_window->handleSwapchainUpdated(c);
	}
}

Rc<core::Surface> AppWindow::makeSurface(NotNull<core::Instance> cinstance) {
#if MODULE_XENOLITH_BACKEND_WEBGPU
	if (cinstance->getApi() == core::InstanceApi::WebGPU) {
		auto ifaceInfo = _window->getSurfaceInterfaceInfo();
		auto instance = static_cast<webgpu::Instance *>(cinstance.get());

		WGPUSurfaceDescriptor desc = WGPU_SURFACE_DESCRIPTOR_INIT;

		WGPUSurfaceSourceXCBWindow xcbSrc = WGPU_SURFACE_SOURCE_XCB_WINDOW_INIT;
		WGPUSurfaceSourceWaylandSurface waylandSrc = WGPU_SURFACE_SOURCE_WAYLAND_SURFACE_INIT;

		switch (ifaceInfo.backend) {
		case sprt::window::SurfaceBackend::Xcb:
			xcbSrc.connection = ifaceInfo.xcb.connection;
			xcbSrc.window = ifaceInfo.xcb.window;
			desc.nextInChain = &xcbSrc.chain;
			break;
		case sprt::window::SurfaceBackend::Wayland:
			waylandSrc.display = ifaceInfo.wayland.display;
			waylandSrc.surface = ifaceInfo.wayland.surface;
			desc.nextInChain = &waylandSrc.chain;
			break;
		case sprt::window::SurfaceBackend::Canvas:
			desc.nextInChain = nullptr; // the JS binding returns the OffscreenCanvas surface
			break;
		default:
			log::source().error("AppWindow",
					"Surface backend is not supported for WebGPU: ", toInt(ifaceInfo.backend));
			return nullptr;
		}

		auto surface = wgpuInstanceCreateSurface(instance->getInstance(), &desc);
		if (!surface) {
			log::source().error("AppWindow", "Fail to create WGPUSurface");
			return nullptr;
		}

		return Rc<webgpu::Surface>::create(instance, surface);
	}
#endif

#if MODULE_XENOLITH_BACKEND_MTL
	if (cinstance->getApi() == core::InstanceApi::Metal) {
		auto ifaceInfo = _window->getSurfaceInterfaceInfo();
		if (ifaceInfo.backend != sprt::window::SurfaceBackend::Metal || !ifaceInfo.metal.layer) {
			log::source().error("AppWindow",
					"Surface backend is not supported for Metal: ", toInt(ifaceInfo.backend));
			return nullptr;
		}

		return Rc<mtl::Surface>::create(static_cast<mtl::Instance *>(cinstance.get()),
				ifaceInfo.metal.layer, this);
	}
#endif

#if MODULE_XENOLITH_BACKEND_SOFT
	if (cinstance->getApi() == core::InstanceApi::Software) {
		auto instance = static_cast<soft::Instance *>(cinstance.get());
		auto ifaceInfo = _window->getSurfaceInterfaceInfo();

		if (ifaceInfo.backend == sprt::window::SurfaceBackend::Headless) {
			// No window system: the surface is synthesized from the window extent and backs a
			// pseudo-swapchain of ordinary bitmaps.
			return Rc<soft::HeadlessSurface>::create(instance, _window->getExtent(), this);
		}

		if (ifaceInfo.backend == sprt::window::SurfaceBackend::Display) {
			log::source().error("AppWindow",
					"Direct-display (KMS) presentation is not implemented for the software "
					"backend: it needs dumb buffers and page flipping, which the DRM binding does "
					"not carry yet");
			return nullptr;
		}

		// Everything else goes through the window system's own CPU buffers, so the rasterizer
		// writes the frame straight into what gets presented. A window system that cannot provide
		// them answers null, and there is no copying fallback: it would silently give up the one
		// property this path exists for.
		auto software = _window->makeSoftwareSurface();
		if (!software) {
			log::source()
					.error("AppWindow",
							"Window system cannot provide a CPU-writable buffer for the software "
							"backend " "(surface backend ",
							toInt(ifaceInfo.backend), ")");
			return nullptr;
		}

		return Rc<soft::Surface>::create(instance, sp::move(software), this);
	}
#endif

#if MODULE_XENOLITH_BACKEND_VK
	auto info = _window->getSurfaceInterfaceInfo();
	if (cinstance->getApi() != core::InstanceApi::Vulkan) {
		return nullptr;
	}

	auto instance = static_cast<vk::Instance *>(cinstance.get());

	if (info.backend == sprt::window::SurfaceBackend::Headless) {
		// No window system: the surface is synthesized from the window extent and backs a
		// pseudo-swapchain of ordinary device images.
		return Rc<vk::HeadlessSurface>::create(instance, _window->getExtent(), this);
	}

	VkSurfaceKHR surface = VK_NULL_HANDLE;

	switch (info.backend) {
	case sprt::window::SurfaceBackend::Android: {

#if defined(VK_KHR_android_surface)
		VkAndroidSurfaceCreateInfoKHR createInfo{
			VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
			nullptr,
			0,
			(ANativeWindow *)info.android.window,
		};

		if (instance->vkCreateAndroidSurfaceKHR(instance->getInstance(), &createInfo, nullptr,
					&surface)
				!= VK_SUCCESS) {
			return nullptr;
		}
#endif
		break;
	}
	case sprt::window::SurfaceBackend::Xcb: {
#if defined(VK_KHR_xcb_surface)
		VkXcbSurfaceCreateInfoKHR createInfo{
			VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
			nullptr,
			0,
			(xcb_connection_t *)info.xcb.connection,
			info.xcb.window,
		};
		if (instance->vkCreateXcbSurfaceKHR(instance->getInstance(), &createInfo, nullptr, &surface)
				!= VK_SUCCESS) {
			return nullptr;
		}
#endif
		break;
	}
	case sprt::window::SurfaceBackend::Wayland: {
#if defined(VK_KHR_wayland_surface)
		VkWaylandSurfaceCreateInfoKHR createInfo{
			VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
			nullptr,
			0,
			(struct wl_display *)info.wayland.display,
			(struct wl_surface *)info.wayland.surface,
		};
		auto ret = instance->vkCreateWaylandSurfaceKHR(instance->getInstance(), &createInfo,
				nullptr, &surface);
		if (ret != VK_SUCCESS) {
			return nullptr;
		}
#endif
		break;
	}
	case sprt::window::SurfaceBackend::Win32: {
#if defined(VK_KHR_win32_surface)
		VkWin32SurfaceCreateInfoKHR createInfo{
			VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
			nullptr,
			0,
			info.windows.hinstance,
			info.windows.hwnd,
		};

		if (instance->vkCreateWin32SurfaceKHR(instance->getInstance(), &createInfo, nullptr,
					&surface)
				!= VK_SUCCESS) {
			return nullptr;
		}
#endif
		break;
	}
	case sprt::window::SurfaceBackend::Metal: {
#if defined(VK_EXT_metal_surface)
		VkMetalSurfaceCreateInfoEXT createInfo{
			VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
			nullptr,
			0,
			info.metal.layer,
		};

		if (instance->vkCreateMetalSurfaceEXT(instance->getInstance(), &createInfo, nullptr,
					&surface)
				!= VK_SUCCESS) {
			return nullptr;
		}
#endif
		break;
	}
	case sprt::window::SurfaceBackend::Display: {
#if defined(VK_KHR_display)
		// Direct-to-display (no window system): create a plane surface on the
		// connector the window system opened, at the mode it resolved. Both travel
		// in info.display — NOT WindowInfo (often a desktop default like 1024x768).
		surface = instance->createDisplayPlaneSurface(info);
		if (surface == VK_NULL_HANDLE) {
			return nullptr;
		}
#endif
		break;
	}
	default: break;
	}

	if (surface != VK_NULL_HANDLE) {
		return Rc<vk::Surface>::create(instance, surface, this);
	}
#endif
	slog().error("XcbWindow", "No available GAPI found for a surface");
	return nullptr;
}

core::FrameConstraints AppWindow::exportConstraints(uint64_t &serial) const {
	auto c = _window->exportConstraints(serial);
	_application->performOnAppThread([this, c] {
		const_cast<sprt::window::FrameConstraints &>(_appFrameConstraints) = c;
	}, const_cast<AppWindow *>(this));

	// A resize changes the geometry too, and this is the one place both are already being read off
	// the native window on the context thread - so the mirror rides along instead of racing a
	// second, separately-timed read.
	notifyWindowGeometry();
	return c;
}

void AppWindow::notifyWindowGeometry() const {
	if (!_window) {
		return;
	}

	auto geometry = _window->getWindowGeometry();
	_application->performOnAppThread([this, geometry] {
		if (_appWindowGeometry == geometry) {
			// Nothing moved and nothing resized. The context thread cannot tell - it has no copy of
			// the mirror - so the comparison belongs here, and it is what keeps a window that is
			// merely redrawing from waking the scene up.
			return;
		}
		// The mirror is `const` to everything that reads it; this is the one writer, on the one
		// thread allowed to write it - the same arrangement _appFrameConstraints has above.
		const_cast<sprt::window::WindowGeometry &>(_appWindowGeometry) = geometry;
		if (_client) {
			_client->handleWindowGeometryChanged(geometry);
		}
	}, const_cast<AppWindow *>(this));
}

void AppWindow::setFrameOrder(uint64_t frameOrder) {
	if (_window) {
		_window->setFrameOrder(frameOrder);
	}
}

void AppWindow::updateConstraints(core::UpdateConstraintsFlags flags) {
	_context->performOnThread([this, flags] {
		// While closing, only EndOfLife may touch the engine: a resize deprecate here would
		// recreate a swapchain on a window that is already tearing down.
		if (_inCloseRequest && !hasFlag(flags, core::UpdateConstraintsFlags::EndOfLife)) {
			return;
		}
		if (_presentationEngine) {
			_presentationEngine->updateConstraints(flags);
		}
	}, this, true);
}

void AppWindow::setReadyForNextFrame() {
	_context->performOnThread([this] {
		if (_inCloseRequest) {
			return;
		}
		if (_presentationEngine) {
			_presentationEngine->setReadyForNextFrame();
		}
	}, this, true);
}

void AppWindow::invalidateRemoteFrames() {
	_context->performOnThread([this] {
		if (_presentationEngine) {
			_presentationEngine->invalidateRemoteFrames();
		}
	}, this);
}

void AppWindow::resetForRenderClientChange() {
	_context->performOnThread([this] {
		if (_presentationEngine) {
			_presentationEngine->resetForRenderClientChange();
		}
	}, this);
}

bool AppWindow::waitUntilFrame() {
	if (!_context->getLooper()->isOnThisThread()) {
		return false;
	}

	if (_presentationEngine) {
		return _presentationEngine->waitUntilFramePresentation();
	}
	return false;
}

void AppWindow::setPresentationOnDemand(bool value) {
	_context->performOnThread([this, value] {
		if (_presentationEngine) {
			_presentationEngine->setRenderOnDemand(value);
		}
	}, this, true);
}

bool AppWindow::isPresentationOnDemand() const {
	return _presentationEngine ? _presentationEngine->isRenderOnDemand() : false;
}

void AppWindow::setPresentationFrameInterval(uint64_t value) {
	_context->performOnThread([this, value] {
		if (_presentationEngine) {
			_presentationEngine->setTargetFrameInterval(value);
		}
	}, this, true);
}

uint64_t AppWindow::getPresentationFrameInterval() const {
	return _presentationEngine ? _presentationEngine->getTargetFrameInterval() : 0;
}

// --- core::RenderServerChannel (client -> server) ---

void AppWindow::compileRenderQueue(const Rc<core::Queue> &queue, Function<void(bool)> &&cb) {
	static_cast<core::Loop *>(_context->getGlLoop())->compileQueue(queue, sp::move(cb));
}

void AppWindow::compileResource(Rc<core::Resource> &&res, Function<void(bool)> &&cb, bool preload) {
	static_cast<core::Loop *>(_context->getGlLoop())
			->compileResource(sp::move(res), sp::move(cb), preload);
}

void AppWindow::compileMaterials(Rc<core::MaterialInputData> &&req,
		const Vector<Rc<core::DependencyEvent>> &deps) {
	static_cast<core::Loop *>(_context->getGlLoop())->compileMaterials(sp::move(req), deps);
}

void AppWindow::compileImage(const Rc<core::DynamicImage> &img, Function<void(bool)> &&cb) {
	static_cast<core::Loop *>(_context->getGlLoop())->compileImage(img, sp::move(cb));
}

void AppWindow::attachRenderQueue(const Rc<core::Queue> &queue) {
	// Announce it to the client (the Director) so it can resolve this graph by name per frame.
	// Director::handleRenderQueueAttached / _availableQueues had no caller at all before this.
	if (_client && queue) {
		_client->handleRenderQueueAttached(queue);
	}

	_context->performOnThread([this, queue] {
		runWithQueue(queue);
		setReadyForNextFrame();
	}, this, false);
}

void AppWindow::setPreferredFrameInterval(uint64_t value) { setPresentationFrameInterval(value); }

core::FrameTimingInfo AppWindow::getFrameTiming() const {
	core::FrameTimingInfo info;
	if (_presentationEngine) {
		info.lastFrameInterval = _presentationEngine->getLastFrameInterval();
		info.avgFrameInterval = _presentationEngine->getAvgFrameInterval();
		info.lastFrameTime = _presentationEngine->getLastFrameTime();
		info.lastFenceFrameTime = _presentationEngine->getLastFenceFrameTime();
		info.lastTimestampFrameTime = _presentationEngine->getLastTimestampFrameTime();
	}
	return info;
}

WindowState AppWindow::getUpdatableStateFlags() const {
	auto caps = getCapabilities();
	WindowState flags = WindowState::None;

	if (hasFlag(caps, WindowCapabilities::AboveBelowState)) {
		flags |= WindowState::Above | WindowState::Below;
	}

	if (hasFlag(caps, WindowCapabilities::DemandsAttentionState)) {
		flags |= WindowState::DemandsAttention;
	}

	if (hasFlag(caps, WindowCapabilities::SkipTaskbarState)) {
		flags |= WindowState::SkipTaskbar | WindowState::SkipPager;
	}

	if (hasFlag(caps, WindowCapabilities::CloseGuard)) {
		flags |= WindowState::CloseGuard | WindowState::CloseRequest;
	}

	if (hasFlag(caps, WindowCapabilities::DecorationState)) {
		flags |= WindowState::DecorationState;
	}

	for (auto it : sp::flags(_state)) {
		switch (it) {
		case WindowState::AllowedMinimize: flags |= WindowState::Minimized; break;
		case WindowState::AllowedShade: flags |= WindowState::Shaded; break;
		case WindowState::AllowedStick: flags |= WindowState::Sticky; break;
		case WindowState::AllowedMaximizeVert: flags |= WindowState::MaximizedVert; break;
		case WindowState::AllowedMaximizeHorz: flags |= WindowState::MaximizedHorz; break;
		case WindowState::AllowedClose: flags |= WindowState::CloseRequest; break;
		case WindowState::AllowedFullscreen: flags |= WindowState::Fullscreen; break;
		default: break;
		}
	}
	return flags;
}

bool AppWindow::enableState(WindowState state) {
	auto c = sprt::popcount(toInt(state));
	if (c != 1 && state != WindowState::Maximized) {
		log::source().error("AppWindow", "enableState: only one flag should be defined in state");
		return false;
	}

	if ((state & getUpdatableStateFlags()) != state) {
		log::source().error("AppWindow", "enableState:", state, " is not updatable");
		return false;
	}

	_context->performOnThread([this, state]() { _window->enableState(state); }, this);
	return true;
}

bool AppWindow::disableState(WindowState state) {
	auto c = sprt::popcount(toInt(state));
	if (c != 1 && state != WindowState::Maximized) {
		log::source().error("AppWindow", "enableState: only one flag should be defined in state");
		return false;
	}

	if ((state & getUpdatableStateFlags()) != state) {
		log::source().error("AppWindow", "disableState:", state, " is not updatable");
		return false;
	}

	_context->performOnThread([this, state]() { _window->disableState(state); }, this);
	return true;
}

void AppWindow::acquireTextInput(TextInputRequest &&req) {
	_context->performOnThread([this, data = move(req)]() {
		if (_window) {
			_window->acquireTextInput(data);
		}
	}, this);
}

void AppWindow::releaseTextInput() {
	_context->performOnThread([this]() {
		if (_window) {
			_window->releaseTextInput();
		}
	}, this);
}

void AppWindow::performTextInput(TextInputCommand &&cmd) {
	_context->performOnThread([this, cmd = sp::move(cmd)]() {
		if (_window) {
			_window->performTextInput(cmd);
		}
	}, this);
}

void AppWindow::updateLayers(sprt::window::Vector<WindowLayer> &&layers) {
	_context->performOnThread([this, layers = sp::move(layers)]() mutable {
		if (_window) {
			_window->updateLayers(sp::move(layers));
		}
	}, this);
}

void AppWindow::acquireScreenInfo(Function<void(NotNull<ScreenInfo>)> &&cb, Ref *ref) {
	_context->performOnThread([this, cb = sp::move(cb), ref = Rc<Ref>(ref)]() mutable {
		auto winfo = _window->getInfo();
		if (hasFlag(winfo->capabilities, WindowCapabilities::DirectOutput)
				&& hasFlag(winfo->flags, WindowCreationFlags::DirectOutput)) {
			auto info = _presentationEngine->getScreenInfo();
			_application->performOnAppThread(
					[cb = sp::move(cb), ref = move(ref), info = move(info)]() mutable {
				cb(info);
				ref = nullptr;
				info = nullptr;
			}, this);
		} else {
			_application->acquireScreenInfo(sp::move(cb), ref);
		}
	}, this);
}

bool AppWindow::setFullscreen(FullscreenInfo &&info, Function<void(Status)> &&cb, Ref *ref) {
	if (!hasFlag(getCapabilities(), WindowCapabilities::Fullscreen)) {
		return false;
	}
	_context->performOnThread(
			[this, info = move(info), cb = sp::move(cb), ref = Rc<Ref>(ref)]() mutable {
		auto winfo = _window->getInfo();
		auto useDirect = hasFlag(winfo->capabilities, WindowCapabilities::DirectOutput)
				&& hasFlag(winfo->flags, WindowCreationFlags::DirectOutput);
		if (useDirect) {
			auto st = _presentationEngine->setFullscreenSurface(info.id, info.mode);
			_application->performOnAppThread([st, cb = sp::move(cb), ref = move(ref)]() mutable {
				cb(st);
				ref = nullptr;
			}, this);
		} else {
			_window->setFullscreen(move(info),
					[this, cb = sp::move(cb), ref = move(ref)](Status st) mutable {
				_application->performOnAppThread(
						[st, cb = sp::move(cb), ref = move(ref)]() mutable {
					cb(st);
					ref = nullptr;
				}, this);
			}, this);
		}
	}, this);
	return true;
}

void AppWindow::setWindowExtent(Extent2 extent, Function<void(Status)> &&cb, Ref *ref) {
	_context->performOnThread([this, extent, cb = sp::move(cb), ref = Rc<Ref>(ref)]() mutable {
		auto st = _window ? _window->setExtent(extent) : Status::ErrorInvalidArguemnt;
		_application->performOnAppThread([st, cb = sp::move(cb), ref = move(ref)]() mutable {
			if (cb) {
				cb(st);
			}
			ref = nullptr;
		}, this);
	}, this);
}

bool AppWindow::isDialogSupported(sprt::window::DialogType type) const {
	return _context->isDialogSupported(type);
}

Status AppWindow::openDialog(NotNull<sprt::window::DialogRequest> req) {
	if (!req->callback) {
		return Status::ErrorInvalidArguemnt;
	}

	// This window owns the dialog: it parents it, blocks for it, and takes it down with itself.
	req->parentWindowId = _windowId;
	_pendingDialogs.emplace_back(req);

	// Wrap the caller's callback so the pending list is pruned on the thread that owns it. The
	// completion is delivered on the app looper, which is this same thread.
	auto cb = sp::move(req->callback);
	req->callback = [this, self = Rc<AppWindow>(this), r = Rc<sprt::window::DialogRequest>(req),
							cb = sp::move(cb)](const sprt::window::DialogResult &res) mutable {
		for (auto it = _pendingDialogs.begin(); it != _pendingDialogs.end(); ++it) {
			if (*it == r) {
				_pendingDialogs.erase(it);
				break;
			}
		}
		cb(res);
	};

	_context->performOnThread([this, req = Rc<sprt::window::DialogRequest>(req)]() mutable {
		auto looper = _application->getLooper();
		if (!looper) {
			return; // app thread already gone; nothing can deliver a completion any more
		}
		// A window that is winding down needs no special case here: either it is already out of
		// the controller's active set, and openDialog declines with ErrorCancelled because the
		// named parent cannot be resolved, or it is still there and performWindowTeardown cancels
		// the dialog moments later. Both answer the callback rather than dropping it.
		_context->openDialog(looper, sp::move(req));
	}, this);
	return Status::Ok;
}

Status AppWindow::cancelDialog(NotNull<sprt::window::DialogRequest> req) {
	_context->performOnThread([this, req = Rc<sprt::window::DialogRequest>(req)]() mutable {
		_context->cancelDialog(req);
	}, this);
	return Status::Ok;
}

bool AppWindow::setPreferredFrameRate(float value, Function<void(Status)> &&cb) {
	_context->performOnThread([this, cb = sp::move(cb), value]() mutable {
		auto st = _window->setPreferredFrameRate(value);
		_application->performOnAppThread([st, cb = sp::move(cb)]() mutable { cb(st); }, this);
	}, this);
	return true;
}

void AppWindow::captureScreenshot(
		Function<void(const core::ImageInfoData &info, BytesView view)> &&cb) {
	_context->performOnThread([this, cb = sp::move(cb)]() mutable {
		_presentationEngine->captureScreenshot(sp::move(cb));
	}, this);
}

bool AppWindow::openWindowMenu(Vec2 pos) {
	if (hasFlag(_state, WindowState::AllowedWindowMenu)) {
		_context->performOnThread([this, pos]() { _window->openWindowMenu(pos); }, this);
		return true;
	}
	return false;
}

void AppWindow::handleBackButton() {
	if (_window) {
		_context->performOnThread([this]() { _window->handleBackButton(); }, this);
	}
}

void AppWindow::handleContextStateUpdate(WindowState state) {
	static constexpr auto FullscreenExclusiveMask =
			WindowState::Fullscreen | WindowState::Focused | WindowState::Enabled;
	if (_contextState != state) {
		auto changes = state ^ _contextState;
		_contextState = state;
		if (hasFlag(changes, FullscreenExclusiveMask)
				&& hasFlagAll(_contextState, FullscreenExclusiveMask)) {
			_presentationEngine->enableExclusiveFullscreen();
		}
	}
}

void AppWindow::synchronizeClose() {
	if (_syncClose) {
		_context->getLooper()->wakeup();
	}
}

} // namespace stappler::xenolith
