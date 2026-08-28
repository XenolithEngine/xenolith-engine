/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/

#ifndef TESTS_TESS_SRC_TESSRASTER_H_
#define TESTS_TESS_SRC_TESSRASTER_H_

#include "SPCommon.h"
#include "SPTess.h"

#include "SPMemory.h"

#include <sprt/runtime/geom/geom.h>

namespace STAPPLER_VERSIONIZED stappler::tessbench {

using sprt::geom::Extent2;

/* RASTERIZING what the tesselator produced, through the engine's own flat queue - and through
nothing else.

WHY THE FLAT QUEUE AND NOT A RASTERIZER OF OUR OWN. The digest in TessBench.h says whether the
SHAPE changed; it cannot say whether the picture did. Two meshes can cover the same area and the
same box and still put a triangle in the wrong place, and the antialias fringe - which is half of
what the tesselator does - is a per-vertex alpha that no area sum can see at all. A raster does see
both, and a raster produced by the engine's own pass is the only one that answers the question
actually being asked: what will a user see.

WHY NO WINDOW. The engine's headless presentation still takes a `PresentationWindow` - "headless"
there means no OS window, not no window at all - so it is the wrong door. `core::Loop::runRenderQueue`
is the right one: it submits a frame with no swapchain and no presentation, which is exactly what an
offscreen render is. So this file owns a Looper, a software Instance, a Loop and one compiled queue,
and drives frames through them directly. No Context, no AppThread, no Director, no Scene.

The software backend rather than Vulkan on purpose: a golden raster has to be reproducible, and a
GPU's rasterization rules are the vendor's business. The CPU path is the engine's own and gives the
same bytes on every machine that runs these tests. */

struct RasterImage {
	uint32_t width = 0;
	uint32_t height = 0;
	mem_std::Vector<uint8_t> data; // RGBA8, row-major, top-left origin

	bool empty() const { return data.empty(); }

	// Fraction of pixels differing by more than `tolerance` in any channel, and the largest
	// per-channel difference seen. Both are needed: a hundred pixels off by one is antialiasing
	// noise, one pixel off by two hundred is a triangle in the wrong place.
	struct Diff {
		uint32_t pixels = 0;
		uint32_t maxDelta = 0;
		bool sizeMismatch = false;
	};

	Diff compare(const RasterImage &, uint32_t tolerance) const;

	/* What a golden holds about a raster, in three numbers.

	NOT the pixels. Four thousand icons in two variants is eight thousand images, and a golden that
	large is one nobody re-reads and nobody reviews - it becomes a rubber stamp. These three say
	the same thing about a change that matters:

	  `nonZero`   how many pixels the icon covers at all - moves when geometry appears or vanishes
	  `coverage`  the sum of all intensities - moves when the antialias fringe changes, and it is
	              the number that makes the aa variant worth rendering
	  `signature` an 8x8 bitmap of the icon, one bit per cell against its own mean: a perceptual
	              hash. Insensitive to a pixel of fringe, and it flips the moment a shape moves.

	Together they separate the two cases a reviewer needs separated: the picture is the same and
	the numbers drifted (fringe), or the picture moved (signature). */
	struct Digest {
		uint32_t nonZero = 0;
		uint64_t coverage = 0;
		uint64_t signature = 0;

		mem_std::String encode() const;
		static bool decode(StringView, Digest &);

		// Bits of the signature that differ. Zero is "the same picture".
		uint32_t distance(const Digest &) const;
	};

	Digest digest() const;

	bool writePng(StringView path) const;
	static bool readPng(StringView path, RasterImage &);
};

// Brings up the looper, the software instance, the loop and the flat queue. Idempotent; returns
// false if the software backend is not available in this build.
SP_PUBLIC bool rasterInit(Extent2 extent);

// Renders one icon through the flat queue. Returns an empty image if the icon does not tesselate.
SP_PUBLIC RasterImage rasterIcon(StringView name, bool antialias);

SP_PUBLIC void rasterFinalize();

} // namespace stappler::tessbench

#endif /* TESTS_TESS_SRC_TESSRASTER_H_ */
