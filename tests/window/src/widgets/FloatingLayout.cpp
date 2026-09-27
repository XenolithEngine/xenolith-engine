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

#include "widgets/FloatingLayout.h"
#include "XL2dLayer.h"
#include "XLUiButton.h"
#include "XLUiPanel.h"
#include "XLUiTooltipSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

static Node *FloatingLayout_addLayer(Node *parent, StringView name, ZOrder z, const Color4F &color) {
	auto node = parent->addChild(Rc<basic2d::Layer>::create(color), z);
	node->setName(name);
	node->setAnchorPoint(Anchor::BottomLeft);
	return node;
}

} // namespace

bool FloatingLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	_below = FloatingLayout_addLayer(this, "below", ZOrder(1), Color::BlueGrey_800);
	auto belowListener = _below->addSystem(Rc<InputListener>::create());
	belowListener->addTapRecognizer([this](const GestureTap &tap) {
		if (tap.event == GestureEvent::Activated) {
			++_belowTaps;
		}
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::MouseLeft}), 1});
	belowListener->addScrollRecognizer([this](const GestureScroll &) {
		++_belowScrolls;
		return true;
	});
	belowListener->addMouseOverRecognizer([this](const GestureData &data) {
		_belowHovered = data.event == GestureEvent::Began || data.event == GestureEvent::Activated;
		return true;
	});
	ui::setTooltip(_below, "below hint");

	_plate = addChild(Rc<ui::Panel>::create(), ZOrder(2));
	_plate->setName("plate");
	_plate->setAnchorPoint(Anchor::BottomLeft);
	_plate->setPosition(Vec2(InitialFrame.origin.x - 100.0f, InitialFrame.origin.y - 100.0f));
	_plate->setContentSize(Size2(240.0f, 200.0f));

	_window = FloatingLayout_addLayer(this, "window", ZOrder(3), Color::Red_500);
	_floating = _window->addSystem(Rc<ui::FloatingSystem>::create());
	_floating->setMinSize(Size2(160.0f, 100.0f));
	_floating->setFrameChangedCallback([this](const Rect &) { ++_frameChanges; });

	_header = FloatingLayout_addLayer(_window, "header", ZOrder(2), Color::Indigo_500);
	_headerButton = _header->addChild(Rc<ui::Button>::create("B", [this] { ++_headerTaps; }),
			ZOrder(1));
	_headerButton->setName("header-button");
	_headerButton->setAnchorPoint(Anchor::BottomLeft);
	_floating->setHeader(_header);

	_inside = FloatingLayout_addLayer(_window, "inside", ZOrder(1), Color::Amber_600);
	auto insideListener = _inside->addSystem(Rc<InputListener>::create());
	insideListener->addTapRecognizer([this](const GestureTap &tap) {
		if (tap.event == GestureEvent::Activated) {
			++_insideTaps;
		}
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::MouseLeft}), 1});
	insideListener->addScrollRecognizer([this](const GestureScroll &) {
		++_insideScrolls;
		return true;
	});

	// The content follows the window's size, as an application's own layout would
	_window->setContentSizeDirtyCallback([this] { placeWindowContent(); });

	_floating->setFrame(InitialFrame);
	return true;
}

void FloatingLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	if (_below) {
		_below->setPosition(Vec2::ZERO);
		_below->setContentSize(Size2(_contentSize.width, getWorkTop()));
	}
}

void FloatingLayout::placeWindowContent() {
	if (!_window) {
		return;
	}
	const auto size = _window->getContentSize();
	_header->setPosition(Vec2(0.0f, size.height - HeaderHeight));
	_header->setContentSize(Size2(size.width, HeaderHeight));
	_headerButton->setPosition(Vec2(4.0f, 2.0f));
	_headerButton->setContentSize(Size2(40.0f, HeaderHeight - 4.0f));
	_inside->setPosition(Vec2(8.0f, 8.0f));
	_inside->setContentSize(Size2(sprt::max(size.width - 16.0f, 0.0f),
			sprt::max(size.height - HeaderHeight - 16.0f, 0.0f)));
}

Value FloatingLayout::encodeState() const {
	Value ret;
	auto frame = _floating ? _floating->getFrame() : Rect();
	Value f;
	f.setDouble(frame.origin.x, "x");
	f.setDouble(frame.origin.y, "y");
	f.setDouble(frame.size.width, "width");
	f.setDouble(frame.size.height, "height");
	ret.setValue(sp::move(f), "frame");
	ret.setBool(_floating && _floating->isDragging(), "dragging");
	ret.setBool(_window && _window->isOverlay(), "overlay");

	ret.setInteger(_belowTaps, "belowTaps");
	ret.setInteger(_belowScrolls, "belowScrolls");
	ret.setBool(_belowHovered, "belowHovered");
	ret.setInteger(_insideTaps, "insideTaps");
	ret.setInteger(_insideScrolls, "insideScrolls");
	ret.setInteger(_headerTaps, "headerTaps");
	ret.setInteger(_frameChanges, "frameChanges");
	ret.setDouble(getContentSize().width, "areaWidth");
	ret.setDouble(getContentSize().height, "areaHeight");

	if (auto tips = ui::TooltipSystem::findForNode(const_cast<FloatingLayout *>(this))) {
		auto hovered = tips->getHoveredTarget();
		ret.setString(hovered ? hovered->getName() : StringView(), "tipHovered");
	}

	Value rects;
	auto put = [&](Node *node) {
		if (!node) {
			return;
		}
		Value item;
		const auto world = node->getWorldBoundingBox();
		item.setDouble(world.origin.x, "x");
		item.setDouble(world.origin.y, "y");
		item.setDouble(world.size.width, "width");
		item.setDouble(world.size.height, "height");
		rects.setValue(sp::move(item), node->getName());
	};
	for (auto node : {_below, _plate, _window, _header, _headerButton, _inside}) { put(node); }
	if (_window) {
		for (auto &it : _window->getChildren()) {
			if (it->getName().starts_with("floating-resize-")) {
				put(it);
			}
		}
	}
	ret.setValue(sp::move(rects), "rects");
	return ret;
}

void FloatingLayout::registerCommands() {
	TestLayout::registerCommands();

	addCommand("state", "The window's frame, what reached the content and the background",
			[this](Value &&) { return encodeState(); });

	addCommand("set-frame", "Put the window at {x, y, width, height}", [this](Value &&args) {
		if (_floating) {
			_floating->setFrame(Rect(float(args.getDouble("x")), float(args.getDouble("y")),
					float(args.getDouble("width")), float(args.getDouble("height"))));
		}
		return encodeState();
	});

	addCommand("reset", "Zero the counters", [this](Value &&) {
		_belowTaps = _belowScrolls = _insideTaps = _insideScrolls = _headerTaps = 0;
		_frameChanges = 0;
		return encodeState();
	});
}

} // namespace stappler::xenolith::app
