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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_SOFTWARE_SURFACE_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_SOFTWARE_SURFACE_H_

#include <sprt/runtime/ref.h>
#include <sprt/runtime/window/types.h>
#include <sprt/runtime/window/mode.h>
#include <sprt/runtime/window/surface_info.h>
#include <sprt/cxx/vector>
#include <sprt/cxx/function>

// A window system that can hand out CPU-writable pixel buffers.
//
// This is the seam a software rasterizer presents through, and it exists separately from
// SurfaceInterfaceInfo on purpose: that one is a const POD of native handles, so it can route
// (its `backend` tag says Wayland or Xcb) but it cannot allocate, cannot negotiate a format and
// cannot carry per-buffer readiness. A CPU swapchain needs all three.
//
// The shape mirrors the Vulkan pair deliberately: a Surface that lives as long as the window, and
// a Swapchain that is rebuilt whenever the size changes. The point of the whole seam is that the
// rasterizer writes into the window system's own memory - a wl_shm buffer, an X SHM segment - with
// no intermediate bitmap and no copy on present.

namespace sprt::window {

class SoftwareSwapchain;

// One slot of the ring. `data`/`stride` are what the rasterizer writes into.
//
// `size` must be exactly stride * height for a single slot, never the whole pool: it is what the
// capture path hands to the bitmap encoder, which rejects anything whose length does not match
// extent * bytes-per-pixel exactly (and reports it as an unsupported format, which points nowhere
// near the real cause).
struct SoftwareBuffer {
	uint8_t *data = nullptr;
	uint32_t stride = 0;
	size_t size = 0;
};

struct SoftwareSwapchainInfo {
	Extent2 extent;
	ImageFormat format = ImageFormat::Undefined;
	uint32_t imageCount = 0;
};

// Lives as long as the window. The analogue of a gAPI surface.
class SPRT_API SoftwareSurface : public Ref {
public:
	virtual ~SoftwareSurface() = default;

	// Report the formats, image counts, usage and current extent this transport can accept.
	//
	// The extent is the transport's job: NativeWindow::getSurfaceOptions is a pass-through by
	// default, and only the Wayland window overrides it (to scale by output density, which
	// therefore still applies on top of whatever is set here).
	virtual SurfaceInfo getSurfaceOptions(SurfaceInfo &&) const = 0;

	virtual Rc<SoftwareSwapchain> makeSwapchain(const SoftwareSwapchainInfo &) = 0;

	virtual void invalidate() = 0;
};

// The ring of buffers, rebuilt wholesale on resize. The analogue of a gAPI swapchain.
class SPRT_API SoftwareSwapchain : public Ref {
public:
	virtual ~SoftwareSwapchain() = default;

	SpanView<SoftwareBuffer> getBuffers() const { return _buffers; }

	uint32_t getBufferCount() const { return uint32_t(_buffers.size()); }

	// Index of a slot the window system is not holding, or Max<uint32_t> with `status` set.
	//
	// MUST NOT BLOCK. On Linux the gAPI loop and the window system share one thread, so the only
	// thread that can dispatch the release event is the one that would be waiting here - a blocking
	// wait is a self-deadlock. Status::Timeout means "every slot is in flight, ask again"; the
	// presentation engine already retries on it.
	virtual uint32_t acquire(Status &status);

	// An empty `damage` means the whole surface changed, the same contract the gAPI present uses.
	virtual Status present(uint32_t index, SpanView<geom::URect> damage) = 0;

	// Optional low-latency hook: the transport calls this when a slot becomes reusable, on the
	// loop thread. Nothing subscribes today - acquisition already recovers through the engine's
	// retry timer and, on Wayland, through the frame callback - but wiring it later needs no
	// change to this interface.
	void setReleaseCallback(Function<void(uint32_t)> &&cb) { _releaseCallback = sprt::move(cb); }

	virtual void invalidate() = 0;

protected:
	// Mark a slot as handed to the window system. Call this at the point the buffer is actually
	// attached or blitted, never at acquire: a release event never arrives for a buffer that was
	// never submitted, so pre-marking collapses the ring on the first frame.
	void setBufferBusy(uint32_t index);

	// Mark a slot as reusable and fire the release callback, if any.
	void setBufferFree(uint32_t index);

	bool isBufferBusy(uint32_t index) const;

	Vector<SoftwareBuffer> _buffers;
	Vector<bool> _busy;
	uint32_t _nextIndex = 0;
	bool _invalid = false;
	Function<void(uint32_t)> _releaseCallback;
};

} // namespace sprt::window

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_SOFTWARE_SURFACE_H_
