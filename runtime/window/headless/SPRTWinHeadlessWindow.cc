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

namespace sprt::window {

HeadlessWindow::~HeadlessWindow() { }

HeadlessWindow::HeadlessWindow() { }

bool HeadlessWindow::init(NotNull<HeadlessContextController> c, Rc<WindowInfo> &&info) {
	// The pseudo-screen is whatever WindowInfo asks for: there is no WM to negotiate with.
	_extent = Extent2(info->rect.width, info->rect.height);

	// No decorations, no cursor, no fullscreen, no subwindows - nothing a WM would provide.
	return NativeWindow::init(c, move(info), WindowCapabilities::None);
}

void HeadlessWindow::mapWindow() { }

void HeadlessWindow::unmapWindow() { }

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

Status HeadlessWindow::setExtent(Extent2 extent) {
	if (extent.width == 0 || extent.height == 0) {
		return Status::ErrorInvalidArguemnt;
	}

	auto clamped = clampWindowExtent(extent, _info->minExtent, _info->maxExtent);
	if (clamped == _extent) {
		return Status::Done; // already at that size, nothing to deprecate
	}

	_extent = clamped;
	_info->rect.width = clamped.width;
	_info->rect.height = clamped.height;

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
