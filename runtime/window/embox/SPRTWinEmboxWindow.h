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

#ifndef RUNTIME_WINDOW_EMBOX_SPRTWINEMBOXWINDOW_H_
#define RUNTIME_WINDOW_EMBOX_SPRTWINEMBOXWINDOW_H_

#include <sprt/runtime/window/native_window.h>
#include <sprt/runtime/window/software_surface.h>
#include <sprt/runtime/window/input.h>
#include <sprt/cxx/atomic>
#include <stdint.h>

#if SPRT_EMBOX

#include <termios.h> // the saved console state (see _uartSavedTermios)

namespace sprt::window {

class EmboxContextController;
class EmboxWindow;

// CPU presentation into the mmap'd framebuffer. virtio-gpu on qemu-armv8a is
// B8G8R8X8, which is the layout the software rasterizer already produces.
// The swapchain slot is a CPU shadow: rasterizing into live HDMI scanout
// (bcm2711 mailbox FB) shows the white scene clear as a flash every frame.
// present() copies the finished frame, then FBIO_UPDATE (ENOTTY on bcm2711).
class EmboxSoftwareSurface final : public SoftwareSurface {
public:
	virtual ~EmboxSoftwareSurface();

	bool init(NotNull<EmboxWindow>);

	virtual SurfaceInfo getSurfaceOptions(SurfaceInfo &&) const override;
	virtual Rc<SoftwareSwapchain> makeSwapchain(const SoftwareSwapchainInfo &) override;
	virtual void invalidate() override;

protected:
	EmboxWindow *_owner = nullptr;
};

class EmboxSoftwareSwapchain final : public SoftwareSwapchain {
public:
	virtual ~EmboxSoftwareSwapchain();

	bool init(NotNull<EmboxWindow>, const SoftwareSwapchainInfo &);

	virtual Status present(uint32_t index, SpanView<geom::URect> damage) override;
	virtual void invalidate() override;

protected:
	EmboxWindow *_owner = nullptr;
	Extent2 _extent;
	/* CPU shadow: composed frames clear-then-draw off-screen. RGA video
	 * writes the scanout and flags present() to skip the copy. */
	uint8_t *_shadow = nullptr;
	size_t _shadowSize = 0;
};

class EmboxWindow : public NativeWindow {
public:
	virtual ~EmboxWindow();
	EmboxWindow();

	bool init(NotNull<EmboxContextController>, Rc<WindowInfo> &&);

	virtual void mapWindow() override { }
	virtual void unmapWindow() override { }
	virtual bool close() override;

	virtual Extent2 getExtent() const override { return _extent; }
	virtual SurfaceInterfaceInfo getSurfaceInterfaceInfo() const override;
	virtual SurfaceInfo getSurfaceOptions(SurfaceInfo &&) const override;
	virtual Rc<SoftwareSurface> makeSoftwareSurface() override;
	virtual PresentationOptions getPreferredOptions() const override;

	int getFd() const { return _fd; }
	uint8_t *getMapping() const { return _mapping; }
	uint32_t getStride() const { return _stride; }
	size_t getMappingSize() const { return _mappingSize; }
	/* rk3588 simplefb: true. Firmware-composited fb: false. XL_DIRECT_FB=0 forces off. */
	bool useDirectScanout() const { return _directScanout; }

protected:
	virtual bool updateTextInput(const TextInputRequest &,
			TextInputFlags = TextInputFlags::RunIfDisabled) override {
		return false;
	}
	virtual void cancelTextInput() override { }

	void teardown();

	/* UART keyboard: stdin is the console. ASCII maps 1:1; CSI/SS3 for
	 * arrows/F-keys; lone ESC is ESCAPE. Serial has no key-release:
	 * auto-release after s_uartKeyHoldUs unless auto-repeat refreshes.
	 * XL_UART_KEYS=0 disables. HID make/break arrives on a side ring. */
	void startUartInput();
	void stopUartInput();
	static void *uartInputThread(void *);
	void uartInputLoop();
	void uartFeed(uint8_t byte, uint64_t nowUs);
	void uartFlushSequence(uint64_t nowUs);
	void uartExpireHeld(uint64_t nowUs);
	void uartPost(InputKeyCode, InputEventName);
	void hidPost(InputKeyCode, InputEventName);
	void uartPostCancel();

	int _fd = -1;
	uint8_t *_mapping = nullptr;
	size_t _mappingSize = 0;
	uint32_t _stride = 0;
	Extent2 _extent;
	bool _directScanout = false;
	bool _closed = false;

	void *_uartThread = nullptr;
	sprt::atomic<bool> _uartRunning = false;
	// stdin belongs to the process, not to this window: both are put back in stopUartInput.
	struct termios _uartSavedTermios = {};
	bool _uartTermiosSaved = false;
	int _uartSavedFlags = -1;
	uint8_t _uartSeq[8] = {};
	size_t _uartSeqLen = 0;
	uint64_t _uartSeqStartUs = 0;
	struct UartHeldKey {
		InputKeyCode keycode;
		uint64_t lastSeenUs;
	};
	UartHeldKey _uartHeld[16] = {};
	size_t _uartHeldCount = 0;
};

} // namespace sprt::window

#endif // SPRT_EMBOX

#endif // RUNTIME_WINDOW_EMBOX_SPRTWINEMBOXWINDOW_H_
