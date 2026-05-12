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

static constexpr uint16_t STATE_NONE = 0;
static constexpr uint16_t STATE_EXPECT_LOW_SURROGATE = 1;

thread_local mbstate_t tl_wctomb_state = {STATE_NONE, 0};
thread_local mbstate_t tl_mbtowc_state = {STATE_NONE, 0};

__SPRT_C_FUNC size_t wcrtomb(char *__SPRT_RESTRICT a, wchar_t c,
		mbstate_t *__SPRT_RESTRICT state) __SPRT_NOEXCEPT {
	if (!state) {
		state = &tl_wctomb_state;
	}

	if constexpr (sizeof(wchar_t) == sizeof(char16_t)) {
		switch (state->_State) {
		case STATE_NONE:
			if (unicode::isUtf16HighSurrogate(c)) [[unlikely]] {
				state->_State = STATE_EXPECT_LOW_SURROGATE;
				state->_Char = c;
				return 0;
			} else {
				return unicode::utf8EncodeBuf(a, MB_CUR_MAX, char16_t(c));
			}
		case STATE_EXPECT_LOW_SURROGATE:
			if (unicode::isUtf16LowSurrogate(c)) {
				auto char32 = unicode::utf16CombineSurrogates(state->_Char, c);
				state->_State = STATE_NONE;
				state->_Char = 0;
				return unicode::utf8EncodeBuf(a, MB_CUR_MAX, char32);
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
		return unicode::utf8EncodeBuf(a, MB_CUR_MAX, char32_t(c));
	}
}

__SPRT_C_FUNC int wctomb(char *a, wchar_t c) __SPRT_NOEXCEPT {
	return wcrtomb(a, c, &tl_wctomb_state);
}

__SPRT_C_FUNC size_t mbrtowc(wchar_t *__SPRT_RESTRICT a, const char *__SPRT_RESTRICT ptr,
		size_t bufLen, mbstate_t *__SPRT_RESTRICT state) __SPRT_NOEXCEPT {
	if (!state) {
		state = &tl_mbtowc_state;
	}

	size_t n = 0;

	if (state->_State == 0) {
		// read first
		uint8_t mask = sprt::unicode::utf8_length_mask[uint8_t(*ptr)];
		state->_State = sprt::unicode::utf8_length_data[uint8_t(*ptr)];
		state->_Char = (*ptr++) & mask;
		--state->_State;
		--bufLen;
	}

	while (state->_State > 0 && bufLen > 0) {
		if ((*ptr & 0xc0) != 0x80) {
			errno = EILSEQ;
			state->_State = STATE_NONE;
			return -1;
		}
		state->_Char <<= 6;
		state->_Char |= (*ptr++ & 0x3f);
		--state->_State;
		--bufLen;
	}

	if (state->_State > 0 && bufLen == 0) {
		// save state;
		return -2;
	}

	if constexpr (sizeof(wchar_t) == sizeof(char16_t)) {
		n += unicode::utf16EncodeBuf((char16_t *)a, 4, state->_Char);
	} else {
		*a = (wchar_t)state->_Char;
		n = 1;
	}

	return n;
}

__SPRT_C_FUNC int mbtowc(__SPRT_ID(wchar_t) * __wc_ptr, const char *__s,
		size_t __n) __SPRT_NOEXCEPT {
	return mbrtowc(__wc_ptr, __s, __n, &tl_mbtowc_state);
}

__SPRT_C_FUNC int mbsinit(const mbstate_t *value) __SPRT_NOEXCEPT {
	if (!value || value->_State == STATE_NONE) {
		return 1;
	}
	return 0;
}

__SPRT_C_FUNC size_t __ctype_get_mb_cur_max() { return 6; }

} // namespace sprt
