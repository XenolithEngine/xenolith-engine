/**
 Copyright (c) 2024-2025 Stappler LLC <admin@stappler.dev>
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

// RFC 3492 Punycode, the bootstring encoding IDNA uses to carry non-ASCII labels
// through DNS. Self-contained: this predates the UTS-46 port and is reused by it
// through the adapters in uts46/uts46.cc, rather than porting ICU's second copy.

namespace sprt::idn {

/* punycode parameters, see http://tools.ietf.org/html/rfc3492#section-5 */
static constexpr auto BASE = 36;
static constexpr auto TMIN = 1;
static constexpr auto TMAX = 26;
static constexpr auto SKEW = 38;
static constexpr auto DAMP = 700;
static constexpr uint32_t MAXINT = 0xFFFF'FFFF;
static constexpr auto INITIAL_N = 128;
static constexpr auto INITIAL_BIAS = 72;

static uint32_t adapt_bias(uint32_t delta, unsigned n_points, int is_first) {
	uint32_t k;

	delta /= is_first ? DAMP : 2;
	delta += delta / n_points;

	/* while delta > 455: delta /= 35 */
	for (k = 0; delta > ((BASE - TMIN) * TMAX) / 2; k += BASE) { delta /= (BASE - TMIN); }

	return k + (((BASE - TMIN + 1) * delta) / (delta + SKEW));
}

static char encode_digit(size_t c) {
	assert(c >= 0 && c <= BASE - TMIN);
	if (c > 25) {
		return c + 22; /* '0'..'9' */
	} else {
		return c + 'a'; /* 'a'..'z' */
	}
}

/* Encode as a generalized variable-length integer. Returns number of bytes written. */
template <typename Callback>
requires requires(const Callback &cb, char c) { cb(c); }
static size_t encode_var_int(const size_t bias, const size_t delta, const Callback &cb) {
	size_t i, k, q, t;

	i = 0;
	k = BASE;
	q = delta;

	while (true) {
		if (k <= bias) {
			t = TMIN;
		} else if (k >= bias + TMAX) {
			t = TMAX;
		} else {
			t = k - bias;
		}

		if (q < t) {
			break;
		}

		cb(encode_digit(t + (q - t) % (BASE - t)));

		q = (q - t) / (BASE - t);
		k += BASE;
	}

	cb(encode_digit(q));

	return i;
}

template <typename Callback>
requires requires(const Callback &cb, char c) { cb(c); }
static bool punycode_encode(const char32_t *const src, const size_t srclen, const Callback &cb) {
	uint32_t b, h;
	uint32_t delta, bias;
	uint32_t m, n;
	uint32_t si, di;

	for (si = 0, di = 0; si < srclen; si++) {
		if (src[si] < 128) {
			++di;
			cb(char(src[si]));
		}
	}

	b = h = di;

	/* Write out delimiter if any basic code points were processed. */
	if (di > 0) {
		cb('-');
	}

	n = INITIAL_N;
	bias = INITIAL_BIAS;
	delta = 0;

	for (; h < srclen; n++, delta++) {
		/* Find next smallest non-basic code point. */
		for (m = MAXINT, si = 0; si < srclen; si++) {
			if (src[si] >= n && src[si] < m) {
				m = src[si];
			}
		}

		if ((m - n) > (MAXINT - delta) / (h + 1)) {
			return false;
		}

		delta += (m - n) * (h + 1);
		n = m;

		for (si = 0; si < srclen; si++) {
			if (src[si] < n) {
				if (++delta == 0) {
					return false;
				}
			} else if (src[si] == n) {
				di += encode_var_int(bias, delta, cb);
				bias = adapt_bias(delta, h + 1, h == b);
				delta = 0;
				h++;
			}
		}
	}

	return true;
}

static int is_basic(unsigned int a) { return (a < 0x80) ? 1 : 0; }

static int digit_decoded(const unsigned char a) {
	if (a >= 0x41 && a <= 0x5A) {
		return a - 0x41;
	}

	if (a >= 0x61 && a <= 0x7A) {
		return a - 0x61;
	}

	if (a >= 0x30 && a <= 0x39) {
		return a - 0x30 + 26;
	}

	return -1;
}

static int adapt(uint32_t delta, uint32_t numpoints, uint32_t firsttime) {
	uint32_t k = 0;

	delta = (firsttime) ? delta / DAMP : delta / 2;
	delta = delta + delta / numpoints;

	while (delta > ((BASE - TMIN) * TMAX) / 2) {
		delta = delta / (BASE - TMIN);
		k = k + BASE;
	}

	return k + (((BASE - TMIN + 1) * delta) / (delta + SKEW));
}

static bool punycode_decode(const char *const pEncoded, const size_t enc_len,
		char32_t *const pDecoded, size_t *pout_length) {
	uint32_t n = INITIAL_N;
	uint32_t i = 0;
	uint32_t bias = INITIAL_BIAS;
	uint32_t processed_in = 0, written_out = 0;
	uint32_t max_out = pDecoded ? uint32_t(*pout_length) : Max<uint32_t>;
	uint32_t basic_count = 0;
	uint32_t loop;

	for (loop = 0; loop < enc_len; loop++) {
		if (pEncoded[loop] == '-') {
			basic_count = loop;
		}
	}

	if (basic_count > 0) {
		if (basic_count > max_out) {
			return false;
		}

		for (loop = 0; loop < basic_count; loop++) {
			if (is_basic(pEncoded[loop]) == 0) {
				return false;
			}

			if (pDecoded) {
				pDecoded[loop] = pEncoded[loop];
			}
			written_out++;
		}
		processed_in = basic_count + 1;
	}

	for (loop = processed_in; loop < enc_len;) {
		uint32_t oldi = i;
		uint32_t w = 1;
		uint32_t k, t;
		int digit;

		for (k = BASE;; k += BASE) {
			if (loop >= enc_len) {
				return false;
			}

			digit = digit_decoded(pEncoded[loop]);
			loop++;

			if (digit < 0) {
				return false;
			}

			if ((uint32_t)digit > (MAXINT - i) / w) {
				return false;
			}

			i = i + digit * w;
			t = (k <= bias) ? TMIN : (k >= bias + TMAX) ? TMAX : k - bias;

			if ((uint32_t)digit < t) {
				break;
			}

			if (w > MAXINT / (BASE - t)) {
				return false;
			}
			w = w * (BASE - t);
		}

		bias = adapt(i - oldi, written_out + 1, (oldi == 0));
		if (i / (written_out + 1) > MAXINT - n) {
			return 0;
		}
		n = n + i / (written_out + 1);
		i %= (written_out + 1);

		// RFC 3492: reject decoded code points outside the Unicode range and
		// surrogate halves before they are written into the output.
		if (n > 0x10FFFF || (n >= 0xD800 && n <= 0xDFFF)) {
			return 0;
		}

		if (written_out >= max_out) {
			return 0;
		}

		if (pDecoded) {
			__sprt_memmove(pDecoded + i + 1, pDecoded + i, (written_out - i) * sizeof(*pDecoded));
			pDecoded[i] = n;
		}
		i++;
		written_out++;
	}

	*pout_length = written_out;
	return true;
}
} // namespace sprt::idn
