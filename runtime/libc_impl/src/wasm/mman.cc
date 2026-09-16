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

// WebAssembly memory-management backend (wasm-port-draft.adoc §3.4).
//
// wasm linear memory has no page protection, locking or residency notion, so the
// protection/advice/lock/residency calls are success-noops (or ENOSYS where a
// truthful answer is impossible). The real anonymous mapping allocator
// (__file_mmap_anon / __file_munmap_anon, over memory.grow) lives in
// wasm/libc_file_ops.cc. This unit also supplies the shared includes the generic
// builtin_mman.cpp body needs (__impl_libc.h, errno, containers), exactly like
// windows/mman.cc does on Windows.

#ifndef __SPRT_BUILD
#define __SPRT_BUILD
#endif

#include <sprt/c/sys/__sprt_mman.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/cxx/mutex>
#include <sprt/cxx/unordered_map>

#include "../../include/__impl_libc.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-function"
#endif

namespace sprt {

// No page protection in wasm: report success without doing anything.
__SPRT_C_FUNC int mprotect(void *addr, size_t len, int prot) __SPRT_NOEXCEPT { return 0; }

__SPRT_C_FUNC int posix_madvise(void *addr, size_t len, int advice) __SPRT_NOEXCEPT { return 0; }

// No paging: locking always "succeeds" (the whole heap is resident).
__SPRT_C_FUNC int mlock(const void *addr, size_t len) __SPRT_NOEXCEPT { return 0; }

__SPRT_C_FUNC int munlock(const void *addr, size_t len) __SPRT_NOEXCEPT { return 0; }

__SPRT_C_FUNC int mlock2(const void *addr, size_t len, int __flags) __SPRT_NOEXCEPT { return 0; }

__SPRT_C_FUNC int madvise(void *addr, size_t len, int advice) __SPRT_NOEXCEPT { return 0; }

// Residency cannot be queried; report the pages as resident (all-ones) is
// misleading, so fail explicitly instead.
__SPRT_C_FUNC int mincore(void *addr, size_t length, unsigned char *vec) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return -1;
}

} // namespace sprt
