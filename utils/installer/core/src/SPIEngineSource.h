#ifndef INSTALLER_CORE_SPIENGINESOURCE_H_
#define INSTALLER_CORE_SPIENGINESOURCE_H_
#include "SPICommon.h"
#include "SPIDirs.h"
#include "SPGitRemote.h" // git::Remote, CloneOptions, CloneProgress

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// The engine source is fetched over git (Smart HTTP, protocol v2 via stappler/git) — NOT a baked
// bundle, and never via FTP (FTP is only for host/target toolchains). Default ref is the moving
// `master` snapshot; the `stage` branch and tags are selectable. Each ref is cloned into its own
// dir under engines/ so several versions coexist and are trivial to switch.

inline StringView engineRepoUrl() { return "https://github.com/XenolithEngine/xenolith-engine.git"; }
inline StringView engineDefaultRef() { return "master"; }

struct EngineRef {
	String name; // short branch/tag name, e.g. "master", "stage", "v0.1.0"
	String oidHex;
	bool isBranch = false;
	bool isTag = false;
};

// Discover the engine repo's refs via git ls-refs (synchronous: no looper). Returns branches and
// tags. `status` (if set) receives "" on success or an error message.
Vector<EngineRef> list_engine_refs(String *status);

using EngineProgressCallback = Function<void(const git::CloneProgress &)>;

struct EngineCloneResult {
	Status status = Status::Ok;
	String ref;            // the ref that was cloned
	String commitHex;      // resolved commit oid
	size_t filesWritten = 0;
	size_t bytesWritten = 0;
	size_t submodulesCloned = 0;
};

// Clone the engine `ref` (branch/tag — defaults to master when empty) into layout.engine_dir(ref)
// as a shallow (depth 1) working tree, WITH submodules (the engine needs the musl submodule).
// `layout.engine_dir(ref)` is <data>/engines/<ref>, so multiple refs coexist and are switchable.
EngineCloneResult clone_engine(StringView ref, const Layout &layout, EngineProgressCallback progress);

// Resolve the engine root (STAPPLER_ROOT) for building: explicit `override` (--engine) >
// $XENOLITH_ENGINE > the cloned default ref dir. `*ok` is false when nothing resolves.
// (Mirrors the Rust provision::resolve_engine_root for full CLI parity — the GUI/build reuse it.)
String resolve_engine_root(const Layout &layout, const String *override, bool *ok);

// Validate a usable engine tree: `make/universal.mk` present AND no whitespace in the path (GNU
// make breaks on spaces in STAPPLER_ROOT). Returns "" on success, else a human-readable reason.
String validate_engine_root(StringView path);

// True when `root` is outside the installer's `data/engines/` tree (a live checkout pointed at via
// --engine / $XENOLITH_ENGINE).
bool is_external_engine(const Layout &layout, StringView root);

} // namespace stappler::xenolith::installer

#endif // INSTALLER_CORE_SPIENGINESOURCE_H_
