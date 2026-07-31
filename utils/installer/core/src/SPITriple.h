#ifndef INSTALLER_CORE_SPITRIPLE_H_
#define INSTALLER_CORE_SPITRIPLE_H_
#include "SPICommon.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// A resolved host: the triple the machine *is*, plus the host archive triple we will actually
// download (identical unless an emulation fallback kicked in).
struct ResolvedHost {
	String native;
	String hostArchive;
	bool viaEmulation = false;
};

// Map a Rust-style arch/os to the server's naming. NOTE: macOS is `apple-macosx`, NOT
// `apple-darwin` (the server/FTP naming). Return "" on unsupported input.
StringView server_arch(StringView arch);
String server_os(StringView os, StringView libc = StringView());

// Build a server triple for (arch, os[, libc]). "" on unsupported.
String host_triple_from(StringView arch, StringView os, StringView libc = StringView());

bool is_known_host(StringView triple);
// When no host toolchain exists for `triple`, the host the machine can run via emulation
// (win-arm64 → x64 host under WOW64). "" if nothing can run it.
StringView host_fallback(StringView triple);

// The running machine's arch/os/libc (compile-time detection).
StringView native_arch();
StringView native_os();
StringView current_libc(StringView os);

// Resolve the running machine into a downloadable host, applying the fallback policy.
ResolvedHost resolve_host(StringView arch, StringView os);

} // namespace stappler::xenolith::installer

#endif // INSTALLER_CORE_SPITRIPLE_H_
