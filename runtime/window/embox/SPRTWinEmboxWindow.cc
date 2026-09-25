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

#if SPRT_EMBOX_ANY

#include "SPRTWinEmboxController.h"
#include <sprt/runtime/log.h>

#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
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

	// CPU-composed frames clear-then-draw; that must stay off-screen or the
	// beam sees black stripes. RGA video writes the scanout directly and
	// present() skips the copy.
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

/* RGA2 blit (weak). Swap 2 = BGRA both sides. */
extern "C" __attribute__((weak)) int rga2_blit(uintptr_t dst, uint32_t dst_stride, uint32_t dst_swap,
		int dx, int dy, int dw, int dh, uintptr_t src, uint32_t src_stride, uint32_t src_swap,
		int sx, int sy, int sw, int sh);

/* fb_dev.c XENOLITH_FB_FLUSH_CACHE ('F', 0x30). rk3588_simplefb is cacheable and the scanout is
 * not IO-coherent; ENOSYS means there is no such ioctl (QEMU ramfb) and nothing to clean. */
static constexpr int XenolithFbFlushCache = 0x4630;

static void flushScanout(int fd, void *ptr, size_t len) {
	if (len == 0) {
		return;
	}
	struct FlushRange {
		void *ptr;
		size_t len;
	} flush = {ptr, len};
	(void)::ioctl(fd, XenolithFbFlushCache, (unsigned long)(uintptr_t)&flush);
}

static int rgaPresentCopy(uint8_t *dst, const uint8_t *shadow, uint32_t w, uint32_t h,
		uint32_t stride) {
	if (!rga2_blit) {
		return -1;
	}
	return rga2_blit(uintptr_t(dst), stride, 2, 0, 0, int(w), int(h),
			uintptr_t(shadow), stride, 2, 0, 0, int(w), int(h));
}

/* Soft backend: last frame already landed in scanout via RGA (weak). */
extern "C" __attribute__((weak)) int xenolith_soft_rga_direct_presented(void);

/* Scanout mapping for RGA video, or 0 if the fb cannot take direct writes. */
static EmboxWindow *s_theScanoutWindow = nullptr;

extern "C" uintptr_t xenolith_soft_scanout_fb(uint32_t *stride) {
	if (s_theScanoutWindow && s_theScanoutWindow->useDirectScanout()) {
		if (stride) {
			*stride = s_theScanoutWindow->getStride();
		}
		return reinterpret_cast<uintptr_t>(s_theScanoutWindow->getMapping());
	}
	return 0;
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

	/* RGA already wrote the visible frame into scanout; still need a cache clean. */
	if (xenolith_soft_rga_direct_presented && xenolith_soft_rga_direct_presented()) {
		flushScanout(_owner->getFd(), dst, _shadowSize);
		return Status::Ok;
	}

	if (damage.empty()) {
		// CPU memcpy of 8.3MB + flush ioctl is 15-25ms at -O0; rga2_blit
		// does the cache clean/invalidate itself.
		if (rgaPresentCopy(dst, _shadow, uint32_t(_extent.width), uint32_t(_extent.height), stride)
				== 0) {
			return Status::Ok;
		}
		/* fall through to the CPU path on error */
	}
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

	// Uncached 1080p writes cost 30-80ms, so the mapping is cached and cleaned by hand instead.
	if (damage.empty()) {
		flushScanout(_owner->getFd(), dst, _shadowSize);
	} else if (area.w > 0 && area.h > 0) {
		flushScanout(_owner->getFd(), dst + size_t(area.y) * stride + size_t(area.x) * 4,
				size_t(area.h - 1) * stride + size_t(area.w) * 4);
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

	// Probe the flush ioctl: success means cacheable DRAM scanout (rk3588).
	// ENOSYS (QEMU ramfb) and firmware-composited fb stay on the shadow path.
	// XL_DIRECT_FB=0 forces the shadow.
	struct FlushProbe {
		void *ptr;
		size_t len;
	} flushProbe = {nullptr, 0};
	_directScanout = ::ioctl(_fd, XenolithFbFlushCache, (unsigned long)(uintptr_t)&flushProbe) == 0;
	if (const char *env = ::getenv("XL_DIRECT_FB")) {
		if (strcmp(env, "0") == 0) {
			_directScanout = false;
		} else if (strcmp(env, "1") == 0) {
			_directScanout = true;
		}
	}
	s_theScanoutWindow = this;

	info->rect.width = _extent.width;
	info->rect.height = _extent.height;
	if (info->imageFormat == ImageFormat::Undefined) {
		info->imageFormat = ImageFormat::B8G8R8A8_UNORM;
	}

	oslog::vpinfo(__SPRT_LOCATION, "EmboxWindow", s_fbPath, " ", _extent.width, "x", _extent.height,
			" stride=", _stride, _directScanout ? " direct-scanout" : " shadow");

	// The same capability EmboxContextController::getCapabilities() advertises, and only when this
	// fb actually took the flush ioctl - a firmware-composited scanout must not claim it.
	auto caps = _directScanout ? WindowCapabilities::DirectOutput : WindowCapabilities::None;
	if (!NativeWindow::init(c, sprt::move(info), caps)) {
		teardown(); // drops s_theScanoutWindow; the fd and the mapping stay by design
		return false;
	}

	startUartInput();
	return true;
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
	stopUartInput();
	if (s_theScanoutWindow == this) {
		s_theScanoutWindow = nullptr;
	}

	// Do not munmap or close /dev/fb0. On bcm2711 the last close blanks HDMI,
	// and process exit is exactly that last close. Scanout stays mapped for
	// the life of the process; virtio-gpu is the same path.
}

namespace {

constexpr uint64_t s_uartPollUs = 20'000; // select timeout; also the release tick

/* UART has no key-release. 350ms covers Terminal.app's ~500ms repeat delay;
 * shorter drops held keys. XL_UART_HOLD_US overrides. */
uint64_t s_uartKeyHoldUs = 350'000;

constexpr uint64_t s_uartSeqTimeoutUs = 50'000; // a partial escape sequence gives up

uint64_t uartNowUs() {
	struct timeval tv = {};
	::gettimeofday(&tv, nullptr);
	return uint64_t(tv.tv_sec) * 1'000'000ull + uint64_t(tv.tv_usec);
}

/* HID usage bytes from xenolith_embox_key_byte. */
constexpr size_t kHidKeyRing = 64;
uint8_t s_hidRing[kHidKeyRing];
sprt::atomic<uint32_t> s_hidHead{0};
sprt::atomic<uint32_t> s_hidTail{0};

/* HID make/break from the OHCI IRQ. Packed usage | (down<<8).
 * Must not use the UART auto-release hold (stuck d-pad / replayed Enter). */
constexpr size_t kHidEvtRing = 64;
uint16_t s_hidEvt[kHidEvtRing];
sprt::atomic<uint32_t> s_hidEvtHead{0};
sprt::atomic<uint32_t> s_hidEvtTail{0};

InputKeyCode hidUsageToCode(uint8_t u) {
	switch (u) {
	case 0x28: return InputKeyCode::ENTER;
	case 0x2C: return InputKeyCode::SPACE;
	case 0x29: return InputKeyCode::ESCAPE;
	case 0x4F: return InputKeyCode::RIGHT;
	case 0x50: return InputKeyCode::LEFT;
	case 0x51: return InputKeyCode::DOWN;
	case 0x52: return InputKeyCode::UP;
	case 0x3A: return InputKeyCode::F1;
	default: break;
	}
	if (u >= 0x04 && u <= 0x1D) {
		return InputKeyCode(uint16_t('A' + (u - 0x04)));
	}
	return InputKeyCode::Unknown;
}

} // namespace

extern "C" void xenolith_embox_key_byte(uint8_t b) {
	auto h = s_hidHead.load();
	auto n = (h + 1u) % uint32_t(kHidKeyRing);
	if (n == s_hidTail.load()) {
		return;
	}
	s_hidRing[h] = b;
	s_hidHead.store(n);
}

extern "C" void xenolith_embox_hid_key(uint8_t usage, int down) {
	auto h = s_hidEvtHead.load();
	auto n = (h + 1u) % uint32_t(kHidEvtRing);
	if (n == s_hidEvtTail.load()) {
		return;
	}
	s_hidEvt[h] = uint16_t(usage) | (down ? 0x100u : 0);
	s_hidEvtHead.store(n);
}

void *EmboxWindow::uartInputThread(void *arg) {
	static_cast<EmboxWindow *>(arg)->uartInputLoop();
	return nullptr;
}

void EmboxWindow::startUartInput() {
	if (::getenv("XL_UART_KEYS") && strcmp(::getenv("XL_UART_KEYS"), "0") == 0) {
		oslog::vpinfo(__SPRT_LOCATION, "EmboxWindow", "uart keys disabled (XL_UART_KEYS=0)");
		return;
	}
	if (const char *env = ::getenv("XL_UART_HOLD_US")) {
		auto v = strtoull(env, nullptr, 10);
		if (v >= 20'000 && v <= 2'000'000) {
			s_uartKeyHoldUs = v;
		}
	}
	oslog::vpinfo(__SPRT_LOCATION, "EmboxWindow", "uart keys: hold=", s_uartKeyHoldUs, "us");

	// Canonical stdin never delivers bytes without a newline; pad keys have none. stdin is process
	// state, not ours: keep the original so stopUartInput can hand the console back.
	struct termios tio = {};
	if (::tcgetattr(STDIN_FILENO, &tio) == 0) {
		_uartSavedTermios = tio;
		_uartTermiosSaved = true;
		tio.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
		tio.c_cc[VMIN] = 1;
		tio.c_cc[VTIME] = 0;
		::tcsetattr(STDIN_FILENO, TCSANOW, &tio);
	} else {
		oslog::vpwarn(__SPRT_LOCATION, "EmboxWindow", "tcgetattr(stdin) failed: ", errno,
				" - keys need a newline");
	}
	_uartSavedFlags = ::fcntl(STDIN_FILENO, F_GETFL, 0);

	pthread_t thread = 0;
	pthread_attr_t attr;
	::pthread_attr_init(&attr);
	size_t stackSize = 256 * 1024; // default Embox stacks are 128 KiB
	::pthread_attr_setstacksize(&attr, stackSize);
	_uartRunning = true;
	if (::pthread_create(&thread, &attr, &EmboxWindow::uartInputThread, this) != 0) {
		_uartRunning = false;
		oslog::vperror(__SPRT_LOCATION, "EmboxWindow", "uart input thread failed: ", errno);
	}
	::pthread_attr_destroy(&attr);
	_uartThread = reinterpret_cast<void *>(thread);
}

void EmboxWindow::stopUartInput() {
	if (!_uartThread) {
		return;
	}
	_uartRunning = false;
	::pthread_join(reinterpret_cast<pthread_t>(_uartThread), nullptr);
	_uartThread = nullptr;

	// Only after the reader is gone: it owns stdin until then.
	if (_uartSavedFlags >= 0) {
		::fcntl(STDIN_FILENO, F_SETFL, _uartSavedFlags);
		_uartSavedFlags = -1;
	}
	if (_uartTermiosSaved) {
		::tcsetattr(STDIN_FILENO, TCSANOW, &_uartSavedTermios);
		_uartTermiosSaved = false;
	}
}

void EmboxWindow::uartInputLoop() {
	uint8_t buf[32];
	if (_uartSavedFlags >= 0) {
		::fcntl(STDIN_FILENO, F_SETFL, _uartSavedFlags | O_NONBLOCK);
	}
	oslog::vpinfo(__SPRT_LOCATION, "EmboxWindow", "uart input loop running");
	while (_uartRunning.load()) {
		uint64_t now = uartNowUs();
		/* A non-blocking read plus a sleep, not select(): embox select() can ignore its timeout and
		 * block on the UART, which would starve the HID rings below. */
		for (;;) {
			auto t = s_hidEvtTail.load();
			if (t == s_hidEvtHead.load()) {
				break;
			}
			uint16_t v = s_hidEvt[t];
			s_hidEvtTail.store((t + 1u) % uint32_t(kHidEvtRing));
			auto code = hidUsageToCode(uint8_t(v));
			if (code != InputKeyCode::Unknown) {
				hidPost(code, (v & 0x100) ? InputEventName::KeyPressed
						: InputEventName::KeyReleased);
			}
		}
		for (;;) {
			auto t = s_hidTail.load();
			if (t == s_hidHead.load()) {
				break;
			}
			uint8_t b = s_hidRing[t];
			s_hidTail.store((t + 1u) % uint32_t(kHidKeyRing));
			uartFeed(b, now);
		}
		ssize_t n = ::read(STDIN_FILENO, buf, sizeof(buf));
		if (n > 0) {
			for (ssize_t i = 0; i < n; ++i) {
				uartFeed(buf[i], now);
			}
		} else {
			uartFlushSequence(now);
		}
		uartExpireHeld(now);
		::usleep(unsigned(s_uartPollUs / 4)); // no useconds_t in the sprt libc (EL0)
	}
	uartPostCancel();
}

void EmboxWindow::uartFeed(uint8_t byte, uint64_t nowUs) {
	if (_uartSeqLen > 0) {
		if (_uartSeqLen < sizeof(_uartSeq)) {
			_uartSeq[_uartSeqLen++] = byte;
		} else {
			uartFlushSequence(nowUs);
			return;
		}
		if (_uartSeqLen == 2 && byte != '[' && byte != 'O') {
			// ESC + non-CSI/SS3: ESC is its own key; re-feed the second byte.
			uint8_t second = byte;
			_uartSeqLen = 0;
			uartPost(InputKeyCode::ESCAPE, InputEventName::KeyPressed);
			uartFeed(second, nowUs);
			return;
		}
		// CSI ends on a letter; SS3 ends on 'P'..'S'.
		if ((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') || byte == '~') {
			uartFlushSequence(nowUs);
		}
		return;
	}

	if (byte == 0x1b) {
		_uartSeq[0] = byte;
		_uartSeqLen = 1;
		_uartSeqStartUs = nowUs;
		return;
	}

	InputKeyCode code = InputKeyCode::Unknown;
	InputEventName event = InputEventName::KeyPressed;
	switch (byte) {
	case '\r':
	case '\n': code = InputKeyCode::ENTER; break;
	case '\t': code = InputKeyCode::TAB; break;
	case 0x7f:
	case 0x08: code = InputKeyCode::BACKSPACE; break;
	default:
		// InputKeyCode is ASCII-aligned; terminals send lowercase for unshifted letters.
		if (byte >= 0x20 && byte < 0x7f) {
			code = InputKeyCode(byte >= 'a' && byte <= 'z' ? byte - 'a' + 'A' : byte);
		}
		break;
	}
	if (code != InputKeyCode::Unknown) {
		uartPost(code, event);
	}
}

void EmboxWindow::uartFlushSequence(uint64_t nowUs) {
	if (_uartSeqLen == 0) {
		return;
	}

	if (_uartSeqLen == 1) {
		if (nowUs - _uartSeqStartUs < s_uartSeqTimeoutUs) {
			return; // still waiting for the next byte
		}
		uartPost(InputKeyCode::ESCAPE, InputEventName::KeyPressed);
		_uartSeqLen = 0;
		return;
	}

	InputKeyCode code = InputKeyCode::Unknown;
	const uint8_t *seq = _uartSeq;
	if (seq[1] == '[' && _uartSeqLen == 3) {
		switch (seq[2]) {
		case 'A': code = InputKeyCode::UP; break;
		case 'B': code = InputKeyCode::DOWN; break;
		case 'C': code = InputKeyCode::RIGHT; break;
		case 'D': code = InputKeyCode::LEFT; break;
		case 'H': code = InputKeyCode::HOME; break;
		case 'F': code = InputKeyCode::END; break;
		default: break;
		}
	} else if (seq[1] == '[' && _uartSeqLen >= 4 && seq[_uartSeqLen - 1] == '~') {
		uint32_t num = 0;
		for (size_t i = 2; i < _uartSeqLen - 1; ++i) {
			if (seq[i] < '0' || seq[i] > '9') {
				num = 0;
				break;
			}
			num = num * 10 + uint32_t(seq[i] - '0');
		}
		switch (num) {
		case 1: code = InputKeyCode::HOME; break;
		case 3: code = InputKeyCode::INSERT; break; // terminals disagree; DELETE has no ASCII slot
		case 4: code = InputKeyCode::END; break;
		case 5: code = InputKeyCode::PAGE_UP; break;
		case 6: code = InputKeyCode::PAGE_DOWN; break;
		case 11: code = InputKeyCode::F1; break;
		case 12: code = InputKeyCode::F2; break;
		case 13: code = InputKeyCode::F3; break;
		case 14: code = InputKeyCode::F4; break;
		case 15: code = InputKeyCode::F5; break;
		case 17: code = InputKeyCode::F6; break;
		case 18: code = InputKeyCode::F7; break;
		case 19: code = InputKeyCode::F8; break;
		default: break;
		}
	} else if (seq[1] == 'O' && _uartSeqLen == 3) {
		switch (seq[2]) {
		case 'P': code = InputKeyCode::F1; break;
		case 'Q': code = InputKeyCode::F2; break;
		case 'R': code = InputKeyCode::F3; break;
		case 'S': code = InputKeyCode::F4; break;
		default: break;
		}
	}

	if (code != InputKeyCode::Unknown) {
		uartPost(code, InputEventName::KeyPressed);
	}
	_uartSeqLen = 0;
}

void EmboxWindow::uartExpireHeld(uint64_t nowUs) {
	size_t w = 0;
	for (size_t i = 0; i < _uartHeldCount; ++i) {
		if (nowUs - _uartHeld[i].lastSeenUs < s_uartKeyHoldUs) {
			_uartHeld[w++] = _uartHeld[i];
		} else {
			InputKeyCode code = _uartHeld[i].keycode;
			// Drop from the array first: uartPost callbacks inspect held state.
			_uartHeld[i] = UartHeldKey{};
			uartPost(code, InputEventName::KeyReleased);
		}
	}
	_uartHeldCount = w;
}

void EmboxWindow::hidPost(InputKeyCode code, InputEventName event) {
	if (code == InputKeyCode::Unknown) {
		return;
	}
	/* HID has real make/break; the UART hold timer is for serial taps only. */
	if (event == InputEventName::KeyReleased) {
		size_t w = 0;
		for (size_t i = 0; i < _uartHeldCount; ++i) {
			if (_uartHeld[i].keycode != code) {
				_uartHeld[w++] = _uartHeld[i];
			}
		}
		_uartHeldCount = w;
	}
	auto ev = InputEventData();
	ev.event = event;
	ev.key.keycode = code;
	ev.key.keysym = 0;
	ev.key.keychar = 0;
	auto looper = _controller->getLooper();
	if (looper) {
		looper->performOnThread([this, ev] {
			if (_closed) {
				return;
			}
			Vector<InputEventData> events;
			events.emplace_back(ev);
			handleInputEvents(sprt::move(events));
		}, this);
	}
}

void EmboxWindow::uartPost(InputKeyCode code, InputEventName event) {
	if (code == InputKeyCode::Unknown) {
		return;
	}

	uint64_t nowUs = uartNowUs();
	if (event == InputEventName::KeyPressed || event == InputEventName::KeyRepeated) {
		bool held = false;
		for (size_t i = 0; i < _uartHeldCount; ++i) {
			if (_uartHeld[i].keycode == code) {
				_uartHeld[i].lastSeenUs = nowUs;
				held = true;
				if (event == InputEventName::KeyPressed) {
					event = InputEventName::KeyRepeated; // terminal auto-repeat, not a new press
				}
				break;
			}
		}
		if (!held && _uartHeldCount < sizeof(_uartHeld) / sizeof(_uartHeld[0])) {
			_uartHeld[_uartHeldCount++] = UartHeldKey{code, nowUs};
		}
	}

	auto ev = InputEventData();
	ev.event = event;
	ev.key.keycode = code;
	ev.key.keysym = 0;
	ev.key.keychar = 0;

	// XL_UART_DIRECT=1 posts from this thread; it can deadlock against the
	// render/app threads (ESC -> pause drawer hang). Default is the looper hop.
	static const bool s_direct = [] {
		const char *env = ::getenv("XL_UART_DIRECT");
		return env && strcmp(env, "1") == 0;
	}();

	if (s_direct && !_handleTextInputFromKeyboard) {
		Vector<InputEventData> events;
		events.emplace_back(ev);
		handleInputEvents(sprt::move(events));
		return;
	}

	// handleInputEvents is looper-thread; performOnThread retains this.
	auto looper = _controller->getLooper();
	if (looper) {
		looper->performOnThread([this, ev] {
			if (_closed) {
				return;
			}
			Vector<InputEventData> events;
			events.emplace_back(ev);
			handleInputEvents(sprt::move(events));
		}, this);
	}
}

void EmboxWindow::uartPostCancel() {
	// Cancel held keys on thread exit (not KeyReleased).
	for (size_t i = 0; i < _uartHeldCount; ++i) {
		auto ev = InputEventData();
		ev.event = InputEventName::KeyCanceled;
		ev.key.keycode = _uartHeld[i].keycode;
		ev.key.keysym = 0;
		ev.key.keychar = 0;
		auto looper = _controller ? _controller->getLooper() : nullptr;
		if (looper) {
			looper->performOnThread([this, ev] {
				if (_closed) {
					return;
				}
				Vector<InputEventData> events;
				events.emplace_back(ev);
				handleInputEvents(sprt::move(events));
			}, this);
		}
	}
	_uartHeldCount = 0;
}

} // namespace sprt::window

#endif // SPRT_EMBOX_ANY
