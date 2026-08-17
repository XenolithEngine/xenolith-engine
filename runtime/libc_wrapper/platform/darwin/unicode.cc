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

#if SPRT_APPLE

#include <sprt/runtime/platform.h>
#include <sprt/runtime/callback.h>
#include <sprt/runtime/stringview.h>
#include <sprt/runtime/unicode.h>
#include <sprt/runtime/utils/uuid.h>
#include <sprt/runtime/utils/dso.h>
#include <sprt/cxx/mutex>


#include "../src/private/SPRTPrivate.h"

#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFLocale.h>

#include <unistd.h>

extern "C" int _NSGetExecutablePath(char *buf, uint32_t *bufsize);

namespace sprt::unicode {

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

// No case mapping here any more, for code points or for strings: it all comes
// from the compiled-in Unicode tables (runtime/src/unicode). CoreFoundation is
// only asked about collation now, which is why the CFString -> callback readers
// that used to be here are gone with it: comparing needs the strings going in,
// not the text coming out.

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
