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


#include "SPISelfUpdate.h"
#include "SPITransport.h"
#include "SPITriple.h"
#include "SPIProcess.h"

#include <sprt/runtime/platform.h>

#include <stdlib.h> // getenv: there is no runtime wrapper for the environment
#include <stdio.h> // rename
#include <errno.h>
#include <string.h> // strerror
#include <sys/stat.h> // chmod

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

namespace {

constexpr StringView kCliTagPrefix = "cli-v";
constexpr StringView kCliReleasesApi =
		"https://api.github.com/repos/XenolithEngine/xenolith-engine/releases?per_page=100";
constexpr StringView kCliDownloadRoot =
		"https://github.com/XenolithEngine/xenolith-engine/releases/download/";

#if SPRT_WINDOWS
constexpr StringView kCliBinaryName = "xenolith-cli.exe";
#else
constexpr StringView kCliBinaryName = "xenolith-cli";
#endif

bool parseCliVersion(StringView v, Vector<uint64_t> &out) {
	if (v.starts_with(kCliTagPrefix)) {
		v.offset(kCliTagPrefix.size());
	} else if (v.is('v')) {
		++v;
	}
	out.clear();
	while (!v.empty()) {
		auto num = v.readChars<StringView::CharGroup<CharGroupId::Numbers>>();
		if (num.empty()) {
			return false;
		}
		out.emplace_back(uint64_t(num.readInteger(10).get(0)));
		if (v.empty()) {
			break;
		}
		if (!v.is('.')) {
			return false;
		}
		++v;
		if (v.empty()) {
			return false;
		}
	}
	return !out.empty();
}

bool isSameHex(StringView a, StringView b) {
	if (a.size() != b.size() || a.empty()) {
		return false;
	}
	for (size_t i = 0; i < a.size(); ++i) {
		if (::tolower(a[i]) != ::tolower(b[i])) {
			return false;
		}
	}
	return true;
}

String getDownloadBase(StringView tag) {
	if (const char *e = ::getenv("XENOLITH_RELEASE_BASE"); e && *e) {
		return toString(StringView(e));
	}
	return toString(kCliDownloadRoot, tag);
}

String getErrno() { return toString(StringView(::strerror(errno))); }

} // namespace

String normalizeCliTag(StringView version) {
	Vector<uint64_t> parts;
	if (!parseCliVersion(version, parts)) {
		return String();
	}
	String ret = toString(kCliTagPrefix);
	for (size_t i = 0; i < parts.size(); ++i) {
		if (i > 0) {
			ret.append(".");
		}
		ret.append(toString(parts[i]));
	}
	return ret;
}

int compareCliVersions(StringView a, StringView b) {
	Vector<uint64_t> pa, pb;
	const bool va = parseCliVersion(a, pa);
	const bool vb = parseCliVersion(b, pb);
	if (!va || !vb) {
		return int(va) - int(vb);
	}
	for (size_t i = 0; i < max(pa.size(), pb.size()); ++i) {
		const uint64_t x = i < pa.size() ? pa[i] : 0;
		const uint64_t y = i < pb.size() ? pb[i] : 0;
		if (x != y) {
			return x < y ? -1 : 1;
		}
	}
	return 0;
}

String getCliAssetTriple() {
	auto os = getNativeOs();
	return makeHostTriple(getNativeArch(), os, os == "linux" ? StringView("musl") : StringView());
}

CliReleaseQuery findLatestCliRelease() {
	CliReleaseQuery result;

	String text;
	auto r = fetchTextRetry(kCliReleasesApi, text);
	if (!r) {
		result.setError(r.status, "cannot list releases: ", r.error);
		return result;
	}

	auto releases = data::json::read<mem_std::Interface>(StringView(text));
	if (!releases.isArray()) {
		// An API error (a rate limit, most often) is a dict with a message.
		result.setError(Status::ErrorInvalidArguemnt, "unexpected reply from GitHub",
				releases.isDictionary() ? StringView(": ") : StringView(),
				releases.getString("message"));
		return result;
	}

	for (const auto &rel : releases.getArray()) {
		auto tag = rel.getString("tag_name");
		if (!StringView(tag).starts_with(kCliTagPrefix) || rel.getBool("draft")
				|| rel.getBool("prerelease") || normalizeCliTag(tag).empty()) {
			continue;
		}
		if (result.tag.empty() || compareCliVersions(tag, result.tag) > 0) {
			result.tag = tag;
		}
	}

	if (result.tag.empty()) {
		result.setError(Status::ErrorNotFound, "no ", kCliTagPrefix, "* release found");
	}
	return result;
}

SelfUpdateResult installCliRelease(StringView tag,
		const Function<void(int64_t, int64_t)> &progress) {
	SelfUpdateResult result;

	auto exe = sprt::platform::getExecPath().str<mem_std::Interface>();
	if (exe.empty() || !isFile(exe)) {
		result.setError(Status::ErrorNotFound, "cannot locate the running executable");
		return result;
	}
	result.path = exe;

	// Staged beside the executable, so the final rename stays on one filesystem.
	auto dir = filepath::root(StringView(exe)).str<mem_std::Interface>();
	auto staging = mergePath(dir, ".xenolith-cli-update");
	if (isDirectory(staging)) {
		filesystem::remove(FileInfo(StringView(staging)), true);
	}
	if (!filesystem::mkdir_recursive(FileInfo(StringView(staging)))) {
		result.setError(Status::ErrorNotPermitted, "cannot write to ", dir,
				" (installed by another user? rerun with the rights to replace ", exe, ")");
		return result;
	}

	auto fail = [&](Status st, auto &&...args) -> SelfUpdateResult & {
		filesystem::remove(FileInfo(StringView(staging)), true);
		result.setError(st, sp::forward<decltype(args)>(args)...);
		return result;
	};

	auto asset = toString("xenolith-cli-", getCliAssetTriple(), ".tar.gz");
	auto base = getDownloadBase(tag);

	Bytes archive;
	auto rf = fetchBytesRetry(toString(base, "/", asset), archive, progress);
	if (!rf) {
		return fail(rf.status, "download ", base, "/", asset, ": ", rf.error);
	}

	String sidecar;
	auto rs = fetchTextRetry(toString(base, "/", asset, ".sha256"), sidecar);
	if (!rs) {
		return fail(rs.status, "download ", base, "/", asset, ".sha256: ", rs.error);
	}

	// The sidecar is "<hex>  <name>"; only the hash is compared.
	StringView expected(sidecar);
	expected = expected.readUntil<StringView::WhiteSpace>();
	auto actual = base16::encode<mem_std::Interface>(
			string::Sha256().update(archive.data(), archive.size()).final());
	if (!isSameHex(expected, actual)) {
		return fail(Status::ErrorInvalidArguemnt, "checksum mismatch for ", asset, ": expected ",
				expected, ", got ", actual);
	}

	auto archivePath = mergePath(staging, asset);
	if (!filesystem::write(FileInfo(StringView(archivePath)),
				BytesView(archive.data(), archive.size()))) {
		return fail(Status::ErrorNotPermitted, "failed to write ", archivePath);
	}

	StringView tarArgs[] = {StringView("tar"), StringView("-xzf"), StringView(archivePath),
		StringView("-C"), StringView(staging)};
	auto tar = runCommand(tarArgs);
	if (!tar) {
		return fail(tar.status, "tar extraction failed: ", tar.error);
	}

	auto fresh = mergePath(staging, kCliBinaryName);
	if (!isFile(fresh)) {
		return fail(Status::ErrorNotFound, asset, " does not contain ", kCliBinaryName);
	}
#if !SPRT_WINDOWS
	::chmod(fresh.data(), 0755);
#endif

	// The new binary has to run here before it replaces one that does.
	String output;
	auto capture = [&output](StringView str) { output.append(str.data(), str.size()); };
	Callback<void(StringView)> captureCallback(capture);
	StringView versionArgs[] = {StringView(fresh), StringView("--version")};
	auto probe = runCommand(versionArgs, StringView(), &captureCallback);
	StringView reported(output);
	reported.skipChars<StringView::WhiteSpace>();
	reported = reported.readUntil<StringView::Chars<'\r', '\n'>>();
	if (!probe || probe.exitCode != 0 || !reported.starts_with("xenolith-cli ")) {
		return fail(Status::ErrorNotSupported, "the downloaded ", kCliBinaryName,
				" does not run on this machine; nothing was replaced");
	}
	result.version = reported.str<mem_std::Interface>();

#if SPRT_WINDOWS
	// A running executable cannot be overwritten, but it can be renamed away.
	auto old = toString(exe, ".old");
	if (isFile(old)) {
		filesystem::remove(FileInfo(StringView(old)));
	}
	if (::rename(exe.data(), old.data()) != 0) {
		return fail(Status::ErrorNotPermitted, "cannot move ", exe, " aside: ", getErrno());
	}
	if (::rename(fresh.data(), exe.data()) != 0) {
		auto reason = getErrno();
		::rename(old.data(), exe.data());
		return fail(Status::ErrorNotPermitted, "cannot replace ", exe, ": ", reason);
	}
	// install.ps1 keeps the old tool name as a second copy rather than a link.
	auto compat = mergePath(dir, "xenolith-installer-cli.exe");
	if (isFile(compat)) {
		filesystem::remove(FileInfo(StringView(compat)));
		if (!filesystem::copy(FileInfo(StringView(exe)), FileInfo(StringView(compat)))) {
			result.warning = toString(compat, " was not updated");
		}
	}
#else
	if (::rename(fresh.data(), exe.data()) != 0) {
		return fail(Status::ErrorNotPermitted, "cannot replace ", exe, ": ", getErrno());
	}
#endif

	filesystem::remove(FileInfo(StringView(staging)), true);
	return result;
}

void cleanupSelfUpdate() {
#if SPRT_WINDOWS
	auto exe = sprt::platform::getExecPath();
	if (!exe.empty()) {
		auto old = toString(exe, ".old");
		if (isFile(old)) {
			filesystem::remove(FileInfo(StringView(old)));
		}
	}
#endif
}

} // namespace stappler::xenolith::installer
