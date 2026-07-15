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

// Filesystem-wide limits for the memfs. Fixed values (there is a single backend),
// returned by both the path and fd forms.
static long __wasm_pathconf(int name) {
	switch (name) {
	case __SPRT_PC_LINK_MAX: return 1; // no hard links
	case __SPRT_PC_MAX_CANON: return 255;
	case __SPRT_PC_MAX_INPUT: return 255;
	case __SPRT_PC_NAME_MAX: return 255;
	case __SPRT_PC_PATH_MAX: return 4096;
	case __SPRT_PC_PIPE_BUF: return 4096;
	case __SPRT_PC_CHOWN_RESTRICTED: return 1;
	case __SPRT_PC_NO_TRUNC: return 1; // names longer than NAME_MAX are an error
	case __SPRT_PC_VDISABLE: return 0;
	case __SPRT_PC_SYNC_IO: return 1;
	case __SPRT_PC_FILESIZEBITS: return 64;
	case __SPRT_PC_SYMLINK_MAX: return -1; // no symlinks
	case __SPRT_PC_2_SYMLINKS: return 0;
	default: return -1;
	}
}

// --- brk / sbrk over WebAssembly memory.grow ----------------------------
//
// wasm32 has no brk syscall; the "program break" is modelled directly on the
// linear memory. It starts at the linker-provided __heap_base (just past static
// data + the shadow stack) and only ever grows upward: memory.grow appends
// zero-initialised 64 KiB pages, and wasm memory can never shrink, so lowering
// the break releases nothing (the pages stay reserved for a later raise). This
// backs mimalloc's wasi OS-primitive layer (src/prim/wasi/prim.c, MI_USE_SBRK),
// which is why the freestanding wasm libc keeps __SPRT_CONFIG_HAVE_UNISTD_BRK on.

extern "C" unsigned char __heap_base; // linker-provided start of the heap region

namespace {

using __wasm_uptr = __UINTPTR_TYPE__;
constexpr __wasm_uptr WASM_PAGE_BYTES = 65536u;

// Current program break; 0 means "not yet initialised" (lazily set to the base).
__wasm_uptr s_wasm_break = 0;

__wasm_uptr __wasm_break_base(void) {
	// 16-byte (max_align_t) aligned so the very first hand-out is aligned.
	__wasm_uptr base = reinterpret_cast<__wasm_uptr>(&__heap_base);
	return (base + 15u) & ~static_cast<__wasm_uptr>(15u);
}

} // namespace

extern "C" int brk(void *__addr) __SPRT_NOEXCEPT {
	const __wasm_uptr base = __wasm_break_base();
	if (s_wasm_break == 0) {
		s_wasm_break = base;
	}
	const __wasm_uptr want = reinterpret_cast<__wasm_uptr>(__addr);
	if (want < base) {
		__sprt_errno = ENOMEM; // cannot move the break below the heap base
		return -1;
	}
	const __wasm_uptr have = static_cast<__wasm_uptr>(__builtin_wasm_memory_size(0)) * WASM_PAGE_BYTES;
	if (want > have) {
		const __wasm_uptr pages = (want - have + WASM_PAGE_BYTES - 1) / WASM_PAGE_BYTES;
		if (static_cast<__SIZE_TYPE__>(__builtin_wasm_memory_grow(0, pages))
				== static_cast<__SIZE_TYPE__>(-1)) {
			__sprt_errno = ENOMEM;
			return -1;
		}
	}
	s_wasm_break = want;
	return 0;
}

extern "C" void *sbrk(__INTPTR_TYPE__ __incr) __SPRT_NOEXCEPT {
	if (s_wasm_break == 0) {
		s_wasm_break = __wasm_break_base();
	}
	const __wasm_uptr old = s_wasm_break;
	if (__incr == 0) {
		return reinterpret_cast<void *>(old);
	}
	const __wasm_uptr want = old + static_cast<__wasm_uptr>(__incr);
	// Overflow / underflow guard in either direction.
	if ((__incr > 0 && want < old) || (__incr < 0 && want > old)) {
		__sprt_errno = ENOMEM;
		return reinterpret_cast<void *>(static_cast<__INTPTR_TYPE__>(-1));
	}
	if (brk(reinterpret_cast<void *>(want)) != 0) {
		return reinterpret_cast<void *>(static_cast<__INTPTR_TYPE__>(-1));
	}
	return reinterpret_cast<void *>(old);
}

extern "C" long pathconf(const char *, int name) __SPRT_NOEXCEPT { return __wasm_pathconf(name); }

extern "C" long fpathconf(int fd, int name) __SPRT_NOEXCEPT {
	if (sprt::__libc::get()->get_fd_handle(fd) == nullptr) {
		__sprt_errno = EBADF;
		return -1;
	}
	return __wasm_pathconf(name);
}

// fsync/fdatasync live in the memfs TU (wasm/libc_path.cc) where the inode + OPFS
// backend are in scope: an OPFS-backed file must be written back on fsync.
