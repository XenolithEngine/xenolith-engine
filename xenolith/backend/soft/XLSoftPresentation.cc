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

#include "XLSoftPresentation.h"
#include "XLSoftObject.h"
#include "XLSoftInstance.h"
#include "XLCoreLoop.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft {

bool Surface::init(Instance *instance, Extent2 extent, Ref *win) {
	if (!core::Surface::init(instance, win)) {
		return false;
	}

	_extent = extent;
	return true;
}

void Surface::invalidate() { _window = nullptr; }

core::SurfaceInfo Surface::getSurfaceOptions(const core::Device &, core::FullScreenExclusiveMode,
		void *) const {
	core::SurfaceInfo info;

	// Nothing is compositing this: one image is enough to render into, and the engine's default
	// of three costs only host memory.
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

	// B8G8R8A8 first, so a capture is byte-comparable with what the Vulkan backend produces on
	// the same machine (core::getBitmap un-swizzles it on save).
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

Swapchain::~Swapchain() { invalidateViews(); }

void Swapchain::invalidateViews() {
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

bool Swapchain::init(Device &dev, NotNull<core::Loop> loop, const core::SurfaceInfo &info,
		const core::SwapchainConfig &cfg, core::ImageInfo &&swapchainImageInfo,
		core::PresentMode presentMode, Surface *surface) {
	swapchainImageInfo.usage |= core::ImageUsage::TransferSrc;

	auto imageCount = sprt::max(cfg.imageCount, uint32_t(1));
	auto viewInfo = getSwapchainImageViewInfo(swapchainImageInfo);

	_images.reserve(imageCount);
	for (uint32_t i = 0; i < imageCount; ++i) {
		auto image = Rc<Image>::create(dev, toString("SoftSwapchainImage[", i, "]"),
				core::ImageInfoData(swapchainImageInfo));
		if (!image) {
			log::source().error("soft::Swapchain", "Fail to allocate image ", i);
			return false;
		}

		auto view = Rc<ImageView>::create(dev, Rc<core::ImageObject>(image.get()), viewInfo);
		if (!view) {
			log::source().error("soft::Swapchain", "Fail to create view for image ", i);
			return false;
		}

		Map<core::ImageViewInfo, Rc<core::ImageView>> views;
		views.emplace(viewInfo, sp::move(view));

		_images.emplace_back(SwapchainImageData{sp::move(image), sp::move(views)});
	}

	_acquired.resize(imageCount, false);

	// Freshly allocated images hold nothing, so the first frame into each must repaint everything.
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

auto Swapchain::acquire(bool lockfree, const Rc<core::Fence> &fence, Status &status)
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

		// Acquisition is instantaneous, so a fence handed in here is already satisfied. Unlike
		// vk::Fence, ours can say so.
		if (fence) {
			fence->setTag("soft::Swapchain::acquire");
			static_cast<Fence *>(fence.get())->signal();
		}

		status = Status::Ok;

		return Rc<SwapchainAcquiredImage>::alloc(index, &_images[index], nullptr, this);
	}

	// Every image is in flight - the engine retries through its acquisition timer.
	status = Status::Timeout;
	return nullptr;
}

Status Swapchain::present(core::DeviceQueue *, core::ImageStorage *image,
		const core::PresentInfo &) {
	if (_invalid) {
		return Status::ErrorCancelled;
	}

	sprt::unique_lock<sprt::mutex> lock(_resourceMutex);

	if (image) {
		// getImageIndex is the underlying object's global id, not a slot number.
		auto id = image->getImageIndex();
		for (uint32_t i = 0; i < uint32_t(_images.size()); ++i) {
			if (_images[i].image && _images[i].image->getIndex() == id) {
				_acquired[i] = false;
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

void Swapchain::invalidateImage(const core::ImageStorage *image, bool) {
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

void Swapchain::invalidateImage(uint32_t index, bool) {
	sprt::unique_lock<sprt::mutex> lock(_resourceMutex);

	if (index < _acquired.size() && _acquired[index]) {
		_acquired[index] = false;
		if (_acquiredImages > 0) {
			--_acquiredImages;
		}
	}
}

Rc<core::ImageView> Swapchain::makeView(const Rc<core::ImageObject> &image,
		const core::ImageViewInfo &info) {
	auto dev = static_cast<Device *>(_object.device);
	auto view = Rc<ImageView>::create(*dev, image, info);
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

Rc<core::Semaphore> Swapchain::acquireSemaphore() {
	// Present is a bookkeeping no-op, so the frame is submitted without a signal semaphore.
	return nullptr;
}

bool Swapchain::releaseSemaphore(Rc<core::Semaphore> &&) { return true; }

core::ImageObject *Swapchain::getLastPresentedImage() const {
	if (_lastPresentedIndex >= _images.size()) {
		return nullptr;
	}
	return _images[_lastPresentedIndex].image.get();
}

bool PresentationEngine::init(NotNull<core::Loop> loop, NotNull<core::Device> device,
		NotNull<core::PresentationWindow> window, core::PresentationOptions opts) {
	// Acquisition is synchronous and host-side; an external fence carries no information.
	opts.acquireImageWithoutFence = true;
	opts.usePresentWindow = false;

	return core::PresentationEngine::init(loop, device, window, opts);
}

bool PresentationEngine::run() {
	if (!_surface) {
		log::source().error("soft::PresentationEngine",
				"No surface bound with PresentationEngine to run()");
		return false;
	}

	auto info = _window->getSurfaceOptions(*_device, _surface);
	auto cfg = _window->selectConfig(info, false);

	if (!createSwapchain(info, move(cfg), cfg.presentMode, true)) {
		log::source().error("soft::PresentationEngine", "Fail to create swapchain");
		return false;
	}

	return core::PresentationEngine::run();
}

Rc<core::ScreenInfo> PresentationEngine::getScreenInfo() const {
	auto ret = Rc<core::ScreenInfo>::create();
	ret->primaryMonitor = maxOf<uint32_t>();
	return ret;
}

Status PresentationEngine::setFullscreenSurface(const core::MonitorId &, const core::ModeInfo &) {
	return Status::ErrorNotSupported;
}

bool PresentationEngine::recreateSwapchain() {
	if (hasFlag(_deprecationFlags, core::UpdateConstraintsFlags::Finalized)) {
		return false;
	}

	_device->waitIdle();

	bool oldSwapchainValid = true;
	if (hasFlag(_deprecationFlags, core::UpdateConstraintsFlags::SwitchToNext)) {
		if (_nextSurface) {
			_surface = move(_nextSurface);
			oldSwapchainValid = false;
		}
	}

	resetFrames();

	if (hasFlag(_deprecationFlags, core::UpdateConstraintsFlags::EndOfLife)) {
		_deprecationFlags |= core::UpdateConstraintsFlags::Finalized;

		auto callbacks = sp::move(_deprecationCallbacks);
		_deprecationCallbacks.clear();

		for (auto &it : callbacks) { it(false); }

		end();

		return false;
	}

	auto fastModeSelected = hasFlag(_deprecationFlags, core::UpdateConstraintsFlags::SwitchToFastMode);
	auto info = _window->getSurfaceOptions(*_device, _surface);
	auto cfg = _window->selectConfig(info, fastModeSelected);

	if (!info.isSupported(cfg)) {
		log::source().error("soft::PresentationEngine", "Presentation with config ", cfg,
				" is not supported for ", info);
		return false;
	}

	if (cfg.extent.width == 0 || cfg.extent.height == 0) {
		return false;
	}

	auto mode = cfg.presentMode;
	if (fastModeSelected && cfg.presentModeFast != core::PresentMode::Unsupported) {
		mode = cfg.presentModeFast;
	}

	bool ret = createSwapchain(info, move(cfg), mode, oldSwapchainValid);

	_deprecationFlags = core::UpdateConstraintsFlags::None;

	auto callbacks = sp::move(_deprecationCallbacks);
	_deprecationCallbacks.clear();

	for (auto &it : callbacks) { it(true); }

	if (ret) {
		_nextPresentWindow = 0;
		_readyForNextFrame = true;
		scheduleNextImage();
	}
	return ret;
}

bool PresentationEngine::createSwapchain(const core::SurfaceInfo &info, core::SwapchainConfig &&cfg,
		core::PresentMode presentMode, bool) {
	auto dev = static_cast<Device *>(_device);

	auto surface = _surface.get_cast<Surface>();
	if (!surface) {
		log::source().error("soft::PresentationEngine", "No software surface bound");
		return false;
	}

	// The swapchain owns its images outright, so there is no handoff to the new one. Retire it
	// explicitly (views first) while everything is still alive.
	auto oldSwapchain = move(_swapchain);
	if (oldSwapchain) {
		if (oldSwapchain->getAcquiredImagesCount() != 0) {
			log::source().warn("soft::PresentationEngine", "Some swapchain images still active");
		}
		oldSwapchain.get_cast<Swapchain>()->invalidateViews();
		oldSwapchain = nullptr;
	}

	surface->setExtent(cfg.extent);

	auto swapchainImageInfo = _window->getSwapchainImageInfo(cfg);

	_swapchain = Rc<Swapchain>::create(*dev, _loop, info, cfg, move(swapchainImageInfo),
			presentMode, surface);
	if (!_swapchain) {
		log::source().error("soft::PresentationEngine", "Fail to create swapchain");
		return false;
	}

	auto newConstraints = _window->exportConstraints(_serial);
	newConstraints.extent = Extent3(cfg.extent, 1);
	newConstraints.transform = cfg.transform;

	_constraints = sp::move(newConstraints);

	Vector<uint64_t> ids;
	auto cache = _loop->getFrameCache();
	for (auto &it : _swapchain.get_cast<Swapchain>()->getImages()) {
		for (auto &iit : it.views) {
			auto id = iit.second->getIndex();
			ids.emplace_back(id);
			iit.second->setReleaseCallback([loop = _loop, cache, id] { cache->removeImageView(id); });
		}
	}

	for (auto &id : ids) { cache->addImageView(id); }

	handleSwapchainUpdated(_constraints);
	_waitForDisplayLink = false;
	_readyForNextFrame = true;
	return true;
}

void PresentationEngine::captureScreenshot(
		Function<void(const core::ImageInfoData &info, BytesView view)> &&cb) {
	auto swapchain = _swapchain.get_cast<Swapchain>();
	auto image = swapchain ? swapchain->getLastPresentedImage() : nullptr;

	if (!image) {
		// Nothing presented yet - fall back to rendering a dedicated offscreen frame.
		core::PresentationEngine::captureScreenshot(sp::move(cb));
		return;
	}

	_loop->captureImage(sp::move(cb), Rc<core::ImageObject>(image),
			core::AttachmentLayout::PresentSrc);
}

} // namespace stappler::xenolith::soft
