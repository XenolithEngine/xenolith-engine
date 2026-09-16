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

#include "XLWgpuDevice.h"
#include "XLWgpuObject.h"
#include "XLWgpuTextureSet.h"
#include "XLCoreImageStorage.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::webgpu {

Device::~Device() {
	_samplers.clear();
	clearShaders();
	invalidateObjects();

	if (_queue) {
		wgpuQueueRelease(_queue);
		_queue = nullptr;
	}
	if (_device) {
		wgpuDeviceRelease(_device);
		_device = nullptr;
	}
	if (_adapterData.adapter) {
		wgpuAdapterRelease(_adapterData.adapter);
		_adapterData.adapter = nullptr;
	}
}

bool Device::init(NotNull<Instance> instance, const Instance::AdapterData &adapterData,
		WGPUDevice device) {
	if (!core::Device::init(instance)) {
		return false;
	}

	_adapterData = adapterData;
	if (_adapterData.adapter) {
		wgpuAdapterAddRef(_adapterData.adapter);
	}

	_device = device;
	_queue = wgpuDeviceGetQueue(_device);


	_limits = WGPU_LIMITS_INIT;
	_nativeLimits.chain.sType = (WGPUSType)WGPUSType_NativeLimits;
	_limits.nextInChain = &_nativeLimits.chain;
	if (wgpuDeviceGetLimits(_device, &_limits) != WGPUStatus_Success) {
		log::source().warn("webgpu::Device", "Fail to read device limits");
	}
	_limits.nextInChain = nullptr;

	WGPUSupportedFeatures features;
	wgpuDeviceGetFeatures(_device, &features);
	_features.reserve(features.featureCount);
	for (size_t i = 0; i < features.featureCount; ++i) {
		_features.emplace_back(features.features[i]);
	}
	wgpuSupportedFeaturesFreeMembers(features);

	// capability snapshot: standard fallbacks engage when a feature is absent
	// (see BackendFeatures docs); a browser build reports none of the native ones
	_backendFeatures.textureComponentSwizzle = hasFeature(WGPUFeatureName_TextureComponentSwizzle);
#if XL_WGPU_NATIVE_API
	_backendFeatures.textureBindingArrays =
			hasFeature(WGPUFeatureName(WGPUNativeFeature_TextureBindingArray));
	_backendFeatures.partiallyBoundArrays =
			hasFeature(WGPUFeatureName(WGPUNativeFeature_PartiallyBoundBindingArray));
	// the SpirV entry point is part of the native API itself (naga translates)
	_backendFeatures.spirvShaders = true;
	_backendFeatures.syncPolling = true;
#endif

	// verification switch: exercise the standard (browser-mode) fallbacks on
	// a native device
	if (::getenv("XL_WGPU_NO_BINDING_ARRAYS")) {
		_backendFeatures.textureBindingArrays = false;
		_backendFeatures.partiallyBoundArrays = false;
	}

	// WebGPU guarantees this depth-stencil set on any device
	_depthFormats.emplace_back(core::ImageFormat::D32_SFLOAT);
	_depthFormats.emplace_back(core::ImageFormat::D24_UNORM_S8_UINT);
	_depthFormats.emplace_back(core::ImageFormat::D16_UNORM);
	if (hasFeature(WGPUFeatureName_Depth32FloatStencil8)) {
		_depthFormats.emplace_back(core::ImageFormat::D32_SFLOAT_S8_UINT);
	}

	_colorFormats.emplace_back(core::ImageFormat::R8G8B8A8_UNORM);
	_colorFormats.emplace_back(core::ImageFormat::B8G8R8A8_UNORM);

	// WebGPU exposes single device queue, capable of all operations
	auto &family = _families.emplace_back(core::DeviceQueueFamily());
	family.index = 0;
	family.count = 1;
	family.preferred = core::QueueFlags::Graphics;
	family.flags = core::QueueFlags::Graphics | core::QueueFlags::Compute
			| core::QueueFlags::Transfer | core::QueueFlags::Present;
	family.queues.emplace_back(Rc<core::DeviceQueue>::create(*this, 0, family.flags));

	return true;
}

bool Device::hasFeature(WGPUFeatureName feature) const {
	for (auto &it : _features) {
		if (it == feature) {
			return true;
		}
	}
	return false;
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
	core::Device::waitIdle();

	if (!_device) {
		return;
	}

#if XL_WGPU_NATIVE_API
	// full drain: wait-polls until work submitted so far reports completion
	bool done = false;

	WGPUQueueWorkDoneCallbackInfo cbInfo = WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT;
	cbInfo.mode = WGPUCallbackMode_AllowProcessEvents;
	cbInfo.callback = [](WGPUQueueWorkDoneStatus, WGPUStringView, void *userdata1, void *) {
		*reinterpret_cast<bool *>(userdata1) = true;
	};
	cbInfo.userdata1 = &done;

	wgpuQueueOnSubmittedWorkDone(_queue, cbInfo);

	while (!done) { wgpuDevicePoll(_device, true, nullptr); }
#else
	log::source().warn("webgpu::Device",
			"waitIdle is not available without synchronous polling, use drain()");
#endif
}

void Device::drain(Function<void()> &&cb) const {
	if (!_device) {
		if (cb) {
			cb();
		}
		return;
	}

	struct DrainContext {
		Function<void()> callback;
	};

	WGPUQueueWorkDoneCallbackInfo cbInfo = WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT;
	cbInfo.mode = WGPUCallbackMode_AllowProcessEvents;
	cbInfo.callback = [](WGPUQueueWorkDoneStatus status, WGPUStringView message, void *userdata1,
			void *) {
		auto ctx = reinterpret_cast<DrainContext *>(userdata1);
		if (status != WGPUQueueWorkDoneStatus_Success) {
			log::source().warn("webgpu::Device", "drain: OnSubmittedWorkDone failed: ",
					toStringView(message));
		}
		if (ctx->callback) {
			ctx->callback();
		}
		delete ctx;
	};
	cbInfo.userdata1 = new DrainContext{sp::move(cb)};

	wgpuQueueOnSubmittedWorkDone(_queue, cbInfo);
}

} // namespace stappler::xenolith::webgpu
