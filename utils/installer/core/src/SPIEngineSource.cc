#include "SPIEngineSource.h"
#include "SPGitRemote.h" // git::Remote, RefListResult, RefInfo
#include "SPFilesystem.h"
#include "SPFilepath.h"

#include <sys/stat.h>
#include <cstdlib>

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

namespace {
// strip the "refs/heads/" (11) or "refs/tags/" (10) prefix
StringView shortRefName(const String &name, bool branch) {
	auto off = branch ? size_t(11) : size_t(10);
	if (name.size() > off) {
		return StringView(name.data() + off, name.size() - off);
	}
	return StringView(name);
}
} // namespace

Vector<EngineRef> list_engine_refs(String *status) {
	Vector<EngineRef> out;

	auto remote = Rc<git::Remote>::create(engineRepoUrl());
	if (!remote) {
		if (status) { *status = toString("failed to allocate git remote"); }
		return out;
	}

	git::RefListResult res;
	remote->listRefs([&res](git::RefListResult &&r) { res = sp::move(r); });

	if (res.status != Status::Ok) {
		if (status) {
			char buf[96];
			snprintf(buf, sizeof(buf), "git ls-refs failed (status %d, http %ld)",
					static_cast<int>(res.status), res.httpCode);
			*status = toString(buf);
		}
		return out;
	}

	for (auto &rf : res.refs) {
		if (rf.isBranch() || rf.isTag()) {
			EngineRef e;
			e.name = toString(shortRefName(rf.name, rf.isBranch()));
			e.oidHex = toString(rf.oid.str());
			e.isBranch = rf.isBranch();
			e.isTag = rf.isTag();
			out.push_back(sp::move(e));
		}
	}
	return out;
}

EngineCloneResult clone_engine(StringView ref, const Layout &layout, EngineProgressCallback progress) {
	EngineCloneResult result;
	auto refStr = ref.empty() ? toString(engineDefaultRef()) : toString(ref);
	result.ref = refStr;

	// 1. resolve the ref's oid via ls-refs (a Remote does one operation — use a fresh one for clone)
	auto refsRemote = Rc<git::Remote>::create(engineRepoUrl());
	if (!refsRemote) {
		result.status = Status::ErrorNotPermitted;
		return result;
	}
	git::RefListResult refs;
	refsRemote->listRefs([&refs](git::RefListResult &&r) { refs = sp::move(r); });
	if (refs.status != Status::Ok) {
		result.status = refs.status;
		return result;
	}

	git::Oid oid;
	bool found = false;
	for (auto &rf : refs.refs) {
		if ((rf.isBranch() || rf.isTag()) && shortRefName(rf.name, rf.isBranch()) == refStr) {
			oid = rf.oid;
			found = true;
			break;
		}
	}
	if (!found || oid.empty()) {
		result.status = Status::ErrorNotFound;
		return result;
	}

	// 2. ensure the engines/ parent exists (idempotent), then clone into engines/<ref>/
	::mkdir(layout.engines_dir().c_str(), 0755);

	// A re-install must replace the previous clone, not collide with it — clear any existing dir.
	auto targetDir = layout.engine_dir(refStr);
	if (filesystem::exists(FileInfo(StringView(targetDir)))) {
		filesystem::remove(FileInfo(StringView(targetDir)), true);
	}

	auto cloneRemote = Rc<git::Remote>::create(engineRepoUrl());
	if (!cloneRemote) {
		result.status = Status::ErrorNotPermitted;
		return result;
	}

	git::CloneOptions opts;
	opts.oid = oid;
	opts.ref = refStr;
	opts.targetDir = layout.engine_dir(refStr);
	opts.depth = 1; // shallow — no history
	opts.recurseSubmodules = true; // the engine needs the musl submodule
	opts.progress = sp::move(progress);

	git::CloneResult cr;
	cloneRemote->clone(sp::move(opts), [&cr](git::CloneResult &&r) { cr = sp::move(r); });

	result.status = cr.status;
	result.commitHex = toString(cr.commit.str());
	result.filesWritten = cr.filesWritten;
	result.bytesWritten = cr.bytesWritten;
	result.submodulesCloned = cr.submodulesCloned;

	// A shallow clone carries no `.git`, so the build system's `sp_detect_build_number` (which
	// falls back to `git rev-list --count HEAD` then `.build_number`) would resolve the module
	// VERSION_BUILD variables to empty — producing an ill-formed buildconfig
	// (`constexpr int XENOLITH_VERSION_BUILD = XENOLITH_VERSION_BUILD;`). Drop a `.build_number`
	// next to the modules so the build resolves it to a concrete value.
	if (result.status == Status::Ok) {
		filesystem::write(FileInfo(StringView(layout.engine_dir(refStr) + "/.build_number")),
				StringView("0"));
	}
	return result;
}

String resolve_engine_root(const Layout &layout, const String *override, bool *ok) {
	if (ok) {
		*ok = true;
	}
	if (override && !override->empty()) {
		*ok = validate_engine_root(*override).empty();
		return *override; // explicit --engine path wins (validated)
	}
	if (const char *e = std::getenv("XENOLITH_ENGINE"); e && *e) {
		String p = toString(e);
		*ok = validate_engine_root(p).empty();
		return p;
	}
	// fall back to the cloned default ref
	auto dir = layout.engine_dir(engineDefaultRef());
	*ok = validate_engine_root(dir).empty();
	return dir;
}

String validate_engine_root(StringView path) {
	bool has_space = false;
	for (auto c : path) {
		if (c == ' ' || c == '\t') {
			has_space = true;
			break;
		}
	}
	if (has_space) {
		return toString("engine path must not contain spaces (GNU make breaks on them)");
	}
	String mk = toString(path) + "/make/universal.mk";
	struct stat st;
	if (::stat(mk.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
		return toString("not a valid engine root (missing make/universal.mk): ") + toString(path);
	}
	return String();
}

bool is_external_engine(const Layout &layout, StringView root) {
	auto engines = layout.engines_dir();
	// Lexical "starts_with" compare (the Rust version tries canonicalize first, falling back to
	// lexical; we keep it simple and lexical — good enough for the default checkout layout).
	return !root.starts_with(StringView(engines));
}

} // namespace stappler::xenolith::installer
