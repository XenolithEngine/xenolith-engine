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

#include "XLSoftDevice.h"
#include "XLSoftObject.h"
#include "XLSoftTextureSet.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft {

bool Device::init(const Instance *instance) {
	if (!core::Device::init(instance)) {
		return false;
	}

	// No depth formats at all: the flat contract has no depth buffer, and advertising one would
	// invite the default (shadow) queue to try to build passes this backend can not execute.
	_colorFormats.emplace_back(core::ImageFormat::R8G8B8A8_UNORM);
	_colorFormats.emplace_back(core::ImageFormat::B8G8R8A8_UNORM);
	_colorFormats.emplace_back(core::ImageFormat::R8_UNORM);

	// One "queue family": work is ordered by the loop's task queue, so a single graphics/transfer
	// queue models it exactly. Compute is not offered - there is no compute path here.
	auto &family = _families.emplace_back(core::DeviceQueueFamily());
	family.index = 0;
	family.count = 1;
	family.preferred = core::QueueFlags::Graphics;
	family.flags = core::QueueFlags::Graphics | core::QueueFlags::Transfer;
	family.queues.emplace_back(Rc<core::DeviceQueue>::create(*this, 0, family.flags));

	return true;
}

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

Rc<core::ImageView> Device::makeImageView(const Rc<core::ImageObject> &image,
		const core::ImageViewInfo &info) {
	return Rc<ImageView>::create(*this, image, info);
}

Rc<core::TextureSet> Device::makeTextureSet(const core::TextureSetLayout &layout) {
	return Rc<TextureSet>::create(*this, static_cast<const TextureSetLayout &>(layout));
}

void Device::waitIdle() const {
	// Every submit completes synchronously inside the raster job, so by the time control returns
	// here there is nothing outstanding to wait for.
	core::Device::waitIdle();
}

} // namespace stappler::xenolith::soft
