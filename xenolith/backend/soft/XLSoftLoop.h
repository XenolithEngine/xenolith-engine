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

#ifndef XENOLITH_BACKEND_SOFT_XLSOFTLOOP_H_
#define XENOLITH_BACKEND_SOFT_XLSOFTLOOP_H_

#include "XLSoftDevice.h"
#include "XLCoreLoop.h"

#include <sprt/runtime/dispatch/handle.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft {

class SP_PUBLIC Loop : public core::Loop {
public:
	virtual ~Loop() = default;

	virtual bool init(NotNull<sprt::dispatch::Looper>, NotNull<core::Instance>,
			Rc<LoopInfo> &&) override;

	virtual void run() override;
	virtual void stop() override;

	virtual bool isRunning() const override;

	virtual void compileResource(Rc<core::Resource> &&req, Function<void(bool)> && = nullptr,
			bool preload = false) const override;
	virtual void compileQueue(const Rc<Queue> &req,
			Function<void(bool)> && = nullptr) const override;

	virtual void compileMaterials(Rc<core::MaterialInputData> &&req,
			const Vector<Rc<DependencyEvent>> & = Vector<Rc<DependencyEvent>>()) const override;
	virtual void compileImage(const Rc<core::DynamicImage> &,
			Function<void(bool)> && = nullptr) const override;

	virtual void runRenderQueue(Rc<FrameRequest> &&req, uint64_t gen = 0,
			Function<void(bool)> && = nullptr) override;

	virtual void performInQueue(Rc<sprt::dispatch::Task> &&) const override;
	virtual void performInQueue(Function<void()> &&func, Ref *target = nullptr) const override;

	virtual void performOnThread(Function<void()> &&func, Ref *target = nullptr,
			bool immediate = false, StringView tag = SP_FUNC) const override;

	virtual Rc<FrameHandle> makeFrame(Rc<FrameRequest> &&, uint64_t gen) override;

	virtual Rc<core::Framebuffer> acquireFramebuffer(const PassData *,
			SpanView<Rc<core::ImageView>>) override;
	virtual void releaseFramebuffer(Rc<core::Framebuffer> &&) override;

	virtual Rc<ImageStorage> acquireImage(const ImageAttachment *, const AttachmentHandle *,
			const core::ImageInfoData &) override;
	virtual void releaseImage(Rc<ImageStorage> &&) override;

	virtual Rc<core::Semaphore> makeSemaphore() override;

	virtual core::ImageFormat getCommonFormat() const override;

	virtual SpanView<core::ImageFormat> getSupportedDepthStencilFormat() const override;

	virtual Rc<core::Fence> acquireFence(core::FenceType) override;

	virtual void signalDependencies(const Vector<Rc<DependencyEvent>> &, Queue *,
			bool success) override;
	virtual void waitForDependencies(const Vector<Rc<DependencyEvent>> &,
			Function<void(bool)> &&) override;

	virtual void waitIdle() override;

	virtual void captureImage(Function<void(const core::ImageInfoData &info, BytesView view)> &&cb,
			const Rc<core::ImageObject> &image, core::AttachmentLayout l) override;

	virtual void captureBuffer(Function<void(const core::BufferInfo &info, BytesView view)> &&cb,
			const Rc<core::BufferObject> &) override;

	virtual Rc<core::PresentationEngine> makePresentationEngine(NotNull<core::PresentationWindow>,
			core::PresentationOptions) override;

	Device *getDevice() const { return _device; }

	const BackendFeatures &getBackendFeatures() const { return _backendFeatures; }

	void scheduleFence(Rc<core::Fence> &&);

	// assign texture slots, create image views, write texture sets and fill per-material data;
	// must be called on the loop thread
	bool updateMaterialSet(NotNull<core::MaterialSet>, SpanView<Rc<core::Material>> materials,
			SpanView<core::MaterialId> dynamicMaterials,
			SpanView<core::MaterialId> materialsToRemove);

protected:
	using core::Loop::init;

	void updateFences();

	struct DependencyRequest : public Ref {
		Vector<Rc<DependencyEvent>> events;
		Function<void(bool)> callback;
		uint32_t signaled = 0;
		bool success = true;
	};

	Rc<Device> _device;
	BackendFeatures _backendFeatures;
	Map<core::DependencyEvent *, Vector<Rc<DependencyRequest>>> _dependencyRequests;
	Rc<sprt::dispatch::TimerHandle> _updateTimerHandle;
	Vector<Rc<core::Fence>> _scheduledFences;
	sprt::atomic<bool> _running = false;
};

} // namespace stappler::xenolith::soft

#endif /* XENOLITH_BACKEND_SOFT_XLSOFTLOOP_H_ */
