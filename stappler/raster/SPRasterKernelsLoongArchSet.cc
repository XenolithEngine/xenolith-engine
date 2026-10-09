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

// One LoongArch kernel set. No include guard: SPRasterKernelsLoongArch.cc includes it once per
// set, with SP_RASTER_LA_NAMESPACE, SP_RASTER_LA_PIXELS, SP_RASTER_LA_TARGET and the
// interleave builtins of that width defined.

namespace SP_RASTER_LA_NAMESPACE {

#define SP_RASTER_LA_INLINE SP_RASTER_LA_TARGET SP_RASTER_KERNEL_INLINE

static constexpr uint32_t Pixels = SP_RASTER_LA_PIXELS;
static constexpr uint32_t Width = Pixels * 4;

using Bytes = uint8_t __attribute__((vector_size(Width)));
using Signed = int8_t __attribute__((vector_size(Width)));
using Half = uint16_t __attribute__((vector_size(Width))); // half of the bytes, widened
using U32 = uint32_t __attribute__((vector_size(Width)));
using I32 = int32_t __attribute__((vector_size(Width)));
using F32 = float __attribute__((vector_size(Width)));

SP_RASTER_LA_INLINE Bytes load(const uint8_t *src) {
	Bytes v;
	__builtin_memcpy(&v, src, sizeof(v));
	return v;
}

SP_RASTER_LA_INLINE void store(uint8_t *dst, Bytes v) { __builtin_memcpy(dst, &v, sizeof(v)); }

SP_RASTER_LA_INLINE Bytes splat(const uint8_t bytes[4]) {
	uint32_t pixel;
	__builtin_memcpy(&pixel, bytes, sizeof(pixel));
	return (Bytes)(U32{} + pixel);
}

// Bytes to 16-bit lanes and back, per 128-bit lane: the interleave orders the halves the way the
// pick-even undoes, and every operation between them is lane-wise.
SP_RASTER_LA_INLINE Half widenLo(Bytes v) {
	return (Half)SP_RASTER_LA_ILVL((Signed)Bytes{}, (Signed)v);
}

SP_RASTER_LA_INLINE Half widenHi(Bytes v) {
	return (Half)SP_RASTER_LA_ILVH((Signed)Bytes{}, (Signed)v);
}

SP_RASTER_LA_INLINE Bytes narrow(Half lo, Half hi) {
	return (Bytes)SP_RASTER_LA_PICKEV((Signed)hi, (Signed)lo);
}

SP_RASTER_LA_INLINE Bytes select(Bytes mask, Bytes ifSet, Bytes ifClear) {
	return (mask & ifSet) | (~mask & ifClear);
}

// round(x / 255) per 16-bit lane.
SP_RASTER_LA_INLINE Half divide255(Half x) {
	auto t = x + uint16_t(128);
	return (t + (t >> 8)) >> 8;
}

SP_RASTER_LA_INLINE void fillConstant(uint8_t *dst, uint32_t count, const SpanConstant &src) {
	auto pattern = splat(src.bytes);

	uint32_t i = 0;
	for (; i + Pixels <= count; i += Pixels) {
		store(dst, pattern);
		dst += Width;
	}
	for (; i < count; ++i) {
		__builtin_memcpy(dst, src.bytes, 4);
		dst += 4;
	}
}

SP_RASTER_LA_INLINE void blendConstant(uint8_t *dst, uint32_t count, SpanConstant src,
		const ChannelLayout &fmt, BlendMode blend) {
	// Premultiplied blends the alpha byte too, with a source byte of 255; Transparent keeps it.
	const bool keepAlpha = blend != BlendMode::Premultiplied;
	if (!keepAlpha) {
		src.bytes[fmt.a] = 255;
	}

	const auto inverse = Half{} + uint16_t(255 - src.alpha);

	const auto srcPixels = splat(src.bytes);
	const auto srcLo = widenLo(srcPixels) * uint16_t(src.alpha);
	const auto srcHi = widenHi(srcPixels) * uint16_t(src.alpha);

	uint8_t maskBytes[4] = {0, 0, 0, 0};
	maskBytes[fmt.a] = keepAlpha ? 0xFF : 0;
	const auto alphaMask = splat(maskBytes);

	uint32_t i = 0;
	for (; i + Pixels <= count; i += Pixels) {
		auto d = load(dst);
		auto lo = divide255(srcLo + widenLo(d) * inverse);
		auto hi = divide255(srcHi + widenHi(d) * inverse);
		store(dst, select(alphaMask, d, narrow(lo, hi)));
		dst += Width;
	}

	for (; i < count; ++i) {
		Kernels_blendPixel(dst, fmt, blend, src.bytes[fmt.r], src.bytes[fmt.g], src.bytes[fmt.b],
				src.alpha);
		dst += 4;
	}
}

// -- textured -------------------------------------------------------------------------------------
//
// The quantization runs in float rather than double, which can cost one step in a channel, as on
// NEON and x86. Everything that does not sample a texture stays bit-identical to scalar.

SP_RASTER_LA_INLINE F32 channel(U32 texels, uint8_t byteIndex) {
	auto masked = (texels >> uint32_t(byteIndex * 8)) & 0xFFu;
	return __builtin_convertvector(masked, F32) * (1.0f / 255.0f);
}

SP_RASTER_LA_INLINE U32 quantize(F32 c) {
	auto scaled = c * 255.0f + 0.5f;
	// Clamped before the conversion: truncation of [0, 255] equals truncation clamped to 255
	scaled = __builtin_elementwise_min(__builtin_elementwise_max(scaled, F32{}), F32{} + 255.0f);
	return __builtin_convertvector(scaled, U32);
}

SP_RASTER_LA_INLINE I32 floorToInt(F32 f) {
	auto t = __builtin_convertvector(f, I32);
	// The compare is all-ones, -1, where the truncation went up
	return t + (I32)(f < __builtin_convertvector(t, F32));
}

SP_RASTER_LA_INLINE Bytes blendVarying(Bytes dst, Bytes src, U32 alpha) {
	// Each pixel's alpha into all four of its bytes, so the widening lines it up with the colours
	auto a8 = (Bytes)(alpha | (alpha << 8) | (alpha << 16) | (alpha << 24));

	const auto full = Half{} + uint16_t(255);
	auto aLo = widenLo(a8), aHi = widenHi(a8);

	auto lo = divide255(widenLo(src) * aLo + widenLo(dst) * (full - aLo));
	auto hi = divide255(widenHi(src) * aHi + widenHi(dst) * (full - aHi));
	return narrow(lo, hi);
}

SP_RASTER_LA_INLINE void textureSpan(SpanContext &ctx, const ChannelLayout &fmt, BlendMode blend,
		const TextureSpan &tex) {
	auto dst = ctx.dst;

	uint8_t maskBytes[4] = {0, 0, 0, 0};
	maskBytes[fmt.a] = 0xFF;
	const auto alphaMask = splat(maskBytes);

	const uint8_t dstIndex[4] = {fmt.r, fmt.g, fmt.b, fmt.a};
	const float vertex[4] = {ctx.r, ctx.g, ctx.b, ctx.a};
	const float vertexStep[4] = {ctx.dr, ctx.dg, ctx.db, ctx.da};

	U32 iota;
	for (uint32_t k = 0; k < Pixels; ++k) { iota[k] = k; }

	uint32_t i = 0;
	for (; i + Pixels <= ctx.count; i += Pixels) {
		// Lanes carry the anchor column, not the offset into the run - see SpanContext.
		auto lane = __builtin_convertvector(iota + (ctx.originOffset + i), F32);

		auto u = (F32{} + ctx.u) + lane * ctx.du;
		auto v = (F32{} + ctx.v) + lane * ctx.dv;

		auto x = floorToInt(u * float(tex.width));
		auto y = floorToInt(v * float(tex.height));

		if (!tex.inRange) {
			if (tex.powerOfTwo) {
				x &= tex.width - 1;
				y &= tex.height - 1;
			} else {
				x = __builtin_elementwise_min(__builtin_elementwise_max(x, I32{}),
						I32{} + (tex.width - 1));
				y = __builtin_elementwise_min(__builtin_elementwise_max(y, I32{}),
						I32{} + (tex.height - 1));
			}
		}

		auto offset = y * int32_t(tex.stride) + (x << 2);

		U32 texels;
		for (uint32_t k = 0; k < Pixels; ++k) {
			uint32_t raw;
			__builtin_memcpy(&raw, tex.pixels + offset[k], 4);
			texels[k] = raw;
		}

		F32 logical[4];
		for (int k = 0; k < 4; ++k) { logical[k] = channel(texels, tex.src[k]); }

		U32 src = U32{};
		U32 quantized[4];
		for (int k = 0; k < 4; ++k) {
			auto s = tex.swizzle[k];
			auto t = (s >= 0) ? logical[s] : ((s == -1) ? F32{} : F32{} + 1.0f);
			auto c = (F32{} + vertex[k]) + lane * vertexStep[k];
			quantized[k] = quantize(c * t);
			src |= quantized[k] << uint32_t(dstIndex[k] * 8);
		}

		auto d = load(dst);
		auto srcBytes = (Bytes)src;

		if (blend == BlendMode::Solid) {
			store(dst, srcBytes);
		} else {
			store(dst, select(alphaMask, d, blendVarying(d, srcBytes, quantized[3])));
		}

		dst += Width;
	}

	if (i < ctx.count) {
		// Only where the run starts changes; the attributes stay anchored where they were, so
		// the tail cannot round differently from the vector part.
		SpanContext tail = ctx;
		tail.dst = dst;
		tail.count = ctx.count - i;
		tail.originOffset = ctx.originOffset + i;
		writeSpanScalar(tail, fmt, blend);
	}
}

SP_RASTER_LA_TARGET
SP_RASTER_KERNEL_FN
static void writeSpan(SpanContext &ctx, const ChannelLayout &fmt, BlendMode blend) {
	if (ctx.count == 0 || fmt.size == 0) {
		return;
	}

	// A textured span with premultiplied output goes to the scalar set.
	const bool vectorSampler = blend != BlendMode::Premultiplied;

	if (vectorSampler && fmt.size == 4 && !isConstantSpan(ctx)) {
		TextureSpan tex;
		if (resolveTextureSpan(ctx, fmt, tex) && tex.linear && tex.inRange) {
			La_textureSpanLinear(ctx, fmt, blend, tex);
			return;
		}
	}

	if (fmt.size != 4 || !isConstantSpan(ctx)) {
		// Bilinear out of range falls through to the scalar set and its column cache.
		TextureSpan tex;
		if (vectorSampler && fmt.size == 4 && resolveTextureSpan(ctx, fmt, tex) && !tex.linear) {
			textureSpan(ctx, fmt, blend, tex);
			return;
		}

		writeSpanScalar(ctx, fmt, blend);
		return;
	}

	auto src = getSpanConstant(ctx, fmt);

	if (blend == BlendMode::Solid) {
		fillConstant(ctx.dst, ctx.count, src);
		return;
	}

	if (src.alpha == 0) {
		return;
	}

	if (src.alpha == 255) {
		// Premultiplied output of an opaque source is the source itself.
		if (blend == BlendMode::Premultiplied) {
			fillConstant(ctx.dst, ctx.count, src);
			return;
		}

		auto dst = ctx.dst;
		for (uint32_t i = 0; i < ctx.count; ++i) {
			dst[fmt.r] = src.bytes[fmt.r];
			dst[fmt.g] = src.bytes[fmt.g];
			dst[fmt.b] = src.bytes[fmt.b];
			dst += 4;
		}
		return;
	}

	blendConstant(ctx.dst, ctx.count, src, fmt, blend);
}

SP_RASTER_LA_TARGET
SP_RASTER_KERNEL_FN
static void fillRect(const Target &target, const URect &rect, const ChannelLayout &fmt,
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
		fillConstant(target.pixels + size_t(y) * size_t(target.stride) + size_t(rect.x) * 4,
				rect.width, src);
	}
}

#undef SP_RASTER_LA_INLINE

} // namespace SP_RASTER_LA_NAMESPACE
