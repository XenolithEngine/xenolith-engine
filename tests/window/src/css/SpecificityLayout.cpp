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

#include "css/SpecificityLayout.h"
#include "XLUiStyleResolver.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using basic2d::Label;
using basic2d::Layer;

namespace {

// green wins on specificity, red/blue are the lower-specificity losers (except source-order
// case where red is the later of two equal-specificity rules)
static constexpr auto s_css = StringView(R"css(
.wrap .item { background-color: #e53935; }
#special     { background-color: #43a047; }

.box         { background-color: #1e88e5; }
layer.box    { background-color: #43a047; }

layer        { background-color: #1e88e5; }
.hi          { background-color: #43a047; }

.a           { background-color: #1e88e5; }
.b           { background-color: #43a047; }

.multi           { background-color: #1e88e5; }
.multi.sel       { background-color: #43a047; }
.multi.sel.deep  { background-color: #fdd835; }
)css");

static const Color4B s_win(0x43, 0xa0, 0x47, 0xff); // green - the specificity/order winner
static const Color4B s_lose(0x1e, 0x88, 0xe5, 0xff); // blue - what a non-matching compound leaves
static const Color4B s_deep(0xfd, 0xd8, 0x35, 0xff); // yellow - the three-token compound

static Color4B resolvedColor(Layer *sw) {
	return ui::StyleResolver::resolveStyleForNode(sw).background().backgroundColor;
}

} // namespace

bool SpecificityLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_css);

	auto makeRowLabel = [this](StringView text) {
		auto label = addChild(Rc<Label>::create(), ZOrder(1));
		label->setFontSize(16);
		label->setString(text);
		label->setColor(Color::Black);
		return label;
	};
	auto makeSwatch = [](Node *parent) {
		auto sw = parent->addChild(Rc<Layer>::create(Color::Black), ZOrder(1));
		sw->addSystem(Rc<ui::StyleResolver>::create());
		return sw;
	};

	// 1. #id (1,0,0) must beat the lower-specificity descendant rule `.wrap .item` (0,2,0).
	//    The OLD fixed order applied complex rules last and got this WRONG (red).
	Layer *sw1 = nullptr;
	{
		auto wrap = addChild(Rc<Node>::create());
		wrap->addStyleClass("wrap");
		auto mid = wrap->addChild(Rc<Node>::create());
		sw1 = makeSwatch(mid);
		sw1->addStyleClass("item");
		sw1->setName("special");
	}

	// 2. `layer.box` (0,1,1) beats `.box` (0,1,0)
	Layer *sw2 = makeSwatch(this);
	sw2->setType("layer");
	sw2->addStyleClass("box");

	// 3. `.hi` (0,1,0) beats bare `layer` (0,0,1)
	Layer *sw3 = makeSwatch(this);
	sw3->setType("layer");
	sw3->addStyleClass("hi");

	// 4. equal specificity `.a`/`.b` (0,1,0) - later source order (`.b`) wins
	Layer *sw4 = makeSwatch(this);
	sw4->addStyleClass("a");
	sw4->addStyleClass("b");

	// 5-7. A compound with more than one class token has no simple string key, so it used to be
	//      indexed under a key nothing ever looked up and silently never matched.
	Layer *sw5 = makeSwatch(this);
	sw5->addStyleClass("multi");
	sw5->addStyleClass("sel");

	Layer *sw6 = makeSwatch(this);
	sw6->addStyleClass("multi");
	sw6->addStyleClass("sel");
	sw6->addStyleClass("deep");

	Layer *sw7 = makeSwatch(this);
	sw7->addStyleClass("multi");

	_rows.emplace_back(Row{makeRowLabel("#id > .wrap .item   (id beats descendant)"), sw1});
	_rows.emplace_back(Row{makeRowLabel("layer.box > .box    (tag+class beats class)"), sw2});
	_rows.emplace_back(Row{makeRowLabel(".hi > layer         (class beats tag)"), sw3});
	_rows.emplace_back(Row{makeRowLabel(".b after .a         (source order tie-break)"), sw4});
	_rows.emplace_back(Row{makeRowLabel(".multi.sel > .multi (two-class compound)"), sw5});
	_rows.emplace_back(Row{makeRowLabel(".multi.sel.deep     (three-class compound)"), sw6});
	_rows.emplace_back(
			Row{makeRowLabel(".multi alone        (compound must not over-match)"), sw7});

	uint32_t checks = 0;
	uint32_t failures = 0;
	auto expectColor = [&](StringView name, Layer *sw, Color4B want) {
		++checks;
		auto got = resolvedColor(sw);
		if (got.r != want.r || got.g != want.g || got.b != want.b) {
			++failures;
			log::source().error("SpecificityTest", name, ": expected rgb(", int(want.r), ",",
					int(want.g), ",", int(want.b), ") got rgb(", int(got.r), ",", int(got.g), ",",
					int(got.b), ")");
		}
	};
	auto expect = [&](StringView name, Layer *sw) { expectColor(name, sw, s_win); };
	expect("id-beats-descendant", sw1);
	expect("tagclass-beats-class", sw2);
	expect("class-beats-tag", sw3);
	expect("source-order-tie", sw4);
	expect("two-class-compound", sw5);
	expectColor("three-class-compound", sw6, s_deep);
	expectColor("compound-no-over-match", sw7, s_lose);
	log::source().warn("SpecificityTest", "SUMMARY: ", checks, " checks, ", failures, " failures");

	return true;
}

void SpecificityLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const auto cs = getContentSize();
	const float swatch = 52.0f;
	const float rowH = 72.0f;
	const float top = getWorkTop() - 96.0f;

	for (size_t i = 0; i < _rows.size(); ++i) {
		const float y = top - float(i) * rowH;
		auto &r = _rows[i];
		r.name->setAnchorPoint(Vec2(0.0f, 0.5f));
		r.name->setPosition(Vec2(24.0f, y + swatch * 0.5f));
		r.swatch->setAnchorPoint(Vec2(0.0f, 0.0f));
		r.swatch->setContentSize(Size2(swatch, swatch));
		r.swatch->setPosition(Vec2(470.0f, y));
	}
}

} // namespace stappler::xenolith::app
