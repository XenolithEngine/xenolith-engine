/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#include "widgets/HitTestLayout.h"
#include "XL2dLayer.h"
#include "XLDynamicStateSystem.h"
#include "XLDirector.h"
#include "XLInputDispatcher.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

// Where the base sits inside the layout's work area. Anchored bottom-left, so every rect below is
// its own origin in the base's space.
static constexpr auto Region =
		Rect(60.0f, 60.0f, HitTestLayout::RegionWidth, HitTestLayout::RegionHeight);

static constexpr auto Under = Rect(40.0f, 240.0f, 220.0f, 140.0f);
static constexpr auto Over = Rect(120.0f, 280.0f, 120.0f, 70.0f);
static constexpr auto Hidden = Rect(320.0f, 280.0f, 160.0f, 100.0f);

// A square, so that turning it 45 degrees makes the difference between its box and itself as large
// as it gets: the box grows by sqrt(2), and its corners are 42 units of nothing
static constexpr auto Rotated = Rect(240.0f, 30.0f, 120.0f, 120.0f);

static constexpr auto Tagged = Rect(20.0f, 20.0f, 160.0f, 120.0f);
static constexpr auto Pad = Rect(20.0f, 170.0f, 80.0f, 40.0f);

// The child overflows the scissor to the right by exactly its own width again
static constexpr auto Clip = Rect(400.0f, 20.0f, 120.0f, 140.0f);
static constexpr auto Clipped = Rect(0.0f, 0.0f, 240.0f, 140.0f);

} // namespace

bool HitTestLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	_region = addBox(this, "region", Region, ZOrder(0), Color::BlueGrey_800, FlagA);

	_under = addBox(_region, "under", Under, ZOrder(1), Color::Teal_700, FlagA);
	_over = addBox(_region, "over", Over, ZOrder(5), Color::Amber_600, FlagA);

	_hidden = addBox(_region, "hidden", Hidden, ZOrder(5), Color::Red_500, FlagA);
	_hidden->setVisible(false);

	_rotated = addBox(_region, "rotated", Rotated, ZOrder(3), Color::Purple_500, FlagA);
	// Around its own middle, so the box and the square share a centre and differ only in the corners
	_rotated->setAnchorPoint(Anchor::Middle);
	_rotated->setPosition(Rotated.origin + Vec2(Rotated.size.width, Rotated.size.height) / 2.0f);
	_rotated->setRotation(sprt::math::to_rad(45.0f));

	_tagged = addBox(_region, "tagged", Tagged, ZOrder(3), Color::Green_600, FlagB);
	_pad = addBox(_region, "pad", Pad, ZOrder(4), Color::Orange_600, FlagA);

	// The clip itself takes part in nothing: it is the frame state, not a target
	_clip = addBox(_region, "clip", Clip, ZOrder(3), Color::Grey_700, HitTestFlags::None);
	// ApplyForAll, not the default DoNotApply: the scissor has to be in force while the CHILDREN
	// are visited, which is when they register
	_clip->addSystem(Rc<DynamicStateSystem>::create(DynamicStateApplyMode::ApplyForAll))
			->enableScissor();

	_clipped = addBox(_clip, "clipped", Clipped, ZOrder(1), Color::Indigo_400, FlagA);

	return true;
}

void HitTestLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	// Pinned to the work area's top-left, so every expected number is derived from the constants
	// above and a window resize does not move them
	if (_region) {
		_region->setPosition(Vec2(Region.origin.x, getWorkTop() - Region.size.height - 20.0f));
	}
}

Node *HitTestLayout::addBox(Node *parent, StringView name, const Rect &rect, ZOrder z,
		const Color4F &color, HitTestFlags flags) {
	auto node = parent->addChild(Rc<basic2d::Layer>::create(color), z);
	node->setName(name);
	node->setAnchorPoint(Anchor::BottomLeft);
	node->setPosition(rect.origin);
	node->setContentSize(rect.size);
	node->setHitTestFlags(flags);
	return node;
}

Value HitTestLayout::encodeRegistry() const {
	Value ret;

	auto dispatcher = getDirector() ? getDirector()->getInputDispatcher() : nullptr;
	if (!dispatcher) {
		return ret;
	}

	Value records;

	// Every application-registered record, topmost first. The engine's own flags are left out: this
	// scene has widgets of its own (the test app's chrome), and their listeners are not the subject
	dispatcher->foreachHitTest(FlagA | FlagB, [&](const InputListenerStorage::HitTestRec &rec) {
		Value item;
		item.setString(rec.node->getName(), "name");
		item.setInteger(int64_t(toInt(rec.flags & (FlagA | FlagB))), "flags");
		item.setInteger(int64_t(rec.order), "order");
		item.setDouble(rec.worldRect.origin.x, "x");
		item.setDouble(rec.worldRect.origin.y, "y");
		item.setDouble(rec.worldRect.size.width, "width");
		item.setDouble(rec.worldRect.size.height, "height");
		item.setBool(rec.scissorEnabled, "clipped");
		records.addValue(sp::move(item));
		return true;
	});

	ret.setValue(sp::move(records), "records");
	ret.setInteger(int64_t(toInt(dispatcher->getHitTestMask())), "mask");
	ret.setInteger(int64_t(dispatcher->getCommittedGeneration()), "generation");
	return ret;
}

Value HitTestLayout::query(const Vec2 &world, HitTestFlags mask, float padding) const {
	Value ret;

	auto dispatcher = getDirector() ? getDirector()->getInputDispatcher() : nullptr;
	if (!dispatcher) {
		return ret;
	}

	Value hits;
	dispatcher->foreachHitTest(mask, [&](const InputListenerStorage::HitTestRec &rec) {
		// The containment test is the asker's, which is the whole reason the walk hands over
		// records rather than answers: this stand's padding is not a drop target's
		if (!rec.contains(world, padding)) {
			return true;
		}
		hits.addString(rec.node->getName());
		return true;
	});

	// Topmost first, because the walk is; the check reads hits[0] as "what would win"
	ret.setValue(sp::move(hits), "hits");
	return ret;
}

void HitTestLayout::registerCommands() {
	TestLayout::registerCommands();

	addCommand("registry", "Every registered node this frame, topmost first, plus the frame mask",
			[this](Value &&) { return encodeRegistry(); });

	addCommand("query", "What is under a world point: {x, y, flags, padding}",
			[this](Value &&args) {
		const Value &in = args;
		auto mask = in.isInteger("flags") ? HitTestFlags(uint32_t(in.getInteger("flags")))
										  : (FlagA | FlagB);
		auto ret = query(Vec2(float(in.getDouble("x")), float(in.getDouble("y"))), mask,
				float(in.getDouble("padding")));
		ret.setValue(encodeRegistry(), "registry");
		return ret;
	});

	addCommand("rects", "The world box of every named node in the stand", [this](Value &&) {
		Value ret;
		auto encode = [&](StringView name, const Node *node) {
			if (!node) {
				return;
			}
			Value item;
			const auto world = node->getWorldBoundingBox();
			item.setDouble(world.origin.x, "x");
			item.setDouble(world.origin.y, "y");
			item.setDouble(world.size.width, "width");
			item.setDouble(world.size.height, "height");
			item.setBool(node->isVisible(), "visible");
			ret.setValue(sp::move(item), name);
		};
		encode("region", _region);
		encode("under", _under);
		encode("over", _over);
		encode("rotated", _rotated);
		encode("hidden", _hidden);
		encode("clip", _clip);
		encode("clipped", _clipped);
		encode("pad", _pad);
		encode("tagged", _tagged);
		return ret;
	});
}

} // namespace stappler::xenolith::app
