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

// The fragment stage's texture unit: this is `texture(sampler2D(...), uv)` of xl_2d_flat.frag,
// written out. Everything here is per-texel work that the kernels call into, so it stays header-
// like: small functions, no state, no allocation.

namespace STAPPLER_VERSIONIZED stappler::raster {

// One texel, decoded to normalized float. Channels absent from the format read as the UNORM
// identity a GPU would produce: 0 for missing colour, 1 for missing alpha.
struct Texel {
	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;
	float a = 1.0f;
};

static inline float Sample_fromUnorm8(uint8_t v) { return float(v) * (1.0f / 255.0f); }

// Wrap a texel coordinate into [0, size). Vulkan address modes; the flat queue's three samplers
// only ever use Repeat and ClampToEdge, the rest are here so an unexpected sampler degrades to
// something defined rather than to an out-of-bounds read.
static inline int32_t Sample_address(int32_t v, int32_t size, AddressMode mode) {
	if (size <= 0) {
		return 0;
	}

	switch (mode) {
	case AddressMode::Repeat: {
		auto r = v % size;
		return r < 0 ? r + size : r;
	}
	case AddressMode::MirroredRepeat: {
		auto period = size * 2;
		auto r = v % period;
		if (r < 0) {
			r += period;
		}
		return r < size ? r : period - r - 1;
	}
	case AddressMode::ClampToEdge:
	case AddressMode::ClampToBorder: return sprt::clamp(v, 0, size - 1);
	}
	return sprt::clamp(v, 0, size - 1);
}

// Fetch one texel of one layer. `layer` is already resolved and clamped by the caller.
static Texel Sample_fetch(const Texture &tex, int32_t x, int32_t y, uint32_t layer) {
	Texel out;
	if (!tex.pixels) {
		return out;
	}

	auto src = tex.pixels + size_t(layer) * tex.layerSize + size_t(y) * tex.stride;

	switch (tex.format) {
	case PixelFormat::R8:
		out.r = Sample_fromUnorm8(src[x]);
		out.g = 0.0f;
		out.b = 0.0f;
		out.a = 1.0f;
		break;
	case PixelFormat::RGBA8888:
		src += size_t(x) * 4;
		out.r = Sample_fromUnorm8(src[0]);
		out.g = Sample_fromUnorm8(src[1]);
		out.b = Sample_fromUnorm8(src[2]);
		out.a = Sample_fromUnorm8(src[3]);
		break;
	case PixelFormat::BGRA8888:
		src += size_t(x) * 4;
		out.b = Sample_fromUnorm8(src[0]);
		out.g = Sample_fromUnorm8(src[1]);
		out.r = Sample_fromUnorm8(src[2]);
		out.a = Sample_fromUnorm8(src[3]);
		break;
	case PixelFormat::Undefined: break;
	}
	return out;
}

static inline float Sample_swizzleChannel(const Texel &t, ComponentMapping mapping,
		float identity) {
	switch (mapping) {
	case ComponentMapping::Identity: return identity;
	case ComponentMapping::Zero: return 0.0f;
	case ComponentMapping::One: return 1.0f;
	case ComponentMapping::R: return t.r;
	case ComponentMapping::G: return t.g;
	case ComponentMapping::B: return t.b;
	case ComponentMapping::A: return t.a;
	}
	return identity;
}

static Color4F Sample_applySwizzle(const Texture &tex, const Texel &t) {
	return Color4F(Sample_swizzleChannel(t, tex.swizzle[0], t.r),
			Sample_swizzleChannel(t, tex.swizzle[1], t.g),
			Sample_swizzleChannel(t, tex.swizzle[2], t.b),
			Sample_swizzleChannel(t, tex.swizzle[3], t.a));
}

// One texel as raw bytes in logical R, G, B, A order - what bilinear filtering works on.
//
// The float form above exists for the nearest path, where a texel is fetched once and multiplied
// straight into the fragment colour. Bilinear is a different shape: four texels, three
// interpolations, and only then a single multiply. Doing that in float means eight converts per
// pixel and four lanes per register where sixteen bytes would fit - so it is done in fixed point,
// on the bytes, which is also what the hardware does.
struct TexelBytes {
	uint8_t v[4] = {0, 0, 0, 255};
};

static TexelBytes Sample_fetchBytes(const Texture &tex, int32_t x, int32_t y, uint32_t layer) {
	TexelBytes out;
	if (!tex.pixels) {
		return out;
	}

	auto src = tex.pixels + size_t(layer) * tex.layerSize + size_t(y) * tex.stride;

	switch (tex.format) {
	case PixelFormat::R8:
		out.v[0] = src[x];
		out.v[1] = 0;
		out.v[2] = 0;
		out.v[3] = 255;
		break;
	case PixelFormat::RGBA8888:
		src += size_t(x) * 4;
		out.v[0] = src[0];
		out.v[1] = src[1];
		out.v[2] = src[2];
		out.v[3] = src[3];
		break;
	case PixelFormat::BGRA8888:
		src += size_t(x) * 4;
		out.v[2] = src[0];
		out.v[1] = src[1];
		out.v[0] = src[2];
		out.v[3] = src[3];
		break;
	case PixelFormat::Undefined: break;
	}
	return out;
}

// Bilinear of four bytes with fixed-point weights, rounded once.
//
// Both interpolations happen before the shift, so there is one rounding for the whole filter
// rather than three - more accurate than the obvious lerp-of-lerps, and no more work. The bound
// that makes it fit: each row sum is at most 255 << SampleWeightBits, and multiplying that by a
// weight again stays inside int32.
//
// Vulkan requires at least 8 fractional bits of weight and leaves anything beyond that to the
// implementation, which is exactly why a bilinear case is compared against the GPU under tolerance
// and never exactly.
uint8_t Sample_bilinearByte(uint8_t t00, uint8_t t10, uint8_t t01, uint8_t t11, int32_t wx,
		int32_t wy) {
	const int32_t ix = SampleWeightOne - wx;
	const int32_t iy = SampleWeightOne - wy;

	const int32_t top = int32_t(t00) * ix + int32_t(t10) * wx;
	const int32_t bottom = int32_t(t01) * ix + int32_t(t11) * wx;

	constexpr int32_t half = 1 << (2 * SampleWeightBits - 1);
	return uint8_t((top * iy + bottom * wy + half) >> (2 * SampleWeightBits));
}

// The fractional part of a texel coordinate, as a weight. Truncated, not rounded: that is what
// hardware does with the bits below its weight precision.
int32_t Sample_weight(float coord, int32_t whole) {
	auto w = int32_t((coord - float(whole)) * float(SampleWeightOne));
	return sprt::clamp(w, 0, SampleWeightOne);
}

// Resolve the third coordinate. A 2D array indexes a layer directly (the shader passes pos.z as
// an integer slice); a 3D image addresses depth in the same linear storage, so both collapse to
// "pick a slice" here.
static inline uint32_t Sample_resolveLayer(const Texture &tex, TextureKind kind, float layer) {
	uint32_t count = 1;
	switch (kind) {
	case TextureKind::Texture2DArray: count = tex.layers; break;
	case TextureKind::Texture3D: count = tex.depth; break;
	default: return tex.baseLayer;
	}

	if (count == 0) {
		return tex.baseLayer;
	}

	auto index = int32_t(sprt::floor(layer + 0.5f));
	return tex.baseLayer + uint32_t(sprt::clamp(index, 0, int32_t(count) - 1));
}

// texture(sampler, vec2/vec3) for one fragment.
//
// Nearest reproduces the GPU exactly - the same texel is selected by the same floor of the same
// scaled coordinate. Linear does not, and cannot: Vulkan leaves the precision of the filter
// weights to the implementation, so a bilinear case is compared under tolerance, never exactly.
static Color4F Sample_texture(const Texture &tex, const Sampler &sampler, TextureKind kind, float u,
		float v, float layer) {
	auto slice = Sample_resolveLayer(tex, kind, layer);

	if (sampler.filter == Filter::Nearest) {
		auto x = Sample_address(int32_t(sprt::floor(u * float(tex.width))), int32_t(tex.width),
				sampler.addressU);
		auto y = Sample_address(int32_t(sprt::floor(v * float(tex.height))), int32_t(tex.height),
				sampler.addressV);
		return Sample_applySwizzle(tex, Sample_fetch(tex, x, y, slice));
	}

	// Half-texel offset: the sample point sits at the texel centre, so the bilinear weights are
	// the fractional distance from one centre to the next.
	auto fx = u * float(tex.width) - 0.5f;
	auto fy = v * float(tex.height) - 0.5f;

	auto x0 = int32_t(sprt::floor(fx));
	auto y0 = int32_t(sprt::floor(fy));
	auto wx = Sample_weight(fx, x0);
	auto wy = Sample_weight(fy, y0);

	auto ax0 = Sample_address(x0, int32_t(tex.width), sampler.addressU);
	auto ax1 = Sample_address(x0 + 1, int32_t(tex.width), sampler.addressU);
	auto ay0 = Sample_address(y0, int32_t(tex.height), sampler.addressV);
	auto ay1 = Sample_address(y0 + 1, int32_t(tex.height), sampler.addressV);

	auto t00 = Sample_fetchBytes(tex, ax0, ay0, slice);
	auto t10 = Sample_fetchBytes(tex, ax1, ay0, slice);
	auto t01 = Sample_fetchBytes(tex, ax0, ay1, slice);
	auto t11 = Sample_fetchBytes(tex, ax1, ay1, slice);

	Texel out;
	out.r = Sample_fromUnorm8(Sample_bilinearByte(t00.v[0], t10.v[0], t01.v[0], t11.v[0], wx, wy));
	out.g = Sample_fromUnorm8(Sample_bilinearByte(t00.v[1], t10.v[1], t01.v[1], t11.v[1], wx, wy));
	out.b = Sample_fromUnorm8(Sample_bilinearByte(t00.v[2], t10.v[2], t01.v[2], t11.v[2], wx, wy));
	out.a = Sample_fromUnorm8(Sample_bilinearByte(t00.v[3], t10.v[3], t01.v[3], t11.v[3], wx, wy));

	return Sample_applySwizzle(tex, out);
}

// The constant a 1x1 image samples to. Folding it into the command turns every plain Layer back
// into the exact solid path M0 had, with no per-pixel fetch at all.
SP_PUBLIC Color4F sampleConstant(const Texture &tex) {
	return Sample_applySwizzle(tex, Sample_fetch(tex, 0, 0, tex.baseLayer));
}

void Sample_bilinearNaive(const TextureSpan &tex, const Sampler &sampler, float u, float v,
		uint8_t out[4]) {
	auto fx = u * float(tex.width) - 0.5f;
	auto fy = v * float(tex.height) - 0.5f;

	auto x0 = int32_t(sprt::floor(fx));
	auto y0 = int32_t(sprt::floor(fy));

	auto wx = Sample_weight(fx, x0);
	auto wy = Sample_weight(fy, y0);

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
		out[k] = Sample_bilinearByte(row0[size_t(ax0) * 4 + s], row0[size_t(ax1) * 4 + s],
				row1[size_t(ax0) * 4 + s], row1[size_t(ax1) * 4 + s], wx, wy);
	}
}

// Where a ComponentMapping sends one destination channel. See TextureSpan::swizzle.
static inline int8_t Sample_resolveMapping(ComponentMapping mapping, int8_t identity) {
	switch (mapping) {
	case ComponentMapping::Identity: return identity;
	case ComponentMapping::Zero: return -1;
	case ComponentMapping::One: return -2;
	case ComponentMapping::R: return 0;
	case ComponentMapping::G: return 1;
	case ComponentMapping::B: return 2;
	case ComponentMapping::A: return 3;
	}
	return identity;
}

bool resolveTextureSpan(const SpanContext &ctx, const ChannelLayout &fmt, TextureSpan &out) {
	if (fmt.size != 4 || ctx.kind != TextureKind::Texture2D || !ctx.texture) {
		return false;
	}

	auto &tex = *ctx.texture;
	if (!tex.pixels || tex.width == 0 || tex.height == 0) {
		return false;
	}

	switch (tex.format) {
	case PixelFormat::RGBA8888: out.src[0] = 0, out.src[1] = 1, out.src[2] = 2, out.src[3] = 3; break;
	case PixelFormat::BGRA8888: out.src[0] = 2, out.src[1] = 1, out.src[2] = 0, out.src[3] = 3; break;
	default: return false; // R8 is the glyph path and never reaches a textured span
	}

	out.pixels = tex.pixels + size_t(tex.baseLayer) * tex.layerSize;
	out.stride = tex.stride;
	out.width = int32_t(tex.width);
	out.height = int32_t(tex.height);
	out.linear = (ctx.sampler.filter == Filter::Linear);

	for (int i = 0; i < 4; ++i) {
		out.swizzle[i] = Sample_resolveMapping(tex.swizzle[i], int8_t(i));
	}

	// Does the span stay inside the texture? The coordinates are affine along it, so the two
	// endpoints bound everything between them. This is the ordinary sprite, and it makes the
	// address mode irrelevant - which matters because the flat queue's nearest sampler is Repeat,
	// and a modulo by a runtime value is the one thing these instruction sets cannot do.
	//
	// The endpoints are the run's, not the anchor's: the attributes are evaluated in the anchor
	// column, which is left of the run and may be far outside the texture. Bounding the wrong
	// interval here would let a bilinear fetch read its eight-byte pair without a clamp.
	auto first = float(ctx.originOffset);
	auto last = float(ctx.originOffset + (ctx.count > 0 ? ctx.count - 1 : 0));
	auto uStart = ctx.u + ctx.du * first;
	auto vStart = ctx.v + ctx.dv * first;
	auto uEnd = ctx.u + ctx.du * last;
	auto vEnd = ctx.v + ctx.dv * last;

	// What the filter has to reach. Nearest needs floor(c*size) in [0, size); bilinear needs both
	// floor(c*size - 0.5) and its neighbour, so the usable range shrinks by a texel at each end.
	auto inside = [linear = out.linear](float a, float b, float size) {
		auto lo = sprt::min(a, b) * size;
		auto hi = sprt::max(a, b) * size;
		if (linear) {
			return (lo - 0.5f) >= 0.0f && (hi - 0.5f) < (size - 1.0f);
		}
		return lo >= 0.0f && hi < size;
	};

	out.inRange = inside(uStart, uEnd, float(tex.width)) && inside(vStart, vEnd, float(tex.height));

	auto isPot = [](uint32_t v) { return v != 0 && (v & (v - 1)) == 0; };
	out.powerOfTwo = isPot(tex.width) && isPot(tex.height);

	if (out.inRange) {
		return true;
	}

	// Outside the texture the coordinate is unbounded, and a float-to-int conversion that
	// overflows int32 is undefined in the scalar path and saturating in the vector one - they
	// would stop agreeing. Well inside what a float represents as an exact integer, so this
	// refuses only spans that are already nonsense.
	constexpr float coordLimit = 1.0f * (1 << 24);
	if (sprt::fabs(uStart) * float(tex.width) > coordLimit
			|| sprt::fabs(uEnd) * float(tex.width) > coordLimit
			|| sprt::fabs(vStart) * float(tex.height) > coordLimit
			|| sprt::fabs(vEnd) * float(tex.height) > coordLimit) {
		return false;
	}

	// Outside the texture the mode decides. Clamp is trivial in a vector; Repeat is only tractable
	// when the size is a power of two, and mirroring never is.
	auto mode = ctx.sampler.addressU;
	if (mode != ctx.sampler.addressV) {
		return false;
	}

	switch (mode) {
	case AddressMode::ClampToEdge:
	case AddressMode::ClampToBorder: return true;
	case AddressMode::Repeat: return out.powerOfTwo;
	case AddressMode::MirroredRepeat: return false;
	}
	return false;
}

} // namespace stappler::raster
