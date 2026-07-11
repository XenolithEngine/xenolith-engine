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

#ifndef XENOLITH_BACKEND_MTL_XLMTLPRESENTATION_H_
#define XENOLITH_BACKEND_MTL_XLMTLPRESENTATION_H_

#include "XLMtlObject.h"
#include "XLCoreSwapchain.h"
#include "XLCorePresentationEngine.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::mtl {

// Surface over CAMetalLayer: the layer comes from the platform window
// (NativeWindow::getSurfaceInterfaceInfo), the surface retains it
class SP_PUBLIC Surface : public core::Surface {
public:
	virtual ~Surface();

	// layer is an ObjC handle to CAMetalLayer (retained)
	bool init(Instance *, void *layer, Ref *window = nullptr);

	virtual void invalidate() override;

	virtual core::SurfaceInfo getSurfaceOptions(const core::Device &,
			core::FullScreenExclusiveMode, void *) const override;

#if __OBJC__
	CAMetalLayer *getLayer() const { return bridgeHandle<CAMetalLayer *>(_layer); }
#endif

protected:
	void *_layer = nullptr; // __bridge_retained CAMetalLayer *
};

/* Metal exposes no swapchain object: images are acquired one at a time via
 * -[CAMetalLayer nextDrawable] (which also paces the frame loop by blocking
 * while all drawables are in flight) and presented with presentDrawable on a
 * command buffer of the device queue, so presentation orders after the
 * frame's rendering. Acquired drawables are held in a slot ring to provide
 * stable image indexes for the presentation engine */
class SP_PUBLIC Swapchain final : public core::Swapchain {
public:
	static constexpr uint32_t SlotCount = 3; // CAMetalLayer's drawable pool limit

	virtual ~Swapchain();

	bool init(Device &, NotNull<core::Loop>, const core::SurfaceInfo &,
			const core::SwapchainConfig &, core::ImageInfo &&, core::PresentMode, Surface *);

	virtual Rc<SwapchainAcquiredImage> acquire(bool lockfree, const Rc<core::Fence> &fence,
			Status &) override;

	virtual Status present(core::DeviceQueue &queue, core::ImageStorage *,
			uint64_t presentWindow) override;
	virtual void invalidateImage(const core::ImageStorage *, bool release) override;
	virtual void invalidateImage(uint32_t, bool release) override;

	virtual Rc<core::ImageView> makeView(const Rc<core::ImageObject> &,
			const core::ImageViewInfo &) override;

	virtual Rc<core::Semaphore> acquireSemaphore() override;
	virtual bool releaseSemaphore(Rc<core::Semaphore> &&) override;

protected:
	using core::Object::init;

	void releaseSlot(uint32_t);

	Device *_device = nullptr;
	core::Loop *_loop = nullptr;
	core::ImageInfo _swapchainImageInfo;
	Vector<SwapchainImageData> _images; // acquired drawable slots
	Vector<void *> _drawables; // __bridge_retained id<CAMetalDrawable>, by slot
	// views created for each slot: invalidated on slot release, so the
	// framebuffer cache drops them before the drawable is recycled
	Vector<Vector<Rc<core::ImageView>>> _slotViews;
	uint32_t _nextIndex = 0;
};

class SP_PUBLIC PresentationEngine final : public core::PresentationEngine {
public:
	virtual ~PresentationEngine() = default;

	virtual bool init(NotNull<core::Loop>, NotNull<core::Device>, NotNull<core::PresentationWindow>,
			core::PresentationOptions) override;

	virtual bool run() override;

	virtual Rc<core::ScreenInfo> getScreenInfo() const override;
	virtual Status setFullscreenSurface(const core::MonitorId &, const core::ModeInfo &) override;

	virtual bool recreateSwapchain() override;
	virtual bool createSwapchain(const core::SurfaceInfo &, core::SwapchainConfig &&cfg,
			core::PresentMode presentMode, bool oldSwapchainValid) override;

protected:
	// the window's requested display-link barrier; createSwapchain disables it
	// for Immediate present mode so presentation runs unbounded
	bool _windowFollowDisplayLinkBarrier = false;
};

} // namespace stappler::xenolith::mtl

#endif /* XENOLITH_BACKEND_MTL_XLMTLPRESENTATION_H_ */
