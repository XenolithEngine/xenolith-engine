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

// Minimal Embox EL0 locale backend.
//
// Only the "C"/"POSIX"/"C.UTF-8" locale exists, and unlike the WebAssembly
// backend -- where a richer one is a question of wiring up Intl -- there is
// nothing on this platform to wire up to. Embox ships no locale data, and the
// ABI has no syscall that would report any. So this is not a first milestone
// standing in for something better; it is the whole locale story unless the
// application carries its own tables.
//
// The C locale is well defined and this implements it exactly: radix '.', no
// digit grouping, ASCII case mapping, and collation by code unit. The byte
// parsers (strtod/scanf) reach the radix through __get_numeric_radix, so they
// are correct rather than merely unlocalised.

#include "locale.h"
#include "../../include/__impl_libc.h"

#include <sprt/runtime/stringview.h>
#include <sprt/c/__sprt_langinfo.h>
#include <sprt/cxx/mutex>
#include <sprt/cxx/type_traits>
#include <sprt/cxx/algorithm>
#include <sprt/cxx/atomic>

namespace sprt {

static constexpr size_t EL0_LOCALE_NAME_MAX = 64;

struct __locale_map {
	wchar_t wname[EL0_LOCALE_NAME_MAX + 1];
	char name[EL0_LOCALE_NAME_MAX + 1];
	// LC_NUMERIC radix; always '.' in the C locale.
	char radix;
	// LC_NUMERIC grouping; always absent in the C locale.
	char thousands_sep;
	unsigned char grouping;
	// LC_CTYPE multibyte width (MB_CUR_MAX): 1 for single-byte "C"/"POSIX", 4 for
	// UTF-8. Per ISO C the program-default "C" locale is single-byte.
	unsigned char mb_cur_max;
};

static_assert(sprt::is_trivially_constructible_v<__locale_map>);

// Single-byte "C"/"POSIX" (MB_CUR_MAX==1) vs UTF-8 "C.UTF8" (MB_CUR_MAX==4). The
// program default stays UTF-8; s_localeMapC backs ONLY an explicit "C"/"POSIX"
// request, which ISO C defines as single-byte — see the windows sibling.
static __locale_map s_localeMapC;
static __locale_map s_localeMapCUtf8;
static __freestanding_locale_struct s_localeStructCUtf8;

void __init_locale() {
	__builtin_memcpy(s_localeMapC.name, "C", 2);
	__builtin_memcpy(s_localeMapC.wname, L"C", 2 * sizeof(wchar_t));
	s_localeMapC.radix = '.';
	s_localeMapC.thousands_sep = 0;
	s_localeMapC.grouping = 0;
	s_localeMapC.mb_cur_max = 1; // ISO C: the "C" locale is single-byte

	__builtin_memcpy(s_localeMapCUtf8.name, "C.UTF8", 7);
	__builtin_memcpy(s_localeMapCUtf8.wname, L"C.UTF8", 7 * sizeof(wchar_t));
	s_localeMapCUtf8.radix = '.';
	s_localeMapCUtf8.thousands_sep = 0;
	s_localeMapCUtf8.grouping = 0;
	s_localeMapCUtf8.mb_cur_max = 4; // UTF-8

	s_localeStructCUtf8 = __freestanding_locale_struct{
		__locale_struct{
			&s_localeMapCUtf8,
			&s_localeMapCUtf8,
			&s_localeMapCUtf8,
			&s_localeMapCUtf8,
			&s_localeMapCUtf8,
			&s_localeMapCUtf8,
		},
		1,
	};
}

__locale_map *__get_default_locale() { return &s_localeMapCUtf8; }

__freestanding_locale_struct *__get_default_locale_struct() { return &s_localeStructCUtf8; }

bool __locale_is_c(const __locale_map *m) {
	return m == nullptr || m == &s_localeMapC || m == &s_localeMapCUtf8;
}

// Only the C / C.UTF-8 locales exist; every other name is unavailable. Empty name
// ("" = native environment) maps to UTF-8, the modern default.
__locale_map *__get_locale(int cat, const char *localeName, size_t len) {
	if (!localeName) {
		return nullptr;
	}
	auto n = StringView(localeName, len);
	if (n == "C" || n == "POSIX") {
		return &s_localeMapC;
	}
	if (n == "C.UTF8" || n == "C.UTF-8" || localeName[0] == 0) {
		return &s_localeMapCUtf8;
	}
	return nullptr;
}

void __free_locale(__locale_map *map) {
	// The static C maps are never heap-allocated; anything else would have been.
	if (map && map != &s_localeMapC && map != &s_localeMapCUtf8) {
		sprt::__delete(map);
	}
}

extern "C" {

char *setlocale(int cat, const char *name) __SPRT_NOEXCEPT {
	if ((unsigned)cat > __SPRT_LC_ALL) {
		return nullptr;
	}
	if (name) {
		auto n = StringView(name);
		if (n == "C" || n == "POSIX") {
			return (char *)s_localeMapC.name;
		}
		if (!(n == "C.UTF8" || n == "C.UTF-8" || name[0] == 0)) {
			__sprt_errno = ENOENT;
			return nullptr;
		}
	}
	return (char *)s_localeMapCUtf8.name;
}

// C-locale lconv: '.' radix, everything else empty / CHAR_MAX-unset per POSIX.
static lconv s_cLconv;
static bool s_cLconvInit = false;

lconv *localeconv(void) __SPRT_NOEXCEPT {
	if (!s_cLconvInit) {
		sprt::memset(&s_cLconv, 0, sizeof(lconv));
		static char s_dot[] = ".";
		static char s_empty[] = "";
		s_cLconv.decimal_point = s_dot;
		s_cLconv.thousands_sep = s_empty;
		s_cLconv.grouping = s_empty;
		s_cLconv.int_curr_symbol = s_empty;
		s_cLconv.currency_symbol = s_empty;
		s_cLconv.mon_decimal_point = s_empty;
		s_cLconv.mon_thousands_sep = s_empty;
		s_cLconv.mon_grouping = s_empty;
		s_cLconv.positive_sign = s_empty;
		s_cLconv.negative_sign = s_empty;
		s_cLconv.int_frac_digits = (char)255;
		s_cLconv.frac_digits = (char)255;
		s_cLconv.p_cs_precedes = (char)255;
		s_cLconv.p_sep_by_space = (char)255;
		s_cLconv.n_cs_precedes = (char)255;
		s_cLconv.n_sep_by_space = (char)255;
		s_cLconv.p_sign_posn = (char)255;
		s_cLconv.n_sign_posn = (char)255;
		s_cLconvInit = true;
	}
	return &s_cLconv;
}

} // extern "C"

// --- collation / case mapping: C locale = ASCII case fold + code-unit compare ---

static size_t __el0_wcslen(const wchar_t *s) {
	size_t n = 0;
	while (s[n]) {
		++n;
	}
	return n;
}

wchar_t __towlower_l(wchar_t ch, const __locale_map *) {
	return (ch >= L'A' && ch <= L'Z') ? wchar_t(ch + (L'a' - L'A')) : ch;
}

wchar_t __towupper_l(wchar_t ch, const __locale_map *) {
	return (ch >= L'a' && ch <= L'z') ? wchar_t(ch - (L'a' - L'A')) : ch;
}

int __wcscmp_l(const wchar_t *l, const wchar_t *r, const __locale_map *) {
	for (; *l && *l == *r; ++l, ++r) { }
	return int(*l) - int(*r);
}

int __wcsncmp_l(const wchar_t *l, const wchar_t *r, size_t n, const __locale_map *) {
	if (n == 0) {
		return 0;
	}
	for (; n > 1 && *l && *l == *r; ++l, ++r, --n) { }
	return int(*l) - int(*r);
}

int __wcscasecmp_l(const wchar_t *l, const wchar_t *r, const __locale_map *loc) {
	wchar_t cl, cr;
	do {
		cl = __towlower_l(*l++, loc);
		cr = __towlower_l(*r++, loc);
	} while (cl && cl == cr);
	return int(cl) - int(cr);
}

int __wcsncasecmp_l(const wchar_t *l, const wchar_t *r, size_t n, const __locale_map *loc) {
	if (n == 0) {
		return 0;
	}
	wchar_t cl, cr;
	do {
		cl = __towlower_l(*l++, loc);
		cr = __towlower_l(*r++, loc);
	} while (--n && cl && cl == cr);
	return int(cl) - int(cr);
}

int __wcscoll_l(const wchar_t *l, const wchar_t *r, const __locale_map *loc) {
	// C locale collates by code unit.
	return __wcscmp_l(l, r, loc);
}

int __strcoll_l(const char *l, const char *r, const __locale_map *) {
	// Only the C/POSIX locale exists here, which collates by byte value — identical to
	// strcmp (the narrow analogue of __wcscoll_l above). Match the Windows backend's
	// null-operand tolerance.
	if (!l || !r) {
		return 0;
	}
	unsigned char cl, cr;
	do {
		cl = (unsigned char)*l++;
		cr = (unsigned char)*r++;
	} while (cl && cl == cr);
	return int(cl) - int(cr);
}

size_t __wcsxfrm_l(wchar_t *__restrict dest, const wchar_t *__restrict src, size_t destSize,
		const __locale_map *) {
	// Identity sort key: comparing the copies with wcscmp orders the same as the C
	// locale wcscoll would.
	size_t len = __el0_wcslen(src);
	if (dest && destSize > 0) {
		size_t cnt = sprt::min(len, destSize - 1);
		for (size_t i = 0; i < cnt; ++i) {
			dest[i] = src[i];
		}
		dest[cnt] = 0;
	}
	return len;
}

size_t __strxfrm_l(char *__restrict dest, const char *__restrict src, size_t destSize,
		const __locale_map *) {
	size_t len = 0;
	while (src[len]) {
		++len;
	}
	if (dest && destSize > 0) {
		size_t cnt = sprt::min(len, destSize - 1);
		for (size_t i = 0; i < cnt; ++i) {
			dest[i] = src[i];
		}
		dest[cnt] = 0;
	}
	return len;
}

char __get_numeric_radix(const __locale_map *) { return '.'; }

__numeric_fmt __get_numeric_fmt(const __locale_map *) { return __numeric_fmt{'.', 0, 0}; }

// The C/POSIX langinfo table lives in runtime_core's weak fallback; reuse it.
char *__nl_langinfo_default(__sprt_nl_item item);

extern "C" {

char *nl_langinfo_l(__sprt_nl_item item, __sprt_locale_t loc) __SPRT_NOEXCEPT {
	return __nl_langinfo_default(item);
}

char *nl_langinfo(__sprt_nl_item item) __SPRT_NOEXCEPT { return __nl_langinfo_default(item); }

} // extern "C"

} // namespace sprt

extern "C" {

// libc++'s C-locale collate facet reaches this; on wasm it happens to live in
// the path file, which is only where it landed. It belongs with the collation.
int __strcoll(const char *l, const char *r) __SPRT_NOEXCEPT {
	return sprt::__strcoll_l(l, r, nullptr);
}

} // extern "C"
