/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
 **/

// NuttX filesystem path layout — minimal port of SPRuntimeFilesystem-wasm.cc.
//
// The qemu-armv8a NuttX flat build has no XDG, no /home, no /usr/share. The
// board's filesystem (procfs + whatever the app registers) is rooted at "/",
// with "/tmp" the conventional writable scratch directory. The soft-renderer
// milestone reads no external files (ExampleScene is procedural), so the
// layout here only needs to satisfy the engine's init-time path enumeration:
// every category maps to "/tmp" so lookups resolve to a writable location the
// NSH shell can inspect, and reads of missing files fail cleanly through the
// shared POSIX LocationInterface (getDefaultInterface in
// SPRuntimeFilesystemPosix.cpp).

#include <sprt/runtime/platform.h>

#if SPRT_NUTTX

#include <sprt/runtime/filesystem/lookup.h>
#include <sprt/runtime/filesystem/filepath.h>
#include <sprt/runtime/enum.h>
#include "private/SPRTFilesystem.h"

#include <stdlib.h>

namespace sprt::filesystem::detail {

StringView _readEnvExt(memory::pool_t *pool, StringView key) {
	if (key == "EXEC_DIR") {
		return filepath::root(platform::getExecPath()).pdup(pool);
	} else if (key == "CWD") {
		StringView ret;
		getCurrentDir([&](StringView path) { ret = path.pdup(pool); });
		return ret;
	}
	auto var = ::getenv(key.data());
	if (!var) {
		return StringView();
	}
	return StringView(var, ::__sprt_strlen(var)).pdup(pool);
}

static constexpr StringView TmpRoot = StringView("/tmp");

static void addLocation(LookupData &data, LocationCategory cat, StringView root, StringView sub) {
	auto &res = data._resourceLocations[toInt(cat)];
	filepath::merge([&](StringView path) {
		res.paths.emplace_back(LocationInfo{
			path.pdup(data._pool),
			LookupFlags::Private | LookupFlags::Public | LookupFlags::Writable,
			LocationFlags::Locateable | LocationFlags::Writable,
			getDefaultInterface(),
		});
	}, root, sub);
	res.init = true;
}

void _initSystemPaths(LookupData &data) {
	auto defaultInterface = getDefaultInterface();
	auto &appConfig = getAppConfig();

	// %PLATFORM% — the read-only app bundle. On NuttX there is no installer
	// bundle; the binary is the flat image. Default to "/app" if AppConfig
	// does not name one so bundled-resource lookups fail predictably.
	auto &bundledLoc = data._resourceLocations[toInt(LocationCategory::Bundled)];
	bundledLoc.init = true;
	if (!appConfig.bundlePath.empty()) {
		appConfig.bundlePath.split<StringView::Chars<':'>>([&](StringView str) {
			auto value = readVariable(data._pool, str);
			if (!value.empty()) {
				bundledLoc.paths.emplace_back(LocationInfo{
					value,
					LookupFlags::Private,
					LocationFlags::Locateable,
					defaultInterface,
				});
			}
		});
	} else {
		bundledLoc.paths.emplace_back(LocationInfo{
			StringView("/app"),
			LookupFlags::Private,
			LocationFlags::Locateable,
			defaultInterface,
		});
	}

	// Everything else maps under /tmp — the one writable directory the
	// qemu-armv8a board ships. The renderer milestone does not read or write
	// any of these; the mapping only needs to be valid for init-time enumeration.
	addLocation(data, LocationCategory::UserHome, TmpRoot, "home");
	addLocation(data, LocationCategory::CommonData, TmpRoot, "data");
	addLocation(data, LocationCategory::CommonConfig, TmpRoot, "config");
	addLocation(data, LocationCategory::CommonState, TmpRoot, "state");
	addLocation(data, LocationCategory::Fonts, TmpRoot, "fonts");

	StringView bundle = appConfig.bundleName.empty() ? StringView("app") : appConfig.bundleName;

	addLocation(data, LocationCategory::AppData, TmpRoot, bundle);
	addLocation(data, LocationCategory::AppConfig, TmpRoot, bundle);
	addLocation(data, LocationCategory::AppState, TmpRoot, bundle);
	addLocation(data, LocationCategory::AppCache, TmpRoot, bundle);
	addLocation(data, LocationCategory::AppRuntime, TmpRoot, bundle);
}

void _termSystemPaths(LookupData &) { }

} // namespace sprt::filesystem::detail

#endif // SPRT_NUTTX
