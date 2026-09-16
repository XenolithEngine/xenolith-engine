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

#include "text/ShapingLayout.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

bool ShapingLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	// light backdrop so the (dark) text is legible regardless of the scene background
	_background =
			addChild(Rc<basic2d::Layer>::create(Color4F(0.95f, 0.95f, 0.96f, 1.0f)), ZOrder(-1));
	_background->setAnchorPoint(Vec2(0.5f, 0.5f));

	using D = font::TextDirection;

	// (text, fontSize, shaping, bidi, base direction)
	auto add = [this](StringView text, uint16_t size, bool shape, bool bidi, D dir) {
		auto l = addChild(Rc<basic2d::Label>::create());
		l->setFontSize(size);
		l->setString(text);
		l->setAnchorPoint(Vec2(0.0f, 0.5f));
		l->setShapingEnabled(shape);
		l->setBidiEnabled(bidi);
		l->setTextDirection(dir);
		_rows.emplace_back(l);
	};

	add("plain  :  office difficult  fi fl ffi", 28, false, false, D::LeftToRight);
	add("shaped :  office difficult  fi fl ffi  (ligatures on)", 28, true, false, D::LeftToRight);
	add("arabic, shaped + bidi (joined, RTL):", 18, false, false, D::LeftToRight);
	add("مرحبا بالعالم", 30, true, true, D::RightToLeft);
	add("RTL base, mixed -- expect visual:  123 <arabic> abc", 18, false, false, D::LeftToRight);
	add("abc عربي 123", 28, true, true, D::RightToLeft);
	add("RTL no-shaping, L4 mirror -- expect:  [abc]", 16, false, false, D::LeftToRight);
	add("[abc]", 26, false, true, D::RightToLeft);
	add("multi-script shaped (Latin + Cyrillic + Greek):", 16, false, false, D::LeftToRight);
	add("Latin Кириллица Ελληνικά", 26, true, false, D::LeftToRight);
	add("unicode-bidi: bidi-override rtl on \"abc\" -- expect:  cba", 16, false, false,
			D::LeftToRight);
	// #6 needs the per-span BidiMode, so build this row explicitly instead of via the helper
	{
		auto l = addChild(Rc<basic2d::Label>::create());
		l->setFontSize(uint16_t(26));
		l->setString("abc");
		l->setAnchorPoint(Vec2(0.0f, 0.5f));
		l->setBidiEnabled(true);
		l->setShapingEnabled(true);
		l->setTextDirection(font::TextDirection::RightToLeft);
		l->setBidiMode(font::BidiMode::BidiOverride);
		_rows.emplace_back(l);
	}
	// #9: font-variant-ligatures off -- compare with the "shaped (ligatures on)" row above
	add("ligatures OFF (font-variant-ligatures: none):", 16, false, false, D::LeftToRight);
	{
		auto l = addChild(Rc<basic2d::Label>::create());
		l->setFontSize(uint16_t(28));
		l->setString("office difficult fi fl ffi");
		l->setAnchorPoint(Vec2(0.0f, 0.5f));
		l->setShapingEnabled(true);
		l->setLigaturesEnabled(false);
		_rows.emplace_back(l);
	}
	// #9: letter-spacing
	add("letter-spacing +8px (shaped):", 16, false, false, D::LeftToRight);
	{
		auto l = addChild(Rc<basic2d::Label>::create());
		l->setFontSize(uint16_t(28));
		l->setString("spacing");
		l->setAnchorPoint(Vec2(0.0f, 0.5f));
		l->setShapingEnabled(true);
		l->setLetterSpacing(8.0f);
		_rows.emplace_back(l);
	}
	// Note: #7 (1->N decomposition) is covered by the stappler shape unit test on DejaVu Sans
	// (U+06C0 -> 2 glyphs); the window's "sans" font has no glyph for such code points to show here.

	return true;
}

void ShapingLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	_background->setPosition(Vec2(_contentSize.width / 2.0f, _contentSize.height / 2.0f));
	_background->setContentSize(_contentSize);

	// The FPS/debug overlay sits in the bottom-left corner; rows that fall into its band are nudged to
	// the right of it so they stay readable in screenshots.
	const float overlayWidth = 480.0f;
	const float overlayHeight = 160.0f;
	float y = getWorkTop() - 40.0f;
	for (auto *l : _rows) {
		const float x = (y < overlayHeight) ? overlayWidth : 40.0f;
		l->setPosition(Vec2(x, y));
		y -= 42.0f;
	}
}

} // namespace stappler::xenolith::app
