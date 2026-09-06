// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// ---------------------------------------------------------------------------
// sys/__sprt_mman.h + cross/macos_sprt/mman.h <-> Darwin <sys/mman.h> parity.
//
// mmap()/mprotect()/madvise() arguments reach libSystem unmodified. MAP_ANON is
// 0x1000 here against Linux's 0x20, which is the kind of divergence that turns
// an anonymous mapping into a file mapping of fd 0x20 if it drifts.
//
// Compile-time only; see check.sh.
// ---------------------------------------------------------------------------

#include <sys/mman.h>

#define SPRT_ABI_HEADER <sprt/c/sys/__sprt_mman.h>
#include "abi_check.h"

// === protection bits =======================================================
SPRT_CONST(PROT_NONE);
SPRT_CONST(PROT_READ);
SPRT_CONST(PROT_WRITE);
SPRT_CONST(PROT_EXEC);

// === mmap() flags ==========================================================
SPRT_CONST(MAP_SHARED);
SPRT_CONST(MAP_PRIVATE);
SPRT_CONST(MAP_FIXED);
SPRT_CONST(MAP_ANON);
SPRT_CONST(MAP_ANONYMOUS);
SPRT_CONST(MAP_NORESERVE);

// === msync() / madvise() ===================================================
SPRT_CONST(MS_ASYNC);
SPRT_CONST(MS_SYNC);
SPRT_CONST(MS_INVALIDATE);
SPRT_CONST(MADV_NORMAL);
SPRT_CONST(MADV_RANDOM);
SPRT_CONST(MADV_SEQUENTIAL);
SPRT_CONST(MADV_WILLNEED);
SPRT_CONST(MADV_DONTNEED);
SPRT_CONST(MADV_FREE);
