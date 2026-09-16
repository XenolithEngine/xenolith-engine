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

// THE lock that serializes linear-memory growth. There is exactly one, and
// every grow path goes through it: brk/sbrk take it around the memory.grow
// itself, and mimalloc's wasi prim layer takes the same lock (through
// __sprt_wasm_grow_lock) around its probe+grow pair, which has to be atomic
// as a whole or two allocators compute the same aligned base.
//
// Recursive, for exactly that reason: the sbrk inside the prim layer's
// probe+grow pair must not block on the lock its own thread already holds.
// The owner is identified by the address of its errno slot - one per thread,
// never null, and already resolved on every path through here.
//
// memory.grow is not atomic across wasm agents and pthread_mutex waits are
// instance-local, so this has to be a raw atomic in shared linear memory. A
// busy-spin would starve the grower (shared memory only grows while the other
// agents are parked), so waiters park in memory.atomic.wait32.
int s_growLock = 0; // 0 when free, else the owning thread's token
int s_growDepth = 0; // recursion depth; only ever touched by the owner
constexpr int64_t WASM_GROW_STUCK_NS = 10 * 1000 * 1000 * 1000ll; // 10 s

// Distinct texts on purpose: "still waiting" is another agent holding the lock
// too long (the tab is alive, the grow is slow or its holder died mid-grow),
// "grow refused" is the engine itself refusing to hand out pages (the heap
// ceiling is reached). They are reported from different places and mean
// different things, so they must never read the same in a log.
const char s_growStuckMsg[] =
		"sprt: wasm grow lock held over 10s by another agent; still waiting\n";
const char s_growFailedMsg[] = "sprt: wasm memory.grow refused, heap ceiling reached (ENOMEM)\n";

int __wasm_grow_token(void) {
	// The errno slot is thread_local, so its address is one per thread, and it is
	// 4-aligned - ORing in the low bit keeps that one-per-thread property and
	// guarantees the token is never 0, the value that means "free".
	return static_cast<int>(reinterpret_cast<__wasm_uptr>(&__sprt_errno)) | 1;
}

void __wasm_grow_lock(void) {
	const int self = __wasm_grow_token();
	if (__atomic_load_n(&s_growLock, __ATOMIC_RELAXED) == self) {
		++s_growDepth; // already ours: the probe+grow pair re-entering through sbrk
		return;
	}
	for (;;) {
		int expected = 0;
		if (__atomic_compare_exchange_n(&s_growLock, &expected, self, false, __ATOMIC_ACQUIRE,
					__ATOMIC_RELAXED)) {
			break;
		}
		// wait32: 0 woken, 1 value changed, 2 timed out. A holder that died
		// mid-grow would hang every waiter; the timeout only makes that
		// observable - the waiter parks again, it must NOT proceed (memory
		// handed out after a mid-grow death overlaps a live allocation).
		if (__builtin_wasm_memory_atomic_wait32(&s_growLock, expected, WASM_GROW_STUCK_NS) == 2) {
			// Raw write(2), not perror/stderr: the FILE lock inside stdio
			// could be held by a thread waiting on this very lock.
			write(2, s_growStuckMsg, sizeof(s_growStuckMsg) - 1);
		}
	}
	s_growDepth = 1;
}

void __wasm_grow_unlock(void) {
	if (s_growDepth > 1) {
		--s_growDepth;
		return;
	}
	s_growDepth = 0;
	__atomic_store_n(&s_growLock, 0, __ATOMIC_RELEASE);
	__builtin_wasm_memory_atomic_notify(&s_growLock, 1);
}

__wasm_uptr __wasm_break_base(void) {
	// 16-byte (max_align_t) aligned so the very first hand-out is aligned.
	__wasm_uptr base = reinterpret_cast<__wasm_uptr>(&__heap_base);
	return (base + 15u) & ~static_cast<__wasm_uptr>(15u);
}

int __wasm_brk_unlocked(void *__addr) {
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
			// Raw write(2) again: this runs with the grow lock held, and
			// perror would take the stderr FILE lock on top of it - a thread
			// holding that lock and allocating would close the cycle.
			write(2, s_growFailedMsg, sizeof(s_growFailedMsg) - 1);
			return -1;
		}
	}
	s_wasm_break = want;
	return 0;
}

} // namespace

// The allocator's OS-primitive layer (mimalloc src/prim/wasi/prim.c) holds this
// across its probe+grow pair; the sbrk it calls in between re-enters it.
extern "C" void __sprt_wasm_grow_lock(void) __SPRT_NOEXCEPT { __wasm_grow_lock(); }

extern "C" void __sprt_wasm_grow_unlock(void) __SPRT_NOEXCEPT { __wasm_grow_unlock(); }

extern "C" int brk(void *__addr) __SPRT_NOEXCEPT {
	__wasm_grow_lock();
	const int rc = __wasm_brk_unlocked(__addr);
	__wasm_grow_unlock();
	return rc;
}

extern "C" void *sbrk(__INTPTR_TYPE__ __incr) __SPRT_NOEXCEPT {
	__wasm_grow_lock();
	if (s_wasm_break == 0) {
		s_wasm_break = __wasm_break_base();
	}
	const __wasm_uptr old = s_wasm_break;
	if (__incr == 0) {
		__wasm_grow_unlock();
		return reinterpret_cast<void *>(old);
	}
	const __wasm_uptr want = old + static_cast<__wasm_uptr>(__incr);
	// Overflow / underflow guard in either direction.
	if ((__incr > 0 && want < old) || (__incr < 0 && want > old)) {
		__sprt_errno = ENOMEM;
		__wasm_grow_unlock();
		return reinterpret_cast<void *>(static_cast<__INTPTR_TYPE__>(-1));
	}
	if (__wasm_brk_unlocked(reinterpret_cast<void *>(want)) != 0) {
		__wasm_grow_unlock();
		return reinterpret_cast<void *>(static_cast<__INTPTR_TYPE__>(-1));
	}
	__wasm_grow_unlock();
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

extern "C" int gethostname(char *name, size_t len) __SPRT_NOEXCEPT {
	static const char host[] = "localhost";
	if (!name || len == 0) {
		__sprt_errno = EINVAL;
		return -1;
	}
	// Truncate to len - 1 chars so the NUL terminator never eats a copied char.
	size_t n = sizeof(host) - 1;
	if (n > len - 1) {
		n = len - 1;
	}
	for (size_t i = 0; i < n; i++) {
		name[i] = host[i];
	}
	name[n] = 0;
	return 0;
}

extern "C" int utimes(const char *, const struct __SPRT_TIMEVAL_NAME[2]) __SPRT_NOEXCEPT {
	return 0;
}

extern "C" int fchown(int, __SPRT_ID(uid_t), __SPRT_ID(gid_t)) __SPRT_NOEXCEPT { return 0; }

// fsync/fdatasync live in the memfs TU (wasm/libc_path.cc) where the inode + OPFS
// backend are in scope: an OPFS-backed file must be written back on fsync.
