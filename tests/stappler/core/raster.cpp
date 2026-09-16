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

// Every rasterizer kernel set against the scalar one, byte for byte.
//
// The screenshot gate (tests/parity/compare.sh --kernels) asks the same question of whole frames,
// but it needs a GPU-less GUI app and a working display path, so it cannot run on a
// cross-compiled target under emulation - which is exactly where NEON has to be checked. This
// asks it of the kernels directly, from a plain CLI test that runs anywhere the module builds.
//
// It is also the sharper of the two on edge cases: it sweeps run lengths across the vector width
// so that every set's scalar tail is exercised, and alphas at 0, 1, 254 and 255 so the early-outs
// are too. A frame of a real scene happens to hit those; it does not promise to.

#include "SPCommon.h"

// Deliberately the internal header: the kernel tables are what varies between sets, and comparing
// them is the whole point. The public entry points resolve one set per process and could only ever
// test the one this machine picked.
#include "SPRasterKernel.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace stappler::raster;

struct Bitmap {
	mem_std::Vector<uint8_t> pixels;
	Target target;

	Bitmap(uint32_t width, uint32_t height, PixelFormat format) {
		auto size = getPixelSize(format);
		// One extra row of guard bytes: a kernel that writes past the end of the last row is a
		// real risk with 8- and 32-byte stores, and it would otherwise corrupt nothing visible.
		pixels.resize(size_t(width) * height * size + 64, 0xCD);
		target.pixels = pixels.data();
		target.width = width;
		target.height = height;
		target.stride = width * size;
		target.format = format;
	}

	// A deterministic non-uniform starting state, so a blend has something to blend against.
	void seed() {
		for (size_t i = 0; i < pixels.size(); ++i) { pixels[i] = uint8_t((i * 37 + 11) & 0xFF); }
	}

	bool guardIntact() const {
		for (size_t i = pixels.size() - 64; i < pixels.size(); ++i) {
			if (pixels[i] != uint8_t((i * 37 + 11) & 0xFF)) {
				return false;
			}
		}
		return true;
	}
};

// Run one operation through a table and return the resulting bytes.
template <typename Fn>
mem_std::Vector<uint8_t> run(PixelFormat format, uint32_t width, Fn &&fn) {
	Bitmap bmp(width, 4, format);
	bmp.seed();
	fn(bmp);
	return bmp.pixels;
}

void checkSpans(const KernelTable &subject, const KernelTable &reference) {
	auto name = getKernelSetName(subject.set);

	const PixelFormat formats[] = {PixelFormat::BGRA8888, PixelFormat::RGBA8888, PixelFormat::R8};

	// Lengths that straddle every vector width in the tree (2 for SWAR, 4 for SSE2/NEON, 8 for
	// AVX2), so the tail path of each is entered.
	const uint32_t widths[] = {1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 33};

	// The early-outs live at the ends; 1 and 254 are the cheapest way to prove they are not off
	// by one.
	const float alphas[] = {0.0f, 1.0f / 255.0f, 0.25f, 0.5f, 254.0f / 255.0f, 1.0f};

	bool ok = true;
	bool guarded = true;

	for (auto format : formats) {
		auto fmt = getChannelLayout(format);
		for (auto width : widths) {
			for (auto alpha : alphas) {
				for (auto blend : {BlendMode::Solid, BlendMode::Transparent}) {
					auto make = [&](const KernelTable &table) {
						return run(format, width, [&](Bitmap &bmp) {
							SpanContext ctx;
							ctx.dst = bmp.target.pixels + bmp.target.stride; // second row
							ctx.count = width;
							ctx.r = 0.8f;
							ctx.g = 0.35f;
							ctx.b = 0.1f;
							ctx.a = alpha;
							table.writeSpan(ctx, fmt, blend);
							guarded = guarded && bmp.guardIntact();
						});
					};

					if (make(subject) != make(reference)) {
						ok = false;
					}
				}
			}
		}
	}

	check(ok, toString(name, ": constant spans match scalar"));
	check(guarded, toString(name, ": constant spans stay inside the bitmap"));
}

void checkInterpolated(const KernelTable &subject, const KernelTable &reference) {
	auto name = getKernelSetName(subject.set);
	auto fmt = getChannelLayout(PixelFormat::BGRA8888);

	bool ok = true;
	for (uint32_t width : {1u, 5u, 16u, 33u}) {
		auto make = [&](const KernelTable &table) {
			return run(PixelFormat::BGRA8888, width, [&](Bitmap &bmp) {
				SpanContext ctx;
				ctx.dst = bmp.target.pixels + bmp.target.stride;
				ctx.count = width;
				ctx.r = 0.1f;
				ctx.g = 0.2f;
				ctx.b = 0.9f;
				ctx.a = 0.75f;
				// A colour that changes along the span: no set specializes this, and each has to
				// hand it to the scalar kernel rather than quietly treat it as constant.
				ctx.dr = 0.01f;
				ctx.dg = -0.005f;
				ctx.db = 0.002f;
				ctx.da = -0.001f;
				table.writeSpan(ctx, fmt, BlendMode::Transparent);
			});
		};
		if (make(subject) != make(reference)) {
			ok = false;
		}
	}

	check(ok, toString(name, ": interpolated spans match scalar"));
}

void checkFills(const KernelTable &subject, const KernelTable &reference) {
	auto name = getKernelSetName(subject.set);

	bool ok = true;
	for (auto format : {PixelFormat::BGRA8888, PixelFormat::RGBA8888, PixelFormat::R8}) {
		auto fmt = getChannelLayout(format);
		for (uint32_t width : {1u, 3u, 8u, 9u, 17u}) {
			auto make = [&](const KernelTable &table) {
				return run(format, width + 2, [&](Bitmap &bmp) {
					// Offset by one pixel so the fill does not start where the row does: an
					// aligned-only store would survive a rect that always begins at x = 0.
					table.fillRect(bmp.target, URect{1, 1, width, 2}, fmt,
							Color4F(0.3f, 0.6f, 0.9f, 0.5f));
				});
			};
			if (make(subject) != make(reference)) {
				ok = false;
			}
		}
	}

	check(ok, toString(name, ": fills match scalar"));
}

// Textured spans, and the one place a tolerance applies.
//
// The scalar sampler quantizes through double; a vector one does it in float, because four doubles
// where eight floats fit throws away half the reason to vectorize. The difference is bounded at one
// step in a channel - the same tolerance the textured cases already carry against Vulkan - so that
// is what is asserted here, together with the fact that it never grows to two.
void checkTextured(const KernelTable &subject, const KernelTable &reference) {
	auto name = getKernelSetName(subject.set);
	auto fmt = getChannelLayout(PixelFormat::BGRA8888);

	// 8x8 with every channel varying: a flat texture would quantize identically either way and
	// prove nothing about the arithmetic.
	constexpr uint32_t texExtent = 8;
	uint8_t texels[texExtent * texExtent * 4];
	for (uint32_t y = 0; y < texExtent; ++y) {
		for (uint32_t x = 0; x < texExtent; ++x) {
			auto p = texels + (y * texExtent + x) * 4;
			p[0] = uint8_t((x * 29 + y * 7) & 0xFF);
			p[1] = uint8_t((x * 53 + y * 101) & 0xFF);
			p[2] = uint8_t((x * 149 + y * 17) & 0xFF);
			p[3] = uint8_t(120 + ((x + y) * 13) % 130);
		}
	}

	Texture texture;
	texture.pixels = texels;
	texture.width = texExtent;
	texture.height = texExtent;
	texture.stride = texExtent * 4;
	texture.layerSize = texExtent * texExtent * 4;
	texture.format = PixelFormat::RGBA8888;

	uint32_t worst = 0;
	uint32_t differing = 0;
	uint32_t overTolerance = 0;
	bool engaged = false;

	for (uint32_t width : {1u, 2u, 3u, 4u, 5u, 7u, 8u, 9u, 16u, 17u, 33u, 64u}) {
		for (auto blend : {BlendMode::Solid, BlendMode::Transparent}) {
			// Magnified, 1:1 and minified, so the address arithmetic is exercised at all three
			// scales rather than only where the texel index barely moves.
			for (float scale : {0.25f, 1.0f, 4.0f}) {
				auto build = [&](const KernelTable &table) {
					return run(PixelFormat::BGRA8888, width, [&](Bitmap &bmp) {
						SpanContext ctx;
						ctx.dst = bmp.target.pixels + bmp.target.stride;
						ctx.count = width;
						ctx.r = 0.9f;
						ctx.g = 0.7f;
						ctx.b = 0.45f;
						ctx.a = 0.8f;
						ctx.dr = -0.002f;
						ctx.dg = 0.001f;
						ctx.texture = &texture;
						ctx.kind = TextureKind::Texture2D;
						ctx.sampler.filter = Filter::Nearest;
						ctx.sampler.addressU = AddressMode::Repeat;
						ctx.sampler.addressV = AddressMode::Repeat;
						ctx.u = 0.05f;
						ctx.v = 0.11f;
						ctx.du = scale / float(width * texExtent);
						ctx.dv = ctx.du * 0.5f;
						table.writeSpan(ctx, fmt, blend);
					});
				};

				auto got = build(subject);
				auto want = build(reference);

				if (got != want) {
					engaged = true;
				}

				for (size_t i = 0; i < want.size(); ++i) {
					auto d = uint32_t(got[i] > want[i] ? got[i] - want[i] : want[i] - got[i]);
					if (d != 0) {
						++differing;
						worst = sprt::max(worst, d);
						if (d > 1) {
							++overTolerance;
						}
					}
				}
			}
		}
	}

	// One step is the smallest an 8-bit channel can express and is what the float quantization can
	// cost. Two is a defect, however few pixels carry it.
	check(overTolerance == 0,
			toString(name, ": textured spans within one step of scalar (", differing,
					" bytes differ, worst ", worst, ", ", overTolerance, " over tolerance)"));

	if (!engaged) {
		sprt::cout << "[ .. ] " << name << ": textured spans byte-identical to scalar here\n";
	}
}

// The bilinear span samplers against a naive per-point one.
//
// This is not a kernel-set comparison: every set routes bilinear to the same code today. It is a
// comparison against an implementation that keeps no state between pixels, which is the only way
// to catch a wrong cache key. The first version of the column cache keyed on (x0, y0) and served
// stale rows whenever v moved along the span - a rotated sprite. The screenshot gate caught it;
// this catches it directly, and would have caught it first.
void checkBilinearSpan() {
	constexpr uint32_t texExtent = 8;
	uint8_t texels[texExtent * texExtent * 4];
	for (uint32_t y = 0; y < texExtent; ++y) {
		for (uint32_t x = 0; x < texExtent; ++x) {
			auto p = texels + (y * texExtent + x) * 4;
			p[0] = uint8_t((x * 31 + y * 11) & 0xFF);
			p[1] = uint8_t((x * 67 + y * 137) & 0xFF);
			p[2] = uint8_t((x * 191 + y * 23) & 0xFF);
			p[3] = uint8_t(100 + ((x * 3 + y * 5) % 150));
		}
	}

	Texture texture;
	texture.pixels = texels;
	texture.width = texExtent;
	texture.height = texExtent;
	texture.stride = texExtent * 4;
	texture.layerSize = texExtent * texExtent * 4;
	texture.format = PixelFormat::RGBA8888;

	auto fmt = getChannelLayout(PixelFormat::BGRA8888);

	uint32_t compared = 0;
	uint32_t mismatched = 0;

	// The third of these is the case the cache got wrong: v moving along the span, which is what a
	// rotated sprite produces and an axis-aligned one never does.
	struct Sweep {
		float du, dv;
	};
	const Sweep sweeps[] = {
		{1.0f / 64.0f, 0.0f}, // magnified, v constant
		{1.0f / 8.0f, 0.0f}, // 1:1, v constant
		{1.0f / 64.0f, 1.0f / 96.0f}, // rotated: both coordinates move
		{1.0f / 4.0f, -1.0f / 32.0f}, // minified and rotated
	};

	for (auto &sweep : sweeps) {
		for (uint32_t width : {1u, 5u, 16u, 37u}) {
			SpanContext ctx;
			ctx.count = width;
			ctx.texture = &texture;
			ctx.kind = TextureKind::Texture2D;
			ctx.sampler.filter = Filter::Linear;
			ctx.sampler.addressU = AddressMode::Repeat;
			ctx.sampler.addressV = AddressMode::Repeat;
			ctx.u = 0.13f;
			ctx.v = 0.29f;
			ctx.du = sweep.du;
			ctx.dv = sweep.dv;

			TextureSpan tex;
			if (!resolveTextureSpan(ctx, fmt, tex)) {
				continue;
			}

			// Reference: every point sampled from scratch.
			mem_std::Vector<uint8_t> want;
			for (uint32_t i = 0; i < width; ++i) {
				uint8_t px[4];
				Sample_bilinearNaive(tex, ctx.sampler, ctx.u + ctx.du * float(i),
						ctx.v + ctx.dv * float(i), px);
				want.insert(want.end(), px, px + 4);
			}

			// Subject: the span sampler, through the kernel that owns bilinear.
			Bitmap bmp(width, 4, PixelFormat::BGRA8888);
			bmp.seed();
			auto span = ctx;
			span.dst = bmp.target.pixels + bmp.target.stride;
			// White, opaque and constant, so the shading stage is the identity and any difference
			// is the sampler's.
			span.r = span.g = span.b = span.a = 1.0f;
			getKernels().writeSpan(span, fmt, BlendMode::Solid);

			for (uint32_t i = 0; i < width; ++i) {
				auto got = bmp.target.pixels + bmp.target.stride + i * 4;
				const uint8_t expect[4] = {
					uint8_t(want[i * 4 + 0]),
					uint8_t(want[i * 4 + 1]),
					uint8_t(want[i * 4 + 2]),
					uint8_t(want[i * 4 + 3]),
				};
				++compared;
				// The kernel writes in the target's byte order and quantizes through the shading
				// stage; with a white constant colour that is byte in, byte out.
				if (got[fmt.r] != expect[0] || got[fmt.g] != expect[1] || got[fmt.b] != expect[2]
						|| got[fmt.a] != expect[3]) {
					++mismatched;
				}
			}
		}
	}

	check(compared > 0, "bilinear: the span sampler was actually exercised");
	check(mismatched == 0,
			toString("bilinear: span sampler matches the naive reference (", compared,
					" pixels, ", mismatched, " mismatches)"));
}

void checkGlyphs(const KernelTable &subject, const KernelTable &reference) {
	auto name = getKernelSetName(subject.set);
	auto fmt = getChannelLayout(PixelFormat::BGRA8888);

	uint8_t coverage[3 * 7];
	for (size_t i = 0; i < sizeof(coverage); ++i) { coverage[i] = uint8_t(i * 29); }

	bool ok = true;
	for (auto blend : {BlendMode::Solid, BlendMode::Transparent}) {
		auto make = [&](const KernelTable &table) {
			return run(PixelFormat::BGRA8888, 9, [&](Bitmap &bmp) {
				GlyphBlit glyph;
				glyph.coverage = coverage;
				glyph.pitch = 7;
				glyph.x = 1;
				glyph.y = 1;
				glyph.width = 7;
				glyph.height = 3;
				glyph.color = Color4F(0.9f, 0.4f, 0.2f, 0.8f);
				glyph.blend = blend;
				glyph.scissor = URect{0, 0, bmp.target.width, bmp.target.height};
				table.blitGlyph(bmp.target, glyph, fmt);
			});
		};
		if (make(subject) != make(reference)) {
			ok = false;
		}
	}

	check(ok, toString(name, ": glyph blits match scalar"));
}

// The analytic row solver against the stepping one it replaced.
//
// This is the check the screenshot gate cannot make. An off-by-one in the integer division shows
// up as one column of one triangle in one row - a frame may well not contain such a triangle, and
// if it does, one pixel out of 300 000 is easy to attribute to something else. Here the two forms
// are held against each other on every row of thousands of triangles, including the shapes a
// scene rarely produces: horizontal edges, zero-area, coverage entirely outside the clip.
void checkRowSpans() {
	// Deterministic: a failure has to be reproducible from the test name alone.
	uint64_t seed = 0x2026'04'04ull;
	auto next = [&seed](int64_t lo, int64_t hi) {
		seed = seed * 6'364'136'223'846'793'005ull + 1'442'695'040'888'963'407ull;
		return lo + int64_t((seed >> 33) % uint64_t(hi - lo + 1));
	};

	uint32_t compared = 0;
	uint32_t mismatched = 0;
	uint32_t nonEmpty = 0;

	for (uint32_t i = 0; i < 4'000; ++i) {
		const int32_t minX = int32_t(next(0, 200));
		const int32_t maxX = minX + int32_t(next(0, 300));

		// Ranges chosen around what 24.8 fixed point actually produces, and deliberately including
		// zero steps (horizontal edges) and values that put the whole row outside the triangle.
		int64_t w[3];
		int64_t step[3];
		for (int k = 0; k < 3; ++k) {
			step[k] = (i % 7 == k) ? 0 : next(-4'096, 4'096) * 256;
			w[k] = next(-300'000, 300'000);
		}

		auto stepping = rowSpanStepping(w[0], w[1], w[2], step[0], step[1], step[2], minX, maxX);
		auto analytic = rowSpanAnalytic(w[0], w[1], w[2], step[0], step[1], step[2], minX, maxX);

		++compared;
		if (!stepping.empty()) {
			++nonEmpty;
		}

		if (stepping.empty() != analytic.empty()
				|| (!stepping.empty()
						&& (stepping.lo != analytic.lo || stepping.hi != analytic.hi))) {
			++mismatched;
		}
	}

	check(mismatched == 0,
			toString("setup: analytic row span matches the stepping reference (", compared,
					" rows, ", mismatched, " mismatches)"));

	// A test where every row came out empty would pass without testing anything.
	check(nonEmpty > compared / 10,
			toString("setup: enough rows were actually covered (", nonEmpty, " of ", compared, ")"));
}

// Drawing a region in pieces must produce exactly what drawing it whole produced.
//
// This is the contract tiling rests on, and it is not free by construction: interpolation is
// positional from the start of the run (base + i*step in float32), so cutting a run somewhere
// else changes both the base and the index it is stepped by, and the last bit with them. The
// screenshot gate cannot see this - a damage rectangle happens to contain primitives rather than
// cut them, so the runs it produces are the same ones. A tile cuts every run it crosses.
//
// Unlike the kernel comparisons this one goes through the public `draw`, because the arithmetic
// under test lives in the triangle setup, not in the pixel loops. That means it exercises
// whichever set this process resolved; run it under SP_RASTER_KERNELS to cover the others.
void checkSpanSplit() {
	constexpr uint32_t width = 96;
	constexpr uint32_t height = 24;
	constexpr uint32_t texExtent = 8;

	uint8_t texels[texExtent * texExtent * 4];
	for (uint32_t y = 0; y < texExtent; ++y) {
		for (uint32_t x = 0; x < texExtent; ++x) {
			auto p = texels + (y * texExtent + x) * 4;
			p[0] = uint8_t((x * 41 + y * 13) & 0xFF);
			p[1] = uint8_t((x * 71 + y * 97) & 0xFF);
			p[2] = uint8_t((x * 157 + y * 19) & 0xFF);
			p[3] = uint8_t(110 + ((x * 7 + y * 3) % 140));
		}
	}

	Texture texture;
	texture.pixels = texels;
	texture.width = texExtent;
	texture.height = texExtent;
	texture.stride = texExtent * 4;
	texture.layerSize = texExtent * texExtent * 4;
	texture.format = PixelFormat::RGBA8888;

	uint32_t differing = 0;
	uint32_t worst = 0;
	uint32_t configs = 0;

	for (auto kind : {TextureKind::Solid, TextureKind::Texture2D}) {
		for (auto filter : {Filter::Nearest, Filter::Linear}) {
			if (kind == TextureKind::Solid && filter == Filter::Linear) {
				continue; // the filter is not consulted at all without a texture
			}
			for (auto blend : {BlendMode::Solid, BlendMode::Transparent}) {
				DrawList list;
				list.textures.emplace_back(texture);

				// Fractional coordinates on purpose: a quad snapped to integers produces runs
				// that start where a tile boundary would anyway, and would hide the very thing
				// this checks. The second shape is skewed so its runs start at a different
				// column on every row.
				auto pushTri = [&](float ax, float ay, float bx, float by, float cx, float cy) {
					auto base = uint32_t(list.vertexes.size());
					list.vertexes.emplace_back(
							Vertex{ax, ay, 0.05f, 0.11f, 0.0f, Color4F(0.9f, 0.2f, 0.4f, 0.85f)});
					list.vertexes.emplace_back(
							Vertex{bx, by, 1.7f, 0.31f, 0.0f, Color4F(0.1f, 0.8f, 0.3f, 0.65f)});
					list.vertexes.emplace_back(
							Vertex{cx, cy, 0.63f, 2.3f, 0.0f, Color4F(0.35f, 0.15f, 0.95f, 0.95f)});
					list.indexes.emplace_back(base);
					list.indexes.emplace_back(base + 1);
					list.indexes.emplace_back(base + 2);
				};

				pushTri(1.37f, 0.75f, 94.11f, 2.4f, 3.9f, 22.6f);
				pushTri(93.2f, 21.8f, 5.6f, 20.1f, 90.4f, 1.9f);
				pushTri(20.25f, 4.5f, 76.75f, 6.5f, 48.5f, 19.5f);

				Command cmd;
				cmd.firstIndex = 0;
				cmd.indexCount = uint32_t(list.indexes.size());
				cmd.blend = blend;
				cmd.scissor = URect{0, 0, width, height};
				cmd.kind = kind;
				cmd.texture = 0;
				cmd.sampler.filter = filter;
				cmd.sampler.addressU = AddressMode::Repeat;
				cmd.sampler.addressV = AddressMode::Repeat;
				cmd.constantColor = Color4F(0.8f, 0.6f, 0.2f, 0.9f);
				list.addCommand(sp::move(cmd));

				auto render = [&](uint32_t tw, uint32_t th) {
					Bitmap bmp(width, height, PixelFormat::BGRA8888);
					bmp.seed();
					for (uint32_t y = 0; y < height; y += th) {
						for (uint32_t x = 0; x < width; x += tw) {
							draw(bmp.target, list,
									URect{x, y, sprt::min(tw, width - x),
										sprt::min(th, height - y)});
						}
					}
					return bmp.pixels;
				};

				auto whole = render(width, height);

				// Widths that are not multiples of any vector width, and a one-pixel column,
				// which cuts every run there is.
				for (uint32_t tw : {1u, 3u, 5u, 8u, 13u, 16u, 17u, 32u, 96u}) {
					for (uint32_t th : {1u, 5u, 24u}) {
						if (tw == width && th == height) {
							continue;
						}
						++configs;
						auto tiled = render(tw, th);
						for (size_t i = 0; i < whole.size(); ++i) {
							auto d = uint32_t(whole[i] > tiled[i] ? whole[i] - tiled[i]
																 : tiled[i] - whole[i]);
							if (d != 0) {
								++differing;
								worst = sprt::max(worst, d);
							}
						}
					}
				}
			}
		}
	}

	check(differing == 0,
			toString("setup [", getActiveKernelSetName(), "]: a region drawn in tiles equals the "
					"same region drawn whole (", configs, " tilings, ", differing,
					" bytes differ, worst ", worst, ")"));
}

// The tile grid: every pixel of the region covered exactly once, nothing outside it, and interior
// cuts on cache-line boundaries.
//
// Covering once is not a nicety. A pixel visited twice has every transparent command in the list
// blended into it twice, and a pixel missed is left holding the previous frame - both are silent
// on a static scene and obvious on a moving one. Counting coverage directly is the only check that
// cannot be fooled by a grid that looks right.
void checkTileGrid() {
	bool covered = true;
	bool inside = true;
	bool aligned = true;
	uint32_t grids = 0;

	constexpr uint32_t extent = 200;
	mem_std::Vector<uint8_t> hits;

	for (uint32_t pixelSize : {1u, 4u}) {
		const uint32_t step = sprt::max(1u, 64u / pixelSize);

		// Region origins that are aligned, one short of a boundary, and arbitrary.
		for (uint32_t rx : {0u, 1u, 15u, 16u, 37u}) {
			for (uint32_t rw : {1u, 7u, 64u, 65u, 163u}) {
				for (uint32_t tw : {0u, 1u, 3u, 16u, 17u, 64u, 256u}) {
					for (uint32_t th : {0u, 1u, 5u, 256u}) {
						if (rx + rw > extent) {
							continue;
						}

						const uint32_t ry = 3;
						const uint32_t rh = 11;
						raster::URect region{rx, ry, rw, rh};
						raster::TilingInfo tiling;
						tiling.width = tw;
						tiling.height = th;

						++grids;
						hits.assign(size_t(extent) * extent, 0);

						raster::makeTileGrid(region, tiling, pixelSize,
								[&](const raster::URect &tile) {
							if (tile.x < rx || tile.y < ry || tile.x + tile.width > rx + rw
									|| tile.y + tile.height > ry + rh || tile.width == 0
									|| tile.height == 0) {
								inside = false;
								return;
							}

							// An interior vertical cut has to land on a cache line; the region's
							// own left edge is wherever the caller put it, and the first tile
							// absorbs that.
							if (tile.x != rx && tw >= step && tile.x % step != 0) {
								aligned = false;
							}

							for (uint32_t y = tile.y; y < tile.y + tile.height; ++y) {
								for (uint32_t x = tile.x; x < tile.x + tile.width; ++x) {
									++hits[size_t(y) * extent + x];
								}
							}
						});

						for (uint32_t y = 0; y < extent; ++y) {
							for (uint32_t x = 0; x < extent; ++x) {
								auto want = (x >= rx && x < rx + rw && y >= ry && y < ry + rh) ? 1
																							   : 0;
								if (hits[size_t(y) * extent + x] != want) {
									covered = false;
								}
							}
						}
					}
				}
			}
		}
	}

	check(covered,
			toString("tiles: every pixel of the region is covered exactly once (", grids,
					" grids)"));
	check(inside, "tiles: no tile reaches outside its region, and none is empty");
	check(aligned, "tiles: interior cuts land on 64-byte boundaries");
}

} // namespace

void performRasterTests() {
	auto tables = getAvailableKernels();
	check(!tables.empty(), "raster: at least one kernel set is available");

	const KernelTable *reference = nullptr;
	for (auto &it : tables) {
		if (it->set == KernelSet::Scalar) {
			reference = it;
		}
	}

	// Scalar is the reference every other set is defined against, so its absence is not a missing
	// optimization - it is a missing definition of "correct".
	check(reference != nullptr, "raster: the scalar set is present");
	if (!reference) {
		return;
	}

	// SWAR is portable C++ over a 64-bit register and needs nothing from the CPU, so a build that
	// does not offer it has lost it by accident.
	bool hasSwar = false;
	for (auto &it : tables) {
		if (it->set == KernelSet::Swar) {
			hasSwar = true;
		}
	}
	check(hasSwar, "raster: the SWAR set is present on every architecture");

	sprt::cout << "[ .. ] raster: kernel sets:";
	for (auto &it : tables) { sprt::cout << " " << getKernelSetName(it->set); }
	sprt::cout << "\n";

	// Not kernel comparisons, but the same kind of claim: two implementations that must agree.
	checkRowSpans();
	checkBilinearSpan();
	checkSpanSplit();
	checkTileGrid();

	for (auto &it : tables) {
		if (it == reference) {
			continue;
		}
		checkSpans(*it, *reference);
		checkInterpolated(*it, *reference);
		checkTextured(*it, *reference);
		checkFills(*it, *reference);
		checkGlyphs(*it, *reference);
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
