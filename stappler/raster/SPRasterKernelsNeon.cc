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
#include "SPRasterAttr.h"

// NEON: four pixels at a time, same algorithm again.
//
// Unlike x86 there is no runtime question to ask. Advanced SIMD is part of the AArch64 base
// architecture, so a probe could only answer "yes" - and anything beyond it (dotprod, i8mm, SVE)
// would need HWCAP from the ELF auxiliary vector, which sprt does not expose at all. So this set
// is compiled in and selected at compile time, while still going through the same table and the
// same byte-for-byte parity gate as the x86 ones.

#if SP_RASTER_NEON
#include <arm_neon.h>
#endif

namespace STAPPLER_VERSIONIZED stappler::raster {

#if SP_RASTER_NEON

static inline uint32_t Neon_pixel(const uint8_t bytes[4]) {
	uint32_t out;
	__builtin_memcpy(&out, bytes, sizeof(out));
	return out;
}

// round(x / 255) per 16-bit lane.
SP_RASTER_KERNEL_INLINE uint16x8_t Neon_divide255(uint16x8_t x) {
	auto t = vaddq_u16(x, vdupq_n_u16(128));
	return vshrq_n_u16(vaddq_u16(t, vshrq_n_u16(t, 8)), 8);
}

// Four pixels of `dst` under one constant source. vmovl/vmovn split and rejoin the byte lanes the
// same way x86's unpack/packus do.
SP_RASTER_KERNEL_INLINE uint8x16_t Neon_blendQuad(uint8x16_t dst, uint16x8_t srcTerm,
		uint16x8_t inverse) {
	auto lo = vmovl_u8(vget_low_u8(dst));
	auto hi = vmovl_u8(vget_high_u8(dst));

	lo = Neon_divide255(vaddq_u16(srcTerm, vmulq_u16(lo, inverse)));
	hi = Neon_divide255(vaddq_u16(srcTerm, vmulq_u16(hi, inverse)));

	return vcombine_u8(vmovn_u16(lo), vmovn_u16(hi));
}

static void Neon_fillConstant(uint8_t *dst, uint32_t count, const SpanConstant &src) {
	auto pattern = vreinterpretq_u8_u32(vdupq_n_u32(Neon_pixel(src.bytes)));

	uint32_t i = 0;
	for (; i + 4 <= count; i += 4) {
		vst1q_u8(dst, pattern);
		dst += 16;
	}
	for (; i < count; ++i) {
		__builtin_memcpy(dst, src.bytes, 4);
		dst += 4;
	}
}

static void Neon_blendConstant(uint8_t *dst, uint32_t count, const SpanConstant &src,
		const ChannelLayout &fmt) {
	auto inverse = vdupq_n_u16(uint16_t(255 - src.alpha));

	auto srcPixels = vreinterpretq_u8_u32(vdupq_n_u32(Neon_pixel(src.bytes)));
	auto srcTerm = vmulq_u16(vmovl_u8(vget_low_u8(srcPixels)), vdupq_n_u16(uint16_t(src.alpha)));

	uint8_t maskBytes[4] = {0, 0, 0, 0};
	maskBytes[fmt.a] = 0xFF;
	auto alphaMask = vreinterpretq_u8_u32(vdupq_n_u32(Neon_pixel(maskBytes)));

	uint32_t i = 0;
	for (; i + 4 <= count; i += 4) {
		auto d = vld1q_u8(dst);
		// vbslq picks per bit: destination alpha where the mask is set, the blend everywhere else.
		vst1q_u8(dst, vbslq_u8(alphaMask, d, Neon_blendQuad(d, srcTerm, inverse)));
		dst += 16;
	}

	for (; i < count; ++i) {
		dst[fmt.r] = Kernels_blend(src.bytes[fmt.r], src.alpha, dst[fmt.r]);
		dst[fmt.g] = Kernels_blend(src.bytes[fmt.g], src.alpha, dst[fmt.g]);
		dst[fmt.b] = Kernels_blend(src.bytes[fmt.b], src.alpha, dst[fmt.b]);
		dst += 4;
	}
}

// -- textured -------------------------------------------------------------------------------------
//
// Same shape as the x86 sampler, and the same bounded trade: the quantization runs in float rather
// than double, which can cost one step in a channel. Everything that does not sample a texture
// stays bit-identical to the scalar reference.

SP_RASTER_KERNEL_INLINE float32x4_t Neon_channel(uint32x4_t texels, uint8_t byteIndex) {
	// The byte index is a per-span constant, not a literal, so the shift has to be the variable
	// form: NEON spells a right shift as a left shift by a negative count.
	auto shifted = vshlq_u32(texels, vdupq_n_s32(-int32_t(byteIndex) * 8));
	auto masked = vandq_u32(shifted, vdupq_n_u32(0xFF));
	return vmulq_n_f32(vcvtq_f32_u32(masked), 1.0f / 255.0f);
}

SP_RASTER_KERNEL_INLINE uint32x4_t Neon_quantize(float32x4_t c) {
	auto scaled = vaddq_f32(vmulq_n_f32(c, 255.0f), vdupq_n_f32(0.5f));
	// vcvtq_u32_f32 truncates and saturates at zero, so only the upper clamp is left to do.
	return vminq_u32(vcvtq_u32_f32(vmaxq_f32(scaled, vdupq_n_f32(0.0f))), vdupq_n_u32(255));
}

SP_RASTER_KERNEL_INLINE uint8x16_t Neon_blendQuadVarying(uint8x16_t dst, uint8x16_t src,
		uint32x4_t alpha) {
	// Each pixel's alpha into all four of its bytes, so the unpack lines it up with the colours.
	auto a8 = vreinterpretq_u8_u32(vorrq_u32(vorrq_u32(alpha, vshlq_n_u32(alpha, 8)),
			vorrq_u32(vshlq_n_u32(alpha, 16), vshlq_n_u32(alpha, 24))));

	auto full = vdupq_n_u16(255);

	auto srcLo = vmovl_u8(vget_low_u8(src)), srcHi = vmovl_u8(vget_high_u8(src));
	auto dstLo = vmovl_u8(vget_low_u8(dst)), dstHi = vmovl_u8(vget_high_u8(dst));
	auto aLo = vmovl_u8(vget_low_u8(a8)), aHi = vmovl_u8(vget_high_u8(a8));

	auto lo = Neon_divide255(
			vaddq_u16(vmulq_u16(srcLo, aLo), vmulq_u16(dstLo, vsubq_u16(full, aLo))));
	auto hi = Neon_divide255(
			vaddq_u16(vmulq_u16(srcHi, aHi), vmulq_u16(dstHi, vsubq_u16(full, aHi))));

	return vcombine_u8(vmovn_u16(lo), vmovn_u16(hi));
}

static void Neon_textureSpan(SpanContext &ctx, const ChannelLayout &fmt, BlendMode blend,
		const TextureSpan &tex) {
	auto dst = ctx.dst;

	uint8_t maskBytes[4] = {0, 0, 0, 0};
	maskBytes[fmt.a] = 0xFF;
	const auto alphaMask = vreinterpretq_u8_u32(vdupq_n_u32(Neon_pixel(maskBytes)));

	const uint8_t dstIndex[4] = {fmt.r, fmt.g, fmt.b, fmt.a};
	const float vertex[4] = {ctx.r, ctx.g, ctx.b, ctx.a};
	const float vertexStep[4] = {ctx.dr, ctx.dg, ctx.db, ctx.da};

	uint32_t i = 0;
	for (; i + 4 <= ctx.count; i += 4) {
		// Lanes carry the anchor column, not the offset into the run - see SpanContext.
		const uint32_t o = ctx.originOffset + i;
		float laneValues[4] = {float(o), float(o + 1), float(o + 2), float(o + 3)};
		auto lane = vld1q_f32(laneValues);

		auto u = vaddq_f32(vdupq_n_f32(ctx.u), vmulq_n_f32(lane, ctx.du));
		auto v = vaddq_f32(vdupq_n_f32(ctx.v), vmulq_n_f32(lane, ctx.dv));

		// vcvtq_s32_f32 truncates toward zero; floor needs the correction where it went up.
		auto toFloor = [](float32x4_t f) {
			auto t = vcvtq_s32_f32(f);
			// The compare yields all-ones where the truncation went up, and all-ones is -1.
			auto adj = vcltq_f32(f, vcvtq_f32_s32(t));
			return vaddq_s32(t, vreinterpretq_s32_u32(adj));
		};

		auto x = toFloor(vmulq_n_f32(u, float(tex.width)));
		auto y = toFloor(vmulq_n_f32(v, float(tex.height)));

		if (!tex.inRange) {
			if (tex.powerOfTwo) {
				x = vandq_s32(x, vdupq_n_s32(tex.width - 1));
				y = vandq_s32(y, vdupq_n_s32(tex.height - 1));
			} else {
				x = vminq_s32(vmaxq_s32(x, vdupq_n_s32(0)), vdupq_n_s32(tex.width - 1));
				y = vminq_s32(vmaxq_s32(y, vdupq_n_s32(0)), vdupq_n_s32(tex.height - 1));
			}
		}

		auto offset = vaddq_s32(vmulq_n_s32(y, int32_t(tex.stride)), vshlq_n_s32(x, 2));

		int32_t off[4];
		vst1q_s32(off, offset);

		uint32_t raw[4];
		for (int k = 0; k < 4; ++k) { __builtin_memcpy(&raw[k], tex.pixels + off[k], 4); }
		auto texels = vld1q_u32(raw);

		float32x4_t logical[4];
		for (int k = 0; k < 4; ++k) { logical[k] = Neon_channel(texels, tex.src[k]); }

		auto src = vdupq_n_u32(0);
		uint32x4_t quantized[4];
		for (int k = 0; k < 4; ++k) {
			auto s = tex.swizzle[k];
			auto t = (s >= 0) ? logical[s]
							  : ((s == -1) ? vdupq_n_f32(0.0f) : vdupq_n_f32(1.0f));
			auto c = vaddq_f32(vdupq_n_f32(vertex[k]), vmulq_n_f32(lane, vertexStep[k]));
			quantized[k] = Neon_quantize(vmulq_f32(c, t));
			src = vorrq_u32(src, vshlq_u32(quantized[k], vdupq_n_s32(dstIndex[k] * 8)));
		}

		auto d = vld1q_u8(dst);
		auto srcBytes = vreinterpretq_u8_u32(src);

		if (blend == BlendMode::Solid) {
			vst1q_u8(dst, srcBytes);
		} else {
			vst1q_u8(dst,
					vbslq_u8(alphaMask, d, Neon_blendQuadVarying(d, srcBytes, quantized[3])));
		}

		dst += 16;
	}

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

// -- bilinear ---------------------------------------------------------------------------------
//
// Vector across channels, not across pixels - the same choice as the x86 kernel and for the same
// reason: gating on `inRange` makes t00 and t10 adjacent texels, so one eight-byte load delivers
// the pair the horizontal filter needs and no lane ever has to move.
//
// vqtbl1q_u8 is the table lookup that stands in for pshufb; it is AArch64-only, which is fine
// because that is the only place this file compiles.
static void Neon_textureSpanLinear(SpanContext &ctx, const ChannelLayout &fmt, BlendMode blend,
		const TextureSpan &tex) {
	auto dst = ctx.dst;

	const uint8_t dstIndex[4] = {fmt.r, fmt.g, fmt.b, fmt.a};
	const float vertex[4] = {ctx.r, ctx.g, ctx.b, ctx.a};
	const float vertexStep[4] = {ctx.dr, ctx.dg, ctx.db, ctx.da};

	// Storage order to logical R G B A, for both texels of the pair. An index of 0xFF yields zero.
	uint8_t srcOrderBytes[16];
	for (int k = 0; k < 16; ++k) { srcOrderBytes[k] = 0xFF; }
	for (int k = 0; k < 4; ++k) {
		srcOrderBytes[k] = tex.src[k];
		srcOrderBytes[k + 4] = uint8_t(tex.src[k] + 4);
	}
	const auto srcOrder = vld1q_u8(srcOrderBytes);

	uint8_t swizzleBytes[16], oneBytes[16], storeBytes[16];
	for (int k = 0; k < 16; ++k) {
		swizzleBytes[k] = 0xFF;
		oneBytes[k] = 0;
		storeBytes[k] = 0xFF;
	}
	for (int k = 0; k < 4; ++k) {
		auto s = tex.swizzle[k];
		if (s >= 0) {
			swizzleBytes[k] = uint8_t(s);
		} else if (s == -2) {
			oneBytes[k] = 0xFF;
		}
		storeBytes[dstIndex[k]] = uint8_t(k);
	}
	const auto swizzleShuffle = vld1q_u8(swizzleBytes);
	const auto oneMask = vld1q_u8(oneBytes);
	const auto storeShuffle = vld1q_u8(storeBytes);

	const auto vertexVec = vld1q_f32(vertex);
	const auto vertexStepVec = vld1q_f32(vertexStep);

	constexpr int32_t half = 1 << (2 * SampleWeightBits - 1);

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

		uint8_t pair0[8], pair1[8];
		__builtin_memcpy(pair0, row0, 8);
		__builtin_memcpy(pair1, row1, 8);

		auto q0 = vqtbl1q_u8(vcombine_u8(vld1_u8(pair0), vdup_n_u8(0)), srcOrder);
		auto q1 = vqtbl1q_u8(vcombine_u8(vld1_u8(pair1), vdup_n_u8(0)), srcOrder);

		auto widen = [](uint8x16_t v, int lane) {
			auto half16 = vmovl_u8(vget_low_u8(v));
			return vreinterpretq_s32_u32(
					lane == 0 ? vmovl_u16(vget_low_u16(half16)) : vmovl_u16(vget_high_u16(half16)));
		};

		auto a = widen(q0, 0), b = widen(q0, 1);
		auto c = widen(q1, 0), d = widen(q1, 1);

		auto ix = vdupq_n_s32(SampleWeightOne - wx);
		auto wxv = vdupq_n_s32(wx);

		auto top = vaddq_s32(vmulq_s32(a, ix), vmulq_s32(b, wxv));
		auto bottom = vaddq_s32(vmulq_s32(c, ix), vmulq_s32(d, wxv));

		auto filtered = vaddq_s32(vmulq_n_s32(top, SampleWeightOne - wy), vmulq_n_s32(bottom, wy));
		filtered = vshrq_n_s32(vaddq_s32(filtered, vdupq_n_s32(half)), 2 * SampleWeightBits);

		auto texelBytes = vreinterpretq_u8_u16(
				vcombine_u16(vmovn_u32(vreinterpretq_u32_s32(filtered)), vdup_n_u16(0)));
		texelBytes = vqtbl1q_u8(vcombine_u8(vmovn_u16(vreinterpretq_u16_u8(texelBytes)),
										  vdup_n_u8(0)),
				vcombine_u8(vcreate_u8(0x0706050403020100ull), vdup_n_u8(0xFF)));

		auto swizzled = vorrq_u8(vqtbl1q_u8(texelBytes, swizzleShuffle), oneMask);

		auto texf = vmulq_n_f32(vcvtq_f32_u32(vmovl_u16(
										 vget_low_u16(vmovl_u8(vget_low_u8(swizzled))))),
				1.0f / 255.0f);
		auto colour = vaddq_f32(vertexVec, vmulq_n_f32(vertexStepVec, float(i)));

		auto scaled = vaddq_f32(vmulq_n_f32(vmulq_f32(colour, texf), 255.0f), vdupq_n_f32(0.5f));
		auto q = vminq_u32(vcvtq_u32_f32(vmaxq_f32(scaled, vdupq_n_f32(0.0f))), vdupq_n_u32(255));

		auto shadedBytes = vqtbl1q_u8(vcombine_u8(vmovn_u16(vcombine_u16(vmovn_u32(q),
														   vdup_n_u16(0))),
											  vdup_n_u8(0)),
				storeShuffle);

		uint8_t shaded[16];
		vst1q_u8(shaded, shadedBytes);

		if (blend == BlendMode::Solid) {
			__builtin_memcpy(dst, shaded, 4);
		} else {
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

SP_RASTER_KERNEL_FN
static void Neon_writeSpan(SpanContext &ctx, const ChannelLayout &fmt, BlendMode blend) {
	if (ctx.count == 0 || fmt.size == 0) {
		return;
	}

	if (fmt.size == 4 && !isConstantSpan(ctx)) {
		TextureSpan tex;
		if (resolveTextureSpan(ctx, fmt, tex) && tex.linear && tex.inRange) {
			Neon_textureSpanLinear(ctx, fmt, blend, tex);
			return;
		}
	}

	if (fmt.size != 4 || !isConstantSpan(ctx)) {
		// Bilinear falls through to the scalar set and its column cache; this samples one texel.
		TextureSpan tex;
		if (resolveTextureSpan(ctx, fmt, tex) && !tex.linear) {
			Neon_textureSpan(ctx, fmt, blend, tex);
			return;
		}

		writeSpanScalar(ctx, fmt, blend);
		return;
	}

	auto src = getSpanConstant(ctx, fmt);

	if (blend == BlendMode::Solid) {
		Neon_fillConstant(ctx.dst, ctx.count, src);
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

	Neon_blendConstant(ctx.dst, ctx.count, src, fmt);
}

SP_RASTER_KERNEL_FN
static void Neon_fillRect(const Target &target, const URect &rect, const ChannelLayout &fmt,
		const Color4F &color) {
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
		Neon_fillConstant(target.pixels + size_t(y) * size_t(target.stride) + size_t(rect.x) * 4,
				rect.width, src);
	}
}

const KernelTable *getNeonKernels() {
	static const KernelTable s_table{
		KernelSet::Neon,
		&Neon_writeSpan,
		&blitGlyphScalar,
		&Neon_fillRect,
	};
	return &s_table;
}

#else

const KernelTable *getNeonKernels() { return nullptr; }

#endif

} // namespace stappler::raster
