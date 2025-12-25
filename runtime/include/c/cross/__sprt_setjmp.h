/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_SETJMP_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_SETJMP_H_

#include <c/bits/__sprt_def.h>

#if SPRT_LINUX

#if defined(__x86_64__) || defined(_M_X64)
typedef unsigned long __SPRT_ID(__jmp_buf)[8];
#elif defined(__aarch64__) || defined(_M_ARM64)
typedef unsigned long __SPRT_ID(__jmp_buf)[22];
#else
#error "Unknown Linux arch"
#endif

typedef struct __SPRT_ID(__jmp_buf_tag) {
	__SPRT_ID(__jmp_buf) __jb;
	unsigned long __fl;
	unsigned long __ss[128 / sizeof(long)];
} __SPRT_ID(jmp_buf)[1];


#elif SPRT_WINDOWS

// Definitions specific to particular setjmp implementations.
#if defined _M_IX86

#define __SPRT__JBLEN  16
#define __SPRT__JBTYPE int

#elif defined _M_X64

typedef struct SPRT_ALIGNAS(16) {
	unsigned long long Part[2];
} __SPRT__JBTYPE;

#define __SPRT__JBLEN  16

#elif defined _M_ARM

#define __SPRT__JBLEN  28
#define __SPRT__JBTYPE int

#elif defined _M_ARM64

#define __SPRT__JBLEN  24
#define __SPRT__JBTYPE unsigned __int64

#else
#error "Unknown Windows arch"
#endif

typedef __SPRT__JBTYPE __SPRT_ID(jmp_buf)[__SPRT__JBLEN];

#elif SPRT_ANDROID

#if defined(__aarch64__)
#define __SPRT__JBLEN 32
#elif defined(__arm__)
#define __SPRT__JBLEN 64
#elif defined(__i386__)
#define __SPRT__JBLEN 10
#elif defined(__riscv)
#define __SPRT__JBLEN 64
#elif defined(__x86_64__)
#define __SPRT__JBLEN 11
#else
#error "Unknown Android arche"
#endif

/** The type of the buffer used by setjmp()/longjmp(). */
typedef long __SPRT_ID(jmp_buf)[__SPRT__JBLEN];

#elif SPRT_MACOS

#error "Unknown OS"

#else
#error "Unknown OS"
#endif


#endif
