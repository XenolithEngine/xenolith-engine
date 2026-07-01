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

// The engine's built-in variables. Part of the makefile unity build (included from SPMakefile.cpp).
// Called once from Makefile::init(), this defines the "xlmake identity": the variables the Stappler
// build system keys on so that ANY consumer of this engine — xlmake, the reusable project loader
// (SPMakefileProject.cc), the in-app source observer — takes the engine-native `init-xlmake.mk`
// path. Previously this was set up by hand only in the xlmake tool; making it intrinsic to the
// engine is what "auto-detects XLMAKE_VERSION for the whole make system".

#include "SPMakefile.h"
#include "SPMakefileDirectives.h"

#if !SPRT_WINDOWS
#include <sys/utsname.h>
#include <dlfcn.h> // runtime glibc detection (gnu_get_libc_version); see below
#endif

namespace STAPPLER_VERSIONIZED stappler::makefile {

void Makefile::setupBuiltinVariables() {
	// Origin::Default: a makefile or the command line may still override any of these.
	auto simple = [&](StringView n, StringView v) { assignSimpleVariable(n, Origin::Default, v); };

	// The engine identity: presence of XLMAKE_VERSION is what selects `init-xlmake.mk`.
	simple("XLMAKE_VERSION", XlmakeVersion);

	// In-process directive markers behind $(WRITE)/$(APPEND)/$(MKDIR)/$(REMOVE)/$(CP)/$(ECHO). An
	// executor recognizes the expanded values; the $(xl_write)/$(xl_mkdir)/$(xl_cat) functions used by
	// `init-xlmake.mk` at load time are already engine built-ins (SPMakefileVariable.cc).
	simple("WRITE", WriteDirectiveMarker);
	simple("APPEND", AppendDirectiveMarker);
	simple("MKDIR", MkdirDirectiveMarker);
	simple("REMOVE", RemoveDirectiveMarker);
	simple("CP", CopyDirectiveMarker);
	simple("ECHO", EchoDirectiveMarker);

	// Host detection: `init-xlmake.mk` reads XL_UNAME_SYSNAME (as UNAME), XL_UNAME_MACHINE (as the
	// host arch) and XL_GLIBC_VERSION (glibc vs musl) to compute STAPPLER_HOST for the current
	// platform. pdup the transient buffers into the pool so the assignments outlive this frame.
#if !SPRT_WINDOWS
	struct utsname u;
	if (::uname(&u) == 0) {
		simple("OS", StringView(u.sysname).pdup(_pool));
		simple("XL_UNAME_SYSNAME", StringView(u.sysname).pdup(_pool));
		simple("XL_UNAME_NODENAME", StringView(u.nodename).pdup(_pool));
		simple("XL_UNAME_RELEASE", StringView(u.release).pdup(_pool));
		simple("XL_UNAME_VERSION", StringView(u.version).pdup(_pool));
		StringView machine(u.machine);
		if (machine == "arm64") {
			machine = StringView("aarch64"); // rewrite as the Xenolith standard name
		}
		simple("XL_UNAME_MACHINE", machine.pdup(_pool));
	}
	// Detect glibc at RUNTIME: the engine is typically compiled against a non-glibc runtime libc yet
	// runs on a glibc host, so a compile-time __GLIBC__ check would be wrong. Resolving the symbol
	// gnu_get_libc_version means glibc; its absence means musl. init-xlmake.mk keys STAPPLER_HOST
	// (…-linux-gnu vs …-linux-musl) on the presence of XL_GLIBC_VERSION.
	auto gnu_get_libc_version = (const char *(*)()) ::dlsym(RTLD_DEFAULT, "gnu_get_libc_version");
	if (gnu_get_libc_version != nullptr) {
		simple("XL_GLIBC_VERSION", StringView(gnu_get_libc_version()).pdup(_pool));
	}
#else // SPRT_WINDOWS
	simple("OS", "Windows_NT");
	simple("XL_UNAME_SYSNAME", "Windows");
#if defined(_M_ARM64) || defined(__aarch64__)
	simple("XL_UNAME_MACHINE", "aarch64");
#else
	simple("XL_UNAME_MACHINE", "x86_64");
#endif
#endif
}

} // namespace stappler::makefile
