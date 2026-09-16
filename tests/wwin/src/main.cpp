/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/

// Browser hello-world: the xenolith-installer template scene — a centered label plus the Xenolith
// logo — running as the real engine in wasm/WebGPU. Exercises text/font rendering (glyph atlas),
// a bundled image resource, and the action system in the browser.

#include "XLCommon.h"
#include "XLContext.h"
#include "XLEntryPoint.h"
#include "XL2dScene.h"
#include "XL2dSceneContent.h"
#include "XL2dLabel.h"
#include "XL2dSprite.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using namespace basic2d;

class WwinScene : public Scene2d {
public:
	virtual ~WwinScene() = default;

	virtual bool init(NotNull<AppThread> app, NotNull<core::RenderServerChannel> window,
			const core::FrameConstraints &constraints) override {
		if (!Scene2d::init(app, window, constraints)) {
			return false;
		}

		auto content = Rc<SceneContent2d>::create();

		_logo = content->addChild(Rc<Sprite>::create("xenolith-2-480.png"), ZOrder(1));
		_logo->setAnchorPoint(Anchor::Middle);
		_logo->setContentSize(Size2(300.0f, 248.0f));
		_logo->setTextureAutofit(Autofit::Contain);

		_label = content->addChild(Rc<Label>::create("Hello from Xenolith!"), ZOrder(2));
		_label->setAnchorPoint(Anchor::Middle);
		_label->setFontSize(32);

		setContent(content);
		return true;
	}

	virtual void handleContentSizeDirty() override {
		Scene2d::handleContentSizeDirty();
		auto cs = getContentSize();
		Vec2 center(cs.width / 2.0f, cs.height / 2.0f);

		if (_logo) {
			_logo->setPosition(Vec2(cs.width / 2.0f, cs.height - 160.0f));
		}

		if (_label) {
			_label->setPosition(center);

			// Gently bob the label up and down forever (starts once the scene has a real size).
			if (!_animStarted) {
				_animStarted = true;
				_label->runAction(Rc<RepeatForever>::create(
						Rc<Sequence>::create(Rc<MoveTo>::create(1.4f, center + Vec2(0.0f, 40.0f)),
								Rc<MoveTo>::create(1.4f, center - Vec2(0.0f, 40.0f)))));
			}
		}
	}

	virtual void buildQueueResources(QueueInfo &, core::Queue::Builder &builder) override {
		// Custom + absolute path is used as-is by the filesystem, so it reaches the wasm libc
		// bundle overlay (/app is the browser-fetch mount) directly, bypassing the Bundled-category
		// location lookup (which needs a real on-disk directory the virtual bundle does not have).
		builder.addImage("xenolith-2-480.png",
				core::ImageInfo(core::ImageFormat::R8G8B8A8_UNORM, core::ImageUsage::Sampled,
						core::ImageHints::Opaque),
				FileInfo("/app/resources/xenolith-2-480.png", FileCategory::Custom));
	}

protected:
	Label *_label = nullptr;
	Sprite *_logo = nullptr;
	bool _animStarted = false;
};

DEFINE_PRIMARY_SCENE_CLASS(WwinScene)

} // namespace stappler::xenolith::app

int main(int argc, const char *argv[]) {
	return STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Context::run(argc, argv);
}
