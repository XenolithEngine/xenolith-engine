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

#include "XLUiChipRow.h"
#include "XLUiLayoutSystem.h"
#include "XLInteractiveComponent.h"
#include "XLInputListener.h"
#include "XLAppWindow.h"
#include "XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// Fallback metrics in points, used only without a LayoutSystem (no `display:flex`).
static constexpr float s_chipRowPadding = 4.0f;
static constexpr float s_chipRowGap = 6.0f;
static constexpr float s_chipRowLineGap = 4.0f;

// The "+" box; width is icon + 2 * 8 to centre the glyph under ui::Button's fallback placement.
static constexpr IconName s_chipRowAddIcon = IconName::Content_add_solid;
static constexpr float s_chipRowAddIconSize = 12.0f;
static constexpr float s_chipRowAddWidth = s_chipRowAddIconSize + 16.0f;
static constexpr float s_chipRowAddHeight = 18.0f;

ChipRow::~ChipRow() { }

bool ChipRow::init() {
	if (!Panel::init()) {
		return false;
	}

	/* The InteractiveComponent must exist before anything reads isEnabled(): a node without one
	reads as state 0, which matches `:disabled`. */
	applyControlEnabled(this, true);

	setType("chip-row");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-chip-row");
	registerStyleAppliers("chip-row");

	_addButton = addChild(Rc<Button>::create([this] {
		// opening the menu also focuses the row in the form
		focus();
		open();
	}),
			ZOrder(2));
	_addButton->setType("button");
	_addButton->setName("add");
	_addButton->addStyleClass("xl-ui-chip-add");
	_addButton->setIcon(s_chipRowAddIcon);
	if (auto glyph = _addButton->getIconSprite()) {
		glyph->setContentSize(Size2(s_chipRowAddIconSize, s_chipRowAddIconSize));
	}
	_addButton->setContentSize(Size2(s_chipRowAddWidth, s_chipRowAddHeight));
	_addButton->setVisible(false);

	_listener = addSystem(Rc<InputListener>::create());

	_listener->addTapRecognizer([this](const GestureTap &tap) {
		// only background taps arrive here; chip listeners are deeper and dispatched first
		if (tap.event == GestureEvent::Activated && isEnabled()) {
			focus();
		}
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}), 1});

	_listener->addMouseOverRecognizer([this](const GestureData &data) {
		switch (data.event) {
		case GestureEvent::Began: _hoverApplied = true; break;
		case GestureEvent::Ended:
		case GestureEvent::Cancelled: _hoverApplied = false; break;
		default: break;
		}
		updateInteractiveState();
		return true;
	}, false);

	InputKeyMask keys;
	keys.set(toInt(InputKeyCode::LEFT));
	keys.set(toInt(InputKeyCode::RIGHT));
	keys.set(toInt(InputKeyCode::HOME));
	keys.set(toInt(InputKeyCode::END));
	keys.set(toInt(InputKeyCode::DELETE));
	keys.set(toInt(InputKeyCode::BACKSPACE));
	keys.set(toInt(InputKeyCode::ENTER));
	keys.set(toInt(InputKeyCode::KP_ENTER));
	keys.set(toInt(InputKeyCode::SPACE));
	_listener->addKeyRecognizer([this](const GestureData &data) { return handleKey(data); },
			InputKeyInfo{sp::move(keys)});

	// Key events carry the pointer location; accept them while focused regardless of the pointer
	// (as ui::Select does).
	_listener->setTouchFilter(
			[this](const InputEvent &event, const InputListener::DefaultEventFilter &cb) {
		if (event.data.isKeyEvent()) {
			return _focused;
		}
		return cb(event);
	});

	// A tap outside gives focus up. Priority 1 puts it above the scene graph and its filter accepts
	// only points outside the widget, so it never competes with the tap above.
	_focusListener = addSystem(Rc<InputListener>::create());
	_focusListener->setPriority(1);
	_focusListener->addTapRecognizer([this](const GestureTap &) {
		// not while the menu is open: the tap picking an option lands in another window
		if (!isOpen()) {
			blur();
		}
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}), 1});
	_focusListener->setTouchFilter(
			[this](const InputEvent &event, const InputListener::DefaultEventFilter &) {
		return !isTouched(event.currentLocation, 0.0f);
	});
	// enabled only while focused
	_focusListener->setEnabled(false);

	/* Answers the wrapped height. Declined (returns false) with a LayoutSystem present, so the flex
	pass measures instead. */
	setMeasureCallback([this](const MeasureConstraints &c, Size2 &result) {
		if (!_autoHeight || getSystemByType<LayoutSystem>()) {
			return false;
		}
		const float width = (c.maxWidth == maxOf<float>()) ? _contentSize.width : c.maxWidth;
		if (width <= 0.0f) {
			return false;
		}
		result.width = width;
		result.height = measureHeight(width);
		return true;
	});

	updateAddButton();
	updateInteractiveState();


	return true;
}

void ChipRow::handleExit() {
	// close the menu with the node leaving the window
	close();
	Panel::handleExit();
}

void ChipRow::handleContentSizeDirty() {
	Panel::handleContentSizeDirty();

	// a LayoutSystem owns the children's geometry when present
	if (getSystemByType<LayoutSystem>()) {
		return;
	}

	if (_contentSize.width <= 0.0f) {
		return;
	}

	layoutRow(_contentSize.width, true);
	updateIntrinsicHeight();
}

void ChipRow::setItems(SpanView<ChipItem> items, bool silent) {
	_items.clear();
	_items.reserve(items.size());
	for (auto &it : items) {
		if (!it.id.empty()) {
			_items.emplace_back(it);
		}
	}

	_selected = -1;
	rebuildChips();

	if (!silent) {
		notifyChange();
	}
}

bool ChipRow::addItem(const ChipItem &item, bool silent) {
	if (item.id.empty() || isFull()) {
		return false;
	}
	if (_unique && indexOf(item.id) >= 0) {
		return false;
	}

	_items.emplace_back(item);
	rebuildChips();

	if (!silent) {
		notifyChange();
	}
	return true;
}

bool ChipRow::addById(StringView id, bool silent) {
	auto option = getOption(id);
	if (!option) {
		return false;
	}
	return addItem(ChipItem{option->id, option->title, option->icon, true}, silent);
}

bool ChipRow::removeItem(uint32_t index, bool silent) {
	if (index >= _items.size()) {
		return false;
	}

	_items.erase(_items.begin() + index);

	// The selection moves to the chip that filled the gap, or to the new last chip, so Delete can
	// be repeated.
	if (_selected >= 0) {
		if (_selected > int32_t(index)) {
			--_selected;
		} else if (_selected == int32_t(index)) {
			_selected = sprt::min(_selected, int32_t(_items.size()) - 1);
		}
	}

	rebuildChips();

	if (!silent) {
		notifyChange();
	}
	return true;
}

bool ChipRow::removeById(StringView id, bool silent) {
	auto index = indexOf(id);
	return index < 0 ? false : removeItem(uint32_t(index), silent);
}

void ChipRow::clearItems(bool silent) {
	if (_items.empty()) {
		return;
	}
	_items.clear();
	_selected = -1;
	rebuildChips();

	if (!silent) {
		notifyChange();
	}
}

int32_t ChipRow::indexOf(StringView id) const {
	for (uint32_t i = 0; i < uint32_t(_items.size()); ++i) {
		if (StringView(_items[i].id) == id) {
			return int32_t(i);
		}
	}
	return -1;
}

Chip *ChipRow::getChipAt(uint32_t index) const {
	return index < _chips.size() ? _chips[index] : nullptr;
}

void ChipRow::setOptions(SpanView<ChipOption> options) {
	_options.clear();
	_options.reserve(options.size());
	for (auto &it : options) { _options.emplace_back(it); }

	if (isOpen()) {
		// the menu was built from the previous list; close rather than rebuild it under the user
		close();
	}

	updateAddButton();
}

const ChipOption *ChipRow::getOption(StringView id) const {
	for (auto &it : _options) {
		if (StringView(it.id) == id) {
			return &it;
		}
	}
	return nullptr;
}

void ChipRow::setAddCallback(AddCallback &&cb) {
	_addCallback = sp::move(cb);
	updateAddButton();
}

void ChipRow::setMaxCount(uint32_t value) {
	if (_maxCount == value) {
		return;
	}
	_maxCount = value;
	// Existing items are not truncated; the limit only restricts adding, and the "+" is disabled
	// until the row is under it.
	updateAddButton();
}

bool ChipRow::isFull() const { return _maxCount > 0 && _items.size() >= _maxCount; }

void ChipRow::setUniqueIds(bool value) {
	if (_unique == value) {
		return;
	}
	_unique = value;
	// existing duplicates are kept; the menu stops offering present ids
	if (isOpen()) {
		close();
	}
}

void ChipRow::setWrapEnabled(bool value) {
	if (_wrap == value) {
		return;
	}
	_wrap = value;
	_contentSizeDirty = true;
	markMeasureDirty();
}

void ChipRow::setEnabled(bool value) {
	// the edit lock has the last word and remembers the requested value for unlocking
	value = resolveEditLock(this, value);
	if (isEnabled() == value) {
		return;
	}
	applyControlEnabled(this, value);
	if (!value) {
		close();
		blur();
	}

	for (auto &it : _chips) { it->setEnabled(isEnabled()); }
	updateAddButton();
	updateInteractiveState();
}

void ChipRow::setAutoHeight(bool value) {
	if (_autoHeight == value) {
		return;
	}
	_autoHeight = value;
	markMeasureDirty();
	updateIntrinsicHeight();
}

float ChipRow::getIntrinsicHeight() const { return measureHeight(_contentSize.width); }

float ChipRow::measureHeight(float width) const {
	// layoutRow without committing positions
	return const_cast<ChipRow *>(this)->layoutRow(width, false);
}

void ChipRow::setIntrinsicHeightCallback(Function<void(float)> &&cb) {
	_intrinsicHeightCallback = sp::move(cb);
	// report the current height to the new listener
	_reportedHeight = nan();
	updateIntrinsicHeight();
}

void ChipRow::select(int32_t index) {
	if (index < -1 || index >= int32_t(_items.size())) {
		index = -1;
	}
	if (_selected == index) {
		return;
	}
	_selected = index;
	updateSelection();
}

void ChipRow::focus() {
	if (_focused || !isEnabled()) {
		return;
	}
	_focused = true;
	_focusApplied = true;
	if (_focusListener) {
		_focusListener->setEnabled(true);
	}
	updateInteractiveState();

	if (_focusCallback) {
		_focusCallback(true);
	}
}

void ChipRow::blur() {
	if (!_focused) {
		return;
	}
	_focused = false;
	_focusApplied = false;
	if (_focusListener) {
		_focusListener->setEnabled(false);
	}

	// the selection is a Delete target, which needs the keyboard
	select(-1);
	updateInteractiveState();

	if (_focusCallback) {
		_focusCallback(false);
	}
}

void ChipRow::focusFromNavigation(bool backwards) {
	if (_focused) {
		// a tap already chose the selection
		return;
	}
	focus();
	if (_items.empty()) {
		return;
	}
	select(backwards ? int32_t(_items.size()) - 1 : 0);
}

bool ChipRow::open() {
	if (!isEnabled() || isOpen() || isFull()) {
		return false;
	}

	// the caller's surface replaces the built-in menu
	if (_addCallback) {
		return _addCallback(this);
	}

	if (_options.empty()) {
		return false;
	}

	auto window = getAppWindow();
	if (!window) {
		return false;
	}

	auto source = makeSource();

	MenuConfig config;
	config.style = _menuStyle;
	config.stylesheet = _popupConfig.stylesheet;
	config.stylesheetCategory = _popupConfig.stylesheetCategory;
	config.stylesheetSource = _popupConfig.stylesheetSource;
	config.title = _popupConfig.title;
	config.idPrefix = _popupConfig.idPrefix.empty() ? String("chip-row") : _popupConfig.idPrefix;
	config.flags = _popupConfig.flags;
	config.preferNative = _popupConfig.preferNative;
	config.keyboard = _popupConfig.keyboard;

	config.onClose = [this] {
		_popup = nullptr;
		removeStyleClass("open");
	};

	// anchored on the "+", not on the row, which may span several lines
	Node *anchor = (_addButton && _addButton->isVisible()) ? static_cast<Node *>(_addButton) : this;

	_popup = openMenuForNode(window, anchor, source, sp::move(config), MenuSide::Below);
	if (!_popup) {
		return false;
	}

	addStyleClass("open");
	return true;
}

void ChipRow::close() {
	if (auto popup = sp::move(_popup)) {
		_popup = nullptr;
		removeStyleClass("open");
		popup->dismiss();
	}
}

void ChipRow::setMenuStyle(const MenuStyle &style) { _menuStyle = style; }

void ChipRow::setPopupConfig(MenuConfig &&config) { _popupConfig = sp::move(config); }

void ChipRow::setChangeCallback(ChangeCallback &&cb) { _changeCallback = sp::move(cb); }

void ChipRow::setFocusCallback(FocusCallback &&cb) { _focusCallback = sp::move(cb); }

void ChipRow::setNavigateCallback(NavigateCallback &&cb) { _navigateCallback = sp::move(cb); }

void ChipRow::rebuildChips() {
	for (auto &it : _chips) { it->removeFromParent(); }
	_chips.clear();

	for (uint32_t i = 0; i < uint32_t(_items.size()); ++i) {
		auto &item = _items[i];

		auto chip = addChild(Rc<Chip>::create(), ZOrder(1));
		chip->setName(mem_std::toString("chip-", i));
		chip->setText(item.title.empty() ? StringView(item.id) : StringView(item.title));
		chip->setIcon(item.icon);
		chip->setRemovable(item.removable);
		chip->setEnabled(isEnabled());
		chip->setTapCallback([this, i](NotNull<Chip>) { handleChipTap(i); });
		chip->setRemoveCallback([this, i](NotNull<Chip>) { handleChipRemove(i); });

		_chips.emplace_back(chip);
	}

	updateSelection();
	updateAddButton();

	_contentSizeDirty = true;
	markMeasureDirty();
	updateIntrinsicHeight();
}

void ChipRow::updateSelection() {
	for (uint32_t i = 0; i < uint32_t(_chips.size()); ++i) {
		_chips[i]->setSelected(int32_t(i) == _selected);
	}
}

void ChipRow::updateAddButton() {
	if (!_addButton) {
		return;
	}

	// hidden when there is nothing to open
	const bool offers = _addCallback || !_options.empty();
	_addButton->setVisible(offers);
	_addButton->setEnabled(isEnabled() && !isFull());

	if (isFull()) {
		addStyleClass("full");
	} else {
		removeStyleClass("full");
	}

	_contentSizeDirty = true;
}

void ChipRow::updateInteractiveState() {
	setOrUpdateComponent<InteractiveComponent>([this](NotNull<InteractiveComponent> state) {
		// The Enabled bit is written by applyControlEnabled, from setEnabled.
		bool dirty = false;
		// The counters are cumulative, so each flag is pushed on an edge and never twice.
		const bool hover = _hoverApplied && sprt::hasFlag(state->state, InteractiveState::Enabled);
		if (hover != sprt::hasFlag(state->state, InteractiveState::Hover)) {
			dirty = state->handleHover(hover ? 1 : -1) || dirty;
		}
		const bool focus = _focusApplied && sprt::hasFlag(state->state, InteractiveState::Enabled);
		if (focus != sprt::hasFlag(state->state, InteractiveState::Focus)) {
			dirty = state->handleFocus(focus ? 1 : -1) || dirty;
		}
		return dirty;
	});
}

void ChipRow::notifyChange() {
	if (_changeCallback) {
		_changeCallback(_items);
	}
}

float ChipRow::layoutRow(float width, bool commit) {
	const float avail = sprt::max(width - s_chipRowPadding * 2.0f, 0.0f);

	float x = 0.0f;
	float lineTop = 0.0f;
	float lineHeight = 0.0f;
	uint32_t lines = 1;
	bool first = true;

	auto place = [&](Node *node) {
		// the natural size through the measurement protocol (ui::Chip::measureNatural for chips)
		Size2 size = LayoutSystem::measureNode(node, MeasureConstraints{MeasureMode::MaxContent});
		if (size.width <= 0.0f) {
			size.width = node->getContentSize().width;
		}
		if (size.height <= 0.0f) {
			size.height = node->getContentSize().height;
		}

		if (!first) {
			if (_wrap && x + s_chipRowGap + size.width > avail) {
				lineTop += lineHeight + s_chipRowLineGap;
				lineHeight = 0.0f;
				x = 0.0f;
				++lines;
			} else {
				x += s_chipRowGap;
			}
		}

		if (commit) {
			node->setContentSize(size);
			node->setAnchorPoint(Anchor::TopLeft);
			// lines run down from the top, so earlier lines stay put when the row grows
			node->setPosition(
					Vec2(s_chipRowPadding + x, _contentSize.height - s_chipRowPadding - lineTop));
		}

		x += size.width;
		lineHeight = sprt::max(lineHeight, size.height);
		first = false;
	};

	for (auto &it : _chips) { place(it); }

	if (_addButton && _addButton->isVisible()) {
		place(_addButton);
	}

	if (commit) {
		_lineCount = first ? 0 : lines;
	}

	return lineTop + lineHeight + s_chipRowPadding * 2.0f;
}

void ChipRow::updateIntrinsicHeight() {
	if (!_autoHeight) {
		return;
	}

	const auto height = getIntrinsicHeight();
	if (!sprt::isnan(_reportedHeight) && _reportedHeight == height) {
		return;
	}
	_reportedHeight = height;

	markMeasureDirty();
	if (_intrinsicHeightCallback) {
		_intrinsicHeightCallback(height);
	}
}

Rc<MenuSource> ChipRow::makeSource() {
	auto source = Rc<MenuSource>::create();
	for (auto &option : _options) {
		auto button = source->addButton(option.id, option.title, option.icon,
				[this, id = option.id](NotNull<MenuSourceButton>) { addById(id); });
		// with unique ids, options already present are disabled
		button->setEnabled(option.enabled && !(_unique && indexOf(option.id) >= 0));
	}
	return source;
}

bool ChipRow::handleKey(const GestureData &data) {
	if (!_focused || !isEnabled() || !data.input) {
		return false;
	}

	const auto &ev = data.input->data;
	if (ev.event != InputEventName::KeyPressed && ev.event != InputEventName::KeyRepeated) {
		return false;
	}

	// while the menu is open, its MenuSystem owns the keyboard
	if (isOpen()) {
		return false;
	}

	const int32_t count = int32_t(_items.size());

	auto step = [&](int32_t delta) {
		if (count == 0) {
			return false;
		}
		int32_t next = _selected < 0 ? (delta > 0 ? 0 : count - 1) : _selected + delta;
		if (next < 0 || next >= count) {
			// no wrap-around, as in ui::Select::step
			return false;
		}
		select(next);
		return true;
	};

	// removal by key obeys the same `removable` flag as the button
	auto removeSelected = [&] {
		if (_selected < 0 || _selected >= count) {
			return false;
		}
		if (!_items[uint32_t(_selected)].removable) {
			return false;
		}
		return removeItem(uint32_t(_selected));
	};

	switch (ev.key.keycode) {
	case InputKeyCode::LEFT: return step(-1);
	case InputKeyCode::RIGHT: return step(1);

	case InputKeyCode::HOME:
		if (count == 0) {
			return false;
		}
		select(0);
		return true;

	case InputKeyCode::END:
		if (count == 0) {
			return false;
		}
		select(count - 1);
		return true;

	case InputKeyCode::DELETE: return removeSelected();

	case InputKeyCode::BACKSPACE:
		if (_selected >= 0) {
			return removeSelected();
		}
		if (count == 0) {
			return false;
		}
		// with nothing selected, select the last chip; the next press removes it
		select(count - 1);
		return true;

	case InputKeyCode::ENTER:
	case InputKeyCode::KP_ENTER:
	case InputKeyCode::SPACE: return open();

	default: break;
	}
	return false;
}

bool ChipRow::handleChipTap(uint32_t index) {
	if (!isEnabled()) {
		return false;
	}
	// focus() reports through the focus callback, which lets the form focus this field
	focus();
	select(int32_t(index));
	return true;
}

bool ChipRow::handleChipRemove(uint32_t index) {
	if (!isEnabled() || index >= _items.size()) {
		return false;
	}
	if (!_items[index].removable) {
		return false;
	}
	focus();
	return removeItem(index);
}

AppWindow *ChipRow::getAppWindow() const {
	auto scene = getScene();
	auto director = scene ? scene->getDirector() : nullptr;
	auto server = director ? director->getRenderServer() : nullptr;
	return server ? dynamic_cast<AppWindow *>(server) : nullptr;
}

} // namespace stappler::xenolith::ui
