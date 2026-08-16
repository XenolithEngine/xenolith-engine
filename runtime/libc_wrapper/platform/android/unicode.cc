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

#include <sprt/runtime/utils/dso.h>

#include <unicode/uchar.h>
#include <unicode/urename.h>
#include <unicode/ustring.h>

#include <android/configuration.h>

#include "../src/private/SPRTPrivate.h"

namespace sprt::unicode {

static qmutex s_collatorMutex;

namespace icujava {

// Lowercasing and uppercasing had a JNI fallback here (UCharacter.toLowerCase
// and friends) for devices without libicu.so, both per code point and per
// string. They are gone: those mappings come from the compiled-in Unicode tables
// now, on every device and with no JNI call. Titlecasing stays, because it needs
// a BreakIterator.

bool totitle(jni::App *app, const callback<void(StringView)> &cb, StringView data) {
	auto env = jni::Env::getEnv();
	auto ret = app->UCharacter
					   .toTitleString(app->UCharacter.getClass().ref(env), env.newString(data),
							   nullptr)
					   .getString();
	cb(ret);
	return true;
}

bool totitle(jni::App *app, const callback<void(WideStringView)> &cb, WideStringView data) {
	auto env = jni::Env::getEnv();
	auto ret = app->UCharacter
					   .toTitleString(app->UCharacter.getClass().ref(env), env.newString(data),
							   nullptr)
					   .getWideString();
	cb(ret);
	return true;
}

bool compare(jni::App *app, StringView l, StringView r, bool caseInsensetive, int *result) {
	auto env = jni::Env::getEnv();

	auto strL = env.newString(l);
	auto strR = env.newString(r);

	auto coll = app->Collator.getInstance(app->Collator.getClass().ref(env));
	if (coll) {
		unique_lock lock(s_collatorMutex);
		app->Collator.setStrength(coll,
				jint(caseInsensetive ? app->Collator.SECONDARY() : app->Collator.TERTIARY()));
		*result = app->Collator._compare(coll, strL, strR);
		return true;
	}
	return false;
}

bool compare(jni::App *app, WideStringView l, WideStringView r, bool caseInsensetive, int *result) {
	auto env = jni::Env::getEnv();

	auto strL = env.newString(l);
	auto strR = env.newString(r);

	auto coll = app->Collator.getInstance(app->Collator.getClass().ref(env));
	if (coll) {
		unique_lock lock(s_collatorMutex);
		app->Collator.setStrength(coll,
				jint(caseInsensetive ? app->Collator.SECONDARY() : app->Collator.TERTIARY()));
		*result = app->Collator._compare(coll, strL, strR);
		return true;
	}
	return false;
}

} // namespace icujava

using cmp_fn = int32_t (*)(const char16_t *s1, int32_t length1, const char16_t *s2, int32_t length2,
		int8_t codePointOrder);
using case_cmp_fn = int32_t (*)(const char16_t *s1, int32_t length1, const char16_t *s2,
		int32_t length2, uint32_t options, int *pErrorCode);

static Dso s_icuNative;


static int32_t (*strToTitle_fn)(char16_t *dest, int32_t destCapacity, const char16_t *src,
		int32_t srcLength, void *iter, const char *locale, int *pErrorCode) = nullptr;

static cmp_fn u_strCompare = nullptr;
static case_cmp_fn u_strCaseCompare = nullptr;

// tolower/toupper are no longer here, for code points or for strings: they come
// from the compiled-in Unicode tables (runtime/src/unicode), so they no longer
// depend on libicu.so being present or on a JNI round trip.

bool totitle(const callback<void(StringView)> &cb, StringView data) {
	if (s_icuNative) {
		bool ret = false;
		toUtf16([&](WideStringView uData) {
			ret = totitle([&](WideStringView result) { toUtf8(cb, result); }, uData);
		}, data);
		if (ret) {
			return ret;
		}
	}
	if (auto app = jni::Env::getApp()) {
		return icujava::totitle(app, cb, data);
	}
	return false;
}

bool totitle(const callback<void(WideStringView)> &cb, WideStringView data) {
	if (s_icuNative) {
		__malloc_u16string str;
		str.resize(data.size());

		int status = 0;
		size_t capacity = str.size();
		auto ptr = str.data();

		auto len =
				strToTitle_fn(ptr, capacity, data.data(), data.size(), nullptr, nullptr, &status);
		if (len <= int32_t(str.size())) {
			str.resize(len);
		} else {
			capacity = len;
			str.resize(capacity);
			ptr = str.data();
			strToTitle_fn(ptr, capacity, data.data(), data.size(), nullptr, nullptr, &status);
		}
		if (status == 0) {
			cb(ptr);
			return true;
		}
	}
	if (auto app = jni::Env::getApp()) {
		return icujava::totitle(app, cb, data);
	}
	return false;
}

bool compare(StringView l, StringView r, int *result) {
	if (u_strCompare) {
		bool ret = false;
		unicode::toUtf16([&](WideStringView lStr) {
			unicode::toUtf16([&](WideStringView rStr) {
				*result = u_strCompare(lStr.data(), lStr.size(), rStr.data(), rStr.size(), 1);
				ret = true;
			}, r);
		}, l);
		if (ret) {
			return true;
		}
	}
	if (auto app = jni::Env::getApp()) {
		return icujava::compare(app, l, r, false, result);
	}
	return false;
}

bool compare(WideStringView l, WideStringView r, int *result) {
	if (u_strCompare) {
		*result = u_strCompare(l.data(), l.size(), r.data(), r.size(), 1);
		return true;
	}
	if (auto app = jni::Env::getApp()) {
		return icujava::compare(app, l, r, false, result);
	}
	return false;
}

bool caseCompare(StringView l, StringView r, int *result) {
	if (u_strCaseCompare) {
		bool ret = false;
		unicode::toUtf16([&](WideStringView lStr) {
			unicode::toUtf16([&](WideStringView rStr) {
				int status = U_ZERO_ERROR;
				*result = u_strCaseCompare(lStr.data(), lStr.size(), rStr.data(), rStr.size(),
						U_COMPARE_CODE_POINT_ORDER, &status);
				ret = status == U_ZERO_ERROR;
			}, r);
		}, l);
		if (ret) {
			return true;
		}
	}
	if (auto app = jni::Env::getApp()) {
		return icujava::compare(app, l, r, true, result);
	}
	return false;
}

bool caseCompare(WideStringView l, WideStringView r, int *result) {
	if (u_strCaseCompare) {
		int status = 0;
		*result = u_strCaseCompare(l.data(), l.size(), r.data(), r.size(),
				U_COMPARE_CODE_POINT_ORDER, &status);
		return true;
	}
	if (auto app = jni::Env::getApp()) {
		return icujava::compare(app, l, r, true, result);
	}
	return false;
}

} // namespace sprt::unicode

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

	unicode::s_icuNative = Dso("libicu.so");
	if (unicode::s_icuNative) {
		unicode::strToTitle_fn =
				unicode::s_icuNative.sym<decltype(unicode::strToTitle_fn)>("u_strToTitle");

		unicode::u_strCompare =
				unicode::s_icuNative.sym<decltype(unicode::u_strCompare)>("u_strCompare");
		unicode::u_strCaseCompare =
				unicode::s_icuNative.sym<decltype(unicode::u_strCaseCompare)>("u_strCaseCompare");
	}

	return true;
}

void terminate() { unicode::s_icuNative.close(); }

memory::pool_t *getConfigPool() { return s_globalConfig._pool; }

} // namespace sprt::platform

namespace sprt::filesystem {

const LocationInfo &getCurrentLocation() { return platform::s_globalConfig.current; }

} // namespace sprt::filesystem

#endif
