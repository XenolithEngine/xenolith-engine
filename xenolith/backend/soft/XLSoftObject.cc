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

#include "XLSoftObject.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft {

// Objects own their memory through Bytes, so the clear callback has nothing to release: the
// handle is informational (it is what the frame graph logs and compares).
static void SoftObject_clear(core::Device *, core::ObjectType, core::ObjectHandle, void *) { }

bool Buffer::setup(Device &dev, const core::BufferInfo &info,
		const Callback<size_t(uint8_t *, uint64_t)> *fill) {
	_info = info;

	if (info.size > 0) {
		_storage.resize(size_t(info.size), uint8_t(0));
	}

	if (fill && info.size > 0) {
		(*fill)(_storage.data(), info.size);
	}

	// The flat vertex stage dereferences material/vertex "device addresses" directly, so the
	// address it gets must be the real host pointer.
	_deviceAddress = reinterpret_cast<uint64_t>(_storage.data());

	// ObjectHandle is void* or uint64_t depending on the build, and nothing here needs to be
	// released through it, so it stays zero for every soft object.
	return core::BufferObject::init(dev, SoftObject_clear, core::ObjectType::Buffer,
			core::ObjectHandle::zero());
}

bool Buffer::init(Device &dev, const core::BufferInfo &info, BytesView initialData) {
	if (initialData.empty()) {
		return setup(dev, info, nullptr);
	}

	auto cb = Callback<size_t(uint8_t *, uint64_t)>([&](uint8_t *mem, uint64_t size) -> size_t {
		auto bytes = sprt::min(uint64_t(initialData.size()), size);
		sprt::memcpy(mem, initialData.data(), size_t(bytes));
		return size_t(bytes);
	});
	return setup(dev, info, &cb);
}

bool Buffer::init(Device &dev, const core::BufferData &data) {
	if (data.data.empty() && !data.memCallback && !data.stdCallback) {
		return setup(dev, data, nullptr);
	}

	auto cb = Callback<size_t(uint8_t *, uint64_t)>([&](uint8_t *mem, uint64_t size) -> size_t {
		return data.writeData(mem, size_t(size));
	});
	return setup(dev, data, &cb);
}

bool Sampler::init(Device &dev, const core::SamplerInfo &info) {
	_info = info;

	return core::Sampler::init(dev, SoftObject_clear, core::ObjectType::Sampler,
			core::ObjectHandle::zero());
}

bool Image::setup(Device &dev, const core::ImageInfoData &info,
		const Callback<size_t(uint8_t *, uint64_t)> *fill) {
	_info = info;

	auto pixelSize = getPixelSize(info.format);
	if (pixelSize == 0) {
		log::source().error("soft::Image", "Unsupported image format: ",
				core::getImageFormatName(info.format));
		return false;
	}

	// Tightly packed rows: no hardware alignment to honour, and a stride that is exactly
	// width * pixelSize keeps readback a single memcpy.
	_stride = info.extent.width * pixelSize;
	_layerSize = _stride * info.extent.height;

	auto layers = sprt::max(uint32_t(info.arrayLayers.get()), uint32_t(1));
	auto total = uint64_t(_layerSize) * uint64_t(layers) * uint64_t(sprt::max(info.extent.depth, 1U));

	if (total > 0) {
		_storage.resize(size_t(total), uint8_t(0));
	}

	if (fill && total > 0) {
		(*fill)(_storage.data(), total);
	}

	return core::ImageObject::init(dev, SoftObject_clear, core::ObjectType::Image,
			core::ObjectHandle::zero(), nullptr, dev.getNextObjectIndex());
}

bool Image::init(Device &dev, StringView name, const core::ImageInfoData &info) {
	if (!setup(dev, info, nullptr)) {
		return false;
	}
	setName(name);
	return true;
}

bool Image::init(Device &dev, const core::ImageData &data) {
	if (data.data.empty() && !data.memCallback && !data.stdCallback) {
		if (!setup(dev, data, nullptr)) {
			return false;
		}
	} else {
		auto cb = Callback<size_t(uint8_t *, uint64_t)>([&](uint8_t *mem, uint64_t size) -> size_t {
			return data.writeData(mem, size_t(size));
		});
		if (!setup(dev, data, &cb)) {
			return false;
		}
	}

	_atlas = data.atlas;
	setName(data.key);
	return true;
}

uint8_t *Image::getLayerData(uint32_t layer) const {
	auto layers = sprt::max(uint32_t(_info.arrayLayers.get()), uint32_t(1))
			* sprt::max(_info.extent.depth, 1U);
	if (layer >= layers) {
		return nullptr;
	}
	return const_cast<uint8_t *>(_storage.data()) + size_t(layer) * size_t(_layerSize);
}

bool ImageView::init(Device &dev, const Rc<core::ImageObject> &image,
		const core::ImageViewInfo &info) {
	_image = image;
	_info = image->getViewInfo(info);

	// Views are keyed by index in the framebuffer cache; a fresh one per view is required.
	_index = dev.getNextObjectIndex();

	return core::ImageView::init(dev, SoftObject_clear, core::ObjectType::ImageView,
			core::ObjectHandle::zero());
}

bool Framebuffer::init(Device &dev, const core::QueuePassData *pass,
		SpanView<Rc<core::ImageView>> views) {
	_renderPass = pass->impl;
	if (!views.empty()) {
		auto extent = views.front()->getFramebufferExtent();
		_extent = Extent2(extent.width, extent.height);
		_layerCount = extent.depth;
	}
	for (auto &it : views) {
		_imageViews.emplace_back(it);
		_viewIds.emplace_back(it->getIndex());
	}

	return core::Framebuffer::init(dev, SoftObject_clear, core::ObjectType::Framebuffer,
			core::ObjectHandle::zero());
}

bool Semaphore::init(Device &dev) {
	return core::Semaphore::init(dev, SoftObject_clear, core::ObjectType::Semaphore,
			core::ObjectHandle::zero());
}

bool Fence::init(Device &dev, core::FenceType type) {
	_type = type;

	return core::Object::init(dev, SoftObject_clear, core::ObjectType::Fence,
			core::ObjectHandle::zero());
}

Status Fence::doCheckFence(bool lockfree) {
	// Work submitted to this backend has already completed by the time anyone asks: rasterization
	// runs to completion inside submit. The only unsignalled state is between reset and the next
	// submit, and no caller waits there.
	return _signaled.load() ? Status::Ok : Status::Suspended;
}

void Fence::doResetFence() { _signaled.store(true); }

} // namespace stappler::xenolith::soft
