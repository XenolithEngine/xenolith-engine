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

#include "render/Scale9Layout.h"
#include "XLTexture.h"
#include "XLResourceCache.h"
#include "XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

static constexpr StringView TextureName("scale9-frame");

// The sub-rect the `atlas` sprite looks into: a quarter of the picture, offset so that neither its
// origin nor its size is a round fraction of the whole - a check that passed on (0, 0, 0.5, 0.5)
// alone could still be dividing by the wrong number.
static constexpr float AtlasX = 0.25f;
static constexpr float AtlasY = 0.125f;
static constexpr float AtlasW = 0.5f;
static constexpr float AtlasH = 0.5f;

// View coordinates are asserted to hundredths - they are laid out in points, and a raw float would
// make an expected value depend on formatting.
static Value encodeViewRect(const Rect &rect) {
	Value ret;
	ret.setInteger(int64_t(std::lround(rect.origin.x * 100.0f)), "x");
	ret.setInteger(int64_t(std::lround(rect.origin.y * 100.0f)), "y");
	ret.setInteger(int64_t(std::lround(rect.size.width * 100.0f)), "w");
	ret.setInteger(int64_t(std::lround(rect.size.height * 100.0f)), "h");
	return ret;
}

// Texture coordinates are normalized, so hundredths would round a 32-pixel border of a 480-pixel
// image (0.0667) into the same number as a 48-pixel one. Millionths keep them apart with room to
// spare: one unit here is a thousandth of a pixel.
static Value encodeTexRect(const Rect &rect) {
	Value ret;
	ret.setInteger(int64_t(std::lround(rect.origin.x * 1'000'000.0f)), "u");
	ret.setInteger(int64_t(std::lround(rect.origin.y * 1'000'000.0f)), "v");
	ret.setInteger(int64_t(std::lround(rect.size.width * 1'000'000.0f)), "uw");
	ret.setInteger(int64_t(std::lround(rect.size.height * 1'000'000.0f)), "vh");
	return ret;
}

static StringView autofitName(basic2d::Autofit value) {
	switch (value) {
	case basic2d::Autofit::None: return StringView("none");
	case basic2d::Autofit::Width: return StringView("width");
	case basic2d::Autofit::Height: return StringView("height");
	case basic2d::Autofit::Cover: return StringView("cover");
	case basic2d::Autofit::Contain: return StringView("contain");
	}
	return StringView("unknown");
}

static basic2d::Autofit autofitValue(StringView name) {
	if (name == "width") {
		return basic2d::Autofit::Width;
	} else if (name == "height") {
		return basic2d::Autofit::Height;
	} else if (name == "cover") {
		return basic2d::Autofit::Cover;
	} else if (name == "contain") {
		return basic2d::Autofit::Contain;
	}
	return basic2d::Autofit::None;
}

} // namespace

Value Scale9Probe::encodePieces() {
	Value ret;

	auto count = _vertexes.getVertexCount() / 4;
	for (size_t i = 0; i < count; ++i) {
		auto quad = _vertexes.getQuad(i * 4, i * 6);

		// tl bl tr br - see VertexArray::Quad
		auto &tl = quad.vertexes[0];
		auto &bl = quad.vertexes[1];
		auto &br = quad.vertexes[3];

		Value piece;
		piece.setValue(
				encodeViewRect(Rect(bl.pos.x, bl.pos.y, br.pos.x - bl.pos.x, tl.pos.y - bl.pos.y)),
				"view");
		// v grows downward, so the top-left vertex carries the SMALLEST v of the quad
		piece.setValue(
				encodeTexRect(Rect(tl.tex.x, tl.tex.y, br.tex.x - tl.tex.x, br.tex.y - tl.tex.y)),
				"tex");
		ret.addValue(sp::move(piece));
	}

	return ret;
}

Value Scale9Probe::encodeState() {
	Value ret;

	auto &slice = getSlice();
	Value sliceValue;
	sliceValue.setInteger(int64_t(std::lround(slice.top * 100.0f)), "top");
	sliceValue.setInteger(int64_t(std::lround(slice.right * 100.0f)), "right");
	sliceValue.setInteger(int64_t(std::lround(slice.bottom * 100.0f)), "bottom");
	sliceValue.setInteger(int64_t(std::lround(slice.left * 100.0f)), "left");
	ret.setValue(sp::move(sliceValue), "slice");

	ret.setValue(encodeViewRect(Rect(Vec2::ZERO, getContentSize())), "content");
	ret.setValue(encodeTexRect(getTextureRect()), "textureRect");
	ret.setBool(isCenterFilled(), "fillCenter");
	ret.setString(autofitName(getTextureAutofit()), "autofit");

	if (auto tex = getTexture()) {
		auto extent = tex->getExtent();
		ret.setInteger(int64_t(extent.width), "textureWidth");
		ret.setInteger(int64_t(extent.height), "textureHeight");
		ret.setBool(tex->isLoaded(), "textureLoaded");
	}

	auto pieces = encodePieces();
	ret.setInteger(int64_t(pieces.size()), "count");
	ret.setValue(sp::move(pieces), "pieces");

	return ret;
}

bool Scale9Layout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	_full = addChild(Rc<Scale9Probe>::create());
	_full->setName("full");
	_full->setSlice(SliceTop, SliceRight, SliceBottom, SliceLeft);

	_atlas = addChild(Rc<Scale9Probe>::create());
	_atlas->setName("atlas");
	_atlas->setSlice(SliceTop, SliceRight, SliceBottom, SliceLeft);
	_atlas->setTextureRect(Rect(AtlasX, AtlasY, AtlasW, AtlasH));

	// One side at zero: the left column has no width, and a quad of no width must not be emitted
	// at all rather than be emitted empty.
	_zero = addChild(Rc<Scale9Probe>::create());
	_zero->setName("zero");
	_zero->setSlice(SliceTop, SliceRight, SliceBottom, 0.0f);

	for (auto it : {_full, _atlas, _zero}) {
		it->setAnchorPoint(Vec2(0.0f, 1.0f));
		it->setContentSize(Size2(SpriteWidth, SpriteHeight));
	}

	return true;
}

Rc<Texture> Scale9Layout::makeTexture() {
	Bytes data;
	data.resize(size_t(TextureWidth) * size_t(TextureHeight) * 4);

	for (uint32_t y = 0; y < TextureHeight; ++y) {
		for (uint32_t x = 0; x < TextureWidth; ++x) {
			const bool border = x < uint32_t(SliceLeft) || x >= TextureWidth - uint32_t(SliceRight)
					|| y < uint32_t(SliceTop) || y >= TextureHeight - uint32_t(SliceBottom);
			auto px = data.data() + (size_t(y) * TextureWidth + x) * 4;
			px[0] = border ? 0xFF : 0x21;
			px[1] = border ? 0x98 : 0x96;
			px[2] = border ? 0x00 : 0xF3;
			px[3] = 0xFF;
		}
	}

	return _director->getResourceCache()->addExternalBitmapImage(TextureName,
			core::ImageInfo(Extent2(TextureWidth, TextureHeight), core::ImageFormat::R8G8B8A8_UNORM,
					core::ImageUsage::Sampled, core::ImageHints::Opaque),
			data, TimeInterval(), TemporaryResourceFlags::CompileWhenAdded);
}

void Scale9Layout::handleEnter(Scene *scene) {
	TestLayout::handleEnter(scene);

	if (auto tex = makeTexture()) {
		for (auto it : {_full, _atlas, _zero}) { it->setTexture(Rc<Texture>(tex)); }
	}
}

void Scale9Layout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	// Position is irrelevant to every claim here - the geometry is reported in each sprite's own
	// coordinates - so this only keeps the three of them apart on screen.
	auto top = getWorkTop() - 16.0f;
	auto x = 32.0f;
	for (auto it : {_full, _atlas, _zero}) {
		if (it) {
			it->setPosition(Vec2(x, top));
			x += SpriteWidth + 24.0f;
		}
	}
}

Scale9Probe *Scale9Layout::getTarget(const Value &args) const {
	auto target = args.getString("target");
	if (target == "atlas") {
		return _atlas;
	} else if (target == "zero") {
		return _zero;
	}
	return _full;
}

void Scale9Layout::registerCommands() {
	addCommand("state", "Report every sprite: slice, content size, texture rect and the quads it "
						"actually wrote",
			[this](Value &&) {
		Value ret;
		for (auto it : {_full, _atlas, _zero}) {
			if (it) {
				ret.setValue(it->encodeState(), it->getName());
			}
		}
		return ret;
	});

	addCommand("set-slice", "Re-slice: {target, top, right, bottom, left}", [this](Value &&args) {
		const Value &a = args;
		auto sprite = getTarget(a);
		sprite->setSlice(float(a.getDouble("top")), float(a.getDouble("right")),
				float(a.getDouble("bottom")), float(a.getDouble("left")));

		Value ret;
		ret.setBool(true, "applied");
		return ret;
	});

	addCommand("set-size", "Resize a sprite: {target, width, height}", [this](Value &&args) {
		const Value &a = args;
		auto sprite = getTarget(a);
		sprite->setContentSize(Size2(float(a.getDouble("width")), float(a.getDouble("height"))));

		Value ret;
		ret.setBool(true, "applied");
		return ret;
	});

	addCommand("set-fill-center", "Draw the middle piece or not: {target, value}",
			[this](Value &&args) {
		const Value &a = args;
		getTarget(a)->setFillCenter(a.getBool("value"));

		Value ret;
		ret.setBool(true, "applied");
		return ret;
	});

	addCommand("set-texture-rect", "Point a sprite at a sub-rect of the texture (normalized): "
								   "{target, x, y, width, height}",
			[this](Value &&args) {
		const Value &a = args;
		getTarget(a)->setTextureRect(Rect(float(a.getDouble("x")), float(a.getDouble("y")),
				float(a.getDouble("width")), float(a.getDouble("height"))));

		Value ret;
		ret.setBool(true, "applied");
		return ret;
	});

	addCommand("set-autofit",
			"Ask for an autofit mode: {target, value}. A nine-slice refuses everything but `none`, "
			"and the answer reports what it kept",
			[this](Value &&args) {
		const Value &a = args;
		auto sprite = getTarget(a);
		sprite->setTextureAutofit(autofitValue(a.getString("value")));

		Value ret;
		ret.setString(autofitName(sprite->getTextureAutofit()), "autofit");
		return ret;
	});
}

} // namespace stappler::xenolith::app
