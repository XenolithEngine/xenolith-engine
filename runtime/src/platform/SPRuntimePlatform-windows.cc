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
#include <sprt/cxx/mutex>
#include <sprt/runtime/filesystem/filepath.h>

#include "private/SPRTPrivate.h"

#include <sprt/wrappers/windows/windows.h>

namespace sprt::platform {

char GlobalConfig::localeBuf[6] = "en-us";
static GlobalConfig s_globalConfig;

bool initialize(AppConfig &&cfg, int &resultCode) {
	// The config pool is per initialize()/terminate() cycle, not per process;
	// see GlobalConfig::_pool in private/SPRTPrivate.h.
	s_globalConfig.init();
	s_globalConfig.config.bundleName = cfg.bundleName.pdup(s_globalConfig.pool());
	s_globalConfig.config.bundlePath = cfg.bundlePath.pdup(s_globalConfig.pool());
	s_globalConfig.config.pathScheme = cfg.pathScheme;

	s_globalConfig.current.lookupType = filesystem::LookupFlags::Public
			| filesystem::LookupFlags::Shared | filesystem::LookupFlags::Writable;
	s_globalConfig.current.locationFlags = filesystem::LocationFlags::Writable;
	s_globalConfig.current.interface = filesystem::getDefaultInterface();

	filesystem::getCurrentDir([&](StringView path) {
		s_globalConfig.current.path = path.pdup(s_globalConfig.pool());
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

void terminate() { s_globalConfig.term(); }

memory::pool_t *getConfigPool() { return s_globalConfig.pool(); }

StringView getOsLocale() {
	static char locale[32] = {0};
	static char16_t wlocale[32] = {0};
	auto len = GetUserDefaultLocaleName((wchar_t *)wlocale, 32);

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
			s_globalConfig.uniqueIdBuf = str.pdup(s_globalConfig.pool());
		});
	}

	return s_globalConfig.uniqueIdBuf;
}

StringView getExecPath() {
	if (s_globalConfig.execPathBuf.empty()) {
		// optimistic multithreaded lazy-init
		platform::getAppPath([](StringView str) {
			unique_lock lock(s_globalConfig.infoMutex);
			s_globalConfig.execPathBuf = str.pdup(s_globalConfig.pool());
		});
	}

	return s_globalConfig.execPathBuf;
}

StringView getHomePath() {
	if (s_globalConfig.homePathBuf.empty()) {
		// optimistic multithreaded lazy-init
		platform::getHomePath([](StringView str) {
			unique_lock lock(s_globalConfig.infoMutex);
			s_globalConfig.homePathBuf = str.pdup(s_globalConfig.pool());
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
