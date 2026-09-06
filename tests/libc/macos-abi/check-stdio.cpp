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
// cross/macos_sprt/{stdio,setjmp}.h + <arch>_sprt/{jmp_buf,fenv}.h <-> Darwin.
//
// setvbuf() modes go to libSystem untranslated, and the buffering/limit values
// are what portable code sizes its buffers from. jmp_buf is the sharpest of the
// three: sprt's setjmp/longjmp save into a caller-provided buffer, so if
// __SPRT__JBLEN were smaller than Darwin's _JBLEN the register save would run
// off the end of the object -- and _JBLEN differs per architecture (x86_64 is
// (9*2)+3+16, arm64 is (14+8+2)*2), which is why this TU is compiled once per
// arch.
//
// Compile-time only; see check.sh.
// ---------------------------------------------------------------------------

#include <stdio.h>
#include <setjmp.h>
#include <fenv.h>

#define SPRT_ABI_HEADER <sprt/c/cross/__sprt_stdio.h>
#define SPRT_ABI_HEADER_2 <sprt/c/cross/__sprt_setjmp.h>
#define SPRT_ABI_HEADER_3 <sprt/c/cross/__sprt_fenv_t.h>
#include "abi_check.h"

// === stdio limits and setvbuf() modes ======================================
SPRT_CONST(BUFSIZ);
SPRT_CONST(FOPEN_MAX);
SPRT_CONST(FILENAME_MAX);
SPRT_CONST(L_tmpnam);
SPRT_CONST(TMP_MAX);
// Darwin spells the setvbuf modes with a leading underscore.
SPRT_CONST_MAP(IOFBF, _IOFBF);
SPRT_CONST_MAP(IOLBF, _IOLBF);
SPRT_CONST_MAP(IONBF, _IONBF);

// === jmp_buf ===============================================================
SPRT_CONST_MAP(_JBLEN, _JBLEN);
SPRT_SIZE(__sprt_native_jmp_buf, jmp_buf);
SPRT_SIZE(__sprt_native_sigjmp_buf, sigjmp_buf);

// === fenv ==================================================================
SPRT_CONST(FE_INEXACT);
SPRT_CONST(FE_UNDERFLOW);
SPRT_CONST(FE_OVERFLOW);
SPRT_CONST(FE_DIVBYZERO);
SPRT_CONST(FE_INVALID);
SPRT_CONST(FE_ALL_EXCEPT);
SPRT_CONST(FE_TONEAREST);
SPRT_CONST(FE_UPWARD);
SPRT_CONST(FE_DOWNWARD);
SPRT_CONST(FE_TOWARDZERO);
