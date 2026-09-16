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

#include "XLCommon.h"

#include "render/RenderLevelLayout.h"
#include "XLAction.h"
#include "director/XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using basic2d::Layer;

namespace {

static constexpr core::RenderingLevel s_levels[] = {
	core::RenderingLevel::Default,
	core::RenderingLevel::Solid,
	core::RenderingLevel::Surface,
	core::RenderingLevel::Transparent,
};

static constexpr StringView s_levelNames[] = {
	StringView("default"),
	StringView("solid"),
	StringView("surface"),
	StringView("transparent"),
};

static constexpr float BoxSize = 90.0f;
static constexpr float BoxStep = 120.0f;

} // namespace

bool RenderLevelLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	// Row 1: the boxes are in FRONT of the backdrop.
	_frontBackdrop = addChild(Rc<Layer>::create(Color::Blue_900), ZOrder(1));
	makeRow(_front, ZOrder(2), Color::Amber_500);

	// Row 2: the boxes are BEHIND the cover, which is opaque and solid.
	makeRow(_behind, ZOrder(1), Color::Amber_500);
	_behindCover = addChild(Rc<Layer>::create(Color::Blue_900), ZOrder(2));

	// Row 3: same as row 1, but every box is created at Solid and switched at runtime - the level
	// of a node that has already been drawn must still take effect.
	_switchedBackdrop = addChild(Rc<Layer>::create(Color::Blue_900), ZOrder(1));
	makeRow(_switched, ZOrder(2), Color::Amber_500);
	for (auto &it : _switched) { it->setRenderingLevel(core::RenderingLevel::Solid); }

	// Row 4: a TRANSLUCENT strip - transparent by its alpha - and two surface boxes in front of it.
	_strip = addChild(Rc<Layer>::create(Color::Blue_900), ZOrder(3));
	_strip->setName("strip");
	_strip->setOpacity(0.9f);
	_strip->setRenderingLevel(core::RenderingLevel::Transparent);
	_over = addChild(Rc<Layer>::create(Color::Amber_500), ZOrder(4));
	_over->setName("over");
	_over->setRenderingLevel(core::RenderingLevel::Surface);
	_apart = addChild(Rc<Layer>::create(Color::Amber_500), ZOrder(4));
	_apart->setName("apart");
	_apart->setRenderingLevel(core::RenderingLevel::Surface);

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(1.0f), [this] { runPhase1(); }));

	return true;
}

void RenderLevelLayout::makeRow(Layer *(&out)[LevelCount], ZOrder z, const Color4F &color) {
	for (size_t i = 0; i < LevelCount; ++i) {
		auto box = addChild(Rc<Layer>::create(color), z);
		box->setName(s_levelNames[i]);
		box->setRenderingLevel(s_levels[i]);
		out[i] = box;
	}
}

void RenderLevelLayout::runPhase1() {
	// The nodes were drawn as Solid first; switching now is the case that used to keep the old
	// material, because setRenderingLevel only refreshed it for a node that was already running.
	for (size_t i = 0; i < LevelCount; ++i) { _switched[i]->setRenderingLevel(s_levels[i]); }

	auto expect = [&](bool cond, StringView what) {
		++_checks;
		if (!cond) {
			++_failures;
			log::source().error("RenderLevelTest", what);
		}
	};

	// the level a node reports must be the one it was given, whether or not it had entered the
	// scene when it was set
	for (size_t i = 0; i < LevelCount; ++i) {
		expect(_front[i]->getRenderingLevel() == s_levels[i],
				string::toString<Interface>("front box '", s_levelNames[i],
						"' does not report its level"));
		expect(_switched[i]->getRenderingLevel() == s_levels[i],
				string::toString<Interface>("switched box '", s_levelNames[i],
						"' does not report its level"));
	}

	log::source()
			.warn("RenderLevelTest", "SUMMARY: ", _checks, " checks, ", _failures,
					" failures; the rest is a visual check - rows 1 and 3 must show four identical "
					"boxes, " "row 2 none");
}

void RenderLevelLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const float left = 40.0f;
	const float rowWidth = BoxStep * float(LevelCount) + 30.0f;

	auto placeRow = [&](Layer *backdrop, Layer *(&row)[LevelCount], float top) {
		if (backdrop) {
			backdrop->setAnchorPoint(Vec2(0.0f, 1.0f));
			backdrop->setPosition(Vec2(left - 15.0f, top + 15.0f));
			backdrop->setContentSize(Size2(rowWidth, BoxSize + 30.0f));
		}
		for (size_t i = 0; i < LevelCount; ++i) {
			row[i]->setAnchorPoint(Vec2(0.0f, 1.0f));
			row[i]->setPosition(Vec2(left + float(i) * BoxStep, top));
			row[i]->setContentSize(Size2(BoxSize, BoxSize));
		}
	};

	const float top = getWorkTop() - 30.0f;
	placeRow(_frontBackdrop, _front, top);
	placeRow(_behindCover, _behind, top - 160.0f);
	placeRow(_switchedBackdrop, _switched, top - 320.0f);

	// Row 4: the strip is as wide as two boxes; `over` sits inside it, `apart` well to its right.
	const float stripTop = top - 480.0f;
	_strip->setAnchorPoint(Vec2(0.0f, 1.0f));
	_strip->setPosition(Vec2(left - 15.0f, stripTop + 15.0f));
	_strip->setContentSize(Size2(BoxStep + BoxSize + 30.0f, BoxSize + 30.0f));
	_over->setAnchorPoint(Vec2(0.0f, 1.0f));
	_over->setPosition(Vec2(left, stripTop));
	_over->setContentSize(Size2(BoxSize, BoxSize));
	placeApart();
}

void RenderLevelLayout::placeApart() {
	const float left = 40.0f;
	const float stripTop = getWorkTop() - 30.0f - 480.0f;
	_apart->setAnchorPoint(Vec2(0.0f, 1.0f));
	// over the strip's second slot, or three steps right of it - clear of the strip by a whole box
	_apart->setPosition(Vec2(left + (_apartOverlaps ? BoxStep : BoxStep * 3.0f), stripTop));
	_apart->setContentSize(Size2(BoxSize, BoxSize));
}

Value RenderLevelLayout::encodeState() const {
	Value ret;
	auto box = [&](Layer *node, StringView name) {
		Value v;
		const auto size = node->getContentSize();
		const auto at = node->convertToWorldSpace(Vec2(size.width * 0.5f, size.height * 0.5f));
		v.setDouble(at.x, "x");
		v.setDouble(at.y, "y");
		switch (node->getRenderingLevel()) {
		case core::RenderingLevel::Solid: v.setString("solid", "level"); break;
		case core::RenderingLevel::Surface: v.setString("surface", "level"); break;
		case core::RenderingLevel::Transparent: v.setString("transparent", "level"); break;
		default: v.setString("other", "level"); break;
		}
		ret.setValue(sp::move(v), name);
	};
	box(_strip, "strip");
	box(_over, "over");
	box(_apart, "apart");
	ret.setBool(_apartOverlaps, "apartOverlaps");

	if (auto director = _director) {
		auto &stat = director->getDrawStat();
		ret.setInteger(stat.surfacePromotedCmds, "surfacePromoted");
		ret.setInteger(stat.surfaceCmds, "surfaceCmds");
		ret.setInteger(stat.transparentCmds, "transparentCmds");
	}
	ret.setDouble(_contentSize.height, "height");
	return ret;
}

void RenderLevelLayout::registerCommands() {
	addCommand("state", "The row-4 boxes (centres in window coordinates) and the frame's pass counters",
			[this](Value &&) { return encodeState(); });

	addCommand("apart", "Move the second surface box over the strip or clear of it: { overlap }",
			[this](Value &&args) {
		_apartOverlaps = args.getBool("overlap");
		placeApart();
		return encodeState();
	});
}

} // namespace stappler::xenolith::app
