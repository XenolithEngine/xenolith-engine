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

namespace STAPPLER_VERSIONIZED stappler::raster {

uint32_t getPixelSize(PixelFormat format) { return getChannelLayout(format).size; }

StringView getPixelFormatName(PixelFormat format) {
	switch (format) {
	case PixelFormat::R8: return StringView("R8");
	case PixelFormat::RGBA8888: return StringView("RGBA8888");
	case PixelFormat::BGRA8888: return StringView("BGRA8888");
	case PixelFormat::Undefined: break;
	}
	return StringView("Undefined");
}

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

// Integer division that rounds toward -infinity / +infinity. C++ truncates toward zero, and the
// row bounds are exactly where that is wrong: an edge entering the row from the left has a
// negative numerator, and a truncating divide would place the span one column too far in.
static inline int64_t Setup_floorDiv(int64_t a, int64_t b) {
	auto q = a / b;
	auto r = a % b;
	return (r != 0 && ((r < 0) != (b < 0))) ? q - 1 : q;
}

static inline int64_t Setup_ceilDiv(int64_t a, int64_t b) {
	auto q = a / b;
	auto r = a % b;
	return (r != 0 && ((r < 0) == (b < 0))) ? q + 1 : q;
}

RowSpan rowSpanStepping(int64_t w0, int64_t w1, int64_t w2, int64_t step0, int64_t step1,
		int64_t step2, int32_t minX, int32_t maxX) {
	int32_t x = minX;

	while (x <= maxX && (w0 < 0 || w1 < 0 || w2 < 0)) {
		w0 += step0;
		w1 += step1;
		w2 += step2;
		++x;
	}

	if (x > maxX) {
		return RowSpan{};
	}

	auto lo = x;
	while (x <= maxX && w0 >= 0 && w1 >= 0 && w2 >= 0) {
		w0 += step0;
		w1 += step1;
		w2 += step2;
		++x;
	}

	return RowSpan{lo, x - 1};
}

RowSpan rowSpanAnalytic(int64_t w0, int64_t w1, int64_t w2, int64_t step0, int64_t step1,
		int64_t step2, int32_t minX, int32_t maxX) {
	// Solve in offsets from minX rather than absolute columns: it keeps the numbers small and
	// makes the initial bounds the clamp, so nothing has to be range-checked afterwards.
	int64_t lo = 0;
	int64_t hi = int64_t(maxX) - int64_t(minX);

	auto apply = [&](int64_t w, int64_t step) {
		if (step > 0) {
			// w + step*t >= 0  <=>  t >= ceil(-w / step)
			lo = sprt::max(lo, Setup_ceilDiv(-w, step));
		} else if (step < 0) {
			// w + step*t >= 0  <=>  t <= floor(w / -step)
			hi = sprt::min(hi, Setup_floorDiv(w, -step));
		} else if (w < 0) {
			// A horizontal edge either covers the whole row or none of it.
			lo = 1;
			hi = 0;
		}
	};

	apply(w0, step0);
	apply(w1, step1);
	apply(w2, step2);

	if (lo > hi) {
		return RowSpan{};
	}

	return RowSpan{minX + int32_t(lo), minX + int32_t(hi)};
}

void fillRect(const Target &target, const URect &rect, const Color4F &color, FillStats *stats) {
	if (target.empty()) {
		return;
	}

	auto fmt = getChannelLayout(target.format);
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

	auto clipped = URect{left, top, right - left, bottom - top};
	if (stats) {
		stats->fillPixels += uint64_t(clipped.width) * uint64_t(clipped.height);
	}
	getKernels().fillRect(target, clipped, fmt, color);
}

// Rasterize one triangle. Scanline walk with incremental edge functions: for each row the three
// edge values are stepped by their dx, and coverage is the sign test of all three.
static void Setup_drawTriangle(const KernelTable &kernels, const Target &target,
		const ChannelLayout &fmt, const Vertex &v0, const Vertex &v1, const Vertex &v2,
		const Command &cmd, const Texture *texture, const URect &clip, uint64_t *spanPixels) {
	// Rejection before anything else is computed. A triangle outside the clip used to pay for
	// three fixed-point conversions, a signed area and a possible rewind before the bounding box
	// finally told it so - fine when the clip was the damage region and the list was walked once,
	// but with tiles the entire list is walked once per tile and that cost is multiplied.
	//
	// Loose by a pixel on each side on purpose: the exact box below is built from the snapped
	// coordinates, and snapping to 1/256 of a pixel can move it. Being loose only sends a triangle
	// on to the exact test; being tight would drop one that covers something.
	{
		auto minXf = sprt::min(v0.x, sprt::min(v1.x, v2.x));
		auto maxXf = sprt::max(v0.x, sprt::max(v1.x, v2.x));
		auto minYf = sprt::min(v0.y, sprt::min(v1.y, v2.y));
		auto maxYf = sprt::max(v0.y, sprt::max(v1.y, v2.y));

		if (maxXf < float(clip.x) - 1.0f || minXf > float(clip.x + clip.width) + 1.0f
				|| maxYf < float(clip.y) - 1.0f || minYf > float(clip.y + clip.height) + 1.0f) {
			return;
		}
	}

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

	// Left edge of the bounding box before anything is clipped away. Attributes are evaluated
	// here rather than at the start of each run, and that is what makes the result independent of
	// where the run happens to start: the value of a pixel becomes a function of its absolute
	// column alone, so drawing a region in tiles produces the same bytes as drawing it whole.
	// Anchoring at the run instead costs a last bit whenever a clip rectangle cuts a run - which a
	// damage rectangle rarely does and a tile boundary always does.
	const int32_t anchorX =
			int32_t(sprt::floor(double(sprt::min(p0.x, sprt::min(p1.x, p2.x))) / SubpixelScale));

	// Bounding box in whole pixels, intersected with the clip rect. Pixel centres are sampled,
	// so the box is derived from the centres too.
	auto minX = sprt::max(int32_t(clip.x), anchorX);
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

	// Per-column steps, hoisted: the row solver needs them, and the old loop recomputed the
	// multiply on every pixel.
	const int64_t stepX0 = a0 * SubpixelScale;
	const int64_t stepX1 = a1 * SubpixelScale;
	const int64_t stepX2 = a2 * SubpixelScale;

	for (int32_t y = minY; y <= maxY; ++y) {
		// One interval per row, solved rather than scanned. The triangle is convex, so there is
		// never a second run to look for - and the scan used to keep stepping to maxX after the
		// run ended, paying for the whole bounding box however narrow the coverage was.
		auto span = rowSpanAnalytic(w0Row, w1Row, w2Row, stepX0, stepX1, stepX2, minX, maxX);

		if (!span.empty()) {
			const int32_t spanStart = span.lo;
			const auto count = uint32_t(span.hi - span.lo + 1);

			// The edge values in the anchor column, not at the start of the run. Integer addition
			// is exact and associative, so jumping there in one multiply is exact whichever side
			// of minX the anchor lies on - and the anchor is never right of the run start, so the
			// index a kernel adds back stays non-negative.
			const int64_t offset = int64_t(anchorX) - int64_t(minX);
			const int64_t s0 = w0Row + stepX0 * offset;
			const int64_t s1 = w1Row + stepX1 * offset;
			const int64_t s2 = w2Row + stepX2 * offset;

			// Barycentrics in the anchor column and their per-pixel derivative. w = 1 throughout
			// (the 2d pipeline is affine), so this is plain linear interpolation.
			double l0 = double(s0) * invArea;
			double l1 = double(s1) * invArea;
			double l2 = double(s2) * invArea;

			double d0 = double(a0) * SubpixelScale * invArea;
			double d1 = double(a1) * SubpixelScale * invArea;
			double d2 = double(a2) * SubpixelScale * invArea;

			ctx.dst = target.pixels + size_t(y) * size_t(target.stride) + size_t(spanStart) * fmt.size;
			ctx.count = count;
			ctx.originOffset = uint32_t(spanStart - anchorX);

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

			if (spanPixels) {
				*spanPixels += uint64_t(count);
			}

			kernels.writeSpan(ctx, fmt, cmd.blend);
		}

		w0Row += b0 * SubpixelScale;
		w1Row += b1 * SubpixelScale;
		w2Row += b2 * SubpixelScale;
	}
}

URect intersectRects(const URect &a, const URect &b) {
	auto x0 = sprt::max(a.x, b.x);
	auto y0 = sprt::max(a.y, b.y);
	auto x1 = sprt::min(a.x + a.width, b.x + b.width);
	auto y1 = sprt::min(a.y + a.height, b.y + b.height);

	if (x1 <= x0 || y1 <= y0) {
		return URect{0, 0, 0, 0};
	}
	return URect{x0, y0, x1 - x0, y1 - y0};
}

uint32_t draw(const Target &target, const DrawList &list, const URect &clip,
		FillStats *stats) {
	if (target.empty() || list.empty() || clip.width == 0 || clip.height == 0) {
		return 0;
	}

	auto fmt = getChannelLayout(target.format);
	if (fmt.size == 0) {
		log::source().error("raster", "Unsupported target format: ",
				getPixelFormatName(target.format));
		return 0;
	}

	// Resolved once for the whole list: the pixel loops are the same for every command in it, and
	// re-asking per command would put a load and a branch on a path that has neither.
	auto &kernels = getKernels();

	uint32_t drawn = 0;

	// Entries, not commands: glyph blits and triangle batches interleave, and the order between
	// them is the painter's order the queue guarantees.
	for (auto &entry : list.entries) {
		if (entry.type == DrawEntry::Glyph) {
			if (entry.index >= list.glyphs.size()) {
				log::source().error("raster", "Glyph entry index is out of bounds");
				continue;
			}

			auto glyph = list.glyphs[entry.index];
			glyph.scissor = intersectRects(glyph.scissor, clip);
			if (glyph.scissor.width == 0 || glyph.scissor.height == 0) {
				continue;
			}

			if (stats) {
				// The glyph's own box clipped by the scissor - NOT the scissor, which is the whole
				// damage region and would over-count by the glyph count. This has to mirror the
				// kernel's own clip exactly (blitGlyphScalar), or the number measures nothing.
				// Transparent texels inside the box do count: the loop walks them.
				auto left = sprt::max(glyph.x, int32_t(glyph.scissor.x));
				auto top = sprt::max(glyph.y, int32_t(glyph.scissor.y));
				auto right = sprt::min(glyph.x + int32_t(glyph.width),
						int32_t(glyph.scissor.x + glyph.scissor.width));
				auto bottom = sprt::min(glyph.y + int32_t(glyph.height),
						int32_t(glyph.scissor.y + glyph.scissor.height));
				if (left < right && top < bottom) {
					stats->glyphPixels += uint64_t(right - left) * uint64_t(bottom - top);
				}
			}

			kernels.blitGlyph(target, glyph, fmt);
			++drawn;
			continue;
		}

		if (entry.index >= list.commands.size()) {
			log::source().error("raster", "Command entry index is out of bounds");
			continue;
		}

		auto &cmd = list.commands[entry.index];
		if (cmd.indexCount < 3 || cmd.scissor.width == 0 || cmd.scissor.height == 0) {
			continue;
		}

		// The damage rectangle narrows every command on top of its own scissor, which is what lets
		// a frame be rasterized region by region instead of over their bounding box.
		auto scissor = intersectRects(cmd.scissor, clip);
		if (scissor.width == 0 || scissor.height == 0) {
			continue;
		}

		auto last = size_t(cmd.firstIndex) + size_t(cmd.indexCount);
		if (last > list.indexes.size()) {
			log::source().error("raster", "Command index range is out of bounds");
			continue;
		}

		const Texture *texture = nullptr;
		if (cmd.kind != TextureKind::Solid) {
			if (cmd.texture >= list.textures.size()) {
				log::source().error("raster", "Command texture index is out of bounds");
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
				log::source().error("raster", "Vertex index is out of bounds");
				break;
			}

			Setup_drawTriangle(kernels, target, fmt, list.vertexes[i0], list.vertexes[i1],
					list.vertexes[i2], cmd, texture, scissor, stats ? &stats->spanPixels : nullptr);
		}

		++drawn;
	}

	return drawn;
}

} // namespace stappler::raster
