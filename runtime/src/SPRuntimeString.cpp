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

#include "SPRuntimeString.h"
#include "SPRuntimeFloat.h"

#include <c/__sprt_string.h>

#include <math.h>

namespace sprt {

char *strappend(char SPRT_NONNULL *buf, size_t *bufSize, const char SPRT_NONNULL *str,
		size_t strSize) {
	if (!bufSize) {
		return buf;
	}

	if (strSize < *bufSize) {
		::__sprt_memcpy(buf, str, strSize);
		buf += strSize;
		*bufSize -= strSize;
		*buf = 0;
		return buf;
	} else if (*bufSize > 1) {
		::__sprt_memcpy(buf, str, *bufSize - 1);
		buf += (*bufSize - 1);
		*bufSize = 1;
		*buf = 0;
		return buf;
	} else {
		return buf;
	}
}

char *strappend(char SPRT_NONNULL *buf, size_t *bufSize, int64_t val) {
	char tmp[INT_MAX_DIGITS];
	auto ret = sprt::itoa(val, tmp, INT_MAX_DIGITS);
	return strappend(buf, bufSize, tmp + INT_MAX_DIGITS - ret, ret);
}

char *strappend(char SPRT_NONNULL *buf, size_t *bufSize, uint64_t val) {
	char tmp[INT_MAX_DIGITS];
	auto ret = sprt::itoa(val, tmp, INT_MAX_DIGITS);
	return strappend(buf, bufSize, tmp + INT_MAX_DIGITS - ret, ret);
}

char *strappend(char SPRT_NONNULL *buf, size_t *bufSize, double val) {
	char tmp[DOUBLE_MAX_DIGITS];
	auto ret = sprt::dtoa(val, tmp, DOUBLE_MAX_DIGITS);
	return strappend(buf, bufSize, tmp, ret);
}

// Copies two characters from src to dst.
template <typename Char>
constexpr static void copy2(Char *dst, const Char *src) {
	::__sprt_memcpy(dst, src, 2 * sizeof(Char));
}

// Converts value in the range [0, 100) to a string.
template <typename Char>
constexpr static auto digits2(size_t value) -> const Char * {
	if constexpr (sizeof(Char) == sizeof(char)) {
		return &"0001020304050607080910111213141516171819"
				"2021222324252627282930313233343536373839"
				"4041424344454647484950515253545556575859"
				"6061626364656667686970717273747576777879"
				"8081828384858687888990919293949596979899"[value * 2];
	} else if constexpr (sizeof(Char) == sizeof(char16_t)) {
		return &u"0001020304050607080910111213141516171819"
				u"2021222324252627282930313233343536373839"
				u"4041424344454647484950515253545556575859"
				u"6061626364656667686970717273747576777879"
				u"8081828384858687888990919293949596979899"[value * 2];
	} else if constexpr (sizeof(Char) == sizeof(char32_t)) {
		return &U"0001020304050607080910111213141516171819"
				U"2021222324252627282930313233343536373839"
				U"4041424344454647484950515253545556575859"
				U"6061626364656667686970717273747576777879"
				U"8081828384858687888990919293949596979899"[value * 2];
	}
	return nullptr;
}

template <typename IntType, typename Char>
static inline size_t unsigned_to_decimal(Char *out, IntType value, size_t size) {
	out += size;
	Char *end = out;
	while (value >= 100) {
		out -= 2;
		copy2(out, digits2<Char>(static_cast<size_t>(value % 100)));
		value /= 100;
	}
	if (value < 10) {
		*--out = static_cast<Char>('0' + value);
		return end - out;
	}
	out -= 2;
	copy2(out, digits2<Char>(static_cast<size_t>(value)));
	return end - out;
}

template <typename IntType>
static inline size_t unsigned_to_decimal_len(IntType value) {
	size_t ret = 0;
	while (value >= 100) {
		ret += 2;
		value /= 100;
	}
	if (value < 10) {
		return ret + 1;
	}
	return ret + 2;
}

namespace dtoa_impl {

#if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && defined(__x86_64__)
namespace gcc_ints {
__extension__ typedef __int128 int128;
__extension__ typedef unsigned __int128 uint128;
} // namespace gcc_ints
#endif

#define UINT64_C2(h, l) ((static_cast<uint64_t>(h) << 32) | static_cast<uint64_t>(l))

struct DiyFp {
	DiyFp() { }

	DiyFp(uint64_t f, int e) : f(f), e(e) { }

	DiyFp(double d) {
		union {
			double d;
			uint64_t u64;
		} u = {d};

		int biased_e = (u.u64 & kDpExponentMask) >> kDpSignificandSize;
		uint64_t significand = (u.u64 & kDpSignificandMask);
		if (biased_e != 0) {
			f = significand + kDpHiddenBit;
			e = biased_e - kDpExponentBias;
		} else {
			f = significand;
			e = kDpMinExponent + 1;
		}
	}

	DiyFp operator-(const DiyFp &rhs) const { return DiyFp(f - rhs.f, e); }

	DiyFp operator*(const DiyFp &rhs) const {
#if defined(_MSC_VER) && defined(_M_AMD64)
		uint64_t h;
		uint64_t l = _umul128(f, rhs.f, &h);
		if (l & (uint64_t(1) << 63)) { // rounding
			h++;
		}
		return DiyFp(h, e + rhs.e + 64);
#elif (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && defined(__x86_64__)
		gcc_ints::uint128 p =
				static_cast<gcc_ints::uint128>(f) * static_cast<gcc_ints::uint128>(rhs.f);
		uint64_t h = p >> 64;
		uint64_t l = static_cast<uint64_t>(p);
		if (l & (uint64_t(1) << 63)) { // rounding
			h++;
		}
		return DiyFp(h, e + rhs.e + 64);
#else
		const uint64_t M32 = 0xFFFF'FFFF;
		const uint64_t a = f >> 32;
		const uint64_t b = f & M32;
		const uint64_t c = rhs.f >> 32;
		const uint64_t d = rhs.f & M32;
		const uint64_t ac = a * c;
		const uint64_t bc = b * c;
		const uint64_t ad = a * d;
		const uint64_t bd = b * d;
		uint64_t tmp = (bd >> 32) + (ad & M32) + (bc & M32);
		tmp += 1U << 31; /// mult_round
		return DiyFp(ac + (ad >> 32) + (bc >> 32) + (tmp >> 32), e + rhs.e + 64);
#endif
	}

	DiyFp Normalize() const {
#if defined(_MSC_VER) && defined(_M_AMD64)
		unsigned long index;
		_BitScanReverse64(&index, f);
		return DiyFp(f << (63 - index), e - (63 - index));
#elif defined(__GNUC__)
		int s = __builtin_clzll(f);
		return DiyFp(f << s, e - s);
#else
		DiyFp res = *this;
		while (!(res.f & kDpHiddenBit)) {
			res.f <<= 1;
			res.e--;
		}
		res.f <<= (kDiySignificandSize - kDpSignificandSize - 1);
		res.e = res.e - (kDiySignificandSize - kDpSignificandSize - 1);
		return res;
#endif
	}

	DiyFp NormalizeBoundary() const {
#if defined(_MSC_VER) && defined(_M_AMD64)
		unsigned long index;
		_BitScanReverse64(&index, f);
		return DiyFp(f << (63 - index), e - (63 - index));
#else
		DiyFp res = *this;
		while (!(res.f & (kDpHiddenBit << 1))) {
			res.f <<= 1;
			res.e--;
		}
		res.f <<= (kDiySignificandSize - kDpSignificandSize - 2);
		res.e = res.e - (kDiySignificandSize - kDpSignificandSize - 2);
		return res;
#endif
	}

	void NormalizedBoundaries(DiyFp *minus, DiyFp *plus) const {
		DiyFp pl = DiyFp((f << 1) + 1, e - 1).NormalizeBoundary();
		DiyFp mi = (f == kDpHiddenBit) ? DiyFp((f << 2) - 1, e - 2) : DiyFp((f << 1) - 1, e - 1);
		mi.f <<= mi.e - pl.e;
		mi.e = pl.e;
		*plus = pl;
		*minus = mi;
	}

	static const int kDiySignificandSize = 64;
	static const int kDpSignificandSize = 52;
	static const int kDpExponentBias = 0x3FF + kDpSignificandSize;
	static const int kDpMinExponent = -kDpExponentBias;
	static const uint64_t kDpExponentMask = UINT64_C2(0x7FF0'0000, 0x0000'0000);
	static const uint64_t kDpSignificandMask = UINT64_C2(0x000F'FFFF, 0xFFFF'FFFF);
	static const uint64_t kDpHiddenBit = UINT64_C2(0x0010'0000, 0x0000'0000);

	uint64_t f;
	int e;
};

static inline DiyFp GetCachedPower(int e, int *K) {
	// 10^-348, 10^-340, ..., 10^340
	static const uint64_t kCachedPowers_F[] = {UINT64_C2(0xfa8f'd5a0, 0x081c'0288),
		UINT64_C2(0xbaae'e17f, 0xa23e'bf76), UINT64_C2(0x8b16'fb20, 0x3055'ac76),
		UINT64_C2(0xcf42'894a, 0x5dce'35ea), UINT64_C2(0x9a6b'b0aa, 0x5565'3b2d),
		UINT64_C2(0xe61a'cf03, 0x3d1a'45df), UINT64_C2(0xab70'fe17, 0xc79a'c6ca),
		UINT64_C2(0xff77'b1fc, 0xbebc'dc4f), UINT64_C2(0xbe56'91ef, 0x416b'd60c),
		UINT64_C2(0x8dd0'1fad, 0x907f'fc3c), UINT64_C2(0xd351'5c28, 0x3155'9a83),
		UINT64_C2(0x9d71'ac8f, 0xada6'c9b5), UINT64_C2(0xea9c'2277, 0x23ee'8bcb),
		UINT64_C2(0xaecc'4991, 0x4078'536d), UINT64_C2(0x823c'1279, 0x5db6'ce57),
		UINT64_C2(0xc210'9436, 0x4dfb'5637), UINT64_C2(0x9096'ea6f, 0x3848'984f),
		UINT64_C2(0xd774'85cb, 0x2582'3ac7), UINT64_C2(0xa086'cfcd, 0x97bf'97f4),
		UINT64_C2(0xef34'0a98, 0x172a'ace5), UINT64_C2(0xb238'67fb, 0x2a35'b28e),
		UINT64_C2(0x84c8'd4df, 0xd2c6'3f3b), UINT64_C2(0xc5dd'4427, 0x1ad3'cdba),
		UINT64_C2(0x936b'9fce, 0xbb25'c996), UINT64_C2(0xdbac'6c24, 0x7d62'a584),
		UINT64_C2(0xa3ab'6658, 0x0d5f'daf6), UINT64_C2(0xf3e2'f893, 0xdec3'f126),
		UINT64_C2(0xb5b5'ada8, 0xaaff'80b8), UINT64_C2(0x8762'5f05, 0x6c7c'4a8b),
		UINT64_C2(0xc9bc'ff60, 0x34c1'3053), UINT64_C2(0x964e'858c, 0x91ba'2655),
		UINT64_C2(0xdff9'7724, 0x7029'7ebd), UINT64_C2(0xa6df'bd9f, 0xb8e5'b88f),
		UINT64_C2(0xf8a9'5fcf, 0x8874'7d94), UINT64_C2(0xb944'7093, 0x8fa8'9bcf),
		UINT64_C2(0x8a08'f0f8, 0xbf0f'156b), UINT64_C2(0xcdb0'2555, 0x6531'31b6),
		UINT64_C2(0x993f'e2c6, 0xd07b'7fac), UINT64_C2(0xe45c'10c4, 0x2a2b'3b06),
		UINT64_C2(0xaa24'2499, 0x6973'92d3), UINT64_C2(0xfd87'b5f2, 0x8300'ca0e),
		UINT64_C2(0xbce5'0864, 0x9211'1aeb), UINT64_C2(0x8cbc'cc09, 0x6f50'88cc),
		UINT64_C2(0xd1b7'1758, 0xe219'652c), UINT64_C2(0x9c40'0000, 0x0000'0000),
		UINT64_C2(0xe8d4'a510, 0x0000'0000), UINT64_C2(0xad78'ebc5, 0xac62'0000),
		UINT64_C2(0x813f'3978, 0xf894'0984), UINT64_C2(0xc097'ce7b, 0xc907'15b3),
		UINT64_C2(0x8f7e'32ce, 0x7bea'5c70), UINT64_C2(0xd5d2'38a4, 0xabe9'8068),
		UINT64_C2(0x9f4f'2726, 0x179a'2245), UINT64_C2(0xed63'a231, 0xd4c4'fb27),
		UINT64_C2(0xb0de'6538, 0x8cc8'ada8), UINT64_C2(0x83c7'088e, 0x1aab'65db),
		UINT64_C2(0xc45d'1df9, 0x4271'1d9a), UINT64_C2(0x924d'692c, 0xa61b'e758),
		UINT64_C2(0xda01'ee64, 0x1a70'8dea), UINT64_C2(0xa26d'a399, 0x9aef'774a),
		UINT64_C2(0xf209'787b, 0xb47d'6b85), UINT64_C2(0xb454'e4a1, 0x79dd'1877),
		UINT64_C2(0x865b'8692, 0x5b9b'c5c2), UINT64_C2(0xc835'53c5, 0xc896'5d3d),
		UINT64_C2(0x952a'b45c, 0xfa97'a0b3), UINT64_C2(0xde46'9fbd, 0x99a0'5fe3),
		UINT64_C2(0xa59b'c234, 0xdb39'8c25), UINT64_C2(0xf6c6'9a72, 0xa398'9f5c),
		UINT64_C2(0xb7dc'bf53, 0x54e9'bece), UINT64_C2(0x88fc'f317, 0xf222'41e2),
		UINT64_C2(0xcc20'ce9b, 0xd35c'78a5), UINT64_C2(0x9816'5af3, 0x7b21'53df),
		UINT64_C2(0xe2a0'b5dc, 0x971f'303a), UINT64_C2(0xa8d9'd153, 0x5ce3'b396),
		UINT64_C2(0xfb9b'7cd9, 0xa4a7'443c), UINT64_C2(0xbb76'4c4c, 0xa7a4'4410),
		UINT64_C2(0x8bab'8eef, 0xb640'9c1a), UINT64_C2(0xd01f'ef10, 0xa657'842c),
		UINT64_C2(0x9b10'a4e5, 0xe991'3129), UINT64_C2(0xe710'9bfb, 0xa19c'0c9d),
		UINT64_C2(0xac28'20d9, 0x623b'f429), UINT64_C2(0x8044'4b5e, 0x7aa7'cf85),
		UINT64_C2(0xbf21'e440, 0x03ac'dd2d), UINT64_C2(0x8e67'9c2f, 0x5e44'ff8f),
		UINT64_C2(0xd433'179d, 0x9c8c'b841), UINT64_C2(0x9e19'db92, 0xb4e3'1ba9),
		UINT64_C2(0xeb96'bf6e, 0xbadf'77d9), UINT64_C2(0xaf87'023b, 0x9bf0'ee6b)};
	static const int16_t kCachedPowers_E[] = {-1'220, -1'193, -1'166, -1'140, -1'113, -1'087,
		-1'060, -1'034, -1'007, -980, -954, -927, -901, -874, -847, -821, -794, -768, -741, -715,
		-688, -661, -635, -608, -582, -555, -529, -502, -475, -449, -422, -396, -369, -343, -316,
		-289, -263, -236, -210, -183, -157, -130, -103, -77, -50, -24, 3, 30, 56, 83, 109, 136, 162,
		189, 216, 242, 269, 295, 322, 348, 375, 402, 428, 455, 481, 508, 534, 561, 588, 614, 641,
		667, 694, 720, 747, 774, 800, 827, 853, 880, 907, 933, 960, 986, 1'013, 1'039, 1'066};

	//int k = static_cast<int>(ceil((-61 - e) * 0.30102999566398114)) + 374;
	double dk = (-61 - e) * 0.30102999566398114
			+ 347; // dk must be positive, so can do ceiling in positive
	int k = static_cast<int>(dk);
	if (dk - k > 0.0) {
		k++;
	}

	unsigned index = static_cast<unsigned>((k >> 3) + 1);
	*K = -(-348 + static_cast<int>(index << 3)); // decimal exponent no need lookup table

	return DiyFp(kCachedPowers_F[index], kCachedPowers_E[index]);
}

template <typename Char>
static inline void GrisuRound(Char *buffer, int len, uint64_t delta, uint64_t rest,
		uint64_t ten_kappa, uint64_t wp_w) {
	while (rest < wp_w && delta - rest >= ten_kappa
			&& (rest + ten_kappa < wp_w || /// closer
					wp_w - rest > rest + ten_kappa - wp_w)) {
		buffer[len - 1]--;
		rest += ten_kappa;
	}
}

static inline unsigned CountDecimalDigit32(uint32_t n) {
	// Simple pure C++ implementation was faster than __builtin_clz version in this situation.
	if (n < 10) {
		return 1;
	}
	if (n < 100) {
		return 2;
	}
	if (n < 1'000) {
		return 3;
	}
	if (n < 10'000) {
		return 4;
	}
	if (n < 100'000) {
		return 5;
	}
	if (n < 1'000'000) {
		return 6;
	}
	if (n < 10'000'000) {
		return 7;
	}
	if (n < 100'000'000) {
		return 8;
	}
	if (n < 1'000'000'000) {
		return 9;
	}
	return 10;
}

template <typename Char>
static inline void DigitGen(const DiyFp &W, const DiyFp &Mp, uint64_t delta, Char *buffer, int *len,
		int *K) {
	static const uint32_t kPow10[] = {1, 10, 100, 1'000, 10'000, 100'000, 1'000'000, 10'000'000,
		100'000'000, 1'000'000'000};
	const DiyFp one(uint64_t(1) << -Mp.e, Mp.e);
	const DiyFp wp_w = Mp - W;
	uint32_t p1 = static_cast<uint32_t>(Mp.f >> -one.e);
	uint64_t p2 = Mp.f & (one.f - 1);
	int kappa = static_cast<int>(CountDecimalDigit32(p1));
	*len = 0;

	while (kappa > 0) {
		uint32_t d;
		switch (kappa) {
		case 10:
			d = p1 / 1'000'000'000;
			p1 %= 1'000'000'000;
			break;
		case 9:
			d = p1 / 100'000'000;
			p1 %= 100'000'000;
			break;
		case 8:
			d = p1 / 10'000'000;
			p1 %= 10'000'000;
			break;
		case 7:
			d = p1 / 1'000'000;
			p1 %= 1'000'000;
			break;
		case 6:
			d = p1 / 100'000;
			p1 %= 100'000;
			break;
		case 5:
			d = p1 / 10'000;
			p1 %= 10'000;
			break;
		case 4:
			d = p1 / 1'000;
			p1 %= 1'000;
			break;
		case 3:
			d = p1 / 100;
			p1 %= 100;
			break;
		case 2:
			d = p1 / 10;
			p1 %= 10;
			break;
		case 1:
			d = p1;
			p1 = 0;
			break;
		default:
#if defined(_MSC_VER)
			__assume(0);
#elif __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
			__builtin_unreachable();
#else
			d = 0;
#endif
		}
		if (d || *len) {
			buffer[(*len)++] = '0' + static_cast<char>(d);
		}
		kappa--;
		uint64_t tmp = (static_cast<uint64_t>(p1) << -one.e) + p2;
		if (tmp <= delta) {
			*K += kappa;
			GrisuRound(buffer, *len, delta, tmp, static_cast<uint64_t>(kPow10[kappa]) << -one.e,
					wp_w.f);
			return;
		}
	}

	// kappa = 0
	for (;;) {
		p2 *= 10;
		delta *= 10;
		char d = static_cast<char>(p2 >> -one.e);
		if (d || *len) {
			buffer[(*len)++] = '0' + d;
		}
		p2 &= one.f - 1;
		kappa--;
		if (p2 < delta) {
			*K += kappa;
			GrisuRound(buffer, *len, delta, p2, one.f, wp_w.f * kPow10[-kappa]);
			return;
		}
	}
}

static inline void DigitGen_len(const DiyFp &W, const DiyFp &Mp, uint64_t delta, int *len, int *K) {
	const DiyFp one(uint64_t(1) << -Mp.e, Mp.e);
	uint32_t p1 = static_cast<uint32_t>(Mp.f >> -one.e);
	uint64_t p2 = Mp.f & (one.f - 1);
	int kappa = static_cast<int>(CountDecimalDigit32(p1));
	*len = 0;

	while (kappa > 0) {
		uint32_t d;
		switch (kappa) {
		case 10:
			d = p1 / 1'000'000'000;
			p1 %= 1'000'000'000;
			break;
		case 9:
			d = p1 / 100'000'000;
			p1 %= 100'000'000;
			break;
		case 8:
			d = p1 / 10'000'000;
			p1 %= 10'000'000;
			break;
		case 7:
			d = p1 / 1'000'000;
			p1 %= 1'000'000;
			break;
		case 6:
			d = p1 / 100'000;
			p1 %= 100'000;
			break;
		case 5:
			d = p1 / 10'000;
			p1 %= 10'000;
			break;
		case 4:
			d = p1 / 1'000;
			p1 %= 1'000;
			break;
		case 3:
			d = p1 / 100;
			p1 %= 100;
			break;
		case 2:
			d = p1 / 10;
			p1 %= 10;
			break;
		case 1:
			d = p1;
			p1 = 0;
			break;
		default:
#if defined(_MSC_VER)
			__assume(0);
#elif __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
			__builtin_unreachable();
#else
			d = 0;
#endif
		}
		if (d || *len) {
			(*len)++;
		}
		kappa--;
		uint64_t tmp = (static_cast<uint64_t>(p1) << -one.e) + p2;
		if (tmp <= delta) {
			*K += kappa;
			return;
		}
	}

	// kappa = 0
	for (;;) {
		p2 *= 10;
		delta *= 10;
		char d = static_cast<char>(p2 >> -one.e);
		if (d || *len) {
			(*len)++;
		}
		p2 &= one.f - 1;
		kappa--;
		if (p2 < delta) {
			*K += kappa;
			return;
		}
	}
}

template <typename Char>
static inline void Grisu2(double value, Char *buffer, int *length, int *K) {
	const DiyFp v(value);
	DiyFp w_m, w_p;
	v.NormalizedBoundaries(&w_m, &w_p);

	const DiyFp c_mk = GetCachedPower(w_p.e, K);
	const DiyFp W = v.Normalize() * c_mk;
	DiyFp Wp = w_p * c_mk;
	DiyFp Wm = w_m * c_mk;
	Wm.f++;
	Wp.f--;
	DigitGen(W, Wp, Wp.f - Wm.f, buffer, length, K);
}

static inline void Grisu2_len(double value, int *length, int *K) {
	const DiyFp v(value);
	DiyFp w_m, w_p;
	v.NormalizedBoundaries(&w_m, &w_p);

	const DiyFp c_mk = GetCachedPower(w_p.e, K);
	const DiyFp W = v.Normalize() * c_mk;
	DiyFp Wp = w_p * c_mk;
	DiyFp Wm = w_m * c_mk;
	Wm.f++;
	Wp.f--;
	DigitGen_len(W, Wp, Wp.f - Wm.f, length, K);
}

static inline const char *GetDigitsLut() {
	static const char cDigitsLut[200] = {'0', '0', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5',
		'0', '6', '0', '7', '0', '8', '0', '9', '1', '0', '1', '1', '1', '2', '1', '3', '1', '4',
		'1', '5', '1', '6', '1', '7', '1', '8', '1', '9', '2', '0', '2', '1', '2', '2', '2', '3',
		'2', '4', '2', '5', '2', '6', '2', '7', '2', '8', '2', '9', '3', '0', '3', '1', '3', '2',
		'3', '3', '3', '4', '3', '5', '3', '6', '3', '7', '3', '8', '3', '9', '4', '0', '4', '1',
		'4', '2', '4', '3', '4', '4', '4', '5', '4', '6', '4', '7', '4', '8', '4', '9', '5', '0',
		'5', '1', '5', '2', '5', '3', '5', '4', '5', '5', '5', '6', '5', '7', '5', '8', '5', '9',
		'6', '0', '6', '1', '6', '2', '6', '3', '6', '4', '6', '5', '6', '6', '6', '7', '6', '8',
		'6', '9', '7', '0', '7', '1', '7', '2', '7', '3', '7', '4', '7', '5', '7', '6', '7', '7',
		'7', '8', '7', '9', '8', '0', '8', '1', '8', '2', '8', '3', '8', '4', '8', '5', '8', '6',
		'8', '7', '8', '8', '8', '9', '9', '0', '9', '1', '9', '2', '9', '3', '9', '4', '9', '5',
		'9', '6', '9', '7', '9', '8', '9', '9'};
	return cDigitsLut;
}

template <typename Char>
static inline size_t WriteExponent(int K, Char *buffer) {
	auto orig = buffer;
	if (K < 0) {
		*buffer++ = '-';
		K = -K;
	}

	if (K >= 100) {
		*buffer++ = '0' + static_cast<char>(K / 100);
		K %= 100;
		const char *d = GetDigitsLut() + K * 2;
		*buffer++ = d[0];
		*buffer++ = d[1];
	} else if (K >= 10) {
		const char *d = GetDigitsLut() + K * 2;
		*buffer++ = d[0];
		*buffer++ = d[1];
	} else {
		*buffer++ = '0' + static_cast<char>(K);
	}

	return buffer - orig;
}

static inline size_t WriteExponent_len(int K) {
	size_t ret = 0;
	if (K < 0) {
		++ret;
		K = -K;
	}

	if (K >= 100) {
		ret += 3;
		K %= 100;
	} else if (K >= 10) {
		ret += 3;
	} else {
		++ret;
	}
	return ret;
}

template <typename Char>
static inline size_t Prettify(Char *buffer, int length, int k) {
	const int kk = length + k; // 10^(kk-1) <= v < 10^kk

	if (length <= kk && kk <= 21) {
		// 1234e7 -> 12340000000
		for (int i = length; i < kk; i++) { buffer[i] = '0'; }
		buffer[kk] = '.';
		buffer[kk + 1] = '0';
		return kk + 2;
	} else if (0 < kk && kk <= 21) {
		// 1234e-2 -> 12.34
		::__sprt_memmove(&buffer[kk + 1], &buffer[kk], (length - kk) * sizeof(Char));
		buffer[kk] = '.';
		return length + 1;
	} else if (-6 < kk && kk <= 0) {
		// 1234e-6 -> 0.001234
		const int offset = 2 - kk;
		::__sprt_memmove(&buffer[offset], &buffer[0], length * sizeof(Char));
		buffer[0] = '0';
		buffer[1] = '.';
		for (int i = 2; i < offset; i++) { buffer[i] = '0'; }
		return length + offset;
	} else if (length == 1) {
		// 1e30
		buffer[1] = 'e';
		return 2 + WriteExponent(kk - 1, &buffer[2]);
	} else {
		// 1234e30 -> 1.234e33
		::__sprt_memmove(&buffer[2], &buffer[1], (length - 1) * sizeof(Char));
		buffer[1] = '.';
		buffer[length + 1] = 'e';
		return length + 2 + WriteExponent(kk - 1, &buffer[0 + length + 2]);
	}
}

static inline size_t Prettify_len(int length, int k) {
	const int kk = length + k; // 10^(kk-1) <= v < 10^kk

	if (length <= kk && kk <= 21) {
		// 1234e7 -> 12340000000
		return kk + 2;
	} else if (0 < kk && kk <= 21) {
		// 1234e-2 -> 12.34
		return length + 1;
	} else if (-6 < kk && kk <= 0) {
		// 1234e-6 -> 0.001234
		const int offset = 2 - kk;
		return length + offset;
	} else if (length == 1) {
		return 2 + WriteExponent_len(kk - 1);
	} else {
		// 1234e30 -> 1.234e33
		return length + 2 + WriteExponent_len(kk - 1);
	}
}

template <typename Char>
static inline size_t dtoa_memcpy(Char *buffer, const Char *data, size_t size) {
	::__sprt_memcpy(buffer, data, size * sizeof(Char));
	return size;
}

template <typename Char>
static inline size_t dtoa_milo(double value, Char *buffer) {
	if (isnan(value)) {
		if constexpr (sizeof(Char) == sizeof(char)) {
			return dtoa_memcpy(buffer, "NaN", 3);
		} else if constexpr (sizeof(Char) == sizeof(char16_t)) {
			return dtoa_memcpy(buffer, u"NaN", 3);
		} else {
			return 0;
		}
	} else if (value == Infinity<double>) {
		if constexpr (sizeof(Char) == sizeof(char)) {
			return dtoa_memcpy(buffer, "inf", 3);
		} else if constexpr (sizeof(Char) == sizeof(char16_t)) {
			return dtoa_memcpy(buffer, u"inf", 3);
		} else {
			return 0;
		}
	} else if (value == -Infinity<double>) {
		if constexpr (sizeof(Char) == sizeof(char)) {
			return dtoa_memcpy(buffer, "-inf", 4);
		} else if constexpr (sizeof(Char) == sizeof(char16_t)) {
			return dtoa_memcpy(buffer, u"-inf", 4);
		} else {
			return 0;
		}
	} else if (value == 0) {
		if constexpr (sizeof(Char) == sizeof(char)) {
			return dtoa_memcpy(buffer, "0.0", 3);
		} else if constexpr (sizeof(Char) == sizeof(char16_t)) {
			return dtoa_memcpy(buffer, u"0.0", 3);
		} else {
			return 0;
		}
	} else {
		size_t ret = 0;
		if (value < 0) {
			++ret;
			*buffer++ = '-';
			value = -value;
		}
		int length, K;
		Grisu2(value, buffer, &length, &K);
		return ret + Prettify(buffer, length, K);
	}
}

static inline size_t dtoa_milo_len(double value) {
	if (isnan(value)) {
		return 3;
	} else if (value == Infinity<double>) {
		return 3;
	} else if (value == -Infinity<double>) {
		return 4;
	} else if (value == 0) {
		return 3;
	} else {
		size_t ret = 0;
		if (value < 0) {
			++ret;
			value = -value;
		}
		int length, K;
		Grisu2_len(value, &length, &K);
		return ret + Prettify_len(length, K);
	}
}

} // namespace dtoa_impl

static size_t _itoa_len(int64_t number) {
	if (number < 0) {
		auto ret = unsigned_to_decimal_len(uint64_t(-number));
		return ret + 1;
	} else {
		return unsigned_to_decimal_len(uint64_t(number));
	}
}

static size_t _itoa_len(uint64_t number) { return unsigned_to_decimal_len(uint64_t(number)); }

static size_t _dtoa_len(double number) { return dtoa_impl::dtoa_milo_len(number); }

size_t itoa(int64_t number, char *buffer, size_t bufSize) {
	if (buffer == nullptr) {
		return _itoa_len(number);
	}

	if (number < 0) {
		auto ret = unsigned_to_decimal(buffer, uint64_t(-number), bufSize);
		buffer[bufSize - ret - 1] = '-';
		return ret + 1;
	} else {
		return unsigned_to_decimal(buffer, uint64_t(number), bufSize);
	}
}

size_t itoa(uint64_t number, char *buffer, size_t bufSize) {
	if (buffer == nullptr) {
		return _itoa_len(number);
	}

	return unsigned_to_decimal(buffer, uint64_t(number), bufSize);
}

size_t itoa(int64_t number, char16_t *buffer, size_t bufSize) {
	if (buffer == nullptr) {
		return _itoa_len(number);
	}

	if (number < 0) {
		auto ret = unsigned_to_decimal(buffer, uint64_t(-number), bufSize);
		buffer[bufSize - ret - 1] = '-';
		return ret + 1;
	} else {
		return unsigned_to_decimal(buffer, uint64_t(number), bufSize);
	}
}

size_t itoa(uint64_t number, char16_t *buffer, size_t bufSize) {
	if (buffer == nullptr) {
		return _itoa_len(number);
	}

	return unsigned_to_decimal(buffer, uint64_t(number), bufSize);
}

size_t dtoa(double number, char *buffer, size_t bufSize) {
	if (buffer == nullptr) {
		return _dtoa_len(number);
	}

	return dtoa_impl::dtoa_milo(number, buffer);
}

size_t dtoa(double number, char16_t *buffer, size_t bufSize) {
	if (buffer == nullptr) {
		return _dtoa_len(number);
	}

	return dtoa_impl::dtoa_milo(number, buffer);
}

enum class SmartType : uint8_t {
	PunctuationBasic = 1 << 0,
	Numbers = 1 << 1,
	WhiteSpace = 1 << 2,
	LatinLowercase = 1 << 3,
	LatinUppercase = 1 << 4,
	Hexadecimial = 1 << 5,
	Base64 = 1 << 6,
	TextPunctuation = 1 << 7
};

// clang-format off
static uint8_t smart_lookup_table[256] = {
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   4,   4,   4,   4,   4,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   4, 129, 129, 129, 129, 129,   1, 129, 129, 129, 129, 193, 129, 193, 129, 193,
	  98,  98,  98,  98,  98,  98,  98,  98,  98,  98, 129, 129,   1, 193,   1, 129,
	 129, 112, 112, 112, 112, 112, 112,  80,  80,  80,  80,  80,  80,  80,  80,  80,
	  80,  80,  80,  80,  80,  80,  80,  80,  80,  80,  80, 129, 129, 129, 129, 193,
	   1, 105, 105, 105, 105, 105, 105,  73,  73,  73,  73,  73,  73,  73,  73,  73,
	  73,  73,  73,  73,  73,  73,  73,  73,  73,  73,  73,   1,   1,   1,   1,   1,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
};
// clang-format on

bool ispunct(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]] & toInt(SmartType::PunctuationBasic);
}

bool isdigit(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]] & toInt(SmartType::Numbers);
}

bool isalpha(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]]
			& (toInt(SmartType::LatinLowercase) | toInt(SmartType::LatinUppercase));
}

bool isspace(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]] & toInt(SmartType::WhiteSpace);
}

bool islower(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]] & toInt(SmartType::LatinLowercase);
}

bool isupper(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]] & toInt(SmartType::LatinUppercase);
}

bool isalnum(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]]
			& (toInt(SmartType::LatinLowercase) | toInt(SmartType::LatinUppercase)
					| toInt(SmartType::Numbers));
}

bool isxdigit(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]] & toInt(SmartType::Hexadecimial);
}

bool isbase64(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]] & toInt(SmartType::Base64);
}

bool istpunct(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]] & toInt(SmartType::TextPunctuation);
}

size_t strlen(const char *str) { return ::__sprt_strlen(str); }

size_t strlen(const char16_t *str) {
	if (str == nullptr) {
		return 0;
	}
	const char16_t *end = str;
	while (*end != u'\0') { ++end; }
	return static_cast<size_t>(end - str);
}

char *_makeCharBuffer(size_t size) {
	if (size == 0) {
		return nullptr;
	}

	auto ret = new char[size];
	ret[0] = 0;
	return ret;
}

char16_t *_makeChar16Buffer(size_t size) {
	if (size == 0) {
		return nullptr;
	}

	auto ret = new char16_t[size];
	ret[0] = 0;
	return ret;
}

void freeCharBuffer(char *buf) {
	if (buf) {
		delete[] buf;
	}
}

void freeCharBuffer(char16_t *buf) {
	if (buf) {
		delete[] buf;
	}
}

} // namespace sprt
