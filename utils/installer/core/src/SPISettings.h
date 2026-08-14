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

#ifndef UTILS_INSTALLER_CORE_SRC_SPISETTINGS_H_
#define UTILS_INSTALLER_CORE_SRC_SPISETTINGS_H_

#include "SPICommon.h"
#include "SPIManifest.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

static constexpr uint32_t kSettingsSchemaVersion = 1;

/* Where the engine and the binary releases come from.

Split out of Settings because this is all the core operations need: an install does not care
whether auto-updates are on, and passing the whole Settings would tie every core entry point to a
type that grows every time the GUI gains a preference.

It is a PARAMETER, deliberately, and never an ambient global. A global would mean every core entry
point silently depends on "settings were loaded first", which the CLI - which does its own argument
parsing before it touches any config - would break sooner or later. A defaulted SourceConfig means
the built-in defaults, so a caller that has no opinion writes nothing. */
struct SP_PUBLIC SourceConfig {
	String engineRepoUrl; // empty -> getDefaultEngineRepoUrl()
	String releasesRoot; // empty -> getDefaultReleasesRoot()

	// Both resolved: never empty, whatever the fields hold.
	String getEngineRepoUrl() const;
	String getReleasesRoot() const;
	String getReleaseBase(StringView release) const;
};

// A per-tool auto-update override. Absent means "follow Settings::autoUpdateReleases".
struct SP_PUBLIC ToolAutoUpdate {
	Kind kind = Kind::Target;
	String id;
	bool enabled = false;
};

/* User preferences, at <config>/settings.json.

Lives in the core rather than in the GUI because BOTH front ends have to honour it: a user who
points the GUI at a mirror and then runs `xenolith-cli list` must reach the same mirror, and that
only works if the CLI reads the same file. Nothing here is required - a missing or unreadable file
yields the defaults, which are exactly what the tools did before this existed. */
struct SP_PUBLIC Settings {
	uint32_t schema = kSettingsSchemaVersion;

	SourceConfig sources;

	bool autoUpdateInstaller = false;
	bool autoUpdateEngine = false;
	bool autoUpdateReleases = false;

	String lang; // "en" | "ru" | "zh"; empty means "whatever the app defaults to"

	Vector<ToolAutoUpdate> tools;

	// A missing, unreadable or malformed file yields the defaults rather than an error: preferences
	// are not load-bearing, and refusing to start because one is corrupt would be worse than
	// ignoring it.
	static Settings load(StringView path);
	bool save(StringView path) const;

	// The per-tool override when there is one, else autoUpdateReleases.
	bool getToolAutoUpdate(Kind, StringView id) const;
	void setToolAutoUpdate(Kind, StringView id, bool);
};

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_CORE_SRC_SPISETTINGS_H_
