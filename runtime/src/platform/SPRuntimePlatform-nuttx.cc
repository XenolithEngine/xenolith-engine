/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
 **/

// NuttX platform layer — minimal port of SPRuntimePlatform-linux.cc.
//
// NuttX is hosted-POSIX-on-its-own-libc, so the shapes mirror linux (env vars
// for locale/home, getcwd for current dir) but the surface is trimmed: the
// qemu-armv8a flat build has no /proc, no /etc/machine-id, no dynamic loader,
// and the FS is whatever the board registers. The renderer milestone reads
// no external files (ExampleScene is procedural), so getExecPath /
// getUniqueDeviceId return empty StringViews — the engine tolerates that.

#include <sprt/runtime/init.h>

#if SPRT_NUTTX

#include <sprt/runtime/filesystem/lookup.h>
#include <sprt/runtime/unicode.h>
#include <sprt/runtime/platform.h>
#include <sprt/cxx/__mutex/unique_lock.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_locale.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

namespace sprt::platform {

char GlobalConfig::localeBuf[6] = "en-us";
static GlobalConfig s_globalConfig;

StringView getOsLocale() {
	// LocaleIdentifier requires `lang-REGION`. POSIX "C"/"POSIX" is rejected and
	// the error path has crashed on NuttX. This board has no real locale DB.
	return StringView("en-us");
}

StringView getUniqueDeviceId() {
	// No /etc/machine-id on NuttX; return empty — callers must tolerate it
	// (the engine uses the unique id only for cache partitioning, irrelevant
	// to the soft-renderer milestone).
	return s_globalConfig.uniqueIdBuf;
}

StringView getExecPath() {
	// No /proc/self/exe on NuttX. The flat image is a single binary; its path
	// is not meaningful. Return empty.
	return s_globalConfig.execPathBuf;
}

StringView getHomePath() {
	if (s_globalConfig.homePathBuf.empty()) {
		auto path = StringView(::getenv("HOME"));
		unique_lock lock(s_globalConfig.infoMutex);
		s_globalConfig.homePathBuf = path.pdup(s_globalConfig._pool);
	}
	return s_globalConfig.homePathBuf;
}

bool initialize(AppConfig &&cfg, int &resultCode) {
	(void)resultCode;
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

#endif // SPRT_NUTTX
