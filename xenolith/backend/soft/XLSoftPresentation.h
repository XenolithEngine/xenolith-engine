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

#ifndef XENOLITH_BACKEND_SOFT_XLSOFTPRESENTATION_H_
#define XENOLITH_BACKEND_SOFT_XLSOFTPRESENTATION_H_

#include "XLSoftDevice.h"
#include "XLCoreSwapchain.h"
#include "XLCorePresentationEngine.h"

#include <sprt/runtime/window/software_surface.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft {

class Instance;

// Everything a soft swapchain does that does not depend on where the pixels end up. Only init,
// acquire and present differ between presenting into a window and presenting into nothing, so
// they are the only three left to the subclasses.
class SP_PUBLIC SwapchainBase : public core::Swapchain {
public:
	virtual ~SwapchainBase();

	virtual void invalidateImage(const core::ImageStorage *image, bool release) override;
	virtual void invalidateImage(uint32_t, bool release) override;

	virtual Rc<core::ImageView> makeView(const Rc<core::ImageObject> &,
			const core::ImageViewInfo &) override;

	virtual Rc<core::Semaphore> acquireSemaphore() override;
	virtual bool releaseSemaphore(Rc<core::Semaphore> &&) override;

	// Present is bookkeeping plus, at most, a window-system call: there is no queue from a Present
	// family to acquire, and soft::Device has no queue families at all.
	virtual bool isPresentQueueRequired() const override { return false; }

	SpanView<SwapchainImageData> getImages() const { return _images; }

	// Image holding the most recently presented frame. Null until the first present.
	core::ImageObject *getLastPresentedImage() const;

	// Retire every view up front, while the images are still alive: doing it lazily makes the
	// release callbacks fire from inside frame-cache teardown, mutating the vector being cleared.
	void invalidateViews();

protected:
	using core::Object::init;

	// Shared tail of every init: publish the negotiated configuration and size the damage tracker.
	bool finalize(Device &, const core::SurfaceInfo &, const core::SwapchainConfig &,
			core::ImageInfo &&, core::PresentMode, core::Surface *);

	void markAcquired(uint32_t index);

	// Release the slot as far as the engine is concerned. This must happen at present, not when
	// the window system hands the buffer back: swapchain recreation waits for
	// getAcquiredImagesCount() to reach zero, and a compositor is free to hold the last presented
	// buffer indefinitely - tying the two together wedges every resize. Whether the buffer itself
	// is reusable is a separate question, and the transport answers it.
	void markPresented(uint32_t index);

	uint32_t findSlot(const core::ImageStorage *) const;

	Vector<SwapchainImageData> _images;
	Vector<bool> _acquired;
	uint32_t _lastPresentedIndex = maxOf<uint32_t>();
};

// A surface backed by a window system that can hand out CPU-writable buffers. Capabilities come
// from the transport, unlike the headless one which has nothing to ask and synthesizes them.
class SP_PUBLIC Surface final : public core::Surface {
public:
	virtual ~Surface() = default;

	bool init(Instance *, Rc<sprt::window::SoftwareSurface> &&, Ref *window = nullptr);

	virtual void invalidate() override;

	virtual core::SurfaceInfo getSurfaceOptions(const core::Device &,
			core::FullScreenExclusiveMode, void *) const override;

	const Rc<sprt::window::SoftwareSurface> &getSoftwareSurface() const { return _software; }

protected:
	Rc<sprt::window::SoftwareSurface> _software;
};

// Swapchain over window-system memory: every image is a view onto a buffer the compositor (or the
// X server) owns, so the rasterizer writes the frame straight into what gets presented.
class SP_PUBLIC Swapchain final : public SwapchainBase {
public:
	virtual ~Swapchain();

	bool init(Device &, NotNull<core::Loop>, const core::SurfaceInfo &,
			const core::SwapchainConfig &, core::ImageInfo &&, core::PresentMode, Surface *);

	virtual Rc<SwapchainAcquiredImage> acquire(bool lockfree, const Rc<core::Fence> &,
			Status &) override;

	virtual Status present(core::DeviceQueue *, core::ImageStorage *,
			const core::PresentInfo &) override;

protected:
	using SwapchainBase::init;

	Rc<sprt::window::SoftwareSwapchain> _software;
};

class SP_PUBLIC PresentationEngine : public core::PresentationEngine {
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

	virtual void captureScreenshot(
			Function<void(const core::ImageInfoData &info, BytesView view)> &&cb) override;

protected:
	// The transport-specific half of createSwapchain. Everything around it - constraints, the
	// frame cache registration, retiring the previous swapchain - is the same either way.
	virtual Rc<SwapchainBase> makeSwapchain(const core::SurfaceInfo &,
			const core::SwapchainConfig &, core::ImageInfo &&, core::PresentMode);
};

} // namespace stappler::xenolith::soft

#endif /* XENOLITH_BACKEND_SOFT_XLSOFTPRESENTATION_H_ */
