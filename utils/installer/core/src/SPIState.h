#ifndef INSTALLER_CORE_SPISTATE_H_
#define INSTALLER_CORE_SPISTATE_H_
#include "SPICommon.h"
#include "SPIManifest.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// The installed-state registry (installed.json) — records what is actually laid down on disk so
// the app can validate the install and compute updates against the remote manifest. Lives in the
// config dir (Layout::installed_manifest). Port of the Rust `state` module.

static constexpr uint32_t kStateSchemaVersion = 1;

struct InstalledComponent {
	String id;          // full id incl. any variant, e.g. "aarch64-apple-macosx+sprt"
	String triple;      // triple without the +variant suffix
	String variant;     // variant after "+", empty if none
	Kind kind;          // Host or Target
	String release;     // release this component came from
	String sha256;      // archive hash (for verify/repair), empty if none
	String installedAt; // RFC 3339 timestamp (caller supplies the clock)
	String path;        // directory the archive was unpacked into
};

struct InstalledState {
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
	Vector<const InstalledComponent *> invalid(const Function<bool(StringView)> &exists) const;
};

} // namespace stappler::xenolith::installer

#endif // INSTALLER_CORE_SPISTATE_H_
