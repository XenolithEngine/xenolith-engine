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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_SETJMP_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_SETJMP_H_

#include <sprt/c/bits/__sprt_uintptr_t.h>
#include <sprt/c/cross/__sprt_setjmp.h>

/*
	SPRT guarantees that C++ destructors (and any others,
	including Rust and Go) will be called correctly during
	longjmp. However, this requires some extra space for a buffer.

	Note, that this behavior is possible only if function, that called
	setjmp remains on stack, which is a general requirement for setjmp.

	It's also used in SPRT's pthread implementation, so, pthread_exit/pthread_cancel should be safe
	for a threads, that created with SPRT's pthread_create and C++ sprt::thread

	OS support:
	* Windows: *-pc-windows-msvc with -fexceptions (no unwinding possible when exceptions disabled)
	* Linux/Android - uses _Unwind_ForcedUnwind
	* MacOS - uses _Unwind_ForcedUnwind
*/

// We should not spawn extra stack frame, so we acquire native setjmp pointer,
// then call it within caller context, and then call CFA lookup with native result

#if defined(_WIN32) || defined(__SPRT_WINDOWS)

// Windows ABI setjmp have 2 arguments,
#define __sprt_setjmp(Buf) __sprt_cfa_setjmp(__sprt_get_setjmp_fn()( ((Buf)[0].__native ), __SPRT_NULL), Buf)

typedef int (*__SPRT_ID(setjmp_fn))(__SPRT_ID(native_jmp_buf), void *);

#else

#define __sprt_setjmp(Buf) __sprt_cfa_setjmp(__sprt_get_setjmp_fn()( ((Buf)[0].__native) ), Buf)

typedef int (*__SPRT_ID(setjmp_fn))(__SPRT_ID(native_jmp_buf));

#endif

__SPRT_BEGIN_DECL

typedef struct __SPRT_ID(__ext_jmp_buf) {
	__SPRT_ID(native_jmp_buf) __native; // OS-defined jmp_buf
	__SPRT_ID(uintptr_t) __cfa; // Canonical Frame Address for stack unwinder
	int __result; // We need to store result value from longjmp during stack unwind
} __SPRT_ID(jmp_buf)[1];

// Performs longjmp
SPRT_API __SPRT_NORETURN void __SPRT_ID(longjmp)(__SPRT_ID(jmp_buf), int);

// Returns native setjmp function pointer
SPRT_API __SPRT_ID(setjmp_fn) __SPRT_ID(get_setjmp_fn)();

// Writes CFA vaule to jmpbuf for stack unwinding, if arg is 0, and returns 0
// Returns arg immediately if it is not 0
SPRT_API int __SPRT_ID(cfa_setjmp)(int arg, __SPRT_ID(jmp_buf));

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_SETJMP_H_
