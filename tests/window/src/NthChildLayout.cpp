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

#include "NthChildLayout.h"
#include "XLUiStyleResolver.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using basic2d::Layer;

namespace {

// One container per feature so every expectation has a single source. `.sw` is the grey
// baseline (a plain simple-key rule); each structural rule is more specific and overrides it.
static constexpr auto s_css = StringView(R"css(
.sw { background-color: #616161; }

.c-first > .item:first-child       { background-color: #e53935; }
.c-last  > .item:last-child        { background-color: #43a047; }
.c-odd   > .item:nth-child(odd)    { background-color: #1e88e5; }
.c-an    > .item:nth-child(3n + 1) { background-color: #8e24aa; }
.c-nlc   > .item:nth-last-child(2) { background-color: #fdd835; }
.c-only  > .item:only-child        { background-color: #00897b; }
.c-type  > layer:nth-of-type(2)    { background-color: #fb8c00; }
.c-empty > .item:empty             { background-color: #6d4c41; }

/* the structural pseudo-class counts as a class, so it outranks the plain `.item` rule */
.c-spec > .item             { background-color: #90a4ae; }
.c-spec > .item:first-child { background-color: #d81b60; }

:root { background-color: #3949ab; }

/* the dynamic containers: only the ends are coloured, so any shift is visible */
.dyn > .item:first-child   { background-color: #e53935; }
.dyn > .item:last-child    { background-color: #43a047; }
.plain > .item:first-child { background-color: #e53935; }
)css");

static const Color4B s_base(0x61, 0x61, 0x61, 0xff);
static const Color4B s_red(0xe5, 0x39, 0x35, 0xff);
static const Color4B s_green(0x43, 0xa0, 0x47, 0xff);
static const Color4B s_blue(0x1e, 0x88, 0xe5, 0xff);
static const Color4B s_purple(0x8e, 0x24, 0xaa, 0xff);
static const Color4B s_yellow(0xfd, 0xd8, 0x35, 0xff);
static const Color4B s_teal(0x00, 0x89, 0x7b, 0xff);
static const Color4B s_orange(0xfb, 0x8c, 0x00, 0xff);
static const Color4B s_brown(0x6d, 0x4c, 0x41, 0xff);
static const Color4B s_blueGrey(0x90, 0xa4, 0xae, 0xff);
static const Color4B s_pink(0xd8, 0x1b, 0x60, 0xff);
static const Color4B s_indigo(0x39, 0x49, 0xab, 0xff);

// sibling order IS z-order, so every sibling gets an explicit, distinct one
static Layer *makeItem(Node *parent, ZOrder z) {
	auto item = parent->addChild(Rc<Layer>::create(Color::Black), z);
	item->addStyleClass("sw");
	item->addStyleClass("item");
	return item;
}

} // namespace

Layer *NthChildLayout::makeList(StringView cls, uint32_t count) {
	auto list = addChild(Rc<Layer>::create(Color::Grey_200), ZOrder(1));
	list->addStyleClass(cls);
	for (uint32_t i = 0; i < count; ++i) { makeItem(list, ZOrder(int16_t(i))); }
	_staticLists.emplace_back(list);
	return list;
}

void NthChildLayout::expectResolved(StringView what, Node *node, const Color4B &c) {
	++_checks;
	auto st = ui::StyleResolver::resolveStyleForNode(node);
	auto got = st.background().backgroundColor;
	if (!st.valid() || got.r != c.r || got.g != c.g || got.b != c.b) {
		++_failures;
		log::source().error("NthChildTest", what, ": expected rgb(", int(c.r), ",", int(c.g), ",",
				int(c.b), ") got rgb(", int(got.r), ",", int(got.g), ",", int(got.b), ")",
				st.valid() ? "" : " (invalid)");
	}
}

void NthChildLayout::expectApplied(StringView what, Layer *node, const Color4B &c) {
	++_checks;
	auto got = Color4B(node->getColor());
	if (got.r != c.r || got.g != c.g || got.b != c.b) {
		++_failures;
		log::source().error("NthChildTest", what, ": expected rgb(", int(c.r), ",", int(c.g), ",",
				int(c.b), ") got rgb(", int(got.r), ",", int(got.g), ",", int(got.b), ")");
	}
}

void NthChildLayout::expectTrue(StringView what, bool value) {
	++_checks;
	if (!value) {
		++_failures;
		log::source().error("NthChildTest", what, ": expected true");
	}
}

bool NthChildLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_css);
	addSystem(Rc<ui::StyleResolver>::create(true));

	makeList("c-first", 3);
	makeList("c-last", 3);
	makeList("c-odd", 5);
	makeList("c-an", 7);
	makeList("c-nlc", 4);
	makeList("c-only", 1);
	makeList("c-only", 2); // two children - :only-child must NOT match either
	makeList("c-spec", 3);

	// mixed types: label, layer, label, layer -> `layer:nth-of-type(2)` is the LAST child
	{
		auto list = addChild(Rc<Layer>::create(Color::Grey_200), ZOrder(1));
		list->addStyleClass("c-type");
		for (uint32_t i = 0; i < 4; ++i) {
			auto item = makeItem(list, ZOrder(int16_t(i)));
			item->setType((i % 2 == 0) ? StringView("label") : StringView("layer"));
		}
		_staticLists.emplace_back(list);
	}

	// :empty - one item with no children, one holding an (invisible) child node
	{
		auto list = addChild(Rc<Layer>::create(Color::Grey_200), ZOrder(1));
		list->addStyleClass("c-empty");
		makeItem(list, ZOrder(0));
		auto occupied = makeItem(list, ZOrder(1));
		occupied->addChild(Rc<Node>::create());
		_staticLists.emplace_back(list);
	}

	// the dynamic list, and the same mutation under a plain Node parent that runs no layout
	_dynList = addChild(Rc<Layer>::create(Color::Grey_200), ZOrder(1));
	_dynList->addStyleClass("dyn");
	for (uint32_t i = 0; i < 3; ++i) { makeItem(_dynList, ZOrder(int16_t(i + 1))); }

	_plainParent = addChild(Rc<Node>::create(), ZOrder(1));
	_plainParent->addStyleClass("plain");
	makeItem(_plainParent, ZOrder(0));
	_plainFirst = makeItem(_plainParent, ZOrder(1));

	// the static half needs no frames; the dynamic half runs once the tree is styled
	runStatic();

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.6f), [this] { runDynamicInsert(); },
			Rc<DelayTime>::create(0.3f), [this] { runDynamicRemove(); },
			Rc<DelayTime>::create(0.3f), [this] { runDynamicReorder(); },
			Rc<DelayTime>::create(0.3f), [this] { report(); }));

	return true;
}

void NthChildLayout::runStatic() {
	auto item = [](Layer *list, size_t i) { return list->getChildren()[i].get(); };

	// :first-child / :last-child
	expectResolved("first-child [0]", item(_staticLists[0], 0), s_red);
	expectResolved("first-child [1]", item(_staticLists[0], 1), s_base);
	expectResolved("first-child [2]", item(_staticLists[0], 2), s_base);
	expectResolved("last-child [0]", item(_staticLists[1], 0), s_base);
	expectResolved("last-child [2]", item(_staticLists[1], 2), s_green);

	// :nth-child(odd) over five items -> 1, 3, 5
	for (size_t i = 0; i < 5; ++i) {
		expectResolved(toString("nth-child(odd) [", i, "]"), item(_staticLists[2], i),
				(i % 2 == 0) ? s_blue : s_base);
	}

	// :nth-child(3n + 1) over seven items -> 1, 4, 7
	for (size_t i = 0; i < 7; ++i) {
		expectResolved(toString("nth-child(3n+1) [", i, "]"), item(_staticLists[3], i),
				(i % 3 == 0) ? s_purple : s_base);
	}

	// :nth-last-child(2) over four items -> the third
	expectResolved("nth-last-child(2) [1]", item(_staticLists[4], 1), s_base);
	expectResolved("nth-last-child(2) [2]", item(_staticLists[4], 2), s_yellow);
	expectResolved("nth-last-child(2) [3]", item(_staticLists[4], 3), s_base);

	// :only-child
	expectResolved("only-child, alone", item(_staticLists[5], 0), s_teal);
	expectResolved("only-child, of two [0]", item(_staticLists[6], 0), s_base);
	expectResolved("only-child, of two [1]", item(_staticLists[6], 1), s_base);

	// specificity: `.item:first-child` (0,2,0) beats `.item` (0,1,0)
	expectResolved("specificity, first", item(_staticLists[7], 0), s_pink);
	expectResolved("specificity, rest", item(_staticLists[7], 1), s_blueGrey);

	// :nth-of-type counts only same-type siblings: the 2nd `layer` is the 4th child
	expectResolved("nth-of-type(2) [1]", item(_staticLists[8], 1), s_base);
	expectResolved("nth-of-type(2) [3]", item(_staticLists[8], 3), s_orange);
	expectResolved("nth-of-type(2), label [2]", item(_staticLists[8], 2), s_base);

	// :empty
	expectResolved("empty, childless", item(_staticLists[9], 0), s_brown);
	expectResolved("empty, occupied", item(_staticLists[9], 1), s_base);

	// :root is the node owning the stylesheet scope, not the scene root
	expectResolved("root, scope owner", this, s_indigo);
	expectResolved("root, a descendant", item(_staticLists[0], 0), s_red);
}

void NthChildLayout::runDynamicInsert() {
	// the applied colours before the mutation
	auto children = _dynList->getChildren();
	expectApplied("dyn initial [0]", static_cast<Layer *>(children[0].get()), s_red);
	expectApplied("dyn initial [1]", static_cast<Layer *>(children[1].get()), s_base);
	expectApplied("dyn initial [2]", static_cast<Layer *>(children[2].get()), s_green);

	// insert in FRONT of the others (sibling order is z-order): the new node becomes
	// :first-child and the old first must lose the rule
	_inserted = makeItem(_dynList, ZOrder(0));

	layoutRows();
}

void NthChildLayout::runDynamicRemove() {
	auto children = _dynList->getChildren();
	expectTrue("dyn insert: child count", children.size() == 4);
	expectApplied("dyn after insert, new first", static_cast<Layer *>(children[0].get()), s_red);
	expectApplied("dyn after insert, old first", static_cast<Layer *>(children[1].get()), s_base);
	expectApplied("dyn after insert, last", static_cast<Layer *>(children[3].get()), s_green);

	// remove it again: the old first must get :first-child back. Nothing moves and nothing
	// resizes, so this only works because the removal re-arms the siblings.
	_inserted->removeFromParent();
	_inserted = nullptr;

	// the same removal under a plain Node parent, which runs no layout at all
	_plainParent->getChildren()[0]->removeFromParent();

	layoutRows();
}

void NthChildLayout::runDynamicReorder() {
	auto children = _dynList->getChildren();
	expectTrue("dyn remove: child count", children.size() == 3);
	expectApplied("dyn after remove [0]", static_cast<Layer *>(children[0].get()), s_red);
	expectApplied("dyn after remove [1]", static_cast<Layer *>(children[1].get()), s_base);
	expectApplied("dyn after remove [2]", static_cast<Layer *>(children[2].get()), s_green);

	// a plain Node parent has no layout, but the structure nudge reaches its children anyway
	expectApplied("plain parent after remove", _plainFirst, s_red);

	// re-order: raising the last item above the rest makes it :last-child -> :first-child
	static_cast<Layer *>(children[2].get())->setLocalZOrder(ZOrder(-1));

	layoutRows();
}

void NthChildLayout::report() {
	auto children = _dynList->getChildren();
	// after the reorder the previously-last item leads the list
	expectApplied("dyn after reorder [0]", static_cast<Layer *>(children[0].get()), s_red);
	expectApplied("dyn after reorder [1]", static_cast<Layer *>(children[1].get()), s_base);
	expectApplied("dyn after reorder [2]", static_cast<Layer *>(children[2].get()), s_green);

	log::source().warn("NthChildTest", "SUMMARY: ", _checks, " checks, ", _failures, " failures");
}

void NthChildLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();
	layoutRows();
}

void NthChildLayout::layoutRows() {
	const float itemSize = 26.0f;
	const float gap = 4.0f;
	const float rowH = 38.0f;
	const float top = getWorkTop() - 24.0f;

	auto placeList = [&](Node *list, size_t row) {
		list->setAnchorPoint(Vec2(0.0f, 1.0f));
		list->setPosition(Vec2(32.0f, top - float(row) * rowH));
		list->setContentSize(Size2(float(list->getChildrenCount()) * (itemSize + gap) + gap,
				itemSize + gap * 2.0f));
		size_t i = 0;
		for (auto &child : list->getChildren()) {
			child->setAnchorPoint(Vec2(0.0f, 0.0f));
			child->setContentSize(Size2(itemSize, itemSize));
			child->setPosition(Vec2(gap + float(i) * (itemSize + gap), gap));
			++i;
		}
	};

	size_t row = 0;
	for (auto &list : _staticLists) { placeList(list, row++); }
	placeList(_dynList, row++);
	placeList(_plainParent, row++);
}

} // namespace stappler::xenolith::app
