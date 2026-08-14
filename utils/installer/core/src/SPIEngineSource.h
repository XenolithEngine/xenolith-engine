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

#ifndef UTILS_INSTALLER_CORE_SRC_SPIENGINESOURCE_H_
#define UTILS_INSTALLER_CORE_SRC_SPIENGINESOURCE_H_

#include "SPICommon.h"
#include "SPIDirs.h"
#include "SPISettings.h" // SourceConfig
#include "SPGitRemote.h" // git::Remote, CloneOptions, CloneProgress

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// The engine source is fetched over git (Smart HTTP, protocol v2 via stappler_git) — NOT a baked
// bundle, and never via FTP (FTP is only for host/target toolchains). Default ref is the moving
// `master` snapshot; the `stage` branch and tags are selectable. Each ref is cloned into its own
// dir under engines/ so several versions coexist and are trivial to switch.

// The built-in repository, used when the user has configured no other. Prefer
// SourceConfig::getEngineRepoUrl() (SPISettings.h), which falls back to this — reaching for the
// default directly bypasses whatever the user chose.
inline StringView getDefaultEngineRepoUrl() {
	return "https://github.com/XenolithEngine/xenolith-engine.git";
}

inline StringView getEngineDefaultRef() { return "master"; }

struct SP_PUBLIC EngineRef {
	String name; // short branch/tag name, e.g. "master", "stage", "v0.1.0"
	String oidHex;
	bool isBranch = false;
	bool isTag = false;
};

// Discover the engine repo's refs via git ls-refs (synchronous: no looper). Returns branches and
// tags; `result` (if set) receives the failure reason. Doubles as a reachability probe for a
// configured repository URL: a `git ls-refs` that answers is a repository that exists and is
// readable, which is exactly what the settings form needs to know.
SP_PUBLIC Vector<EngineRef> listEngineRefs(const SourceConfig &sources,
		OperationResult *result = nullptr);

inline Vector<EngineRef> listEngineRefs(OperationResult *result = nullptr) {
	return listEngineRefs(SourceConfig(), result);
}

using EngineProgressCallback = Function<void(const git::CloneProgress &)>;

struct SP_PUBLIC EngineCloneResult : OperationResult {
	String ref; // the ref that was cloned
	String commitHex; // resolved commit oid
	size_t filesWritten = 0;
	size_t bytesWritten = 0;
	size_t submodulesCloned = 0;
};

// Clone the engine `ref` (branch/tag — defaults to master when empty) into
// `layout.getEngineDir(ref)` as a shallow (depth 1) working tree, WITH submodules (the engine needs
// the musl submodule). That is <data>/engines/<ref>, so multiple refs coexist and are switchable.
SP_PUBLIC EngineCloneResult cloneEngine(const SourceConfig &sources, StringView ref,
		const Layout &layout, EngineProgressCallback progress);

inline EngineCloneResult cloneEngine(StringView ref, const Layout &layout,
		EngineProgressCallback progress) {
	return cloneEngine(SourceConfig(), ref, layout, sp::move(progress));
}

// Resolve the engine root (STAPPLER_ROOT) for building: explicit `engineOverride` (--engine) >
// $XENOLITH_ENGINE > the cloned default ref dir. `*ok` is false when nothing resolves.
SP_PUBLIC String resolveEngineRoot(const Layout &layout, StringView engineOverride, bool *ok);

// Validate a usable engine tree: `make/universal.mk` present AND no whitespace in the path (the
// build breaks on spaces in STAPPLER_ROOT). Returns "" on success, else a human-readable reason.
SP_PUBLIC String validateEngineRoot(StringView path);

// True when `root` is outside the installer's `data/engines/` tree (a live checkout pointed at via
// --engine / $XENOLITH_ENGINE).
SP_PUBLIC bool isExternalEngine(const Layout &layout, StringView root);

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_CORE_SRC_SPIENGINESOURCE_H_
