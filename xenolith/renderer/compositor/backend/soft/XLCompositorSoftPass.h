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

#ifndef XENOLITH_RENDERER_COMPOSITOR_BACKEND_SOFT_XLCOMPOSITORSOFTPASS_H_
#define XENOLITH_RENDERER_COMPOSITOR_BACKEND_SOFT_XLCOMPOSITORSOFTPASS_H_

#include "XLCompositorQueue.h"

#if MODULE_XENOLITH_RENDERER_COMPOSITOR_SOFT

#include "XLSoftQueuePass.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::compositor::soft {

namespace sf = stappler::xenolith::soft;

// Blends the planes of a PlaneSetInput into the output image, in row bands on the thread pool.
class SP_PUBLIC CompositorPassHandle : public sf::QueuePassHandle {
public:
	virtual ~CompositorPassHandle() = default;

	virtual bool prepare(core::FrameQueue &, Function<void(bool)> &&) override;
	virtual void submit(core::FrameQueue &, Rc<core::FrameSync> &&,
			Function<void(bool)> &&onSubmited, Function<void(bool)> &&onComplete) override;

protected:
	const core::AttachmentHandle *_planeSet = nullptr;
};

// One graphics pass with no programs and no pipelines: the output image and the plane set.
class SP_PUBLIC CompositorPass : public core::QueuePassTyped<CompositorPassHandle> {
public:
	static bool makeRenderQueue(core::Queue::Builder &, const CompositorQueueInfo &);

	virtual ~CompositorPass() = default;

	virtual bool init(core::Queue::Builder &, core::QueuePassBuilder &,
			const CompositorQueueInfo &);

	const core::AttachmentData *getOutput() const { return _output; }
	const core::AttachmentData *getPlaneSet() const { return _planeSet; }

protected:
	using core::QueuePass::init;

	const core::AttachmentData *_output = nullptr;
	const core::AttachmentData *_planeSet = nullptr;
};

} // namespace stappler::xenolith::compositor::soft

#endif

#endif /* XENOLITH_RENDERER_COMPOSITOR_BACKEND_SOFT_XLCOMPOSITORSOFTPASS_H_ */
