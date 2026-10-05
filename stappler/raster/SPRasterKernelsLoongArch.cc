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

// LoongArch: the NEON algorithm on LSX (4 pixels per step) and LASX (8 pixels per step).
//
// LSX is the base of the la64v1.0 target, so its set always compiles and is used wherever the
// kernel reports it; LASX is chosen at run time like AVX2. Both sets are one body, included twice
// from SPRasterKernelsLoongArchSet.cc: a helper inlined into a target("lasx") function must carry
// that target itself, and a template cannot take an attribute conditionally.

#if SP_RASTER_LOONGARCH
#include <lsxintrin.h>
#endif

namespace STAPPLER_VERSIONIZED stappler::raster {

#if SP_RASTER_LOONGARCH

bool cpuHasLsx();
bool cpuHasLasx();

using LaU8x4 = uint8_t __attribute__((vector_size(4)));
using LaU8x16 = uint8_t __attribute__((vector_size(16)));
using LaU64x2 = uint64_t __attribute__((vector_size(16)));
using LaI32x4 = int32_t __attribute__((vector_size(16)));
using LaU32x4 = uint32_t __attribute__((vector_size(16)));
using LaF32x4 = float __attribute__((vector_size(16)));

// Byte table lookup, tbl-style: an index of 16 or more yields zero. vshuf.b takes indices below 16
// from its second operand and the rest from its first, which is kept zero.
static constexpr uint8_t La_zeroIndex = 16;

SP_RASTER_KERNEL_INLINE LaU8x16 La_lookup(LaU8x16 table, LaU8x16 index) {
	return (LaU8x16)__lsx_vshuf_b((__m128i)LaU8x16{}, (__m128i)table, (__m128i)index);
}

SP_RASTER_KERNEL_INLINE LaU8x16 La_lowBytes(LaU8x4 v) {
	return __builtin_shufflevector(v, LaU8x4{}, 0, 1, 2, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4);
}

SP_RASTER_KERNEL_INLINE LaI32x4 La_widen(LaU8x16 v, int quad) {
	auto bytes = quad == 0 ? __builtin_shufflevector(v, v, 0, 1, 2, 3)
						   : __builtin_shufflevector(v, v, 4, 5, 6, 7);
	return __builtin_convertvector(bytes, LaI32x4);
}

// -- bilinear ---------------------------------------------------------------------------------
//
// Vector across channels, not across pixels, as on NEON and x86: gated on `inRange`, t00 and t10
// are adjacent texels and one eight-byte load delivers the pair. One pixel at a time has nothing
// for 256 bits to add, so the LASX set uses this one as well.
static void La_textureSpanLinear(SpanContext &ctx, const ChannelLayout &fmt, BlendMode blend,
		const TextureSpan &tex) {
	auto dst = ctx.dst;

	const uint8_t dstIndex[4] = {fmt.r, fmt.g, fmt.b, fmt.a};

	// Storage order to logical R G B A, for both texels of the pair
	LaU8x16 srcOrder, swizzleShuffle, oneMask = LaU8x16{}, storeShuffle;
	for (int k = 0; k < 16; ++k) {
		srcOrder[k] = La_zeroIndex;
		swizzleShuffle[k] = La_zeroIndex;
		storeShuffle[k] = La_zeroIndex;
	}
	for (int k = 0; k < 4; ++k) {
		srcOrder[k] = tex.src[k];
		srcOrder[k + 4] = uint8_t(tex.src[k] + 4);

		auto s = tex.swizzle[k];
		if (s >= 0) {
			swizzleShuffle[k] = uint8_t(s);
		} else if (s == -2) {
			oneMask[k] = 0xFF;
		}
		storeShuffle[dstIndex[k]] = uint8_t(k);
	}

	const LaF32x4 vertexVec = {ctx.r, ctx.g, ctx.b, ctx.a};
	const LaF32x4 vertexStepVec = {ctx.dr, ctx.dg, ctx.db, ctx.da};

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

		uint64_t pair0, pair1;
		__builtin_memcpy(&pair0, row0, 8);
		__builtin_memcpy(&pair1, row1, 8);

		auto q0 = La_lookup((LaU8x16)LaU64x2{pair0, 0}, srcOrder);
		auto q1 = La_lookup((LaU8x16)LaU64x2{pair1, 0}, srcOrder);

		auto a = La_widen(q0, 0), b = La_widen(q0, 1);
		auto c = La_widen(q1, 0), d = La_widen(q1, 1);

		const int32_t ix = SampleWeightOne - wx;

		auto top = a * ix + b * wx;
		auto bottom = c * ix + d * wx;

		auto filtered = top * (SampleWeightOne - wy) + bottom * wy;
		filtered = (filtered + half) >> (2 * SampleWeightBits);

		auto texelBytes = La_lowBytes(__builtin_convertvector(filtered, LaU8x4));
		auto swizzled = La_lookup(texelBytes, swizzleShuffle) | oneMask;

		auto texf = __builtin_convertvector(
							__builtin_convertvector(__builtin_shufflevector(swizzled, swizzled, 0, 1, 2, 3),
									LaU32x4),
							LaF32x4)
				* (1.0f / 255.0f);
		auto colour = vertexVec + vertexStepVec * float(i);

		auto scaled = (colour * texf) * 255.0f + 0.5f;
		scaled = __builtin_elementwise_min(__builtin_elementwise_max(scaled, LaF32x4{}),
				LaF32x4{} + 255.0f);
		auto q = __builtin_convertvector(scaled, LaU32x4);

		auto shadedBytes = La_lookup(La_lowBytes(__builtin_convertvector(q, LaU8x4)), storeShuffle);

		uint8_t shaded[16];
		__builtin_memcpy(shaded, &shadedBytes, sizeof(shaded));

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

// LSX, four pixels per step
#define SP_RASTER_LA_NAMESPACE La_lsx
#define SP_RASTER_LA_PIXELS 4
#define SP_RASTER_LA_TARGET
#define SP_RASTER_LA_ILVL __builtin_lsx_vilvl_b
#define SP_RASTER_LA_ILVH __builtin_lsx_vilvh_b
#define SP_RASTER_LA_PICKEV __builtin_lsx_vpickev_b
#include "SPRasterKernelsLoongArchSet.cc"
#undef SP_RASTER_LA_NAMESPACE
#undef SP_RASTER_LA_PIXELS
#undef SP_RASTER_LA_TARGET
#undef SP_RASTER_LA_ILVL
#undef SP_RASTER_LA_ILVH
#undef SP_RASTER_LA_PICKEV

// LASX, eight pixels per step
#define SP_RASTER_LA_NAMESPACE La_lasx
#define SP_RASTER_LA_PIXELS 8
#define SP_RASTER_LA_TARGET SP_RASTER_TARGET("lasx")
#define SP_RASTER_LA_ILVL __builtin_lasx_xvilvl_b
#define SP_RASTER_LA_ILVH __builtin_lasx_xvilvh_b
#define SP_RASTER_LA_PICKEV __builtin_lasx_xvpickev_b
#include "SPRasterKernelsLoongArchSet.cc"
#undef SP_RASTER_LA_NAMESPACE
#undef SP_RASTER_LA_PIXELS
#undef SP_RASTER_LA_TARGET
#undef SP_RASTER_LA_ILVL
#undef SP_RASTER_LA_ILVH
#undef SP_RASTER_LA_PICKEV

const KernelTable *getLsxKernels() {
	if (!cpuHasLsx()) {
		return nullptr;
	}
	static const KernelTable s_table{
		KernelSet::Lsx,
		&La_lsx::writeSpan,
		&blitGlyphScalar,
		&La_lsx::fillRect,
	};
	return &s_table;
}

const KernelTable *getLasxKernels() {
	if (!cpuHasLasx()) {
		return nullptr;
	}
	static const KernelTable s_table{
		KernelSet::Lasx,
		&La_lasx::writeSpan,
		&blitGlyphScalar,
		&La_lasx::fillRect,
	};
	return &s_table;
}

#else

const KernelTable *getLsxKernels() { return nullptr; }
const KernelTable *getLasxKernels() { return nullptr; }

#endif

} // namespace stappler::raster
