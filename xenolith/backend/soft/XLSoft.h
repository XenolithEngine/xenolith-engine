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
// Only what the flat queue needs is implemented; depth/stencil, MSAA, compute and sRGB are out
// of scope (see soft-raster-backend-plan.md).

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft {

// The rasterizer is a stappler module; the alias lets the backend and the 2d renderer spell it
// `soft::raster`.
namespace raster = ::stappler::raster;

class Instance;
class Device;
class Loop;

// What the rasterizer is allowed to do on this build. Filled once, at Loop init.
struct SP_PUBLIC BackendFeatures {
	// Number of worker threads the rasterizer may fan tiles out to: the loop's pool plus the
	// submitting thread, which takes part in the work itself.
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

/* The frame budget (XL_SOFT_BUDGET=N): how the wall-clock period between two presents splits
into stages, all on the loop thread:

	wait     previous present -> start of this frame's render half (app update and visit;
	         preStartFrame = false, so nothing overlaps). XL_FRAME_ACCOUNT=1 splits it further.
	vertex   VertexAttachmentHandle::loadVertexes - the vertex plan and vertex/index/transform
	         arrays; proportional to the scene, not to the damage.
	record   recordSubpass - vertex stage, material and texture resolution, glyph runs.
	clear    the attachment load op, inside the damaged regions only.
	raster   drawTiled - the pixel loops, fork and join included (the XL_SOFT_PROFILE span).
	present  Swapchain::present - on a framebuffer window, the copy into the scanout mapping.
	other    the residual, not a stage; large when frames reach present without the pass
	         (e.g. skipped by the damage tracker).

Off unless the variable is set; the check is cheap enough for shipping builds. */
enum class FrameStage : uint32_t {
	Wait,
	Vertex,
	Record,
	Clear,
	Raster,
	Present,
	Count
};

// Whether XL_SOFT_BUDGET named a non-zero interval. Check before taking a clock.
SP_PUBLIC bool isFrameBudgetEnabled();

// Add to a stage of the frame being accounted. Safe from any thread.
SP_PUBLIC void addFrameStageTime(FrameStage, uint64_t micros);

// Open the frame: charges everything since the previous present to `wait`. Called once per frame,
// at the start of the render half.
SP_PUBLIC void openFrameBudget();

// Close the frame: charges the period since the previous close and reports every Nth time.
// Called from present, the only place that sees every frame (skipped frames are never rasterized).
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
