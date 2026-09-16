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

#include "XLGlesWindowedPresentation.h"
#include "XLGlesObject.h"
#include "XLGlesDevice.h"
#include "XLCoreLoop.h"
#include "XLCoreSwapchain.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles {

bool WindowedSurface::init(Instance *instance, sprt::window::SurfaceBackend backend,
		void *nativeDisplay, void *nativeWindow, Extent2 extent, Ref *win) {
	if (!core::Surface::init(instance, win)) {
		return false;
	}

	_backend = backend;
	_nativeDisplay = nativeDisplay;
	_nativeWindow = nativeWindow;
	_extent = extent;
	return true;
}

void WindowedSurface::invalidate() { _window = nullptr; }

core::SurfaceInfo WindowedSurface::getSurfaceOptions(const core::Device &,
		core::FullScreenExclusiveMode, void *) const {
	core::SurfaceInfo info;

	info.minImageCount = 1;
	info.maxImageCount = 8;

	// The swapchain size comes from currentExtent (Context::handleAppWindowSurfaceUpdate). EGL has
	// no query for it, so WindowedPresentationEngine pushes the window extent in before each read.
	// maxImageExtent must not equal it, or every growth is clamped back to the old size.
	info.currentExtent = _extent;
	info.minImageExtent = Extent2(1, 1);
	info.maxImageExtent = Extent2(1 << 14, 1 << 14);
	info.maxImageArrayLayers = 1;

	info.supportedCompositeAlpha = core::CompositeAlphaFlags::Opaque;
	info.supportedTransforms = core::SurfaceTransformFlags::Identity;
	info.currentTransform = core::SurfaceTransformFlags::Identity;

	info.supportedUsageFlags = core::ImageUsage::ColorAttachment | core::ImageUsage::TransferSrc
			| core::ImageUsage::TransferDst | core::ImageUsage::Sampled;

	// R8G8B8A8 first, matching the loop's common format and what capture reads back.
	info.formats.emplace_back(core::ImageFormat::R8G8B8A8_UNORM,
			core::ColorSpace::SRGB_NONLINEAR_KHR);
	info.formats.emplace_back(core::ImageFormat::R8_UNORM, core::ColorSpace::SRGB_NONLINEAR_KHR);

	// EGL has two present modes: eglSwapInterval(1) is Fifo, eglSwapInterval(0) is Immediate;
	// Mailbox cannot be guaranteed. Immediate first: the PresentationEngine paces presents itself,
	// and a blocking swap would park the loop thread (see the swap interval in present()).
	info.presentModes.emplace_back(core::PresentMode::Immediate);
	info.presentModes.emplace_back(core::PresentMode::Fifo);

	return info;
}

WindowedSwapchain::~WindowedSwapchain() {
	invalidateViews();
	if (auto dev = static_cast<Device *>(_object.device)) {
		dev->destroyWindowSurface(_windowSurface, _nativeEglWindow);
	}
}

bool WindowedSwapchain::init(Device &dev, NotNull<core::Loop>, const core::SurfaceInfo &info,
		const core::SwapchainConfig &cfg, core::ImageInfo &&swapchainImageInfo,
		core::PresentMode presentMode, WindowedSurface *surface) {
	swapchainImageInfo.usage |= core::ImageUsage::TransferSrc;

	auto imageCount = sprt::max(cfg.imageCount, uint32_t(1));
	auto viewInfo = getSwapchainImageViewInfo(swapchainImageInfo);

	_images.reserve(imageCount);
	for (uint32_t i = 0; i < imageCount; ++i) {
		auto image = Rc<Image>::create(dev, toString("GlesWindowedSwapchainImage[", i, "]"),
				core::ImageInfoData(swapchainImageInfo), uint64_t(i));
		if (!image) {
			log::source().error("gles::WindowedSwapchain", "Fail to allocate image ", i);
			return false;
		}

		auto view = Rc<ImageView>::create(dev, Rc<core::ImageObject>(image.get()), viewInfo);
		if (!view) {
			log::source().error("gles::WindowedSwapchain", "Fail to create view for image ", i);
			return false;
		}

		Map<core::ImageViewInfo, Rc<core::ImageView>> views;
		views.emplace(viewInfo, sp::move(view));

		_images.emplace_back(SwapchainImageData{sp::move(image), sp::move(views)});
	}

	_extent = cfg.extent;

	// The EGLWindowSurface is created lazily in present(), so a refused surface (no libwayland-egl,
	// a handle not live yet) is retried instead of failing swapchain creation. The native handle
	// lives on the surface, which finalize() keeps reachable.
	_wsurface = surface;
	_windowSurface = EGL_NO_SURFACE;

	return finalize(dev, info, cfg, move(swapchainImageInfo), presentMode, surface);
}

auto WindowedSwapchain::acquire(bool lockfree, const Rc<core::Fence> &fence, Status &status)
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

		if (fence) {
			fence->setTag("gles::WindowedSwapchain::acquire");
			static_cast<Fence *>(fence.get())->signal();
		}

		status = Status::Ok;

		return Rc<SwapchainAcquiredImage>::alloc(index, &_images[index], nullptr, this);
	}

	status = Status::Timeout;
	return nullptr;
}

Status WindowedSwapchain::present(core::DeviceQueue *, core::ImageStorage *image,
		const core::PresentInfo &info) {
	if (_invalid) {
		return Status::ErrorCancelled;
	}

	sprt::unique_lock<sprt::mutex> lock(_resourceMutex);

	auto slot = maxOf<uint32_t>();
	if (image) {
		slot = findSlot(image);
		if (slot != maxOf<uint32_t>()) {
			markPresented(slot);
		}

		// Mark the image as holding a complete frame; without it the swapchain drops the image's
		// damage snapshot (invalidateImage). This clears ImageStorage::_image, so getImageIndex()
		// must be used before it: the slot is captured above.
		static_cast<core::SwapchainImage *>(image)->setPresented();
	}

	if (_acquiredImages > 0) {
		--_acquiredImages;
	}
	++_presentedFrames;
	_presentTime = sp::platform::clock(ClockType::Monotonic);

	// Copy the presented texture onto the window surface and swap it to the screen. This runs on
	// the loop thread with the render context current, so we can rebind that same context to the
	// window surface, blit, present, then restore the render surface - no second context needed.
	if (!image) {
		return Status::Ok;
	}

	auto dev = static_cast<Device *>(_object.device);
	auto &t = dev->getTable();

	// Lazily create (or retry creating) the window surface; retries are throttled to once a second
	// on a failing stack while the scene keeps rendering into its textures.
	if (_windowSurface == EGL_NO_SURFACE) {
		auto now = sp::platform::clock(ClockType::Monotonic);
		if (_surfaceCreateAttempt != 0 && now - _surfaceCreateAttempt < TimeInterval::seconds(1).toMicros()) {
			return Status::Ok; // not yet mapped; try again next second
		}
		_surfaceCreateAttempt = now;
		if (!dev->createWindowSurface(_wsurface->backend(), _wsurface->nativeWindow(), _extent,
					_windowSurface, _nativeEglWindow)) {
			log::source().verbose("gles::WindowedSwapchain", "Window surface not ready yet, retrying");
			return Status::Ok; // the frame is rendered into its texture; just not shown this once
		}
	}

	if (!t.glBlitFramebuffer || !t.eglSwapBuffers) {
		log::source().error("gles::WindowedSwapchain", "Missing glBlitFramebuffer/eglSwapBuffers");
		return Status::ErrorNotSupported;
	}

	auto dpy = dev->getDisplay();
	auto ctx = dev->getContext();
	auto renderSurface = dev->getRenderSurface();

	if (slot == maxOf<uint32_t>()) {
		return Status::Ok;
	}
	auto texture = _images[slot].image.get_cast<Image>();
	if (!texture) {
		return Status::Ok;
	}

	const uint32_t w = _extent.width;
	const uint32_t h = _extent.height;

	if (!t.eglMakeCurrent(dpy, _windowSurface, _windowSurface, ctx)) {
		log::source().error("gles::WindowedSwapchain", "Fail to make the window surface current, "
				"error ", EGLint(t.eglGetError()));
		return Status::ErrorNotSupported;
	}

	// Swap interval, once per surface, set with the window surface current. Only Fifo blocks: at
	// interval 1 eglSwapBuffers waits for a frame callback that a hidden surface never gets, which
	// would park the loop thread. Pacing belongs to the PresentationEngine.
	if (!_swapIntervalSet && t.eglSwapInterval) {
		const EGLint interval = (getPresentMode() == core::PresentMode::Fifo) ? 1 : 0;
		if (!t.eglSwapInterval(dpy, interval)) {
			log::source().warn("gles::WindowedSwapchain", "Fail to set swap interval ", interval,
					", error ", EGLint(t.eglGetError()));
		}
		_swapIntervalSet = true;
	}

	t.glViewport(0, 0, GLsizei(w), GLsizei(h));
	// The texture lives in an FBO, so attach it to a temporary read framebuffer and blit it onto
	// the window's default framebuffer (draw FBO 0).
	GLuint fbo = 0;
	if (t.glGenFramebuffers) {
		t.glGenFramebuffers(1, &fbo);
	}
	t.glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
	t.glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
			texture->getGlName(), 0);

	t.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // the window's default framebuffer
	// The default framebuffer shows row 0 at the bottom, while the texture holds the image's top
	// row there, so the source rectangle is read upside down (srcY0 = h, srcY1 = 0).
	t.glBlitFramebuffer(0, GLsizei(h), GLsizei(w), 0, 0, 0, GLsizei(w), GLsizei(h),
			GL_COLOR_BUFFER_BIT, GL_NEAREST);

	t.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	if (fbo != 0 && t.glDeleteFramebuffers) {
		t.glDeleteFramebuffers(1, &fbo);
	}

	// Hand the compositor the damaged rectangles, computed against the presented snapshot (empty
	// means everything). They are top-origin, while EGL measures from the bottom left, hence the
	// flip. The tracker never produces more than MaxRects.
	EGLint rects[4 * core::SwapchainDamage::MaxRects];
	EGLint count = 0;
	if (dev->hasSwapWithDamage() && !info.damage.empty()) {
		for (auto &it : info.damage) {
			if (count == EGLint(core::SwapchainDamage::MaxRects)) {
				break;
			}
			auto *r = rects + count * 4;
			r[0] = EGLint(it.x);
			r[1] = EGLint(h) - EGLint(it.y) - EGLint(it.height);
			r[2] = EGLint(it.width);
			r[3] = EGLint(it.height);
			++count;
		}
	}

	if (count > 0) {
		t.eglSwapBuffersWithDamageKHR(dpy, _windowSurface, rects, count);
	} else {
		t.eglSwapBuffers(dpy, _windowSurface);
	}

	// Restore the render surface so the next frame renders into textures as usual.
	if (!t.eglMakeCurrent(dpy, renderSurface, renderSurface, ctx)) {
		log::source().error("gles::WindowedSwapchain", "Fail to restore the render surface, error ",
				EGLint(t.eglGetError()));
	}

	return Status::Ok;
}

void WindowedPresentationEngine::syncSurfaceExtent() {
	auto surface = _surface.get_cast<WindowedSurface>();
	if (!surface || !_window) {
		return;
	}

	// Read through a throwaway serial: the engine's own is bumped by createSwapchain, and this is
	// a peek at the window's geometry rather than a new frame's worth of constraints.
	uint64_t serial = 0;
	auto constraints = _window->exportConstraints(serial);
	if (constraints.extent.width != 0 && constraints.extent.height != 0) {
		surface->setExtent(Extent2(constraints.extent.width, constraints.extent.height));
	}
}

bool WindowedPresentationEngine::run() {
	syncSurfaceExtent();
	return PresentationEngine::run();
}

bool WindowedPresentationEngine::recreateSwapchain() {
	syncSurfaceExtent();
	return PresentationEngine::recreateSwapchain();
}

Rc<SwapchainBase> WindowedPresentationEngine::makeSwapchain(const core::SurfaceInfo &info,
		const core::SwapchainConfig &cfg, core::ImageInfo &&swapchainImageInfo,
		core::PresentMode presentMode) {
	auto dev = static_cast<Device *>(_device);

	auto surface = _surface.get_cast<WindowedSurface>();
	if (!surface) {
		log::source().error("gles::WindowedPresentationEngine", "No windowed surface bound");
		return nullptr;
	}

	surface->setExtent(cfg.extent);

	return Rc<WindowedSwapchain>::create(*dev, _loop, info, cfg, move(swapchainImageInfo),
			presentMode, surface);
}

} // namespace stappler::xenolith::gles
