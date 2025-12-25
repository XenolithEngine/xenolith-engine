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

#include "SPRuntimeHash.h"
#include <c/__sprt_string.h>

namespace sprt::sha1 {

/* SHA f()-functions */
#define f1(x, y, z)	((x & y) | (~x & z))
#define f2(x, y, z)	(x ^ y ^ z)
#define f3(x, y, z)	((x & y) | (x & z) | (y & z))
#define f4(x, y, z)	(x ^ y ^ z)

/* SHA constants */
static constexpr uint32_t CONST1 = 0x5a82'7999L;
static constexpr uint32_t CONST2 = 0x6ed9'eba1L;
static constexpr uint32_t CONST3 = 0x8f1b'bcdcL;
static constexpr uint32_t CONST4 = 0xca62'c1d6L;

static constexpr uint32_t SHA_BLOCKSIZE = 64;

/* 32-bit rotate */

#define ROT32(x, n)	((x << n) | (x >> (32 - n)))

#define FUNC(n, i) \
	temp = ROT32(A,5) + f##n(B,C,D) + E + W[i] + CONST##n; \
	E = D; D = C; C = ROT32(B,30); B = A; A = temp

/* do SHA transformation */
static void sha_transform(Ctx &sha_info) {
	int i;
	uint32_t temp, A, B, C, D, E, W[80];

	for (i = 0; i < 16; ++i) { W[i] = sha_info.data[i]; }
	for (i = 16; i < 80; ++i) {
		W[i] = W[i - 3] ^ W[i - 8] ^ W[i - 14] ^ W[i - 16];

		W[i] = ROT32(W[i], 1);
	}

	A = sha_info.digest[0];
	B = sha_info.digest[1];
	C = sha_info.digest[2];
	D = sha_info.digest[3];
	E = sha_info.digest[4];

	// clang-format off
	FUNC(1, 0);  FUNC(1, 1);  FUNC(1, 2);  FUNC(1, 3);  FUNC(1, 4);
	FUNC(1, 5);  FUNC(1, 6);  FUNC(1, 7);  FUNC(1, 8);  FUNC(1, 9);
	FUNC(1,10);  FUNC(1,11);  FUNC(1,12);  FUNC(1,13);  FUNC(1,14);
	FUNC(1,15);  FUNC(1,16);  FUNC(1,17);  FUNC(1,18);  FUNC(1,19);

	FUNC(2,20);  FUNC(2,21);  FUNC(2,22);  FUNC(2,23);  FUNC(2,24);
	FUNC(2,25);  FUNC(2,26);  FUNC(2,27);  FUNC(2,28);  FUNC(2,29);
	FUNC(2,30);  FUNC(2,31);  FUNC(2,32);  FUNC(2,33);  FUNC(2,34);
	FUNC(2,35);  FUNC(2,36);  FUNC(2,37);  FUNC(2,38);  FUNC(2,39);

	FUNC(3,40);  FUNC(3,41);  FUNC(3,42);  FUNC(3,43);  FUNC(3,44);
	FUNC(3,45);  FUNC(3,46);  FUNC(3,47);  FUNC(3,48);  FUNC(3,49);
	FUNC(3,50);  FUNC(3,51);  FUNC(3,52);  FUNC(3,53);  FUNC(3,54);
	FUNC(3,55);  FUNC(3,56);  FUNC(3,57);  FUNC(3,58);  FUNC(3,59);

	FUNC(4,60);  FUNC(4,61);  FUNC(4,62);  FUNC(4,63);  FUNC(4,64);
	FUNC(4,65);  FUNC(4,66);  FUNC(4,67);  FUNC(4,68);  FUNC(4,69);
	FUNC(4,70);  FUNC(4,71);  FUNC(4,72);  FUNC(4,73);  FUNC(4,74);
	FUNC(4,75);  FUNC(4,76);  FUNC(4,77);  FUNC(4,78);  FUNC(4,79);
	// clang-format on

	sha_info.digest[0] += A;
	sha_info.digest[1] += B;
	sha_info.digest[2] += C;
	sha_info.digest[3] += D;
	sha_info.digest[4] += E;
}

/* count is the number of bytes to do an endian flip */
static void maybe_byte_reverse(uint32_t *buffer, int count) {
	int i;
	uint8_t ct[4], *cp;

	/* do the swap only if it is little endian */
	if constexpr (endian::native == endian::little) {
		count /= sizeof(uint32_t);
		cp = reinterpret_cast<uint8_t *>(buffer);
		for (i = 0; i < count; ++i) {
			ct[0] = cp[0];
			ct[1] = cp[1];
			ct[2] = cp[2];
			ct[3] = cp[3];
			cp[0] = ct[3];
			cp[1] = ct[2];
			cp[2] = ct[1];
			cp[3] = ct[0];
			cp += sizeof(uint32_t);
		}
	}
}

void sha_init(Ctx &sha_info) {
	sha_info.digest[0] = 0x6745'2301L;
	sha_info.digest[1] = 0xefcd'ab89L;
	sha_info.digest[2] = 0x98ba'dcfeL;
	sha_info.digest[3] = 0x1032'5476L;
	sha_info.digest[4] = 0xc3d2'e1f0L;
	sha_info.count_lo = 0L;
	sha_info.count_hi = 0L;
	sha_info.local = 0;
}

void sha_process(Ctx &sha_info, const uint8_t *buffer, uint32_t count) {
	unsigned int i;

	if ((sha_info.count_lo + (count << 3)) < sha_info.count_lo) {
		++sha_info.count_hi;
	}
	sha_info.count_lo += count << 3;
	sha_info.count_hi += count >> 29;
	if (sha_info.local) {
		i = SHA_BLOCKSIZE - sha_info.local;
		if (i > count) {
			i = count;
		}
		__sprt_memcpy(reinterpret_cast<uint8_t *>(sha_info.data) + sha_info.local, buffer, i);
		count -= i;
		buffer += i;
		sha_info.local += i;
		if (sha_info.local == SHA_BLOCKSIZE) {
			maybe_byte_reverse(sha_info.data, SHA_BLOCKSIZE);
			sha_transform(sha_info);
		} else {
			return;
		}
	}
	while (count >= SHA_BLOCKSIZE) {
		__sprt_memcpy(sha_info.data, buffer, SHA_BLOCKSIZE);
		buffer += SHA_BLOCKSIZE;
		count -= SHA_BLOCKSIZE;
		maybe_byte_reverse(sha_info.data, SHA_BLOCKSIZE);
		sha_transform(sha_info);
	}
	__sprt_memcpy(sha_info.data, buffer, count);
	sha_info.local = count;
}

void sha_done(Ctx &sha_info, uint8_t digest[Length]) {
	int count, i, j;
	uint32_t lo_bit_count, hi_bit_count, k;

	lo_bit_count = sha_info.count_lo;
	hi_bit_count = sha_info.count_hi;
	count = int32_t((lo_bit_count >> 3) & 0x3f);
	reinterpret_cast<uint8_t *>(sha_info.data)[count++] = 0x80;
	if (count > int(SHA_BLOCKSIZE - 8)) {
		__sprt_memset(reinterpret_cast<uint8_t *>(sha_info.data) + count, 0, SHA_BLOCKSIZE - count);
		maybe_byte_reverse(sha_info.data, SHA_BLOCKSIZE);
		sha_transform(sha_info);
		__sprt_memset(reinterpret_cast<uint8_t *>(sha_info.data), 0, SHA_BLOCKSIZE - 8);
	} else {
		__sprt_memset(reinterpret_cast<uint8_t *>(sha_info.data) + count, 0,
				SHA_BLOCKSIZE - 8 - count);
	}
	maybe_byte_reverse(sha_info.data, SHA_BLOCKSIZE);
	sha_info.data[14] = hi_bit_count;
	sha_info.data[15] = lo_bit_count;
	sha_transform(sha_info);

	for (i = 0, j = 0; j < int(Length); i++) {
		k = sha_info.digest[i];
		digest[j++] = uint8_t((k >> 24) & 0xff);
		digest[j++] = uint8_t((k >> 16) & 0xff);
		digest[j++] = uint8_t((k >> 8) & 0xff);
		digest[j++] = uint8_t(k & 0xff);
	}
}

} // namespace sprt::sha1

namespace sprt::sha256 {

using sha256_state = Ctx;

typedef uint32_t u32;
typedef uint64_t u64;

// clang-format off

static const u32 K[64] = {
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL, 0x3956c25bUL,
    0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL, 0xd807aa98UL, 0x12835b01UL,
    0x243185beUL, 0x550c7dc3UL, 0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL,
    0xc19bf174UL, 0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
    0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL, 0x983e5152UL,
    0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL, 0xc6e00bf3UL, 0xd5a79147UL,
    0x06ca6351UL, 0x14292967UL, 0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL,
    0x53380d13UL, 0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
    0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL, 0xd192e819UL,
    0xd6990624UL, 0xf40e3585UL, 0x106aa070UL, 0x19a4c116UL, 0x1e376c08UL,
    0x2748774cUL, 0x34b0bcb5UL, 0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL,
    0x682e6ff3UL, 0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
    0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};

// clang-format on

static u32 sha_min(u32 x, u32 y) { return x < y ? x : y; }

static u32 load32(const unsigned char *y) {
	return (u32(y[0]) << 24) | (u32(y[1]) << 16) | (u32(y[2]) << 8) | (u32(y[3]) << 0);
}

static void store64(u64 x, unsigned char *y) {
	for (int i = 0; i != 8; ++i) { y[i] = (x >> ((7 - i) * 8)) & 255; }
}

static void store32(u32 x, unsigned char *y) {
	for (int i = 0; i != 4; ++i) { y[i] = (x >> ((3 - i) * 8)) & 255; }
}

static u32 Ch(u32 x, u32 y, u32 z) { return z ^ (x & (y ^ z)); }
static u32 Maj(u32 x, u32 y, u32 z) { return ((x | y) & z) | (x & y); }
static u32 Rot(u32 x, u32 n) { return (x >> (n & 31)) | (x << (32 - (n & 31))); }
static u32 Sh(u32 x, u32 n) { return x >> n; }
static u32 Sigma0(u32 x) { return Rot(x, 2) ^ Rot(x, 13) ^ Rot(x, 22); }
static u32 Sigma1(u32 x) { return Rot(x, 6) ^ Rot(x, 11) ^ Rot(x, 25); }
static u32 Gamma0(u32 x) { return Rot(x, 7) ^ Rot(x, 18) ^ Sh(x, 3); }
static u32 Gamma1(u32 x) { return Rot(x, 17) ^ Rot(x, 19) ^ Sh(x, 10); }

static void sha_compress(sha256_state &md, const unsigned char *buf) {
	u32 S[8], W[64], t0, t1, t;

	// Copy state into S
	for (int i = 0; i < 8; i++) { S[i] = md.state[i]; }

	// Copy the state into 512-bits into W[0..15]
	for (int i = 0; i < 16; i++) { W[i] = load32(buf + (4 * i)); }

	// Fill W[16..63]
	for (int i = 16; i < 64; i++) {
		W[i] = Gamma1(W[i - 2]) + W[i - 7] + Gamma0(W[i - 15]) + W[i - 16];
	}

	// Compress
	auto RND = [&](u32 a, u32 b, u32 c, u32 &d, u32 e, u32 f, u32 g, u32 &h, u32 i) {
		t0 = h + Sigma1(e) + Ch(e, f, g) + K[i] + W[i];
		t1 = Sigma0(a) + Maj(a, b, c);
		d += t0;
		h = t0 + t1;
	};

	for (int i = 0; i < 64; ++i) {
		RND(S[0], S[1], S[2], S[3], S[4], S[5], S[6], S[7], i);
		t = S[7];
		S[7] = S[6];
		S[6] = S[5];
		S[5] = S[4];
		S[4] = S[3];
		S[3] = S[2];
		S[2] = S[1];
		S[1] = S[0];
		S[0] = t;
	}

	// Feedback
	for (int i = 0; i < 8; i++) { md.state[i] = md.state[i] + S[i]; }
}

// Public interface

void sha_init(sha256_state &md) {
	md.curlen = 0;
	md.length = 0;
	md.state[0] = 0x6A09'E667UL;
	md.state[1] = 0xBB67'AE85UL;
	md.state[2] = 0x3C6E'F372UL;
	md.state[3] = 0xA54F'F53AUL;
	md.state[4] = 0x510E'527FUL;
	md.state[5] = 0x9B05'688CUL;
	md.state[6] = 0x1F83'D9ABUL;
	md.state[7] = 0x5BE0'CD19UL;
}

void sha_process(sha256_state &md, const uint8_t *src, uint32_t inlen) {
	const u32 block_size = sizeof(sha256_state::buf);
	auto in = static_cast<const unsigned char *>(src);

	while (inlen > 0) {
		if (md.curlen == 0 && inlen >= block_size) {
			sha_compress(md, in);
			md.length += block_size * 8;
			in += block_size;
			inlen -= block_size;
		} else {
			u32 n = sha_min(inlen, (block_size - md.curlen));
			::__sprt_memcpy(md.buf + md.curlen, in, n);
			md.curlen += n;
			in += n;
			inlen -= n;

			if (md.curlen == block_size) {
				sha_compress(md, md.buf);
				md.length += 8 * block_size;
				md.curlen = 0;
			}
		}
	}
}

void sha_done(sha256_state &md, uint8_t out[Length]) {
	// Increase the length of the message
	md.length += md.curlen * 8;

	// Append the '1' bit
	md.buf[md.curlen++] = static_cast<unsigned char>(0x80);

	// If the length is currently above 56 bytes we append zeros then compress.
	// Then we can fall back to padding zeros and length encoding like normal.
	if (md.curlen > 56) {
		while (md.curlen < 64) { md.buf[md.curlen++] = 0; }
		sha_compress(md, md.buf);
		md.curlen = 0;
	}

	// Pad upto 56 bytes of zeroes
	while (md.curlen < 56) { md.buf[md.curlen++] = 0; }

	// Store length
	store64(md.length, md.buf + 56);
	sha_compress(md, md.buf);

	// Copy output
	for (int i = 0; i < 8; i++) {
		store32(md.state[i], static_cast<unsigned char *>(out) + (4 * i));
	}
}

} // namespace sprt::sha256

namespace sprt::sha512 {

using sha512_state = Ctx;

typedef uint32_t u32;
typedef uint64_t u64;

// clang-format off

static const u64 K[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL, 0x3956c25bf348b538ULL,
    0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL, 0xd807aa98a3030242ULL, 0x12835b0145706fbeULL,
    0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL, 0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL,
    0xc19bf174cf692694ULL, 0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL, 0x983e5152ee66dfabULL,
    0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL, 0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
    0x06ca6351e003826fULL, 0x142929670a0e6e70ULL, 0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL,
    0x53380d139d95b3dfULL, 0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL, 0xd192e819d6ef5218ULL,
    0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL, 0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL,
    0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL, 0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL,
    0x682e6ff3d6b2b8a3ULL, 0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL, 0xca273eceea26619cULL,
    0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL, 0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL,
    0x113f9804bef90daeULL, 0x1b710b35131c471bULL, 0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL,
    0x431d67c49c100d4cULL, 0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

// clang-format on

static u32 sha_min(u32 x, u32 y) { return x < y ? x : y; }

static void store64(u64 x, unsigned char *y) {
	for (int i = 0; i != 8; ++i) { y[i] = (x >> ((7 - i) * 8)) & 255; }
}

static u64 load64(const unsigned char *y) {
	u64 res = 0;
	for (int i = 0; i != 8; ++i) { res |= u64(y[i]) << ((7 - i) * 8); }
	return res;
}

static u64 Ch(u64 x, u64 y, u64 z) { return z ^ (x & (y ^ z)); }
static u64 Maj(u64 x, u64 y, u64 z) { return ((x | y) & z) | (x & y); }
static u64 Rot(u64 x, u64 n) { return (x >> (n & 63)) | (x << (64 - (n & 63))); }
static u64 Sh(u64 x, u64 n) { return x >> n; }
static u64 Sigma0(u64 x) { return Rot(x, 28) ^ Rot(x, 34) ^ Rot(x, 39); }
static u64 Sigma1(u64 x) { return Rot(x, 14) ^ Rot(x, 18) ^ Rot(x, 41); }
static u64 Gamma0(u64 x) { return Rot(x, 1) ^ Rot(x, 8) ^ Sh(x, 7); }
static u64 Gamma1(u64 x) { return Rot(x, 19) ^ Rot(x, 61) ^ Sh(x, 6); }

static void sha_compress(sha512_state &md, const unsigned char *buf) {
	u64 S[8], W[80], t0, t1;

	// Copy state into S
	for (int i = 0; i < 8; i++) { S[i] = md.state[i]; }

	// Copy the state into 1024-bits into W[0..15]
	for (int i = 0; i < 16; i++) { W[i] = load64(buf + (8 * i)); }

	// Fill W[16..79]
	for (int i = 16; i < 80; i++) {
		W[i] = Gamma1(W[i - 2]) + W[i - 7] + Gamma0(W[i - 15]) + W[i - 16];
	}

	// Compress
	auto RND = [&](u64 a, u64 b, u64 c, u64 &d, u64 e, u64 f, u64 g, u64 &h, u64 i) {
		t0 = h + Sigma1(e) + Ch(e, f, g) + K[i] + W[i];
		t1 = Sigma0(a) + Maj(a, b, c);
		d += t0;
		h = t0 + t1;
	};

	for (int i = 0; i < 80; i += 8) {
		RND(S[0], S[1], S[2], S[3], S[4], S[5], S[6], S[7], i + 0);
		RND(S[7], S[0], S[1], S[2], S[3], S[4], S[5], S[6], i + 1);
		RND(S[6], S[7], S[0], S[1], S[2], S[3], S[4], S[5], i + 2);
		RND(S[5], S[6], S[7], S[0], S[1], S[2], S[3], S[4], i + 3);
		RND(S[4], S[5], S[6], S[7], S[0], S[1], S[2], S[3], i + 4);
		RND(S[3], S[4], S[5], S[6], S[7], S[0], S[1], S[2], i + 5);
		RND(S[2], S[3], S[4], S[5], S[6], S[7], S[0], S[1], i + 6);
		RND(S[1], S[2], S[3], S[4], S[5], S[6], S[7], S[0], i + 7);
	}

	// Feedback
	for (int i = 0; i < 8; i++) { md.state[i] = md.state[i] + S[i]; }
}

// Public interface

void sha_init(sha512_state &md) {
	md.curlen = 0;
	md.length = 0;
	md.state[0] = 0x6a09'e667'f3bc'c908ULL;
	md.state[1] = 0xbb67'ae85'84ca'a73bULL;
	md.state[2] = 0x3c6e'f372'fe94'f82bULL;
	md.state[3] = 0xa54f'f53a'5f1d'36f1ULL;
	md.state[4] = 0x510e'527f'ade6'82d1ULL;
	md.state[5] = 0x9b05'688c'2b3e'6c1fULL;
	md.state[6] = 0x1f83'd9ab'fb41'bd6bULL;
	md.state[7] = 0x5be0'cd19'137e'2179ULL;
}

void sha_process(sha512_state &md, const uint8_t *src, uint32_t inlen) {
	const u32 block_size = sizeof(sha512_state::buf);
	auto in = static_cast<const unsigned char *>(src);

	while (inlen > 0) {
		if (md.curlen == 0 && inlen >= block_size) {
			sha_compress(md, in);
			md.length += block_size * 8;
			in += block_size;
			inlen -= block_size;
		} else {
			u32 n = sha_min(inlen, (block_size - md.curlen));
			__sprt_memcpy(md.buf + md.curlen, in, n);
			md.curlen += n;
			in += n;
			inlen -= n;

			if (md.curlen == block_size) {
				sha_compress(md, md.buf);
				md.length += 8 * block_size;
				md.curlen = 0;
			}
		}
	}
}

void sha_done(sha512_state &md, uint8_t out[Length]) {
	// Increase the length of the message
	md.length += md.curlen * 8ULL;

	// Append the '1' bit
	md.buf[md.curlen++] = static_cast<unsigned char>(0x80);

	// If the length is currently above 112 bytes we append zeros then compress.
	// Then we can fall back to padding zeros and length encoding like normal.
	if (md.curlen > 112) {
		while (md.curlen < 128) { md.buf[md.curlen++] = 0; }
		sha_compress(md, md.buf);
		md.curlen = 0;
	}

	// Pad upto 120 bytes of zeroes
	// note: that from 112 to 120 is the 64 MSB of the length.  We assume that
	// you won't hash 2^64 bits of data... :-)
	while (md.curlen < 120) { md.buf[md.curlen++] = 0; }

	// Store length
	store64(md.length, md.buf + 120);
	sha_compress(md, md.buf);

	// Copy output
	for (int i = 0; i < 8; i++) {
		store64(md.state[i], static_cast<unsigned char *>(out) + (8 * i));
	}
}

} // namespace sprt::sha512
