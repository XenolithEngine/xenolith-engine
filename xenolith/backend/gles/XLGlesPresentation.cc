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

#include "XLGlesPresentation.h"
#include "XLGlesObject.h"
#include "XLCoreLoop.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles {

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

bool PresentationEngine::init(NotNull<core::Loop> loop, NotNull<core::Device> device,
		NotNull<core::PresentationWindow> window, core::PresentationOptions opts) {
	// Acquisition is host-side and instantaneous; an external fence carries no information.
	opts.acquireImageWithoutFence = true;

	// Rendering happens on the GPU but in a single-threaded context: starting the next frame
	// before the previous one finished does not fill an idle pipeline, it just makes two frames
	// race for the same GL state.
	opts.preStartFrame = false;

	return core::PresentationEngine::init(loop, device, window, opts);
}

bool PresentationEngine::run() {
	if (!_surface) {
		log::source().error("gles::PresentationEngine",
				"No surface bound with PresentationEngine to run()");
		return false;
	}

	auto info = _window->getSurfaceOptions(*_device, _surface);
	auto cfg = _window->selectConfig(info, false);

	if (!createSwapchain(info, move(cfg), cfg.presentMode, true)) {
		log::source().error("gles::PresentationEngine", "Fail to create swapchain");
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
		log::source().error("gles::PresentationEngine", "Presentation with config ", cfg,
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

	// Build the new swapchain BEFORE retiring the old one. The previous textures may still be
	// attached to a framebuffer the frame graph holds, and tearing them down first would pull the
	// names out from under it.
	auto newSwapchain = makeSwapchain(info, cfg, move(swapchainImageInfo), presentMode);

	auto oldSwapchain = move(_swapchain);
	if (oldSwapchain) {
		if (oldSwapchain->getAcquiredImagesCount() != 0) {
			log::source().warn("gles::PresentationEngine", "Some swapchain images still active");
		}
		oldSwapchain.get_cast<SwapchainBase>()->invalidateViews();
		oldSwapchain = nullptr;
	}

	if (!newSwapchain) {
		log::source().error("gles::PresentationEngine", "Fail to create swapchain");
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

Rc<SwapchainBase> PresentationEngine::makeSwapchain(const core::SurfaceInfo &,
		const core::SwapchainConfig &, core::ImageInfo &&, core::PresentMode) {
	// Window presentation needs an EGL surface bound to the window and a blit of the texture onto
	// it - that is M2. The headless engine overrides this with a real construction; reaching here
	// means a gapped window asked for a swapchain on a backend that cannot give one yet.
	log::source().error("gles::PresentationEngine", "Window presentation is not supported in M1");
	return nullptr;
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

	// The texture holds the last presented frame and nothing else draws into it, so this is a
	// plain read of what was on screen - never a place to draw into.
	_loop->captureImage(sp::move(cb), Rc<core::ImageObject>(image),
			core::AttachmentLayout::PresentSrc);
}

} // namespace stappler::xenolith::gles
