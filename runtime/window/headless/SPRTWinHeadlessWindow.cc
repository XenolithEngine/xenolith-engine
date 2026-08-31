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

#include "SPRTWinHeadlessWindow.h"

#if __SPRT_RUNTIME_CONFIG_HAVE_WINDOW

#include "SPRTWinHeadlessController.h"
#include <sprt/runtime/log.h>

namespace sprt::window {

HeadlessWindow::~HeadlessWindow() { }

HeadlessWindow::HeadlessWindow() { }

HeadlessContextController *HeadlessWindow::getHeadlessController() const {
	return static_cast<HeadlessContextController *>(_controller.get());
}

bool HeadlessWindow::init(NotNull<HeadlessContextController> c, Rc<WindowInfo> &&info) {
	if (info->type != WindowType::Root) {
		// An auxiliary window is positioned relative to a parent that must already be there;
		// ContextController::createWindow has checked that, but not that it is one of ours.
		auto *parent = dynamic_cast<HeadlessWindow *>(c->findWindow(info->parent));
		if (!parent) {
			oslog::vperror(__SPRT_LOCATION, "HeadlessWindow",
					"Auxiliary window parent is not available");
			return false;
		}

		const auto size = clampWindowExtent(Extent2(info->rect.width, info->rect.height),
				info->minExtent, info->maxExtent);
		const auto workArea = c->getVirtualScreenRect();
		const auto parentRect = parent->getContentScreenRect();

		if (info->type == WindowType::Popup || info->type == WindowType::Tooltip) {
			// Same positioner every other backend feeds, with the whole virtual desktop as the
			// work area - there is only ever one "monitor" here.
			info->rect = computeWindowPlacement(info->placement, size, parentRect, workArea);
		} else {
			// Dialog and Utility are placed by the window manager everywhere else. This one
			// centers them over their parent and keeps them on the virtual screen.
			const int32_t w = int32_t(size.width);
			const int32_t h = int32_t(size.height);
			int32_t x = parentRect.x + (int32_t(parentRect.width) - w) / 2;
			int32_t y = parentRect.y + (int32_t(parentRect.height) - h) / 2;

			// A window wider than the screen keeps its left/top edge visible rather than being
			// pushed off the other way, so max() is applied last.
			x = sprt::max(workArea.x, sprt::min(x, workArea.x + int32_t(workArea.width) - w));
			y = sprt::max(workArea.y, sprt::min(y, workArea.y + int32_t(workArea.height) - h));

			info->rect = IRect(x, y, size.width, size.height);
		}
	}

	// The pseudo-screen is whatever WindowInfo asks for: there is no WM to negotiate with.
	_extent = Extent2(info->rect.width, info->rect.height);

	// The requested actions are all granted: the emulated WM has no policy of its own, and this is
	// what tells an application which window buttons to draw.
	if (hasFlag(info->flags, WindowCreationFlags::AllowClose)) {
		info->state |= WindowState::AllowedClose;
	}
	if (hasFlag(info->flags, WindowCreationFlags::AllowResize)) {
		info->state |= WindowState::AllowedResize;
	}
	if (hasFlag(info->flags, WindowCreationFlags::AllowMove)) {
		info->state |= WindowState::AllowedMove;
	}

	// There is no device to probe, so the emulated WM declares one: this backend injects mouse
	// events and nothing else, and a widget that adapts to the input devices available should see
	// what it would see on a desktop. ContextFlags::HeadlessNoPointer takes it away, which is the
	// only way to reach the touch-shaped branch of such a widget under test.
	if (c->hasPointerDevice()) {
		info->state |= WindowState::InputPointer;
	}

	// Subwindows is the one capability this backend really has: the controller creates auxiliary
	// windows itself, so a menu can open a submenu off this window like anywhere else. Everything
	// else - decorations, cursors, fullscreen, an OS icon - needs a window system.
	return NativeWindow::init(c, move(info), c->getCapabilities());
}

void HeadlessWindow::mapWindow() {
	if (_mapped) {
		return;
	}
	_mapped = true;

	// Every window must emit its state exactly once as it maps: the app-thread mirror
	// (RenderServerChannel::getWindowState) starts empty and this event is the only thing that ever
	// fills it. Enabled is raised here rather than in init() precisely so that the transition
	// happens now - a menu takes neither focus nor the pointer, so nothing else below would emit
	// anything for it at all.
	//
	// Nothing here is blocked to begin with; a modal Dialog clears Enabled on its parent through
	// ContextController::retainModalBlock, exactly as it does with a window system in play.
	updateState(0, _info->state | WindowState::Enabled);

	getHeadlessController()->handleWindowMapped(this);
}

void HeadlessWindow::unmapWindow() {
	if (!_mapped) {
		return;
	}
	_mapped = false;
	getHeadlessController()->handleWindowUnmapped(this);
}

bool HeadlessWindow::close() {
	if (_closed) {
		return true;
	}

	// There is no window manager to route the close back to us, so the notification the WM-backed
	// windows get for free has to be raised here. Without it the controller never drops the window
	// and the process hangs after the presentation engine is torn down.
	_closed = true;
	if (!_controller->notifyWindowClosed(this)) {
		_closed = false;
		return false;
	}
	return true;
}

Extent2 HeadlessWindow::getExtent() const { return _extent; }

IRect HeadlessWindow::getContentScreenRect() const {
	return IRect(_info->rect.x, _info->rect.y, _extent.width, _extent.height);
}

void HeadlessWindow::updateFocusState(bool focused) {
	updateState(0,
			focused ? (_info->state | WindowState::Focused)
					: (_info->state & ~WindowState::Focused));
}

void HeadlessWindow::updatePointerState(bool within) {
	updateState(0,
			within ? (_info->state | WindowState::Pointer)
				   : (_info->state & ~WindowState::Pointer));
}

SurfaceInterfaceInfo HeadlessWindow::getSurfaceInterfaceInfo() const {
	SurfaceInterfaceInfo ret;
	ret.backend = SurfaceBackend::Headless;
	return ret;
}

SurfaceInfo HeadlessWindow::getSurfaceOptions(SurfaceInfo &&info) const {
	// The window - not the surface - is the authority on the pseudo-screen size here, and it is
	// what setExtent() moves. Without this the surface would keep reporting the extent it was
	// built with and a resize would recreate the swapchain at the old size.
	info.currentExtent = _extent;
	info.minImageExtent = Extent2(1, 1);
	info.maxImageExtent = _extent;
	return sprt::move(info);
}

PresentationOptions HeadlessWindow::getPreferredOptions() const {
	PresentationOptions opts;

	// Selects the pseudo-swapchain presentation engine in the gAPI backend.
	opts.headless = true;

	// There is no display link and no WM tick, so frames are driven entirely by
	// setReadyForNextFrame (the `frame` command on the inspector socket). Without this a headless
	// process would burn a GPU and a core rendering frames nobody will ever look at.
	opts.renderOnDemand = true;
	opts.followDisplayLink = false;
	opts.followDisplayLinkBarrier = false;

	// No vsync to pace against: present is a memcpy-less no-op, so a present window would only add
	// latency between "render this" and "the image is readable".
	opts.usePresentWindow = false;

	// The pseudo-swapchain hands out images synchronously and can not signal a VkFence from the
	// host, so the engine must take the fence-less acquisition path.
	opts.acquireImageWithoutFence = true;

	return opts;
}

bool HeadlessWindow::applyExtent(Extent2 extent) {
	auto clamped = clampWindowExtent(extent, _info->minExtent, _info->maxExtent);
	if (clamped == _extent) {
		return false;
	}

	_extent = clamped;
	_info->rect.width = clamped.width;
	_info->rect.height = clamped.height;

	// A menu can not survive its owner being resized under it - the anchor it was placed against
	// has moved. Every backend does this; here the resize is the only way geometry ever changes.
	_controller->dismissChildPopups(this, "owner-resized");

	// ...which is also why this is the only place the application's view of the geometry can go
	// stale here: there is no move to report.
	_controller->notifyWindowGeometryChanged(this);
	return true;
}

bool HeadlessWindow::setContentExtent(Extent2 extent) {
	if (extent.width == 0 || extent.height == 0) {
		return false;
	}

	// The caller (AppWindow::setContentExtent) updates the presentation constraints itself once
	// this returns true, so - unlike setExtent - nothing is notified from here.
	return applyExtent(extent);
}

Status HeadlessWindow::setExtent(Extent2 extent) {
	if (extent.width == 0 || extent.height == 0) {
		return Status::ErrorInvalidArguemnt;
	}

	if (!applyExtent(extent)) {
		return Status::Done; // already at that size, nothing to deprecate
	}

	// Same path a WM-driven resize takes: deprecate the swapchain so the next frame is rendered
	// (and captured) at the new extent.
	_controller->notifyWindowConstraintsChanged(this, UpdateConstraintsFlags::WindowResized);
	return Status::Ok;
}

// There is no on-screen keyboard to raise, but the window still stands in for the IME so that a
// headless run can be driven through the real text-input path: events injected into
// handleInputEvents are intercepted by the shared TextInputProcessor, and performTextInput() can
// reproduce composition. Declining here would leave isTextInputEnabled() false and make text input
// untestable without a display.
bool HeadlessWindow::updateTextInput(const TextInputRequest &, TextInputFlags) {
	if (_textInput) {
		_textInput->handleInputEnabled(true);
	}
	return true;
}

void HeadlessWindow::cancelTextInput() {
	if (_textInput) {
		_textInput->handleInputEnabled(false);
	}
}

} // namespace sprt::window

#endif // __SPRT_RUNTIME_CONFIG_HAVE_WINDOW
