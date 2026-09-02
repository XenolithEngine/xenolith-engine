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

// One GL context per device, current on the loop thread. The instance probes each EGL device
// with a temporary context and hands back everything it learned (DeviceInfo); init reopens that
// same display - platform device, surfaceless or default - and keeps its own context alive for
// the life of the loop. Everything that touches the API (object creation, pass submission,
// readback) runs on the thread that made the context current; a call from anywhere else must go
// through scheduleRelease() instead of issuing GL directly.
class SP_PUBLIC Device final : public core::Device {
public:
	virtual ~Device() = default;

	bool init(const Instance *, const DeviceInfo &);

	virtual void end() override;
	virtual void waitIdle() const override;

	// True while the context exists. Clear callbacks run on whatever thread drops a reference, so
	// they check this and hand their delete to scheduleRelease() instead of calling GL themselves.
	bool isAlive() const { return _alive.load(); }

	// The instance owns the resolved function pointers and outlives every device it made, so a
	// pointer hop is enough - keeping a copy (or a reference) here would only duplicate state.
	// Defined in XLGlesInstance.cc: the downcast needs Instance to be complete, which this header
	// does not guarantee for every includer.
	const EglTable &getTable() const;

	// Monotonic id handed to every ImageView: the frame cache keys framebuffers by it, so it must
	// be unique per view and never reused.
	uint64_t getNextObjectIndex() { return _objectIndex.fetch_add(1) + 1; }

	// --- Windowed WSI (M2): the loop thread's context/display/render surface are what a windowed
	//     swapchain needs to blit its texture onto an EGLWindowSurface and eglSwapBuffers. The
	//     render surface is the pbuffer/surfaceless one makeCurrent used at init; presenting
	//     temporarily rebinds the context to the window surface, then restores this one.
	EGLDisplay getDisplay() const { return _display; }
	EGLContext getContext() const { return _context; }
	EGLSurface getRenderSurface() const { return _surface; }

	// Create an EGLWindowSurface on this device's display for the given native window handle.
	// Each platform extension spells its native window differently, and neither spells it the way
	// the window system hands it over: wayland needs the wl_surface wrapped in a wl_egl_window
	// (created here, returned through outNativeWindow, and owned by the caller), xcb needs a
	// pointer to the window id rather than the id itself. Fails when the driver lacks
	// eglCreatePlatformWindowSurfaceEXT, when libwayland-egl is missing, or when the config does
	// not carry EGL_WINDOW_BIT - all of which are the "windowed presentation unavailable" case.
	bool createWindowSurface(sprt::window::SurfaceBackend backend, void *nativeWindow,
			Extent2 extent, EGLSurface &out, void *&outNativeWindow);

	// Undo createWindowSurface: the EGLSurface first, then the native window it was built on -
	// the wl_egl_window has to outlive the surface that references it. Both handles are cleared.
	//
	// There is no resize counterpart on purpose: a window that changes size makes the presentation
	// engine build a new swapchain, and the new one creates its own pair at the new extent while
	// the old one destroys its own here.
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
