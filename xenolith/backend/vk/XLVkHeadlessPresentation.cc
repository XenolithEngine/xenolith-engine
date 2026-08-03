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

#include "XLVkHeadlessPresentation.h"
#include "XLVkDevice.h"
#include "XLVkAllocator.h"
#include "XLVkLoop.h"
#include "XLCoreFrameCache.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

HeadlessSurface::~HeadlessSurface() { }

bool HeadlessSurface::init(Instance *instance, Extent2 extent, Ref *win) {
	if (!core::Surface::init(instance, win)) {
		return false;
	}

	_extent = extent;
	return true;
}

void HeadlessSurface::invalidate() { _window = nullptr; }

core::SurfaceInfo HeadlessSurface::getSurfaceOptions(const core::Device &, //
		core::FullScreenExclusiveMode, void *) const {
	core::SurfaceInfo info;

	// A pseudo-swapchain is not bounded by a compositor: one image is enough to render, and the
	// engine's default of three is well within what we can allocate.
	info.minImageCount = 1;
	info.maxImageCount = 8;

	info.currentExtent = _extent;
	info.minImageExtent = Extent2(1, 1);
	info.maxImageExtent = _extent;
	info.maxImageArrayLayers = 1;

	info.supportedCompositeAlpha = core::CompositeAlphaFlags::Opaque;
	info.supportedTransforms = core::SurfaceTransformFlags::Identity;
	info.currentTransform = core::SurfaceTransformFlags::Identity;

	// TransferSrc is the whole point: whatever is rendered must stay readable through
	// Loop::captureImage without an extra offscreen pass.
	info.supportedUsageFlags = core::ImageUsage::ColorAttachment | core::ImageUsage::TransferSrc
			| core::ImageUsage::TransferDst | core::ImageUsage::Sampled;

	// B8G8R8A8 first, so a capture matches what a real swapchain would have produced on the same
	// machine (core::getBitmap already un-swizzles it).
	info.formats.emplace_back(core::ImageFormat::B8G8R8A8_UNORM,
			core::ColorSpace::SRGB_NONLINEAR_KHR);
	info.formats.emplace_back(core::ImageFormat::R8G8B8A8_UNORM,
			core::ColorSpace::SRGB_NONLINEAR_KHR);

	// Nothing paces us, so every mode is "present immediately".
	info.presentModes.emplace_back(core::PresentMode::Immediate);
	info.presentModes.emplace_back(core::PresentMode::Fifo);
	info.presentModes.emplace_back(core::PresentMode::Mailbox);

	return info;
}

HeadlessSwapchain::~HeadlessSwapchain() { invalidateViews(); }

void HeadlessSwapchain::invalidateViews() {
	// Mirrors core::Swapchain::SwapchainData::invalidate: the frame cache is told the view is gone
	// while everything is still alive, and the framebuffers that referenced it are retired in an
	// orderly way. A Framebuffer holding an Rc to the view keeps the object itself alive, which is
	// what makes this safe to do before the images are dropped.
	for (auto &it : _images) {
		for (auto &v : it.views) {
			if (v.second) {
				v.second->runReleaseCallback();
				v.second->invalidate();
				v.second = nullptr;
			}
		}
		it.views.clear();
	}
}

bool HeadlessSwapchain::init(Device &dev, NotNull<core::Loop> loop, const core::SurfaceInfo &info,
		const core::SwapchainConfig &cfg, ImageInfo &&swapchainImageInfo,
		core::PresentMode presentMode, HeadlessSurface *surface) {
	// AppWindow asks for ColorAttachment [| TransferDst]; TransferSrc is what turns these into
	// always-capturable buffers.
	swapchainImageInfo.usage |= core::ImageUsage::TransferSrc;

	auto imageCount = sprt::max(cfg.imageCount, uint32_t(1));
	auto viewInfo = getSwapchainImageViewInfo(swapchainImageInfo);

	_images.reserve(imageCount);
	for (uint32_t i = 0; i < imageCount; ++i) {
		auto image = dev.getAllocator()->spawnPersistent(AllocationUsage::DeviceLocal,
				toString("HeadlessSwapchainImage[", i, "]"), swapchainImageInfo, false);
		if (!image) {
			log::source().error("vk::HeadlessSwapchain", "Fail to allocate image ", i);
			return false;
		}

		auto view = Rc<ImageView>::create(dev, image.get(), viewInfo);
		if (!view) {
			log::source().error("vk::HeadlessSwapchain", "Fail to create view for image ", i);
			return false;
		}

		Map<ImageViewInfo, Rc<core::ImageView>> views;
		views.emplace(viewInfo, sp::move(view));

		_images.emplace_back(SwapchainImageData{sp::move(image), sp::move(views)});
	}

	_acquired.resize(imageCount, false);

	// The images are freshly allocated with undefined content, so the first frame into each index
	// must report full damage. Note that the image indexes here are allocator-assigned object ids,
	// not 0..N-1, so computeRedrawArea always falls back to a full redraw - which is exactly right
	// for images nobody is compositing incrementally.
	_damage.resize(imageCount);

	_presentMode = presentMode;
	_imageInfo = move(swapchainImageInfo);
	_config = cfg;
	_config.imageCount = imageCount;
	_surface = surface;
	_surfaceInfo = info;

	return core::Object::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle, void *) { },
			core::ObjectType::Swapchain, core::ObjectHandle::zero());
}

auto HeadlessSwapchain::acquire(bool lockfree, const Rc<core::Fence> &fence, Status &status)
		-> Rc<SwapchainAcquiredImage> {
	if (_deprecated || _invalid) {
		status = Status::ErrorCancelled;
		return nullptr;
	}

	sprt::unique_lock<sprt::mutex> lock(_resourceMutex);

	auto count = uint32_t(_images.size());
	for (uint32_t i = 0; i < count; ++i) {
		auto index = (_nextIndex + i) % count;
		if (_acquired[index]) {
			continue;
		}

		_acquired[index] = true;
		_nextIndex = (index + 1) % count;
		++_acquiredImages;

		status = Status::Ok;

		// No wait semaphore: acquisition is instantaneous and nothing on the GPU produced this
		// image. Handing out a freshly created (unsignalled) binary semaphore here would put it in
		// pWaitSemaphores and hang the submit forever.
		return Rc<SwapchainAcquiredImage>::alloc(index, &_images[index], nullptr, this);
	}

	// Every image is in flight - the engine retries through its acquisition timer.
	status = Status::Timeout;
	return nullptr;
}

Status HeadlessSwapchain::present(core::DeviceQueue *, core::ImageStorage *image,
		const core::PresentInfo &) {
	if (_invalid) {
		return Status::ErrorCancelled;
	}

	sprt::unique_lock<sprt::mutex> lock(_resourceMutex);

	if (image) {
		// ImageStorage::getImageIndex is the underlying ImageObject's global id, not a slot
		// number, so the slot has to be looked up rather than indexed
		auto id = image->getImageIndex();
		for (uint32_t i = 0; i < uint32_t(_images.size()); ++i) {
			if (_images[i].image && _images[i].image->getIndex() == id) {
				_acquired[i] = false;
				// This is now "what is on screen": the screenshot command reads it back directly
				// instead of rendering another frame.
				_lastPresentedIndex = i;
				break;
			}
		}
	}

	if (_acquiredImages > 0) {
		--_acquiredImages;
	}
	++_presentedFrames;
	_presentTime = sp::platform::clock(ClockType::Monotonic);

	return Status::Ok;
}

void HeadlessSwapchain::invalidateImage(const core::ImageStorage *image, bool) {
	if (!image) {
		return;
	}

	sprt::unique_lock<sprt::mutex> lock(_resourceMutex);

	auto id = image->getImageIndex();
	for (uint32_t i = 0; i < uint32_t(_images.size()); ++i) {
		if (_images[i].image && _images[i].image->getIndex() == id) {
			if (_acquired[i]) {
				_acquired[i] = false;
				if (_acquiredImages > 0) {
					--_acquiredImages;
				}
			}
			break;
		}
	}
}

void HeadlessSwapchain::invalidateImage(uint32_t index, bool) {
	sprt::unique_lock<sprt::mutex> lock(_resourceMutex);

	if (index < _acquired.size() && _acquired[index]) {
		_acquired[index] = false;
		if (_acquiredImages > 0) {
			--_acquiredImages;
		}
	}
}

Rc<core::ImageView> HeadlessSwapchain::makeView(const Rc<core::ImageObject> &image,
		const ImageViewInfo &info) {
	auto dev = static_cast<Device *>(_object.device);
	auto view = Rc<ImageView>::create(*dev, static_cast<Image *>(image.get()), info);
	if (!view) {
		return nullptr;
	}

	for (auto &it : _images) {
		if (it.image == image) {
			it.views.emplace(info, view);
			break;
		}
	}

	return view;
}

Rc<core::Semaphore> HeadlessSwapchain::acquireSemaphore() {
	// Nothing waits on the render's completion here - present is a bookkeeping no-op - so the
	// frame is submitted without a signal semaphore at all.
	return nullptr;
}

bool HeadlessSwapchain::releaseSemaphore(Rc<core::Semaphore> &&) { return true; }

core::ImageObject *HeadlessSwapchain::getLastPresentedImage() const {
	if (_lastPresentedIndex >= _images.size()) {
		return nullptr;
	}
	return _images[_lastPresentedIndex].image.get();
}

Status HeadlessPresentationEngine::setFullscreenSurface(const core::MonitorId &,
		const core::ModeInfo &) {
	return Status::ErrorNotSupported;
}

bool HeadlessPresentationEngine::createSwapchain(const core::SurfaceInfo &info,
		core::SwapchainConfig &&cfg, core::PresentMode presentMode, bool) {
	auto dev = static_cast<Device *>(_device);

	auto surface = _surface.get_cast<HeadlessSurface>();
	if (!surface) {
		log::source().error("vk::HeadlessPresentationEngine", "No headless surface bound");
		return false;
	}

	// The swapchain owns its images outright, so there is no old-swapchain handoff to do. Retire it
	// explicitly (views first, see invalidateViews) so the frame cache is unwound while everything
	// is still alive, then let it go.
	auto oldSwapchain = move(_swapchain);
	if (oldSwapchain) {
		if (oldSwapchain->getAcquiredImagesCount() != 0) {
			log::source().warn("vk::HeadlessPresentationEngine",
					"Some swapchain images still active");
		}
		oldSwapchain.get_cast<HeadlessSwapchain>()->invalidateViews();
		oldSwapchain = nullptr;
	}

	surface->setExtent(cfg.extent);

	auto swapchainImageInfo = _window->getSwapchainImageInfo(cfg);

	_swapchain = Rc<HeadlessSwapchain>::create(*dev, _loop, info, cfg, move(swapchainImageInfo),
			presentMode, surface);
	if (!_swapchain) {
		log::source().error("vk::HeadlessPresentationEngine", "Fail to create swapchain");
		return false;
	}

	auto newConstraints = _window->exportConstraints(_serial);
	newConstraints.extent = Extent3(cfg.extent, 1);
	newConstraints.transform = cfg.transform;

	_constraints = sp::move(newConstraints);

	Vector<uint64_t> ids;
	auto cache = _loop->getFrameCache();
	for (auto &it : _swapchain.get_cast<HeadlessSwapchain>()->getImages()) {
		for (auto &iit : it.views) {
			auto id = iit.second->getIndex();
			ids.emplace_back(id);
			// the loop keeps the frame cache alive for as long as a view can outlive us
			iit.second->setReleaseCallback(
					[loop = _loop, cache, id] { cache->removeImageView(id); });
		}
	}

	for (auto &id : ids) { cache->addImageView(id); }

	handleSwapchainUpdated(_constraints);
	_waitForDisplayLink = false;
	_readyForNextFrame = true;
	return true;
}

void HeadlessPresentationEngine::captureScreenshot(
		Function<void(const core::ImageInfoData &info, BytesView view)> &&cb) {
	auto swapchain = _swapchain.get_cast<HeadlessSwapchain>();
	auto image = swapchain ? swapchain->getLastPresentedImage() : nullptr;

	if (!image) {
		// Nothing has been presented yet - fall back to rendering a dedicated offscreen frame.
		core::PresentationEngine::captureScreenshot(sp::move(cb));
		return;
	}

	// The pseudo-swapchain image still holds the last presented frame and carries TransferSrc, so
	// it can be read back as-is. A swapchain image's render pass leaves it in PresentSrc (the
	// PresentSrc -> TransferSrcOptimal remap in FrameQueue only applies to non-swapchain images).
	_loop->captureImage(sp::move(cb), Rc<core::ImageObject>(image),
			core::AttachmentLayout::PresentSrc);
}

} // namespace stappler::xenolith::vk
