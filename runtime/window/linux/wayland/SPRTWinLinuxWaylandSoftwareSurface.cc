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

#include "SPRTWinLinuxWaylandSoftwareSurface.h"
#include "SPRTWinLinuxWaylandDisplay.h"
#include "SPRTWinLinuxWaylandWindow.h"
#include "../SPRTWinLinux.h"

#include <sprt/runtime/log.h>

#include <sys/mman.h>
#include <unistd.h>

namespace sprt::window {

// The compositor is free to hold the last attached buffer for as long as it likes, so a ring of
// two would stall every other frame waiting for a release. Three is also what the engine asks for
// by default.
static constexpr uint32_t WaylandSoftware_minImageCount = 3;

static struct wl_buffer_listener s_WaylandSoftwareBufferListener{
	.release =
			[](void *data, wl_buffer *buffer) {
	reinterpret_cast<WaylandSoftwareSwapchain *>(data)->handleBufferRelease(buffer);
},
};

WaylandSoftwareSurface::~WaylandSoftwareSurface() { }

bool WaylandSoftwareSurface::init(NotNull<WaylandWindow> window) {
	auto display = window->getDisplay();
	if (!display || !display->shm || !display->shm->shm) {
		oslog::vperror(__SPRT_LOCATION, "WaylandSoftwareSurface",
				"Compositor does not provide wl_shm");
		return false;
	}

	_display = display;
	_wayland = display->wayland;
	_window = window;
	_surface = window->getSurface();

	return _surface != nullptr;
}

SurfaceInfo WaylandSoftwareSurface::getSurfaceOptions(SurfaceInfo &&info) const {
	info.minImageCount = WaylandSoftware_minImageCount;
	info.maxImageCount = 8;

	info.minImageExtent = Extent2(1, 1);
	info.maxImageExtent = Extent2(Max<uint16_t>, Max<uint16_t>);

	// WL_SHM_FORMAT_xRGB8888 is a 32-bit host-endian word, which on a little-endian machine is the
	// byte order B,G,R,A - exactly B8G8R8A8_UNORM. Report nothing else: the engine's default
	// preference is R8G8B8A8_UNORM, and offering it here would have it pick a format the
	// compositor cannot accept.
	info.formats.emplace_back(ImageFormat::B8G8R8A8_UNORM, ColorSpace::SRGB_NONLINEAR_KHR);

	// Frames are paced by wl_surface.frame, which is what Fifo describes.
	info.presentModes.emplace_back(PresentMode::Fifo);

	return sprt::move(info);
}

Rc<SoftwareSwapchain> WaylandSoftwareSurface::makeSwapchain(const SoftwareSwapchainInfo &info) {
	if (!_display || !_surface) {
		return nullptr;
	}

	return Rc<WaylandSoftwareSwapchain>::create(_display, _surface, info);
}

void WaylandSoftwareSurface::invalidate() {
	_surface = nullptr;
	_window = nullptr;
	_display = nullptr;
	_wayland = nullptr;
}

WaylandSoftwareSwapchain::~WaylandSoftwareSwapchain() { destroyPool(); }

bool WaylandSoftwareSwapchain::init(NotNull<WaylandDisplay> display, wl_surface *surface,
		const SoftwareSwapchainInfo &info) {
	if (info.format != ImageFormat::B8G8R8A8_UNORM) {
		oslog::vperror(__SPRT_LOCATION, "WaylandSoftwareSwapchain", "Unsupported format: ",
				uint32_t(info.format));
		return false;
	}

	if (info.extent.width == 0 || info.extent.height == 0 || info.imageCount == 0) {
		return false;
	}

	_display = display;
	_wayland = display->wayland;
	_surface = surface;
	_extent = info.extent;

	// The stride is ours to choose, so keep rows packed: the screenshot path hands a slot straight
	// to the bitmap encoder, which only accepts a buffer whose length is exactly extent * bpp.
	auto stride = uint32_t(info.extent.width * 4);
	auto slotSize = size_t(stride) * size_t(info.extent.height);
	auto total = slotSize * size_t(info.imageCount);

	auto fd = createAnonymousFile(total);
	if (fd < 0) {
		oslog::vperror(__SPRT_LOCATION, "WaylandSoftwareSwapchain",
				"Fail to allocate shared memory of ", total, " bytes");
		return false;
	}

	auto mapping = ::mmap(nullptr, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (mapping == MAP_FAILED) {
		oslog::vperror(__SPRT_LOCATION, "WaylandSoftwareSwapchain", "Fail to map shared memory");
		::close(fd);
		return false;
	}

	_mapping = reinterpret_cast<uint8_t *>(mapping);
	_mappingSize = total;

	// The pool keeps its own reference to the file, so the descriptor is ours to drop right away.
	_pool = wl_shm_create_pool(display->shm->shm, fd, int32_t(total));
	::close(fd);

	if (!_pool) {
		oslog::vperror(__SPRT_LOCATION, "WaylandSoftwareSwapchain", "Fail to create wl_shm_pool");
		return false;
	}

	_buffers.reserve(info.imageCount);
	_wlBuffers.reserve(info.imageCount);

	for (uint32_t i = 0; i < info.imageCount; ++i) {
		auto offset = slotSize * size_t(i);

		// xRGB rather than ARGB: the window is opaque, and telling the compositor so lets it skip
		// blending the whole surface.
		auto buffer = wl_shm_pool_create_buffer(_pool, int32_t(offset), int32_t(info.extent.width),
				int32_t(info.extent.height), int32_t(stride), WL_SHM_FORMAT_XRGB8888);
		if (!buffer) {
			oslog::vperror(__SPRT_LOCATION, "WaylandSoftwareSwapchain", "Fail to create buffer ",
					i);
			return false;
		}

		wl_buffer_add_listener(buffer, &s_WaylandSoftwareBufferListener, this);

		_wlBuffers.emplace_back(buffer);
		_buffers.emplace_back(SoftwareBuffer{_mapping + offset, stride, slotSize});
	}

	_busy.resize(info.imageCount, false);

	return true;
}

Status WaylandSoftwareSwapchain::present(uint32_t index, SpanView<geom::URect> damage) {
	if (_invalid || index >= _wlBuffers.size() || !_surface) {
		return Status::ErrorCancelled;
	}

	wl_surface_attach(_surface, _wlBuffers[index], 0, 0);

	if (damage.empty()) {
		// Empty means "everything changed", the same contract the gAPI present uses. Deliberately
		// not zero rectangles, which would claim nothing changed at all.
		wl_surface_damage_buffer(_surface, 0, 0, Max<int32_t>, Max<int32_t>);
	} else {
		for (auto &it : damage) {
			wl_surface_damage_buffer(_surface, int32_t(it.x), int32_t(it.y), int32_t(it.width),
					int32_t(it.height));
		}
	}

	wl_surface_commit(_surface);

	// Busy from the attach, never from the acquire: a release event only ever arrives for a buffer
	// that was actually attached, so marking earlier would collapse the ring on the first frame.
	setBufferBusy(index);

	// The flush itself lives in WaylandWindow::handleFramePresented, which the engine calls right
	// after this - the same place the XCB window flushes.
	return Status::Ok;
}

void WaylandSoftwareSwapchain::handleBufferRelease(wl_buffer *buffer) {
	for (uint32_t i = 0; i < uint32_t(_wlBuffers.size()); ++i) {
		if (_wlBuffers[i] == buffer) {
			setBufferFree(i);
			return;
		}
	}
}

void WaylandSoftwareSwapchain::invalidate() {
	_invalid = true;
	_surface = nullptr;
}

void WaylandSoftwareSwapchain::destroyPool() {
	// Buffers first: destroying the pool while a wl_buffer still references it is legal, but the
	// buffers have to go before the mapping does, and this keeps the order obvious.
	for (auto &it : _wlBuffers) {
		if (it) {
			wl_buffer_destroy(it);
		}
	}
	_wlBuffers.clear();
	_buffers.clear();
	_busy.clear();

	if (_pool) {
		wl_shm_pool_destroy(_pool);
		_pool = nullptr;
	}

	if (_mapping) {
		::munmap(_mapping, _mappingSize);
		_mapping = nullptr;
		_mappingSize = 0;
	}
}

} // namespace sprt::window
