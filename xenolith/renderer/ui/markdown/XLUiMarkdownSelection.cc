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

#include "XLUiMarkdownSelection.h"
#include "XLUiMarkdownView.h"
#include "XLDynamicStateSystem.h"
#include "XLUiLayoutSystem.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

static constexpr auto s_autoScrollTag = "MarkdownAutoScroll"_tag;

bool MarkdownSelectionSystem::init(NotNull<MarkdownView> view) {
	if (!InputListener::init(0)) {
		return false;
	}

	_view = view;
	setSystemPriority(MarkdownSelectionPriority);

	// The document is text, so the pointer says so; the link case flips it per move.
	setCursor(WindowCursor::Text);

	addTapRecognizer([this](const GestureTap &tap) { return handleTap(tap); },
			InputTapInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}), 3,
				InputTapFlags::Immediate});

	addPressRecognizer([this](const GesturePress &press) { return handlePress(press); },
			InputPressInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft})});

	addSwipeRecognizer([this](const GestureSwipe &swipe) { return handleSwipe(swipe); },
			InputSwipeInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}),
				SelectionThreshold, true});

	addMoveRecognizer([this](const GestureData &data) { return handleMove(data); });

	auto &hotkeys = EngineHotkeys::get();
	auto bind = [this](HotkeyId id) {
		addHotkey(id, [this](HotkeyId id, const InputEvent &ev) {
			return handleSelectionHotkey(id, ev);
		}, HotkeyFlags::FocusedOnly);
	};

	bind(hotkeys.textSelectAll);
	bind(hotkeys.textCopy);
	bind(hotkeys.back);

	// Copying the source instead of the normalized fragment has no engine-wide binding.
	_copySourceHotkey = HotkeyRegistry::getInstance()->add("xenolith.ui.markdown.copySource",
			HotkeyCombo::parse("Ctrl+Shift+C"),
			"Copy the selected markdown as a raw slice of the source");
	bind(_copySourceHotkey);

	return true;
}

/* The handles a finger drags the selection by, built on demand. Each ignores its parent's
DynamicStateSystem state, so the document's scroll does not clip it at the view's edge. */
void MarkdownSelectionSystem::makeHandles() {
	if (_handleStart || !_view) {
		return;
	}

	auto make = [&](bool start) {
		auto handle = _view->addChild(Rc<basic2d::VectorSprite>::create(Size2(48, 48)), ZOrder(10));
		handle->addPath(vg::VectorPath().openForWriting([&](vg::PathWriter &writer) {
			if (start) {
				writer.moveTo(48, 48)
						.lineTo(24, 48)
						.arcTo(24, 24, 0, true, true, 48, 24)
						.closePath();
			} else {
				writer.moveTo(0, 48).lineTo(0, 24).arcTo(24, 24, 0, true, true, 24, 48).closePath();
			}
		}));
		handle->setContentSize(Size2(24.0f, 24.0f));
		handle->setAnchorPoint(start ? Vec2(1.0f, 1.0f) : Vec2(0.0f, 1.0f));
		handle->setColor(Color::Blue_500);
		handle->setOpacity(192);
		handle->setVisible(false);

		auto state = handle->addSystem(Rc<DynamicStateSystem>::create());
		state->setIgnoreParentState(true);
		state->setStateApplyMode(DynamicStateApplyMode::ApplyForAll);

		// Placed by hand, so out of the view's flex flow.
		handle->setComponent<OutOfFlowComponent>();
		return handle;
	};

	_handleStart = make(true);
	_handleEnd = make(false);
}

void MarkdownSelectionSystem::updateHandles() {
	auto range = _view->getSelectionRange();
	auto visible = _touchMode && range.second > range.first;

	if (!_handleStart) {
		if (!visible) {
			return;
		}
		makeHandles();
	}

	_handleStart->setVisible(visible);
	_handleEnd->setVisible(visible);
	if (!visible) {
		return;
	}

	auto flow = _view->getFlow();
	auto begin = flow->getPointForPosition(range.first);
	auto end = flow->getPointForPosition(range.second);

	if (begin.first.isValid()) {
		_handleStart->setPosition(_view->convertToNodeSpace(begin.first));
	}
	if (end.first.isValid()) {
		_handleEnd->setPosition(_view->convertToNodeSpace(end.first));
	}
}

Vec2 MarkdownSelectionSystem::getHandlePosition(bool start) const {
	auto handle = start ? _handleStart : _handleEnd;
	if (!handle || !handle->isVisible()) {
		return Vec2::INVALID;
	}

	// The anchor, not the origin: the teardrop's point, on the caret.
	return _view->convertToWorldSpace(handle->getPosition().xy());
}

void MarkdownSelectionSystem::extendTo(uint32_t position) {
	_view->setSelectionRange(sprt::min(_anchor, position), sprt::max(_anchor, position));
}

bool MarkdownSelectionSystem::handleTap(const GestureTap &tap) {
	auto flow = _view->getFlow();
	if (!flow || flow->getLength() == 0) {
		return false;
	}

	// The keyboard follows the click: Ctrl+C belongs to the document that was last touched.
	setFocused();

	auto position = flow->getPositionForPoint(tap.pos);
	auto shift = hasFlag(tap.input->data.getModifiers(), InputModifier::Shift);

	if (tap.count >= 3) {
		auto block = flow->getBlockRange(position);
		_anchor = block.first;
		_view->setSelectionRange(block.first, block.second);
		return true;
	}

	if (tap.count == 2) {
		auto word = flow->getWordRange(position);
		_anchor = word.first;
		_view->setSelectionRange(word.first, word.second);
		return true;
	}

	if (shift) {
		extendTo(position);
		return true;
	}

	// A tap on a link follows it; a drag starting on a link selects instead.
	if (auto link = flow->findLink(position)) {
		_view->clearSelection();
		_view->handleLinkActivated(*link);
		return true;
	}

	_touchMode = false;
	_anchor = position;
	_view->setSelectionRange(position, position);
	return true;
}

bool MarkdownSelectionSystem::handlePress(const GesturePress &press) {
	if (press.event != GestureEvent::Activated) {
		return true;
	}

	// A long press is how a finger starts a selection; a mouse has the drag for that.
	if (!hasFlag(press.input->data.getModifiers(), InputModifier::Touch)) {
		return true;
	}

	auto flow = _view->getFlow();
	if (!flow || flow->getLength() == 0) {
		return true;
	}

	auto word = flow->getWordRange(flow->getPositionForPoint(press.pos));
	_touchMode = true;
	_anchor = word.first;
	_view->setSelectionRange(word.first, word.second);
	setExclusiveForTouch(press.getId());
	return true;
}

bool MarkdownSelectionSystem::handleSwipe(const GestureSwipe &swipe) {
	auto flow = _view->getFlow();
	if (!flow || flow->getLength() == 0) {
		return false;
	}

	switch (swipe.event) {
	case GestureEvent::Began:
		if (_touchMode && _handleStart && _handleStart->isVisible()) {
			if (_handleStart->isTouched(swipe.firstTouch, HandlePadding)) {
				_handleDrag = 1;
			} else if (_handleEnd->isTouched(swipe.firstTouch, HandlePadding)) {
				_handleDrag = 2;
			}
		}

		if (_handleDrag != 0) {
			// dragging one edge: the anchor is the other one
			auto range = _view->getSelectionRange();
			_anchor = (_handleDrag == 1) ? range.second : range.first;
		} else {
			// A finger off the handles pans the document: decline so the scroll keeps the gesture.
			// The tap recognizer still tracks the pointer.
			if (hasFlag(swipe.input->data.getModifiers(), InputModifier::Touch)) {
				return false;
			}
			_touchMode = false;
			_anchor = flow->getPositionForPoint(swipe.firstTouch);
			_view->setSelectionRange(_anchor, _anchor);
		}

		_dragging = true;

		// Take the pointer before either scroll acts; per touch, so a second finger stays free.
		setExclusiveForTouch(swipe.getId());
		return true;

	case GestureEvent::Activated:
		extendTo(flow->getPositionForPoint(swipe.secondTouch));
		setAutoScrollTarget(swipe.secondTouch);
		return true;

	case GestureEvent::Ended:
	case GestureEvent::Cancelled:
		_dragging = false;
		_handleDrag = 0;
		setAutoScrollTarget(Vec2::INVALID);
		return true;
	}
	return true;
}

bool MarkdownSelectionSystem::handleMove(const GestureData &data) {
	auto flow = _view->getFlow();
	if (!flow || flow->getLength() == 0 || _dragging) {
		return false;
	}

	// Inline links are style ranges, not nodes, so there is no `:hover`; the cursor shows them.
	auto position = flow->getPositionForPoint(data.location());
	setCursor(flow->findLink(position) ? WindowCursor::Pointer : WindowCursor::Text);
	return false; // a hover is nobody's to consume
}

bool MarkdownSelectionSystem::handleSelectionHotkey(HotkeyId id, const InputEvent &) {
	auto &hotkeys = EngineHotkeys::get();

	if (id == hotkeys.textSelectAll) {
		_view->selectAll();
		return true;
	}
	if (id == hotkeys.textCopy) {
		return _view->copy();
	}
	if (id == _copySourceHotkey) {
		return _view->copy(document::MarkdownMarkup::Raw);
	}
	if (id == hotkeys.back) {
		if (!_view->hasSelection()) {
			return false; // nothing of ours to dismiss; let the scene have its Escape
		}
		_view->clearSelection();
		return true;
	}
	return false;
}

void MarkdownSelectionSystem::setAutoScrollTarget(Vec2 world) {
	if (_autoScrollTarget == world || (!_autoScrollTarget.isValid() && !world.isValid())) {
		return;
	}

	_autoScrollTarget = world;

	// Scheduling alone is not enough: a pointer parked at the edge produces no frames.
	if (!_owner) {
		return;
	}
	if (!_autoScrollTarget.isValid()) {
		_owner->stopAllActionsByTag(s_autoScrollTag);
	} else if (!_owner->getActionByTag(s_autoScrollTag)) {
		_owner->runAction(Rc<RenderContinuously>::create(), s_autoScrollTag);
	}
}

void MarkdownSelectionSystem::update(const UpdateTime &time) {
	InputListener::update(time);

	if (!_autoScrollTarget.isValid() || !_owner || !_view) {
		return;
	}

	auto scroll = _view->getScrollSystem();
	if (!scroll) {
		return;
	}

	auto local = _owner->convertToNodeSpace(_autoScrollTarget);
	auto size = _owner->getContentSize();
	auto edge = sprt::min(AutoScrollEdge, size.height / 3.0f);
	if (edge <= 0.0f) {
		return;
	}

	// Node space is Y-up and the scroll offset Y-down: a small local.y scrolls further down.
	auto delta = 0.0f;
	if (local.y < edge) {
		delta = 1.0f - math::clamp(local.y / edge, 0.0f, 1.0f);
	} else if (local.y > size.height - edge) {
		delta = -(1.0f - math::clamp((size.height - local.y) / edge, 0.0f, 1.0f));
	}

	if (delta != 0.0f) {
		scroll->scrollBy(Vec2(0.0f, delta * AutoScrollSpeed * time.dt));

		// The document moved under a still pointer, so re-read the selection at the same point.
		extendTo(_view->getFlow()->getPositionForPoint(_autoScrollTarget));
	}
}

} // namespace stappler::xenolith::ui
