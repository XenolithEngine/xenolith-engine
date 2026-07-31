#include "SPITriple.h"

#include <cstdio>
#include <cstdlib>

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

StringView server_arch(StringView arch) {
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

String server_os(StringView os, StringView libc) {
	if (os == "macos" || os == "ios") {
		return "apple-macosx";
	}
	if (os == "windows") {
		return "pc-windows-msvc";
	}
	if (os == "linux" || os == "android") {
		String lib = libc.empty() ? toString("gnu") : toString(libc);
		return String("unknown-linux-") + lib;
	}
	return String();
}

String host_triple_from(StringView arch, StringView os, StringView libc) {
	auto a = server_arch(arch);
	auto o = server_os(os, libc);
	if (a.empty() || o.empty()) {
		return String();
	}
	return toString(a) + "-" + o;
}

bool is_known_host(StringView triple) {
	for (auto h : kKnownHosts) {
		if (triple == StringView(h)) {
			return true;
		}
	}
	return false;
}

StringView host_fallback(StringView triple) {
	if (is_known_host(triple)) {
		return triple;
	}
	if (triple == "aarch64-pc-windows-msvc") {
		return "x86_64-pc-windows-msvc"; // Windows-on-ARM runs x86_64 under emulation
	}
	return StringView();
}

StringView native_arch() {
#if defined(__aarch64__) || defined(_M_ARM64)
	return "aarch64";
#elif defined(__x86_64__) || defined(_M_X64)
	return "x86_64";
#elif defined(__riscv) && (__riscv_xlen == 64)
	return "riscv64";
#else
	return StringView();
#endif
}

StringView native_os() {
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

StringView current_libc(StringView os) {
	if (os != "linux") {
		return StringView();
	}
#if SPRT_LINUX
	// musl installs /lib/ld-musl-<arch>.so.1 and Alpine adds /etc/alpine-release; glibc has neither
	bool musl = false;
	auto probe = [](const char *p) -> bool {
		FILE *f = std::fopen(p, "r");
		if (f) {
			std::fclose(f);
			return true;
		}
		return false;
	};
	musl = probe("/lib/ld-musl-aarch64.so.1") || probe("/lib/ld-musl-x86_64.so.1")
			|| probe("/lib/ld-musl-riscv64.so.1") || probe("/etc/alpine-release");
	return musl ? "musl" : "gnu";
#else
	return "gnu";
#endif
}

ResolvedHost resolve_host(StringView arch, StringView os) {
	ResolvedHost r;
	auto libc = current_libc(os);
	r.native = host_triple_from(arch, os, libc);
	if (is_known_host(r.native)) {
		r.hostArchive = r.native;
		r.viaEmulation = false;
		return r;
	}
	auto fb = host_fallback(r.native);
	if (!fb.empty()) {
		r.hostArchive = toString(fb);
		r.viaEmulation = true;
	} else {
		r.hostArchive.clear();
	}
	return r;
}

} // namespace stappler::xenolith::installer
