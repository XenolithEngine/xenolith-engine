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

#include <stdlib.h> // getenv: there is no runtime wrapper for the environment
#include <string.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

namespace {

// Host triples for which a toolchain archive exists on the server (RELEASE_HOSTS in
// runtime/toolchains/Makefile).
// OFFLINE fast-path used by `detect`; the fetched manifest is authoritative — keep in sync.
constexpr const char *kKnownHosts[] = {
	"aarch64-apple-macosx",
	"x86_64-apple-macosx",
	"x86_64-pc-windows-msvc",
	"aarch64-pc-windows-msvc",
	"x86_64-unknown-linux-gnu",
	"aarch64-unknown-linux-gnu",
	"riscv64-unknown-linux-gnu",
	"loongarch64-unknown-linux-gnu",
	"x86_64-unknown-linux-musl",
	"aarch64-unknown-linux-musl",
	"riscv64-unknown-linux-musl",
	"loongarch64-unknown-linux-musl",
};

#if SPRT_LINUX
// Little-endian unsigned integer of `width` bytes at `off`; false when it does not fit.
bool readLe(const mem_std::Bytes &data, size_t off, size_t width, uint64_t &out) {
	if (off > data.size() || width > data.size() - off) {
		return false;
	}
	out = 0;
	for (size_t i = 0; i < width; ++i) { out |= uint64_t(data[off + i]) << (8 * i); }
	return true;
}

// The PT_INTERP path of a little-endian ELF executable; empty for a static or a non-ELF file.
String readElfInterpreter(StringView path) {
	auto data = filesystem::readIntoMemory<mem_std::Interface>(FileInfo(path), 0, 4_KiB);
	if (data.size() < 64 || data[0] != 0x7f || data[1] != 'E' || data[2] != 'L' || data[3] != 'F'
			|| data[5] != 1) {
		return String();
	}

	const bool is64 = data[4] == 2;
	uint64_t phoff = 0, phentsize = 0, phnum = 0;
	if (!readLe(data, is64 ? 0x20 : 0x1C, is64 ? 8 : 4, phoff)
			|| !readLe(data, is64 ? 0x36 : 0x2A, 2, phentsize)
			|| !readLe(data, is64 ? 0x38 : 0x2C, 2, phnum)) {
		return String();
	}

	for (uint64_t i = 0; i < phnum; ++i) {
		const size_t ph = phoff + i * phentsize;
		uint64_t type = 0, offset = 0, size = 0;
		if (!readLe(data, ph, 4, type)) {
			return String();
		}
		if (type != 3) { // PT_INTERP
			continue;
		}
		if (!readLe(data, ph + (is64 ? 0x08 : 0x04), is64 ? 8 : 4, offset)
				|| !readLe(data, ph + (is64 ? 0x20 : 0x10), is64 ? 8 : 4, size)
				|| offset > data.size() || size > data.size() - offset) {
			return String();
		}
		auto interp = reinterpret_cast<const char *>(data.data() + offset);
		return String(interp, ::strnlen(interp, size_t(size)));
	}
	return String();
}
#endif

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
	if (arch == "loongarch64") {
		return "loongarch64";
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
#elif __SPRT_ARCH_ID == __SPRT_ARCH_ID_LOONGARCH64
	return "loongarch64";
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
	// The distribution's libc is the one its shell is linked to: a second libc installed beside it
	// (musl-libc on Fedora, musl on Debian) brings its loader but no system binary uses it.
	for (auto exe : {StringView("/bin/sh"), StringView("/usr/bin/env")}) {
		auto interp = readElfInterpreter(exe);
		auto loader = filepath::lastComponent(StringView(interp));
		if (loader.starts_with("ld-musl-")) {
			return "musl";
		}
		if (loader.starts_with("ld-linux")) {
			return "gnu";
		}
	}
	// Neither names a loader (a static busybox): only Alpine is known to ship that way.
	return filesystem::exists(FileInfo("/etc/alpine-release")) ? "musl" : "gnu";
#else
	return "gnu";
#endif
}

ResolvedHost resolveHost(StringView native) {
	ResolvedHost r;
	r.native = native.str<mem_std::Interface>();
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

ResolvedHost resolveHost(StringView arch, StringView os) {
	return resolveHost(StringView(makeHostTriple(arch, os, getCurrentLibc(os))));
}

ResolvedHost resolveNativeHost() {
	if (const char *e = ::getenv("STAPPLER_HOST"); e && *e) {
		return resolveHost(StringView(e));
	}
	return resolveHost(getNativeArch(), getNativeOs());
}

} // namespace stappler::xenolith::installer
