/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/

#ifndef TESTS_SPRITE_SRC_SPRITERASTER_H_
#define TESTS_SPRITE_SRC_SPRITERASTER_H_

#include "SPCommon.h"
#include "SPMemory.h"

#include "XL2dCommandList.h"

#include <sprt/runtime/geom/geom.h>

namespace STAPPLER_VERSIONIZED stappler::spritebench {

using sprt::geom::Extent2;

/* The frame one draw reads back, RGBA8, top-left origin. */
struct RasterImage {
	uint32_t width = 0;
	uint32_t height = 0;
	mem_std::Vector<uint8_t> data;

	bool empty() const { return data.empty(); }

	// The red channel at a pixel - geometry is drawn white on black, so red is coverage.
	uint8_t at(uint32_t x, uint32_t y) const {
		return data[(size_t(y) * width + x) * 4];
	}

	bool sameBytes(const RasterImage &other) const {
		return width == other.width && height == other.height && data == other.data;
	}
};

/* Brings the engine up the way tests/tess does: a software instance, one flat queue of the asked
extent, no window and no device. What it rasterizes is what the engine rasterizes for a real
sprite, on hardware-independent bytes. */
bool rasterInit(Extent2 extent);
void rasterFinalize();

/* One frame. `build` places the draws - pushVertexArray of whatever meshes the check built, in
the order a scene would push them, through the projection the bench owns and the queue's own
transparent solid material. An empty image is a failure and is reported to the log. */
RasterImage rasterDraw(const Callback<void(const xenolith::Mat4 &, xenolith::core::MaterialId,
		xenolith::basic2d::CommandList &)> &build);

} // namespace stappler::spritebench

#endif /* TESTS_SPRITE_SRC_SPRITERASTER_H_ */
