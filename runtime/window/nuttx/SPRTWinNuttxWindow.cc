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

#include "SPRTWinNuttxWindow.h"

#if SPRT_NUTTX

#include "SPRTWinNuttxController.h"
#include <sprt/runtime/log.h>

#include <nuttx/config.h>
#include <nuttx/cache.h>
#include <nuttx/video/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

namespace sprt::window {

static constexpr const char *s_fbPath = "/dev/fb0";

NuttxSoftwareSurface::~NuttxSoftwareSurface() { invalidate(); }

bool NuttxSoftwareSurface::init(NotNull<NuttxWindow> window) {
	if (!window->getMapping() || window->getFd() < 0) {
		return false;
	}
	_owner = window;
	return true;
}

SurfaceInfo NuttxSoftwareSurface::getSurfaceOptions(SurfaceInfo &&info) const {
	info.minImageCount = 1;
	info.maxImageCount = 1;
	info.currentExtent = _owner ? _owner->getExtent() : Extent2(0, 0);
	info.minImageExtent = Extent2(1, 1);
	info.maxImageExtent = info.currentExtent;
	info.formats.emplace_back(ImageFormat::B8G8R8A8_UNORM, ColorSpace::SRGB_NONLINEAR_KHR);
	info.presentModes.emplace_back(PresentMode::Immediate);
	info.presentModes.emplace_back(PresentMode::Fifo);
	return sprt::move(info);
}

Rc<SoftwareSwapchain> NuttxSoftwareSurface::makeSwapchain(const SoftwareSwapchainInfo &info) {
	if (!_owner) {
		return nullptr;
	}
	return Rc<NuttxSoftwareSwapchain>::create(_owner, info);
}

void NuttxSoftwareSurface::invalidate() { _owner = nullptr; }

NuttxSoftwareSwapchain::~NuttxSoftwareSwapchain() { invalidate(); }

bool NuttxSoftwareSwapchain::init(NotNull<NuttxWindow> window, const SoftwareSwapchainInfo &info) {
	if (info.format != ImageFormat::B8G8R8A8_UNORM) {
		oslog::vperror(__SPRT_LOCATION, "NuttxSoftwareSwapchain", "Unsupported format: ",
				uint32_t(info.format));
		return false;
	}
	if (!window->getMapping() || info.extent.width == 0 || info.extent.height == 0) {
		return false;
	}

	_owner = window;
	_extent = info.extent;

	auto stride = window->getStride();
	_shadowSize = size_t(stride) * size_t(info.extent.height);
	if (window->getMappingSize() < _shadowSize) {
		oslog::vperror(__SPRT_LOCATION, "NuttxSoftwareSwapchain",
				"Framebuffer mapping is smaller than the swapchain extent");
		return false;
	}

	_shadow = static_cast<uint8_t *>(::malloc(_shadowSize));
	if (!_shadow) {
		oslog::vperror(__SPRT_LOCATION, "NuttxSoftwareSwapchain",
				"Failed to allocate ", _shadowSize, "-byte present shadow");
		return false;
	}

	_buffers.emplace_back(SoftwareBuffer{_shadow, stride, _shadowSize});
	_busy.resize(1, false);
	return true;
}

Status NuttxSoftwareSwapchain::present(uint32_t index, SpanView<geom::URect> damage) {
	if (_invalid || !_owner || !_shadow || index != 0) {
		return Status::ErrorCancelled;
	}

	struct fb_area_s area;
	if (damage.empty()) {
		area.x = 0;
		area.y = 0;
		area.w = fb_coord_t(_extent.width);
		area.h = fb_coord_t(_extent.height);
	} else {
		uint32_t x0 = _extent.width;
		uint32_t y0 = _extent.height;
		uint32_t x1 = 0;
		uint32_t y1 = 0;
		for (auto &it : damage) {
			if (it.x < x0) {
				x0 = it.x;
			}
			if (it.y < y0) {
				y0 = it.y;
			}
			if (it.x + it.width > x1) {
				x1 = it.x + it.width;
			}
			if (it.y + it.height > y1) {
				y1 = it.y + it.height;
			}
		}
		if (x1 > _extent.width) {
			x1 = _extent.width;
		}
		if (y1 > _extent.height) {
			y1 = _extent.height;
		}
		area.x = fb_coord_t(x0);
		area.y = fb_coord_t(y0);
		area.w = fb_coord_t(x1 > x0 ? x1 - x0 : 0);
		area.h = fb_coord_t(y1 > y0 ? y1 - y0 : 0);
	}

	auto *dst = _owner->getMapping();
	const uint32_t stride = _owner->getStride();
	if (damage.empty()) {
		::memcpy(dst, _shadow, _shadowSize);
	} else if (area.w > 0 && area.h > 0) {
		const size_t rowBytes = size_t(area.w) * 4;
		const size_t xOff = size_t(area.x) * 4;
		for (fb_coord_t row = 0; row < area.h; ++row) {
			const size_t off = size_t(area.y + row) * stride + xOff;
			::memcpy(dst + off, _shadow + off, rowBytes);
		}
	}

#ifdef CONFIG_ARCH_DCACHE
	if (dst && _owner->getMappingSize() > 0) {
		up_flush_dcache(uintptr_t(dst), uintptr_t(dst) + _owner->getMappingSize());
	}
#endif

#ifdef FBIO_UPDATE
	// QEMU virtio-gpu needs RESOURCE_FLUSH via FBIO_UPDATE. bcm2711 mailbox
	// FB is live GPU memory (no updatearea), so the ioctl returns ENOTTY.
	// Scanout is already visible — do not fail the present.
	if (::ioctl(_owner->getFd(), FBIO_UPDATE, (unsigned long)(uintptr_t)&area) < 0
			&& errno != ENOTTY && errno != ENOSYS) {
		oslog::vperror(__SPRT_LOCATION, "NuttxSoftwareSwapchain",
				"FBIO_UPDATE failed: ", errno);
		return Status::ErrorUnknown;
	}
#else
	(void)area;
#endif
	return Status::Ok;
}

void NuttxSoftwareSwapchain::invalidate() {
	_invalid = true;
	_owner = nullptr;
	_buffers.clear();
	_busy.clear();
	if (_shadow) {
		::free(_shadow);
		_shadow = nullptr;
	}
	_shadowSize = 0;
}

NuttxWindow::~NuttxWindow() { teardown(); }

NuttxWindow::NuttxWindow() { }

bool NuttxWindow::init(NotNull<NuttxContextController> c, Rc<WindowInfo> &&info) {
	_fd = ::open(s_fbPath, O_RDWR);
	if (_fd < 0) {
		oslog::vperror(__SPRT_LOCATION, "NuttxWindow", "open(", s_fbPath, ") failed: ", errno);
		return false;
	}

	struct fb_videoinfo_s vinfo = {};
	struct fb_planeinfo_s pinfo = {};
	if (::ioctl(_fd, FBIOGET_VIDEOINFO, (unsigned long)(uintptr_t)&vinfo) < 0
			|| ::ioctl(_fd, FBIOGET_PLANEINFO, (unsigned long)(uintptr_t)&pinfo) < 0) {
		oslog::vperror(__SPRT_LOCATION, "NuttxWindow", "FBIOGET_* failed: ", errno);
		teardown();
		return false;
	}

	if (pinfo.bpp != 32
			|| (vinfo.fmt != FB_FMT_RGB32 && vinfo.fmt != FB_FMT_RGBA32)) {
		oslog::vperror(__SPRT_LOCATION, "NuttxWindow",
				"Framebuffer is not 32-bit RGB (fmt=", uint32_t(vinfo.fmt), " bpp=",
				uint32_t(pinfo.bpp), "); the software rasterizer produces B8G8R8A8");
		teardown();
		return false;
	}

	auto mapping = ::mmap(nullptr, pinfo.fblen, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FILE, _fd,
			0);
	if (mapping == MAP_FAILED) {
		oslog::vperror(__SPRT_LOCATION, "NuttxWindow", "mmap(", s_fbPath, ") failed: ", errno);
		teardown();
		return false;
	}

	_mapping = reinterpret_cast<uint8_t *>(mapping);
	_mappingSize = pinfo.fblen;
	_stride = pinfo.stride;
	_extent = Extent2(vinfo.xres, vinfo.yres);

	info->rect.width = _extent.width;
	info->rect.height = _extent.height;
	if (info->imageFormat == ImageFormat::Undefined) {
		info->imageFormat = ImageFormat::B8G8R8A8_UNORM;
	}

	oslog::vpinfo(__SPRT_LOCATION, "NuttxWindow", s_fbPath, " ", _extent.width, "x", _extent.height,
			" stride=", _stride);

	return NativeWindow::init(c, sprt::move(info), WindowCapabilities::None);
}

bool NuttxWindow::close() {
	if (_closed) {
		return true;
	}
	_closed = true;
	if (!_controller->notifyWindowClosed(this)) {
		_closed = false;
		return false;
	}
	teardown();
	return true;
}

SurfaceInterfaceInfo NuttxWindow::getSurfaceInterfaceInfo() const {
	SurfaceInterfaceInfo ret;
	ret.backend = SurfaceBackend::Surface;
	return ret;
}

SurfaceInfo NuttxWindow::getSurfaceOptions(SurfaceInfo &&info) const {
	info.currentExtent = _extent;
	info.minImageExtent = Extent2(1, 1);
	info.maxImageExtent = _extent;
	return sprt::move(info);
}

Rc<SoftwareSurface> NuttxWindow::makeSoftwareSurface() {
	return Rc<NuttxSoftwareSurface>::create(this);
}

PresentationOptions NuttxWindow::getPreferredOptions() const {
	PresentationOptions opts;
	// Keep the director ticking so the first (and only) scene presents without
	// a mouse/input wake-up. NuttX has no pointer; render-on-demand would leave
	// the framebuffer at the clear color forever.
	opts.renderOnDemand = false;
	opts.followDisplayLink = false;
	opts.followDisplayLinkBarrier = false;
	opts.usePresentWindow = false;
	opts.acquireImageWithoutFence = true;
	return opts;
}

void NuttxWindow::teardown() {
	// Do not munmap or close /dev/fb0. On bcm2711 the last close blanks HDMI,
	// and process exit is exactly that last close. Scanout stays mapped for
	// the life of the process; virtio-gpu is the same path.
}

} // namespace sprt::window

#endif // SPRT_NUTTX
