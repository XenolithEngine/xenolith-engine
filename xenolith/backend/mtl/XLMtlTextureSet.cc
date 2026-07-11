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

#include "XLMtlTextureSet.h"
#include "XLCoreQueue.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::mtl {

uint32_t TextureSetLayout::getLayoutImageCount(Device &dev,
		const core::TextureSetLayoutData &data) {
	// Metal 3 argument buffers hold up to ~500k resource handles on any
	// supported device, the handles themselves are the only per-slot cost
	return data.imageCountIndexed;
}

bool TextureSetLayout::init(Device &dev, const core::TextureSetLayoutData &data) {
	if (![dev.getDevice() supportsFamily:MTLGPUFamilyMetal3]) {
		// pre-Metal3 devices would need MTLArgumentEncoder-based writes
		log::source().error("mtl::TextureSetLayout",
				"texture sets require a Metal 3 device (gpuResourceID argument buffers): ",
				data.key);
		return false;
	}

	_device = &dev;
	_imageCount = getLayoutImageCount(dev, data);
	_samplersCount = uint32_t(data.compiledSamplers.size());
	// argument buffer slots are written individually, unused ones are padded
	// with the empty image - effectively always partially bound
	_partiallyBound = true;

	for (auto &it : data.compiledSamplers) { _compiledSamplers.emplace_back(it); }

	_emptyImage = data.queue->emptyImage;
	_solidImage = data.queue->solidImage;

	return core::Object::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle, void *) { },
			core::ObjectType::DescriptorSetLayout, core::ObjectHandle::zero());
}

bool TextureSet::init(Device &dev, const TextureSetLayout &layout) {
	_device = &dev;
	_setLayout = &layout;
	_count = layout.getImageCount();

	const uint64_t size =
			uint64_t(layout.getSamplersCount() + layout.getImageCount()) * sizeof(MTLResourceID);

	@autoreleasepool {
		id<MTLBuffer> buffer = [dev.getDevice() newBufferWithLength:size
															options:MTLResourceStorageModeShared];
		if (!buffer) {
			log::source().error("mtl::TextureSet", "Fail to create argument buffer");
			return false;
		}
		buffer.label = @"TextureSet";
		_buffer = retainHandle(buffer);
	}

	return core::Object::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle ptr, void *) {
		releaseHandle((void *)ptr.get());
	}, core::ObjectType::DescriptorPool, core::ObjectHandle(_buffer));
}

void TextureSet::write(const core::MaterialLayout &set) {
	auto handles = reinterpret_cast<MTLResourceID *>(getArgumentBuffer().contents);

	for (auto &it : _setLayout->getCompiledSamplers()) {
		*handles++ = it.get_cast<Sampler>()->getSampler().gpuResourceID;
	}

	auto emptyImage = _setLayout->getEmptyImage();
	if (!emptyImage || emptyImage->views.empty() || !emptyImage->views.front()->view) {
		log::source().error("mtl::TextureSet", "No empty image view in layout");
		return;
	}

	auto &emptyViewRc = emptyImage->views.front()->view;
	auto emptyHandle = emptyViewRc.get_cast<ImageView>()->getTextureView().gpuResourceID;

	const uint32_t imageCount = _setLayout->getImageCount();
	const uint32_t usedCount = sprt::min(set.usedImageSlots, imageCount);

	_layoutIndexes.clear();
	_layoutIndexes.resize(imageCount, 0);

	// residency needs each UNIQUE texture once, not one entry per slot: the
	// padding below points thousands of slots at the same empty image, and
	// calling useResource per slot (O(imageCount)) dominated frame recording
	_residencyViews.clear();
	_residencyViews.emplace_back(emptyViewRc);

	// used slots: real image handles
	for (uint32_t i = 0; i < usedCount; ++i) {
		if (set.imageSlots[i].image) {
			auto &viewRc = set.imageSlots[i].image;
			handles[i] = viewRc.get_cast<ImageView>()->getTextureView().gpuResourceID;
			_layoutIndexes[i] = viewRc->getIndex();
			_residencyViews.emplace_back(viewRc);
		} else {
			handles[i] = emptyHandle;
		}
	}

	// unused slots: pad with the empty image handle (already resident via the
	// single entry above)
	for (uint32_t i = usedCount; i < imageCount; ++i) { handles[i] = emptyHandle; }
}

} // namespace stappler::xenolith::mtl
