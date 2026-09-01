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

SwapchainBase::~SwapchainBase() { }

void SwapchainBase::invalidateViews() {
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

bool SwapchainBase::finalize(Device &dev, const core::SurfaceInfo &info,
		const core::SwapchainConfig &cfg, core::ImageInfo &&swapchainImageInfo,
		core::PresentMode presentMode, core::Surface *surface) {
	_acquired.resize(_images.size(), false);

	// Freshly allocated images hold nothing, so the first frame into each must repaint everything.
	_damage.resize(uint32_t(_images.size()));

	_presentMode = presentMode;
	_imageInfo = move(swapchainImageInfo);
	_config = cfg;
	_config.imageCount = uint32_t(_images.size());
	_surface = surface;
	_surfaceInfo = info;

	return core::Object::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle, void *) { },
			core::ObjectType::Swapchain, core::ObjectHandle::zero());
}

void SwapchainBase::markAcquired(uint32_t index) {
	_acquired[index] = true;
	++_acquiredImages;
}

void SwapchainBase::markPresented(uint32_t index) {
	_acquired[index] = false;
	_lastPresentedIndex = index;
}

uint32_t SwapchainBase::findSlot(const core::ImageStorage *image) const {
	// getImageIndex is the slot the image was created with, which is also what the damage tracker
	// keys its per-slot snapshots on.
	auto id = image->getImageIndex();
	for (uint32_t i = 0; i < uint32_t(_images.size()); ++i) {
		if (_images[i].image && _images[i].image->getIndex() == id) {
			return i;
		}
	}
	return maxOf<uint32_t>();
}

void SwapchainBase::invalidateImage(const core::ImageStorage *image, bool) {
	if (!image) {
		return;
	}

	sprt::unique_lock<sprt::mutex> lock(_resourceMutex);

	auto index = findSlot(image);
	if (index != maxOf<uint32_t>() && _acquired[index]) {
		_acquired[index] = false;
		if (_acquiredImages > 0) {
			--_acquiredImages;
		}
	}
}

void SwapchainBase::invalidateImage(uint32_t index, bool) {
	sprt::unique_lock<sprt::mutex> lock(_resourceMutex);

	if (index < _acquired.size() && _acquired[index]) {
		_acquired[index] = false;
		if (_acquiredImages > 0) {
			--_acquiredImages;
		}
	}
}

Rc<core::ImageView> SwapchainBase::makeView(const Rc<core::ImageObject> &image,
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

Rc<core::Semaphore> SwapchainBase::acquireSemaphore() {
	// Nothing produced the image asynchronously, so the frame is submitted without a signal
	// semaphore to wait on.
	return nullptr;
}

bool SwapchainBase::releaseSemaphore(Rc<core::Semaphore> &&) { return true; }

core::ImageObject *SwapchainBase::getLastPresentedImage() const {
	if (_lastPresentedIndex >= _images.size()) {
		return nullptr;
	}
	return _images[_lastPresentedIndex].image.get();
}

bool Surface::init(Instance *instance, Rc<sprt::window::SoftwareSurface> &&software, Ref *win) {
	if (!software) {
		return false;
	}

	if (!core::Surface::init(instance, win)) {
		return false;
	}

	_software = move(software);
	return true;
}

void Surface::invalidate() {
	if (_software) {
		_software->invalidate();
		_software = nullptr;
	}
	_window = nullptr;
}

core::SurfaceInfo Surface::getSurfaceOptions(const core::Device &, core::FullScreenExclusiveMode,
		void *) const {
	core::SurfaceInfo info;

	info.supportedUsageFlags = core::ImageUsage::ColorAttachment | core::ImageUsage::TransferSrc
			| core::ImageUsage::TransferDst | core::ImageUsage::Sampled;
	info.maxImageArrayLayers = 1;
	info.supportedCompositeAlpha = core::CompositeAlphaFlags::Opaque;
	info.supportedTransforms = core::SurfaceTransformFlags::Identity;
	info.currentTransform = core::SurfaceTransformFlags::Identity;

	// The transport fills in the formats, image counts, present modes and extent it can honour.
	// The extent is its job and not the window's: only WaylandWindow overrides
	// NativeWindow::getSurfaceOptions (to scale by output density), and the base is a pass-through
	// - so on X nothing else would ever set it, and the swapchain would be asked for 0x0 buffers.
	return _software->getSurfaceOptions(move(info));
}

Swapchain::~Swapchain() {
	invalidateViews();
	if (_software) {
		_software->invalidate();
		_software = nullptr;
	}
}

bool Swapchain::init(Device &dev, NotNull<core::Loop>, const core::SurfaceInfo &info,
		const core::SwapchainConfig &cfg, core::ImageInfo &&swapchainImageInfo,
		core::PresentMode presentMode, Surface *surface) {
	swapchainImageInfo.usage |= core::ImageUsage::TransferSrc;

	auto imageCount = sprt::max(cfg.imageCount, uint32_t(1));

	_software = surface->getSoftwareSurface()->makeSwapchain(sprt::window::SoftwareSwapchainInfo{
		.extent = Extent2(swapchainImageInfo.extent.width, swapchainImageInfo.extent.height),
		.format = swapchainImageInfo.format,
		.imageCount = imageCount,
	});

	if (!_software) {
		log::source().error("soft::Swapchain", "Window system refused to provide ", imageCount,
				" buffers of ", swapchainImageInfo.extent.width, "x",
				swapchainImageInfo.extent.height);
		return false;
	}

	auto buffers = _software->getBuffers();
	auto viewInfo = getSwapchainImageViewInfo(swapchainImageInfo);

	_images.reserve(buffers.size());
	for (uint32_t i = 0; i < uint32_t(buffers.size()); ++i) {
		auto &buf = buffers[i];

		// The image is a view onto the window system's memory, keyed by its slot: the damage
		// tracker indexes per-slot snapshots by exactly this number, and partial redraw is only
		// correct because the buffer still holds the frame it last displayed.
		auto image = Rc<Image>::create(dev, toString("SoftSwapchainImage[", i, "]"),
				core::ImageInfoData(swapchainImageInfo), uint64_t(i), buf.data, buf.stride,
				buf.size);
		if (!image) {
			log::source().error("soft::Swapchain", "Fail to wrap buffer ", i);
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

	return finalize(dev, info, cfg, move(swapchainImageInfo), presentMode, surface);
}

auto Swapchain::acquire(bool, const Rc<core::Fence> &fence, Status &status)
		-> Rc<SwapchainAcquiredImage> {
	if (_deprecated || _invalid) {
		status = Status::ErrorCancelled;
		return nullptr;
	}

	sprt::unique_lock<sprt::mutex> lock(_resourceMutex);

	// Never blocking, regardless of what `lockfree` asks for: the window system's release event is
	// dispatched by this very thread, so waiting here would be waiting for ourselves. Timeout is
	// the expected answer when every buffer is in flight, and the engine retries on it.
	auto index = _software->acquire(status);
	if (index >= _images.size()) {
		return nullptr;
	}

	markAcquired(index);

	// Acquisition is instantaneous, so a fence handed in here is already satisfied.
	if (fence) {
		fence->setTag("soft::Swapchain::acquire");
		static_cast<Fence *>(fence.get())->signal();
	}

	status = Status::Ok;

	return Rc<SwapchainAcquiredImage>::alloc(index, &_images[index], nullptr, this);
}

Status Swapchain::present(core::DeviceQueue *, core::ImageStorage *image,
		const core::PresentInfo &info) {
	if (_invalid) {
		return Status::ErrorCancelled;
	}

	Status st = Status::Ok;

	do {
		sprt::unique_lock<sprt::mutex> lock(_resourceMutex);

		auto index = image ? findSlot(image) : maxOf<uint32_t>();
		if (index == maxOf<uint32_t>()) {
			return Status::ErrorInvalidArguemnt;
		}

		{
			// On a framebuffer window this is the copy into the scanout mapping plus the cache
			// maintenance that publishes it - the one stage whose cost is set by the window
			// system rather than by the scene.
			FrameStageTimer timer(FrameStage::Present);
			st = _software->present(index, info.damage);
		}

		markPresented(index);

		if (_acquiredImages > 0) {
			--_acquiredImages;
		}
		++_presentedFrames;
		_presentTime = sp::platform::clock(ClockType::Monotonic);

		// Close the account here rather than in runPass: a frame the damage tracker let through
		// unchanged never reaches the pass, and charging the period only to the frames that did
		// rasterize would report a frame rate the window never ran at.
		closeFrameBudget();
	} while (0);

	if (!sprt::status::isSuccessful(st)) {
		_invalid = true;
	}

	return st;
}

bool PresentationEngine::init(NotNull<core::Loop> loop, NotNull<core::Device> device,
		NotNull<core::PresentationWindow> window, core::PresentationOptions opts) {
	// Acquisition is host-side and instantaneous; an external fence carries no information.
	opts.acquireImageWithoutFence = true;

	// Rendering happens on the CPU, so starting the next frame before the previous one finished
	// does not fill an idle pipeline - it just makes two frames compete for the same cores.
	opts.preStartFrame = false;

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
	_waitForDisplayLink = false;

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

	if (hasFlag(_deprecationFlags, core::UpdateConstraintsFlags::EnableLiveResize)) {
		_liveResizeEnabled = true;
	} else if (hasFlag(_deprecationFlags, core::UpdateConstraintsFlags::DisableLiveResize)) {
		_liveResizeEnabled = false;
	}

	auto fastModeSelected = _liveResizeEnabled
			|| hasFlag(_deprecationFlags, core::UpdateConstraintsFlags::SwitchToFastMode);

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

	if (_liveResizeEnabled) {
		cfg.liveResize = true;
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

bool PresentationEngine::createSwapchain(const core::SurfaceInfo &info,
		core::SwapchainConfig &&cfg, core::PresentMode presentMode, bool) {
	auto swapchainImageInfo = _window->getSwapchainImageInfo(cfg);

	// Build the new swapchain BEFORE retiring the old one. With window-system memory the previous
	// buffers may still be held by the compositor, and the transport keeps its pool alive until
	// they come back - tearing down first would pull the mapping out from under them.
	auto newSwapchain = makeSwapchain(info, cfg, move(swapchainImageInfo), presentMode);

	auto oldSwapchain = move(_swapchain);
	if (oldSwapchain) {
		if (oldSwapchain->getAcquiredImagesCount() != 0) {
			log::source().warn("soft::PresentationEngine", "Some swapchain images still active");
		}
		oldSwapchain.get_cast<SwapchainBase>()->invalidateViews();
		oldSwapchain = nullptr;
	}

	if (!newSwapchain) {
		log::source().error("soft::PresentationEngine", "Fail to create swapchain");
		return false;
	}

	_swapchain = move(newSwapchain);

	auto newConstraints = _window->exportConstraints(_serial);
	newConstraints.extent = Extent3(cfg.extent, 1);
	newConstraints.transform = cfg.transform;

	_constraints = sp::move(newConstraints);

	Vector<uint64_t> ids;
	auto cache = _loop->getFrameCache();
	for (auto &it : _swapchain.get_cast<SwapchainBase>()->getImages()) {
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

Rc<SwapchainBase> PresentationEngine::makeSwapchain(const core::SurfaceInfo &info,
		const core::SwapchainConfig &cfg, core::ImageInfo &&swapchainImageInfo,
		core::PresentMode presentMode) {
	auto dev = static_cast<Device *>(_device);

	auto surface = _surface.get_cast<Surface>();
	if (!surface) {
		log::source().error("soft::PresentationEngine", "No software surface bound");
		return nullptr;
	}

	return Rc<Swapchain>::create(*dev, _loop, info, cfg, move(swapchainImageInfo), presentMode,
			surface);
}

void PresentationEngine::captureScreenshot(
		Function<void(const core::ImageInfoData &info, BytesView view)> &&cb) {
	auto swapchain = _swapchain.get_cast<SwapchainBase>();
	auto image = swapchain ? swapchain->getLastPresentedImage() : nullptr;

	if (!image) {
		// Nothing presented yet - fall back to rendering a dedicated offscreen frame.
		core::PresentationEngine::captureScreenshot(sp::move(cb));
		return;
	}

	// The buffer stays mapped and readable while the window system displays it, so this is a plain
	// read of what is on screen - never a place to draw into.
	_loop->captureImage(sp::move(cb), Rc<core::ImageObject>(image),
			core::AttachmentLayout::PresentSrc);
}

} // namespace stappler::xenolith::soft
