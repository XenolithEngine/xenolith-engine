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

#ifndef XENOLITH_BACKEND_SOFT_RASTER_XLSOFTRASTER_H_
#define XENOLITH_BACKEND_SOFT_RASTER_XLSOFTRASTER_H_

#include "XLSoft.h"

// Rasterizer core. This layer knows nothing about core:: objects or about the 2d renderer: it
// takes plain data in and writes pixels out. That is what keeps it unit-testable and what will
// let the SIMD kernels be swapped in behind a table of function pointers.

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft::raster {

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
	core::ImageFormat format = core::ImageFormat::Undefined;

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
	core::ImageFormat format = core::ImageFormat::Undefined;

	// Component swizzle of the image view (ColorMode). Applied after filtering, as the Vulkan
	// spec does - and mandatory, not cosmetic: every predefined material of the flat queue is
	// R8_UNORM under ComponentMapping(R, R, R, One), which without it samples to pure red.
	core::ComponentMapping swizzle[4] = {core::ComponentMapping::Identity,
		core::ComponentMapping::Identity, core::ComponentMapping::Identity,
		core::ComponentMapping::Identity};
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

struct SP_PUBLIC DrawList {
	Vector<Vertex> vertexes;
	Vector<uint32_t> indexes;
	Vector<Command> commands;
	Vector<Texture> textures;

	void clear() {
		vertexes.clear();
		indexes.clear();
		commands.clear();
		textures.clear();
	}

	bool empty() const { return commands.empty(); }
};

// Fill a rectangle with a constant color, ignoring whatever was there. This is the clear op of a
// render pass attachment, and it is also the fast path a solid axis-aligned quad will take later.
SP_PUBLIC void fillRect(const Target &, const URect &, const Color4F &);

// The value a 1x1 texture samples to, swizzle applied. Used to build Command::constantColor.
SP_PUBLIC Color4F sampleConstant(const Texture &);

// Execute a command list against a target. Returns the number of commands actually rasterized -
// a command whose scissor is empty or whose triangles are degenerate contributes nothing.
SP_PUBLIC uint32_t draw(const Target &, const DrawList &);

} // namespace stappler::xenolith::soft::raster

#endif /* XENOLITH_BACKEND_SOFT_RASTER_XLSOFTRASTER_H_ */
