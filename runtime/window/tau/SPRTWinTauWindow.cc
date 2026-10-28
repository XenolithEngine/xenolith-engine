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

#include "SPRTWinTauWindow.h"

#if SPRT_TAU

#include "SPRTWinTauController.h"
#include <sprt/runtime/log.h>
#include <sprt/c/cross/tau_sprt/sprt_tau.h>
#include <sprt/c/sys/__sprt_poll.h>

#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

namespace sprt::window {

namespace {

constexpr uint64_t s_reconnectUs = 100'000;
constexpr int s_inputPollMs = 20; // also the release tick
constexpr uint64_t s_seqTimeoutUs = 50'000; // a partial escape sequence gives up

// 350 ms covers a terminal's repeat delay; shorter drops held keys. XL_UART_HOLD_US overrides.
uint64_t s_keyHoldUs = 350'000;

uint64_t nowUs() {
	struct timeval tv = {};
	::gettimeofday(&tv, nullptr);
	return uint64_t(tv.tv_sec) * 1'000'000ull + uint64_t(tv.tv_usec);
}

} // namespace

TauSoftwareSurface::~TauSoftwareSurface() { invalidate(); }

bool TauSoftwareSurface::init(NotNull<TauWindow> window) {
	_owner = window;
	return true;
}

SurfaceInfo TauSoftwareSurface::getSurfaceOptions(SurfaceInfo &&info) const {
	info.minImageCount = 2;
	info.maxImageCount = 2;
	info.currentExtent = _owner ? _owner->getExtent() : Extent2(0, 0);
	info.minImageExtent = Extent2(1, 1);
	info.maxImageExtent = info.currentExtent;
	info.formats.emplace_back(ImageFormat::B8G8R8A8_UNORM, ColorSpace::SRGB_NONLINEAR_KHR);
	info.presentModes.emplace_back(PresentMode::Immediate);
	info.presentModes.emplace_back(PresentMode::Fifo);
	return sprt::move(info);
}

Rc<SoftwareSwapchain> TauSoftwareSurface::makeSwapchain(const SoftwareSwapchainInfo &info) {
	if (!_owner) {
		return nullptr;
	}
	return Rc<TauSoftwareSwapchain>::create(_owner, info);
}

void TauSoftwareSurface::invalidate() { _owner = nullptr; }

TauSoftwareSwapchain::~TauSoftwareSwapchain() { invalidate(); }

bool TauSoftwareSwapchain::init(NotNull<TauWindow> window, const SoftwareSwapchainInfo &info) {
	if (info.format != ImageFormat::B8G8R8A8_UNORM) {
		oslog::vperror(__SPRT_LOCATION, "TauSoftwareSwapchain", "Unsupported format: ", uint32_t(info.format));
		return false;
	}
	if (info.extent.width == 0 || info.extent.height == 0 || info.extent.width > window->getExtent().width
			|| info.extent.height > window->getExtent().height) {
		return false;
	}

	_owner = window;
	_extent = info.extent;

	auto stride = window->getStride();
	_shadowSize = size_t(stride) * size_t(info.extent.height);
	uint32_t imageCount = info.imageCount < 1 ? 1 : (info.imageCount > 2 ? 2 : info.imageCount);
	for (uint32_t i = 0; i < imageCount; ++i) {
		_shadows[i] = static_cast<uint8_t *>(::malloc(_shadowSize));
		if (!_shadows[i]) {
			oslog::vperror(__SPRT_LOCATION, "TauSoftwareSwapchain", "Failed to allocate ", _shadowSize,
					"-byte present shadow");
			return false;
		}
		_buffers.emplace_back(SoftwareBuffer{_shadows[i], stride, _shadowSize});
	}
	_busy.resize(imageCount, false);
	return true;
}

Status TauSoftwareSwapchain::present(uint32_t index, SpanView<geom::URect> damage) {
	if (_invalid || !_owner || _buffers.empty() || index >= _buffers.size()) {
		return Status::ErrorCancelled;
	}

	uint32_t x0 = 0, y0 = 0, x1 = _extent.width, y1 = _extent.height;
	if (!damage.empty()) {
		x0 = _extent.width;
		y0 = _extent.height;
		x1 = 0;
		y1 = 0;
		for (auto &it : damage) {
			x0 = it.x < x0 ? it.x : x0;
			y0 = it.y < y0 ? it.y : y0;
			x1 = it.x + it.width > x1 ? it.x + it.width : x1;
			y1 = it.y + it.height > y1 ? it.y + it.height : y1;
		}
		// Rows from whole 16-pixel groups: an aligned row copies by words, not bytes. The shadow
		// holds the whole frame, so the extra pixels are the right ones.
		if (x1 > x0) {
			x0 &= ~15U;
			x1 = (x1 + 15U) & ~15U;
		}
		x1 = x1 > _extent.width ? _extent.width : x1;
		y1 = y1 > _extent.height ? _extent.height : y1;
	}
	// A frame without damage still goes to show(): a lost display is connected again there.
	_owner->show(_buffers[index].data, x0, y0, x1 > x0 ? x1 - x0 : 0, y1 > y0 ? y1 - y0 : 0, damage.empty());
	return Status::Ok;
}

void TauSoftwareSwapchain::invalidate() {
	_invalid = true;
	_owner = nullptr;
	_buffers.clear();
	_busy.clear();
	for (auto &s : _shadows) {
		if (s) {
			::free(s);
			s = nullptr;
		}
	}
	_shadowSize = 0;
}

TauWindow::~TauWindow() { teardown(); }

TauWindow::TauWindow() { }

bool TauWindow::connect(bool wait) {
	struct __sprt_tau_display_info in = {};
	int st = __sprt_tau_display_open(wait ? 1 : 0, &in);
	if (st < 0) {
		if (wait) {
			oslog::vperror(__SPRT_LOCATION, "TauWindow", "no display: ", -st);
		}
		return false;
	}
	if (in.format != __SPRT_TAU_DISPLAY_B8G8R8A8) {
		oslog::vperror(__SPRT_LOCATION, "TauWindow", "the display is not B8G8R8A8: ", in.format);
		__sprt_tau_display_close();
		return false;
	}
	void *data[2] = {nullptr, nullptr};
	unsigned long size = 0;
	if (__sprt_tau_display_buffer(0, &data[0], &size) < 0 || __sprt_tau_display_buffer(1, &data[1], &size) < 0) {
		return false;
	}
	if (_extent.width == 0) {
		_extent = Extent2(in.width, in.height);
		_stride = in.stride;
	} else if (in.width != _extent.width || in.height != _extent.height || in.stride != _stride) {
		oslog::vperror(__SPRT_LOCATION, "TauWindow", "the display came back as ", in.width, "x", in.height,
				" stride ", in.stride, ", the window is ", _extent.width, "x", _extent.height);
		__sprt_tau_display_close();
		return false;
	}
	_buffers[0] = static_cast<uint8_t *>(data[0]);
	_buffers[1] = static_cast<uint8_t *>(data[1]);
	_next = 0;
	_wholeLeft = 2;
	return true;
}

void TauWindow::show(const uint8_t *frame, uint32_t x, uint32_t y, uint32_t w, uint32_t h, bool whole) {
	if (_lost) {
		uint64_t now = nowUs();
		if (now - _lostTriedUs < s_reconnectUs) {
			return;
		}
		_lostTriedUs = now;
		if (!connect(false)) {
			return;
		}
		_lost = false;
		oslog::vpinfo(__SPRT_LOCATION, "TauWindow", "the display is back");
	}
	whole = whole || _wholeLeft > 0;
	if (!whole && (w == 0 || h == 0)) {
		return;
	}
	uint8_t *target = _buffers[_next];
	const Rect own = whole ? Rect{0, 0, _extent.width, _extent.height} : Rect{x, y, w, h};
	if (whole) {
		x = y = 0;
		w = _extent.width;
		h = _extent.height;
		::memcpy(target, frame, size_t(_stride) * h);
		_wholeLeft = _wholeLeft > 0 ? _wholeLeft - 1 : 0;
	} else {
		if (_last.w > 0 && _last.h > 0) {
			uint32_t x1 = x + w > _last.x + _last.w ? x + w : _last.x + _last.w;
			uint32_t y1 = y + h > _last.y + _last.h ? y + h : _last.y + _last.h;
			x = x < _last.x ? x : _last.x;
			y = y < _last.y ? y : _last.y;
			w = x1 - x;
			h = y1 - y;
		}
		const size_t rowBytes = size_t(w) * 4;
		for (uint32_t row = y; row < y + h; ++row) {
			const size_t off = size_t(row) * _stride + size_t(x) * 4;
			::memcpy(target + off, frame + off, rowBytes);
		}
	}
	int st = __sprt_tau_display_present(_next, x, y, w, h);
	_last = own;
	_next ^= 1;
	if (_latency && st >= 0) {
		showLatency();
	}
	if (st == -__SPRT_TAU_ECONNRESET) {
		_lost = true;
		_lostTriedUs = nowUs();
		_buffers[0] = _buffers[1] = nullptr;
		oslog::vpwarn(__SPRT_LOCATION, "TauWindow", "the display is lost");
	} else if (st < 0) {
		// The buffer's image on the display is not what was copied into it.
		_wholeLeft = 2;
		oslog::vperror(__SPRT_LOCATION, "TauWindow", "present failed: ", -st);
	}
}

bool TauWindow::init(NotNull<TauContextController> c, Rc<WindowInfo> &&info) {
	if (c->isVulkanDisplay()) {
		// The WSI opened the session: of dev.display@1, or for a GPU the planes of dev.display@2.
		struct __sprt_tau_display_info in = {};
		struct __sprt_tau_display2_info planes = {};
		if (__sprt_tau_display_query(&in, nullptr) == 0) {
			_extent = Extent2(in.width, in.height);
			_stride = in.stride;
		} else if (__sprt_tau_display2_query(&planes, nullptr) == 0) {
			_extent = Extent2(planes.width, planes.height);
			_stride = planes.width * 4;
		} else {
			return false;
		}
		oslog::vpinfo(__SPRT_LOCATION, "TauWindow", "Vulkan display WSI");
	} else if (!connect(true)) {
		return false;
	}

	info->rect.width = _extent.width;
	info->rect.height = _extent.height;
	if (info->imageFormat == ImageFormat::Undefined) {
		info->imageFormat = ImageFormat::B8G8R8A8_UNORM;
	}

	oslog::vpinfo(__SPRT_LOCATION, "TauWindow", "display ", _extent.width, "x", _extent.height, " stride ", _stride);

	if (!NativeWindow::init(c, sprt::move(info), WindowCapabilities::None)) {
		teardown();
		return false;
	}

	startInput();
	return true;
}

bool TauWindow::close() {
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

SurfaceInterfaceInfo TauWindow::getSurfaceInterfaceInfo() const {
	SurfaceInterfaceInfo ret;
	if (static_cast<TauContextController *>(_controller.get())->isVulkanDisplay()) {
		ret.backend = SurfaceBackend::Display;
		ret.display.fd = -1;
		ret.display.width = _extent.width;
		ret.display.height = _extent.height;
	} else {
		ret.backend = SurfaceBackend::Surface;
	}
	return ret;
}

void TauWindow::handleFramePresented(const PresentationFrameInfo &) {
	if (_latency && static_cast<TauContextController *>(_controller.get())->isVulkanDisplay()) {
		showLatency();
	}
}

SurfaceInfo TauWindow::getSurfaceOptions(SurfaceInfo &&info) const {
	info.currentExtent = _extent;
	info.minImageExtent = Extent2(1, 1);
	info.maxImageExtent = _extent;
	return sprt::move(info);
}

Rc<SoftwareSurface> TauWindow::makeSoftwareSurface() { return Rc<TauSoftwareSurface>::create(this); }

PresentationOptions TauWindow::getPreferredOptions() const {
	PresentationOptions opts;
	// No pointer wakes the director: render-on-demand would leave the display at its first frame.
	opts.renderOnDemand = false;
	opts.followDisplayLink = false;
	opts.followDisplayLinkBarrier = false;
	opts.usePresentWindow = false;
	opts.acquireImageWithoutFence = true;
	return opts;
}

void TauWindow::teardown() {
	stopInput();
	if (_buffers[0]) {
		_buffers[0] = _buffers[1] = nullptr;
		__sprt_tau_display_close();
	}
}

void *TauWindow::inputThread(void *arg) {
	static_cast<TauWindow *>(arg)->inputLoop();
	return nullptr;
}

void TauWindow::startInput() {
	_console = !(::getenv("XL_UART_KEYS") && strcmp(::getenv("XL_UART_KEYS"), "0") == 0);
	_usb = !(::getenv("XL_USB_INPUT") && strcmp(::getenv("XL_USB_INPUT"), "0") == 0);
	_latency = ::getenv("XL_INPUT_LATENCY") && strcmp(::getenv("XL_INPUT_LATENCY"), "1") == 0;
	if (!_console && !_usb) {
		return;
	}
	if (const char *env = ::getenv("XL_UART_HOLD_US")) {
		auto v = strtoull(env, nullptr, 10);
		if (v >= 20'000 && v <= 2'000'000) {
			s_keyHoldUs = v;
		}
	}
	pthread_t thread = 0;
	_running = true;
	if (::pthread_create(&thread, nullptr, &TauWindow::inputThread, this) != 0) {
		_running = false;
		oslog::vperror(__SPRT_LOCATION, "TauWindow", "input thread failed: ", errno);
		return;
	}
	_thread = reinterpret_cast<void *>(thread);
}

void TauWindow::stopInput() {
	if (!_thread) {
		return;
	}
	_running = false;
	::pthread_join(reinterpret_cast<pthread_t>(_thread), nullptr);
	_thread = nullptr;
}

// The console stream of stdin has no O_NONBLOCK and no termios on Tau: poll() with a timeout
// keeps the release tick going, and a read after POLLIN does not block. USB input is taken on
// the same tick; its events also wake the poll.
void TauWindow::inputLoop() {
	uint8_t buf[32];
	while (_running.load()) {
		if (_usb) {
			pollUsb(nowUs());
		}
		if (!_console) {
			::usleep(unsigned(s_inputPollMs) * 1'000);
			continue;
		}
		struct __SPRT_POLLFD_NAME p = {STDIN_FILENO, __SPRT_POLLIN, 0};
		int ready = __SPRT_ID(poll)(&p, 1, s_inputPollMs);
		uint64_t now = nowUs();
		ssize_t n = ready > 0 && (p.revents & __SPRT_POLLIN) ? ::read(STDIN_FILENO, buf, sizeof(buf)) : 0;
		if (n > 0) {
			for (ssize_t i = 0; i < n; ++i) {
				feed(buf[i], now);
			}
		} else {
			flushSequence(now);
		}
		expireHeld(now);
		if (ready < 0 || (ready > 0 && n <= 0)) {
			::usleep(unsigned(s_inputPollMs) * 1'000); // stdin closed or failed: keep the release tick
		}
	}
	postCancel();
	usbCancel();
	if (_usbOpen) {
		__sprt_tau_input_close();
		_usbOpen = false;
	}
}

void TauWindow::feed(uint8_t byte, uint64_t now) {
	if (_seqLen > 0) {
		if (_seqLen < sizeof(_seq)) {
			_seq[_seqLen++] = byte;
		} else {
			flushSequence(now);
			return;
		}
		if (_seqLen == 2 && byte != '[' && byte != 'O') {
			// ESC and not CSI or SS3: ESC is its own key, the byte comes again.
			_seqLen = 0;
			post(InputKeyCode::ESCAPE, InputEventName::KeyPressed);
			feed(byte, now);
			return;
		}
		if ((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') || byte == '~') {
			flushSequence(now);
		}
		return;
	}

	if (byte == 0x1b) {
		_seq[0] = byte;
		_seqLen = 1;
		_seqStartUs = now;
		return;
	}

	InputKeyCode code = InputKeyCode::Unknown;
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
	post(code, InputEventName::KeyPressed);
}

void TauWindow::flushSequence(uint64_t now) {
	if (_seqLen == 0) {
		return;
	}
	if (_seqLen == 1) {
		if (now - _seqStartUs < s_seqTimeoutUs) {
			return;
		}
		post(InputKeyCode::ESCAPE, InputEventName::KeyPressed);
		_seqLen = 0;
		return;
	}

	InputKeyCode code = InputKeyCode::Unknown;
	const uint8_t *seq = _seq;
	if (seq[1] == '[' && _seqLen == 3) {
		switch (seq[2]) {
		case 'A': code = InputKeyCode::UP; break;
		case 'B': code = InputKeyCode::DOWN; break;
		case 'C': code = InputKeyCode::RIGHT; break;
		case 'D': code = InputKeyCode::LEFT; break;
		case 'H': code = InputKeyCode::HOME; break;
		case 'F': code = InputKeyCode::END; break;
		default: break;
		}
	} else if (seq[1] == '[' && _seqLen >= 4 && seq[_seqLen - 1] == '~') {
		uint32_t num = 0;
		for (size_t i = 2; i < _seqLen - 1; ++i) {
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
	} else if (seq[1] == 'O' && _seqLen == 3) {
		switch (seq[2]) {
		case 'P': code = InputKeyCode::F1; break;
		case 'Q': code = InputKeyCode::F2; break;
		case 'R': code = InputKeyCode::F3; break;
		case 'S': code = InputKeyCode::F4; break;
		default: break;
		}
	}
	post(code, InputEventName::KeyPressed);
	_seqLen = 0;
}

void TauWindow::expireHeld(uint64_t now) {
	size_t w = 0;
	for (size_t i = 0; i < _heldCount; ++i) {
		if (now - _held[i].lastSeenUs < s_keyHoldUs) {
			_held[w++] = _held[i];
		} else {
			InputKeyCode code = _held[i].keycode;
			_held[i] = HeldKey{};
			post(code, InputEventName::KeyReleased);
		}
	}
	_heldCount = w;
}

void TauWindow::post(InputKeyCode code, InputEventName event) {
	if (code == InputKeyCode::Unknown) {
		return;
	}
	if (event == InputEventName::KeyPressed) {
		bool held = false;
		for (size_t i = 0; i < _heldCount; ++i) {
			if (_held[i].keycode == code) {
				_held[i].lastSeenUs = nowUs();
				held = true;
				event = InputEventName::KeyRepeated; // the terminal's auto-repeat, not a new press
				break;
			}
		}
		if (!held && _heldCount < sizeof(_held) / sizeof(_held[0])) {
			_held[_heldCount++] = HeldKey{code, nowUs()};
		}
	}

	auto ev = InputEventData();
	ev.event = event;
	ev.key.keycode = code;
	ev.key.keysym = 0;
	ev.key.keychar = 0;
	postEvent(ev);
}

void TauWindow::postCancel() {
	for (size_t i = 0; i < _heldCount; ++i) {
		auto ev = InputEventData();
		ev.event = InputEventName::KeyCanceled;
		ev.key.keycode = _held[i].keycode;
		ev.key.keysym = 0;
		ev.key.keychar = 0;
		postEvent(ev);
	}
	_heldCount = 0;
}

// handleInputEvents runs on the looper's thread; performOnThread retains the window.
void TauWindow::postEvent(const InputEventData &ev) {
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

} // namespace sprt::window

#endif // SPRT_TAU
