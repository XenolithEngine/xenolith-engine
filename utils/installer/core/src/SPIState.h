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

#ifndef UTILS_INSTALLER_CORE_SRC_SPISTATE_H_
#define UTILS_INSTALLER_CORE_SRC_SPISTATE_H_

#include "SPICommon.h"
#include "SPIManifest.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// The installed-state registry (installed.json) — records what is actually laid down on disk so
// the app can validate the install and compute updates against the remote manifest. Lives in the
// config dir (Layout::getInstalledManifest).

static constexpr uint32_t kStateSchemaVersion = 1;

struct SP_PUBLIC InstalledComponent {
	String id; // full id incl. any variant, e.g. "aarch64-apple-macosx+sprt"
	String triple; // triple without the +variant suffix
	String variant; // variant after "+", empty if none
	Kind kind = Kind::Target;
	String release; // release this component came from
	String sha256; // archive hash (for verify/repair), empty if none
	String installedAt; // RFC 3339 timestamp
	String path; // directory the archive was unpacked into
};

struct SP_PUBLIC InstalledState {
	uint32_t schema = kStateSchemaVersion;
	Vector<InstalledComponent> components;

	// Load the registry; an empty state if the file does not exist.
	static InstalledState load(StringView path);

	// Persist, creating parent directories as needed.
	bool save(StringView path) const;

	// Insert or replace by (id, kind) — a triple can be both a host and a target, so id alone
	// is not a unique key.
	void upsert(InstalledComponent c);

	// Remove by (id, kind); returns whether anything was removed.
	bool remove(StringView id, Kind kind);

	// Look up by (id, kind).
	const InstalledComponent *get(StringView id, Kind kind) const;

	// Components whose on-disk path fails `exists` — the registry claims them installed but
	// they are missing/corrupt. The existence check is injected so this stays pure.
	Vector<const InstalledComponent *> getInvalid(const Function<bool(StringView)> &exists) const;
};

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_CORE_SRC_SPISTATE_H_
