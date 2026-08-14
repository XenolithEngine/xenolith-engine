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

#include "SPRTWinEmboxWindow.h"

#if SPRT_EMBOX

#include "SPRTWinEmboxController.h"
#include <sprt/runtime/log.h>

#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

namespace sprt::window {

static constexpr const char *s_fbPath = "/dev/fb0";

EmboxSoftwareSurface::~EmboxSoftwareSurface() { invalidate(); }

bool EmboxSoftwareSurface::init(NotNull<EmboxWindow> window) {
	if (!window->getMapping() || window->getFd() < 0) {
		return false;
	}
	_owner = window;
	return true;
}

SurfaceInfo EmboxSoftwareSurface::getSurfaceOptions(SurfaceInfo &&info) const {
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

Rc<SoftwareSwapchain> EmboxSoftwareSurface::makeSwapchain(const SoftwareSwapchainInfo &info) {
	if (!_owner) {
		return nullptr;
	}
	return Rc<EmboxSoftwareSwapchain>::create(_owner, info);
}

void EmboxSoftwareSurface::invalidate() { _owner = nullptr; }

EmboxSoftwareSwapchain::~EmboxSoftwareSwapchain() { invalidate(); }

bool EmboxSoftwareSwapchain::init(NotNull<EmboxWindow> window, const SoftwareSwapchainInfo &info) {
	if (info.format != ImageFormat::B8G8R8A8_UNORM) {
		oslog::vperror(__SPRT_LOCATION, "EmboxSoftwareSwapchain", "Unsupported format: ",
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
		oslog::vperror(__SPRT_LOCATION, "EmboxSoftwareSwapchain",
				"Framebuffer mapping is smaller than the swapchain extent");
		return false;
	}

	_shadow = static_cast<uint8_t *>(::malloc(_shadowSize));
	if (!_shadow) {
		oslog::vperror(__SPRT_LOCATION, "EmboxSoftwareSwapchain",
				"Failed to allocate ", _shadowSize, "-byte present shadow");
		return false;
	}

	_buffers.emplace_back(SoftwareBuffer{_shadow, stride, _shadowSize});
	_busy.resize(1, false);
	return true;
}

Status EmboxSoftwareSwapchain::present(uint32_t index, SpanView<geom::URect> damage) {
	if (_invalid || !_owner || !_shadow || index != 0) {
		return Status::ErrorCancelled;
	}

	struct {
		uint32_t x = 0;
		uint32_t y = 0;
		uint32_t w = 0;
		uint32_t h = 0;
	} area;
	if (damage.empty()) {
		area.x = 0;
		area.y = 0;
		area.w = uint32_t(_extent.width);
		area.h = uint32_t(_extent.height);
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
		area.x = uint32_t(x0);
		area.y = uint32_t(y0);
		area.w = uint32_t(x1 > x0 ? x1 - x0 : 0);
		area.h = uint32_t(y1 > y0 ? y1 - y0 : 0);
	}

	auto *dst = _owner->getMapping();
	const uint32_t stride = _owner->getStride();
	if (damage.empty()) {
		::memcpy(dst, _shadow, _shadowSize);
	} else if (area.w > 0 && area.h > 0) {
		const size_t rowBytes = size_t(area.w) * 4;
		const size_t xOff = size_t(area.x) * 4;
		for (uint32_t row = 0; row < area.h; ++row) {
			const size_t off = size_t(area.y + row) * stride + xOff;
			::memcpy(dst + off, _shadow + off, rowBytes);
		}
	}


#ifdef FBIO_UPDATE
	// Some scanouts need FBIO_UPDATE; live mappings return ENOTTY/ENOSYS
	// and are already visible — do not fail the present.
	if (::ioctl(_owner->getFd(), FBIO_UPDATE, (unsigned long)(uintptr_t)&area) < 0
			&& errno != ENOTTY && errno != ENOSYS) {
		oslog::vperror(__SPRT_LOCATION, "EmboxSoftwareSwapchain",
				"FBIO_UPDATE failed: ", errno);
		return Status::ErrorUnknown;
	}
#else
	(void)area;
#endif
	static bool s_loggedFirstPresent = false;
	if (!s_loggedFirstPresent) {
		s_loggedFirstPresent = true;
		oslog::vpinfo(__SPRT_LOCATION, "EmboxSoftwareSwapchain", "first present ", _extent.width,
				"x", _extent.height, " shadow=", _shadowSize);
	}
	return Status::Ok;
}

void EmboxSoftwareSwapchain::invalidate() {
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

EmboxWindow::~EmboxWindow() { teardown(); }

EmboxWindow::EmboxWindow() { }

bool EmboxWindow::init(NotNull<EmboxContextController> c, Rc<WindowInfo> &&info) {
	_fd = ::open(s_fbPath, O_RDWR);
	if (_fd < 0) {
		oslog::vperror(__SPRT_LOCATION, "EmboxWindow", "open(", s_fbPath, ") failed: ", errno);
		return false;
	}

	struct fb_var_screeninfo vinfo = {};
	struct fb_fix_screeninfo finfo = {};
	if (::ioctl(_fd, FBIOGET_VSCREENINFO, (unsigned long)(uintptr_t)&vinfo) < 0
			|| ::ioctl(_fd, FBIOGET_FSCREENINFO, (unsigned long)(uintptr_t)&finfo) < 0) {
		oslog::vperror(__SPRT_LOCATION, "EmboxWindow", "FBIOGET_* failed: ", errno);
		teardown();
		return false;
	}

	if (vinfo.bits_per_pixel != 32) {
		oslog::vperror(__SPRT_LOCATION, "EmboxWindow",
				"Framebuffer is not 32-bit RGB (bpp=", 
				uint32_t(vinfo.bits_per_pixel), "); the software rasterizer produces B8G8R8A8");
		teardown();
		return false;
	}

	auto mapping = ::mmap(nullptr, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FILE, _fd,
			0);
	if (mapping == MAP_FAILED || mapping == nullptr) {
		oslog::vperror(__SPRT_LOCATION, "EmboxWindow", "mmap(", s_fbPath, ") failed: ", errno);
		teardown();
		return false;
	}

	_mapping = reinterpret_cast<uint8_t *>(mapping);
	_mappingSize = finfo.smem_len;
	_stride = finfo.line_length;
	_extent = Extent2(vinfo.xres, vinfo.yres);

	info->rect.width = _extent.width;
	info->rect.height = _extent.height;
	if (info->imageFormat == ImageFormat::Undefined) {
		info->imageFormat = ImageFormat::B8G8R8A8_UNORM;
	}

	oslog::vpinfo(__SPRT_LOCATION, "EmboxWindow", s_fbPath, " ", _extent.width, "x", _extent.height,
			" stride=", _stride);

	return NativeWindow::init(c, sprt::move(info), WindowCapabilities::None);
}

bool EmboxWindow::close() {
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

SurfaceInterfaceInfo EmboxWindow::getSurfaceInterfaceInfo() const {
	SurfaceInterfaceInfo ret;
	ret.backend = SurfaceBackend::Surface;
	return ret;
}

SurfaceInfo EmboxWindow::getSurfaceOptions(SurfaceInfo &&info) const {
	info.currentExtent = _extent;
	info.minImageExtent = Extent2(1, 1);
	info.maxImageExtent = _extent;
	return sprt::move(info);
}

Rc<SoftwareSurface> EmboxWindow::makeSoftwareSurface() {
	return Rc<EmboxSoftwareSurface>::create(this);
}

PresentationOptions EmboxWindow::getPreferredOptions() const {
	PresentationOptions opts;
	// Keep the director ticking so the first (and only) scene presents without
	// a mouse/input wake-up. Embox has no pointer; render-on-demand would leave
	// the framebuffer at the clear color forever.
	opts.renderOnDemand = false;
	opts.followDisplayLink = false;
	opts.followDisplayLinkBarrier = false;
	opts.usePresentWindow = false;
	opts.acquireImageWithoutFence = true;
	return opts;
}

void EmboxWindow::teardown() {
	// Do not munmap or close /dev/fb0. On bcm2711 the last close blanks HDMI,
	// and process exit is exactly that last close. Scanout stays mapped for
	// the life of the process; virtio-gpu is the same path.
}

} // namespace sprt::window

#endif // SPRT_EMBOX
