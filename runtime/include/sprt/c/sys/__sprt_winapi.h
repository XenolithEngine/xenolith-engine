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

#ifndef CORE_RUNTIME_INCLUDE_C_SYS___SPRT_WINAPI_H_
#define CORE_RUNTIME_INCLUDE_C_SYS___SPRT_WINAPI_H_

#include <sprt/c/cross/__sprt_config.h>
#include <sprt/c/cross/__sprt_sysid.h>
#include <sprt/c/bits/__sprt_uint32_t.h>
#include <sprt/c/bits/__sprt_size_t.h>

/*
	This defines some WinAPI umbrella methods available for use in the Stappler Runtime

	Methods are defined in libc style, without WinAPI-specific variable types.
*/

// clang-format off
#define __SPRT_WINAPI_INFINITE 0xFFFFFFFF
// clang-format on

#if __SPRT_CONFIG_HAVE_WINAPI || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS

__SPRT_BEGIN_DECL

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API __SPRT_ID(pid_t) __SPRT_ID(GetCurrentThreadId)();

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API __SPRT_ID(uint32_t) __SPRT_ID(GetLastError)();

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API int __SPRT_ID(WaitOnAddress)(volatile void *Address, void *CompareAddress,
		__SPRT_ID(size_t) AddressSize, __SPRT_ID(uint32_t) dwMilliseconds);

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API void __SPRT_ID(WakeByAddressSingle)(void *Address);

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API void __SPRT_ID(WakeByAddressAll)(void *Address);

// clang-format off
#define __SPRT_LOCALE_NAME_USER_DEFAULT            nullptr
#define __SPRT_LOCALE_NAME_INVARIANT               u""
#define __SPRT_LOCALE_NAME_SYSTEM_DEFAULT          u"!x-sys-default-locale"

#define __SPRT_LCMAP_LOWERCASE           0x00000100  // lower case letters
#define __SPRT_LCMAP_UPPERCASE           0x00000200  // UPPER CASE LETTERS
#define __SPRT_LCMAP_TITLECASE           0x00000300  // Title Case Letters
#define __SPRT_LCMAP_SORTKEY             0x00000400  // WC sort key (normalize)
#define __SPRT_LCMAP_BYTEREV             0x00000800  // byte reversal
#define __SPRT_LCMAP_HIRAGANA            0x00100000  // map katakana to hiragana
#define __SPRT_LCMAP_KATAKANA            0x00200000  // map hiragana to katakana
#define __SPRT_LCMAP_HALFWIDTH           0x00400000  // map double byte to single byte
#define __SPRT_LCMAP_FULLWIDTH           0x00800000  // map single byte to double byte
#define __SPRT_LCMAP_LINGUISTIC_CASING   0x01000000  // use linguistic rules for casing
#define __SPRT_LCMAP_SIMPLIFIED_CHINESE  0x02000000  // map traditional chinese to simplified chinese
#define __SPRT_LCMAP_TRADITIONAL_CHINESE 0x04000000  // map simplified chinese to traditional chinese

#define __SPRT_NORM_IGNORECASE           0x00000001  // ignore case
#define __SPRT_NORM_IGNORENONSPACE       0x00000002  // ignore nonspacing chars
#define __SPRT_NORM_IGNORESYMBOLS        0x00000004  // ignore symbols

#define __SPRT_LINGUISTIC_IGNORECASE     0x00000010  // linguistically appropriate 'ignore case'
#define __SPRT_LINGUISTIC_IGNOREDIACRITIC 0x00000020  // linguistically appropriate 'ignore nonspace'

#define __SPRT_NORM_IGNOREKANATYPE       0x00010000  // ignore kanatype
#define __SPRT_NORM_IGNOREWIDTH          0x00020000  // ignore width
#define __SPRT_NORM_LINGUISTIC_CASING    0x08000000  // use linguistic rules for casing

#define __SPRT_SORT_STRINGSORT           0x00001000  // use string sort method
#define __SPRT_SORT_DIGITSASNUMBERS      0x00000008  // use digits as numbers sort method

#define __SPRT_CSTR_LESS_THAN            1           // string 1 less than string 2
#define __SPRT_CSTR_EQUAL                2           // string 1 equal to string 2
#define __SPRT_CSTR_GREATER_THAN         3           // string 1 greater than string 2
// clang-format on

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API int __SPRT_ID(_LCMapString)(const char16_t *lpLocaleName, __SPRT_ID(uint32_t) dwMapFlags,
		const char16_t *lpSrcStr, int cchSrc, char16_t *lpDestStr, int cchDest);

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API int __SPRT_ID(_CompareString)(const char16_t *lpLocaleName, __SPRT_ID(uint32_t) dwMapFlags,
		const char16_t *lpString1, int cchCount1, const char16_t *lpString2, int cchCount2);

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API int __SPRT_ID(_GetUserDefaultLocaleName)(char16_t *wlocale, int bufSize);


#define __SPRT_IDN_ALLOW_UNASSIGNED        0x01  // Allow unassigned "query" behavior per RFC 3454
#define __SPRT_IDN_USE_STD3_ASCII_RULES    0x02  // Enforce STD3 ASCII restrictions for legal characters
#define __SPRT_IDN_EMAIL_ADDRESS           0x04  // Enable EAI algorithmic fallback for email local parts behavior
#define __SPRT_IDN_RAW_PUNYCODE            0x08  // Disable validation and mapping of punycode.

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API int __SPRT_ID(_IdnToAscii)(__SPRT_ID(uint32_t) dwFlags, const char16_t *lpUnicodeCharStr,
		int cchUnicodeChar, char16_t *lpASCIICharStr, int cchASCIIChar);

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API int __SPRT_ID(_IdnToUnicode)(__SPRT_ID(uint32_t) dwFlags, const char16_t *lpUnicodeCharStr,
		int cchUnicodeChar, char16_t *lpASCIICharStr, int cchASCIIChar);

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API __SPRT_ID(uint32_t)
		__SPRT_ID(_GetCurrentDirectory)(__SPRT_ID(uint32_t) nBufferLength, char16_t *lpBuffer);

__SPRT_END_DECL

#endif

#endif
