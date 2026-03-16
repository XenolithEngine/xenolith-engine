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

#define __SPRT_BUILD 1

#include <sprt/runtime/platform.h>

#if SPRT_MACOS

#include <sprt/runtime/platform.h>
#include <sprt/runtime/callback.h>
#include <sprt/runtime/stringview.h>
#include <sprt/runtime/unicode.h>
#include <sprt/runtime/uuid.h>

#include <unicode/uidna.h>

#include "private/SPRTPrivate.h"
#include "private/SPRTDso.h"

#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFLocale.h>

#include <unistd.h>

extern "C" int _NSGetExecutablePath(char *buf, uint32_t *bufsize);

namespace sprt::unicode {

static char32_t convertChar(char32_t c, const callback<void(CFMutableStringRef, CFLocaleRef)> &cb) {
	auto locale = CFLocaleCopyCurrent();
	UniChar buf[4] = {0};
	char32_t buf2[2];
	auto num = unicode::utf16EncodeBuf((char16_t *)buf, 4, c);
	auto str = CFStringCreateMutable(nullptr, 2);
	CFStringAppendCharacters(str, buf, num);
	cb(str, locale);
	CFStringLowercase(str, locale);

	auto bytes = CFStringGetCStringPtr(str, kCFStringEncodingUTF32);

	if (bytes == nullptr) {
		if (CFStringGetCString(str, (char *)buf2, 2 * sizeof(char32_t), kCFStringEncodingUTF32)) {
			c = buf2[0];
		}
	} else {
		c = ((char32_t *)bytes)[0];
	}

	CFRelease(str);
	CFRelease(locale);
	return c;
}

static CFMutableStringRef makeString(WideStringView str) {
	auto ret = CFStringCreateMutable(nullptr, str.size());
	CFStringAppendCharacters(ret, (UniChar *)str.data(), str.size());
	return ret;
}

static CFMutableStringRef makeString(StringViewUtf8 str) {
	auto ret = CFStringCreateMutable(nullptr, str.code_size());
	UniChar buf[4] = {0};
	str.foreach ([&](char32_t c) {
		auto num = unicode::utf16EncodeBuf((char16_t *)buf, 4, c);
		CFStringAppendCharacters(ret, buf, num);
	});
	return ret;
}

static bool toString(CFMutableStringRef str, const callback<void(StringView)> &cb) {
	auto len = CFStringGetLength(str);
	auto bytes = (const char *)CFStringGetCStringPtr(str, kCFStringEncodingUTF16);

	if (bytes == nullptr) {
		char32_t buf[len + 1];
		if (CFStringGetCString(str, (char *)buf, len * sizeof(char32_t), kCFStringEncodingUTF16)) {
			cb(StringView((char *)buf, len));
			return true;
		}
	} else {
		cb(StringView(bytes, len));
		return true;
	}
	return false;
}

static bool toWideString(CFMutableStringRef str, const callback<void(WideStringView)> &cb) {
	auto len = CFStringGetLength(str);
	auto bytes = (const char16_t *)CFStringGetCStringPtr(str, kCFStringEncodingUTF16);

	if (bytes == nullptr) {
		char32_t buf[len + 1];
		if (CFStringGetCString(str, (char *)buf, len * sizeof(char32_t), kCFStringEncodingUTF16)) {
			cb(WideStringView((char16_t *)buf, len));
			return true;
		}
	} else {
		cb(WideStringView(bytes, len));
		return true;
	}
	return false;
}

char32_t tolower(char32_t c) {
	return convertChar(c, [](CFMutableStringRef str, CFLocaleRef locale) {
		CFStringLowercase(str, locale); //
	});
}

char32_t toupper(char32_t c) {
	return convertChar(c, [](CFMutableStringRef str, CFLocaleRef locale) {
		CFStringUppercase(str, locale); //
	});
}

char32_t totitle(char32_t c) {
	return convertChar(c, [](CFMutableStringRef str, CFLocaleRef locale) {
		CFStringCapitalize(str, locale); //
	});
}

bool toupper(const callback<void(StringView)> &cb, StringView data) {
	auto locale = CFLocaleCopyCurrent();
	auto str = makeString(data);

	CFStringUppercase(str, locale);

	auto ret = toString(str, cb);

	CFRelease(str);
	CFRelease(locale);
	return ret;
}
bool totitle(const callback<void(StringView)> &cb, StringView data) {
	auto locale = CFLocaleCopyCurrent();
	auto str = makeString(data);

	CFStringCapitalize(str, locale);

	auto ret = toString(str, cb);

	CFRelease(str);
	CFRelease(locale);
	return ret;
}
bool tolower(const callback<void(StringView)> &cb, StringView data) {
	auto locale = CFLocaleCopyCurrent();
	auto str = makeString(data);

	CFStringLowercase(str, locale);

	auto ret = toString(str, cb);

	CFRelease(str);
	CFRelease(locale);
	return ret;
}

bool toupper(const callback<void(WideStringView)> &cb, WideStringView data) {
	auto locale = CFLocaleCopyCurrent();
	auto str = makeString(data);

	CFStringUppercase(str, locale);

	auto ret = toWideString(str, cb);

	CFRelease(str);
	CFRelease(locale);
	return ret;
}
bool totitle(const callback<void(WideStringView)> &cb, WideStringView data) {
	auto locale = CFLocaleCopyCurrent();
	auto str = makeString(data);

	CFStringCapitalize(str, locale);

	auto ret = toWideString(str, cb);

	CFRelease(str);
	CFRelease(locale);
	return ret;
}
bool tolower(const callback<void(WideStringView)> &cb, WideStringView data) {
	auto locale = CFLocaleCopyCurrent();
	auto str = makeString(data);

	CFStringLowercase(str, locale);

	auto ret = toWideString(str, cb);

	CFRelease(str);
	CFRelease(locale);
	return ret;
}

bool compare(StringView l, StringView r, int *res) {
	if (!res) {
		return false;
	}

	auto locale = CFLocaleCopyCurrent();
	auto lstr = makeString(l);
	auto rstr = makeString(r);

	*res = CFStringCompareWithOptionsAndLocale(lstr, rstr, CFRangeMake(0, CFStringGetLength(lstr)),
			kCFCompareLocalized, locale);

	CFRelease(rstr);
	CFRelease(lstr);
	CFRelease(locale);
	return true;
}

bool compare(WideStringView l, WideStringView r, int *res) {
	if (!res) {
		return false;
	}

	auto locale = CFLocaleCopyCurrent();
	auto lstr = makeString(l);
	auto rstr = makeString(r);

	*res = CFStringCompareWithOptionsAndLocale(lstr, rstr, CFRangeMake(0, CFStringGetLength(lstr)),
			kCFCompareLocalized, locale);

	CFRelease(rstr);
	CFRelease(lstr);
	CFRelease(locale);
	return true;
}

bool caseCompare(StringView l, StringView r, int *res) {
	if (!res) {
		return false;
	}

	auto locale = CFLocaleCopyCurrent();
	auto lstr = makeString(l);
	auto rstr = makeString(r);

	*res = CFStringCompareWithOptionsAndLocale(lstr, rstr, CFRangeMake(0, CFStringGetLength(lstr)),
			kCFCompareLocalized | kCFCompareCaseInsensitive, locale);

	CFRelease(rstr);
	CFRelease(lstr);
	CFRelease(locale);
	return true;
}

bool caseCompare(WideStringView l, WideStringView r, int *res) {
	if (!res) {
		return false;
	}

	auto locale = CFLocaleCopyCurrent();
	auto lstr = makeString(l);
	auto rstr = makeString(r);

	*res = CFStringCompareWithOptionsAndLocale(lstr, rstr, CFRangeMake(0, CFStringGetLength(lstr)),
			kCFCompareLocalized | kCFCompareCaseInsensitive, locale);

	CFRelease(rstr);
	CFRelease(lstr);
	CFRelease(locale);
	return true;
}

bool idnToAscii(const callback<void(StringView)> &cb, StringView source) {
	UErrorCode err = U_ZERO_ERROR;
	auto idna = uidna_openUTS46(UIDNA_CHECK_BIDI | UIDNA_NONTRANSITIONAL_TO_ASCII, &err);
	if (err == U_ZERO_ERROR) {
		UIDNAInfo info = {0, 0};
		char buffer[1_KiB] = {0};
		auto retLen = uidna_nameToASCII_UTF8(idna, source.data(), (int)source.size(), buffer,
				1_KiB - 1, &info, &err);
		uidna_close(idna);
		if (retLen > 0 && err == 0 && !info.errors) {
			cb(StringView(buffer, retLen));
			return true;
		}
	}
	return false;
}

bool idnToUnicode(const callback<void(StringView)> &cb, StringView source) {
	UErrorCode err = U_ZERO_ERROR;
	auto idna = uidna_openUTS46(UIDNA_CHECK_BIDI | UIDNA_NONTRANSITIONAL_TO_UNICODE, &err);
	if (err == U_ZERO_ERROR) {
		char buffer[1_KiB] = {0};
		UIDNAInfo info = {0, 0};
		auto retLen = uidna_nameToUnicodeUTF8(idna, source.data(), (int)source.size(), buffer,
				1_KiB - 1, &info, &err);
		uidna_close(idna);
		if (retLen > 0 && err == 0 && !info.errors) {
			cb(StringView(buffer, retLen));
			return true;
		}
	}
	return false;
}

} // namespace sprt::unicode

namespace sprt::platform {

char GlobalConfig::localeBuf[6] = "en-us";
static GlobalConfig s_globalConfig;

thread_local char tl_localeBuf[64] = {0};

StringView getOsLocale() {
	CFLocaleRef cflocale = CFLocaleCopyCurrent();
	auto value = (CFStringRef)CFLocaleGetIdentifier(cflocale);
	CFStringGetCString(value, tl_localeBuf, 64, kCFStringEncodingUTF8);
	CFRelease(cflocale);
	return StringView(tl_localeBuf);
}

typedef int (*gethostuuid_fn)(uint8_t[16], const struct timespec *);

StringView getUniqueDeviceId() {
	if (s_globalConfig.uniqueIdBuf.empty()) {
		// optimistic multithreaded lazy-init
		// it can allocate more-then needed memory but protected from general lock

		Dso dso = Dso(StringView(), DsoFlags::Self);

		uint8_t uuid[UuidSize];
		struct timespec ts = {0, 0};
		auto gethostuuid = dso.sym<gethostuuid_fn>("gethostuuid");
		if (gethostuuid && gethostuuid(uuid, &ts) == 0) {
			char fmt[UuidFormattedSize];
			formatuuid(fmt, uuid);

			unique_lock lock(s_globalConfig.infoMutex);
			s_globalConfig.uniqueIdBuf = StringView(fmt).pdup(s_globalConfig._pool);
		}
	}

	return s_globalConfig.uniqueIdBuf;
}

StringView getExecPath() {
	if (s_globalConfig.execPathBuf.empty()) {
		// optimistic multithreaded lazy-init
		// it can allocate more-then needed memory but protected from general lock

		uint32_t bufSize = 1'024;
		auto buf = __sprt_typed_malloca(char, bufSize);

		_NSGetExecutablePath(buf, &bufSize);

		unique_lock lock(s_globalConfig.infoMutex);
		s_globalConfig.execPathBuf = StringView(buf, bufSize).pdup(s_globalConfig._pool);

		__sprt_freea(buf);
	}

	return s_globalConfig.execPathBuf;
}

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

bool initialize(AppConfig &&cfg, int &resultCode) {
	s_globalConfig.config.bundleName = cfg.bundleName.pdup(s_globalConfig._pool);
	s_globalConfig.config.bundlePath = cfg.bundlePath.pdup(s_globalConfig._pool);
	s_globalConfig.config.pathScheme = cfg.pathScheme;

	s_globalConfig.current.lookupType = filesystem::LookupFlags::Public
			| filesystem::LookupFlags::Shared | filesystem::LookupFlags::Writable;
	s_globalConfig.current.locationFlags = filesystem::LocationFlags::Writable;
	s_globalConfig.current.interface = filesystem::getDefaultInterface();

	filesystem::getCurrentDir([&](StringView path) {
		s_globalConfig.current.path = path.pdup(s_globalConfig._pool);
	});

	return true;
}

void terminate() { }

memory::pool_t *getConfigPool() { return s_globalConfig._pool; }

} // namespace sprt::platform

namespace sprt {

const AppConfig &getAppConfig() { return platform::s_globalConfig.config; }

} // namespace sprt

namespace sprt::filesystem {

const LocationInfo &getCurrentLocation() { return platform::s_globalConfig.current; }

} // namespace sprt::filesystem

#endif
