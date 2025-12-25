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

#include "SPRuntimeInit.h"

#if SPRT_ANDROID

#include "SPRuntimeUnicode.h"
#include "SPRuntimeStringBuffer.h"
#include "jni/SPRuntimeJni.h"
#include "private/SPRTDso.h"

#include <unicode/uchar.h>
#include <unicode/urename.h>
#include <unicode/ustring.h>

#include <stdlib.h>
#include <sys/random.h>
#include <android/configuration.h>

#include <mutex>

namespace sprt::unicode {

static std::mutex s_collatorMutex;

namespace icujava {

char32_t tolower(jni::App *app, char32_t c) {
	return app->UCharacter.toLowerChar(app->UCharacter.getClass().ref(), jint(c));
}

char32_t toupper(jni::App *app, char32_t c) {
	return app->UCharacter.toUpperChar(app->UCharacter.getClass().ref(), jint(c));
}

char32_t totitle(jni::App *app, char32_t c) {
	return app->UCharacter.toTitleChar(app->UCharacter.getClass().ref(), jint(c));
}

bool toupper(jni::App *app, const callback<void(StringView)> &cb, StringView data) {
	auto env = jni::Env::getEnv();
	auto ret =
			app->UCharacter.toUpperString(app->UCharacter.getClass().ref(env), env.newString(data))
					.getString();
	cb(ret);
	return true;
}

bool totitle(jni::App *app, const callback<void(StringView)> &cb, StringView data) {
	auto env = jni::Env::getEnv();
	auto ret =
			app->UCharacter.toTitleString(app->UCharacter.getClass().ref(env), env.newString(data))
					.getString();
	cb(ret);
	return true;
}

bool tolower(jni::App *app, const callback<void(StringView)> &cb, StringView data) {
	auto env = jni::Env::getEnv();
	auto ret =
			app->UCharacter.toLowerString(app->UCharacter.getClass().ref(env), env.newString(data))
					.getString();
	cb(ret);
	return true;
}

bool toupper(jni::App *app, const callback<void(WideStringView)> &cb, WideStringView data) {
	auto env = jni::Env::getEnv();
	auto ret =
			app->UCharacter.toUpperString(app->UCharacter.getClass().ref(env), env.newString(data))
					.getWideString();
	cb(ret);
	return true;
}

bool totitle(jni::App *app, const callback<void(WideStringView)> &cb, WideStringView data) {
	auto env = jni::Env::getEnv();
	auto ret =
			app->UCharacter.toTitleString(app->UCharacter.getClass().ref(env), env.newString(data))
					.getWideString();
	cb(ret);
	return true;
}

bool tolower(jni::App *app, const callback<void(WideStringView)> &cb, WideStringView data) {
	auto env = jni::Env::getEnv();
	auto ret =
			app->UCharacter.toLowerString(app->UCharacter.getClass().ref(env), env.newString(data))
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
		std::unique_lock lock(s_collatorMutex);
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
		std::unique_lock lock(s_collatorMutex);
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

static int32_t (*tolower_fn)(int32_t) = nullptr;
static int32_t (*toupper_fn)(int32_t) = nullptr;
static int32_t (*totitle_fn)(int32_t) = nullptr;

static int32_t (*strToLower_fn)(char16_t *dest, int32_t destCapacity, const char16_t *src,
		int32_t srcLength, const char *locale, int *pErrorCode) = nullptr;

static int32_t (*strToUpper_fn)(char16_t *dest, int32_t destCapacity, const char16_t *src,
		int32_t srcLength, const char *locale, int *pErrorCode) = nullptr;

static int32_t (*strToTitle_fn)(char16_t *dest, int32_t destCapacity, const char16_t *src,
		int32_t srcLength, void *iter, const char *locale, int *pErrorCode) = nullptr;

static cmp_fn u_strCompare = nullptr;
static case_cmp_fn u_strCaseCompare = nullptr;

char32_t tolower(char32_t c) {
	if (s_icuNative) {
		return char32_t(tolower_fn(int32_t(c)));
	}
	if (auto app = jni::Env::getApp()) {
		return icujava::tolower(app, c);
	}
	return c;
}

char32_t toupper(char32_t c) {
	if (s_icuNative) {
		return char32_t(toupper_fn(int32_t(c)));
	}
	if (auto app = jni::Env::getApp()) {
		return icujava::toupper(app, c);
	}
	return c;
}

char32_t totitle(char32_t c) {
	if (s_icuNative) {
		return char32_t(totitle_fn(int32_t(c)));
	}
	if (auto app = jni::Env::getApp()) {
		return icujava::totitle(app, c);
	}
	return c;
}

bool toupper(const callback<void(StringView)> &cb, StringView data) {
	if (s_icuNative) {
		bool ret = false;
		toUtf16([&](WideStringView uData) {
			ret = toupper([&](WideStringView result) { toUtf8(cb, result); }, uData);
		}, data);
		if (ret) {
			return ret;
		}
	}
	if (auto app = jni::Env::getApp()) {
		return icujava::toupper(app, cb, data);
	}
	return false;
}

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

bool tolower(const callback<void(StringView)> &cb, StringView data) {
	if (s_icuNative) {
		bool ret = false;
		toUtf16([&](WideStringView uData) {
			ret = tolower([&](WideStringView result) { toUtf8(cb, result); }, uData);
		}, data);
		if (ret) {
			return ret;
		}
	}
	if (auto app = jni::Env::getApp()) {
		return icujava::tolower(app, cb, data);
	}
	return false;
}

bool toupper(const callback<void(WideStringView)> &cb, WideStringView data) {
	if (s_icuNative) {
		StringBuffer<char16_t> ret;

		int status = 0;
		size_t capacity = data.size();
		auto ptr = ret.prepare(capacity);

		auto len = strToUpper_fn(ptr, capacity, data.data(), data.size(), nullptr, &status);
		if (len <= int32_t(ret.size())) {
			ret.commit(len);
		} else {
			capacity = len;
			ptr = ret.prepare(capacity);
			strToUpper_fn(ptr, capacity, data.data(), data.size(), nullptr, &status);
		}
		if (status == 0) {
			cb(ptr);
			return true;
		}
	}
	if (auto app = jni::Env::getApp()) {
		return icujava::toupper(app, cb, data);
	}
	return false;
}

bool totitle(const callback<void(WideStringView)> &cb, WideStringView data) {
	if (s_icuNative) {
		StringBuffer<char16_t> ret;

		int status = 0;
		size_t capacity = data.size();
		auto ptr = ret.prepare(capacity);

		auto len =
				strToTitle_fn(ptr, capacity, data.data(), data.size(), nullptr, nullptr, &status);
		if (len <= int32_t(ret.size())) {
			ret.commit(len);
		} else {
			capacity = len;
			ptr = ret.prepare(capacity);
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

bool tolower(const callback<void(WideStringView)> &cb, WideStringView data) {
	if (s_icuNative) {
		StringBuffer<char16_t> ret;

		int status = 0;
		size_t capacity = data.size();
		auto ptr = ret.prepare(capacity);

		auto len = strToLower_fn(ptr, capacity, data.data(), data.size(), nullptr, &status);
		if (len <= int32_t(ret.size())) {
			ret.commit(len);
		} else {
			capacity = len;
			ptr = ret.prepare(capacity);
			strToLower_fn(ptr, capacity, data.data(), data.size(), nullptr, &status);
		}
		if (status == 0) {
			cb(ptr);
			return true;
		}
	}
	if (auto app = jni::Env::getApp()) {
		return icujava::tolower(app, cb, data);
	}
	return false;
}

bool compare(StringView l, StringView r, int *result) {
	if (u_strCompare) {
		bool ret = false;
		unicode::toUtf16([&](WideStringView lStr) {
			unicode::toUtf16([&](WideStringView rStr) {
				int status = -1;
				*result = u_strCompare(lStr.data(), lStr.size(), rStr.data(), rStr.size(), 1);
				ret = status == U_ZERO_ERROR;
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
				int status;
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

bool idnToAscii(const callback<void(StringView)> &cb, StringView source) {
	if (source.empty()) {
		return false;
	}

	auto app = jni::Env::getApp();
	auto env = jni::Env::getEnv();

	if (app && env) {
		auto str = app->IDN.toASCII(app->IDN.getClass().ref(env), env.newString(source), 0);
		if (str) {
			cb(str.getString());
			return true;
		}
	}
	return false;
}

bool idnToUnicode(const callback<void(StringView)> &cb, StringView source) {
	if (source.empty()) {
		return false;
	}

	auto app = jni::Env::getApp();
	auto env = jni::Env::getEnv();

	if (app && env) {
		auto str = app->IDN.toUnicode(app->IDN.getClass().ref(env), env.newString(source), 0);
		if (str) {
			cb(str.getString());
			return true;
		}
	}
	return false;
}

} // namespace sprt::unicode

// Mimic IDN2 API for CURL
namespace sprt::idn {

static constexpr auto IDN2_VERSION = "2.3.2-lisbstappler";

static constexpr auto IDN2_OK = 0;

static constexpr auto IDN2_JNI = -1'000;
static constexpr auto IDN2_JNI_STR = "Fail to acquire JNI context";

static constexpr auto IDN2_CONV_TOASCII = -1'001;
static constexpr auto IDN2_CONV_TOASCII_STR = "Fail to call java.net.IDN.toASCII";

static constexpr auto IDN2_CONV_TOUNICODE = -1'002;
static constexpr auto IDN2_CONV_TOUNICODE_STR = "Fail to call java.net.IDN.toUnicode";

static constexpr auto IDN2_ALLOW_UNASSIGNED = 16;
static constexpr auto IDN2_USE_STD3_ASCII_RULES = 32;

extern "C" SPRT_GLOBAL int idn2_lookup_u8(const uint8_t *src, uint8_t **lookupname, int flags) {
	if (!src) {
		if (lookupname) {
			*lookupname = nullptr;
		}
		return IDN2_OK;
	}

	auto app = jni::Env::getApp();
	auto env = jni::Env::getEnv();
	if (!app || !env) {
		return IDN2_JNI;
	}

	jint options = 0;

	if (flags & IDN2_USE_STD3_ASCII_RULES) {
		options |= app->IDN.USE_STD3_ASCII_RULES();
	}

	if (flags & IDN2_ALLOW_UNASSIGNED) {
		options |= app->IDN.ALLOW_UNASSIGNED();
	}

	auto str = app->IDN.toASCII(app->IDN.getClass().ref(env),
			env.newString(StringView((const char *)src)), jint(options));
	if (str) {
		auto out = str.getString();

		auto buf = new char[out.size() + 1];
		::__sprt_memcpy(buf, out.data(), out.size());
		buf[out.size()] = 0;

		*lookupname = (uint8_t *)buf;
		return IDN2_JNI;
	} else {
		return IDN2_CONV_TOASCII;
	}
}

extern "C" SPRT_GLOBAL int idn2_lookup_ul(const char *src, char **lookupname, int flags) {
	if (!src) {
		if (lookupname) {
			*lookupname = nullptr;
		}
		return IDN2_OK;
	}

	auto app = jni::Env::getApp();
	auto env = jni::Env::getEnv();
	if (!app || !env) {
		return IDN2_JNI;
	}

	jint options = 0;

	if (flags & IDN2_USE_STD3_ASCII_RULES) {
		options |= app->IDN.USE_STD3_ASCII_RULES();
	}

	if (flags & IDN2_ALLOW_UNASSIGNED) {
		options |= app->IDN.ALLOW_UNASSIGNED();
	}

	auto str = app->IDN.toASCII(app->IDN.getClass().ref(env), env.newString(StringView(src)),
			jint(options));
	if (str) {
		auto out = str.getString();

		auto buf = new char[out.size() + 1];
		::__sprt_memcpy(buf, out.data(), out.size());
		buf[out.size()] = 0;

		*lookupname = buf;
		return IDN2_JNI;
	} else {
		return IDN2_CONV_TOASCII;
	}
}

extern "C" SPRT_GLOBAL int idn2_to_unicode_8z8z(const char *src, char **lookupname, int flags) {
	if (!src) {
		if (lookupname) {
			*lookupname = nullptr;
		}
		return IDN2_OK;
	}

	auto app = jni::Env::getApp();
	auto env = jni::Env::getEnv();
	if (!app || !env) {
		return IDN2_JNI;
	}

	jint options = 0;

	if (flags & IDN2_USE_STD3_ASCII_RULES) {
		options |= app->IDN.USE_STD3_ASCII_RULES();
	}

	if (flags & IDN2_ALLOW_UNASSIGNED) {
		options |= app->IDN.ALLOW_UNASSIGNED();
	}

	auto str = app->IDN.toUnicode(app->IDN.getClass().ref(env), env.newString(StringView(src)),
			jint(options));
	if (str) {
		auto out = str.getString();

		auto buf = new char[out.size() + 1];
		::__sprt_memcpy(buf, out.data(), out.size());
		buf[out.size()] = 0;

		*lookupname = buf;
		return IDN2_JNI;
	} else {
		return IDN2_CONV_TOUNICODE;
	}
}

extern "C" SPRT_GLOBAL const char *idn2_strerror(int rc) {
	switch (rc) {
	case IDN2_OK: return "Success"; break;
	case IDN2_JNI: return IDN2_JNI_STR; break;
	case IDN2_CONV_TOASCII: return IDN2_CONV_TOASCII_STR; break;
	case IDN2_CONV_TOUNICODE: return IDN2_CONV_TOUNICODE_STR; break;
	}
	return nullptr;
}

extern "C" SPRT_GLOBAL const char *idn2_strerror_name(int rc) {
	switch (rc) {
	case IDN2_OK: return "IDN2_OK"; break;
	case IDN2_JNI: return "IDN2_JNI"; break;
	case IDN2_CONV_TOASCII: return "IDN2_CONV_TOASCII"; break;
	case IDN2_CONV_TOUNICODE: return "IDN2_CONV_TOUNICODE"; break;
	}
	return nullptr;
}

extern "C" SPRT_GLOBAL void idn2_free(void *ptr) {
	if (ptr) {
		delete[] (char *)ptr;
	}
}

extern "C" SPRT_GLOBAL const char *idn2_check_version(const char *req_version) {
	if (!req_version || strcmp(req_version, IDN2_VERSION) <= 0) {
		return IDN2_VERSION;
	}

	return NULL;
}

} // namespace sprt::idn

namespace sprt::platform {

static char s_locale[6] = "en-us";
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

size_t makeRandomBytes(uint8_t *buf, size_t count) {
	::arc4random_buf(buf, count);
	return count;
}

StringView getOsLocale() { return StringView(s_locale); }

bool initialize(int &resultCode) {
	s_self = Dso(StringView(), DsoFlags::Self);
	if (s_self) {
		_timespec_get = s_self.sym<decltype(_timespec_get)>("timespec_get");
		_timespec_getres = s_self.sym<decltype(_timespec_getres)>("timespec_getres");
		_getlogin_r = s_self.sym<decltype(_getlogin_r)>("getlogin_r");
		_copy_file_range = s_self.sym<decltype(_copy_file_range)>("copy_file_range");
		_futimes = s_self.sym<decltype(_futimes)>("futimes");
		_lutimes = s_self.sym<decltype(_lutimes)>("lutimes");
		_futimesat = s_self.sym<decltype(_futimesat)>("futimesat");
		_sync_file_range = s_self.sym<decltype(_sync_file_range)>("sync_file_range");
		_mlock2 = s_self.sym<decltype(_mlock2)>("mlock2");
	}

	// init locale
	auto cfg = jni::Env::getApp()->config;
	if (cfg) {
		AConfiguration_getLanguage(cfg, platform::s_locale);
		AConfiguration_getCountry(cfg, platform::s_locale + 3);
	}

	unicode::s_icuNative = Dso("libicu.so");
	if (unicode::s_icuNative) {
		unicode::tolower_fn = unicode::s_icuNative.sym<decltype(unicode::tolower_fn)>("u_tolower");
		unicode::toupper_fn = unicode::s_icuNative.sym<decltype(unicode::toupper_fn)>("u_toupper");
		unicode::totitle_fn = unicode::s_icuNative.sym<decltype(unicode::totitle_fn)>("u_totitle");
		unicode::strToLower_fn =
				unicode::s_icuNative.sym<decltype(unicode::strToLower_fn)>("u_strToLower");
		unicode::strToUpper_fn =
				unicode::s_icuNative.sym<decltype(unicode::strToUpper_fn)>("u_strToUpper");
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

} // namespace sprt::platform

#endif
