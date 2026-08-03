/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/

// Headless smoke test and backend-parity scene: run the engine with no window system at all
// (`--headless`) and drive it entirely over the inspector socket. Every drawable here is built in
// code - there is not a single bundled resource and not a single Label, so the scene renders
// identically on any backend that can rasterize the flat queue, including one with no font support.
//
//   ./headlesstest --headless -W 640 -H 480
//   XENOLITH_INSPECTOR_ADDRESS=unix:/tmp/xl-headless.sock ./headlesstest --headless
//   XL_FLAT_QUEUE=1 ./headlesstest --headless --gapi soft
//
// tests/parity/compare.sh drives this scene through both backends and diffs the screenshots.

#include "XLCommon.h"
#include "XLContext.h"
#include "XLEntryPoint.h"
#include "XL2dScene.h"
#include "XL2dSceneContent.h"
#include "XL2dLayer.h"
#include "XL2dSprite.h"
#include "XL2dVectorSprite.h"
#include "XLSceneInspector.h"
#include "XLDynamicStateSystem.h"

#include <stdlib.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using namespace basic2d;

static constexpr auto CheckerTextureName = "headless-checker";

// Procedural texture: a 4px checker in two saturated colours inside a white frame. The frame is
// what makes nearest and linear tell each other apart - a checker alone repeats its edges, while a
// border produces a single unambiguous gradient ramp under bilinear filtering.
static constexpr uint32_t CheckerExtent = 16;
static constexpr uint32_t CheckerCell = 4;

static void HeadlessScene_writeChecker(uint8_t *mem, uint64_t size) {
	static constexpr uint8_t colors[2][4] = {{230, 60, 40, 255}, {40, 90, 220, 255}};
	static constexpr uint8_t border[4] = {255, 255, 255, 255};

	if (size < uint64_t(CheckerExtent) * CheckerExtent * 4) {
		return;
	}

	for (uint32_t y = 0; y < CheckerExtent; ++y) {
		for (uint32_t x = 0; x < CheckerExtent; ++x) {
			const uint8_t *src = border;
			if (x > 0 && y > 0 && x + 1 < CheckerExtent && y + 1 < CheckerExtent) {
				src = colors[((x / CheckerCell) + (y / CheckerCell)) % 2];
			}
			sprt::memcpy(mem + (y * CheckerExtent + x) * 4, src, 4);
		}
	}
}

class HeadlessScene : public Scene2d {
public:
	virtual ~HeadlessScene() = default;

	virtual bool init(NotNull<AppThread> app, NotNull<core::RenderServerChannel> window,
			const core::FrameConstraints &constraints) override {
		if (!Scene2d::init(app, window, constraints)) {
			return false;
		}

		auto content = Rc<SceneContent2d>::create();

		// Three drawables, all centred and overlapping, each exercising a different path through
		// the renderer. The `show` command isolates them, so a parity case can compare exactly one.

		// Textured quad: sampling, filters, swizzle. Drawn first (bottom). It lives inside a
		// deliberately undersized container so that turning that container's scissor on clips it -
		// which is the only way to exercise per-state scissor from a scene this small.
		_clip = content->addChild(Rc<Node>::create(), ZOrder(1));
		_clip->setAnchorPoint(Anchor::Middle);
		_clip->setContentSize(Size2(90.0f, 90.0f));
		// ApplyForAll, not ApplyForNodesBelow: "below" and "above" split the children by sign of
		// their z-order, and the sprite sits above. With the wrong mode the state is built but
		// never applied, and the clipped case silently renders identically to the unclipped one.
		_clipState =
				_clip->addSystem(Rc<DynamicStateSystem>::create(DynamicStateApplyMode::ApplyForAll));

		_sprite = _clip->addChild(Rc<Sprite>::create(StringView(CheckerTextureName)), ZOrder(1));
		_sprite->setAnchorPoint(Anchor::Middle);
		_sprite->setContentSize(Size2(128.0f, 128.0f));
		_sprite->setSamplerIndex(core::SamplerIndex::DefaultFilterNearest);

		// Vector figure: reaches the backend as CommandType::Deferred, and its tessellated fringe is
		// where a wrong fill rule shows up as seams first.
		_vector = content->addChild(Rc<VectorSprite>::create(Size2(160.0f, 160.0f), makeVectorPath()),
				ZOrder(2));
		_vector->setAnchorPoint(Anchor::Middle);
		_vector->setContentSize(Size2(160.0f, 160.0f));

		// A plain coloured rectangle is enough to tell "the frame rendered" from "the capture is
		// black" by eye, and its colour is what the demo commands change. On top, so that lowering
		// its opacity blends it over the texture underneath.
		_box = content->addChild(Rc<Layer>::create(Color4F(0.2f, 0.4f, 0.9f, 1.0f)), ZOrder(3));
		_box->setAnchorPoint(Anchor::Middle);
		_box->setContentSize(Size2(200.0f, 120.0f));

		setContent(content);

		// The FPS counter changes every frame and is text, so it is the one region that can never
		// match between two runs or two backends. Off by default keeps screenshots comparable.
		setFpsVisible(false);

		registerCommands(content);

		return true;
	}

	virtual void handleContentSizeDirty() override {
		Scene2d::handleContentSizeDirty();

		auto cs = getContentSize();
		auto center = Vec2(cs.width / 2.0f, cs.height / 2.0f);

		if (_box) {
			_box->setPosition(center);
		}
		if (_clip) {
			_clip->setPosition(center);
		}
		if (_sprite) {
			_sprite->setPosition(_clip->getContentSize() / 2.0f);
		}
		if (_vector) {
			_vector->setPosition(center);
		}
	}

protected:
	// A rounded box with a circular hole: two subpaths with opposite winding, so the even-odd rule
	// and the vertex-AA fringe are both exercised on one figure.
	static VectorPath makeVectorPath() {
		VectorPath path;
		path.openForWriting([](vg::PathWriter &writer) {
			writer.addBox(10.0f, 10.0f, 140.0f, 140.0f, 24.0f);
			writer.addCircle(80.0f, 80.0f, 36.0f);
		});
		path.setWindingRule(vg::Winding::EvenOdd);
		path.setFillColor(Color4B(40, 200, 120, 255));
		path.setAntialiased(true);
		return path;
	}

	void registerCommands(SceneContent2d *content) {
		// SceneContent gets an inspector attached in its init(), so it is already there.
		inspector::addCommand(content, "set-color",
				"Set the box colour: args { r, g, b } in 0..1",
				[this](Value &&args, Function<void(Value &&)> &&done) {
			if (!_box) {
				done(makeError("no box"));
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
				done(makeError("no box"));
				return;
			}
			_box->setContentSize(Size2(float(args.getDouble("width", 200.0)),
					float(args.getDouble("height", 120.0))));

			Value result;
			result.setDouble(double(_box->getContentSize().width), "width");
			result.setDouble(double(_box->getContentSize().height), "height");
			done(sp::move(result));
		});

		inspector::addCommand(content, "show",
				"Show only some drawables: args { layer, sprite, vector } as booleans",
				[this](Value &&args, Function<void(Value &&)> &&done) {
			auto apply = [&](Node *node, StringView key) {
				if (node && args.hasValue(key)) {
					node->setVisible(args.getBool(key));
				}
			};
			apply(_box, "layer");
			apply(_clip, "sprite");
			apply(_vector, "vector");

			Value result;
			result.setBool(_box && _box->isVisible(), "layer");
			result.setBool(_clip && _clip->isVisible(), "sprite");
			result.setBool(_vector && _vector->isVisible(), "vector");
			done(sp::move(result));
		});

		inspector::addCommand(content, "filter",
				"Sprite sampler: args { name } = nearest | linear | linear-clamped",
				[this](Value &&args, Function<void(Value &&)> &&done) {
			if (!_sprite) {
				done(makeError("no sprite"));
				return;
			}

			auto name = args.getString("name");
			core::SamplerIndex index = core::SamplerIndex::DefaultFilterNearest;
			if (name == "linear") {
				index = core::SamplerIndex::DefaultFilterLinear;
			} else if (name == "linear-clamped") {
				index = core::SamplerIndex::DefaultFilterLinearClamped;
			} else if (name != "nearest") {
				done(makeError(toString("unknown filter: ", name)));
				return;
			}

			_sprite->setSamplerIndex(index);

			Value result;
			result.setString(name, "filter");
			done(sp::move(result));
		});

		inspector::addCommand(content, "rotate",
				"Rotate the sprite: args { angle } in degrees - takes sampling off the axis-aligned "
				"case",
				[this](Value &&args, Function<void(Value &&)> &&done) {
			if (!_sprite) {
				done(makeError("no sprite"));
				return;
			}
			auto angle = float(args.getDouble("angle"));
			_sprite->setRotation(sprt::math::to_rad(angle));

			Value result;
			result.setDouble(double(angle), "angle");
			done(sp::move(result));
		});

		inspector::addCommand(content, "clip",
				"Clip the sprite to its 90x90 container: args { enabled } - per-state scissor",
				[this](Value &&args, Function<void(Value &&)> &&done) {
			if (!_clipState) {
				done(makeError("no clip state"));
				return;
			}
			auto enabled = args.getBool("enabled");
			if (enabled) {
				_clipState->enableScissor();
			} else {
				_clipState->disableScissor();
			}

			Value result;
			result.setBool(_clipState->isScissorEnabled(), "enabled");
			done(sp::move(result));
		});

		inspector::addCommand(content, "alpha",
				"Box opacity: args { value } in 0..1 - selects the Transparent pipeline",
				[this](Value &&args, Function<void(Value &&)> &&done) {
			if (!_box) {
				done(makeError("no box"));
				return;
			}
			auto value = float(args.getDouble("value", 1.0));
			_box->setOpacity(value);

			Value result;
			result.setDouble(double(_box->getOpacity()), "value");
			done(sp::move(result));
		});
	}

	static Value makeError(StringView text) {
		Value err;
		err.setString(text, "error");
		return err;
	}

	virtual void buildQueueResources(QueueInfo &info, core::Queue::Builder &builder) override {
		// XL_FLAT_QUEUE=1 - the lightweight render queue: no shadows, particles, depth buffer or
		// postprocessing. Same name and same semantics as tests/window, because a backend-parity
		// comparison is only meaningful when both sides build the same frame graph - and the
		// software backend has no choice, it is always served the flat queue.
		if (auto value = ::getenv("XL_FLAT_QUEUE")) {
			if (StringView(value) != "0") {
				info.type = QueueType::Flat;
				log::source().info("HeadlessScene", "Using flat (lightweight) 2d render queue");
			}
		}

		builder.addImage(CheckerTextureName,
				core::ImageInfo(Extent2(CheckerExtent, CheckerExtent),
						core::ImageFormat::R8G8B8A8_UNORM, core::ImageUsage::Sampled,
						core::ImageHints::Opaque),
				[](uint8_t *mem, uint64_t size, const core::ImageData::DataCallback &) {
			HeadlessScene_writeChecker(mem, size);
		});
	}

	Layer *_box = nullptr;
	Node *_clip = nullptr;
	DynamicStateSystem *_clipState = nullptr;
	Sprite *_sprite = nullptr;
	VectorSprite *_vector = nullptr;
};

DEFINE_PRIMARY_SCENE_CLASS(HeadlessScene)

} // namespace stappler::xenolith::app

int main(int argc, const char *argv[]) {
	return STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Context::run(argc, argv);
}
