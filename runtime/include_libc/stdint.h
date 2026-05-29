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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_STDINT_H_
#define CORE_RUNTIME_INCLUDE_LIBC_STDINT_H_

/*
	Dispatch header for <stdint.h>:
	- hosted SPRT build -> forwards to the system <stdint.h> (#include_next)
	- otherwise         -> SPRT's own definitions via sprt/wrappers/libc/stdint.h

	This header declares only types and macros (no functions). In C++ the types are
	defined in namespace sprt::_cstdint and pulled into the global scope in
	freestanding / non-SPRT-build mode.

	Types:
	  exact-width:        int8_t..int64_t, uint8_t..uint64_t
	  minimum-width:      int_least8_t..int_least64_t, uint_least8_t..uint_least64_t
	  fastest min-width:  int_fast8_t..int_fast64_t, uint_fast8_t..uint_fast64_t
	  greatest-width:     intmax_t, uintmax_t
	  pointer-holding:    intptr_t, uintptr_t
	  (in C++ the sizes are checked with static_assert)

	Constant-builder macros (append the right suffix to an integer literal):
	  INT8_C/INT16_C/INT32_C/INT64_C, UINT8_C/UINT16_C/UINT32_C/UINT64_C

	Limit macros:
	  exact-width:  INT8_MAX..INT64_MAX, INT8_MIN..INT64_MIN, UINT8_MAX..UINT64_MAX
	  greatest:     INTMAX_MIN/INTMAX_MAX, UINTMAX_MAX
	  pointer:      INTPTR_MIN/INTPTR_MAX, UINTPTR_MAX
	  other types:  PTRDIFF_MIN/PTRDIFF_MAX, SIZE_MAX, WCHAR_MIN/WCHAR_MAX,
	                WINT_MIN/WINT_MAX, SIG_ATOMIC_MIN/SIG_ATOMIC_MAX
	  non-standard: UINT8_MIN..UINT64_MIN (SPRT additions, all defined as 0)
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <stdint.h>

#else

#include <sprt/wrappers/libc/stdint.h>

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_STDINT_H_
