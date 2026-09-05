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

#include "XL2dGlesClearPass.h"

#if MODULE_XENOLITH_RENDERER_BASIC2D_GLES

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::gles {

bool ClearPass::makeRenderQueue(Queue::Builder &builder, RenderQueueInfo &info) {
	using namespace core;

	// Partial redraw and frame skipping both rest on preserving the image between frames, which
	// this queue does not do - so damage tracking is forced off, whatever a caller asked for.
	builder.setDamageFlags(QueueDamageFlags::None);
	builder.setApi(InstanceApi::GLES);
	builder.setTypeTag(toInt(QueueType::Flat));

	builder.addPass("GlesClearPass", PassType::Graphics, RenderOrderingHighest,
			[&](QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		return Rc<ClearPass>::create(builder, passBuilder, info);
	});

	return true;
}

bool ClearPass::init(Queue::Builder &queueBuilder, QueuePassBuilder &passBuilder,
		const RenderQueueInfo &info) {
	using namespace core;

	// The one attachment: the presented image. PresentSrc is what Queue::getPresentImageOutput
	// looks for when it resolves a frame's output, and clearOnLoad is what turns this queue into
	// "the background colour" - the compiler sets loadOp = Clear from it (Queue_buildLoadStore).
	_output =
			queueBuilder.addAttachemnt("Output", [&](AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsOutput();

		return Rc<glesb::ImageAttachment>::create(builder,
				ImageInfo(info.extent, core::ForceImageUsage(core::ImageUsage::ColorAttachment),
						info.target->getCommonFormat()),
				core::ImageAttachment::AttachmentInfo{
					.initialLayout = AttachmentLayout::Undefined,
					.finalLayout = AttachmentLayout::PresentSrc,
					.clearOnLoad = true,
					.clearColor = info.backgroundColor,
				});
	});

	auto colorAttachment = passBuilder.addAttachment(_output);

	passBuilder.addSubpass([&](SubpassBuilder &subpassBuilder) {
		subpassBuilder.addColor(colorAttachment,
				AttachmentDependencyInfo{
					PipelineStage::ColorAttachmentOutput,
					AccessType::ColorAttachmentWrite,
					PipelineStage::ColorAttachmentOutput,
					AccessType::ColorAttachmentWrite,
					FrameRenderPassState::Submitted,
				},
				AttachmentLayout::ColorAttachmentOptimal);
	});

	return core::QueuePass::init(passBuilder);
}

Rc<core::QueuePassHandle> ClearPass::makeFrameHandle(const FrameQueue &handle) {
	return Rc<ClearPassHandle>::create(*this, handle);
}

} // namespace stappler::xenolith::basic2d::gles

#endif
