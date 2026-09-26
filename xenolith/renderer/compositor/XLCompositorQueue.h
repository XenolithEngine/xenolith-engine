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

#ifndef XENOLITH_RENDERER_COMPOSITOR_XLCOMPOSITORQUEUE_H_
#define XENOLITH_RENDERER_COMPOSITOR_XLCOMPOSITORQUEUE_H_

#include "XLCompositorBlend.h"
#include "XLCorePlaneSource.h"
#include "XLCoreAttachment.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

class Loop;

}

namespace STAPPLER_VERSIONIZED stappler::xenolith::compositor {

// The input attachment every compositor queue declares, by this name.
static constexpr StringView PlaneSetAttachmentName = "CompositorPlaneSet";

struct SP_PUBLIC PlaneInput {
	// Holding the frame keeps its image out of the window's ring until this frame is done.
	Rc<core::PlaneFrame> frame;
	URect src;
	IRect dst;
	float alpha = 1.0f;
	PlaneBlend blend = PlaneBlend::Opaque;
	uint32_t plane = 0;
};

// One composed frame: the planes bottom to top, over the background.
class SP_PUBLIC PlaneSetInput : public core::AttachmentInputData {
public:
	virtual ~PlaneSetInput() = default;

	Vector<PlaneInput> planes;
	Color4F background;
	uint64_t snapshot = 0;
};

using PlaneSetAttachment = core::AttachmentTyped<core::AttachmentHandle, core::GenericAttachment>;

struct SP_PUBLIC CompositorQueueInfo {
	core::Loop *target = nullptr;
	Extent2 extent;
	Color4F background = Color4F::BLACK;
};

} // namespace stappler::xenolith::compositor

#endif /* XENOLITH_RENDERER_COMPOSITOR_XLCOMPOSITORQUEUE_H_ */
