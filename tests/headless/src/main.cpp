/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/

// Headless smoke test: run the engine with no window system at all (`--headless`) and drive it
// entirely over the inspector socket. The scene registers a couple of commands so the framed
// protocol's `commands` / `invoke` path has something real to exercise.
//
//   ./headlesstest --headless -W 640 -H 480
//   XENOLITH_INSPECTOR_ADDRESS=unix:/tmp/xl-headless.sock ./headlesstest --headless

#include "XLCommon.h"
#include "XLContext.h"
#include "XLEntryPoint.h"
#include "XL2dScene.h"
#include "XL2dSceneContent.h"
#include "XL2dLayer.h"
#include "XLSceneInspector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using namespace basic2d;

class HeadlessScene : public Scene2d {
public:
	virtual ~HeadlessScene() = default;

	virtual bool init(NotNull<AppThread> app, NotNull<core::RenderServerChannel> window,
			const core::FrameConstraints &constraints) override {
		if (!Scene2d::init(app, window, constraints)) {
			return false;
		}

		auto content = Rc<SceneContent2d>::create();

		// A plain coloured rectangle is enough to tell "the frame rendered" from "the capture is
		// black" by eye, and its colour is what the demo commands change.
		_box = content->addChild(Rc<Layer>::create(Color4F(0.2f, 0.4f, 0.9f, 1.0f)), ZOrder(1));
		_box->setAnchorPoint(Anchor::Middle);
		_box->setContentSize(Size2(200.0f, 120.0f));

		setContent(content);

		// SceneContent gets an inspector attached in its init(), so it is already there.
		inspector::addCommand(content, "set-color",
				"Set the box colour: args { r, g, b } in 0..1",
				[this](Value &&args, Function<void(Value &&)> &&done) {
			if (!_box) {
				Value err;
				err.setString("no box", "error");
				done(sp::move(err));
				return;
			}
			_box->setColor(Color4F(float(args.getDouble("r", 1.0)), float(args.getDouble("g", 1.0)),
					float(args.getDouble("b", 1.0)), 1.0f));

			Value result;
			result.setBool(true, "applied");
			done(sp::move(result));
		});

		inspector::addCommand(content, "box-size", "Resize the box: args { width, height }",
				[this](Value &&args, Function<void(Value &&)> &&done) {
			if (!_box) {
				Value err;
				err.setString("no box", "error");
				done(sp::move(err));
				return;
			}
			_box->setContentSize(Size2(float(args.getDouble("width", 200.0)),
					float(args.getDouble("height", 120.0))));

			Value result;
			result.setDouble(double(_box->getContentSize().width), "width");
			result.setDouble(double(_box->getContentSize().height), "height");
			done(sp::move(result));
		});

		return true;
	}

	virtual void handleContentSizeDirty() override {
		Scene2d::handleContentSizeDirty();

		auto cs = getContentSize();
		if (_box) {
			_box->setPosition(Vec2(cs.width / 2.0f, cs.height / 2.0f));
		}
	}

protected:
	Layer *_box = nullptr;
};

DEFINE_PRIMARY_SCENE_CLASS(HeadlessScene)

} // namespace stappler::xenolith::app

int main(int argc, const char *argv[]) {
	return STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Context::run(argc, argv);
}
