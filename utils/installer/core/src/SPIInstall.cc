#include "SPIInstall.h"
#include "SPICatalogue.h"
#include "SPITransport.h"
#include "SPIState.h"
#include "SPIProcess.h"

#include "SPFilesystem.h"
#include "SPFilepath.h"
#include "SPCoreCrypto.h"

#include <ctime>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

namespace {

String to_hex(const uint8_t *p, size_t n) {
	static const char *h = "0123456789abcdef";
	String out;
	out.resize(n * 2);
	for (size_t i = 0; i < n; ++i) {
		out[i * 2] = h[p[i] >> 4];
		out[i * 2 + 1] = h[p[i] & 0xf];
	}
	return out;
}

String rfc3339_now() {
	auto now = std::time(nullptr);
	struct tm tm;
	gmtime_r(&now, &tm);
	char b[32];
	std::strftime(b, sizeof(b), "%Y-%m-%dT%H:%M:%SZ", &tm);
	return toString(b);
}

bool is_dir(StringView path) {
	struct stat st;
	return ::lstat(path.data(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool is_link(StringView path) {
	struct stat st;
	return ::lstat(path.data(), &st) == 0 && S_ISLNK(st.st_mode);
}

// Relative path from `from_dir` to `to` (both absolute). A relative symlink target survives the
// whole data root moving (an absolute one would dangle). Falls back to `to` when there is no
// common prefix. Mirrors the Rust `relative_link`.
String relative_link(StringView fromDir, StringView to) {
	auto split = [](StringView p) {
		Vector<StringView> out;
		size_t i = 0;
		if (!p.empty() && p[0] == '/') {
			++i;
		}
		while (i < p.size()) {
			size_t j = i;
			while (j < p.size() && p[j] != '/') {
				++j;
			}
			if (j > i) {
				out.emplace_back(p.data() + i, j - i);
			}
			i = (j < p.size()) ? j + 1 : j;
		}
		return out;
	};
	auto a = split(fromDir);
	auto b = split(to);
	size_t common = 0;
	while (common < a.size() && common < b.size() && a[common] == b[common]) {
		++common;
	}
	if (common == 0) {
		return toString(to);
	}
	String rel;
	for (size_t k = common; k < a.size(); ++k) {
		if (!rel.empty()) {
			rel += "/";
		}
		rel += "..";
	}
	for (size_t k = common; k < b.size(); ++k) {
		if (!rel.empty()) {
			rel += "/";
		}
		rel.append(b[k].data(), b[k].size());
	}
	if (rel.empty()) {
		rel = ".";
	}
	return rel;
}

bool clear_link(StringView path) {
	if (is_link(path) || (::access(path.data(), F_OK) == 0 && !is_dir(path))) {
		return ::unlink(path.data()) == 0;
	}
	if (is_dir(path)) {
		return filesystem::remove(FileInfo(StringView(path)), true);
	}
	return true; // nothing there
}

bool link_dir(StringView src, StringView dst) {
	// The symlink target resolves relative to the link's PARENT directory, not the link itself.
	auto pos = dst.rfind('/');
	StringView parentDir = (pos < dst.size()) ? StringView(dst.data(), pos) : StringView(".");
	String rel = relative_link(parentDir, src);
	return ::symlink(rel.data(), dst.data()) == 0;
}

Vector<String> list_child_dirs(StringView dir) {
	Vector<String> out;
	auto *d = ::opendir(dir.empty() ? "." : dir.data());
	if (!d) {
		return out;
	}
	struct dirent *ent;
	while ((ent = ::readdir(d))) {
		if (std::strcmp(ent->d_name, ".") == 0 || std::strcmp(ent->d_name, "..") == 0) {
			continue;
		}
		String child = toString(dir) + "/" + toString(ent->d_name);
		if (is_dir(StringView(child))) {
			out.push_back(toString(ent->d_name));
		}
	}
	::closedir(d);
	return out;
}

// If `dir`'s only entry is exactly one subdir whose name matches one of `names` (the component id
// or its base triple), return its path — the SDK archives wrap files in a single `<triple>/` dir
// that we promote to avoid doubly-nested `…/<id>/<id>`.
String wrapper_dir(StringView dir, StringView id, StringView triple) {
	auto children = list_child_dirs(dir);
	if (children.size() == 1) {
		const auto &name = children[0];
		if (name == id || name == triple) {
			return toString(dir) + "/" + name;
		}
	}
	return String();
}

TransportResult fetch_text_retry(StringView url, String &out, int attempts = 4) {
	TransportResult r;
	for (int i = 0; i < attempts; ++i) {
		out.clear();
		r = fetch_text(url, out);
		if (r.status == Status::Ok && !out.empty()) {
			return r;
		}
	}
	return r;
}

TransportResult fetch_bytes_retry(StringView url, Bytes &out,
		const Function<void(int64_t, int64_t)> &progress, int attempts = 4) {
	TransportResult r;
	for (int i = 0; i < attempts; ++i) {
		out.clear();
		r = fetch_bytes(url, out, progress);
		if (r.status == Status::Ok && !out.empty()) {
			return r;
		}
	}
	return r;
}

} // namespace

String component_dir(const Layout &layout, Kind kind, StringView id) {
	return layout.toolchain_dir(kind, id);
}

InstallResult install_component(StringView id, const Layout &layout, bool wantHost, bool wantTarget,
		const Function<void(int64_t, int64_t)> &progress) {
	InstallResult result;
	const auto base = ftp_release_base();

	// 1. Fetch the catalogue (retry the flaky FTP listing).
	String hostsText, targetsText;
	auto r1 = fetch_text_retry(toString(base) + "/hosts/", hostsText);
	if (r1.status != Status::Ok) {
		result.status = r1.status;
		result.error = toString("hosts: ") + r1.error;
		return result;
	}
	auto r2 = fetch_text_retry(toString(base) + "/targets/", targetsText);
	if (r2.status != Status::Ok) {
		result.status = r2.status;
		result.error = toString("targets: ") + r2.error;
		return result;
	}

	auto comps = build_catalogue(hostsText, targetsText);
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
			result.status = Status::ErrorNotFound;
			result.error = toString("no host toolchain '") + toString(id) + toString("' in catalogue");
			return result;
		}
		kinds.push_back(Kind::Host);
	} else if (wantTarget) {
		if (!hasTarget) {
			result.status = Status::ErrorNotFound;
			result.error = toString("no target '") + toString(id) + toString("' in catalogue");
			return result;
		}
		kinds.push_back(Kind::Target);
	} else {
		if (hasHost) {
			kinds.push_back(Kind::Host);
		}
		if (hasTarget) {
			kinds.push_back(Kind::Target);
		}
		if (kinds.empty()) {
			result.status = Status::ErrorNotFound;
			result.error = toString("unknown component: ") + toString(id);
			return result;
		}
	}

	filesystem::mkdir_recursive(FileInfo(StringView(layout.download_tmp())));
	filesystem::mkdir_recursive(FileInfo(StringView(layout.toolchains_hosts_dir())));
	filesystem::mkdir_recursive(FileInfo(StringView(layout.toolchains_targets_dir())));

	auto state = InstalledState::load(layout.installed_manifest());

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

		String url = toString(base) + "/" + toString(kind_dir(kind)) + "/" + comp->id + ".tar.xz";

		Bytes archive;
		auto rf = fetch_bytes_retry(url, archive, progress);
		if (rf.status != Status::Ok) {
			out.status = rf.status;
			result.installed.push_back(sp::move(out));
			result.status = rf.status;
			result.error = toString("download ") + comp->id + toString(": ") + rf.error;
			return result;
		}
		// Download size check — guards against a truncated transfer.
		if (archive.size() != comp->size) {
			out.status = Status::ErrorUnknown;
			result.installed.push_back(sp::move(out));
			result.status = Status::ErrorUnknown;
			char buf[96];
			snprintf(buf, sizeof(buf), "size mismatch for %s: expected %llu, got %zu",
					comp->id.c_str(), (unsigned long long)comp->size, archive.size());
			result.error = toString(buf);
			return result;
		}
		out.bytes = archive.size();

		string::Sha256 ctx;
		ctx.update(archive.data(), archive.size());
		uint8_t digest[32];
		ctx.final(digest);
		out.sha256 = to_hex(digest, 32);

		// Stage the archive on disk so `tar` can read it.
		String tmpFile = layout.download_tmp() + "/" + comp->id + ".tar.xz";
		if (!filesystem::write(FileInfo(StringView(tmpFile)),
					BytesView(archive.data(), archive.size()))) {
			out.status = Status::ErrorUnknown;
			result.installed.push_back(sp::move(out));
			result.status = Status::ErrorUnknown;
			result.error = toString("failed to write ") + tmpFile;
			return result;
		}

		String finalDir = component_dir(layout, kind, comp->id);
		auto pos = finalDir.rfind('/');
		String parent = (pos != String::npos)
				? toString(StringView(finalDir.data(), pos))
				: toString(".");
		String staging = parent + "/.staging-" + comp->id;
		if (is_dir(StringView(staging))) {
			filesystem::remove(FileInfo(StringView(staging)), true);
		}
		filesystem::mkdir_recursive(FileInfo(StringView(staging)));

		// Extract into staging.
		int rc = run_process({toString("tar"), toString("-xf"), tmpFile, toString("-C"), staging});
		if (rc != 0) {
			out.status = Status::ErrorUnknown;
			result.installed.push_back(sp::move(out));
			result.status = Status::ErrorUnknown;
			char buf[64];
			snprintf(buf, sizeof(buf), "tar extraction failed (exit %d)", rc);
			result.error = toString(buf);
			filesystem::remove(FileInfo(StringView(staging)), true);
			return result;
		}

		// Promote a single top-level <id>/<triple> wrapper, then atomically swap into place.
		String placed = wrapper_dir(StringView(staging), StringView(comp->id), StringView(comp->triple));
		if (placed.empty()) {
			placed = staging;
		}
		if (is_dir(StringView(finalDir))) {
			filesystem::remove(FileInfo(StringView(finalDir)), true);
		}
		if (::rename(placed.c_str(), finalDir.c_str()) != 0) {
			out.status = Status::ErrorUnknown;
			result.installed.push_back(sp::move(out));
			result.status = Status::ErrorUnknown;
			result.error = toString("failed to place ") + finalDir;
			return result;
		}
		if (placed != staging && is_dir(StringView(staging))) {
			filesystem::remove(FileInfo(StringView(staging)), true);
		}

		InstalledComponent ic;
		ic.id = comp->id;
		ic.triple = comp->triple;
		ic.variant = comp->variant;
		ic.kind = kind;
		ic.release = toString(default_release());
		ic.sha256 = out.sha256;
		ic.installedAt = rfc3339_now();
		ic.path = finalDir;
		state.upsert(sp::move(ic));

		out.destPath = sp::move(finalDir);
		result.installed.push_back(sp::move(out));
	}

	if (!state.save(layout.installed_manifest())) {
		result.status = Status::ErrorUnknown;
		result.error = toString("failed to save ") + layout.installed_manifest();
	}
	return result;
}

bool link_toolchains_into_engine_path(const Layout &layout, StringView engineRoot) {
	String engineTc = toString(engineRoot) + "/toolchains";
	bool ok = true;
	for (auto kind : {Kind::Host, Kind::Target}) {
		String storeKind = layout.toolchains_store_dir() + "/" + toString(kind_dir(kind));
		if (!is_dir(StringView(storeKind))) {
			continue;
		}
		String linkKind = engineTc + "/" + toString(kind_dir(kind));
		filesystem::mkdir_recursive(FileInfo(StringView(linkKind)));
		for (const auto &name : list_child_dirs(StringView(storeKind))) {
			String link = linkKind + "/" + name;
			if (!clear_link(StringView(link))) {
				ok = false;
				continue;
			}
			String src = storeKind + "/" + name;
			if (!link_dir(StringView(src), StringView(link))) {
				ok = false;
			}
		}
	}
	return ok;
}

bool link_toolchains_into_engine(const Layout &layout, StringView engineRef) {
	return link_toolchains_into_engine_path(layout, layout.engine_dir(engineRef));
}

bool relink_all_engines(const Layout &layout) {
	auto engines = layout.engines_dir();
	if (!is_dir(StringView(engines))) {
		return true;
	}
	bool ok = true;
	for (const auto &name : list_child_dirs(StringView(engines))) {
		if (!link_toolchains_into_engine(layout, name)) {
			ok = false;
		}
	}
	return ok;
}

bool uninstall(const Layout &layout, Kind kind, StringView id) {
	auto dir = component_dir(layout, kind, id);
	if (is_dir(StringView(dir))) {
		return filesystem::remove(FileInfo(StringView(dir)), true);
	}
	return true;
}

} // namespace stappler::xenolith::installer
