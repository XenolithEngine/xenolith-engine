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

#include <sprt/c/__sprt_locale.h>

#include "../include/__impl_libc.h"

#include "wchar.h"
#include "locale.h"
#include "stdlib.h"

namespace sprt {

static wchar_t __towlower(wchar_t c) {
	if (c >= L'A' && c <= L'Z') {
		return c + 32;
	}
	return c;
}

static wchar_t __towupper(wchar_t c) {
	if (c >= L'a' && c <= L'z') {
		return c - 32;
	}
	return c;
}

static int __wcscmp(const wchar_t *l, const wchar_t *r) {
	for (; *l == *r && *l && *r; l++, r++);
	return *l < *r ? -1 : *l > *r;
}

static int __wcsncmp(const wchar_t *l, const wchar_t *r, size_t n) {
	for (; n && *l == *r && *l && *r; n--, l++, r++);
	return n ? (*l < *r ? -1 : *l > *r) : 0;
}

static int __wcsncasecmp(const wchar_t *l, const wchar_t *r, size_t n) {
	if (!n--) {
		return 0;
	}
	for (; *l && *r && n && (*l == *r || __towlower(*l) == __towlower(*r)); l++, r++, n--);
	return __towlower(*l) - __towlower(*r);
}

static int __wcscasecmp(const wchar_t *l, const wchar_t *r) { return __wcsncasecmp(l, r, -1); }

extern "C" {

__sprt_wint_t towlower(__sprt_wint_t ch) __SPRT_NOEXCEPT {
	auto map = __get_effective_locale_map(__SPRT_LC_CTYPE);
	if (map == __get_default_locale()) {
		return __towlower(ch);
	}
	return __towlower_l(ch, map);
}

__sprt_wint_t towupper(__sprt_wint_t ch) __SPRT_NOEXCEPT {
	auto map = __get_effective_locale_map(__SPRT_LC_CTYPE);
	if (map == __get_default_locale()) {
		return __towupper(ch);
	}
	return __towupper_l(ch, map);
}

int wcscmp(const wchar_t *l, const wchar_t *r) __SPRT_NOEXCEPT {
	auto map = __get_effective_locale_map(__SPRT_LC_COLLATE);
	if (map == __get_default_locale()) {
		return __wcscmp(l, r);
	}
	return __wcscmp_l(l, r, map);
}

int wcsncmp(const wchar_t *l, const wchar_t *r, size_t n) __SPRT_NOEXCEPT {
	auto map = __get_effective_locale_map(__SPRT_LC_COLLATE);
	if (map == __get_default_locale()) {
		return __wcsncmp(l, r, n);
	}
	return __wcsncmp_l(l, r, n, map);
}

int wcscasecmp(const wchar_t *l, const wchar_t *r) __SPRT_NOEXCEPT {
	auto map = __get_effective_locale_map(__SPRT_LC_COLLATE);
	if (map == __get_default_locale()) {
		return __wcscasecmp(l, r);
	}
	return __wcscasecmp_l(l, r, map);
}

int wcsncasecmp(const wchar_t *l, const wchar_t *r, size_t n) __SPRT_NOEXCEPT {
	auto map = __get_effective_locale_map(__SPRT_LC_COLLATE);
	if (map == __get_default_locale()) {
		return __wcsncasecmp(l, r, n);
	}
	return __wcsncasecmp_l(l, r, n, map);
}

int wcscoll(const wchar_t *l, const wchar_t *r) __SPRT_NOEXCEPT {
	auto map = __get_effective_locale_map(__SPRT_LC_COLLATE);
	if (map == __get_default_locale()) {
		return wcscmp(l, r);
	}
	return __wcscoll_l(l, r, map);
}

size_t wcsxfrm(wchar_t *__restrict dest, const wchar_t *__restrict src, size_t n) __SPRT_NOEXCEPT {
	auto map = __get_effective_locale_map(__SPRT_LC_COLLATE);
	if (map == __get_default_locale()) {
		size_t l = wcslen(src);
		if (l < n) {
			wmemcpy(dest, src, l + 1);
		} else if (n) {
			wmemcpy(dest, src, n - 1);
			dest[n - 1] = 0;
		}
		return l;
	}
	return __wcsxfrm_l(dest, src, n, __get_effective_locale_map(__SPRT_LC_COLLATE));
}
}

__SPRT_C_FUNC wint_t btowc(int c) __SPRT_NOEXCEPT {
	if (c == __SPRT_EOF) {
		return WEOF;
	} else {
		int b = (unsigned char)c;
		return b < 128U ? b : (0xdfff & (signed char)(c));
	}
}

__SPRT_C_FUNC int wctob(wint_t c) __SPRT_NOEXCEPT {
	if (c < 128U) {
		return c;
	}
	if ((unsigned)(c)-0xdf80 < 0x80) {
		return (unsigned char)c;
	}
	return __SPRT_EOF;
}

} // namespace sprt
