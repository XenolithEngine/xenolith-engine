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

#ifndef RUNTIME_WINDOW_HEADLESS_SPRTWINHEADLESSCONTROLLER_H_
#define RUNTIME_WINDOW_HEADLESS_SPRTWINHEADLESSCONTROLLER_H_

#include <sprt/runtime/init.h>

#if __SPRT_RUNTIME_CONFIG_HAVE_WINDOW

#include <sprt/runtime/window/controller.h>

namespace sprt::window {

class HeadlessWindow;

// Cross-platform pseudo-controller: runs the engine on a plain dispatch::Looper with no window
// system underneath.
//
// Selected at runtime (not by platform #if, unlike every other controller) when ContextFlags::
// Headless is set - see ContextController::create. The one thing it must get right for the gAPI is
// getSupportInfo(): an empty backendMask is what makes the Vulkan instance skip VK_KHR_surface and
// every WSI extension, and what makes Context::makeLoop drop the presentation requirement from
// device selection.
//
// It also stands in for the window manager itself: auxiliary windows (Dialog, Utility, Popup,
// Tooltip) really are created here - each with its own pseudo-swapchain - and this class supplies
// what a WM would otherwise decide for them: the virtual screen they are laid out on, where each
// one lands, which one holds focus and the pointer, and where those go when it disappears. That is
// what makes WindowCapabilities::Subwindows honest in headless mode, so ui::SubWindow takes the
// native path here instead of falling back to an in-scene overlay, and menus, dialogs and palettes
// can be exercised with no display at all.
//
// Control comes from outside the process over the inspector socket (scene dump, screenshots,
// scene-registered commands, input injection, frame stepping, shutdown).
class HeadlessContextController : public ContextController {
public:
	static Rc<HeadlessContextController> create(NotNull<Context>, ContextConfig &&,
			NotNull<dispatch::Looper>);

	static void acquireDefaultConfig(ContextConfig &, NativeContextHandle *);

	virtual ~HeadlessContextController();

	virtual bool init(NotNull<Context>, ContextConfig &&, NotNull<dispatch::Looper>);

	virtual int run(NotNull<ContextContainer>) override;

	virtual bool isCursorSupported(WindowCursor, bool serverSide) const override { return false; }
	virtual WindowCapabilities getCapabilities() const override;

	// Whether windows of this context report WindowState::InputPointer. True unless
	// ContextFlags::HeadlessNoPointer was asked for - see that flag for why it exists.
	bool hasPointerDevice() const;
	virtual void openUrl(StringView) override;

	// An in-process clipboard. There is no window system to own a selection here, so the base
	// class's ErrorNotImplemented would make copy/paste untestable in exactly the mode the test
	// harness runs in. This keeps the last written data in the controller and hands it back: same
	// API, same callbacks, no OS involved
	virtual Status readFromClipboard(Rc<ClipboardRequest> &&) override;
	virtual Status probeClipboard(Rc<ClipboardProbe> &&) override;
	virtual Status writeToClipboard(Rc<ClipboardData> &&) override;

	// Pointer and focus follow the window that input is injected into, the way a window manager
	// moves them before it delivers the event. Also raises the window that was pressed.
	virtual void notifyWindowInputEvents(NotNull<NativeWindow>, Vector<InputEventData> &&) override;

	virtual void notifyWindowDeallocated(NotNull<NativeWindow>) override;

	// The virtual desktop auxiliary windows are placed on: the bounding box of every live Root
	// window, which is what a work area means when the application IS the whole screen. Falls back
	// to the configured root window geometry while no window exists yet.
	IRect getVirtualScreenRect() const;

	NativeWindow *getFocusedWindow() const { return _focusedWindow; }
	NativeWindow *getPointerWindow() const { return _pointerWindow; }

	// Hand focus (or the pointer) to `w`, taking it away from whoever held it. Null drops it
	// entirely. A Popup or Tooltip is never a valid focus target and is ignored here - the window
	// a menu belongs to keeps focus while the menu is up, as it does on X11 and Win32.
	void setFocusedWindow(NativeWindow *w);
	void setPointerWindow(NativeWindow *w);

	// Called by HeadlessWindow when it enters / leaves the virtual screen. Maintains the stacking
	// order and moves focus and pointer onto and off the window.
	void handleWindowMapped(NotNull<HeadlessWindow>);
	void handleWindowUnmapped(NotNull<HeadlessWindow>);

protected:
	virtual bool loadWindow(Rc<WindowInfo> &&) override;

	// Bottom-to-top stacking order of mapped windows. There is nothing to draw and nothing to
	// overlap, but "which window is on top" is still the answer to where focus goes when the
	// window that had it disappears.
	void raiseWindow(NativeWindow *w);
	NativeWindow *getTopmostFocusable(NativeWindow *except) const;

	Rc<ClipboardData> _clipboard;

	Vector<NativeWindow *> _stack;
	NativeWindow *_focusedWindow = nullptr;
	NativeWindow *_pointerWindow = nullptr;
};

} // namespace sprt::window

#endif // __SPRT_RUNTIME_CONFIG_HAVE_WINDOW

#endif // RUNTIME_WINDOW_HEADLESS_SPRTWINHEADLESSCONTROLLER_H_
