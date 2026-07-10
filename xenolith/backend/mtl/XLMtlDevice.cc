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

#include "XLMtlDevice.h"
#include "XLMtlObject.h"
#include "XLMtlTextureSet.h"
#include "XLCoreImageStorage.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::mtl {

Device::~Device() {
	_samplers.clear();
	clearShaders();
	invalidateObjects();

	releaseHandle(_queue);
	_queue = nullptr;

	releaseHandle(_device);
	_device = nullptr;

	releaseHandle(_deviceData.device);
	_deviceData.device = nullptr;
}

bool Device::init(NotNull<Instance> instance, const Instance::DeviceData &deviceData) {
	if (!core::Device::init(instance)) {
		return false;
	}

	_deviceData = deviceData;

	auto device = bridgeHandle<id<MTLDevice>>(deviceData.device);
	_deviceData.device = retainHandle(device);
	_device = retainHandle(device);

	id<MTLCommandQueue> queue = [device newCommandQueue];
	if (!queue) {
		log::source().error("mtl::Device", "Fail to create command queue on device: ",
				_deviceData.name);
		return false;
	}
	_queue = retainHandle(queue);

	// Metal guarantees this depth-stencil set on any device; D24S8 needs a
	// support query (isDepth24Stencil8PixelFormatSupported, macOS-only)
	_depthFormats.emplace_back(core::ImageFormat::D32_SFLOAT);
	_depthFormats.emplace_back(core::ImageFormat::D32_SFLOAT_S8_UINT);
	_depthFormats.emplace_back(core::ImageFormat::D16_UNORM);

	_colorFormats.emplace_back(core::ImageFormat::R8G8B8A8_UNORM);
	_colorFormats.emplace_back(core::ImageFormat::B8G8R8A8_UNORM);

	// Metal exposes a single device queue type, capable of all operations
	auto &family = _families.emplace_back(core::DeviceQueueFamily());
	family.index = 0;
	family.count = 1;
	family.preferred = core::QueueFlags::Graphics;
	family.flags = core::QueueFlags::Graphics | core::QueueFlags::Compute
			| core::QueueFlags::Transfer | core::QueueFlags::Present;
	family.queues.emplace_back(Rc<core::DeviceQueue>::create(*this, 0, family.flags));

	return true;
}

Rc<core::Framebuffer> Device::makeFramebuffer(const core::QueuePassData *pass,
		SpanView<Rc<core::ImageView>> views) {
	return Rc<Framebuffer>::create(*this, pass, views);
}

Rc<core::ImageStorage> Device::makeImage(StringView name, const core::ImageInfoData &info) {
	if (auto image = Rc<Image>::create(*this, name, info)) {
		return Rc<core::ImageStorage>::create(move(image));
	}
	return nullptr;
}

Rc<core::Semaphore> Device::makeSemaphore() { return Rc<Semaphore>::create(*this); }

Rc<core::Sampler> Device::getSampler(const core::SamplerInfo &info) {
	sprt::unique_lock<sprt::mutex> lock(_samplerMutex);
	for (auto &it : _samplers) {
		if (it->getInfo() == info) {
			return it;
		}
	}

	if (auto sampler = Rc<Sampler>::create(*this, info)) {
		return _samplers.emplace_back(move(sampler));
	}
	return nullptr;
}

Rc<core::ImageView> Device::makeImageView(const Rc<core::ImageObject> &image,
		const core::ImageViewInfo &info) {
	return Rc<ImageView>::create(*this, image, info);
}

Rc<core::TextureSet> Device::makeTextureSet(const core::TextureSetLayout &layout) {
	return Rc<TextureSet>::create(*this, static_cast<const TextureSetLayout &>(layout));
}

void Device::waitIdle() const {
	// Metal has no device-level wait: submit an empty command buffer and wait
	// for it - the queue executes buffers in submission order, so its
	// completion implies completion of everything submitted before it
	auto queue = bridgeHandle<id<MTLCommandQueue>>(_queue);
	if (!queue) {
		return;
	}

	@autoreleasepool {
		id<MTLCommandBuffer> buf = [queue commandBuffer];
		[buf commit];
		[buf waitUntilCompleted];
	}
}

} // namespace stappler::xenolith::mtl
