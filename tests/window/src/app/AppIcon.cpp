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

#include "XLCommon.h" // IWYU pragma: keep
#include "XLContextInfo.h"
#include "XLEntryPoint.h"
#include "XLWindowInfo.h"

#include "app/AppIcon.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// The icon lives in .rodata via BundleFS (LOCAL_EMBED_DIRS := icons), so it resolves with no
// on-disk bundle, no working directory and no platform-specific resource layout - which is what
// makes it usable from the config function, before anything else has started.
static constexpr StringView s_appIconPath("icons/app-icon.png");

const Rc<WindowIcon> &getAppIcon() {
	// Decoded once and shared by every window: makeWindowIcon resamples the source to the whole
	// size ladder, which is not work to repeat per window.
	static Rc<WindowIcon> s_icon =
			makeWindowIcon(FileInfo{s_appIconPath, FileCategory::Embedded});
	return s_icon;
}

DEFINE_CONFIG_FUNCTION((ContextConfig &cfg) {
	if (!cfg.window) {
		cfg.window = Rc<sprt::window::WindowInfo>::alloc();
	}

	cfg.window->icon = getAppIcon();
});

} // namespace stappler::xenolith::app
