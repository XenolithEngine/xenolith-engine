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

#include "XLCommon.h"
#include "XLEntryPoint.h"
#include "WmHostScene.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::wm {

// The host window: a fixed size (--width/--height, 1280x800 by default), which is the screen the
// planes are laid out on, and nothing to resize or maximize.
DEFINE_CONFIG_FUNCTION((ContextConfig &cfg) {
	if (!cfg.window) {
		cfg.window = Rc<sprt::window::WindowInfo>::alloc();
	}

	if (cfg.window->rect.width == 0) {
		cfg.window->rect.width = 1'280;
	}
	if (cfg.window->rect.height == 0) {
		cfg.window->rect.height = 800;
	}

	auto extent = Extent2(cfg.window->rect.width, cfg.window->rect.height);
	cfg.window->minExtent = extent;
	cfg.window->maxExtent = extent;
	cfg.window->title = "Xenolith OS";
	cfg.window->flags = sprt::window::WindowCreationFlags::AllowMove
			| sprt::window::WindowCreationFlags::AllowMinimize
			| sprt::window::WindowCreationFlags::AllowClose;
});

DEFINE_PRIMARY_SCENE_CLASS(WmHostScene)

} // namespace stappler::xenolith::wm
