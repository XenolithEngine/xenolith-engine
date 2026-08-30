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

#include "widgets/ScrollBarLayout.h"
#include "XL2dLabel.h"
#include "XL2dLayer.h"
#include "XLInputListener.h"
#include "XLUiStyleResolver.h"
#include "XLUiStyleSystem.h"
#include "XLUiPanel.h"
#include "XL2dLayerRounded.h"
#include "director/XLDirector.h"
#include "XLInputDispatcher.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using basic2d::ScrollController;
using basic2d::ScrollView;

namespace {

// The bar's whole appearance, from the sheet: the check reads these values back, so a rule that
// never reached the node shows up as the engine's own default rather than as a wrong colour.
static constexpr auto s_scrollbarCss = StringView(R"css(
scroll-indicator-track {
	background-color: #00000000;
	border-radius: 5px;
}
scroll-indicator-track:hover {
	background-color: #00000040;
}
scroll-indicator {
	background-color: #b0b0b0a0;
	border-radius: 3px;
}

/* The bar became something to aim at. Thickness is written by the widget, so this is the only
   vocabulary a sheet has for that state - and an outline is the half a LayerRounded cannot draw
   at all, which is what makes it the interesting one to check. */
scroll-indicator.active {
	outline: 2px solid #ff8000ff;
}

/* "Remove the bar" as a stylesheet says it. The widget still calls setVisible(true) on the track
   whenever it lays the bar out, so this is also the check that display:none is not undone by it.

   The class is on the bar itself rather than on an ancestor because a resolver re-resolves a
   descendant when THAT node's classes change; an ancestor's class changing does not re-arm the
   subtree, so a rule scoped that way would go on matching whatever it matched at startup. A sheet
   that hides the bar for good - `tree-view scroll-indicator-track { display: none }` - is not
   affected: it matches from the first resolve. */
scroll-indicator-track.bar-hidden {
	display: none;
}
)css");

} // namespace

bool ScrollBarLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	addSystem(Rc<ui::StyleSystem>::create(s_scrollbarCss));
	addSystem(Rc<ui::StyleResolver>::create(true));

	_scroll = addChild(Rc<ScrollView>::create(ScrollView::Vertical), ZOrder(1));
	_scroll->setName("bar-scroll");
	_scroll->setAnchorPoint(Anchor::MiddleTop);

	_controller = _scroll->setController(Rc<ScrollController>::create());
	// The view's own answer to a press, beside the rows'. Two counters rather than one because a
	// press that never arrived and a press the bar swallowed read the same on the row counter
	_scroll->setTapCallback([this](int, const Vec2 &) { ++_viewTaps; });

	for (size_t i = 0; i < RowCount; ++i) {
		_controller->addItem([this, i](const ScrollController::Item &item) -> Rc<Node> {
			auto row =
					Rc<basic2d::Layer>::create((i % 2) ? Color::BlueGrey_700 : Color::BlueGrey_800);
			row->setContentSize(Size2(item.size.width, RowHeight));

			auto label = row->addChild(Rc<basic2d::Label>::create(), ZOrder(1));
			label->setFontSize(16);
			label->setString(string::toString<Interface>("row ", i));
			label->setAnchorPoint(Vec2(0.0f, 0.0f));
			label->setPosition(Vec2(8.0f, 8.0f));

			// The rows run the FULL width, under the bar. A press that reaches one is a press the
			// bar did not take, which is the only way to tell "the track swallowed it" from "the
			// track was not in the way".
			auto listener = row->addSystem(Rc<InputListener>::create());
			// One tap, explicitly: the default recognizer waits out the double-tap window before
			// it activates, and a check that reads the counter on the next frame would see the
			// press that DID arrive as a press that never came
			listener->addTapRecognizer([this](const GestureTap &) {
				++_rowTaps;
				return true;
			}, InputTapInfo{1});

			return row;
		}, RowHeight);
	}

	return true;
}

void ScrollBarLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	// A fixed viewport, not the layout's: every expected number below is derived from ViewHeight,
	// and a window resize must not move them.
	if (_scroll) {
		_scroll->setContentSize(Size2(ViewWidth, ViewHeight));
		_scroll->setPosition(Vec2(_contentSize.width / 2.0f, _contentSize.height - 40.0f));
	}
}

Value ScrollBarLayout::encodeRect(const Node *node) {
	Value ret;
	if (!node) {
		return ret;
	}
	ret.setDouble(node->getPosition().x, "x");
	ret.setDouble(node->getPosition().y, "y");
	ret.setDouble(node->getContentSize().width, "width");
	ret.setDouble(node->getContentSize().height, "height");
	ret.setDouble(node->getOpacity(), "opacity");
	ret.setBool(node->isVisible(), "visible");

	// Where to aim a synthetic pointer. Derived by the ENGINE rather than by the script summing
	// positions down the tree: the anchors along that chain are not all the same, and a script that
	// assumed they were would be testing its own arithmetic.
	const auto world = node->getWorldBoundingBox();
	Value box;
	box.setDouble(world.origin.x, "x");
	box.setDouble(world.origin.y, "y");
	box.setDouble(world.size.width, "width");
	box.setDouble(world.size.height, "height");
	ret.setValue(sp::move(box), "world");
	return ret;
}

Value ScrollBarLayout::encodePaint(const Node *node) {
	Value ret;
	if (!node) {
		return ret;
	}

	auto hex = [](const Color4B &c) {
		return string::toString<Interface>("#",
				base16::encode<Interface>(BytesView(reinterpret_cast<const uint8_t *>(&c), 4)));
	};

	// Which kind of node is answering. The whole point of the swap is that this changes, so it is
	// reported rather than inferred from the values below
	ret.setBool(dynamic_cast<const ui::Panel *>(node) != nullptr, "styled");

	if (auto style = node->getComponent<ui::PanelStyleComponent>()) {
		ret.setString(hex(style->backgroundColor), "fill");
		ret.setDouble(style->borderRadiusTopLeft, "radius");
		ret.setDouble(style->outlineWidth, "outlineWidth");
		ret.setString(hex(style->outlineColor), "outlineColor");
	} else if (auto layer = dynamic_cast<const basic2d::LayerRounded *>(node)) {
		// A LayerRounded has no outline to report, which is the claim being checked: 0 here is the
		// engine saying it cannot draw one, not a rule asking for none
		ret.setString(hex(layer->getPathColor()), "fill");
		ret.setDouble(layer->getBorderRadius(), "radius");
		ret.setDouble(0.0, "outlineWidth");
	}

	// The node's own colour, which is where the default applier puts `background-color` when the
	// node has no style component of its own - so "the sheet reached it" is answerable either way
	ret.setString(hex(Color4B(node->getColor())), "color");

	// setVisible and `display:none` are two different mechanisms and only their conjunction is what
	// the user sees; the widget writes the first one on every layout pass
	ret.setBool(node->isVisible(), "visible");
	ret.setBool(node->isEffectivelyVisible(), "effectivelyVisible");

	Value classes;
	if (auto set = node->getStyleClasses()) {
		for (auto &it : *set) { classes.addString(it); }
	}
	ret.setValue(sp::move(classes), "classes");
	return ret;
}

Value ScrollBarLayout::encodeState() const {
	Value ret;
	if (!_scroll) {
		return ret;
	}

	ret.setValue(encodeRect(_scroll->getIndicatorTrackNode()), "track");
	ret.setValue(encodeRect(_scroll->getIndicatorNode()), "thumb");

	ret.setValue(encodePaint(_scroll->getIndicatorTrackNode()), "trackPaint");
	ret.setValue(encodePaint(_scroll->getIndicatorNode()), "thumbPaint");
	ret.setBool(_styled, "styled");

	ret.setDouble(_scroll->getIndicatorThickness(), "thickness");
	ret.setDouble(_scroll->getIndicatorRelativePosition(), "relative");

	ret.setDouble(_scroll->getScrollPosition(), "scrollPosition");
	ret.setDouble(_scroll->getScrollMinPosition(), "scrollMin");
	ret.setDouble(_scroll->getScrollMaxPosition(), "scrollMax");
	ret.setDouble(_scroll->getScrollLength(), "scrollLength");
	ret.setDouble(_scroll->getScrollSize(), "scrollSize");

	// What the window says about the devices attached, which is what the bar's interactivity is a
	// function of. Reported so a failed assertion says WHICH of the two halves is wrong.
	if (auto dir = getDirector()) {
		auto state = dir->getInputDispatcher()->getWindowState();
		ret.setBool(hasFlag(state, core::WindowState::InputPointer), "hasInputPointer");
		ret.setBool(hasFlag(state, core::WindowState::Pointer), "pointerWithin");
	}

	ret.setInteger(int64_t(_rowTaps), "rowTaps");
	ret.setInteger(int64_t(_viewTaps), "viewTaps");
	ret.setInteger(int64_t(RowCount), "rowCount");
	ret.setDouble(RowHeight, "rowHeight");
	ret.setDouble(ViewHeight, "viewHeight");
	return ret;
}

void ScrollBarLayout::registerCommands() {
	addCommand("state", "Bar geometry, scroll bounds, device flags and the row tap counter",
			[this](Value &&) { return encodeState(); });

	addCommand("scroll", "Set the scroll position directly: {position} or {relative}",
			[this](Value &&args) {
		const Value &in = args;
		if (in.isDouble("relative") || in.isInteger("relative")) {
			_scroll->setIndicatorRelativePosition(float(in.getDouble("relative")));
		} else {
			_scroll->setScrollPosition(float(in.getDouble("position")));
		}
		return encodeState();
	});

	addCommand("reset-taps", "Zero the row tap counter", [this](Value &&) {
		_rowTaps = 0;
		_viewTaps = 0;
		return encodeState();
	});

	addCommand("set-styled", "Swap the bar for nodes a stylesheet can paint (one-way)",
			[this](Value &&) {
		ui::useStyledScrollIndicator(_scroll);
		_styled = true;
		return encodeState();
	});

	addCommand("set-hidden", "Add or remove the class whose rule is `display: none` on the track",
			[this](Value &&args) {
		// On the bar's own node: that is the change a resolver re-arms on. See the sheet above
		if (auto track = _scroll->getIndicatorTrackNode()) {
			if (args.getBool("value")) {
				track->addStyleClass("bar-hidden");
			} else {
				track->removeStyleClass("bar-hidden");
			}
		}
		return encodeState();
	});
}

} // namespace stappler::xenolith::app
