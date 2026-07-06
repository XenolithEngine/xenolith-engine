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

#include "XLWgpuTextureSet.h"
#include "XLCoreQueue.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::webgpu {

uint32_t TextureSetLayout::getLayoutImageCount(Device &dev,
		const core::TextureSetLayoutData &data) {
	uint32_t maxImageCount = data.imageCount;
	if (dev.hasFeature(WGPUFeatureName(WGPUNativeFeature_PartiallyBoundBindingArray))) {
		maxImageCount = data.imageCountIndexed;
	}

	auto &limits = dev.getLimits();
	auto imageLimit = limits.maxSampledTexturesPerShaderStage;
	if (imageLimit > 2) {
		imageLimit -= 2;
	}

	// binding array elements are limited separately (wgpu native limit)
	auto &nativeLimits = dev.getNativeLimits();
	if (nativeLimits.maxBindingArrayElementsPerShaderStage > 0) {
		imageLimit = sprt::min(imageLimit, nativeLimits.maxBindingArrayElementsPerShaderStage);
	}

	return sprt::min(maxImageCount, imageLimit);
}

bool TextureSetLayout::init(Device &dev, const core::TextureSetLayoutData &data) {
	if (!dev.hasFeature(WGPUFeatureName(WGPUNativeFeature_TextureBindingArray))) {
		log::source().error("webgpu::TextureSetLayout",
				"TextureBindingArray feature is not supported by device");
		return false;
	}

	_device = &dev;
	_imageCount = getLayoutImageCount(dev, data);
	_samplersCount = uint32_t(data.compiledSamplers.size());
	_partiallyBound =
			dev.hasFeature(WGPUFeatureName(WGPUNativeFeature_PartiallyBoundBindingArray));

	for (auto &it : data.compiledSamplers) { _compiledSamplers.emplace_back(it); }

	// wgpu-native still consumes array sizes via the legacy chained struct,
	// the standard bindingArraySize field is not honored yet
	WGPUBindGroupLayoutEntryExtras samplersCountExtra{};
	samplersCountExtra.chain.sType = (WGPUSType)WGPUSType_BindGroupLayoutEntryExtras;
	samplersCountExtra.count = _samplersCount;

	WGPUBindGroupLayoutEntryExtras imagesCountExtra{};
	imagesCountExtra.chain.sType = (WGPUSType)WGPUSType_BindGroupLayoutEntryExtras;
	imagesCountExtra.count = _imageCount;

	WGPUBindGroupLayoutEntry entries[2];

	entries[0] = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
	entries[0].binding = 0;
	entries[0].visibility = WGPUShaderStage_Fragment | WGPUShaderStage_Compute;
	entries[0].bindingArraySize = _samplersCount;
	entries[0].nextInChain = &samplersCountExtra.chain;
	entries[0].sampler.type = WGPUSamplerBindingType_Filtering;

	entries[1] = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
	entries[1].binding = 1;
	entries[1].visibility = WGPUShaderStage_Fragment | WGPUShaderStage_Compute;
	entries[1].bindingArraySize = _imageCount;
	entries[1].nextInChain = &imagesCountExtra.chain;
	entries[1].texture.sampleType = WGPUTextureSampleType_Float;
	entries[1].texture.viewDimension = WGPUTextureViewDimension_2D;

	WGPUBindGroupLayoutDescriptor layoutDesc = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
	layoutDesc.label = WGPUStringView{data.key.data(), data.key.size()};
	layoutDesc.entryCount = 2;
	layoutDesc.entries = entries;

	_layout = wgpuDeviceCreateBindGroupLayout(dev.getDevice(), &layoutDesc);
	if (!_layout) {
		log::source().error("webgpu::TextureSetLayout",
				"Fail to create bind group layout: ", data.key);
		return false;
	}

	_emptyImage = data.queue->emptyImage;
	_solidImage = data.queue->solidImage;

	return core::Object::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle ptr, void *) {
		wgpuBindGroupLayoutRelease((WGPUBindGroupLayout)ptr.get());
	}, core::ObjectType::DescriptorSetLayout, core::ObjectHandle(_layout));
}

TextureSet::~TextureSet() {
	if (_bindGroup) {
		wgpuBindGroupRelease(_bindGroup);
		_bindGroup = nullptr;
	}
}

bool TextureSet::init(Device &dev, const TextureSetLayout &layout) {
	_device = &dev;
	_setLayout = &layout;
	_count = layout.getImageCount();

	return core::Object::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle, void *) { },
			core::ObjectType::DescriptorPool, core::ObjectHandle::zero());
}

void TextureSet::write(const core::MaterialLayout &set) {
	Vector<WGPUSampler> samplers;
	samplers.reserve(_setLayout->getSamplersCount());
	for (auto &it : _setLayout->getCompiledSamplers()) {
		samplers.emplace_back(it.get_cast<Sampler>()->getSampler());
	}

	auto emptyImage = _setLayout->getEmptyImage();
	if (!emptyImage || emptyImage->views.empty() || !emptyImage->views.front()->view) {
		log::source().error("webgpu::TextureSet", "No empty image view in layout");
		return;
	}

	auto emptyView =
			emptyImage->views.front()->view.get_cast<ImageView>()->getTextureView();

	const uint32_t imageCount = _setLayout->isPartiallyBound()
			? sprt::max(set.usedImageSlots, 1U)
			: _setLayout->getImageCount();

	_layoutIndexes.resize(imageCount, 0);

	Vector<WGPUTextureView> views;
	views.reserve(imageCount);

	for (uint32_t i = 0; i < imageCount; ++i) {
		if (i < set.usedImageSlots && set.imageSlots[i].image) {
			views.emplace_back(
					set.imageSlots[i].image.get_cast<ImageView>()->getTextureView());
			_layoutIndexes[i] = set.imageSlots[i].image->getIndex();
		} else {
			views.emplace_back(emptyView);
			_layoutIndexes[i] = 0;
		}
	}

	WGPUBindGroupEntryExtras samplersExtra{};
	samplersExtra.chain.sType = (WGPUSType)WGPUSType_BindGroupEntryExtras;
	samplersExtra.samplers = samplers.data();
	samplersExtra.samplerCount = samplers.size();

	WGPUBindGroupEntryExtras viewsExtra{};
	viewsExtra.chain.sType = (WGPUSType)WGPUSType_BindGroupEntryExtras;
	viewsExtra.textureViews = views.data();
	viewsExtra.textureViewCount = views.size();

	WGPUBindGroupEntry entries[2];
	entries[0] = WGPU_BIND_GROUP_ENTRY_INIT;
	entries[0].binding = 0;
	entries[0].nextInChain = &samplersExtra.chain;

	entries[1] = WGPU_BIND_GROUP_ENTRY_INIT;
	entries[1].binding = 1;
	entries[1].nextInChain = &viewsExtra.chain;

	WGPUBindGroupDescriptor groupDesc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
	groupDesc.label = WGPUStringView{"TextureSet", WGPU_STRLEN};
	groupDesc.layout = _setLayout->getLayout();
	groupDesc.entryCount = 2;
	groupDesc.entries = entries;

	auto group = wgpuDeviceCreateBindGroup(_device->getDevice(), &groupDesc);
	if (!group) {
		log::source().error("webgpu::TextureSet", "Fail to create bind group");
		return;
	}

	if (_bindGroup) {
		wgpuBindGroupRelease(_bindGroup);
	}
	_bindGroup = group;
}

} // namespace stappler::xenolith::webgpu
