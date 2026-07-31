#ifndef INSTALLER_CORE_SPIINSTALL_H_
#define INSTALLER_CORE_SPIINSTALL_H_
#include "SPICommon.h"
#include "SPIDirs.h"
#include "SPIManifest.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

struct InstallOutcome {
	Status status = Status::Ok;
	String id;
	Kind kind = Kind::Target;
	String destPath; // final on-disk location (the shared store)
	uint64_t bytes = 0;
	String sha256;
};

struct InstallResult {
	Status status = Status::Ok;
	String error;
	Vector<InstallOutcome> installed; // one per installed kind (a triple can be host AND target)
};

// On-disk location of an installed toolchain in the shared, engine-independent store:
// `<data>/toolchains/<hosts|targets>/<id>`. Engines symlink to these.
String component_dir(const Layout &layout, Kind kind, StringView id);

// Install every catalogue component whose id == `id`. `wantHost`/`wantTarget` narrow to one kind;
// with both false, every kind that exists under the id is installed (a triple that is both a host
// toolchain and a target gets both). For each: download `<base>/<kind_dir>/<id>.tar.xz` (with a
// size check), extract into a sibling `.staging-<id>` dir, promote the archive's single top-level
// `<id>`/`<triple>` wrapper if present, atomically swap into `component_dir`, record sha256, and
// upsert into installed.json. Signature verification is deferred (no public key distributed yet);
// the catalogue's signature rule (drop unsigned) remains the security gate.
InstallResult install_component(StringView id, const Layout &layout, bool wantHost,
		bool wantTarget, const Function<void(int64_t, int64_t)> &progress = {});

// Symlink every toolchain from the shared store into an engine root's `toolchains/` dir, so that
// engine's build (STAPPLER_ROOT = engine root) can find them. Targets are RELATIVE so they survive
// the data root moving. Works for installed bundles and an external checkout.
bool link_toolchains_into_engine_path(const Layout &layout, StringView engineRoot);
// Refresh toolchain links in one installed engine (`<data>/engines/<ref>`).
bool link_toolchains_into_engine(const Layout &layout, StringView engineRef);
// Refresh toolchain links in every installed engine — call after a toolchain is added/removed.
bool relink_all_engines(const Layout &layout);

// Remove an installed component's files. Idempotent — a missing directory is not an error.
bool uninstall(const Layout &layout, Kind kind, StringView id);

} // namespace stappler::xenolith::installer

#endif // INSTALLER_CORE_SPIINSTALL_H_
