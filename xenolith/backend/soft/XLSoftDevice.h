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

#ifndef XENOLITH_BACKEND_SOFT_XLSOFTDEVICE_H_
#define XENOLITH_BACKEND_SOFT_XLSOFTDEVICE_H_

#include "XLSoftInstance.h"
#include "XLCoreDevice.h"
#include "SPRaster.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft {

class SP_PUBLIC Device final : public core::Device {
public:
	virtual ~Device() = default;

	bool init(const Instance *);

	// Monotonic id handed to every ImageView: the frame cache keys framebuffers by it, so it must
	// be unique per view and never reused.
	uint64_t getNextObjectIndex() { return _objectIndex.fetch_add(1) + 1; }

	// Samplers are immutable value objects here, so identical requests share one instance.
	Rc<core::Sampler> getSampler(const core::SamplerInfo &);

	virtual Rc<core::Framebuffer> makeFramebuffer(const core::QueuePassData *,
			SpanView<Rc<core::ImageView>>) override;
	virtual Rc<core::ImageStorage> makeImage(StringView, const core::ImageInfoData &) override;
	virtual Rc<core::Semaphore> makeSemaphore() override;
	virtual Rc<core::ImageView> makeImageView(const Rc<core::ImageObject> &,
			const core::ImageViewInfo &) override;
	virtual Rc<core::TextureSet> makeTextureSet(const core::TextureSetLayout &) override;

	// A rasterization running on the pool while the loop thread goes on; waitIdle waits for its
	// pixels. Jobs that have finished are dropped on the next call.
	void addRasterJob(Rc<raster::TiledDrawJob> &&);

	virtual void waitIdle() const override;

protected:
	using core::Device::init;

	mutable sprt::mutex _rasterJobsMutex;
	Vector<Rc<raster::TiledDrawJob>> _rasterJobs;

	sprt::atomic<uint64_t> _objectIndex = 1;

	sprt::mutex _samplerMutex;
	Vector<Rc<core::Sampler>> _samplers;
};

} // namespace stappler::xenolith::soft

#endif /* XENOLITH_BACKEND_SOFT_XLSOFTDEVICE_H_ */
