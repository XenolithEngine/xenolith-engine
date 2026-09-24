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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_VIRTUAL_WINDOW_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_VIRTUAL_WINDOW_H_

#include <sprt/runtime/window/native_window.h>

namespace sprt::window {

/* A window with no window system behind it: what it renders is read by a compositor in this same
process - a window manager's plane - and never shown by the OS.

ContextController::createWindow makes one for WindowCreationFlags::Virtual on EVERY controller, in
place of loadWindow, so a window manager running on X11 opens no second OS window for a client, and
one running headless puts no client window into the headless stacking order.

The gAPI is told SurfaceBackend::Headless and renders into a pseudo-swapchain of ordinary device
images, exactly as for the headless pseudo-window - which is this class's other half (see
HeadlessWindow, built on top of it). Everything a window system would decide comes from the owner
instead:

* the extent is WindowInfo's, changed by setExtent - the window manager's resize;
* focus, pointer and minimized are the window manager's (updateVirtualState); the host controller
  never touches them, and the window maps with Enabled and the host's input devices only;
* no capabilities: no subwindows (a popup on such a window is an in-scene overlay), no position,
  no decorations, no fullscreen, no state an application may ask for but CloseGuard/CloseRequest.

Frames: until a compositor claims the window (AppWindow::setExternalDisplayLink) it renders on
demand, like the headless window; publication follows the GPU fence (earlyPresent off), so the
image a compositor reads is complete. */
class SPRT_API VirtualWindow : public NativeWindow {
public:
	// What a window manager decides for a virtual window. updateVirtualState ignores anything else.
	static constexpr WindowState ManagedState = WindowState::Focused | WindowState::Pointer
			| WindowState::Minimized | WindowState::InputPointer | WindowState::InputTouch;

	virtual ~VirtualWindow();

	virtual bool init(NotNull<ContextController>, Rc<WindowInfo> &&, WindowCapabilities) override;

	virtual void mapWindow() override;
	virtual void unmapWindow() override;
	virtual bool close() override;

	virtual bool isMapped() const override { return _mapped; }

	virtual Extent2 getExtent() const override;

	virtual SurfaceInterfaceInfo getSurfaceInterfaceInfo() const override;

	virtual SurfaceInfo getSurfaceOptions(SurfaceInfo &&) const override;

	virtual PresentationOptions getPreferredOptions() const override;

	virtual bool setContentExtent(Extent2) override;

	// Resize the window. Deprecates the swapchain, so the next frame is rendered at the new extent.
	// Context thread.
	virtual Status setExtent(Extent2) override;

	// Only CloseGuard and CloseRequest - the application's own business. Everything else is a
	// window manager decision (updateVirtualState).
	virtual bool enableState(WindowState) override;
	virtual bool disableState(WindowState) override;

	// Raise (`value`) or drop the bits of `mask` that are in ManagedState. Context thread.
	void updateVirtualState(WindowState mask, bool value);

	void updateFocusState(bool value) { updateVirtualState(WindowState::Focused, value); }
	void updatePointerState(bool value) { updateVirtualState(WindowState::Pointer, value); }

protected:
	virtual bool updateTextInput(const TextInputRequest &,
			TextInputFlags flags = TextInputFlags::RunIfDisabled) override;
	virtual void cancelTextInput() override;

	// The input devices a virtual window reports: the host's, read off the first mapped window that
	// is not virtual. None while there is no such window.
	WindowState getHostInputState() const;

	// Shared by setExtent and setContentExtent; true if the extent actually moved.
	bool applyExtent(Extent2);

	Extent2 _extent;
	bool _mapped = false;
	bool _closed = false;
};

} // namespace sprt::window

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_VIRTUAL_WINDOW_H_
