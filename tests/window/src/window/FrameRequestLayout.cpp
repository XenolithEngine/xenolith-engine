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

#include "window/FrameRequestLayout.h"
#include "XLAction.h"
#include "XLAppThread.h"
#include "XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Lays its cells out across its own width, inside the visit that finds the width changed.
class FrameRequestRow : public Node {
public:
	static constexpr uint32_t CellCount = 5;

	virtual bool init() override {
		if (!Node::init()) {
			return false;
		}
		for (uint32_t i = 0; i < CellCount; ++i) {
			auto cell = addChild(Rc<basic2d::Layer>::create(Color::Amber_500), ZOrder(1));
			cell->setAnchorPoint(Anchor::BottomLeft);
			_cells.emplace_back(cell);
		}
		return true;
	}

	virtual void handleContentSizeDirty() override {
		Node::handleContentSizeDirty();
		const float step = _contentSize.width / float(CellCount);
		for (uint32_t i = 0; i < CellCount; ++i) {
			_cells[i]->setContentSize(Size2(step - 8.0f, _contentSize.height));
			_cells[i]->setPosition(Vec2(step * float(i), 0.0f));
		}
	}

protected:
	Vector<basic2d::Layer *> _cells;
};

// Placed in the visit by a task that runs after the frame: the task writes a position computed from
// the content size, so the first write after a resize is a change and the next one is not.
class FrameRequestEcho : public basic2d::Layer {
public:
	virtual bool init() override { return basic2d::Layer::init(Color::Purple_400); }

	virtual void handleContentSizeDirty() override {
		basic2d::Layer::handleContentSizeDirty();
		if (!_director) {
			return;
		}
		_director->getApplication()->performOnAppThread([this] {
			setPositionX(_originX + _contentSize.width);
		}, this, true);
	}

	void setOriginX(float value) { _originX = value; }

protected:
	float _originX = 0.0f;
};

bool FrameRequestLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	// TestLayout keeps every test drawing with a RenderContinuously; this one is about the window
	// drawing only what it asks for.
	stopAllActions();

	_box = addChild(Rc<basic2d::Layer>::create(Color::Teal_500), ZOrder(1));
	_box->setContentSize(Size2(120.0f, 120.0f));
	_box->setAnchorPoint(Anchor::BottomLeft);

	_label = addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	_label->setString("Frame requests");
	_label->setFontSize(28);
	_label->setColor(Color::Grey_900);
	_label->setAnchorPoint(Anchor::BottomLeft);

	_row = addChild(Rc<FrameRequestRow>::create(), ZOrder(1));
	_row->setContentSize(Size2(300.0f, 40.0f));
	_row->setAnchorPoint(Anchor::BottomLeft);

	_echo = addChild(Rc<FrameRequestEcho>::create(), ZOrder(1));
	_echo->setContentSize(Size2(60.0f, 60.0f));
	_echo->setAnchorPoint(Anchor::BottomLeft);

	return true;
}

void FrameRequestLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const float top = getWorkTop();
	_box->setPosition(Vec2(_boxMoved ? 240.0f : 40.0f, top - 180.0f));
	_label->setPosition(Vec2(40.0f, top - 240.0f));
	_row->setPosition(Vec2(40.0f, top - 320.0f));
	static_cast<FrameRequestEcho *>(_echo)->setOriginX(40.0f);
	_echo->setPositionY(top - 420.0f);
}

void FrameRequestLayout::registerCommands() {
	addCommand("move", "Move the box once, between two fixed places", [this](Value &&) {
		_boxMoved = !_boxMoved;
		_box->setPositionX(_boxMoved ? 240.0f : 40.0f);
		Value ret;
		ret.setBool(_boxMoved, "moved");
		return ret;
	}, true);

	addCommand("same", "Set the box to the place it already has", [this](Value &&) {
		_box->setPosition(_box->getPosition());
		_box->setContentSize(_box->getContentSize());
		return Value(true);
	}, true);

	addCommand("text", "Set the label: { text }", [this](Value &&args) {
		const Value &req = args;
		_label->setString(req.getString("text"));
		return Value(true);
	}, true);

	addCommand("resize", "Switch the row between two widths; its cells follow in the visit",
			[this](Value &&) {
		_rowWide = !_rowWide;
		_row->setContentSize(Size2(_rowWide ? 500.0f : 300.0f, 40.0f));
		Value ret;
		ret.setBool(_rowWide, "wide");
		return ret;
	}, true);

	addCommand("echo", "Resize the node that places itself again after the frame",
			[this](Value &&) {
		_echoWide = !_echoWide;
		_echo->setContentSize(Size2(_echoWide ? 120.0f : 60.0f, 60.0f));
		Value ret;
		ret.setBool(_echoWide, "wide");
		return ret;
	}, true);

	addCommand("animate", "Move the box there and back: { seconds }", [this](Value &&args) {
		const Value &req = args;
		const float seconds = float(req.getDouble("seconds", 0.5));
		const auto pos = _box->getPosition();
		_box->stopAllActions();
		_box->runAction(Rc<Sequence>::create(Rc<MoveTo>::create(seconds / 2.0f,
															 Vec2(pos.x + 200.0f, pos.y)),
				Rc<MoveTo>::create(seconds / 2.0f, Vec2(pos.x, pos.y))));
		return Value(true);
	}, true);
}

} // namespace stappler::xenolith::app
