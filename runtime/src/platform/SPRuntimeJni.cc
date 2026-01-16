/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
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

#include <sprt/jni/jni.h>

#if SPRT_ANDROID

#include <sprt/c/__sprt_ctype.h>
#include <sprt/runtime/mutex.h>

#include <android/configuration.h>
#include <android/asset_manager.h>
#include <sprt/jni/native_activity.h>

#include "private/SPRTDso.h"
#include "private/SPRTSpecific.h"

#include <math.h>
#include <nl_types.h>
#include <wchar.h>
#include <fcntl.h>

extern "C" AAssetManager *AAssetManager_fromJava(JNIEnv *env, jobject assetManager);

namespace sprt::platform {

static Dso s_self;

int (*_timespec_get)(struct timespec *__ts, int __base) = nullptr;
int (*_timespec_getres)(struct timespec *__ts, int __base) = nullptr;
int (*_getlogin_r)(char *__buffer, size_t __buffer_size) = nullptr;
ssize_t (*_copy_file_range)(int __fd_in, off_t *__off_in, int __fd_out, off_t *__off_out,
		size_t __length, unsigned int __flags) = nullptr;
int (*_futimes)(int __fd, const struct timeval __times[2]) = nullptr;
int (*_lutimes)(const char *__path, const struct timeval __times[2]) = nullptr;
int (*_futimesat)(int __dir_fd, const char *__path, const struct timeval __times[2]) = nullptr;
int (*_sync_file_range)(int __fd, off64_t __offset, off64_t __length,
		unsigned int __flags) = nullptr;
int (*_mlock2)(const void *__addr, size_t __size, int __flags) = nullptr;
int (*_memfd_create)(const char *__name, unsigned __flags) = nullptr;

nl_catd (*_catopen)(const char *__name, int __flag) = nullptr;
char *(*_catgets)(nl_catd __catalog, int __set_number, int __msg_number,
		const char *__msg) = nullptr;
int (*_catclose)(nl_catd __catalog) = nullptr;

int (*_pthread_setschedprio)(pthread_t __pthread, int __priority) = nullptr;

char *(*_ctermid)(char *__buf) = nullptr;

int (*_getsubopt)(char **__option, char *const *__tokens, char **__value_ptr) = nullptr;

int (*_getentropy)(void *__buffer, size_t __buffer_size) = nullptr;

ssize_t (*_getrandom)(void *__buffer, size_t __buffer_size, unsigned int __flags) = nullptr;

size_t (*_wcsftime_l)(wchar_t *__buf, size_t __n, const wchar_t *__fmt, const struct tm *__tm,
		locale_t __l) = nullptr;

void *(*_aligned_alloc)(size_t __alignment, size_t __size) = nullptr;

int (*_creat64)(const char *__path, mode_t __mode) = nullptr;
int (*_open64)(const char *__path, int __flags, ...) = nullptr;
int (*_openat64)(int __dir_fd, const char *__path, int __flags, ...) = nullptr;
int (*_fallocate64)(int __fd, int __mode, off64_t __offset, off64_t __length) = nullptr;
int (*_posix_fadvise64)(int __fd, off64_t __offset, off64_t __length, int __advice) = nullptr;
int (*_posix_fallocate64)(int __fd, off64_t __offset, off64_t __length) = nullptr;

} // namespace sprt::platform

namespace sprt::jni {

static Rc<App> s_app;

struct JavaThread {
	JavaVM *vm = nullptr;
	JNIEnv *env = nullptr;
	bool attached = false;

	~JavaThread() {
		if (attached) {
			vm->DetachCurrentThread();
		}
	}

	bool init(JavaVM *v, JNIEnv *e, bool a) {
		vm = v;
		env = e;
		attached = a;
		return true;
	}
};

static thread_local JavaThread tl_thread;

Local::~Local() {
	if (_obj) {
		_env->DeleteLocalRef(_obj);
	}
}

Local::Local(jobject obj, JNIEnv *env) : _obj(obj), _env(env) { }

Local::Local(Local &&other) {
	_obj = other._obj;
	_env = other._env;
	other._obj = nullptr;
	other._env = nullptr;
}

Local &Local::operator=(Local &&other) {
	if (&other == this) {
		return *this;
	}

	if (_obj) {
		_env->DeleteLocalRef(_obj);
		_obj = nullptr;
	}

	_obj = other._obj;
	_env = other._env;
	other._obj = nullptr;
	other._env = nullptr;
	return *this;
}

Local::Local(sprt::nullptr_t) : _obj(nullptr), _env(nullptr) { }

Local &Local::operator=(sprt::nullptr_t) {
	if (_obj) {
		_env->DeleteLocalRef(_obj);
		_obj = nullptr;
	}

	_obj = nullptr;
	_env = nullptr;
	return *this;
}

Global Local::getGlobal() const { return Global(*this); }

LocalClass::LocalClass(jclass obj, JNIEnv *env) : Local(obj, env) { }

LocalClass::LocalClass(LocalClass &&other) : Local(move_unsafe(other)) { }

LocalClass &LocalClass::operator=(LocalClass &&other) {
	if (&other == this) {
		return *this;
	}
	Local::operator=(move(other));
	return *this;
}

LocalClass::LocalClass(sprt::nullptr_t) : Local(nullptr) { }

LocalClass &LocalClass::operator=(sprt::nullptr_t) {
	if (_obj) {
		_env->DeleteLocalRef(_obj);
		_obj = nullptr;
	}

	_obj = nullptr;
	_env = nullptr;
	return *this;
}


GlobalClass LocalClass::getGlobal() const { return GlobalClass(*this); }

Global::~Global() {
	if (_obj) {
		Env::getEnv().deleteGlobalRef(_obj);
		_obj = nullptr;
	}
}

Global::Global(const Local &obj) {
	if (obj) {
		_obj = obj.getEnv()->NewGlobalRef(obj);
	}
}

Global::Global(const Ref &obj) {
	if (obj) {
		_obj = obj.getEnv()->NewGlobalRef(obj);
	}
}

Global::Global(const Global &other) {
	if (other._obj) {
		_obj = Env::getEnv().newGlobalRef(other._obj);
	}
}

Global &Global::operator=(const Global &other) {
	if (&other == this) {
		return *this;
	}

	auto env = Env::getEnv();

	if (_obj) {
		env.deleteGlobalRef(_obj);
		_obj = nullptr;
	}

	if (other._obj) {
		_obj = env.newGlobalRef(other._obj);
	}
	return *this;
}

Global::Global(Global &&other) {
	_obj = other._obj;
	other._obj = nullptr;
}

Global &Global::operator=(Global &&other) {
	if (&other == this) {
		return *this;
	}

	if (_obj) {
		Env::getEnv().deleteGlobalRef(_obj);
		_obj = nullptr;
	}

	_obj = other._obj;
	other._obj = nullptr;
	return *this;
}

Global::Global(sprt::nullptr_t) : _obj(nullptr) { }

Global &Global::operator=(sprt::nullptr_t) {
	if (_obj) {
		Env::getEnv().deleteGlobalRef(_obj);
		_obj = nullptr;
	}

	_obj = nullptr;
	return *this;
}

JNIEnv *Global::getEnv() const { return Env::getEnv(); }

Ref Global::ref(JNIEnv *env) const { return Ref(*this, env ? env : Env::getEnv().env()); }

LocalString::~LocalString() { reset(); }

LocalString::LocalString(jstring obj, JNIEnv *env) : Local(obj, env) { }

LocalString::LocalString(LocalString &&other) : Local(move(other)) { swap(other); }

LocalString &LocalString::operator=(LocalString &&other) {
	if (&other == this) {
		return *this;
	}

	reset();

	Local::operator=(move(other));
	swap(other);
	return *this;
}

LocalString::LocalString(sprt::nullptr_t) : Local(nullptr) { }

LocalString &LocalString::operator=(sprt::nullptr_t) {
	reset();

	Local::operator=(nullptr);
	return *this;
}

GlobalString LocalString::getGlobal() const { return GlobalString(*this); }

GlobalString::GlobalString(const LocalString &obj) : Global(nullptr) {
	if (obj) {
		_obj = obj.getEnv()->NewGlobalRef(obj);
	}
}

GlobalString::GlobalString(const RefString &obj) : Global(nullptr) {
	if (obj) {
		_obj = obj.getEnv()->NewGlobalRef(obj);
	}
}

GlobalString::GlobalString(const GlobalString &other) : Global(other) { }

GlobalString &GlobalString::operator=(const GlobalString &other) {
	if (&other == this) {
		return *this;
	}

	Global::operator=(other);
	return *this;
}

GlobalString::GlobalString(GlobalString &&other) : Global(move(other)) { }

GlobalString &GlobalString::operator=(GlobalString &&other) {
	if (&other == this) {
		return *this;
	}

	Global::operator=(move(other));
	return *this;
}

GlobalString::GlobalString(sprt::nullptr_t) : Global(nullptr) { }

GlobalString &GlobalString::operator=(sprt::nullptr_t) {
	Global::operator=(nullptr);
	return *this;
}

RefString GlobalString::ref(JNIEnv *env) const {
	return RefString(*this, env ? env : Env::getEnv().env());
}

GlobalClass::GlobalClass(const LocalClass &obj) : Global(nullptr) {
	if (obj) {
		_obj = obj.getEnv()->NewGlobalRef(obj);
	}
}

GlobalClass::GlobalClass(const RefClass &obj) : Global(nullptr) {
	if (obj) {
		_obj = obj.getEnv()->NewGlobalRef(obj);
	}
}

GlobalClass::GlobalClass(const GlobalClass &other) : Global(other) { }

GlobalClass &GlobalClass::operator=(const GlobalClass &other) {
	if (&other == this) {
		return *this;
	}

	Global::operator=(other);
	return *this;
}

GlobalClass::GlobalClass(GlobalClass &&other) : Global(move(other)) { }

GlobalClass &GlobalClass::operator=(GlobalClass &&other) {
	if (&other == this) {
		return *this;
	}

	Global::operator=(move(other));
	return *this;
}

GlobalClass::GlobalClass(sprt::nullptr_t) : Global(nullptr) { }

GlobalClass &GlobalClass::operator=(sprt::nullptr_t) {
	Global::operator=(nullptr);
	return *this;
}

RefClass GlobalClass::ref(JNIEnv *env) const {
	return RefClass(*this, env ? env : Env::getEnv().env());
}

Global Ref::getGlobal() const { return Global(*this); }

RefString::~RefString() { reset(); }

GlobalString RefString::getGlobal() const { return GlobalString(*this); }

GlobalClass RefClass::getGlobal() const { return GlobalClass(*this); }

static sprt::qmutex s_infoMutex;

App *App::alloc(const RefClass &cl) { return new (sprt::nothrow) App(cl); }

App::~App() {
	auto env = Env::getEnv();
	auto jAppRef = jApplication.ref(env);

	Application.setNative(jAppRef, 0);

	if (config) {
		AConfiguration_delete(config);
		config = nullptr;
	}

	classLoader.finalize();
}

App::App(const RefClass &cl) : Application(cl) {
	jApplication = Application.s_application(cl);

	jAssetManager = Application.getAssets(jApplication.ref(cl.getEnv()));
	if (jAssetManager) {
		nAssetManager = AAssetManager_fromJava(cl.getEnv(), jAssetManager);
	}

	auto jAppRef = jApplication.ref(cl.getEnv());

	ClipboardManager.service =
			Application.getSystemService(jAppRef, Application.CLIPBOARD_SERVICE(cl));

	DisplayManager.service = Application.getSystemService(jAppRef, Application.DISPLAY_SERVICE(cl));

	if (auto resObj = Application.getResources(jAppRef)) {
		if (auto jConf = Resources.getConfiguration(resObj)) {
			handleConfigurationChanged(jConf);
		}
	}

	classLoader.init(this, cl.getEnv());
	Application.setNative(jAppRef, reinterpret_cast<jlong>(this));
}

void App::inspectDrawables(const callback<void(StringView, jint)> &cb) {
	auto env = Env::getEnv();
	auto jAppRef = jApplication.ref(Env::getEnv());

	auto packageName = Application.getPackageName(jAppRef);

	auto drawableClassName = StreamTraits<char>::toString(packageName.getString(), ".R$drawable");
	auto drawablesClass = classLoader.findClass(env, drawableClassName);
	if (drawablesClass) {
		classLoader.foreachField(drawablesClass,
				[&](StringView type, StringView name, const jni::Ref &obj) {
			if (type.equals("int")) {
				cb(name, classLoader.getIntField(drawablesClass, obj));
			}
		});
	}
}

void App::handleConfigurationChanged(const jni::Ref &ref) {
	sprt::unique_lock lock(s_infoMutex);

	if (config) {
		AConfiguration_delete(config);
		config = nullptr;
	}

	config = AConfiguration_new();
	AConfiguration_fromAssetManager(config, nAssetManager);

	sdkVersion = AConfiguration_getSdkVersion(config);

	currentInfo = makeInfo(ref);

	if (configurationHandler) {
		configurationHandler(currentInfo.get());
	}
}

void App::handleLowMemory(const jni::Ref &ref) {
	if (lowMemoryHandler) {
		lowMemoryHandler();
	}
}

static memory::dynstring getApplicationName(App *proxy, const jni::Ref &ctx) {
	auto jAppInfo = proxy->Application.getApplicationInfo(ctx);
	if (!jAppInfo) {
		return memory::dynstring();
	}

	auto labelRes = proxy->ApplicationInfo.labelRes(jAppInfo);
	if (labelRes == 0) {
		auto jNonLocalizedLabel = proxy->ApplicationInfo.nonLocalizedLabel(jAppInfo);
		return StreamTraits<char>::toString(
				proxy->CharSequence.toString(jNonLocalizedLabel).getString());
	} else {
		auto jAppName = proxy->Application.getString(ctx, jint(labelRes));
		return jAppName.getString().str<memory::dynstring>();
	}
}

static memory::dynstring getApplicationVersion(App *proxy, const jni::Ref &ctx,
		const jni::RefString &jPackageName) {
	auto jpm = proxy->Application.getPackageManager(ctx);
	if (!jpm) {
		return memory::dynstring();
	}

	auto jinfo = proxy->PackageManager.getPackageInfo(jpm, jPackageName, 0);
	if (!jinfo) {
		return memory::dynstring();
	}

	auto jversion = proxy->PackageInfo.versionName(jinfo);
	if (!jversion) {
		return memory::dynstring();
	}

	return jversion.getString().str<memory::dynstring>();
}

static StringView getSystemAgent(App *proxy, const jni::Env &env) {
	return proxy->System.getProperty(proxy->System.getClass().ref(env), env.newString("http.agent"))
			.getString();
}

static StringView getUserAgent(App *proxy, const jni::Ref &ctx) {
	if (proxy->WebSettings && proxy->WebSettings.getDefaultUserAgent) {
		return proxy->WebSettings
				.getDefaultUserAgent(proxy->WebSettings.getClass().ref(ctx.getEnv()), ctx)
				.getString();
	}
	return StringView();
}

void App::setActivityLoader(memory::dynfunction<bool(ANativeActivity *, BytesView)> &&cb) {
	activityLoader = move(cb);
}

void App::setConfigurationHandler(memory::dynfunction<void(jni::ApplicationInfo *)> &&cb) {
	configurationHandler = move(cb);
}

void App::setLowMemoryHandler(memory::dynfunction<void()> &&cb) { lowMemoryHandler = move(cb); }

Rc<jni::ApplicationInfo> App::makeInfo(const jni::Ref &ref) {
	auto info = Rc<jni::ApplicationInfo>::alloc();

	auto env = Env::getEnv();
	auto ctx = jni::Ref(jApplication, env);

	auto jPackageName = Application.getPackageName(ctx);
	if (jPackageName) {
		info->bundleName = jPackageName.getString().str<memory::dynstring>();
		info->applicationName = getApplicationName(this, ctx);
		info->applicationVersion = getApplicationVersion(this, ctx, jPackageName);
		info->systemAgent = getSystemAgent(this, env).str<memory::dynstring>();
		info->userAgent = getUserAgent(this, ctx).str<memory::dynstring>();
	}

	// Use DP size as fallback
	int32_t widthPixels = AConfiguration_getScreenWidthDp(config);
	int32_t heightPixels = AConfiguration_getScreenHeightDp(config);
	float displayDensity = NaN<float>;

	if (auto resObj = Application.getResources(ctx)) {
		if (auto dmObj = Resources.getDisplayMetrics(resObj)) {
			displayDensity = DisplayMetrics.density(dmObj);
			heightPixels = DisplayMetrics.heightPixels(dmObj);
			widthPixels = DisplayMetrics.widthPixels(dmObj);
		}
	}

	array<char, 6> language;
	__sprt_memcpy(language.data(), "en-us", 6);
	AConfiguration_getLanguage(config, language.data());
	AConfiguration_getCountry(config, language.data() + 3);

	for (auto &it : language) {
		if (it) {
			it = ::__sprt_tolower(it);
		}
	}

	info->locale = StringView(language.data(), 5).str<memory::dynstring>();

	if (isnan(displayDensity)) {
		auto densityValue = AConfiguration_getDensity(config);
		switch (densityValue) {
		case ACONFIGURATION_DENSITY_LOW: displayDensity = 0.75f; break;
		case ACONFIGURATION_DENSITY_MEDIUM: displayDensity = 1.0f; break;
		case ACONFIGURATION_DENSITY_TV: displayDensity = 1.5f; break;
		case ACONFIGURATION_DENSITY_HIGH: displayDensity = 1.5f; break;
		case 280: displayDensity = 2.0f; break;
		case ACONFIGURATION_DENSITY_XHIGH: displayDensity = 2.0f; break;
		case 360: displayDensity = 3.0f; break;
		case 400: displayDensity = 3.0f; break;
		case 420: displayDensity = 3.0f; break;
		case ACONFIGURATION_DENSITY_XXHIGH: displayDensity = 3.0f; break;
		case 560: displayDensity = 4.0f; break;
		case ACONFIGURATION_DENSITY_XXXHIGH: displayDensity = 4.0f; break;
		default: displayDensity = 1.0f; break;
		}

		widthPixels = widthPixels * displayDensity;
		heightPixels = heightPixels * displayDensity;
	}

	info->density = displayDensity;

	int32_t orientation = AConfiguration_getOrientation(config);

	switch (orientation) {
	case ACONFIGURATION_ORIENTATION_ANY:
	case ACONFIGURATION_ORIENTATION_SQUARE:
		info->pixelWidth = widthPixels;
		info->pixelHeight = heightPixels;
		break;
	case ACONFIGURATION_ORIENTATION_PORT:
		info->pixelWidth = sprt::min(widthPixels, heightPixels);
		info->pixelHeight = sprt::max(widthPixels, heightPixels);
		break;
	case ACONFIGURATION_ORIENTATION_LAND:
		info->pixelWidth = sprt::max(widthPixels, heightPixels);
		info->pixelHeight = sprt::min(widthPixels, heightPixels);
		break;
	}

	info->dpWidth = float(info->pixelWidth) / displayDensity;
	info->dpWidth = float(info->pixelHeight) / displayDensity;
	info->orientation = jni::ApplicationInfo::Orientation(orientation);

	info->isEmulator = Application.isEmulator(ctx);

	return info;
}

bool App::loadActivity(ANativeActivity *a, BytesView data) {
	if (!activityLoader) {
		return false;
	}
	return activityLoader(a, data);
}

Rc<jni::ApplicationInfo> App::getCurrentInfo() {
	sprt::unique_lock lock(s_infoMutex);
	return currentInfo;
}

static JNIEnv *getVmEnv(JavaVM *vm) {
	void *ret = nullptr;
	if (vm) {
		vm->GetEnv(&ret, JNI_VERSION_1_6);
	} else {
		log::vprint(log::LogType::Error, __SPRT_LOCATION, "JNI", "JavaVM not found");
	}
	return reinterpret_cast<JNIEnv *>(ret);
}

Env Env::getEnv() {
	if (!tl_thread.env) {
		if (s_app && s_app->vm) {
			auto env = getVmEnv(s_app->vm);
			if (env) {
				tl_thread.init(s_app->vm, env, false);
			} else if (s_app && s_app->vm) {
				s_app->vm->AttachCurrentThread(&env, nullptr);
				if (env) {
					tl_thread.init(s_app->vm, env, true);
				}
			}
		}
	}
	return tl_thread.env;
}

App *Env::getApp() { return s_app; }

ClassLoader *Env::getClassLoader() {
	return &s_app->classLoader; // &s_classLoader;
}

static void Application_handleConfigurationChanged(JNIEnv *env, jobject thiz, jlong native,
		jobject config) {
	if (native) {
		auto app = reinterpret_cast<App *>(native);
		app->handleConfigurationChanged(jni::Ref(config, env));
	}
}

static void Application_handleLowMemory(JNIEnv *env, jobject thiz, jlong native) {
	if (native) {
		auto app = reinterpret_cast<App *>(native);
		app->handleLowMemory(jni::Ref(thiz, env));
	}
}

static JNINativeMethod s_AppNativeMethods[] = {
	{"handleConfigurationChanged", "(JLandroid/content/res/Configuration;)V",
		reinterpret_cast<void *>(&Application_handleConfigurationChanged)},
	{"handleLowMemory", "(J)V", reinterpret_cast<void *>(&Application_handleLowMemory)},
};

void Env::loadJava(JavaVM *vm) {
	auto env = getVmEnv(vm);
	tl_thread.init(vm, env, false);

	platform::s_self = Dso(StringView(), DsoFlags::Self);
	if (platform::s_self) {
		platform::_timespec_get =
				platform::s_self.sym<decltype(platform::_timespec_get)>("timespec_get");
		platform::_timespec_getres =
				platform::s_self.sym<decltype(platform::_timespec_getres)>("timespec_getres");
		platform::_getlogin_r = platform::s_self.sym<decltype(platform::_getlogin_r)>("getlogin_r");
		platform::_copy_file_range =
				platform::s_self.sym<decltype(platform::_copy_file_range)>("copy_file_range");
		platform::_futimes = platform::s_self.sym<decltype(platform::_futimes)>("futimes");
		platform::_lutimes = platform::s_self.sym<decltype(platform::_lutimes)>("lutimes");
		platform::_futimesat = platform::s_self.sym<decltype(platform::_futimesat)>("futimesat");
		platform::_sync_file_range =
				platform::s_self.sym<decltype(platform::_sync_file_range)>("sync_file_range");
		platform::_mlock2 = platform::s_self.sym<decltype(platform::_mlock2)>("mlock2");
		platform::_memfd_create =
				platform::s_self.sym<decltype(platform::_memfd_create)>("memfd_create");
		platform::_catopen = platform::s_self.sym<decltype(platform::_catopen)>("catopen");
		platform::_catgets = platform::s_self.sym<decltype(platform::_catgets)>("catgets");
		platform::_catclose = platform::s_self.sym<decltype(platform::_catclose)>("catclose");
		platform::_pthread_setschedprio =
				platform::s_self.sym<decltype(platform::_pthread_setschedprio)>(
						"pthread_setschedprio");
		platform::_ctermid = platform::s_self.sym<decltype(platform::_ctermid)>("ctermid");
		platform::_getsubopt = platform::s_self.sym<decltype(platform::_getsubopt)>("getsubopt");
		platform::_getentropy = platform::s_self.sym<decltype(platform::_getentropy)>("getentropy");
		platform::_getrandom = platform::s_self.sym<decltype(platform::_getrandom)>("getrandom");
		platform::_wcsftime_l = platform::s_self.sym<decltype(platform::_wcsftime_l)>("wcsftime_l");
		platform::_aligned_alloc =
				platform::s_self.sym<decltype(platform::_aligned_alloc)>("aligned_alloc");

		platform::_creat64 = platform::s_self.sym<decltype(platform::_creat64)>("creat64");
		platform::_open64 = platform::s_self.sym<decltype(platform::_open64)>("open64");
		platform::_openat64 = platform::s_self.sym<decltype(platform::_openat64)>("openat64");
		platform::_fallocate64 =
				platform::s_self.sym<decltype(platform::_fallocate64)>("fallocate64");
		platform::_posix_fadvise64 =
				platform::s_self.sym<decltype(platform::_posix_fadvise64)>("posix_fadvise64");
		platform::_posix_fallocate64 =
				platform::s_self.sym<decltype(platform::_posix_fallocate64)>("posix_fallocate64");
	}

	auto applicationClass = LocalClass(env->FindClass("org/stappler/runtime/Application"), env);

	if (applicationClass) {
		s_app = Rc<App>::alloc(RefClass(applicationClass));
		s_app->vm = vm;
	}

	if (!s_app) {
		log:: vprint(log::LogType::Fatal, __SPRT_LOCATION, "JNI",
						"Fail to load AppProxy; org/stappler/runtime/Application class was not "
						"defined " "properly?");
	}

	applicationClass.registerNatives(s_AppNativeMethods);
}

void Env::finalizeJava() { s_app = nullptr; }

void Env::checkErrors() const { detail::checkErrors(_env); }

bool ClassLoader::init(App *a, JNIEnv *env) {
	app = a;
	auto thiz = Ref(app->jApplication, env);

	auto currentClassLoader = app->Class.getClassLoader(thiz.getClass());

	auto codeCacheDir = app->Application.getCodeCacheDir(thiz);
	auto codeCachePath = app->File.getAbsolutePath(codeCacheDir);

	auto packageName = app->Application.getPackageName(thiz);
	auto packageManager = app->Application.getPackageManager(thiz);

	LocalString publicSourceDir;
	LocalString nativeLibraryDir;

	if (packageName && packageManager) {
		auto applicationInfo =
				app->PackageManager.getApplicationInfo(packageManager, packageName, 0);
		if (applicationInfo) {
			publicSourceDir = app->ApplicationInfo.publicSourceDir(applicationInfo);
			nativeLibraryDir = app->ApplicationInfo.nativeLibraryDir(applicationInfo);
		}
	}

	if (!codeCachePath || !publicSourceDir) {
		return false;
	}

	if (currentClassLoader) {
		appClassLoader = currentClassLoader;
		appClassLoaderClass = currentClassLoader.getClass();

		auto className = currentClassLoader.getClassName();

		log::vprint(log::LogType::Info, __SPRT_LOCATION, "JNI",
				"App: ClassLoader: ", className.getString());

		auto dexClassLoader =
				app->DexClassLoader.constructor(app->DexClassLoader.getClass().ref(env),
						publicSourceDir, codeCachePath, nativeLibraryDir, currentClassLoader);
		if (dexClassLoader) {
			apkClassLoader = dexClassLoader;
			apkClassLoaderClass = dexClassLoader.getClass();
		}
	}

	this->apkPath = publicSourceDir.getString().str<memory::dynstring>();
	this->nativeLibraryDir = nativeLibraryDir.getString().str<memory::dynstring>();

	return true;
}

void ClassLoader::finalize() {
	appClassLoader = nullptr;
	appClassLoaderClass = nullptr;
	apkClassLoader = nullptr;
	apkClassLoaderClass = nullptr;
}

void ClassLoader::foreachMethod(const jni::RefClass &cl,
		const callback<void(StringView, const jni::Ref &)> &cb) {
	auto methods = app->Class.getMethods(cl);
	for (auto it : methods) { cb(app->Method.getName(it).getString(), it); }
}

void ClassLoader::foreachField(const jni::RefClass &cl,
		const callback<void(StringView, StringView, const jni::Ref &)> &cb) {

	auto fields = app->Class.getFields(cl);

	for (auto it : fields) {
		cb(app->Field.getType(it).getName().getString(), app->Field.getName(it).getString(), it);
	}
}

int ClassLoader::getIntField(const jni::Ref &origin, const jni::Ref &field) {
	return app->Field.getInt(field, origin);
}

jni::LocalClass ClassLoader::findClass(const jni::Env &env, StringView data) {
	return findClass(env.newString(data));
}

jni::LocalClass ClassLoader::findClass(const jni::RefString &str) {
	return app->DexClassLoader.loadClass(apkClassLoader.ref(str.getEnv()), str, jboolean(1));
}

ClassProxy::ClassProxy(const char *name) {
	auto env = Env::getEnv();
	if (env) {
		_class = env.findClass(name);
	}
}

ApplicationInfo *ApplicationInfo::alloc() { return new (sprt::nothrow) ApplicationInfo; }

bool ApplicationInfo::init(const jni::Ref &ref) {
	jConfig = ref;
	return true;
}

} // namespace sprt::jni

namespace sprt::jni::detail {

auto TypeInfo<jobject>::wrap(Type t, JNIEnv *env) -> Result { return Result(t, env); }

auto TypeInfo<jstring>::wrap(Type t, JNIEnv *env) -> Result { return Result(t, env); }

auto TypeInfo<jclass>::wrap(Type t, JNIEnv *env) -> Result { return Result(t, env); }

auto TypeInfo<jobjectArray>::wrap(Type t, JNIEnv *env) -> Result { return Result(t, env); }

auto TypeInfo<jbooleanArray>::wrap(Type t, JNIEnv *env) -> Result { return Result(t, env); }

auto TypeInfo<jbyteArray>::wrap(Type t, JNIEnv *env) -> Result { return Result(t, env); }

auto TypeInfo<jcharArray>::wrap(Type t, JNIEnv *env) -> Result { return Result(t, env); }

auto TypeInfo<jshortArray>::wrap(Type t, JNIEnv *env) -> Result { return Result(t, env); }

auto TypeInfo<jintArray>::wrap(Type t, JNIEnv *env) -> Result { return Result(t, env); }

auto TypeInfo<jlongArray>::wrap(Type t, JNIEnv *env) -> Result { return Result(t, env); }

auto TypeInfo<jfloatArray>::wrap(Type t, JNIEnv *env) -> Result { return Result(t, env); }

auto TypeInfo<jdoubleArray>::wrap(Type t, JNIEnv *env) -> Result { return Result(t, env); }

void checkErrors(JNIEnv *env) {
	if (env->ExceptionCheck()) {
		// Read exception msg
		auto e = Local(env->ExceptionOccurred(), env);
		env->ExceptionClear(); // clears the exception; e seems to remain valid

		auto clazz = e.getClass();
		auto classClass = clazz.getClass();
		jmethodID getName = classClass.getMethodID("getName", "()Ljava/lang/String;");
		jmethodID getMessage = clazz.getMethodID("getMessage", "()Ljava/lang/String;");

		auto message = e.callMethod<jstring>(getMessage);
		auto exName = clazz.callMethod<jstring>(getName);

		log::vprint(log::LogType::Error, __SPRT_LOCATION, "JNI", "[", exName.getString(), "] ",
				message.getString());
	}
}

} // namespace sprt::jni::detail

#endif
