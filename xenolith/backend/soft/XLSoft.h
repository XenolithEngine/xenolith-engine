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

#ifndef XENOLITH_BACKEND_SOFT_XLSOFT_H_
#define XENOLITH_BACKEND_SOFT_XLSOFT_H_

#include "XLCore.h" // IWYU pragma: keep
#include "XLCoreInfo.h" // IWYU pragma: keep
#include "SPRaster.h" // IWYU pragma: keep

// The software backend executes the flat 2d render queue on the CPU: there is no driver, no
// shader compiler and no device memory - "device memory" is malloc'd, an image is a linear
// bitmap and a pipeline is a key that selects a C++ kernel.
//
// Only what the flat queue needs is implemented; see soft-raster-backend-plan.md for the
// contract and for what is deliberately out of scope (depth/stencil, MSAA, compute, sRGB).

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft {

// The rasterizer is a stappler module now (it depends on nothing but stappler_core and is useful
// outside a renderer). The alias keeps the backend and the 2d renderer spelling it `soft::raster`,
// which is where it reads best from their side.
namespace raster = ::stappler::raster;

class Instance;
class Device;
class Loop;

// What the rasterizer is allowed to do on this build. Filled once, at Loop init.
struct SP_PUBLIC BackendFeatures {
	// Number of worker threads the rasterizer may fan tiles out to. M0 rasterizes inline on the
	// loop thread, so this is informational until tiling lands.
	uint32_t threadCount = 1;
};

// The blend state is the rasterizer's, not the backend's; the backend only picks one.
using raster::BlendMode;

// core::ImageFormat -> what the rasterizer can address. This is the boundary translation, and it
// belongs here: it happens once per image and once per texture, never per pixel.
inline raster::PixelFormat getRasterFormat(core::ImageFormat format) {
	switch (format) {
	case core::ImageFormat::R8_UNORM: return raster::PixelFormat::R8;
	case core::ImageFormat::R8G8B8A8_UNORM: return raster::PixelFormat::RGBA8888;
	case core::ImageFormat::B8G8R8A8_UNORM: return raster::PixelFormat::BGRA8888;
	default: return raster::PixelFormat::Undefined;
	}
}

// Bytes per pixel for a format the backend accepts. Returns 0 for anything it can not address
// linearly, which is how callers detect an unsupported image.
inline uint32_t getPixelSize(core::ImageFormat format) {
	return raster::getPixelSize(getRasterFormat(format));
}

/* ---- the frame budget (XL_SOFT_BUDGET=N) --------------------------------------------------------

XL_SOFT_PROFILE answers "how fast did the rasterizer run". This answers the question one level up:
of the wall-clock period between two presents, how much went where.

Everything the software backend does to a frame happens on the loop thread, in a fixed order, so
the frame really is a sum of named stages plus whatever is left:

	wait     the gap between the previous present and the start of this frame's render half.
	         The software presentation engine sets preStartFrame = false, so nothing overlaps
	         here: this is the app thread's update and scene visit, plus whatever the frame graph
	         spends getting from one to the other. It is a stage of the frame like any other, and
	         on a scene-heavy build it is the largest one.
	vertex   VertexAttachmentHandle::loadVertexes - the vertex plan and the vertex/index/transform
	         arrays. Proportional to the scene, not to the damage: a frame that repaints twelve
	         percent of the surface still walks every command.
	record   recordSubpass - the vertex stage (per vertex), material and texture resolution, glyph
	         run emission; produces the draw list.
	clear    the attachment load op, inside the damaged regions only.
	raster   drawTiled - the pixel loops, fork and join included. The same span XL_SOFT_PROFILE
	         times, reported here so the two can be read against each other.
	present  Swapchain::present - on a framebuffer window, the copy into the scanout mapping and
	         the cache maintenance that publishes it.
	other    period minus all of the above. NOT a stage: it is the residual, and with `wait` in
	         place it should be small. A large `other` means a frame reached present without
	         passing through the stages - the damage tracker skipping the pass is the ordinary
	         cause - and the report is then describing frames it did not measure.

`wait` is where the app thread's half lands, but it does not say what that half spent it on. When
`wait` dominates, this instrument has said all it can and the next question needs XL_FRAME_ACCOUNT=1
(the visit, and deferred work against waiting for it); see docs/agents/measuring-frames.md.

Off unless the variable is set, and the check is one relaxed load in five places per frame - cheap
enough to leave in a shipping build, which matters because the boards this backend runs on are not
the boards a profiler runs on. */
enum class FrameStage : uint32_t {
	Wait,
	Vertex,
	Record,
	Clear,
	Raster,
	Present,
	Count
};

// Whether XL_SOFT_BUDGET named a non-zero interval. Read this before taking a clock: the point of
// the guard is that a build with the instrument compiled in still pays nothing for it.
SP_PUBLIC bool isFrameBudgetEnabled();

// Add to a stage of the frame being accounted. Safe from any thread.
SP_PUBLIC void addFrameStageTime(FrameStage, uint64_t micros);

// Open the frame: charges everything since the previous present to `wait`. Called once per frame,
// at the first thing the render half does, so that what it measures is the gap and nothing else.
SP_PUBLIC void openFrameBudget();

// Close the frame: charges the period since the previous close and reports every Nth time.
// Called from present, because present is the last thing that happens to a frame and the only
// place that sees every frame - a frame the damage tracker skipped never reaches runPass.
SP_PUBLIC void closeFrameBudget();

// Times its scope into one stage. Does nothing, not even a clock read, when the budget is off.
class SP_PUBLIC FrameStageTimer {
public:
	explicit FrameStageTimer(FrameStage stage)
	: _stage(stage), _enabled(isFrameBudgetEnabled()) {
		if (_enabled) {
			_started = Time::now();
		}
	}

	~FrameStageTimer() { stop(); }

	// Explicit end, for a scope that outlives the region of interest.
	void stop() {
		if (_enabled) {
			_enabled = false;
			addFrameStageTime(_stage, (Time::now() - _started).toMicros());
		}
	}

	FrameStageTimer(const FrameStageTimer &) = delete;
	FrameStageTimer &operator=(const FrameStageTimer &) = delete;

private:
	FrameStage _stage;
	bool _enabled;
	Time _started;
};

} // namespace stappler::xenolith::soft

#endif /* XENOLITH_BACKEND_SOFT_XLSOFT_H_ */
