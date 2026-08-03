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

#include "XLSoftRaster.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft::raster {

// Byte offsets of the colour channels within a pixel of a given format. Anything the backend
// accepts is 8-bit and interleaved, so a permutation is all a kernel needs to know.
struct ChannelLayout {
	uint8_t r = 0;
	uint8_t g = 1;
	uint8_t b = 2;
	uint8_t a = 3;
	uint8_t size = 4;
	bool hasAlpha = true;
};

static ChannelLayout Kernels_getLayout(core::ImageFormat format) {
	switch (format) {
	case core::ImageFormat::R8G8B8A8_UNORM: return ChannelLayout{0, 1, 2, 3, 4, true};
	case core::ImageFormat::B8G8R8A8_UNORM: return ChannelLayout{2, 1, 0, 3, 4, true};
	// Single-channel targets exist for font atlases; red carries the value and there is no alpha
	// to blend against, so the "alpha" slot aliases it.
	case core::ImageFormat::R8_UNORM: return ChannelLayout{0, 0, 0, 0, 1, false};
	default: return ChannelLayout{0, 1, 2, 3, 0, true};
	}
}

static inline uint8_t Kernels_toUnorm8(float v) {
	// The multiply must happen in double. In float, 0.9f * 255.0f rounds up to exactly 229.5f and
	// the +0.5 then carries it to 230, while the GPU (and double here) sees 229.49999 and produces
	// 229 - a visible 1/255 mismatch against the Vulkan reference on every such colour.
	auto i = int32_t(double(v) * 255.0 + 0.5);
	return uint8_t(sprt::clamp(i, 0, 255));
}

// round(x / 255) for x in [0, 255*255], without a division.
static inline uint32_t Kernels_divide255(uint32_t x) {
	auto t = x + 128;
	return (t + (t >> 8)) >> 8;
}

// Colour and texture coordinate of one pixel, plus the per-pixel increments along the span. Kept
// as separate scalars (SoA) rather than a Color4F so a vector kernel can load four lanes of one
// channel at a time.
struct SpanContext {
	uint8_t *dst = nullptr;
	uint32_t count = 0;

	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;
	float a = 0.0f;

	float dr = 0.0f;
	float dg = 0.0f;
	float db = 0.0f;
	float da = 0.0f;

	float u = 0.0f;
	float v = 0.0f;
	float layer = 0.0f;

	float du = 0.0f;
	float dv = 0.0f;
	float dlayer = 0.0f;

	// fragment stage inputs; `texture` is null exactly when kind is Solid
	const Texture *texture = nullptr;
	Sampler sampler;
	TextureKind kind = TextureKind::Solid;
	Color4F constantColor = Color4F::WHITE;
};

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

// Blending disabled: plain write, destination alpha included.
template <TextureKind Kind>
static void Kernels_writeSpanSolid(SpanContext &ctx, const ChannelLayout &fmt) {
	auto dst = ctx.dst;
	auto r = ctx.r, g = ctx.g, b = ctx.b, a = ctx.a;
	auto u = ctx.u, v = ctx.v, layer = ctx.layer;

	for (uint32_t i = 0; i < ctx.count; ++i) {
		auto c = Kernels_shade<Kind>(ctx, r, g, b, a, u, v, layer);

		dst[fmt.r] = Kernels_toUnorm8(c.r);
		if (fmt.size > 1) {
			dst[fmt.g] = Kernels_toUnorm8(c.g);
			dst[fmt.b] = Kernels_toUnorm8(c.b);
			dst[fmt.a] = Kernels_toUnorm8(c.a);
		}

		r += ctx.dr;
		g += ctx.dg;
		b += ctx.db;
		a += ctx.da;
		u += ctx.du;
		v += ctx.dv;
		layer += ctx.dlayer;
		dst += fmt.size;
	}
}

// color = SrcAlpha/OneMinusSrcAlpha (Add); alpha = Zero/One (Add), i.e. destination alpha is
// left exactly as it was. This is the flat contract's only blend state.
template <TextureKind Kind>
static void Kernels_writeSpanTransparent(SpanContext &ctx, const ChannelLayout &fmt) {
	auto dst = ctx.dst;
	auto r = ctx.r, g = ctx.g, b = ctx.b, a = ctx.a;
	auto u = ctx.u, v = ctx.v, layer = ctx.layer;

	for (uint32_t i = 0; i < ctx.count; ++i) {
		auto c = Kernels_shade<Kind>(ctx, r, g, b, a, u, v, layer);

		uint32_t sa = Kernels_toUnorm8(c.a);
		if (sa == 255) {
			dst[fmt.r] = Kernels_toUnorm8(c.r);
			if (fmt.size > 1) {
				dst[fmt.g] = Kernels_toUnorm8(c.g);
				dst[fmt.b] = Kernels_toUnorm8(c.b);
			}
		} else if (sa != 0) {
			uint32_t inv = 255 - sa;
			dst[fmt.r] = uint8_t(Kernels_divide255(
					uint32_t(Kernels_toUnorm8(c.r)) * sa + uint32_t(dst[fmt.r]) * inv));
			if (fmt.size > 1) {
				dst[fmt.g] = uint8_t(Kernels_divide255(
						uint32_t(Kernels_toUnorm8(c.g)) * sa + uint32_t(dst[fmt.g]) * inv));
				dst[fmt.b] = uint8_t(Kernels_divide255(
						uint32_t(Kernels_toUnorm8(c.b)) * sa + uint32_t(dst[fmt.b]) * inv));
			}
		}

		r += ctx.dr;
		g += ctx.dg;
		b += ctx.db;
		a += ctx.da;
		u += ctx.du;
		v += ctx.dv;
		layer += ctx.dlayer;
		dst += fmt.size;
	}
}

// blend x texture kind: eight specializations, picked by a plain switch. The plan calls for a
// table of function pointers, but that is the SIMD step (M4) - a switch keeps the scalar
// reference readable and lets the compiler inline the sampler.
template <TextureKind Kind>
static inline void Kernels_writeSpanKind(SpanContext &ctx, const ChannelLayout &fmt,
		BlendMode blend) {
	switch (blend) {
	case BlendMode::Solid: Kernels_writeSpanSolid<Kind>(ctx, fmt); break;
	case BlendMode::Transparent: Kernels_writeSpanTransparent<Kind>(ctx, fmt); break;
	}
}

static void Kernels_writeSpan(SpanContext &ctx, const ChannelLayout &fmt, BlendMode blend) {
	if (ctx.count == 0 || fmt.size == 0) {
		return;
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

} // namespace stappler::xenolith::soft::raster
