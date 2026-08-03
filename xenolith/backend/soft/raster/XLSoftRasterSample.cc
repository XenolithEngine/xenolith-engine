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

// The fragment stage's texture unit: this is `texture(sampler2D(...), uv)` of xl_2d_flat.frag,
// written out. Everything here is per-texel work that the kernels call into, so it stays header-
// like: small functions, no state, no allocation.

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft::raster {

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
	case core::ImageFormat::R8_UNORM:
		out.r = Sample_fromUnorm8(src[x]);
		out.g = 0.0f;
		out.b = 0.0f;
		out.a = 1.0f;
		break;
	case core::ImageFormat::R8G8B8A8_UNORM:
		src += size_t(x) * 4;
		out.r = Sample_fromUnorm8(src[0]);
		out.g = Sample_fromUnorm8(src[1]);
		out.b = Sample_fromUnorm8(src[2]);
		out.a = Sample_fromUnorm8(src[3]);
		break;
	case core::ImageFormat::B8G8R8A8_UNORM:
		src += size_t(x) * 4;
		out.b = Sample_fromUnorm8(src[0]);
		out.g = Sample_fromUnorm8(src[1]);
		out.r = Sample_fromUnorm8(src[2]);
		out.a = Sample_fromUnorm8(src[3]);
		break;
	default: break;
	}
	return out;
}

static inline float Sample_swizzleChannel(const Texel &t, core::ComponentMapping mapping,
		float identity) {
	switch (mapping) {
	case core::ComponentMapping::Identity: return identity;
	case core::ComponentMapping::Zero: return 0.0f;
	case core::ComponentMapping::One: return 1.0f;
	case core::ComponentMapping::R: return t.r;
	case core::ComponentMapping::G: return t.g;
	case core::ComponentMapping::B: return t.b;
	case core::ComponentMapping::A: return t.a;
	}
	return identity;
}

static Color4F Sample_applySwizzle(const Texture &tex, const Texel &t) {
	return Color4F(Sample_swizzleChannel(t, tex.swizzle[0], t.r),
			Sample_swizzleChannel(t, tex.swizzle[1], t.g),
			Sample_swizzleChannel(t, tex.swizzle[2], t.b),
			Sample_swizzleChannel(t, tex.swizzle[3], t.a));
}

static inline Texel Sample_lerp(const Texel &a, const Texel &b, float f) {
	Texel out;
	out.r = a.r + (b.r - a.r) * f;
	out.g = a.g + (b.g - a.g) * f;
	out.b = a.b + (b.b - a.b) * f;
	out.a = a.a + (b.a - a.a) * f;
	return out;
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
	auto wx = fx - float(x0);
	auto wy = fy - float(y0);

	auto ax0 = Sample_address(x0, int32_t(tex.width), sampler.addressU);
	auto ax1 = Sample_address(x0 + 1, int32_t(tex.width), sampler.addressU);
	auto ay0 = Sample_address(y0, int32_t(tex.height), sampler.addressV);
	auto ay1 = Sample_address(y0 + 1, int32_t(tex.height), sampler.addressV);

	auto top = Sample_lerp(Sample_fetch(tex, ax0, ay0, slice), Sample_fetch(tex, ax1, ay0, slice),
			wx);
	auto bottom = Sample_lerp(Sample_fetch(tex, ax0, ay1, slice),
			Sample_fetch(tex, ax1, ay1, slice), wx);

	return Sample_applySwizzle(tex, Sample_lerp(top, bottom, wy));
}

// The constant a 1x1 image samples to. Folding it into the command turns every plain Layer back
// into the exact solid path M0 had, with no per-pixel fetch at all.
SP_PUBLIC Color4F sampleConstant(const Texture &tex) {
	return Sample_applySwizzle(tex, Sample_fetch(tex, 0, 0, tex.baseLayer));
}

} // namespace stappler::xenolith::soft::raster
