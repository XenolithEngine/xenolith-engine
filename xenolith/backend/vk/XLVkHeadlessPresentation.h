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

#ifndef XENOLITH_BACKEND_VK_XLVKHEADLESSPRESENTATION_H_
#define XENOLITH_BACKEND_VK_XLVKHEADLESSPRESENTATION_H_

#include "XLVkPresentationEngine.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

// Surface for a window that has no window system behind it.
//
// vk::Surface refuses a VK_NULL_HANDLE, so this is a sibling rather than a subclass: there is no
// VkSurfaceKHR at all, and the surface capabilities are synthesized from the window extent instead
// of being queried from a WSI implementation.
class SP_PUBLIC HeadlessSurface : public core::Surface {
public:
	virtual ~HeadlessSurface();

	bool init(Instance *instance, Extent2 extent, Ref *window = nullptr);

	virtual void invalidate() override;

	virtual core::SurfaceInfo getSurfaceOptions(const core::Device &, core::FullScreenExclusiveMode,
			void *) const override;

	void setExtent(Extent2 extent) { _extent = extent; }

protected:
	Extent2 _extent;
};

// Pseudo-swapchain: a ring of ordinary device images that imitate swapchain images.
//
// Every image also carries ImageUsage::TransferSrc, so Loop::captureImage can read the current
// screen without rendering an extra frame.
//
// Acquisition is synchronous and hands out no semaphore (an unsignalled one would deadlock the
// submit). The engine must run with acquireImageWithoutFence: vk::Fence has no host-signal path.
//
// With a plane source attached (a virtual window, see core::PlaneSource) every presented image is
// published as a frame, and its slot stays pinned - skipped by acquire - until the last reader lets
// go. Ownership moves by layout: a device task takes the image PresentSrc -> ShaderReadOnly before
// the frame is published, and back to PresentSrc before the slot is unpinned. Between the two the
// image is read by any number of compositor frames and written by none.
class SP_PUBLIC HeadlessSwapchain final : public core::Swapchain {
public:
	virtual ~HeadlessSwapchain();

	bool init(Device &dev, NotNull<core::Loop>, const core::SurfaceInfo &,
			const core::SwapchainConfig &, ImageInfo &&, core::PresentMode, HeadlessSurface *);

	virtual Rc<SwapchainAcquiredImage> acquire(bool lockfree, const Rc<core::Fence> &fence,
			Status &) override;

	virtual Status present(core::DeviceQueue *queue, core::ImageStorage *,
			const core::PresentInfo &) override;

	virtual bool isPresentQueueRequired() const override { return false; }

	virtual void invalidateImage(const core::ImageStorage *image, bool release) override;
	virtual void invalidateImage(uint32_t, bool release) override;

	virtual Rc<core::ImageView> makeView(const Rc<core::ImageObject> &,
			const ImageViewInfo &) override;

	virtual Rc<core::Semaphore> acquireSemaphore() override;
	virtual bool releaseSemaphore(Rc<core::Semaphore> &&) override;

	SpanView<SwapchainImageData> getImages() const { return _images; }

	// The image holding the most recently presented frame - the "current screen". Null until the
	// first frame is presented.
	core::ImageObject *getLastPresentedImage() const;
	uint32_t getLastPresentedSlot() const { return _lastPresentedIndex; }

	// Publish every presented image to `source` from now on, and hand it this generation's slots.
	void attachPlaneSource(NotNull<core::PlaneSource>);

	core::PlaneSlotTable *getSlotTable() const { return _slots; }

	// Run every view's release callback and destroy the VkImageView, up front and in one place.
	// Doing it lazily (letting the views die with the swapchain) makes their callbacks fire from
	// inside FrameCache::_autorelease teardown, which mutates the very vector being cleared.
	// Called by the engine before a swapchain is replaced, and by the destructor.
	void invalidateViews();

protected:
	using core::Object::init;

	// Move a presented image into the plane: layout task, then publish (loop thread).
	void publishPlaneFrame(uint32_t slot, Rc<Image> &&);

	Vector<SwapchainImageData> _images;
	Vector<bool> _acquired;
	uint32_t _nextIndex = 0;
	uint32_t _lastPresentedIndex = maxOf<uint32_t>();

	core::Loop *_loop = nullptr;
	Rc<core::PlaneSlotTable> _slots;
	Rc<core::PlaneSource> _planeSource;
};

// Presentation engine for a headless window.
//
// Inherits the Vulkan engine's run()/recreateSwapchain() (they only talk to PresentationWindow and
// to createSwapchain, both of which work unchanged) and replaces:
//  - createSwapchain: builds the pseudo-swapchain instead of a VkSwapchainKHR;
//  - captureScreenshot: reads back the latest published plane frame, or the last presented image,
//    instead of rendering an extra frame.
class SP_PUBLIC HeadlessPresentationEngine final : public PresentationEngine {
public:
	virtual ~HeadlessPresentationEngine() = default;

	virtual Status setFullscreenSurface(const core::MonitorId &, const core::ModeInfo &) override;

	virtual bool createSwapchain(const core::SurfaceInfo &, core::SwapchainConfig &&cfg,
			core::PresentMode presentMode, bool oldSwapchainValid) override;

	virtual void captureScreenshot(
			Function<void(const core::ImageInfoData &info, BytesView view)> &&cb) override;
};

} // namespace stappler::xenolith::vk

#endif /* XENOLITH_BACKEND_VK_XLVKHEADLESSPRESENTATION_H_ */
