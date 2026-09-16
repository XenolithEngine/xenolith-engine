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

#include "SPRTWinLinuxDisplay.h"

#if SPRT_LINUX

#include "SPRTWinLinuxController.h"

namespace sprt::window {

DisplayWindow::~DisplayWindow() { }

DisplayWindow::DisplayWindow() { }

bool DisplayWindow::init(NotNull<LinuxContextController> c, Rc<DrmDevice> &&dev,
		Rc<WindowInfo> &&info) {
	if (!dev) {
		return false;
	}

	// Direct display: exclusive fullscreen on a plane, no server-side chrome.
	auto caps = WindowCapabilities::Fullscreen | WindowCapabilities::FullscreenExclusive
			| WindowCapabilities::FullscreenWithMode;

	_drm = sprt::move(dev);

	// The scanout size is whatever the connector's mode is, NOT WindowInfo::rect
	// (which carries a desktop default like 1024x768).
	auto mode = _drm->getMode();
	_extent = Extent2(mode.width, mode.height);

	if (!NativeWindow::init(c, move(info), caps)) {
		return false;
	}

	return true;
}

void DisplayWindow::mapWindow() {
	// No OS window to map; the display plane surface is presented directly.
}

void DisplayWindow::unmapWindow() { }

bool DisplayWindow::close() {
	// Nothing to release at the window-system level.
	return true;
}

Extent2 DisplayWindow::getExtent() const { return _extent; }

SurfaceInterfaceInfo DisplayWindow::getSurfaceInterfaceInfo() const {
	SurfaceInterfaceInfo ret;
	ret.backend = SurfaceBackend::Display;
	if (_drm) {
		// The gAPI acquires this connector and builds a plane surface at this mode.
		// The fd stays owned by _drm and must outlive the surface (the gAPI becomes
		// DRM master for the acquired display), so it is passed, never transferred.
		auto mode = _drm->getMode();
		ret.display.fd = _drm->getFd();
		ret.display.connectorId = _drm->getConnectorId();
		ret.display.crtcId = _drm->getCrtcId();
		ret.display.width = mode.width;
		ret.display.height = mode.height;
		ret.display.rate = mode.rate;
	}
	return ret;
}

bool DisplayWindow::updateTextInput(const TextInputRequest &, TextInputFlags) {
	// No on-screen keyboard / IME in direct-display mode yet.
	return false;
}

void DisplayWindow::cancelTextInput() { }

PresentationOptions DisplayWindow::getPreferredOptions() const {
	PresentationOptions opts;
	// Pace frames on the DRM page-flip: block in vkAcquireNextImageKHR until a
	// presented image recycles. Without this the presentation engine polls with
	// timeout=0 and, lacking any display-link tick in KMS mode, wedges after the
	// swapchain images are exhausted (image stays FLIPPING, acquire keeps timing
	// out and nothing re-drives scheduleNextImage).
	opts.acquireImageWithoutFence = true;
	return opts;
}

} // namespace sprt::window

#endif
