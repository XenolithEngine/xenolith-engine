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

#ifndef CORE_RUNTIME_INCLUDE_C_LIBC_STDDEF_H_
#define CORE_RUNTIME_INCLUDE_C_LIBC_STDDEF_H_

/*
	Dispatch header for <stddef.h>:
	- hosted SPRT build -> forwards to the system <stddef.h> (#include_next)
	- otherwise         -> SPRT's own definitions via sprt/wrappers/libc/stddef.h,
	                       plus the stdint.h wrapper (the MSVC toolchain expects some
	                       stdint types to be reachable through <stddef.h>)

	This header declares only types and macros (no functions). In C++ the types are
	defined in namespace sprt::_cstddef and pulled into the global scope in
	freestanding / non-SPRT-build mode.

	Macros:
	  NULL                    - null pointer constant
	  offsetof(type, member)  - byte offset of a struct/union member

	Standard types:
	  size_t      - size / unsigned object-size type
	  ptrdiff_t   - signed result of subtracting two pointers
	  max_align_t - type with the largest fundamental alignment
	  wchar_t     - wide character type (in C; built-in in C++)
	  nullptr_t   - type of nullptr (C++ always; C only from C23)
	  byte        - C++-only enum class of bytes (sprt::byte)

	Extension types (non-ISO-stddef, surfaced here for convenience):
	  ssize_t        - signed size type (POSIX)
	  off_t/off64_t  - file offset type (off64_t is an LFS alias of off_t)
	  wint_t         - integer type holding any wide char plus WEOF
	  rsize_t        - bounds-checked size type (Annex K)
	  errno_t        - error-code type (Annex K)
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <stddef.h>

#else

#include <sprt/wrappers/libc/stddef.h>

// For MSVC build toolchain, some stdint.h types placed in stddef
#include <sprt/wrappers/libc/stdint.h>

#endif

#endif // CORE_RUNTIME_INCLUDE_C_LIBC_STDDEF_H_
