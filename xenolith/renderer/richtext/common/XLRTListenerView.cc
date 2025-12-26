/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#include "XLRTListenerView.h"
#include "SPDocLayoutResult.h"
#include "XLDirector.h"
#include "XLInputListener.h"
#include "XLDynamicStateSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::richtext {

XL_DECLARE_EVENT_CLASS(ListenerView, onSelection);
XL_DECLARE_EVENT_CLASS(ListenerView, onExternalLink);

bool ListenerView::Selection::init(ListenerView *view) {
	if (!Sprite::init()) {
		return false;
	}

	_markerStart = addChild(Rc<basic2d::VectorSprite>::create(Size2(48, 48)), ZOrder(1));
	(void)_markerStart->addPath(vg::VectorPath().openForWriting([](vg::PathWriter &writer) {
		writer.moveTo(48, 48).lineTo(24, 48).arcTo(24, 24, 0, true, true, 48, 24).closePath();
	}));
	_markerStart->setContentSize(Size2(24.0f, 24.0f));
	_markerStart->setAnchorPoint(Vec2(1.0f, 1.0f));
	_markerStart->setColor(Color::Blue_500);
	_markerStart->setOpacity(192);
	_markerStart->setVisible(false);

	auto markerStartComp = _markerStart->addSystem(Rc<DynamicStateSystem>::create());
	markerStartComp->setIgnoreParentState(true);
	markerStartComp->setStateApplyMode(DynamicStateApplyMode::ApplyForAll);

	_markerEnd = addChild(Rc<basic2d::VectorSprite>::create(Size2(48, 48)), ZOrder(1));
	(void)_markerEnd->addPath(vg::VectorPath().openForWriting([](vg::PathWriter &writer) {
		writer.moveTo(0, 48).lineTo(0, 24).arcTo(24, 24, 0, true, true, 24, 48).closePath();
	}));
	_markerEnd->setContentSize(Size2(24.0f, 24.0f));
	_markerEnd->setAnchorPoint(Vec2(0.0f, 1.0f));
	_markerEnd->setColor(Color::Blue_500);
	_markerEnd->setOpacity(192);
	_markerEnd->setVisible(false);

	auto markerEndComp = _markerEnd->addSystem(Rc<DynamicStateSystem>::create());
	markerEndComp->setIgnoreParentState(true);
	markerEndComp->setStateApplyMode(DynamicStateApplyMode::ApplyForAll);

	_view = view;

	setColor(Color::Blue_400);
	setOpacity(0.35f);

	setCascadeOpacityEnabled(false);

	_selectionBounds.first = SelectionPosition{maxOf<size_t>(), 0};
	_selectionBounds.second = SelectionPosition{maxOf<size_t>(), 0};

	return true;
}

void ListenerView::Selection::clearSelection() {
	_index = 0;
	_object = nullptr;
	_vertexes.clear();

	_markerStart->setVisible(false);
	_markerEnd->setVisible(false);

	_selectionBounds.first = SelectionPosition{maxOf<size_t>(), 0};
	_selectionBounds.second = SelectionPosition{maxOf<size_t>(), 0};

	setEnabled(false);
}

void ListenerView::Selection::selectLabel(const Object *obj, const Vec2 &loc) {
	if (auto label = obj->asLabel()) {
		_index = obj->index;
		_object = obj;

		if (_mode != SelectMode::Full) {
			_selectionBounds.first = SelectionPosition{_object->index, 0};
			_selectionBounds.second =
					SelectionPosition{_object->index, uint32_t(label->layout.chars.size() - 1)};
		} else {
			const float d = _inputDensity;
			Vec2 locInLabel((loc - obj->bbox.origin));

			uint32_t charNumber = label->layout
										  .getChar(int32_t(roundf(locInLabel.x * d)),
												  int32_t(roundf(locInLabel.y * d)))
										  .first;
			if (charNumber == maxOf<uint32_t>()) {
				clearSelection();
				return;
			}

			auto b = label->layout.selectWord(charNumber);
			_selectionBounds = pair(SelectionPosition{obj->index, b.first},
					SelectionPosition{obj->index, b.first + b.second - 1});
		}
		_vertexesDirty = true;
		setEnabled(true);
	}
}

void ListenerView::Selection::selectWholeLabel() {
	if (_object) {
		if (auto label = _object->asLabel()) {
			_selectionBounds.first = SelectionPosition{_object->index, 0};
			_selectionBounds.second =
					SelectionPosition{_object->index, uint32_t(label->layout.chars.size() - 1)};

			_vertexesDirty = true;
		}
	}
}

void ListenerView::Selection::emplaceRect(const Rect &rect, size_t idx, size_t count) {
	Vec2 origin;
	if (_view->isVertical()) {
		origin = Vec2(rect.origin.x, -rect.origin.y - _view->getObjectsOffset() - rect.size.height);
	} else {
		auto res = _view->getResult();
		if (res) {
			size_t page = size_t(floorf((rect.origin.y + rect.size.height)
					/ res->result->getMedia().surfaceSize.height));
			auto pageData = res->result->getPageData(page, 0);

			origin = Vec2(rect.origin.x + pageData.viewRect.origin.x + pageData.margin.left,
					pageData.margin.bottom
							+ (pageData.texRect.size.height - rect.origin.y - rect.size.height
									+ pageData.texRect.origin.y));
		} else {
			return;
		}
	}

	auto quad = _vertexes.addQuad();
	quad.setGeometry(Vec4(origin, 0, 1), rect.size);

	if (_mode == SelectMode::Full) {
		if (idx == 0) {
			_markerStart->setPosition(origin);
			_markerStart->setVisible(true);
		}
		if (idx + 1 == count) {
			_markerEnd->setPosition(origin + Vec2(rect.size.width, 0));
			_markerEnd->setVisible(true);
		}
	}
}

void ListenerView::Selection::updateVertexes(FrameInfo &frame) {
	if (!_view->getResult()) {
		return;
	}

	auto res = _view->getResult()->result;
	_vertexes.clear();
	Vector<Rect> rects;
	if (_selectionBounds.first.object == _selectionBounds.second.object) {
		auto obj = res->getObject(_selectionBounds.first.object);
		if (obj && obj->isLabel()) {
			obj->asLabel()->layout.getLabelRects(
					[&](const Rect &rect) { rects.emplace_back(rect); },
					_selectionBounds.first.position, _selectionBounds.second.position,
					_inputDensity, obj->bbox.origin, Padding());
		}
	} else {
		auto firstObj = res->getObject(_selectionBounds.first.object);
		auto secondObj = res->getObject(_selectionBounds.second.object);

		if (firstObj && firstObj->isLabel()) {
			auto label = firstObj->asLabel();
			label->layout.getLabelRects([&](const Rect &rect) { rects.emplace_back(rect); },
					_selectionBounds.first.position, uint32_t(label->layout.chars.size() - 1),
					_inputDensity, firstObj->bbox.origin, Padding());
		}
		for (size_t i = _selectionBounds.first.object + 1; i < _selectionBounds.second.object;
				++i) {
			auto obj = res->getObject(i);
			if (obj && obj->isLabel()) {
				auto label = obj->asLabel();
				label->layout.getLabelRects([&](const Rect &rect) { rects.emplace_back(rect); }, 0,
						uint32_t(label->layout.chars.size() - 1), _inputDensity, obj->bbox.origin,
						Padding());
			}
		}
		if (secondObj && secondObj->isLabel()) {
			auto label = secondObj->asLabel();
			label->layout.getLabelRects([&](const Rect &rect) { rects.emplace_back(rect); }, 0,
					_selectionBounds.second.position, _inputDensity, secondObj->bbox.origin,
					Padding());
		}
	}

	size_t rectIdx = 0;
	for (auto &it : rects) {
		emplaceRect(it, rectIdx, rects.size());
		++rectIdx;
	}
	updateColor();
	_vertexColorDirty = true;
}

bool ListenerView::Selection::onTap(int, Vec2 vec) { return true; }

bool ListenerView::Selection::onPressBegin(Vec2) { return true; }
bool ListenerView::Selection::onLongPress(Vec2 vec, TimeInterval, int count) {
	if (!_markerTarget) {
		auto res = _view->getResult();
		if (res && count == 1) {
			auto loc = _view->convertToObjectSpace(vec);
			if (auto obj = getSelectedObject(res->result, loc)) {
				selectLabel(obj, loc);
			} else {
				clearSelection();
			}
		}
		if (count > 2 && _mode == SelectMode::Full) {
			selectWholeLabel();
			return false;
		}
	}
	return true;
}
bool ListenerView::Selection::onPressEnd(Vec2 vec, TimeInterval time) {
	if (isEnabled() && time < TimeInterval::milliseconds(425)) {
		auto loc = _view->convertToObjectSpace(vec);
		auto obj = getSelectedObject(_view->getResult()->result, loc);
		const float d = _inputDensity;
		if (!obj) {
			clearSelection();
			return true;
		}

		if (_mode != SelectMode::Full) {
			if (obj->index != _selectionBounds.first.object) {
				clearSelection();
			}
			return true;
		}

		auto label = obj->asLabel();
		Vec2 locInLabel((loc - obj->bbox.origin));
		uint32_t charNumber = label->layout
									  .getChar(int32_t(roundf(locInLabel.x * d)),
											  int32_t(roundf(locInLabel.y * d)))
									  .first;

		if (charNumber == maxOf<uint32_t>()) {
			clearSelection();
			return true;
		}

		if (_selectionBounds.first.object == _selectionBounds.second.object
				&& obj->index == _selectionBounds.first.object) {
			if (charNumber < _selectionBounds.first.position) {
				_selectionBounds.first.position = charNumber;
				_vertexesDirty = true;
			} else if (charNumber > _selectionBounds.second.position) {
				_selectionBounds.second.position = charNumber;
				_vertexesDirty = true;
			} else if (charNumber != _selectionBounds.first.position
					&& charNumber != _selectionBounds.second.position) {
				if (charNumber - _selectionBounds.first.position
						< _selectionBounds.second.position - charNumber) {
					_selectionBounds.first.position = charNumber;
					_vertexesDirty = true;
				} else {
					_selectionBounds.second.position = charNumber;
					_vertexesDirty = true;
				}
			}
		} else if (_selectionBounds.first.object >= obj->index
				&& (_selectionBounds.first.object - obj->index) < 10) {
			_selectionBounds.first.object = obj->index;
			_selectionBounds.first.position = charNumber;
			_vertexesDirty = true;
		} else if (_selectionBounds.second.object <= obj->index
				&& (obj->index - _selectionBounds.second.object) < 10) {
			_selectionBounds.second.object = obj->index;
			_selectionBounds.second.position = charNumber;
			_vertexesDirty = true;
		} else if (_selectionBounds.first.object < obj->index
				&& obj->index < _selectionBounds.second.object) {
			if (obj->index - _selectionBounds.first.object
					< _selectionBounds.second.object - obj->index) {
				_selectionBounds.first.object = obj->index;
				_selectionBounds.first.position = charNumber;
				_vertexesDirty = true;
			} else {
				_selectionBounds.second.object = obj->index;
				_selectionBounds.second.position = charNumber;
				_vertexesDirty = true;
			}
		} else {
			clearSelection();
		}
	}
	return true;
}

bool ListenerView::Selection::onPressCancel(Vec2) { return true; }

bool ListenerView::Selection::onSwipeBegin(Vec2 loc) {
	if (_enabled) {
		if (_markerStart->isTouched(loc, 16.0f)) {
			_markerTarget = _markerStart;
			return true;
		} else if (_markerEnd->isTouched(loc, 16.0f)) {
			_markerTarget = _markerEnd;
			return true;
		}
	}
	return false;
}
bool ListenerView::Selection::onSwipe(Vec2 vec, Vec2 d) {
	if (!_markerTarget) {
		return false;
	}

	const float density = _inputDensity;
	auto size = _markerTarget->getContentSize();
	auto anchor = _markerTarget->getAnchorPoint();
	auto offset = Vec2(anchor.x * size.width - size.width / 2.0f, anchor.y * size.height * 1.35f);
	auto loc = _view->convertToObjectSpace(vec + offset * density);

	if (!_object || _object->type != document::Object::Type::Label
			|| !_object->bbox.containsPoint(loc)) {
		auto obj = getSelectedObject(_view->getResult()->result, loc,
				(_markerTarget == _markerStart ? _selectionBounds.second.object
											   : _selectionBounds.first.object),
				int32_t((_markerTarget == _markerStart ? -1 : 1)
						* (_selectionBounds.second.object - _selectionBounds.first.object + 12)));
		if (!obj) {
			return true;
		} else {
			if (obj->type == document::Object::Type::Label) {
				_object = obj;
			} else {
				return true;
			}
		}
	}

	if (_object && _object->type == document::Object::Type::Label) {
		auto label = _object->asLabel();
		Vec2 locInLabel((loc - _object->bbox.origin));

		if (_markerTarget == _markerStart) {
			uint32_t charNumber = label->layout
										  .getChar(int32_t(roundf(locInLabel.x * density)),
												  int32_t(roundf(locInLabel.y * density)),
												  font::CharSelectMode::Prefix)
										  .first;
			if (charNumber != maxOf<uint32_t>()) {
				if (_object->index != _selectionBounds.second.object
						|| (charNumber != _selectionBounds.first.position
								&& charNumber <= _selectionBounds.second.position)) {
					_selectionBounds.first.object = _object->index;
					_selectionBounds.first.position = charNumber;
					_vertexesDirty = true;
				}
			}
		} else {
			uint32_t charNumber = label->layout
										  .getChar(int32_t(roundf(locInLabel.x * density)),
												  int32_t(roundf(locInLabel.y * density)),
												  font::CharSelectMode::Suffix)
										  .first;
			if (charNumber != maxOf<uint32_t>()) {
				if (_object->index != _selectionBounds.first.object
						|| (charNumber != _selectionBounds.second.position
								&& charNumber >= _selectionBounds.first.position)) {
					_selectionBounds.second.object = _object->index;
					_selectionBounds.second.position = charNumber;
					_vertexesDirty = true;
				}
			}
		}
	}

	return true;
}
bool ListenerView::Selection::onSwipeEnd(Vec2 v) {
	if (!_markerTarget) {
		return false;
	}

	_markerTarget = nullptr;
	return true;
}

void ListenerView::Selection::setEnabled(bool value) {
	if (value != _enabled) {
		_enabled = value;
		onSelection(_view, value);
	}
}

bool ListenerView::Selection::isEnabled() const { return _enabled; }

void ListenerView::Selection::setMode(SelectMode m) { _mode = m; }
ListenerView::SelectMode ListenerView::Selection::getMode() const { return _mode; }

bool ListenerView::Selection::hasSelection() const {
	return _selectionBounds.first.object != maxOf<size_t>()
			&& _selectionBounds.first.object != maxOf<size_t>();
}

String ListenerView::Selection::getSelectedString(size_t maxWords) const {
	if (!_view->getResult()) {
		return String();
	}

	WideString ret;

	if (hasSelection()) {
		auto res = _view->getResult()->result;
		if (_selectionBounds.first.object == _selectionBounds.second.object) {
			auto obj = res->getObject(_selectionBounds.first.object);
			if (obj && obj->isLabel()) {
				obj->asLabel()->layout.str([&](char32_t ch) { ret.push_back(ch); },
						_selectionBounds.first.position, _selectionBounds.second.position, maxWords,
						true);
			}
		} else if (maxWords != maxOf<size_t>()) {
			auto obj = res->getObject(_selectionBounds.first.object);
			if (obj && obj->isLabel()) {
				auto l = obj->asLabel();
				l->layout.str([&](char32_t ch) { ret.push_back(ch); },
						_selectionBounds.first.position, uint32_t(l->layout.chars.size() - 1),
						maxWords, true);
			}
		} else {
			auto firstObj = res->getObject(_selectionBounds.first.object);
			auto lastObj = res->getObject(_selectionBounds.second.object);

			if (firstObj && firstObj->isLabel()) {
				auto l = firstObj->asLabel();
				l->layout.str([&](char16_t ch) { ret.push_back(ch); },
						_selectionBounds.first.position, uint32_t(l->layout.chars.size() - 1),
						maxWords, true);
			}

			for (size_t i = _selectionBounds.first.object + 1; i < _selectionBounds.second.object;
					++i) {
				auto obj = res->getObject(i);
				if (obj && obj->isLabel()) {
					auto l = obj->asLabel();
					l->layout.str([&](char32_t ch) { ret.push_back(ch); }, 0,
							uint32_t(l->layout.chars.size() - 1), maxWords, true);
				}
			}

			if (lastObj && lastObj->isLabel()) {
				auto l = lastObj->asLabel();
				l->layout.str([&](char32_t ch) { ret.push_back(ch); }, 0,
						_selectionBounds.second.position, maxWords, true);
			}
		}
	}
	if (ret.empty()) {
		return String();
	}
	return string::toUtf8<Interface>(ret);
}

Pair<ListenerView::SelectionPosition, ListenerView::SelectionPosition>
ListenerView::Selection::getSelectionPosition() const {
	return _selectionBounds;
}

StringView ListenerView::Selection::getSelectedHash() const {
	if (auto res = _view->getResult()) {
		if (_mode != SelectMode::Full) {
			auto obj = res->result->getObject(_selectionBounds.first.object);
			if (obj->isLabel()) {
				return obj->asLabel()->hash;
			}
		}
	}
	return StringView();
}

size_t ListenerView::Selection::getSelectedSourceIndex() const {
	if (auto res = _view->getResult()) {
		if (_mode != SelectMode::Full) {
			auto obj = res->result->getObject(_selectionBounds.first.object);
			if (obj->isLabel()) {
				return obj->asLabel()->sourceIndex;
			}
		}
	}
	return 0;
}

bool ListenerView::Selection::shouldReceiveTouch(Vec2 loc) const {
	if (_enabled) {
		if (_markerStart->isVisible() && _markerStart->isTouched(loc, 16.0f)) {
			return true;
		} else if (_markerEnd->isVisible() && _markerEnd->isTouched(loc, 16.0f)) {
			return true;
		}
	}
	return false;
}

const Object *ListenerView::Selection::getSelectedObject(document::LayoutResult *res,
		Vec2 loc) const {
	auto objs = res->getObjects();
	for (auto &it : objs) {
		if (it->isLabel()) {
			if (_mode != SelectMode::Indexed
					|| (!it->asLabel()->hash.empty() && it->asLabel()->sourceIndex != 0)) {
				auto bbox = it->bbox;
				auto maxAdvance = it->asLabel()->layout.maxAdvance;
				auto advance = maxAdvance / res->getMedia().density;
				bbox.size.width = advance;
				if (bbox.containsPoint(loc, 12.0f)) {
					return it;
				}
			}
		}
	}
	return nullptr;
}

const Object *ListenerView::Selection::getSelectedObject(document::LayoutResult *res, Vec2 loc,
		size_t pos, int32_t offset) const {
	auto objs = res->getObjects();
	if (offset < 0 && pos > 0) {
		if (-offset > int32_t(pos)) {
			offset = int32_t(-pos);
		}

		auto end = objs.begin() + pos + 1;
		auto start = objs.begin() + (pos + offset);
		for (auto it = start; it != end; ++it) {
			if ((*it)->isLabel()) {
				if (loc.x >= (*it)->bbox.getMinX() - 8.0f && loc.x <= (*it)->bbox.getMaxX() + 8.0f
						&& loc.y >= (*it)->bbox.getMinY() - 8.0f
						&& loc.y <= (*it)->bbox.getMaxY() + 8.0f) {
					return (*it);
				}
			}
		}

	} else if (offset > 0 && pos < objs.size()) {
		if (pos + offset > objs.size()) {
			offset = int32_t(objs.size() - pos);
		}

		auto start = objs.begin() + pos;
		auto end = objs.begin() + (pos + 1 + offset);
		for (auto it = start; it != end; ++it) {
			if ((*it) && (*it)->isLabel()) {
				if (loc.x >= (*it)->bbox.getMinX() - 8.0f && loc.x <= (*it)->bbox.getMaxX() + 8.0f
						&& loc.y >= (*it)->bbox.getMinY() - 8.0f
						&& loc.y <= (*it)->bbox.getMaxY() + 8.0f) {
					return (*it);
				}
			}
		}
	}
	return nullptr;
}

ListenerView::~ListenerView() { }

bool ListenerView::init(Layout l, CommonSource *source, Vector<String> &&ids) {
	if (!CommonView::init(l, source, sp::move(ids))) {
		return false;
	}

	_selection = addChild(Rc<Selection>::create(this), ZOrder(10));

	_inputListener->setTouchFilter(
			[this](const InputEvent &f, const InputListener::DefaultEventFilter &fn) {
		return fn(f) || _selection->shouldReceiveTouch(f.currentLocation);
	});

	return true;
}

void ListenerView::setUseSelection(bool value) { _useSelection = value; }

void ListenerView::handleExit() {
	if (isSelectionEnabled()) {
		disableSelection();
	}
	CommonView::handleExit();
}

void ListenerView::setLayout(Layout l) {
	_selection->clearSelection();
	CommonView::setLayout(l);
}

void ListenerView::onTap(int count, Vec2 vec) {
	if (count == 1 && _tapCallback) {
		if (_linksEnabled) {
			if (auto res = getResult()) {
				auto loc = convertToObjectSpace(vec);
				auto objs = res->result->getRefs();
				for (auto &it : objs) {
					if (isObjectTapped(loc, *it)) {
						return;
					}
				}
			}
		}

		_tapCallback(count, vec);
	}
}

void ListenerView::onObjectPressEnd(Vec2 vec, const Object &obj) {
	if (auto ref = obj.asLink()) {
		onLink(ref->target, ref->mode, ref->text, vec);
	}
}

void ListenerView::onLink(StringView ref, StringView target, WideStringView text, Vec2 pos) {
	if (ref.starts_with("http://") || ref.starts_with("https://")) {
		_director->getApplication()->openUrl(ref);
		onExternalLink(this, ref);
	} else if (ref.starts_with("mailto:")) {
		_director->getApplication()->openUrl(ref);
		onExternalLink(this, ref);
	} else if (ref.starts_with("tel:")) {
		_director->getApplication()->openUrl(ref);
		onExternalLink(this, ref);
	}
}

bool ListenerView::onSwipeEventBegin(uint32_t id, Vec2 loc, Vec2 d, Vec2 v) {
	if (_useSelection && _selection->isEnabled() && _selection->onSwipeBegin(loc)) {
		return _selection->onSwipe(loc, d);
	}
	return CommonView::onSwipeEventBegin(id, loc, d, v);
}
bool ListenerView::onSwipeEvent(uint32_t id, Vec2 loc, Vec2 d, Vec2 v) {
	if (_useSelection && _selection->isEnabled() && _selection->onSwipe(loc, d)) {
		return true;
	}
	return CommonView::onSwipeEvent(id, loc, d, v);
}
bool ListenerView::onSwipeEventEnd(uint32_t id, Vec2 loc, Vec2 d, Vec2 v) {
	if (_useSelection && _selection->isEnabled() && _selection->onSwipeEnd(v)) {
		return true;
	}
	return CommonView::onSwipeEventEnd(id, loc, d, v);
}

bool ListenerView::onPressBegin(Vec2 loc) {
	if (_useSelection && _selection->isEnabled()) {
		return _selection->onPressBegin(loc);
	}
	return CommonView::onPressBegin(loc);
}

bool ListenerView::onLongPress(Vec2 vec, TimeInterval interval, int count) {
	if (CommonView::onLongPress(vec, interval, count) && _useSelection) {
		return _selection->onLongPress(vec, interval, count);
	}
	return false;
}

bool ListenerView::onPressEnd(Vec2 loc, TimeInterval time) {
	if (_useSelection && _selection->isEnabled()) {
		return _selection->onPressEnd(loc, time);
	}
	return CommonView::onPressEnd(loc, time);
}
bool ListenerView::onPressCancel(Vec2 loc, TimeInterval time) {
	if (_useSelection && _selection->isEnabled()) {
		return _selection->onPressCancel(loc);
	}
	return CommonView::onPressCancel(loc, time);
}

void ListenerView::disableSelection() { _selection->clearSelection(); }
bool ListenerView::isSelectionEnabled() const { return _selection->isEnabled(); }

void ListenerView::setSelectMode(SelectMode m) { _selection->setMode(m); }
ListenerView::SelectMode ListenerView::getSelectMode() const { return _selection->getMode(); }

String ListenerView::getSelectedString(size_t maxWords) const {
	return _selection->getSelectedString(maxWords);
}
Pair<ListenerView::SelectionPosition, ListenerView::SelectionPosition>
ListenerView::getSelectionPosition() const {
	return _selection->getSelectionPosition();
}

StringView ListenerView::getSelectedHash() const { return _selection->getSelectedHash(); }
size_t ListenerView::getSelectedSourceIndex() const { return _selection->getSelectedSourceIndex(); }

void ListenerView::onPosition() {
	_selection->setPosition(_root->getPosition());
	_selection->setContentSize(_root->getContentSize());
	CommonView::onPosition();
}

void ListenerView::onRenderer(RendererResult *res, bool status) {
	_selection->clearSelection();
	CommonView::onRenderer(res, status);
}

} // namespace stappler::xenolith::richtext
