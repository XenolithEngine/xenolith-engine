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

#include "InstallerInit.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

DEFINE_CONFIG_FUNCTION((ContextConfig &cfg) {
	if (!cfg.window) {
		cfg.window = Rc<sprt::window::WindowInfo>::alloc();
	}

	// Acquire user-space decorations on all platforms
	cfg.window->flags = sprt::window::WindowCreationFlags::Regular
			| sprt::window::WindowCreationFlags::UserSpaceDecorations
			| sprt::window::WindowCreationFlags::PreferServerSideCursors;

	// design.md: the main window may not go below 1024x768. `rect` already defaults to that size;
	// this is the floor the backend clamps to and forwards to the window manager, so the layout
	// below never has to cope with a width the tree pane and a table cannot share.
	cfg.window->minExtent = Extent2(1'024, 768);
});

} // namespace stappler::xenolith::installer
