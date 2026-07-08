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

#if SPRT_WASM

#include <sprt/runtime/filesystem/lookup.h>
#include <sprt/runtime/filesystem/filepath.h>
#include <sprt/runtime/enum.h>
#include "private/SPRTFilesystem.h"

// The wasm VFS routes by mount prefix (see wasm/libc_file_ops.cc): "/tmp" and every
// unmounted path is an in-memory tmpfs; "/opfs" is the persistent, browser-backed
// Origin Private File System; a path served by the JS bundle overlay is read-only.
// All file I/O therefore rides the shared POSIX LocationInterface (getDefaultInterface,
// SPRuntimeFilesystemPosix.cpp) — this unit only lays out which category maps to which
// mount, plus the environment-variable seam.

namespace sprt::filesystem::detail {

StringView _readEnvExt(memory::pool_t *pool, StringView key) {
	if (key == "EXEC_DIR") {
		return filepath::root(platform::getExecPath()).pdup(pool);
	} else if (key == "CWD") {
		StringView ret;
		getCurrentDir([&](StringView path) { ret = path.pdup(pool); });
		return ret;
	}
	// The sandbox exposes no environment variables (no getenv in the wasm libc).
	return StringView();
}

// Mount roots. Temporary categories live in the tmpfs (lost on reload); persistent
// categories live in OPFS (survive reload); the read-only app bundle comes from JS.
static constexpr StringView TmpRoot = StringView("/tmp");
static constexpr StringView OpfsRoot = StringView("/opfs");

static void addLocation(LookupData &data, LocationCategory cat, StringView root,
		StringView sub) {
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

	// %PLATFORM% — the read-only app bundle (JS fetch overlay), from AppConfig.bundlePath.
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
		// No explicit bundlePath: default the read-only bundle to "/app", matching the platform
		// resource dir (see SPRuntimePlatform-wasm.cc). Bundled files then resolve to
		// "/app/<path>" and reach the fetch overlay via the libc bundle_read host import.
		bundledLoc.paths.emplace_back(LocationInfo{
			StringView("/app"),
			LookupFlags::Private,
			LocationFlags::Locateable,
			defaultInterface,
		});
	}

	StringView bundle = appConfig.bundleName.empty() ? StringView("app") : appConfig.bundleName;

	// Persistent → OPFS.
	addLocation(data, LocationCategory::UserHome, OpfsRoot, "home");
	addLocation(data, LocationCategory::CommonData, OpfsRoot, "data");
	addLocation(data, LocationCategory::CommonConfig, OpfsRoot, "config");
	addLocation(data, LocationCategory::CommonState, OpfsRoot, "state");
	addLocation(data, LocationCategory::Fonts, OpfsRoot, "fonts");
	filepath::merge([&](StringView appRoot) {
		addLocation(data, LocationCategory::AppData, appRoot, "data");
		addLocation(data, LocationCategory::AppConfig, appRoot, "config");
		addLocation(data, LocationCategory::AppState, appRoot, "state");
	}, OpfsRoot, StringView("app"), bundle);

	// Temporary → tmpfs.
	addLocation(data, LocationCategory::CommonCache, TmpRoot, "cache");
	addLocation(data, LocationCategory::CommonRuntime, TmpRoot, "runtime");
	filepath::merge([&](StringView appRoot) {
		addLocation(data, LocationCategory::AppCache, appRoot, "cache");
		addLocation(data, LocationCategory::AppRuntime, appRoot, "runtime");
	}, TmpRoot, StringView("app"), bundle);

	// User media directories: collapse onto the persistent home root.
	for (auto it :
			each<LocationCategory, LocationCategory::UserDesktop, LocationCategory::UserVideos>()) {
		auto &res = data._resourceLocations[toInt(it)];
		if (res.paths.empty()) {
			filepath::merge([&](StringView path) {
				res.paths.emplace_back(LocationInfo{
					path.pdup(data._pool),
					LookupFlags::Shared,
					LocationFlags::Locateable,
					defaultInterface,
				});
			}, OpfsRoot, "home");
			res.init = true;
		}
	}
}

void _termSystemPaths(LookupData &) { }

} // namespace sprt::filesystem::detail

#endif
