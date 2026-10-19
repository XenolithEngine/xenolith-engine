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

#ifndef RUNTIME_WINDOW_TAU_SPRTWINTAUWINDOW_H_
#define RUNTIME_WINDOW_TAU_SPRTWINTAUWINDOW_H_

#include <sprt/runtime/window/native_window.h>
#include <sprt/runtime/window/software_surface.h>
#include <sprt/runtime/window/input.h>
#include <sprt/c/cross/tau_sprt/sprt_tau.h>
#include <sprt/cxx/atomic>
#include <stdint.h>

#if SPRT_TAU

namespace sprt::window {

class TauContextController;
class TauWindow;

class TauSoftwareSurface final : public SoftwareSurface {
public:
	virtual ~TauSoftwareSurface();

	bool init(NotNull<TauWindow>);

	virtual SurfaceInfo getSurfaceOptions(SurfaceInfo &&) const override;
	virtual Rc<SoftwareSwapchain> makeSwapchain(const SoftwareSwapchainInfo &) override;
	virtual void invalidate() override;

protected:
	TauWindow *_owner = nullptr;
};

// The swapchain's slots are CPU shadows; present() hands the damaged rows of one to the window,
// which copies them into a display buffer and shows it. Two shadows let the rasterizer draw one
// while the other presents.
class TauSoftwareSwapchain final : public SoftwareSwapchain {
public:
	virtual ~TauSoftwareSwapchain();

	bool init(NotNull<TauWindow>, const SoftwareSwapchainInfo &);

	virtual Status present(uint32_t index, SpanView<geom::URect> damage) override;
	virtual void invalidate() override;

protected:
	TauWindow *_owner = nullptr;
	Extent2 _extent;
	uint8_t *_shadows[2] = {nullptr, nullptr};
	size_t _shadowSize = 0;
};

class TauWindow : public NativeWindow {
public:
	virtual ~TauWindow();
	TauWindow();

	bool init(NotNull<TauContextController>, Rc<WindowInfo> &&);

	virtual void mapWindow() override { }
	virtual void unmapWindow() override { }
	virtual bool close() override;

	virtual Extent2 getExtent() const override { return _extent; }
	virtual SurfaceInterfaceInfo getSurfaceInterfaceInfo() const override;
	virtual SurfaceInfo getSurfaceOptions(SurfaceInfo &&) const override;
	virtual Rc<SoftwareSurface> makeSoftwareSurface() override;
	virtual PresentationOptions getPreferredOptions() const override;

	uint32_t getStride() const { return _stride; }

	// Copies rows [y, y + h) from x, w pixels wide, of a whole frame at the display's stride into
	// the display buffer not on screen and shows it; `whole` shows the frame, an empty rectangle
	// nothing. That buffer last got the frame before the previous one, so the previous frame's
	// rectangle is copied too, and the first two frames after a connect go whole. A lost display
	// drops the frame and is connected again at most every s_reconnectUs.
	void show(const uint8_t *frame, uint32_t x, uint32_t y, uint32_t w, uint32_t h, bool whole);

protected:
	virtual bool updateTextInput(const TextInputRequest &, TextInputFlags = TextInputFlags::RunIfDisabled) override {
		return false;
	}
	virtual void cancelTextInput() override { }

	bool connect(bool wait);
	void teardown();

	/* Console keys: stdin is the console. ASCII maps 1:1; CSI/SS3 for arrows and F-keys; a lone
	 * ESC is ESCAPE. A serial line has no key release: a key is released s_keyHoldUs after it was
	 * last seen. XL_UART_KEYS=0 disables. */
	void startInput();
	void stopInput();
	static void *inputThread(void *);
	void inputLoop();
	void feed(uint8_t byte, uint64_t nowUs);
	void flushSequence(uint64_t nowUs);
	void expireHeld(uint64_t nowUs);
	void post(InputKeyCode, InputEventName);
	void postCancel();
	void postEvent(const InputEventData &);

	/* USB input: the events of dev.input@1 (__sprt_tau_input_*), polled on the input thread.
	 * Touches are Begin/Move/End by contact, a mouse moves a cursor in the window, a tablet places
	 * it, buttons are Begin/End and the wheel Scroll at the cursor; HID keys repeat after
	 * s_usbRepeatDelayUs. The session is opened again after a loss. XL_USB_INPUT=0 disables. */
	void pollUsb(uint64_t nowUs);
	void usbEvent(const __sprt_tau_input_event &, uint64_t nowUs);
	void usbPointer(const __sprt_tau_input_event &, bool absolute);
	void usbKey(uint32_t usage, bool down, uint64_t nowUs);
	void usbRepeat(uint64_t nowUs);
	void usbCancel();
	InputEventData usbPoint(InputEventName, uint32_t id, InputMouseButton, float x, float y) const;

	struct Rect {
		uint32_t x, y, w, h;
	};

	uint8_t *_buffers[2] = {nullptr, nullptr}; // the display's buffers, while connected
	uint32_t _next = 0; // the buffer the next frame goes into
	uint32_t _wholeLeft = 0; // frames to show whole: buffers not yet holding a whole frame
	Rect _last = {0, 0, 0, 0}; // the damage of the last frame shown, without the one before it
	uint32_t _stride = 0;
	Extent2 _extent;
	bool _closed = false;
	bool _lost = false;
	uint64_t _lostTriedUs = 0;

	void *_thread = nullptr;
	sprt::atomic<bool> _running = false;
	uint8_t _seq[8] = {};
	size_t _seqLen = 0;
	uint64_t _seqStartUs = 0;
	struct HeldKey {
		InputKeyCode keycode;
		uint64_t lastSeenUs;
	};
	HeldKey _held[16] = {};
	size_t _heldCount = 0;
	bool _console = false;

	struct UsbDevice {
		uint32_t id, kind, xMax, yMax;
	};
	struct UsbKey {
		InputKeyCode keycode;
		uint64_t repeatUs; // when it repeats next
	};
	bool _usb = false;
	bool _usbOpen = false;
	uint64_t _usbTriedUs = 0;
	UsbDevice _usbDevices[8] = {};
	size_t _usbDeviceCount = 0;
	uint32_t _contacts[16] = {}; // touches down, by contact
	size_t _contactCount = 0;
	float _cursorX = 0.0f, _cursorY = 0.0f; // window coordinates, y up
	bool _cursorSet = false;
	uint32_t _buttons = 0; // bit n: button n + 1 down
	UsbKey _usbKeys[8] = {};
	size_t _usbKeyCount = 0;
	InputModifier _modifiers = InputModifier::None;
};

} // namespace sprt::window

#endif // SPRT_TAU

#endif // RUNTIME_WINDOW_TAU_SPRTWINTAUWINDOW_H_
