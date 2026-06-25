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

	_presentationEngine = static_cast<core::Loop *>(_context->getGlLoop())
								  ->makePresentationEngine(this, w->getPreferredOptions());

	return _presentationEngine != nullptr;
}

void AppWindow::runWithQueue(const Rc<core::Queue> &queue) {
	if (!_presentationEngine->isRunning()) {
		_presentationEngine->run();

		_presentationEngine->scheduleNextImage([this](core::PresentationFrame *, bool success) {
			// Map windows after frame was rendered
			_window->mapWindow();
		});
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

void AppWindow::end() {
	if (!_presentationEngine) {
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

	_application->performOnAppThread([this, engine = move(engine)]() mutable {
		_client = nullptr; // the Director (client endpoint) is being destroyed below
		_application->handleAppWindowDestroyed(this, sp::move(_director));
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

	if (_context->getLooper()->isOnThisThread()
			&& _presentationEngine->getOptions().syncConstraintsUpdate) {
		_syncClose = true;
	}

	_inCloseRequest = true;
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
		} else {
			_presentationEngine->updateConstraints(core::UpdateConstraintsFlags::EndOfLife,
					[this, w = Rc<NativeWindow>(w)](bool) {
				// successful stop
				end();
				_window = nullptr;
			});
		}
	}, this);

	if (_syncClose) {
		// run looper until successful close
		// wakeup signal should be in AppWindow::end()
		_context->getLooper()->run();
		_syncClose = true;
		_window = nullptr;
	}
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
		}
	},
			this);
}

void AppWindow::handleFrameReady(NotNull<core::PresentationFrame> frame) {
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
#if MODULE_XENOLITH_BACKEND_VK
	auto info = _window->getSurfaceInterfaceInfo();
	if (cinstance->getApi() != core::InstanceApi::Vulkan) {
		return nullptr;
	}

	auto instance = static_cast<vk::Instance *>(cinstance.get());

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
	return c;
}

void AppWindow::setFrameOrder(uint64_t frameOrder) {
	if (_window) {
		_window->setFrameOrder(frameOrder);
	}
}

void AppWindow::updateConstraints(core::UpdateConstraintsFlags flags) {
	_context->performOnThread([this, flags] {
		if (_presentationEngine) {
			_presentationEngine->updateConstraints(flags);
		}
	}, this, true);
}

void AppWindow::setReadyForNextFrame() {
	_context->performOnThread([this] {
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
