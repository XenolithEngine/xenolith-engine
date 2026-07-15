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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_UCHAR_H_
#define CORE_RUNTIME_INCLUDE_LIBC_UCHAR_H_

/*
	Dispatch header for <uchar.h>:
	- hosted SPRT build -> forwards to the system <uchar.h> (#include_next)
	- otherwise         -> SPRT's own declarations (defined inline below)

	Types:
	  char16_t  - UTF-16 code unit (a C keyword in C++; uint_least16_t in C)
	  char32_t  - UTF-32 code unit (a C keyword in C++; uint_least32_t in C)
	  mbstate_t - conversion state, size_t

	Functions:
	  mbrtoc16 - UTF-8 -> UTF-16 (one code unit; surrogate pairs span two calls)
	  c16rtomb - UTF-16 -> UTF-8 (combines surrogate pairs across calls)
	  mbrtoc32 - UTF-8 -> UTF-32 (one code point per call)
	  c32rtomb - UTF-32 -> UTF-8
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <uchar.h>

#else

#include <sprt/c/__sprt_wchar.h>
#include <sprt/c/bits/__sprt_null.h>

#ifndef __cplusplus
typedef __SPRT_ID(char16_t) char16_t;
typedef __SPRT_ID(char32_t) char32_t;
#endif

// C mandates that <uchar.h> declare size_t (and mbstate_t) — the mbrtoc*/c*rtomb
// signatures use it. Match <wchar.h>'s approach (an identical typedef is allowed to
// repeat); without it a C++ TU including <cuchar> sees only std::size_t, not global.
typedef __SPRT_ID(size_t) size_t;

#if !__SPRT_MBSTATE_DIRECT
typedef __SPRT_MBSTATE_NAME mbstate_t;
#endif

#ifndef NULL
#define NULL __SPRT_NULL
#endif

__SPRT_BEGIN_DECL

SPRT_UMBRELLA_FUNC
__SPRT_ID(size_t) mbrtoc16(char16_t *__SPRT_RESTRICT __pc16, const char *__SPRT_RESTRICT __s,
		__SPRT_ID(size_t) __n, mbstate_t *__SPRT_RESTRICT __ps) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(mbrtoc16)(__pc16, __s, __n, __ps);
}
#endif

SPRT_UMBRELLA_FUNC
__SPRT_ID(size_t) c16rtomb(char *__SPRT_RESTRICT __s, char16_t __c16,
		mbstate_t *__SPRT_RESTRICT __ps) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(c16rtomb)(__s, __c16, __ps);
}
#endif

SPRT_UMBRELLA_FUNC
__SPRT_ID(size_t) mbrtoc32(char32_t *__SPRT_RESTRICT __pc32, const char *__SPRT_RESTRICT __s,
		__SPRT_ID(size_t) __n, mbstate_t *__SPRT_RESTRICT __ps) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(mbrtoc32)(__pc32, __s, __n, __ps);
}
#endif

SPRT_UMBRELLA_FUNC
__SPRT_ID(size_t) c32rtomb(char *__SPRT_RESTRICT __s, char32_t __c32,
		mbstate_t *__SPRT_RESTRICT __ps) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(c32rtomb)(__s, __c32, __ps);
}
#endif

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_UCHAR_H_
