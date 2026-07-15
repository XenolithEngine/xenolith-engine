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

#include <wchar.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sprt/cxx/detail/ctypes.h>
#include <sprt/runtime/unicode.h>

namespace sprt {

// Defined in builtin_locale.cpp: MB_CUR_MAX of the effective LC_CTYPE locale
// (1 for the single-byte "C"/"POSIX" locale, 4 for UTF-8). The mb/wc conversion
// functions below switch between a single-byte identity map and UTF-8 on it, so
// the "C" locale is genuinely single-byte (ISO C) while *.UTF-8 stays multibyte.
size_t __get_effective_mb_cur_max();

// Largest number of bytes a single valid Unicode scalar value (<= U+10FFFF)
// occupies in UTF-8. Must not exceed MB_LEN_MAX (4): callers size their
// conversion buffers `MB_LEN_MAX`, and the C standard requires
// MB_CUR_MAX <= MB_LEN_MAX.
static constexpr size_t _MB_CUR_MAX = 4;

static constexpr uint16_t STATE_NONE = 0;
static constexpr uint16_t STATE_EXPECT_LOW_SURROGATE = 1;

thread_local mbstate_t tl_wctomb_state = {STATE_NONE, 0};
thread_local mbstate_t tl_wcsrtombs_state = {STATE_NONE, 0};
thread_local mbstate_t tl_mbtowc_state = {STATE_NONE, 0};
thread_local mbstate_t tl_mbsrtowcs_state = {STATE_NONE, 0};

static size_t __wcrtomb(char *__SPRT_RESTRICT __dst, size_t __dstSize, wchar_t __src,
		mbstate_t *__SPRT_RESTRICT state) __SPRT_NOEXCEPT {
	if (!state) {
		state = &tl_wctomb_state;
	}

	if (__src == 0) {
		if (state->_State == STATE_NONE) {
			if (__dstSize > 0) {
				__dst[0] = 0;
			}
			return 1;
		} else {
			state->_State = STATE_NONE;
			return -2;
		}
	}

	if (__get_effective_mb_cur_max() == 1) {
		// Single-byte "C"/"POSIX" locale: identity map for scalar values 0..255,
		// EILSEQ above that. No shift state, so it bypasses the surrogate machine.
		char32_t __c = (char32_t)__src;
		if (__c > 0xFF) {
			errno = EILSEQ;
			return -1;
		}
		if (__dst) {
			if (__dstSize < 1) {
				errno = EOVERFLOW;
				return -1;
			}
			__dst[0] = (char)(unsigned char)__c;
		}
		return 1;
	}

	if constexpr (sizeof(wchar_t) == sizeof(char16_t)) {
		switch (state->_State) {
		case STATE_NONE:
			if (unicode::isUtf16HighSurrogate(__src)) [[unlikely]] {
				state->_State = STATE_EXPECT_LOW_SURROGATE;
				state->_Char = __src;
				return -2;
			} else {
				auto len = unicode::utf8EncodeLength(char16_t(__src));
				if (__dst) {
					if (__dstSize >= len) {
						return unicode::utf8EncodeBuf(__dst, __dstSize, char16_t(__src));
					} else {
						errno = EOVERFLOW;
						return -1;
					}
				}
				return len;
			}
		case STATE_EXPECT_LOW_SURROGATE:
			if (unicode::isUtf16LowSurrogate(__src)) {
				auto char32 = unicode::utf16CombineSurrogates(state->_Char, __src);
				state->_State = STATE_NONE;
				state->_Char = 0;

				auto len = unicode::utf8EncodeLength(char32);
				if (__dst) {
					if (__dstSize >= len) {
						return unicode::utf8EncodeBuf(__dst, __dstSize, char32);
					} else {
						errno = EOVERFLOW;
						return -1;
					}
				}
				return len;
			} else {
				state->_State = STATE_NONE;
				state->_Char = 0;
				errno = EILSEQ;
				return -1;
			}
			break;
		default:
			state->_State = STATE_NONE;
			state->_Char = 0;
			errno = EILSEQ;
			return -1;
		}
	} else {
		char32_t c = char32_t(__src);
		// The C multibyte functions accept only valid Unicode scalar values.
		// The sprt::unicode backend also understands the obsolete 5/6-byte
		// UTF-8 forms (code points above U+10FFFF), but those are not valid
		// characters here — reject them rather than emit > MB_CUR_MAX bytes.
		if (c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) {
			errno = EILSEQ;
			return -1;
		}
		auto len = unicode::utf8EncodeLength(c);
		if (__dst) {
			if (__dstSize >= len) {
				return unicode::utf8EncodeBuf(__dst, __dstSize, c);
			} else {
				errno = EOVERFLOW;
				return -1;
			}
		}
		return len;
	}
}

__SPRT_C_FUNC size_t wcrtomb(char *__SPRT_RESTRICT __dst, wchar_t __src,
		mbstate_t *__SPRT_RESTRICT state) __SPRT_NOEXCEPT {
	// Callers provide a buffer of at most MB_CUR_MAX (== _MB_CUR_MAX) bytes.
	// Promise exactly that so a code point requiring more (an invalid scalar
	// value above U+10FFFF) is rejected with EOVERFLOW instead of overrunning
	// the caller's MB_LEN_MAX-sized buffer.
	auto ret = __wcrtomb(__dst, _MB_CUR_MAX, __src, state);
	if (ret == -2) {
		return 0;
	}
	return ret;
}

__SPRT_C_FUNC int wctomb(char *a, wchar_t c) __SPRT_NOEXCEPT {
	return wcrtomb(a, c, &tl_wctomb_state);
}

__SPRT_C_FUNC size_t wcsnrtombs(char *__SPRT_RESTRICT __dst, const wchar_t **__SPRT_RESTRICT __src,
		size_t __ssize, size_t __dsize, mbstate_t *__SPRT_RESTRICT __ps) __SPRT_NOEXCEPT {
	if (!__src || !*__src) {
		errno = EINVAL;
		return -1;
	}

	if (__ps == nullptr) {
		__ps = &tl_wcsrtombs_state;
	}

	if (__dst == nullptr) {
		__dsize = Max<size_t>;
	}

	size_t ret = 0;
	while (**__src != 0 && __dsize > 0 && __ssize > 0) {
		auto converted = __wcrtomb(__dst, __dsize, **__src, __ps);
		--__ssize;
		if (converted == -1) {
			return -1;
		} else if (converted == -2) {
			return ret;
		}
		(*__src) += 1;
		if (__dst) {
			__dst += converted;
		}
		ret += converted;
		__dsize -= converted;
	}

	__ps->_Char = 0;
	__ps->_State = 0;

	if (__dsize > 0) {
		if (__dst) {
			*__dst = 0;
		}
	}
	return ret;
}

__SPRT_C_FUNC size_t wcsrtombs(char *__SPRT_RESTRICT __dst, const wchar_t **__SPRT_RESTRICT __src,
		size_t __dsize, mbstate_t *__SPRT_RESTRICT __ps) __SPRT_NOEXCEPT {
	return wcsnrtombs(__dst, __src, Max<size_t>, __dsize, __ps);
}

__SPRT_C_FUNC size_t wcstombs(char *__dst, const wchar_t *__src, size_t __dsize) __SPRT_NOEXCEPT {
	mbstate_t state = {0, 0};
	return wcsrtombs(__dst, &__src, __dsize, &state);
}

static size_t __mbrtowc(wchar_t *__SPRT_RESTRICT __dst, size_t __dstLen,
		char const * __SPRT_RESTRICT * __src, size_t *__srcLen,
		mbstate_t *__SPRT_RESTRICT state) __SPRT_NOEXCEPT {
	if (!state) {
		state = &tl_mbtowc_state;
	}

	// If src is not fefined - return 0 for complete state, or -1 for incomplete state
	if (!__src || !*__src) {
		if (state->_State == STATE_NONE) {
			state->_Char = 0;
			return 0;
		} else {
			errno = EILSEQ;
			state->_State = STATE_NONE;
			return -1;
		}
	}

	auto &ptr = *__src;

	// No bytes available to examine (e.g. mbrtowc(pwc, s, 0, st)). Report an
	// incomplete sequence instead of reading `*ptr` and decrementing the
	// (zero) source length, which would underflow to SIZE_MAX and walk the
	// continuation loop off the end of the buffer.
	if (*__srcLen == 0) {
		return -2;
	}

	if (*ptr == 0) {
		if (state->_State == STATE_NONE) {
			return 0;
		} else {
			return -2;
		}
	}

	if (__get_effective_mb_cur_max() == 1) {
		// Single-byte "C"/"POSIX" locale: each byte is one character (identity
		// 0..255), so one byte is consumed and one wide unit produced.
		unsigned char __b = (unsigned char)*ptr;
		++ptr;
		--(*__srcLen);
		if (__dst) {
			if (__dstLen < 1) {
				errno = EOVERFLOW;
				return -1;
			}
			*__dst = (wchar_t)__b;
		}
		return 1;
	}

	if (state->_State == STATE_NONE) {
		// read first
		uint8_t mask = sprt::unicode::utf8_length_mask[uint8_t(*ptr)];
		state->_State = sprt::unicode::utf8_length_data[uint8_t(*ptr)] - 1;
		state->_Char = (*ptr++) & mask;
		--(*__srcLen);
	}

	while (state->_State > 0 && *__srcLen > 0 && *ptr != 0) {
		if ((*ptr & 0xc0) != 0x80) {
			errno = EILSEQ;
			state->_State = STATE_NONE;
			return -1;
		}
		state->_Char <<= 6;
		state->_Char |= (*ptr++ & 0x3f);
		--state->_State;
		--(*__srcLen);
	}

	if (state->_State > 0 && (*__srcLen == 0 || *ptr == 0)) {
		return -2;
	}

	if constexpr (sizeof(wchar_t) == sizeof(char16_t)) {
		auto len = unicode::utf16EncodeLength(state->_Char);

		if (__dst) {
			if (__dstLen >= len) {
				return unicode::utf16EncodeBuf((char16_t *)__dst, __dstLen, state->_Char);
			} else {
				errno = EOVERFLOW;
				return -1;
			}
		}
		return len;
	} else {
		if (__dst) {
			if (__dstLen > 0) {
				*__dst = (wchar_t)state->_Char;
			} else {
				errno = EOVERFLOW;
				return -1;
			}
		}
		return 1;
	}
}

__SPRT_C_FUNC size_t mbrtowc(wchar_t *__SPRT_RESTRICT __dst, const char *__SPRT_RESTRICT __src,
		size_t __srcLen, mbstate_t *__SPRT_RESTRICT state) __SPRT_NOEXCEPT {
	// The destination is a single `wchar_t`. Promise exactly one slot so that
	// an astral code point (which decodes to a UTF-16 surrogate *pair* when
	// `wchar_t` is 16-bit) is rejected with EOVERFLOW rather than writing two
	// units past the caller's one-element buffer.
	//
	// __mbrtowc reports the number of wchar_t units produced (what mbsnrtowcs
	// consumes); mbrtowc must instead report the number of *bytes* consumed from
	// the source, so derive it from how far __mbrtowc advanced __srcLen. The
	// status returns (0 == NUL, (size_t)-1 == EILSEQ/EOVERFLOW, (size_t)-2 ==
	// incomplete) pass straight through.
	const size_t before = __srcLen;
	const size_t r = __mbrtowc(__dst, 1, &__src, &__srcLen, state);
	if (r == size_t(0) || r == size_t(-1) || r == size_t(-2)) {
		return r;
	}
	return before - __srcLen;
}

__SPRT_C_FUNC int mbtowc(__SPRT_ID(wchar_t) * __wc_ptr, const char *__s,
		size_t __n) __SPRT_NOEXCEPT {
	return mbrtowc(__wc_ptr, __s, __n, &tl_mbtowc_state);
}

// mblen is mbtowc without storing the wide character (the result is the byte
// length of the next multibyte sequence). A NULL `__s` queries whether the
// encoding is state-dependent — it is not (UTF-8), so that returns 0.
__SPRT_C_FUNC int mblen(const char *__s, size_t __n) __SPRT_NOEXCEPT {
	return mbtowc(nullptr, __s, __n);
}

__SPRT_C_FUNC size_t mbsnrtowcs(wchar_t *__SPRT_RESTRICT __dst, const char **__SPRT_RESTRICT __src,
		size_t __ssize, size_t __dsize, mbstate_t *__SPRT_RESTRICT __ps) __SPRT_NOEXCEPT {
	if (!__src || !*__src) {
		errno = EINVAL;
		return -1;
	}

	if (__ps == nullptr) {
		__ps = &tl_mbsrtowcs_state;
	}

	if (__dst == nullptr) {
		__dsize = Max<size_t>;
	}

	size_t ret = 0;
	while (**__src != 0 && __dsize > 0 && __ssize > 0) {
		auto converted = __mbrtowc(__dst, __dsize, __src, &__ssize, __ps);
		if (converted == -1) {
			return -1;
		} else if (converted == -2) {
			return ret;
		}
		if (__dst) {
			__dst += converted;
		}
		__dsize -= converted;
		ret += converted;
	}

	__ps->_Char = 0;
	__ps->_State = 0;

	if (__dsize > 0) {
		if (__dst) {
			*__dst = 0;
		}
	}
	return ret;
}

__SPRT_C_FUNC size_t mbsrtowcs(wchar_t *__restrict __dst, const char **__restrict __src,
		size_t __dsize, mbstate_t *__restrict __ps) __SPRT_NOEXCEPT {
	return mbsnrtowcs(__dst, __src, Max<size_t>, __dsize, __ps);
}

__SPRT_C_FUNC size_t mbstowcs(wchar_t *__dst, const char *__src, size_t __dsize) __SPRT_NOEXCEPT {
	mbstate_t state = {0, 0};
	return mbsrtowcs(__dst, &__src, __dsize, &state);
}

__SPRT_C_FUNC int mbsinit(const mbstate_t *value) __SPRT_NOEXCEPT {
	if (!value || value->_State == STATE_NONE) {
		return 1;
	}
	return 0;
}

__SPRT_C_FUNC size_t __ctype_get_mb_cur_max() __SPRT_NOEXCEPT {
	return __get_effective_mb_cur_max();
}

__SPRT_C_FUNC int wcwidth(wchar_t wc) __SPRT_NOEXCEPT {
	return unicode::utf8EncodeLength(char32_t(wc));
}

__SPRT_C_FUNC int wcswidth(const wchar_t *wcs, size_t n) __SPRT_NOEXCEPT {
	if (!wcs) {
		return 0;
	}
	int ret = 0;
	uint8_t offset = 0;
	while (*wcs && n > 0) {
		if constexpr (sizeof(wchar_t) == sizeof(char16_t)) {
			auto c = unicode::utf16Decode32((const char16_t *)wcs, n, offset);
			if (c) {
				ret += unicode::utf8EncodeLength(c);
				wcs += offset;
				n -= offset;
			} else {
				errno = EILSEQ;
				return -1;
			}
		} else {
			ret += unicode::utf8EncodeLength(char32_t(*wcs++));
			--n;
		}
	}
	return ret;
}

// ---- <uchar.h> char16_t/char32_t conversions ----
// Sentinel stored in mbstate_t::_State by mbrtoc16 to mean "a high surrogate was
// already returned; _Char holds the low surrogate to return on the next call".
// Must not collide with the {1,2,3} continuation-byte counts used during a
// partial UTF-8 sequence.
static constexpr unsigned STATE_C16_PENDING_LOW = 0x10000u;

thread_local mbstate_t tl_uchar_state = {STATE_NONE, 0};

// Decode one code point from a UTF-8 stream, resuming a partial sequence from
// *st. Returns bytes consumed (>0), 0 for NUL, (size_t)-1 EILSEQ, (size_t)-2
// incomplete; writes the code point to *cp on success.
static size_t __mbrtocp(char32_t *cp, const char *s, size_t n, mbstate_t *st) {
	uint32_t ch;
	unsigned remaining;
	size_t consumed = 0;

	if (st->_State >= 1 && st->_State <= 3) {
		ch = st->_Char;
		remaining = st->_State;
	} else {
		if (n == 0) {
			return (size_t)-2;
		}
		unsigned char b = (unsigned char)s[0];
		if (b < 0x80) {
			*cp = b;
			st->_State = STATE_NONE;
			st->_Char = 0;
			return b == 0 ? 0 : 1;
		}
		uint8_t len = unicode::utf8_length_data[b];
		if (len < 2 || len > 4) { // lone continuation byte or invalid lead
			errno = EILSEQ;
			return (size_t)-1;
		}
		ch = (uint32_t)(b & (0x7Fu >> len));
		remaining = (unsigned)(len - 1);
		consumed = 1;
	}

	while (remaining > 0) {
		if (consumed >= n) {
			st->_Char = ch;
			st->_State = remaining;
			return (size_t)-2;
		}
		unsigned char b = (unsigned char)s[consumed];
		if ((b & 0xC0u) != 0x80u) {
			errno = EILSEQ;
			return (size_t)-1;
		}
		ch = (ch << 6) | (uint32_t)(b & 0x3Fu);
		--remaining;
		++consumed;
	}

	st->_State = STATE_NONE;
	st->_Char = 0;
	*cp = (char32_t)ch;
	return consumed;
}

__SPRT_C_FUNC size_t mbrtoc32(char32_t *__pc32, const char *__s, size_t __n,
		mbstate_t *__ps) __SPRT_NOEXCEPT {
	if (!__ps) {
		__ps = &tl_uchar_state;
	}
	if (!__s) {
		char32_t tmp = 0;
		return mbrtoc32(&tmp, "", 1, __ps);
	}
	char32_t cp = 0;
	size_t r = __mbrtocp(&cp, __s, __n, __ps);
	if (r == (size_t)-1 || r == (size_t)-2) {
		return r;
	}
	if (__pc32) {
		*__pc32 = cp;
	}
	return r;
}

__SPRT_C_FUNC size_t mbrtoc16(char16_t *__pc16, const char *__s, size_t __n,
		mbstate_t *__ps) __SPRT_NOEXCEPT {
	if (!__ps) {
		__ps = &tl_uchar_state;
	}
	if (__ps->_State == STATE_C16_PENDING_LOW) {
		if (__pc16) {
			*__pc16 = (char16_t)__ps->_Char;
		}
		__ps->_State = STATE_NONE;
		__ps->_Char = 0;
		return (size_t)-3;
	}
	if (!__s) {
		char16_t tmp = 0;
		return mbrtoc16(&tmp, "", 1, __ps);
	}
	char32_t cp = 0;
	size_t r = __mbrtocp(&cp, __s, __n, __ps);
	if (r == (size_t)-1 || r == (size_t)-2) {
		return r;
	}
	if (cp <= 0xFFFF) {
		if (__pc16) {
			*__pc16 = (char16_t)cp;
		}
		return r;
	}
	// Astral: report the high surrogate now and stash the low surrogate.
	cp -= 0x10000u;
	if (__pc16) {
		*__pc16 = (char16_t)(0xD800u + (cp >> 10));
	}
	__ps->_Char = 0xDC00u + (cp & 0x3FFu);
	__ps->_State = STATE_C16_PENDING_LOW;
	return r;
}

__SPRT_C_FUNC size_t c32rtomb(char *__s, char32_t __c32, mbstate_t *__ps) __SPRT_NOEXCEPT {
	(void)__ps;
	if (!__s) {
		return 1;
	}
	if (__c32 > 0x10FFFFu || (__c32 >= 0xD800u && __c32 <= 0xDFFFu)) {
		errno = EILSEQ;
		return (size_t)-1;
	}
	return unicode::utf8EncodeBuf(__s, _MB_CUR_MAX, __c32);
}

__SPRT_C_FUNC size_t c16rtomb(char *__s, char16_t __c16, mbstate_t *__ps) __SPRT_NOEXCEPT {
	if (!__ps) {
		__ps = &tl_uchar_state;
	}
	// wchar_t is 16-bit (UTF-16) on this target and wcrtomb already combines
	// surrogate pairs across calls using the same mbstate convention.
	return wcrtomb(__s, (wchar_t)__c16, __ps);
}

} // namespace sprt
