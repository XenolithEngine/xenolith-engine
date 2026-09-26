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


#ifndef XENOLITH_RENDERER_COMPOSITOR_BACKEND_VK_XLCOMPOSITORVKPASS_H_
#define XENOLITH_RENDERER_COMPOSITOR_BACKEND_VK_XLCOMPOSITORVKPASS_H_

#include "XLCompositorQueue.h"
#include "XLCompositorPipe.h"

#if MODULE_XENOLITH_RENDERER_COMPOSITOR_VK

#include "XLVkQueuePass.h"
#include "XLVkAttachment.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::compositor::vk {

namespace xvk = stappler::xenolith::vk;

// The images of a frame's planes as a CombinedImageSampler array. Every element is written each
// frame, the spares with the queue's empty image; nothing is reported for barriers, since the
// planes' layouts are the windows' own business.
class SP_PUBLIC PlaneSetHandle : public xvk::ImageAttachmentHandle {
public:
	virtual ~PlaneSetHandle() = default;

	virtual bool writeDescriptor(const core::QueuePassHandle &,
			core::DescriptorImageInfo &) override;

	virtual uint32_t enumerateDirtyDescriptors(const PassHandle &, const core::PipelineDescriptor &,
			const core::DescriptorBinding &, const Callback<void(uint32_t)> &) const override;

	virtual void enumerateAttachmentObjects(
			const Callback<void(core::Object *, const core::SubresourceRangeInfo &)> &) override { }

	void setImages(Vector<Rc<core::ImageView>> &&, Rc<core::ImageView> &&empty,
			Rc<core::Sampler> &&);

protected:
	Vector<Rc<core::ImageView>> _views;
	Rc<core::ImageView> _empty;
	Rc<core::Sampler> _sampler;
};

class SP_PUBLIC PlaneSetVkAttachment
: public core::AttachmentTyped<PlaneSetHandle, core::GenericAttachment> {
public:
	virtual ~PlaneSetVkAttachment() = default;
};

// Draws the frame: the background, then a quad per plane, bottom to top.
class SP_PUBLIC CompositorPassHandle : public xvk::QueuePassHandle {
public:
	virtual ~CompositorPassHandle() = default;

	// Waits for a shared lock on every plane's frame (a copy of one may be in flight) before the
	// descriptors and commands are prepared.
	virtual bool prepare(core::FrameQueue &, Function<void(bool)> &&) override;

protected:
	struct Plane {
		Rc<core::PlaneFrame> frame;
		Rc<core::ImageView> view;
		URect dst; // clipped to the output
		UVec2 srcOrigin;
		float alpha = 1.0f;
		PlaneBlend blend = PlaneBlend::Opaque;
	};

	virtual Vector<const core::CommandBuffer *> doPrepareCommands(core::FrameHandle &) override;
	virtual void doComplete(core::FrameQueue &, Function<void(bool)> &&, bool) override;

	bool prepareLocked(core::FrameQueue &);
	void collectPlanes(const PlaneSetInput &, const Extent2 &output);

	Vector<Plane> _planes;
	Vector<Rc<core::PlaneFrameLock>> _locks;
	Function<void(bool)> _onLocked;
	Color4F _background = Color4F::BLACK;
	Extent2 _extent;
	uint32_t _pendingLocks = 0;
	bool _lockDeferred = false;
};

class SP_PUBLIC CompositorPass : public core::QueuePassTyped<CompositorPassHandle, xvk::QueuePass> {
public:
	static constexpr uint32_t MaxPlanes = 8;

	static bool makeRenderQueue(core::Queue::Builder &, const CompositorQueueInfo &);
	static PlaneCaps getCaps();

	virtual ~CompositorPass() = default;

	virtual bool init(core::Queue::Builder &, core::QueuePassBuilder &,
			const CompositorQueueInfo &);

	const core::AttachmentData *getOutput() const { return _output; }
	const core::AttachmentData *getPlaneSet() const { return _planeSet; }

	// Nearest, clamped: the planes are copied 1:1. Made on the first frame, on the loop thread.
	const Rc<core::Sampler> &acquireSampler(xvk::Device &);

protected:
	using xvk::QueuePass::init;

	const core::AttachmentData *_output = nullptr;
	const core::AttachmentData *_planeSet = nullptr;
	Rc<core::Sampler> _sampler;
};

} // namespace stappler::xenolith::compositor::vk

#endif

#endif /* XENOLITH_RENDERER_COMPOSITOR_BACKEND_VK_XLCOMPOSITORVKPASS_H_ */
