/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/

// Headless smoke test and backend-parity scene: run the engine with no window system at all
// (`--headless`) and drive it entirely over the inspector socket. Every drawable here is built in
// code - there is not a single bundled resource and not a single Label, so the scene renders
// identically on any backend that can rasterize the flat queue, including one with no font support.
//
//   ./headlesstest --headless --width 640 --height 480
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
#include "XL2dLabel.h"
#include "XLSceneInspector.h"
#include "XLDynamicStateSystem.h"

#include <stdlib.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using namespace basic2d;

static constexpr auto CheckerTextureName = "headless-checker";

// A second, deliberately large texture. The checker above is 16x16, so a sprite of any real size
// magnifies it enormously: every fetch lands in L1 and neighbouring pixels read the same texel.
// That measures nothing about a sampler. This one is bigger than L2, so a 1:1 or minified draw
// actually streams texels from memory, which is what a real sprite does.
//
// Tiles rather than noise: noise would measure the memory system and nothing else, while a
// structured pattern keeps a wrong fetch visible to the eye on a screenshot.
static constexpr auto LargeTextureName = "headless-large";
static constexpr uint32_t LargeExtent = 1'024;
static constexpr uint32_t LargeCell = 32;

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

static void HeadlessScene_writeLarge(uint8_t *mem, uint64_t size) {
	if (size < uint64_t(LargeExtent) * LargeExtent * 4) {
		return;
	}

	for (uint32_t y = 0; y < LargeExtent; ++y) {
		for (uint32_t x = 0; x < LargeExtent; ++x) {
			// Cell index drives the colour, and the position inside the cell shades it: enough
			// variety that no two neighbouring tiles look alike, so a misaddressed fetch shows.
			auto cell = (x / LargeCell) + (y / LargeCell) * (LargeExtent / LargeCell);
			uint8_t px[4] = {uint8_t(40 + (cell * 37) % 200), uint8_t(40 + (cell * 91) % 200),
				uint8_t(40 + (cell * 53) % 200), 255};
			px[(cell % 3)] = uint8_t(px[cell % 3] / 2 + (x % LargeCell) * 4);
			sprt::memcpy(mem + (uint64_t(y) * LargeExtent + x) * 4, px, 4);
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
		_vector = content->addChild(Rc<VectorSprite>::create(Size2(160.0f, 160.0f)), ZOrder(2));
		_vector->addPath(makeVectorPath(), "figure");
		_vector->setAnchorPoint(Anchor::Middle);
		_vector->setContentSize(Size2(160.0f, 160.0f));

		// A plain coloured rectangle is enough to tell "the frame rendered" from "the capture is
		// black" by eye, and its colour is what the demo commands change. On top, so that lowering
		// its opacity blends it over the texture underneath.
		_box = content->addChild(Rc<Layer>::create(Color4F(0.2f, 0.4f, 0.9f, 1.0f)), ZOrder(3));
		_box->setAnchorPoint(Anchor::Middle);
		_box->setContentSize(Size2(200.0f, 120.0f));

		// Text. The only drawable here that needs a font backend at all, and the reason the scene
		// can no longer be rendered by a backend without one. Latin + Cyrillic + digits, so a
		// missing glyph shows up as a hole rather than as an empty frame.
		_label = content->addChild(Rc<Label>::create(), ZOrder(4));
		_label->setAnchorPoint(Anchor::Middle);
		_label->setFontSize(20);
		_label->setString("Xenolith Кириллица 0123");
		_label->setColor(Color4F(0.05f, 0.05f, 0.1f, 1.0f));

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
		if (_label) {
			_label->setPosition(center);
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
		// Stroke and dash controls: the vector figure is the only drawable whose geometry is
		// tessellated per frame, so this is where a dash pattern can actually be looked at.
		inspector::addCommand(content, "stroke",
				"Stroke the vector figure: args { width, cap: butt|round|square, "
				"dash: [on, off, ...], offset }",
				[this](Value &&args, Function<void(Value &&)> &&done) {
			auto path = _vector ? _vector->getPath("figure") : nullptr;
			if (!path) {
				done(makeError("no vector path"));
				return;
			}

			const auto width = float(args.getDouble("width", 0.0));
			path->setStrokeWidth(width);
			path->setStrokeColor(Color4B(230, 80, 40, 255));
			path->setStyle(width > 0.0f ? vg::DrawFlags::FillAndStroke : vg::DrawFlags::Fill);

			auto cap = args.getString("cap");
			if (cap == "round") {
				path->setLineCup(vg::LineCup::Round);
			} else if (cap == "square") {
				path->setLineCup(vg::LineCup::Square);
			} else if (cap == "butt") {
				path->setLineCup(vg::LineCup::Butt);
			}

			mem_std::Vector<float> dash;
			for (auto &it : args.getArray("dash")) { dash.emplace_back(float(it.getDouble())); }
			path->setDashArray(dash);
			path->setDashOffset(float(args.getDouble("offset", 0.0)));

			Value result;
			result.setDouble(path->getStrokeWidth(), "width");
			result.setInteger(int64_t(path->getDashArray().size()), "dash");
			result.setDouble(path->getDashOffset(), "offset");
			done(sp::move(result));
		});

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

		// Same as box-size, for the sprite. Parity does not need it - a checkerboard is a
		// checkerboard at any size - but the rasterizer benchmark does: at the default 128x128 a
		// textured span kernel is measured mostly on per-command overhead, and the whole question
		// there is throughput over a large area.
		inspector::addCommand(content, "sprite-size",
				"Resize the sprite and its clip node: args { width, height }",
				[this](Value &&args, Function<void(Value &&)> &&done) {
			if (!_sprite || !_clip) {
				done(makeError("no sprite"));
				return;
			}
			auto size = Size2(float(args.getDouble("width", 128.0)),
					float(args.getDouble("height", 128.0)));
			_sprite->setContentSize(size);
			// The clip node bounds the sprite, so it has to grow too or the resize is invisible.
			_clip->setContentSize(size);

			Value result;
			result.setDouble(double(_sprite->getContentSize().width), "width");
			result.setDouble(double(_sprite->getContentSize().height), "height");
			done(sp::move(result));
		});

		// Which texture the sprite samples. Only the benchmark needs it: the 16x16 checker is
		// magnified by any real sprite size, and a magnified fetch measures the cache, not the
		// sampler. With the large texture at 1:1 every pixel reads its own texel.
		inspector::addCommand(content, "sprite-image",
				"Pick the sprite's texture: args { name } = checker | large",
				[this](Value &&args, Function<void(Value &&)> &&done) {
			if (!_sprite) {
				done(makeError("no sprite"));
				return;
			}
			auto name = args.getString("name");
			if (name == "large") {
				_sprite->setTexture(StringView(LargeTextureName));
			} else if (name == "checker" || name.empty()) {
				_sprite->setTexture(StringView(CheckerTextureName));
			} else {
				done(makeError("unknown texture: use checker or large"));
				return;
			}

			Value result;
			result.setString(name.empty() ? StringView("checker") : name, "name");
			done(sp::move(result));
		});

		// A grid of small quads. The rest of the scene is a handful of large objects, which hides
		// everything that costs per primitive rather than per pixel - triangle setup, the bounding
		// box scan, the division by area. At count=40 this is 1600 quads instead of two.
		//
		// The gap matters: without it the cells merge into one flat fill and the case goes back to
		// measuring memory bandwidth.
		inspector::addCommand(content, "grid",
				"Grid of small quads: args { count } cells per side, 0 removes it",
				[this](Value &&args, Function<void(Value &&)> &&done) {
			auto content2d = getContent();
			if (!content2d) {
				done(makeError("no content"));
				return;
			}

			for (auto &it : _grid) { it->removeFromParent(); }
			_grid.clear();

			auto count = uint32_t(sprt::max(args.getInteger("count", 0), int64_t(0)));
			if (count > 0) {
				auto cs = getContentSize();
				auto cellW = cs.width / float(count);
				auto cellH = cs.height / float(count);
				constexpr float gap = 2.0f;

				for (uint32_t y = 0; y < count; ++y) {
					for (uint32_t x = 0; x < count; ++x) {
						auto tint = float((x + y) % 3) / 3.0f;
						auto node = content2d->addChild(
								Rc<Layer>::create(Color4F(0.15f + tint, 0.55f - tint * 0.4f, 0.85f,
										1.0f)),
								ZOrder(0));
						node->setAnchorPoint(Anchor::BottomLeft);
						node->setContentSize(Size2(sprt::max(cellW - gap, 1.0f),
								sprt::max(cellH - gap, 1.0f)));
						node->setPosition(Vec2(float(x) * cellW, float(y) * cellH));
						_grid.emplace_back(node);
					}
				}
			}

			Value result;
			result.setInteger(int64_t(_grid.size()), "quads");
			done(sp::move(result));
		});

		inspector::addCommand(content, "show",
				"Show only some drawables: args { layer, sprite, vector, label } as booleans",
				[this](Value &&args, Function<void(Value &&)> &&done) {
			auto apply = [&](Node *node, StringView key) {
				if (node && args.hasValue(key)) {
					node->setVisible(args.getBool(key));
				}
			};
			apply(_box, "layer");
			apply(_clip, "sprite");
			apply(_vector, "vector");
			apply(_label, "label");

			Value result;
			result.setBool(_box && _box->isVisible(), "layer");
			result.setBool(_clip && _clip->isVisible(), "sprite");
			result.setBool(_vector && _vector->isVisible(), "vector");
			result.setBool(_label && _label->isVisible(), "label");
			done(sp::move(result));
		});

		inspector::addCommand(content, "text", "Set the label string: args { value }",
				[this](Value &&args, Function<void(Value &&)> &&done) {
			if (!_label) {
				done(makeError("no label"));
				return;
			}
			_label->setString(args.getString("value"));

			Value result;
			result.setString(args.getString("value"), "value");
			done(sp::move(result));
		});

		inspector::addCommand(content, "font-size",
				"Set the label font size in points: args { value }",
				[this](Value &&args, Function<void(Value &&)> &&done) {
			if (!_label) {
				done(makeError("no label"));
				return;
			}
			auto size = uint16_t(args.getInteger("value", 20));
			_label->setFontSize(size);

			Value result;
			result.setInteger(int64_t(size), "value");
			done(sp::move(result));
		});

		// Underlines do not come from a glyph: they are quads keyed to CharId::SourceMax, whose
		// atlas entry is a single white pixel. That is a separate path through the renderer from
		// every other quad in this scene.
		inspector::addCommand(content, "underline",
				"Underline the label: args { enabled }",
				[this](Value &&args, Function<void(Value &&)> &&done) {
			if (!_label) {
				done(makeError("no label"));
				return;
			}
			auto enabled = args.getBool("enabled");
			_label->setTextDecoration(
					enabled ? font::TextDecoration::Underline : font::TextDecoration::None);

			Value result;
			result.setBool(enabled, "enabled");
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

		builder.addImage(LargeTextureName,
				core::ImageInfo(Extent2(LargeExtent, LargeExtent), core::ImageFormat::R8G8B8A8_UNORM,
						core::ImageUsage::Sampled, core::ImageHints::Opaque),
				[](uint8_t *mem, uint64_t size, const core::ImageData::DataCallback &) {
			HeadlessScene_writeLarge(mem, size);
		});
	}

	Layer *_box = nullptr;
	Node *_clip = nullptr;
	DynamicStateSystem *_clipState = nullptr;
	Sprite *_sprite = nullptr;
	VectorSprite *_vector = nullptr;
	// Non-owning: the nodes belong to the content, this is just the list to take down on the next
	// `grid` command.
	Vector<Node *> _grid;
	Label *_label = nullptr;
};

DEFINE_PRIMARY_SCENE_CLASS(HeadlessScene)

} // namespace stappler::xenolith::app

int main(int argc, const char *argv[]) {
	return STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Context::run(argc, argv);
}
