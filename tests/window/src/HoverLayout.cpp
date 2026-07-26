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

#include "HoverLayout.h"
#include "XLUiStyleSystem.h"
#include "XLUiInteractiveComponent.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using basic2d::Label;
using basic2d::Layer;
using ui::InteractiveComponent;
using ui::InteractiveState;

namespace {

static constexpr auto s_hoverCss = StringView(R"css(
.sw { background-color: #616161; }
.sw:hover { background-color: #e53935; }
.sw:active { background-color: #1e88e5; }
.sw:checked { background-color: #43a047; }
.sw:disabled { background-color: #9c27b0; }
)css");

static const Color4B s_base(0x61, 0x61, 0x61, 0xff); // .sw
static const Color4B s_hover(0xe5, 0x39, 0x35, 0xff); // :hover
static const Color4B s_active(0x1e, 0x88, 0xe5, 0xff); // :active
static const Color4B s_checked(0x43, 0xa0, 0x47, 0xff); // :checked
static const Color4B s_disabled(0x9c, 0x27, 0xb0, 0xff); // :disabled

static Color4B resolvedColor(Layer *sw) {
	return ui::StyleResolver::resolveStyleForNode(sw).background().backgroundColor;
}

} // namespace

Layer *HoverLayout::makeSwatch(Node *parent, InteractiveState state, ui::StyleResolver **outResolver) {
	auto swatch = parent->addChild(Rc<Layer>::create(Color::Black), ZOrder(1));
	swatch->addStyleClass("sw");
	// fixed interactive state for the test (normally driven by an input listener); mutate in
	// place WITHOUT firing ComponentsDirty (return false) - StyleResolver reads it on enter
	swatch->setOrUpdateComponent<InteractiveComponent>([state](InteractiveComponent *ic) {
		ic->state = state;
		return false;
	});
	auto resolver = swatch->addSystem(Rc<ui::StyleResolver>::create());
	if (outResolver) {
		*outResolver = resolver;
	}
	return swatch;
}

bool HoverLayout::init() {
	if (!SceneLayout2d::init()) {
		return false;
	}

	addSystem(Rc<ui::StyleSystem>::create(s_hoverCss));

	auto makeRowLabel = [this](StringView text) {
		auto label = addChild(Rc<Label>::create(), ZOrder(1));
		label->setFontSize(18);
		label->setString(text);
		label->setColor(Color::Black);
		return label;
	};

	using S = InteractiveState;
	_rows.emplace_back(Row{makeRowLabel(":none/base   .sw"), makeSwatch(this, S::Enabled)});
	_rows.emplace_back(Row{makeRowLabel(":hover       .sw:hover"),
		makeSwatch(this, S::Enabled | S::Hover)});
	_rows.emplace_back(Row{makeRowLabel(":active      .sw:active"),
		makeSwatch(this, S::Enabled | S::Active)});
	_rows.emplace_back(Row{makeRowLabel(":checked     .sw:checked"),
		makeSwatch(this, S::Enabled | S::Checked)});
	_rows.emplace_back(Row{makeRowLabel(":disabled    .sw:disabled"), makeSwatch(this, S::None)});

	// runtime transition swatch: starts non-hovered (grey), flips to :hover at t=0.6s
	_transition = makeSwatch(this, S::Enabled, &_transitionResolver);
	_rows.emplace_back(Row{makeRowLabel("transition   (grey -> :hover)"), _transition});

	// static pseudo-match assertions (independent of the applier)
	uint32_t checks = 0;
	uint32_t failures = 0;
	auto expect = [&](StringView name, Layer *sw, const Color4B &c) {
		++checks;
		auto got = resolvedColor(sw);
		if (got.r != c.r || got.g != c.g || got.b != c.b) {
			++failures;
			log::source().error("HoverTest", name, ": expected rgb(", int(c.r), ",", int(c.g), ",",
					int(c.b), ") got rgb(", int(got.r), ",", int(got.g), ",", int(got.b), ")");
		}
	};
	expect("base", _rows[0].swatch, s_base);
	expect("hover", _rows[1].swatch, s_hover);
	expect("active", _rows[2].swatch, s_active);
	expect("checked", _rows[3].swatch, s_checked);
	expect("disabled", _rows[4].swatch, s_disabled);
	expect("transition-initial", _transition, s_base);
	log::source().warn("HoverTest", "STATIC: ", checks, " checks, ", failures, " failures");

	return true;
}

void HoverLayout::handleContentSizeDirty() {
	SceneLayout2d::handleContentSizeDirty();

	const auto cs = getContentSize();
	const float swatch = 64.0f;
	const float rowH = 88.0f;
	const float top = cs.height - 96.0f;

	for (size_t i = 0; i < _rows.size(); ++i) {
		const float y = top - float(i) * rowH;
		auto &r = _rows[i];
		r.name->setAnchorPoint(Vec2(0.0f, 0.5f));
		r.name->setPosition(Vec2(24.0f, y + swatch * 0.5f));
		r.swatch->setAnchorPoint(Vec2(0.0f, 0.0f));
		r.swatch->setContentSize(Size2(swatch, swatch));
		r.swatch->setPosition(Vec2(360.0f, y));
	}

	// drive the runtime transition once (guard so it only fires on the first real layout)
	if (_transition && _transitionResolver
			&& !_transition->getComponent<InteractiveComponent>()->hoverCounter) {
		// flip to hovered, then re-resolve/apply -> a real transition to the :hover color
		_transition->setOrUpdateComponent<InteractiveComponent>([](InteractiveComponent *ic) {
			ic->handleHover(+1);
			return false;
		});
		_transitionResolver->apply();

		auto got = resolvedColor(_transition);
		const bool ok = got.r == s_hover.r && got.g == s_hover.g && got.b == s_hover.b;
		log::source().warn("HoverTest", "TRANSITION: ", ok ? "PASS" : "FAIL", " (color rgb(",
				int(got.r), ",", int(got.g), ",", int(got.b), "))");
	}
}

} // namespace stappler::xenolith::app
