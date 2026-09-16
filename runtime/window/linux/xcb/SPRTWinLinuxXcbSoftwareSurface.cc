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

#include "SPRTWinLinuxXcbSoftwareSurface.h"
#include "SPRTWinLinuxXcbWindow.h"
#include "SPRTWinLinuxXcbLibrary.h"
#include "../SPRTWinLinux.h"

#include <sprt/runtime/log.h>

#include <sys/mman.h>
#include <unistd.h>

namespace sprt::window {

// The server releases a segment as soon as it has copied out of it, so the busy window is much
// shorter than a compositor's. Three keeps it uniform with the Wayland path and with what the
// engine asks for anyway.
static constexpr uint32_t XcbSoftware_minImageCount = 3;

// Resolve the sentinel the window creation code leaves behind when it inherits from the parent.
static uint8_t XcbSoftware_resolveDepth(NotNull<XcbConnection> conn, uint8_t depth) {
	if (depth == XCB_COPY_FROM_PARENT || depth == 0) {
		return uint8_t(conn->getDefaultScreen()->root_depth);
	}
	return depth;
}

// True when the server's on-the-wire pixel layout for `depth` is byte order B,G,R,A - the one
// layout the rasterizer produces. Anything else is refused outright rather than blitted as
// silently wrong colours.
static bool XcbSoftware_isBgra(NotNull<XcbConnection> conn, uint8_t depth) {
	auto xcb = conn->getXcb();
	auto setup = xcb->xcb_get_setup(conn->getConnection());
	if (!setup || setup->image_byte_order != XCB_IMAGE_ORDER_LSB_FIRST) {
		return false;
	}

	auto screen = conn->getDefaultScreen();

	auto depthIter = xcb->xcb_screen_allowed_depths_iterator(screen);
	for (; depthIter.rem; xcb->xcb_depth_next(&depthIter)) {
		if (depthIter.data->depth != depth) {
			continue;
		}

		auto visualIter = xcb->xcb_depth_visuals_iterator(depthIter.data);
		for (; visualIter.rem; xcb->xcb_visualtype_next(&visualIter)) {
			auto v = visualIter.data;
			if (v->red_mask == 0x00FF'0000 && v->green_mask == 0x0000'FF00
					&& v->blue_mask == 0x0000'00FF) {
				return true;
			}
		}
	}
	return false;
}

XcbSoftwareSurface::~XcbSoftwareSurface() { }

bool XcbSoftwareSurface::init(NotNull<XcbWindow> window) {
	auto conn = window->getXcbConnection();
	if (!conn) {
		return false;
	}

	uint32_t major = 0;
	uint32_t minor = 0;
	conn->getShmVersion(major, minor);

	if (major == 0) {
		// No copy-through-the-socket fallback on purpose: xcb_put_image would push our own buffer
		// through the protocol connection every frame, giving up the one property this path
		// exists for. Failing here is honest; degrading silently reads as "soft is just slow".
		oslog::vperror(__SPRT_LOCATION, "XcbSoftwareSurface",
				"MIT-SHM is not available on this display, so the software backend cannot present "
				"without copying; use a local display or the Vulkan backend");
		return false;
	}

	if (major < 1 || (major == 1 && minor < 2)) {
		oslog::vperror(__SPRT_LOCATION, "XcbSoftwareSurface", "MIT-SHM ", major, ".", minor,
				" is too old: file-descriptor passing (1.2) is required");
		return false;
	}

	_connection = conn;
	_xcb = conn->getXcb();
	_owner = window;
	_window = window->getOutputWindow();
	_depth = XcbSoftware_resolveDepth(conn, window->getDepth());

	if (!XcbSoftware_isBgra(conn, _depth)) {
		oslog::vperror(__SPRT_LOCATION, "XcbSoftwareSurface",
				"Display pixel layout at depth ", _depth,
				" is not 32-bit little-endian BGRA, which is the only one the software "
				"rasterizer produces");
		return false;
	}

	return _window != 0;
}

SurfaceInfo XcbSoftwareSurface::getSurfaceOptions(SurfaceInfo &&info) const {
	info.minImageCount = XcbSoftware_minImageCount;
	info.maxImageCount = 8;

	// Nobody else fills this in on X: the Vulkan path gets it from the server's surface
	// capabilities, and XcbWindow - unlike WaylandWindow - has no getSurfaceOptions override. Left
	// unset it stays 0x0 and the swapchain is asked for zero-sized buffers.
	info.currentExtent = _owner ? _owner->getExtent() : Extent2(0, 0);

	info.minImageExtent = Extent2(1, 1);
	info.maxImageExtent = Extent2(Max<uint16_t>, Max<uint16_t>);

	info.formats.emplace_back(ImageFormat::B8G8R8A8_UNORM, ColorSpace::SRGB_NONLINEAR_KHR);

	// Nothing paces a put_image; the engine's own present window is what spaces frames out.
	info.presentModes.emplace_back(PresentMode::Immediate);
	info.presentModes.emplace_back(PresentMode::Fifo);

	return sprt::move(info);
}

Rc<SoftwareSwapchain> XcbSoftwareSurface::makeSwapchain(const SoftwareSwapchainInfo &info) {
	if (!_connection || !_window) {
		return nullptr;
	}

	return Rc<XcbSoftwareSwapchain>::create(_connection, _window, _depth, info);
}

void XcbSoftwareSurface::invalidate() {
	_window = 0;
	_owner = nullptr;
	_xcb = nullptr;
	_connection = nullptr;
}

XcbSoftwareSwapchain::~XcbSoftwareSwapchain() { teardown(); }

bool XcbSoftwareSwapchain::init(NotNull<XcbConnection> conn, xcb_window_t window, uint8_t depth,
		const SoftwareSwapchainInfo &info) {
	if (info.format != ImageFormat::B8G8R8A8_UNORM) {
		oslog::vperror(__SPRT_LOCATION, "XcbSoftwareSwapchain", "Unsupported format: ",
				uint32_t(info.format));
		return false;
	}

	if (info.extent.width == 0 || info.extent.height == 0 || info.imageCount == 0) {
		return false;
	}

	_connection = conn;
	_xcb = conn->getXcb();
	_window = window;
	_depth = depth;
	_extent = info.extent;

	// Ours to choose, so keep rows packed: the screenshot path hands a slot straight to the bitmap
	// encoder, which only accepts a length of exactly extent * bytes-per-pixel.
	auto stride = uint32_t(info.extent.width * 4);
	_slotSize = size_t(stride) * size_t(info.extent.height);

	auto total = _slotSize * size_t(info.imageCount);

	auto fd = createAnonymousFile(total);
	if (fd < 0) {
		oslog::vperror(__SPRT_LOCATION, "XcbSoftwareSwapchain", "Fail to allocate shared memory of ",
				total, " bytes");
		return false;
	}

	auto mapping = ::mmap(nullptr, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (mapping == MAP_FAILED) {
		oslog::vperror(__SPRT_LOCATION, "XcbSoftwareSwapchain", "Fail to map shared memory");
		::close(fd);
		return false;
	}

	_mapping = reinterpret_cast<uint8_t *>(mapping);
	_mappingSize = total;

	// attach_fd consumes the descriptor, and unlike a System V segment it leaves nothing behind if
	// this process dies.
	_segment = _xcb->xcb_generate_id(conn->getConnection());
	_xcb->xcb_shm_attach_fd(conn->getConnection(), _segment, fd, 0);

	_gc = _xcb->xcb_generate_id(conn->getConnection());
	_xcb->xcb_create_gc(conn->getConnection(), _gc, _window, 0, nullptr);

	_buffers.reserve(info.imageCount);
	for (uint32_t i = 0; i < info.imageCount; ++i) {
		_buffers.emplace_back(SoftwareBuffer{_mapping + _slotSize * size_t(i), stride, _slotSize});
	}

	_busy.resize(info.imageCount, false);

	// Keyed by segment, because that is what the completion event carries: it names the shmseg it
	// finished with, and the blit target is the output window, which the window map is not keyed by.
	conn->attachShmSwapchain(_segment, this);

	return true;
}

Status XcbSoftwareSwapchain::present(uint32_t index, SpanView<geom::URect> damage) {
	if (_invalid || index >= _buffers.size() || !_window) {
		return Status::ErrorCancelled;
	}

	auto c = _connection->getConnection();
	auto offset = uint32_t(_slotSize * size_t(index));

	auto blit = [&](uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t sendEvent) {
		_xcb->xcb_shm_put_image(c, _window, _gc,
				uint16_t(_extent.width), uint16_t(_extent.height), // total size
				x, y, w, h, // source rect inside the slot
				int16_t(x), int16_t(y), // destination in the window
				_depth, XCB_IMAGE_FORMAT_Z_PIXMAP, sendEvent, _segment, offset);
	};

	if (damage.empty()) {
		// Empty means "everything changed", the same contract the gAPI present uses.
		blit(0, 0, uint16_t(_extent.width), uint16_t(_extent.height), 1);
	} else {
		// Only the last request asks for a completion event: they are processed in order, so the
		// final one answering is proof the server is done with the whole slot, and one event per
		// damage rectangle would just be noise to filter.
		for (uint32_t i = 0; i < uint32_t(damage.size()); ++i) {
			auto &it = damage[i];
			blit(uint16_t(it.x), uint16_t(it.y), uint16_t(it.width), uint16_t(it.height),
					(i + 1 == uint32_t(damage.size())) ? 1 : 0);
		}
	}

	// Busy from the blit, never from the acquire: a completion event only ever arrives for a slot
	// that was actually submitted.
	setBufferBusy(index);

	// The flush lives in XcbWindow::handleFramePresented, which the engine calls right after this.
	return Status::Ok;
}

void XcbSoftwareSwapchain::handleCompletion(uint32_t offset) {
	if (_slotSize == 0) {
		return;
	}
	setBufferFree(uint32_t(offset / _slotSize));
}

void XcbSoftwareSwapchain::invalidate() {
	_invalid = true;
	_window = 0;
}

void XcbSoftwareSwapchain::teardown() {
	if (_connection && _segment) {
		_connection->detachShmSwapchain(_segment);

		auto c = _connection->getConnection();
		_xcb->xcb_shm_detach(c, _segment);
		if (_gc) {
			_xcb->xcb_free_gc(c, _gc);
			_gc = 0;
		}
		// The detach has to reach the server before the mapping goes: it is still reading from
		// this segment until it processes the request.
		_xcb->xcb_flush(c);
		_segment = 0;
	}

	_buffers.clear();
	_busy.clear();

	if (_mapping) {
		::munmap(_mapping, _mappingSize);
		_mapping = nullptr;
		_mappingSize = 0;
	}
}

} // namespace sprt::window
