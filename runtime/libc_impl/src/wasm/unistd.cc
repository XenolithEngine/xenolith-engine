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

// WebAssembly unistd backend.
//
// SKELETON SCOPE: this unit only brings the shared libc internals into scope so
// the generic builtin_unistd.cpp body (read/write/close/dup/... over __fd_ops)
// compiles. The wasm path family (open/openat/access/mkdir/unlink/rename,
// getcwd/chdir as a virtual cwd, pipe over a futex ring, sysconf/pathconf
// constants — wasm-port-draft.adoc §3.3) is a later milestone and is not defined
// yet; those symbols resolve when that milestone lands.

#ifndef __SPRT_BUILD
#define __SPRT_BUILD
#endif

#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/__sprt_unistd.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_time.h>

// Pulls in __libc, StringView, the fd dispatch tables, and (via sys/stat.h) the
// utimensat declaration that builtin_unistd.cpp's utime() forwards to.
#include "../../include/__impl_libc.h"

// Process / user identity. A wasm module is a single sandboxed "process" with no user
// model, so these report fixed sentinels (pid 1, uid/gid 0) — enough for callers that
// only compare or log them. libc_impl provides the plain names the wrapper forwards to.
extern "C" __SPRT_ID(pid_t) getpid(void) __SPRT_NOEXCEPT { return 1; }
extern "C" __SPRT_ID(pid_t) getppid(void) __SPRT_NOEXCEPT { return 0; }
extern "C" __SPRT_ID(uid_t) getuid(void) __SPRT_NOEXCEPT { return 0; }
extern "C" __SPRT_ID(uid_t) geteuid(void) __SPRT_NOEXCEPT { return 0; }
extern "C" __SPRT_ID(gid_t) getgid(void) __SPRT_NOEXCEPT { return 0; }
extern "C" __SPRT_ID(gid_t) getegid(void) __SPRT_NOEXCEPT { return 0; }

// usleep over the runtime's futex-timeout sleep (nanosleep).
extern "C" int usleep(__SPRT_ID(time_t) useconds) __SPRT_NOEXCEPT {
	struct __SPRT_TIMESPEC_NAME ts;
	ts.tv_sec = (__SPRT_ID(time_t))(useconds / 1000000);
	ts.tv_nsec = (long)((useconds % 1000000) * 1000);
	return __SPRT_ID(nanosleep)(&ts, nullptr);
}

extern "C" long sysconf(int name) __SPRT_NOEXCEPT {
	switch (name) {
	case __SPRT_SC_PAGESIZE: return 65536; // wasm page granularity
	case __SPRT_SC_NPROCESSORS_CONF:
	case __SPRT_SC_NPROCESSORS_ONLN: return 1;
	default: return -1;
	}
}

extern "C" long pathconf(const char *, int) __SPRT_NOEXCEPT { return -1; }
