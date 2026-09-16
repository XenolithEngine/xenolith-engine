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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_ASSERT_H_
#define CORE_RUNTIME_INCLUDE_LIBC_ASSERT_H_

/*
	Dispatch header for <assert.h>:
	- hosted SPRT build -> forwards to the system <assert.h> (#include_next)
	- otherwise         -> SPRT's own definition via sprt/wrappers/libc/assert.h

	This header declares only the assert macro (no types or functions):

	  assert(expr) - if NDEBUG is defined, expands to a no-op void expression;
	                 otherwise evaluates expr and, when it is false, calls the
	                 failure handler with the stringized expression, __FILE__,
	                 __LINE__ and the enclosing function name (which aborts).

	Note: standard <assert.h> is meant to be re-includable so NDEBUG can toggle assert
	per translation unit, but this wrapper guards with #ifndef assert, so the first
	include's definition wins for the rest of the unit.
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <assert.h>

#else

#include <sprt/wrappers/libc/assert.h>

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_ASSERT_H_
