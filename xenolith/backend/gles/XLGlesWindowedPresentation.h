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

#ifndef XENOLITH_BACKEND_GLES_XLGLESWINDOWEDPRESENTATION_H_
#define XENOLITH_BACKEND_GLES_XLGLESWINDOWEDPRESENTATION_H_

#include "XLGlesPresentation.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles {

// A surface backed by a real window (Wayland wl_surface or an xcb window). The native handles
// travel with the surface so the swapchain can build the matching EGLWindowSurface; capabilities
// are synthesized from the window extent exactly as HeadlessSurface does - nothing is queried.
class SP_PUBLIC WindowedSurface final : public core::Surface {
public:
	virtual ~WindowedSurface() = default;

	bool init(Instance *, sprt::window::SurfaceBackend backend, void *nativeDisplay,
			void *nativeWindow, Extent2 extent, Ref *window = nullptr);

	virtual void invalidate() override;

	virtual core::SurfaceInfo getSurfaceOptions(const core::Device &,
			core::FullScreenExclusiveMode, void *) const override;

	void setExtent(Extent2 extent) { _extent = extent; }

	sprt::window::SurfaceBackend backend() const { return _backend; }
	void *nativeDisplay() const { return _nativeDisplay; }
	void *nativeWindow() const { return _nativeWindow; }

protected:
	sprt::window::SurfaceBackend _backend = sprt::window::SurfaceBackend::Headless;
	void *_nativeDisplay = nullptr;
	void *_nativeWindow = nullptr;
	Extent2 _extent;
};

// Windowed swapchain: the same ring of GL textures as HeadlessSwapchain (rendering is identical),
// but present() copies the presented texture onto an EGLWindowSurface and eglSwapBuffers it to the
// screen. The window surface is created once in init from the surface's native handle; presenting
// temporarily makes the loop context current on it, blits, swaps, then restores the render surface.
class SP_PUBLIC WindowedSwapchain final : public SwapchainBase {
public:
	virtual ~WindowedSwapchain();

	bool init(Device &, NotNull<core::Loop>, const core::SurfaceInfo &,
			const core::SwapchainConfig &, core::ImageInfo &&, core::PresentMode,
			WindowedSurface *);

	virtual Rc<SwapchainAcquiredImage> acquire(bool lockfree, const Rc<core::Fence> &fence,
			Status &) override;

	virtual Status present(core::DeviceQueue *, core::ImageStorage *,
			const core::PresentInfo &) override;

protected:
	using SwapchainBase::init;

	uint32_t _nextIndex = 0;
	EGLSurface _windowSurface = EGL_NO_SURFACE;
	Extent2 _extent;
	WindowedSurface *_wsurface = nullptr; // the surface holding the native window handle
	uint64_t _surfaceCreateAttempt = 0; // throttles lazy-surface retries (monotonic microseconds)
};

// Presentation engine for a windowed gles surface. run()/recreateSwapchain() are inherited
// unchanged - they only talk to the PresentationWindow and makeSwapchain - which here builds the
// EGLWindowSurface and the texture ring that presents through it.
class SP_PUBLIC WindowedPresentationEngine final : public PresentationEngine {
public:
	virtual ~WindowedPresentationEngine() = default;

protected:
	virtual Rc<SwapchainBase> makeSwapchain(const core::SurfaceInfo &,
			const core::SwapchainConfig &, core::ImageInfo &&, core::PresentMode) override;
};

} // namespace stappler::xenolith::gles

#endif /* XENOLITH_BACKEND_GLES_XLGLESWINDOWEDPRESENTATION_H_ */
