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

#include "agentchat/AgentChatLayout.h"
#include "XLEntryPoint.h"
#include "XL2dScene.h"
#include "XL2dSceneContent.h"
#include "XLUiStyleSystem.h"
#include "XLUiStyleResolver.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

DEFINE_CONFIG_FUNCTION((ContextConfig &cfg) {
	if (!cfg.window) {
		cfg.window = Rc<sprt::window::WindowInfo>::alloc();
	}

	cfg.window->flags = sprt::window::WindowCreationFlags::Regular
			| sprt::window::WindowCreationFlags::PreferServerSideDecoration;
	cfg.window->minExtent = Extent2(900, 700);
});

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

		/* The stylesheet goes HERE and not on the layout. The model picker opens its list as an
		overlay pushed onto this content, which makes it a sibling of the layout rather than a
		descendant: a resolver installed one level down cannot reach it. On the content, one
		recursive resolver covers the layout and every overlay beside it, and `:root` - where the
		custom properties are declared - is the content itself. */
		content->addSystem(Rc<ui::StyleSystem>::create(getAgentChatStylesheet()));
		content->addSystem(Rc<ui::StyleResolver>::create(true));

		content->pushLayout(Rc<AgentChatLayout>::create());
		setContent(content);

		return true;
	}
};

DEFINE_PRIMARY_SCENE_CLASS(MainScene)

} // namespace stappler::xenolith::examples
