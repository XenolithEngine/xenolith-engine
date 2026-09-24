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

#include <sprt/runtime/window/virtual_window.h>
#include <sprt/runtime/window/controller.h>
#include <sprt/runtime/log.h>

namespace sprt::window {

VirtualWindow::~VirtualWindow() { }

bool VirtualWindow::init(NotNull<ContextController> c, Rc<WindowInfo> &&info,
		WindowCapabilities caps) {
	// The size is whatever WindowInfo asks for: the window manager that created the window has
	// already decided it, and there is nothing else to negotiate with.
	_extent = Extent2(info->rect.width, info->rect.height);
	return NativeWindow::init(c, move(info), caps);
}

WindowState VirtualWindow::getHostInputState() const {
	// A virtual window takes its input from the host window it is composed into, so it has what the
	// host has. The first mapped window that is not virtual stands for the host: the input devices
	// are the process's, not a window's.
	for (auto *w : _controller->getAllWindows()) {
		auto wi = w->getInfo();
		if (w != this && wi && w->isMapped() && !hasFlag(wi->flags, WindowCreationFlags::Virtual)) {
			return wi->state & (WindowState::InputPointer | WindowState::InputTouch);
		}
	}
	return WindowState::None;
}

void VirtualWindow::mapWindow() {
	if (_mapped) {
		return;
	}
	_mapped = true;

	// Every window must emit its state exactly once as it maps: the app-thread mirror starts empty
	// and this event is the only thing that fills it. Focus and the pointer are NOT raised here - a
	// virtual window is focused when its window manager says so (updateVirtualState).
	updateState(0, _info->state | WindowState::Enabled | getHostInputState());
}

void VirtualWindow::unmapWindow() { _mapped = false; }

bool VirtualWindow::close() {
	if (_closed) {
		return true;
	}

	// No window system routes the close back to us, so the notification a WM-backed window gets for
	// free is raised here, as the headless window does.
	_closed = true;
	if (!_controller->notifyWindowClosed(this)) {
		_closed = false;
		return false;
	}
	return true;
}

Extent2 VirtualWindow::getExtent() const { return _extent; }

SurfaceInterfaceInfo VirtualWindow::getSurfaceInterfaceInfo() const {
	SurfaceInterfaceInfo ret;
	ret.backend = SurfaceBackend::Headless;
	return ret;
}

SurfaceInfo VirtualWindow::getSurfaceOptions(SurfaceInfo &&info) const {
	// The window is the authority on its size, and setExtent() is what moves it: without this the
	// surface would keep reporting the extent it was built with, and a resize would recreate the
	// swapchain at the old size.
	info.currentExtent = _extent;
	info.minImageExtent = Extent2(1, 1);
	info.maxImageExtent = _extent;
	return sprt::move(info);
}

PresentationOptions VirtualWindow::getPreferredOptions() const {
	PresentationOptions opts;

	// The pseudo-swapchain presentation engine, as for the headless window.
	opts.headless = true;

	// Frames on demand. There is no display link until a compositor claims the window and switches
	// the barrier on (core::PresentationEngine::setFollowDisplayLinkBarrier); until then a frame is
	// rendered whenever the application asks for one.
	opts.renderOnDemand = true;
	opts.followDisplayLink = false;
	opts.followDisplayLinkBarrier = false;
	opts.usePresentWindow = false;

	// Present after the frame's fence, not on submission: presenting is what hands the image to the
	// compositor, which must read a finished frame.
	opts.earlyPresent = false;

	// The pseudo-swapchain hands out images synchronously and can not signal a fence.
	opts.acquireImageWithoutFence = true;

	return opts;
}

bool VirtualWindow::applyExtent(Extent2 extent) {
	auto clamped = clampWindowExtent(extent, _info->minExtent, _info->maxExtent);
	if (clamped == _extent) {
		return false;
	}

	_extent = clamped;
	_info->rect.width = clamped.width;
	_info->rect.height = clamped.height;

	// A menu can not survive its owner being resized under it - the anchor it was placed against
	// has moved.
	_controller->dismissChildPopups(this, "owner-resized");
	_controller->notifyWindowGeometryChanged(this);
	return true;
}

bool VirtualWindow::setContentExtent(Extent2 extent) {
	if (extent.width == 0 || extent.height == 0) {
		return false;
	}

	// The caller (AppWindow::setContentExtent) updates the presentation constraints itself once this
	// returns true, so - unlike setExtent - nothing is notified from here.
	return applyExtent(extent);
}

Status VirtualWindow::setExtent(Extent2 extent) {
	if (extent.width == 0 || extent.height == 0) {
		return Status::ErrorInvalidArguemnt;
	}

	if (!applyExtent(extent)) {
		return Status::Done; // already at that size, nothing to deprecate
	}

	// Same path a WM-driven resize takes: deprecate the swapchain so the next frame is rendered at the
	// new extent.
	_controller->notifyWindowConstraintsChanged(this, UpdateConstraintsFlags::WindowResized);
	return Status::Ok;
}

bool VirtualWindow::enableState(WindowState state) {
	// Maximized, fullscreen, minimized and the rest are where the window manager puts the window, and
	// it does that through updateVirtualState. What is left is the application's own close guard.
	if ((state & ~(WindowState::CloseGuard | WindowState::CloseRequest)) != WindowState::None) {
		return false;
	}
	return NativeWindow::enableState(state);
}

bool VirtualWindow::disableState(WindowState state) {
	if ((state & ~(WindowState::CloseGuard | WindowState::CloseRequest)) != WindowState::None) {
		return false;
	}
	return NativeWindow::disableState(state);
}

void VirtualWindow::updateVirtualState(WindowState mask, bool value) {
	auto managed = mask & ManagedState;
	if (managed != mask) {
		// Everything else is either the application's (CloseGuard) or no window manager's at all.
		oslog::vpwarn(__SPRT_LOCATION, "VirtualWindow", "state outside ManagedState is ignored");
	}
	if (managed == WindowState::None) {
		return;
	}
	updateState(0, value ? (_info->state | managed) : (_info->state & ~managed));
}

// There is no on-screen keyboard to raise, but the window still stands in for the IME, so text input
// works through the shared TextInputProcessor exactly as on the headless window: events delivered to
// handleInputEvents are intercepted by it, and performTextInput() reproduces composition.
bool VirtualWindow::updateTextInput(const TextInputRequest &, TextInputFlags) {
	if (_textInput) {
		_textInput->handleInputEnabled(true);
	}
	return true;
}

void VirtualWindow::cancelTextInput() {
	if (_textInput) {
		_textInput->handleInputEnabled(false);
	}
}

} // namespace sprt::window
