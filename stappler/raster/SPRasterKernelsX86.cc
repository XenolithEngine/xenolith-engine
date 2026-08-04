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

#include "SPRasterKernel.h"

// SSE2 and AVX2: the SWAR algorithm with more lanes, and nothing else.
//
// The arithmetic is the one in SPRasterKernelsSwar.cc - unpack bytes to 16-bit lanes, multiply,
// round-divide by 255, pack back - so the results are identical by construction rather than by
// testing. What changes is 2 pixels per iteration, then 4, then 8.
//
// Intrinsics are the native headers, not SIMDe: SIMDe picks its implementation while the
// preprocessor runs, from __AVX2__ and friends, and under __attribute__((target)) those macros are
// not defined. It would compile a portable fallback and call it AVX2 - which is exactly the shape
// of bug that later gets reported as "SIMD did not help".
//
// SSE4.1 exists here now, and only because the bilinear kernel gave it something to be. It was
// refused twice before on the correct grounds: for the constant-source and nearest kernels it buys
// one instruction out of fifteen, and a table entry that measures the same as SSE2 reads as
// evidence that vectorizing does not pay. Bilinear is different in kind - pmulld, pshufb, packusdw
// and cvtepu8 are all SSE4.1, and SSE2 cannot host that kernel at all.

#include "SPRasterAttr.h"

#if SP_RASTER_X86
#include <immintrin.h>
#endif

namespace STAPPLER_VERSIONIZED stappler::raster {

#if SP_RASTER_X86

bool cpuHasSse2();
bool cpuHasSse41();
bool cpuHasAvx2();

// One 4-byte pixel broadcast into every lane of a register.
static inline uint32_t X86_pixel(const uint8_t bytes[4]) {
	uint32_t out;
	__builtin_memcpy(&out, bytes, sizeof(out));
	return out;
}

// -- SSE2 ---------------------------------------------------------------------------------------

// round(x / 255) per 16-bit lane. _mm_srli_epi16 shifts inside each lane, so unlike the SWAR form
// there is no neighbour to mask away.
SP_RASTER_TARGET("sse2")
SP_RASTER_KERNEL_INLINE __m128i X86_divide255(__m128i x) {
	auto t = _mm_add_epi16(x, _mm_set1_epi16(128));
	return _mm_srli_epi16(_mm_add_epi16(t, _mm_srli_epi16(t, 8)), 8);
}

// Four pixels of `dst` under one constant source. srcTerm is src*srcAlpha in 16-bit lanes.
SP_RASTER_TARGET("sse2")
SP_RASTER_KERNEL_INLINE __m128i X86_blendQuad(__m128i dst, __m128i srcTerm, __m128i inverse) {
	auto zero = _mm_setzero_si128();

	auto lo = _mm_unpacklo_epi8(dst, zero);
	auto hi = _mm_unpackhi_epi8(dst, zero);

	// mullo is exact here: every product is below 65536, which is the whole point of the
	// 16-bit-lane bound.
	lo = X86_divide255(_mm_add_epi16(srcTerm, _mm_mullo_epi16(lo, inverse)));
	hi = X86_divide255(_mm_add_epi16(srcTerm, _mm_mullo_epi16(hi, inverse)));

	// packus is the exact inverse of the two unpacks, so bytes come back in their original order.
	return _mm_packus_epi16(lo, hi);
}

SP_RASTER_TARGET("sse2")
static void X86_sse2_fillConstant(uint8_t *dst, uint32_t count, const SpanConstant &src) {
	auto pattern = _mm_set1_epi32(int32_t(X86_pixel(src.bytes)));

	uint32_t i = 0;
	for (; i + 4 <= count; i += 4) {
		_mm_storeu_si128(reinterpret_cast<__m128i *>(dst), pattern);
		dst += 16;
	}
	for (; i < count; ++i) {
		__builtin_memcpy(dst, src.bytes, 4);
		dst += 4;
	}
}

SP_RASTER_TARGET("sse2")
static void X86_sse2_blendConstant(uint8_t *dst, uint32_t count, const SpanConstant &src,
		const ChannelLayout &fmt) {
	auto zero = _mm_setzero_si128();
	auto inverse = _mm_set1_epi16(int16_t(255 - src.alpha));

	auto srcPixels = _mm_set1_epi32(int32_t(X86_pixel(src.bytes)));
	auto srcTerm = _mm_mullo_epi16(_mm_unpacklo_epi8(srcPixels, zero),
			_mm_set1_epi16(int16_t(src.alpha)));

	// 0xFF at the alpha byte of every pixel: what the blend computed there is discarded and the
	// destination's own alpha put back, because the flat contract preserves it.
	uint8_t maskBytes[4] = {0, 0, 0, 0};
	maskBytes[fmt.a] = 0xFF;
	auto alphaMask = _mm_set1_epi32(int32_t(X86_pixel(maskBytes)));

	uint32_t i = 0;
	for (; i + 4 <= count; i += 4) {
		auto d = _mm_loadu_si128(reinterpret_cast<const __m128i *>(dst));
		auto blended = X86_blendQuad(d, srcTerm, inverse);
		_mm_storeu_si128(reinterpret_cast<__m128i *>(dst),
				_mm_or_si128(_mm_andnot_si128(alphaMask, blended), _mm_and_si128(alphaMask, d)));
		dst += 16;
	}

	// Tail through the scalar arithmetic - the same arithmetic, so the seam is invisible.
	for (; i < count; ++i) {
		dst[fmt.r] = Kernels_blend(src.bytes[fmt.r], src.alpha, dst[fmt.r]);
		dst[fmt.g] = Kernels_blend(src.bytes[fmt.g], src.alpha, dst[fmt.g]);
		dst[fmt.b] = Kernels_blend(src.bytes[fmt.b], src.alpha, dst[fmt.b]);
		dst += 4;
	}
}

// -- SSE2, textured -------------------------------------------------------------------------------
//
// Unlike everything above, this path is NOT bit-identical to the scalar sampler, and that is a
// deliberate, bounded trade. The scalar quantizes through double (`int32(double(v)*255 + 0.5)`);
// doing the same here would put four doubles in a register where eight floats fit and throw away
// half the point. The difference is one step in a channel, on partially-covered values only - the
// same tolerance the textured cases already carry against Vulkan, for the same reason.
//
// Everything that does not sample a texture stays exact. The tolerance is granted here and nowhere
// else.

// floor() as an int, without SSE4.1's roundps: truncate, then subtract one where truncation went
// the wrong way. `cmplt` yields all-ones, which is -1, so the correction is an add.
SP_RASTER_TARGET("sse2")
SP_RASTER_KERNEL_INLINE __m128i X86_floorToInt(__m128 x) {
	auto t = _mm_cvttps_epi32(x);
	auto adj = _mm_cmplt_ps(x, _mm_cvtepi32_ps(t));
	return _mm_add_epi32(t, _mm_castps_si128(adj));
}

SP_RASTER_TARGET("sse2")
SP_RASTER_KERNEL_INLINE __m128i X86_clampInt(__m128i v, int32_t hi) {
	// SSE2 has no epi32 min/max; the select is two compares and a blend, which is still cheaper
	// than leaving the lane to a scalar tail.
	auto zero = _mm_setzero_si128();
	auto hiV = _mm_set1_epi32(hi);
	auto lowMask = _mm_cmplt_epi32(v, zero);
	v = _mm_or_si128(_mm_andnot_si128(lowMask, v), _mm_and_si128(lowMask, zero));
	auto highMask = _mm_cmpgt_epi32(v, hiV);
	return _mm_or_si128(_mm_andnot_si128(highMask, v), _mm_and_si128(highMask, hiV));
}

// One channel of four texels, as floats in [0, 1].
SP_RASTER_TARGET("sse2")
SP_RASTER_KERNEL_INLINE __m128 X86_channel(__m128i texels, uint8_t byteIndex) {
	auto shifted = _mm_srli_epi32(texels, byteIndex * 8);
	auto masked = _mm_and_si128(shifted, _mm_set1_epi32(0xFF));
	return _mm_mul_ps(_mm_cvtepi32_ps(masked), _mm_set1_ps(1.0f / 255.0f));
}

// c * 255 + 0.5, truncated and clamped - the float twin of Kernels_toUnorm8.
SP_RASTER_TARGET("sse2")
SP_RASTER_KERNEL_INLINE __m128i X86_quantize(__m128 c) {
	auto scaled = _mm_add_ps(_mm_mul_ps(c, _mm_set1_ps(255.0f)), _mm_set1_ps(0.5f));
	return X86_clampInt(_mm_cvttps_epi32(scaled), 255);
}

// Four pixels of `dst` under four different sources, each with its own alpha. The constant-source
// blend above is this with the source hoisted; the lane arithmetic is the same.
SP_RASTER_TARGET("sse2")
SP_RASTER_KERNEL_INLINE __m128i X86_blendQuadVarying(__m128i dst, __m128i src, __m128i alpha) {
	auto zero = _mm_setzero_si128();

	// Replicate each pixel's alpha into all four of its bytes, so unpacking it lines the lanes up
	// with the colour lanes without a shuffle.
	auto a8 = _mm_or_si128(_mm_or_si128(alpha, _mm_slli_epi32(alpha, 8)),
			_mm_or_si128(_mm_slli_epi32(alpha, 16), _mm_slli_epi32(alpha, 24)));

	auto full = _mm_set1_epi16(255);

	auto srcLo = _mm_unpacklo_epi8(src, zero), srcHi = _mm_unpackhi_epi8(src, zero);
	auto dstLo = _mm_unpacklo_epi8(dst, zero), dstHi = _mm_unpackhi_epi8(dst, zero);
	auto aLo = _mm_unpacklo_epi8(a8, zero), aHi = _mm_unpackhi_epi8(a8, zero);

	auto lo = X86_divide255(_mm_add_epi16(_mm_mullo_epi16(srcLo, aLo),
			_mm_mullo_epi16(dstLo, _mm_sub_epi16(full, aLo))));
	auto hi = X86_divide255(_mm_add_epi16(_mm_mullo_epi16(srcHi, aHi),
			_mm_mullo_epi16(dstHi, _mm_sub_epi16(full, aHi))));

	return _mm_packus_epi16(lo, hi);
}

SP_RASTER_TARGET("sse2")
static void X86_sse2_textureSpan(SpanContext &ctx, const ChannelLayout &fmt, BlendMode blend,
		const TextureSpan &tex) {
	auto dst = ctx.dst;

	const auto widthF = _mm_set1_ps(float(tex.width));
	const auto heightF = _mm_set1_ps(float(tex.height));
	const auto strideI = _mm_set1_epi32(int32_t(tex.stride));

	uint8_t maskBytes[4] = {0, 0, 0, 0};
	maskBytes[fmt.a] = 0xFF;
	const auto alphaMask = _mm_set1_epi32(int32_t(X86_pixel(maskBytes)));

	const uint8_t dstIndex[4] = {fmt.r, fmt.g, fmt.b, fmt.a};

	uint32_t i = 0;
	for (; i + 4 <= ctx.count; i += 4) {
		// Lanes carry the anchor column, not the offset into the run - see SpanContext.
		const uint32_t o = ctx.originOffset + i;
		auto lane = _mm_setr_ps(float(o), float(o + 1), float(o + 2), float(o + 3));

		auto u = _mm_add_ps(_mm_set1_ps(ctx.u), _mm_mul_ps(_mm_set1_ps(ctx.du), lane));
		auto v = _mm_add_ps(_mm_set1_ps(ctx.v), _mm_mul_ps(_mm_set1_ps(ctx.dv), lane));

		auto x = X86_floorToInt(_mm_mul_ps(u, widthF));
		auto y = X86_floorToInt(_mm_mul_ps(v, heightF));

		if (!tex.inRange) {
			if (tex.powerOfTwo) {
				x = _mm_and_si128(x, _mm_set1_epi32(tex.width - 1));
				y = _mm_and_si128(y, _mm_set1_epi32(tex.height - 1));
			} else {
				x = X86_clampInt(x, tex.width - 1);
				y = X86_clampInt(y, tex.height - 1);
			}
		}

		// offset = y*stride + x*4. mullo_epi32 is SSE4.1, so the row offset goes through the
		// 32x32->64 multiply that SSE2 does have, twice.
		auto rowLo = _mm_mul_epu32(y, strideI);
		auto rowHi = _mm_mul_epu32(_mm_srli_si128(y, 4), _mm_srli_si128(strideI, 4));
		auto row = _mm_unpacklo_epi32(_mm_shuffle_epi32(rowLo, 0x08),
				_mm_shuffle_epi32(rowHi, 0x08));
		auto offset = _mm_add_epi32(row, _mm_slli_epi32(x, 2));

		alignas(16) int32_t off[4];
		_mm_store_si128(reinterpret_cast<__m128i *>(off), offset);

		uint32_t raw[4];
		for (int k = 0; k < 4; ++k) { __builtin_memcpy(&raw[k], tex.pixels + off[k], 4); }
		auto texels = _mm_loadu_si128(reinterpret_cast<const __m128i *>(raw));

		// Texel channels in logical R, G, B, A order, then the view's swizzle on top.
		__m128 logical[4];
		for (int k = 0; k < 4; ++k) { logical[k] = X86_channel(texels, tex.src[k]); }

		__m128 shaded[4];
		const float vertex[4] = {ctx.r, ctx.g, ctx.b, ctx.a};
		const float vertexStep[4] = {ctx.dr, ctx.dg, ctx.db, ctx.da};

		for (int k = 0; k < 4; ++k) {
			auto s = tex.swizzle[k];
			auto t = (s >= 0) ? logical[s] : ((s == -1) ? _mm_setzero_ps() : _mm_set1_ps(1.0f));
			auto c = _mm_add_ps(_mm_set1_ps(vertex[k]),
					_mm_mul_ps(_mm_set1_ps(vertexStep[k]), lane));
			shaded[k] = _mm_mul_ps(c, t);
		}

		// Assemble the source pixel in the target's byte order.
		auto src = _mm_setzero_si128();
		__m128i quantized[4];
		for (int k = 0; k < 4; ++k) {
			quantized[k] = X86_quantize(shaded[k]);
			src = _mm_or_si128(src, _mm_slli_epi32(quantized[k], dstIndex[k] * 8));
		}

		auto d = _mm_loadu_si128(reinterpret_cast<const __m128i *>(dst));

		if (blend == BlendMode::Solid) {
			_mm_storeu_si128(reinterpret_cast<__m128i *>(dst), src);
		} else {
			auto blended = X86_blendQuadVarying(d, src, quantized[3]);
			_mm_storeu_si128(reinterpret_cast<__m128i *>(dst),
					_mm_or_si128(_mm_andnot_si128(alphaMask, blended), _mm_and_si128(alphaMask, d)));
		}

		dst += 16;
	}

	// Tail: hand the rest to the scalar sampler by narrowing the context to what is left.
	if (i < ctx.count) {
		// Only where the run starts changes; the attributes stay anchored where they were, so
		// the tail no longer re-bases them and cannot round differently from the vector part.
		SpanContext tail = ctx;
		tail.dst = dst;
		tail.count = ctx.count - i;
		tail.originOffset = ctx.originOffset + i;
		writeSpanScalar(tail, fmt, blend);
	}
}

// -- AVX2 ---------------------------------------------------------------------------------------

SP_RASTER_TARGET("avx2")
SP_RASTER_KERNEL_INLINE __m256i X86_divide255_avx2(__m256i x) {
	auto t = _mm256_add_epi16(x, _mm256_set1_epi16(128));
	return _mm256_srli_epi16(_mm256_add_epi16(t, _mm256_srli_epi16(t, 8)), 8);
}

// Eight pixels at a time. unpack and packus both operate within the two 128-bit halves, and they
// are inverses of each other there, so the halves never need to be permuted back.
SP_RASTER_TARGET("avx2")
SP_RASTER_KERNEL_INLINE __m256i X86_blendOcto(__m256i dst, __m256i srcTerm, __m256i inverse) {
	auto zero = _mm256_setzero_si256();

	auto lo = _mm256_unpacklo_epi8(dst, zero);
	auto hi = _mm256_unpackhi_epi8(dst, zero);

	lo = X86_divide255_avx2(_mm256_add_epi16(srcTerm, _mm256_mullo_epi16(lo, inverse)));
	hi = X86_divide255_avx2(_mm256_add_epi16(srcTerm, _mm256_mullo_epi16(hi, inverse)));

	return _mm256_packus_epi16(lo, hi);
}

SP_RASTER_TARGET("avx2")
static void X86_avx2_fillConstant(uint8_t *dst, uint32_t count, const SpanConstant &src) {
	auto pattern = _mm256_set1_epi32(int32_t(X86_pixel(src.bytes)));

	uint32_t i = 0;
	for (; i + 8 <= count; i += 8) {
		_mm256_storeu_si256(reinterpret_cast<__m256i *>(dst), pattern);
		dst += 32;
	}
	for (; i < count; ++i) {
		__builtin_memcpy(dst, src.bytes, 4);
		dst += 4;
	}
}

SP_RASTER_TARGET("avx2")
static void X86_avx2_blendConstant(uint8_t *dst, uint32_t count, const SpanConstant &src,
		const ChannelLayout &fmt) {
	auto zero = _mm256_setzero_si256();
	auto inverse = _mm256_set1_epi16(int16_t(255 - src.alpha));

	auto srcPixels = _mm256_set1_epi32(int32_t(X86_pixel(src.bytes)));
	auto srcTerm = _mm256_mullo_epi16(_mm256_unpacklo_epi8(srcPixels, zero),
			_mm256_set1_epi16(int16_t(src.alpha)));

	uint8_t maskBytes[4] = {0, 0, 0, 0};
	maskBytes[fmt.a] = 0xFF;
	auto alphaMask = _mm256_set1_epi32(int32_t(X86_pixel(maskBytes)));

	uint32_t i = 0;
	for (; i + 8 <= count; i += 8) {
		auto d = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(dst));
		auto blended = X86_blendOcto(d, srcTerm, inverse);
		_mm256_storeu_si256(reinterpret_cast<__m256i *>(dst),
				_mm256_or_si256(_mm256_andnot_si256(alphaMask, blended),
						_mm256_and_si256(alphaMask, d)));
		dst += 32;
	}

	for (; i < count; ++i) {
		dst[fmt.r] = Kernels_blend(src.bytes[fmt.r], src.alpha, dst[fmt.r]);
		dst[fmt.g] = Kernels_blend(src.bytes[fmt.g], src.alpha, dst[fmt.g]);
		dst[fmt.b] = Kernels_blend(src.bytes[fmt.b], src.alpha, dst[fmt.b]);
		dst += 4;
	}
}

// -- AVX2, textured -------------------------------------------------------------------------------

SP_RASTER_TARGET("avx2")
SP_RASTER_KERNEL_INLINE __m256i X86_floorToInt_avx2(__m256 x) {
	// vroundps is AVX, not AVX2-only, so the truncate-and-correct dance is unnecessary here.
	return _mm256_cvtps_epi32(_mm256_round_ps(x, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC));
}

SP_RASTER_TARGET("avx2")
SP_RASTER_KERNEL_INLINE __m256 X86_channel_avx2(__m256i texels, uint8_t byteIndex) {
	auto masked = _mm256_and_si256(_mm256_srli_epi32(texels, byteIndex * 8),
			_mm256_set1_epi32(0xFF));
	return _mm256_mul_ps(_mm256_cvtepi32_ps(masked), _mm256_set1_ps(1.0f / 255.0f));
}

SP_RASTER_TARGET("avx2")
SP_RASTER_KERNEL_INLINE __m256i X86_quantize_avx2(__m256 c) {
	auto scaled = _mm256_add_ps(_mm256_mul_ps(c, _mm256_set1_ps(255.0f)), _mm256_set1_ps(0.5f));
	auto i = _mm256_cvttps_epi32(scaled);
	i = _mm256_max_epi32(i, _mm256_setzero_si256());
	return _mm256_min_epi32(i, _mm256_set1_epi32(255));
}

SP_RASTER_TARGET("avx2")
SP_RASTER_KERNEL_INLINE __m256i X86_blendOctoVarying(__m256i dst, __m256i src, __m256i alpha) {
	auto zero = _mm256_setzero_si256();

	auto a8 = _mm256_or_si256(_mm256_or_si256(alpha, _mm256_slli_epi32(alpha, 8)),
			_mm256_or_si256(_mm256_slli_epi32(alpha, 16), _mm256_slli_epi32(alpha, 24)));

	auto full = _mm256_set1_epi16(255);

	auto srcLo = _mm256_unpacklo_epi8(src, zero), srcHi = _mm256_unpackhi_epi8(src, zero);
	auto dstLo = _mm256_unpacklo_epi8(dst, zero), dstHi = _mm256_unpackhi_epi8(dst, zero);
	auto aLo = _mm256_unpacklo_epi8(a8, zero), aHi = _mm256_unpackhi_epi8(a8, zero);

	auto lo = X86_divide255_avx2(_mm256_add_epi16(_mm256_mullo_epi16(srcLo, aLo),
			_mm256_mullo_epi16(dstLo, _mm256_sub_epi16(full, aLo))));
	auto hi = X86_divide255_avx2(_mm256_add_epi16(_mm256_mullo_epi16(srcHi, aHi),
			_mm256_mullo_epi16(dstHi, _mm256_sub_epi16(full, aHi))));

	return _mm256_packus_epi16(lo, hi);
}

SP_RASTER_TARGET("avx2")
static void X86_avx2_textureSpan(SpanContext &ctx, const ChannelLayout &fmt, BlendMode blend,
		const TextureSpan &tex) {
	auto dst = ctx.dst;

	const auto widthF = _mm256_set1_ps(float(tex.width));
	const auto heightF = _mm256_set1_ps(float(tex.height));

	uint8_t maskBytes[4] = {0, 0, 0, 0};
	maskBytes[fmt.a] = 0xFF;
	const auto alphaMask = _mm256_set1_epi32(int32_t(X86_pixel(maskBytes)));

	const uint8_t dstIndex[4] = {fmt.r, fmt.g, fmt.b, fmt.a};
	const float vertex[4] = {ctx.r, ctx.g, ctx.b, ctx.a};
	const float vertexStep[4] = {ctx.dr, ctx.dg, ctx.db, ctx.da};

	uint32_t i = 0;
	for (; i + 8 <= ctx.count; i += 8) {
		// Lanes carry the anchor column, not the offset into the run - see SpanContext.
		auto lane = _mm256_add_ps(_mm256_set1_ps(float(ctx.originOffset + i)),
				_mm256_setr_ps(0, 1, 2, 3, 4, 5, 6, 7));

		auto u = _mm256_add_ps(_mm256_set1_ps(ctx.u), _mm256_mul_ps(_mm256_set1_ps(ctx.du), lane));
		auto v = _mm256_add_ps(_mm256_set1_ps(ctx.v), _mm256_mul_ps(_mm256_set1_ps(ctx.dv), lane));

		auto x = X86_floorToInt_avx2(_mm256_mul_ps(u, widthF));
		auto y = X86_floorToInt_avx2(_mm256_mul_ps(v, heightF));

		if (!tex.inRange) {
			if (tex.powerOfTwo) {
				x = _mm256_and_si256(x, _mm256_set1_epi32(tex.width - 1));
				y = _mm256_and_si256(y, _mm256_set1_epi32(tex.height - 1));
			} else {
				x = _mm256_min_epi32(_mm256_max_epi32(x, _mm256_setzero_si256()),
						_mm256_set1_epi32(tex.width - 1));
				y = _mm256_min_epi32(_mm256_max_epi32(y, _mm256_setzero_si256()),
						_mm256_set1_epi32(tex.height - 1));
			}
		}

		// AVX2 has both the 32-bit multiply and the gather that SSE2 lacks, which is the whole
		// reason this variant exists rather than reusing the 128-bit one.
		auto offset = _mm256_add_epi32(_mm256_mullo_epi32(y, _mm256_set1_epi32(int32_t(tex.stride))),
				_mm256_slli_epi32(x, 2));
		auto texels = _mm256_i32gather_epi32(reinterpret_cast<const int *>(tex.pixels), offset, 1);

		__m256 logical[4];
		for (int k = 0; k < 4; ++k) { logical[k] = X86_channel_avx2(texels, tex.src[k]); }

		auto src = _mm256_setzero_si256();
		__m256i quantized[4];
		for (int k = 0; k < 4; ++k) {
			auto s = tex.swizzle[k];
			auto t = (s >= 0) ? logical[s]
							  : ((s == -1) ? _mm256_setzero_ps() : _mm256_set1_ps(1.0f));
			auto c = _mm256_add_ps(_mm256_set1_ps(vertex[k]),
					_mm256_mul_ps(_mm256_set1_ps(vertexStep[k]), lane));
			quantized[k] = X86_quantize_avx2(_mm256_mul_ps(c, t));
			src = _mm256_or_si256(src, _mm256_slli_epi32(quantized[k], dstIndex[k] * 8));
		}

		auto d = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(dst));

		if (blend == BlendMode::Solid) {
			_mm256_storeu_si256(reinterpret_cast<__m256i *>(dst), src);
		} else {
			auto blended = X86_blendOctoVarying(d, src, quantized[3]);
			_mm256_storeu_si256(reinterpret_cast<__m256i *>(dst),
					_mm256_or_si256(_mm256_andnot_si256(alphaMask, blended),
							_mm256_and_si256(alphaMask, d)));
		}

		dst += 32;
	}

	if (i < ctx.count) {
		// Only where the run starts changes; the attributes stay anchored where they were, so
		// the tail no longer re-bases them and cannot round differently from the vector part.
		SpanContext tail = ctx;
		tail.dst = dst;
		tail.count = ctx.count - i;
		tail.originOffset = ctx.originOffset + i;
		// Four at a time for what is left, then one at a time for the last three.
		X86_sse2_textureSpan(tail, fmt, blend, tex);
	}
}

// -- AVX2, bilinear -------------------------------------------------------------------------------
//
// Vector across CHANNELS, not across pixels - four channels of one pixel per 128-bit register.
//
// The other way round is the reflex, and it is the wrong one here. Bilinear needs t00 and t01
// paired for the vertical filter and t00/t10 for the horizontal, and with pixels in the lanes
// those pairs land four lanes apart and every step turns into a shuffle. With channels in the
// lanes the pairs are already adjacent in memory: gating on `inRange` makes t00 and t10 two
// consecutive texels, so one eight-byte load delivers both, and the whole filter is three
// multiplies and two adds with no lane movement at all.
//
// The weights need a 32-bit multiply (11 fractional bits put the horizontal product over 16), so
// this path is pmulld and therefore SSE4.1 - which is what finally gives that instruction set
// something to be, after two milestones of correctly refusing it.

SP_RASTER_TARGET("sse4.1")
SP_RASTER_KERNEL_INLINE __m128i X86_widenLow(__m128i bytes) {
	auto zero = _mm_setzero_si128();
	return _mm_unpacklo_epi16(_mm_unpacklo_epi8(bytes, zero), zero);
}

SP_RASTER_TARGET("sse4.1")
static void X86_textureSpanLinear(SpanContext &ctx, const ChannelLayout &fmt, BlendMode blend,
		const TextureSpan &tex) {
	auto dst = ctx.dst;

	const uint8_t dstIndex[4] = {fmt.r, fmt.g, fmt.b, fmt.a};
	const float vertex[4] = {ctx.r, ctx.g, ctx.b, ctx.a};
	const float vertexStep[4] = {ctx.dr, ctx.dg, ctx.db, ctx.da};

	// The source channel order, as a byte shuffle: the texel arrives in its storage order and has
	// to come out as logical R G B A before the swizzle can index it.
	const auto srcOrder = _mm_setr_epi8(int8_t(tex.src[0]), int8_t(tex.src[1]), int8_t(tex.src[2]),
			int8_t(tex.src[3]), int8_t(tex.src[0] + 4), int8_t(tex.src[1] + 4),
			int8_t(tex.src[2] + 4), int8_t(tex.src[3] + 4), -1, -1, -1, -1, -1, -1, -1, -1);

	constexpr int32_t half = 1 << (2 * SampleWeightBits - 1);

	// The shading stage, resolved once. The swizzle becomes a byte shuffle plus a mask for the
	// channels that are the constant One; -1 in a pshufb index yields zero, which covers Zero.
	alignas(16) int8_t swizzleBytes[16];
	alignas(16) int8_t oneBytes[16];
	for (int k = 0; k < 16; ++k) {
		swizzleBytes[k] = -1;
		oneBytes[k] = 0;
	}
	for (int k = 0; k < 4; ++k) {
		auto s = tex.swizzle[k];
		if (s >= 0) {
			swizzleBytes[k] = int8_t(s);
		} else if (s == -2) {
			oneBytes[k] = int8_t(0xFF);
		}
	}
	const auto swizzleShuffle = _mm_load_si128(reinterpret_cast<const __m128i *>(swizzleBytes));
	const auto oneMask = _mm_load_si128(reinterpret_cast<const __m128i *>(oneBytes));

	// Where each shaded channel lands in the target pixel, as one more byte shuffle.
	alignas(16) int8_t storeBytes[16];
	for (int k = 0; k < 16; ++k) { storeBytes[k] = -1; }
	for (int k = 0; k < 4; ++k) { storeBytes[dstIndex[k]] = int8_t(k); }
	const auto storeShuffle = _mm_load_si128(reinterpret_cast<const __m128i *>(storeBytes));

	const auto vertexVec = _mm_setr_ps(vertex[0], vertex[1], vertex[2], vertex[3]);
	const auto vertexStepVec =
			_mm_setr_ps(vertexStep[0], vertexStep[1], vertexStep[2], vertexStep[3]);
	const auto zero = _mm_setzero_si128();

	// The counter runs in anchor columns, not in offsets into the run - see SpanContext.
	for (uint32_t i = ctx.originOffset, end = ctx.originOffset + ctx.count; i < end; ++i) {
		auto fx = (ctx.u + ctx.du * float(i)) * float(tex.width) - 0.5f;
		auto fy = (ctx.v + ctx.dv * float(i)) * float(tex.height) - 0.5f;

		auto x0 = int32_t(__builtin_floorf(fx));
		auto y0 = int32_t(__builtin_floorf(fy));

		auto wx = Sample_weight(fx, x0);
		auto wy = Sample_weight(fy, y0);

		auto row0 = tex.pixels + size_t(y0) * tex.stride + size_t(x0) * 4;
		auto row1 = row0 + tex.stride;

		// Eight bytes each: (t00, t10) and (t01, t11), already adjacent because the span was
		// checked to stay inside the texture.
		auto q0 = _mm_shuffle_epi8(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(row0)),
				srcOrder);
		auto q1 = _mm_shuffle_epi8(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(row1)),
				srcOrder);

		auto a = X86_widenLow(q0);
		auto b = X86_widenLow(_mm_srli_si128(q0, 4));
		auto c = X86_widenLow(q1);
		auto d = X86_widenLow(_mm_srli_si128(q1, 4));

		auto ix = _mm_set1_epi32(SampleWeightOne - wx);
		auto wxv = _mm_set1_epi32(wx);
		auto iy = _mm_set1_epi32(SampleWeightOne - wy);
		auto wyv = _mm_set1_epi32(wy);

		auto top = _mm_add_epi32(_mm_mullo_epi32(a, ix), _mm_mullo_epi32(b, wxv));
		auto bottom = _mm_add_epi32(_mm_mullo_epi32(c, ix), _mm_mullo_epi32(d, wxv));

		auto filtered = _mm_add_epi32(_mm_mullo_epi32(top, iy), _mm_mullo_epi32(bottom, wyv));
		filtered = _mm_srai_epi32(_mm_add_epi32(filtered, _mm_set1_epi32(half)),
				2 * SampleWeightBits);

		// Out of the filter and through the shading stage without ever leaving the register.
		// The scalar form quantized each channel through a double conversion, and four of those
		// per pixel were costing more than the filter they followed.
		auto texelBytes = _mm_packus_epi16(_mm_packus_epi32(filtered, zero), zero);
		auto swizzled = _mm_or_si128(_mm_shuffle_epi8(texelBytes, swizzleShuffle), oneMask);

		auto texf = _mm_mul_ps(_mm_cvtepi32_ps(_mm_cvtepu8_epi32(swizzled)),
				_mm_set1_ps(1.0f / 255.0f));
		auto colour = _mm_add_ps(vertexVec, _mm_mul_ps(vertexStepVec, _mm_set1_ps(float(i))));

		auto scaled = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(colour, texf), _mm_set1_ps(255.0f)),
				_mm_set1_ps(0.5f));
		auto q = _mm_min_epi32(_mm_max_epi32(_mm_cvttps_epi32(scaled), zero),
				_mm_set1_epi32(255));

		auto shadedBytes = _mm_shuffle_epi8(_mm_packus_epi16(_mm_packus_epi32(q, zero), zero),
				storeShuffle);

		if (blend == BlendMode::Solid) {
			_mm_storeu_si32(dst, shadedBytes);
		} else {
			alignas(16) uint8_t shaded[16];
			_mm_store_si128(reinterpret_cast<__m128i *>(shaded), shadedBytes);

			uint32_t sa = shaded[fmt.a];
			if (sa == 255) {
				dst[fmt.r] = shaded[fmt.r];
				dst[fmt.g] = shaded[fmt.g];
				dst[fmt.b] = shaded[fmt.b];
			} else if (sa != 0) {
				dst[fmt.r] = Kernels_blend(shaded[fmt.r], sa, dst[fmt.r]);
				dst[fmt.g] = Kernels_blend(shaded[fmt.g], sa, dst[fmt.g]);
				dst[fmt.b] = Kernels_blend(shaded[fmt.b], sa, dst[fmt.b]);
			}
		}

		dst += 4;
	}
}

// Bilinear needs the span to stay inside the texture, because it reads t00 and t10 as one
// eight-byte load. Outside that it goes to the scalar path and its column cache.
SP_RASTER_TARGET("sse4.1")
SP_RASTER_KERNEL_INLINE bool X86_tryBilinear(SpanContext &ctx, const ChannelLayout &fmt,
		BlendMode blend) {
	if (ctx.count == 0 || fmt.size != 4 || isConstantSpan(ctx)) {
		return false;
	}

	TextureSpan tex;
	if (!resolveTextureSpan(ctx, fmt, tex) || !tex.linear || !tex.inRange) {
		return false;
	}

	X86_textureSpanLinear(ctx, fmt, blend, tex);
	return true;
}

// -- span and rect entry points -----------------------------------------------------------------

// The opaque case: colour bytes are a plain store, but destination alpha still has to survive.
template <typename Fill>
static inline void X86_writeSpanImpl(SpanContext &ctx, const ChannelLayout &fmt, BlendMode blend,
		Fill &&fill,
		void (*blendConstant)(uint8_t *, uint32_t, const SpanConstant &, const ChannelLayout &),
		void (*textureSpan)(SpanContext &, const ChannelLayout &, BlendMode, const TextureSpan &)) {
	if (ctx.count == 0 || fmt.size == 0) {
		return;
	}

	if (fmt.size != 4 || !isConstantSpan(ctx)) {
		// A textured run that this set can sample goes to the vector sampler; anything else -
		// linear filtering, an array texture, a wrap it cannot do - to the scalar one.
		// Bilinear is not this kernel's: it samples one texel. A linear span falls through to the
		// scalar set, which has the column-caching loop for it.
		TextureSpan tex;
		if (textureSpan && resolveTextureSpan(ctx, fmt, tex) && !tex.linear) {
			textureSpan(ctx, fmt, blend, tex);
			return;
		}

		writeSpanScalar(ctx, fmt, blend);
		return;
	}

	auto src = getSpanConstant(ctx, fmt);

	if (blend == BlendMode::Solid) {
		fill(ctx.dst, ctx.count, src);
		return;
	}

	if (src.alpha == 0) {
		return;
	}

	if (src.alpha == 255) {
		auto dst = ctx.dst;
		for (uint32_t i = 0; i < ctx.count; ++i) {
			dst[fmt.r] = src.bytes[fmt.r];
			dst[fmt.g] = src.bytes[fmt.g];
			dst[fmt.b] = src.bytes[fmt.b];
			dst += 4;
		}
		return;
	}

	blendConstant(ctx.dst, ctx.count, src, fmt);
}

template <typename Fill>
static inline void X86_fillRectImpl(const Target &target, const URect &rect,
		const ChannelLayout &fmt, const Color4F &color, Fill &&fill) {
	if (fmt.size != 4) {
		fillRectScalar(target, rect, fmt, color);
		return;
	}

	SpanConstant src;
	src.bytes[fmt.r] = Kernels_toUnorm8(color.r);
	src.bytes[fmt.g] = Kernels_toUnorm8(color.g);
	src.bytes[fmt.b] = Kernels_toUnorm8(color.b);
	src.bytes[fmt.a] = Kernels_toUnorm8(color.a);

	for (uint32_t y = rect.y; y < rect.y + rect.height; ++y) {
		fill(target.pixels + size_t(y) * size_t(target.stride) + size_t(rect.x) * 4, rect.width,
				src);
	}
}

SP_RASTER_TARGET("sse2")
SP_RASTER_KERNEL_FN
static void X86_sse2_writeSpan(SpanContext &ctx, const ChannelLayout &fmt, BlendMode blend) {
	X86_writeSpanImpl(ctx, fmt, blend, &X86_sse2_fillConstant, &X86_sse2_blendConstant,
			&X86_sse2_textureSpan);
}

SP_RASTER_TARGET("sse2")
SP_RASTER_KERNEL_FN
static void X86_sse2_fillRect(const Target &target, const URect &rect, const ChannelLayout &fmt,
		const Color4F &color) {
	X86_fillRectImpl(target, rect, fmt, color, &X86_sse2_fillConstant);
}

// The target attribute is not decoration here. SP_RASTER_KERNEL_FN carries `flatten`, so the AVX2
// kernels below get inlined into this function - and a function without the target cannot host
// them. Without it clang does not diagnose anything at the front end: it accepts the code and then
// the backend fails to select the instruction.
SP_RASTER_TARGET("avx2")
SP_RASTER_KERNEL_FN
static void X86_avx2_writeSpan(SpanContext &ctx, const ChannelLayout &fmt, BlendMode blend) {
	if (X86_tryBilinear(ctx, fmt, blend)) {
		return;
	}

	X86_writeSpanImpl(ctx, fmt, blend, &X86_avx2_fillConstant, &X86_avx2_blendConstant,
			&X86_avx2_textureSpan);
}

SP_RASTER_TARGET("avx2")
SP_RASTER_KERNEL_FN
static void X86_avx2_fillRect(const Target &target, const URect &rect, const ChannelLayout &fmt,
		const Color4F &color) {
	X86_fillRectImpl(target, rect, fmt, color, &X86_avx2_fillConstant);
}

const KernelTable *getSse2Kernels() {
	if (!cpuHasSse2()) {
		return nullptr;
	}
	static const KernelTable s_table{
		KernelSet::Sse2,
		&X86_sse2_writeSpan,
		&blitGlyphScalar,
		&X86_sse2_fillRect,
	};
	return &s_table;
}

// SSE4.1 finally has content, and it is the bilinear kernel: pmulld for the weights, pshufb for
// the swizzle, packusdw and cvtepu8 for the round trip through bytes. SSE2 has none of those, so
// on an SSE2-only machine bilinear stays on the scalar column-cached path - which is why this set
// exists rather than being folded into that one.
//
// Its constant-source and nearest kernels are the SSE2 ones: those genuinely gain nothing here,
// which was the correct reason to refuse this set twice before.
SP_RASTER_TARGET("sse4.1")
SP_RASTER_KERNEL_FN
static void X86_sse41_writeSpan(SpanContext &ctx, const ChannelLayout &fmt, BlendMode blend) {
	if (X86_tryBilinear(ctx, fmt, blend)) {
		return;
	}

	X86_writeSpanImpl(ctx, fmt, blend, &X86_sse2_fillConstant, &X86_sse2_blendConstant,
			&X86_sse2_textureSpan);
}

const KernelTable *getSse41Kernels() {
	if (!cpuHasSse41()) {
		return nullptr;
	}
	static const KernelTable s_table{
		KernelSet::Sse41,
		&X86_sse41_writeSpan,
		&blitGlyphScalar,
		&X86_sse2_fillRect,
	};
	return &s_table;
}

const KernelTable *getAvx2Kernels() {
	if (!cpuHasAvx2()) {
		return nullptr;
	}
	static const KernelTable s_table{
		KernelSet::Avx2,
		&X86_avx2_writeSpan,
		&blitGlyphScalar,
		&X86_avx2_fillRect,
	};
	return &s_table;
}

#else

const KernelTable *getSse2Kernels() { return nullptr; }
const KernelTable *getSse41Kernels() { return nullptr; }
const KernelTable *getAvx2Kernels() { return nullptr; }

#endif

} // namespace stappler::raster
