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

#ifndef UTILS_INSTALLER_CORE_SRC_SPITRIPLE_H_
#define UTILS_INSTALLER_CORE_SRC_SPITRIPLE_H_

#include "SPICommon.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// A resolved host: the triple the machine *is*, plus the host archive triple we will actually
// download (identical unless an emulation fallback kicked in).
struct SP_PUBLIC ResolvedHost {
	String native;
	String hostArchive;
	bool viaEmulation = false;
};

// Map an arch/os pair to the server's naming. NOTE: macOS is `apple-macosx`, NOT `apple-darwin`
// (the server/FTP naming). Both return "" on unsupported input.
SP_PUBLIC StringView getServerArch(StringView arch);
SP_PUBLIC String getServerOs(StringView os, StringView libc = StringView());

// Build a server triple for (arch, os[, libc]). "" on unsupported.
SP_PUBLIC String makeHostTriple(StringView arch, StringView os, StringView libc = StringView());

SP_PUBLIC bool isKnownHost(StringView triple);

// When no host toolchain exists for `triple`, the host the machine can run via emulation
// (win-arm64 → x64 host under WOW64). "" if nothing can run it.
SP_PUBLIC StringView getHostFallback(StringView triple);

// The running machine's arch/os/libc (compile-time detection).
SP_PUBLIC StringView getNativeArch();
SP_PUBLIC StringView getNativeOs();
SP_PUBLIC StringView getCurrentLibc(StringView os);

// Resolve the running machine into a downloadable host, applying the fallback policy.
SP_PUBLIC ResolvedHost resolveHost(StringView arch, StringView os);

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_CORE_SRC_SPITRIPLE_H_
