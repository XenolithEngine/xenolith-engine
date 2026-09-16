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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_LINUX_DRM_SPRTWINLINUXDRMDISPLAYCONFIGMANAGER_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_LINUX_DRM_SPRTWINLINUXDRMDISPLAYCONFIGMANAGER_H_

#include "SPRTWinLinuxDrmDevice.h"

#if SPRT_LINUX

namespace sprt::window {

// Monitor/mode enumeration for direct-to-display (KMS) mode.
//
// Read-only by design: applyDisplayConfig is not implemented because the gAPI
// takes DRM master when it acquires the display (vkAcquireDrmDisplayEXT), so a
// drmModeSetCrtc from here would fight it. Mode switching goes through the gAPI
// instead (PresentationEngine picks a display mode), matching DRM modes to gAPI
// ones by (width, height, rate).
class SPRT_API DrmDisplayConfigManager : public DisplayConfigManager {
public:
	virtual ~DrmDisplayConfigManager() = default;

	virtual bool init(NotNull<DrmDevice>, Function<void(NotNull<DisplayConfigManager>)> &&);

	virtual void invalidate() override;

	void update();

protected:
	virtual void prepareDisplayConfigUpdate(Function<void(DisplayConfig *)> &&) override;
	virtual void applyDisplayConfig(NotNull<DisplayConfig>, Function<void(Status)> &&) override;

	Rc<DrmDevice> _device;
};

} // namespace sprt::window

#endif // SPRT_LINUX

#endif /* CORE_RUNTIME_PRIVATE_WINDOW_LINUX_DRM_SPRTWINLINUXDRMDISPLAYCONFIGMANAGER_H_ */
