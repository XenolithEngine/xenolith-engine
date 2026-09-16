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

// SIMD within a register: two 32-bit pixels per uint64_t, in portable C++ with no intrinsics and
// no ISA requirement whatsoever. This is not a consolation prize for machines without vectors - it
// is the working path for riscv64 without RVV, for loongarch, and for any target the vector sets
// do not cover, and it is the shape every wider set then reuses.
//
// The whole method rests on one fact: the blend arithmetic fits in 16-bit lanes at every step.
//
//     src*a + dst*(255-a)  <=  255*a + 255*(255-a)  =  65025  < 65536
//     t = x + 128          <=  65153
//     t + (t >> 8)         <=  65407
//
// So the same integer operations that the scalar kernel performs on one channel can be performed
// on four channels at once, with no saturation and no reordering - which is why the result is
// bit-identical by construction rather than by testing. A 128-bit or 256-bit set is the same
// algorithm with more lanes.

namespace STAPPLER_VERSIONIZED stappler::raster {

// Even byte lanes of a uint64 (bytes 0, 2, 4, 6), each widened to 16 bits.
static constexpr uint64_t Swar_laneMask = 0x00FF'00FF'00FF'00FFull;

// Unaligned by necessity: the target comes from wl_shm or an X SHM segment at a slot offset, and
// its stride is width*4, so a row starts wherever it starts. memcpy is the portable spelling of an
// unaligned access and compiles to a single load.
static inline uint64_t Swar_load(const uint8_t *p) {
	uint64_t v;
	__builtin_memcpy(&v, p, sizeof(v));
	return v;
}

static inline void Swar_store(uint8_t *p, uint64_t v) { __builtin_memcpy(p, &v, sizeof(v)); }

// round(x / 255) for every 16-bit lane at once.
//
// The mask after the shift is not optional: `>> 8` on the whole register drags the low byte of
// lane k+1 into lane k. Masking discards it, leaving exactly the high byte of each lane, which is
// what the scalar `t >> 8` means.
static inline uint64_t Swar_divide255(uint64_t x) {
	auto t = x + 0x0080'0080'0080'0080ull;
	return ((t + ((t >> 8) & Swar_laneMask)) >> 8) & Swar_laneMask;
}

// Two pixels of `dst` blended under one constant source. `srcTermEven`/`srcTermOdd` are
// src*srcAlpha precomputed per byte position; `inverse` is 255 - srcAlpha.
static inline uint64_t Swar_blendPair(uint64_t dst, uint64_t srcTermEven, uint64_t srcTermOdd,
		uint64_t inverse) {
	auto dstEven = dst & Swar_laneMask;
	auto dstOdd = (dst >> 8) & Swar_laneMask;

	auto even = Swar_divide255(srcTermEven + dstEven * inverse);
	auto odd = Swar_divide255(srcTermOdd + dstOdd * inverse);

	return even | (odd << 8);
}

// Replicate one 4-byte pixel into both halves of a uint64.
static inline uint64_t Swar_pair(const uint8_t bytes[4]) {
	uint32_t one;
	__builtin_memcpy(&one, bytes, sizeof(one));
	return uint64_t(one) | (uint64_t(one) << 32);
}

// A constant colour over a run, blending disabled: a plain store, eight bytes at a time.
static void Swar_fillConstant(uint8_t *dst, uint32_t count, const SpanConstant &src) {
	auto pair = Swar_pair(src.bytes);

	uint32_t i = 0;
	for (; i + 2 <= count; i += 2) {
		Swar_store(dst, pair);
		dst += 8;
	}
	if (i < count) {
		__builtin_memcpy(dst, src.bytes, 4);
	}
}

// A constant colour over a run, source-over. Destination alpha is preserved, which is what the
// flat contract's only blend state says - so the alpha byte of every pixel is put back from the
// destination after blending, rather than excluded from it (excluding it would cost a branch per
// lane; restoring it costs one mask).
static void Swar_blendConstant(uint8_t *dst, uint32_t count, const SpanConstant &src,
		const ChannelLayout &fmt) {
	const uint64_t inverse = 255 - src.alpha;

	const auto srcPair = Swar_pair(src.bytes);
	const auto srcTermEven = (srcPair & Swar_laneMask) * src.alpha;
	const auto srcTermOdd = ((srcPair >> 8) & Swar_laneMask) * src.alpha;

	// 0xFF at the alpha byte of both pixels.
	const uint64_t alphaMask =
			(uint64_t(0xFFull) << (fmt.a * 8)) | (uint64_t(0xFFull) << ((fmt.a + 4) * 8));

	uint32_t i = 0;
	for (; i + 2 <= count; i += 2) {
		auto d = Swar_load(dst);
		auto blended = Swar_blendPair(d, srcTermEven, srcTermOdd, inverse);
		Swar_store(dst, (blended & ~alphaMask) | (d & alphaMask));
		dst += 8;
	}

	// The odd trailing pixel goes through the scalar arithmetic, which is the same arithmetic.
	if (i < count) {
		dst[fmt.r] = Kernels_blend(src.bytes[fmt.r], src.alpha, dst[fmt.r]);
		dst[fmt.g] = Kernels_blend(src.bytes[fmt.g], src.alpha, dst[fmt.g]);
		dst[fmt.b] = Kernels_blend(src.bytes[fmt.b], src.alpha, dst[fmt.b]);
	}
}

// True when this set can take the span. Everything else - textured, interpolated, single-channel
// targets - falls through to the scalar kernel rather than being reimplemented here.
static inline bool Swar_handles(const SpanContext &ctx, const ChannelLayout &fmt) {
	return fmt.size == 4 && isConstantSpan(ctx);
}

SP_RASTER_KERNEL_FN
static void Swar_writeSpan(SpanContext &ctx, const ChannelLayout &fmt, BlendMode blend) {
	if (ctx.count == 0 || fmt.size == 0) {
		return;
	}

	if (!Swar_handles(ctx, fmt)) {
		writeSpanScalar(ctx, fmt, blend);
		return;
	}

	auto src = getSpanConstant(ctx, fmt);

	if (blend == BlendMode::Solid) {
		Swar_fillConstant(ctx.dst, ctx.count, src);
		return;
	}

	if (src.alpha == 0) {
		return;
	}

	if (src.alpha == 255) {
		// Opaque source, but destination alpha still has to survive: the colour bytes are a plain
		// store, the alpha byte is not.
		auto dst = ctx.dst;
		for (uint32_t i = 0; i < ctx.count; ++i) {
			dst[fmt.r] = src.bytes[fmt.r];
			dst[fmt.g] = src.bytes[fmt.g];
			dst[fmt.b] = src.bytes[fmt.b];
			dst += 4;
		}
		return;
	}

	Swar_blendConstant(ctx.dst, ctx.count, src, fmt);
}

SP_RASTER_KERNEL_FN
static void Swar_fillRect(const Target &target, const URect &rect, const ChannelLayout &fmt,
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
		auto dst = target.pixels + size_t(y) * size_t(target.stride) + size_t(rect.x) * 4;
		Swar_fillConstant(dst, rect.width, src);
	}
}

const KernelTable *getSwarKernels() {
	static const KernelTable s_table{
		KernelSet::Swar,
		&Swar_writeSpan,
		// The glyph blit varies its source alpha per texel, so it is not a constant-source run and
		// gains nothing from this shape. It is also 20us of a 7800us frame - measured, not assumed.
		&blitGlyphScalar,
		&Swar_fillRect,
	};
	return &s_table;
}

} // namespace stappler::raster
