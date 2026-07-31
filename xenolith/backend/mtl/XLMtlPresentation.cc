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

#include "XLMtlPresentation.h"
#include "XLMtlPipeline.h"
#include "XLMtlLoop.h"
#include "XLCoreFrameCache.h"

#include <TargetConditionals.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::mtl {

Surface::~Surface() { invalidate(); }

bool Surface::init(Instance *instance, void *layer, Ref *window) {
	if (!layer) {
		return false;
	}

	_instance = instance;
	_window = window;
	_layer = retainHandle(bridgeHandle<CAMetalLayer *>(layer));
	return true;
}

void Surface::invalidate() {
	releaseHandle(_layer);
	_layer = nullptr;
	_window = nullptr;
}

core::SurfaceInfo Surface::getSurfaceOptions(const core::Device &dev, core::FullScreenExclusiveMode,
		void *) const {
	core::SurfaceInfo ret;

	// CAMetalLayer does not expose capability queries; limits are nominal.
	// The current extent comes from the layer itself (as MoltenVK reports it
	// for VkSurfaceKHR): explicit drawableSize wins, otherwise scaled bounds;
	// an unconfigured (headless) layer keeps the "undefined" marker and the
	// PresentationWindow must patch it
	ret.minImageCount = 2;
	ret.maxImageCount = 3; // CAMetalLayer maximumDrawableCount range is 2..3
	ret.currentExtent = Extent2(maxOf<uint32_t>(), maxOf<uint32_t>());

	auto layer = getLayer();
	CGSize drawableSize = layer.drawableSize;
	if (drawableSize.width < 1.0 || drawableSize.height < 1.0) {
		const CGSize bounds = layer.bounds.size;
		const CGFloat scale = layer.contentsScale > 0.0 ? layer.contentsScale : 1.0;
		drawableSize = CGSizeMake(bounds.width * scale, bounds.height * scale);
	}
	if (drawableSize.width >= 1.0 && drawableSize.height >= 1.0) {
		ret.currentExtent = Extent2(uint32_t(drawableSize.width), uint32_t(drawableSize.height));
	}
	ret.minImageExtent = Extent2(1, 1);
	ret.maxImageExtent = Extent2(16'384, 16'384);
	ret.maxImageArrayLayers = 1;
	ret.supportedCompositeAlpha = core::CompositeAlphaFlags::Opaque;
	ret.supportedTransforms = core::SurfaceTransformFlags::Identity;
	ret.currentTransform = core::SurfaceTransformFlags::Identity;

	ret.supportedUsageFlags = core::ImageUsage::ColorAttachment | core::ImageUsage::TransferSrc
			| core::ImageUsage::TransferDst;

	ret.formats.emplace_back(
			sprt::pair(core::ImageFormat::B8G8R8A8_UNORM, core::ColorSpace::SRGB_NONLINEAR_KHR));
	ret.formats.emplace_back(
			sprt::pair(core::ImageFormat::B8G8R8A8_SRGB, core::ColorSpace::SRGB_NONLINEAR_KHR));
	ret.formats.emplace_back(sprt::pair(core::ImageFormat::R16G16B16A16_SFLOAT,
			core::ColorSpace::SRGB_NONLINEAR_KHR));

	// displaySyncEnabled=YES -> Fifo, NO -> Immediate (macOS only)
	ret.presentModes.emplace_back(core::PresentMode::Fifo);
#if !TARGET_OS_IPHONE
	ret.presentModes.emplace_back(core::PresentMode::Immediate);
#endif

	return ret;
}

Swapchain::~Swapchain() {
	for (uint32_t i = 0; i < uint32_t(_images.size()); ++i) { releaseSlot(i); }
	_images.clear();
	_drawables.clear();
}

bool Swapchain::init(Device &dev, NotNull<core::Loop> loop, const core::SurfaceInfo &info,
		const core::SwapchainConfig &cfg, core::ImageInfo &&swapchainImageInfo,
		core::PresentMode presentMode, Surface *surface) {
	_device = &dev;
	_loop = loop;
	_surface = surface;
	_surfaceInfo = info;
	_config = cfg;
	_presentMode = presentMode;
	_imageInfo = swapchainImageInfo;
	_swapchainImageInfo = move(swapchainImageInfo);

	_images.resize(SlotCount);
	_drawables.resize(SlotCount, nullptr);
	_slotViews.resize(SlotCount);

	auto layer = surface->getLayer();
	layer.device = dev.getDevice();
	layer.pixelFormat = getMTLPixelFormat(cfg.imageFormat);
	layer.drawableSize = CGSizeMake(cfg.extent.width, cfg.extent.height);
	layer.maximumDrawableCount = sprt::min(sprt::max(cfg.imageCount, 2U), 3U);
	// framebufferOnly forbids texture views and blits from the drawable; the
	// engine needs both (framebuffer cache views, capture readback)
	layer.framebufferOnly = NO;
#if !TARGET_OS_IPHONE
	layer.displaySyncEnabled = presentMode != core::PresentMode::Immediate;
#endif

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

	@autoreleasepool {
		// blocks while all drawables are in flight (frame pacing); returns nil
		// after the layer's timeout - the engine retries via its timer
		id<CAMetalDrawable> drawable = [_surface.get_cast<Surface>()->getLayer() nextDrawable];
		if (!drawable) {
			status = Status::Timeout;
			return nullptr;
		}

		auto index = _nextIndex % SlotCount;
		++_nextIndex;

		releaseSlot(index);

		auto imageInfo = core::ImageInfoData(_swapchainImageInfo);

		auto &slot = _images[index];
		slot.image = Rc<Image>::create(*_device, drawable.texture, "SwapchainImage", imageInfo);
		if (!slot.image) {
			status = Status::ErrorCancelled;
			return nullptr;
		}

		_drawables[index] = retainHandle(drawable);

		sprt::unique_lock<sprt::mutex> lock(_resourceMutex);
		++_acquiredImages;

		if (fence) {
			fence->setTag("mtl::Swapchain::acquire");
			// acquisition is synchronous, signal the fence in place
			static_cast<Fence *>(fence.get())->signal();
		}

		status = Status::Ok;

		return Rc<SwapchainAcquiredImage>::alloc(index, &slot, acquireSemaphore(), this);
	}
}

Status Swapchain::present(core::DeviceQueue &, core::ImageStorage *image,
		const core::PresentInfo &) {
	if (_invalid) {
		return Status::ErrorCancelled;
	}

	if (!image) {
		return Status::ErrorInvalidArguemnt;
	}

	auto img = image->getImage();

	uint32_t index = maxOf<uint32_t>();
	for (uint32_t i = 0; i < uint32_t(_images.size()); ++i) {
		if (_images[i].image == img) {
			index = i;
			break;
		}
	}

	if (index == maxOf<uint32_t>() || !_drawables[index]) {
		return Status::ErrorInvalidArguemnt;
	}

	@autoreleasepool {
		// present through a command buffer of the device queue: MTLCommandQueue
		// executes buffers in submission order, so presentation happens after
		// the frame's rendering (already committed by QueuePassHandle::submit)
		auto drawable = bridgeHandle<id<CAMetalDrawable>>(_drawables[index]);
		id<MTLCommandBuffer> commands = [_device->getQueue() commandBuffer];
		[commands presentDrawable:drawable];
		[commands commit];
	}

	sprt::unique_lock<sprt::mutex> lock(_resourceMutex);

	// the drawable is consumed by present; drop slot references immediately,
	// so the framebuffer cache releases its views and the layer can recycle
	// the drawable (the presentation system keeps it alive until onscreen)
	static_cast<Image *>(_images[index].image.get())->invalidateTexture();
	releaseSlot(index);

	if (_acquiredImages > 0) {
		--_acquiredImages;
	}
	++_presentedFrames;
	_presentTime = sp::platform::clock(ClockType::Monotonic);

	return Status::Ok;
}

void Swapchain::invalidateImage(const core::ImageStorage *image, bool) {
	if (image) {
		invalidateImage(uint32_t(image->getImageIndex()), false);
	}
}

void Swapchain::invalidateImage(uint32_t index, bool) {
	sprt::unique_lock<sprt::mutex> lock(_resourceMutex);
	if (index < _images.size() && _images[index].image) {
		// an acquired-but-not-presented drawable must be dropped back to the
		// layer's pool immediately, otherwise it stays checked out until the
		// slot is reused SlotCount frames later - starving nextDrawable and
		// capping the frame rate (unpresented drawables are returned to the
		// pool on release, no present needed)
		static_cast<Image *>(_images[index].image.get())->invalidateTexture();
		releaseSlot(index);
		if (_acquiredImages > 0) {
			--_acquiredImages;
		}
	}
}

Rc<core::ImageView> Swapchain::makeView(const Rc<core::ImageObject> &image,
		const core::ImageViewInfo &info) {
	auto view = Rc<ImageView>::create(*_device, image, info);
	if (!view) {
		return nullptr;
	}

	auto cache = _loop->getFrameCache();
	cache->addImageView(view->getIndex());
	view->setReleaseCallback([loop = Rc<core::Loop>(_loop), id = view->getIndex()] {
		loop->performOnThread([loop, id] { loop->getFrameCache()->removeImageView(id); }, nullptr,
				true);
	});

	for (uint32_t i = 0; i < uint32_t(_images.size()); ++i) {
		if (_images[i].image == image) {
			_slotViews[i].emplace_back(view);
			break;
		}
	}

	return view;
}

Rc<core::Semaphore> Swapchain::acquireSemaphore() { return Rc<Semaphore>::create(*_device); }

bool Swapchain::releaseSemaphore(Rc<core::Semaphore> &&) { return true; }

void Swapchain::releaseSlot(uint32_t index) {
	for (auto &it : _slotViews[index]) { it->runReleaseCallback(); }
	_slotViews[index].clear();

	auto &slot = _images[index];
	slot.views.clear();
	slot.image = nullptr;

	releaseHandle(_drawables[index]);
	_drawables[index] = nullptr;
}

bool PresentationEngine::init(NotNull<core::Loop> loop, NotNull<core::Device> device,
		NotNull<core::PresentationWindow> window, core::PresentationOptions opts) {
	// nextDrawable is synchronous, external fence has no meaning
	opts.acquireImageWithoutFence = true;
	// CAMetalLayer paces the frame loop itself (nextDrawable blocks while all
	// drawables are in flight, presentation is vsync-bound by displaySync)
	opts.usePresentWindow = false;

	// remember the window's requested barrier: createSwapchain toggles it per
	// present mode (Immediate must run unbounded)
	_windowFollowDisplayLinkBarrier = opts.followDisplayLinkBarrier;

	return core::PresentationEngine::init(loop, device, window, opts);
}

bool PresentationEngine::run() {
	if (!_surface) {
		log::source().error("mtl::PresentationEngine",
				"No surface bound with PresentationEngine to run()");
		return false;
	}

	auto info = _window->getSurfaceOptions(*_device, _surface);
	auto cfg = _window->selectConfig(info, false);

	if (!createSwapchain(info, move(cfg), cfg.presentMode, true)) {
		log::source().error("mtl::PresentationEngine", "Fail to create swapchain");
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

	// drain in-flight frames before reconfiguring the layer
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

	auto fastModeSelected =
			hasFlag(_deprecationFlags, core::UpdateConstraintsFlags::SwitchToFastMode);
	auto info = _window->getSurfaceOptions(*_device, _surface);
	auto cfg = _window->selectConfig(info, fastModeSelected);

	if (!info.isSupported(cfg)) {
		log::source().error("mtl::PresentationEngine", "Presentation with config ", cfg,
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

	auto swapchainImageInfo = _window->getSwapchainImageInfo(cfg);

	auto oldSwapchain = move(_swapchain);

	if (oldSwapchain && oldSwapchain->getAcquiredImagesCount() != 0) {
		log::source().warn("mtl::PresentationEngine", "Some swapchain images still active");
	}

	_swapchain = Rc<Swapchain>::create(*dev, _loop.get(), info, cfg, move(swapchainImageInfo),
			presentMode, _surface.get_cast<Surface>());

	if (!_swapchain) {
		log::source().error("mtl::PresentationEngine", "Fail to create swapchain");
		return false;
	}

	// Pacing depends on the present mode. The macOS window requests
	// followDisplayLinkBarrier (the next frame is gated on the CADisplayLink
	// tick), which caps the loop at the display refresh - correct for Fifo,
	// but Immediate must run unbounded. Drop the barrier for Immediate so each
	// present schedules the next frame right away (the layer's
	// displaySyncEnabled=NO already lets present skip the vsync wait); restore
	// it for any vsync-locked mode.
	_options.followDisplayLinkBarrier =
			_windowFollowDisplayLinkBarrier && presentMode != core::PresentMode::Immediate;

	auto newConstraints = _window->exportConstraints(_serial);
	newConstraints.extent = Extent3(cfg.extent, 1);
	newConstraints.transform = cfg.transform;

	_constraints = sp::move(newConstraints);

	handleSwapchainUpdated(_constraints);
	_readyForNextFrame = true;
	return true;
}

} // namespace stappler::xenolith::mtl
