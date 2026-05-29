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

#include "locale.h"
#include "wchar.h"
#include "../../include/__impl_libc.h"

#include <sprt/runtime/stringview.h>
#include <sprt/wrappers/windows/windows.h>
#include <sprt/cxx/mutex>
#include <sprt/cxx/type_traits>
#include <sprt/cxx/algorithm>
#include <sprt/cxx/atomic>

namespace sprt {

struct __locale_map {
	wchar_t wname[LOCALE_NAME_MAX_LENGTH + 1];
	char name[LOCALE_NAME_MAX_LENGTH + 1];
};

static_assert(sprt::is_trivially_constructible_v<__locale_map>);

static __locale_map s_localeMapCUtf8;
static __freestanding_locale_struct s_localeStructCUtf8;

void __init_locale() {
	memcpy(s_localeMapCUtf8.name, "C.UTF8", 7);
	memcpy(s_localeMapCUtf8.wname, L"C.UTF8", 7 * sizeof(wchar_t));

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

__locale_map *__get_locale(int cat, const char *localeName, size_t len) {
	if (!localeName) {
		return nullptr;
	}
	auto n = StringView(localeName, len);
	if (n == "C" || n == "POSIX" || n == "C.UTF8") {
		return &s_localeMapCUtf8;
	}
	if (localeName[0] == 0) {
		wchar_t wname[LOCALE_NAME_MAX_LENGTH + 1];
		char name[LOCALE_NAME_MAX_LENGTH + 1];
		if (!GetUserDefaultLocaleName(wname, LOCALE_NAME_MAX_LENGTH)) {
			return nullptr;
		}
		unicode::toUtf8([&](StringView str) {
			memcpy(name, str.data(), sprt::min(str.size(), size_t(LOCALE_NAME_MAX_LENGTH)));
		}, WideStringView((char16_t *)wname));

		return __libc::get()->get_cached_locale(name, strlen(name), [&]() {
			auto map = new (nothrow) __locale_map;
			memcpy(map->name, name, strlen(name));
			memcpy(map->wname, wname, strlen(wname) * sizeof(wchar_t));
			return map;
		});
	} else {
		__locale_map *m = nullptr;
		unicode::toUtf16([&](WideStringView str) {
			if (IsValidLocaleName((wchar_t *)str.data())) {
				m = __libc::get()->get_cached_locale(localeName, strlen(localeName), [&]() {
					auto map = new (nothrow) __locale_map;
					memcpy(map->name, localeName, strlen(localeName));
					memcpy(map->wname, str.data(), str.size() * sizeof(wchar_t));
					return map;
				});
			}
		}, StringView(localeName));
		return m;
	}
}

void __free_locale(__locale_map *map) { sprt::__delete(map); }

static char s_localeTemporaryBuffer[(LOCALE_NAME_MAX_LENGTH + 1) * __SPRT_LC_ALL];

extern "C" {

char *setlocale(int cat, const char *name) __SPRT_NOEXCEPT {
	if ((unsigned)cat > __SPRT_LC_ALL) {
		return 0;
	}

	auto libc = __libc::get();
	const __locale_map *lm = nullptr;

	sprt::unique_lock lock(libc->defaultLocaleMutex);

	/* For LC_ALL, setlocale is required to return a string which
	 * encodes the current setting for all categories. The format of
	 * this string is unspecified, and only the following code, which
	 * performs both the serialization and deserialization, depends
	 * on the format, so it can easily be changed if needed. */
	if (cat == __SPRT_LC_ALL) {
		int i;
		if (name) {
			__locale_struct tmp_locale;

			int i = 0;
			bool failed = false;
			__locale_map *found = nullptr;

			StringView nameView(name);
			nameView.split<StringView::Chars<';'>>([&](StringView part) {
				if (i > __SPRT_LC_ALL) {
					return false;
				}

				found = __get_locale(i, part.data(), part.size());
				if (found && i < __SPRT_LC_ALL) {
					tmp_locale.cat[i] = found;
				} else {
					__sprt_errno = ENOENT;
					failed = true;
					return false;
				}
				++i;
				return true;
			});

			if (failed) {
				return nullptr;
			}

			while (i < __SPRT_LC_ALL) { tmp_locale.cat[i++] = found; }

			libc->defaultLocale = tmp_locale;
		}

		char *buffer = s_localeTemporaryBuffer;
		size_t bufSize = sizeof(s_localeTemporaryBuffer) - 1;

		const char *part;

		int same = 0;
		for (i = 0; i < __SPRT_LC_ALL; i++) {
			part = libc->defaultLocale.cat[i]->name;
			lm = libc->defaultLocale.cat[i];
			if (lm == libc->defaultLocale.cat[0]) {
				same++;
			}

			if (lm) {
				buffer = strappend(buffer, &bufSize, lm->name, strlen(lm->name));
			} else {
				buffer = strappend(buffer, &bufSize, 'C');
			}
			buffer = strappend(buffer, &bufSize, ';');
		}
		return same == __SPRT_LC_ALL ? (char *)part : s_localeTemporaryBuffer;
	}

	if (name) {
		lm = __get_locale(cat, name, strlen(name));
		if (!lm) {
			__sprt_errno = ENOENT;
			return nullptr;
		}
		libc->defaultLocale.cat[cat] = lm;
	} else {
		lm = libc->defaultLocale.cat[cat];
	}

	return lm ? (char *)lm->name : (char *)"C";
}

static constexpr size_t DataBufferSize = 2'048;
static thread_local lconv tl_lconv;
static thread_local char tl_lconvData[DataBufferSize];

static char *__allocateBuffer(const wchar_t *str, char **buf, size_t *bufSize) {
	char *ret = nullptr;
	unicode::toUtf8([&](StringView str) {
		auto len = str.size() + 1;
		if (*bufSize >= len) {
			ret = *buf;
			memcpy(*buf, str.data(), len);
			*bufSize += len;
			*buf += len;
		}
	}, (char16_t *)str);
	return ret;
}

static char *__convertGrouping(const wchar_t *grouping, char **buf, size_t *bufSize) {
	if (!grouping) {
		return nullptr;
	}

	// Convert grouping to ASCII format (e.g., "\x03\x02\x01\x00")
	int len = (int)wcslen(grouping);
	if (*bufSize >= len + 1) {
		char *result = *buf;
		*bufSize += len;
		*buf += len;

		for (int i = 0; i < len; i++) { result[i] = (char)(grouping[i] - L'0'); }
		result[len] = '\0';
		return result;
	}
	return nullptr;
}

static bool __populate_lconv(lconv *lconv, const wchar_t *localeNameNimeric,
		const wchar_t *localeNameMonetary) {
	static constexpr DWORD WideBufferSize = 256;
	wchar_t wideBuffer[WideBufferSize];

	if (!lconv || !localeNameNimeric || !localeNameMonetary) {
		return false;
	}

	sprt::memset(lconv, 0, sizeof(__sprt_lconv));

	tl_lconvData[0] = 0;
	char *targetBuf = tl_lconvData;
	size_t bufSize = DataBufferSize - 1;

	// Decimal point
	if (GetLocaleInfoEx(localeNameNimeric, LOCALE_SDECIMAL, wideBuffer, WideBufferSize)) {
		lconv->decimal_point = __allocateBuffer(wideBuffer, &targetBuf, &bufSize);
	}

	// Thousands separator
	if (GetLocaleInfoEx(localeNameNimeric, LOCALE_STHOUSAND, wideBuffer, WideBufferSize)) {
		lconv->thousands_sep = __allocateBuffer(wideBuffer, &targetBuf, &bufSize);
	}

	// Grouping
	if (GetLocaleInfoEx(localeNameNimeric, LOCALE_SGROUPING, wideBuffer, WideBufferSize)) {
		lconv->grouping = __convertGrouping(wideBuffer, &targetBuf, &bufSize);
	}

	// Currency symbol
	if (GetLocaleInfoEx(localeNameMonetary, LOCALE_SCURRENCY, wideBuffer, WideBufferSize)) {
		lconv->currency_symbol = __allocateBuffer(wideBuffer, &targetBuf, &bufSize);
	}

	// International currency symbol
	if (GetLocaleInfoEx(localeNameMonetary, LOCALE_SINTLSYMBOL, wideBuffer, WideBufferSize)) {
		lconv->int_curr_symbol = __allocateBuffer(wideBuffer, &targetBuf, &bufSize);
	}

	// Monetary decimal point
	if (GetLocaleInfoEx(localeNameMonetary, LOCALE_SMONDECIMALSEP, wideBuffer, WideBufferSize)) {
		lconv->mon_decimal_point = __allocateBuffer(wideBuffer, &targetBuf, &bufSize);
	}

	// Monetary thousands separator
	if (GetLocaleInfoEx(localeNameMonetary, LOCALE_SMONTHOUSANDSEP, wideBuffer, WideBufferSize)) {
		lconv->mon_thousands_sep = __allocateBuffer(wideBuffer, &targetBuf, &bufSize);
	}

	// Monetary grouping
	if (GetLocaleInfoEx(localeNameMonetary, LOCALE_SMONGROUPING, wideBuffer, WideBufferSize)) {
		lconv->mon_grouping = __allocateBuffer(wideBuffer, &targetBuf, &bufSize);
	}

	// Positive sign
	if (GetLocaleInfoEx(localeNameNimeric, LOCALE_SPOSITIVESIGN, wideBuffer, WideBufferSize)) {
		lconv->positive_sign = __allocateBuffer(wideBuffer, &targetBuf, &bufSize);
	}

	// Negative sign
	if (GetLocaleInfoEx(localeNameNimeric, LOCALE_SNEGATIVESIGN, wideBuffer, WideBufferSize)) {
		lconv->negative_sign = __allocateBuffer(wideBuffer, &targetBuf, &bufSize);
	}

	// Fractional digits
	if (GetLocaleInfoEx(localeNameNimeric, LOCALE_IDIGITS, wideBuffer, WideBufferSize)) {
		lconv->frac_digits = (char)wcstol(wideBuffer, nullptr, 10);
	}

	// Positive currency symbol precedes value
	if (GetLocaleInfoEx(localeNameMonetary, LOCALE_IPOSSYMPRECEDES, wideBuffer, WideBufferSize)) {
		lconv->p_cs_precedes = (char)wcstol(wideBuffer, nullptr, 10);
	}

	// Positive currency separated by space
	if (GetLocaleInfoEx(localeNameMonetary, LOCALE_IPOSSEPBYSPACE, wideBuffer, WideBufferSize)) {
		lconv->p_sep_by_space = (char)wcstol(wideBuffer, nullptr, 10);
	}

	// Negative currency symbol precedes value
	if (GetLocaleInfoEx(localeNameMonetary, LOCALE_INEGSYMPRECEDES, wideBuffer, WideBufferSize)) {
		lconv->n_cs_precedes = (char)wcstol(wideBuffer, nullptr, 10);
	}

	// Negative currency separated by space
	if (GetLocaleInfoEx(localeNameMonetary, LOCALE_INEGSEPBYSPACE, wideBuffer, WideBufferSize)) {
		lconv->n_sep_by_space = (char)wcstol(wideBuffer, nullptr, 10);
	}

	// Positive sign position (1-4)
	if (GetLocaleInfoEx(localeNameNimeric, LOCALE_IPOSSIGNPOSN, wideBuffer, WideBufferSize)) {
		lconv->p_sign_posn = (char)wcstol(wideBuffer, nullptr, 10);
	}

	// Negative sign position (5-8)
	if (GetLocaleInfoEx(localeNameNimeric, LOCALE_INEGSIGNPOSN, wideBuffer, WideBufferSize)) {
		lconv->n_sign_posn = (char)wcstol(wideBuffer, nullptr, 10);
	}

	lconv->int_p_cs_precedes = lconv->p_cs_precedes;
	lconv->int_p_sep_by_space = lconv->p_sep_by_space;
	lconv->int_n_cs_precedes = lconv->n_cs_precedes;
	lconv->int_n_sep_by_space = lconv->n_sep_by_space;
	lconv->int_p_sign_posn = lconv->p_sign_posn;
	lconv->int_n_sign_posn = lconv->n_sign_posn;

	return true;
}

lconv *localeconv(void) __SPRT_NOEXCEPT {
	if (__populate_lconv(&tl_lconv, __get_effective_locale_map(__SPRT_LC_NUMERIC)->wname,
				__get_effective_locale_map(__SPRT_LC_MONETARY)->wname)) {
		return &tl_lconv;
	}
	return nullptr;
}
}

wchar_t __towlower_l(wchar_t ch, const __locale_map *locMap) {
	const wchar_t *locName = nullptr;
	if (locMap == NULL) {
		locName = LOCALE_NAME_USER_DEFAULT;
	} else {
		locName = locMap->wname;
	}

	// Use LCMapStringEx with LCMAP_LOWERCASE flag
	// We pass a single character as input
	wchar_t input[] = {ch, L'\0'};
	wchar_t output[2] = {0};

	int result = LCMapStringEx(locName, // Locale name
			LCMAP_LOWERCASE, // Convert to lowercase
			input, // Source string (single char + null)
			-1, // Negative value means null-terminated
			output, // Destination buffer
			2, // Destination size (2 chars)
			nullptr, // Version info (not needed)
			nullptr, // Reserved
			0 // Sort handle (not needed)
	);

	if (result > 0) {
		return output[0];
	}

	return ch;
}

wchar_t __towupper_l(wchar_t ch, const __locale_map *locMap) {
	const wchar_t *locName = nullptr;
	if (locMap == NULL) {
		locName = LOCALE_NAME_USER_DEFAULT;
	} else {
		locName = locMap->wname;
	}

	// Use LCMapStringEx with LCMAP_LOWERCASE flag
	// We pass a single character as input
	wchar_t input[] = {ch, L'\0'};
	wchar_t output[2] = {0};

	int result = LCMapStringEx(locName, // Locale name
			LCMAP_UPPERCASE, // Convert to lowercase
			input, // Source string (single char + null)
			-1, // Negative value means null-terminated
			output, // Destination buffer
			2, // Destination size (2 chars)
			nullptr, // Version info (not needed)
			nullptr, // Reserved
			0 // Sort handle (not needed)
	);

	if (result > 0) {
		return output[0];
	}

	return ch;
}

int __wcscmp_l(const wchar_t *l, const wchar_t *r, const __locale_map *locMap) {
	// Use default locale if none specified
	const wchar_t *locName = nullptr;
	if (locMap == NULL) {
		locName = LOCALE_NAME_USER_DEFAULT;
	} else {
		locName = locMap->wname;
	}

	auto res = CompareStringEx(locName, NORM_IGNOREWIDTH, l, strlen(l), r, strlen(r), nullptr,
			nullptr, 0);
	if (res != 0) {
		return res - 2;
	}
	return 0;
}

int __wcsncmp_l(const wchar_t *l, const wchar_t *r, size_t n, const __locale_map *locMap) {
	// Use default locale if none specified
	const wchar_t *locName = nullptr;
	if (locMap == NULL) {
		locName = LOCALE_NAME_USER_DEFAULT;
	} else {
		locName = locMap->wname;
	}

	auto res = CompareStringEx(locName, NORM_IGNOREWIDTH, l, sprt::min(strlen(l), n), r,
			sprt::min(strlen(r), n), nullptr, nullptr, 0);
	if (res != 0) {
		return res - 2;
	}
	return 0;
}

int __wcscasecmp_l(const wchar_t *l, const wchar_t *r, const __locale_map *locMap) {
	// Use default locale if none specified
	const wchar_t *locName = nullptr;
	if (locMap == NULL) {
		locName = LOCALE_NAME_USER_DEFAULT;
	} else {
		locName = locMap->wname;
	}

	auto res = CompareStringEx(locName, NORM_IGNORECASE | NORM_IGNOREWIDTH, l, strlen(l), r,
			strlen(r), nullptr, nullptr, 0);
	if (res != 0) {
		return res - 2;
	}
	return 0;
}

int __wcsncasecmp_l(const wchar_t *l, const wchar_t *r, size_t n, const __locale_map *locMap) {
	// Use default locale if none specified
	const wchar_t *locName = nullptr;
	if (locMap == NULL) {
		locName = LOCALE_NAME_USER_DEFAULT;
	} else {
		locName = locMap->wname;
	}

	auto res = CompareStringEx(locName, NORM_IGNORECASE | NORM_IGNOREWIDTH, l,
			sprt::min(strlen(l), n), r, sprt::min(strlen(r), n), nullptr, nullptr, 0);
	if (res != 0) {
		return res - 2;
	}
	return 0;
}

int __wcscoll_l(const wchar_t *l, const wchar_t *r, const __locale_map *locMap) {
	// Use default locale if none specified
	const wchar_t *locName = nullptr;
	if (locMap == NULL) {
		locName = LOCALE_NAME_USER_DEFAULT;
	} else {
		locName = locMap->wname;
	}

	auto res = CompareStringEx(locName, 0, l, strlen(l), r, strlen(r), nullptr, nullptr, 0);
	if (res != 0) {
		return res - 2;
	}
	return 0;
}

size_t __wcsxfrm_l(wchar_t *__restrict dest, const wchar_t *__restrict src, size_t destSize,
		const __locale_map *locMap) {
	if (src == NULL) {
		if (dest != NULL && destSize > 0) {
			dest[0] = L'\0';
		}
		return 0;
	}

	// Get source string length
	auto src_len = strlen(src);

	// Use default locale if none specified
	const wchar_t *locName = nullptr;
	if (locMap == NULL) {
		locName = LOCALE_NAME_USER_DEFAULT;
	} else {
		locName = locMap->wname;
	}

	DWORD flags = LCMAP_SORTKEY;

	// First call: determine required buffer size
	// LCMapStringEx returns the required size when dest is NULL
	auto required_len =
			LCMapStringEx(locName, flags, src, src_len, nullptr, 0, nullptr, nullptr, 0);

	// Check for errors
	if (required_len == 0) {
		if (dest != nullptr && destSize > 0) {
			dest[0] = L'\0';
		}
		return 0;
	}

	// required_len includes the null terminator
	// Calculate actual characters needed (excluding null)
	int chars_needed = required_len - 1;

	// Check if destination buffer is large enough
	if (destSize == 0 || (size_t)required_len > destSize) {
		// Buffer too small - return required size
		// Don't write anything to avoid buffer overflow
		return (size_t)chars_needed;
	}

	// Perform the transformation
	auto dest_len =
			LCMapStringEx(locName, flags, src, src_len, dest, required_len, nullptr, nullptr, 0);

	if (dest_len == 0) {
		// Error occurred
		if (dest != nullptr && destSize > 0) {
			dest[0] = L'\0';
		}
		return 0;
	}

	// Ensure null termination
	if ((size_t)dest_len < destSize) {
		dest[dest_len] = L'\0';
	} else if (destSize > 0) {
		dest[destSize - 1] = L'\0';
	}

	// Return length excluding null terminator
	return (size_t)(dest_len - 1);
}

} // namespace sprt
