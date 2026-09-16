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

#include "css/CombinatorLayout.h"
#include "XLUiStyleResolver.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using basic2d::Label;
using basic2d::Layer;

namespace {

// stylesheet under test: one simple base rule + one rule per combinator. The base grey is
// a simple selector (fast path); each combinator rule overrides background-color when its
// ancestor/sibling structure matches.
static constexpr auto s_combinatorCss = StringView(R"css(
.swatch { background-color: #616161; }
.outer .d-hit { background-color: #e53935; }
.outer > .c-hit { background-color: #43a047; }
.a + .adj-hit { background-color: #1e88e5; }
.a ~ .g-hit { background-color: #fdd835; }
.g1 .g2 > .g3 { background-color: #8e24aa; }
)css");

// expected resolved colors
static const Color4B s_base(0x61, 0x61, 0x61, 0xff); // grey - no combinator matched
static const Color4B s_red(0xe5, 0x39, 0x35, 0xff); // descendant
static const Color4B s_green(0x43, 0xa0, 0x47, 0xff); // child
static const Color4B s_blue(0x1e, 0x88, 0xe5, 0xff); // adjacent sibling
static const Color4B s_yellow(0xfd, 0xd8, 0x35, 0xff); // general sibling
static const Color4B s_purple(0x8e, 0x24, 0xaa, 0xff); // mixed 3-compound chain

} // namespace

Layer *CombinatorLayout::makeSwatch(Node *parent, StringView cls, ZOrder z) {
	// NOTE: sibling combinators (`+`, `~`) match against the parent's child vector, which is
	// ordered by z-order. Sibling nodes here get explicit, distinct z-orders so the on-screen
	// document order is deterministic (equal z-orders would leave sibling order unspecified).
	auto swatch = parent->addChild(Rc<Layer>::create(Color::Black), z);
	swatch->addStyleClass("swatch");
	swatch->addStyleClass(cls);
	// the resolver turns the resolved background-color into the layer's on-screen color
	swatch->addSystem(Rc<ui::StyleResolver>::create());
	return swatch;
}

bool CombinatorLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	// one stylesheet scope for the whole subtree
	setStyleSheet(s_combinatorCss);

	auto makeRowLabel = [this](StringView text) {
		auto label = addChild(Rc<Label>::create(), ZOrder(1));
		label->setFontSize(18);
		label->setString(text);
		label->setColor(Color::Black);
		return label;
	};

	// --- descendant: `.outer .d-hit` -------------------------------------------
	Layer *dHit = nullptr;
	Layer *dMiss = nullptr;
	{
		auto outer = addChild(Rc<Node>::create());
		outer->addStyleClass("outer");
		auto mid = outer->addChild(Rc<Node>::create());
		dHit = makeSwatch(mid, "d-hit", ZOrder(0)); // .outer is a (grand)ancestor -> matches

		auto missRoot = addChild(Rc<Node>::create()); // no .outer ancestor
		dMiss = makeSwatch(missRoot, "d-hit", ZOrder(0)); // -> stays base grey
	}

	// --- child: `.outer > .c-hit` ----------------------------------------------
	Layer *cHit = nullptr;
	Layer *cMiss = nullptr;
	{
		auto outer = addChild(Rc<Node>::create());
		outer->addStyleClass("outer");
		cHit = makeSwatch(outer, "c-hit", ZOrder(0)); // immediate child of .outer -> matches

		auto outer2 = addChild(Rc<Node>::create());
		outer2->addStyleClass("outer");
		auto mid = outer2->addChild(Rc<Node>::create());
		cMiss = makeSwatch(mid, "c-hit", ZOrder(0)); // grandchild of .outer -> no match
	}

	// --- adjacent sibling: `.a + .adj-hit` -------------------------------------
	// (z-orders set the on-screen sibling order the matcher walks)
	Layer *adjHit = nullptr;
	Layer *adjMiss = nullptr;
	{
		auto parent = addChild(Rc<Node>::create());
		auto a = parent->addChild(Rc<Node>::create(), ZOrder(0));
		a->addStyleClass("a");
		adjHit = makeSwatch(parent, "adj-hit", ZOrder(1)); // immediately follows .a -> matches

		auto parent2 = addChild(Rc<Node>::create());
		auto a2 = parent2->addChild(Rc<Node>::create(), ZOrder(0));
		a2->addStyleClass("a");
		parent2->addChild(Rc<Node>::create(), ZOrder(1)); // gap sibling breaks adjacency
		adjMiss = makeSwatch(parent2, "adj-hit", ZOrder(2)); // -> no match
	}

	// --- general sibling: `.a ~ .g-hit` ----------------------------------------
	Layer *genHit = nullptr;
	Layer *genMiss = nullptr;
	{
		auto parent = addChild(Rc<Node>::create());
		auto a = parent->addChild(Rc<Node>::create(), ZOrder(0));
		a->addStyleClass("a");
		parent->addChild(Rc<Node>::create(), ZOrder(1)); // gap is fine for general sibling
		genHit = makeSwatch(parent, "g-hit", ZOrder(2)); // some preceding .a -> matches

		auto parent2 = addChild(Rc<Node>::create());
		genMiss = makeSwatch(parent2, "g-hit", ZOrder(0)); // swatch is FIRST...
		auto a2 = parent2->addChild(Rc<Node>::create(), ZOrder(1));
		a2->addStyleClass("a"); // ...the .a comes after -> no preceding .a -> no match
	}

	// --- mixed 3-compound chain: `.g1 .g2 > .g3` -------------------------------
	// .g3 must be a CHILD of .g2, and .g2 a DESCENDANT of .g1
	Layer *mixHit = nullptr;
	Layer *mixMiss = nullptr;
	{
		auto g1 = addChild(Rc<Node>::create());
		g1->addStyleClass("g1");
		auto g2 = g1->addChild(Rc<Node>::create());
		g2->addStyleClass("g2");
		mixHit = makeSwatch(g2, "g3", ZOrder(0)); // child of .g2, descendant of .g1 -> matches

		auto g1b = addChild(Rc<Node>::create());
		g1b->addStyleClass("g1");
		auto notG2 = g1b->addChild(Rc<Node>::create()); // NOT .g2
		mixMiss = makeSwatch(notG2, "g3", ZOrder(0)); // child-combinator step fails -> no match
	}

	_rows.emplace_back(Row{makeRowLabel("descendant  .outer .d-hit"), dHit, dMiss});
	_rows.emplace_back(Row{makeRowLabel("child       .outer > .c-hit"), cHit, cMiss});
	_rows.emplace_back(Row{makeRowLabel("adjacent    .a + .adj-hit"), adjHit, adjMiss});
	_rows.emplace_back(Row{makeRowLabel("general     .a ~ .g-hit"), genHit, genMiss});
	_rows.emplace_back(Row{makeRowLabel("chain       .g1 .g2 > .g3"), mixHit, mixMiss});

	// programmatic assertion (the ancestor chain is already complete: the swatches'
	// scope is this layout node, which they descend from)
	uint32_t checks = 0;
	uint32_t failures = 0;
	auto expect = [&](StringView name, Layer *sw, const Color4B &c) {
		++checks;
		auto st = ui::StyleResolver::resolveStyleForNode(sw);
		auto got = st.background().backgroundColor;
		if (!st.valid() || got.r != c.r || got.g != c.g || got.b != c.b) {
			++failures;
			log::source().error("CombinatorTest", name, ": expected rgb(", int(c.r), ",", int(c.g),
					",", int(c.b), ") got rgb(", int(got.r), ",", int(got.g), ",", int(got.b), ")",
					st.valid() ? "" : " (invalid)");
		}
	};

	expect("descendant hit", dHit, s_red);
	expect("descendant miss", dMiss, s_base);
	expect("child hit", cHit, s_green);
	expect("child miss", cMiss, s_base);
	expect("adjacent hit", adjHit, s_blue);
	expect("adjacent miss", adjMiss, s_base);
	expect("general hit", genHit, s_yellow);
	expect("general miss", genMiss, s_base);
	expect("chain hit", mixHit, s_purple);
	expect("chain miss", mixMiss, s_base);

	log::source().warn("CombinatorTest", "SUMMARY: ", checks, " checks, ", failures, " failures");

	return true;
}

void CombinatorLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const float swatch = 72.0f;
	const float rowH = 108.0f;
	const float top = getWorkTop() - 96.0f;
	const float labelX = 24.0f;
	const float hitX = 320.0f;
	const float missX = 420.0f;

	for (size_t i = 0; i < _rows.size(); ++i) {
		const float y = top - float(i) * rowH;
		auto &r = _rows[i];

		r.name->setAnchorPoint(Vec2(0.0f, 0.5f));
		r.name->setPosition(Vec2(labelX, y + swatch * 0.5f));

		r.hit->setAnchorPoint(Vec2(0.0f, 0.0f));
		r.hit->setContentSize(Size2(swatch, swatch));
		r.hit->setPosition(Vec2(hitX, y));

		r.miss->setAnchorPoint(Vec2(0.0f, 0.0f));
		r.miss->setContentSize(Size2(swatch, swatch));
		r.miss->setPosition(Vec2(missX, y));
	}
}

} // namespace stappler::xenolith::app
