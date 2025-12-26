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

#include "XLAndroidContextController.h"
#include "XLAndroidDisplayConfigManager.h"
#include "XLAndroidActivity.h"
#include "XLAndroid.h"
#include "SPThread.h"
#include "SPUrl.h"

#if MODULE_XENOLITH_BACKEND_VK
#include "XLVkInstance.h"
#endif

#include <android/native_activity.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

static jstring AndroidContextController_getTypeForUri(JNIEnv *env, jobject thiz, jlong ptr,
		jstring uri) {
	auto ctx = reinterpret_cast<AndroidContextController *>(ptr);
	if (ctx) {
		auto str = ctx->getClipboardTypeForUri(jni::RefString(uri, env).getString());
		return jni::Env(env).newStringRef(StringView(str));
	}
	return nullptr;
}

static jstring AndroidContextController_getPathForUri(JNIEnv *env, jobject thiz, jlong ptr,
		jstring uri) {
	auto ctx = reinterpret_cast<AndroidContextController *>(ptr);
	if (ctx) {
		auto str = ctx->getClipboardPathForUri(jni::RefString(uri, env).getString());
		return jni::Env(env).newStringRef(StringView(str));
	}
	return nullptr;
}

static JNINativeMethod s_clipboardContentProviderMethods[] = {
	{"getTypeForUri", "(JLjava/lang/String;)Ljava/lang/String;",
		reinterpret_cast<void *>(&AndroidContextController_getTypeForUri)},
	{"getPathForUri", "(JLjava/lang/String;)Ljava/lang/String;",
		reinterpret_cast<void *>(&AndroidContextController_getPathForUri)},
};

static void registerClipboardContentProviderMethods(const jni::RefClass &cl) {
	static std::mutex s_mutex;
	static std::set<std::string> s_classes;

	s_mutex.lock();
	auto className = cl.getName();
	auto classNameStr = StringView(className.getString()).str<memory::StandartInterface>();

	auto it = s_classes.find(classNameStr);
	if (it == s_classes.end()) {
		cl.registerNatives(s_clipboardContentProviderMethods);
		s_classes.emplace(classNameStr);
	}
	s_mutex.unlock();
}

void AndroidContextController::acquireDefaultConfig(ContextConfig &cfg) {
	auto env = jni::Env::getEnv();
	auto config = jni::Env::getApp()->getCurrentInfo();

	auto formatSupport = NativeBufferFormatSupport::get();

	if (config->isEmulator) {
		// emulators often do not support this format for swapchains
		formatSupport.R8G8B8A8_UNORM = false;
		formatSupport.R8G8B8X8_UNORM = false;
	}

	env.checkErrors();

	if (!cfg.context) {
		cfg.context = Rc<ContextInfo>::alloc();
	}

	cfg.context->bundleName = StringView(config->bundleName).str<Interface>();
	cfg.context->appName = StringView(config->applicationName).str<Interface>();
	cfg.context->appVersion = StringView(config->applicationVersion).str<Interface>();
	cfg.context->userLanguage = StringView(config->locale).str<Interface>();
	cfg.context->userAgent = StringView(config->userAgent).str<Interface>();
	cfg.context->appVersionCode = SP_MAKE_API_VERSION(cfg.context->appVersion);

	if (!cfg.loop) {
		cfg.loop = Rc<core::LoopInfo>::alloc();
	}

	if (formatSupport.valid) {
		if (formatSupport.R8G8B8A8_UNORM || formatSupport.R8G8B8X8_UNORM) {
			cfg.loop->defaultFormat = cfg.window->imageFormat = core::ImageFormat::R8G8B8A8_UNORM;
		} else if (formatSupport.R10G10B10A2_UNORM) {
			cfg.loop->defaultFormat = cfg.window->imageFormat =
					core::ImageFormat::A2B10G10R10_UNORM_PACK32;
		} else if (formatSupport.R16G16B16A16_FLOAT) {
			cfg.loop->defaultFormat = cfg.window->imageFormat =
					core::ImageFormat::R16G16B16A16_SFLOAT;
		} else if (formatSupport.R5G6B5_UNORM) {
			cfg.loop->defaultFormat = cfg.window->imageFormat =
					core::ImageFormat::R5G6B5_UNORM_PACK16;
		}
	} else {
		cfg.loop->defaultFormat = cfg.window->imageFormat = core::ImageFormat::R8G8B8A8_UNORM;
	}

	if (!cfg.instance) {
		cfg.instance = Rc<core::InstanceInfo>::alloc();
	}

	cfg.instance->api = core::InstanceApi::Vulkan;
	cfg.instance->flags = core::InstanceFlags::Validation;
}

AndroidContextController::~AndroidContextController() {
	auto app = jni::Env::getApp();
	if (app) {
		app->setActivityLoader(nullptr);
	}
}

bool AndroidContextController::init(NotNull<Context> ctx, ContextConfig &&config) {
	if (!ContextController::init(ctx)) {
		return false;
	}

	auto env = jni::Env::getEnv();
	auto app = jni::Env::getApp();

	auto clipboardContentProviderClass = ClipboardContentProvider.getClass().ref(env);

	filesystem::remove(FileInfo("clipboard_content", FileCategory::AppCache), true, true);

	registerClipboardContentProviderMethods(clipboardContentProviderClass);

	// try to bind with clipboard content provider
	ClipboardContentProvider.thiz = ClipboardContentProvider.Self(clipboardContentProviderClass);

	if (ClipboardContentProvider.thiz) {
		ClipboardContentProvider.setNative(ClipboardContentProvider.thiz.ref(env),
				reinterpret_cast<jlong>(this));
		_clipboardAuthority = StringView(
				ClipboardContentProvider.getAuthority(ClipboardContentProvider.thiz.ref(env))
						.getString())
									  .str<Interface>();
	}

	_contextInfo = move(config.context);
	_windowInfo = move(config.window);
	_instanceInfo = move(config.instance);
	_loopInfo = move(config.loop);

	Value info;


	auto classLoader = jni::Env::getClassLoader();
	if (classLoader) {
		auto ctx = app->jApplication.ref(jni::Env::getEnv());
		_networkConnectivity = Rc<NetworkConnectivity>::create(ctx, [this](NetworkFlags flags) {
			if (_looper) {
				_looper->performOnThread([this, flags] { handleNetworkStateChanged(flags); }, this);
			}
		});

		if (_networkConnectivity) {
			handleNetworkStateChanged(_networkConnectivity->flags);
		}

		_clipboardListener = Rc<ClipboardListener>::create(ctx, [this]() {
			if (_looper) {
				_looper->performOnThread([this] { handleClipboardUpdate(); }, this);
			}
		});
	}

	auto v = info.emplace("drawables");
	app->inspectDrawables([&](StringView name, jint idx) { v.setInteger(idx, name); });

	info.setString(_contextInfo->bundleName, "bundleName");
	info.setString(_contextInfo->appName, "applicationName");
	info.setString(_contextInfo->appVersion, "applicationVersion");
	info.setString(_contextInfo->userAgent, "userAgent");
	info.setString(_contextInfo->userLanguage, "locale");
	info.setDouble(_windowInfo->density, "density");
	info.setValue(Value({Value(_windowInfo->rect.width), Value(_windowInfo->rect.height)}), "size");
	info.setInteger(app->sdkVersion, "sdk");

	saveApplicationInfo(info);

	// We use epoll-based looper instead of ALooper-based to use wait/run on it. locking all other android processing
	// It's critical to correctly process onNativeWindowRedrawNeeded event
	//
	// Epoll fd can be added to general system ALooper with ALooper_addFd + _looper->getQueue()->getHandle()
	_looper = event::Looper::acquire(event::LooperInfo{
		.workersCount = _contextInfo->mainThreadsCount,
		.engineMask = event::QueueEngine::EPoll,
	});

	return true;
}

int AndroidContextController::run(NotNull<ContextContainer> c) {
	memory::context ctx(thread::ThreadInfo::getThreadInfo()->threadPool);

	_displayConfigManager =
			Rc<AndroidDisplayConfigManager>::create(this, [this](NotNull<DisplayConfigManager> m) {
		handleSystemNotification(SystemNotification::DisplayChanged);
	});

	auto instance = loadInstance();
	if (!instance) {
		log::source().error("AndroidContextController", "Fail to load gAPI instance");
		_resultCode = -1;
		return -1;
	} else {
		if (auto loop = makeLoop(instance)) {
			_context->handleGraphicsLoaded(loop);
			loop = nullptr;
		} else {
			log::source().error("AndroidContextController", "Fail to create gAPI loop");
			_resultCode = -1;
			return -1;
		}
	}

	auto alooper = ALooper_forThread();

	if (_looper) {
		ALooper_addFd(alooper, _looper->getQueue()->getHandle(), 0, ALOOPER_EVENT_INPUT,
				[](int fd, int events, void *data) {
			auto controller = reinterpret_cast<AndroidContextController *>(data);
			if ((events & ALOOPER_EVENT_INPUT) != 0) {
				controller->_looper->poll();
			}
			return 1;
		}, this);
	}

	_container = c;

	auto app = jni::Env::getApp();
	app->setActivityLoader([this](ANativeActivity *a, sprt::BytesView data) {
		return loadActivity(a, BytesView(data.data(), data.size()));
	});

	app->setLowMemoryHandler([this] {
		log::source().info("AndroidContextController", "onLowMemory");
		handleSystemNotification(SystemNotification::LowMemory);
	});

	app->setConfigurationHandler([this](sprt::jni::ApplicationInfo *info) {
		log::source().info("AndroidContextController", "onConfigurationChanged");
		handleSystemNotification(SystemNotification::ConfigurationChanged);
	});

	resume();
	return 0;
}

bool AndroidContextController::isCursorSupported(WindowCursor, bool serverSide) const {
	return false;
}

WindowCapabilities AndroidContextController::getCapabilities() const {
	auto caps = WindowCapabilities::PreserveDirector;
	if (jni::Env::getApp()->sdkVersion >= 30) {
		caps |= WindowCapabilities::PreferredFrameRate | WindowCapabilities::DecorationState;
	}
	return caps;
}

bool AndroidContextController::loadActivity(ANativeActivity *a, BytesView data) {
	auto activity = Rc<AndroidActivity>::create(this, a, data);
	if (activity) {
		if (_stopTimer) {
			_stopTimer->cancel();
			_stopTimer = nullptr;
		}
		resume();
		if (activity->run()) {
			_activities.emplace(activity);
			return true;
		}
	}
	return false;
}

void AndroidContextController::destroyActivity(NotNull<AndroidActivity> a) {
	_activities.erase(a.get());
	if (_activities.empty()) {
		if (_stopTimer) {
			_stopTimer->cancel();
			_stopTimer = nullptr;
		}
		_stopTimer = _looper->schedule(TimeInterval::seconds(19),
				[this](event::Handle *, bool success) { stop(); });
	}
}

jni::Ref AndroidContextController::getSelf() const {
	auto app = jni::Env::getApp();
	auto env = jni::Env::getEnv();

	return app->jApplication.ref(env);
}

Rc<WindowInfo> AndroidContextController::makeWindowInfo(ANativeWindow *w) const {
	auto appInfo = sprt::jni::Env::getApp()->getCurrentInfo();
	auto window = Rc<WindowInfo>::alloc();

	auto info = _context->getInfo();

	window->id = info->bundleName;
	window->title = info->appName;
	window->rect = IRect{0, 0, static_cast<uint32_t>(ANativeWindow_getWidth(w)),
		uint32_t(ANativeWindow_getHeight(w))};
	window->density = appInfo->density;

	if (auto loop = _context->getGlLoop()) {
		window->imageFormat = loop->getInfo()->defaultFormat;
	}

	window->flags = WindowCreationFlags::Regular;
	return window;
}

Status AndroidContextController::readFromClipboard(Rc<ClipboardRequest> &&req) {
	auto app = jni::Env::getApp();
	auto env = jni::Env::getEnv();

	auto manager = app->ClipboardManager.service.ref(env);

	auto clipData = app->ClipboardManager.getPrimaryClip(manager);
	if (clipData) {
		auto desc = app->ClipData.getDescription(clipData);
		if (!desc) {
			req->dataCallback(Status::ErrorInvalidArguemnt, BytesView(), StringView());
			return Status::ErrorInvalidArguemnt;
		}

		Vector<StringView> types;
		auto nTypes = app->ClipDescription.getMimeTypeCount(desc);
		for (uint32_t idx = 0; idx < nTypes; ++idx) {
			auto str = app->ClipDescription.getMimeType(desc, jint(idx));
			types.emplace_back(StringView(str.getString()).pdup());
		}

		auto type = req->typeCallback(types);

		auto typeIt = std::find(types.begin(), types.end(), type);
		if (typeIt == types.end()) {
			req->dataCallback(Status::ErrorInvalidArguemnt, BytesView(), StringView());
			return Status::ErrorInvalidArguemnt;
		}

		auto item = app->ClipData.getItemAt(clipData, jint(typeIt - types.begin()));
		if (!item) {
			req->dataCallback(Status::ErrorInvalidArguemnt, BytesView(), StringView());
			return Status::ErrorInvalidArguemnt;
		}

		auto uri = app->ClipDataItem.getUri(item);
		if (uri) {
			auto resolver = app->Application.getContentResolver(app->jApplication.ref(env));
			auto stream = app->ContentResolver.openInputStream(resolver, uri);
			if (!stream) {
				req->dataCallback(Status::ErrorInvalidArguemnt, BytesView(), StringView());
				return Status::ErrorInvalidArguemnt;
			}

			readClipboardStream(sp::move(req), stream, type);
			return Status::Ok;
		} else {
			// try text
			auto textSeq = app->ClipDataItem.getText(item);
			if (!textSeq) {
				req->dataCallback(Status::ErrorInvalidArguemnt, BytesView(), StringView());
				return Status::ErrorInvalidArguemnt;
			}

			auto str = app->CharSequence.toString(textSeq);
			auto strData = str.getString();

			req->dataCallback(Status::ErrorInvalidArguemnt, BytesView(strData), type);
			return Status::Ok;
		}
	} else {
		req->dataCallback(Status::Declined, BytesView(), StringView());
	}

	return Status::Declined;
}

Status AndroidContextController::probeClipboard(Rc<ClipboardProbe> &&probe) {
	auto app = jni::Env::getApp();
	auto env = jni::Env::getEnv();

	auto manager = app->ClipboardManager.service.ref(env);

	auto desc = app->ClipboardManager.getPrimaryClipDescription(manager);
	if (desc) {
		Vector<StringView> types;
		auto nTypes = app->ClipDescription.getMimeTypeCount(desc);
		for (uint32_t idx = 0; idx < nTypes; ++idx) {
			auto str = app->ClipDescription.getMimeType(desc, jint(idx));
			types.emplace_back(StringView(str.getString()).pdup());
		}

		probe->typeCallback(Status::Ok, types);
	} else {
		probe->typeCallback(Status::Declined, SpanView<StringView>());
	}
	return Status::Ok;
}

Status AndroidContextController::writeToClipboard(Rc<ClipboardData> &&data) {
	auto app = jni::Env::getApp();
	auto env = jni::Env::getEnv();

	if (data->types.size() == 0) {
		if (_clipboardClip) {
			auto manager = app->ClipboardManager.service.ref(env);
			auto clipData = app->ClipboardManager.getPrimaryClip(manager);
			if (env.isSame(_clipboardClip, clipData)) {
				app->ClipboardManager.clearPrimaryClip(manager);
			}
		}

		return Status::Declined;
	}

	if (data->label.empty()) {
		data->label = "Xenolith Clipboard";
	}

	if (data->types.size() == 1) {
		auto &type = data->types.front();
		if (StringView(type).starts_with("text/")) {
			auto d = data->encodeCallback(type);

			auto item = app->ClipDataItem.constructorWithText(app->ClipDataItem.getClass().ref(env),
					env.newString(StringView((const char *)d.data(), d.size())));
			auto mimeArray = env.newArray<jstring>(1, env.findClass("java/lang/String"));
			mimeArray.setElement(0, jni::Ref(env.newString(StringView(type))));
			auto clipData = app->ClipData.constructor(app->ClipData.getClass().ref(env),
					env.newString(StringView(data->label)), mimeArray, item);

			app->ClipboardManager.setPrimaryClip(app->ClipboardManager.service.ref(env), clipData);
			return Status::Ok;
		}
	}

	clearClipboard();

	Vector<String> uris;

	auto mimeArray = env.newArray<jstring>(data->types.size(), env.findClass("java/lang/String"));
	jint i = 0;
	for (auto &it : data->types) {
		auto index = i++;

		mimeArray.setElement(index, jni::Ref(env.newString(StringView(it))));

		uris.emplace_back(toString("content://", _clipboardAuthority, "/clipboard_content/",
				data->initial.toMicros(), "/", index, "?displayName=", data->label));
	}

	auto item = app->ClipDataItem.constructorWithUri(app->ClipDataItem.getClass().ref(env),
			app->Uri.parse(app->Uri.getClass().ref(env), env.newString(StringView(uris.front()))));
	auto clipData = app->ClipData.constructor(app->ClipData.getClass().ref(env),
			env.newString(StringView(data->label)), mimeArray, item);
	for (i = 1; i < uris.size(); ++i) {
		item = app->ClipDataItem.constructorWithUri(app->ClipDataItem.getClass().ref(env),
				app->Uri.parse(app->Uri.getClass().ref(env),
						env.newString(StringView(uris.at(i)))));
		app->ClipData.addItem(clipData, item);
	}

	_clipboardData = sp::move(data);
	app->ClipboardManager.setPrimaryClip(app->ClipboardManager.service.ref(env), clipData);

	return Status::Ok;
}

String AndroidContextController::getClipboardTypeForUri(StringView uri) {
	UrlView u(uri);
	if (u.scheme == "content" && u.host == _clipboardAuthority) {
		auto idx = filepath::lastComponent(u.path).readInteger(10).get(0);
		if (_clipboardData && _clipboardData->types.size() > idx) {
			return _clipboardData->types.at(idx);
		}
	}
	return String();
}

String AndroidContextController::getClipboardPathForUri(StringView uri) {
	UrlView u(uri);
	if (u.scheme == "content" && u.host == _clipboardAuthority) {
		auto serial = filepath::lastComponent(filepath::root(u.path)).readInteger(10).get(0);
		auto idx = filepath::lastComponent(u.path).readInteger(10).get(0);
		if (_clipboardData && _clipboardData->types.size() > idx
				&& _clipboardData->initial == serial) {
			auto path = toString("clipboard_content/", _clipboardData->initial.toMicros());
			auto fullPath = filesystem::findPath<Interface>(FileInfo(path, FileCategory::AppCache));
			auto targetPath = toString(fullPath, "/", idx);

			if (!filesystem::exists(FileInfo{targetPath})) {
				filesystem::mkdir_recursive(FileInfo{fullPath});
				auto data = _clipboardData->encodeCallback(_clipboardData->types.at(idx));
				filesystem::write(FileInfo{targetPath}, data);
			}

			return targetPath;
		}
	}
	return String();
}

void AndroidContextController::openUrl(StringView str) {
	if (_activities.empty()) {
		return;
	}

	auto app = jni::Env::getApp();
	auto env = jni::Env::getEnv();

	auto uri = app->Uri.parse(app->Uri.getClass().ref(env), env.newString(str));
	auto intent = app->Intent.constructor(app->Intent.getClass().ref(env),
			app->Intent.ACTION_VIEW(app->Intent.getClass().ref(env)), uri);

	auto it = _activities.begin();

	(*it)->getProxy()->Activity.startActivity(jni::Ref((*it)->getActivity()->clazz, env), intent);
}

Rc<core::Instance> AndroidContextController::loadInstance() {
	Rc<core::Instance> instance;
#if MODULE_XENOLITH_BACKEND_VK
	auto instanceInfo = move(_instanceInfo);
	_instanceInfo = nullptr;

	auto instanceBackendInfo = Rc<vk::InstanceBackendInfo>::create();
	instanceBackendInfo->setup = [this](vk::InstanceData &data, const vk::InstanceInfo &info) {
		auto ctxInfo = _context->getInfo();

		if (info.availableBackends.test(toInt(vk::SurfaceBackend::Android))) {
			data.enableBackends.set(toInt(vk::SurfaceBackend::Android));
		}

		data.applicationName = ctxInfo->appName;
		data.applicationVersion = ctxInfo->appVersion;
		data.checkPresentationSupport = [](const vk::Instance *inst, VkPhysicalDevice device,
												uint32_t queueIdx) {
			// On Android, all physical devices and queue families must be capable of presentation with
			// any native window. As a result there is no Android-specific query for these capabilities.
			vk::SurfaceBackendMask ret;
			ret.set(toInt(vk::SurfaceBackend::Android));
			return ret;
		};
		return true;
	};

	instanceInfo->backend = move(instanceBackendInfo);

	instance = core::Instance::create(move(instanceInfo));
#else
	log::source().error("LinuxContextController", "No available gAPI backends found");
	_resultCode = -1;
#endif
	return instance;
}

void AndroidContextController::readClipboardStream(Rc<ClipboardRequest> &&req,
		const jni::Ref &stream, StringView type) {
	// offload job to background thread
	_looper->performAsync(
			[req = sp::move(req), stream = stream.getGlobal(), type = type.str<Interface>()] {
		auto app = jni::Env::getApp();
		auto env = jni::Env::getEnv();

		auto streamRef = stream.ref(env);
		auto data = app->InputStream.readAllBytes(streamRef);
		auto bytes = data.getArray();

		req->dataCallback(Status::Ok, BytesView((const uint8_t *)bytes.data(), bytes.size()), type);
	}, this);
}

void AndroidContextController::handleClipboardUpdate() {
	auto app = jni::Env::getApp();
	auto env = jni::Env::getEnv();

	if (_clipboardClip) {
		// clear clipboard if primary clip changed
		auto manager = app->ClipboardManager.service.ref(env);
		auto clipData = app->ClipboardManager.getPrimaryClip(manager);
		if (!env.isSame(_clipboardClip, clipData)) {
			clearClipboard();
		}
	}

	_context->handleSystemNotification(SystemNotification::ClipboardChanged);
}

void AndroidContextController::clearClipboard() {
	if (_clipboardData) {
		filesystem::remove(
				FileInfo(toString("clipboard_content/", _clipboardData->initial.toMicros()),
						FileCategory::AppCache),
				true, true);
	}
	_clipboardClip = nullptr;
	_clipboardData = nullptr;
}

} // namespace stappler::xenolith::platform
