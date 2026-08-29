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

#include "XLGlesHeadlessPresentation.h"
#include "XLGlesObject.h"
#include "XLCoreLoop.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles {

bool HeadlessSurface::init(Instance *instance, Extent2 extent, Ref *win) {
	if (!core::Surface::init(instance, win)) {
		return false;
	}

	_extent = extent;
	return true;
}

void HeadlessSurface::invalidate() { _window = nullptr; }

core::SurfaceInfo HeadlessSurface::getSurfaceOptions(const core::Device &,
		core::FullScreenExclusiveMode, void *) const {
	core::SurfaceInfo info;

	// Nothing is compositing this: one image is enough to render into, and the engine's default
	// of three costs only GPU memory.
	info.minImageCount = 1;
	info.maxImageCount = 8;

	info.currentExtent = _extent;
	info.minImageExtent = Extent2(1, 1);
	info.maxImageExtent = _extent;
	info.maxImageArrayLayers = 1;

	info.supportedCompositeAlpha = core::CompositeAlphaFlags::Opaque;
	info.supportedTransforms = core::SurfaceTransformFlags::Identity;
	info.currentTransform = core::SurfaceTransformFlags::Identity;

	info.supportedUsageFlags = core::ImageUsage::ColorAttachment | core::ImageUsage::TransferSrc
			| core::ImageUsage::TransferDst | core::ImageUsage::Sampled;

	// R8G8B8A8 first: the loop's common format is B8G8R8A8 on Linux, but the backend maps it onto
	// RGBA8 storage anyway - offering it here keeps the negotiated swapchain in the byte order
	// the capture path reads back.
	info.formats.emplace_back(core::ImageFormat::R8G8B8A8_UNORM,
			core::ColorSpace::SRGB_NONLINEAR_KHR);
	info.formats.emplace_back(core::ImageFormat::R8_UNORM, core::ColorSpace::SRGB_NONLINEAR_KHR);

	// Nothing paces us, so every mode is "present immediately".
	info.presentModes.emplace_back(core::PresentMode::Immediate);
	info.presentModes.emplace_back(core::PresentMode::Fifo);
	info.presentModes.emplace_back(core::PresentMode::Mailbox);

	return info;
}

HeadlessSwapchain::~HeadlessSwapchain() { invalidateViews(); }

bool HeadlessSwapchain::init(Device &dev, NotNull<core::Loop>, const core::SurfaceInfo &info,
		const core::SwapchainConfig &cfg, core::ImageInfo &&swapchainImageInfo,
		core::PresentMode presentMode, HeadlessSurface *surface) {
	swapchainImageInfo.usage |= core::ImageUsage::TransferSrc;

	auto imageCount = sprt::max(cfg.imageCount, uint32_t(1));
	auto viewInfo = getSwapchainImageViewInfo(swapchainImageInfo);

	_images.reserve(imageCount);
	for (uint32_t i = 0; i < imageCount; ++i) {
		auto image = Rc<Image>::create(dev, toString("GlesSwapchainImage[", i, "]"),
				core::ImageInfoData(swapchainImageInfo), uint64_t(i));
		if (!image) {
			log::source().error("gles::HeadlessSwapchain", "Fail to allocate image ", i);
			return false;
		}

		auto view = Rc<ImageView>::create(dev, Rc<core::ImageObject>(image.get()), viewInfo);
		if (!view) {
			log::source().error("gles::HeadlessSwapchain", "Fail to create view for image ", i);
			return false;
		}

		Map<core::ImageViewInfo, Rc<core::ImageView>> views;
		views.emplace(viewInfo, sp::move(view));

		_images.emplace_back(SwapchainImageData{sp::move(image), sp::move(views)});
	}

	return finalize(dev, info, cfg, move(swapchainImageInfo), presentMode, surface);
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

		markAcquired(index);
		_nextIndex = (index + 1) % count;

		// Acquisition is instantaneous, so a fence handed in here is already satisfied. Unlike a
		// driver-backed fence, ours can say so directly.
		if (fence) {
			fence->setTag("gles::HeadlessSwapchain::acquire");
			static_cast<Fence *>(fence.get())->signal();
		}

		status = Status::Ok;

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
		auto index = findSlot(image);
		if (index != maxOf<uint32_t>()) {
			markPresented(index);
		}
	}

	if (_acquiredImages > 0) {
		--_acquiredImages;
	}
	++_presentedFrames;
	_presentTime = sp::platform::clock(ClockType::Monotonic);

	return Status::Ok;
}

bool HeadlessPresentationEngine::init(NotNull<core::Loop> loop, NotNull<core::Device> device,
		NotNull<core::PresentationWindow> window, core::PresentationOptions opts) {
	// There is no window system to pace against, so a present window would only delay a frame
	// nobody is waiting on.
	opts.usePresentWindow = false;

	return PresentationEngine::init(loop, device, window, opts);
}

Rc<SwapchainBase> HeadlessPresentationEngine::makeSwapchain(const core::SurfaceInfo &info,
		const core::SwapchainConfig &cfg, core::ImageInfo &&swapchainImageInfo,
		core::PresentMode presentMode) {
	auto dev = static_cast<Device *>(_device);

	auto surface = _surface.get_cast<HeadlessSurface>();
	if (!surface) {
		log::source().error("gles::HeadlessPresentationEngine", "No headless surface bound");
		return nullptr;
	}

	surface->setExtent(cfg.extent);

	return Rc<HeadlessSwapchain>::create(*dev, _loop, info, cfg, move(swapchainImageInfo),
			presentMode, surface);
}

} // namespace stappler::xenolith::gles
