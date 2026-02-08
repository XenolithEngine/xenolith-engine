/**
 Copyright (c) 2025 Xenolith Team <admin@xenolith.studio>

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

#include <sprt/runtime/init.h>

#if SPRT_WINDOWS

#include <sprt/runtime/stringview.h>
#include <sprt/runtime/platform.h>
#include <sprt/runtime/mutex.h>
#include <sprt/runtime/filesystem/filepath.h>

#include <sys/winapi.h>

#include "private/SPRTPrivate.h"

namespace sprt::unicode {

static auto mapBuffer(WideStringView data, char16_t *buf, size_t count, int flags) {
	return _LCMapString(LOCALE_NAME_USER_DEFAULT, flags, data.data(), data.size(), buf, int(count));
}

template <typename Interface>
auto mapString(WideStringView data, int flags) {
	auto bufSize = LCMapStringEx(LOCALE_NAME_SYSTEM_DEFAULT, flags, (wchar_t *)data.data(),
			data.size(), (wchar_t *)nullptr, 0, nullptr, nullptr, 0);

	typename Interface::WideStringType ret;
	ret.resize(bufSize);

	mapBuffer(data, ret.data(), ret.size(), flags);

	return ret;
}

char32_t tolower(char32_t c) {
	char16_t bufA[4];
	char16_t bufB[8];

	auto size = unicode::utf16EncodeBuf(bufA, 3, c);
	auto bufSize = mapBuffer(WideStringView(bufA, size), bufB, 8, LCMAP_LOWERCASE);
	if (bufSize > 0) {
		uint8_t off;
		return unicode::utf16Decode32(bufB, 8, off);
	}
	return c;
}

char32_t toupper(char32_t c) {
	char16_t bufA[4];
	char16_t bufB[8];

	auto size = unicode::utf16EncodeBuf(bufA, 3, c);
	auto bufSize = mapBuffer(WideStringView(bufA, size), bufB, 8, LCMAP_UPPERCASE);
	if (bufSize > 0) {
		uint8_t off;
		return unicode::utf16Decode32(bufB, 8, off);
	}
	return c;
}

char32_t totitle(char32_t c) {
	char16_t bufA[4];
	char16_t bufB[8];

	auto size = unicode::utf16EncodeBuf(bufA, 3, c);
	auto bufSize = mapBuffer(WideStringView(bufA, size), bufB, 8, LCMAP_TITLECASE);
	if (bufSize > 0) {
		uint8_t off;
		return unicode::utf16Decode32(bufB, 8, off);
	}
	return c;
}

bool toupper(const callback<void(StringView)> &cb, StringView data) {
	bool ret = false;
	toUtf16([&](WideStringView uData) {
		ret = toupper([&](WideStringView result) { toUtf8(cb, result); }, uData);
	}, data);
	return ret;
}

bool totitle(const callback<void(StringView)> &cb, StringView data) {
	bool ret = false;
	toUtf16([&](WideStringView uData) {
		ret = totitle([&](WideStringView result) { toUtf8(cb, result); }, uData);
	}, data);
	return ret;
}

bool tolower(const callback<void(StringView)> &cb, StringView data) {
	bool ret = false;
	toUtf16([&](WideStringView uData) {
		ret = tolower([&](WideStringView result) { toUtf8(cb, result); }, uData);
	}, data);
	return ret;
}

bool toupper(const callback<void(WideStringView)> &cb, WideStringView data) {
	auto bufSize = mapBuffer(data, nullptr, 0, LCMAP_UPPERCASE);
	if (bufSize <= 0) {
		return false;
	}

	auto buf = __sprt_typed_malloca(char16_t, bufSize + 1);
	bufSize = mapBuffer(data, buf, bufSize + 1, LCMAP_UPPERCASE);
	if (bufSize <= 0) {
		__sprt_freea(buf);
		return false;
	}
	buf[bufSize] = 0;

	cb(WideStringView(buf, bufSize));
	__sprt_freea(buf);
	return true;
}

bool totitle(const callback<void(WideStringView)> &cb, WideStringView data) {
	auto bufSize = mapBuffer(data, nullptr, 0, LCMAP_LOWERCASE);
	if (bufSize <= 0) {
		return false;
	}

	auto buf = __sprt_typed_malloca(char16_t, bufSize + 1);
	bufSize = mapBuffer(data, buf, bufSize + 1, LCMAP_LOWERCASE);
	if (bufSize <= 0) {
		__sprt_freea(buf);
		return false;
	}
	buf[bufSize] = 0;

	cb(WideStringView(buf, bufSize));
	__sprt_freea(buf);
	return true;
}

bool tolower(const callback<void(WideStringView)> &cb, WideStringView data) {
	auto bufSize = mapBuffer(data, nullptr, 0, LCMAP_TITLECASE);
	if (bufSize <= 0) {
		return false;
	}

	auto buf = __sprt_typed_malloca(char16_t, bufSize + 1);
	bufSize = mapBuffer(data, buf, bufSize + 1, LCMAP_TITLECASE);
	if (bufSize <= 0) {
		__sprt_freea(buf);
		return false;
	}
	buf[bufSize] = 0;

	cb(WideStringView(buf, bufSize));
	__sprt_freea(buf);
	return true;
}

bool compare(StringView l, StringView r, int *result) {
	bool ret = false;
	unicode::toUtf16([&](WideStringView lStr) {
		unicode::toUtf16([&](WideStringView rStr) { ret = compare(lStr, rStr, result); }, r);
	}, l);
	return ret;
}

bool compare(WideStringView l, WideStringView r, int *result) {
	auto ret = _CompareString(LOCALE_NAME_SYSTEM_DEFAULT, NORM_LINGUISTIC_CASING, l.data(),
			l.size(), r.data(), r.size());
	if (ret > 0) {
		*result = ret - CSTR_EQUAL;
		return true;
	}
	return false;
}

bool caseCompare(StringView l, StringView r, int *result) {
	bool ret = false;
	unicode::toUtf16([&](WideStringView lStr) {
		unicode::toUtf16([&](WideStringView rStr) { ret = caseCompare(lStr, rStr, result); }, r);
	}, l);
	return ret;
}

bool caseCompare(WideStringView l, WideStringView r, int *result) {
	auto ret = _CompareString(LOCALE_NAME_SYSTEM_DEFAULT,
			NORM_LINGUISTIC_CASING | NORM_IGNORECASE | LINGUISTIC_IGNORECASE, l.data(), l.size(),
			r.data(), r.size());
	if (ret > 0) {
		*result = ret - CSTR_EQUAL;
		return true;
	}
	return false;
}

bool idnToAscii(const callback<void(StringView)> &cb, StringView source) {
	bool ret = false;
	unicode::toUtf16([&](WideStringView usource) {
		auto bufSize = _IdnToAscii(0, usource.data(), usource.size(), nullptr, 0);
		if (bufSize == 0) {
			return;
		}

		auto buf = __sprt_typed_malloca(char16_t, bufSize + 1);

		bufSize = _IdnToAscii(0, usource.data(), usource.size(), buf, bufSize + 1);
		if (bufSize == 0) {
			__sprt_freea(buf);
			return;
		}

		buf[bufSize] = 0;
		unicode::toUtf8(cb, WideStringView(buf, bufSize));

		__sprt_freea(buf);
		ret = true;
	}, source);
	return ret;
}

bool idnToUnicode(const callback<void(StringView)> &cb, StringView source) {
	bool ret = false;
	unicode::toUtf16([&](WideStringView usource) {
		auto bufSize = _IdnToUnicode(0, usource.data(), usource.size(), nullptr, 0);
		if (bufSize == 0) {
			return;
		}

		auto buf = __sprt_typed_malloca(char16_t, bufSize + 1);

		bufSize = _IdnToUnicode(0, usource.data(), usource.size(), buf, bufSize + 1);
		if (bufSize == 0) {
			__sprt_freea(buf);
			return;
		}

		buf[bufSize] = 0;
		unicode::toUtf8(cb, WideStringView(buf, bufSize));

		__sprt_freea(buf);
		ret = true;
	}, source);
	return ret;
}

} // namespace sprt::unicode

namespace sprt::platform {

struct GlobalConfig {
	qmutex infoMutex;
	StringView uniqueIdBuf;
	StringView execPathBuf;
	StringView homePathBuf;

	AppConfig config;

	filesystem::LocationInfo current;

	memory::pool_t *_pool = memory::pool::create(memory::self_contained_allocator);
};

static GlobalConfig s_globalConfig;

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

	if (platform::isAppContainer()
			|| s_globalConfig.config.pathScheme < AppLocationScheme::ContainerRelative) {
		return true;
	}

	if (!platform::initAppContainer(s_globalConfig.config.bundleName,
				s_globalConfig.config.appName)) {
		return false;
	}

	if (s_globalConfig.config.pathScheme == AppLocationScheme::ContainerRelative) {
		// only use container for paths
		return true;
	}

	return platform::runSelfInContainer(resultCode);
}

void terminate() { }

memory::pool_t *getConfigPool() { return s_globalConfig._pool; }

StringView getOsLocale() {
	static char locale[32] = {0};
	static char16_t wlocale[32] = {0};
	auto len = _GetUserDefaultLocaleName(wlocale, 32);

	if (locale[0] == 0) {
		auto writePtr = locale;
		auto ptr = wlocale;
		auto end = ptr + len - 1;
		while (*ptr && ptr < end) {
			uint8_t offset = 0;
			auto c = unicode::utf16Decode32(ptr, 32, offset);
			if (offset > 0) {
				writePtr += unicode::utf8EncodeBuf(writePtr, 32, c);
				ptr += offset;
			} else {
				break;
			}
		}
	}
	return StringView(locale);
}

StringView getUniqueDeviceId() {
	if (s_globalConfig.uniqueIdBuf.empty()) {
		// optimistic multithreaded lazy-init
		platform::getMachineId([](StringView str) {
			unique_lock lock(s_globalConfig.infoMutex);
			s_globalConfig.uniqueIdBuf = str.pdup(s_globalConfig._pool);
		});
	}

	return s_globalConfig.uniqueIdBuf;
}

StringView getExecPath() {
	if (s_globalConfig.execPathBuf.empty()) {
		// optimistic multithreaded lazy-init
		platform::getAppPath([](StringView str) {
			unique_lock lock(s_globalConfig.infoMutex);
			s_globalConfig.execPathBuf = str.pdup(s_globalConfig._pool);
		});
	}

	return s_globalConfig.execPathBuf;
}

StringView getHomePath() {
	if (s_globalConfig.homePathBuf.empty()) {
		// optimistic multithreaded lazy-init
		platform::getHomePath([](StringView str) {
			unique_lock lock(s_globalConfig.infoMutex);
			s_globalConfig.homePathBuf = str.pdup(s_globalConfig._pool);
		});
	}
	return s_globalConfig.homePathBuf;
}

} // namespace sprt::platform

namespace sprt {

const AppConfig &getAppConfig() { return platform::s_globalConfig.config; }

} // namespace sprt

namespace sprt::filesystem {

const LocationInfo &getCurrentLocation() { return platform::s_globalConfig.current; }

} // namespace sprt::filesystem

#endif
