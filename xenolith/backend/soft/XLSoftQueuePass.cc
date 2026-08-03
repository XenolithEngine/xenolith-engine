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

#include "XLSoftQueuePass.h"
#include "XLSoftObject.h"
#include "XLSoftLoop.h"

#include "XLCoreFrameQueue.h"
#include "XLCoreFrameHandle.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft {

bool RenderPass::init(Device &dev, const core::QueuePassData &data) {
	return core::Object::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle, void *) { },
			core::ObjectType::RenderPass, core::ObjectHandle::zero());
}

bool CommandBuffer::init(Device &dev) {
	_device = &dev;
	return true;
}

void CommandBuffer::setScissor(const URect &rect) {
	// Clamp once, here, so no kernel has to defend against a scissor that leaves the target.
	auto left = sprt::min(rect.x, _target.width);
	auto top = sprt::min(rect.y, _target.height);
	auto right = sprt::min(uint32_t(rect.x + rect.width), _target.width);
	auto bottom = sprt::min(uint32_t(rect.y + rect.height), _target.height);

	if (left >= right || top >= bottom) {
		_scissor = URect{0, 0, 0, 0};
		return;
	}

	_scissor = URect{left, top, right - left, bottom - top};
}

void QueuePassHandle::recordSubpass(core::FrameQueue &q, const core::SubpassData &subpass,
		CommandBuffer &buf) {
	if (subpass.commandsCallback) {
		subpass.commandsCallback(q, subpass, buf);
	}
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

bool QueuePassHandle::runPass(core::FrameQueue &q) {
	auto getViewForAttachment =
			[&](const core::AttachmentSubpassData *desc) -> Rc<core::ImageView> {
		auto aIt = _queueData->attachmentMap.find(desc->pass->attachment);
		if (aIt == _queueData->attachmentMap.end() || !aIt->second->image) {
			return nullptr;
		}

		auto imgAttachment =
				static_cast<core::ImageAttachment *>(desc->pass->attachment->attachment.get());
		auto viewInfo = imgAttachment->getImageViewInfo(aIt->second->image->getInfo(), *desc->pass);
		return aIt->second->image->getView(viewInfo);
	};

	for (auto &subpass : _data->subpasses) {
		if (subpass->outputImages.empty()) {
			log::source().error("soft::QueuePassHandle", "Subpass has no colour output: ",
					subpass->key);
			return false;
		}

		// MRT is out of scope: the flat contract writes exactly one colour attachment, and
		// quietly rasterizing into the first of several would be worse than refusing.
		if (subpass->outputImages.size() > 1) {
			log::source().error("soft::QueuePassHandle",
					"Multiple colour outputs are not supported: ", subpass->key);
			return false;
		}

		auto out = subpass->outputImages.front();
		auto view = getViewForAttachment(out);
		if (!view) {
			log::source().error("soft::QueuePassHandle", "No image view for attachment: ",
					out->key);
			return false;
		}

		auto image = view->getImage().get_cast<Image>();
		if (!image) {
			log::source().error("soft::QueuePassHandle", "Attachment is not a software image: ",
					out->key);
			return false;
		}

		auto &info = image->getInfo();

		raster::Target target;
		target.pixels = image->getData();
		target.width = info.extent.width;
		target.height = info.extent.height;
		target.stride = image->getStride();
		target.format = info.format;

		if (target.empty() || getPixelSize(target.format) == 0) {
			log::source().error("soft::QueuePassHandle", "Attachment is not rasterizable: ",
					out->key, " (format ", core::getImageFormatName(target.format), ")");
			return false;
		}

		auto buf = Rc<CommandBuffer>::create(*_device);
		if (!buf) {
			return false;
		}

		buf->setTarget(target);
		buf->setScissor(URect{0, 0, target.width, target.height});

		// Load op. Clear is the only one that touches memory: Load keeps what is already there,
		// which is what makes partial redraw free for this backend later on.
		if (out->pass->loadOp == core::AttachmentLoadOp::Clear) {
			auto imgAttachment =
					static_cast<core::ImageAttachment *>(out->pass->attachment->attachment.get());
			raster::fillRect(target, URect{0, 0, target.width, target.height},
					imgAttachment->getClearColor());
		}

		recordSubpass(q, *subpass, *buf);

		raster::draw(target, buf->getDrawList());
	}

	return true;
}

bool QueuePassHandle::prepare(core::FrameQueue &q, Function<void(bool)> &&cb) {
	_device = static_cast<Device *>(q.getFrame()->getDevice());
	_softLoop = static_cast<Loop *>(q.getFrame()->getLoop());

	return core::QueuePassHandle::prepare(q, sp::move(cb));
}

void QueuePassHandle::submit(core::FrameQueue &q, Rc<core::FrameSync> &&sync,
		Function<void(bool)> &&onSubmited, Function<void(bool)> &&onComplete) {
	// Rasterization is synchronous: by the time the pass returns, the pixels are written. The
	// fence is acquired anyway because the frame graph drives completion through it.
	auto success = runPass(q);

	_fence = _loop->acquireFence(core::FenceType::Default);
	if (!_fence) {
		onSubmited(false);
		return;
	}

	_fence->setTag(getName());
	_fence->addRelease([this, guard = Rc<core::FrameQueue>(&q),
							   onComplete = sp::move(onComplete)](bool fenceSuccess) {
		for (auto &it : _data->completeCallbacks) { it(*guard, *_data, fenceSuccess); }
		onComplete(fenceSuccess);
	}, this, "soft::QueuePassHandle::submit");

	// Nothing armed this fence on a device queue - there is no queue - so arm it by hand. Without
	// this core::Fence::check short-circuits on a non-Armed state and the release callbacks (which
	// is how the frame graph learns the pass completed) never run.
	_fence->setArmed();

	for (auto &it : _data->submittedCallbacks) { it(q, *_data, success); }

	onSubmited(success);

	auto fence = move(_fence);
	_fence = nullptr;
	fence->schedule(*_loop);
}

} // namespace stappler::xenolith::soft
