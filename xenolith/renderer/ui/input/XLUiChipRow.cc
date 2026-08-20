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
#include "XLUiInteractiveComponent.h"
#include "XLInputListener.h"
#include "XLAppWindow.h"
#include "XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// The fallback's metrics, in points. Only the fallback: a styled row lays its chips out with
// `display:flex; flex-wrap:wrap` and none of the arithmetic below runs.
static constexpr float s_chipRowPadding = 4.0f;
static constexpr float s_chipRowGap = 6.0f;
static constexpr float s_chipRowLineGap = 4.0f;

// The "+" box. Its width is icon + 2 * 8 for the same reason ui::Chip's remove button's is: that is
// what centres a glyph under ui::Button's own fallback placement.
static constexpr IconName s_chipRowAddIcon = IconName::Content_add_solid;
static constexpr float s_chipRowAddIconSize = 12.0f;
static constexpr float s_chipRowAddWidth = s_chipRowAddIconSize + 16.0f;
static constexpr float s_chipRowAddHeight = 18.0f;

ChipRow::~ChipRow() { }

bool ChipRow::init() {
	if (!Panel::init()) {
		return false;
	}

	setType("chip-row");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-chip-row");
	registerStyleAppliers("chip-row");

	_addButton = addChild(Rc<Button>::create([this] {
		// The tap that opens the menu is also the tap that puts the row in the form's hands.
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
		// Only the background gets here: a tap on a chip is consumed by the chip's own listener,
		// which is deeper in the tree and therefore dispatched first.
		if (tap.event == GestureEvent::Activated && _enabled) {
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

	// A key event carries the pointer location, so the default filter would answer the arrows only
	// while the mouse hovers the row. A focused widget owns the keyboard wherever the pointer is -
	// the same seam, and the same reason, as ui::Select's.
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
		// Not while the menu is up: the tap that picks an option lands in another window, and
		// blurring on it would take the row out of the form ring mid-choice.
		if (!isOpen()) {
			blur();
		}
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}), 1});
	_focusListener->setTouchFilter(
			[this](const InputEvent &event, const InputListener::DefaultEventFilter &) {
		return !isTouched(event.currentLocation, 0.0f);
	});
	// Off until there is focus to lose - ui::Select's and ui::TextInput's do the same.
	_focusListener->setEnabled(false);

	/* The wrapped height, through the protocol every other measurable node answers.

	Declined when a LayoutSystem is present: `display:flex` placed the chips, so the flex pass is
	both the better answer and the only one that agrees with what is drawn. Returning false here
	lets the request fall through to it. */
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
	// The surface hangs off a window this node is leaving; a menu left standing over a row that is
	// no longer on screen is one the user has to dismiss by hand.
	close();
	Panel::handleExit();
}

void ChipRow::handleContentSizeDirty() {
	Panel::handleContentSizeDirty();

	// A LayoutSystem - from `display:flex` or added by hand - owns the children's geometry, and the
	// placement below would be a second writer of the same positions.
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

	// The selection stays where the hand was: on whatever moved into the gap, or on the new last
	// chip when the gap was at the end. That is what lets Delete be pressed twice in a row.
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
		// The menu was built from the previous list. Rebuilding it under the user is worse than
		// closing it: the row they were about to click would move.
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
	// Deliberately does NOT truncate: a limit lowered under a value that already exceeds it is a
	// declaration about what may be ADDED, and silently dropping members would destroy data nobody
	// asked to lose. The "+" goes dead until the row is back under the limit.
	updateAddButton();
}

bool ChipRow::isFull() const { return _maxCount > 0 && _items.size() >= _maxCount; }

void ChipRow::setUniqueIds(bool value) {
	if (_unique == value) {
		return;
	}
	_unique = value;
	// Same rule as the limit's: existing duplicates are left alone, and the menu stops offering
	// what is already there.
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
	// The lock has the last word, and remembers what was asked for so unlocking can give it
	// back. A no-op, and one pointer test, on a control nobody locked.
	value = resolveEditLock(this, value);
	if (_enabled == value) {
		return;
	}
	_enabled = value;
	if (!_enabled) {
		close();
		blur();
	}
	applyControlEnabled(this, _enabled);

	for (auto &it : _chips) { it->setEnabled(_enabled); }
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
	// The measurement is the placement with the writing turned off - see layoutRow.
	return const_cast<ChipRow *>(this)->layoutRow(width, false);
}

void ChipRow::setIntrinsicHeightCallback(Function<void(float)> &&cb) {
	_intrinsicHeightCallback = sp::move(cb);
	// A fresh listener has been told nothing yet, so the current height is news to it.
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
	if (_focused || !_enabled) {
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

	// The selection exists to be what Delete takes off, and Delete needs the keyboard. Keeping it
	// painted on a row that no longer has one would show a target that cannot be hit.
	select(-1);
	updateInteractiveState();

	if (_focusCallback) {
		_focusCallback(false);
	}
}

void ChipRow::focusFromNavigation(bool backwards) {
	if (_focused) {
		// A tap already decided what is selected and the form is only catching up with it.
		return;
	}
	focus();
	if (_items.empty()) {
		return;
	}
	select(backwards ? int32_t(_items.size()) - 1 : 0);
}

bool ChipRow::open() {
	if (!_enabled || isOpen() || isFull()) {
		return false;
	}

	// A surface of the caller's own comes first: it is the reason the seam exists, and a row that
	// has one is not offering the built-in list at all.
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

	// Anchored on the "+" rather than on the row: the row may be three lines tall, and a menu
	// dropped off its bottom edge would open nowhere near the button that was pressed.
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
		chip->setEnabled(_enabled);
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

	// A button that opens nothing is worse than no button: it invites a press that cannot do
	// anything and says nothing about why.
	const bool offers = _addCallback || !_options.empty();
	_addButton->setVisible(offers);
	_addButton->setEnabled(_enabled && !isFull());

	if (isFull()) {
		addStyleClass("full");
	} else {
		removeStyleClass("full");
	}

	_contentSizeDirty = true;
}

void ChipRow::updateInteractiveState() {
	setOrUpdateComponent<InteractiveComponent>([this](NotNull<InteractiveComponent> state) {
		// The Enabled bit and the `disabled` class are applyControlEnabled's, from setEnabled.
		bool dirty = false;
		// The counters are cumulative, so each flag is pushed on an edge and never twice.
		const bool hover = _hoverApplied && _enabled;
		if (hover != sprt::hasFlag(state->state, InteractiveState::Hover)) {
			dirty = state->handleHover(hover ? 1 : -1) || dirty;
		}
		const bool focus = _focusApplied && _enabled;
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
		// The natural size, through the protocol - which for a ui::Chip is its own measureNatural
		// and for anything else is whatever that node answers with.
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
			// Lines run DOWN from the top of the box, so the first one stays put when the row grows
			// a line - the alternative makes every chip jump whenever the last one wraps.
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
		// What is already here is not on offer, and says so by being dead rather than by refusing
		// after the fact.
		button->setEnabled(option.enabled && !(_unique && indexOf(option.id) >= 0));
	}
	return source;
}

bool ChipRow::handleKey(const GestureData &data) {
	if (!_focused || !_enabled || !data.input) {
		return false;
	}

	const auto &ev = data.input->data;
	if (ev.event != InputEventName::KeyPressed && ev.event != InputEventName::KeyRepeated) {
		return false;
	}

	// While the menu is up its own MenuSystem answers the keyboard, in its own window. Anything
	// this node did here would be a second reader of the same key.
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
			// Deliberately does not wrap: a row is not a dial, and running off its end by holding
			// an arrow down is not a choice anyone made. Same as ui::Select::step.
			return false;
		}
		select(next);
		return true;
	};

	// Removal by key obeys the same `removable` flag the button does - a fixed member of a chain
	// must not be reachable by one route and not by the other.
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
		// Nothing is selected, so there is nothing to delete YET: this press selects the last chip
		// and the next one takes it off. Removal always has a visible target.
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
	if (!_enabled) {
		return false;
	}
	// The tap does two things, and the second one is not the widget's to do alone: focus() reports
	// through the focus callback, which is how the FORM learns to hand this field the keyboard.
	focus();
	select(int32_t(index));
	return true;
}

bool ChipRow::handleChipRemove(uint32_t index) {
	if (!_enabled || index >= _items.size()) {
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
