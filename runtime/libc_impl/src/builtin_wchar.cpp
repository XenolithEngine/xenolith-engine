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
#include "stdio.h"

#if SPRT_WINDOWS
#include "windows/wchar.cc"
#endif

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


__SPRT_C_FUNC int wcscpy_s(wchar_t *dest, size_t dest_size, const wchar_t *src) {
	if (!dest || !src || dest_size == 0 || dest_size > __SPRT_RSIZE_MAX) {
		__sprt_errno = EINVAL;
		return EINVAL;
	}

	auto len = sprt::strlen(src);
	if (dest_size < len + 1) {
		__sprt_errno = ERANGE;
		return ERANGE;
	}

	wcscpy(dest, src);
	dest[len] = 0;
	return 0;
}

__SPRT_C_FUNC int wcsncpy_s(wchar_t *dest, size_t numberOfElements, const wchar_t *src,
		size_t count) {
	if (!dest || !src || numberOfElements == 0 || numberOfElements > __SPRT_RSIZE_MAX) {
		__sprt_errno = EINVAL;
		return EINVAL;
	}

	auto len = wcsnlen(src, count);
	if (count == ((size_t)-1)) { // _TRUNCATE
		len = sprt::min(len, numberOfElements - 1);
		wcsncpy(dest, src, len);
		dest[len] = 0;
		return 0;
	} else if (numberOfElements < len + 1) {
		dest[0] = 0;
		__sprt_errno = ERANGE;
		return ERANGE;
	}

	wcsncpy(dest, src, len);
	dest[len] = 0;
	return 0;
}

__SPRT_C_FUNC int wcstombs_s(size_t *pReturnValue, char *mbstr, size_t sizeInBytes,
		const wchar_t *wcstr, size_t count) {
	if (!mbstr && sizeInBytes > 0) {
		__sprt_errno = EINVAL;
		return EINVAL;
	}
	if (!wcstr) {
		__sprt_errno = EINVAL;
		return EINVAL;
	}

	auto ssize = wcslen(wcstr);

	if (count != ((size_t)-1)) { // _TRUNCATE
		auto maxDSize = wcstombs(nullptr, wcstr, 0);
		if (maxDSize == (size_t)-1 || maxDSize == (size_t)-2) {
			return __sprt_errno;
		}

		if (sizeInBytes < maxDSize + 1) {
			__sprt_errno = ERANGE;
			return ERANGE;
		}
	}

	mbstate_t state{0, 0};
	auto ret = wcsnrtombs(mbstr, &wcstr, ssize, sizeInBytes - 1, &state);
	if (ret >= 0) {
		mbstr[ret] = 0;
		if (pReturnValue) {
			*pReturnValue = ret;
		}
		return 0;
	}
	return __sprt_errno;
}


} // namespace sprt
