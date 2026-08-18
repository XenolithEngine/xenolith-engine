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
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#include "XLCommon.h"

#include "dock/DockDemoLayout.h"
#include "XLEntryPoint.h"
// MainScene below is a basic2d::Scene2d with a SceneContent2d inside: both are only DECLARED in
// the headers DockDemoLayout.h pulls in, so their definitions come from these two.
#include "XL2dScene.h"
#include "XL2dSceneContent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

// Window identity for the example: a regular window with user-space decorations, and a floor so the
// dock tree always has room to show its frames side by side.
DEFINE_CONFIG_FUNCTION((ContextConfig &cfg) {
	if (!cfg.window) {
		cfg.window = Rc<sprt::window::WindowInfo>::alloc();
	}

	cfg.window->flags = sprt::window::WindowCreationFlags::Regular
			| sprt::window::WindowCreationFlags::UserSpaceDecorations;
	cfg.window->minExtent = Extent2(1024, 640);
});

// The single scene: a stock 2D scene whose content hosts the DockFrame demo.
class MainScene : public basic2d::Scene2d {
public:
	virtual ~MainScene() = default;

	virtual bool init(NotNull<AppThread> app, NotNull<core::RenderServerChannel> window,
			const core::FrameConstraints &constraints) override {
		if (!basic2d::Scene2d::init(app, window, constraints)) {
			return false;
		}

		auto content = Rc<basic2d::SceneContent2d>::create();
		content->setDefaultLights();
		content->pushLayout(Rc<DockDemoLayout>::create());
		setContent(content);

		return true;
	}
};

DEFINE_PRIMARY_SCENE_CLASS(MainScene)

} // namespace stappler::xenolith::examples
