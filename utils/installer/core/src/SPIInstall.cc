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

#include "SPIInstall.h"
#include "SPICatalogue.h"
#include "SPITransport.h"
#include "SPIState.h"
#include "SPIProcess.h"

#include "SPCoreCrypto.h"

// Symlinks and the atomic staging swap have no equivalent in stappler::filesystem: `stat` follows
// links (so a dangling one cannot be detected), and `move` degrades to copy+remove across devices,
// which is exactly the atomicity this code depends on. The runtime ships a POSIX libc everywhere,
// so these calls stay portable.
#include <unistd.h>
#include <stdio.h> // rename
#include <sys/stat.h> // lstat

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

namespace {

// The POSIX calls below need a NUL-terminated path; a StringView does not guarantee one.
inline String toPath(StringView path) { return toString(path); }

bool isLink(StringView path) {
	struct stat st;
	return ::lstat(toPath(path).data(), &st) == 0 && S_ISLNK(st.st_mode);
}

// Relative path from `fromDir` to `to` (both absolute). A relative symlink target survives the
// whole data root moving (an absolute one would dangle). Falls back to `to` when there is no
// common prefix.
String makeRelativeLink(StringView fromDir, StringView to) {
	auto split = [](StringView p) {
		Vector<StringView> out;
		p.split<StringView::Chars<'/'>>([&](StringView c) { out.emplace_back(c); });
		return out;
	};

	auto a = split(fromDir);
	auto b = split(to);
	size_t common = 0;
	while (common < a.size() && common < b.size() && a[common] == b[common]) { ++common; }
	if (common == 0) {
		return toString(to);
	}

	mem_std::StringStream rel;
	for (size_t k = common; k < a.size(); ++k) { rel << (rel.empty() ? "" : "/") << ".."; }
	for (size_t k = common; k < b.size(); ++k) { rel << (rel.empty() ? "" : "/") << b[k]; }
	return rel.empty() ? toString(".") : rel.str();
}

bool clearLink(StringView path) {
	if (isLink(path) || (filesystem::exists(FileInfo(path)) && !isDirectory(path))) {
		return ::unlink(toPath(path).data()) == 0;
	}
	if (isDirectory(path)) {
		return filesystem::remove(FileInfo(path), true);
	}
	return true; // nothing there
}

bool linkDir(StringView src, StringView dst) {
	// The symlink target resolves relative to the link's PARENT directory, not the link itself.
	auto rel = makeRelativeLink(filepath::root(dst), src);
	return ::symlink(rel.data(), toPath(dst).data()) == 0;
}

Vector<String> listChildDirs(StringView dir) {
	Vector<String> out;
	filesystem::ftw(FileInfo(dir), [&](const FileInfo &info, FileType type) {
		// ftw reports the walked directory itself first; only real subdirectories are wanted.
		if (type == FileType::Dir && info.path != dir) {
			out.emplace_back(filepath::lastComponent(info.path).str<mem_std::Interface>());
		}
		return true;
	}, 1);
	return out;
}

// If `dir`'s only entry is exactly one subdir named after the component id or its base triple,
// return its path — the SDK archives wrap files in a single `<triple>/` dir that is promoted to
// avoid a doubly-nested `…/<id>/<id>`.
String getWrapperDir(StringView dir, StringView id, StringView triple) {
	auto children = listChildDirs(dir);
	if (children.size() == 1 && (children[0] == id || children[0] == triple)) {
		return mergePath(dir, children[0]);
	}
	return String();
}

// The FTP listing and the archive download are both flaky enough to be worth a few attempts.
constexpr int kTransportAttempts = 4;

TransportResult fetchTextRetry(StringView url, String &out) {
	TransportResult r;
	for (int i = 0; i < kTransportAttempts; ++i) {
		out.clear();
		r = fetchText(url, out);
		if (r && !out.empty()) {
			return r;
		}
	}
	return r;
}

TransportResult fetchBytesRetry(StringView url, Bytes &out,
		const Function<void(int64_t, int64_t)> &progress) {
	TransportResult r;
	for (int i = 0; i < kTransportAttempts; ++i) {
		out.clear();
		r = fetchBytes(url, out, progress);
		if (r && !out.empty()) {
			return r;
		}
	}
	return r;
}

} // namespace

String getComponentDir(const Layout &layout, Kind kind, StringView id) {
	return layout.getToolchainDir(kind, id);
}

InstallResult installComponent(StringView id, const Layout &layout, bool wantHost, bool wantTarget,
		const Function<void(int64_t, int64_t)> &progress) {
	InstallResult result;
	const auto base = getFtpReleaseBase();

	// 1. Fetch the catalogue. These are URLs, not paths: the trailing slash is what makes the FTP
	// server list a directory, so they are built by concatenation and never through filepath.
	String hostsText, targetsText;
	auto r1 = fetchTextRetry(toString(base, "/hosts/"), hostsText);
	if (!r1) {
		result.setError(r1.status, "hosts: ", r1.error);
		return result;
	}
	auto r2 = fetchTextRetry(toString(base, "/targets/"), targetsText);
	if (!r2) {
		result.setError(r2.status, "targets: ", r2.error);
		return result;
	}

	auto comps = buildCatalogue(hostsText, targetsText);
	bool hasHost = false;
	bool hasTarget = false;
	for (const auto &c : comps) {
		if (c.id == id) {
			if (c.kind == Kind::Host) {
				hasHost = true;
			} else {
				hasTarget = true;
			}
		}
	}

	Vector<Kind> kinds;
	if (wantHost) {
		if (!hasHost) {
			result.setError(Status::ErrorNotFound, "no host toolchain '", id, "' in catalogue");
			return result;
		}
		kinds.emplace_back(Kind::Host);
	} else if (wantTarget) {
		if (!hasTarget) {
			result.setError(Status::ErrorNotFound, "no target '", id, "' in catalogue");
			return result;
		}
		kinds.emplace_back(Kind::Target);
	} else {
		if (hasHost) {
			kinds.emplace_back(Kind::Host);
		}
		if (hasTarget) {
			kinds.emplace_back(Kind::Target);
		}
		if (kinds.empty()) {
			result.setError(Status::ErrorNotFound, "unknown component: ", id);
			return result;
		}
	}

	filesystem::mkdir_recursive(FileInfo(StringView(layout.getDownloadDir())));
	filesystem::mkdir_recursive(FileInfo(StringView(layout.getHostsDir())));
	filesystem::mkdir_recursive(FileInfo(StringView(layout.getTargetsDir())));

	auto state = InstalledState::load(layout.getInstalledManifest());

	for (auto kind : kinds) {
		const CatalogueComponent *comp = nullptr;
		for (const auto &c : comps) {
			if (c.id == id && c.kind == kind) {
				comp = &c;
				break;
			}
		}
		if (!comp) {
			continue;
		}

		InstallOutcome out;
		out.id = comp->id;
		out.kind = kind;

		auto url = toString(base, "/", getKindDirName(kind), "/", comp->id, ".tar.xz");

		Bytes archive;
		auto rf = fetchBytesRetry(url, archive, progress);
		if (!rf) {
			out.status = rf.status;
			result.installed.emplace_back(sp::move(out));
			result.setError(rf.status, "download ", comp->id, ": ", rf.error);
			return result;
		}

		// Download size check — guards against a truncated transfer.
		if (archive.size() != comp->size) {
			out.status = Status::ErrorUnknown;
			result.installed.emplace_back(sp::move(out));
			result.setError(Status::ErrorUnknown, "size mismatch for ", comp->id, ": expected ",
					comp->size, ", got ", archive.size());
			return result;
		}
		out.bytes = archive.size();
		out.sha256 = base16::encode<mem_std::Interface>(
				string::Sha256().update(archive.data(), archive.size()).final());

		// Stage the archive on disk so `tar` can read it.
		auto tmpFile = mergePath(layout.getDownloadDir(), toString(comp->id, ".tar.xz"));
		if (!filesystem::write(FileInfo(StringView(tmpFile)),
					BytesView(archive.data(), archive.size()))) {
			out.status = Status::ErrorUnknown;
			result.installed.emplace_back(sp::move(out));
			result.setError(Status::ErrorUnknown, "failed to write ", tmpFile);
			return result;
		}

		auto finalDir = getComponentDir(layout, kind, comp->id);
		auto staging =
				mergePath(filepath::root(StringView(finalDir)), toString(".staging-", comp->id));
		if (isDirectory(staging)) {
			filesystem::remove(FileInfo(StringView(staging)), true);
		}
		filesystem::mkdir_recursive(FileInfo(StringView(staging)));

		// Extract into staging.
		StringView tarArgs[] = {StringView("tar"), StringView("-xf"), StringView(tmpFile),
			StringView("-C"), StringView(staging)};
		auto tar = runCommand(tarArgs);
		if (!tar) {
			out.status = tar.status;
			result.installed.emplace_back(sp::move(out));
			result.setError(tar.status, "tar extraction failed: ", tar.error);
			filesystem::remove(FileInfo(StringView(staging)), true);
			return result;
		}

		// Promote a single top-level <id>/<triple> wrapper, then atomically swap into place.
		auto placed =
				getWrapperDir(StringView(staging), StringView(comp->id), StringView(comp->triple));
		if (placed.empty()) {
			placed = staging;
		}
		if (isDirectory(finalDir)) {
			filesystem::remove(FileInfo(StringView(finalDir)), true);
		}
		if (::rename(placed.data(), finalDir.data()) != 0) {
			out.status = Status::ErrorUnknown;
			result.installed.emplace_back(sp::move(out));
			result.setError(Status::ErrorUnknown, "failed to place ", finalDir);
			return result;
		}
		if (placed != staging && isDirectory(staging)) {
			filesystem::remove(FileInfo(StringView(staging)), true);
		}

		InstalledComponent ic;
		ic.id = comp->id;
		ic.triple = comp->triple;
		ic.variant = comp->variant;
		ic.kind = kind;
		ic.release = toString(getDefaultRelease());
		ic.sha256 = out.sha256;
		ic.installedAt = Time::now().toIso8601<String>(0);
		ic.path = finalDir;
		state.upsert(sp::move(ic));

		out.destPath = sp::move(finalDir);
		result.installed.emplace_back(sp::move(out));
	}

	if (!state.save(layout.getInstalledManifest())) {
		result.setError(Status::ErrorUnknown, "failed to save ", layout.getInstalledManifest());
	}
	return result;
}

static bool hasOwnToolchains(StringView engineRoot) {
	auto engineTc = mergePath(engineRoot, "toolchains");
	for (auto kind : {Kind::Host, Kind::Target}) {
		auto dir = mergePath(engineTc, getKindDirName(kind));
		if (!isDirectory(dir)) {
			continue;
		}
		// listChildDirs walks with stat(), which follows symlinks, so a link to the store also
		// reports as a directory — isLink is what separates the two.
		for (const auto &name : listChildDirs(StringView(dir))) {
			if (!isLink(mergePath(dir, name))) {
				return true;
			}
		}
	}
	return false;
}

bool linkToolchainsIntoEnginePath(const Layout &layout, StringView engineRoot) {
	// An engine root holding a REAL toolchain directory is a developer's working tree that builds
	// its own toolchains: refuse outright rather than replace them. Linking is destructive by
	// design (clearLink removes what is in the way), and a locally built sysroot is not
	// reproducible from the store — so a "heal the links" call must never touch one. Only links,
	// which this function itself created, are replaced without asking.
	if (hasOwnToolchains(engineRoot)) {
		return false;
	}

	auto engineTc = mergePath(engineRoot, "toolchains");
	bool ok = true;
	for (auto kind : {Kind::Host, Kind::Target}) {
		auto storeKind = mergePath(layout.getToolchainsDir(), getKindDirName(kind));
		if (!isDirectory(storeKind)) {
			continue;
		}

		auto linkKind = mergePath(engineTc, getKindDirName(kind));
		filesystem::mkdir_recursive(FileInfo(StringView(linkKind)));

		for (const auto &name : listChildDirs(StringView(storeKind))) {
			auto link = mergePath(linkKind, name);
			if (!clearLink(link)) {
				ok = false;
				continue;
			}
			if (!linkDir(mergePath(storeKind, name), link)) {
				ok = false;
			}
		}
	}
	return ok;
}

bool linkToolchainsIntoEngine(const Layout &layout, StringView engineRef) {
	return linkToolchainsIntoEnginePath(layout, layout.getEngineDir(engineRef));
}

bool relinkAllEngines(const Layout &layout) {
	auto engines = layout.getEnginesDir();
	if (!isDirectory(engines)) {
		return true;
	}
	bool ok = true;
	for (const auto &name : listChildDirs(StringView(engines))) {
		if (!linkToolchainsIntoEngine(layout, name)) {
			ok = false;
		}
	}
	return ok;
}

bool removeComponent(const Layout &layout, Kind kind, StringView id) {
	auto dir = getComponentDir(layout, kind, id);
	if (isDirectory(dir)) {
		return filesystem::remove(FileInfo(StringView(dir)), true);
	}
	return true;
}

} // namespace stappler::xenolith::installer
