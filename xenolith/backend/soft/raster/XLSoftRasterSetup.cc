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

// A vertex snapped to the subpixel grid. Snapping before the edge functions - rather than
// evaluating them in float - is what makes a shared edge produce bit-identical coverage from
// both of the triangles that own it.
struct FixedVertex {
	int32_t x = 0;
	int32_t y = 0;
};

static inline int32_t Setup_toFixed(float v) {
	return int32_t(sprt::floor(double(v) * double(SubpixelScale) + 0.5));
}

// Twice the signed area of (a, b, c). int64 because the operands are 24.8 fixed point: at 4K the
// products already exceed what int32 can hold.
static inline int64_t Setup_orient2d(const FixedVertex &a, const FixedVertex &b,
		const FixedVertex &c) {
	return int64_t(b.x - a.x) * int64_t(c.y - a.y) - int64_t(b.y - a.y) * int64_t(c.x - a.x);
}

// Top-left fill rule, Vulkan's: a pixel exactly on an edge belongs to the triangle only if that
// edge is a top or a left edge. Without this, adjacent triangles either both claim a shared edge
// (double blending, a visible seam on the AA fringe) or neither does (a crack).
static inline bool Setup_isTopLeft(const FixedVertex &a, const FixedVertex &b) {
	// Y grows downwards here: "top" is a horizontal edge going left, "left" is any edge going up.
	return (a.y == b.y && b.x < a.x) || (b.y < a.y);
}

void fillRect(const Target &target, const URect &rect, const Color4F &color) {
	if (target.empty()) {
		return;
	}

	auto fmt = Kernels_getLayout(target.format);
	if (fmt.size == 0) {
		return;
	}

	auto left = sprt::min(rect.x, target.width);
	auto top = sprt::min(rect.y, target.height);
	auto right = sprt::min(uint32_t(rect.x + rect.width), target.width);
	auto bottom = sprt::min(uint32_t(rect.y + rect.height), target.height);

	if (left >= right || top >= bottom) {
		return;
	}

	SpanContext ctx;
	ctx.count = right - left;
	ctx.r = color.r;
	ctx.g = color.g;
	ctx.b = color.b;
	ctx.a = color.a;

	for (uint32_t y = top; y < bottom; ++y) {
		ctx.dst = target.pixels + size_t(y) * size_t(target.stride) + size_t(left) * fmt.size;
		Kernels_writeSpan(ctx, fmt, BlendMode::Solid);
	}
}

// Rasterize one triangle. Scanline walk with incremental edge functions: for each row the three
// edge values are stepped by their dx, and coverage is the sign test of all three.
static void Setup_drawTriangle(const Target &target, const ChannelLayout &fmt, const Vertex &v0,
		const Vertex &v1, const Vertex &v2, const Command &cmd, const Texture *texture,
		const URect &clip) {
	FixedVertex p0{Setup_toFixed(v0.x), Setup_toFixed(v0.y)};
	FixedVertex p1{Setup_toFixed(v1.x), Setup_toFixed(v1.y)};
	FixedVertex p2{Setup_toFixed(v2.x), Setup_toFixed(v2.y)};

	auto area = Setup_orient2d(p0, p1, p2);
	if (area == 0) {
		return; // degenerate, no coverage by definition
	}

	// Culling is off in the flat contract, so a clockwise triangle is not discarded - it is
	// flipped, and the fill rule is applied to the rewound version.
	const Vertex *a = &v0;
	const Vertex *b = &v1;
	const Vertex *c = &v2;
	if (area < 0) {
		sprt::swap(p1, p2);
		b = &v2;
		c = &v1;
		area = -area;
	}

	// Bounding box in whole pixels, intersected with the clip rect. Pixel centres are sampled,
	// so the box is derived from the centres too.
	auto minX = sprt::max(int32_t(clip.x),
			int32_t(sprt::floor(double(sprt::min(p0.x, sprt::min(p1.x, p2.x))) / SubpixelScale)));
	auto minY = sprt::max(int32_t(clip.y),
			int32_t(sprt::floor(double(sprt::min(p0.y, sprt::min(p1.y, p2.y))) / SubpixelScale)));
	auto maxX = sprt::min(int32_t(clip.x + clip.width) - 1,
			int32_t(sprt::ceil(double(sprt::max(p0.x, sprt::max(p1.x, p2.x))) / SubpixelScale)));
	auto maxY = sprt::min(int32_t(clip.y + clip.height) - 1,
			int32_t(sprt::ceil(double(sprt::max(p0.y, sprt::max(p1.y, p2.y))) / SubpixelScale)));

	minX = sprt::max(minX, 0);
	minY = sprt::max(minY, 0);
	maxX = sprt::min(maxX, int32_t(target.width) - 1);
	maxY = sprt::min(maxY, int32_t(target.height) - 1);

	if (minX > maxX || minY > maxY) {
		return;
	}

	// Bias of -1 on a non-top-left edge turns the ">= 0" test into "> 0" for that edge only.
	const int64_t bias0 = Setup_isTopLeft(p1, p2) ? 0 : -1;
	const int64_t bias1 = Setup_isTopLeft(p2, p0) ? 0 : -1;
	const int64_t bias2 = Setup_isTopLeft(p0, p1) ? 0 : -1;

	// Per-pixel steps of each edge function.
	const int64_t a0 = int64_t(p1.y - p2.y), b0 = int64_t(p2.x - p1.x);
	const int64_t a1 = int64_t(p2.y - p0.y), b1 = int64_t(p0.x - p2.x);
	const int64_t a2 = int64_t(p0.y - p1.y), b2 = int64_t(p1.x - p0.x);

	FixedVertex origin{minX * SubpixelScale + SubpixelScale / 2,
		minY * SubpixelScale + SubpixelScale / 2};

	int64_t w0Row = Setup_orient2d(p1, p2, origin) + bias0;
	int64_t w1Row = Setup_orient2d(p2, p0, origin) + bias1;
	int64_t w2Row = Setup_orient2d(p0, p1, origin) + bias2;

	const double invArea = 1.0 / double(area);

	SpanContext ctx;
	ctx.texture = texture;
	ctx.sampler = cmd.sampler;
	ctx.kind = cmd.kind;
	ctx.constantColor = cmd.constantColor;

	for (int32_t y = minY; y <= maxY; ++y) {
		int64_t w0 = w0Row;
		int64_t w1 = w1Row;
		int64_t w2 = w2Row;

		int32_t x = minX;
		while (x <= maxX) {
			// Skip to the first covered pixel of the row.
			if (w0 < 0 || w1 < 0 || w2 < 0) {
				w0 += a0 * SubpixelScale;
				w1 += a1 * SubpixelScale;
				w2 += a2 * SubpixelScale;
				++x;
				continue;
			}

			// Collect the whole covered run, so the kernel gets a span rather than a pixel.
			int32_t spanStart = x;
			int64_t s0 = w0, s1 = w1, s2 = w2;
			while (x <= maxX && w0 >= 0 && w1 >= 0 && w2 >= 0) {
				w0 += a0 * SubpixelScale;
				w1 += a1 * SubpixelScale;
				w2 += a2 * SubpixelScale;
				++x;
			}

			auto count = uint32_t(x - spanStart);

			// Barycentrics at the span start and their per-pixel derivative. w = 1 throughout
			// (the 2d pipeline is affine), so this is plain linear interpolation.
			double l0 = double(s0) * invArea;
			double l1 = double(s1) * invArea;
			double l2 = double(s2) * invArea;

			double d0 = double(a0) * SubpixelScale * invArea;
			double d1 = double(a1) * SubpixelScale * invArea;
			double d2 = double(a2) * SubpixelScale * invArea;

			ctx.dst = target.pixels + size_t(y) * size_t(target.stride) + size_t(spanStart) * fmt.size;
			ctx.count = count;

			ctx.r = float(l0 * a->color.r + l1 * b->color.r + l2 * c->color.r);
			ctx.g = float(l0 * a->color.g + l1 * b->color.g + l2 * c->color.g);
			ctx.b = float(l0 * a->color.b + l1 * b->color.b + l2 * c->color.b);
			ctx.a = float(l0 * a->color.a + l1 * b->color.a + l2 * c->color.a);

			ctx.dr = float(d0 * a->color.r + d1 * b->color.r + d2 * c->color.r);
			ctx.dg = float(d0 * a->color.g + d1 * b->color.g + d2 * c->color.g);
			ctx.db = float(d0 * a->color.b + d1 * b->color.b + d2 * c->color.b);
			ctx.da = float(d0 * a->color.a + d1 * b->color.a + d2 * c->color.a);

			// Texture coordinates ride the same barycentrics. Interpolating them for a Solid
			// command would be dead work, and the compiler cannot see that from here.
			if (cmd.kind != TextureKind::Solid) {
				ctx.u = float(l0 * a->u + l1 * b->u + l2 * c->u);
				ctx.v = float(l0 * a->v + l1 * b->v + l2 * c->v);
				ctx.layer = float(l0 * a->layer + l1 * b->layer + l2 * c->layer);

				ctx.du = float(d0 * a->u + d1 * b->u + d2 * c->u);
				ctx.dv = float(d0 * a->v + d1 * b->v + d2 * c->v);
				ctx.dlayer = float(d0 * a->layer + d1 * b->layer + d2 * c->layer);
			}

			Kernels_writeSpan(ctx, fmt, cmd.blend);
		}

		w0Row += b0 * SubpixelScale;
		w1Row += b1 * SubpixelScale;
		w2Row += b2 * SubpixelScale;
	}
}

uint32_t draw(const Target &target, const DrawList &list) {
	if (target.empty() || list.empty()) {
		return 0;
	}

	auto fmt = Kernels_getLayout(target.format);
	if (fmt.size == 0) {
		log::source().error("soft::raster", "Unsupported target format: ",
				core::getImageFormatName(target.format));
		return 0;
	}

	uint32_t drawn = 0;

	for (auto &cmd : list.commands) {
		if (cmd.indexCount < 3 || cmd.scissor.width == 0 || cmd.scissor.height == 0) {
			continue;
		}

		auto last = size_t(cmd.firstIndex) + size_t(cmd.indexCount);
		if (last > list.indexes.size()) {
			log::source().error("soft::raster", "Command index range is out of bounds");
			continue;
		}

		const Texture *texture = nullptr;
		if (cmd.kind != TextureKind::Solid) {
			if (cmd.texture >= list.textures.size()) {
				log::source().error("soft::raster", "Command texture index is out of bounds");
				continue;
			}
			texture = &list.textures[cmd.texture];
		}

		for (uint32_t i = 0; i + 2 < cmd.indexCount; i += 3) {
			auto i0 = list.indexes[cmd.firstIndex + i];
			auto i1 = list.indexes[cmd.firstIndex + i + 1];
			auto i2 = list.indexes[cmd.firstIndex + i + 2];

			if (i0 >= list.vertexes.size() || i1 >= list.vertexes.size()
					|| i2 >= list.vertexes.size()) {
				log::source().error("soft::raster", "Vertex index is out of bounds");
				break;
			}

			Setup_drawTriangle(target, fmt, list.vertexes[i0], list.vertexes[i1], list.vertexes[i2],
					cmd, texture, cmd.scissor);
		}

		++drawn;
	}

	return drawn;
}

} // namespace stappler::xenolith::soft::raster
