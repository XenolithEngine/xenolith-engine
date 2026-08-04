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

#ifndef STAPPLER_RASTER_SPRASTERKERNEL_H_
#define STAPPLER_RASTER_SPRASTERKERNEL_H_

#include "SPRaster.h"
#include "SPRasterAttr.h"

// Internal contract between the triangle setup and the pixel loops. Not part of the module's
// public surface: everything here is an implementation detail of one translation unit.

namespace STAPPLER_VERSIONIZED stappler::raster {

// Byte offsets of the colour channels within a pixel of a given format. Anything the module
// accepts is 8-bit and interleaved, so a permutation is all a kernel needs to know.
struct ChannelLayout {
	uint8_t r = 0;
	uint8_t g = 1;
	uint8_t b = 2;
	uint8_t a = 3;
	uint8_t size = 4;
	bool hasAlpha = true;
};

inline ChannelLayout getChannelLayout(PixelFormat format) {
	switch (format) {
	case PixelFormat::RGBA8888: return ChannelLayout{0, 1, 2, 3, 4, true};
	case PixelFormat::BGRA8888: return ChannelLayout{2, 1, 0, 3, 4, true};
	// Single-channel targets exist for font atlases; red carries the value and there is no alpha
	// to blend against, so the "alpha" slot aliases it.
	case PixelFormat::R8: return ChannelLayout{0, 0, 0, 0, 1, false};
	case PixelFormat::Undefined: break;
	}
	return ChannelLayout{0, 1, 2, 3, 0, true};
}

// Colour and texture coordinate of one pixel, plus the per-pixel increments along the span. Kept
// as separate scalars (SoA) rather than a Color4F so a vector kernel can load four lanes of one
// channel at a time.
struct SpanContext {
	uint8_t *dst = nullptr;
	uint32_t count = 0;

	// Column of the first pixel, counted from the anchor the attributes below were evaluated in -
	// the left edge of the triangle's bounding box, before any clipping. A kernel therefore
	// interpolates at `originOffset + i`, never at `i`, and the value it computes depends only on
	// which column the pixel is in, not on where the run it belongs to happened to start. That is
	// what lets the same region be drawn in one pass or in tiles and come out byte for byte the
	// same; see the anchor note in Setup_drawTriangle.
	uint32_t originOffset = 0;

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

// The arithmetic contract. Every kernel set - scalar, SWAR, vector - must produce exactly these
// values, so the definitions live here rather than in any one set: they are shared, not copied.

inline uint8_t Kernels_toUnorm8(float v) {
	// The multiply must happen in double. In float, 0.9f * 255.0f rounds up to exactly 229.5f and
	// the +0.5 then carries it to 230, while the GPU (and double here) sees 229.49999 and produces
	// 229 - a visible 1/255 mismatch against the Vulkan reference on every such colour.
	auto i = int32_t(double(v) * 255.0 + 0.5);
	return uint8_t(sprt::clamp(i, 0, 255));
}

// round(x / 255) for x in [0, 255*255], without a division.
inline uint32_t Kernels_divide255(uint32_t x) {
	auto t = x + 128;
	return (t + (t >> 8)) >> 8;
}

// src*srcAlpha + dst*(1-srcAlpha), with both source operands already quantized to 8 bits.
//
// The obvious "more correct" alternative - keep the source in float, blend, and round once at the
// end - was implemented and measured against Vulkan, and it is *worse*: 966 differing pixels
// instead of 20 on the `alpha` case, 268 instead of 40 on `label`, and identical everywhere the
// difference comes from sampling rather than blending. The hardware evidently converts the
// fragment colour to the attachment's precision before blending, which the Vulkan specification
// permits ("a precision no lower than that of the color attachment format"). So this quantize-then
// -blend form is not an approximation of what the GPU does - it is a model of it, and the closest
// one available. Do not "fix" it into floating point.
//
// What remains is a last-bit difference on partially covered pixels, which the parity harness
// accepts by design (one step in one channel is the smallest an 8-bit channel can express).
inline uint8_t Kernels_blend(uint32_t src, uint32_t srcAlpha, uint8_t dst) {
	return uint8_t(Kernels_divide255(src * srcAlpha + uint32_t(dst) * (255 - srcAlpha)));
}

// Fractional bits of a bilinear filter weight.
//
// Vulkan requires at least 8 and leaves the rest to the implementation. Eight was tried and is not
// enough here: it puts 39 pixels of `sprite-rotated` two steps away from the Vulkan reference,
// where the gate allows one. The hardware evidently keeps more, so this keeps more too.
//
// Eleven is the ceiling for int32: the filter's largest intermediate is 255 << (2*bits), and
// 255 << 22 is just under 2^31. Going further would mean 64-bit intermediates, which would cost
// more than the precision is worth.
static constexpr int32_t SampleWeightBits = 11;
static constexpr int32_t SampleWeightOne = 1 << SampleWeightBits;

// Bilinear of one channel of four texels, weights in SampleWeightBits fixed point. Shared, not
// copied: the scalar sampler, the magnification walk and the vector kernels all have to produce
// the same byte.
SP_PUBLIC uint8_t Sample_bilinearByte(uint8_t t00, uint8_t t10, uint8_t t01, uint8_t t11,
		int32_t wx, int32_t wy);

// Fractional part of a texel coordinate as a weight, truncated to SampleWeightBits.
SP_PUBLIC int32_t Sample_weight(float coord, int32_t whole);


// One pixel of the source colour, already quantized, laid out in the target's byte order.
//
// This is the shape every vector kernel actually wants. A span whose colour does not change and
// whose texture is a folded constant has no float work left at all: the shading collapses to these
// four bytes, and what remains is integer blending, which is exactly what splits into lanes
// without changing a single result. In a real UI most of the surface is such spans - backgrounds,
// buttons, separators, every plain Layer.
struct SpanConstant {
	uint8_t bytes[4] = {0, 0, 0, 0};
	uint32_t alpha = 0; // the source alpha, before it was placed into `bytes`
};

// True when the span's colour is the same at every pixel of it.
inline bool isConstantSpan(const SpanContext &ctx) {
	return ctx.kind == TextureKind::Solid && ctx.dr == 0.0f && ctx.dg == 0.0f && ctx.db == 0.0f
			&& ctx.da == 0.0f;
}

// Quantize a constant span once. Same operations as the per-pixel path, so the bytes are the same
// bytes - this is a hoist, not a second implementation.
inline SpanConstant getSpanConstant(const SpanContext &ctx, const ChannelLayout &fmt) {
	auto &c = ctx.constantColor;
	SpanConstant out;
	out.bytes[fmt.r] = Kernels_toUnorm8(ctx.r * c.r);
	if (fmt.size > 1) {
		out.bytes[fmt.g] = Kernels_toUnorm8(ctx.g * c.g);
		out.bytes[fmt.b] = Kernels_toUnorm8(ctx.b * c.b);
		out.bytes[fmt.a] = Kernels_toUnorm8(ctx.a * c.a);
	}
	out.alpha = out.bytes[fmt.a];
	return out;
}

// Everything a vector kernel needs to sample a texture, resolved once per span.
//
// The scalar sampler re-derives all of this per pixel - the format switch, the swizzle switch, the
// address mode switch - which is fine when it is one pixel among a hundred instructions and
// nonsense when four or eight pixels are in flight. Hoisting it is most of what makes the vector
// path worth having; the lane arithmetic is the smaller half.
struct TextureSpan {
	const uint8_t *pixels = nullptr;
	uint32_t stride = 0;
	int32_t width = 1;
	int32_t height = 1;

	// Byte offset of each source channel inside a texel, in R, G, B, A order.
	uint8_t src[4] = {0, 1, 2, 3};

	// For each destination channel: which source channel feeds it, or a constant. Resolved from
	// the view's ComponentMapping so no kernel has to look at it again.
	//   >= 0 - index into `src`
	//   -1   - constant 0
	//   -2   - constant 1
	int8_t swizzle[4] = {0, 1, 2, 3};

	// Bilinear rather than nearest. Four texels and a weighted sum per pixel instead of one fetch.
	bool linear = false;

	// True when the whole span is known to land inside the texture, so wrapping is a no-op. That
	// is the ordinary sprite - one that shows its texture once - and it lets Repeat, which needs a
	// modulo the vector units cannot do, be skipped entirely. For a bilinear span this accounts for
	// the +1 neighbour, which is why an unrotated sprite showing its whole texture still fails it:
	// the last column's neighbour is off the edge.
	bool inRange = false;

	// Set when the texture dimensions are powers of two, so Repeat becomes a mask. Only consulted
	// when `inRange` is false.
	bool powerOfTwo = false;
};

// Can this span be sampled by a vector kernel, and if so, with what?
//
// Deliberately narrow. Everything it refuses - array and 3D textures, linear filtering, single
// channel targets, a Repeat that would need a real modulo - falls back to the scalar sampler,
// which handles every case and is the definition of correct.
SP_PUBLIC bool resolveTextureSpan(const SpanContext &, const ChannelLayout &, TextureSpan &);

// Bilinear at one point, fetched fresh, with no reuse between calls: the reference the span
// samplers are held against.
//
// The fast paths cache a texel column across pixels, and getting the cache key wrong is invisible
// in most frames - it took a rotated sprite to expose a missing `wy` in it. A naive form that
// cannot be wrong, held against them over a sweep of coordinates, catches that class directly.
SP_PUBLIC void Sample_bilinearNaive(const TextureSpan &, const Sampler &, float u, float v,
		uint8_t out[4]);


// Which implementation of the pixel loops is in use.
//
// Scalar and Swar exist on every architecture by construction - one is plain C++ per pixel, the
// other is plain C++ over a 64-bit register - so there is always something to fall back to and
// always a reference to compare against. The rest are gated on what the CPU can actually do.
enum class KernelSet {
	Scalar,
	Swar,
	Sse2,
	Sse41,
	Avx2,
	Neon,
};

SP_PUBLIC StringView getKernelSetName(KernelSet);

// The hot path, and nothing else. Everything outside this table is called once per command or
// less often, where an indirect call would be noise.
//
// Note what is NOT indirect: there is no per-pixel dispatch anywhere. `writeSpan` is one call per
// horizontal run of a triangle, `blitGlyph` one call per glyph, `fillRect` one per rectangle.
struct KernelTable {
	KernelSet set = KernelSet::Scalar;

	// Shade and write one horizontal run.
	void (*writeSpan)(SpanContext &, const ChannelLayout &, BlendMode) = nullptr;

	// One glyph coverage bitmap onto the target, 1:1, already scissored by the caller.
	void (*blitGlyph)(const Target &, const GlyphBlit &, const ChannelLayout &) = nullptr;

	// Constant colour over a rectangle that the caller has already clipped and found non-empty.
	void (*fillRect)(const Target &, const URect &, const ChannelLayout &, const Color4F &) =
			nullptr;
};

// Resolved once per process: SP_RASTER_KERNELS -> what the CPU supports -> Scalar.
SP_PUBLIC const KernelTable &getKernels();

// Every set that can actually run here, best first. Used by the parity gate and the benchmark;
// Scalar is always present and is always the reference the others are compared against.
SP_PUBLIC SpanView<const KernelTable *> getAvailableKernels();

// The covered part of one scanline, as pixel columns. Empty when `lo > hi`.
//
// Exposed only so the unit test can hold the analytic solver against the stepping one; nothing
// outside the rasterizer has a use for it.
struct RowSpan {
	int32_t lo = 0;
	int32_t hi = -1;

	bool empty() const { return lo > hi; }
};

// Where the three edge functions are all non-negative along a row.
//
// `w0..w2` are their values at column `minX` (biases already folded in) and `step0..2` their
// per-column increments. Both forms must agree exactly on every input:
//
//   Stepping - what the rasterizer did until M4.25: walk the row, testing three signs per column.
//   O(width) whatever the covered run turns out to be, and it keeps walking after the run ends.
//
//   Analytic - solve each `w_k(x) >= 0` for x and intersect. Three divisions, no loop. The
//   triangle is convex, so the covered set of a row is a single interval and there is nothing
//   the loop could find that this misses.
RowSpan rowSpanStepping(int64_t w0, int64_t w1, int64_t w2, int64_t step0, int64_t step1,
		int64_t step2, int32_t minX, int32_t maxX);
RowSpan rowSpanAnalytic(int64_t w0, int64_t w1, int64_t w2, int64_t step0, int64_t step1,
		int64_t step2, int32_t minX, int32_t maxX);

// Per-set table accessors. Each is defined by its own subunit and returns null when the set is
// not implemented for this architecture.
const KernelTable *getScalarKernels();
const KernelTable *getSwarKernels();
const KernelTable *getSse2Kernels();
const KernelTable *getSse41Kernels();
const KernelTable *getAvx2Kernels();
const KernelTable *getNeonKernels();

// The scalar implementations, callable directly. A specialized set uses them for the spans it does
// not accelerate - a textured or interpolated run - instead of carrying a second copy of code that
// would then have to be kept bit-identical by review.
void writeSpanScalar(SpanContext &, const ChannelLayout &, BlendMode);
void blitGlyphScalar(const Target &, const GlyphBlit &, const ChannelLayout &);
void fillRectScalar(const Target &, const URect &, const ChannelLayout &, const Color4F &);

} // namespace stappler::raster

#endif /* STAPPLER_RASTER_SPRASTERKERNEL_H_ */
