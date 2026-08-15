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
#include "SPIDirs.h" // Layout — applyTo() is what makes the stored paths take effect

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// 2: added enginePath / toolchainsPath. Readers of an older file get the defaults for both, and a
// version-1 reader ignores keys it does not know — so the bump is informational, not a gate.
static constexpr uint32_t kSettingsSchemaVersion = 2;

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

/* One settings.json key, described well enough to be driven generically.

The GUI form and `xenolith-cli config` both work off this table instead of each carrying its own
key→field switch: two switches is exactly how a key ends up settable from one front end and
invisible from the other, which is the thing this whole store exists to prevent. `boolean` is what
the CLI needs to know how to read the text the user typed. */
struct SP_PUBLIC SettingsField {
	StringView key;
	bool boolean = false; // else a string
	StringView description; // one line, English — the CLI prints it, the GUI has its own strings
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

	/* Where the LOCAL SDK is, as opposed to where it is downloaded from.

	`enginePath` is the checkout to build against (STAPPLER_ROOT). It sits BELOW --engine and
	$XENOLITH_ENGINE in resolveEngineRoot and above the installer's own clone, so a stored
	preference never silently redirects a CI job that exported the variable, while a developer who
	works from a live checkout stops passing --engine to every command.

	`toolchainsPath` is the store hosts and targets are searched in — and installed into, because
	"where the targets are" cannot be two different places without the installer listing one set and
	the build finding another. Empty means <data>/toolchains, which is what an installation that
	never configured one has always used.

	Both reach the core through Layout (applyTo below), never as a global. */
	String enginePath;
	String toolchainsPath;

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

	/* Push the two local paths into `layout`, which is how they reach the core at all — every
	install, link, list and build path derives its directories from a Layout.

	Call it right after Layout::resolve*, on the layout the settings were loaded through. An empty
	field leaves the layout's own default in place, so calling this on a fresh install changes
	nothing. */
	void applyTo(Layout &) const;

	// The settings.json keys, in the order a front end should present them.
	static SpanView<SettingsField> getFields();
	static const SettingsField *getField(StringView key); // nullptr when the key is unknown

	// Read/write one field by key. `set` returns false for an unknown key and changes nothing; a
	// string field written with an empty value goes back to its default.
	Value getFieldValue(StringView key) const;
	bool setFieldValue(StringView key, const Value &);

	// The per-tool override when there is one, else autoUpdateReleases.
	bool getToolAutoUpdate(Kind, StringView id) const;
	void setToolAutoUpdate(Kind, StringView id, bool);
};

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_CORE_SRC_SPISETTINGS_H_
