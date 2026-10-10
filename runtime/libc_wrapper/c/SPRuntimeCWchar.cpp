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

#define __SPRT_BUILD 1
#define _CRT_SECURE_NO_WARNINGS 1

#include <sprt/c/__sprt_wchar.h>
#include <sprt/c/__sprt_wctype.h>
#include <sprt/c/__sprt_stdarg.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/runtime/log.h>

#include <wchar.h>
#include <wctype.h>
#include <time.h>

#if SPRT_HOSTED_RTOS
// NuttX <wchar.h> ships only the C99 minimum; sprt's umbrella re-exports the
// POSIX/BSD extensions, so pull the side-header declaring them.
#include <wchar_extras.h>
#endif

#ifndef SPRT_APPLE
#include <uchar.h>
#endif

#include "time/time_internals.h"

#if SPRT_ANDROID
namespace sprt::platform {

extern size_t (*_wcsftime_l)(wchar_t *__buf, size_t __n, const wchar_t *__fmt,
		const struct tm *__tm, locale_t __l);

// Bionic added wctrans/towctrans (and the _l variants) only in API 26; these are
// resolved at runtime (jni.cc), null on older devices -> runtime_core fallback.
extern wctrans_t (*_wctrans)(const char *__name);
extern wint_t (*_towctrans)(wint_t __wc, wctrans_t __transform);
extern wctrans_t (*_wctrans_l)(const char *__name, locale_t __l);
extern wint_t (*_towctrans_l)(wint_t __wc, wctrans_t __transform, locale_t __l);

} // namespace sprt::platform
#endif

#if SPRT_APPLE
#include <xlocale.h>
// Apple's SDK ships no <uchar.h> conversion functions, so implement them below
// on top of the runtime's UTF-8 <-> UTF-16/UTF-32 primitives; these pull in the
// scalar-value helpers and the host errno numbers (EILSEQ) they report with.
#include <errno.h>
#include <sprt/runtime/unicode.h>
#endif

static_assert(sizeof(mbstate_t) == sizeof(__SPRT_MBSTATE_NAME));
static_assert(alignof(mbstate_t) == alignof(__SPRT_MBSTATE_NAME));
static_assert(sizeof(wctype_t) == sizeof(__sprt_wctype_t));
static_assert(sizeof(wint_t) == sizeof(__sprt_wint_t));
static_assert(sizeof(wctrans_t) == sizeof(__sprt_wctrans_t));
static_assert(WEOF == __SPRT_WEOF);

namespace sprt {

// lcc/e2k: umbrella-обёртки инлайнятся полностью (сборка с -O1), слабых копий
// в образе нет — прямые вызовы libc резолвятся в glibc без версионных алиасов
#define __SPRT_LIBC(name) name


// Look up a standard mapping by name; "toupper"/"tolower" yield a sentinel
// handle, anything else null (with EINVAL).
__SPRT_ID(wctrans_t) __wctrans_fallback(const char *name) __SPRT_NOEXCEPT;

// Apply a handle from __wctrans_fallback; a null/unknown handle returns wc.
__SPRT_ID(wint_t)
__towctrans_fallback(__SPRT_ID(wint_t) wc, __SPRT_ID(wctrans_t) desc) __SPRT_NOEXCEPT;

} // namespace sprt

namespace sprt {

__SPRT_C_FUNC __SPRT_ID(wchar_t)
		* __SPRT_ID(wcscpy)(__SPRT_ID(wchar_t) * __SPRT_RESTRICT a,
				const __SPRT_ID(wchar_t) * __SPRT_RESTRICT b) {
	return __SPRT_LIBC(wcscpy)(a, b);
}

__SPRT_C_FUNC __SPRT_ID(wchar_t)
		* __SPRT_ID(wcsncpy)(__SPRT_ID(wchar_t) * __SPRT_RESTRICT a,
				const __SPRT_ID(wchar_t) * __SPRT_RESTRICT b, __SPRT_ID(size_t) s) {
	return __SPRT_LIBC(wcsncpy)(a, b, s);
}

__SPRT_C_FUNC __SPRT_ID(wchar_t)
		* __SPRT_ID(wcscat)(__SPRT_ID(wchar_t) * __SPRT_RESTRICT a,
				const __SPRT_ID(wchar_t) * __SPRT_RESTRICT b) {
	return __SPRT_LIBC(wcscat)(a, b);
}

__SPRT_C_FUNC __SPRT_ID(wchar_t)
		* __SPRT_ID(wcsncat)(__SPRT_ID(wchar_t) * __SPRT_RESTRICT a,
				const __SPRT_ID(wchar_t) * __SPRT_RESTRICT b, __SPRT_ID(size_t) s) {
	return __SPRT_LIBC(wcsncat)(a, b, s);
}

__SPRT_C_FUNC int __SPRT_ID(wcscmp)(const __SPRT_ID(wchar_t) * a, const __SPRT_ID(wchar_t) * b) {
	return __SPRT_LIBC(wcscmp)(a, b);
}

__SPRT_C_FUNC int __SPRT_ID(
		wcsncmp)(const __SPRT_ID(wchar_t) * a, const __SPRT_ID(wchar_t) * b, __SPRT_ID(size_t) s) {
	return __SPRT_LIBC(wcsncmp)(a, b, s);
}

__SPRT_C_FUNC int __SPRT_ID(wcscoll)(const __SPRT_ID(wchar_t) * a, const __SPRT_ID(wchar_t) * b) {
	return __SPRT_LIBC(wcscoll)(a, b);
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(wcsxfrm)(__SPRT_ID(wchar_t) * __SPRT_RESTRICT a,
		const __SPRT_ID(wchar_t) * __SPRT_RESTRICT b, __SPRT_ID(size_t) s) {
	return __SPRT_LIBC(wcsxfrm)(a, b, s);
}

__SPRT_C_FUNC const __SPRT_ID(wchar_t)
		* __SPRT_ID(wcschr)(const __SPRT_ID(wchar_t) * a, __SPRT_ID(wchar_t) b) {
	return __SPRT_LIBC(wcschr)(a, b);
}

__SPRT_C_FUNC const __SPRT_ID(wchar_t)
		* __SPRT_ID(wcsrchr)(const __SPRT_ID(wchar_t) * a, __SPRT_ID(wchar_t) b) {
	return __SPRT_LIBC(wcsrchr)(a, b);
}

__SPRT_C_FUNC __SPRT_ID(size_t)
		__SPRT_ID(wcscspn)(const __SPRT_ID(wchar_t) * a, const __SPRT_ID(wchar_t) * b) {
	return __SPRT_LIBC(wcscspn)(a, b);
}
__SPRT_C_FUNC __SPRT_ID(size_t)
		__SPRT_ID(wcsspn)(const __SPRT_ID(wchar_t) * a, const __SPRT_ID(wchar_t) * b) {
	return __SPRT_LIBC(wcsspn)(a, b);
}

__SPRT_C_FUNC const __SPRT_ID(wchar_t)
		* __SPRT_ID(wcspbrk)(const __SPRT_ID(wchar_t) * a, const __SPRT_ID(wchar_t) * b) {
	return __SPRT_LIBC(wcspbrk)(a, b);
}

__SPRT_C_FUNC __SPRT_ID(wchar_t)
		* __SPRT_ID(wcstok)(__SPRT_ID(wchar_t) * __SPRT_RESTRICT a,
				const __SPRT_ID(wchar_t) * __SPRT_RESTRICT b,
				__SPRT_ID(wchar_t) * *__SPRT_RESTRICT c) {
	return __SPRT_LIBC(wcstok)(a, b, c);
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(wcslen)(const __SPRT_ID(wchar_t) * v) {
	return __SPRT_LIBC(wcslen)(v);
}

__SPRT_C_FUNC const __SPRT_ID(wchar_t)
		* __SPRT_ID(wcsstr)(const __SPRT_ID(wchar_t) * __SPRT_RESTRICT a,
				const __SPRT_ID(wchar_t) * __SPRT_RESTRICT b) {
	return __SPRT_LIBC(wcsstr)(a, b);
}

__SPRT_C_FUNC const __SPRT_ID(wchar_t)
		* __SPRT_ID(wcswcs)(const __SPRT_ID(wchar_t) * a, const __SPRT_ID(wchar_t) * b) {
	return ::wcsstr(a, b);
}

__SPRT_C_FUNC const __SPRT_ID(wchar_t)
		* __SPRT_ID(
				wmemchr)(const __SPRT_ID(wchar_t) * a, __SPRT_ID(wchar_t) b, __SPRT_ID(size_t) s) {
	return __SPRT_LIBC(wmemchr)(a, b, s);
}

__SPRT_C_FUNC
int __SPRT_ID(
		wmemcmp)(const __SPRT_ID(wchar_t) * a, const __SPRT_ID(wchar_t) * b, __SPRT_ID(size_t) s) {
	return __SPRT_LIBC(wmemcmp)(a, b, s);
}

__SPRT_C_FUNC __SPRT_ID(wchar_t)
		* __SPRT_ID(wmemcpy)(__SPRT_ID(wchar_t) * __SPRT_RESTRICT a,
				const __SPRT_ID(wchar_t) * __SPRT_RESTRICT b, __SPRT_ID(size_t) s) {
	return __SPRT_LIBC(wmemcpy)(a, b, s);
}

__SPRT_C_FUNC __SPRT_ID(wchar_t)
		* __SPRT_ID(wmemmove)(__SPRT_ID(wchar_t) * a, const __SPRT_ID(wchar_t) * b,
				__SPRT_ID(size_t) s) {
	return __SPRT_LIBC(wmemmove)(a, b, s);
}

__SPRT_C_FUNC __SPRT_ID(wchar_t)
		* __SPRT_ID(wmemset)(__SPRT_ID(wchar_t) * a, __SPRT_ID(wchar_t) c, __SPRT_ID(size_t) s) {
	return __SPRT_LIBC(wmemset)(a, c, s);
}

__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(btowc)(int val) { return __SPRT_LIBC(btowc)(val); }

__SPRT_C_FUNC int __SPRT_ID(wctob)(__SPRT_ID(wint_t) val) { return __SPRT_LIBC(wctob)(val); }

__SPRT_C_FUNC int __SPRT_ID(mbsinit)(const __SPRT_MBSTATE_NAME *val) {
	return __SPRT_LIBC(mbsinit)((const ::mbstate_t *)val);
}

__SPRT_C_FUNC __SPRT_ID(size_t)
		__SPRT_ID(mbrtowc)(__SPRT_ID(wchar_t) * __SPRT_RESTRICT a, const char *__SPRT_RESTRICT b,
				__SPRT_ID(size_t) s, __SPRT_MBSTATE_NAME *__SPRT_RESTRICT state) {
	return __SPRT_LIBC(mbrtowc)(a, b, s, (::mbstate_t *)state);
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(wcrtomb)(char *__SPRT_RESTRICT a, __SPRT_ID(wchar_t) c,
		__SPRT_MBSTATE_NAME *__SPRT_RESTRICT state) {
	return __SPRT_LIBC(wcrtomb)(a, c, (::mbstate_t *)state);
}

#if SPRT_APPLE
// ---- <uchar.h> conversions for Apple ----
// macOS/iOS libc provides no mbrtoc16/c16rtomb/mbrtoc32/c32rtomb, so they are
// implemented here on top of the runtime's UTF-8 <-> UTF-16/UTF-32 primitives.
// The runtime treats the multibyte encoding as UTF-8 unconditionally, so the
// logic mirrors the freestanding implementation in runtime/libc_impl
// (builtin_multibyte.cpp), which the tests/libc suite already checks against the
// host glibc for behavioural identity.
namespace {

// Largest number of bytes a valid Unicode scalar value (<= U+10FFFF) occupies in
// UTF-8; equals MB_CUR_MAX for the UTF-8 encoding the runtime assumes.
constexpr size_t kUcharMaxUtf8 = 4;

// Values stored in UcharState::state. 0 is the initial state; 1..3 count the
// UTF-8 continuation bytes still owed for a partial scalar (with `ch` holding the
// bits decoded so far). The two sentinels carry surrogate state across calls and
// are chosen not to collide with the {1,2,3} continuation counts.
constexpr uint32_t kUcharStateNone = 0;
// mbrtoc16 emitted a high surrogate; `ch` holds the low surrogate owed to the
// next call (returned with the (size_t)-3 status, consuming no input).
constexpr uint32_t kUcharStatePendingLow = 0x1'0000u;
// c16rtomb saw a high surrogate; `ch` holds it while we await the matching low
// surrogate to assemble the astral scalar value.
constexpr uint32_t kUcharStateHighSurrogate = 0x2'0000u;

// Conversion state laid over the platform mbstate_t storage. Apple never writes
// this object itself (it has no <uchar.h>), so we fully own the representation.
struct UcharState {
	uint32_t state;
	uint32_t ch;
};
static_assert(sizeof(UcharState) <= sizeof(__SPRT_MBSTATE_NAME));

thread_local __SPRT_MBSTATE_NAME tl_uchar_state = {};

static UcharState *ucharState(__SPRT_MBSTATE_NAME *st) {
	return reinterpret_cast<UcharState *>(st ? st : &tl_uchar_state);
}

// Decode one code point from a UTF-8 stream, resuming a partial sequence from
// *st. Returns bytes consumed (>0), 0 for NUL, (size_t)-1 EILSEQ, (size_t)-2
// incomplete; writes the scalar value to *cp on success.
static size_t __mbrtocp(char32_t *cp, const char *s, size_t n, UcharState *st) {
	uint32_t ch;
	unsigned remaining;
	size_t consumed = 0;

	if (st->state >= 1 && st->state <= 3) {
		ch = st->ch;
		remaining = st->state;
	} else {
		if (n == 0) {
			return (size_t)-2;
		}
		unsigned char b = (unsigned char)s[0];
		if (b < 0x80) {
			*cp = b;
			st->state = kUcharStateNone;
			st->ch = 0;
			return b == 0 ? 0 : 1;
		}
		uint8_t len = unicode::utf8_length_data[b];
		if (len < 2 || len > 4) { // lone continuation byte or invalid lead
			__sprt_errno = EILSEQ;
			return (size_t)-1;
		}
		ch = (uint32_t)(b & (0x7Fu >> len));
		remaining = (unsigned)(len - 1);
		consumed = 1;
	}

	while (remaining > 0) {
		if (consumed >= n) {
			st->ch = ch;
			st->state = remaining;
			return (size_t)-2;
		}
		unsigned char b = (unsigned char)s[consumed];
		if ((b & 0xC0u) != 0x80u) {
			__sprt_errno = EILSEQ;
			return (size_t)-1;
		}
		ch = (ch << 6) | (uint32_t)(b & 0x3Fu);
		--remaining;
		++consumed;
	}

	st->state = kUcharStateNone;
	st->ch = 0;
	*cp = (char32_t)ch;
	return consumed;
}

} // namespace
#endif

__SPRT_C_FUNC __SPRT_ID(size_t)
		__SPRT_ID(mbrtoc16)(__SPRT_ID(char16_t) * __SPRT_RESTRICT a, const char *__SPRT_RESTRICT b,
				__SPRT_ID(size_t) s, __SPRT_MBSTATE_NAME *__SPRT_RESTRICT st) {
#if SPRT_APPLE
	auto state = ucharState(st);
	// A surrogate pair is reported across two calls: the pending low surrogate is
	// returned now (no bytes consumed) with the (size_t)-3 status.
	if (state->state == kUcharStatePendingLow) {
		if (a) {
			*a = (__SPRT_ID(char16_t))state->ch;
		}
		state->state = kUcharStateNone;
		state->ch = 0;
		return (size_t)-3;
	}
	// A null source resets to the initial conversion state (decode an embedded
	// NUL): completes with 0, or reports EILSEQ if a partial sequence was pending.
	const char *src = b ? b : "";
	size_t srcLen = b ? s : 1;
	char32_t cp = 0;
	size_t r = __mbrtocp(&cp, src, srcLen, state);
	if (r == (size_t)-1 || r == (size_t)-2) {
		return r;
	}
	if (cp <= 0xFFFF) {
		if (a) {
			*a = (__SPRT_ID(char16_t))cp;
		}
		return r;
	}
	// Astral: report the high surrogate now and stash the low surrogate.
	cp -= 0x1'0000u;
	if (a) {
		*a = (__SPRT_ID(char16_t))(0xD800u + (cp >> 10));
	}
	state->ch = 0xDC00u + (cp & 0x3FFu);
	state->state = kUcharStatePendingLow;
	return r;
#else
	return __SPRT_LIBC(mbrtoc16)(a, b, s, (::mbstate_t *)st);
#endif
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(c16rtomb)(char *__SPRT_RESTRICT a, __SPRT_ID(char16_t) c,
		__SPRT_MBSTATE_NAME *__SPRT_RESTRICT st) {
#if SPRT_APPLE
	auto state = ucharState(st);
	// A null destination behaves as __SPRT_LIBC(c16rtomb)(buf, u'\0', st) with an internal buf.
	char scratch[kUcharMaxUtf8];
	char *dst = a ? a : scratch;
	__SPRT_ID(char16_t) unit = a ? c : u'\0';

	if (state->state == kUcharStateHighSurrogate) {
		if (unicode::isUtf16LowSurrogate(unit)) {
			char32_t cp = unicode::utf16CombineSurrogates((char16_t)state->ch, unit);
			state->state = kUcharStateNone;
			state->ch = 0;
			return unicode::utf8EncodeBuf(dst, kUcharMaxUtf8, cp);
		}
		// A high surrogate not followed by a low surrogate is an error.
		state->state = kUcharStateNone;
		state->ch = 0;
		__sprt_errno = EILSEQ;
		return (size_t)-1;
	}

	if (unit == 0) {
		dst[0] = 0;
		return 1;
	}
	if (unicode::isUtf16HighSurrogate(unit)) {
		// Hold the high surrogate; nothing is emitted until its low surrogate.
		state->ch = unit;
		state->state = kUcharStateHighSurrogate;
		return 0;
	}
	if (unicode::isUtf16LowSurrogate(unit)) {
		__sprt_errno = EILSEQ;
		return (size_t)-1;
	}
	return unicode::utf8EncodeBuf(dst, kUcharMaxUtf8, (char32_t)unit);
#else
	return __SPRT_LIBC(c16rtomb)(a, c, (::mbstate_t *)st);
#endif
}

__SPRT_C_FUNC __SPRT_ID(size_t)
		__SPRT_ID(mbrtoc32)(__SPRT_ID(char32_t) * __SPRT_RESTRICT a, const char *__SPRT_RESTRICT b,
				__SPRT_ID(size_t) s, __SPRT_MBSTATE_NAME *__SPRT_RESTRICT st) {
#if SPRT_APPLE
	auto state = ucharState(st);
	// A null source resets to the initial conversion state (decode an embedded
	// NUL): completes with 0, or reports EILSEQ if a partial sequence was pending.
	const char *src = b ? b : "";
	size_t srcLen = b ? s : 1;
	char32_t cp = 0;
	size_t r = __mbrtocp(&cp, src, srcLen, state);
	if (r == (size_t)-1 || r == (size_t)-2) {
		return r;
	}
	if (a) {
		*a = cp;
	}
	return r;
#else
	return __SPRT_LIBC(mbrtoc32)(a, b, s, (::mbstate_t *)st);
#endif
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(c32rtomb)(char *__SPRT_RESTRICT a, __SPRT_ID(char32_t) c,
		__SPRT_MBSTATE_NAME *__SPRT_RESTRICT st) {
#if SPRT_APPLE
	(void)st;
	if (!a) {
		return 1;
	}
	// Only valid Unicode scalar values encode; reject surrogates and out-of-range
	// code points instead of emitting the runtime's extended (5/6-byte) UTF-8.
	if (c > 0x10'FFFFu || (c >= 0xD800u && c <= 0xDFFFu)) {
		__sprt_errno = EILSEQ;
		return (size_t)-1;
	}
	return unicode::utf8EncodeBuf(a, kUcharMaxUtf8, c);
#else
	return __SPRT_LIBC(c32rtomb)(a, c, (::mbstate_t *)st);
#endif
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(mbrlen)(const char *__SPRT_RESTRICT a,
		__SPRT_ID(size_t) c, __SPRT_MBSTATE_NAME *__SPRT_RESTRICT state) {
	return __SPRT_LIBC(mbrlen)(a, c, (::mbstate_t *)state);
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(mbsrtowcs)(__SPRT_ID(wchar_t) * __SPRT_RESTRICT a,
		const char **__SPRT_RESTRICT ret, __SPRT_ID(size_t) s,
		__SPRT_MBSTATE_NAME *__SPRT_RESTRICT state) {
	return __SPRT_LIBC(mbsrtowcs)(a, ret, s, (::mbstate_t *)state);
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(wcsrtombs)(char *__SPRT_RESTRICT a,
		const __SPRT_ID(wchar_t) * *__SPRT_RESTRICT ret, __SPRT_ID(size_t) s,
		__SPRT_MBSTATE_NAME *__SPRT_RESTRICT state) {
	return __SPRT_LIBC(wcsrtombs)(a, ret, s, (::mbstate_t *)state);
}

__SPRT_C_FUNC float __SPRT_ID(wcstof)(const __SPRT_ID(wchar_t) * __SPRT_RESTRICT a,
		__SPRT_ID(wchar_t) * *__SPRT_RESTRICT ret) {
	return __SPRT_LIBC(wcstof)(a, ret);
}

__SPRT_C_FUNC double __SPRT_ID(wcstod)(const __SPRT_ID(wchar_t) * __SPRT_RESTRICT a,
		__SPRT_ID(wchar_t) * *__SPRT_RESTRICT ret) {
	return __SPRT_LIBC(wcstod)(a, ret);
}

__SPRT_C_FUNC long double __SPRT_ID(wcstold)(const __SPRT_ID(wchar_t) * __SPRT_RESTRICT a,
		__SPRT_ID(wchar_t) * *__SPRT_RESTRICT ret) {
	return __SPRT_LIBC(wcstold)(a, ret);
}

__SPRT_C_FUNC long __SPRT_ID(wcstol)(const __SPRT_ID(wchar_t) * __SPRT_RESTRICT a,
		__SPRT_ID(wchar_t) * *__SPRT_RESTRICT ret, int base) {
	return __SPRT_LIBC(wcstol)(a, ret, base);
}

__SPRT_C_FUNC unsigned long __SPRT_ID(wcstoul)(const __SPRT_ID(wchar_t) * __SPRT_RESTRICT a,
		__SPRT_ID(wchar_t) * *__SPRT_RESTRICT ret, int base) {
	return __SPRT_LIBC(wcstoul)(a, ret, base);
}

__SPRT_C_FUNC long long __SPRT_ID(wcstoll)(const __SPRT_ID(wchar_t) * __SPRT_RESTRICT a,
		__SPRT_ID(wchar_t) * *__SPRT_RESTRICT ret, int base) {
	return __SPRT_LIBC(wcstoll)(a, ret, base);
}

__SPRT_C_FUNC unsigned long long __SPRT_ID(wcstoull)(const __SPRT_ID(wchar_t) * __SPRT_RESTRICT a,
		__SPRT_ID(wchar_t) * *__SPRT_RESTRICT ret, int base) {
	return __SPRT_LIBC(wcstoull)(a, ret, base);
}

__SPRT_C_FUNC int __SPRT_ID(fwide)(__SPRT_ID(FILE) * f, int c) { return __SPRT_LIBC(fwide)(f, c); }

__SPRT_C_FUNC int __SPRT_ID(wprintf)(const __SPRT_ID(wchar_t) * __SPRT_RESTRICT fmt, ...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = ::vwprintf(fmt, list);

	__sprt_va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(fwprintf)(__SPRT_ID(FILE) * __SPRT_RESTRICT f,
		const __SPRT_ID(wchar_t) * __SPRT_RESTRICT fmt, ...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = ::vfwprintf(f, fmt, list);

	__sprt_va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(swprintf)(__SPRT_ID(wchar_t) * __SPRT_RESTRICT buf,
		__SPRT_ID(size_t) size, const __SPRT_ID(wchar_t) * __SPRT_RESTRICT fmt, ...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = ::vswprintf(buf, size, fmt, list);

	__sprt_va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(
		vwprintf)(const __SPRT_ID(wchar_t) * __SPRT_RESTRICT fmt, __sprt_va_list list) {
	return __SPRT_LIBC(vwprintf)(fmt, list);
}

__SPRT_C_FUNC int __SPRT_ID(vfwprintf)(__SPRT_ID(FILE) * __SPRT_RESTRICT f,
		const __SPRT_ID(wchar_t) * __SPRT_RESTRICT fmt, __sprt_va_list list) {
	return __SPRT_LIBC(vfwprintf)(f, fmt, list);
}

__SPRT_C_FUNC int __SPRT_ID(vswprintf)(__SPRT_ID(wchar_t) * __SPRT_RESTRICT buf,
		__SPRT_ID(size_t) size, const __SPRT_ID(wchar_t) * __SPRT_RESTRICT fmt,
		__sprt_va_list list) {
	return __SPRT_LIBC(vswprintf)(buf, size, fmt, list);
}

__SPRT_C_FUNC int __SPRT_ID(wscanf)(const __SPRT_ID(wchar_t) * __SPRT_RESTRICT fmt, ...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = ::vwscanf(fmt, list);

	__sprt_va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(fwscanf)(__SPRT_ID(FILE) * __SPRT_RESTRICT f,
		const __SPRT_ID(wchar_t) * __SPRT_RESTRICT fmt, ...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = ::vfwscanf(f, fmt, list);

	__sprt_va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(swscanf)(const __SPRT_ID(wchar_t) * __SPRT_RESTRICT buf,
		const __SPRT_ID(wchar_t) * __SPRT_RESTRICT fmt, ...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = ::vswscanf(buf, fmt, list);

	__sprt_va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(
		vwscanf)(const __SPRT_ID(wchar_t) * __SPRT_RESTRICT fmt, __sprt_va_list list) {
	return __SPRT_LIBC(vwscanf)(fmt, list);
}

__SPRT_C_FUNC int __SPRT_ID(vfwscanf)(__SPRT_ID(FILE) * __SPRT_RESTRICT f,
		const __SPRT_ID(wchar_t) * __SPRT_RESTRICT fmt, __sprt_va_list list) {
	return __SPRT_LIBC(vfwscanf)(f, fmt, list);
}

__SPRT_C_FUNC int __SPRT_ID(vswscanf)(const __SPRT_ID(wchar_t) * __SPRT_RESTRICT buf,
		const __SPRT_ID(wchar_t) * __SPRT_RESTRICT fmt, __sprt_va_list list) {
	return __SPRT_LIBC(vswscanf)(buf, fmt, list);
}

__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(fgetwc)(__SPRT_ID(FILE) * f) { return __SPRT_LIBC(fgetwc)(f); }
__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(getwc)(__SPRT_ID(FILE) * f) { return __SPRT_LIBC(getwc)(f); }
__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(getwchar)(void) { return __SPRT_LIBC(getwchar)(); }

__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(fputwc)(__SPRT_ID(wchar_t) c, __SPRT_ID(FILE) * f) {
	return __SPRT_LIBC(fputwc)(c, f);
}
__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(putwc)(__SPRT_ID(wchar_t) c, __SPRT_ID(FILE) * f) {
	return __SPRT_LIBC(putwc)(c, f);
}
__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(putwchar)(__SPRT_ID(wchar_t) c) { return __SPRT_LIBC(putwchar)(c); }

__SPRT_C_FUNC __SPRT_ID(wchar_t)
		* __SPRT_ID(fgetws)(__SPRT_ID(wchar_t) * __SPRT_RESTRICT a, int c,
				__SPRT_ID(FILE) * __SPRT_RESTRICT f) {
	return __SPRT_LIBC(fgetws)(a, c, f);
}
__SPRT_C_FUNC int __SPRT_ID(
		fputws)(const __SPRT_ID(wchar_t) * __SPRT_RESTRICT a, __SPRT_ID(FILE) * __SPRT_RESTRICT f) {
	return __SPRT_LIBC(fputws)(a, f);
}

__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(ungetwc)(__SPRT_ID(wint_t) c, __SPRT_ID(FILE) * f) {
	return __SPRT_LIBC(ungetwc)(c, f);
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(wcsftime)(__SPRT_ID(wchar_t) * __SPRT_RESTRICT a,
		__SPRT_ID(size_t) s, const __SPRT_ID(wchar_t) * __SPRT_RESTRICT b,
		const struct __SPRT_TM_NAME *__SPRT_RESTRICT _tm) {
#if __STDC_HOSTED__ == 0
	return __SPRT_LIBC(wcsftime)(a, s, b, _tm);
#else
	auto native = internal::getNativeTm(_tm);

	return __SPRT_LIBC(wcsftime)(a, s, b, &native);
#endif
}

__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(fgetwc_unlocked)(__SPRT_ID(FILE) * f) {
#if SPRT_ANDROID || SPRT_APPLE || SPRT_EMBOX
	return ::fgetwc(f);
#else
	return ::fgetwc_unlocked(f);
#endif
}

__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(getwc_unlocked)(__SPRT_ID(FILE) * f) {
#if SPRT_ANDROID || SPRT_APPLE || SPRT_EMBOX
	return ::getwc(f);
#else
	return ::getwc_unlocked(f);
#endif
}

__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(getwchar_unlocked)(void) {
#if SPRT_ANDROID || SPRT_APPLE || SPRT_EMBOX
	return ::getwchar();
#else
	return ::getwchar_unlocked();
#endif
}

__SPRT_C_FUNC __SPRT_ID(wint_t)
		__SPRT_ID(fputwc_unlocked)(__SPRT_ID(wchar_t) c, __SPRT_ID(FILE) * f) {
#if SPRT_ANDROID || SPRT_APPLE || SPRT_EMBOX
	return ::fputwc(c, f);
#else
	return ::fputwc_unlocked(c, f);
#endif
}

__SPRT_C_FUNC __SPRT_ID(wint_t)
		__SPRT_ID(putwc_unlocked)(__SPRT_ID(wchar_t) c, __SPRT_ID(FILE) * f) {
#if SPRT_ANDROID || SPRT_APPLE || SPRT_EMBOX
	return ::putwc(c, f);
#else
	return ::putwc_unlocked(c, f);
#endif
}

__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(putwchar_unlocked)(__SPRT_ID(wchar_t) c) {
#if SPRT_ANDROID || SPRT_APPLE || SPRT_EMBOX
	return putwchar(c);
#else
	return putwchar_unlocked(c);
#endif
}

__SPRT_C_FUNC __SPRT_ID(wchar_t)
		* __SPRT_ID(fgetws_unlocked)(__SPRT_ID(wchar_t) * __SPRT_RESTRICT ptr, int c,
				__SPRT_ID(FILE) * __SPRT_RESTRICT f) {
#if SPRT_ANDROID || SPRT_APPLE || SPRT_EMBOX
	return ::fgetws(ptr, c, f);
#else
	return fgetws_unlocked(ptr, c, f);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(fputws_unlocked)(const __SPRT_ID(wchar_t) * __SPRT_RESTRICT ptr,
		__SPRT_ID(FILE) * __SPRT_RESTRICT f) {
#if SPRT_ANDROID || SPRT_APPLE || SPRT_EMBOX
	return ::fputws(ptr, f);
#else
	return fputws_unlocked(ptr, f);
#endif
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(wcsftime_l)(__SPRT_ID(wchar_t) * __SPRT_RESTRICT ptr,
		__SPRT_ID(size_t) size, const __SPRT_ID(wchar_t) * __SPRT_RESTRICT fmt,
		const struct __SPRT_TM_NAME *__SPRT_RESTRICT _tm, __SPRT_ID(locale_t) loc) {
#if __STDC_HOSTED__ == 0
	return __SPRT_LIBC(wcsftime_l)(ptr, size, fmt, _tm, loc);
#else
	auto native = internal::getNativeTm(_tm);
#if SPRT_ANDROID
	if (platform::_wcsftime_l) {
		return platform::_wcsftime_l(ptr, size, fmt, &native, loc);
	}
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (Android: API not available)");
	*__sprt___errno_location() = ENOSYS;
	return 0;
#elif SPRT_EMBOX
	(void)loc;
	return ::wcsftime(ptr, size, fmt, &native);
#else
	return __SPRT_LIBC(wcsftime_l)(ptr, size, fmt, &native, loc);
#endif
#endif
}

__SPRT_C_FUNC __SPRT_ID(FILE)
		* __SPRT_ID(open_wmemstream)(__SPRT_ID(wchar_t) * *ptr, __SPRT_ID(size_t) * s) {
#if __SPRT_CONFIG_HAVE_STDIO_OPEN_MEMSTREAM
	return ::open_wmemstream(ptr, s);
#else
	__sprt_errno = ENOSYS;
	return nullptr;
#endif
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(mbsnrtowcs)(__SPRT_ID(wchar_t) * __SPRT_RESTRICT dest,
		const char **__SPRT_RESTRICT src, __SPRT_ID(size_t) count, __SPRT_ID(size_t) destSize,
		__SPRT_MBSTATE_NAME *__SPRT_RESTRICT state) {
	return __SPRT_LIBC(mbsnrtowcs)(dest, src, count, destSize, (mbstate_t *)state);
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(wcsnrtombs)(char *__SPRT_RESTRICT dest,
		const __SPRT_ID(wchar_t) **__SPRT_RESTRICT src, __SPRT_ID(size_t) count,
		__SPRT_ID(size_t) destSize, __SPRT_MBSTATE_NAME *__SPRT_RESTRICT state) {
	return __SPRT_LIBC(wcsnrtombs)(dest, src, count, destSize, (mbstate_t *)state);
}

__SPRT_C_FUNC __SPRT_ID(wchar_t) * __SPRT_ID(wcsdup)(const __SPRT_ID(wchar_t) * ptr) {
	return __SPRT_LIBC(wcsdup)(ptr);
}

__SPRT_C_FUNC __SPRT_ID(size_t)
		__SPRT_ID(wcsnlen)(const __SPRT_ID(wchar_t) * ptr, __SPRT_ID(size_t) len) {
	return __SPRT_LIBC(wcsnlen)(ptr, len);
}

__SPRT_C_FUNC __SPRT_ID(wchar_t)
		* __SPRT_ID(wcpcpy)(__SPRT_ID(wchar_t) * __SPRT_RESTRICT ptr,
				const __SPRT_ID(wchar_t) * __SPRT_RESTRICT buf) {
	return __SPRT_LIBC(wcpcpy)(ptr, buf);
}

__SPRT_C_FUNC __SPRT_ID(wchar_t)
		* __SPRT_ID(wcpncpy)(__SPRT_ID(wchar_t) * __SPRT_RESTRICT a,
				const __SPRT_ID(wchar_t) * __SPRT_RESTRICT b, __SPRT_ID(size_t) size) {
	return __SPRT_LIBC(wcpncpy)(a, b, size);
}

__SPRT_C_FUNC int __SPRT_ID(
		wcscasecmp)(const __SPRT_ID(wchar_t) * a, const __SPRT_ID(wchar_t) * b) {
#if SPRT_EMBOX
	return ::wcsncasecmp(a, b, static_cast<size_t>(-1));
#else
	return __SPRT_LIBC(wcscasecmp)(a, b);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(wcscasecmp_l)(const __SPRT_ID(wchar_t) * a,
		const __SPRT_ID(wchar_t) * b, __SPRT_ID(locale_t) loc) {
#if SPRT_EMBOX
	(void)loc;
	return ::wcsncasecmp(a, b, static_cast<size_t>(-1));
#else
	return ::wcscasecmp_l(a, b, loc);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(wcsncasecmp)(const __SPRT_ID(wchar_t) * a, const __SPRT_ID(wchar_t) * b,
		__SPRT_ID(size_t) s) {
	return ::wcsncasecmp(a, b, s);
}

__SPRT_C_FUNC int __SPRT_ID(wcsncasecmp_l)(const __SPRT_ID(wchar_t) * a,
		const __SPRT_ID(wchar_t) * b, __SPRT_ID(size_t) s, __SPRT_ID(locale_t) loc) {
#if SPRT_EMBOX
	(void)loc;
	return ::wcsncasecmp(a, b, s);
#else
	return ::wcsncasecmp_l(a, b, s, loc);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(wcscoll_l)(const __SPRT_ID(wchar_t) * a, const __SPRT_ID(wchar_t) * b,
		__SPRT_ID(locale_t) loc) {
#if SPRT_EMBOX
	(void)loc;
	return ::wcscoll(a, b);
#else
	return __SPRT_LIBC(wcscoll_l)(a, b, loc);
#endif
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(wcsxfrm_l)(__SPRT_ID(wchar_t) * __SPRT_RESTRICT a,
		const __SPRT_ID(wchar_t) * __SPRT_RESTRICT b, __SPRT_ID(size_t) s,
		__SPRT_ID(locale_t) loc) {
#if SPRT_EMBOX
	(void)loc;
	return ::wcsxfrm(a, b, s);
#else
	return __SPRT_LIBC(wcsxfrm_l)(a, b, s, loc);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(wcwidth)(__SPRT_ID(wchar_t) c) {
#if SPRT_EMBOX
	if (c == 0) {
		return 0;
	}
	return ::iswprint(static_cast<wint_t>(c)) ? 1 : -1;
#else
	return __SPRT_LIBC(wcwidth)(c);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(wcswidth)(const __SPRT_ID(wchar_t) * ptr, __SPRT_ID(size_t) s) {
#if SPRT_EMBOX
	int total = 0;
	for (__SPRT_ID(size_t) i = 0; i < s && ptr && ptr[i]; ++i) {
		int w = __SPRT_ID(wcwidth)(ptr[i]);
		if (w < 0) {
			return -1;
		}
		total += w;
	}
	return total;
#else
	return __SPRT_LIBC(wcswidth)(ptr, s);
#endif
}

__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(towlower)(__SPRT_ID(wint_t) wc) { return __SPRT_LIBC(towlower)(wc); }
__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(towupper)(__SPRT_ID(wint_t) wc) { return __SPRT_LIBC(towupper)(wc); }

__SPRT_C_FUNC int __SPRT_ID(iswalnum)(__SPRT_ID(wint_t) wc) { return __SPRT_LIBC(iswalnum)(wc); }
__SPRT_C_FUNC int __SPRT_ID(iswalpha)(__SPRT_ID(wint_t) wc) { return __SPRT_LIBC(iswalpha)(wc); }
__SPRT_C_FUNC int __SPRT_ID(iswblank)(__SPRT_ID(wint_t) wc) { return __SPRT_LIBC(iswblank)(wc); }
__SPRT_C_FUNC int __SPRT_ID(iswcntrl)(__SPRT_ID(wint_t) wc) { return __SPRT_LIBC(iswcntrl)(wc); }
__SPRT_C_FUNC int __SPRT_ID(iswdigit)(__SPRT_ID(wint_t) wc) { return __SPRT_LIBC(iswdigit)(wc); }
__SPRT_C_FUNC int __SPRT_ID(iswgraph)(__SPRT_ID(wint_t) wc) { return __SPRT_LIBC(iswgraph)(wc); }
__SPRT_C_FUNC int __SPRT_ID(iswlower)(__SPRT_ID(wint_t) wc) { return __SPRT_LIBC(iswlower)(wc); }
__SPRT_C_FUNC int __SPRT_ID(iswprint)(__SPRT_ID(wint_t) wc) { return __SPRT_LIBC(iswprint)(wc); }
__SPRT_C_FUNC int __SPRT_ID(iswpunct)(__SPRT_ID(wint_t) wc) { return __SPRT_LIBC(iswpunct)(wc); }
__SPRT_C_FUNC int __SPRT_ID(iswspace)(__SPRT_ID(wint_t) wc) { return __SPRT_LIBC(iswspace)(wc); }
__SPRT_C_FUNC int __SPRT_ID(iswupper)(__SPRT_ID(wint_t) wc) { return __SPRT_LIBC(iswupper)(wc); }
__SPRT_C_FUNC int __SPRT_ID(iswxdigit)(__SPRT_ID(wint_t) wc) { return __SPRT_LIBC(iswxdigit)(wc); }

__SPRT_C_FUNC int __SPRT_ID(iswctype)(__SPRT_ID(wint_t) wc, __SPRT_ID(wctype_t) t) {
	return __SPRT_LIBC(iswctype)(wc, t);
}
__SPRT_C_FUNC __SPRT_ID(wctype_t) __SPRT_ID(wctype)(const char *name) { return __SPRT_LIBC(wctype)(name); }

__SPRT_C_FUNC int __SPRT_ID(iswalnum_l)(__SPRT_ID(wint_t) wc, __SPRT_ID(locale_t) loc) {
#if SPRT_EMBOX
	(void)loc;
	return ::iswalnum(wc);
#else
	return __SPRT_LIBC(iswalnum_l)(wc, loc);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(iswalpha_l)(__SPRT_ID(wint_t) wc, __SPRT_ID(locale_t) loc) {
#if SPRT_EMBOX
	(void)loc;
	return ::iswalpha(wc);
#else
	return __SPRT_LIBC(iswalpha_l)(wc, loc);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(iswblank_l)(__SPRT_ID(wint_t) wc, __SPRT_ID(locale_t) loc) {
#if SPRT_EMBOX
	(void)loc;
	return ::iswblank(wc);
#else
	return __SPRT_LIBC(iswblank_l)(wc, loc);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(iswcntrl_l)(__SPRT_ID(wint_t) wc, __SPRT_ID(locale_t) loc) {
#if SPRT_EMBOX
	(void)loc;
	return ::iswcntrl(wc);
#else
	return __SPRT_LIBC(iswcntrl_l)(wc, loc);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(iswdigit_l)(__SPRT_ID(wint_t) wc, __SPRT_ID(locale_t) loc) {
#if SPRT_EMBOX
	(void)loc;
	return ::iswdigit(wc);
#else
	return __SPRT_LIBC(iswdigit_l)(wc, loc);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(iswgraph_l)(__SPRT_ID(wint_t) wc, __SPRT_ID(locale_t) loc) {
#if SPRT_EMBOX
	(void)loc;
	return ::iswgraph(wc);
#else
	return __SPRT_LIBC(iswgraph_l)(wc, loc);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(iswlower_l)(__SPRT_ID(wint_t) wc, __SPRT_ID(locale_t) loc) {
#if SPRT_EMBOX
	(void)loc;
	return ::iswlower(wc);
#else
	return __SPRT_LIBC(iswlower_l)(wc, loc);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(iswprint_l)(__SPRT_ID(wint_t) wc, __SPRT_ID(locale_t) loc) {
#if SPRT_EMBOX
	(void)loc;
	return ::iswprint(wc);
#else
	return __SPRT_LIBC(iswprint_l)(wc, loc);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(iswpunct_l)(__SPRT_ID(wint_t) wc, __SPRT_ID(locale_t) loc) {
#if SPRT_EMBOX
	(void)loc;
	return ::iswpunct(wc);
#else
	return __SPRT_LIBC(iswpunct_l)(wc, loc);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(iswspace_l)(__SPRT_ID(wint_t) wc, __SPRT_ID(locale_t) loc) {
#if SPRT_EMBOX
	(void)loc;
	return ::iswspace(wc);
#else
	return __SPRT_LIBC(iswspace_l)(wc, loc);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(iswupper_l)(__SPRT_ID(wint_t) wc, __SPRT_ID(locale_t) loc) {
#if SPRT_EMBOX
	(void)loc;
	return ::iswupper(wc);
#else
	return __SPRT_LIBC(iswupper_l)(wc, loc);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(iswxdigit_l)(__SPRT_ID(wint_t) wc, __SPRT_ID(locale_t) loc) {
#if SPRT_EMBOX
	(void)loc;
	return ::iswxdigit(wc);
#else
	return ::iswxdigit_l(wc, loc);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(
		iswctype_l)(__SPRT_ID(wint_t) wc, __SPRT_ID(wctype_t) t, __SPRT_ID(locale_t) loc) {
#if SPRT_EMBOX
	(void)loc;
	return ::iswctype(wc, t);
#else
	return __SPRT_LIBC(iswctype_l)(wc, t, loc);
#endif
}
__SPRT_C_FUNC __SPRT_ID(wint_t)
		__SPRT_ID(towlower_l)(__SPRT_ID(wint_t) wc, __SPRT_ID(locale_t) loc) {
#if SPRT_EMBOX
	(void)loc;
	return ::towlower(wc);
#else
	return __SPRT_LIBC(towlower_l)(wc, loc);
#endif
}
__SPRT_C_FUNC __SPRT_ID(wint_t)
		__SPRT_ID(towupper_l)(__SPRT_ID(wint_t) wc, __SPRT_ID(locale_t) loc) {
#if SPRT_EMBOX
	(void)loc;
	return ::towupper(wc);
#else
	return __SPRT_LIBC(towupper_l)(wc, loc);
#endif
}
__SPRT_C_FUNC __SPRT_ID(wctype_t) __SPRT_ID(wctype_l)(const char *name, __SPRT_ID(locale_t) loc) {
#if SPRT_EMBOX
	(void)loc;
	return ::wctype(name);
#else
	return __SPRT_LIBC(wctype_l)(name, loc);
#endif
}

// wctrans/towctrans (and their _l variants) share the pointer-based wctrans_t
// ABI, so on most targets they forward straight to the platform libc (hosted) or
// libc_impl (freestanding). Android is special: Bionic added these only in API
// 26, so they are resolved at runtime (jni.cc) and, when the device is older,
// fall back to the shared runtime_core impl (the same one libc_impl uses). Bionic
// types wctrans_t as `const void *` whereas the SPRT ABI uses `const int *`, so
// the platform forwards cast between the two.
__SPRT_C_FUNC __SPRT_ID(wctrans_t) __SPRT_ID(wctrans)(const char *name) {
#if SPRT_ANDROID
	if (platform::_wctrans) {
		return (__SPRT_ID(wctrans_t))platform::_wctrans(name);
	}
	return __wctrans_fallback(name);
#elif SPRT_HOSTED_RTOS
	// NuttX wctrans_t is `int`, sprt's ABI is `const int *`. Round-trip through
	// the integer value so the call type-checks.
	return (__SPRT_ID(wctrans_t))(intptr_t)__SPRT_LIBC(wctrans)(name);
#else
	return __SPRT_LIBC(wctrans)(name);
#endif
}
__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(towctrans)(__SPRT_ID(wint_t) wc, __SPRT_ID(wctrans_t) t) {
#if SPRT_ANDROID
	if (platform::_towctrans) {
		return platform::_towctrans(wc, (wctrans_t)t);
	}
	return __towctrans_fallback(wc, t);
#elif SPRT_HOSTED_RTOS
	return __SPRT_LIBC(towctrans)(wc, (wctrans_t)(intptr_t)t);
#else
	return __SPRT_LIBC(towctrans)(wc, t);
#endif
}
__SPRT_C_FUNC __SPRT_ID(wctrans_t) __SPRT_ID(wctrans_l)(const char *name, __SPRT_ID(locale_t) loc) {
#if SPRT_ANDROID
	if (platform::_wctrans_l) {
		return (__SPRT_ID(wctrans_t))platform::_wctrans_l(name, loc);
	}
	return __wctrans_fallback(name);
#elif SPRT_HOSTED_RTOS
	(void)loc;
	return (__SPRT_ID(wctrans_t))(intptr_t)::wctrans(name);
#else
	return __SPRT_LIBC(wctrans_l)(name, loc);
#endif
}
__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(
		towctrans_l)(__SPRT_ID(wint_t) wc, __SPRT_ID(wctrans_t) t, __SPRT_ID(locale_t) loc) {
#if SPRT_ANDROID
	if (platform::_towctrans_l) {
		return platform::_towctrans_l(wc, (wctrans_t)t, loc);
	}
	return __towctrans_fallback(wc, t);
#elif SPRT_HOSTED_RTOS
	(void)loc;
	return ::towctrans(wc, (wctrans_t)(intptr_t)t);
#else
	return ::towctrans_l(wc, t, loc);
#endif
}

} // namespace sprt
