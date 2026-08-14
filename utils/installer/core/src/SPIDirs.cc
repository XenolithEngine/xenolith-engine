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

#include "SPIDirs.h"

#include <stdlib.h> // getenv: there is no runtime wrapper for the environment

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

Layout Layout::fromHome(StringView home) {
	Layout l;
	l.config = mergePath(home, "config");
	l.data = mergePath(home, "data");
	l.cache = mergePath(home, "cache");
	return l;
}

// Where an installation made before the switch to location categories lives: one root under the
// shared data dir, holding config/data/cache side by side.
static constexpr StringView kLegacyDir("xenolith");

// First location the runtime offers for a category — the user-level, writable one. Empty when the
// platform does not populate that category at all (macOS fills AppData/AppCache but not AppConfig;
// Windows has no CommonConfig).
static String categoryRoot(FileCategory category) {
	String ret;
	filesystem::enumeratePaths(category, [&](const LocationInfo &, StringView path) -> bool {
		ret = toString(path);
		return false; // stop at the first
	});
	return ret;
}

Layout Layout::system() {
	// Nothing here is spelled out per platform: the App* categories are the runtime's app-specific,
	// read-write locations, placed from APPCONFIG_BUNDLE_NAME (XDG on Linux, the system AppData
	// folder on Windows, ~/Library on macOS). The CLI and the GUI declare the SAME bundle name and
	// the same APPCONFIG_APP_PATH_COMMON, which is what makes both land on one store — see the note
	// in utils/installer/Makefile.
	auto dataRoot = categoryRoot(FileCategory::AppData);
	if (dataRoot.empty()) {
		// No app directory at all (a stripped or sandboxed environment) — stay runnable rather than
		// hand back empty paths that would fail much later.
		return fromHome(mergePath(StringView("/tmp"), kLegacyDir));
	}

	// Compatibility: an installation made before this switch keeps config, data and cache under one
	// root in the SHARED data dir (<CommonData>/xenolith/{config,data,cache}). Keep using it when it
	// is there, so an already-installed SDK is not stranded.
	if (auto commonRoot = categoryRoot(FileCategory::CommonData); !commonRoot.empty()) {
		auto legacyRoot = mergePath(commonRoot, kLegacyDir);
		if (isDirectory(mergePath(legacyRoot, "data"))) {
			return fromHome(legacyRoot);
		}
	}

	auto configRoot = categoryRoot(FileCategory::AppConfig);
	auto cacheRoot = categoryRoot(FileCategory::AppCache);

	Layout l;
	l.config = configRoot.empty() ? mergePath(dataRoot, "config") : toString(configRoot);
	l.data = dataRoot;
	l.cache = cacheRoot.empty() ? mergePath(dataRoot, "cache") : toString(cacheRoot);
	return l;
}

Layout Layout::resolve(StringView prefix, StringView envHome) {
	if (!prefix.empty()) {
		return fromHome(prefix);
	}
	if (!envHome.empty()) {
		return fromHome(envHome);
	}
	return system();
}

Layout Layout::resolveFromEnv(StringView prefix) {
	const char *e = ::getenv("XENOLITH_HOME");
	return resolve(prefix, (e && *e) ? StringView(e) : StringView());
}

String Layout::getInstalledManifest() const { return mergePath(config, "installed.json"); }

String Layout::getProjectsManifest() const { return mergePath(config, "projects.json"); }

String Layout::getSettingsManifest() const { return mergePath(config, "settings.json"); }

String Layout::getToolchainsDir() const { return mergePath(data, "toolchains"); }

String Layout::getHostsDir() const { return mergePath(getToolchainsDir(), "hosts"); }

String Layout::getTargetsDir() const { return mergePath(getToolchainsDir(), "targets"); }

String Layout::getToolchainDir(Kind kind, StringView id) const {
	return mergePath(kind == Kind::Host ? getHostsDir() : getTargetsDir(), id);
}

String Layout::getEnginesDir() const { return mergePath(data, "engines"); }

String Layout::getEngineDir(StringView ref) const { return mergePath(getEnginesDir(), ref); }

String Layout::getDownloadDir() const { return mergePath(cache, "downloads"); }

} // namespace stappler::xenolith::installer
