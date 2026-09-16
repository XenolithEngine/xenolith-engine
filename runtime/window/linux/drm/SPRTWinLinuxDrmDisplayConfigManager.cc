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

#include "SPRTWinLinuxDrmDisplayConfigManager.h"

#if SPRT_LINUX

namespace sprt::window {

bool DrmDisplayConfigManager::init(NotNull<DrmDevice> dev,
		Function<void(NotNull<DisplayConfigManager>)> &&cb) {
	if (!DisplayConfigManager::init(sprt::move(cb))) {
		return false;
	}

	// KMS scans out at the mode's own resolution, no compositor-side upscaling
	_scalingMode = ScalingMode::DirectScaling;
	_device = dev;

	update();

	return true;
}

void DrmDisplayConfigManager::invalidate() {
	_device = nullptr;
	DisplayConfigManager::invalidate();
}

void DrmDisplayConfigManager::update() { prepareDisplayConfigUpdate(nullptr); }

void DrmDisplayConfigManager::prepareDisplayConfigUpdate(Function<void(DisplayConfig *)> &&cb) {
	if (!_device) {
		if (cb) {
			cb(nullptr);
		}
		return;
	}

	auto config = Rc<DisplayConfig>::create();
	if (!_device->readConfig(config)) {
		if (cb) {
			cb(nullptr);
		}
		return;
	}

	if (cb) {
		cb(config);
	}

	handleConfigChanged(config);
}

void DrmDisplayConfigManager::applyDisplayConfig(NotNull<DisplayConfig>, Function<void(Status)> &&cb) {
	// See the class comment: the gAPI holds DRM master for the acquired display,
	// so mode changes must go through it, not through drmModeSetCrtc from here.
	cb(Status::ErrorNotImplemented);
}

} // namespace sprt::window

#endif // SPRT_LINUX
