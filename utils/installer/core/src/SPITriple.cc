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

#include "SPITriple.h"

#include "SPFilesystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

namespace {

// Host triples for which a toolchain archive exists on the server (sdk-v0beta1 host set).
// OFFLINE fast-path used by `detect`; the fetched manifest is authoritative — keep in sync.
constexpr const char *kKnownHosts[] = {
	"aarch64-apple-macosx",
	"x86_64-apple-macosx",
	"x86_64-pc-windows-msvc",
	"x86_64-unknown-linux-gnu",
	"aarch64-unknown-linux-gnu",
	"riscv64-unknown-linux-gnu",
	"x86_64-unknown-linux-musl",
	"aarch64-unknown-linux-musl",
	"riscv64-unknown-linux-musl",
};

} // namespace

StringView getServerArch(StringView arch) {
	if (arch == "aarch64" || arch == "arm64") {
		return "aarch64";
	}
	if (arch == "x86_64" || arch == "amd64") {
		return "x86_64";
	}
	if (arch == "riscv64") {
		return "riscv64";
	}
	return StringView();
}

String getServerOs(StringView os, StringView libc) {
	if (os == "macos" || os == "ios") {
		return toString("apple-macosx");
	}
	if (os == "windows") {
		return toString("pc-windows-msvc");
	}
	if (os == "linux" || os == "android") {
		return toString("unknown-linux-", libc.empty() ? StringView("gnu") : libc);
	}
	return String();
}

String makeHostTriple(StringView arch, StringView os, StringView libc) {
	auto a = getServerArch(arch);
	auto o = getServerOs(os, libc);
	if (a.empty() || o.empty()) {
		return String();
	}
	return toString(a, "-", o);
}

bool isKnownHost(StringView triple) {
	for (auto h : kKnownHosts) {
		if (triple == StringView(h)) {
			return true;
		}
	}
	return false;
}

StringView getHostFallback(StringView triple) {
	if (isKnownHost(triple)) {
		return triple;
	}
	if (triple == "aarch64-pc-windows-msvc") {
		return "x86_64-pc-windows-msvc"; // Windows-on-ARM runs x86_64 under emulation
	}
	return StringView();
}

StringView getNativeArch() {
#if __SPRT_ARCH_ID == __SPRT_ARCH_ID_AARCH64
	return "aarch64";
#elif __SPRT_ARCH_ID == __SPRT_ARCH_ID_X86_64
	return "x86_64";
#elif __SPRT_ARCH_ID == __SPRT_ARCH_ID_RISCV64
	return "riscv64";
#else
	return StringView();
#endif
}

StringView getNativeOs() {
#if SPRT_APPLE
	return "macos";
#elif SPRT_WINDOWS
	return "windows";
#elif SPRT_LINUX
	return "linux";
#else
	return StringView();
#endif
}

StringView getCurrentLibc(StringView os) {
	if (os != "linux") {
		return StringView();
	}
#if SPRT_LINUX
	// musl installs /lib/ld-musl-<arch>.so.1 and Alpine adds /etc/alpine-release; glibc has neither
	auto probe = [](StringView p) { return filesystem::exists(FileInfo(p)); };
	bool musl = probe("/lib/ld-musl-aarch64.so.1") || probe("/lib/ld-musl-x86_64.so.1")
			|| probe("/lib/ld-musl-riscv64.so.1") || probe("/etc/alpine-release");
	return musl ? "musl" : "gnu";
#else
	return "gnu";
#endif
}

ResolvedHost resolveHost(StringView arch, StringView os) {
	ResolvedHost r;
	r.native = makeHostTriple(arch, os, getCurrentLibc(os));
	if (isKnownHost(r.native)) {
		r.hostArchive = r.native;
		r.viaEmulation = false;
		return r;
	}
	auto fb = getHostFallback(r.native);
	if (!fb.empty()) {
		r.hostArchive = toString(fb);
		r.viaEmulation = true;
	} else {
		r.hostArchive.clear();
	}
	return r;
}

} // namespace stappler::xenolith::installer
