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

#include "XLGlesQueuePass.h"
#include "XLGlesObject.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles {

bool RenderPass::init(Device &dev, const core::QueuePassData &) {
	return core::Object::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle, void *) { },
			core::ObjectType::RenderPass, core::ObjectHandle::zero());
}

bool CommandBuffer::init(Device &dev) {
	_device = &dev;
	return true;
}

URect QueuePassHandle::rotateScissor(const core::FrameConstraints &constraints,
		const URect &scissor) {
	// Y flip first: scene space grows upwards, the target downwards.
	int32_t x = int32_t(scissor.x);
	int32_t y = int32_t(constraints.extent.height - scissor.y - scissor.height);
	uint32_t width = scissor.width;
	uint32_t height = scissor.height;

	switch (core::getPureTransform(constraints.transform)) {
	case core::SurfaceTransformFlags::Rotate90:
		y = int32_t(scissor.x);
		x = int32_t(scissor.y);
		sprt::swap(width, height);
		break;
	case core::SurfaceTransformFlags::Rotate180: y = int32_t(scissor.y); break;
	case core::SurfaceTransformFlags::Rotate270:
		y = int32_t(constraints.extent.height - scissor.x - scissor.width);
		x = int32_t(constraints.extent.width - scissor.y - scissor.height);
		sprt::swap(width, height);
		break;
	default: break;
	}

	if (x < 0) {
		width = (uint32_t(-x) < width) ? width - uint32_t(-x) : 0;
		x = 0;
	}

	if (y < 0) {
		height = (uint32_t(-y) < height) ? height - uint32_t(-y) : 0;
		y = 0;
	}

	return URect{uint32_t(x), uint32_t(y), width, height};
}

bool QueuePassHandle::prepare(core::FrameQueue &q, Function<void(bool)> &&cb) {
	_device = static_cast<Device *>(q.getFrame()->getDevice());

	return core::QueuePassHandle::prepare(q, sp::move(cb));
}

// Apply the pass's load ops to its framebuffer. The attachment list and the framebuffer are built
// from the same ordered vector (the frame queue collects the views while walking _data->attachments),
// so position i here is GL_COLOR_ATTACHMENT0 + i there - no lookup needed.
bool QueuePassHandle::runPass(core::FrameQueue &) {
	auto fbo = static_cast<const Framebuffer *>(getFramebuffer());
	if (!fbo || !fbo->isComplete()) {
		log::source().error("gles::QueuePassHandle", "No usable framebuffer for pass: ",
				getName());
		return false;
	}

	for (auto &subpass : _data->subpasses) {
		if (subpass->commandsCallback) {
			log::source().error("gles::QueuePassHandle",
					"Draw commands are not supported in M1: ", subpass->key);
			return false;
		}
	}

	auto &table = _device->getTable();
	if (!table.glBindFramebuffer || !table.glViewport) {
		log::source().error("gles::QueuePassHandle", "GL framebuffer entrypoints are missing");
		return false;
	}

	table.glBindFramebuffer(GL_FRAMEBUFFER, fbo->getGlName());
	auto extent = fbo->getExtent();
	table.glViewport(0, 0, GLsizei(extent.width), GLsizei(extent.height));

	for (size_t i = 0; i < _data->attachments.size(); ++i) {
		const auto *att = _data->attachments[i];
		if (att->loadOp == core::AttachmentLoadOp::Clear) {
			auto imgAtt = static_cast<const core::ImageAttachment *>(
					att->attachment->attachment.get());
			auto color = imgAtt->getClearColor();
			float value[4] = { color.r, color.g, color.b, color.a };
			if (!table.glClearBufferfv) {
				log::source().error("gles::QueuePassHandle", "glClearBufferfv is missing");
				return false;
			}
			table.glClearBufferfv(GL_COLOR, GLint(i), value);
		} else if (att->loadOp == core::AttachmentLoadOp::DontCare && table.glInvalidateFramebuffer) {
			const GLenum attachment = GL_COLOR_ATTACHMENT0 + GLsizei(i);
			table.glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, &attachment);
		}
	}

	table.glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return true;
}

void QueuePassHandle::submit(core::FrameQueue &q, Rc<core::FrameSync> &&sync,
		Function<void(bool)> &&onSubmited, Function<void(bool)> &&onComplete) {
	// Objects dropped on any thread since the last frame wait here for their GL delete; doing it
	// before this pass's work keeps one thread owning every API call.
	if (_device) { _device->drainPendingReleases(); }

	auto success = runPass(q);

	// The frame graph learns about completion through the fence's release callbacks, so a failed
	// pass still has to produce one - otherwise the frame would stall in Submission state with no
	// diagnostics (the base handle fails for exactly this reason).
	_fence = _loop->acquireFence(core::FenceType::Default);
	if (!_fence) {
		onSubmited(false);
		return;
	}

	// Order the pass's completion against its resources with a GL fence created after all of the
	// work above. A host-only fence (the failure path) reads as signalled at once, which is what
	// the callbacks expect when nothing was actually submitted.
	if (success && _device) {
		auto &table = _device->getTable();
		GLsync glSync = nullptr;
		if (!table.glFenceSync || (glSync = table.glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0))
				== nullptr) {
			log::source().error("gles::QueuePassHandle", "Fail to create a GL fence sync");
			success = false;
		} else if (auto fence = _fence.get_cast<Fence>()) {
			fence->setGlSync(glSync); // attaches and arms: unsignalled until glClientWaitSync sees it
		}
	}

	_fence->setTag(getName());
	_fence->addRelease([this, guard = Rc<core::FrameQueue>(&q),
							   onComplete = sp::move(onComplete)](bool fenceSuccess) {
		for (auto &it : _data->completeCallbacks) { it(*guard, *_data, fenceSuccess); }
		onComplete(fenceSuccess);
	}, this, "gles::QueuePassHandle::submit");

	// Nothing armed this fence on a device queue - there is no queue - so arm it by hand. Without
	// this core::Fence::check short-circuits on a non-Armed state and the release callbacks never
	// run (the soft backend needs the same call for the same reason).
	_fence->setArmed();

	for (auto &it : _data->submittedCallbacks) { it(q, *_data, success); }

	onSubmited(success);

	auto fence = move(_fence);
	_fence = nullptr;
	fence->schedule(*_loop);
}

} // namespace stappler::xenolith::gles
