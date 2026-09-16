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

#ifndef XENOLITH_BACKEND_GLES_XLGLESDEVICE_H_
#define XENOLITH_BACKEND_GLES_XLGLESDEVICE_H_

#include "XLGlesInstance.h"
#include "XLCoreDevice.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles {

// One GL context per device, current on the loop thread. init reopens the display the instance
// probe used (DeviceInfo) and keeps its context for the life of the loop. All GL calls run on
// that thread; other threads must go through scheduleRelease().
class SP_PUBLIC Device final : public core::Device {
public:
	virtual ~Device() = default;

	bool init(const Instance *, const DeviceInfo &);

	virtual void end() override;
	virtual void waitIdle() const override;

	// True while the context exists. Clear callbacks run on whatever thread drops a reference, so
	// they check this and hand their delete to scheduleRelease() instead of calling GL themselves.
	bool isAlive() const { return _alive.load(); }

	// The instance owns the resolved function pointers and outlives every device. Defined in
	// XLGlesInstance.cc, where Instance is complete.
	const EglTable &getTable() const;

	// Monotonic id handed to every ImageView: the frame cache keys framebuffers by it, so it must
	// be unique per view and never reused.
	uint64_t getNextObjectIndex() { return _objectIndex.fetch_add(1) + 1; }

	// --- Windowed WSI: the loop thread's context/display/render surface are what a windowed
	//     swapchain needs to blit its texture onto an EGLWindowSurface and eglSwapBuffers. The
	//     render surface is the pbuffer/surfaceless one makeCurrent used at init; presenting
	//     temporarily rebinds the context to the window surface, then restores this one.
	EGLDisplay getDisplay() const { return _display; }
	EGLContext getContext() const { return _context; }
	EGLSurface getRenderSurface() const { return _surface; }

	// Whether this display can take a damage region with the swap (EGL_KHR_swap_buffers_with_damage).
	// False means the present falls back to a plain eglSwapBuffers, which is a full-surface hint.
	bool hasSwapWithDamage() const { return _swapWithDamage; }

	// Create an EGLWindowSurface on this device's display for the given native window handle.
	// Wayland wraps the wl_surface in a wl_egl_window (returned through outNativeWindow, owned by
	// the caller); xcb takes a pointer to the window id. Fails when the driver lacks
	// eglCreatePlatformWindowSurfaceEXT, libwayland-egl is missing, or the config lacks
	// EGL_WINDOW_BIT, meaning windowed presentation is unavailable.
	bool createWindowSurface(sprt::window::SurfaceBackend backend, void *nativeWindow,
			Extent2 extent, EGLSurface &out, void *&outNativeWindow);

	// Undo createWindowSurface: the EGLSurface first, then the native window it was built on -
	// the wl_egl_window has to outlive the surface that references it. Both handles are cleared.
	// No resize counterpart: a resize builds a new swapchain with its own pair.
	void destroyWindowSurface(EGLSurface &surface, void *&nativeWindow);

	// Queue a GL delete for execution on the loop thread (drainPendingReleases). Safe from any
	// thread: when end() has already run, the call is dropped - context teardown reclaims every
	// name that was ever allocated.
	void scheduleRelease(Function<void()> &&fn);

	// Run everything queued by scheduleRelease. Loop thread only, with the context current.
	void drainPendingReleases();

	// Samplers are immutable value objects here, so identical requests share one instance.
	Rc<core::Sampler> getSampler(const core::SamplerInfo &);

	virtual Rc<core::Framebuffer> makeFramebuffer(const core::QueuePassData *,
			SpanView<Rc<core::ImageView>>) override;
	virtual Rc<core::ImageStorage> makeImage(StringView, const core::ImageInfoData &) override;
	virtual Rc<core::Semaphore> makeSemaphore() override;
	virtual Rc<core::ImageView> makeImageView(const Rc<core::ImageObject> &,
			const core::ImageViewInfo &) override;
	virtual Rc<core::TextureSet> makeTextureSet(const core::TextureSetLayout &) override;

protected:
	using core::Device::init;

	DeviceInfo _deviceInfo;
	EGLDisplay _display = EGL_NO_DISPLAY;
	// False when _display was opened on the session's own wayland/xcb connection: that display is
	// EGL's shared handle for a connection this backend does not own, and end() must not terminate
	// it. See Device::end.
	bool _ownsDisplay = true;
	bool _swapWithDamage = false;
	EGLConfig _config = nullptr;
	EGLContext _context = EGL_NO_CONTEXT;
	EGLSurface _surface = EGL_NO_SURFACE;

	sprt::atomic<bool> _alive = false;
	sprt::atomic<uint64_t> _objectIndex = 1;

	sprt::mutex _samplerMutex;
	Vector<Rc<core::Sampler>> _samplers;

	mutable sprt::mutex _releaseMutex;
	Vector<Function<void()>> _pendingReleases;
};

} // namespace stappler::xenolith::gles

#endif /* XENOLITH_BACKEND_GLES_XLGLESDEVICE_H_ */
