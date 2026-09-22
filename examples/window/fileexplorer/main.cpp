/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons whom the Software is
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

#include "fileexplorer/FileExplorerLayout.h"
#include "fileexplorer/FileExplorerLocale.h"
#include "XLEntryPoint.h"
// MainScene below is a basic2d::Scene2d with a SceneContent2d inside: both are only DECLARED in
// the headers FileExplorerLayout.h pulls in, so their definitions come from these two.
#include "XL2dScene.h"
#include "XL2dSceneContent.h"
#include "XLUiStyleSystem.h"
#include "XLUiStyleResolver.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

// A navigator needs width before it needs anything else: the places tree, the toolbar and a grid
// of at least a few columns have to be on screen together, or none of what the example is about
// can be seen at once.
DEFINE_CONFIG_FUNCTION((ContextConfig &cfg) {
	if (!cfg.window) {
		cfg.window = Rc<sprt::window::WindowInfo>::alloc();
	}

	cfg.window->flags = sprt::window::WindowCreationFlags::Regular;
	cfg.window->minExtent = Extent2(1'180, 720);
});

// The single scene: a stock 2D scene whose content hosts the explorer.
class MainScene : public basic2d::Scene2d {
public:
	virtual ~MainScene() = default;

	virtual bool init(NotNull<AppThread> app, NotNull<core::RenderServerChannel> window,
			const core::FrameConstraints &constraints) override {
		if (!basic2d::Scene2d::init(app, window, constraints)) {
			return false;
		}

		// The string tables, before anything carrying a tag is built - and before the StyleSystem
		// enters, because that is where the `rtl` media flag is first seeded.
		defineFileExplorerLocales();

		auto content = Rc<basic2d::SceneContent2d>::create();
		content->setDefaultLights();

		// The sheet goes on the CONTENT, not on the layout: a context menu is pushed beside the
		// layout rather than under it, and a sheet installed one level lower would style the rows
		// it was opened from and none of the menu that pops out of them.
		content->addSystem(Rc<ui::StyleSystem>::create(getFileExplorerStylesheet()));
		content->addSystem(Rc<ui::StyleResolver>::create(true));

		content->pushLayout(Rc<FileExplorerLayout>::create());
		setContent(content);

		return true;
	}
};

DEFINE_PRIMARY_SCENE_CLASS(MainScene)

} // namespace stappler::xenolith::examples
