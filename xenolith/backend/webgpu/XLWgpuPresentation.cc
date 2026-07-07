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

#include "XLWgpuPresentation.h"
#include "XLWgpuLoop.h"
#include "XLCoreFrameCache.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::webgpu {

core::ImageFormat getCoreFormat(WGPUTextureFormat fmt) {
	switch (fmt) {
	case WGPUTextureFormat_R8Unorm: return core::ImageFormat::R8_UNORM; break;
	case WGPUTextureFormat_RG8Unorm: return core::ImageFormat::R8G8_UNORM; break;
	case WGPUTextureFormat_RGBA8Unorm: return core::ImageFormat::R8G8B8A8_UNORM; break;
	case WGPUTextureFormat_RGBA8UnormSrgb: return core::ImageFormat::R8G8B8A8_SRGB; break;
	case WGPUTextureFormat_BGRA8Unorm: return core::ImageFormat::B8G8R8A8_UNORM; break;
	case WGPUTextureFormat_BGRA8UnormSrgb: return core::ImageFormat::B8G8R8A8_SRGB; break;
	case WGPUTextureFormat_RGBA16Float: return core::ImageFormat::R16G16B16A16_SFLOAT; break;
	case WGPUTextureFormat_RGBA32Float: return core::ImageFormat::R32G32B32A32_SFLOAT; break;
	default: break;
	}
	return core::ImageFormat::Undefined;
}

core::PresentMode getCorePresentMode(WGPUPresentMode mode) {
	switch (mode) {
	case WGPUPresentMode_Fifo: return core::PresentMode::Fifo; break;
	case WGPUPresentMode_FifoRelaxed: return core::PresentMode::FifoRelaxed; break;
	case WGPUPresentMode_Immediate: return core::PresentMode::Immediate; break;
	case WGPUPresentMode_Mailbox: return core::PresentMode::Mailbox; break;
	default: break;
	}
	return core::PresentMode::Unsupported;
}

WGPUPresentMode getWGPUPresentMode(core::PresentMode mode) {
	switch (mode) {
	case core::PresentMode::Fifo: return WGPUPresentMode_Fifo; break;
	case core::PresentMode::FifoRelaxed: return WGPUPresentMode_FifoRelaxed; break;
	case core::PresentMode::Immediate: return WGPUPresentMode_Immediate; break;
	case core::PresentMode::Mailbox: return WGPUPresentMode_Mailbox; break;
	case core::PresentMode::Unsupported: break;
	}
	return WGPUPresentMode_Undefined;
}

Surface::~Surface() { invalidate(); }

bool Surface::init(Instance *instance, WGPUSurface surface, Ref *window) {
	if (!surface) {
		return false;
	}
	_instance = instance;
	_window = window;
	_surface = surface;
	return true;
}

void Surface::invalidate() {
	if (_surface) {
		wgpuSurfaceUnconfigure(_surface);
		wgpuSurfaceRelease(_surface);
		_surface = nullptr;
	}
}

core::SurfaceInfo Surface::getSurfaceOptions(const core::Device &dev,
		core::FullScreenExclusiveMode, void *) const {
	core::SurfaceInfo ret;

	auto &adapterData = static_cast<const Device &>(dev).getAdapterData();

	WGPUSurfaceCapabilities caps;
	if (wgpuSurfaceGetCapabilities(_surface, adapterData.adapter, &caps) != WGPUStatus_Success) {
		log::source().error("webgpu::Surface", "Fail to get surface capabilities");
		return ret;
	}

	// WebGPU does not expose image count or extent limits; extent is defined
	// by the window (PresentationWindow patches it), image counts are nominal
	ret.minImageCount = 2;
	ret.maxImageCount = 3;
	ret.currentExtent = Extent2(maxOf<uint32_t>(), maxOf<uint32_t>());
	ret.minImageExtent = Extent2(1, 1);
	ret.maxImageExtent = Extent2(16'384, 16'384);
	ret.maxImageArrayLayers = 1;
	ret.supportedCompositeAlpha = core::CompositeAlphaFlags::Opaque;
	ret.supportedTransforms = core::SurfaceTransformFlags::Identity;
	ret.currentTransform = core::SurfaceTransformFlags::Identity;

	ret.supportedUsageFlags = core::ImageUsage::ColorAttachment;
	if (caps.usages & WGPUTextureUsage_CopySrc) {
		ret.supportedUsageFlags |= core::ImageUsage::TransferSrc;
	}
	if (caps.usages & WGPUTextureUsage_CopyDst) {
		ret.supportedUsageFlags |= core::ImageUsage::TransferDst;
	}
	if (caps.usages & WGPUTextureUsage_TextureBinding) {
		ret.supportedUsageFlags |= core::ImageUsage::Sampled;
	}

	for (size_t i = 0; i < caps.formatCount; ++i) {
		auto fmt = getCoreFormat(caps.formats[i]);
		if (fmt != core::ImageFormat::Undefined) {
			ret.formats.emplace_back(sprt::pair(fmt, core::ColorSpace::SRGB_NONLINEAR_KHR));
		}
	}

	for (size_t i = 0; i < caps.presentModeCount; ++i) {
		auto mode = getCorePresentMode(caps.presentModes[i]);
		// wgpu-native v29: Mailbox/Immediate break the surface texture
		// lifecycle (the texture acquired after a present is reported
		// destroyed); expose only FIFO modes until fixed upstream
		if (mode == core::PresentMode::Mailbox || mode == core::PresentMode::Immediate) {
			continue;
		}
		if (mode != core::PresentMode::Unsupported) {
			ret.presentModes.emplace_back(mode);
		}
	}

	wgpuSurfaceCapabilitiesFreeMembers(caps);

	return ret;
}

Swapchain::~Swapchain() {
	for (uint32_t i = 0; i < uint32_t(_images.size()); ++i) { releaseSlot(i); }
	_images.clear();
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
	_slotViews.resize(SlotCount);

	WGPUSurfaceConfiguration surfaceConfig = WGPU_SURFACE_CONFIGURATION_INIT;
	surfaceConfig.device = dev.getDevice();
	surfaceConfig.format = getWGPUFormat(cfg.imageFormat);
	surfaceConfig.usage = getWGPUTextureUsage(_swapchainImageInfo.usage);
	surfaceConfig.width = cfg.extent.width;
	surfaceConfig.height = cfg.extent.height;
	surfaceConfig.presentMode = getWGPUPresentMode(presentMode);
	surfaceConfig.alphaMode = WGPUCompositeAlphaMode_Opaque;

	wgpuSurfaceConfigure(surface->getSurface(), &surfaceConfig);

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

	// WebGPU allows single current texture; engine will retry via acquisition timer
	if (_acquiredImages > 0) {
		status = Status::Timeout;
		return nullptr;
	}

	WGPUSurfaceTexture surfaceTexture = WGPU_SURFACE_TEXTURE_INIT;
	wgpuSurfaceGetCurrentTexture(_surface.get_cast<Surface>()->getSurface(), &surfaceTexture);

	switch (surfaceTexture.status) {
	case WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal: _deprecated = true; [[fallthrough]];
	case WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal: {
		auto index = _nextIndex % SlotCount;
		++_nextIndex;

		releaseSlot(index);

		auto imageInfo = core::ImageInfoData(_swapchainImageInfo);

		auto &slot = _images[index];
		slot.image = Rc<Image>::create(*_device, surfaceTexture.texture, "SwapchainImage",
				imageInfo);
		if (!slot.image) {
			wgpuTextureRelease(surfaceTexture.texture);
			status = Status::ErrorCancelled;
			return nullptr;
		}

		sprt::unique_lock<sprt::mutex> lock(_resourceMutex);
		++_acquiredImages;

		if (fence) {
			fence->setTag("webgpu::Swapchain::acquire");
			// acquisition is synchronous, signal the fence in place
			static_cast<Fence *>(fence.get())->signal();
		}

		status = surfaceTexture.status == WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal
				? Status::Ok
				: Status::Suboptimal;

		return Rc<SwapchainAcquiredImage>::alloc(index, &slot, acquireSemaphore(), this);
		break;
	}
	case WGPUSurfaceGetCurrentTextureStatus_Timeout: status = Status::Timeout; break;
	case WGPUSurfaceGetCurrentTextureStatus_Outdated:
	case WGPUSurfaceGetCurrentTextureStatus_Lost:
		_deprecated = true;
		status = Status::ErrorCancelled;
		break;
	default:
		status = Status::ErrorUnknown;
		log::source().error("webgpu::Swapchain", "Fail to acquire surface texture: ",
				toInt(surfaceTexture.status));
		break;
	}

	if (surfaceTexture.texture) {
		wgpuTextureRelease(surfaceTexture.texture);
	}

	return nullptr;
}

Status Swapchain::present(core::DeviceQueue &, core::ImageStorage *image, uint64_t) {
	if (_invalid) {
		return Status::ErrorCancelled;
	}

	auto result = wgpuSurfacePresent(_surface.get_cast<Surface>()->getSurface());

	sprt::unique_lock<sprt::mutex> lock(_resourceMutex);

	// the texture is consumed by present; drop slot references immediately,
	// so the framebuffer cache releases its views and wgpu can recycle
	// the underlying surface texture
	if (image) {
		if (auto img = image->getImage()) {
			for (uint32_t i = 0; i < uint32_t(_images.size()); ++i) {
				if (_images[i].image == img) {
					// release the wgpu texture wrapper NOW, while the surface
					// still counts it as presented; a deferred drop (after the
					// next acquire) would discard the next frame's texture
					static_cast<Image *>(_images[i].image.get())->invalidateTexture();
					releaseSlot(i);
					break;
				}
			}
		}
	}

	if (_acquiredImages > 0) {
		--_acquiredImages;
	}
	++_presentedFrames;
	_presentTime = sp::platform::clock(ClockType::Monotonic);

	if (result != WGPUStatus_Success) {
		return Status::ErrorCancelled;
	}

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
}

SimpleSwapchain::~SimpleSwapchain() { releaseCurrent(); }

bool SimpleSwapchain::init(Device &dev, NotNull<core::Loop> loop, const core::SurfaceInfo &info,
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

	WGPUSurfaceConfiguration surfaceConfig = WGPU_SURFACE_CONFIGURATION_INIT;
	surfaceConfig.device = dev.getDevice();
	surfaceConfig.format = getWGPUFormat(cfg.imageFormat);
	surfaceConfig.usage = getWGPUTextureUsage(_swapchainImageInfo.usage);
	surfaceConfig.width = cfg.extent.width;
	surfaceConfig.height = cfg.extent.height;
	surfaceConfig.presentMode = getWGPUPresentMode(presentMode);
	surfaceConfig.alphaMode = WGPUCompositeAlphaMode_Opaque;

	wgpuSurfaceConfigure(surface->getSurface(), &surfaceConfig);

	return core::Object::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle, void *) { },
			core::ObjectType::Swapchain, core::ObjectHandle::zero());
}

auto SimpleSwapchain::acquire(bool lockfree, const Rc<core::Fence> &fence, Status &status)
		-> Rc<SwapchainAcquiredImage> {
	if (_deprecated || _invalid) {
		status = Status::ErrorCancelled;
		return nullptr;
	}

	// one current texture per frame; the engine retries via its timer
	if (_acquiredImages > 0) {
		status = Status::Timeout;
		return nullptr;
	}

	WGPUSurfaceTexture surfaceTexture = WGPU_SURFACE_TEXTURE_INIT;
	wgpuSurfaceGetCurrentTexture(_surface.get_cast<Surface>()->getSurface(), &surfaceTexture);

	switch (surfaceTexture.status) {
	case WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal: _deprecated = true; [[fallthrough]];
	case WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal: {
		releaseCurrent();

		auto imageInfo = core::ImageInfoData(_swapchainImageInfo);
		_current.image = Rc<Image>::create(*_device, surfaceTexture.texture, "SwapchainImage",
				imageInfo);
		if (!_current.image) {
			wgpuTextureRelease(surfaceTexture.texture);
			status = Status::ErrorCancelled;
			return nullptr;
		}

		// browser contract: the texture is used within the current frame; its
		// default view is created right away
		core::ImageViewInfo defaultViewInfo;
		switch (imageInfo.imageType) {
		case core::ImageType::Image1D:
			defaultViewInfo.type = core::ImageViewType::ImageView1D;
			break;
		case core::ImageType::Image2D:
			defaultViewInfo.type = core::ImageViewType::ImageView2D;
			break;
		case core::ImageType::Image3D:
			defaultViewInfo.type = core::ImageViewType::ImageView3D;
			break;
		}
		defaultViewInfo = imageInfo.getViewInfo(defaultViewInfo);
		if (auto view = makeView(_current.image, defaultViewInfo)) {
			_current.views.emplace(defaultViewInfo, move(view));
		}

		sprt::unique_lock<sprt::mutex> lock(_resourceMutex);
		++_acquiredImages;

		if (fence) {
			fence->setTag("webgpu::SimpleSwapchain::acquire");
			static_cast<Fence *>(fence.get())->signal();
		}

		status = surfaceTexture.status == WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal
				? Status::Ok
				: Status::Suboptimal;

		return Rc<SwapchainAcquiredImage>::alloc(0, &_current, acquireSemaphore(), this);
		break;
	}
	case WGPUSurfaceGetCurrentTextureStatus_Timeout: status = Status::Timeout; break;
	case WGPUSurfaceGetCurrentTextureStatus_Outdated:
	case WGPUSurfaceGetCurrentTextureStatus_Lost:
		_deprecated = true;
		status = Status::ErrorCancelled;
		break;
	default:
		status = Status::ErrorUnknown;
		log::source().error("webgpu::SimpleSwapchain", "Fail to acquire surface texture: ",
				toInt(surfaceTexture.status));
		break;
	}

	if (surfaceTexture.texture) {
		wgpuTextureRelease(surfaceTexture.texture);
	}

	return nullptr;
}

Status SimpleSwapchain::present(core::DeviceQueue &, core::ImageStorage *image, uint64_t) {
	if (_invalid) {
		return Status::ErrorCancelled;
	}

	auto result = WGPUStatus_Success;

#if XL_WGPU_NATIVE_API
	// native seam: browsers present implicitly at the end of the frame task
	result = wgpuSurfacePresent(_surface.get_cast<Surface>()->getSurface());

	if (_current.image) {
		// release the wgpu texture wrapper NOW, while the surface still
		// counts it as presented; a deferred drop (after the next acquire)
		// would discard the next frame's texture (wgpu-native v29)
		static_cast<Image *>(_current.image.get())->invalidateTexture();
	}
#endif

	sprt::unique_lock<sprt::mutex> lock(_resourceMutex);

	releaseCurrent();

	if (_acquiredImages > 0) {
		--_acquiredImages;
	}
	++_presentedFrames;
	_presentTime = sp::platform::clock(ClockType::Monotonic);

	if (result != WGPUStatus_Success) {
		return Status::ErrorCancelled;
	}

	return Status::Ok;
}

void SimpleSwapchain::invalidateImage(const core::ImageStorage *image, bool) {
	sprt::unique_lock<sprt::mutex> lock(_resourceMutex);
	if (_acquiredImages > 0) {
		--_acquiredImages;
	}
}

void SimpleSwapchain::invalidateImage(uint32_t, bool) {
	sprt::unique_lock<sprt::mutex> lock(_resourceMutex);
	if (_acquiredImages > 0) {
		--_acquiredImages;
	}
}

Rc<core::ImageView> SimpleSwapchain::makeView(const Rc<core::ImageObject> &image,
		const core::ImageViewInfo &info) {
	// reuse the view created at acquisition when compatible
	if (_current.image == image) {
		for (auto &it : _current.views) {
			if (it.second->getInfo().format == info.format
					&& it.second->getInfo().type == info.type) {
				return it.second;
			}
		}
	}

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

	if (_current.image == image) {
		_currentViews.emplace_back(view);
	}

	return view;
}

void SimpleSwapchain::releaseCurrent() {
	for (auto &it : _currentViews) { it->runReleaseCallback(); }
	_currentViews.clear();
	_current.views.clear();
	_current.image = nullptr;
}

Rc<core::Semaphore> SimpleSwapchain::acquireSemaphore() { return Rc<Semaphore>::create(*_device); }

bool SimpleSwapchain::releaseSemaphore(Rc<core::Semaphore> &&) { return true; }

bool PresentationEngine::init(NotNull<core::Loop> loop, NotNull<core::Device> device,
		NotNull<core::PresentationWindow> window, core::PresentationOptions opts) {
	// texture acquisition in WebGPU is synchronous, external fence has no meaning
	opts.acquireImageWithoutFence = true;

	// wgpuSurfacePresent presents the CURRENT surface texture (there is no image
	// argument), so a present deferred to a later time window would consume the
	// texture of the NEXT frame; presents must run immediately
	opts.usePresentWindow = false;

	return core::PresentationEngine::init(loop, device, window, opts);
}

bool PresentationEngine::run() {
	if (!_surface) {
		log::source().error("webgpu::PresentationEngine",
				"No surface bound with PresentationEngine to run()");
		return false;
	}

	auto info = _window->getSurfaceOptions(*_device, _surface);
	auto cfg = _window->selectConfig(info, false);

	if (!createSwapchain(info, move(cfg), cfg.presentMode, true)) {
		log::source().error("webgpu::PresentationEngine", "Fail to create swapchain");
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

	// defensive drain before reconfigure; the standard WebGPU model allows
	// reconfiguration with frames in flight (a browser cannot block here)
	if (static_cast<Device *>(_device)->getBackendFeatures().syncPolling) {
		_device->waitIdle();
	}

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
		log::source().error("webgpu::PresentationEngine", "Presentation with config ", cfg,
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

bool PresentationEngine::createSwapchain(const core::SurfaceInfo &info,
		core::SwapchainConfig &&cfg, core::PresentMode presentMode, bool) {
	auto dev = static_cast<Device *>(_device);

	auto swapchainImageInfo = _window->getSwapchainImageInfo(cfg);

	auto oldSwapchain = move(_swapchain);

	if (oldSwapchain && oldSwapchain->getAcquiredImagesCount() != 0) {
		log::source().warn("webgpu::PresentationEngine", "Some swapchain images still active");
	}

	// browser presentation model (single current texture, implicit present)
	// is the default when synchronous polling is unavailable; can be forced
	// for verification on a native device
	const bool simplePresent =
			!dev->getBackendFeatures().syncPolling || ::getenv("XL_WGPU_SIMPLE_PRESENT");

	if (simplePresent) {
		_swapchain = Rc<SimpleSwapchain>::create(*dev, _loop.get(), info, cfg,
				move(swapchainImageInfo), presentMode, _surface.get_cast<Surface>());
	} else {
		_swapchain = Rc<Swapchain>::create(*dev, _loop.get(), info, cfg,
				move(swapchainImageInfo), presentMode, _surface.get_cast<Surface>());
	}

	if (!_swapchain) {
		log::source().error("webgpu::PresentationEngine", "Fail to create swapchain");
		return false;
	}

	auto newConstraints = _window->exportConstraints(_serial);
	newConstraints.extent = Extent3(cfg.extent, 1);
	newConstraints.transform = cfg.transform;

	_constraints = sp::move(newConstraints);

	handleSwapchainUpdated(_constraints);
	_readyForNextFrame = true;
	return true;
}

} // namespace stappler::xenolith::webgpu
