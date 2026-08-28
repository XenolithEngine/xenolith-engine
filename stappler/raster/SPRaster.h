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

#ifndef STAPPLER_RASTER_SPRASTER_H_
#define STAPPLER_RASTER_SPRASTER_H_

#include "SPCommon.h" // IWYU pragma: keep
#include "SPMemory.h" // IWYU pragma: keep
#include "SPLog.h" // IWYU pragma: keep

#include <sprt/runtime/geom/color.h>
#include <sprt/runtime/geom/geom.h>

// CPU rasterizer. This layer takes plain data in and writes pixels out: it knows nothing about a
// graphics API, a window system or a scene graph, and depends on nothing but stappler_core. That
// is what keeps it unit-testable, reusable outside a renderer, and what lets the SIMD kernels be
// swapped in behind a table of function pointers.

namespace STAPPLER_VERSIONIZED stappler::raster {

// The draw list is rebuilt every frame and outlives no pool, so the malloc model is the right one.
using namespace mem_std;

using sprt::geom::Color4F;
using sprt::geom::ComponentMapping;
using sprt::geom::URect;

// Every byte layout the rasterizer can address linearly, and nothing beyond it. Deliberately not
// core::ImageFormat: an enum of exactly the supported cases makes "unsupported format" impossible
// to represent past the boundary, instead of something every kernel has to re-check. The
// translation from whatever the caller's format vocabulary is happens once per target and once
// per texture.
enum class PixelFormat {
	Undefined,
	R8, // glyph coverage, 1x1 constants
	RGBA8888,
	BGRA8888, // the swapchain format on Linux
};

// Blend modes of the flat contract. There are exactly two, and they are not configurable:
// materials pick one through PipelineMaterialInfo.
enum class BlendMode {
	// blending disabled, plain write
	Solid,
	// color = SrcAlpha/OneMinusSrcAlpha (Add), alpha = Zero/One (Add):
	// destination alpha is preserved and nothing is premultiplied
	Transparent,
};

// Bytes per pixel. Zero only for Undefined, which is how a caller detects an unsupported target.
SP_PUBLIC uint32_t getPixelSize(PixelFormat);

SP_PUBLIC StringView getPixelFormatName(PixelFormat);

// Name of the pixel loops actually in use ("scalar", "swar", "avx2", ...). Public so that whoever
// reports a timing can state what it timed - a benchmark that silently measured the fallback
// produces a real number for the wrong thing, and nothing in the picture gives it away.
SP_PUBLIC StringView getActiveKernelSetName();

// Subpixel precision of the edge functions. 8 bits (24.8) is what GPUs use, and matching it is
// what keeps shared edges of adjacent triangles from cracking or double-blending - which the
// vertex-AA fringe the tessellator emits would show immediately.
static constexpr int32_t SubpixelBits = 8;
static constexpr int32_t SubpixelScale = 1 << SubpixelBits;

// Destination bitmap. Rows are tightly packed, `stride` is kept explicit anyway so a sub-image
// (a damage rect, later a tile) can be handed to a kernel without copying.
struct SP_PUBLIC Target {
	uint8_t *pixels = nullptr;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t stride = 0;
	PixelFormat format = PixelFormat::Undefined;

	bool empty() const { return !pixels || width == 0 || height == 0; }
};

// A vertex after the vertex stage: screen space, pixels, Y down (framebuffer convention).
// Attributes are kept as floats here; the kernels convert once per span. `layer` is the third
// texture coordinate, taken from the vertex's pre-transform z exactly as xl_2d_flat.vert does.
struct SP_PUBLIC Vertex {
	float x = 0.0f;
	float y = 0.0f;
	float u = 0.0f;
	float v = 0.0f;
	float layer = 0.0f;
	Color4F color;
};

// Which fragment path a draw takes. Solid is not an optimization of a missing texture: a 1x1
// image samples to the same value everywhere, so its contribution is folded into a per-command
// constant and the fetch disappears. Every plain Layer resolves to that case.
enum class TextureKind {
	Solid,
	Texture2D,
	Texture2DArray,
	Texture3D,
};

enum class Filter {
	Nearest,
	Linear,
};

enum class AddressMode {
	Repeat,
	MirroredRepeat,
	ClampToEdge,
	ClampToBorder,
};

// Resolved from core::SamplerInfo at record time; the flat queue only ever uses three of these.
struct SP_PUBLIC Sampler {
	Filter filter = Filter::Nearest;
	AddressMode addressU = AddressMode::Repeat;
	AddressMode addressV = AddressMode::Repeat;
	AddressMode addressW = AddressMode::Repeat;
};

// A sampled image: the pixels of a soft::Image plus the part of its view a sampler needs. Mip
// levels are absent by construction - the flat queue never requests them.
struct SP_PUBLIC Texture {
	const uint8_t *pixels = nullptr;
	uint32_t width = 1;
	uint32_t height = 1;
	uint32_t depth = 1;
	uint32_t layers = 1;
	uint32_t stride = 0;
	uint32_t layerSize = 0;
	uint32_t baseLayer = 0;
	PixelFormat format = PixelFormat::Undefined;

	// Component swizzle of the image view (ColorMode). Applied after filtering, as the Vulkan
	// spec does - and mandatory, not cosmetic: every predefined material of the flat queue is
	// R8_UNORM under ComponentMapping(R, R, R, One), which without it samples to pure red.
	ComponentMapping swizzle[4] = {ComponentMapping::Identity, ComponentMapping::Identity,
		ComponentMapping::Identity, ComponentMapping::Identity};
};

// One draw: a range of the index array plus the state it must be rasterized with. Commands are
// executed strictly in list order - that is the painter's order the flat queue guarantees.
struct SP_PUBLIC Command {
	uint32_t firstIndex = 0;
	uint32_t indexCount = 0;
	BlendMode blend = BlendMode::Solid;
	// Clip rectangle in target pixels, already intersected with the viewport by the caller.
	URect scissor;

	TextureKind kind = TextureKind::Solid;
	uint32_t texture = 0; // index into DrawList::textures, unused when kind is Solid
	Sampler sampler;
	// the constant the fragment stage multiplies by when kind is Solid
	Color4F constantColor = Color4F::WHITE;
};

// A coverage bitmap copied straight onto the target, one source texel per destination pixel.
//
// This is how text is drawn, and it is the normal path rather than a special case: a Label is
// normalized, so the shared vertex plan reduces its model matrix to identity plus a floored
// translation, and the label's scale has already gone into the font size. The glyph therefore lands
// on integer pixels at 1:1 - there is nothing to interpolate, and since labels sample with the
// nearest filter, this produces exactly what a GPU texture fetch would.
struct SP_PUBLIC GlyphBlit {
	const uint8_t *coverage = nullptr;
	uint32_t pitch = 0;

	// destination rectangle in target pixels; its size is the size of the coverage bitmap
	int32_t x = 0;
	int32_t y = 0;
	uint16_t width = 0;
	uint16_t height = 0;

	Color4F color;
	BlendMode blend = BlendMode::Solid;
	URect scissor;
};

// The two kinds of work a list holds, kept in one sequence because painter's order runs across
// both: a glyph drawn after a rectangle must land on top of it.
struct SP_PUBLIC DrawEntry {
	enum Type {
		Triangles,
		Glyph,
	};

	Type type = Triangles;
	uint32_t index = 0; // into DrawList::commands or DrawList::glyphs
};

struct SP_PUBLIC DrawList {
	Vector<Vertex> vertexes;
	Vector<uint32_t> indexes;
	Vector<Command> commands;
	Vector<Texture> textures;
	Vector<GlyphBlit> glyphs;
	Vector<DrawEntry> entries;

	void addCommand(Command &&cmd) {
		entries.emplace_back(DrawEntry{DrawEntry::Triangles, uint32_t(commands.size())});
		commands.emplace_back(sp::move(cmd));
	}

	void addGlyph(GlyphBlit &&glyph) {
		entries.emplace_back(DrawEntry{DrawEntry::Glyph, uint32_t(glyphs.size())});
		glyphs.emplace_back(sp::move(glyph));
	}

	void clear() {
		vertexes.clear();
		indexes.clear();
		commands.clear();
		textures.clear();
		glyphs.clear();
		entries.clear();
	}

	bool empty() const { return entries.empty(); }
};

// Pixels the kernels actually wrote, as opposed to the area they were handed.
//
// The damage area already reported by a caller answers "how much of the surface was in play"; this
// answers "how much work was done inside it", and the two differ by the overdraw. A frame that
// repaints everything and a frame that repaints a clock differ here by two orders of magnitude,
// which is the only way to tell them apart from outside - the picture is identical either way.
//
// Counted at the three kernel entry points and nowhere else, so the number is writes issued, not
// pixels visibly changed: a pixel covered by two commands counts twice, which is the point.
struct SP_PUBLIC FillStats {
	uint64_t spanPixels = 0; // writeSpan - one horizontal run of a triangle
	uint64_t glyphPixels = 0; // blitGlyph - one glyph coverage box, post-scissor
	uint64_t fillPixels = 0; // fillRect - attachment clears and solid axis-aligned quads

	uint64_t total() const { return spanPixels + glyphPixels + fillPixels; }

	void add(const FillStats &o) {
		spanPixels += o.spanPixels;
		glyphPixels += o.glyphPixels;
		fillPixels += o.fillPixels;
	}
};

// Fill a rectangle with a constant color, ignoring whatever was there. This is the clear op of a
// render pass attachment, and it is also the fast path a solid axis-aligned quad will take later.
SP_PUBLIC void fillRect(const Target &, const URect &, const Color4F &, FillStats * = nullptr);

// The value a 1x1 texture samples to, swizzle applied. Used to build Command::constantColor.
SP_PUBLIC Color4F sampleConstant(const Texture &);

// Intersection of two rectangles; zero-sized when they do not overlap.
SP_PUBLIC URect intersectRects(const URect &, const URect &);

// Execute a command list against a target, confined to `clip`. Returns the number of commands
// actually rasterized - a command whose scissor is empty or whose triangles are degenerate
// contributes nothing.
//
// Partial redraw calls this once per damage rectangle, so that several small changes do not force
// the rasterizer over their bounding box. Those rectangles MUST be pairwise disjoint: a pixel
// visited twice would have every transparent command blended into it twice.
SP_PUBLIC uint32_t draw(const Target &, const DrawList &, const URect &clip,
		FillStats * = nullptr);

// How a region is cut up before it is rasterized, and by how many threads.
//
// Two effects that have nothing to do with each other, kept separately so each can be measured on
// its own: cutting alone makes the working set of one pass small enough to stay in cache - a run
// across the full width of a 1080p surface has evicted its own texture by the time the next row
// starts - while threads divide whatever is left.
struct SP_PUBLIC TilingInfo {
	uint32_t width = 0; // 0: do not cut horizontally
	uint32_t height = 0; // 0: do not cut vertically
	uint32_t threads = 1; // 1: the calling thread alone, no dispatch at all; 0: whatever the
						  // thread pool can supply

	bool tiled() const { return width != 0 || height != 0; }
};

// The tiles of one region: pairwise disjoint, and covering it exactly.
//
// Vertical cuts are placed on multiples of 64 bytes of the target *in absolute columns*, because
// alignment is a property of the address and the address counts from the start of the row, not
// from the start of the region. Two things come of that. Neighbouring tiles never share a cache
// line, which is what makes writing them from two threads cost nothing extra; and where the row
// pitch is itself a multiple of 64 - which 1920x4 is - the loads inside a tile are aligned.
//
// A region rarely starts on such a boundary, so its first tile is short. That is the intent: one
// narrow tile at the edge rather than every tile in the row misaligned.
SP_PUBLIC void makeTileGrid(const URect &region, const TilingInfo &, uint32_t pixelSize,
		const Callback<void(const URect &)> &);

// What a call to drawTiled actually did, as opposed to what it was asked to do.
//
// Reported separately from TilingInfo on purpose. A thread count the pool cannot supply, or a
// region too small to cut, silently turns a measurement of the parallel path into a measurement of
// the serial one - and the picture is identical either way, so nothing gives it away. Whoever
// prints a timing prints these.
struct SP_PUBLIC TilingStats {
	uint32_t tiles = 0; // tiles the regions were cut into
	uint32_t workers = 0; // threads that took part, the calling one included

	// Summed across every tile and every worker. Tiles are disjoint, so this double-counts nothing
	// that the untiled path would have counted once.
	FillStats fill;
};

// Rasterize a whole set of damage regions, cut into tiles, optionally across a thread pool.
//
// The regions must be pairwise disjoint for the same reason a single call to `draw` must not be
// repeated over overlapping rectangles, and the tiles of all regions are gathered into one list
// rather than joined per region: a frame whose regions differ in size would otherwise spend most
// of its time waiting for the largest one alone.
//
// Returns commands rasterized, counted once per tile - a work count, not a command count.
SP_PUBLIC uint32_t drawTiled(const Target &, const DrawList &, SpanView<URect> regions,
		const TilingInfo &, TilingStats * = nullptr);

// The tiling a caller with no opinion of its own should use, resolved once. Overridden entirely by
// SP_RASTER_TILE=WxH|off and SP_RASTER_THREADS=N, which is how the benchmark measures the two
// effects apart and how the parity gate compares tiled against untiled.
//
// 256x256 by default, and every thread the pool will give. The size is where the measurements put
// it: large enough that the per-tile cost of walking the command list again stays small, small
// enough that a tile's texture footprint survives in cache - which is worth 1.6x on a bilinear
// sprite before a second thread is involved. Below 128 the re-walk starts to dominate and the
// solid fill, which has nothing to gain, starts to lose.
//
// Defaulting it on is safe rather than bold: with damage tracking doing its job the regions are
// small, one tile covers them, and nothing changes. It is the full-surface frame - a resize, a
// first paint, a scrolling view - that gets cut up.
SP_PUBLIC const TilingInfo &getDefaultTiling();

} // namespace stappler::raster

#endif /* STAPPLER_RASTER_SPRASTER_H_ */
