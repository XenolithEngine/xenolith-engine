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

#include "XLAndroidWindow.h"
#include "XLAndroidContextController.h"
#include "XLAndroidActivity.h"
#include "XLAppWindow.h"

#if MODULE_XENOLITH_BACKEND_VK
#include "XLVkPresentationEngine.h" // IWYU pragma: keep
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

static core::ImageFormat AndroidWindow_getFormat(int32_t fmt) {
	switch (fmt) {
	case AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM: return core::ImageFormat::R8G8B8A8_UNORM; break;
	case AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM: return core::ImageFormat::R8G8B8A8_UNORM; break;
	case AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM: return core::ImageFormat::R8G8B8_UNORM; break;
	case AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM: return core::ImageFormat::R5G6B5_UNORM_PACK16; break;
	case AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT:
		return core::ImageFormat::R16G16B16A16_SFLOAT;
		break;
	case AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM:
		return core::ImageFormat::A2R10G10B10_UNORM_PACK32;
		break;
	case AHARDWAREBUFFER_FORMAT_BLOB: break;
	case AHARDWAREBUFFER_FORMAT_D16_UNORM: return core::ImageFormat::D16_UNORM; break;
	case AHARDWAREBUFFER_FORMAT_D24_UNORM: return core::ImageFormat::X8_D24_UNORM_PACK32; break;
	case AHARDWAREBUFFER_FORMAT_D24_UNORM_S8_UINT:
		return core::ImageFormat::D24_UNORM_S8_UINT;
		break;
	case AHARDWAREBUFFER_FORMAT_D32_FLOAT: return core::ImageFormat::D32_SFLOAT; break;
	case AHARDWAREBUFFER_FORMAT_D32_FLOAT_S8_UINT:
		return core::ImageFormat::D32_SFLOAT_S8_UINT;
		break;
	case AHARDWAREBUFFER_FORMAT_S8_UINT: return core::ImageFormat::S8_UINT; break;
	case AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420: break;
	case AHARDWAREBUFFER_FORMAT_YCbCr_P010: break;
	case AHARDWAREBUFFER_FORMAT_R8_UNORM: return core::ImageFormat::R8_UNORM; break;
	case AHARDWAREBUFFER_FORMAT_R16_UINT: return core::ImageFormat::R16_UINT; break;
	case AHARDWAREBUFFER_FORMAT_R16G16_UINT: return core::ImageFormat::R16G16_UINT; break;
	case AHARDWAREBUFFER_FORMAT_R10G10B10A10_UNORM:
		return core::ImageFormat::R10X6G10X6B10X6A10X6_UNORM_4PACK16;
		break;
	}
	return core::ImageFormat::Undefined;
}

static void AndroidWindow_refreshRateCallback(int64_t vsyncPeriodNanos, void *data) {
	auto w = reinterpret_cast<AndroidWindow *>(data);
	w->setVsyncPeriod(vsyncPeriodNanos);
}

AndroidWindow::~AndroidWindow() {
	if (_AChoreographer_registerRefreshRateCallback && _AChoreographer_unregisterRefreshRateCallback
			&& _refreshRateCallbackRegistred) {
		_AChoreographer_unregisterRefreshRateCallback(_choreographer,
				AndroidWindow_refreshRateCallback, this);
	}
	if (_window) {
		ANativeWindow_release(_window);
		_window = nullptr;
	}
}

bool AndroidWindow::init(NotNull<AndroidActivity> activity, Rc<WindowInfo> &&info,
		ANativeWindow *n) {
	auto controller = activity->getController();

	if (!NativeWindow::init(controller, sp::move(info), controller->getCapabilities())) {
		return false;
	}

	_activity = activity;

	_window = n;
	ANativeWindow_acquire(_window);

	_selfHandle = Dso(StringView(), DsoFlags::Self);
	if (_selfHandle) {
		_ANativeWindow_setBuffersTransform =
				_selfHandle.sym<decltype(_ANativeWindow_setBuffersTransform)>(
						"ANativeWindow_setBuffersTransform");
		_ANativeWindow_setFrameRate = _selfHandle.sym<decltype(_ANativeWindow_setFrameRate)>(
				"ANativeWindow_setFrameRate");
		_ANativeWindow_setFrameRateWithChangeStrategy =
				_selfHandle.sym<decltype(_ANativeWindow_setFrameRateWithChangeStrategy)>(
						"ANativeWindow_setFrameRateWithChangeStrategy");

		_AChoreographer_postFrameCallback64 =
				_selfHandle.sym<decltype(_AChoreographer_postFrameCallback64)>(
						"AChoreographer_postFrameCallback64");
		_AChoreographer_postVsyncCallback =
				_selfHandle.sym<decltype(_AChoreographer_postVsyncCallback)>(
						"AChoreographer_postVsyncCallback");
		_AChoreographer_registerRefreshRateCallback =
				_selfHandle.sym<decltype(_AChoreographer_registerRefreshRateCallback)>(
						"AChoreographer_registerRefreshRateCallback");
		_AChoreographer_unregisterRefreshRateCallback =
				_selfHandle.sym<decltype(_AChoreographer_unregisterRefreshRateCallback)>(
						"AChoreographer_unregisterRefreshRateCallback");
	}

	_choreographer = AChoreographer_getInstance();

	_identityExtent = _extent =
			Extent2(ANativeWindow_getWidth(_window), ANativeWindow_getHeight(_window));
	_format = AndroidWindow_getFormat(ANativeWindow_getFormat(_window));

	_info->rect.width = _identityExtent.width;
	_info->rect.height = _identityExtent.height;

	_info->density = acquireWindowDensity();
	_info->state |= _activity->getDecorationState();

	return true;
}

void AndroidWindow::mapWindow() {
	if (_AChoreographer_registerRefreshRateCallback
			&& _AChoreographer_unregisterRefreshRateCallback) {
		_AChoreographer_registerRefreshRateCallback(_choreographer,
				AndroidWindow_refreshRateCallback, this);
		_refreshRateCallbackRegistred = true;
	}

	postFrameCallback();
}

void AndroidWindow::unmapWindow() { _unmapped = true; }

bool AndroidWindow::close() {
	if (_unmapped) {
		return true;
	}

	if (!_controller->notifyWindowClosed(this)) {
		if (hasFlag(_info->state, WindowState::CloseGuard)) {
			updateState(0, _info->state | WindowState::CloseRequest);
		}
		return false;
	}

	// prevent Director from preservation when user close app intentionally
	_info->capabilities &= ~WindowCapabilities::PreserveDirector;
	_activity->finish();
	return true;
}

void AndroidWindow::handleFramePresented(NotNull<core::PresentationFrame> frame) {
	NativeWindow::handleFramePresented(frame);

	postFrameCallback();
}

core::SurfaceInfo AndroidWindow::getSurfaceOptions(const core::Device &dev,
		NotNull<core::Surface> surface) const {
	auto opts = NativeWindow::getSurfaceOptions(dev, surface);

	XL_ANDROID_LOG("AndroidWindow::getSurfaceOptions ", opts.currentExtent.width, " ",
			opts.currentExtent.height, " ", toInt(opts.currentTransform));

	uint32_t width = _extent.width;
	uint32_t height = _extent.height;

	if (hasFlag(opts.currentTransform, core::SurfaceTransformFlags::Rotate90)
			|| hasFlag(opts.currentTransform, core::SurfaceTransformFlags::Rotate270)
			|| hasFlag(opts.currentTransform, core::SurfaceTransformFlags::Rotate90)
			|| hasFlag(opts.currentTransform, core::SurfaceTransformFlags::Rotate270)) {
		opts.currentExtent.height = width;
		opts.currentExtent.width = height;
	} else {
		opts.currentExtent = Extent2(width, height);
	}

	opts.currentTransform |= core::SurfaceTransformFlags::PreRotated;

	auto formatSupport = NativeBufferFormatSupport::get();

	auto it = opts.formats.begin();
	while (it != opts.formats.end()) {
		switch (it->first) {
		case core::ImageFormat::R8G8B8A8_UNORM:
		case core::ImageFormat::R8G8B8A8_SRGB:
			if (!formatSupport.R8G8B8A8_UNORM) {
				it = opts.formats.erase(it);
			} else {
				++it;
			}
			break;
		case core::ImageFormat::R8G8B8_UNORM:
			if (!formatSupport.R8G8B8_UNORM) {
				it = opts.formats.erase(it);
			} else {
				++it;
			}
			break;
		case core::ImageFormat::R5G6B5_UNORM_PACK16:
			if (!formatSupport.R5G6B5_UNORM) {
				it = opts.formats.erase(it);
			} else {
				++it;
			}
			break;
		case core::ImageFormat::R16G16B16A16_SFLOAT:
			if (!formatSupport.R16G16B16A16_FLOAT) {
				it = opts.formats.erase(it);
			} else {
				++it;
			}
			break;
		default: ++it;
		}
	}

	return opts;
}

core::FrameConstraints AndroidWindow::exportConstraints() const {
	auto c = NativeWindow::exportConstraints();

	if (_vsyncPeriodNanos) {
		// convert to micros
		c.frameInterval = _vsyncPeriodNanos / 1'000;
	}

	return sp::move(c);
}

Extent2 AndroidWindow::getExtent() const { return _extent; }

Rc<core::Surface> AndroidWindow::makeSurface(NotNull<core::Instance> cinstance) {
#if MODULE_XENOLITH_BACKEND_VK
	if (cinstance->getApi() != core::InstanceApi::Vulkan) {
		return nullptr;
	}

	auto instance = static_cast<vk::Instance *>(cinstance.get());

	VkSurfaceKHR targetSurface = VK_NULL_HANDLE;
	VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo;
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.pNext = nullptr;
	surfaceCreateInfo.flags = 0;
	surfaceCreateInfo.window = _window;

	if (instance->vkCreateAndroidSurfaceKHR(instance->getInstance(), &surfaceCreateInfo, nullptr,
				&targetSurface)
			!= VK_SUCCESS) {
		log::source().error("ViewImpl", "fail to create surface");
		return nullptr;
	}

	return Rc<vk::Surface>::create(instance, targetSurface, this);
#else
	log::source().error("WindowsWindow", "No available GAPI found for a surface");
	return nullptr;
#endif
}

core::PresentationOptions AndroidWindow::getPreferredOptions() const {
	auto opts = NativeWindow::getPreferredOptions();

	// With AChoreographer, we can use DisplayLink with barrier presentation mode
	if (_choreographer) {
		opts.followDisplayLinkBarrier = true;
	}

	opts.syncConstraintsUpdate = true;
	return opts;
}

static void AndroidWindow_updateDecorationState(ActivityProxy *proxy, jobject activity,
		const Callback<void(const jni::Ref &window, const jni::Ref &ic)> &cb) {
	auto app = jni::Env::getApp();
	auto env = jni::Env::getEnv();

	if (app->WindowInsetsController) {
		auto w = proxy->Activity.getWindow(jni::Ref(activity, env));
		auto ic = app->Window.getInsetsController(w);
		if (ic) {
			cb(w, ic);
		}
	}
}

bool AndroidWindow::enableState(WindowState state) {
	if (NativeWindow::enableState(state)) {
		return true;
	}

	auto app = jni::Env::getApp();
	auto env = jni::Env::getEnv();

	if (state == WindowState::DecorationNavigationVisible) {
		AndroidWindow_updateDecorationState(_activity->getProxy(), _activity->getActivity()->clazz,
				[&](const jni::Ref &window, const jni::Ref &ic) {
			app->WindowInsetsController.show(ic,
					app->WindowInsetType.navigationBars(app->WindowInsetType.getClass().ref(env)));
		});
		return true;
	} else if (state == WindowState::DecorationStatusBarVisible) {
		AndroidWindow_updateDecorationState(_activity->getProxy(), _activity->getActivity()->clazz,
				[&](const jni::Ref &window, const jni::Ref &ic) {
			app->WindowInsetsController.show(ic,
					app->WindowInsetType.statusBars(app->WindowInsetType.getClass().ref(env)));
		});
		return true;
	} else if (state == WindowState::DecorationStatusBarLight) {
		AndroidWindow_updateDecorationState(_activity->getProxy(), _activity->getActivity()->clazz,
				[&](const jni::Ref &window, const jni::Ref &ic) {
			app->WindowInsetsController.setSystemBarsAppearance(ic,
					app->WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS(),
					app->WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS());
		});
		updateState(0, _info->state | WindowState::DecorationStatusBarLight);
		return true;
	} else if (state == WindowState::DecorationNavigationLight) {
		AndroidWindow_updateDecorationState(_activity->getProxy(), _activity->getActivity()->clazz,
				[&](const jni::Ref &window, const jni::Ref &ic) {
			app->WindowInsetsController.setSystemBarsAppearance(ic,
					app->WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS(),
					app->WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS());
		});
		updateState(0, _info->state | WindowState::DecorationNavigationLight);
		return true;
	} else if (state == WindowState::DecorationShowBySwipe) {
		AndroidWindow_updateDecorationState(_activity->getProxy(), _activity->getActivity()->clazz,
				[&](const jni::Ref &window, const jni::Ref &ic) {
			app->WindowInsetsController.setSystemBarsBehavior(ic,
					app->WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE());
		});
		updateState(0, _info->state | WindowState::DecorationShowBySwipe);
		return true;
	}

	return false;
}

bool AndroidWindow::disableState(WindowState state) {
	if (NativeWindow::disableState(state)) {
		return true;
	}

	auto app = jni::Env::getApp();
	auto env = jni::Env::getEnv();

	if (state == WindowState::DecorationNavigationVisible) {
		AndroidWindow_updateDecorationState(_activity->getProxy(), _activity->getActivity()->clazz,
				[&](const jni::Ref &window, const jni::Ref &ic) {
			app->WindowInsetsController.hide(ic,
					app->WindowInsetType.navigationBars(app->WindowInsetType.getClass().ref(env)));
		});
		return true;
	} else if (state == WindowState::DecorationStatusBarVisible) {
		AndroidWindow_updateDecorationState(_activity->getProxy(), _activity->getActivity()->clazz,
				[&](const jni::Ref &window, const jni::Ref &ic) {
			app->WindowInsetsController.hide(ic,
					app->WindowInsetType.statusBars(app->WindowInsetType.getClass().ref(env)));
		});
		return true;
	} else if (state == WindowState::DecorationStatusBarLight) {
		AndroidWindow_updateDecorationState(_activity->getProxy(), _activity->getActivity()->clazz,
				[&](const jni::Ref &window, const jni::Ref &ic) {
			app->WindowInsetsController.setSystemBarsAppearance(ic, 0,
					app->WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS());
		});
		updateState(0, _info->state & ~WindowState::DecorationStatusBarLight);
		return true;
	} else if (state == WindowState::DecorationNavigationLight) {
		AndroidWindow_updateDecorationState(_activity->getProxy(), _activity->getActivity()->clazz,
				[&](const jni::Ref &window, const jni::Ref &ic) {
			app->WindowInsetsController.setSystemBarsAppearance(ic, 0,
					app->WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS());
		});
		updateState(0, _info->state & ~WindowState::DecorationNavigationLight);
		return true;
	} else if (state == WindowState::DecorationShowBySwipe) {
		AndroidWindow_updateDecorationState(_activity->getProxy(), _activity->getActivity()->clazz,
				[&](const jni::Ref &window, const jni::Ref &ic) {
			app->WindowInsetsController.setSystemBarsBehavior(ic,
					app->WindowInsetsController.BEHAVIOR_DEFAULT());
		});
		updateState(0, _info->state & ~WindowState::DecorationShowBySwipe);
		return true;
	}

	return false;
}

void AndroidWindow::updateWindow(bool sync) {
	auto density = acquireWindowDensity();
	auto extent = Extent2(ANativeWindow_getWidth(_window), ANativeWindow_getHeight(_window));
	if (_extent != extent || _info->density != density) {
		auto flags = (_extent != extent) ? core::UpdateConstraintsFlags::WindowResized
										 : core::UpdateConstraintsFlags::None;
		if (flags != core::UpdateConstraintsFlags::None) {
			flags |= core::UpdateConstraintsFlags::SyncUpdate;
		}
		_extent = extent;
		_info->density = density;
		_controller->notifyWindowConstraintsChanged(this, flags);
	}
}

void AndroidWindow::setContentPadding(const Padding &padding) {
	if (_info->decorationInsets != padding) {
		XL_ANDROID_LOG("AndroidWindow::setContentPadding ", (void *)_window, " ", padding);
		_info->decorationInsets = padding;
		_controller->notifyWindowConstraintsChanged(this, core::UpdateConstraintsFlags::None);
	}
}

void AndroidWindow::setVsyncPeriod(uint64_t v) {
	if (v != _vsyncPeriodNanos) {
		_vsyncPeriodNanos = v;
		_controller->notifyWindowConstraintsChanged(this, core::UpdateConstraintsFlags::None);
	}
}

void AndroidWindow::postDisplayLink() {
	if (_appWindow && !_unmapped) {
		_appWindow->update(core::PresentationUpdateFlags::DisplayLink);
	}
}

void AndroidWindow::updateLayers(Vector<WindowLayer> &&layers) {
	if (_layers != layers) {
		NativeWindow::updateLayers(sp::move(layers));

		bool hasBackButtonHandler = false;
		for (auto &it : _layers) {
			if (hasFlag(it.flags, WindowLayerFlags::BackButtonHandler)) {
				hasBackButtonHandler = true;
			}
		}

		_activity->setBackButtonHandlerEnabled(hasBackButtonHandler);
	}
}

void AndroidWindow::handleBackButton() { _activity->handleBackButton(); }

bool AndroidWindow::updateTextInput(const TextInputRequest &, TextInputFlags flags) {
	return false;
}

void AndroidWindow::cancelTextInput() { }

void AndroidWindow::postFrameCallback() {
	struct AndroidWindowFrameData {
		AndroidWindow *window;
	};

	auto data = new AndroidWindowFrameData;
	data->window = this;

	if (_choreographer) {
		if (_AChoreographer_postVsyncCallback) {
			_AChoreographer_postVsyncCallback(_choreographer,
					[](const AChoreographerFrameCallbackData *callbackData, void *data) {
				auto d = reinterpret_cast<AndroidWindowFrameData *>(data);
				d->window->postDisplayLink();
				delete d;
			}, data);
		} else if (_AChoreographer_postFrameCallback64) {
			_AChoreographer_postFrameCallback64(_choreographer,
					[](int64_t frameTimeNanos, void *data) {
				auto d = reinterpret_cast<AndroidWindowFrameData *>(data);
				d->window->postDisplayLink();
				delete d;
			}, data);
		} else {
			AChoreographer_postFrameCallback(_choreographer, [](long frameTimeNanos, void *data) {
				auto d = reinterpret_cast<AndroidWindowFrameData *>(data);
				d->window->postDisplayLink();
				delete d;
			}, data);
		}
	}
}

float AndroidWindow::acquireWindowDensity() const {
	auto app = jni::Env::getApp();
	auto ref = jni::Ref(_activity->getActivity()->clazz, jni::Env::getEnv());
	auto proxy = _activity->getProxy();

	auto wm = proxy->Activity.getWindowManager(ref);
	if (app->WindowMetrics && app->WindowMetrics.getDensity
			&& app->WindowManager.getCurrentWindowMetrics) {
		auto metrics = app->WindowManager.getCurrentWindowMetrics(wm);
		return app->WindowMetrics.getDensity(metrics);
	} else {
		jni::Local display;
		if (proxy->Activity.getDisplay) {
			display = proxy->Activity.getDisplay(ref);
		} else {
			display = app->WindowManager.getDefaultDisplay(wm);
		}

		auto dm = app->DisplayMetrics.constructor(app->DisplayMetrics.getClass().ref(ref.getEnv()));
		app->Display.getMetrics(display, dm);

		return app->DisplayMetrics.density(dm);
	}
	return _info->density;
}

Status AndroidWindow::setPreferredFrameRate(float value) {
	if (_ANativeWindow_setFrameRateWithChangeStrategy) {
		return sprt::status::errnoToStatus(_ANativeWindow_setFrameRateWithChangeStrategy(_window,
				value, 0,
				hasFlag(_info->flags, WindowCreationFlags::OnlySeamlessFrameRateSwitch) ? 0 : 1));
	} else if (_ANativeWindow_setFrameRate) {
		return sprt::status::errnoToStatus(_ANativeWindow_setFrameRate(_window, value, 0));
	}
	return Status::ErrorNotImplemented;
}

} // namespace stappler::xenolith::platform
