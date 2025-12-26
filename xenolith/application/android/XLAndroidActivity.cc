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

#include "XLAndroidActivity.h"
#include "XLAndroidContextController.h"
#include "XLAndroidInput.h"
#include "XLAppWindow.h"
#include "SPThread.h"

#include <android/native_activity.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

static void AppSupportActivity_handleActivityResult(JNIEnv *env, jobject thiz, jlong native_pointer,
		jint request_code, jint result_code, jobject data) {
	auto activity = reinterpret_cast<AndroidActivity *>(native_pointer);
	if (activity) {
		activity->handleActivityResult(request_code, result_code, data);
	}
}

static void AppSupportActivity_handleInsetsVisible(JNIEnv *env, jobject thiz, jlong native_pointer,
		jboolean statusBarVisible, jboolean navigationVisible) {
	auto activity = reinterpret_cast<AndroidActivity *>(native_pointer);
	if (activity) {
		activity->handleInsetsVisible(statusBarVisible, navigationVisible);
	}
}

static void AppSupportActivity_handleContentInsets(JNIEnv *env, jobject thiz, jlong native_pointer,
		jint top, jint right, jint bottom, jint left) {
	auto activity = reinterpret_cast<AndroidActivity *>(native_pointer);
	if (activity) {
		activity->handleContentInsets(Padding(top, right, bottom, left));
	}
}

static void AppSupportActivity_handleImeInsets(JNIEnv *env, jobject thiz, jlong native_pointer,
		jint top, jint right, jint bottom, jint left) {
	auto activity = reinterpret_cast<AndroidActivity *>(native_pointer);
	if (activity) {
		activity->handleImeInsets(Padding(top, right, bottom, left));
	}
}

static void AppSupportActivity_handleBackInvoked(JNIEnv *env, jobject thiz, jlong native_pointer) {
	auto activity = reinterpret_cast<AndroidActivity *>(native_pointer);
	if (activity) {
		activity->handleBackInvoked();
	}
}

static JNINativeMethod s_activityMethods[] = {
	{"handleActivityResult", "(JIILandroid/content/Intent;)V",
		reinterpret_cast<void *>(&AppSupportActivity_handleActivityResult)},
	{"handleInsetsVisible", "(JZZ)V",
		reinterpret_cast<void *>(&AppSupportActivity_handleInsetsVisible)},
	{"handleContentInsets", "(JIIII)V",
		reinterpret_cast<void *>(&AppSupportActivity_handleContentInsets)},
	{"handleImeInsets", "(JIIII)V", reinterpret_cast<void *>(&AppSupportActivity_handleImeInsets)},
	{"handleBackInvoked", "(J)V", reinterpret_cast<void *>(&AppSupportActivity_handleBackInvoked)},
};

static void registerActivityMethods(const jni::RefClass &cl) {
	static std::mutex s_mutex;
	static std::set<std::string> s_classes;

	s_mutex.lock();
	auto className = cl.getName();
	auto classNameStr = StringView(className.getString()).str<memory::StandartInterface>();

	auto it = s_classes.find(classNameStr);
	if (it == s_classes.end()) {
		cl.registerNatives(s_activityMethods);
		s_classes.emplace(classNameStr);
	}
	s_mutex.unlock();
}

bool AndroidActivity::init(AndroidContextController *controller, ANativeActivity *a,
		BytesView data) {
	_controller = controller;
	_activity = a;
	_proxy = Rc<ActivityProxy>::create(a);

	auto thiz = jni::Ref(a->clazz, a->env);
	_proxy->Activity.setNative(thiz, reinterpret_cast<jlong>(this));

	registerActivityMethods(thiz.getClass());

	return true;
}

bool AndroidActivity::run() {
	memory::context ctx(thread::ThreadInfo::getThreadInfo()->threadPool);

	_activity->instance = this;

	_activity->callbacks->onContentRectChanged = [](ANativeActivity *a, const ARect *r) {
		reinterpret_cast<AndroidActivity *>(a->instance)
				->handleContentRectChanged(Padding(r->top, r->right, r->bottom, r->left));
	};

	_activity->callbacks->onDestroy = [](ANativeActivity *a) {
		reinterpret_cast<AndroidActivity *>(a->instance)->handleDestroy();
	};
	_activity->callbacks->onInputQueueCreated = [](ANativeActivity *a, AInputQueue *queue) {
		reinterpret_cast<AndroidActivity *>(a->instance)->handleInputQueueCreated(queue);
	};
	_activity->callbacks->onInputQueueDestroyed = [](ANativeActivity *a, AInputQueue *queue) {
		reinterpret_cast<AndroidActivity *>(a->instance)->handleInputQueueDestroyed(queue);
	};

	_activity->callbacks->onNativeWindowCreated = [](ANativeActivity *a, ANativeWindow *window) {
		reinterpret_cast<AndroidActivity *>(a->instance)->handleNativeWindowCreated(window);
	};

	_activity->callbacks->onNativeWindowDestroyed = [](ANativeActivity *a, ANativeWindow *window) {
		reinterpret_cast<AndroidActivity *>(a->instance)->handleNativeWindowDestroyed(window);
	};

	_activity->callbacks->onNativeWindowRedrawNeeded = [](ANativeActivity *a,
															   ANativeWindow *window) {
		reinterpret_cast<AndroidActivity *>(a->instance)->handleNativeWindowRedrawNeeded(window);
	};

	_activity->callbacks->onNativeWindowResized = [](ANativeActivity *a, ANativeWindow *window) {
		reinterpret_cast<AndroidActivity *>(a->instance)->handleNativeWindowResized(window);
	};
	_activity->callbacks->onPause = [](ANativeActivity *a) {
		reinterpret_cast<AndroidActivity *>(a->instance)->handlePause();
	};
	_activity->callbacks->onResume = [](ANativeActivity *a) {
		reinterpret_cast<AndroidActivity *>(a->instance)->handleResume();
	};
	_activity->callbacks->onSaveInstanceState = [](ANativeActivity *a, size_t *outLen) {
		return reinterpret_cast<AndroidActivity *>(a->instance)->handleSaveInstanceState(outLen);
	};
	_activity->callbacks->onStart = [](ANativeActivity *a) {
		reinterpret_cast<AndroidActivity *>(a->instance)->handleStart();
	};
	_activity->callbacks->onStop = [](ANativeActivity *a) {
		reinterpret_cast<AndroidActivity *>(a->instance)->handleStop();
	};
	_activity->callbacks->onWindowFocusChanged = [](ANativeActivity *a, int focused) {
		reinterpret_cast<AndroidActivity *>(a->instance)->handleWindowFocusChanged(focused);
	};

	return true;
}

void AndroidActivity::notifyWindowInputEvents(Vector<core::InputEventData> &&vec) {
	for (auto &it : vec) {
		if (it.hasLocation()) {
			it.input.y = ANativeWindow_getHeight(_window->getWindow()) - it.input.y;
		}
	}
	_window->handleInputEvents(sp::move(vec));
}

void AndroidActivity::notifyEnableState(WindowState state) {
	_window->updateState(0, _window->getInfo()->state | state);
}

void AndroidActivity::notifyDisableState(WindowState state) {
	_window->updateState(0, _window->getInfo()->state & ~state);
}

ActivityProxy *AndroidActivity::getProxy() const { return _proxy; }

Context *AndroidActivity::getContext() const { return _controller->getContext(); }

void AndroidActivity::handleActivityResult(jint request, jint result, jobject data) {
	XL_ANDROID_LOG("AndroidActivity::handleActivityResult: ", request, " ", result);
}

void AndroidActivity::handleInsetsVisible(bool statusBarVisible, bool navigationVisible) {
	XL_ANDROID_LOG("AndroidActivity::handleInsetsVisible: ", statusBarVisible, " ",
			navigationVisible);

	WindowState decorationState = WindowState::None;
	if (statusBarVisible) {
		decorationState |= WindowState::DecorationStatusBarVisible;
	}
	if (navigationVisible) {
		decorationState |= WindowState::DecorationNavigationVisible;
	}

	auto app = jni::Env::getApp();
	auto env = jni::Env::getEnv();

	if (app->WindowInsetsController) {
		auto w = _proxy->Activity.getWindow(jni::Ref(_activity->clazz, env));
		auto ic = app->Window.getInsetsController(w);
		if (ic) {
			auto b = app->WindowInsetsController.getSystemBarsBehavior(ic);
			if (b == app->WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE()) {
				decorationState |= WindowState::DecorationShowBySwipe;
			}

			auto a = app->WindowInsetsController.getSystemBarsAppearance(ic);

			if (hasFlag(a, app->WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS())) {
				decorationState |= WindowState::DecorationStatusBarLight;
			}
			if (hasFlag(a, app->WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS())) {
				decorationState |= WindowState::DecorationNavigationLight;
			}
		}
	}

	if (_decorationState != decorationState) {
		_decorationState = decorationState;

		if (_window) {
			auto newState =
					(_window->getInfo()->state & ~WindowState::DecorationState) | _decorationState;
			_window->updateState(0, newState);
		}
	}
}

void AndroidActivity::handleContentInsets(const Padding &p) {
	getContext()->performTemporary([&] {
		XL_ANDROID_LOG("AndroidActivity::AndroidActivity::handleContentInsets ", (void *)_activity,
				" ", p);
		_contentInsets = p;
		updateInsets();
	});
}

void AndroidActivity::handleImeInsets(const Padding &p) {
	getContext()->performTemporary([&] {
		XL_ANDROID_LOG("AndroidActivity::AndroidActivity::handleImeInsets ", (void *)_activity, " ",
				p);
		_imeInsets = p;
		updateInsets();
	});
}

void AndroidActivity::handleBackInvoked() {
	for (auto &it : _input) { it.second->handleBackInvoked(); }
}

void AndroidActivity::handleDisplayChanged() {
	if (_window) {
		_window->updateWindow(false);
	}
}

void AndroidActivity::finish() {
	_proxy->Activity.finishAffinity(jni::Ref(_activity->clazz, jni::Env::getEnv()));
}

void AndroidActivity::handleBackButton() {
	XL_ANDROID_LOG("AndroidActivity::handleBackButton");
	_proxy->Activity.onBackPressed(jni::Ref(_activity->clazz, jni::Env::getEnv()));
}

void AndroidActivity::setBackButtonHandlerEnabled(bool enabled) {
	_proxy->Activity.setBackButtonHandlerEnabled(jni::Ref(_activity->clazz, jni::Env::getEnv()),
			jboolean(enabled));
}

void AndroidActivity::handleContentRectChanged(const Padding &p) {
	getContext()->performTemporary([&] {
		XL_ANDROID_LOG("AndroidActivity::handleContentRectChanged ", (void *)_activity, " ", p);
	});
}

void AndroidActivity::handleInputQueueCreated(AInputQueue *queue) {
	getContext()->performTemporary([&] {
		XL_ANDROID_LOG("AndroidActivity::handleInputQueueCreated ", (void *)_activity);
		_input.emplace(queue, Rc<InputQueue>::create(this, queue));
	});
}

void AndroidActivity::handleInputQueueDestroyed(AInputQueue *queue) {
	getContext()->performTemporary([&] {
		XL_ANDROID_LOG("AndroidActivity::handleInputQueueDestroyed ", (void *)_activity);
		_input.erase(queue);
	});
}

void *AndroidActivity::handleSaveInstanceState(size_t *outLen) {
	void *ret = nullptr;
	getContext()->performTemporary([&] {
		XL_ANDROID_LOG("AndroidActivity::handleSaveInstanceState ", (void *)_activity);

		auto stateData = _controller->saveContextState();
		if (stateData) {
			auto newData = data::write(stateData, EncodeFormat::CborCompressed);

			*outLen = newData.size();
			ret = ::malloc(newData.size());
			::__sprt_memcpy(ret, newData.data(), newData.size());
		}
	});

	return ret;
}

void AndroidActivity::handleNativeWindowCreated(ANativeWindow *window) {
	getContext()->performTemporary([&] {
		XL_ANDROID_LOG("AndroidActivity::handleNativeWindowCreated ", (void *)_activity, " ",
				(void *)window, " ", ANativeWindow_getWidth(window), " ",
				ANativeWindow_getHeight(window));

		Rc<AndroidWindow> win;
		auto wInfo = _controller->makeWindowInfo(window);

		// override window id with Activity class name
		wInfo->id += toString(":", _proxy->Activity.getClass().getName().getString());

		if (_controller->configureWindow(wInfo)) {
			win = Rc<AndroidWindow>::create(this, move(wInfo), window);
			if (win) {
				win->setContentPadding(_fullInsets);
				_window = sp::move(win);
				_controller->notifyWindowCreated(_window);
				return true;
			}
		}

		return false;
	});
}

void AndroidActivity::handleNativeWindowDestroyed(ANativeWindow *window) {
	getContext()->performTemporary([&] {
		XL_ANDROID_LOG("AndroidActivity::handleNativeWindowDestroyed ", (void *)_activity, " ",
				(void *)window);

		if (_window->getWindow() == window) {
			_controller->notifyWindowClosed(_window,
					WindowCloseOptions::CloseInPlace | WindowCloseOptions::IgnoreExitGuard);
			_window = nullptr;
		}
	});
}

void AndroidActivity::handleNativeWindowRedrawNeeded(ANativeWindow *window) {
	getContext()->performTemporary([&] {
		XL_ANDROID_LOG("AndroidActivity::handleNativeWindowRedrawNeeded ", (void *)_activity, " ",
				(void *)window);
		if (_window->getWindow() == window) {
			_window->getAppWindow()->waitUntilFrame();
		}
	});
}

void AndroidActivity::handleNativeWindowResized(ANativeWindow *window) {
	getContext()->performTemporary([&] {
		XL_ANDROID_LOG("AndroidActivity::handleNativeWindowResized ", (void *)_activity, " ",
				(void *)window, " ", ANativeWindow_getWidth(window), " ",
				ANativeWindow_getHeight(window));

		if (_window->getWindow() == window) {
			_window->updateWindow(true);
		}
	});
}

void AndroidActivity::handlePause() {
	getContext()->performTemporary(
			[&] { XL_ANDROID_LOG("AndroidActivity::handlePause ", (void *)_activity); });
}

void AndroidActivity::handleStart() {
	getContext()->performTemporary([&] {
		XL_ANDROID_LOG("AndroidActivity::handleStart ", (void *)_activity);
		if (_window) {
			_window->updateState(0, _window->getInfo()->state | WindowState::Background);
		}
	});
}

void AndroidActivity::handleResume() {
	getContext()->performTemporary(
			[&] { XL_ANDROID_LOG("AndroidActivity::handleResume ", (void *)_activity); });
}

void AndroidActivity::handleStop() {
	getContext()->performTemporary([&] {
		XL_ANDROID_LOG("AndroidActivity::handleStop ", (void *)_activity);
		if (_window) {
			_window->updateState(0, _window->getInfo()->state & ~WindowState::Background);
		}
	});
}

void AndroidActivity::handleDestroy() {
	auto id = retain();
	getContext()->performTemporary([&] {
		XL_ANDROID_LOG("AndroidActivity::handleDestroy: ", (void *)_activity);
		_controller->destroyActivity(this);
	});
	release(id);
}

void AndroidActivity::handleWindowFocusChanged(int focused) {
	getContext()->performTemporary([&] {
		XL_ANDROID_LOG("AndroidActivity::handleWindowFocusChanged: ", (void *)_activity, " ",
				focused);
		if (_window) {
			if (focused) {
				_window->updateState(0, _window->getInfo()->state | WindowState::Focused);
			} else {
				_window->updateState(0, _window->getInfo()->state & ~WindowState::Focused);
			}
		}
	});
}

void AndroidActivity::updateInsets() {
	Padding full(std::max(_contentInsets.top, _imeInsets.top),
			std::max(_contentInsets.right, _imeInsets.right),
			std::max(_contentInsets.bottom, _imeInsets.bottom),
			std::max(_contentInsets.left, _imeInsets.left));
	if (_fullInsets != full) {
		_fullInsets = full;
		if (_window) {
			_window->setContentPadding(_fullInsets);
		}
	}
}

} // namespace stappler::xenolith::platform
