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

#if SPRT_WINDOWS
#include "windows/locale.cc"
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
			sprt::__delete_n(loc);
			src = nullptr;
		}
	}
	return src;
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

__SPRT_C_FUNC size_t strxfrm_l(char *__restrict dest, const char *__restrict src, size_t n,
		locale_t loc) __SPRT_NOEXCEPT {
	size_t l = strlen(src);
	if (n > l) {
		strcpy(dest, src);
	}
	return l;
}

__SPRT_C_FUNC size_t strxfrm(char *dest, const char *src, size_t n) __SPRT_NOEXCEPT {
	return strxfrm_l(dest, src, n, nullptr);
}
