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

#include "SPIEngineSource.h"
#include "SPGitRemote.h" // git::Remote, RefListResult, RefInfo

#include <stdlib.h> // getenv: there is no runtime wrapper for the environment

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

namespace {

// strip the "refs/heads/" / "refs/tags/" prefix
StringView getShortRefName(StringView name, bool branch) {
	StringView prefix = branch ? StringView("refs/heads/") : StringView("refs/tags/");
	if (name.starts_with(prefix)) {
		return StringView(name.data() + prefix.size(), name.size() - prefix.size());
	}
	return name;
}

} // namespace

Vector<EngineRef> listEngineRefs(const SourceConfig &sources, OperationResult *result) {
	Vector<EngineRef> out;

	auto remote = Rc<git::Remote>::create(sources.getEngineRepoUrl());
	if (!remote) {
		if (result) {
			result->setError(Status::ErrorNotPermitted, "failed to allocate git remote");
		}
		return out;
	}

	git::RefListResult res;
	remote->listRefs([&res](git::RefListResult &&r) { res = sp::move(r); });

	if (!isSuccessful(res.status)) {
		if (result) {
			result->setError(res.status, "git ls-refs failed (status ", int(res.status), ", http ",
					res.httpCode, ")");
		}
		return out;
	}

	for (auto &rf : res.refs) {
		if (rf.isBranch() || rf.isTag()) {
			EngineRef e;
			e.name = getShortRefName(rf.name, rf.isBranch()).str<mem_std::Interface>();
			e.oidHex = toString(rf.oid.str());
			e.isBranch = rf.isBranch();
			e.isTag = rf.isTag();
			out.emplace_back(sp::move(e));
		}
	}
	return out;
}

EngineCloneResult cloneEngine(const SourceConfig &sources, StringView ref, const Layout &layout,
		EngineProgressCallback progress) {
	EngineCloneResult result;
	auto refStr = ref.empty() ? toString(getEngineDefaultRef()) : toString(ref);
	result.ref = refStr;

	const auto repoUrl = sources.getEngineRepoUrl();

	// 1. resolve the ref's oid via ls-refs (a Remote does one operation — use a fresh one for clone)
	auto refsRemote = Rc<git::Remote>::create(repoUrl);
	if (!refsRemote) {
		result.setError(Status::ErrorNotPermitted, "failed to allocate git remote");
		return result;
	}

	git::RefListResult refs;
	refsRemote->listRefs([&refs](git::RefListResult &&r) { refs = sp::move(r); });
	if (!isSuccessful(refs.status)) {
		result.setError(refs.status, "git ls-refs failed (http ", refs.httpCode, ")");
		return result;
	}

	git::Oid oid;
	for (auto &rf : refs.refs) {
		if ((rf.isBranch() || rf.isTag()) && getShortRefName(rf.name, rf.isBranch()) == refStr) {
			oid = rf.oid;
			break;
		}
	}
	if (oid.empty()) {
		result.setError(Status::ErrorNotFound, "no such engine ref: ", refStr);
		return result;
	}

	// 2. ensure the engines/ parent exists (idempotent), then clone into engines/<ref>/
	filesystem::mkdir_recursive(FileInfo(StringView(layout.getEnginesDir())));

	// A re-install must replace the previous clone, not collide with it — clear any existing dir.
	auto targetDir = layout.getEngineDir(refStr);
	if (filesystem::exists(FileInfo(StringView(targetDir)))) {
		filesystem::remove(FileInfo(StringView(targetDir)), true);
	}

	auto cloneRemote = Rc<git::Remote>::create(repoUrl);
	if (!cloneRemote) {
		result.setError(Status::ErrorNotPermitted, "failed to allocate git remote");
		return result;
	}

	git::CloneOptions opts;
	opts.oid = oid;
	opts.ref = refStr;
	opts.targetDir = targetDir;
	opts.depth = 1; // shallow — no history
	opts.recurseSubmodules = true; // the engine needs the musl submodule
	opts.progress = sp::move(progress);

	git::CloneResult cr;
	cloneRemote->clone(sp::move(opts), [&cr](git::CloneResult &&r) { cr = sp::move(r); });

	result.commitHex = toString(cr.commit.str());
	result.filesWritten = cr.filesWritten;
	result.bytesWritten = cr.bytesWritten;
	result.submodulesCloned = cr.submodulesCloned;
	if (!isSuccessful(cr.status)) {
		result.setError(cr.status, "clone of '", refStr, "' failed");
		return result;
	}

	// A shallow clone carries no `.git`, so the build system's `sp_detect_build_number` (which
	// falls back to `git rev-list --count HEAD` then `.build_number`) would resolve the module
	// VERSION_BUILD variables to empty — producing an ill-formed buildconfig
	// (`constexpr int XENOLITH_VERSION_BUILD = XENOLITH_VERSION_BUILD;`). Drop a `.build_number`
	// next to the modules so the build resolves it to a concrete value.
	filesystem::write(FileInfo(StringView(mergePath(targetDir, ".build_number"))), StringView("0"));
	return result;
}

String resolveEngineRoot(const Layout &layout, StringView engineOverride, bool *ok) {
	if (ok) {
		*ok = true;
	}
	if (!engineOverride.empty()) {
		auto p = toString(engineOverride);
		*ok = validateEngineRoot(p).empty();
		return p; // explicit --engine path wins (validated)
	}
	if (const char *e = ::getenv("XENOLITH_ENGINE"); e && *e) {
		auto p = toString(e);
		*ok = validateEngineRoot(p).empty();
		return p;
	}
	// fall back to the cloned default ref
	auto dir = layout.getEngineDir(getEngineDefaultRef());
	*ok = validateEngineRoot(dir).empty();
	return dir;
}

String validateEngineRoot(StringView path) {
	// A space in the path is fine: the build hands STAPPLER_ROOT to make encoded
	// (makefile::encodePathSpaces, see buildProject), and the make engine carries path-internal
	// spaces through to the shell itself.
	if (!isFile(mergePath(path, "make/universal.mk"))) {
		return toString("not a valid engine root (missing make/universal.mk): ", path);
	}
	return String();
}

bool isExternalEngine(const Layout &layout, StringView root) {
	// Lexical prefix compare — good enough for the default checkout layout.
	return !root.starts_with(StringView(layout.getEnginesDir()));
}

} // namespace stappler::xenolith::installer
