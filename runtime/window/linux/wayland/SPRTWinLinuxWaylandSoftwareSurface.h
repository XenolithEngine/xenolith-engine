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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_LINUX_WAYLAND_SPRTWINLINUXWAYLANDSOFTWARESURFACE_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_LINUX_WAYLAND_SPRTWINLINUXWAYLANDSOFTWARESURFACE_H_

#include "SPRTWinLinuxWaylandProtocol.h"

#if SPRT_LINUX

#include <sprt/runtime/window/software_surface.h>

namespace sprt::window {

struct WaylandDisplay;
class WaylandWindow;
class WaylandSoftwareSwapchain;

// wl_shm presentation for a host rasterizer: the swapchain images ARE the compositor's buffers,
// so a frame is rasterized straight into what gets attached, with no intermediate bitmap.
class SPRT_API WaylandSoftwareSurface final : public SoftwareSurface {
public:
	virtual ~WaylandSoftwareSurface();

	bool init(NotNull<WaylandWindow>);

	virtual SurfaceInfo getSurfaceOptions(SurfaceInfo &&) const override;

	virtual Rc<SoftwareSwapchain> makeSwapchain(const SoftwareSwapchainInfo &) override;

	virtual void invalidate() override;

protected:
	Rc<WaylandDisplay> _display;
	Rc<WaylandLibrary> _wayland;
	WaylandWindow *_window = nullptr;
	wl_surface *_surface = nullptr;
};

// One memfd, one wl_shm_pool, N wl_buffers carved out of it at successive offsets.
//
// The pool and its mapping outlive this object when they have to: a compositor may still hold a
// buffer from a swapchain that has already been replaced, so teardown waits for the last
// wl_buffer.release rather than pulling the memory out from under it.
class SPRT_API WaylandSoftwareSwapchain final : public SoftwareSwapchain {
public:
	virtual ~WaylandSoftwareSwapchain();

	bool init(NotNull<WaylandDisplay>, wl_surface *, const SoftwareSwapchainInfo &);

	virtual Status present(uint32_t index, SpanView<geom::URect> damage) override;

	virtual void invalidate() override;

	// wl_buffer.release: the compositor is done with this slot.
	void handleBufferRelease(wl_buffer *);

protected:
	void destroyPool();

	Rc<WaylandDisplay> _display;
	Rc<WaylandLibrary> _wayland;
	wl_surface *_surface = nullptr;

	wl_shm_pool *_pool = nullptr;
	uint8_t *_mapping = nullptr;
	size_t _mappingSize = 0;

	Vector<wl_buffer *> _wlBuffers;
	Extent2 _extent;
};

} // namespace sprt::window

#endif

#endif /* CORE_RUNTIME_PRIVATE_WINDOW_LINUX_WAYLAND_SPRTWINLINUXWAYLANDSOFTWARESURFACE_H_ */
