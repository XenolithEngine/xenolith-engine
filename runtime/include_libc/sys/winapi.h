/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SYS_WINAPI_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_WINAPI_H_

#include <sprt/c/sys/__sprt_winapi.h>

#if __SPRT_CONFIG_HAVE_WINAPI || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS

#define _WINAPI_INFINITE __SPRT_WINAPI_INFINITE

__SPRT_BEGIN_DECL

SPRT_FORCEINLINE __SPRT_ID(pid_t) _GetCurrentThreadId() { return __sprt_GetCurrentThreadId(); }

SPRT_FORCEINLINE __SPRT_ID(uint32_t) _GetLastError() { return __sprt_GetLastError(); }

SPRT_FORCEINLINE int _WaitOnAddress(volatile void *Address, void *CompareAddress,
		__SPRT_ID(size_t) AddressSize, __SPRT_ID(uint32_t) dwMilliseconds) {
	return __sprt_WaitOnAddress(Address, CompareAddress, AddressSize, dwMilliseconds);
}

SPRT_FORCEINLINE void _WakeByAddressSingle(void *Address) { __sprt_WakeByAddressSingle(Address); }

SPRT_FORCEINLINE void _WakeByAddressAll(void *Address) { __sprt_WakeByAddressAll(Address); }


#define LOCALE_NAME_USER_DEFAULT __SPRT_LOCALE_NAME_USER_DEFAULT
#define LOCALE_NAME_INVARIANT __SPRT_LOCALE_NAME_INVARIANT
#define LOCALE_NAME_SYSTEM_DEFAULT __SPRT_LOCALE_NAME_SYSTEM_DEFAULT

#define LCMAP_LOWERCASE __SPRT_LCMAP_LOWERCASE
#define LCMAP_UPPERCASE __SPRT_LCMAP_UPPERCASE
#define LCMAP_TITLECASE __SPRT_LCMAP_TITLECASE
#define LCMAP_SORTKEY __SPRT_LCMAP_SORTKEY
#define LCMAP_BYTEREV __SPRT_LCMAP_BYTEREV
#define LCMAP_HIRAGANA __SPRT_LCMAP_HIRAGANA
#define LCMAP_KATAKANA __SPRT_LCMAP_KATAKANA
#define LCMAP_HALFWIDTH __SPRT_LCMAP_HALFWIDTH
#define LCMAP_FULLWIDTH __SPRT_LCMAP_FULLWIDTH
#define LCMAP_LINGUISTIC_CASING __SPRT_LCMAP_LINGUISTIC_CASING
#define LCMAP_SIMPLIFIED_CHINESE __SPRT_LCMAP_SIMPLIFIED_CHINESE
#define LCMAP_TRADITIONAL_CHINESE __SPRT_LCMAP_TRADITIONAL_CHINESE

#define NORM_IGNORECASE __SPRT_NORM_IGNORECASE
#define NORM_IGNORENONSPACE __SPRT_NORM_IGNORENONSPACE
#define NORM_IGNORESYMBOLS __SPRT_NORM_IGNORESYMBOLS

#define LINGUISTIC_IGNORECASE __SPRT_LINGUISTIC_IGNORECASE
#define LINGUISTIC_IGNOREDIACRITIC __SPRT_LINGUISTIC_IGNOREDIACRITIC

#define NORM_IGNOREKANATYPE __SPRT_NORM_IGNOREKANATYPE
#define NORM_IGNOREWIDTH __SPRT_NORM_IGNOREWIDTH
#define NORM_LINGUISTIC_CASING __SPRT_NORM_LINGUISTIC_CASING

#define SORT_STRINGSORT __SPRT_SORT_STRINGSORT
#define SORT_DIGITSASNUMBERS __SPRT_SORT_DIGITSASNUMBERS

#define CSTR_LESS_THAN __SPRT_CSTR_LESS_THAN
#define CSTR_EQUAL __SPRT_CSTR_EQUAL
#define CSTR_GREATER_THAN __SPRT_CSTR_GREATER_THAN

SPRT_FORCEINLINE int _LCMapString(const char16_t *lpLocaleName, __SPRT_ID(uint32_t) dwMapFlags,
		const char16_t *lpSrcStr, int cchSrc, char16_t *lpDestStr, int cchDest) {
	return __sprt__LCMapString(lpLocaleName, dwMapFlags, lpSrcStr, cchSrc, lpDestStr, cchDest);
}

SPRT_FORCEINLINE int _CompareString(const char16_t *lpLocaleName, __SPRT_ID(uint32_t) dwMapFlags,
		const char16_t *lpString1, int cchCount1, const char16_t *lpString2, int cchCount2) {
	return __sprt__CompareString(lpLocaleName, dwMapFlags, lpString1, cchCount1, lpString2,
			cchCount2);
}

SPRT_FORCEINLINE int _GetUserDefaultLocaleName(char16_t *wlocale, int bufSize) {
	return __sprt__GetUserDefaultLocaleName(wlocale, bufSize);
}


#define IDN_ALLOW_UNASSIGNED __SPRT_IDN_ALLOW_UNASSIGNED
#define IDN_USE_STD3_ASCII_RULES __SPRT_IDN_USE_STD3_ASCII_RULES
#define IDN_EMAIL_ADDRESS __SPRT_IDN_EMAIL_ADDRESS
#define IDN_RAW_PUNYCODE __SPRT_IDN_RAW_PUNYCODE

SPRT_FORCEINLINE int _IdnToAscii(__SPRT_ID(uint32_t) dwFlags, const char16_t *lpUnicodeCharStr,
		int cchUnicodeChar, char16_t *lpASCIICharStr, int cchASCIIChar) {
	return __sprt__IdnToAscii(dwFlags, lpUnicodeCharStr, cchUnicodeChar, lpASCIICharStr,
			cchASCIIChar);
}

SPRT_FORCEINLINE int _IdnToUnicode(__SPRT_ID(uint32_t) dwFlags, const char16_t *lpUnicodeCharStr,
		int cchUnicodeChar, char16_t *lpASCIICharStr, int cchASCIIChar) {
	return __sprt__IdnToUnicode(dwFlags, lpUnicodeCharStr, cchUnicodeChar, lpASCIICharStr,
			cchASCIIChar);
}

SPRT_FORCEINLINE __SPRT_ID(uint32_t)
		_GetCurrentDirectory(__SPRT_ID(uint32_t) nBufferLength, char *lpBuffer) {
	return __sprt__GetCurrentDirectory(nBufferLength, lpBuffer);
}

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYS_WINAPI_H_
