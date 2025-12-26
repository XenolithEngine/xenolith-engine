/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "XLVkAttachment.h"
#include "XLCoreEnum.h"
#include "XLVkDevice.h"
#include "XLVkTextureSet.h"
#include "XLCoreFrameQueue.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

bool BufferAttachmentHandle::writeDescriptor(const core::QueuePassHandle &,
		core::DescriptorBufferInfo &info) {
	if (info.index < _buffers.size()) {
		auto &v = _buffers[info.index];
		info.buffer = v.buffer;
		info.offset = v.offset;
		info.range = v.size;
		return true;
	}
	return false;
}

uint32_t BufferAttachmentHandle::enumerateDirtyDescriptors(const PassHandle &,
		const PipelineDescriptor &, const core::DescriptorBinding &binding,
		const Callback<void(uint32_t)> &cb) const {
	uint32_t ret = 0;
	uint32_t idx = 0;
	for (auto &it : _buffers) {
		if (it.dirty || it.buffer != binding.get(idx).data
				|| it.buffer->getObjectData().handle != binding.get(idx).object) {
			cb(idx);
			++ret;
		}
		++idx;
	}
	return ret;
}

void BufferAttachmentHandle::enumerateAttachmentObjects(
		const Callback<void(core::Object *, const core::SubresourceRangeInfo &)> &cb) {
	for (auto &it : _buffers) {
		cb(it.buffer, core::SubresourceRangeInfo(core::ObjectType::Buffer, it.offset, it.size));
	}
}

void BufferAttachmentHandle::clearBufferViews() { _buffers.clear(); }

void BufferAttachmentHandle::addBufferView(Buffer *buffer, VkDeviceSize offset, VkDeviceSize size,
		bool dirty) {
	addBufferView(Rc<Buffer>(buffer), offset, size, dirty);
}

void BufferAttachmentHandle::addBufferView(Rc<Buffer> &&buffer, VkDeviceSize offset,
		VkDeviceSize size, bool dirty) {
	auto s = buffer->getSize();
	_buffers.emplace_back(
			BufferView{move(buffer), offset, min(VkDeviceSize(s - offset), size), dirty});
}

auto BufferAttachment::makeFrameHandle(const FrameQueue &queue) -> Rc<AttachmentHandle> {
	if (_frameHandleCallback) {
		auto ret = _frameHandleCallback(*this, queue);
		if (isStatic()) {
			if (auto b = dynamic_cast<BufferAttachmentHandle *>(ret.get())) {
				auto statics = getStaticBuffers();
				for (auto &it : statics) { b->addBufferView(static_cast<Buffer *>(it)); }
			}
		}
		return ret;
	} else {
		auto ret = Rc<BufferAttachmentHandle>::create(this, queue);
		if (isStatic()) {
			auto statics = getStaticBuffers();
			for (auto &it : statics) { ret->addBufferView(static_cast<Buffer *>(it)); }
		}
		return ret;
	}
}

auto ImageAttachment::makeFrameHandle(const FrameQueue &queue) -> Rc<AttachmentHandle> {
	if (_frameHandleCallback) {
		return _frameHandleCallback(*this, queue);
	} else {
		return Rc<ImageAttachmentHandle>::create(this, queue);
	}
}

core::ImageStorage *ImageAttachmentHandle::getImage() const { return _queueData->image; }

bool ImageAttachmentHandle::writeDescriptor(const core::QueuePassHandle &queue,
		core::DescriptorImageInfo &info) {
	auto image = _queueData->image;
	if (!image) {
		return false;
	}

	bool allowSwizzle = (info.descriptor->type == core::DescriptorType::SampledImage);
	ImageViewInfo viewInfo(image->getInfo());
	viewInfo.setup(info.descriptor->attachment->colorMode, allowSwizzle);
	if (auto view = image->getView(viewInfo)) {
		info.layout = info.descriptor->layout;
		info.imageView = static_cast<ImageView *>(view.get());
		return true;
	}

	return false;
}

uint32_t ImageAttachmentHandle::enumerateDirtyDescriptors(const PassHandle &pass,
		const PipelineDescriptor &d, const core::DescriptorBinding &binding,
		const Callback<void(uint32_t)> &cb) const {
	if (getImage()) {
		cb(0);
		return 1;
	}
	return 0;
}

void ImageAttachmentHandle::enumerateAttachmentObjects(
		const Callback<void(core::Object *, const core::SubresourceRangeInfo &)> &cb) {
	auto img = getImage();
	cb(img->getImage(),
			core::SubresourceRangeInfo(core::ObjectType::Image, img->getImage()->getAspects()));
}

} // namespace stappler::xenolith::vk
