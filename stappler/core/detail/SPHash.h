/**
Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#ifndef STAPPLER_CORE_DETAIL_SPHASH_H_
#define STAPPLER_CORE_DETAIL_SPHASH_H_

#include "SPPlatformInit.h"
#include "SPRuntimeInt.h"

// A part of SPCore.h, DO NOT include this directly

// Based on XXH (https://cyan4973.github.io/xxHash/#benchmarks)
// constexpr implementation from https://github.com/ekpyron/xxhashct

// Requires C++17

namespace STAPPLER_VERSIONIZED stappler::hash {

#define SP_HASH_INLINE [[gnu::always_inline]]

class SP_PUBLIC xxh32 {
public:
	static constexpr sprt::uint32_t hash(const char *input, sprt::uint32_t len,
			sprt::uint32_t seed) {
		return finalize((len >= 16 ? h16bytes(input, len, seed) : seed + PRIME5) + len,
				(input) + (len & ~0xF), len & 0xF);
	}

private:
	static constexpr sprt::uint32_t PRIME1 = 0x9E37'79B1U;
	static constexpr sprt::uint32_t PRIME2 = 0x85EB'CA77U;
	static constexpr sprt::uint32_t PRIME3 = 0xC2B2'AE3DU;
	static constexpr sprt::uint32_t PRIME4 = 0x27D4'EB2FU;
	static constexpr sprt::uint32_t PRIME5 = 0x1656'67B1U;

	SP_HASH_INLINE static constexpr sprt::uint32_t rotl(sprt::uint32_t x, int r) {
		return ((x << r) | (x >> (32 - r)));
	}
	SP_HASH_INLINE static constexpr sprt::uint32_t round(sprt::uint32_t acc,
			const sprt::uint32_t input) {
		return rotl(acc + (input * PRIME2), 13) * PRIME1;
	}
	SP_HASH_INLINE static constexpr sprt::uint32_t avalanche_step(const sprt::uint32_t h,
			const int rshift, const sprt::uint32_t prime) {
		return (h ^ (h >> rshift)) * prime;
	}
	SP_HASH_INLINE static constexpr sprt::uint32_t avalanche(const sprt::uint32_t h) {
		return avalanche_step(avalanche_step(avalanche_step(h, 15, PRIME2), 13, PRIME3), 16, 1);
	}
	SP_HASH_INLINE static constexpr sprt::uint32_t endian32(const char *v) {
		return sprt::uint32_t(static_cast<sprt::uint8_t>(v[0]))
				| (sprt::uint32_t(static_cast<sprt::uint8_t>(v[1])) << 8)
				| (sprt::uint32_t(static_cast<sprt::uint8_t>(v[2])) << 16)
				| (sprt::uint32_t(static_cast<sprt::uint8_t>(v[3])) << 24);
	}
	SP_HASH_INLINE static constexpr sprt::uint32_t fetch32(const char *p, const sprt::uint32_t v) {
		return round(v, endian32(p));
	}
	SP_HASH_INLINE static constexpr sprt::uint32_t finalize(sprt::uint32_t h, const char *p,
			sprt::uint32_t len) {
		while (len >= 4) {
			h = rotl(h + (endian32(p) * PRIME3), 17) * PRIME4;
			len -= 4;
			p += 4;
		}
		while (len > 0) {
			h = rotl(h + (static_cast<sprt::uint8_t>(*p++) * PRIME5), 11) * PRIME1;
			--len;
		}
		return avalanche(h);
	}
	SP_HASH_INLINE static constexpr sprt::uint32_t h16bytes(const char *p, sprt::uint32_t len,
			sprt::uint32_t v1, sprt::uint32_t v2, sprt::uint32_t v3, sprt::uint32_t v4) {
		const char *const limit = p + len - 16;
		do {
			v1 = fetch32(p, v1);
			p += 4;
			v2 = fetch32(p, v2);
			p += 4;
			v3 = fetch32(p, v3);
			p += 4;
			v4 = fetch32(p, v4);
			p += 4;
		} while (p <= limit);
		return rotl(v1, 1) + rotl(v2, 7) + rotl(v3, 12) + rotl(v4, 18);
	}
	SP_HASH_INLINE static constexpr sprt::uint32_t h16bytes(const char *p, sprt::uint32_t len,
			const sprt::uint32_t seed) {
		return h16bytes(p, len, seed + PRIME1 + PRIME2, seed + PRIME2, seed, seed - PRIME1);
	}
};

class SP_PUBLIC xxh64 {
public:
	static constexpr sprt::uint64_t hash(const char *p, sprt::uint64_t len, sprt::uint64_t seed) {
		return finalize((len >= 32 ? h32bytes(p, len, seed) : seed + PRIME5) + len,
				p + (len & ~0x1F), len & 0x1F);
	}

private:
	static constexpr sprt::uint64_t PRIME1 =
			0x9E37'79B1'85EB'CA87ULL; /*!< 0b1001111000110111011110011011000110000101111010111100101010000111 */
	static constexpr sprt::uint64_t PRIME2 =
			0xC2B2'AE3D'27D4'EB4FULL; /*!< 0b1100001010110010101011100011110100100111110101001110101101001111 */
	static constexpr sprt::uint64_t PRIME3 =
			0x1656'67B1'9E37'79F9ULL; /*!< 0b0001011001010110011001111011000110011110001101110111100111111001 */
	static constexpr sprt::uint64_t PRIME4 =
			0x85EB'CA77'C2B2'AE63ULL; /*!< 0b1000010111101011110010100111011111000010101100101010111001100011 */
	static constexpr sprt::uint64_t PRIME5 =
			0x27D4'EB2F'1656'67C5ULL; /*!< 0b0010011111010100111010110010111100010110010101100110011111000101 */

	SP_HASH_INLINE static constexpr sprt::uint64_t rotl(sprt::uint64_t x, int r) {
		return ((x << r) | (x >> (64 - r)));
	}
	SP_HASH_INLINE static constexpr sprt::uint64_t mix1(const sprt::uint64_t h,
			const sprt::uint64_t prime, int rshift) {
		return (h ^ (h >> rshift)) * prime;
	}
	SP_HASH_INLINE static constexpr sprt::uint64_t mix2(const sprt::uint64_t p,
			const sprt::uint64_t v = 0) {
		return rotl(v + p * PRIME2, 31) * PRIME1;
	}
	SP_HASH_INLINE static constexpr sprt::uint64_t mix3(const sprt::uint64_t h,
			const sprt::uint64_t v) {
		return (h ^ mix2(v)) * PRIME1 + PRIME4;
	}
	SP_HASH_INLINE static constexpr sprt::uint32_t endian32(const char *v) {
		return sprt::uint32_t(static_cast<sprt::uint8_t>(v[0]))
				| (sprt::uint32_t(static_cast<sprt::uint8_t>(v[1])) << 8)
				| (sprt::uint32_t(static_cast<sprt::uint8_t>(v[2])) << 16)
				| (sprt::uint32_t(static_cast<sprt::uint8_t>(v[3])) << 24);
	}
	SP_HASH_INLINE static constexpr sprt::uint64_t endian64(const char *v) {
		return sprt::uint64_t(static_cast<sprt::uint8_t>(v[0]))
				| (sprt::uint64_t(static_cast<sprt::uint8_t>(v[1])) << 8)
				| (sprt::uint64_t(static_cast<sprt::uint8_t>(v[2])) << 16)
				| (sprt::uint64_t(static_cast<sprt::uint8_t>(v[3])) << 24)
				| (sprt::uint64_t(static_cast<sprt::uint8_t>(v[4])) << 32)
				| (sprt::uint64_t(static_cast<sprt::uint8_t>(v[5])) << 40)
				| (sprt::uint64_t(static_cast<sprt::uint8_t>(v[6])) << 48)
				| (sprt::uint64_t(static_cast<sprt::uint8_t>(v[7])) << 56);
	}
	SP_HASH_INLINE static constexpr sprt::uint64_t fetch64(const char *p,
			const sprt::uint64_t v = 0) {
		return mix2(endian64(p), v);
	}
	SP_HASH_INLINE static constexpr sprt::uint64_t fetch32(const char *p) {
		return sprt::uint64_t(endian32(p)) * PRIME1;
	}
	SP_HASH_INLINE static constexpr sprt::uint64_t finalize(sprt::uint64_t h,
			const char *__restrict__ p, sprt::uint64_t len) {
		while (len >= 8) {
			h = rotl(h ^ fetch64(p), 27) * PRIME1 + PRIME4;
			p += 8;
			len -= 8;
		}
		if (len >= 4) {
			h = rotl(h ^ fetch32(p), 23) * PRIME2 + PRIME3;
			p += 4;
			len -= 4;
		}
		while (len > 0) {
			h = rotl(h ^ ((static_cast<sprt::uint8_t>(*p++)) * PRIME5), 11) * PRIME1;
			--len;
		}
		return (mix1(mix1(mix1(h, PRIME2, 33), PRIME3, 29), 1, 32));
	}
	SP_HASH_INLINE static constexpr sprt::uint64_t h32bytes(const char *__restrict__ p,
			sprt::uint64_t len, sprt::uint64_t v1, sprt::uint64_t v2, sprt::uint64_t v3,
			sprt::uint64_t v4) {
		const char *const limit = p + len - 32;
		do {
			v1 = fetch64(p, v1);
			p += 8;
			v2 = fetch64(p, v2);
			p += 8;
			v3 = fetch64(p, v3);
			p += 8;
			v4 = fetch64(p, v4);
			p += 8;
		} while (p <= limit);
		return mix3(
				mix3(mix3(mix3(rotl(v1, 1) + rotl(v2, 7) + rotl(v3, 12) + rotl(v4, 18), v1), v2),
						v3),
				v4);
	}
	SP_HASH_INLINE static constexpr sprt::uint64_t h32bytes(const char *__restrict__ p,
			sprt::uint64_t len, const sprt::uint64_t seed) {
		return h32bytes(p, len, seed + PRIME1 + PRIME2, seed + PRIME2, seed, seed - PRIME1);
	}
};

inline constexpr sprt::uint32_t hash32(const char *str, sprt::uint32_t len,
		sprt::uint32_t seed = 0) {
	return xxh32::hash(str, len, seed);
}

inline constexpr sprt::uint64_t hash64(const char *str, sprt::size_t len, sprt::uint64_t seed = 0) {
	return xxh64::hash(str, len, seed);
}

inline constexpr sprt::size_t hashSize(const char *str, sprt::size_t len, sprt::uint64_t seed = 0) {
	if constexpr (sizeof(sprt::size_t) == 4) {
		return xxh32::hash(str, len, seed);
	} else {
		return xxh64::hash(str, len, seed);
	}
}

} // namespace stappler::hash

namespace STAPPLER_VERSIONIZED stappler {

// used for naming/hashing (like "MyTag"_tag)
constexpr sprt::uint32_t operator""_hash(const char *str, sprt::size_t len) {
	return stappler::hash::hash32(str, sprt::uint32_t(len));
}
constexpr sprt::uint32_t operator""_tag(const char *str, sprt::size_t len) {
	return stappler::hash::hash32(str, sprt::uint32_t(len));
}

constexpr sprt::uint64_t operator""_hash64(const char *str, sprt::size_t len) {
	return stappler::hash::hash64(str, len);
}
constexpr sprt::uint64_t operator""_tag64(const char *str, sprt::size_t len) {
	return stappler::hash::hash64(str, len);
}

} // namespace STAPPLER_VERSIONIZED stappler

#endif /* LIBSTAPPLER_COMMON_DETAIL_SPHASH_H_ */
