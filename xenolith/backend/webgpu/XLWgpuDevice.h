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

#ifndef XENOLITH_BACKEND_WEBGPU_XLWGPUDEVICE_H_
#define XENOLITH_BACKEND_WEBGPU_XLWGPUDEVICE_H_

#include "XLWgpuInstance.h"
#include "XLCoreDevice.h"
#include "XLCoreDeviceQueue.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::webgpu {

class SP_PUBLIC Device final : public core::Device {
public:
	virtual ~Device();

	bool init(NotNull<Instance>, const Instance::AdapterData &, WGPUDevice);

	WGPUDevice getDevice() const { return _device; }
	WGPUQueue getQueue() const { return _queue; }

	const Instance::AdapterData &getAdapterData() const { return _adapterData; }
	const WGPULimits &getLimits() const { return _limits; }
	const WGPUNativeLimits &getNativeLimits() const { return _nativeLimits; }
	SpanView<WGPUFeatureName> getFeatures() const { return _features; }

	bool hasFeature(WGPUFeatureName) const;

	const BackendFeatures &getBackendFeatures() const { return _backendFeatures; }

	uint64_t getNextObjectIndex() { return _objectIndex.fetch_add(1) + 1; }

	// samplers are cached by SamplerInfo
	Rc<core::Sampler> getSampler(const core::SamplerInfo &);

	virtual Rc<core::Framebuffer> makeFramebuffer(const core::QueuePassData *,
			SpanView<Rc<core::ImageView>>) override;
	virtual Rc<core::ImageStorage> makeImage(StringView, const core::ImageInfoData &) override;
	virtual Rc<core::Semaphore> makeSemaphore() override;
	virtual Rc<core::ImageView> makeImageView(const Rc<core::ImageObject> &,
			const core::ImageViewInfo &) override;
	virtual Rc<core::TextureSet> makeTextureSet(const core::TextureSetLayout &) override;

	// blocks until all submitted work is complete; requires synchronous
	// polling (native API) - in a browser build it only logs a warning,
	// use drain() there
	virtual void waitIdle() const override;

	// asynchronous drain (portable, standard API): the callback fires when
	// all work submitted to the queue SO FAR is complete; delivered by the
	// device poll (loop timer) or the browser event loop - NOT necessarily
	// on the caller's thread, see Loop::drain for a thread-routed variant
	void drain(Function<void()> &&) const;

protected:
	Instance::AdapterData _adapterData;
	WGPUDevice _device = nullptr;
	WGPUQueue _queue = nullptr;
	BackendFeatures _backendFeatures;
	WGPULimits _limits;
	WGPUNativeLimits _nativeLimits = {};
	Vector<WGPUFeatureName> _features;
	sprt::atomic<uint64_t> _objectIndex = 1;

	sprt::mutex _samplerMutex;
	Vector<Rc<core::Sampler>> _samplers;
};

} // namespace stappler::xenolith::webgpu

#endif /* XENOLITH_BACKEND_WEBGPU_XLWGPUDEVICE_H_ */
