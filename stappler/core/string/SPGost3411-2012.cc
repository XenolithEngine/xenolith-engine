/**
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

// based on original implementation by Alexey Degtyarev <alexey@renatasystems.org>
// https://github.com/adegtyarev/streebog

// see original license: https://github.com/adegtyarev/streebog/blob/master/LICENSE

#include "SPCoreCrypto.h"

#if XWIN
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#endif

#include "simde/x86/sse2.h"

#if XWIN
#pragma clang diagnostic pop
#endif

#if defined(__GNUC__) || defined(__clang__)
#define RESTRICT __restrict__
#else
#define RESTRICT
#endif

// TODO: replace with better detection
#if (__i386__) || (_M_IX86) || (__x86_64__) || (_M_X64) || (__arm__) || (_M_ARM) || (__arm64__) \
		|| (__arm64) || defined(__aarch64__) || defined(__e2k__)
#define __GOST3411_LITTLE_ENDIAN__ 1
#else
#define __GOST3411_BIG_ENDIAN__ 1
#endif

// TODO: add detection
//#define HAVE_ADDCARRY_U64

#define __GOST3411_LOAD_SSE2__ 1

#ifndef __GOST3411_LOAD_SSE2__

#define X(x, y, z) { \
    z->QWORD[0] = x->QWORD[0] ^ y->QWORD[0]; \
    z->QWORD[1] = x->QWORD[1] ^ y->QWORD[1]; \
    z->QWORD[2] = x->QWORD[2] ^ y->QWORD[2]; \
    z->QWORD[3] = x->QWORD[3] ^ y->QWORD[3]; \
    z->QWORD[4] = x->QWORD[4] ^ y->QWORD[4]; \
    z->QWORD[5] = x->QWORD[5] ^ y->QWORD[5]; \
    z->QWORD[6] = x->QWORD[6] ^ y->QWORD[6]; \
    z->QWORD[7] = x->QWORD[7] ^ y->QWORD[7]; \
}

#ifndef __GOST3411_BIG_ENDIAN__
#define __XLPS_FOR for (_i = 0; _i <= 7; _i++)
#define _datai _i
#else
#define __XLPS_FOR for (_i = 7; _i >= 0; _i--)
#define _datai 7 - _i
#endif

#define XLPS(x, y, data) { \
    unsigned long long r0, r1, r2, r3, r4, r5, r6, r7; \
    int _i; \
    \
    r0 = x->QWORD[0] ^ y->QWORD[0]; \
    r1 = x->QWORD[1] ^ y->QWORD[1]; \
    r2 = x->QWORD[2] ^ y->QWORD[2]; \
    r3 = x->QWORD[3] ^ y->QWORD[3]; \
    r4 = x->QWORD[4] ^ y->QWORD[4]; \
    r5 = x->QWORD[5] ^ y->QWORD[5]; \
    r6 = x->QWORD[6] ^ y->QWORD[6]; \
    r7 = x->QWORD[7] ^ y->QWORD[7]; \
    \
    \
    __XLPS_FOR \
    {\
        data->QWORD[_datai]  = _Ax(0, (r0 >> (_i << 3)) & 0xFF); \
        data->QWORD[_datai] ^= _Ax(1, (r1 >> (_i << 3)) & 0xFF); \
        data->QWORD[_datai] ^= _Ax(2, (r2 >> (_i << 3)) & 0xFF); \
        data->QWORD[_datai] ^= _Ax(3, (r3 >> (_i << 3)) & 0xFF); \
        data->QWORD[_datai] ^= _Ax(4, (r4 >> (_i << 3)) & 0xFF); \
        data->QWORD[_datai] ^= _Ax(5, (r5 >> (_i << 3)) & 0xFF); \
        data->QWORD[_datai] ^= _Ax(6, (r6 >> (_i << 3)) & 0xFF); \
        data->QWORD[_datai] ^= _Ax(7, (r7 >> (_i << 3)) & 0xFF); \
    }\
}

#define ROUND(i, Ki, data) { \
    XLPS(Ki, (&_C(i)), Ki); \
    XLPS(Ki, data, data); \
}

#else

#define LO(v) ((unsigned char) (v))
#define HI(v) ((unsigned char) (((unsigned int) (v)) >> 8))

#ifdef __i386__
#define EXTRACT EXTRACT32
#else
#define EXTRACT EXTRACT64
#endif

#ifndef __ICC
#define _mm_cvtsi64_m64(v) (__m64) v
#define _mm_cvtm64_si64(v) (long long) v
#endif

#define LOAD(P, xmm0, xmm1, xmm2, xmm3) { \
    const simde__m128i *__m128p = (const simde__m128i *) &P[0]; \
    xmm0 = simde_mm_loadu_si128(&__m128p[0]); \
    xmm1 = simde_mm_loadu_si128(&__m128p[1]); \
    xmm2 = simde_mm_loadu_si128(&__m128p[2]); \
    xmm3 = simde_mm_loadu_si128(&__m128p[3]); \
}

#define UNLOAD(P, xmm0, xmm1, xmm2, xmm3) { \
    simde__m128i *__m128p = (simde__m128i *) &P[0]; \
    simde_mm_store_si128(&__m128p[0], xmm0); \
    simde_mm_store_si128(&__m128p[1], xmm1); \
    simde_mm_store_si128(&__m128p[2], xmm2); \
    simde_mm_store_si128(&__m128p[3], xmm3); \
}

#define X128R(xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7) { \
    xmm0 = simde_mm_xor_si128(xmm0, xmm4); \
    xmm1 = simde_mm_xor_si128(xmm1, xmm5); \
    xmm2 = simde_mm_xor_si128(xmm2, xmm6); \
    xmm3 = simde_mm_xor_si128(xmm3, xmm7); \
}

#define X128M(P, xmm0, xmm1, xmm2, xmm3) { \
    const simde__m128i *__m128p = (const simde__m128i *) &P[0]; \
    xmm0 = simde_mm_xor_si128(xmm0, simde_mm_loadu_si128(&__m128p[0])); \
    xmm1 = simde_mm_xor_si128(xmm1, simde_mm_loadu_si128(&__m128p[1])); \
    xmm2 = simde_mm_xor_si128(xmm2, simde_mm_loadu_si128(&__m128p[2])); \
    xmm3 = simde_mm_xor_si128(xmm3, simde_mm_loadu_si128(&__m128p[3])); \
}

#define simde_mm_xor_64(mm0, mm1) simde_mm_xor_si64(mm0, simde_mm_cvtsi64_m64(mm1))

#define EXTRACT32(row, xmm0, xmm1, xmm2, xmm3, xmm4) { \
    unsigned short ax; \
    simde__m64 mm0, mm1; \
     \
    ax = (unsigned short) simde_mm_extract_epi16(xmm0, row + 0); \
    mm0  = simde_mm_cvtsi64_m64(_Ax(0, LO(ax))); \
    mm1  = simde_mm_cvtsi64_m64(_Ax(0, HI(ax))); \
    \
    ax = (unsigned short) simde_mm_extract_epi16(xmm0, row + 4); \
    mm0 = simde_mm_xor_64(mm0, _Ax(1, LO(ax))); \
    mm1 = simde_mm_xor_64(mm1, _Ax(1, HI(ax))); \
    \
    ax = (unsigned short) simde_mm_extract_epi16(xmm1, row + 0); \
    mm0 = simde_mm_xor_64(mm0, _Ax(2, LO(ax))); \
    mm1 = simde_mm_xor_64(mm1, _Ax(2, HI(ax))); \
    \
    ax = (unsigned short) simde_mm_extract_epi16(xmm1, row + 4); \
    mm0 = simde_mm_xor_64(mm0, _Ax(3, LO(ax))); \
    mm1 = simde_mm_xor_64(mm1, _Ax(3, HI(ax))); \
    \
    ax = (unsigned short) simde_mm_extract_epi16(xmm2, row + 0); \
    mm0 = simde_mm_xor_64(mm0, _Ax(4, LO(ax))); \
    mm1 = simde_mm_xor_64(mm1, _Ax(4, HI(ax))); \
    \
    ax = (unsigned short) simde_mm_extract_epi16(xmm2, row + 4); \
    mm0 = simde_mm_xor_64(mm0, _Ax(5, LO(ax))); \
    mm1 = simde_mm_xor_64(mm1, _Ax(5, HI(ax))); \
    \
    ax = (unsigned short) simde_mm_extract_epi16(xmm3, row + 0); \
    mm0 = simde_mm_xor_64(mm0, _Ax(6, LO(ax))); \
    mm1 = simde_mm_xor_64(mm1, _Ax(6, HI(ax))); \
    \
    ax = (unsigned short) simde_mm_extract_epi16(xmm3, row + 4); \
    mm0 = simde_mm_xor_64(mm0, _Ax(7, LO(ax))); \
    mm1 = simde_mm_xor_64(mm1, _Ax(7, HI(ax))); \
    \
    xmm4 = simde_mm_set_epi64(mm1, mm0); \
}

#define __EXTRACT64(row, xmm0, xmm1, xmm2, xmm3, xmm4) { \
    __m128i tmm4; \
    register unsigned long long r0, r1; \
    r0  = _Ax(0, simde_mm_extract_epi8(xmm0, row + 0)); \
    r0 ^= _Ax(1, simde_mm_extract_epi8(xmm0, row + 8)); \
    r0 ^= _Ax(2, simde_mm_extract_epi8(xmm1, row + 0)); \
    r0 ^= _Ax(3, simde_mm_extract_epi8(xmm1, row + 8)); \
    r0 ^= _Ax(4, simde_mm_extract_epi8(xmm2, row + 0)); \
    r0 ^= _Ax(5, simde_mm_extract_epi8(xmm2, row + 8)); \
    r0 ^= _Ax(6, simde_mm_extract_epi8(xmm3, row + 0)); \
    r0 ^= _Ax(7, simde_mm_extract_epi8(xmm3, row + 8)); \
    \
    r1  = _Ax(0, simde_mm_extract_epi8(xmm0, row + 1)); \
    r1 ^= _Ax(1, simde_mm_extract_epi8(xmm0, row + 9)); \
    r1 ^= _Ax(2, simde_mm_extract_epi8(xmm1, row + 1)); \
    r1 ^= _Ax(3, simde_mm_extract_epi8(xmm1, row + 9)); \
    r1 ^= _Ax(4, simde_mm_extract_epi8(xmm2, row + 1)); \
    r1 ^= _Ax(5, simde_mm_extract_epi8(xmm2, row + 9)); \
    r1 ^= _Ax(6, simde_mm_extract_epi8(xmm3, row + 1)); \
    r1 ^= _Ax(7, simde_mm_extract_epi8(xmm3, row + 9)); \
    xmm4 = simde_mm_cvtsi64_si128((long long) r0); \
    tmm4 = simde_mm_cvtsi64_si128((long long) r1); \
    xmm4 = simde_mm_unpacklo_epi64(xmm4, tmm4); \
}

#define EXTRACT64(row, xmm0, xmm1, xmm2, xmm3, xmm4) { \
    simde__m128i tmm4; \
    unsigned short ax; \
    unsigned long long r0, r1; \
     \
    ax = (unsigned short) simde_mm_extract_epi16(xmm0, row + 0); \
    r0  = _Ax(0, LO(ax)); \
    r1  = _Ax(0, HI(ax)); \
    \
    ax = (unsigned short) simde_mm_extract_epi16(xmm0, row + 4); \
    r0 ^= _Ax(1, LO(ax)); \
    r1 ^= _Ax(1, HI(ax)); \
    \
    ax = (unsigned short) simde_mm_extract_epi16(xmm1, row + 0); \
    r0 ^= _Ax(2, LO(ax)); \
    r1 ^= _Ax(2, HI(ax)); \
    \
    ax = (unsigned short) simde_mm_extract_epi16(xmm1, row + 4); \
    r0 ^= _Ax(3, LO(ax)); \
    r1 ^= _Ax(3, HI(ax)); \
    \
    ax = (unsigned short) simde_mm_extract_epi16(xmm2, row + 0); \
    r0 ^= _Ax(4, LO(ax)); \
    r1 ^= _Ax(4, HI(ax)); \
    \
    ax = (unsigned short) simde_mm_extract_epi16(xmm2, row + 4); \
    r0 ^= _Ax(5, LO(ax)); \
    r1 ^= _Ax(5, HI(ax)); \
    \
    ax = (unsigned short) simde_mm_extract_epi16(xmm3, row + 0); \
    r0 ^= _Ax(6, LO(ax)); \
    r1 ^= _Ax(6, HI(ax)); \
    \
    ax = (unsigned short) simde_mm_extract_epi16(xmm3, row + 4); \
    r0 ^= _Ax(7, LO(ax)); \
    r1 ^= _Ax(7, HI(ax)); \
    \
    xmm4 = simde_mm_cvtsi64_si128((long long) r0); \
    tmm4 = simde_mm_cvtsi64_si128((long long) r1); \
    xmm4 = simde_mm_unpacklo_epi64(xmm4, tmm4); \
}

#define XLPS128M(P, xmm0, xmm1, xmm2, xmm3) { \
    simde__m128i tmm0, tmm1, tmm2, tmm3; \
    X128M(P, xmm0, xmm1, xmm2, xmm3); \
    \
    EXTRACT(0, xmm0, xmm1, xmm2, xmm3, tmm0); \
    EXTRACT(1, xmm0, xmm1, xmm2, xmm3, tmm1); \
    EXTRACT(2, xmm0, xmm1, xmm2, xmm3, tmm2); \
    EXTRACT(3, xmm0, xmm1, xmm2, xmm3, tmm3); \
    \
    xmm0 = tmm0; \
    xmm1 = tmm1; \
    xmm2 = tmm2; \
    xmm3 = tmm3; \
}

#define XLPS128R(xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7) { \
    simde__m128i tmm0, tmm1, tmm2, tmm3; \
    X128R(xmm4, xmm5, xmm6, xmm7, xmm0, xmm1, xmm2, xmm3); \
    \
    EXTRACT(0, xmm4, xmm5, xmm6, xmm7, tmm0); \
    EXTRACT(1, xmm4, xmm5, xmm6, xmm7, tmm1); \
    EXTRACT(2, xmm4, xmm5, xmm6, xmm7, tmm2); \
    EXTRACT(3, xmm4, xmm5, xmm6, xmm7, tmm3); \
    \
    xmm4 = tmm0; \
    xmm5 = tmm1; \
    xmm6 = tmm2; \
    xmm7 = tmm3; \
}

#define ROUND128(i, xmm0, xmm2, xmm4, xmm6, xmm1, xmm3, xmm5, xmm7) { \
    XLPS128M((&_C(i)), xmm0, xmm2, xmm4, xmm6); \
    XLPS128R(xmm0, xmm2, xmm4, xmm6, xmm1, xmm3, xmm5, xmm7); \
}

#endif

namespace STAPPLER_VERSIONIZED stappler::crypto {

static const union uint512_u _buffer0 = {
	{0x0ULL, 0x0ULL, 0x0ULL, 0x0ULL, 0x0ULL, 0x0ULL, 0x0ULL, 0x0ULL}};

static const uint512_u &_buffer512() {
	if constexpr (sprt::endian::native == sprt::endian::little) {
		static const uint512_u _b512 = {
			{0x0000'0000'0000'0200ULL, 0x0ULL, 0x0ULL, 0x0ULL, 0x0ULL, 0x0ULL, 0x0ULL, 0x0ULL}};
		return _b512;
	} else {
		static const uint512_u _b512 = {
			{0x0002'0000'0000'0000ULL, 0x0ULL, 0x0ULL, 0x0ULL, 0x0ULL, 0x0ULL, 0x0ULL, 0x0ULL}};
		return _b512;
	}
}

static const uint512_u &_C(int v) {
	if constexpr (sprt::endian::native == sprt::endian::little) {
		static const uint512_u C[12] = {
			{{0xdd80'6559'f2a6'4507ULL, 0x0576'7436'cc74'4d23ULL, 0xa242'2a08'a460'd315ULL,
				0x4b7c'e091'9267'6901ULL, 0x714e'b88d'7585'c4fcULL, 0x2f6a'7643'2e45'd016ULL,
				0xebcb'2f81'c065'7c1fULL, 0xb108'5bda'1eca'dae9ULL}},
			{{0xe679'0470'21b1'9bb7ULL, 0x55dd'a21b'd7cb'cd56ULL, 0x5cb5'61c2'db0a'a7caULL,
				0x9ab5'176b'12d6'9958ULL, 0x61d5'5e0f'16b5'0131ULL, 0xf3fe'ea72'0a23'2b98ULL,
				0x4fe3'9d46'0f70'b5d7ULL, 0x6fa3'b58a'a99d'2f1aULL}},
			{{0x991e'96f5'0aba'0ab2ULL, 0xc2b6'f443'867a'db31ULL, 0xc1c9'3a37'6062'db09ULL,
				0xd3e2'0fe4'9035'9eb1ULL, 0xf2ea'7514'b129'7b7bULL, 0x06f1'5e5f'529c'1f8bULL,
				0x0a39'fc28'6a3d'8435ULL, 0xf574'dcac'2bce'2fc7ULL}},
			{{0x220c'bebc'84e3'd12eULL, 0x3453'eaa1'93e8'37f1ULL, 0xd8b7'1333'9352'03beULL,
				0xa9d7'2c82'ed03'd675ULL, 0x9d72'1cad'685e'353fULL, 0x488e'857e'335c'3c7dULL,
				0xf948'e1a0'5d71'e4ddULL, 0xef1f'dfb3'e815'66d2ULL}},
			{{0x6017'58fd'7c6c'fe57ULL, 0x7a56'a27e'a9ea'63f5ULL, 0xdfff'00b7'2327'1a16ULL,
				0xbfcd'1747'253a'f5a3ULL, 0x359e'35d7'800f'ffbdULL, 0x7f15'1c1f'1686'104aULL,
				0x9a3f'410c'6ca9'2363ULL, 0x4bea'6bac'ad47'4799ULL}},
			{{0xfa68'407a'4664'7d6eULL, 0xbf71'c572'3690'4f35ULL, 0x0af2'1f66'c2be'c6b6ULL,
				0xcffa'a6b7'1c9a'b7b4ULL, 0x187f'9ab4'9af0'8ec6ULL, 0x2d66'c4f9'5142'a46cULL,
				0x6fa4'c33b'7a30'39c0ULL, 0xae4f'aeae'1d3a'd3d9ULL}},
			{{0x8886'564d'3a14'd493ULL, 0x3517'454c'a23c'4af3ULL, 0x0647'6983'284a'0504ULL,
				0x0992'abc5'2d82'2c37ULL, 0xd347'3e33'197a'93c9ULL, 0x399e'c6c7'e6bf'87c9ULL,
				0x51ac'86fe'bf24'0954ULL, 0xf4c7'0e16'eeaa'c5ecULL}},
			{{0xa47f'0dd4'bf02'e71eULL, 0x36ac'c235'5951'a8d9ULL, 0x69d1'8d2b'd1a5'c42fULL,
				0xf489'2bcb'929b'0690ULL, 0x89b4'443b'4ddb'c49aULL, 0x4eb7'f871'9c36'de1eULL,
				0x03e7'aa02'0c6e'4141ULL, 0x9b1f'5b42'4d93'c9a7ULL}},
			{{0x7261'4451'8323'5adbULL, 0x0e38'dc92'cb1f'2a60ULL, 0x7b2b'8a9a'a607'9c54ULL,
				0x800a'440b'dbb2'ceb1ULL, 0x3cd9'55b7'e00d'0984ULL, 0x3a7d'3a1b'2589'4224ULL,
				0x944c'9ad8'ec16'5fdeULL, 0x378f'5a54'1631'229bULL}},
			{{0x74b4'c7fb'9845'9cedULL, 0x3698'fad1'153b'b6c3ULL, 0x7a1e'6c30'3b76'52f4ULL,
				0x9fe7'6702'af69'334bULL, 0x1fff'e18a'1b33'6103ULL, 0x8941'e71c'ff8a'78dbULL,
				0x382a'e548'b2e4'f3f3ULL, 0xabbe'dea6'8005'6f52ULL}},
			{{0x6bca'a4cd'81f3'2d1bULL, 0xdea2'594a'c06f'd85dULL, 0xefba'cd1d'7d47'6e98ULL,
				0x8a1d'71ef'ea48'b9caULL, 0x2001'8021'1484'6679ULL, 0xd8fa'6bbb'ebab'0761ULL,
				0x3002'c6cd'635a'fe94ULL, 0x7bcd'9ed0'efc8'89fbULL}},
			{{0x48bc'924a'f11b'd720ULL, 0xfaf4'17d5'd9b2'1b99ULL, 0xe71d'a4aa'88e1'2852ULL,
				0x5d80'ef9d'1891'cc86ULL, 0xf820'12d4'3021'9f9bULL, 0xcda4'3c32'bcdf'1d77ULL,
				0xd213'80b0'0449'b17aULL, 0x378e'e767'f116'31baULL}}};
		return C[v];
	} else {
		static const uint512_u C[12] = {
			{{0x0745'a6f2'5965'80ddULL, 0x234d'74cc'3674'7605ULL, 0x15d3'60a4'082a'42a2ULL,
				0x0169'6792'91e0'7c4bULL, 0xfcc4'8575'8db8'4e71ULL, 0x16d0'452e'4376'6a2fULL,
				0x1f7c'65c0'812f'cbebULL, 0xe9da'ca1e'da5b'08b1ULL}},
			{{0xb79b'b121'7004'79e6ULL, 0x56cd'cbd7'1ba2'dd55ULL, 0xcaa7'0adb'c261'b55cULL,
				0x5899'd612'6b17'b59aULL, 0x3101'b516'0f5e'd561ULL, 0x982b'230a'72ea'fef3ULL,
				0xd7b5'700f'469d'e34fULL, 0x1a2f'9da9'8ab5'a36fULL}},
			{{0xb20a'ba0a'f596'1e99ULL, 0x31db'7a86'43f4'b6c2ULL, 0x09db'6260'373a'c9c1ULL,
				0xb19e'3590'e40f'e2d3ULL, 0x7b7b'29b1'1475'eaf2ULL, 0x8b1f'9c52'5f5e'f106ULL,
				0x3584'3d6a'28fc'390aULL, 0xc72f'ce2b'acdc'74f5ULL}},
			{{0x2ed1'e384'bcbe'0c22ULL, 0xf137'e893'a1ea'5334ULL, 0xbe03'5293'3313'b7d8ULL,
				0x75d6'03ed'822c'd7a9ULL, 0x3f35'5e68'ad1c'729dULL, 0x7d3c'5c33'7e85'8e48ULL,
				0xdde4'715d'a0e1'48f9ULL, 0xd266'15e8'b3df'1fefULL}},
			{{0x57fe'6c7c'fd58'1760ULL, 0xf563'eaa9'7ea2'567aULL, 0x161a'2723'b700'ffdfULL,
				0xa3f5'3a25'4717'cdbfULL, 0xbdff'0f80'd735'9e35ULL, 0x4a10'8616'1f1c'157fULL,
				0x6323'a96c'0c41'3f9aULL, 0x9947'47ad'ac6b'ea4bULL}},
			{{0x6e7d'6446'7a40'68faULL, 0x354f'9036'72c5'71bfULL, 0xb6c6'bec2'661f'f20aULL,
				0xb4b7'9a1c'b7a6'facfULL, 0xc68e'f09a'b49a'7f18ULL, 0x6ca4'4251'f9c4'662dULL,
				0xc039'307a'3bc3'a46fULL, 0xd9d3'3a1d'aeae'4faeULL}},
			{{0x93d4'143a'4d56'8688ULL, 0xf34a'3ca2'4c45'1735ULL, 0x0405'4a28'8369'4706ULL,
				0x372c'822d'c5ab'9209ULL, 0xc993'7a19'333e'47d3ULL, 0xc987'bfe6'c7c6'9e39ULL,
				0x5409'24bf'fe86'ac51ULL, 0xecc5'aaee'160e'c7f4ULL}},
			{{0x1ee7'02bf'd40d'7fa4ULL, 0xd9a8'5159'35c2'ac36ULL, 0x2fc4'a5d1'2b8d'd169ULL,
				0x9006'9b92'cb2b'89f4ULL, 0x9ac4'db4d'3b44'b489ULL, 0x1ede'369c'71f8'b74eULL,
				0x4141'6e0c'02aa'e703ULL, 0xa7c9'934d'425b'1f9bULL}},
			{{0xdb5a'2383'5144'6172ULL, 0x602a'1fcb'92dc'380eULL, 0x549c'07a6'9a8a'2b7bULL,
				0xb1ce'b2db'0b44'0a80ULL, 0x8409'0de0'b755'd93cULL, 0x2442'8925'1b3a'7d3aULL,
				0xde5f'16ec'd89a'4c94ULL, 0x9b22'3116'545a'8f37ULL}},
			{{0xed9c'4598'fbc7'b474ULL, 0xc3b6'3b15'd1fa'9836ULL, 0xf452'763b'306c'1e7aULL,
				0x4b33'69af'0267'e79fULL, 0x0361'331b'8ae1'ff1fULL, 0xdb78'8aff'1ce7'4189ULL,
				0xf3f3'e4b2'48e5'2a38ULL, 0x526f'0580'a6de'beabULL}},
			{{0x1b2d'f381'cda4'ca6bULL, 0x5dd8'6fc0'4a59'a2deULL, 0x986e'477d'1dcd'baefULL,
				0xcab9'48ea'ef71'1d8aULL, 0x7966'8414'2180'0120ULL, 0x6107'abeb'bb6b'fad8ULL,
				0x94fe'5a63'cdc6'0230ULL, 0xfb89'c8ef'd09e'cd7bULL}},
			{{0x20d7'1bf1'4a92'bc48ULL, 0x991b'b2d9'd517'f4faULL, 0x5228'e188'aaa4'1de7ULL,
				0x86cc'9118'9def'805dULL, 0x9b9f'2130'd412'20f8ULL, 0x771d'dfbc'323c'a4cdULL,
				0x7ab1'4904'b080'13d2ULL, 0xba31'16f1'67e7'8e37ULL}}};
		return C[v];
	}
}

static inline const unsigned long long &_Ax(int a, int b) {
	if constexpr (sprt::endian::native == sprt::endian::little) {
		alignas(16) static const unsigned long long Ax[8][256] = {
			{0xd01f'715b'5c7e'f8e6ULL, 0x16fa'2409'8077'8325ULL, 0xa8a4'2e85'7ee0'49c8ULL,
				0x6ac1'068f'a186'465bULL, 0x6e41'7bd7'a2e9'320bULL, 0x665c'8167'a437'daabULL,
				0x7666'681a'a896'17f6ULL, 0x4b95'9163'700b'dcf5ULL, 0xf14b'e6b7'8df3'6248ULL,
				0xc585'bd68'9a62'5cffULL, 0x9557'd7fc'a67d'82cbULL, 0x89f0'b969'af6d'd366ULL,
				0xb083'3d48'749f'6c35ULL, 0xa199'8c23'b1ec'bc7cULL, 0x8d70'c431'ac02'a736ULL,
				0xd6df'bc2f'd0a8'b69eULL, 0x37ae'b3e5'51fa'198bULL, 0x0b7d'128a'40b5'cf9cULL,
				0x5a8f'2008'b578'0cbcULL, 0xedec'8822'84e3'33e5ULL, 0xd25f'c177'd3c7'c2ceULL,
				0x5e0f'5d50'b617'78ecULL, 0x1d87'3683'c0c2'4cb9ULL, 0xad04'0bcb'b45d'208cULL,
				0x2f89'a028'5b85'3c76ULL, 0x5732'fff6'791b'8d58ULL, 0x3e93'1143'9ef6'ec3fULL,
				0xc918'3a80'9fd3'c00fULL, 0x83ad'f3f5'260a'01eeULL, 0xa679'1941'f4e8'ef10ULL,
				0x103a'e97d'0ca1'cd5dULL, 0x2ce9'4812'1dee'1b4aULL, 0x3973'8421'dbf2'bf53ULL,
				0x093d'a2a6'cf0c'f5b4ULL, 0xcd98'47d8'9cbc'b45fULL, 0xf956'1c07'8b2d'8ae8ULL,
				0x9c6a'755a'6971'777fULL, 0xbc1e'baa0'712e'f0c5ULL, 0x72e6'1542'abf9'63a6ULL,
				0x78bb'5fde'229e'b12eULL, 0x14ba'9425'0fce'b90dULL, 0x844d'6697'630e'5282ULL,
				0x98ea'0802'6a1e'032fULL, 0xf06b'bea1'4421'7f5cULL, 0xdb62'63d1'1ccb'377aULL,
				0x641c'314b'2b8e'e083ULL, 0x320e'96ab'9b47'70cfULL, 0x1ee7'deb9'86a9'6b85ULL,
				0xe96c'f57a'878c'47b5ULL, 0xfdd6'615f'8842'feb8ULL, 0xc838'6296'5601'dd1bULL,
				0x2ea9'f83e'9257'2162ULL, 0xf876'4411'42ff'97fcULL, 0xeb2c'4556'0835'7d9dULL,
				0x5612'a7e0'b0c9'904cULL, 0x6c01'cbfb'2d50'0823ULL, 0x4548'a6a7'fa03'7a2dULL,
				0xabc4'c6bf'388b'6ef4ULL, 0xbade'77d4'fdf8'bebdULL, 0x799b'07c8'eb4c'ac3aULL,
				0x0c9d'87e8'05b1'9cf0ULL, 0xcb58'8aac'106a'fa27ULL, 0xea0c'1d40'c1e7'6089ULL,
				0x2869'354a'1e81'6f1aULL, 0xff96'd173'07fb'c490ULL, 0x9f0a'9d60'2f1a'5043ULL,
				0x9637'3fc6'e016'a5f7ULL, 0x5292'dab8'b3a6'e41cULL, 0x9b8a'e038'2c75'2413ULL,
				0x4f15'ec3b'7364'a8a5ULL, 0x3fb3'4955'5724'f12bULL, 0xc7c5'0d44'15db'66d7ULL,
				0x92b7'429e'e379'd1a7ULL, 0xd37f'9961'1a15'dfdaULL, 0x2314'27c0'5e34'a086ULL,
				0xa439'a96d'7b51'd538ULL, 0xb403'4010'77f0'1865ULL, 0xdda2'aea5'901d'7902ULL,
				0x0a5d'4a9c'8967'd288ULL, 0xc265'280a'df66'0f93ULL, 0x8bb0'0945'20d4'e94eULL,
				0x2a29'8566'9138'5532ULL, 0x42a8'33c5'bf07'2941ULL, 0x73c6'4d54'622b'7eb2ULL,
				0x07e0'9562'4504'536cULL, 0x8a90'5153'e906'f45aULL, 0x6f61'23c1'6b3b'2f1fULL,
				0xc6e5'5552'dc09'7bc3ULL, 0x4468'feb1'33d1'6739ULL, 0xe211'e7f0'c739'8829ULL,
				0xa2f9'6419'f787'9b40ULL, 0x1907'4bdb'c3ad'38e9ULL, 0xf4eb'c3f9'474e'0b0cULL,
				0x4388'6bd3'76d5'3455ULL, 0xd802'8beb'5aa0'1046ULL, 0x51f2'3282'f5cd'c320ULL,
				0xe7b1'c2be'0d84'e16dULL, 0x081d'fab0'06de'e8a0ULL, 0x3b33'340d'544b'857bULL,
				0x7f5b'cabc'679a'e242ULL, 0x0edd'37c4'8a08'a6d8ULL, 0x81ed'43d9'a9b3'3bc6ULL,
				0xb1a3'655e'bd4d'7121ULL, 0x69a1'eeb5'e7ed'6167ULL, 0xf6ab'73d5'c8f7'3124ULL,
				0x1a67'a3e1'85c6'1fd5ULL, 0x2dc9'1004'd43c'065eULL, 0x0240'b02c'8fb9'3a28ULL,
				0x90f7'f2b2'6cc0'eb8fULL, 0x3cd3'a16f'114f'd617ULL, 0xaae4'9ea9'f159'73e0ULL,
				0x06c0'cd74'8cd6'4e78ULL, 0xda42'3bc7'd519'2a6eULL, 0xc345'701c'16b4'1287ULL,
				0x6d21'93ed'e482'1537ULL, 0xfcf6'3949'4190'e3acULL, 0x7c3b'2286'21f1'c57eULL,
				0xfb16'ac2b'0494'b0c0ULL, 0xbf7e'529a'3745'd7f9ULL, 0x6881'b6a3'2e3f'7c73ULL,
				0xca78'd2ba'd9b8'e733ULL, 0xbbfe'2fc2'342a'a3a9ULL, 0x0dbd'dffe'cc63'81e4ULL,
				0x70a6'a56e'2440'598eULL, 0xe4d1'2a84'4bef'c651ULL, 0x8c50'9c27'65d0'ba22ULL,
				0xee8c'6018'c288'14d9ULL, 0x17da'7c1f'49a5'9e31ULL, 0x609c'4c13'28e1'94d3ULL,
				0xb3e3'd572'32f4'4b09ULL, 0x91d7'aaa4'a512'f69bULL, 0x0ffd'6fd2'43da'bbccULL,
				0x50d2'6a94'3c1f'de34ULL, 0x6be1'5e99'6854'5b4fULL, 0x9477'8fea'6faf'9fdfULL,
				0x2b09'dd70'58ea'4826ULL, 0x677c'd971'6de5'c7bfULL, 0x49d5'214f'ffb2'e6ddULL,
				0x0360'e83a'466b'273cULL, 0x1fc7'86af'4f7b'7691ULL, 0xa0b9'd435'783e'a168ULL,
				0xd49f'0c03'5f11'8cb6ULL, 0x0120'5816'c9d2'1d14ULL, 0xac24'53dd'7d8f'3d98ULL,
				0x5452'17cc'3f70'aa64ULL, 0x26b4'028e'9489'c9c2ULL, 0xdec2'469f'd676'5e3eULL,
				0x0480'7d58'036f'7450ULL, 0xe5f1'7292'823d'db45ULL, 0xf30b'569b'024a'5860ULL,
				0x62dc'fc3f'a758'aefbULL, 0xe84c'ad6c'4e5e'5aa1ULL, 0xccb8'1fce'556e'a94bULL,
				0x53b2'82ae'7a74'f908ULL, 0x1b47'fbf7'4c14'02c1ULL, 0x368e'ebf3'9828'049fULL,
				0x7afb'eff2'ad27'8b06ULL, 0xbe5e'0a8c'fe97'caedULL, 0xcfd8'f7f4'1305'8e77ULL,
				0xf78b'2bc3'0125'2c30ULL, 0x4d55'5c17'fcdd'928dULL, 0x5f2f'0546'7fc5'65f8ULL,
				0x24f4'b2a2'1b30'f3eaULL, 0x860d'd6bb'ecb7'68aaULL, 0x4c75'0401'350f'8f99ULL,
				0x0000'0000'0000'0000ULL, 0xeccc'd034'4d31'2ef1ULL, 0xb523'1806'be22'0571ULL,
				0xc105'c030'990d'28afULL, 0x653c'695d'e25c'fd97ULL, 0x159a'cc33'c61c'a419ULL,
				0xb89e'c7f8'7241'8495ULL, 0xa984'7693'b732'54dcULL, 0x58cf'9024'3ac1'3694ULL,
				0x59ef'c832'f313'2b80ULL, 0x5c4f'ed7c'39ae'42c4ULL, 0x828d'abe3'efd8'1cfaULL,
				0xd13f'294d'95ac'e5f2ULL, 0x7d1b'7a90'e823'd86aULL, 0xb643'f03c'f849'224dULL,
				0x3df3'f979'd89d'cb03ULL, 0x7426'd836'272f'2ddeULL, 0xdfe2'1e89'1fa4'432aULL,
				0x3a13'6c1b'9d99'986fULL, 0xfa36'f43d'cd46'add4ULL, 0xc025'9826'50df'35bbULL,
				0x856d'3e81'aadc'4f96ULL, 0xc4a5'e57e'53b0'41ebULL, 0x4708'168b'75ba'4005ULL,
				0xaf44'bbe7'3be4'1aa4ULL, 0x9717'67d0'29c4'b8e3ULL, 0xb9be'9fee'bb93'9981ULL,
				0x2154'97ec'd18d'9aaeULL, 0x316e'7e91'dd2c'57f3ULL, 0xcef8'afe2'dad7'9363ULL,
				0x3853'dc37'1220'a247ULL, 0x35ee'03c9'de43'23a3ULL, 0xe691'9aa8'c456'fc79ULL,
				0xe051'57dc'4880'b201ULL, 0x7bdb'b7e4'64f5'9612ULL, 0x127a'5951'8318'f775ULL,
				0x332e'cebd'5295'6ddbULL, 0x8f30'741d'23bb'9d1eULL, 0xd922'd3fd'9372'0d52ULL,
				0x7746'300c'6144'0ae2ULL, 0x25d4'eab4'd2e2'eefeULL, 0x7506'8020'eefd'30caULL,
				0x135a'0147'4aca'ea61ULL, 0x304e'2687'14fe'4ae7ULL, 0xa519'f17b'b283'c82cULL,
				0xdc82'f6b3'59cf'6416ULL, 0x5baf'781e'7caa'11a8ULL, 0xb2c3'8d64'fb26'561dULL,
				0x34ce'5bdf'1791'3eb7ULL, 0x5d6f'b56a'f07c'5fd0ULL, 0x1827'13cd'0a7f'25fdULL,
				0x9e2a'c576'e6c8'4d57ULL, 0x9aaa'b82e'e5a7'3907ULL, 0xa3d9'3c0f'3e55'8654ULL,
				0x7e7b'92aa'ae48'ff56ULL, 0x872d'8ead'2565'75beULL, 0x41c8'dbff'f96c'0e7dULL,
				0x99ca'5014'a3cc'1e3bULL, 0x40e8'83e9'30be'1369ULL, 0x1ca7'6e95'0910'51adULL,
				0x4e35'b42d'bab6'b5b1ULL, 0x05a0'254e'cabd'6944ULL, 0xe171'0fca'8152'af15ULL,
				0xf22b'0e8d'cb98'4574ULL, 0xb763'a82a'319b'3f59ULL, 0x63fc'a429'6e8a'b3efULL,
				0x9d4a'2d4c'a0a3'6a6bULL, 0xe331'bfe6'0eeb'953dULL, 0xd5bf'5415'96c3'91a2ULL,
				0xf5cb'9bef'8e9c'1618ULL, 0x4628'4e9d'bc68'5d11ULL, 0x2074'cffa'185f'87baULL,
				0xbd3e'e2b6'b8fc'edd1ULL, 0xae64'e3f1'f236'07b0ULL, 0xfeb6'8965'ce29'd984ULL,
				0x5572'4fda'f6a2'b770ULL, 0x2949'6d5c'd753'720eULL, 0xa759'4157'3d3a'f204ULL,
				0x8e10'2c0b'ea69'800aULL, 0x111a'b16b'c573'd049ULL, 0xd7ff'e439'197a'ab8aULL,
				0xefac'380e'0b5a'09cdULL, 0x48f5'7959'3660'fbc9ULL, 0x2234'7fd6'97e6'bd92ULL,
				0x61bc'1405'e133'89c7ULL, 0x4ab5'c975'b9d9'c1e1ULL, 0x80cd'1bcf'6061'26d2ULL,
				0x7186'fd78'ed92'449aULL, 0x9397'1a88'2aab'ccb3ULL, 0x88d0'e17f'66bf'ce72ULL,
				0x2794'5a98'5d5b'd4d6ULL},
			{0xde55'3f8c'05a8'11c8ULL, 0x1906'b596'31b4'f565ULL, 0x436e'70d6'b196'4ff7ULL,
				0x36d3'43cb'8b1e'9d85ULL, 0x843d'facc'858a'ab5aULL, 0xfdfc'95c2'99bf'c7f9ULL,
				0x0f63'4bde'a1d5'1fa2ULL, 0x6d45'8b3b'76ef'b3cdULL, 0x85c3'f77c'f859'3f80ULL,
				0x3c91'315f'be73'7cb2ULL, 0x2148'b033'66ac'e398ULL, 0x18f8'b826'4c67'61bfULL,
				0xc830'c1c4'95c9'fb0fULL, 0x981a'7610'2086'a0aaULL, 0xaa16'0121'42f3'5760ULL,
				0x35cc'5406'0c76'3cf6ULL, 0x4290'7d66'cc45'db2dULL, 0x8203'd44b'965a'f4bcULL,
				0x3d6f'3cef'c3a0'e868ULL, 0xbc73'ff69'd292'bda7ULL, 0x8722'ed01'02e2'0a29ULL,
				0x8f81'85e8'cd34'deb7ULL, 0x9b05'61dd'a7ee'01d9ULL, 0x5335'a019'3227'fad6ULL,
				0xc9ce'cc74'e81a'6fd5ULL, 0x54f5'832e'5c24'31eaULL, 0x99e4'7ba0'5d55'3470ULL,
				0xf7be'e756'acd2'26ceULL, 0x384e'05a5'5718'16fdULL, 0xd136'7452'a47d'0e6aULL,
				0xf29f'de1c'386a'd85bULL, 0x320c'7731'6275'f7caULL, 0xd0c8'79e2'd9ae'9ab0ULL,
				0xdb74'06c6'9110'ef5dULL, 0x4550'5e51'a246'1011ULL, 0xfc02'9872'e46c'5323ULL,
				0xfa3c'b6f5'f7bc'0cc5ULL, 0x031f'17cd'8768'a173ULL, 0xbd8d'f2d9'af41'297dULL,
				0x9d3b'4f5a'b43e'5e3fULL, 0x4071'671b'36fe'ee84ULL, 0x7162'07e7'd3e3'b83dULL,
				0x48d2'0ff2'f928'3a1aULL, 0x2776'9eb4'757c'bc7eULL, 0x5c56'ebc7'93f2'e574ULL,
				0xa48b'474f'9ef5'dc18ULL, 0x52cb'ada9'4ff4'6e0cULL, 0x60c7'da98'2d81'99c6ULL,
				0x0e9d'466e'dc06'8b78ULL, 0x4eec'2175'eaf8'65fcULL, 0x550b'8e9e'21f7'a530ULL,
				0x6b7b'a5bc'653f'ec2bULL, 0x5eb7'f1ba'6949'd0ddULL, 0x57ea'94e3'db4c'9099ULL,
				0xf640'eae6'd101'b214ULL, 0xdd4a'2841'82c0'b0bbULL, 0xff1d'8fbf'6304'f250ULL,
				0xb8ac'cb93'3bf9'd7e8ULL, 0xe886'7c47'8eb6'8c4dULL, 0x3f8e'2692'391b'ddc1ULL,
				0xcb2f'd609'12a1'5a7cULL, 0xaec9'35db'ab98'3d2fULL, 0xf55f'fd2b'5669'1367ULL,
				0x80e2'ce36'6ce1'c115ULL, 0x179b'f3f8'edb2'7e1dULL, 0x01fe'0db0'7dd3'94daULL,
				0xda8a'0b76'ecc3'7b87ULL, 0x44ae'53e1'df95'84cbULL, 0xb310'b4b7'7347'a205ULL,
				0xdfab'323c'787b'8512ULL, 0x3b51'1268'd070'b78eULL, 0x65e6'e3d2'b939'6753ULL,
				0x6864'b271'e257'4d58ULL, 0x2597'84c9'8fc7'89d7ULL, 0x02e1'1a7d'fabb'35a9ULL,
				0x8841'a6df'a337'158bULL, 0x7ade'78c3'9b5d'cdd0ULL, 0xb7cf'804d'9a2c'c84aULL,
				0x20b6'bd83'1b7f'7742ULL, 0x75bd'331d'3a88'd272ULL, 0x418f'6aab'4b2d'7a5eULL,
				0xd995'1cbb'6bab'daf4ULL, 0xb631'8dfd'e7ff'5c90ULL, 0x1f38'9b11'2264'aa83ULL,
				0x492c'0242'84fb'aec0ULL, 0xe33a'0363'c608'f9a0ULL, 0x2688'9304'08af'28a4ULL,
				0xc753'8a1a'341c'e4adULL, 0x5da8'e677'ee21'71aeULL, 0x8c9e'9225'4a5c'7fc4ULL,
				0x63d8'cd55'aae9'38b5ULL, 0x29eb'd8da'a97a'3706ULL, 0x9598'27b3'7be8'8aa1ULL,
				0x1484'e435'6ada'df6eULL, 0xa794'5082'199d'7d6bULL, 0xbf6c'e8a4'55fa'1cd4ULL,
				0x9cc5'42ea'c9ed'cae5ULL, 0x79c1'6f0e'1c35'6ca3ULL, 0x89bf'ab6f'dee4'8151ULL,
				0xd417'4d18'30c5'f0ffULL, 0x9258'0484'15eb'419dULL, 0x6139'd728'5052'0d1cULL,
				0x6a85'a80c'18ec'78f1ULL, 0xcd11'f88e'0171'059aULL, 0xccef'f53e'7ca2'9140ULL,
				0xd229'639f'2315'af19ULL, 0x90b9'1ef9'ef50'7434ULL, 0x5977'd28d'074a'1be1ULL,
				0x3113'60fc'e51d'56b9ULL, 0xc093'a92d'5a1f'2f91ULL, 0x1a19'a25b'b6dc'5416ULL,
				0xeb99'6b8a'09de'2d3eULL, 0xfee3'820f'1ed7'668aULL, 0xd708'5ad5'b7ad'518cULL,
				0x7fff'4189'0fe5'3345ULL, 0xec59'48bd'67dd'e602ULL, 0x2fd5'f65d'baaa'68e0ULL,
				0xa575'4aff'e326'48c2ULL, 0xf8dd'ac88'0d07'396cULL, 0x6fa4'9146'8c54'8664ULL,
				0x0c7c'5c13'26bd'bed1ULL, 0x4a33'158f'0393'0fb3ULL, 0x699a'bfc1'9f84'd982ULL,
				0xe4fa'2054'a80b'329cULL, 0x6707'f9af'4382'52faULL, 0x08a3'68e9'cfd6'd49eULL,
				0x47b1'442c'58fd'25b8ULL, 0xbbb3'dc5e'bc91'769bULL, 0x1665'fe48'9061'eac7ULL,
				0x33f2'7a81'1fa6'6310ULL, 0x93a6'0934'6838'd547ULL, 0x30ed'6d4c'98ce'c263ULL,
				0x1dd9'816c'd8df'9f2aULL, 0x9466'2a03'063b'1e7bULL, 0x83fd'd9fb'eb89'6066ULL,
				0x7b20'7573'e68e'590aULL, 0x5f49'fc0a'149a'4407ULL, 0x3432'59b6'71a5'a82cULL,
				0xfbc2'bb45'8a6f'981fULL, 0xc272'b350'a0a4'1a38ULL, 0x3aaf'1fd8'ada3'2354ULL,
				0x6cbb'868b'0b3c'2717ULL, 0xa2b5'69c8'8d25'83feULL, 0xf180'c9d1'bf02'7928ULL,
				0xaf37'386b'd64b'a9f5ULL, 0x12ba'cab2'790a'8088ULL, 0x4c0d'3b08'1043'5055ULL,
				0xb2ee'b907'0e94'36dfULL, 0xc5b2'9067'cea7'd104ULL, 0xdcb4'25f1'ff13'2461ULL,
				0x4f12'2cc5'972b'f126ULL, 0xac28'2fa6'5123'0886ULL, 0xe7e5'3799'2f63'93efULL,
				0xe61b'3a29'52b0'0735ULL, 0x709c'0a57'ae30'2ce7ULL, 0xe025'14ae'4160'58d3ULL,
				0xc44c'9dd7'b374'45deULL, 0x5a68'c540'8022'ba92ULL, 0x1c27'8cdc'a50c'0bf0ULL,
				0x6e5a'9cf6'f187'12beULL, 0x86dc'e0b1'7f31'9ef3ULL, 0x2d34'ec20'4011'5d49ULL,
				0x4bcd'183f'7e40'9b69ULL, 0x2815'd56a'd4a9'a3dcULL, 0x2469'8979'f214'1d0dULL,
				0x0000'0000'0000'0000ULL, 0x1ec6'96a1'5fb7'3e59ULL, 0xd86b'110b'1678'4e2eULL,
				0x8e7f'8858'b0e7'4a6dULL, 0x063e'2e87'13d0'5fe6ULL, 0xe2c4'0ed3'bbdb'6d7aULL,
				0xb1f1'aeca'89fc'97acULL, 0xe1db'191e'3cb3'cc09ULL, 0x6418'ee62'c4ea'f389ULL,
				0xc6ad'87aa'49cf'7077ULL, 0xd6f6'5765'ca7e'c556ULL, 0x9afb'6c6d'da3d'9503ULL,
				0x7ce0'5644'888d'9236ULL, 0x8d60'9f95'378f'eb1eULL, 0x23a9'aa4e'9c17'd631ULL,
				0x6226'c0e5'd73a'ac6fULL, 0x5614'9953'a69f'0443ULL, 0xeeb8'52c0'9d66'd3abULL,
				0x2b0a'c2a7'53c1'02afULL, 0x07c0'2337'6e03'cb3cULL, 0x2cca'e190'3dc2'c993ULL,
				0xd3d7'6e2f'5ec6'3bc3ULL, 0x9e24'5897'3356'ff4cULL, 0xa66a'5d32'644e'e9b1ULL,
				0x0a42'7294'356d'e137ULL, 0x783f'62be'61e6'f879ULL, 0x1344'c702'04d9'1452ULL,
				0x5b96'c8f0'fdf1'2e48ULL, 0xa909'16ec'c59b'f613ULL, 0xbe92'e514'2829'880eULL,
				0x727d'102a'548b'194eULL, 0x1be7'afeb'cb0f'c0ccULL, 0x3e70'2b22'44c8'491bULL,
				0xd5e9'40a8'4d16'6425ULL, 0x66f9'f41f'3e51'c620ULL, 0xabe8'0c91'3f20'c3baULL,
				0xf07e'c461'c2d1'edf2ULL, 0xf361'd3ac'45b9'4c81ULL, 0x0521'394a'94b8'fe95ULL,
				0xadd6'2216'2cf0'9c5cULL, 0xe978'71f7'f365'1897ULL, 0xf4a1'f09b'2bba'87bdULL,
				0x095d'6559'b205'4044ULL, 0x0bbc'7f24'48be'75edULL, 0x2af4'cf17'2e12'9675ULL,
				0x157a'e985'1709'4bb4ULL, 0x9fda'5527'4e85'6b96ULL, 0x9147'1349'9283'e0eeULL,
				0xb952'c623'462a'4332ULL, 0x7443'3ead'475b'46a8ULL, 0x8b5e'b112'245f'b4f8ULL,
				0xa34b'6478'f0f6'1724ULL, 0x11a5'dd7f'fe62'21fbULL, 0xc16d'a49d'27cc'bb4bULL,
				0x76a2'24d0'bde0'7301ULL, 0x8aa0'bca2'598c'2022ULL, 0x4df3'36b8'6d90'c48fULL,
				0xea67'663a'740d'b9e4ULL, 0xef46'5f70'e0b5'4771ULL, 0x39b0'0815'2acb'8227ULL,
				0x7d1e'5bf4'f55e'06ecULL, 0x105b'd0cf'83b1'b521ULL, 0x775c'2960'c033'e7dbULL,
				0x7e01'4c39'7236'a79fULL, 0x811c'c386'1132'55cfULL, 0xeda7'450d'1a0e'72d8ULL,
				0x5889'df3d'7a99'8f3bULL, 0x2e2b'fbed'c779'fc3aULL, 0xce0e'ef43'8619'a4e9ULL,
				0x372d'4e7b'f6cd'095fULL, 0x04df'34fa'e96b'6a4fULL, 0xf923'a138'70d4'adb6ULL,
				0xa1aa'7e05'0a4d'228dULL, 0xa8f7'1b5c'b848'62c9ULL, 0xb52e'9a30'6097'fde3ULL,
				0x0d82'51a3'5b6e'2a0bULL, 0x2257'a7fe'e1c4'42ebULL, 0x7383'1d9a'2958'8d94ULL,
				0x51d4'ba64'c89c'cf7fULL, 0x502a'b7d4'b54f'5ba5ULL, 0x9779'3dce'8153'bf08ULL,
				0xe504'2de4'd5d8'a646ULL, 0x9687'307e'fc80'2bd2ULL, 0xa054'73b5'779e'b657ULL,
				0xb4d0'9780'1d44'6939ULL, 0xcff0'e2f3'fbca'3033ULL, 0xc38c'bee0'dd77'8ee2ULL,
				0x464f'499c'252e'b162ULL, 0xcad1'dbb9'6f72'cea6ULL, 0xba4d'd1ee'c142'e241ULL,
				0xb00f'a37a'f42f'0376ULL},
			{0xcce4'cd3a'a968'b245ULL, 0x089d'5484'e80b'7fafULL, 0x6382'46c1'b354'8304ULL,
				0xd2fe'0ec8'c235'5492ULL, 0xa7fb'df7f'f237'4eeeULL, 0x4df1'600c'9233'7a16ULL,
				0x84e5'03ea'523b'12fbULL, 0x0790'bbfd'53ab'0c4aULL, 0x198a'780f'38f6'ea9dULL,
				0x2ab3'0c8f'55ec'48cbULL, 0xe0f7'fed6'b2c4'9db5ULL, 0xb6ec'f3f4'22ca'dbdcULL,
				0x409c'9a54'1358'df11ULL, 0xd3ce'8a56'dfde'3fe3ULL, 0xc3e9'2243'12c8'c1a0ULL,
				0x0d6d'fa58'816b'a507ULL, 0xddf3'e1b1'7995'2777ULL, 0x04c0'2a42'748b'b1d9ULL,
				0x94c2'abff'9f2d'ecb8ULL, 0x4f91'752d'a8f8'acf4ULL, 0x7868'2bef'b169'bf7bULL,
				0xe1c7'7a48'af2f'f6c4ULL, 0x0c5d'7ec6'9c80'ce76ULL, 0x4cc1'e492'8fd8'1167ULL,
				0xfeed'3d24'd999'7b62ULL, 0x518b'b6df'c3a5'4a23ULL, 0x6dbf'2d26'151f'9b90ULL,
				0xb5bc'624b'05ea'664fULL, 0xe86a'aa52'5acf'e21aULL, 0x4801'ced0'fb53'a0beULL,
				0xc914'63e6'c008'68edULL, 0x1027'a815'cd16'fe43ULL, 0xf670'69a0'3192'04cdULL,
				0xb04c'cc97'6c8a'bce7ULL, 0xc0b9'b3fc'35e8'7c33ULL, 0xf380'c77c'58f2'de65ULL,
				0x50bb'3241'de4e'2152ULL, 0xdf93'f490'435e'f195ULL, 0xf1e0'd25d'6239'0887ULL,
				0xaf66'8bfb'1a3c'3141ULL, 0xbc11'b251'f00a'7291ULL, 0x73a5'eed4'7e42'7d47ULL,
				0x25be'e3f6'ee4c'3b2eULL, 0x43cc'0beb'3478'6282ULL, 0xc824'e778'dde3'039cULL,
				0xf97d'86d9'8a32'7728ULL, 0xf2b0'43e2'4519'b514ULL, 0xe297'ebf7'880f'4b57ULL,
				0x3a94'a49a'98fa'b688ULL, 0x8685'16cb'68f0'c419ULL, 0xeffa'11af'0964'ee50ULL,
				0xa4ab'4ec0'd517'f37dULL, 0xa9c6'b498'547c'567aULL, 0x8e18'424f'80fb'bbb6ULL,
				0x0bcd'c53b'cf2b'c23cULL, 0x1377'39aa'ea36'43d0ULL, 0x2c13'33ec'1bac'2ff0ULL,
				0x8d48'd3f0'a7db'0625ULL, 0x1e1a'c3f2'6b5d'e6d7ULL, 0xf520'f81f'16b2'b95eULL,
				0x9f0f'6ec4'5006'2e84ULL, 0x0130'849e'1deb'6b71ULL, 0xd45e'31ab'8c75'33a9ULL,
				0x6522'79a2'fd14'e43fULL, 0x3209'f01e'70f1'c927ULL, 0xbe71'a770'cac1'a473ULL,
				0x0e3d'6be7'a64b'1894ULL, 0x7ec8'148c'ff29'd840ULL, 0xcb74'76c7'fac3'be0fULL,
				0x7295'6a4a'63a9'1636ULL, 0x37f9'5ec2'1991'138fULL, 0x9e3f'ea5a'4ded'45f5ULL,
				0x7b38'ba50'9649'02e8ULL, 0x222e'580b'bde7'3764ULL, 0x61e2'53e0'899f'55e6ULL,
				0xfc8d'2805'e352'ad80ULL, 0x3599'4be3'235a'c56dULL, 0x09ad'd01a'f5e0'14deULL,
				0x5e86'59a6'7805'39c6ULL, 0xb17c'4809'7161'd796ULL, 0x0260'1521'3acb'd6e2ULL,
				0xd1ae'9f77'e515'e901ULL, 0xb7dc'776a'3f21'b0adULL, 0xaba6'a1b9'6eb7'8098ULL,
				0x9bcf'4486'248d'9f5dULL, 0x5826'66c5'3645'5efdULL, 0xfdbd'ac9b'feb9'c6f1ULL,
				0xc479'99be'4163'cdeaULL, 0x7655'4008'1722'a7efULL, 0x3e54'8ed8'ec71'0751ULL,
				0x3d04'1f67'cb51'bac2ULL, 0x7958'af71'ac82'd40aULL, 0x36c9'da5c'047a'78feULL,
				0xed9a'048e'33af'38b2ULL, 0x26ee'7249'c96c'86bdULL, 0x9002'81bd'eba6'5d61ULL,
				0x1117'2c8b'd0fd'9532ULL, 0xea0a'bf73'6004'34f8ULL, 0x42fc'8f75'2993'09f3ULL,
				0x34a9'cf7d'3eb1'ae1cULL, 0x2b83'8811'4807'23baULL, 0x5ce6'4c87'42ce'ef24ULL,
				0x1ada'e9b0'1fd6'570eULL, 0x3c34'9bf9'd6ba'd1b3ULL, 0x8245'3c89'1c7b'75c0ULL,
				0x9792'3a40'b80d'512bULL, 0x4a61'dbf1'c198'765cULL, 0xb48c'e6d5'1801'0d3eULL,
				0xcfb4'5c85'8e48'0fd6ULL, 0xd933'cbf3'0d1e'96aeULL, 0xd70e'a014'ab55'8e3aULL,
				0xc189'3762'2803'1742ULL, 0x9262'949c'd16d'8b83ULL, 0xeb3a'3bed'7def'5f89ULL,
				0x4931'4a4e'e6b8'cbcfULL, 0xdcc3'652f'647e'4c06ULL, 0xda63'5a4c'2a3e'2b3dULL,
				0x470c'21a9'40f3'd35bULL, 0x3159'61a1'57d1'74b4ULL, 0x6672'e81d'da34'59acULL,
				0x5b76'f77a'1165'e36eULL, 0x445c'b016'67d3'6ec8ULL, 0xc549'1d20'5c88'a69bULL,
				0x456c'3488'7a38'05b9ULL, 0xffdd'b9ba'c472'1013ULL, 0x99af'51a7'1e46'49bfULL,
				0xa15b'e01c'bc77'29d5ULL, 0x52db'2760'e485'f7b0ULL, 0x8c78'576e'ba30'6d54ULL,
				0xae56'0f65'07d7'5a30ULL, 0x95f2'2f61'82c6'87c9ULL, 0x71c5'fbf5'4489'aba5ULL,
				0xca44'f259'e728'd57eULL, 0x88b8'7d2c'cebb'dc8dULL, 0xbab1'8d32'be4a'15aaULL,
				0x8be8'ec93'e99b'611eULL, 0x17b7'13e8'9ebd'f209ULL, 0xb31c'5d28'4baa'0174ULL,
				0xeeca'9531'148f'8521ULL, 0xb8d1'9813'8481'c348ULL, 0x8988'f9b2'd350'b7fcULL,
				0xb9e1'1c8d'996a'a839ULL, 0x5a46'73e4'0c8e'881fULL, 0x1687'9776'8356'9978ULL,
				0xbf41'23ee'd72a'cf02ULL, 0x4ea1'f1b3'b513'c785ULL, 0xe767'452b'e16f'91ffULL,
				0x7505'd1b7'3002'1a7cULL, 0xa59b'ca5e'c8fc'980cULL, 0xad06'9eda'20f7'e7a3ULL,
				0x38f4'b1bb'a231'606aULL, 0x60d2'd77e'9474'3e97ULL, 0x9aff'c018'3966'f42cULL,
				0x248e'6768'f3a7'505fULL, 0xcdd4'49a4'b483'd934ULL, 0x87b5'9255'751b'af68ULL,
				0x1bea'6d2e'023d'3c7fULL, 0x6b1f'1245'5b5f'fcabULL, 0x7435'5529'2de9'710dULL,
				0xd803'4f6d'10f5'fddfULL, 0xc619'8c9f'7ba8'1b08ULL, 0xbb81'09ac'a3a1'7edbULL,
				0xfa2d'1766'ad12'cabbULL, 0xc729'0801'6643'7079ULL, 0x9c5f'ff7b'7726'9317ULL,
				0x0000'0000'0000'0000ULL, 0x15d7'06c9'a476'24ebULL, 0x6fdf'3807'2fd4'4d72ULL,
				0x5fb6'dd38'65ee'52b7ULL, 0xa33b'f53d'86bc'ff37ULL, 0xe657'c1b5'fc84'fa8eULL,
				0xaa96'2527'735c'ebe9ULL, 0x39c4'3525'bfda'0b1bULL, 0x204e'4d2a'872c'e186ULL,
				0x7a08'3ece'8ba2'6999ULL, 0x554b'9c9d'b72e'fbfaULL, 0xb22c'd9b6'5641'6a05ULL,
				0x96a2'bede'a5e6'3a5aULL, 0x8025'29a8'26b0'a322ULL, 0x8115'ad36'3b5b'c853ULL,
				0x8375'b817'0190'1eb1ULL, 0x3069'e53f'4a3a'1fc5ULL, 0xbd21'36cf'ede1'19e0ULL,
				0x18ba'fc91'251d'81ecULL, 0x1d4a'524d'4c7d'5b44ULL, 0x05f0'aedc'6960'daa8ULL,
				0x29e3'9d30'72cc'f558ULL, 0x70f5'7f6b'5962'c0d4ULL, 0x989f'd539'03ad'22ceULL,
				0xf84d'0247'97d9'1c59ULL, 0x547b'1803'aac5'908bULL, 0xf0d0'56c3'7fd2'63f6ULL,
				0xd56e'b535'919e'58d8ULL, 0x1c7a'd6d3'5196'3035ULL, 0x2e73'26cd'2167'f912ULL,
				0xac36'1a44'3d1c'8cd2ULL, 0x697f'0764'6194'2a49ULL, 0x4b51'5f6f'dc73'1d2dULL,
				0x8ad8'680d'f470'0a6fULL, 0x41ac'1eca'0eb3'b460ULL, 0x7d98'8533'd809'65d3ULL,
				0xa8f6'3006'4997'3d0bULL, 0x7765'c496'0ac9'cc9eULL, 0x7ca8'01ad'c5e2'0ea2ULL,
				0xdea3'700e'5eb5'9ae4ULL, 0xa06b'6482'a19c'42a4ULL, 0x6a2f'96db'46b4'97daULL,
				0x27de'f6d7'd487'edccULL, 0x463c'a537'5d18'b82aULL, 0xa6cb'5be1'efdc'259fULL,
				0x53eb'a3fe'f96e'9cc1ULL, 0xce84'd81b'93a3'64a7ULL, 0xf410'7c81'0b59'd22fULL,
				0x3339'7480'6d1a'a256ULL, 0x0f0d'ef79'bba0'73e5ULL, 0x231e'dc95'a00c'5c15ULL,
				0xe437'd494'c64f'2c6cULL, 0x9132'0523'f64d'3610ULL, 0x6742'6c83'c7df'32ddULL,
				0x6eef'bc99'323f'2603ULL, 0x9d6f'7be5'6acd'f866ULL, 0x5916'e25b'2bae'358cULL,
				0x7ff8'9012'e2c2'b331ULL, 0x0350'91bf'2720'bd93ULL, 0x561b'0d22'900e'4669ULL,
				0x28d3'19ae'6f27'9e29ULL, 0x2f43'a253'3c8c'9263ULL, 0xd09e'1be9'f8fe'8270ULL,
				0xf740'ed3e'2c79'6fbcULL, 0xdb53'ded2'37d5'404cULL, 0x62b2'c25f'aebf'e875ULL,
				0x0afd'41a5'd2c0'a94dULL, 0x6412'fd3c'e0ff'8f4eULL, 0xe3a7'6f69'95e4'2026ULL,
				0x6c8f'a9b8'08f4'f0e1ULL, 0xc2d9'a6dd'0f23'aad1ULL, 0x8f28'c6d1'9d10'd0c7ULL,
				0x85d5'8774'4fd0'798aULL, 0xa20b'71a3'9b57'9446ULL, 0x684f'83fa'7c7f'4138ULL,
				0xe507'500a'dba4'471dULL, 0x3f64'0a46'f19a'6c20ULL, 0x1247'bd34'f7dd'28a1ULL,
				0x2d23'b772'0647'4481ULL, 0x9352'1002'cc86'e0f2ULL, 0x572b'89bc'8de5'2d18ULL,
				0xfb1d'93f8'b0f9'a1caULL, 0xe95a'2ecc'4724'896bULL, 0x3ba4'2004'8511'ddf9ULL,
				0xd63e'248a'b6be'e54bULL, 0x5dd6'c819'5f25'8455ULL, 0x06a0'3f63'4e40'673bULL,
				0x1f2a'476c'76b6'8da6ULL, 0x217e'c9b4'9ac7'8af7ULL, 0xecaa'8010'2e44'53c3ULL,
				0x14e7'8257'b99d'4f9aULL},
			{0x2032'9b2c'c87b'ba05ULL, 0x4f5e'b6f8'6546'a531ULL, 0xd4f4'4775'f751'b6b1ULL,
				0x8266'a47b'850d'fa8bULL, 0xbb98'6aa1'5a6c'a985ULL, 0xc979'eb08'f9ae'0f99ULL,
				0x2da6'f447'a237'5ea1ULL, 0x1e74'275d'cd7d'8576ULL, 0xbc20'180a'800b'c5f8ULL,
				0xb4a2'f701'b2dc'65beULL, 0xe726'946f'981b'6d66ULL, 0x48e6'c453'bf21'c94cULL,
				0x42ca'd993'0f0a'4195ULL, 0xefa4'7b64'aacc'cd20ULL, 0x7118'0a89'6040'9a42ULL,
				0x8bb3'329b'f6a4'4e0cULL, 0xd34c'35de'2d36'daccULL, 0xa92f'5b7c'bc23'dc96ULL,
				0xb31a'85aa'68bb'09c3ULL, 0x13e0'4836'a731'61d2ULL, 0xb24d'fc41'29c5'1d02ULL,
				0x8ae4'4b70'b7da'5acdULL, 0xe671'ed84'd965'79a7ULL, 0xa4bb'3417'd66f'3832ULL,
				0x4572'ab38'd56d'2de8ULL, 0xb1b4'7761'ea47'215cULL, 0xe81c'09cf'70ab'a15dULL,
				0xffbd'b872'ce7f'90acULL, 0xa878'2297'fd5d'c857ULL, 0x0d94'6f6b'6a4c'e4a4ULL,
				0xe4df'1f4f'5b99'5138ULL, 0x9ebc'71ed'ca8c'5762ULL, 0x0a2c'1dc0'b02b'88d9ULL,
				0x3b50'3c11'5d9d'7b91ULL, 0xc643'76a8'111e'c3a2ULL, 0xcec1'99a3'23c9'63e4ULL,
				0xdc76'a87e'c586'16f7ULL, 0x09d5'96e0'73a9'b487ULL, 0x1458'3a9d'7d56'0dafULL,
				0xf4c6'dc59'3f2a'0cb4ULL, 0xdd21'd195'84f8'0236ULL, 0x4a48'3698'3ddd'e1d3ULL,
				0xe588'66a4'1ae7'45f9ULL, 0xf591'a5b2'7e54'1875ULL, 0x891d'c050'7458'6693ULL,
				0x5b06'8c65'1810'a89eULL, 0xa303'46bc'0c08'544fULL, 0x3dbf'3751'c684'032dULL,
				0x2a1e'86ec'7850'32dcULL, 0xf73f'5779'fca8'30eaULL, 0xb60c'05ca'3020'4d21ULL,
				0x0cc3'1680'2b32'f065ULL, 0x8770'241b'dd96'be69ULL, 0xb861'e181'99ee'95dbULL,
				0xf805'cad9'1418'fcd1ULL, 0x29e7'0dcc'bbd2'0e82ULL, 0xc714'0f43'5060'd763ULL,
				0x0f3a'9da0'e8b0'cc3bULL, 0xa254'3f57'4d76'408eULL, 0xbd77'61e1'c175'd139ULL,
				0x4b1f'4f73'7ca3'f512ULL, 0x6dc2'df1f'2fc1'37abULL, 0xf1d0'5c39'67b1'4856ULL,
				0xa742'bf37'15ed'046cULL, 0x6540'3014'1d16'97edULL, 0x07b8'72ab'da67'6c7dULL,
				0x3ce8'4eba'87fa'17ecULL, 0xc1fb'0403'cb79'afdfULL, 0x3e46'bc71'0506'3f73ULL,
				0x278a'e987'121c'd678ULL, 0xa1ad'b477'8ef4'7cd0ULL, 0x26dd'906c'5362'c2b9ULL,
				0x0516'8060'589b'44e2ULL, 0xfbfc'41f9'd79a'c08fULL, 0x0e6d'e44b'a9ce'd8faULL,
				0x9feb'0806'8bf2'43a3ULL, 0x7b34'1749'd06b'129bULL, 0x229c'69e7'4a87'929aULL,
				0xe09e'e6c4'427c'011bULL, 0x5692'e30e'725c'4c3aULL, 0xda99'a33e'5e9f'6e4bULL,
				0x353d'd85a'f453'a36bULL, 0x2524'1b4c'90e0'fee7ULL, 0x5de9'8725'8309'd022ULL,
				0xe230'140f'c080'2984ULL, 0x9328'1e86'a0c0'b3c6ULL, 0xf229'd719'a433'7408ULL,
				0x6f6c'2dd4'ad3d'1f34ULL, 0x8ea5'b2fb'ae3f'0aeeULL, 0x8331'dd90'c473'ee4aULL,
				0x346a'a1b1'b52d'b7aaULL, 0xdf8f'235e'0604'2aa9ULL, 0xcc6f'6b68'a135'4b7bULL,
				0x6c95'a6f4'6ebf'236aULL, 0x52d3'1a85'6bb9'1c19ULL, 0x1a35'ded6'd498'd555ULL,
				0xf37e'aef2'e54d'60c9ULL, 0x72e1'81a9'a3c2'a61cULL, 0x9853'7aad'5195'2fdeULL,
				0x16f6'c856'ffaa'2530ULL, 0xd960'281e'9d1d'5215ULL, 0x3a07'45fa'1ce3'6f50ULL,
				0x0b7b'642b'f155'9c18ULL, 0x59a8'7eae'9aec'8001ULL, 0x5e10'0c05'408b'ec7cULL,
				0x0441'f98b'19e5'5023ULL, 0xd70d'cc55'34d3'8aefULL, 0x927f'676d'e1be'a707ULL,
				0x9769'e70d'b925'e3e5ULL, 0x7a63'6ea2'9115'065aULL, 0x468b'2018'16ef'11b6ULL,
				0xab81'a9b7'3edf'f409ULL, 0xc0ac'7de8'8a07'bb1eULL, 0x1f23'5eb6'8c03'91b7ULL,
				0x6056'b074'458d'd30fULL, 0xbe8e'eac1'02f7'ed67ULL, 0xcd38'1283'e04b'5fbaULL,
				0x5cbe'fece'c277'c4e3ULL, 0xd21b'4c35'6c48'ce0dULL, 0x1019'c316'64b3'5d8cULL,
				0x2473'62a7'd19e'ea26ULL, 0xebe5'82ef'b329'9d03ULL, 0x02ae'f2cb'82fc'289fULL,
				0x8627'5df0'9ce8'aaa8ULL, 0x28b0'7427'faac'1a43ULL, 0x38a9'b731'9e1f'47cfULL,
				0xc82e'92e3'b8d0'1b58ULL, 0x06ef'0b40'9b19'78bcULL, 0x62f8'42bf'c771'fb90ULL,
				0x9904'0346'10eb'3b1fULL, 0xded8'5ab5'477a'3e68ULL, 0x90d1'95a6'6342'8f98ULL,
				0x5384'636e'2ac7'08d8ULL, 0xcbd7'19c3'7b52'2706ULL, 0xae97'29d7'6644'b0ebULL,
				0x7c8c'65e2'0a0c'7ee6ULL, 0x80c8'56b0'07f1'd214ULL, 0x8c0b'4030'2cc3'2271ULL,
				0xdbce'dad5'1fe1'7a8aULL, 0x740e'8ae9'38db'dea0ULL, 0xa615'c6dc'5493'10adULL,
				0x19cc'55f6'171a'e90bULL, 0x49b1'bdb8'fe5f'dd8dULL, 0xed0a'89af'2830'e5bfULL,
				0x6a7a'adb4'f5a6'5bd6ULL, 0x7e22'9729'88f0'5679ULL, 0xf952'b332'5566'e810ULL,
				0x39fe'ceda'df61'530eULL, 0x6101'c99f'04f3'c7ceULL, 0x2e5f'7f67'61b5'62ffULL,
				0xf087'25d2'26cf'5c97ULL, 0x63af'3b54'860f'ef51ULL, 0x8ff2'cb10'ef41'1e2fULL,
				0x884a'b9bb'3526'7252ULL, 0x4df0'4433'e7ba'8daeULL, 0x9afd'8866'd369'0741ULL,
				0x66b9'bb34'de94'abb3ULL, 0x9baa'f18d'9217'1380ULL, 0x543c'11c5'f0a0'64a5ULL,
				0x17a1'b1bd'bed4'31f1ULL, 0xb5f5'8eea'f3a2'717fULL, 0xc355'f6c8'4985'8740ULL,
				0xec5d'f044'694e'f17eULL, 0xd837'51f5'dc63'46d4ULL, 0xfc44'3352'0dfd'acf2ULL,
				0x0000'0000'0000'0000ULL, 0x5a51'f58e'596e'bc5fULL, 0x3285'aaf1'2e34'cf16ULL,
				0x8d5c'39db'6dbd'36b0ULL, 0x12b7'31dd'e64f'7513ULL, 0x9490'6c2d'7aa7'dfbbULL,
				0x302b'583a'acc8'e789ULL, 0x9d45'facd'090e'6b3cULL, 0x2165'e2c7'8905'aec4ULL,
				0x68d4'5f7f'775a'7349ULL, 0x189b'2c1d'5664'fdcaULL, 0xe1c9'9f2f'0302'15daULL,
				0x6983'2694'3624'6788ULL, 0x8489'af3b'1e14'8237ULL, 0xe94b'7024'31d5'b59cULL,
				0x33d2'd31a'6f4a'dbd7ULL, 0xbfd9'932a'4389'f9a6ULL, 0xb0e3'0e8a'ab39'359dULL,
				0xd1e2'c715'afca'f253ULL, 0x150f'4376'3c28'196eULL, 0xc4ed'8463'93e2'eb3dULL,
				0x03f9'8b20'c382'3c5eULL, 0xfd13'4ab9'4c83'b833ULL, 0x556b'682e'b1de'7064ULL,
				0x36c4'537a'37d1'9f35ULL, 0x7559'f302'79a5'ca61ULL, 0x799a'e582'5297'3a04ULL,
				0x9c12'8326'4870'7ffdULL, 0x78cd'9c69'13e9'2ec5ULL, 0x1d8d'ac7d'0eff'b928ULL,
				0x439d'a078'4e74'5554ULL, 0x4133'52b3'cc88'7dcbULL, 0xbacf'134a'1b12'bd44ULL,
				0x114e'bafd'25cd'494dULL, 0x2f08'068c'20cb'763eULL, 0x76a0'7822'ba27'f63fULL,
				0xeab2'fb04'f257'89c2ULL, 0xe367'6de4'81fe'3d45ULL, 0x1b62'a73d'95e6'c194ULL,
				0x6417'49ff'5c68'832cULL, 0xa5ec'4dfc'9711'2cf3ULL, 0xf668'2e92'bdd6'242bULL,
				0x3f11'c59a'4478'2bb2ULL, 0x317c'21d1'edb6'f348ULL, 0xd65a'b5be'75ad'9e2eULL,
				0x6b2d'd45f'b4d8'4f17ULL, 0xfaab'3812'96e4'd44eULL, 0xd0b5'befe'eeb4'e692ULL,
				0x0882'ef0b'32d7'a046ULL, 0x512a'91a5'a83b'2047ULL, 0x963e'9ee6'f85b'f724ULL,
				0x4e09'cf13'2438'b1f0ULL, 0x77f7'01c9'fb59'e2feULL, 0x7ddb'1c09'4b72'6a27ULL,
				0x5f47'75ee'01f5'f8bdULL, 0x9186'ec4d'223c'9b59ULL, 0xfeea'c199'8f01'846dULL,
				0xac39'db1c'e4b8'9874ULL, 0xb75b'7c21'715e'59e0ULL, 0xafc0'503c'273a'a42aULL,
				0x6e3b'543f'ec43'0bf5ULL, 0x704f'7362'213e'8e83ULL, 0x58ff'0745'db92'94c0ULL,
				0x67ee'c2df'9fea'bf72ULL, 0xa0fa'cd9c'cf8a'6811ULL, 0xb936'986a'd890'811aULL,
				0x95c7'15c6'3bd9'cb7aULL, 0xca80'6028'3a2c'33c7ULL, 0x507d'e84e'e945'3486ULL,
				0x85de'd6d0'5f6a'96f6ULL, 0x1cda'd596'4f81'ade9ULL, 0xd5a3'3e9e'b62f'a270ULL,
				0x4064'2b58'8df6'690aULL, 0x7f75'eec2'c98e'42b8ULL, 0x2cf1'8dac'e349'4a60ULL,
				0x23cb'100c'0bf9'865bULL, 0xeef3'028f'ebb2'd9e1ULL, 0x4425'd2d3'9413'3929ULL,
				0xaad6'd05c'7fa1'e0c8ULL, 0xad6e'a2f7'a5c6'8cb5ULL, 0xc202'8f23'08fb'9381ULL,
				0x819f'2f5b'468f'c6d5ULL, 0xc5ba'fd88'd29c'fffcULL, 0x47dc'59f3'5791'0577ULL,
				0x2b49'ff07'392e'261dULL, 0x57c5'9ae5'3322'58fbULL, 0x73b6'f842'e2bc'b2ddULL,
				0xcf96'e048'62b7'7725ULL, 0x4ca7'3dd8'a6c4'996fULL, 0x0157'79eb'417e'14c1ULL,
				0x3793'2a91'76af'8bf4ULL},
			{0x190a'2c9b'249d'f23eULL, 0x2f62'f8b6'2263'e1e9ULL, 0x7a7f'7547'4099'3655ULL,
				0x330b'7ba4'd556'4d9fULL, 0x4c17'a16a'4667'2582ULL, 0xb22f'08eb'7d05'f5b8ULL,
				0x535f'47f4'0bc1'48ccULL, 0x3aec'5d27'd488'3037ULL, 0x10ed'0a18'2543'8f96ULL,
				0x5161'01f7'2c23'3d17ULL, 0x13cc'6f94'9fd0'4eaeULL, 0x7398'53c4'4147'4bfdULL,
				0x6537'93d9'0d3f'5b1bULL, 0x5240'647b'96b0'fc2fULL, 0x0c84'890a'd276'23e0ULL,
				0xd718'9b32'703a'aea3ULL, 0x2685'de35'23bd'9c41ULL, 0x9931'7c5b'11bf'fefaULL,
				0x0d9b'aa85'4f07'9703ULL, 0x70b9'3648'fbd4'8ac5ULL, 0xa804'41fc'e30b'c6beULL,
				0x7287'704b'dc36'ff1eULL, 0xb653'84ed'33dc'1f13ULL, 0xd364'1734'3ee3'4408ULL,
				0x39cd'38ab'6e1b'f10fULL, 0x5ab8'6177'0a1f'3564ULL, 0x0eba'cf09'f594'563bULL,
				0xd045'72b8'8470'8530ULL, 0x3cae'9722'bdb3'af47ULL, 0x4a55'6b6f'2f5c'baf2ULL,
				0xe170'4f1f'76c4'bd74ULL, 0x5ec4'ed71'44c6'dfcfULL, 0x16af'c01d'4c78'10e6ULL,
				0x283f'113c'd629'ca7aULL, 0xaf59'a876'1741'ed2dULL, 0xeed5'a399'1e21'5facULL,
				0x3bf3'7ea8'49f9'84d4ULL, 0xe413'e096'a56c'e33cULL, 0x2c43'9d3a'98f0'20d1ULL,
				0x6375'59dc'6404'c46bULL, 0x9e6c'95d1'e5f5'd569ULL, 0x24bb'9836'045f'e99aULL,
				0x44ef'a466'dac8'ecc9ULL, 0xc6ea'b2a5'c808'95d6ULL, 0x803b'50c0'3522'0cc4ULL,
				0x0321'658c'ba93'c138ULL, 0x8f9e'bc46'5dc7'ee1cULL, 0xd15a'5137'1901'31d3ULL,
				0x0fa5'ec86'68e5'e2d8ULL, 0x91c9'7957'8d10'37b1ULL, 0x0642'ca05'693b'9f70ULL,
				0xefca'8016'8350'eb4fULL, 0x38d2'1b24'f36a'45ecULL, 0xbeab'81e1'af73'd658ULL,
				0x8cbf'd9ca'e754'2f24ULL, 0xfd19'cc0d'81f1'1102ULL, 0x0ac6'430f'bb4d'bc90ULL,
				0x1d76'a09d'6a44'1895ULL, 0x2a01'573f'f1cb'bfa1ULL, 0xb572'e161'894f'de2bULL,
				0x8124'734f'a853'b827ULL, 0x614b'1fdf'43e6'b1b0ULL, 0x68ac'395c'4238'cc18ULL,
				0x21d8'37bf'd7f7'b7d2ULL, 0x20c7'1430'4a86'0331ULL, 0x5cfa'ab72'6324'aa14ULL,
				0x74c5'ba4e'b50d'606eULL, 0xf3a3'0304'7465'4739ULL, 0x23e6'71bc'f015'c209ULL,
				0x45f0'87e9'47b9'582aULL, 0xd8bd'77b4'18df'4c7bULL, 0xe06f'6c90'ebb5'0997ULL,
				0x0bd9'6080'263c'0873ULL, 0x7e03'f941'0e40'dcfeULL, 0xb8e9'4be4'c648'4928ULL,
				0xfb5b'0608'e8ca'8e72ULL, 0x1a2b'4917'9e0e'3306ULL, 0x4e29'e769'6185'5059ULL,
				0x4f36'c4e6'fcf4'e4baULL, 0x4974'0ee3'95cf'7bcaULL, 0xc296'3ea3'86d1'7f7dULL,
				0x90d6'5ad8'1061'8352ULL, 0x12d3'4c1b'02a1'fa4dULL, 0xfa44'2587'75bb'3a91ULL,
				0x1815'0f14'b9ec'46ddULL, 0x1491'861e'6b9a'653dULL, 0x9a10'19d7'ab2c'3fc2ULL,
				0x3668'd42d'06fe'13d7ULL, 0xdcc1'fbb2'5606'a6d0ULL, 0x9694'90dd'795a'1c22ULL,
				0x3549'b1a1'bc6d'd2efULL, 0xc94f'5e23'a0ed'770eULL, 0xb9f6'686b'5b39'fdcbULL,
				0xc4d4'f4a6'efea'e00dULL, 0xe732'851a'1fff'2204ULL, 0x94aa'd6de'5eb8'69f9ULL,
				0x3f8f'f2ae'0720'6e7fULL, 0xfe38'a981'3b62'd03aULL, 0xa7a1'ad7a'8bee'2466ULL,
				0x7b60'56c8'dde8'82b6ULL, 0x302a'1e28'6fc5'8ca7ULL, 0x8da0'fa45'7a25'9bc7ULL,
				0xb330'2b64'e074'415bULL, 0x5402'ae7e'ff8b'635fULL, 0x08f8'050c'9caf'c94bULL,
				0xae46'8bf9'8a30'59ceULL, 0x88c3'55cc'a98d'c58fULL, 0xb10e'6d67'c796'3480ULL,
				0xbad7'0de7'e1aa'3cf3ULL, 0xbfb4'a26e'3202'62bbULL, 0xcb71'1820'870f'02d5ULL,
				0xce12'b7a9'54a7'5c9dULL, 0x563c'e87d'd869'1684ULL, 0x9f73'b65e'7884'618aULL,
				0x2b1e'74b0'6cba'0b42ULL, 0x47ce'c1ea'605b'2df1ULL, 0x1c69'8312'f735'ac76ULL,
				0x5fdb'cefe'd9b7'6b2cULL, 0x831a'354c'8fb1'cdfcULL, 0x8205'16c3'12c0'791fULL,
				0xb74c'a762'aead'abf0ULL, 0xfc06'ef82'1c80'a5e1ULL, 0x5723'cbf2'4518'a267ULL,
				0x9d4d'f05d'5f66'1451ULL, 0x5886'2774'2dfd'40bfULL, 0xda83'31b7'3f3d'39a0ULL,
				0x17b0'e392'd109'a405ULL, 0xf965'400b'cf28'fba9ULL, 0x7c3d'bf42'29a2'a925ULL,
				0x023e'4603'27e2'75dbULL, 0x6cd0'b55a'0ce1'26b3ULL, 0xe62d'a695'828e'96e7ULL,
				0x42ad'6e63'b3f3'73b9ULL, 0xe50c'c319'381d'57dfULL, 0xc5cb'd729'729b'54eeULL,
				0x46d1'e265'fd2a'9912ULL, 0x6428'b056'904e'eff8ULL, 0x8be2'3040'131e'04b7ULL,
				0x6709'd5da'2add'2ec0ULL, 0x075d'e98a'f44a'2b93ULL, 0x8447'dcc6'7bfb'e66fULL,
				0x6616'f655'b7ac'9a23ULL, 0xd607'b8bd'ed4b'1a40ULL, 0x0563'af89'd3a8'5e48ULL,
				0x3db1'b4ad'20c2'1ba4ULL, 0x11f2'2997'b832'3b75ULL, 0x2920'32b3'4b58'7e99ULL,
				0x7f1c'dace'9331'681dULL, 0x8e81'9fc9'c0b6'5affULL, 0xa1e3'677f'e2d5'bb16ULL,
				0xcd33'd225'ee34'9da5ULL, 0xd9a2'543b'85ae'f898ULL, 0x795e'10cb'fa0a'f76dULL,
				0x25a4'bbb9'992e'5d79ULL, 0x7841'3344'677b'438eULL, 0xf082'6688'cef6'8601ULL,
				0xd27b'34bb'a392'f0ebULL, 0x551d'8df1'62fa'd7bcULL, 0x1e57'c511'd0d7'd9adULL,
				0xdeff'bdb1'71e4'd30bULL, 0xf4fe'ea8e'802f'6caaULL, 0xa480'c8f6'317d'e55eULL,
				0xa0fc'44f0'7fa4'0ff5ULL, 0x95b5'f551'c3c9'dd1aULL, 0x22f9'5233'6d64'76eaULL,
				0x0000'0000'0000'0000ULL, 0xa6be'8ef5'169f'9085ULL, 0xcc2c'f1aa'7345'2946ULL,
				0x2e7d'db39'bf12'550aULL, 0xd526'dd31'57d8'db78ULL, 0x486b'2d6c'08be'cf29ULL,
				0x9b0f'3a58'365d'8b21ULL, 0xac78'cdfa'add2'2c15ULL, 0xbc95'c7e2'8891'a383ULL,
				0x6a92'7f5f'65da'b9c3ULL, 0xc389'1d2c'1ba0'cb9eULL, 0xeaa9'2f9f'50f8'b507ULL,
				0xcf0d'9426'c9d6'e87eULL, 0xca6e'3baf'1a7e'b636ULL, 0xab25'2470'5998'0786ULL,
				0x69b3'1ad3'df49'78fbULL, 0xe251'2a93'cc57'7c4cULL, 0xff27'8a0e'a613'64d9ULL,
				0x71a6'15c7'66a5'3e26ULL, 0x89dc'7643'34fc'716cULL, 0xf87a'6384'5259'4f4aULL,
				0xf2bc'208b'e914'f3daULL, 0x8766'b94a'c168'2757ULL, 0xbbc8'2e68'7cdb'8810ULL,
				0x626a'7a53'f975'7088ULL, 0xa2c2'02f3'5846'7a2eULL, 0x4d08'82e5'db16'9161ULL,
				0x09e7'2683'01de'7da8ULL, 0xe897'699c'771a'c0dcULL, 0xc850'7dac'3d9c'c3edULL,
				0xc0a8'78a0'a133'0aa6ULL, 0x978b'b352'e42b'a8c1ULL, 0xe988'4a13'ea6b'743fULL,
				0x279a'fdba'becc'28a2ULL, 0x047c'8c06'4ed9'eaabULL, 0x507e'2278'b152'89f4ULL,
				0x5999'04fb'b08c'f45cULL, 0xbd8a'e46d'15e0'1760ULL, 0x3135'3da7'f2b4'3844ULL,
				0x8558'ff49'e68a'528cULL, 0x76fb'fc4d'92ef'15b5ULL, 0x3456'922e'211c'660cULL,
				0x8679'9ac5'5c19'93b4ULL, 0x3e90'd121'9a51'da9cULL, 0x2d5c'beb5'0581'9432ULL,
				0x982e'5fd4'8cce'4a19ULL, 0xdb9c'1238'a24c'8d43ULL, 0xd439'febe'caa9'6f9bULL,
				0x418c'0bef'0960'b281ULL, 0x158e'a591'f6eb'd1deULL, 0x1f48'e69e'4da6'6d4eULL,
				0x8afd'13cf'8e6f'b054ULL, 0xf5e1'c901'1d5e'd849ULL, 0xe34e'091c'5126'c8afULL,
				0xad67'ee75'30a3'98f6ULL, 0x43b2'4dec'2e82'c75aULL, 0x75da'99c1'287c'd48dULL,
				0x92e8'1cdb'3783'f689ULL, 0xa3dd'217c'c537'cecdULL, 0x6054'3c50'de97'0553ULL,
				0x93f7'3f54'aaf2'426aULL, 0xa91b'6273'7e7a'725dULL, 0xf19d'4507'5387'32e2ULL,
				0x77e4'dfc2'0f9e'a156ULL, 0x7d22'9ccd'b4d3'1dc6ULL, 0x1b34'6a98'037f'87e5ULL,
				0xedf4'c615'a4b2'9e94ULL, 0x4093'2860'9411'0662ULL, 0xb011'4ee8'5ae7'8063ULL,
				0x6ff1'd0d6'b672'e78bULL, 0x6dcf'96d5'9190'9250ULL, 0xdfe0'9e3e'ec95'67e8ULL,
				0x3214'582b'4827'f97cULL, 0xb46d'c2ee'143e'6ac8ULL, 0xf6c0'ac8d'a7cd'1971ULL,
				0xebb6'0c10'cd89'01e4ULL, 0xf7df'8f02'3abc'ad92ULL, 0x9c52'd3d2'c217'a0b2ULL,
				0x6b8d'5cd0'f8ab'0d20ULL, 0x3777'f7a2'9b8f'a734ULL, 0x011f'238f'9d71'b4e3ULL,
				0xc1b7'5b2f'3c42'be45ULL, 0x5de5'88fd'fe55'1ef7ULL, 0x6eee'f359'2b03'5368ULL,
				0xaa3a'07ff'c4e9'b365ULL, 0xeceb'e59a'39c3'2a77ULL, 0x5ba7'42f8'976e'8187ULL,
				0x4b4a'48e0'b22d'0e11ULL, 0xddde'd83d'cb77'1233ULL, 0xa59f'eb79'ac0c'51bdULL,
				0xc7f5'912a'5579'2135ULL},
			{0x6d6a'e046'68a9'b08aULL, 0x3ab3'f04b'0be8'c743ULL, 0xe51e'166b'54b3'c908ULL,
				0xbe90'a9eb'35c2'f139ULL, 0xb2c7'0666'37f2'bec1ULL, 0xaa69'4561'3392'202cULL,
				0x9a28'c36f'3b52'01ebULL, 0xddce'5a93'ab53'6994ULL, 0x0e34'133e'f638'2827ULL,
				0x52a0'2ba1'ec55'048bULL, 0xa2f8'8f97'c4b2'a177ULL, 0x8640'e513'ca22'51a5ULL,
				0xcdf1'd362'5813'7622ULL, 0xfe6c'b708'dedf'8ddbULL, 0x8a17'4a9e'c812'1e5dULL,
				0x6798'9603'6b81'560eULL, 0x59ed'0333'9579'5feeULL, 0x1dd7'78ab'8b74'edafULL,
				0xee53'3ef9'2d9f'926dULL, 0x2a8c'79ba'f8a8'd8f5ULL, 0x6bcf'398e'69b1'19f6ULL,
				0xe204'9174'2faf'dd95ULL, 0x2764'88e0'809c'2aecULL, 0xea95'5b82'd88f'5cceULL,
				0x7102'c63a'99d9'e0c4ULL, 0xf976'3017'a5c3'9946ULL, 0x429f'a250'1f15'1b3dULL,
				0x4659'c72b'ea05'd59eULL, 0x984b'7fdc'cf5a'6634ULL, 0xf742'2329'53fb'b161ULL,
				0x3041'860e'08c0'21c7ULL, 0x747b'fd96'16cd'9386ULL, 0x4bb1'3671'9231'2787ULL,
				0x1b72'a163'8a6c'44d3ULL, 0x4a0e'68a6'e835'9a66ULL, 0x169a'5039'f258'b6caULL,
				0xb98a'2ef4'4ede'e5a4ULL, 0xd908'3fe8'5e43'a737ULL, 0x967f'6ce2'3962'4e13ULL,
				0x8874'f62d'3c1a'7982ULL, 0x3c16'2983'0af0'6e3fULL, 0x9165'ebfd'427e'5a8eULL,
				0xb5dd'8179'4cee'aa5cULL, 0x0de8'f15a'7834'f219ULL, 0x70bd'98ed'e3dd'5d25ULL,
				0xaccc'9ca9'328a'8950ULL, 0x5666'4eda'1945'ca28ULL, 0x221d'b34c'0f88'59aeULL,
				0x26db'd637'fa98'970dULL, 0x1acd'ffb4'f068'f932ULL, 0x4585'254f'6409'0fa0ULL,
				0x72de'245e'17d5'3afaULL, 0x1546'b25d'7c54'6cf4ULL, 0x207e'0fff'fb80'3e71ULL,
				0xfaaa'd273'2bcf'4378ULL, 0xb462'dfae'36ea'17bdULL, 0xcf92'6fd1'ac1b'11fdULL,
				0xe067'2dc7'dba7'ba4aULL, 0xd3fa'49ad'5d6b'41b3ULL, 0x8ba8'1449'b216'a3bcULL,
				0x14f9'ec8a'0650'd115ULL, 0x40fc'1ee3'eb1d'7ce2ULL, 0x23a2'ed9b'758c'e44fULL,
				0x782c'521b'14fd'dc7eULL, 0x1c68'267c'f170'504eULL, 0xbcf3'1558'c1ca'96e6ULL,
				0xa781'b43b'4ba6'd235ULL, 0xf6fd'7dfe'29ff'0c80ULL, 0xb0a4'bad5'c3fa'd91eULL,
				0xd199'f51e'a963'266cULL, 0x4143'4034'9119'c103ULL, 0x5405'f269'ed4d'adf7ULL,
				0xabd6'1bb6'4996'9dcdULL, 0x6813'dbea'e7bd'c3c8ULL, 0x65fb'2ab0'9f89'31d1ULL,
				0xf1e7'fae1'52e3'181dULL, 0xc1a6'7cef'5a23'39daULL, 0x7a4f'eea8'e0f5'bba1ULL,
				0x1e0b'9acf'0578'3791ULL, 0x5b8e'bf80'6171'3831ULL, 0x80e5'3cdb'cb3a'f8d9ULL,
				0x7e89'8bd3'15e5'7502ULL, 0xc6bc'fbf0'213f'2d47ULL, 0x95a3'8e86'b76e'942dULL,
				0x092e'9421'8d24'3cbaULL, 0x8339'debf'4536'22e7ULL, 0xb11b'e402'b9fe'64ffULL,
				0x57d9'100d'6341'77c9ULL, 0xcc4e'8db5'2217'cbc3ULL, 0x3b0c'ae9c'71ec'7aa2ULL,
				0xfb15'8ca4'51cb'fe99ULL, 0x2b33'276d'82ac'6514ULL, 0x01bf'5ed7'7a04'bde1ULL,
				0xc560'1994'af33'f779ULL, 0x75c4'a341'6cc9'2e67ULL, 0xf384'4652'a6eb'7fc2ULL,
				0x3487'e375'fdd0'ef64ULL, 0x18ae'4307'0460'9eedULL, 0x4d14'efb9'9329'8efbULL,
				0x815a'620c'b13e'4538ULL, 0x125c'3542'0748'7869ULL, 0x9eee'a614'ce42'cf48ULL,
				0xce2d'3106'd61f'ac1cULL, 0xbbe9'9247'bad6'827bULL, 0x071a'871f'7b1c'149dULL,
				0x2e4a'1cc1'0db8'1656ULL, 0x77a7'1ff2'98c1'49b8ULL, 0x06a5'd9c8'0118'a97cULL,
				0xad73'c27e'488e'34b1ULL, 0x443a'7b98'1e0d'b241ULL, 0xe3bb'cfa3'55ab'6074ULL,
				0x0af2'7645'0328'e684ULL, 0x7361'7a89'6dd1'871bULL, 0x5852'5de4'ef7d'e20fULL,
				0xb7be'3dca'b8e6'cd83ULL, 0x1911'1dd0'7e64'230cULL, 0x8423'59a0'3e2a'367aULL,
				0x103f'89f1'f340'1fb6ULL, 0xdc71'0444'd157'd475ULL, 0xb835'7023'34da'5845ULL,
				0x4320'fc87'6511'a6dcULL, 0xd026'abc9'd367'9b8dULL, 0x1725'0eee'885c'0b2bULL,
				0x90da'b52a'387a'e76fULL, 0x31fe'd8d9'72c4'9c26ULL, 0x89cb'a8fa'461e'c463ULL,
				0x2ff5'4216'77bc'abb7ULL, 0x396f'122f'85e4'1d7dULL, 0xa09b'3324'30ba'c6a8ULL,
				0xc888'e8ce'd707'0560ULL, 0xaeaf'201a'c682'ee8fULL, 0x1180'd726'8944'a257ULL,
				0xf058'a436'28e7'a5fcULL, 0xbd4c'4b8f'bbce'2b07ULL, 0xa124'6df3'4abe'7b49ULL,
				0x7d55'69b7'9be9'af3cULL, 0xa9b5'a705'bd9e'fa12ULL, 0xdb6b'835b'aa4b'c0e8ULL,
				0x0579'3bac'8f14'7342ULL, 0x21c1'5128'8184'8390ULL, 0xfdb0'556c'50d3'57e5ULL,
				0x613d'4fcb'6a99'ff72ULL, 0x03dc'e264'8e0c'da3eULL, 0xe949'b9e6'5683'86f0ULL,
				0xfc0f'0bbb'2ad7'ea04ULL, 0x6a70'6759'13b5'a417ULL, 0x7f36'd504'6fe1'c8e3ULL,
				0x0c57'af8d'0230'4ff8ULL, 0x3222'3abd'fcc8'4618ULL, 0x0891'caf6'f720'815bULL,
				0xa63e'eaec'31a2'6fd4ULL, 0x2507'3453'7494'4d33ULL, 0x49d2'8ac2'6639'4058ULL,
				0xf521'9f9a'a7f3'd6beULL, 0x2d96'fea5'83b4'cc68ULL, 0x5a31'e157'1b75'85d0ULL,
				0x8ed1'2fe5'3d02'd0feULL, 0xdfad'e620'5f5b'0e4bULL, 0x4cab'b16e'e92d'331aULL,
				0x04c6'657b'f510'cea3ULL, 0xd73c'2cd6'a87b'8f10ULL, 0xe1d8'7310'a1a3'07abULL,
				0x6cd5'be91'12ad'0d6bULL, 0x97c0'3235'4366'f3f2ULL, 0xd4e0'ceb2'2677'552eULL,
				0x0000'0000'0000'0000ULL, 0x2950'9bde'76a4'02cbULL, 0xc27a'9e8b'd42f'e3e4ULL,
				0x5ef7'842c'ee65'4b73ULL, 0xaf10'7ecd'bc86'536eULL, 0x3fca'cbe7'84fc'b401ULL,
				0xd55f'9065'5c73'e8cfULL, 0xe6c2'f40f'dabf'1336ULL, 0xe8f6'e731'2c87'3b11ULL,
				0xeb2a'0555'a28b'e12fULL, 0xe4a1'48bc'2eb7'74e9ULL, 0x9b97'9db8'4156'bc0aULL,
				0x6eb6'0222'e6a5'6ab4ULL, 0x87ff'bbc4'b026'ec44ULL, 0xc703'a527'5b3b'90a6ULL,
				0x47e6'99fc'9001'687fULL, 0x9c8d'1aa7'3a4a'a897ULL, 0x7cea'3760'e1ed'12ddULL,
				0x4ec8'0ddd'1d25'54c5ULL, 0x13e3'6b95'7d4c'c588ULL, 0x5d2b'6648'6069'914dULL,
				0x92b9'0999'cc72'80b0ULL, 0x517c'c9c5'6259'deb5ULL, 0xc937'b619'ad03'b881ULL,
				0xec30'824a'd997'f5b2ULL, 0xa45d'565f'c5aa'080bULL, 0xd683'7201'd27f'32f1ULL,
				0x635e'f378'9e91'98adULL, 0x531f'7576'9651'b96aULL, 0x4f77'530a'6721'e924ULL,
				0x486d'd415'1c3d'fdb9ULL, 0x5f48'dafb'9461'f692ULL, 0x375b'0111'73dc'355aULL,
				0x3da9'7754'70f4'd3deULL, 0x8d0d'cd81'b30e'0ac0ULL, 0x36e4'5fc6'09d8'88bbULL,
				0x55ba'acbe'9749'1016ULL, 0x8cb2'9356'c90a'b721ULL, 0x7618'4125'e2c5'f459ULL,
				0x99f4'210b'b55e'dbd5ULL, 0x6f09'5cf5'9ca1'd755ULL, 0x9f51'f8c3'b446'72a9ULL,
				0x3538'bda2'87d4'5285ULL, 0x50c3'9712'185d'6354ULL, 0xf23b'1885'dcef'c223ULL,
				0x7993'0ccc'6ef9'619fULL, 0xed8f'dc9d'a393'4853ULL, 0xcb54'0aaa'590b'df5eULL,
				0x5c94'389f'1a6d'2cacULL, 0xe77d'aad8'a0bb'aed7ULL, 0x28ef'c509'0ca0'bf2aULL,
				0xbf2f'f73c'4fc6'4cd8ULL, 0xb378'58b1'4df6'0320ULL, 0xf8c9'6ec0'dfc7'24a7ULL,
				0x8286'8068'3f32'9f06ULL, 0x941c'd051'cd6a'29ccULL, 0xc3c5'c05c'ae2b'5e05ULL,
				0xb601'631d'c2e2'7062ULL, 0xc019'2238'2027'843bULL, 0x24b8'6a84'0e90'f0d2ULL,
				0xd245'177a'276f'fc52ULL, 0x0f8b'4de9'8c3c'95c6ULL, 0x3e75'9530'fef8'09e0ULL,
				0x0b4d'2892'792c'5b65ULL, 0xc4df'4743'd537'4a98ULL, 0xa5e2'0888'bfae'b5eaULL,
				0xba56'cc90'c0d2'3f9aULL, 0x38d0'4cf8'ffe0'a09cULL, 0x62e1'adaf'e495'254cULL,
				0x0263'bcb3'f408'67dfULL, 0xcaeb'547d'230f'62bfULL, 0x6082'111c'109d'4293ULL,
				0xdad4'dd8c'd04f'7d09ULL, 0xefec'602e'579b'2f8cULL, 0x1fb4'c418'7f7c'8a70ULL,
				0xffd3'e9df'a4db'303aULL, 0x7bf0'b07f'9af1'0640ULL, 0xf49e'c14d'ddf7'6b5fULL,
				0x8f6e'7132'4706'6d1fULL, 0x339d'646a'86cc'fbf9ULL, 0x6444'7467'e58d'8c30ULL,
				0x2c29'a072'f9b0'7189ULL, 0xd8b7'613f'2447'1ad6ULL, 0x6627'c8d4'1185'ebefULL,
				0xa347'd140'beb6'1c96ULL, 0xde12'b8f7'255f'b3aaULL, 0x9d32'4470'404e'1576ULL,
				0x9306'574e'b676'3d51ULL, 0xa80a'f9d2'c79a'47f3ULL, 0x859c'0777'442e'8b9bULL,
				0x69ac'853d'9db9'7e29ULL},
			{0xc340'7dfc'2de6'377eULL, 0x5b9e'93ee'a425'6f77ULL, 0xadb5'8fdd'50c8'45e0ULL,
				0x5219'ff11'a75b'ed86ULL, 0x356b'61cf'd90b'1de9ULL, 0xfb8f'406e'25ab'e037ULL,
				0x7a5a'0231'c0f6'0796ULL, 0x9d3c'd216'e1f5'020bULL, 0x0c65'50fb'6b48'd8f3ULL,
				0xf575'08c4'27ff'1c62ULL, 0x4ad3'5ffa'71cb'407dULL, 0x6290'a2da'1666'aa6dULL,
				0xe284'ec23'4935'5f9fULL, 0xb3c3'07c5'3d7c'84ecULL, 0x05e2'3c04'6836'5a02ULL,
				0x190b'ac4d'6c9e'bfa8ULL, 0x94bb'bee9'e28b'80faULL, 0xa34f'c777'529c'b9b5ULL,
				0xcc7b'39f0'95bc'd978ULL, 0x2426'addb'0ce5'32e3ULL, 0x7e79'3293'12ce'4fc7ULL,
				0xab09'a72e'ebec'2917ULL, 0xf8d1'5499'f6b9'd6c2ULL, 0x1a55'b8ba'bf8c'895dULL,
				0xdb8a'dd17'fb76'9a85ULL, 0xb57f'2f36'8658'e81bULL, 0x8acd'36f1'8f3f'41f6ULL,
				0x5ce3'b7bb'a50f'11d3ULL, 0x114d'cc14'd5ee'2f0aULL, 0xb91a'7fcd'ed10'30e8ULL,
				0x81d5'425f'e55d'e7a1ULL, 0xb621'3bc1'554a'deeeULL, 0x8014'4ef9'5f53'f5f2ULL,
				0x1e76'8818'6db4'c10cULL, 0x3b91'2965'db5f'e1bcULL, 0xc281'715a'97e8'252dULL,
				0x54a5'd7e2'1c7f'8171ULL, 0x4b12'535c'cbc5'522eULL, 0x1d28'9cef'bea6'f7f9ULL,
				0x6ef5'f221'7d2e'729eULL, 0xe6a7'dc81'9b0d'17ceULL, 0x1b94'b41c'0582'9b0eULL,
				0x33d7'493c'622f'711eULL, 0xdcf7'f942'fa5c'e421ULL, 0x600f'ba8b'7f7a'8ecbULL,
				0x46b6'0f01'1a83'988eULL, 0x235b'898e'0dcf'4c47ULL, 0x957a'b24f'5885'92a9ULL,
				0x4354'3305'72b5'c28cULL, 0xa5f3'ef84'e9b8'd542ULL, 0x8c71'1e02'341b'2d01ULL,
				0x0b18'74ae'6a62'a657ULL, 0x1213'd8e3'06fc'19ffULL, 0xfe6d'7c6a'4d9d'ba35ULL,
				0x65ed'868f'174c'd4c9ULL, 0x8852'2ea0'e623'6550ULL, 0x8993'2206'5c2d'7703ULL,
				0xc01e'690b'fef4'018bULL, 0x9159'82ed'8abd'daf8ULL, 0xbe67'5b98'ec3a'4e4cULL,
				0xa996'bf7f'82f0'0db1ULL, 0xe1da'f8d4'9a27'696aULL, 0x2eff'd5d3'dc89'86e7ULL,
				0xd153'a51f'2b1a'2e81ULL, 0x18ca'a0eb'd690'adfbULL, 0x390e'3134'b243'c51aULL,
				0x2778'b92c'dff7'0416ULL, 0x029f'1851'691c'24a6ULL, 0x5e7c'afea'cc13'3575ULL,
				0xfa4e'4cc8'9fa5'f264ULL, 0x5a5f'9f48'1e2b'7d24ULL, 0x484c'47ab'18d7'64dbULL,
				0x400a'27f2'a1a7'f479ULL, 0xaeeb'9b2a'83da'7315ULL, 0x721c'6268'7986'9734ULL,
				0x0423'30a2'd238'4851ULL, 0x85f6'72fd'3765'aff0ULL, 0xba44'6b3a'3e02'061dULL,
				0x73dd'6ece'c388'8567ULL, 0xffac'70cc'f793'a866ULL, 0xdfa9'edb5'294e'd2d4ULL,
				0x6c6a'ea70'1432'5638ULL, 0x834a'5a0e'8c41'c307ULL, 0xcdba'3556'2fb2'cb2bULL,
				0x0ad9'7808'd06c'b404ULL, 0x0f3b'440c'b85a'ee06ULL, 0xe5f9'c876'481f'213bULL,
				0x98de'ee12'89c3'5809ULL, 0x5901'8bbf'cd39'4bd1ULL, 0xe01b'f472'2029'7b39ULL,
				0xde68'e113'9340'c087ULL, 0x9fa3'ca47'88e9'26adULL, 0xbb85'679c'840c'144eULL,
				0x53d8'f3b7'1d55'ffd5ULL, 0x0da4'5c5d'd146'caa0ULL, 0x6f34'fe87'c720'60cdULL,
				0x57fb'c315'cf6d'b784ULL, 0xcee4'21a1'fca0'fddeULL, 0x3d2d'0196'607b'8d4bULL,
				0x642c'8a29'ad42'c69aULL, 0x14af'f010'bdd8'7508ULL, 0xac74'837b'eac6'57b3ULL,
				0x3216'459a'd821'634dULL, 0x3fb2'19c7'0967'a9edULL, 0x06bc'28f3'bb24'6cf7ULL,
				0xf208'2c91'26d5'62c6ULL, 0x66b3'9278'c45e'e23cULL, 0xbd39'4f6f'3f28'78b9ULL,
				0xfd33'689d'9e8f'8cc0ULL, 0x37f4'799e'b017'394fULL, 0x108c'c0b2'6fe0'3d59ULL,
				0xda4b'd1b1'4178'88d6ULL, 0xb09d'1332'ee6e'b219ULL, 0x2f3e'd975'6687'94b4ULL,
				0x58c0'8719'7737'5982ULL, 0x7561'463d'78ac'e990ULL, 0x0987'6cff'037e'82f1ULL,
				0x7fb8'3e35'a8c0'5d94ULL, 0x26b9'b58a'65f9'1645ULL, 0xef20'b07e'9873'953fULL,
				0x3148'516d'0b33'55b8ULL, 0x41cb'2b54'1ba9'e62aULL, 0x7904'16c6'13e4'3163ULL,
				0xa011'd380'818e'8f40ULL, 0x3a50'25c3'6151'f3efULL, 0xd570'95bd'f922'66d0ULL,
				0x498d'4b0d'a2d9'7688ULL, 0x8b0c'3a57'3531'53a5ULL, 0x21c4'91df'64d3'68e1ULL,
				0x8f2f'0af5'e709'1bf4ULL, 0x2da1'c124'0f9b'b012ULL, 0xc43d'59a9'2ccc'49daULL,
				0xbfa6'573e'5634'5c1fULL, 0x828b'56a8'364f'd154ULL, 0x9a41'f643'e0df'7cafULL,
				0xbcf8'43c9'8526'6aeaULL, 0x2b1d'e9d7'b4bf'dce5ULL, 0x2005'9d79'dedd'7ab2ULL,
				0x6dab'e6d6'ae3c'446bULL, 0x45e8'1bf6'c991'ae7bULL, 0x6351'ae7c'ac68'b83eULL,
				0xa432'e322'53b6'c711ULL, 0xd092'a9b9'9114'3cd2ULL, 0xcac7'1103'2e98'b58fULL,
				0xd8d4'c9e0'2864'ac70ULL, 0xc5fc'550f'96c2'5b89ULL, 0xd7ef'8dec'903e'4276ULL,
				0x6772'9ede'7e50'f06fULL, 0xeac2'8c7a'f045'cf3dULL, 0xb15c'1f94'5460'a04aULL,
				0x9cfd'deb0'5bfb'1058ULL, 0x93c6'9abc'e3a1'fe5eULL, 0xeb03'80dc'4a4b'dd6eULL,
				0xd20d'b1e8'f808'1874ULL, 0x229a'8528'b7c1'5e14ULL, 0x4429'1750'739f'bc28ULL,
				0xd3cc'bd4e'4206'0a27ULL, 0xf62b'1c33'f4ed'2a97ULL, 0x86a8'660a'e477'9905ULL,
				0xd62e'814a'2a30'5025ULL, 0x4777'03a7'a08d'8addULL, 0x7b9b'0e97'7af8'15c5ULL,
				0x78c5'1a60'a9ea'2330ULL, 0xa6ad'fb73'3aaa'e3b7ULL, 0x97e5'aa1e'3199'b60fULL,
				0x0000'0000'0000'0000ULL, 0xf4b4'0462'9df1'0e31ULL, 0x5564'db44'a671'9322ULL,
				0x9207'961a'59af'ec0dULL, 0x9624'a6b8'8b97'a45cULL, 0x3635'7538'0a19'2b1cULL,
				0x2c60'cd82'b595'a241ULL, 0x7d27'2664'c1dc'7932ULL, 0x7142'769f'aa94'a1c1ULL,
				0xa1d0'df26'3b80'9d13ULL, 0x1630'e841'd4c4'51aeULL, 0xc1df'65ad'44fa'13d8ULL,
				0x13d2'd445'bcf2'0bacULL, 0xd915'c546'926a'be23ULL, 0x38cf'3d92'084d'd749ULL,
				0xe766'd027'2103'059dULL, 0xc763'4d5e'ffde'7f2fULL, 0x077d'2455'012a'7ea4ULL,
				0xedbf'a82f'f16f'b199ULL, 0xaf2a'978c'39d4'6146ULL, 0x4295'3fa3'c8bb'd0dfULL,
				0xcb06'1da5'9496'a7dcULL, 0x25e7'a17d'b6eb'20b0ULL, 0x34aa'6d69'6305'0fbaULL,
				0xa76c'f7d5'80a4'f1e4ULL, 0xf7ea'1095'4ee3'38c4ULL, 0xfcf2'643b'2481'9e93ULL,
				0xcf25'2d07'46ae'ef8dULL, 0x4ef0'6f58'a3f3'082cULL, 0x563a'cfb3'7563'a5d7ULL,
				0x5086'e740'ce47'c920ULL, 0x2982'f186'dda3'f843ULL, 0x8769'6aac'5e79'8b56ULL,
				0x5d22'bb1d'1f01'0380ULL, 0x035e'14f7'd312'36f5ULL, 0x3cec'0d30'da75'9f18ULL,
				0xf3c9'2037'9cdb'7095ULL, 0xb8db'736b'571e'22bbULL, 0xdd36'f5e4'4052'f672ULL,
				0xaac8'ab88'51e2'3b44ULL, 0xa857'b3d9'38fe'1fe2ULL, 0x17f1'e4e7'6eca'43fdULL,
				0xec7e'a489'4b61'a3caULL, 0x9e62'c6e1'32e7'34feULL, 0xd4b1'991b'432c'7483ULL,
				0x6ad6'c283'af16'3acfULL, 0x1ce9'9049'04a8'e5aaULL, 0x5fbd'a34c'761d'2726ULL,
				0xf910'583f'4cb7'c491ULL, 0xc6a2'41f8'45d0'6d7cULL, 0x4f31'63fe'19fd'1a7fULL,
				0xe99c'988d'2357'f9c8ULL, 0x8eee'0653'5d07'09a7ULL, 0x0efa'48aa'0254'fc55ULL,
				0xb4be'2390'3c56'fa48ULL, 0x763f'52ca'abbe'df65ULL, 0xeee1'bcd8'227d'876cULL,
				0xe345'e085'f33b'4dccULL, 0x3e73'1561'b369'bbbeULL, 0x2843'fd20'67ad'ea10ULL,
				0x2adc'e571'0eb1'ceb6ULL, 0xb7e0'3767'ef44'ccbdULL, 0x8db0'12a4'8e15'3f52ULL,
				0x61ce'b62d'c574'9c98ULL, 0xe85d'942b'9959'eb9bULL, 0x4c6f'7709'caef'2c8aULL,
				0x8437'7e5b'8d6b'bda3ULL, 0x3089'5dcb'b13d'47ebULL, 0x74a0'4a9b'c2a2'fbc3ULL,
				0x6b17'ce25'1518'289cULL, 0xe438'c4d0'f211'3368ULL, 0x1fb7'84be'd7ba'd35fULL,
				0x9b80'fae5'5ad1'6efcULL, 0x77fe'5e6c'11b0'cd36ULL, 0xc858'0952'4784'9129ULL,
				0x0846'6059'b970'90a2ULL, 0x01c1'0ca6'ba0e'1253ULL, 0x6988'd674'7c04'0c3aULL,
				0x6849'dad2'c60a'1e69ULL, 0x5147'ebe6'7449'db73ULL, 0xc999'05f4'fd8a'837aULL,
				0x991f'e2b4'33cd'4a5aULL, 0xf097'34c0'4fc9'4660ULL, 0xa28e'cbd1'e892'abe6ULL,
				0xf156'3866'f5c7'5433ULL, 0x4dae'7baf'70e1'3ed9ULL, 0x7ce6'2ac2'7bd2'6b61ULL,
				0x7083'7a39'109a'b392ULL, 0x9098'8e4b'30b3'c8abULL, 0xb202'0b63'8772'96bfULL,
				0x156e'fcb6'07d6'675bULL},
			{0xe63f'55ce'97c3'31d0ULL, 0x25b5'06b0'015b'ba16ULL, 0xc870'6e29'e6ad'9ba8ULL,
				0x5b43'd377'5d52'1f6aULL, 0x0bfa'3d57'7035'106eULL, 0xab95'fc17'2afb'0e66ULL,
				0xf64b'6397'9e7a'3276ULL, 0xf58b'4562'649d'ad4bULL, 0x48f7'c3db'ae0c'83f1ULL,
				0xff31'9166'42f5'c8c5ULL, 0xcbb0'48dc'1c4a'0495ULL, 0x66b8'f83c'df62'2989ULL,
				0x35c1'30e9'08e2'b9b0ULL, 0x7c76'1a61'f0b3'4fa1ULL, 0x3601'161c'f205'268dULL,
				0x9e54'ccfe'2219'b7d6ULL, 0x8b7d'90a5'3894'0837ULL, 0x9cd4'0358'8ea3'5d0bULL,
				0xbc3c'6fea'9ccc'5b5aULL, 0xe5ff'733b'6d24'aeedULL, 0xceed'22de'0f7e'b8d2ULL,
				0xec85'81ca'b1ab'545eULL, 0xb961'05e8'8ff8'e71dULL, 0x8ca0'3501'871a'5eadULL,
				0x76cc'ce65'd6db'2a2fULL, 0x5883'f582'a7b5'8057ULL, 0x3f7b'e4ed'2e8a'dc3eULL,
				0x0fe7'be06'355c'd9c9ULL, 0xee05'4e6c'1d11'be83ULL, 0x1074'3659'09b9'03a6ULL,
				0x5dde'9f80'b481'3c10ULL, 0x4a77'0c7d'02b6'692cULL, 0x5379'c8d5'd780'9039ULL,
				0xb406'7448'161e'd409ULL, 0x5f5e'5026'183b'd6cdULL, 0xe898'029b'f4c2'9df9ULL,
				0x7fb6'3c94'0a54'd09cULL, 0xc517'1f89'7f4b'a8bcULL, 0xa6f2'8db7'b31d'3d72ULL,
				0x2e4f'3be7'716e'aa78ULL, 0x0d67'71a0'99e6'3314ULL, 0x8207'6254'e41b'f284ULL,
				0x2f0f'd2b4'2733'df98ULL, 0x5c9e'76d3'e2dc'49f0ULL, 0x7aeb'5696'1960'6cdbULL,
				0x8347'8b07'b246'8764ULL, 0xcfad'cb8d'5923'cd32ULL, 0x85da'c7f0'5b95'a41eULL,
				0xb546'9d1b'4043'a1e9ULL, 0xb821'ecbb'd9a5'92fdULL, 0x1b8e'0b0e'798c'13c8ULL,
				0x62a5'7b6d'9a0b'e02eULL, 0xfcf1'b793'b812'57f8ULL, 0x9d94'ea0b'd8fe'28ebULL,
				0x4cea'408a'eb65'4a56ULL, 0x2328'4a47'e888'996cULL, 0x2d8f'1d12'8b89'3545ULL,
				0xf4cb'ac31'32c0'd8abULL, 0xbd7c'86b9'ca91'2ebaULL, 0x3a26'8eef'3dbe'6079ULL,
				0xf0d6'2f60'77a9'110cULL, 0x2735'c916'ade1'50cbULL, 0x89fd'5f03'942e'e2eaULL,
				0x1ace'e25d'2fd1'6628ULL, 0x90f3'9bab'4118'1bffULL, 0x430d'fe8c'de39'939fULL,
				0xf70b'8ac4'c827'4796ULL, 0x1c53'aeaa'c602'4552ULL, 0x13b4'10ac'f35e'9c9bULL,
				0xa532'ab42'49fa'a24fULL, 0x2b12'51e5'625a'163fULL, 0xd7e3'e676'da48'41c7ULL,
				0xa7b2'64e4'e540'4892ULL, 0xda84'97d6'43ae'72d3ULL, 0x861a'e105'a172'3b23ULL,
				0x38a6'4149'9104'8aa4ULL, 0x6578'dec9'2585'b6b4ULL, 0x0280'cfa6'acba'eaddULL,
				0x88bd'b650'c273'970aULL, 0x9333'bd5e'bbff'84c2ULL, 0x4e6a'8f2c'47df'a08bULL,
				0x321c'954d'b76c'ef2aULL, 0x418d'312a'7283'7942ULL, 0xb29b'38bf'ffcd'f773ULL,
				0x6c02'2c38'f90a'4c07ULL, 0x5a03'3a24'0b0f'6a8aULL, 0x1f93'885f'3ce5'da6fULL,
				0xc38a'537e'9698'8bc6ULL, 0x39e6'a81a'c759'ff44ULL, 0x2992'9e43'cee0'fce2ULL,
				0x40cd'd879'24de'0ca2ULL, 0xe9d8'ebc8'a29f'e819ULL, 0x0c27'98f3'cfbb'46f4ULL,
				0x55e4'8422'3e53'b343ULL, 0x4650'948e'cd0d'2fd8ULL, 0x20e8'6cb2'126f'0651ULL,
				0x6d42'c56b'af57'39e7ULL, 0xa06f'c140'5ace'1e08ULL, 0x7bab'bfc5'4f3d'193bULL,
				0x424d'17df'8864'e67fULL, 0xd804'5870'ef14'980eULL, 0xc6d7'397c'85ac'3781ULL,
				0x21a8'85e1'4432'73b1ULL, 0x67f8'116f'893f'5c69ULL, 0x24f5'efe3'5706'cff6ULL,
				0xd563'29d0'76f2'ab1aULL, 0x5e1e'b975'4e66'a32dULL, 0x28d2'7710'98bd'8902ULL,
				0x8f60'13f4'7dfd'c190ULL, 0x17a9'93fd'b637'553cULL, 0xe0a2'1939'7e10'12aaULL,
				0x786b'9930'b5da'8606ULL, 0x6e82'e39e'55b0'a6daULL, 0x875a'0856'f72f'4ec3ULL,
				0x3741'ff4f'a458'536dULL, 0xac48'59b3'9575'58fcULL, 0x7ef6'd5c7'5c09'a57cULL,
				0xc04a'758b'6c7f'14fbULL, 0xf9ac'dd91'ab26'ebbfULL, 0x7391'a467'c5ef'9668ULL,
				0x335c'7c1e'e131'9acaULL, 0xa915'33b1'8641'e4bbULL, 0xe4bf'9a68'3b79'db0dULL,
				0x8e20'faa7'2ba0'b470ULL, 0x51f9'0773'7b3a'7ae4ULL, 0x2268'a314'bed5'ec8cULL,
				0xd944'b123'b949'edeeULL, 0x31dc'b3b8'4d8b'7017ULL, 0xd3fe'6527'9f21'8860ULL,
				0x097a'f2f1'dc8f'fab3ULL, 0x9b09'a6fc'312d'0b91ULL, 0xcc6d'ed78'a3c4'520fULL,
				0x3481'd9ba'5ebf'cc50ULL, 0x4f2a'667f'1182'd56bULL, 0xdfd9'fdd4'509a'ce94ULL,
				0x2675'2045'fbbc'252bULL, 0xbffc'491f'662b'c467ULL, 0xdd59'3272'fc20'2449ULL,
				0x3cbb'c218'd46d'4303ULL, 0x91b3'72f8'1745'6e1fULL, 0x681f'af69'bc63'85a0ULL,
				0xb686'bbee'baa4'3ed4ULL, 0x1469'b508'4cd0'ca01ULL, 0x98c9'8009'cbca'94acULL,
				0x6438'379a'73d8'c354ULL, 0xc2ca'ba2d'c0c5'fe26ULL, 0x3e3b'0dbe'78d7'a9deULL,
				0x50b9'ee20'2d67'0f04ULL, 0x4590'b27b'37ea'b0e5ULL, 0x6025'b4cb'36b1'0af3ULL,
				0xfb2c'1237'079c'0162ULL, 0xa12f'2813'0c93'6be8ULL, 0x4b37'e52e'54eb'1cccULL,
				0x083a'1ba2'8ad2'8f53ULL, 0xc10a'9cd8'3a22'611bULL, 0x9f14'25ad'7444'c236ULL,
				0x069d'4cf7'e9d3'237aULL, 0xedc5'6899'e7f6'21beULL, 0x778c'2736'8086'5fcfULL,
				0x309c'5aeb'1bd6'05f7ULL, 0x8de0'dc52'd147'2b4dULL, 0xf8ec'34c2'fd7b'9e5fULL,
				0xea18'cd3d'5878'7724ULL, 0xaad5'1544'7ca6'7b86ULL, 0x9989'695a'9d97'e14cULL,
				0x0000'0000'0000'0000ULL, 0xf196'c633'21f4'64ecULL, 0x7111'6bc1'6955'7cb5ULL,
				0xaf88'7f46'6f92'c7c1ULL, 0x972e'3e0f'fe96'4d65ULL, 0x190e'c4a8'd536'f915ULL,
				0x95ae'f1a9'522c'a7b8ULL, 0xdc19'db21'aa7d'51a9ULL, 0x94ee'18fa'0471'd258ULL,
				0x8087'adf2'48a1'1859ULL, 0xc457'f6da'2916'dd5cULL, 0xfa6c'fb64'51c1'7482ULL,
				0xf256'e0c6'db13'fbd1ULL, 0x6a9f'60cf'10d9'6f7dULL, 0x4daa'a9d9'bd38'3fb6ULL,
				0x03c0'26f5'fae7'9f3dULL, 0xde99'1487'06c7'bb74ULL, 0x2a52'b8b6'3407'63dfULL,
				0x6fc2'0acd'03ed'd33aULL, 0xd423'c083'20af'defaULL, 0xbbe1'ca4e'2342'0dc0ULL,
				0x966e'd75c'a8cb'3885ULL, 0xeb58'246e'0e25'02c4ULL, 0x055d'6a02'1334'bc47ULL,
				0xa472'4211'1fa7'd7afULL, 0xe362'3fcc'84f7'8d97ULL, 0x81c7'44a1'1efc'6db9ULL,
				0xaec8'9615'39cf'b221ULL, 0xf316'0995'8d4e'8e31ULL, 0x63e5'923e'cc56'95ceULL,
				0x4710'7ddd'9b50'5a38ULL, 0xa3af'e7b5'a029'8135ULL, 0x792b'7063'e387'f3e6ULL,
				0x0140'e953'565d'75e0ULL, 0x12f4'f9ff'a503'e97bULL, 0x750c'e890'2c3c'b512ULL,
				0xdbc4'7e85'15f3'0733ULL, 0x1ed3'610c'6ab8'af8fULL, 0x5239'2186'81dd'e5d9ULL,
				0xe222'd69f'd2aa'f877ULL, 0xfe71'7835'14a8'bd25ULL, 0xcaf0'a18f'4a17'7175ULL,
				0x6165'5d98'60ec'7f13ULL, 0xe77f'bc9d'c19e'4430ULL, 0x2ccf'f441'ddd4'40a5ULL,
				0x16e9'7aae'e06a'20dcULL, 0xa855'dae2'd01c'915bULL, 0x1d13'47f9'905f'30b2ULL,
				0xb7c6'52bd'ecf9'4b34ULL, 0xd03e'43d2'65c6'175dULL, 0xfdb1'5ec0'ee4f'2218ULL,
				0x5764'4b84'92e9'599eULL, 0x07dd'a5a4'bf8e'569aULL, 0x54a4'6d71'680e'c6a3ULL,
				0x5624'a2d7'c4b4'2c7eULL, 0xbebc'a04c'3076'b187ULL, 0x7d36'f332'a6ee'3a41ULL,
				0x3b66'67bc'6be3'1599ULL, 0x695f'463a'ea3e'f040ULL, 0xad08'b0e0'c328'2d1cULL,
				0xb15b'1e4a'052a'684eULL, 0x44d0'5b28'61b7'c505ULL, 0x1529'5c5b'1a8d'bfe1ULL,
				0x744c'01c3'7a61'c0f2ULL, 0x59c3'1cd1'f1e8'f5b7ULL, 0xef45'a73f'4b4c'cb63ULL,
				0x6bdf'899c'4684'1a9dULL, 0x3dfb'2b4b'8230'36e3ULL, 0xa2ef'0ee6'f674'f4d5ULL,
				0x184e'2dfb'836b'8cf5ULL, 0x1134'df0a'5fe4'7646ULL, 0xbaa1'231d'751f'7820ULL,
				0xd17e'aa81'339b'62bdULL, 0xb01b'f719'5377'1daeULL, 0x849a'2ea3'0dc8'd1feULL,
				0x7051'8292'3f08'0955ULL, 0x0ea7'5755'6301'ac29ULL, 0x041d'8351'4569'c9a7ULL,
				0x0aba'd404'2668'658eULL, 0x49b7'2a88'f851'f611ULL, 0x8a3d'79f6'6ec9'7dd7ULL,
				0xcd2d'042b'f599'27efULL, 0xc930'877a'b0f0'ee48ULL, 0x9273'540d'eda2'f122ULL,
				0xc797'd02f'd3f1'4261ULL, 0xe1e2'f06a'284d'674aULL, 0xd2be'8c74'c97c'fd80ULL,
				0x9a49'4faf'6770'7e71ULL, 0xb3db'd1ec'a990'8293ULL, 0x72d1'4d34'93b2'e388ULL,
				0xd6a3'0f25'8c15'3427ULL}};
		return Ax[a][b];
	} else {
		static const unsigned long long Ax[8][256] = {
			{0xe6f8'7e5c'5b71'1fd0ULL, 0x2583'7780'0924'fa16ULL, 0xc849'e07e'852e'a4a8ULL,
				0x5b46'86a1'8f06'c16aULL, 0x0b32'e9a2'd77b'416eULL, 0xabda'37a4'6781'5c66ULL,
				0xf617'96a8'1a68'6676ULL, 0xf5dc'0b70'6391'954bULL, 0x4862'f38d'b7e6'4bf1ULL,
				0xff5c'629a'68bd'85c5ULL, 0xcb82'7da6'fcd7'5795ULL, 0x66d3'6daf'69b9'f089ULL,
				0x356c'9f74'483d'83b0ULL, 0x7cbc'ecb1'238c'99a1ULL, 0x36a7'02ac'31c4'708dULL,
				0x9eb6'a8d0'2fbc'dfd6ULL, 0x8b19'fa51'e5b3'ae37ULL, 0x9ccf'b540'8a12'7d0bULL,
				0xbc0c'78b5'0820'8f5aULL, 0xe533'e384'2288'ecedULL, 0xcec2'c7d3'77c1'5fd2ULL,
				0xec78'17b6'505d'0f5eULL, 0xb94c'c2c0'8336'871dULL, 0x8c20'5db4'cb0b'04adULL,
				0x763c'855b'28a0'892fULL, 0x588d'1b79'f6ff'3257ULL, 0x3fec'f69e'4311'933eULL,
				0x0fc0'd39f'803a'18c9ULL, 0xee01'0a26'f5f3'ad83ULL, 0x10ef'e8f4'4119'79a6ULL,
				0x5dcd'a10c'7de9'3a10ULL, 0x4a1b'ee1d'1248'e92cULL, 0x53bf'f2db'2184'7339ULL,
				0xb4f5'0ccf'a6a2'3d09ULL, 0x5fb4'bc9c'd847'98cdULL, 0xe88a'2d8b'071c'56f9ULL,
				0x7f77'7169'5a75'6a9cULL, 0xc5f0'2e71'a0ba'1ebcULL, 0xa663'f9ab'4215'e672ULL,
				0x2eb1'9e22'de5f'bb78ULL, 0x0db9'ce0f'2594'ba14ULL, 0x8252'0e63'9766'4d84ULL,
				0x2f03'1e6a'0208'ea98ULL, 0x5c7f'2144'a1be'6bf0ULL, 0x7a37'cb1c'd163'62dbULL,
				0x83e0'8e2b'4b31'1c64ULL, 0xcf70'479b'ab96'0e32ULL, 0x856b'a986'b9de'e71eULL,
				0xb547'8c87'7af5'6ce9ULL, 0xb8fe'4288'5f61'd6fdULL, 0x1bdd'0156'9662'38c8ULL,
				0x6221'5792'3ef8'a92eULL, 0xfc97'ff42'1144'76f8ULL, 0x9d7d'3508'5645'2cebULL,
				0x4c90'c9b0'e0a7'1256ULL, 0x2308'502d'fbcb'016cULL, 0x2d7a'03fa'a7a6'4845ULL,
				0xf46e'8b38'bfc6'c4abULL, 0xbdbe'f8fd'd477'debaULL, 0x3aac'4ceb'c807'9b79ULL,
				0xf09c'b105'e887'9d0cULL, 0x27fa'6a10'ac8a'58cbULL, 0x8960'e7c1'401d'0ceaULL,
				0x1a6f'811e'4a35'6928ULL, 0x90c4'fb07'73d1'96ffULL, 0x4350'1a2f'609d'0a9fULL,
				0xf7a5'16e0'c63f'3796ULL, 0x1ce4'a6b3'b8da'9252ULL, 0x1324'752c'38e0'8a9bULL,
				0xa5a8'6473'3bec'154fULL, 0x2bf1'2457'5549'b33fULL, 0xd766'db15'440d'c5c7ULL,
				0xa7d1'79e3'9e42'b792ULL, 0xdadf'151a'6199'7fd3ULL, 0x86a0'345e'c027'1423ULL,
				0x38d5'517b'6da9'39a4ULL, 0x6518'f077'1040'03b4ULL, 0x0279'1d90'a5ae'a2ddULL,
				0x88d2'6789'9c4a'5d0aULL, 0x930f'66df'0a28'65c2ULL, 0x4ee9'd420'4509'b08bULL,
				0x3255'3891'6685'292aULL, 0x4129'07bf'c533'a842ULL, 0xb27e'2b62'544d'c673ULL,
				0x6c53'0445'6295'e007ULL, 0x5af4'06e9'5351'908aULL, 0x1f2f'3b6b'c123'616fULL,
				0xc37b'09dc'5255'e5c6ULL, 0x3967'd133'b1fe'6844ULL, 0x2988'39c7'f0e7'11e2ULL,
				0x409b'87f7'1964'f9a2ULL, 0xe938'adc3'db4b'0719ULL, 0x0c0b'4e47'f9c3'ebf4ULL,
				0x5534'd576'd36b'8843ULL, 0x4610'a05a'eb8b'02d8ULL, 0x20c3'cdf5'8232'f251ULL,
				0x6de1'840d'bec2'b1e7ULL, 0xa0e8'de06'b0fa'1d08ULL, 0x7b85'4b54'0d34'333bULL,
				0x42e2'9a67'bcca'5b7fULL, 0xd8a6'088a'c437'dd0eULL, 0xc63b'b3a9'd943'ed81ULL,
				0x2171'4dbd'5e65'a3b1ULL, 0x6761'ede7'b5ee'a169ULL, 0x2431'f7c8'd573'abf6ULL,
				0xd51f'c685'e1a3'671aULL, 0x5e06'3cd4'0410'c92dULL, 0x283a'b98f'2cb0'4002ULL,
				0x8feb'c06c'b2f2'f790ULL, 0x17d6'4f11'6fa1'd33cULL, 0xe073'59f1'a99e'e4aaULL,
				0x784e'd68c'74cd'c006ULL, 0x6e2a'19d5'c73b'42daULL, 0x8712'b416'1c70'45c3ULL,
				0x3715'82e4'ed93'216dULL, 0xace3'9041'4939'f6fcULL, 0x7ec5'f121'8622'3b7cULL,
				0xc0b0'9404'2bac'16fbULL, 0xf9d7'4537'9a52'7ebfULL, 0x737c'3f2e'a3b6'8168ULL,
				0x33e7'b8d9'bad2'78caULL, 0xa9a3'2a34'c22f'febbULL, 0xe481'63cc'fedf'bd0dULL,
				0x8e59'4024'6ea5'a670ULL, 0x51c6'ef4b'842a'd1e4ULL, 0x22ba'd065'279c'508cULL,
				0xd914'88c2'1860'8ceeULL, 0x319e'a549'1f7c'da17ULL, 0xd394'e128'134c'9c60ULL,
				0x094b'f432'72d5'e3b3ULL, 0x9bf6'12a5'a4aa'd791ULL, 0xccbb'da43'd26f'fd0fULL,
				0x34de'1f3c'946a'd250ULL, 0x4f5b'5468'995e'e16bULL, 0xdf9f'af6f'ea8f'7794ULL,
				0x2648'ea58'70dd'092bULL, 0xbfc7'e56d'71d9'7c67ULL, 0xdde6'b2ff'4f21'd549ULL,
				0x3c27'6b46'3ae8'6003ULL, 0x9176'7b4f'af86'c71fULL, 0x68a1'3e78'35d4'b9a0ULL,
				0xb68c'115f'030c'9fd4ULL, 0x141d'd2c9'1658'2001ULL, 0x983d'8f7d'dd53'24acULL,
				0x64aa'703f'cc17'5254ULL, 0xc2c9'8994'8e02'b426ULL, 0x3e5e'76d6'9f46'c2deULL,
				0x5074'6f03'587d'8004ULL, 0x45db'3d82'9272'f1e5ULL, 0x6058'4a02'9b56'0bf3ULL,
				0xfbae'58a7'3ffc'dc62ULL, 0xa15a'5e4e'6cad'4ce8ULL, 0x4ba9'6e55'ce1f'b8ccULL,
				0x08f9'747a'ae82'b253ULL, 0xc102'144c'f7fb'471bULL, 0x9f04'2898'f3eb'8e36ULL,
				0x068b'27ad'f2ef'fb7aULL, 0xedca'97fe'8c0a'5ebeULL, 0x778e'0513'f4f7'd8cfULL,
				0x302c'2501'c32b'8bf7ULL, 0x8d92'ddfc'175c'554dULL, 0xf865'c57f'4605'2f5fULL,
				0xeaf3'301b'a2b2'f424ULL, 0xaa68'b7ec'bbd6'0d86ULL, 0x998f'0f35'0104'754cULL,
				0x0000'0000'0000'0000ULL, 0xf12e'314d'34d0'ccecULL, 0x7105'22be'0618'23b5ULL,
				0xaf28'0d99'30c0'05c1ULL, 0x97fd'5ce2'5d69'3c65ULL, 0x19a4'1cc6'33cc'9a15ULL,
				0x9584'4172'f8c7'9eb8ULL, 0xdc54'32b7'9376'84a9ULL, 0x9436'c13a'2490'cf58ULL,
				0x802b'13f3'32c8'ef59ULL, 0xc442'ae39'7ced'4f5cULL, 0xfa1c'd8ef'e3ab'8d82ULL,
				0xf2e5'ac95'4d29'3fd1ULL, 0x6ad8'23e8'907a'1b7dULL, 0x4d22'49f8'3cf0'43b6ULL,
				0x03cb'9dd8'79f9'f33dULL, 0xde2d'2f27'36d8'2674ULL, 0x2a43'a41f'891e'e2dfULL,
				0x6f98'999d'1b6c'133aULL, 0xd4ad'46cd'3df4'36faULL, 0xbb35'df50'2698'25c0ULL,
				0x964f'dcaa'813e'6d85ULL, 0xeb41'b053'7ee5'a5c4ULL, 0x0540'ba75'8b16'0847ULL,
				0xa41a'e43b'e7bb'44afULL, 0xe3b8'c429'd067'1797ULL, 0x8199'93bb'ee9f'beb9ULL,
				0xae9a'8dd1'ec97'5421ULL, 0xf357'2cdd'917e'6e31ULL, 0x6393'd7da'e2af'f8ceULL,
				0x47a2'2012'37dc'5338ULL, 0xa323'43de'c903'ee35ULL, 0x79fc'56c4'a89a'91e6ULL,
				0x01b2'8048'dc57'51e0ULL, 0x1296'f564'e4b7'db7bULL, 0x75f7'1883'5159'7a12ULL,
				0xdb6d'9552'bdce'2e33ULL, 0x1e9d'bb23'1d74'308fULL, 0x520d'7293'fdd3'22d9ULL,
				0xe20a'4461'0c30'4677ULL, 0xfeee'e2d2'b4ea'd425ULL, 0xca30'fdee'2080'0675ULL,
				0x61ea'ca4a'4701'5a13ULL, 0xe74a'fe14'8726'4e30ULL, 0x2cc8'83b2'7bf1'19a5ULL,
				0x1664'cf59'b3f6'82dcULL, 0xa811'aa7c'1e78'af5bULL, 0x1d56'26fb'648d'c3b2ULL,
				0xb73e'9117'df5b'ce34ULL, 0xd05f'7cf0'6ab5'6f5dULL, 0xfd25'7f0a'cd13'2718ULL,
				0x574d'c8e6'76c5'2a9eULL, 0x0739'a7e5'2eb8'aa9aULL, 0x5486'553e'0f3c'd9a3ULL,
				0x56ff'48ae'aa92'7b7eULL, 0xbe75'6525'ad8e'2d87ULL, 0x7d0e'6cf9'ffdb'c841ULL,
				0x3b1e'cca3'1450'ca99ULL, 0x6913'be30'e983'e840ULL, 0xad51'1009'956e'a71cULL,
				0xb1b5'b6ba'2db4'354eULL, 0x4469'bdca'4e25'a005ULL, 0x15af'5281'ca0f'71e1ULL,
				0x7445'98cb'8d0e'2bf2ULL, 0x593f'9b31'2aa8'63b7ULL, 0xefb3'8a6e'29a4'fc63ULL,
				0x6b6a'a3a0'4c2d'4a9dULL, 0x3d95'eb0e'e6bf'31e3ULL, 0xa291'c396'1554'bfd5ULL,
				0x1816'9c8e'ef9b'cbf5ULL, 0x115d'68bc'9d4e'2846ULL, 0xba87'5f18'facf'7420ULL,
				0xd1ed'fcb8'b6e2'3ebdULL, 0xb007'36f2'f1e3'64aeULL, 0x84d9'29ce'6589'b6feULL,
				0x70b7'a2f6'da4f'7255ULL, 0x0e72'53d7'5c6d'4929ULL, 0x04f2'3a3d'5741'59a7ULL,
				0x0a80'69ea'0b2c'108eULL, 0x49d0'73c5'6bb1'1a11ULL, 0x8aab'7a19'39e4'ffd7ULL,
				0xcd09'5a0b'0e38'acefULL, 0xc9fb'6036'5979'f548ULL, 0x92bd'e697'd67f'3422ULL,
				0xc789'33e1'0514'bc61ULL, 0xe1c1'd9b9'75c9'b54aULL, 0xd226'6160'cf1b'cd80ULL,
				0x9a44'92ed'78fd'8671ULL, 0xb3cc'ab2a'881a'9793ULL, 0x72ce'bf66'7fe1'd088ULL,
				0xd6d4'5b5d'985a'9427ULL},
			{0xc811'a805'8c3f'55deULL, 0x65f5'b431'96b5'0619ULL, 0xf74f'96b1'd670'6e43ULL,
				0x859d'1e8b'cb43'd336ULL, 0x5aab'8a85'ccfa'3d84ULL, 0xf9c7'bf99'c295'fcfdULL,
				0xa21f'd5a1'de4b'630fULL, 0xcdb3'ef76'3b8b'456dULL, 0x803f'59f8'7cf7'c385ULL,
				0xb27c'73be'5f31'913cULL, 0x98e3'ac66'33b0'4821ULL, 0xbf61'674c'26b8'f818ULL,
				0x0ffb'c995'c4c1'30c8ULL, 0xaaa0'8620'1076'1a98ULL, 0x6057'f342'2101'16aaULL,
				0xf63c'760c'0654'cc35ULL, 0x2ddb'45cc'667d'9042ULL, 0xbcf4'5a96'4bd4'0382ULL,
				0x68e8'a0c3'ef3c'6f3dULL, 0xa7bd'92d2'69ff'73bcULL, 0x290a'e202'01ed'2287ULL,
				0xb7de'34cd'e885'818fULL, 0xd901'eea7'dd61'059bULL, 0xd6fa'2732'19a0'3553ULL,
				0xd56f'1ae8'74cc'cec9ULL, 0xea31'245c'2e83'f554ULL, 0x7034'555d'a07b'e499ULL,
				0xce26'd2ac'56e7'bef7ULL, 0xfd16'1857'a505'4e38ULL, 0x6a0e'7da4'5274'36d1ULL,
				0x5bd8'6a38'1cde'9ff2ULL, 0xcaf7'7562'3177'0c32ULL, 0xb09a'aed9'e279'c8d0ULL,
				0x5def'1091'c606'74dbULL, 0x1110'46a2'515e'5045ULL, 0x2353'6ce4'7298'02fcULL,
				0xc50c'bcf7'f5b6'3cfaULL, 0x73a1'6887'cd17'1f03ULL, 0x7d29'41af'd9f2'8dbdULL,
				0x3f5e'3eb4'5a4f'3b9dULL, 0x84ee'fe36'1b67'7140ULL, 0x3db8'e3d3'e707'6271ULL,
				0x1a3a'28f9'f20f'd248ULL, 0x7ebc'7c75'b49e'7627ULL, 0x74e5'f293'c7eb'565cULL,
				0x18dc'f59e'4f47'8ba4ULL, 0x0c6e'f44f'a9ad'cb52ULL, 0xc699'812d'98da'c760ULL,
				0x788b'06dc'6e46'9d0eULL, 0xfc65'f8ea'7521'ec4eULL, 0x30a5'f721'9e8e'0b55ULL,
				0x2bec'3f65'bca5'7b6bULL, 0xddd0'4969'baf1'b75eULL, 0x9990'4cdb'e394'ea57ULL,
				0x14b2'01d1'e6ea'40f6ULL, 0xbbb0'c082'4128'4addULL, 0x50f2'0463'bf8f'1dffULL,
				0xe8d7'f93b'93cb'acb8ULL, 0x4d8c'b68e'477c'86e8ULL, 0xc1dd'1b39'9226'8e3fULL,
				0x7c5a'a112'09d6'2fcbULL, 0x2f3d'98ab'db35'c9aeULL, 0x6713'6956'2bfd'5ff5ULL,
				0x15c1'e16c'36ce'e280ULL, 0x1d7e'b2ed'f8f3'9b17ULL, 0xda94'd37d'b00d'fe01ULL,
				0x877b'c3ec'760b'8adaULL, 0xcb84'95df'e153'ae44ULL, 0x05a2'4773'b7b4'10b3ULL,
				0x1285'7b78'3c32'abdfULL, 0x8eb7'70d0'6812'513bULL, 0x5367'39b9'd2e3'e665ULL,
				0x584d'57e2'71b2'6468ULL, 0xd789'c78f'c984'9725ULL, 0xa935'bbfa'7d1a'e102ULL,
				0x8b15'37a3'dfa6'4188ULL, 0xd0cd'5d9b'c378'de7aULL, 0x4ac8'2c9a'4d80'cfb7ULL,
				0x4277'7f1b'83bd'b620ULL, 0x72d2'883a'1d33'bd75ULL, 0x5e7a'2d4b'ab6a'8f41ULL,
				0xf4da'ab6b'bb1c'95d9ULL, 0x905c'ffe7'fd8d'31b6ULL, 0x83aa'6422'119b'381fULL,
				0xc0ae'fb84'4202'2c49ULL, 0xa0f9'08c6'6303'3ae3ULL, 0xa428'af08'0493'8826ULL,
				0xade4'1c34'1a8a'53c7ULL, 0xae71'21ee'77e6'a85dULL, 0xc47f'5c4a'2592'9e8cULL,
				0xb538'e9aa'55cd'd863ULL, 0x0637'7aa9'dad8'eb29ULL, 0xa18a'e87b'b327'9895ULL,
				0x6edf'da6a'35e4'8414ULL, 0x6b7d'9d19'8250'94a7ULL, 0xd41c'fa55'a4e8'6cbfULL,
				0xe5ca'edc9'ea42'c59cULL, 0xa36c'351c'0e6f'c179ULL, 0x5181'e4de'6fab'bf89ULL,
				0xfff0'c530'184d'17d4ULL, 0x9d41'eb15'8404'5892ULL, 0x1c0d'5250'28d7'3961ULL,
				0xf178'ec18'0ca8'856aULL, 0x9a05'7101'8ef8'11cdULL, 0x4091'a27c'3ef5'efccULL,
				0x19af'1523'9f63'29d2ULL, 0x3474'50ef'f91e'b990ULL, 0xe11b'4a07'8dd2'7759ULL,
				0xb956'1de5'fc60'1331ULL, 0x912f'1f5a'2da9'93c0ULL, 0x1654'dcb6'5ba2'191aULL,
				0x3e2d'de09'8a6b'99ebULL, 0x8a66'd71e'0f82'e3feULL, 0x8c51'adb7'd55a'08d7ULL,
				0x4533'e50f'8941'ff7fULL, 0x02e6'dd67'bd48'59ecULL, 0xe068'aaba'5df6'd52fULL,
				0xc248'26e3'ff4a'75a5ULL, 0x6c39'070d'88ac'ddf8ULL, 0x6486'548c'4691'a46fULL,
				0xd1be'bd26'135c'7c0cULL, 0xb30f'9303'8f15'334aULL, 0x82d9'849f'c1bf'9a69ULL,
				0x9c32'0ba8'5420'fae4ULL, 0xfa52'8243'aff9'0767ULL, 0x9ed4'd6cf'e968'a308ULL,
				0xb825'fd58'2c44'b147ULL, 0x9b76'91bc'5edc'b3bbULL, 0xc7ea'6190'48fe'6516ULL,
				0x1063'a61f'817a'f233ULL, 0x47d5'3868'3409'a693ULL, 0x63c2'ce98'4c6d'ed30ULL,
				0x2a9f'dfd8'6c81'd91dULL, 0x7b1e'3b06'032a'6694ULL, 0x6660'89eb'fbd9'fd83ULL,
				0x0a59'8ee6'7375'207bULL, 0x0744'9a14'0afc'495fULL, 0x2ca8'a571'b659'3234ULL,
				0x1f98'6f8a'45bb'c2fbULL, 0x381a'a4a0'50b3'72c2ULL, 0x5423'a3ad'd81f'af3aULL,
				0x1727'3c0b'8b86'bb6cULL, 0xfe83'258d'c869'b5a2ULL, 0x2879'02bf'd1c9'80f1ULL,
				0xf5a9'4bd6'6b38'37afULL, 0x8880'0a79'b2ca'ba12ULL, 0x5550'4310'083b'0d4cULL,
				0xdf36'940e'07b9'eeb2ULL, 0x04d1'a7ce'6790'b2c5ULL, 0x6124'13ff'f125'b4dcULL,
				0x26f1'2b97'c52c'124fULL, 0x8608'2351'a62f'28acULL, 0xef93'632f'9937'e5e7ULL,
				0x3507'b052'293a'1be6ULL, 0xe72c'30ae'570a'9c70ULL, 0xd358'6041'ae14'25e0ULL,
				0xde45'74b3'd79d'4cc4ULL, 0x92ba'2280'40c5'685aULL, 0xf00b'0ca5'dc8c'271cULL,
				0xbe12'87f1'f69c'5a6eULL, 0xf39e'317f'b1e0'dc86ULL, 0x495d'1140'20ec'342dULL,
				0x699b'407e'3f18'cd4bULL, 0xdca3'a9d4'6ad5'1528ULL, 0x0d1d'14f2'7989'6924ULL,
				0x0000'0000'0000'0000ULL, 0x593e'b75f'a196'c61eULL, 0x2e4e'7816'0b11'6bd8ULL,
				0x6d4a'e7b0'5888'7f8eULL, 0xe65f'd013'872e'3e06ULL, 0x7a6d'dbbb'd30e'c4e2ULL,
				0xac97'fc89'caae'f1b1ULL, 0x09cc'b33c'1e19'dbe1ULL, 0x89f3'eac4'62ee'1864ULL,
				0x7770'cf49'aa87'adc6ULL, 0x56c5'7eca'6557'f6d6ULL, 0x0395'3dda'6d6c'fb9aULL,
				0x3692'8d88'4456'e07cULL, 0x1eeb'8f37'959f'608dULL, 0x31d6'179c'4eaa'a923ULL,
				0x6fac'3ad7'e5c0'2662ULL, 0x4304'9fa6'5399'1456ULL, 0xabd3'669d'c052'b8eeULL,
				0xaf02'c153'a7c2'0a2bULL, 0x3ccb'036e'3723'c007ULL, 0x93c9'c23d'90e1'ca2cULL,
				0xc33b'c65e'2f6e'd7d3ULL, 0x4cff'5633'9758'249eULL, 0xb1e9'4e64'325d'6aa6ULL,
				0x37e1'6d35'9472'420aULL, 0x79f8'e661'be62'3f78ULL, 0x5214'd904'02c7'4413ULL,
				0x482e'f1fd'f0c8'965bULL, 0x13f6'9bc5'ec16'09a9ULL, 0x0e88'2928'14e5'92beULL,
				0x4e19'8b54'2a10'7d72ULL, 0xccc0'0fcb'ebaf'e71bULL, 0x1b49'c844'222b'703eULL,
				0x2564'164d'a840'e9d5ULL, 0x20c6'513e'1ff4'f966ULL, 0xbac3'203f'910c'e8abULL,
				0xf2ed'd1c2'61c4'7ef0ULL, 0x814c'b945'acd3'61f3ULL, 0x95fe'b894'4a39'2105ULL,
				0x5c9c'f02c'1622'd6adULL, 0x9718'65f3'f771'78e9ULL, 0xbd87'ba2b'9bf0'a1f4ULL,
				0x4440'05b2'5965'5d09ULL, 0xed75'be48'247f'bc0bULL, 0x7596'122e'17cf'f42aULL,
				0xb44b'0917'85e9'7a15ULL, 0x966b'854e'2755'da9fULL, 0xeee0'8392'4913'4791ULL,
				0x3243'2a46'23c6'52b9ULL, 0xa846'5b47'ad3e'4374ULL, 0xf8b4'5f24'12b1'5e8bULL,
				0x2417'f6f0'7864'4ba3ULL, 0xfb21'62fe'7fdd'a511ULL, 0x4bbb'cc27'9da4'6dc1ULL,
				0x0173'e0bd'd024'a276ULL, 0x2220'8c59'a2bc'a08aULL, 0x8fc4'906d'b836'f34dULL,
				0xe4b9'0d74'3a66'67eaULL, 0x7147'b5e0'705f'46efULL, 0x2782'cb2a'1508'b039ULL,
				0xec06'5ef5'f45b'1e7dULL, 0x21b5'b183'cfd0'5b10ULL, 0xdbe7'33c0'6029'5c77ULL,
				0x9fa7'3672'394c'017eULL, 0xcf55'3211'86c3'1c81ULL, 0xd872'0e1a'0d45'a7edULL,
				0x3b8f'997a'3ddf'8958ULL, 0x3afc'79c7'edfb'2b2eULL, 0xe9a4'1986'43ef'0eceULL,
				0x5f09'cdf6'7b4e'2d37ULL, 0x4f6a'6be9'fa34'df04ULL, 0xb6ad'd470'38a1'23f9ULL,
				0x8d22'4d0a'057e'aaa1ULL, 0xc962'48b8'5c1b'f7a8ULL, 0xe3fd'9760'309a'2eb5ULL,
				0x0b2a'6e5b'a351'820dULL, 0xeb42'c4e1'fea7'5722ULL, 0x948d'5829'9a1d'8373ULL,
				0x7fcf'9cc8'64ba'd451ULL, 0xa55b'4fb5'd4b7'2a50ULL, 0x08bf'5381'ce3d'7997ULL,
				0x46a6'd8d5'e42d'04e5ULL, 0xd22b'80fc'7e30'8796ULL, 0x57b6'9e77'b573'54a0ULL,
				0x3969'441d'8097'd0b4ULL, 0x3330'cafb'f3e2'f0cfULL, 0xe28e'77dd'e0be'8cc3ULL,
				0x62b1'2e25'9c49'4f46ULL, 0xa6ce'726f'b9db'd1caULL, 0x41e2'42c1'eed1'4dbaULL,
				0x7603'2ff4'7aa3'0fb0ULL},
			{0x45b2'68a9'3acd'e4ccULL, 0xaf7f'0be8'8454'9d08ULL, 0x0483'54b3'c146'8263ULL,
				0x9254'35c2'c80e'fed2ULL, 0xee4e'37f2'7fdf'fba7ULL, 0x167a'3392'0c60'f14dULL,
				0xfb12'3b52'ea03'e584ULL, 0x4a0c'ab53'fdbb'9007ULL, 0x9dea'f638'0f78'8a19ULL,
				0xcb48'ec55'8f0c'b32aULL, 0xb59d'c4b2'd6fe'f7e0ULL, 0xdcdb'ca22'f4f3'ecb6ULL,
				0x11df'5813'549a'9c40ULL, 0xe33f'dedf'568a'ced3ULL, 0xa0c1'c812'4322'e9c3ULL,
				0x07a5'6b81'58fa'6d0dULL, 0x7727'9579'b1e1'f3ddULL, 0xd9b1'8b74'422a'c004ULL,
				0xb8ec'2d9f'ffab'c294ULL, 0xf4ac'f8a8'2d75'914fULL, 0x7bbf'69b1'ef2b'6878ULL,
				0xc4f6'2faf'487a'c7e1ULL, 0x76ce'809c'c67e'5d0cULL, 0x6711'd88f'92e4'c14cULL,
				0x627b'99d9'243d'edfeULL, 0x234a'a5c3'dfb6'8b51ULL, 0x909b'1f15'262d'bf6dULL,
				0x4f66'ea05'4b62'bcb5ULL, 0x1ae2'cf5a'52aa'6ae8ULL, 0xbea0'53fb'd0ce'0148ULL,
				0xed68'08c0'e663'14c9ULL, 0x43fe'16cd'15a8'2710ULL, 0xcd04'9231'a069'70f6ULL,
				0xe7bc'8a6c'97cc'4cb0ULL, 0x337c'e835'fcb3'b9c0ULL, 0x65de'f258'7cc7'80f3ULL,
				0x5221'4ede'4132'bb50ULL, 0x95f1'5e43'90f4'93dfULL, 0x8708'3962'5dd2'e0f1ULL,
				0x4131'3c1a'fb8b'66afULL, 0x9172'0af0'51b2'11bcULL, 0x477d'427e'd4ee'a573ULL,
				0x2e3b'4cee'f6e3'be25ULL, 0x8262'7834'eb0b'cc43ULL, 0x9c03'e3dd'78e7'24c8ULL,
				0x2877'328a'd986'7df9ULL, 0x14b5'1945'e243'b0f2ULL, 0x574b'0f88'f7eb'97e2ULL,
				0x88b6'fa98'9aa4'943aULL, 0x19c4'f068'cb16'8586ULL, 0x50ee'6409'af11'faefULL,
				0x7df3'17d5'c04e'aba4ULL, 0x7a56'7c54'98b4'c6a9ULL, 0xb6bb'fb80'4f42'188eULL,
				0x3cc2'2bcf'3bc5'cd0bULL, 0xd043'36ea'aa39'7713ULL, 0xf02f'ac1b'ec33'132cULL,
				0x2506'dba7'f0d3'488dULL, 0xd7e6'5d6b'f2c3'1a1eULL, 0x5eb9'b216'1ff8'20f5ULL,
				0x842e'0650'c46e'0f9fULL, 0x716b'eb1d'9e84'3001ULL, 0xa933'758c'ab31'5ed4ULL,
				0x3fe4'14fd'a279'2265ULL, 0x27c9'f170'1ef0'0932ULL, 0x73a4'c1ca'70a7'71beULL,
				0x9418'4ba6'e76b'3d0eULL, 0x40d8'29ff'8c14'c87eULL, 0x0fbe'c3fa'c776'74cbULL,
				0x3616'a963'4a6a'9572ULL, 0x8f13'9119'c25e'f937ULL, 0xf545'ed4d'5aea'3f9eULL,
				0xe802'4996'50ba'387bULL, 0x6437'e7bd'0b58'2e22ULL, 0xe655'9f89'e053'e261ULL,
				0x80ad'52e3'0528'8dfcULL, 0x6dc5'5a23'e34b'9935ULL, 0xde14'e0f5'1ad0'ad09ULL,
				0xc639'0578'a659'865eULL, 0x96d7'6171'0948'7cb1ULL, 0xe2d6'cb3a'2115'6002ULL,
				0x01e9'15e5'779f'aed1ULL, 0xadb0'213f'6a77'dcb7ULL, 0x9880'b76e'b9a1'a6abULL,
				0x5d9f'8d24'8644'cf9bULL, 0xfd5e'4536'c566'2658ULL, 0xf1c6'b9fe'9bac'bdfdULL,
				0xeacd'6341'be99'79c4ULL, 0xefa7'2217'0840'5576ULL, 0x5107'71ec'd88e'543eULL,
				0xc2ba'51cb'671f'043dULL, 0x0ad4'82ac'71af'5879ULL, 0xfe78'7a04'5cda'c936ULL,
				0xb238'af33'8e04'9aedULL, 0xbd86'6cc9'4972'ee26ULL, 0x615d'a6eb'bd81'0290ULL,
				0x3295'fdd0'8b2c'1711ULL, 0xf834'0460'73bf'0aeaULL, 0xf309'9329'758f'fc42ULL,
				0x1cae'b13e'7dcf'a934ULL, 0xba23'0748'1188'832bULL, 0x24ef'ce42'874c'e65cULL,
				0x0e57'd61f'b0e9'da1aULL, 0xb3d1'bad6'f99b'343cULL, 0xc075'7b1c'893c'4582ULL,
				0x2b51'0db8'403a'9297ULL, 0x5c76'98c1'f1db'614aULL, 0x3e0d'0118'd5e6'8cb4ULL,
				0xd60f'488e'855c'b4cfULL, 0xae96'1e0d'f3cb'33d9ULL, 0x3a8e'55ab'14a0'0ed7ULL,
				0x4217'0328'6237'89c1ULL, 0x838b'6dd1'9c94'6292ULL, 0x895f'ef7d'ed3b'3aebULL,
				0xcfcb'b8e6'4e4a'3149ULL, 0x064c'7e64'2f65'c3dcULL, 0x3d2b'3e2a'4c5a'63daULL,
				0x5bd3'f340'a921'0c47ULL, 0xb474'd157'a161'5931ULL, 0xac59'34da'1de8'7266ULL,
				0x6ee3'6511'7af7'765bULL, 0xc86e'd367'16b0'5c44ULL, 0x9ba6'885c'201d'49c5ULL,
				0xb905'387a'8834'6c45ULL, 0x1310'72c4'bab9'ddffULL, 0xbf49'461e'a751'af99ULL,
				0xd529'77bc'1ce0'5ba1ULL, 0xb0f7'85e4'6027'db52ULL, 0x546d'30ba'6e57'788cULL,
				0x305a'd707'650f'56aeULL, 0xc987'c682'612f'f295ULL, 0xa5ab'8944'f5fb'c571ULL,
				0x7ed5'28e7'59f2'44caULL, 0x8ddc'bbce'2c7d'b888ULL, 0xaa15'4abe'328d'b1baULL,
				0x1e61'9be9'93ec'e88bULL, 0x09f2'bd9e'e813'b717ULL, 0x7401'aa4b'285d'1cb3ULL,
				0x2185'8f14'3195'caeeULL, 0x48c3'8184'1398'd1b8ULL, 0xfcb7'50d3'b2f9'8889ULL,
				0x39a8'6a99'8d1c'e1b9ULL, 0x1f88'8e0c'e473'465aULL, 0x7899'5683'7697'8716ULL,
				0x02cf'2ad7'ee23'41bfULL, 0x85c7'13b5'b3f1'a14eULL, 0xff91'6fe1'2b45'67e7ULL,
				0x7c1a'0230'b7d1'0575ULL, 0x0c98'fcc8'5eca'9ba5ULL, 0xa3e7'f720'da9e'06adULL,
				0x6a60'31a2'bbb1'f438ULL, 0x973e'7494'7ed7'd260ULL, 0x2cf4'6639'18c0'ff9aULL,
				0x5f50'a7f3'6867'8e24ULL, 0x34d9'83b4'a449'd4cdULL, 0x68af'1b75'5592'b587ULL,
				0x7f3c'3d02'2e6d'ea1bULL, 0xabfc'5f5b'4512'1f6bULL, 0x0d71'e92d'2955'3574ULL,
				0xdffd'f510'6d4f'03d8ULL, 0x081b'a87b'9f8c'19c6ULL, 0xdb7e'a1a3'ac09'81bbULL,
				0xbbca'12ad'6617'2dfaULL, 0x7970'4366'0108'29c7ULL, 0x1793'2677'7bff'5f9cULL,
				0x0000'0000'0000'0000ULL, 0xeb24'76a4'c906'd715ULL, 0x724d'd42f'0738'df6fULL,
				0xb752'ee65'38dd'b65fULL, 0x37ff'bc86'3df5'3ba3ULL, 0x8efa'84fc'b5c1'57e6ULL,
				0xe9eb'5c73'2725'96aaULL, 0x1b0b'dabf'2535'c439ULL, 0x86e1'2c87'2a4d'4e20ULL,
				0x9969'a28b'ce3e'087aULL, 0xfafb'2eb7'9d9c'4b55ULL, 0x056a'4156'b6d9'2cb2ULL,
				0x5a3a'e6a5'debe'a296ULL, 0x22a3'b026'a829'2580ULL, 0x53c8'5b3b'36ad'1581ULL,
				0xb11e'9001'17b8'7583ULL, 0xc51f'3a4a'3fe5'6930ULL, 0xe019'e1ed'cf36'21bdULL,
				0xec81'1d25'91fc'ba18ULL, 0x445b'7d4c'4d52'4a1dULL, 0xa8da'6069'dcae'f005ULL,
				0x58f5'cc72'309d'e329ULL, 0xd4c0'6259'6b7f'f570ULL, 0xce22'ad03'39d5'9f98ULL,
				0x591c'd997'4702'4df8ULL, 0x8b90'c5aa'0318'7b54ULL, 0xf663'd27f'c356'd0f0ULL,
				0xd858'9e91'35b5'6ed5ULL, 0x3530'9651'd3d6'7a1cULL, 0x12f9'6721'cd26'732eULL,
				0xd28c'1c3d'441a'36acULL, 0x492a'9461'6407'7f69ULL, 0x2d1d'73dc'6f5f'514bULL,
				0x6f0a'70f4'0d68'd88aULL, 0x60b4'b30e'ca1e'ac41ULL, 0xd365'09d8'3385'987dULL,
				0x0b3d'9749'0630'f6a8ULL, 0x9ecc'c90a'96c4'6577ULL, 0xa20e'e2c5'ad01'a87cULL,
				0xe49a'b55e'0e70'a3deULL, 0xa442'9ca1'8264'6ba0ULL, 0xda97'b446'db96'2f6aULL,
				0xcced'87d4'd7f6'de27ULL, 0x2ab8'185d'37a5'3c46ULL, 0x9f25'dcef'e15b'cba6ULL,
				0xc19c'6ef9'fea3'eb53ULL, 0xa764'a393'1bd8'84ceULL, 0x2fd2'590b'817c'10f4ULL,
				0x56a2'1a6d'8074'3933ULL, 0xe573'a0bb'79ef'0d0fULL, 0x155c'0ca0'95dc'1e23ULL,
				0x6c2c'4fc6'94d4'37e4ULL, 0x1036'4df6'2305'3291ULL, 0xdd32'dfc7'836c'4267ULL,
				0x0326'3f32'99bc'ef6eULL, 0x66f8'cd6a'e57b'6f9dULL, 0x8c35'ae2b'5be2'1659ULL,
				0x31b3'c2e2'1290'f87fULL, 0x93bd'2027'bf91'5003ULL, 0x6946'0e90'220d'1b56ULL,
				0x299e'276f'ae19'd328ULL, 0x6392'8c3c'53a2'432fULL, 0x7082'fef8'e91b'9ed0ULL,
				0xbc6f'792c'3eed'40f7ULL, 0x4c40'd537'd2de'53dbULL, 0x75e8'bfae'5fc2'b262ULL,
				0x4da9'c0d2'a541'fd0aULL, 0x4e8f'ffe0'3cfd'1264ULL, 0x2620'e495'696f'a7e3ULL,
				0xe1f0'f408'b8a9'8f6cULL, 0xd1aa'230f'dda6'd9c2ULL, 0xc7d0'109d'd1c6'288fULL,
				0x8a79'd04f'7487'd585ULL, 0x4694'579b'a371'0ba2ULL, 0x3841'7f7c'fa83'4f68ULL,
				0x1d47'a4db'0a50'07e5ULL, 0x206c'9af1'460a'643fULL, 0xa128'ddf7'34bd'4712ULL,
				0x8144'4706'72b7'232dULL, 0xf2e0'86cc'0210'5293ULL, 0x182d'e58d'bc89'2b57ULL,
				0xcaa1'f9b0'f893'1dfbULL, 0x6b89'2447'cc2e'5ae9ULL, 0xf9dd'1185'0420'a43bULL,
				0x4be5'beb6'8a24'3ed6ULL, 0x5584'255f'19c8'd65dULL, 0x3b67'404e'633f'a006ULL,
				0xa68d'b676'6c47'2a1fULL, 0xf78a'c79a'b4c9'7e21ULL, 0xc353'442e'1080'aaecULL,
				0x9a4f'9db9'5782'e714ULL},
			{0x05ba'7bc8'2c9b'3220ULL, 0x31a5'4665'f8b6'5e4fULL, 0xb1b6'51f7'7547'f4d4ULL,
				0x8bfa'0d85'7ba4'6682ULL, 0x85a9'6c5a'a16a'98bbULL, 0x990f'aef9'08eb'79c9ULL,
				0xa15e'37a2'47f4'a62dULL, 0x7685'7dcd'5d27'741eULL, 0xf8c5'0b80'0a18'20bcULL,
				0xbe65'dcb2'01f7'a2b4ULL, 0x666d'1b98'6f94'26e7ULL, 0x4cc9'21bf'53c4'e648ULL,
				0x9541'0a0f'93d9'ca42ULL, 0x20cd'ccaa'647b'a4efULL, 0x429a'4060'890a'1871ULL,
				0x0c4e'a4f6'9b32'b38bULL, 0xccda'362d'de35'4cd3ULL, 0x96dc'23bc'7c5b'2fa9ULL,
				0xc309'bb68'aa85'1ab3ULL, 0xd261'31a7'3648'e013ULL, 0x021d'c529'41fc'4db2ULL,
				0xcd5a'dab7'704b'e48aULL, 0xa779'65d9'84ed'71e6ULL, 0x3238'6fd6'1734'bba4ULL,
				0xe82d'6dd5'38ab'7245ULL, 0x5c21'47ea'6177'b4b1ULL, 0x5da1'ab70'cf09'1ce8ULL,
				0xac90'7fce'72b8'bdffULL, 0x57c8'5dfd'9722'78a8ULL, 0xa4e4'4c6a'6b6f'940dULL,
				0x3851'995b'4f1f'dfe4ULL, 0x6257'8cca'ed71'bc9eULL, 0xd988'2bb0'c01d'2c0aULL,
				0x917b'9d5d'113c'503bULL, 0xa2c3'1e11'a876'43c6ULL, 0xe463'c923'a399'c1ceULL,
				0xf716'86c5'7ea8'76dcULL, 0x87b4'a973'e096'd509ULL, 0xaf0d'567d'9d3a'5814ULL,
				0xb40c'2a3f'59dc'c6f4ULL, 0x3602'f884'95d1'21ddULL, 0xd3e1'dd3d'9836'484aULL,
				0xf945'e71a'a466'88e5ULL, 0x7518'547e'b2a5'91f5ULL, 0x9366'5874'50c0'1d89ULL,
				0x9ea8'1018'658c'065bULL, 0x4f54'080c'bc46'03a3ULL, 0x2d03'84c6'5137'bf3dULL,
				0xdc32'5078'ec86'1e2aULL, 0xea30'a8fc'7957'3ff7ULL, 0x214d'2030'ca05'0cb6ULL,
				0x65f0'322b'8016'c30cULL, 0x69be'96dd'1b24'7087ULL, 0xdb95'ee99'81e1'61b8ULL,
				0xd1fc'1814'd9ca'05f8ULL, 0x820e'd2bb'cc0d'e729ULL, 0x63d7'6050'430f'14c7ULL,
				0x3bcc'b0e8'a09d'3a0fULL, 0x8e40'764d'573f'54a2ULL, 0x39d1'75c1'e161'77bdULL,
				0x12f5'a37c'734f'1f4bULL, 0xab37'c12f'1fdf'c26dULL, 0x5648'b167'395c'd0f1ULL,
				0x6c04'ed15'37bf'42a7ULL, 0xed97'161d'1430'4065ULL, 0x7d6c'67da'ab72'b807ULL,
				0xec17'fa87'ba4e'e83cULL, 0xdfaf'79cb'0304'fbc1ULL, 0x733f'0605'71bc'463eULL,
				0x78d6'1c12'87e9'8a27ULL, 0xd07c'f48e'77b4'ada1ULL, 0xb9c2'6253'6c90'dd26ULL,
				0xe244'9b58'6080'1605ULL, 0x8fc0'9ad7'f941'fcfbULL, 0xfad8'cea9'4be4'6d0eULL,
				0xa343'f28b'0608'eb9fULL, 0x9b12'6bd0'4917'347bULL, 0x9a92'874a'e769'9c22ULL,
				0x1b01'7c42'c4e6'9ee0ULL, 0x3a4c'5c72'0ee3'9256ULL, 0x4b6e'9f5e'3ea3'99daULL,
				0x6ba3'53f4'5ad8'3d35ULL, 0xe7fe'e090'4c1b'2425ULL, 0x22d0'0983'2587'e95dULL,
				0x8429'80c0'0f14'30e2ULL, 0xc6b3'c0a0'861e'2893ULL, 0x0874'33a4'19d7'29f2ULL,
				0x341f'3dad'd42d'6c6fULL, 0xee0a'3fae'fbb2'a58eULL, 0x4aee'73c4'90dd'3183ULL,
				0xaab7'2db5'b1a1'6a34ULL, 0xa92a'0406'5e23'8fdfULL, 0x7b4b'35a1'686b'6fccULL,
				0x6a23'bf6e'f4a6'956cULL, 0x191c'b96b'851a'd352ULL, 0x55d5'98d4'd6de'351aULL,
				0xc960'4de5'f2ae'7ef3ULL, 0x1ca6'c2a3'a981'e172ULL, 0xde2f'9551'ad7a'5398ULL,
				0x3025'aaff'56c8'f616ULL, 0x1552'1d9d'1e28'60d9ULL, 0x506f'e31c'fa45'073aULL,
				0x189c'55f1'2b64'7b0bULL, 0x0180'ec9a'ae7e'a859ULL, 0x7cec'8b40'050c'105eULL,
				0x2350'e519'8bf9'4104ULL, 0xef8a'd334'55cc'0dd7ULL, 0x07a7'bee1'6d67'7f92ULL,
				0xe5e3'25b9'0de7'6997ULL, 0x5a06'1591'a26e'637aULL, 0xb611'ef16'1820'8b46ULL,
				0x09f4'df3e'b7a9'81abULL, 0x1ebb'078a'e87d'acc0ULL, 0xb791'038c'b65e'231fULL,
				0x0fd3'8d45'74b0'5660ULL, 0x67ed'f702'c1ea'8ebeULL, 0xba5f'4be0'8312'38cdULL,
				0xe3c4'77c2'cefe'be5cULL, 0x0dce'486c'354c'1bd2ULL, 0x8c5d'b364'16c3'1910ULL,
				0x26ea'9ed1'a762'7324ULL, 0x039d'29b3'ef82'e5ebULL, 0x9f28'fc82'cbf2'ae02ULL,
				0xa8aa'e89c'f05d'2786ULL, 0x431a'acfa'2774'b028ULL, 0xcf47'1f9e'31b7'a938ULL,
				0x581b'd0b8'e392'2ec8ULL, 0xbc78'199b'400b'ef06ULL, 0x90fb'71c7'bf42'f862ULL,
				0x1f3b'eb10'4603'0499ULL, 0x683e'7a47'b55a'd8deULL, 0x988f'4263'a695'd190ULL,
				0xd808'c72a'6e63'8453ULL, 0x0627'527b'c319'd7cbULL, 0xebb0'4466'd729'97aeULL,
				0xe67e'0c0a'e265'8c7cULL, 0x14d2'f107'b056'c880ULL, 0x7122'c32c'3040'0b8cULL,
				0x8a7a'e11f'd5da'cedbULL, 0xa0de'db38'e98a'0e74ULL, 0xad10'9354'dcc6'15a6ULL,
				0x0be9'1a17'f655'cc19ULL, 0x8ddd'5ffe'b8bd'b149ULL, 0xbfe5'3028'af89'0aedULL,
				0xd65b'a6f5'b4ad'7a6aULL, 0x7956'f088'2997'227eULL, 0x10e8'6655'32b3'52f9ULL,
				0x0e53'61df'dace'fe39ULL, 0xcec7'f304'9fc9'0161ULL, 0xff62'b561'677f'5f2eULL,
				0x975c'cf26'd225'87f0ULL, 0x51ef'0f86'543b'af63ULL, 0x2f1e'41ef'10cb'f28fULL,
				0x5272'2635'bbb9'4a88ULL, 0xae8d'bae7'3344'f04dULL, 0x4107'69d3'6688'fd9aULL,
				0xb3ab'94de'34bb'b966ULL, 0x8013'1792'8df1'aa9bULL, 0xa564'a0f0'c511'3c54ULL,
				0xf131'd4be'bdb1'a117ULL, 0x7f71'a2f3'ea8e'f5b5ULL, 0x4087'8549'c8f6'55c3ULL,
				0x7ef1'4e69'44f0'5decULL, 0xd446'63dc'f551'37d8ULL, 0xf2ac'fd0d'5233'44fcULL,
				0x0000'0000'0000'0000ULL, 0x5fbc'6e59'8ef5'515aULL, 0x16cf'342e'f1aa'8532ULL,
				0xb036'bd6d'db39'5c8dULL, 0x1375'4fe6'dd31'b712ULL, 0xbbdf'a77a'2d6c'9094ULL,
				0x89e7'c8ac'3a58'2b30ULL, 0x3c6b'0e09'cdfa'459dULL, 0xc4ae'0589'c7e2'6521ULL,
				0x4973'5a77'7f5f'd468ULL, 0xcafd'6456'1d2c'9b18ULL, 0xda15'0203'2f9f'c9e1ULL,
				0x8867'2436'9426'8369ULL, 0x3782'141e'3baf'8984ULL, 0x9cb5'd531'2470'4be9ULL,
				0xd7db'4a6f'1ad3'd233ULL, 0xa6f9'8943'2a93'd9bfULL, 0x9d35'39ab'8a0e'e3b0ULL,
				0x53f2'caaf'15c7'e2d1ULL, 0x6e19'283c'7643'0f15ULL, 0x3deb'e293'6384'edc4ULL,
				0x5e3c'82c3'208b'f903ULL, 0x33b8'834c'b94a'13fdULL, 0x6470'deb1'2e68'6b55ULL,
				0x359f'd137'7a53'c436ULL, 0x61ca'a579'02f3'5975ULL, 0x043a'9752'82e5'9a79ULL,
				0xfd7f'7048'2683'129cULL, 0xc52e'e913'699c'cd78ULL, 0x28b9'ff0e'7dac'8d1dULL,
				0x5455'744e'78a0'9d43ULL, 0xcb7d'88cc'b352'3341ULL, 0x44bd'121b'4a13'cfbaULL,
				0x4d49'cd25'fdba'4e11ULL, 0x3e76'cb20'8c06'082fULL, 0x3ff6'27ba'2278'a076ULL,
				0xc289'57f2'04fb'b2eaULL, 0x453d'fe81'e46d'67e3ULL, 0x94c1'e695'3da7'621bULL,
				0x2c83'685c'ff49'1764ULL, 0xf32c'1197'fc4d'eca5ULL, 0x2b24'd6bd'922e'68f6ULL,
				0xb22b'7844'9ac5'113fULL, 0x48f3'b6ed'd121'7c31ULL, 0x2e9e'ad75'beb5'5ad6ULL,
				0x174f'd8b4'5fd4'2d6bULL, 0x4ed4'e496'1238'abfaULL, 0x92e6'b4ee'febe'b5d0ULL,
				0x46a0'd732'0bef'8208ULL, 0x4720'3ba8'a591'2a51ULL, 0x24f7'5bf8'e69e'3e96ULL,
				0xf0b1'3824'13cf'094eULL, 0xfee2'59fb'c901'f777ULL, 0x276a'724b'091c'db7dULL,
				0xbdf8'f501'ee75'475fULL, 0x599b'3c22'4dec'8691ULL, 0x6d84'018f'99c1'eafeULL,
				0x7498'b8e4'1cdb'39acULL, 0xe059'5e71'217c'5bb7ULL, 0x2aa4'3a27'3c50'c0afULL,
				0xf50b'43ec'3f54'3b6eULL, 0x838e'3e21'6273'4f70ULL, 0xc094'92db'4507'ff58ULL,
				0x72bf'ea9f'dfc2'ee67ULL, 0x1168'8acf'9ccd'faa0ULL, 0x1a81'90d8'6a98'36b9ULL,
				0x7acb'd93b'c615'c795ULL, 0xc733'2c3a'2860'80caULL, 0x8634'45e9'4ee8'7d50ULL,
				0xf696'6a5f'd0d6'de85ULL, 0xe9ad'814f'96d5'da1cULL, 0x70a2'2fb6'9e3e'a3d5ULL,
				0x0a69'f68d'582b'6440ULL, 0xb842'8ec9'c2ee'757fULL, 0x604a'49e3'ac8d'f12cULL,
				0x5b86'f90b'0c10'cb23ULL, 0xe1d9'b2eb'8f02'f3eeULL, 0x2939'1394'd3d2'2544ULL,
				0xc8e0'a17f'5cd0'd6aaULL, 0xb58c'c6a5'f7a2'6eadULL, 0x8193'fb08'238f'02c2ULL,
				0xd5c6'8f46'5b2f'9f81ULL, 0xfcff'9cd2'88fd'bac5ULL, 0x7705'9157'f359'dc47ULL,
				0x1d26'2e39'07ff'492bULL, 0xfb58'2233'e59a'c557ULL, 0xddb2'bce2'42f8'b673ULL,
				0x2577'b762'48e0'96cfULL, 0x6f99'c4a6'd83d'a74cULL, 0xc114'7e41'eb79'5701ULL,
				0xf48b'af76'912a'9337ULL},
			{0x3ef2'9d24'9b2c'0a19ULL, 0xe9e1'6322'b6f8'622fULL, 0x5536'9940'4775'7f7aULL,
				0x9f4d'56d5'a47b'0b33ULL, 0x8225'6746'6aa1'174cULL, 0xb8f5'057d'eb08'2fb2ULL,
				0xcc48'c10b'f447'5f53ULL, 0x3730'88d4'275d'ec3aULL, 0x968f'4325'180a'ed10ULL,
				0x173d'232c'f701'6151ULL, 0xae4e'd09f'946f'cc13ULL, 0xfd4b'4741'c453'9873ULL,
				0x1b5b'3f0d'd993'3765ULL, 0x2ffc'b096'7b64'4052ULL, 0xe023'76d2'0a89'840cULL,
				0xa3ae'3a70'329b'18d7ULL, 0x419c'bd23'35de'8526ULL, 0xfafe'bf11'5b7c'3199ULL,
				0x0397'074f'85aa'9b0dULL, 0xc58a'd4fb'4836'b970ULL, 0xbec6'0be3'fc41'04a8ULL,
				0x1eff'36dc'4b70'8772ULL, 0x131f'dc33'ed84'53b6ULL, 0x0844'e33e'3417'64d3ULL,
				0x0ff1'1b6e'ab38'cd39ULL, 0x6435'1f0a'7761'b85aULL, 0x3b56'94f5'09cf'ba0eULL,
				0x3085'7084'b872'45d0ULL, 0x47af'b3bd'2297'ae3cULL, 0xf2ba'5c2f'6f6b'554aULL,
				0x74bd'c476'1f4f'70e1ULL, 0xcfdf'c644'71ed'c45eULL, 0xe610'784c'1dc0'af16ULL,
				0x7aca'29d6'3c11'3f28ULL, 0x2ded'4117'76a8'59afULL, 0xac5f'211e'99a3'd5eeULL,
				0xd484'f949'a87e'f33bULL, 0x3ce3'6ca5'96e0'13e4ULL, 0xd120'f098'3a9d'432cULL,
				0x6bc4'0464'dc59'7563ULL, 0x69d5'f5e5'd195'6c9eULL, 0x9ae9'5f04'3698'bb24ULL,
				0xc9ec'c8da'66a4'ef44ULL, 0xd695'08c8'a5b2'eac6ULL, 0xc40c'2235'c050'3b80ULL,
				0x38c1'93ba'8c65'2103ULL, 0x1cee'c75d'46bc'9e8fULL, 0xd331'0119'3751'5ad1ULL,
				0xd8e2'e568'86ec'a50fULL, 0xb137'108d'5779'c991ULL, 0x709f'3b69'05ca'4206ULL,
				0x4feb'5083'1680'caefULL, 0xec45'6af3'241b'd238ULL, 0x58d6'73af'e181'abbeULL,
				0x242f'54e7'cad9'bf8cULL, 0x0211'f181'0dcc'19fdULL, 0x90bc'4dbb'0f43'c60aULL,
				0x9518'446a'9da0'761dULL, 0xa1bf'cbf1'3f57'012aULL, 0x2bde'4f89'61e1'72b5ULL,
				0x27b8'53a8'4f73'2481ULL, 0xb0b1'e643'df1f'4b61ULL, 0x18cc'3842'5c39'ac68ULL,
				0xd2b7'f7d7'bf37'd821ULL, 0x3103'864a'3014'c720ULL, 0x14aa'2463'72ab'fa5cULL,
				0x6e60'0db5'4eba'c574ULL, 0x3947'6574'0403'a3f3ULL, 0x09c2'15f0'bc71'e623ULL,
				0x2a58'b947'e987'f045ULL, 0x7b4c'df18'b477'bdd8ULL, 0x9709'b5eb'906c'6fe0ULL,
				0x7308'3c26'8060'd90bULL, 0xfedc'400e'41f9'037eULL, 0x2849'48c6'e44b'e9b8ULL,
				0x728e'cae8'0806'5bfbULL, 0x0633'0e9e'1749'2b1aULL, 0x5950'8561'69e7'294eULL,
				0xbae4'f4fc'e6c4'364fULL, 0xca7b'cf95'e30e'7449ULL, 0x7d7f'd186'a33e'96c2ULL,
				0x5283'6110'd85a'd690ULL, 0x4dfa'a102'1b4c'd312ULL, 0x913a'bb75'8725'44faULL,
				0xdd46'ecb9'140f'1518ULL, 0x3d65'9a6b'1e86'9114ULL, 0xc23f'2cab'd719'109aULL,
				0xd713'fe06'2dd4'6836ULL, 0xd0a6'0656'b2fb'c1dcULL, 0x221c'5a79'dd90'9496ULL,
				0xefd2'6dbc'a1b1'4935ULL, 0x0e77'eda0'235e'4fc9ULL, 0xcbfd'395b'6b68'f6b9ULL,
				0x0de0'eaef'a6f4'd4c4ULL, 0x0422'ff1f'1a85'32e7ULL, 0xf969'b85e'ded6'aa94ULL,
				0x7f6e'2007'aef2'8f3fULL, 0x3ad0'623b'81a9'38feULL, 0x6624'ee8b'7aad'a1a7ULL,
				0xb682'e8dd'c856'607bULL, 0xa78c'c56f'281e'2a30ULL, 0xc79b'257a'45fa'a08dULL,
				0x5b41'74e0'642b'30b3ULL, 0x5f63'8bff'7eae'0254ULL, 0x4bc9'af9c'0c05'f808ULL,
				0xce59'308a'f98b'46aeULL, 0x8fc5'8da9'cc55'c388ULL, 0x8034'96c7'676d'0eb1ULL,
				0xf33c'aae1'e70d'd7baULL, 0xbb62'0232'6ea2'b4bfULL, 0xd502'0f87'2018'71cbULL,
				0x9d5c'a754'a9b7'12ceULL, 0x8416'69d8'7de8'3c56ULL, 0x8a61'8478'5eb6'739fULL,
				0x420b'ba6c'b074'1e2bULL, 0xf12d'5b60'eac1'ce47ULL, 0x76ac'35f7'1283'691cULL,
				0x2c6b'b7d9'fece'db5fULL, 0xfccd'b18f'4c35'1a83ULL, 0x1f79'c012'c316'0582ULL,
				0xf0ab'adae'62a7'4cb7ULL, 0xe1a5'801c'82ef'06fcULL, 0x67a2'1845'f2cb'2357ULL,
				0x5114'665f'5df0'4d9dULL, 0xbf40'fd2d'7427'8658ULL, 0xa039'3d3f'b731'83daULL,
				0x05a4'09d1'92e3'b017ULL, 0xa9fb'28cf'0b40'65f9ULL, 0x25a9'a229'42bf'3d7cULL,
				0xdb75'e227'0346'3e02ULL, 0xb326'e10c'5ab5'd06cULL, 0xe796'8e82'95a6'2de6ULL,
				0xb973'f3b3'636e'ad42ULL, 0xdf57'1d38'19c3'0ce5ULL, 0xee54'9b72'29d7'cbc5ULL,
				0x1299'2afd'65e2'd146ULL, 0xf8ef'4e90'56b0'2864ULL, 0xb704'1e13'4030'e28bULL,
				0xc02e'dd2a'dad5'0967ULL, 0x932b'4af4'8ae9'5d07ULL, 0x6fe6'fb7b'c6dc'4784ULL,
				0x239a'acb7'55f6'1666ULL, 0x401a'4bed'bdb8'07d6ULL, 0x485e'a8d3'89af'6305ULL,
				0xa41b'c220'adb4'b13dULL, 0x753b'32b8'9729'f211ULL, 0x997e'584b'b332'2029ULL,
				0x1d68'3193'ceda'1c7fULL, 0xff5a'b6c0'c99f'818eULL, 0x16bb'd5e2'7f67'e3a1ULL,
				0xa59d'34ee'25d2'33cdULL, 0x98f8'ae85'3b54'a2d9ULL, 0x6df7'0afa'cb10'5e79ULL,
				0x795d'2e99'b9bb'a425ULL, 0x8e43'7b67'4433'4178ULL, 0x0186'f6ce'8866'82f0ULL,
				0xebf0'92a3'bb34'7bd2ULL, 0xbcd7'fa62'f18d'1d55ULL, 0xadd9'd7d0'11c5'571eULL,
				0x0bd3'e471'b1bd'ffdeULL, 0xaa6c'2f80'8eea'fef4ULL, 0x5ee5'7d31'f6c8'80a4ULL,
				0xf50f'a47f'f044'fca0ULL, 0x1add'c9c3'51f5'b595ULL, 0xea76'646d'3352'f922ULL,
				0x0000'0000'0000'0000ULL, 0x8590'9f16'f58e'bea6ULL, 0x4629'4573'aaf1'2cccULL,
				0x0a55'12bf'39db'7d2eULL, 0x78db'd857'31dd'26d5ULL, 0x29cf'be08'6c2d'6b48ULL,
				0x218b'5d36'583a'0f9bULL, 0x152c'd2ad'facd'78acULL, 0x83a3'9188'e2c7'95bcULL,
				0xc3b9'da65'5f7f'926aULL, 0x9ecb'a01b'2c1d'89c3ULL, 0x07b5'f850'9f2f'a9eaULL,
				0x7ee8'd6c9'2694'0dcfULL, 0x36b6'7e1a'af3b'6ecaULL, 0x8607'9859'7024'25abULL,
				0xfb78'49df'd31a'b369ULL, 0x4c7c'57cc'932a'51e2ULL, 0xd964'13a6'0e8a'27ffULL,
				0x263e'a566'c715'a671ULL, 0x6c71'fc34'4376'dc89ULL, 0x4a4f'5952'8463'7af8ULL,
				0xdaf3'14e9'8b20'bcf2ULL, 0x5727'68c1'4ab9'6687ULL, 0x1088'db7c'682e'c8bbULL,
				0x8870'75f9'537a'6a62ULL, 0x2e7a'4658'f302'c2a2ULL, 0x6191'16db'e582'084dULL,
				0xa87d'de01'8326'e709ULL, 0xdcc0'1a77'9c69'97e8ULL, 0xedc3'9c3d'ac7d'50c8ULL,
				0xa60a'33a1'a078'a8c0ULL, 0xc1a8'2be4'52b3'8b97ULL, 0x3f74'6bea'134a'88e9ULL,
				0xa228'ccbe'bafd'9a27ULL, 0xabea'd94e'068c'7c04ULL, 0xf489'52b1'7822'7e50ULL,
				0x5cf4'8cb0'fb04'9959ULL, 0x6017'e015'6de4'8abdULL, 0x4438'b4f2'a73d'3531ULL,
				0x8c52'8ae6'49ff'5885ULL, 0xb515'ef92'4dfc'fb76ULL, 0x0c66'1c21'2e92'5634ULL,
				0xb493'195c'c59a'7986ULL, 0x9cda'519a'21d1'903eULL, 0x3294'8105'b5be'5c2dULL,
				0x194a'ce8c'd45f'2e98ULL, 0x438d'4ca2'3812'9cdbULL, 0x9b6f'a9ca'befe'39d4ULL,
				0x81b2'6009'ef0b'8c41ULL, 0xded1'ebf6'91a5'8e15ULL, 0x4e6d'a64d'9ee6'481fULL,
				0x54b0'6f8e'cf13'fd8aULL, 0x49d8'5e1d'01c9'e1f5ULL, 0xafc8'2651'1c09'4ee3ULL,
				0xf698'a330'75ee'67adULL, 0x5ac7'822e'ec4d'b243ULL, 0x8dd4'7c28'c199'da75ULL,
				0x89f6'8337'db1c'e892ULL, 0xcdce'37c5'7c21'dda3ULL, 0x5305'97de'503c'5460ULL,
				0x6a42'f2aa'543f'f793ULL, 0x5d72'7a7e'7362'1ba9ULL, 0xe232'8753'0745'9df1ULL,
				0x56a1'9e0f'c2df'e477ULL, 0xc61d'd3b4'cd9c'227dULL, 0xe587'7f03'986a'341bULL,
				0x949e'b2a4'15c6'f4edULL, 0x6206'1194'6028'9340ULL, 0x6380'e75a'e84e'11b0ULL,
				0x8be7'72b6'd6d0'f16fULL, 0x5092'9091'd596'cf6dULL, 0xe867'95ec'3e9e'e0dfULL,
				0x7cf9'2748'2b58'1432ULL, 0xc86a'3e14'eec2'6db4ULL, 0x7119'cda7'8dac'c0f6ULL,
				0xe401'89cd'100c'b6ebULL, 0x92ad'bc3a'028f'dff7ULL, 0xb2a0'17c2'd2d3'529cULL,
				0x200d'abf8'd05c'8d6bULL, 0x34a7'8f9b'a2f7'7737ULL, 0xe3b4'719d'8f23'1f01ULL,
				0x45be'423c'2f5b'b7c1ULL, 0xf71e'55fe'fd88'e55dULL, 0x6853'032b'59f3'ee6eULL,
				0x65b3'e9c4'ff07'3aaaULL, 0x772a'c339'9ae5'ebecULL, 0x8781'6e97'f842'a75bULL,
				0x110e'2db2'e048'4a4bULL, 0x3312'77cb'3dd8'deddULL, 0xbd51'0cac'79eb'9fa5ULL,
				0x3521'7955'2a91'f5c7ULL},
			{0x8ab0'a968'46e0'6a6dULL, 0x43c7'e80b'4bf0'b33aULL, 0x08c9'b354'6b16'1ee5ULL,
				0x39f1'c235'eba9'90beULL, 0xc1be'f237'6606'c7b2ULL, 0x2c20'9233'6145'69aaULL,
				0xeb01'523b'6fc3'289aULL, 0x9469'53ab'935a'ceddULL, 0x2728'38f6'3e13'340eULL,
				0x8b04'55ec'a12b'a052ULL, 0x77a1'b2c4'978f'f8a2ULL, 0xa551'22ca'13e5'4086ULL,
				0x2276'1358'62d3'f1cdULL, 0xdb8d'dfde'08b7'6cfeULL, 0x5d1e'12c8'9e4a'178aULL,
				0x0e56'816b'0396'9867ULL, 0xee5f'7995'3303'ed59ULL, 0xafed'748b'ab78'd71dULL,
				0x6d92'9f2d'f93e'53eeULL, 0xf5d8'a8f8'ba79'8c2aULL, 0xf619'b169'8e39'cf6bULL,
				0x95dd'af2f'7491'04e2ULL, 0xec2a'9c80'e088'6427ULL, 0xce5c'8fd8'825b'95eaULL,
				0xc4e0'd999'3ac6'0271ULL, 0x4699'c3a5'1730'76f9ULL, 0x3d1b'151f'50a2'9f42ULL,
				0x9ed5'05ea'2bc7'5946ULL, 0x3466'5acf'dc7f'4b98ULL, 0x61b1'fb53'2923'42f7ULL,
				0xc721'c008'0e86'4130ULL, 0x8693'cd16'96fd'7b74ULL, 0x8727'3192'7136'b14bULL,
				0xd344'6c8a'63a1'721bULL, 0x669a'35e8'a668'0e4aULL, 0xcab6'58f2'3950'9a16ULL,
				0xa4e5'de4e'f42e'8ab9ULL, 0x37a7'435e'e83f'08d9ULL, 0x134e'6239'e26c'7f96ULL,
				0x8279'1a3c'2df6'7488ULL, 0x3f6e'f00a'8329'163cULL, 0x8e5a'7e42'fdeb'6591ULL,
				0x5caa'ee4c'7981'ddb5ULL, 0x19f2'3478'5af1'e80dULL, 0x255d'dde3'ed98'bd70ULL,
				0x5089'8a32'a99c'ccacULL, 0x28ca'4519'da4e'6656ULL, 0xae59'880f'4cb3'1d22ULL,
				0x0d97'98fa'37d6'db26ULL, 0x32f9'68f0'b4ff'cd1aULL, 0xa00f'0964'4f25'8545ULL,
				0xfa3a'd517'5e24'de72ULL, 0xf46c'547c'5db2'4615ULL, 0x713e'80fb'ff0f'7e20ULL,
				0x7843'cf2b'73d2'aafaULL, 0xbd17'ea36'aedf'62b4ULL, 0xfd11'1bac'd16f'92cfULL,
				0x4aba'a7db'c72d'67e0ULL, 0xb341'6b5d'ad49'fad3ULL, 0xbca3'16b2'4914'a88bULL,
				0x15d1'5006'8aec'f914ULL, 0xe27c'1deb'e31e'fc40ULL, 0x4fe4'8c75'9bed'a223ULL,
				0x7edc'fd14'1b52'2c78ULL, 0x4e50'70f1'7c26'681cULL, 0xe696'cac1'5815'f3bcULL,
				0x35d2'a64b'3bb4'81a7ULL, 0x800c'ff29'fe7d'fdf6ULL, 0x1ed9'fac3'd5ba'a4b0ULL,
				0x6c26'63a9'1ef5'99d1ULL, 0x03c1'1991'3440'4341ULL, 0xf7ad'4ded'69f2'0554ULL,
				0xcd9d'9649'b61b'd6abULL, 0xc8c3'bde7'eadb'1368ULL, 0xd131'899f'b02a'fb65ULL,
				0x1d18'e352'e1fa'e7f1ULL, 0xda39'235a'ef7c'a6c1ULL, 0xa1bb'f5e0'a8ee'4f7aULL,
				0x9137'7805'cf9a'0b1eULL, 0x3138'7161'80bf'8e5bULL, 0xd9f8'3acb'db3c'e580ULL,
				0x0275'e515'd38b'897eULL, 0x472d'3f21'f0fb'bcc6ULL, 0x2d94'6eb7'868e'a395ULL,
				0xba3c'248d'2194'2e09ULL, 0xe722'3645'bfde'3983ULL, 0xff64'feb9'02e4'1bb1ULL,
				0xc977'4163'0d10'd957ULL, 0xc3cb'1722'b58d'4eccULL, 0xa27a'ec71'9cae'0c3bULL,
				0x99fe'cb51'a48c'15fbULL, 0x1465'ac82'6d27'332bULL, 0xe1bd'047a'd75e'bf01ULL,
				0x79f7'33af'9419'60c5ULL, 0x672e'c96c'41a3'c475ULL, 0xc27f'eba6'5246'84f3ULL,
				0x64ef'd0fd'75e3'8734ULL, 0xed9e'6004'0743'ae18ULL, 0xfb8e'2993'b9ef'144dULL,
				0x3845'3eb1'0c62'5a81ULL, 0x6978'4807'4235'5c12ULL, 0x48cf'42ce'14a6'ee9eULL,
				0x1cac'1fd6'0631'2dceULL, 0x7b82'd6ba'4792'e9bbULL, 0x9d14'1c7b'1f87'1a07ULL,
				0x5616'b80d'c11c'4a2eULL, 0xb849'c198'f21f'a777ULL, 0x7ca9'1801'c8d9'a506ULL,
				0xb134'8e48'7ec2'73adULL, 0x41b2'0d1e'987b'3a44ULL, 0x7460'ab55'a3cf'bbe3ULL,
				0x84e6'2803'4576'f20aULL, 0x1b87'd16d'897a'6173ULL, 0x0fe2'7def'e45d'5258ULL,
				0x83cd'e6b8'ca3d'beb7ULL, 0x0c23'647e'd01d'1119ULL, 0x7a36'2a3e'a059'2384ULL,
				0xb61f'40f3'f189'3f10ULL, 0x75d4'57d1'4404'71dcULL, 0x4558'da34'2370'35b8ULL,
				0xdca6'1165'87fc'2043ULL, 0x8d9b'67d3'c9ab'26d0ULL, 0x2b0b'5c88'ee0e'2517ULL,
				0x6fe7'7a38'2ab5'da90ULL, 0x269c'c472'd9d8'fe31ULL, 0x63c4'1e46'faa8'cb89ULL,
				0xb7ab'bc77'1642'f52fULL, 0x7d1d'e485'2f12'6f39ULL, 0xa8c6'ba30'2433'9ba0ULL,
				0x6005'07d7'cee8'88c8ULL, 0x8fee'82c6'1a20'afaeULL, 0x57a2'4489'26d7'8011ULL,
				0xfca5'e728'36a4'58f0ULL, 0x072b'cebb'8f4b'4cbdULL, 0x497b'be4a'f36d'24a1ULL,
				0x3caf'e99b'b769'557dULL, 0x12fa'9ebd'05a7'b5a9ULL, 0xe8c0'4baa'5b83'6bdbULL,
				0x4273'148f'ac3b'7905ULL, 0x9083'8481'2851'c121ULL, 0xe557'd350'6c55'b0fdULL,
				0x72ff'996a'cb4f'3d61ULL, 0x3eda'0c8e'64e2'dc03ULL, 0xf086'8356'e6b9'49e9ULL,
				0x04ea'd72a'bb0b'0ffcULL, 0x17a4'b513'5967'706aULL, 0xe3c8'e16f'04d5'367fULL,
				0xf84f'3002'8daf'570cULL, 0x1846'c8fc'bd3a'2232ULL, 0x5b81'20f7'f6ca'9108ULL,
				0xd46f'a231'ecea'3ea6ULL, 0x334d'9474'5334'0725ULL, 0x5840'3966'c28a'd249ULL,
				0xbed6'f3a7'9a9f'21f5ULL, 0x68cc'b483'a5fe'962dULL, 0xd085'751b'57e1'315aULL,
				0xfed0'023d'e52f'd18eULL, 0x4b0e'5b5f'20e6'addfULL, 0x1a33'2de9'6eb1'ab4cULL,
				0xa3ce'10f5'7b65'c604ULL, 0x108f'7ba8'd62c'3cd7ULL, 0xab07'a3a1'1073'd8e1ULL,
				0x6b0d'ad12'91be'd56cULL, 0xf2f3'6643'3532'c097ULL, 0x2e55'7726'b2ce'e0d4ULL,
				0x0000'0000'0000'0000ULL, 0xcb02'a476'de9b'5029ULL, 0xe4e3'2fd4'8b9e'7ac2ULL,
				0x734b'65ee'2c84'f75eULL, 0x6e53'86bc'cd7e'10afULL, 0x01b4'fc84'e7cb'ca3fULL,
				0xcfe8'735c'6590'5fd5ULL, 0x3613'bfda'0ff4'c2e6ULL, 0x113b'872c'31e7'f6e8ULL,
				0x2fe1'8ba2'5505'2aebULL, 0xe974'b72e'bc48'a1e4ULL, 0x0abc'5641'b89d'979bULL,
				0xb46a'a5e6'2202'b66eULL, 0x44ec'26b0'c4bb'ff87ULL, 0xa690'3b5b'27a5'03c7ULL,
				0x7f68'0190'fc99'e647ULL, 0x97a8'4a3a'a71a'8d9cULL, 0xdd12'ede1'6037'ea7cULL,
				0xc554'251d'dd0d'c84eULL, 0x88c5'4c7d'956b'e313ULL, 0x4d91'6960'4866'2b5dULL,
				0xb080'72cc'9909'b992ULL, 0xb5de'5962'c5c9'7c51ULL, 0x81b8'03ad'19b6'37c9ULL,
				0xb2f5'97d9'4a82'30ecULL, 0x0b08'aac5'5f56'5da4ULL, 0xf132'7fd2'0172'83d6ULL,
				0xad98'919e'78f3'5e63ULL, 0x6ab9'5196'7675'1f53ULL, 0x24e9'2167'0a53'774fULL,
				0xb9fd'3d1c'15d4'6d48ULL, 0x92f6'6194'fbda'485fULL, 0x5a35'dc73'1101'5b37ULL,
				0xded3'f470'5477'a93dULL, 0xc00a'0eb3'81cd'0d8dULL, 0xbb88'd809'c65f'e436ULL,
				0x1610'4997'beac'ba55ULL, 0x21b7'0ac9'5693'b28cULL, 0x59f4'c5e2'2541'1876ULL,
				0xd5db'5eb5'0b21'f499ULL, 0x55d7'a19c'f55c'096fULL, 0xa972'46b4'c3f8'519fULL,
				0x8552'd487'a2bd'3835ULL, 0x5463'5d18'1297'c350ULL, 0x23c2'efdc'8518'3bf2ULL,
				0x9f61'f96e'cc0c'9379ULL, 0x5348'93a3'9ddc'8fedULL, 0x5edf'0b59'aa0a'54cbULL,
				0xac2c'6d1a'9f38'945cULL, 0xd7ae'bba0'd8aa'7de7ULL, 0x2abf'a00c'09c5'ef28ULL,
				0xd84c'c64f'3cf7'2fbfULL, 0x2003'f64d'b158'78b3ULL, 0xa724'c7df'c06e'c9f8ULL,
				0x069f'323f'6880'8682ULL, 0xcc29'6acd'51d0'1c94ULL, 0x055e'2bae'5cc0'c5c3ULL,
				0x6270'e2c2'1d63'01b6ULL, 0x3b84'2720'3822'19c0ULL, 0xd2f0'900e'846a'b824ULL,
				0x52fc'6f27'7a17'45d2ULL, 0xc695'3c8c'e94d'8b0fULL, 0xe009'f8fe'3095'753eULL,
				0x655b'2c79'9228'4d0bULL, 0x984a'37d5'4347'dfc4ULL, 0xeab5'aebf'8808'e2a5ULL,
				0x9a3f'd2c0'90cc'56baULL, 0x9ca0'e0ff'f84c'd038ULL, 0x4c25'95e4'afad'e162ULL,
				0xdf67'08f4'b3bc'6302ULL, 0xbf62'0f23'7d54'ebcaULL, 0x9342'9d10'1c11'8260ULL,
				0x097d'4fd0'8cdd'd4daULL, 0x8c2f'9b57'2e60'ecefULL, 0x708a'7c7f'18c4'b41fULL,
				0x3a30'dba4'dfe9'd3ffULL, 0x4006'f19a'7fb0'f07bULL, 0x5f6b'f7dd'4dc1'9ef4ULL,
				0x1f6d'0647'3271'6e8fULL, 0xf9fb'cc86'6a64'9d33ULL, 0x308c'8de5'6774'4464ULL,
				0x8971'b0f9'72a0'292cULL, 0xd61a'4724'3f61'b7d8ULL, 0xefeb'8511'd4c8'2766ULL,
				0x961c'b6be'40d1'47a3ULL, 0xaab3'5f25'f7b8'12deULL, 0x7615'4e40'7044'329dULL,
				0x513d'76b6'4e57'0693ULL, 0xf347'9ac7'd2f9'0aa8ULL, 0x9b8b'2e44'7707'9c85ULL,
				0x297e'b99d'3d85'ac69ULL},
			{0x7e37'e62d'fc7d'40c3ULL, 0x776f'25a4'ee93'9e5bULL, 0xe045'c850'dd8f'b5adULL,
				0x86ed'5ba7'11ff'1952ULL, 0xe91d'0bd9'cf61'6b35ULL, 0x37e0'ab25'6e40'8ffbULL,
				0x9607'f6c0'3102'5a7aULL, 0x0b02'f5e1'16d2'3c9dULL, 0xf3d8'486b'fb50'650cULL,
				0x621c'ff27'c408'75f5ULL, 0x7d40'cb71'fa5f'd34aULL, 0x6daa'6616'daa2'9062ULL,
				0x9f5f'3549'23ec'84e2ULL, 0xec84'7c3d'c507'c3b3ULL, 0x025a'3668'043c'e205ULL,
				0xa8bf'9e6c'4dac'0b19ULL, 0xfa80'8be2'e9be'bb94ULL, 0xb5b9'9c52'77c7'4fa3ULL,
				0x78d9'bc95'f039'7bccULL, 0xe332'e50c'dbad'2624ULL, 0xc74f'ce12'9332'797eULL,
				0x1729'eceb'2ea7'09abULL, 0xc2d6'b9f6'9954'd1f8ULL, 0x5d89'8cbf'bab8'551aULL,
				0x859a'76fb'17dd'8adbULL, 0x1be8'5886'362f'7fb5ULL, 0xf641'3f8f'f136'cd8aULL,
				0xd311'0fa5'bbb7'e35cULL, 0x0a2f'eed5'14cc'4d11ULL, 0xe830'10ed'cd7f'1ab9ULL,
				0xa1e7'5de5'5f42'd581ULL, 0xeede'4a55'c13b'21b6ULL, 0xf2f5'535f'f94e'1480ULL,
				0x0cc1'b46d'1888'761eULL, 0xbce1'5fdb'6529'913bULL, 0x2d25'e897'5a71'81c2ULL,
				0x7181'7f1c'e2d7'a554ULL, 0x2e52'c5cb'5c53'124bULL, 0xf9f7'a6be'ef9c'281dULL,
				0x9e72'2e7d'21f2'f56eULL, 0xce17'0d9b'81dc'a7e6ULL, 0x0e9b'8205'1cb4'941bULL,
				0x1e71'2f62'3c49'd733ULL, 0x21e4'5cfa'42f9'f7dcULL, 0xcb8e'7a7f'8bba'0f60ULL,
				0x8e98'831a'010f'b646ULL, 0x474c'cf0d'8e89'5b23ULL, 0xa992'8558'4fb2'7a95ULL,
				0x8cc2'b572'0533'5443ULL, 0x42d5'b8e9'84ef'f3a5ULL, 0x012d'1b34'021e'718cULL,
				0x57a6'626a'ae74'180bULL, 0xff19'fc06'e3d8'1312ULL, 0x35ba'9d4d'6a7c'6dfeULL,
				0xc9d4'4c17'8f86'ed65ULL, 0x5065'23e6'a02e'5288ULL, 0x0377'2d5c'0622'9389ULL,
				0x8b01'f4fe'0b69'1ec0ULL, 0xf8da'bd8a'ed82'5991ULL, 0x4c4e'3aec'985b'67beULL,
				0xb10d'f082'7fbf'96a9ULL, 0x6a69'279a'd4f8'dae1ULL, 0xe786'89dc'd3d5'ff2eULL,
				0x812e'1a2b'1fa5'53d1ULL, 0xfbad'90d6'eba0'ca18ULL, 0x1ac5'43b2'3431'0e39ULL,
				0x1604'f7df'2cb9'7827ULL, 0xa624'1c69'5118'9f02ULL, 0x7535'13cc'eaaf'7c5eULL,
				0x64f2'a59f'c84c'4efaULL, 0x247d'2b1e'489f'5f5aULL, 0xdb64'd718'ab47'4c48ULL,
				0x79f4'a7a1'f227'0a40ULL, 0x1573'da83'2a9b'ebaeULL, 0x3497'8679'6862'1c72ULL,
				0x5148'38d2'a230'2304ULL, 0xf0af'6537'fd72'f685ULL, 0x1d06'023e'3a6b'44baULL,
				0x6785'88c3'ce6e'dd73ULL, 0x66a8'93f7'cc70'acffULL, 0xd4d2'4e29'b5ed'a9dfULL,
				0x3856'3214'70ea'6a6cULL, 0x07c3'418c'0e5a'4a83ULL, 0x2bcb'b22f'5635'bacdULL,
				0x04b4'6cd0'0878'd90aULL, 0x06ee'5ab8'0c44'3b0fULL, 0x3b21'1f48'76c8'f9e5ULL,
				0x0958'c389'12ee'de98ULL, 0xd14b'39cd'bf8b'0159ULL, 0x397b'2920'72f4'1be0ULL,
				0x87c0'4093'13e1'68deULL, 0xad26'e988'47ca'a39fULL, 0x4e14'0c84'9c67'85bbULL,
				0xd5ff'551d'b7f3'd853ULL, 0xa0ca'46d1'5d5c'a40dULL, 0xcd60'20c7'87fe'346fULL,
				0x84b7'6dcf'15c3'fb57ULL, 0xdefd'a0fc'a121'e4ceULL, 0x4b8d'7b60'9601'2d3dULL,
				0x9ac6'42ad'298a'2c64ULL, 0x0875'd8bd'10f0'af14ULL, 0xb357'c6ea'7b83'74acULL,
				0x4d63'21d8'9a45'1632ULL, 0xeda9'6709'c719'b23fULL, 0xf76c'24bb'f328'bc06ULL,
				0xc662'd526'912c'08f2ULL, 0x3ce2'5ec4'7892'b366ULL, 0xb978'283f'6f4f'39bdULL,
				0xc08c'8f9e'9d68'33fdULL, 0x4f39'17b0'9e79'f437ULL, 0x593d'e06f'b2c0'8c10ULL,
				0xd688'7841'b1d1'4bdaULL, 0x19b2'6eee'3213'9db0ULL, 0xb494'8766'75d9'3e2fULL,
				0x8259'3777'1987'c058ULL, 0x90e9'ac78'3d46'6175ULL, 0xf182'7e03'ff6c'8709ULL,
				0x945d'c0a8'353e'b87fULL, 0x4516'f965'8ab5'b926ULL, 0x3f95'7398'7eb0'20efULL,
				0xb855'330b'6d51'4831ULL, 0x2ae6'a91b'542b'cb41ULL, 0x6331'e413'c616'0479ULL,
				0x408f'8e81'80d3'11a0ULL, 0xeff3'5161'c325'503aULL, 0xd066'22f9'bd95'70d5ULL,
				0x8876'd9a2'0d4b'8d49ULL, 0xa553'3135'573a'0c8bULL, 0xe168'd364'df91'c421ULL,
				0xf41b'09e7'f50a'2f8fULL, 0x12b0'9b0f'24c1'a12dULL, 0xda49'cc2c'a959'3dc4ULL,
				0x1f5c'3456'3e57'a6bfULL, 0x54d1'4f36'a856'8b82ULL, 0xaf7c'dfe0'43f6'419aULL,
				0xea6a'2685'c943'f8bcULL, 0xe5dc'bfb4'd7e9'1d2bULL, 0xb27a'ddde'799d'0520ULL,
				0x6b44'3cae'd6e6'ab6dULL, 0x7bae'91c9'f61b'e845ULL, 0x3eb8'68ac'7cae'5163ULL,
				0x11c7'b653'22e3'32a4ULL, 0xd23c'1491'b9a9'92d0ULL, 0x8fb5'982e'0311'c7caULL,
				0x70ac'6428'e0c9'd4d8ULL, 0x895b'c296'0f55'fcc5ULL, 0x7642'3e90'ec8d'efd7ULL,
				0x6ff0'507e'de9e'7267ULL, 0x3dcf'45f0'7a8c'c2eaULL, 0x4aa0'6054'941f'5cb1ULL,
				0x5810'fb5b'b0de'fd9cULL, 0x5efe'a1e3'bc9a'c693ULL, 0x6edd'4b4a'dc80'03ebULL,
				0x7418'08f8'e8b1'0dd2ULL, 0x145e'c1b7'2885'9a22ULL, 0x28bc'9f73'5017'2944ULL,
				0x270a'0642'4ebd'ccd3ULL, 0x972a'edf4'331c'2bf6ULL, 0x0599'77e4'0a66'a886ULL,
				0x2550'302a'4a81'2ed6ULL, 0xdd8a'8da0'a703'7747ULL, 0xc515'f87a'970e'9b7bULL,
				0x3023'eaa9'601a'c578ULL, 0xb7e3'aa3a'73fb'ada6ULL, 0x0fb6'9931'1eaa'e597ULL,
				0x0000'0000'0000'0000ULL, 0x310e'f19d'6204'b4f4ULL, 0x2293'71a6'44db'6455ULL,
				0x0dec'af59'1a96'0792ULL, 0x5ca4'978b'b8a6'2496ULL, 0x1c2b'190a'3875'3536ULL,
				0x41a2'95b5'82cd'602cULL, 0x3279'dcc1'6426'277dULL, 0xc1a1'94aa'9f76'4271ULL,
				0x139d'803b'26df'd0a1ULL, 0xae51'c4d4'41e8'3016ULL, 0xd813'fa44'ad65'dfc1ULL,
				0xac0b'f2bc'45d4'd213ULL, 0x23be'6a92'46c5'15d9ULL, 0x49d7'4d08'923d'cf38ULL,
				0x9d05'0321'27d0'66e7ULL, 0x2f7f'deff'5e4d'63c7ULL, 0xa47e'2a01'5524'7d07ULL,
				0x99b1'6ff1'2fa8'bfedULL, 0x4661'd439'8c97'2aafULL, 0xdfd0'bbc8'a33f'9542ULL,
				0xdca7'9694'a51d'06cbULL, 0xb020'ebb6'7da1'e725ULL, 0xba0f'0563'696d'aa34ULL,
				0xe4f1'a480'd5f7'6ca7ULL, 0xc438'e34e'9510'eaf7ULL, 0x939e'8124'3b64'f2fcULL,
				0x8def'ae46'072d'25cfULL, 0x2c08'f3a3'586f'f04eULL, 0xd7a5'6375'b3cf'3a56ULL,
				0x20c9'47ce'40e7'8650ULL, 0x43f8'a3dd'86f1'8229ULL, 0x568b'795e'ac6a'6987ULL,
				0x8003'011f'1dbb'225dULL, 0xf536'12d3'f714'5e03ULL, 0x189f'75da'300d'ec3cULL,
				0x9570'db9c'3720'c9f3ULL, 0xbb22'1e57'6b73'dbb8ULL, 0x72f6'5240'e4f5'36ddULL,
				0x443b'e251'88ab'c8aaULL, 0xe21f'fe38'd9b3'57a8ULL, 0xfd43'ca6e'e7e4'f117ULL,
				0xcaa3'614b'89a4'7eecULL, 0xfe34'e732'e1c6'629eULL, 0x8374'2c43'1b99'b1d4ULL,
				0xcf3a'16af'83c2'd66aULL, 0xaae5'a804'4990'e91cULL, 0x2627'1d76'4ca3'bd5fULL,
				0x91c4'b74c'3f58'10f9ULL, 0x7c6d'd045'f841'a2c6ULL, 0x7f1a'fd19'fe63'314fULL,
				0xc8f9'5723'8d98'9ce9ULL, 0xa709'075d'5306'ee8eULL, 0x55fc'5402'aa48'fa0eULL,
				0x48fa'563c'9023'beb4ULL, 0x65df'beab'ca52'3f76ULL, 0x6c87'7d22'd8bc'e1eeULL,
				0xcc4d'3bf3'85e0'45e3ULL, 0xbebb'69b3'6115'733eULL, 0x10ea'ad67'20fd'4328ULL,
				0xb6ce'b10e'71e5'dc2aULL, 0xbdcc'44ef'6737'e0b7ULL, 0x523f'158e'a412'b08dULL,
				0x989c'74c5'2db6'ce61ULL, 0x9beb'5999'2b94'5de8ULL, 0x8a2c'efca'0977'6f4cULL,
				0xa3bd'6b8d'5b7e'3784ULL, 0xeb47'3db1'cb5d'8930ULL, 0xc3fb'a2c2'9b4a'a074ULL,
				0x9c28'1815'25ce'176bULL, 0x6833'11f2'd0c4'38e4ULL, 0x5fd3'bad7'be84'b71fULL,
				0xfc6e'd15a'e5fa'809bULL, 0x36cd'b011'6c5e'fe77ULL, 0x2991'8447'5209'58c8ULL,
				0xa290'70b9'5960'4608ULL, 0x5312'0eba'a60c'c101ULL, 0x3a0c'047c'74d6'8869ULL,
				0x691e'0ac6'd2da'4968ULL, 0x73db'4974'e6eb'4751ULL, 0x7a83'8afd'f405'99c9ULL,
				0x5a4a'cd33'b4e2'1f99ULL, 0x6046'c94f'c034'97f0ULL, 0xe6ab'92e8'd1cb'8ea2ULL,
				0x3354'c7f5'6638'56f1ULL, 0xd93e'e170'af7b'ae4dULL, 0x616b'd27b'c22a'e67cULL,
				0x92b3'9a10'397a'8370ULL, 0xabc8'b330'4b8e'9890ULL, 0xbf96'7287'630b'02b2ULL,
				0x5b67'd607'b6fc'6e15ULL},
			{0xd031'c397'ce55'3fe6ULL, 0x16ba'5b01'b006'b525ULL, 0xa89b'ade6'296e'70c8ULL,
				0x6a1f'525d'77d3'435bULL, 0x6e10'3570'573d'fa0bULL, 0x660e'fb2a'17fc'95abULL,
				0x7632'7a9e'9763'4bf6ULL, 0x4bad'9d64'6245'8bf5ULL, 0xf183'0cae'dbc3'f748ULL,
				0xc5c8'f542'6691'31ffULL, 0x9504'4a1c'dc48'b0cbULL, 0x8929'62df'3cf8'b866ULL,
				0xb0b9'e208'e930'c135ULL, 0xa14f'b3f0'611a'767cULL, 0x8d26'05f2'1c16'0136ULL,
				0xd6b7'1922'fecc'549eULL, 0x3708'9438'a590'7d8bULL, 0x0b5d'a38e'5803'd49cULL,
				0x5a5b'cc9c'ea6f'3cbcULL, 0xedae'246d'3b73'ffe5ULL, 0xd2b8'7e0f'de22'edceULL,
				0x5e54'abb1'ca81'85ecULL, 0x1de7'f88f'e805'61b9ULL, 0xad5e'1a87'0135'a08cULL,
				0x2f2a'dbd6'65ce'cc76ULL, 0x5780'b5a7'82f5'8358ULL, 0x3edc'8a2e'ede4'7b3fULL,
				0xc9d9'5c35'06be'e70fULL, 0x83be'111d'6c4e'05eeULL, 0xa603'b909'5936'7410ULL,
				0x103c'81b4'809f'de5dULL, 0x2c69'b602'7d0c'774aULL, 0x3990'80d7'd5c8'7953ULL,
				0x09d4'1e16'4874'06b4ULL, 0xcdd6'3b18'2650'5e5fULL, 0xf99d'c2f4'9b02'98e8ULL,
				0x9cd0'540a'943c'b67fULL, 0xbca8'4b7f'891f'17c5ULL, 0x723d'1db3'b78d'f2a6ULL,
				0x78aa'6e71'e73b'4f2eULL, 0x1433'e699'a071'670dULL, 0x84f2'1be4'5462'0782ULL,
				0x98df'3327'b4d2'0f2fULL, 0xf049'dce2'd376'9e5cULL, 0xdb6c'6019'9656'eb7aULL,
				0x6487'46b2'078b'4783ULL, 0x32cd'2359'8dcb'adcfULL, 0x1ea4'955b'f0c7'da85ULL,
				0xe9a1'4340'1b9d'46b5ULL, 0xfd92'a5d9'bbec'21b8ULL, 0xc813'8c79'0e0b'8e1bULL,
				0x2ee0'0b9a'6d7b'a562ULL, 0xf857'12b8'93b7'f1fcULL, 0xeb28'fed8'0bea'949dULL,
				0x564a'65eb'8a40'ea4cULL, 0x6c99'88e8'474a'2823ULL, 0x4535'898b'121d'8f2dULL,
				0xabd8'c032'31ac'cbf4ULL, 0xba2e'91ca'b986'7cbdULL, 0x7960'be3d'ef8e'263aULL,
				0x0c11'a977'602f'd6f0ULL, 0xcb50'e1ad'16c9'3527ULL, 0xeae2'2e94'035f'fd89ULL,
				0x2866'd12f'5de2'ce1aULL, 0xff1b'1841'ab9b'f390ULL, 0x9f93'39de'8cfe'0d43ULL,
				0x9647'27c8'c48a'0bf7ULL, 0x5245'02c6'aaae'531cULL, 0x9b9c'5ef3'ac10'b413ULL,
				0x4fa2'fa49'42ab'32a5ULL, 0x3f16'5a62'e551'122bULL, 0xc741'48da'76e6'e3d7ULL,
				0x9248'40e5'e464'b2a7ULL, 0xd372'ae43'd697'84daULL, 0x233b'72a1'05e1'1a86ULL,
				0xa48a'0491'4941'a638ULL, 0xb4b6'8525'c9de'7865ULL, 0xddea'baac'a6cf'8002ULL,
				0x0a97'73c2'50b6'bd88ULL, 0xc284'ffbb'5ebd'3393ULL, 0x8ba0'df47'2c8f'6a4eULL,
				0x2aef'6cb7'4d95'1c32ULL, 0x4279'8372'2a31'8d41ULL, 0x73f7'cdff'bf38'9bb2ULL,
				0x074c'0af9'382c'026cULL, 0x8a6a'0f0b'243a'035aULL, 0x6fda'e53c'5f88'931fULL,
				0xc68b'9896'7e53'8ac3ULL, 0x44ff'59c7'1aa8'e639ULL, 0xe2fc'e0ce'439e'9229ULL,
				0xa20c'de24'79d8'cd40ULL, 0x19e8'9fa2'c8eb'd8e9ULL, 0xf446'bbcf'f398'270cULL,
				0x43b3'533e'2284'e455ULL, 0xd82f'0dcd'8e94'5046ULL, 0x5106'6f12'b26c'e820ULL,
				0xe739'57af'6bc5'426dULL, 0x081e'ce5a'40c1'6fa0ULL, 0x3b19'3d4f'c5bf'ab7bULL,
				0x7fe6'6488'df17'4d42ULL, 0x0e98'14ef'7058'04d8ULL, 0x8137'ac85'7c39'd7c6ULL,
				0xb173'3244'e185'a821ULL, 0x695c'3f89'6f11'f867ULL, 0xf6cf'0657'e3ef'f524ULL,
				0x1aab'f276'd029'63d5ULL, 0x2da3'664e'75b9'1e5eULL, 0x0289'bd98'1077'd228ULL,
				0x90c1'fd7d'f413'608fULL, 0x3c55'37b6'fd93'a917ULL, 0xaa12'107e'3919'a2e0ULL,
				0x0686'dab5'3099'6b78ULL, 0xdaa6'b055'9ee3'826eULL, 0xc34e'2ff7'5608'5a87ULL,
				0x6d53'58a4'4fff'4137ULL, 0xfc58'7595'b359'48acULL, 0x7ca5'095c'c7d5'f67eULL,
				0xfb14'7f6c'8b75'4ac0ULL, 0xbfeb'26ab'91dd'acf9ULL, 0x6896'efc5'67a4'9173ULL,
				0xca9a'31e1'1e7c'5c33ULL, 0xbbe4'4186'b133'15a9ULL, 0x0ddb'793b'689a'bfe4ULL,
				0x70b4'a02b'a7fa'208eULL, 0xe47a'3a7b'7307'f951ULL, 0x8cec'd5be'14a3'6822ULL,
				0xeeed'49b9'23b1'44d9ULL, 0x1770'8b4d'b8b3'dc31ULL, 0x6088'219f'2765'fed3ULL,
				0xb3fa'8fdc'f1f2'7a09ULL, 0x910b'2d31'fca6'099bULL, 0x0f52'c4a3'78ed'6dccULL,
				0x50cc'bf5e'bad9'8134ULL, 0x6bd5'8211'7f66'2a4fULL, 0x94ce'9a50'd4fd'd9dfULL,
				0x2b25'bcfb'4520'7526ULL, 0x67c4'2b66'1f49'fcbfULL, 0x4924'20fc'7232'59ddULL,
				0x0343'6dd4'18c2'bb3cULL, 0x1f6e'4517'f872'b391ULL, 0xa085'63bc'69af'1f68ULL,
				0xd43e'a4ba'eebb'86b6ULL, 0x01ca'd04c'08b5'6914ULL, 0xac94'cacb'0980'c998ULL,
				0x54c3'd873'9a37'3864ULL, 0x26fe'c5c0'2dba'cac2ULL, 0xdea9'd778'be0d'3b3eULL,
				0x040f'672d'20ee'b950ULL, 0xe5b0'ea37'7bb2'9045ULL, 0xf30a'b136'cbb4'2560ULL,
				0x6201'9c07'3712'2cfbULL, 0xe86b'930c'1328'2fa1ULL, 0xcc1c'eb54'2ee5'374bULL,
				0x538f'd28a'a21b'3a08ULL, 0x1b61'223a'd89c'0ac1ULL, 0x36c2'4474'ad25'149fULL,
				0x7a23'd3e9'f74c'9d06ULL, 0xbe21'f6e7'9968'c5edULL, 0xcf5f'8680'3627'8c77ULL,
				0xf705'd61b'eb5a'9c30ULL, 0x4d2b'47d1'52dc'e08dULL, 0x5f9e'7bfd'c234'ecf8ULL,
				0x2477'7858'3dcd'18eaULL, 0x867b'a67c'4415'd5aaULL, 0x4ce1'979d'5a69'8999ULL,
				0x0000'0000'0000'0000ULL, 0xec64'f421'33c6'96f1ULL, 0xb57c'5569'c16b'1171ULL,
				0xc1c7'926f'467f'88afULL, 0x654d'96fe'0f3e'2e97ULL, 0x15f9'36d5'a8c4'0e19ULL,
				0xb8a7'2c52'a9f1'ae95ULL, 0xa951'7daa'21db'19dcULL, 0x58d2'7104'fa18'ee94ULL,
				0x5918'a148'f2ad'8780ULL, 0x5cdd'1629'daf6'57c4ULL, 0x8274'c151'64fb'6cfaULL,
				0xd1fb'13db'c6e0'56f2ULL, 0x7d6f'd910'cf60'9f6aULL, 0xb63f'38bd'd9a9'aa4dULL,
				0x3d9f'e7fa'f526'c003ULL, 0x74bb'c706'8714'99deULL, 0xdf63'0734'b6b8'522aULL,
				0x3ad3'ed03'cd0a'c26fULL, 0xfade'af20'83c0'23d4ULL, 0xc00d'4223'4eca'e1bbULL,
				0x8538'cba8'5cd7'6e96ULL, 0xc402'250e'6e24'58ebULL, 0x47bc'3413'026a'5d05ULL,
				0xafd7'a71f'1142'72a4ULL, 0x978d'f784'cc3f'62e3ULL, 0xb96d'fc1e'a144'c781ULL,
				0x21b2'cf39'1596'c8aeULL, 0x318e'4e8d'9509'16f3ULL, 0xce95'56cc'3e92'e563ULL,
				0x385a'509b'dd7d'1047ULL, 0x3581'29a0'b5e7'afa3ULL, 0xe6f3'87e3'6370'2b79ULL,
				0xe075'5d56'53e9'4001ULL, 0x7be9'03a5'fff9'f412ULL, 0x12b5'3c2c'90e8'0c75ULL,
				0x3307'f315'857e'c4dbULL, 0x8faf'b86a'0c61'd31eULL, 0xd9e5'dd81'8621'3952ULL,
				0x77f8'aad2'9fd6'22e2ULL, 0x25bd'a814'3578'71feULL, 0x7571'174a'8fa1'f0caULL,
				0x137f'ec60'985d'6561ULL, 0x3044'9ec1'9dbc'7fe7ULL, 0xa540'd4dd'41f4'cf2cULL,
				0xdc20'6ae0'ae7a'e916ULL, 0x5b91'1cd0'e2da'55a8ULL, 0xb230'5f90'f947'131dULL,
				0x344b'f9ec'bd52'c6b7ULL, 0x5d17'c665'd243'3ed0ULL, 0x1822'4fee'c05e'b1fdULL,
				0x9e59'e992'844b'6457ULL, 0x9a56'8ebf'a4a5'dd07ULL, 0xa3c6'0e68'716d'a454ULL,
				0x7e2c'b4c4'd7a2'2456ULL, 0x87b1'7630'4ca0'bcbeULL, 0x413a'eea6'32f3'367dULL,
				0x9915'e36b'bc67'663bULL, 0x40f0'3eea'3a46'5f69ULL, 0x1c2d'28c3'e0b0'08adULL,
				0x4e68'2a05'4a1e'5bb1ULL, 0x05c5'b761'285b'd044ULL, 0xe1bf'8d1a'5b5c'2915ULL,
				0xf2c0'617a'c301'4c74ULL, 0xb7f5'e8f1'd11c'c359ULL, 0x63cb'4c4b'3fa7'45efULL,
				0x9d1a'8446'9c89'df6bULL, 0xe336'3082'4b2b'fb3dULL, 0xd5f4'74f6'e60e'efa2ULL,
				0xf58c'6b83'fb2d'4e18ULL, 0x4676'e45f'0adf'3411ULL, 0x2078'1f75'1d23'a1baULL,
				0xbd62'9b33'81aa'7ed1ULL, 0xae1d'7753'19f7'1bb0ULL, 0xfed1'c80d'a32e'9a84ULL,
				0x5509'083f'9282'5170ULL, 0x29ac'0163'5557'a70eULL, 0xa7c9'6945'5183'1d04ULL,
				0x8e65'6826'04d4'ba0aULL, 0x11f6'51f8'882a'b749ULL, 0xd77d'c96e'f679'3d8aULL,
				0xef27'99f5'2b04'2dcdULL, 0x48ee'f0b0'7a87'30c9ULL, 0x22f1'a2ed'0d54'7392ULL,
				0x6142'f1d3'2fd0'97c7ULL, 0x4a67'4d28'6af0'e2e1ULL, 0x80fd'7cc9'748c'bed2ULL,
				0x717e'7067'af4f'499aULL, 0x9382'90a9'ecd1'dbb3ULL, 0x88e3'b293'344d'd172ULL,
				0x2734'158c'250f'a3d6ULL}};
		return Ax[a][b];
	}
}

namespace {

static inline void pad(Gost3411_Ctx *CTX) {
	memset(&(CTX->buffer.B[CTX->bufsize]), 0, sizeof(CTX->buffer) - CTX->bufsize);
	CTX->buffer.B[CTX->bufsize] = 1;
}

static void add512(uint512_u *RESTRICT x, const uint512_u *RESTRICT y) {
	if constexpr (sprt::endian::native == sprt::endian::little) {
		unsigned int CF = 0;
		unsigned int i;

#ifdef HAVE_ADDCARRY_U64
		for (i = 0; i < 8; i++) {
			CF = _addcarry_u64(CF, x->QWORD[i], y->QWORD[i], &(x->QWORD[i]));
		}
#else
		for (i = 0; i < 8; i++) {
			const unsigned long long left = x->QWORD[i];
			unsigned long long sum;

			sum = left + y->QWORD[i] + CF;
			/*
			 * (sum == left): is noop, because it's possible only
			 * when `left' is added with `0 + 0' or with `ULLONG_MAX + 1',
			 * in that case `CF' (carry) retain previous value, which is correct,
			 * because when `left + 0 + 0' there was no overflow (thus no carry),
			 * and when `left + ULLONG_MAX + 1' value is wrapped back to
			 * itself with overflow, thus creating carry.
			 *
			 * (sum != left):
			 * if `sum' is not wrapped (sum > left) there should not be carry,
			 * if `sum' is wrapped (sum < left) there should be carry.
			 */
			if (sum != left) {
				CF = (sum < left);
			}
			x->QWORD[i] = sum;
		}
#endif /* !__x86_64__ */
	} else {
		const unsigned char *yp;
		unsigned char *xp;
		unsigned int i;
		int buf;

		xp = (unsigned char *)&x[0];
		yp = (const unsigned char *)&y[0];

		buf = 0;
		for (i = 0; i < 64; i++) {
			buf = xp[i] + yp[i] + (buf >> 8);
			xp[i] = (unsigned char)buf & 0xFF;
		}
	}
}

static void g(union uint512_u *h, const union uint512_u *RESTRICT N,
		const union uint512_u *RESTRICT m) {
#ifdef __GOST3411_LOAD_SSE2__
	simde__m128i xmm0, xmm2, xmm4, xmm6; /* XMMR0-quadruple */
	simde__m128i xmm1, xmm3, xmm5, xmm7; /* XMMR1-quadruple */
	unsigned int i;

	LOAD(N, xmm0, xmm2, xmm4, xmm6);
	XLPS128M(h, xmm0, xmm2, xmm4, xmm6);

	LOAD(m, xmm1, xmm3, xmm5, xmm7);
	XLPS128R(xmm0, xmm2, xmm4, xmm6, xmm1, xmm3, xmm5, xmm7);

	for (i = 0; i < 11; i++) { ROUND128(i, xmm0, xmm2, xmm4, xmm6, xmm1, xmm3, xmm5, xmm7); }

	XLPS128M((&_C(11)), xmm0, xmm2, xmm4, xmm6);
	X128R(xmm0, xmm2, xmm4, xmm6, xmm1, xmm3, xmm5, xmm7);

	X128M(h, xmm0, xmm2, xmm4, xmm6);
	X128M(m, xmm0, xmm2, xmm4, xmm6);

	UNLOAD(h, xmm0, xmm2, xmm4, xmm6);

	/* Restore the Floating-point status on the CPU */
	simde_mm_empty();
#else
	union uint512_u Ki, data;
	unsigned int i;

	XLPS(h, N, (&data));

	/* Starting E() */
	Ki = data;
	XLPS((&Ki), ((const union uint512_u *)&m[0]), (&data));

	for (i = 0; i < 11; i++) { ROUND(i, (&Ki), (&data)); }

	XLPS((&Ki), (&C[11]), (&Ki));
	X((&Ki), (&data), (&data));
	/* E() done */

	X((&data), h, (&data));
	X((&data), ((const union uint512_u *)&m[0]), h);
#endif
}

static inline void stage2(Gost3411_Ctx *CTX, const union uint512_u *data) {
	g(&(CTX->h), &(CTX->N), data);

	add512(&(CTX->N), &_buffer512());
	add512(&(CTX->Sigma), data);
}

static inline void stage3(Gost3411_Ctx *CTX) {
	pad(CTX);
	g(&(CTX->h), &(CTX->N), &(CTX->buffer));
	add512(&(CTX->Sigma), &CTX->buffer);

	memset(&(CTX->buffer.B[0]), 0, sizeof(uint512_u));
	CTX->buffer.QWORD[0] = sprt::byteorder::HostToLittle(CTX->bufsize << 3);
	add512(&(CTX->N), &(CTX->buffer));

	g(&(CTX->h), &_buffer0, &(CTX->N));
	g(&(CTX->h), &_buffer0, &(CTX->Sigma));
}

/*
 * Initialize gost2012 hash context structure
 */
void gost3411_hash_init(Gost3411_Ctx *CTX, const unsigned int digest_size) {
	memset(CTX, 0, sizeof(Gost3411_Ctx));

	CTX->digest_size = digest_size;
	/*
	 * IV for 512-bit hash should be 0^512
	 * IV for 256-bit hash should be (00000001)^64
	 *
	 * It's already zeroed when CTX is cleared above, so we only
	 * need to set it to 0x01-s for 256-bit hash.
	 */
	if (digest_size == 256) {
		memset(&CTX->h, 0x01, sizeof(uint512_u));
	}
}

/*
 * Hash block of arbitrary length
 *
 */
void gost3411_hash_update(Gost3411_Ctx *CTX, const unsigned char *data, size_t len) {
	size_t bufsize = CTX->bufsize;

	if (bufsize == 0) {
		while (len >= 64) {
			memcpy(&CTX->buffer.B[0], data, 64);
			stage2(CTX, &(CTX->buffer));
			data += 64;
			len -= 64;
		}
	}

	while (len) {
		size_t chunksize = 64 - bufsize;
		if (chunksize > len) {
			chunksize = len;
		}

		memcpy(&CTX->buffer.B[bufsize], data, chunksize);

		bufsize += chunksize;
		len -= chunksize;
		data += chunksize;

		if (bufsize == 64) {
			stage2(CTX, &(CTX->buffer));
			bufsize = 0;
		}
	}
	CTX->bufsize = bufsize;
}

/*
 * Compute hash value from current state of ctx
 * state of hash ctx becomes invalid and cannot be used for further
 * hashing.
 */
void gost3411_hash_finish(Gost3411_Ctx *CTX, unsigned char *digest) {
	stage3(CTX);

	CTX->bufsize = 0;

	if (CTX->digest_size == 256) {
		memcpy(digest, &(CTX->h.QWORD[4]), 32);
	} else {
		memcpy(digest, &(CTX->h.QWORD[0]), 64);
	}
}

} // namespace

Gost3411_512::Buf Gost3411_512::make(const CoderSource &source, const StringView &salt) {
	return Gost3411_512()
			.update(salt.empty() ? StringView(SP_SECURE_KEY) : salt)
			.update(source)
			.final();
}

Gost3411_512::Buf Gost3411_512::hmac(const CoderSource &data, const CoderSource &key) {
	Buf ret;
	std::array<uint8_t, Length * 2> keyData = {0};

	Gost3411_512 hashCtx;
	if (key.size() > Length * 2) {
		hashCtx.update(key).final(keyData.data());
	} else {
		memcpy(keyData.data(), key.data(), key.size());
	}

	for (auto &it : keyData) { it ^= HMAC_I_PAD; }

	hashCtx.init().update(keyData).update(data).final(ret.data());

	for (auto &it : keyData) { it ^= HMAC_I_PAD ^ HMAC_O_PAD; }

	hashCtx.init().update(keyData).update(ret).final(ret.data());
	return ret;
}

Gost3411_512::Gost3411_512() { gost3411_hash_init(&ctx, 512); }
Gost3411_512 &Gost3411_512::init() {
	gost3411_hash_init(&ctx, 512);
	return *this;
}

Gost3411_512 &Gost3411_512::update(const uint8_t *ptr, size_t len) {
	if (len > 0) {
		gost3411_hash_update(&ctx, ptr, len);
	}
	return *this;
}

Gost3411_512 &Gost3411_512::update(const CoderSource &source) {
	return update(source.data(), source.size());
}

Gost3411_512::Buf Gost3411_512::final() {
	Gost3411_512::Buf ret;
	gost3411_hash_finish(&ctx, ret.data());
	return ret;
}
void Gost3411_512::final(uint8_t *buf) { gost3411_hash_finish(&ctx, buf); }


Gost3411_256::Buf Gost3411_256::make(const CoderSource &source, const StringView &salt) {
	return Gost3411_256()
			.update(salt.empty() ? StringView(SP_SECURE_KEY) : salt)
			.update(source)
			.final();
}

Gost3411_256::Buf Gost3411_256::hmac(const CoderSource &data, const CoderSource &key) {
	Buf ret;
	std::array<uint8_t, Length * 2> keyData = {0};

	Gost3411_256 hashCtx;
	if (key.size() > Length * 2) {
		hashCtx.update(key).final(keyData.data());
	} else {
		memcpy(keyData.data(), key.data(), key.size());
	}

	for (auto &it : keyData) { it ^= HMAC_I_PAD; }

	hashCtx.init().update(keyData).update(data).final(ret.data());

	for (auto &it : keyData) { it ^= HMAC_I_PAD ^ HMAC_O_PAD; }

	hashCtx.init().update(keyData).update(ret).final(ret.data());
	return ret;
}

Gost3411_256::Gost3411_256() { gost3411_hash_init(&ctx, 256); }
Gost3411_256 &Gost3411_256::init() {
	gost3411_hash_init(&ctx, 256);
	return *this;
}

Gost3411_256 &Gost3411_256::update(const uint8_t *ptr, size_t len) {
	if (len) {
		gost3411_hash_update(&ctx, ptr, len);
	}
	return *this;
}

Gost3411_256 &Gost3411_256::update(const CoderSource &source) {
	return update(source.data(), source.size());
}

Gost3411_256::Buf Gost3411_256::final() {
	Gost3411_256::Buf ret;
	gost3411_hash_finish(&ctx, ret.data());
	return ret;
}
void Gost3411_256::final(uint8_t *buf) { gost3411_hash_finish(&ctx, buf); }

} // namespace stappler::crypto
