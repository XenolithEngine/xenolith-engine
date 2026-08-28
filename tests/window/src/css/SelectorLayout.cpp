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

#include "css/SelectorLayout.h"

#include "XLUiStyleResolver.h"
#include "XLInteractiveComponent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using basic2d::Layer;

namespace {

/* Every pair here is written to CONFLICT: the interesting half of a functional pseudo-class is not
whether it matches, it is what it counts for when two rules want the same property.

Order matters in three places and is load-bearing, not incidental:
  * `.wa` comes AFTER `.wa:where(.wb)`, so it wins only if `:where()` really counts for nothing;
  * `.sp.sp2` (two classes) is written after `.sp:is(#spec, .zzz)` (a class and an id), so the
    `:is()` rule wins only by specificity, never by position;
  * each `.badN` fallback follows the malformed rule it is paired with, so a parser that swallowed
    the rest of the sheet after a refusal would take the fallback with it. */
static constexpr auto s_selectorCss = StringView(R"css(
.base { background-color: #616161; color: #101010; }

.n1:not(.x) { background-color: #43a047; }
.n2:not(swatch) { background-color: #1e88e5; }
.n3:not(#hit) { background-color: #8e24aa; }
.n4:not(:hover) { background-color: #fb8c00; }
.n5:not(.x, .y) { background-color: #7cb342; }

.i1:is(.a, .b) { background-color: #00897b; }
.i2:where(.a, .b) { background-color: #5e35b1; }

.sp:is(#spec, .zzz) { color: #ff0000; }
.sp.sp2 { color: #00ff00; }

.wa:where(.wb) { background-color: #d81b60; }
.wa { background-color: #c0ca33; }

.bad1:is(.a .b) { background-color: #000001; }
.bad1 { background-color: #00acc1; }

.bad2:is(:not(.a)) { background-color: #000002; }
.bad2 { background-color: #00acc1; }

.bad3:is() { background-color: #000003; }
.bad3 { background-color: #00acc1; }

.bad4:is(:first-child) { background-color: #000004; }
.bad4 { background-color: #00acc1; }

.bad5 { background-color: #00acc1; }

/* Last on purpose: an unbalanced argument is the one refusal whose recovery could eat what
   follows it, so nothing follows it. */
.bad5:not(.a { background-color: #000005; }
)css");

static int64_t encodeColor(const Color4B &c) {
	return (int64_t(c.r) << 16) | (int64_t(c.g) << 8) | int64_t(c.b);
}

} // namespace

Layer *SelectorLayout::addSample(StringView name, StringView type,
		sprt::initializer_list<StringView> classes) {
	auto node = addChild(Rc<Layer>::create(Color::Black), ZOrder(int16_t(_samples.size() + 1)));
	node->setName(name);
	if (!type.empty()) {
		node->setType(type);
	}
	node->addStyleClass("base");
	for (auto &cl : classes) { node->addStyleClass(cl); }
	node->addSystem(Rc<ui::StyleResolver>::create());
	_samples.emplace_back(Sample{name.str<Interface>(), node});
	return node;
}

Value SelectorLayout::encodeSample(const Sample &sample) const {
	Value ret;
	auto style = ui::StyleResolver::resolveStyleForNode(sample.node);
	ret.setInteger(encodeColor(style.background().backgroundColor), "background");
	auto c = style.color();
	ret.setInteger(encodeColor(Color4B(c.r, c.g, c.b, 255)), "color");
	return ret;
}

bool SelectorLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_selectorCss);

	// `:not(.class)` - and the same node with the class, which is the only way to see that the
	// negation is a test rather than a no-op
	addSample("not-class-off", StringView(), {"n1"});
	addSample("not-class-on", StringView(), {"n1", "x"});

	// `:not(tag)` and `:not(#id)`: the two halves of a compound that are not classes
	addSample("not-tag-off", StringView("layer"), {"n2"});
	addSample("not-tag-on", StringView("swatch"), {"n2"});
	addSample("not-id-off", StringView(), {"n3"});
	addSample("hit", StringView(), {"n3"}); // its NAME is the css #id the rule excludes

	// `:not(:hover)` folds into the forbid mask the matcher already had, so this asserts that a
	// state and a class are negated the same way
	addSample("not-state-off", StringView(), {"n4"});
	auto hovered = addSample("not-state-on", StringView(), {"n4"});
	hovered->setOrUpdateComponent<InteractiveComponent>([](InteractiveComponent *ic) {
		ic->state = InteractiveState::Enabled | InteractiveState::Hover;
		return false;
	});

	// `:not(a, b)` is "neither"
	addSample("not-list-none", StringView(), {"n5"});
	addSample("not-list-one", StringView(), {"n5", "y"});

	// `:is()` matches any option and nothing else; `:where()` matches identically
	addSample("is-a", StringView(), {"i1", "a"});
	addSample("is-b", StringView(), {"i1", "b"});
	addSample("is-c", StringView(), {"i1", "c"});
	addSample("where-a", StringView(), {"i2", "a"});
	addSample("where-c", StringView(), {"i2", "c"});

	// specificity: `:is(#spec, .zzz)` is worth an id, and beats two classes
	auto spec = addSample("spec", StringView(), {"sp", "sp2"});
	spec->setName("spec"); // the #id the :is() argument names

	// specificity: `:where()` is worth nothing, so the LATER plain rule wins
	addSample("where-both", StringView(), {"wa", "wb"});
	addSample("where-one", StringView(), {"wa"});

	// refusals: the malformed rule must not apply, and its neighbour must survive
	addSample("bad1", StringView(), {"bad1"});
	addSample("bad2", StringView(), {"bad2"});
	addSample("bad3", StringView(), {"bad3"});
	addSample("bad4", StringView(), {"bad4"});
	addSample("bad5", StringView(), {"bad5"});

	return true;
}

void SelectorLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	// Nothing here is asserted by looking at it; the grid only makes the stand readable by hand.
	const float size = 40.0f;
	const float step = 48.0f;
	float x = 32.0f;
	float y = getWorkTop() - 16.0f;
	for (auto &it : _samples) {
		it.node->setAnchorPoint(Vec2(0.0f, 1.0f));
		it.node->setContentSize(Size2(size, size));
		it.node->setPosition(Vec2(x, y));
		x += step;
		if (x > 900.0f) {
			x = 32.0f;
			y -= step;
		}
	}
}

Node *SelectorLayout::getTarget(const Value &args) const {
	auto name = args.getString("target");
	for (auto &it : _samples) {
		if (it.name == name) {
			return it.node;
		}
	}
	return nullptr;
}

void SelectorLayout::registerCommands() {
	addCommand("state", "Every sample and the style that resolved for it", [this](Value &&) {
		Value ret;
		for (auto &it : _samples) { ret.setValue(encodeSample(it), it.name); }
		return ret;
	});

	addCommand("set-class", "Add or remove a class at runtime: {target, class, value}",
			[this](Value &&args) {
		const Value &a = args;
		Value ret;
		if (auto node = getTarget(a)) {
			if (a.getBool("value")) {
				node->addStyleClass(a.getString("class"));
			} else {
				node->removeStyleClass(a.getString("class"));
			}
			ret.setBool(true, "applied");
		}
		return ret;
	});
}

} // namespace stappler::xenolith::app
