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

#include <sprt/runtime/init.h>

#if SPRT_WASM

#include <sprt/runtime/filesystem/lookup.h>
#include <sprt/runtime/filesystem/filepath.h>
#include <sprt/runtime/platform.h>
#include <sprt/cxx/__mutex/unique_lock.h>
#include <sprt/c/__sprt_time.h>
#include <sprt/c/__sprt_unistd.h>

extern "C" {

// T1 host import: nanoseconds for the given clock id (see JS `clock_now`);
// clkid 1 == MONOTONIC, else REALTIME. Matches runtime/core/wasm/clock_gettime.cc.
__attribute__((import_module("sprt"), import_name("clock_now"))) double __sprt_host_clock_now(
		int clkid);

// Copies the host UI locale (BCP-47, e.g. "en-US") as UTF-8 into [dst, dst+cap);
// returns the byte length written (never NUL-terminated by the host), or 0 if the
// host cannot report a locale. See JS `os_locale`.
__attribute__((import_module("sprt"), import_name("os_locale"))) int __sprt_host_os_locale(
		char *dst, int cap);
}

namespace sprt::platform {

char GlobalConfig::localeBuf[6] = "en-us";
static GlobalConfig s_globalConfig;

// The browser/node sandbox exposes no notion of CPU-time clocks; every clock maps
// to the single monotonic/realtime host import. Hardware falls back to monotonic.
static uint64_t nanoclockFor(ClockType type) {
	int clkid = 1; // MONOTONIC
	switch (type) {
	case ClockType::Realtime: clkid = 0; break;
	case ClockType::Default:
	case ClockType::Monotonic:
	case ClockType::Process:
	case ClockType::Thread:
	case ClockType::Hardware: clkid = 1; break;
	}
	return static_cast<uint64_t>(__sprt_host_clock_now(clkid));
}

uint64_t clock(ClockType type) { return nanoclockFor(type) / 1'000; }

uint64_t nanoclock(ClockType type) { return nanoclockFor(type); }

void sleep(uint64_t microseconds) {
	::__sprt_usleep(static_cast<__SPRT_ID(time_t)>(microseconds));
}

// WebAssembly linear memory grows in fixed 64 KiB pages.
uint32_t getMemoryPageSize() { return 65'536; }

StringView getOsLocale() {
	// Query the host once and cache in the config pool.
	static StringView s_locale = [] {
		char buf[64];
		auto len = __sprt_host_os_locale(buf, int(sizeof(buf)));
		if (len <= 0) {
			return StringView();
		}
		return StringView(buf, size_t(len)).pdup(s_globalConfig._pool);
	}();
	return s_locale;
}

// No stable per-device identifier is available inside the sandbox.
StringView getUniqueDeviceId() { return StringView(); }

StringView getExecPath() {
	if (s_globalConfig.execPathBuf.empty()) {
		// The wasm module has no real path on disk; expose a stable synthetic exec
		// path so executable-relative resource lookup resolves under a known VFS root.
		// bundlePath (if any) supplies the directory; otherwise fall back to "/app".
		auto &cfg = s_globalConfig.config;
		StringView dir = cfg.bundlePath.empty() ? StringView("/app") : cfg.bundlePath;
		StringView name = cfg.appName.empty() ? StringView("app.wasm") : cfg.appName;

		unique_lock lock(s_globalConfig.infoMutex);
		filepath::merge([&](StringView path) {
			s_globalConfig.execPathBuf = path.pdup(s_globalConfig._pool);
		}, dir, name);
	}
	return s_globalConfig.execPathBuf;
}

StringView getHomePath() {
	if (s_globalConfig.homePathBuf.empty()) {
		// No environment/home concept in the sandbox; the VFS root is the home root.
		unique_lock lock(s_globalConfig.infoMutex);
		s_globalConfig.homePathBuf = StringView("/").pdup(s_globalConfig._pool);
	}
	return s_globalConfig.homePathBuf;
}

bool initialize(AppConfig &&cfg, int &resultCode) {
	s_globalConfig.config.bundleName = cfg.bundleName.pdup(s_globalConfig._pool);
	s_globalConfig.config.appName = cfg.appName.pdup(s_globalConfig._pool);
	s_globalConfig.config.bundlePath = cfg.bundlePath.pdup(s_globalConfig._pool);
	s_globalConfig.config.versionCode = cfg.versionCode;
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
