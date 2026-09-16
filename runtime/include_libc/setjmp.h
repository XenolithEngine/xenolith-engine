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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SETJMP_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SETJMP_H_

/*
	Dispatch header for <setjmp.h>:
	- hosted SPRT build -> forwards to the system <setjmp.h> (#include_next)
	- otherwise         -> SPRT's own definitions via sprt/c/__sprt_setjmp.h

	On the SPRT-own path the standard names are macro aliases of the SPRT internals:

	Type:
	  jmp_buf - array type holding a saved calling environment

	Macros / functions:
	  setjmp(env)        - save the current environment into env; returns 0 on the
	                       direct call and non-zero when reached via longjmp
	  longjmp(env, val)  - restore an environment saved by setjmp, making that
	                       setjmp return val (a 0 val is reported as 1)

	Note: the sigsetjmp/siglongjmp (signal-mask-saving) variants are not provided here.
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <setjmp.h>

#else

#include <sprt/c/__sprt_setjmp.h>

__SPRT_BEGIN_DECL

#define sigjmp_buf __SPRT_ID(sigjmp_buf)
#define sigsetjmp __SPRT_ID(sigsetjmp)
#define siglongjmp __SPRT_ID(siglongjmp)

#define jmp_buf __SPRT_ID(jmp_buf)
#define setjmp __SPRT_ID(setjmp)
#define longjmp __SPRT_ID(longjmp)

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SETJMP_H_
