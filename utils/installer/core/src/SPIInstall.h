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

#ifndef UTILS_INSTALLER_CORE_SRC_SPIINSTALL_H_
#define UTILS_INSTALLER_CORE_SRC_SPIINSTALL_H_

#include "SPICommon.h"
#include "SPIDirs.h"
#include "SPIManifest.h"
#include "SPISettings.h" // SourceConfig

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

struct SP_PUBLIC InstallOutcome {
	Status status = Status::Ok;
	String id;
	Kind kind = Kind::Target;
	String destPath; // final on-disk location (the shared store)
	uint64_t bytes = 0;
	String sha256;
};

struct SP_PUBLIC InstallResult : OperationResult {
	Vector<InstallOutcome> installed; // one per installed kind (a triple can be host AND target)
};

// On-disk location of an installed toolchain in the shared, engine-independent store:
// `<data>/toolchains/<hosts|targets>/<id>`. Engines symlink to these.
SP_PUBLIC String getComponentDir(const Layout &layout, Kind kind, StringView id);

// Install every catalogue component whose id == `id`. `wantHost`/`wantTarget` narrow to one kind;
// with both false, every kind that exists under the id is installed (a triple that is both a host
// toolchain and a target gets both). For each: download `<base>/<kind>/<id>.tar.xz` (with a size
// check), extract into a sibling `.staging-<id>` dir, promote the archive's single top-level
// `<id>`/`<triple>` wrapper if present, atomically swap into `getComponentDir`, record sha256, and
// upsert into installed.json. Signature verification is deferred (no public key distributed yet);
// the catalogue's signature rule (drop unsigned) remains the security gate.
//
// `sources` says which mirror to install from and `release` which release directory under it; an
// empty `release` means getDefaultRelease(), NOT the newest one — resolving the active release is
// a separate network round trip (resolveActiveRelease) and this function does not make it, so a
// caller that has already resolved one must pass it.
SP_PUBLIC InstallResult installComponent(const SourceConfig &sources, StringView release,
		StringView id, const Layout &layout, bool wantHost, bool wantTarget,
		const Function<void(int64_t, int64_t)> &progress = {});

inline InstallResult installComponent(StringView id, const Layout &layout, bool wantHost,
		bool wantTarget, const Function<void(int64_t, int64_t)> &progress = {}) {
	return installComponent(SourceConfig(), StringView(), id, layout, wantHost, wantTarget,
			progress);
}

// Symlink every toolchain from the shared store into an engine root's `toolchains/` dir, so that
// engine's build (STAPPLER_ROOT = engine root) can find them. Targets are RELATIVE so they survive
// the data root moving. Works for installed bundles and an external checkout.
//
// REFUSES (returns false, changes nothing) when the engine root already holds a REAL toolchain
// directory rather than a link: that is an engine developer's tree building its own toolchains, and
// linking would delete them. Existing links — the ones this function made — are replaced silently.
SP_PUBLIC bool linkToolchainsIntoEnginePath(const Layout &layout, StringView engineRoot);

// Refresh toolchain links in one installed engine (`<data>/engines/<ref>`).
SP_PUBLIC bool linkToolchainsIntoEngine(const Layout &layout, StringView engineRef);

// Refresh toolchain links in every installed engine — call after a toolchain is added/removed.
SP_PUBLIC bool relinkAllEngines(const Layout &layout);

// Remove an installed component's files. Idempotent — a missing directory is not an error.
SP_PUBLIC bool removeComponent(const Layout &layout, Kind kind, StringView id);

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_CORE_SRC_SPIINSTALL_H_
