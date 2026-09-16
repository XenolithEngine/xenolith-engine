/**
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

// Platform integration for android: the locale, the device id, the paths, and
// the APK-aware startup every target has to answer for.
//
// This file was `unicode.cc` until android stopped being asked anything about
// Unicode. Case mapping went to the compiled-in tables (runtime/src/unicode)
// with the rest of the port, and comparison followed: `u_strCompare` out of
// libicu.so and `java.text.Collator` over JNI were collation, and the runtime no
// longer claims to collate. Nothing here dlopens libicu.so any more, and
// jni::App has no Collator proxy.

#include <sprt/runtime/platform.h>

#if SPRT_ANDROID

#include <sprt/runtime/stringview.h>
#include <sprt/cxx/mutex>
#include <sprt/runtime/platform.h>
#include <sprt/runtime/filesystem/lookup.h>
#include <sprt/jni/jni.h>

#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/__sprt_unistd.h>
#include <sprt/c/__sprt_dirent.h>
#include <sprt/c/__sprt_limits.h>

#include <android/configuration.h>

#include "../src/private/SPRTPrivate.h"

namespace sprt::platform {

char GlobalConfig::localeBuf[6] = "en-us";
static GlobalConfig s_globalConfig;

StringView getUniqueDeviceId() { return s_globalConfig.uniqueIdBuf; }

StringView getExecPath() { return s_globalConfig.execPathBuf; }

StringView getHomePath() {
	if (s_globalConfig.homePathBuf.empty()) {
		// optimistic multithreaded lazy-init
		// it can allocate more-then needed memory but protected from general lock

		auto path = StringView(__sprt_getenv("HOME"));

		unique_lock lock(s_globalConfig.infoMutex);
		s_globalConfig.homePathBuf = path.pdup(s_globalConfig._pool);
	}
	return s_globalConfig.homePathBuf;
}

StringView getOsLocale() { return StringView(s_globalConfig.locale); }

static bool checkApkFile(StringView path) {
	int fd = ::__sprt_open(path.data(), __SPRT_O_RDONLY);
	if (fd == -1) {
		return false;
	}

	if (auto f = __sprt_fdopen(fd, "r")) {
		__sprt_fclose(f);
		return true;
	}

	__sprt_close(fd);
	return false;
}

bool initialize(sprt::AppConfig &&appcfg, int &resultCode) {
	s_globalConfig.config.bundleName = appcfg.bundleName.pdup(s_globalConfig._pool);
	s_globalConfig.config.bundlePath = appcfg.bundlePath.pdup(s_globalConfig._pool);
	s_globalConfig.config.pathScheme = appcfg.pathScheme;

	s_globalConfig.current.lookupType = filesystem::LookupFlags::Public
			| filesystem::LookupFlags::Shared | filesystem::LookupFlags::Writable;
	s_globalConfig.current.locationFlags = filesystem::LocationFlags::Writable;
	s_globalConfig.current.interface = filesystem::getDefaultInterface();

	filesystem::getCurrentDir([&](StringView path) {
		s_globalConfig.current.path = path.pdup(s_globalConfig._pool);
	});

	// init locale
	auto app = jni::Env::getApp();
	auto env = jni::Env::getEnv();

	auto apkPath = app->classLoader.getApkPath();

	if (apkPath.empty() || !checkApkFile(apkPath)) {
		char fullpath[__SPRT_PATH_MAX] = "/proc/self/fd/";
		char refpath[__SPRT_PATH_MAX] = {0};
		struct __SPRT_DIRENT_NAME *dp = nullptr;
		auto dir = ::__sprt_opendir("/proc/self/fd");
		while ((dp = __sprt_readdir(dir)) != NULL) {
			if (dp->d_name[0] != '.') {
				__sprt_memcpy(fullpath + "/proc/self/fd/"_len, dp->d_name,
						__sprt_strlen(dp->d_name) + 1);
				auto nbytes = __sprt_readlink(fullpath, refpath, __SPRT_PATH_MAX);
				if (nbytes > 0) {
					StringView path(refpath, nbytes);
					if (path.ends_with(".apk") && path.starts_with("/data/")) {
						if (checkApkFile(refpath)) {
							s_globalConfig.execPathBuf =
									StringView(refpath, nbytes).pdup(s_globalConfig._pool);
							break;
						}
					}
				}
			}
		}
		__sprt_closedir(dir);
	} else {
		s_globalConfig.execPathBuf = apkPath.pdup(s_globalConfig._pool);
	}

	auto thiz = sprt::jni::Ref(app->jApplication, env);
	auto filesDir = app->Application.getFilesDir(thiz);
	if (filesDir) {
		s_globalConfig.homePathBuf = StringView(app->File.getAbsolutePath(filesDir).getString())
											 .pdup(s_globalConfig._pool);
	}

	if (s_globalConfig.current.path.empty()) {
		auto storageDir =
				app->Environment.getExternalStorageDirectory(env, app->Environment.getClass());
		if (storageDir) {
			auto path = app->File.getAbsolutePath(storageDir);
			if (path) {
				s_globalConfig.current = filesystem::LocationInfo{
					StringView(path.getString()).pdup(s_globalConfig._pool),
					filesystem::LookupFlags::Shared | filesystem::LookupFlags::Writable,
					filesystem::LocationFlags::Locateable,
					filesystem::getDefaultInterface(),
				};
			}
		}

		if (s_globalConfig.current.path.empty()) {
			s_globalConfig.current = filesystem::LocationInfo{
				s_globalConfig.homePathBuf,
				filesystem::LookupFlags::Private | filesystem::LookupFlags::Writable,
				filesystem::LocationFlags::Locateable,
				filesystem::getDefaultInterface(),
			};
		}
	}

	auto androidId = app->SettingsSecure.ANDROID_ID(env, app->SettingsSecure.getClass());

	s_globalConfig.uniqueIdBuf = StringView(androidId.getString()).pdup(s_globalConfig._pool);

	auto cfg = jni::Env::getApp()->config;
	if (cfg) {
		AConfiguration_getLanguage(cfg, s_globalConfig.localeBuf);
		AConfiguration_getCountry(cfg, &s_globalConfig.localeBuf[3]);
	}

	return true;
}

void terminate() { }

memory::pool_t *getConfigPool() { return s_globalConfig._pool; }

} // namespace sprt::platform

namespace sprt::filesystem {

const LocationInfo &getCurrentLocation() { return platform::s_globalConfig.current; }

} // namespace sprt::filesystem

#endif
