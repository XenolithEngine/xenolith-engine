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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_LIMITS_H_
#define CORE_RUNTIME_INCLUDE_LIBC_LIMITS_H_

/*
	Dispatch header for <limits.h>:
	- hosted SPRT build -> forwards to the system <limits.h> (#include_next)
	- otherwise         -> SPRT's own definitions via sprt/wrappers/libc/limits.h,
	                       which also pulls in the <inttypes.h> wrapper

	This header declares only macros (no types or functions):

	ISO C fundamental-type limits:
	  CHAR_BIT  - bits in a char
	  MB_LEN_MAX - max bytes in a multibyte character
	  CHAR_MIN/CHAR_MAX, SCHAR_MIN/SCHAR_MAX, UCHAR_MAX
	  SHRT_MIN/SHRT_MAX, USHRT_MAX
	  INT_MIN/INT_MAX, UINT_MAX
	  LONG_MIN/LONG_MAX, ULONG_MAX
	  LLONG_MIN/LLONG_MAX, ULLONG_MAX
	  WORD_BIT  - bits in an int (XSI)

	POSIX limits:
	  PATH_MAX (unless an MSVC-compat build already provides it), NAME_MAX,
	  PIPE_BUF, FILESIZEBITS, NGROUPS_MAX, ARG_MAX, IOV_MAX, SYMLOOP_MAX,
	  SSIZE_MAX, TZNAME_MAX, TTY_NAME_MAX, HOST_NAME_MAX
	  NL_ARGMAX, NL_MSGMAX, NL_SETMAX, NL_TEXTMAX - message-catalog limits
	  SIZEOF_OFF_T - size in bytes of off_t (non-standard, defined as 8)

	Windows CRT-compatibility limits:
	  _I8_MIN/_I8_MAX/_UI8_MAX, _I16_MIN/_I16_MAX/_UI16_MAX,
	  _I32_MIN/_I32_MAX/_UI32_MAX, _I64_MIN/_I64_MAX/_UI64_MAX
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <limits.h>

#else

#include <sprt/wrappers/libc/limits.h>

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_LIMITS_H_
