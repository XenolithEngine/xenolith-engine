/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "MaterialInputTextContainer.h"
#include "MaterialSurfaceInterior.h"
#include "MaterialStyleContainer.h"
#include "MaterialEasing.h"
#include "XLFrameContext.h"
#include "XL2dLayer.h"
#include "XLAction.h"
#include "XL2dVectorSprite.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

InputTextContainer::~InputTextContainer() { }

bool InputTextContainer::init() {
	if (!Node::init()) {
		return false;
	}

	_label = addChild(Rc<TypescaleLabel>::create(TypescaleRole::BodyLarge), ZOrder(-1));
	_label->setAnchorPoint(Anchor::BottomLeft);

	_label->setTransformDirtyCallback([this](const Mat4 &) { updateCursorPointers(); });

	_caret = _label->addChild(Rc<Layer>::create());
	_caret->setAnchorPoint(Anchor::BottomLeft);
	_caret->setOpacity(0.0f);

	_cursorPointer = addChild(Rc<IconSprite>::create(IconName::Stappler_CursorIcon), ZOrder(1));
	_cursorPointer->setContentSize(Size2(24.0f, 24.0f));
	_cursorPointer->setAnchorPoint(Vec2(0.5f, _cursorAnchor));
	_cursorPointer->setBlendColor(ColorRole::Primary, 1.0f);
	_cursorPointer->setVisible(false);

	_selectionPointerStart =
			addChild(Rc<IconSprite>::create(IconName::Stappler_SelectioinStartIcon), ZOrder(1));
	_selectionPointerStart->setContentSize(Size2(24.0f, 24.0f));
	_selectionPointerStart->setAnchorPoint(Vec2(1.0f, _cursorAnchor));
	_selectionPointerStart->setBlendColor(ColorRole::Primary, 1.0f);
	_selectionPointerStart->setVisible(false);

	_selectionPointerEnd =
			addChild(Rc<IconSprite>::create(IconName::Stappler_SelectioinEndIcon), ZOrder(1));
	_selectionPointerEnd->setContentSize(Size2(24.0f, 24.0f));
	_selectionPointerEnd->setAnchorPoint(Vec2(0.0f, _cursorAnchor));
	_selectionPointerEnd->setBlendColor(ColorRole::Primary, 1.0f);
	_selectionPointerEnd->setVisible(false);

	_scissorComponent =
			addSystem(Rc<DynamicStateSystem>::create(DynamicStateApplyMode::ApplyForNodesBelow));
	_scissorComponent->enableScissor(Padding(0.0f, 2.0f));

	return true;
}

void InputTextContainer::update(const UpdateTime &time) {
	Node::update(time);

	if (_selectedPointer) {
		if (hasHorizontalOverflow()) {
			const auto width = _contentSize.width;
			const auto offset = std::min(48.0f, width / 3.0f);
			const auto xPos = _selectedPointer->getPosition().x;
			const auto labelWidth = _label->getContentSize().width;
			const auto labelPos = _label->getPosition().x;
			const auto minPos = width - labelWidth;
			const auto maxPos = 0.0f;

			const auto maxv = 300.0f;

			if (xPos < offset) {
				const float relPos = 1.0f - math::clamp(xPos / offset, 0.0f, 1.0f);
				_label->setPositionX(std::min(maxPos, labelPos + relPos * maxv * time.dt));
			} else if (xPos > width - offset) {
				const float relPos = 1.0f - math::clamp((width - xPos) / offset, 0.0f, 1.0f);
				_label->setPositionX(std::max(minPos, labelPos - relPos * maxv * time.dt));
			}
		}
	}
}

void InputTextContainer::handleContentSizeDirty() {
	Node::handleContentSizeDirty();

	_label->setPosition(
			Vec2(0.0f, 0.0f) + Vec2(_label->getContentSize() - _contentSize) * _adjustment);
	_caret->setContentSize(Size2(1.5f, _label->getFontHeight()));
}

bool InputTextContainer::visitDraw(FrameInfo &frame, NodeVisitFlags parentFlags) {
	if (!_visible) {
		return false;
	}

	if (_cursorDirty) {
		updateCursorPosition();
		_cursorDirty = false;
	}

	auto style = frame.getSystem<SurfaceInterior>(SurfaceInterior::SystemFrameTag);
	auto styleContainer = frame.getSystem<StyleContainer>(StyleContainer::SystemFrameTag);
	if (style && styleContainer) {
		if (auto scheme = styleContainer->getScheme(style->getStyle().schemeTag)) {
			auto c = scheme->get(ColorRole::Primary);
			auto currentColor = _caret->getColor();
			currentColor.a = 1.0f;
			if (currentColor != c) {
				_caret->setColor(c, false);
			}
		}
	}

	return Node::visitDraw(frame, parentFlags);
}

void InputTextContainer::setEnabled(bool value) {
	if (value != _enabled) {
		_enabled = value;
		_caret->stopAllActions();
		_caret->runAction(makeEasing(Rc<FadeTo>::create(0.2f, _enabled ? 1.0f : 0.0f)));

		if (!_enabled) {
			unscheduleCursorPointer();
			stopActionByTag("RenderContinuously"_tag);
			setPointerEnabled(false);
		}
	}
}

void InputTextContainer::setCursor(TextCursor cursor) {
	if (_cursor != cursor) {
		_cursor = cursor;
		_cursorDirty = true;
		_caret->setVisible(_cursor.length == 0);
		if (_cursor.length > 0) {
			_label->setSelectionCursor(_cursor);
		} else {
			_label->setSelectionCursor(TextCursor::InvalidCursor);
		}
	}
}

void InputTextContainer::handleLabelChanged() {
	_cursorDirty = true;
	unscheduleCursorPointer();
	setPointerEnabled(false);
}

TextCursor InputTextContainer::getCursorForPosition(const Vec2 &loc) {
	if (_label->isTouched(loc, 4)) {
		auto chIdx = _label->getCharIndex(_label->convertToNodeSpace(loc));
		if (chIdx.first != maxOf<uint32_t>()) {
			if (chIdx.second) {
				return TextCursor(chIdx.first + 1);
			} else {
				return TextCursor(chIdx.first);
			}
		}
	}
	return TextCursor::InvalidCursor;
}

bool InputTextContainer::hasHorizontalOverflow() const {
	return _label->getContentSize().width > _contentSize.width;
}

void InputTextContainer::moveHorizontalOverflow(float d) {
	_label->stopAllActionsByTag("InputTextContainerAdjust"_tag);

	const auto labelWidth = _label->getContentSize().width;
	const auto width = _contentSize.width;
	const auto minPos = width - labelWidth;
	const auto maxPos = 0.0f;

	auto newpos = _label->getPosition().x + d;
	if (newpos < minPos) {
		newpos = minPos;
	} else if (newpos > maxPos) {
		newpos = maxPos;
	}

	_label->setPositionX(newpos);
}

IconSprite *InputTextContainer::getTouchedCursor(const Vec2 &vec, float padding) {
	if (_cursorPointer->isVisible() && _cursorPointer->isTouched(vec, padding)) {
		return _cursorPointer;
	}
	if (_selectionPointerStart->isVisible() && _selectionPointerStart->isTouched(vec, padding)) {
		return _selectionPointerStart;
	}
	if (_selectionPointerEnd->isVisible() && _selectionPointerEnd->isTouched(vec, padding)) {
		return _selectionPointerEnd;
	}
	return nullptr;
}

bool InputTextContainer::handleLongPress(const Vec2 &pt, uint32_t tickCount) {
	if (tickCount == 1) {
		if (_selectedPointer
				|| (_cursorPointer->isVisible() && _cursorPointer->getOpacity() > 0.0f
						&& _cursorPointer->isTouched(pt))
				|| (_selectionPointerStart->isVisible()
						&& _selectionPointerStart->getOpacity() > 0.0f
						&& _selectionPointerStart->isTouched(pt))
				|| (_selectionPointerEnd->isVisible() && _selectionPointerEnd->getOpacity() > 0.0f
						&& _selectionPointerEnd->isTouched(pt))) {
			return false;
		}

		auto pos = _label->convertToNodeSpace(pt);

		auto chIdx = _label->getCharIndex(pos, font::CharSelectMode::Center);
		if (chIdx.first != maxOf<uint32_t>()) {
			auto word = _label->selectWord(chIdx.first);
			setCursor(word);
			if (_cursorCallback) {
				_cursorCallback(_cursor);
			}
			scheduleCursorPointer();
			return true;
		}

	} else if (tickCount == 3) {
		setCursor(TextCursor(0, uint32_t(_label->getCharsCount())));
		if (_cursorCallback) {
			_cursorCallback(_cursor);
		}
		scheduleCursorPointer();
	}
	return false;
}

bool InputTextContainer::handleSwipeBegin(const Vec2 &pt) {
	auto c = getTouchedCursor(pt);
	if (c != nullptr) {
		unscheduleCursorPointer();
		_selectedPointer = c;
		scheduleUpdate();
		runAction(Rc<RenderContinuously>::create(), "RenderContinuously"_tag);
		return true;
	}
	return false;
}

bool InputTextContainer::handleSwipe(const Vec2 &pt, const Vec2 &delta) {
	if (_selectedPointer) {
		unscheduleCursorPointer();
		auto size = _selectedPointer->getContentSize();
		auto anchor = _selectedPointer->getAnchorPoint();
		auto offset =
				Vec2(anchor.x * size.width - size.width / 2.0f, (anchor.y + 1.0f) * size.height);

		auto locInLabel = _label->convertToNodeSpace(pt) + offset;

		if (_selectedPointer == _cursorPointer) {
			auto chIdx = _label->getCharIndex(locInLabel);
			if (chIdx.first != maxOf<uint32_t>()) {
				auto cursorIdx = chIdx.first;
				if (chIdx.second) {
					++cursorIdx;
				}

				if (_cursor.start != cursorIdx) {
					setCursor(TextCursor(cursorIdx));
					if (_cursorCallback) {
						_cursorCallback(_cursor);
					}
				}
			}
		} else if (_selectedPointer == _selectionPointerStart) {
			auto charNumber = _label->getCharIndex(locInLabel, font::CharSelectMode::Prefix).first;
			if (charNumber != maxOf<uint32_t>()) {
				if (charNumber != _cursor.start && charNumber < _cursor.start + _cursor.length) {
					setCursor(
							TextCursor(charNumber, (_cursor.start + _cursor.length) - charNumber));
				}
			}
		} else if (_selectedPointer == _selectionPointerEnd) {
			auto charNumber = _label->getCharIndex(locInLabel, font::CharSelectMode::Suffix).first;
			if (charNumber != maxOf<uint32_t>()) {
				if (charNumber != _cursor.start + _cursor.length - 1
						&& charNumber >= _cursor.start) {
					setCursor(TextCursor(_cursor.start, charNumber - _cursor.start + 1));
				}
			}
		}
		return true;
	}
	return false;
}

bool InputTextContainer::handleSwipeEnd(const Vec2 &pt) {
	if (_selectedPointer) {
		_selectedPointer = nullptr;
		if (_scheduled && _running) {
			stopActionByTag("RenderContinuously"_tag);
			unscheduleUpdate();
		}
		scheduleCursorPointer();
		return true;
	}
	return false;
}

void InputTextContainer::touchPointers() {
	if (!_label->empty()) {
		scheduleCursorPointer();
	}
}

void InputTextContainer::setCursorCallback(Function<void(TextCursor)> &&cb) {
	_cursorCallback = sp::move(cb);
}

const Function<void(TextCursor)> &InputTextContainer::getCursorCallback() const {
	return _cursorCallback;
}

void InputTextContainer::updateCursorPosition() {
	Vec2 cpos;
	if (_label->empty()) {
		cpos = Vec2(0.0f, 0.0f);
	} else {
		cpos = _label->getCursorPosition(_cursor.start);
	}
	_caret->setPosition(cpos);

	const auto labelWidth = _label->getContentSize().width;
	const auto width = _contentSize.width;
	const auto minPos = width - std::max(labelWidth, cpos.x);
	const auto maxPos = 0.0f;

	if (labelWidth <= width) {
		runAdjustLabel(0.0f);
	} else {
		auto maxWidth = std::min(width / 4.0f, 60.0f);
		auto containerPos = _label->getNodeToParentTransform().transformPoint(cpos);
		if (containerPos.x < maxWidth || containerPos.x > width - maxWidth) {
			auto newpos = width / 2.0f - cpos.x;
			if (newpos < minPos) {
				newpos = minPos;
			} else if (newpos > maxPos) {
				newpos = maxPos;
			}

			runAdjustLabel(newpos);
		}
	}

	updateCursorPointers();
}

void InputTextContainer::updateCursorPointers() {
	const auto width = _contentSize.width;

	auto &t = _label->getNodeToParentTransform();

	if (_cursor.length > 0) {
		auto endPos = t.transformPoint(
				_label->getCursorPosition(_cursor.start + _cursor.length - 1, false));
		_selectionPointerEnd->setPosition(
				endPos + Vec2(_caret->getContentSize().width / 2.0f, 0.0f));

		if (endPos.x >= 0.0f && endPos.x <= width) {
			_selectionPointerEnd->setOpacity(1.0f);
		} else if (endPos.x < 0.0f) {
			auto op = math::clamp((endPos.x + 10.0f) / 10.0f, 0.0f, 1.0f);
			_selectionPointerEnd->setOpacity(op);
		} else if (endPos.x > width) {
			auto op = math::clamp((width - endPos.x + 10.0f) / 10.0f, 0.0f, 1.0f);
			_selectionPointerEnd->setOpacity(op);
		}
	}

	auto cursorPos = t.transformPoint(_label->getCursorPosition(_cursor.start, true));

	_cursorPointer->setPosition(cursorPos + Vec2(_caret->getContentSize().width / 2.0f, 0.0f));
	_selectionPointerStart->setPosition(
			cursorPos + Vec2(_caret->getContentSize().width / 2.0f, 0.0f));

	if (cursorPos.x >= 0.0f && cursorPos.x <= width) {
		_cursorPointer->setOpacity(1.0f);
		_selectionPointerStart->setOpacity(1.0f);
	} else if (cursorPos.x < 0.0f) {
		auto op = math::clamp((cursorPos.x + 10.0f) / 10.0f, 0.0f, 1.0f);
		_cursorPointer->setOpacity(op);
		_selectionPointerStart->setOpacity(op);
	} else if (cursorPos.x > width) {
		auto op = math::clamp((width - cursorPos.x + 10.0f) / 10.0f, 0.0f, 1.0f);
		_cursorPointer->setOpacity(op);
		_selectionPointerStart->setOpacity(op);
	}

	if (_pointerEnabled && !_label->empty()) {
		if (_cursor.length == 0) {
			_cursorPointer->setVisible(true);
			_selectionPointerStart->setVisible(false);
			_selectionPointerEnd->setVisible(false);
		} else {
			_cursorPointer->setVisible(false);
			_selectionPointerStart->setVisible(true);
			_selectionPointerEnd->setVisible(true);
		}
	} else {
		_cursorPointer->setVisible(false);
		_selectionPointerStart->setVisible(false);
		_selectionPointerEnd->setVisible(false);
	}
}

void InputTextContainer::runAdjustLabel(float pos) {
	if (_selectedPointer) {
		return;
	}

	if (_label->getPosition().x == pos) {
		_label->stopAllActionsByTag("InputTextContainerAdjust"_tag);
		return;
	}

	_label->stopAllActionsByTag("InputTextContainerAdjust"_tag);

	const float minT = 0.05f;
	const float maxT = 0.35f;
	const float labelPos = _label->getPosition().x;

	auto dist = std::fabs(labelPos - pos);

	if (_enabled) {
		if (dist > _contentSize.width * 0.5f) {
			auto targetPos =
					labelPos - std::copysign(dist - _contentSize.width * 0.25f, labelPos - pos);
			_label->setPositionX(targetPos);
			dist = _contentSize.width * 0.5f;
		}
	}

	auto t = minT;
	if (dist < 16.0f) {
		t = minT;
	} else if (dist > 80.0f) {
		t = maxT;
	} else {
		t = progress(minT, maxT, (dist - 16.0f) / 80.0f);
	}

	auto a = makeEasing(Rc<MoveTo>::create(t, Vec2(pos, _label->getPosition().y)));
	_label->runAction(a, "InputTextContainerAdjust"_tag);
}

void InputTextContainer::scheduleCursorPointer() {
	stopAllActionsByTag("TextFieldCursorPointer"_tag);
	setPointerEnabled(true);
	if (_cursor.length == 0) {
		runAction(Rc<Sequence>::create(3.5f, [this] { setPointerEnabled(false); }),
				"TextFieldCursorPointer"_tag);
	}
}

void InputTextContainer::unscheduleCursorPointer() {
	stopAllActionsByTag("TextFieldCursorPointer"_tag);
}

void InputTextContainer::setPointerEnabled(bool value) {
	if (_pointerEnabled != value) {
		_pointerEnabled = value;
		updateCursorPointers();
	}
}

} // namespace stappler::xenolith::material2d
