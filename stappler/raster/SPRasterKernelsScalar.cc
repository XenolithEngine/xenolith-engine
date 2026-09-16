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

// The scalar kernel set: one pixel at a time, plain C++, no intrinsics. It exists on every
// architecture and is the reference every other set is required to reproduce byte for byte.

namespace STAPPLER_VERSIONIZED stappler::raster {

// The fragment shader, per pixel: outColor = fragColor * textureColor (xl_2d_flat.frag).
// Templated on the texture kind so the Solid case compiles down to the plain multiply M0 had -
// which is what keeps a plain Layer byte-identical to the GPU.
template <TextureKind Kind>
static inline Color4F Kernels_shade(const SpanContext &ctx, float r, float g, float b, float a,
		float u, float v, float layer) {
	if constexpr (Kind == TextureKind::Solid) {
		auto &c = ctx.constantColor;
		return Color4F(r * c.r, g * c.g, b * c.b, a * c.a);
	} else {
		auto t = Sample_texture(*ctx.texture, ctx.sampler, Kind, u, v, layer);
		return Color4F(r * t.r, g * t.g, b * t.b, a * t.a);
	}
}

// Attribute at offset i of the span. Positional, not accumulated - and that choice is what makes
// a vector kernel possible at all.
//
// Accumulating (`r += dr` per pixel) rounds once per step, so lane i of a vector, which computes
// the value in one multiply, cannot reproduce the chain: the sequence of roundings is inherently
// sequential and no register width fixes it. Evaluating base + i*step instead is the same two
// operations in the same order on both sides, so scalar and vector agree bit for bit.
//
// It is also the more correct of the two: the drift an accumulation collects over a long span
// disappears, and a GPU evaluates attributes positionally rather than iteratively.
static inline float Kernels_at(float base, float step, uint32_t i) {
	return base + step * float(i);
}

// Blending disabled: plain write, destination alpha included.
template <TextureKind Kind>
static void Kernels_writeSpanSolid(SpanContext &ctx, const ChannelLayout &fmt) {
	auto dst = ctx.dst;

	// The counter runs in anchor columns, not in offsets into the run: `dst` walks the run, `i`
	// says which column each pixel is in. Interpolating from the run's own start would make the
	// result depend on where the run was cut, and a tile cuts every run it crosses.
	for (uint32_t i = ctx.originOffset, end = ctx.originOffset + ctx.count; i < end; ++i) {
		auto c = Kernels_shade<Kind>(ctx, Kernels_at(ctx.r, ctx.dr, i), Kernels_at(ctx.g, ctx.dg, i),
				Kernels_at(ctx.b, ctx.db, i), Kernels_at(ctx.a, ctx.da, i),
				Kernels_at(ctx.u, ctx.du, i), Kernels_at(ctx.v, ctx.dv, i),
				Kernels_at(ctx.layer, ctx.dlayer, i));

		dst[fmt.r] = Kernels_toUnorm8(c.r);
		if (fmt.size > 1) {
			dst[fmt.g] = Kernels_toUnorm8(c.g);
			dst[fmt.b] = Kernels_toUnorm8(c.b);
			dst[fmt.a] = Kernels_toUnorm8(c.a);
		}

		dst += fmt.size;
	}
}

// color = SrcAlpha/OneMinusSrcAlpha (Add); alpha = Zero/One (Add), i.e. destination alpha is
// left exactly as it was. This is the flat contract's only blend state.
template <TextureKind Kind>
static void Kernels_writeSpanTransparent(SpanContext &ctx, const ChannelLayout &fmt) {
	auto dst = ctx.dst;

	for (uint32_t i = ctx.originOffset, end = ctx.originOffset + ctx.count; i < end; ++i) {
		auto c = Kernels_shade<Kind>(ctx, Kernels_at(ctx.r, ctx.dr, i), Kernels_at(ctx.g, ctx.dg, i),
				Kernels_at(ctx.b, ctx.db, i), Kernels_at(ctx.a, ctx.da, i),
				Kernels_at(ctx.u, ctx.du, i), Kernels_at(ctx.v, ctx.dv, i),
				Kernels_at(ctx.layer, ctx.dlayer, i));

		uint32_t sa = Kernels_toUnorm8(c.a);
		if (sa == 255) {
			dst[fmt.r] = Kernels_toUnorm8(c.r);
			if (fmt.size > 1) {
				dst[fmt.g] = Kernels_toUnorm8(c.g);
				dst[fmt.b] = Kernels_toUnorm8(c.b);
			}
		} else if (sa != 0) {
			dst[fmt.r] = Kernels_blend(Kernels_toUnorm8(c.r), sa, dst[fmt.r]);
			if (fmt.size > 1) {
				dst[fmt.g] = Kernels_blend(Kernels_toUnorm8(c.g), sa, dst[fmt.g]);
				dst[fmt.b] = Kernels_blend(Kernels_toUnorm8(c.b), sa, dst[fmt.b]);
			}
		}

		dst += fmt.size;
	}
}

// blend x texture kind: eight specializations, picked by a plain switch. The switch is inside one
// kernel rather than eight table entries on purpose - the ISA choice is what the table is for, and
// multiplying its width by eight would buy nothing but entries to get wrong.
template <TextureKind Kind>
static inline void Kernels_writeSpanKind(SpanContext &ctx, const ChannelLayout &fmt,
		BlendMode blend) {
	switch (blend) {
	case BlendMode::Solid: Kernels_writeSpanSolid<Kind>(ctx, fmt); break;
	case BlendMode::Transparent: Kernels_writeSpanTransparent<Kind>(ctx, fmt); break;
	}
}

// Bilinear along a span, reusing the texel column between pixels.
//
// The naive form fetches four texels per pixel. But a magnified sprite - a scaled icon, a
// stretched 9-patch, anything drawn larger than its texture - has several consecutive pixels
// inside the same texel cell, and they all read the same four bytes. At 8x magnification that is
// eight identical fetch quads in a row.
//
// So the column is cached. Regrouping the filter makes the cache exact rather than approximate:
//
//   v = (t00*ix + t10*wx)*iy + (t01*ix + t11*wx)*wy
//     = (t00*iy + t01*wy)*ix + (t10*iy + t11*wy)*wx
//
// These are the same four products in a different order, and since nothing is rounded until the
// final shift, the two groupings give the same byte. The second one puts the per-column work
// (A and B) outside the horizontal weight, so it survives across pixels that share a column.
//
// Two keys, because the two halves of the work invalidate at different rates.
//
//   (x0, y0)     - the sixteen bytes of the texel quad. The expensive part.
//   (x0, y0, wy) - a and b, which have the vertical weight folded in. Cheap arithmetic.
//
// An axis-aligned sprite holds both: v is constant along a scanline, so wy never moves and x0
// changes once per texel. A rotated one moves v along the span, so wy walks every pixel and the
// second key misses - but the first still holds, and the fetches are what cost.
//
// The second key really is three-part. It was two at first, and served stale rows for exactly the
// rotated case; `sprite-rotated` caught it, and now so does the unit test.
struct LinearColumn {
	int32_t x0 = INT32_MIN;
	int32_t y0 = INT32_MIN;
	int32_t wy = -1;

	uint8_t t[2][2][4]; // [row][column][channel], logical R G B A order

	int32_t a[4] = {0, 0, 0, 0}; // vertical filter of column x0, unrounded
	int32_t b[4] = {0, 0, 0, 0}; // ... and of column x0 + 1
};

SP_RASTER_KERNEL_INLINE void Scalar_sampleLinear(const TextureSpan &tex, const Sampler &sampler,
		float u, float v, LinearColumn &cache, uint8_t out[4]) {
	// Half-texel offset: the sample point sits at a texel centre.
	auto fx = u * float(tex.width) - 0.5f;
	auto fy = v * float(tex.height) - 0.5f;

	auto x0 = int32_t(sprt::floor(fx));
	auto y0 = int32_t(sprt::floor(fy));

	auto wx = Sample_weight(fx, x0);
	auto wy = Sample_weight(fy, y0);

	if (x0 != cache.x0 || y0 != cache.y0) {
		int32_t ax0 = x0, ax1 = x0 + 1, ay0 = y0, ay1 = y0 + 1;
		if (!tex.inRange) {
			ax0 = Sample_address(ax0, tex.width, sampler.addressU);
			ax1 = Sample_address(ax1, tex.width, sampler.addressU);
			ay0 = Sample_address(ay0, tex.height, sampler.addressV);
			ay1 = Sample_address(ay1, tex.height, sampler.addressV);
		}

		auto row0 = tex.pixels + size_t(ay0) * tex.stride;
		auto row1 = tex.pixels + size_t(ay1) * tex.stride;

		for (int k = 0; k < 4; ++k) {
			auto s = tex.src[k];
			cache.t[0][0][k] = row0[size_t(ax0) * 4 + s];
			cache.t[0][1][k] = row0[size_t(ax1) * 4 + s];
			cache.t[1][0][k] = row1[size_t(ax0) * 4 + s];
			cache.t[1][1][k] = row1[size_t(ax1) * 4 + s];
		}

		cache.x0 = x0;
		cache.y0 = y0;
		cache.wy = -1; // the derived values below belong to the old quad
	}

	if (wy != cache.wy) {
		const int32_t iy = SampleWeightOne - wy;
		for (int k = 0; k < 4; ++k) {
			cache.a[k] = int32_t(cache.t[0][0][k]) * iy + int32_t(cache.t[1][0][k]) * wy;
			cache.b[k] = int32_t(cache.t[0][1][k]) * iy + int32_t(cache.t[1][1][k]) * wy;
		}
		cache.wy = wy;
	}

	const int32_t ix = SampleWeightOne - wx;
	constexpr int32_t half = 1 << (2 * SampleWeightBits - 1);
	for (int k = 0; k < 4; ++k) {
		out[k] = uint8_t((cache.a[k] * ix + cache.b[k] * wx + half) >> (2 * SampleWeightBits));
	}
}

// The vertical weight is part of the cached column, so a span whose v moves would invalidate it
// on every pixel anyway. It is still correct - `y0` is part of the key - just no faster.
SP_RASTER_KERNEL_FN
static void Scalar_textureSpanLinear(SpanContext &ctx, const ChannelLayout &fmt, BlendMode blend,
		const TextureSpan &tex) {
	auto dst = ctx.dst;
	LinearColumn cache;

	const float vertex[4] = {ctx.r, ctx.g, ctx.b, ctx.a};
	const float vertexStep[4] = {ctx.dr, ctx.dg, ctx.db, ctx.da};
	const uint8_t dstIndex[4] = {fmt.r, fmt.g, fmt.b, fmt.a};

	for (uint32_t i = ctx.originOffset, end = ctx.originOffset + ctx.count; i < end; ++i) {
		uint8_t texel[4];
		Scalar_sampleLinear(tex, ctx.sampler, Kernels_at(ctx.u, ctx.du, i),
				Kernels_at(ctx.v, ctx.dv, i), cache, texel);

		uint8_t shaded[4];
		for (int k = 0; k < 4; ++k) {
			auto s = tex.swizzle[k];
			auto t = (s >= 0) ? float(texel[s]) * (1.0f / 255.0f) : ((s == -1) ? 0.0f : 1.0f);
			shaded[k] = Kernels_toUnorm8(Kernels_at(vertex[k], vertexStep[k], i) * t);
		}

		if (blend == BlendMode::Solid) {
			for (int k = 0; k < 4; ++k) { dst[dstIndex[k]] = shaded[k]; }
		} else {
			uint32_t sa = shaded[3];
			if (sa == 255) {
				for (int k = 0; k < 3; ++k) { dst[dstIndex[k]] = shaded[k]; }
			} else if (sa != 0) {
				for (int k = 0; k < 3; ++k) {
					dst[dstIndex[k]] = Kernels_blend(shaded[k], sa, dst[dstIndex[k]]);
				}
			}
		}

		dst += 4;
	}
}

SP_RASTER_KERNEL_FN
void writeSpanScalar(SpanContext &ctx, const ChannelLayout &fmt, BlendMode blend) {
	if (ctx.count == 0 || fmt.size == 0) {
		return;
	}

	// Bilinear gets its own loop, because the win there is not per pixel but across pixels: the
	// column cache. This sits in the scalar set on purpose - every architecture reaches it, and a
	// vector set that cannot take a span still lands here rather than on the naive four fetches.
	if (ctx.kind == TextureKind::Texture2D && ctx.sampler.filter == Filter::Linear) {
		TextureSpan tex;
		if (resolveTextureSpan(ctx, fmt, tex)) {
			Scalar_textureSpanLinear(ctx, fmt, blend, tex);
			return;
		}
	}

	// a textured kind without a texture would dereference null in the sampler
	auto kind = ctx.texture ? ctx.kind : TextureKind::Solid;

	switch (kind) {
	case TextureKind::Solid: Kernels_writeSpanKind<TextureKind::Solid>(ctx, fmt, blend); break;
	case TextureKind::Texture2D:
		Kernels_writeSpanKind<TextureKind::Texture2D>(ctx, fmt, blend);
		break;
	case TextureKind::Texture2DArray:
		Kernels_writeSpanKind<TextureKind::Texture2DArray>(ctx, fmt, blend);
		break;
	case TextureKind::Texture3D:
		Kernels_writeSpanKind<TextureKind::Texture3D>(ctx, fmt, blend);
		break;
	}
}

// One glyph onto the target: source texel i of row j lands on destination pixel (x + i, y + j),
// with no interpolation and no coordinate arithmetic beyond the offset. What the coverage scales is
// the source alpha, exactly as sampling an R8 atlas under ComponentMapping(R, R, R, One) would -
// the texture contributes (1, 1, 1, coverage) and the fragment colour is multiplied by it.
SP_RASTER_KERNEL_FN
void blitGlyphScalar(const Target &target, const GlyphBlit &glyph, const ChannelLayout &fmt) {
	if (!glyph.coverage || glyph.width == 0 || glyph.height == 0 || fmt.size == 0) {
		return;
	}

	// Clip against the scissor in integer pixels; nothing here can produce a partial pixel.
	auto left = sprt::max(glyph.x, int32_t(glyph.scissor.x));
	auto top = sprt::max(glyph.y, int32_t(glyph.scissor.y));
	auto right = sprt::min(glyph.x + int32_t(glyph.width), int32_t(glyph.scissor.x + glyph.scissor.width));
	auto bottom = sprt::min(glyph.y + int32_t(glyph.height),
			int32_t(glyph.scissor.y + glyph.scissor.height));

	if (left >= right || top >= bottom) {
		return;
	}

	const uint32_t red = Kernels_toUnorm8(glyph.color.r);
	const uint32_t green = Kernels_toUnorm8(glyph.color.g);
	const uint32_t blue = Kernels_toUnorm8(glyph.color.b);
	const float alpha = glyph.color.a;

	for (int32_t row = top; row < bottom; ++row) {
		auto src = glyph.coverage + size_t(row - glyph.y) * glyph.pitch + size_t(left - glyph.x);
		auto dst = target.pixels + size_t(row) * target.stride + size_t(left) * fmt.size;

		for (int32_t col = left; col < right; ++col) {
			// The coverage is the texture's alpha channel: what the fragment stage would compute
			// as fragColor.a * textureColor.a.
			const float srcAlpha = alpha * (float(*src) / 255.0f);

			if (glyph.blend == BlendMode::Solid) {
				dst[fmt.r] = uint8_t(red);
				if (fmt.size > 1) {
					dst[fmt.g] = uint8_t(green);
					dst[fmt.b] = uint8_t(blue);
					dst[fmt.a] = Kernels_toUnorm8(srcAlpha);
				}
			} else {
				uint32_t sa = Kernels_toUnorm8(srcAlpha);
				if (sa == 255) {
					dst[fmt.r] = uint8_t(red);
					if (fmt.size > 1) {
						dst[fmt.g] = uint8_t(green);
						dst[fmt.b] = uint8_t(blue);
					}
				} else if (sa != 0) {
					dst[fmt.r] = Kernels_blend(red, sa, dst[fmt.r]);
					if (fmt.size > 1) {
						dst[fmt.g] = Kernels_blend(green, sa, dst[fmt.g]);
						dst[fmt.b] = Kernels_blend(blue, sa, dst[fmt.b]);
					}
				}
			}

			++src;
			dst += fmt.size;
		}
	}
}

// Constant colour over a rectangle the caller has already clipped. Written through the span kernel
// so the two agree by construction rather than by review; a vector set replaces it with a real
// fill, which is where the win is.
SP_RASTER_KERNEL_FN
void fillRectScalar(const Target &target, const URect &rect, const ChannelLayout &fmt,
		const Color4F &color) {
	SpanContext ctx;
	ctx.count = rect.width;
	ctx.r = color.r;
	ctx.g = color.g;
	ctx.b = color.b;
	ctx.a = color.a;

	for (uint32_t y = rect.y; y < rect.y + rect.height; ++y) {
		ctx.dst = target.pixels + size_t(y) * size_t(target.stride) + size_t(rect.x) * fmt.size;
		writeSpanScalar(ctx, fmt, BlendMode::Solid);
	}
}

const KernelTable *getScalarKernels() {
	static const KernelTable s_table{
		KernelSet::Scalar,
		&writeSpanScalar,
		&blitGlyphScalar,
		&fillRectScalar,
	};
	return &s_table;
}

} // namespace stappler::raster
