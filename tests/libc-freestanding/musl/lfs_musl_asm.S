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

/* musl's assembly string functions under __lfs_musl_asm_* names. lfs_impls.cpp
 * reads LFS_MUSL_ASM_* from the same predefines to know which exist. */

#define memcpy __lfs_musl_asm_memcpy
#define memmove __lfs_musl_asm_memmove
#define memset __lfs_musl_asm_memset
#define __memcpy_fwd __lfs_musl_asm___memcpy_fwd

#if defined(__x86_64__)
#include "../../../runtime/musl-libc/src/string/x86_64/memcpy.s"
#include "../../../runtime/musl-libc/src/string/x86_64/memmove.s"
#include "../../../runtime/musl-libc/src/string/x86_64/memset.s"
#elif defined(__aarch64__)
#include "../../../runtime/musl-libc/src/string/aarch64/memcpy.S"
#include "../../../runtime/musl-libc/src/string/aarch64/memset.S"
#endif

#if defined(__linux__) && defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif
