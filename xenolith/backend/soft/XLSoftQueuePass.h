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

#ifndef XENOLITH_BACKEND_SOFT_XLSOFTQUEUEPASS_H_
#define XENOLITH_BACKEND_SOFT_XLSOFTQUEUEPASS_H_

#include "XLSoftDevice.h"
#include "XLSoftPipeline.h"
#include "XLCoreQueuePass.h"
#include "XLCoreAttachment.h"
#include "SPRaster.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft {

class Loop;

// The render pass has no state to hold: it only identifies the pass inside the frame graph, the
// same way the WebGPU one does.
class SP_PUBLIC RenderPass final : public core::RenderPass {
public:
	virtual ~RenderPass() = default;

	bool init(Device &, const core::QueuePassData &);
};

// "Recording" here means building a DrawList. The subpass callback of the 2d renderer appends to
// it, and the pass executes the whole list at submit time, in order - which is exactly the
// painter's order the flat queue promises.
class SP_PUBLIC CommandBuffer final : public core::CommandBuffer {
public:
	virtual ~CommandBuffer() = default;

	bool init(Device &);

	void setTarget(const raster::Target &target) { _target = target; }
	const raster::Target &getTarget() const { return _target; }

	raster::DrawList &getDrawList() { return _drawList; }
	const raster::DrawList &getDrawList() const { return _drawList; }

	// Current clip, already intersected with the target extent. Commands appended by the renderer
	// pick it up when they are emitted.
	void setScissor(const URect &);
	const URect &getScissor() const { return _scissor; }

protected:
	Device *_device = nullptr;
	raster::Target _target;
	raster::DrawList _drawList;
	URect _scissor;
};

class SP_PUBLIC QueuePassHandle : public core::QueuePassHandle {
public:
	virtual ~QueuePassHandle() = default;

	// Scene-space scissor -> target pixels, honouring the surface pre-rotation. Ported from
	// vk::QueuePassHandle: the transform is a property of the presented surface, not of the API,
	// so both backends have to agree on it or clipped content would land in different places.
	static URect rotateScissor(const core::FrameConstraints &constraints, const URect &scissor);

	virtual bool prepare(core::FrameQueue &, Function<void(bool)> &&) override;
	virtual void submit(core::FrameQueue &, Rc<core::FrameSync> &&,
			Function<void(bool)> &&onSubmited, Function<void(bool)> &&onComplete) override;

protected:
	// per-subpass recording hook: the default runs SubpassData::commandsCallback
	virtual void recordSubpass(core::FrameQueue &, const core::SubpassData &, CommandBuffer &);

	// Resolve the pass output into a rasterizer target, apply its load op, then record and
	// execute every subpass. Returns false when the pass has no usable colour output.
	bool runPass(core::FrameQueue &);

	// Regions of the target this frame has to repaint, from the swapchain's damage tracker. Kept
	// as a list rather than collapsed into a bounding box, so that changes far apart do not cost
	// everything between them; the regions are pairwise disjoint, which is what makes it safe to
	// rasterize each one separately.
	//
	// Returns false when the image already holds this frame and it can be skipped outright.
	bool computeRedrawArea(core::FrameQueue &, const raster::Target &, Vector<URect> &areas);

	Device *_device = nullptr;
	Loop *_softLoop = nullptr;
};

class SP_PUBLIC ImageAttachment
	: public core::AttachmentTyped<core::AttachmentHandle, core::ImageAttachment> {
public:
	virtual ~ImageAttachment() = default;
};

} // namespace stappler::xenolith::soft

#endif /* XENOLITH_BACKEND_SOFT_XLSOFTQUEUEPASS_H_ */
