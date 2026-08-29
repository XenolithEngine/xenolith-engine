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

#include "XLGlesTextureSet.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles {

bool TextureSetLayout::init(Device &dev, const core::TextureSetLayoutData &data) {
	_imageCount = data.imageCountIndexed;
	_samplersCount = uint32_t(data.compiledSamplers.size());
	_partiallyBound = true;

	for (auto &it : data.compiledSamplers) { _compiledSamplers.emplace_back(it); }

	_emptyImage = data.queue->emptyImage;
	_solidImage = data.queue->solidImage;

	return core::Object::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle, void *) { },
			core::ObjectType::DescriptorSetLayout, core::ObjectHandle::zero());
}

bool TextureSet::init(Device &dev, const TextureSetLayout &layout) {
	_setLayout = &layout;
	_count = layout.getImageCount();

	return core::Object::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle, void *) { },
			core::ObjectType::DescriptorPool, core::ObjectHandle::zero());
}

void TextureSet::write(const core::MaterialLayout &set) {
	// Writing a set is just retaining the views: M1 has no binding to update and the M2 draw path
	// reads the slot table directly.
	_slotViews.clear();
	_slotViews.resize(set.usedImageSlots);
	_layoutIndexes.clear();
	_layoutIndexes.resize(set.usedImageSlots, 0);

	for (uint32_t i = 0; i < set.usedImageSlots; ++i) {
		if (set.imageSlots[i].image) {
			_slotViews[i] = set.imageSlots[i].image;
			_layoutIndexes[i] = set.imageSlots[i].image->getIndex();
		}
	}
}

} // namespace stappler::xenolith::gles
