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

#include <sprt/c/bits/__sprt_def.h>

#include "locale.h"
#include "string.h"
#include "wctype.h"
#include "../include/__impl_libc.h"

#include <sprt/c/__sprt_nl_types.h>

#if SPRT_WINDOWS
#include "windows/locale.cc"
#elif SPRT_WASM
#include "wasm/locale.cc"
#elif SPRT_EMBOX_USER
#include "embox_user/locale.cc"
#endif

namespace sprt {

static thread_local locale_t tl_locale = 0;

const __locale_map *__get_effective_locale_map(int c) {
	if (c >= __SPRT_LC_ALL) {
		return nullptr;
	}
	if (tl_locale) {
		return tl_locale->data.cat[c];
	} else {
		auto libc = __libc::get();
		unique_lock lock(libc->defaultLocaleMutex);
		return libc->defaultLocale.cat[c];
	}
}

char __get_effective_numeric_radix() {
	return __get_numeric_radix(__get_effective_locale_map(__SPRT_LC_NUMERIC));
}

// MB_CUR_MAX of the effective LC_CTYPE locale: 1 for the single-byte "C"/"POSIX"
// locale (the program default), 4 for UTF-8. Read by __ctype_get_mb_cur_max() and
// the mb/wc conversion helpers in builtin_multibyte.cpp.
size_t __get_effective_mb_cur_max() {
	auto m = __get_effective_locale_map(__SPRT_LC_CTYPE);
	return (m && m->mb_cur_max) ? m->mb_cur_max : 1;
}

__numeric_fmt __get_effective_numeric_fmt() {
	return __get_numeric_fmt(__get_effective_locale_map(__SPRT_LC_NUMERIC));
}

extern "C" {

locale_t newlocale(int mask, const char *name, locale_t loc) __SPRT_NOEXCEPT {
	if (!name || loc == __SPRT_LC_GLOBAL_LOCALE) {
		__sprt_errno = EINVAL;
		return nullptr;
	}

	__locale_struct newStruct;

	if (loc) {
		newStruct = loc->data;
	} else {
		auto def = __get_default_locale();
		newStruct = __locale_struct{
			def,
			def,
			def,
			def,
			def,
			def,
		};
	}

	auto len = sprt::strlen(name);
	for (auto it : sprt::flags(unsigned(mask))) {
		if (it <= __SPRT_LC_MESSAGES_MASK) {
			auto cat = sprt::countr_zero(it);
			newStruct.cat[cat] = __get_locale(cat, name, len);
			if (!newStruct.cat[cat]) {
				__sprt_errno = ENOENT;
				return nullptr;
			}
		} else {
			__sprt_errno = EINVAL;
			return nullptr;
		}
	}

	if (!loc) {
		loc = new (sprt::nothrow) __freestanding_locale_struct;
		if (!loc) {
			__sprt_errno = ENOMEM;
			return nullptr;
		}
		loc->data = newStruct;
		loc->refcount = 1;
	}

	return loc;
}

locale_t uselocale(locale_t loc) __SPRT_NOEXCEPT {
	locale_t src = nullptr;
	if (loc == nullptr) {
		return tl_locale == nullptr ? __SPRT_LC_GLOBAL_LOCALE : tl_locale;
	} else if (loc == __SPRT_LC_GLOBAL_LOCALE) {
		src = tl_locale;
		tl_locale = nullptr;
	} else {
		src = tl_locale;
		_atomic::fetchAdd(&loc->refcount, uint32_t(1));
		tl_locale = loc;
	}

	if (src) {
		if (_atomic::fetchSub(&src->refcount, uint32_t(1)) == 1) {
			// The OLD locale (src) is the one whose refcount reached 0; free it,
			// not the newly-installed `loc` that tl_locale now points to.
			sprt::__delete_n(src);
			src = nullptr;
		}
	}
	// POSIX: the previous locale is reported as LC_GLOBAL_LOCALE when the thread was
	// on the global locale — NEVER as nullptr (that is reserved for errors). Returning
	// nullptr here breaks callers that save-and-restore only on a non-null result
	// (libc++'s __locale_guard: `if (__old_loc_) uselocale(__old_loc_)`); they would
	// skip restoring the global state and leave the thread permanently pinned to `loc`.
	return src ? src : __SPRT_LC_GLOBAL_LOCALE;
}

locale_t duplocale(locale_t loc) __SPRT_NOEXCEPT {
	if (loc == __SPRT_LC_GLOBAL_LOCALE || loc == nullptr) {
		auto libc = __libc::get();

		unique_lock lock(libc->defaultLocaleMutex);

		auto ret = new (sprt::nothrow) __freestanding_locale_struct;
		if (!ret) {
			__sprt_errno = ENOMEM;
			return nullptr;
		}
		ret->data = libc->defaultLocale;
		ret->refcount = 1;
		return ret;
	} else {
		auto ret = new (sprt::nothrow) __freestanding_locale_struct;
		if (!ret) {
			__sprt_errno = ENOMEM;
			return nullptr;
		}
		ret->data = loc->data;
		ret->refcount = 1;
		return ret;
	}
}

void freelocale(locale_t loc) __SPRT_NOEXCEPT {
	if (loc == __SPRT_LC_GLOBAL_LOCALE || loc == nullptr) {
		return;
	}

	if (_atomic::fetchSub(&loc->refcount, uint32_t(1)) == 1) {
		sprt::__delete_n(loc);
	}
}

wint_t towlower_l(wint_t ch, locale_t loc) __SPRT_NOEXCEPT {
	if (loc == nullptr || loc == __SPRT_LC_GLOBAL_LOCALE) {
		return __towlower_l(ch, nullptr);
	} else {
		return __towlower_l(ch, loc->data.cat[__SPRT_LC_COLLATE]);
	}
}

wint_t towupper_l(wint_t ch, locale_t loc) __SPRT_NOEXCEPT {
	if (loc == nullptr || loc == __SPRT_LC_GLOBAL_LOCALE) {
		return __towupper_l(ch, nullptr);
	} else {
		return __towupper_l(ch, loc->data.cat[__SPRT_LC_COLLATE]);
	}
}

int wcscasecmp_l(const wchar_t *l, const wchar_t *r, locale_t loc) __SPRT_NOEXCEPT {
	if (loc == nullptr || loc == __SPRT_LC_GLOBAL_LOCALE) {
		return __wcscasecmp_l(l, r, nullptr);
	} else {
		return __wcscasecmp_l(l, r, loc->data.cat[__SPRT_LC_COLLATE]);
	}
}

int wcsncasecmp_l(const wchar_t *l, const wchar_t *r, __sprt_size_t n,
		locale_t loc) __SPRT_NOEXCEPT {
	if (loc == nullptr || loc == __SPRT_LC_GLOBAL_LOCALE) {
		return __wcsncasecmp_l(l, r, n, nullptr);
	} else {
		return __wcsncasecmp_l(l, r, n, loc->data.cat[__SPRT_LC_COLLATE]);
	}
}

int wcscoll_l(const wchar_t *l, const wchar_t *r, locale_t loc) __SPRT_NOEXCEPT {
	if (loc == nullptr || loc == __SPRT_LC_GLOBAL_LOCALE) {
		return __wcscoll_l(l, r, nullptr);
	} else {
		return __wcscoll_l(l, r, loc->data.cat[__SPRT_LC_COLLATE]);
	}
}

size_t wcsxfrm_l(wchar_t *__restrict dest, const wchar_t *__restrict src, __sprt_size_t destSize,
		locale_t loc) __SPRT_NOEXCEPT {
	if (loc == nullptr || loc == __SPRT_LC_GLOBAL_LOCALE) {
		return __wcsxfrm_l(dest, src, destSize, nullptr);
	} else {
		return __wcsxfrm_l(dest, src, destSize, loc->data.cat[__SPRT_LC_COLLATE]);
	}
}
}
} // namespace sprt

__SPRT_C_FUNC int strcoll_l(const char *s1, const char *s2, locale_t loc) __SPRT_NOEXCEPT {
	// Mirrors strxfrm_l / wcscoll_l: an absent/global locale resolves to the
	// user-default collation, otherwise the explicit locale's LC_COLLATE category.
	if (loc == nullptr || loc == __SPRT_LC_GLOBAL_LOCALE) {
		return sprt::__strcoll_l(s1, s2, nullptr);
	}
	return sprt::__strcoll_l(s1, s2, loc->data.cat[__SPRT_LC_COLLATE]);
}

__SPRT_C_FUNC size_t strxfrm_l(char *__restrict dest, const char *__restrict src, size_t n,
		locale_t loc) __SPRT_NOEXCEPT {
	// Mirrors wcsxfrm_l: an absent/global locale resolves to the user-default
	// collation, otherwise the explicit locale's LC_COLLATE category is used.
	if (loc == nullptr || loc == __SPRT_LC_GLOBAL_LOCALE) {
		return sprt::__strxfrm_l(dest, src, n, nullptr);
	}
	return sprt::__strxfrm_l(dest, src, n, loc->data.cat[__SPRT_LC_COLLATE]);
}

// Real implementation lives under the internal name __strxfrm. The freestanding
// libc umbrella routes the public strxfrm() through __sprt_strxfrm_impl, which
// must reach the implementation by this distinct name to avoid recursing back
// into itself; the public strxfrm() symbol below simply forwards here.
//
// Mirrors strcoll/wcsxfrm: in the C/POSIX locale the transform is the identity
// (strcmp then matches strcoll); otherwise produce a Windows sort key so that
// strcmp() of two keys agrees with strcoll().
__SPRT_C_FUNC size_t __strxfrm(char *dest, const char *src, size_t n) __SPRT_NOEXCEPT {
	auto map = sprt::__get_effective_locale_map(__SPRT_LC_COLLATE);
	if (sprt::__locale_is_c(map)) {
		size_t l = sprt::strlen(src);
		if (n > l) {
			sprt::strcpy(dest, src);
		}
		return l;
	}
	return sprt::__strxfrm_l(dest, src, n, map);
}

__SPRT_C_FUNC size_t strxfrm(char *dest, const char *src, size_t n) __SPRT_NOEXCEPT {
	return __strxfrm(dest, src, n);
}

char __get_locale_numeric_radix(__freestanding_locale_struct *loc) {
	if (loc == nullptr || loc == __SPRT_LC_GLOBAL_LOCALE) {
		return sprt::__get_effective_numeric_radix();
	}
	return sprt::__get_numeric_radix(loc->data.cat[__SPRT_LC_NUMERIC]);
}

sprt::__numeric_fmt __get_locale_numeric_fmt(__freestanding_locale_struct *loc) {
	if (loc == nullptr || loc == __SPRT_LC_GLOBAL_LOCALE) {
		return sprt::__get_effective_numeric_fmt();
	}
	return sprt::__get_numeric_fmt(loc->data.cat[__SPRT_LC_NUMERIC]);
}


// libc_impl is the libc on freestanding targets, so it defines the strong plain
// catopen/catgets/catclose. There is no catalog backend here, so they are just
// the honest empty-catalog fallback from runtime_core (the wrapper's weak
// nl_langinfo/catopen references resolve to these on Windows). The __sprt_* SPRT-
// API symbols live in the libc_wrapper, which is built for every target.
namespace sprt {
__SPRT_ID(nl_catd) __catopen_empty(const char *, int);
char *__catgets_empty(__SPRT_ID(nl_catd), int, int, const char *);
int __catclose_empty(__SPRT_ID(nl_catd));
} // namespace sprt

__SPRT_C_FUNC __SPRT_ID(nl_catd) catopen(const char *name, int flag) __SPRT_NOEXCEPT {
	return sprt::__catopen_empty(name, flag);
}

__SPRT_C_FUNC char *catgets(__SPRT_ID(nl_catd) catd, int set_id, int msg_id,
		const char *msg) __SPRT_NOEXCEPT {
	return sprt::__catgets_empty(catd, set_id, msg_id, msg);
}

__SPRT_C_FUNC int catclose(__SPRT_ID(nl_catd) catd) __SPRT_NOEXCEPT {
	return sprt::__catclose_empty(catd);
}
