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

#include "XLUiMenuSystem.h"
#include "XLUiMenuItem.h"
#include "XLUiMenuPopup.h" // Escape and Left ask the chain to take a level down
#include "XLUiStyleSystem.h"
#include "XLUiLayoutSystem.h"
#include "XLInputListener.h"
#include "XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

uint64_t MenuSystem::Id = System::GetNextSystemId();

using DescriptionStyle = basic2d::Label::DescriptionStyle;

static DescriptionStyle MenuSystem_textStyle(uint16_t fontSize, float density) {
	DescriptionStyle ret;
	ret.font.fontSize = font::FontSize(fontSize);
	// The same density the Label will be shaped at. getLabelSize divides the shaped extent by it,
	// so a mismatch here is a menu that is measured for one display and drawn on another.
	ret.font.density = density;
	return ret;
}

// The columns and the fixed width they consume, before the text column gets what is left. A column
// that is not there takes its gap with it - a menu with no icons is not indented by an empty one.
struct MenuSystem_Columns {
	float leading = 0.0f;
	float shortcut = 0.0f;
	float trailing = 0.0f;
	float naturalText = 0.0f;
	float naturalCustom = 0.0f;

	float fixed(const MenuStyle &style) const {
		float ret = style.paddingHorizontal * 2.0f;
		if (leading > 0.0f) {
			ret += leading + style.gap;
		}
		if (shortcut > 0.0f) {
			ret += shortcut + style.gap;
		}
		if (trailing > 0.0f) {
			ret += trailing + style.gap;
		}
		return ret;
	}
};

static MenuSystem_Columns MenuSystem_collectColumns(font::FontController *controller,
		NotNull<MenuSource> source, const MenuStyle &style, float density) {
	MenuSystem_Columns ret;

	const auto titleStyle = MenuSystem_textStyle(style.fontSize, density);
	const auto subtitleStyle = MenuSystem_textStyle(style.subtitleFontSize, density);
	const auto shortcutStyle = MenuSystem_textStyle(style.shortcutFontSize, density);

	bool hasLeading = false;
	bool hasTrailing = false;

	for (auto &it : source->getItems()) {
		if (!it->isVisible()) {
			continue;
		}

		switch (it->getType()) {
		case MenuSourceItem::Type::Button: {
			auto button = static_cast<MenuSourceButton *>(it.get());

			if (button->getLeadingIcon() != IconName::None || button->isChecked()) {
				hasLeading = true;
			}
			if (button->getTrailingIcon() != IconName::None || button->hasSubmenu()) {
				hasTrailing = true;
			}

			if (style.showShortcuts && button->hasShortcut()) {
				StringStream text;
				button->encodeShortcut([&](StringView str) { text << str; });
				ret.shortcut = sprt::max(ret.shortcut,
						basic2d::Label::getStringWidth(controller, shortcutStyle, text.str()));
			}

			if (auto title = button->getTitle(); !title.empty()) {
				ret.naturalText = sprt::max(ret.naturalText,
						basic2d::Label::getStringWidth(controller, titleStyle, title));
			}
			if (auto subtitle = button->getSubtitle(); !subtitle.empty()) {
				ret.naturalText = sprt::max(ret.naturalText,
						basic2d::Label::getStringWidth(controller, subtitleStyle, subtitle));
			}
			break;
		}
		case MenuSourceItem::Type::Custom: {
			// A custom node spans the whole row, not just the text column: it is not a caption
			// with an icon beside it, it is whatever the application put there.
			auto custom = static_cast<MenuSourceCustom *>(it.get());
			ret.naturalCustom = sprt::max(ret.naturalCustom,
					custom->measure(MeasureConstraints{MeasureMode::MaxContent}).width);
			break;
		}
		case MenuSourceItem::Type::Separator: break;
		}
	}

	ret.leading = hasLeading ? style.iconSize : 0.0f;
	ret.trailing = hasTrailing ? style.iconSize : 0.0f;
	return ret;
}

// Fill in the row heights for a width that is already decided. This is where the text actually
// wraps, and it is the only place a row height is ever computed.
static void MenuSystem_resolveRows(font::FontController *controller, NotNull<MenuSource> source,
		const MenuStyle &style, float density, MenuMetrics &metrics) {
	const auto titleStyle = MenuSystem_textStyle(style.fontSize, density);
	const auto subtitleStyle = MenuSystem_textStyle(style.subtitleFontSize, density);

	const float contentWidth = sprt::max(metrics.size.width - style.paddingHorizontal * 2.0f, 0.0f);

	float height = style.paddingVertical * 2.0f;

	for (auto &it : source->getItems()) {
		if (!it->isVisible()) {
			continue;
		}

		MenuMetrics::Row row;
		row.item = it.get();

		switch (it->getType()) {
		case MenuSourceItem::Type::Button: {
			auto button = static_cast<MenuSourceButton *>(it.get());

			if (auto title = button->getTitle(); !title.empty()) {
				row.titleHeight = basic2d::Label::getLabelSize(controller, titleStyle, title,
						style.wrapTitle ? metrics.textColumn : 0.0f)
										  .height;
			}
			if (auto subtitle = button->getSubtitle(); !subtitle.empty()) {
				row.subtitleHeight = basic2d::Label::getLabelSize(controller, subtitleStyle,
						subtitle, style.wrapSubtitle ? metrics.textColumn : 0.0f)
											 .height;
			}

			row.height = sprt::max(style.itemMinHeight,
					style.itemPaddingVertical * 2.0f + row.titleHeight + row.subtitleHeight);
			break;
		}
		case MenuSourceItem::Type::Custom: {
			auto custom = static_cast<MenuSourceCustom *>(it.get());
			row.height = custom->measure(MeasureConstraints{MeasureMode::Normal, contentWidth,
											 maxOf<float>()})
								 .height;
			row.height = sprt::max(row.height, 0.0f);
			break;
		}
		case MenuSourceItem::Type::Separator: row.height = style.separatorHeight; break;
		}

		height += row.height;
		metrics.rows.emplace_back(row);
	}

	metrics.size.height = height;
}

MenuMetrics MenuSystem::measureAtWidth(font::FontController *controller, NotNull<MenuSource> source,
		const MenuStyle &style, float width, float density) {
	MenuMetrics ret;
	if (!controller) {
		return ret;
	}

	const auto columns = MenuSystem_collectColumns(controller, source, style, density);

	ret.leadingColumn = columns.leading;
	ret.shortcutColumn = columns.shortcut;
	ret.trailingColumn = columns.trailing;
	ret.size.width = sprt::max(width, 0.0f);
	// Never negative: a menu squeezed below its own furniture gets a zero text column and clipped
	// text, which is ugly but is not a layout that runs backwards.
	ret.textColumn = sprt::max(ret.size.width - columns.fixed(style), 0.0f);

	MenuSystem_resolveRows(controller, source, style, density, ret);
	return ret;
}

MenuMetrics MenuSystem::measure(font::FontController *controller, NotNull<MenuSource> source,
		const MenuStyle &style, const MeasureConstraints &constraints, float density) {
	MenuMetrics ret;
	if (!controller) {
		return ret;
	}

	const auto columns = MenuSystem_collectColumns(controller, source, style, density);
	const float fixed = columns.fixed(style);

	// What the menu would like: the widest row, either a caption between its columns or a custom
	// node spanning the whole width.
	const float natural = sprt::max(fixed + columns.naturalText,
			style.paddingHorizontal * 2.0f + columns.naturalCustom);

	// MaxContent means "ideal, nothing wrapping it", so the caller's bound does not apply there -
	// only the menu's own maximum, which is what keeps a long command from becoming a screen-wide
	// menu even when there is room.
	float limit = style.maxWidth;
	if (constraints.mode != MeasureMode::MaxContent && constraints.maxWidth != maxOf<float>()) {
		limit = sprt::min(limit, constraints.maxWidth);
	}

	float width = sprt::clamp(natural, sprt::min(style.minWidth, limit), limit);
	if (constraints.mode == MeasureMode::MinContent) {
		// The narrowest a menu is willing to be: its furniture plus the declared minimum. Below
		// that the text column is gone and there is nothing left to shrink.
		width = sprt::min(width, sprt::max(style.minWidth, fixed));
	}

	return measureAtWidth(controller, source, style, width, density);
}

MenuMetrics MenuSystem::measureForNode(NotNull<Node> node, NotNull<MenuSource> source,
		const MenuStyle &style, const MeasureConstraints &constraints) {
	auto director = node->getDirector();
	auto app = director ? director->getApplication() : nullptr;
	auto controller = app ? app->getExtension<font::FontController>() : nullptr;
	return measure(controller, source, style, constraints, node->getInputDensity());
}

MenuSystem *MenuSystem::findForNode(Node *node) {
	while (node) {
		if (auto menu = node->getSystemByType<MenuSystem>()) {
			return menu;
		}
		node = node->getParent();
	}
	return nullptr;
}

MenuSystem::~MenuSystem() { }

bool MenuSystem::init() {
	if (!System::init()) {
		return false;
	}

	_systemPriority = MenuDefaultPriority;
	_frameTag = MenuSystem::Id;

	// HandleMeasure so an inline menu is a valid fit-content item; HandleLayoutChildren because
	// this system, not a LayoutSystem, places the rows.
	setSystemFlags(SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents
			| SystemFlags::HandleNodeEvents | SystemFlags::HandleMeasure
			| SystemFlags::HandleLayoutChildren);
	return true;
}

bool MenuSystem::init(MenuSource *source) {
	if (!init()) {
		return false;
	}
	_source = source;
	return true;
}

bool MenuSystem::init(MenuSource *source, const MenuStyle &style) {
	if (!init(source)) {
		return false;
	}
	_style = style;
	return true;
}

void MenuSystem::handleAdded(Node *owner) {
	System::handleAdded(owner);

	sprt_passert(owner->getSystemByType<LayoutSystem>() == nullptr,
			"MenuSystem owns its children's geometry: the menu node must not carry a LayoutSystem");

	// Tell the style resolver that the rows' ContentSize is this system's business: a CSS width on
	// a menu-item becomes an intrinsic hint rather than a committed size that would be overwritten
	// on every pass, and `display:flex` on the menu cannot add a second writer.
	owner->setComponent<SystemManagedLayout>();

	if (_source) {
		_source->addObserver(this);
	}

	// The mode may have been set before the system had an owner to hang the group off.
	if (_keyboardEnabled) {
		enableKeyboard();
	}

	_itemsDirty = true;
	owner->markLayoutChildrenDirty();
}

void MenuSystem::handleRemoved() {
	if (_source) {
		_source->removeObserver(this);
	}
	disableKeyboard();
	System::handleRemoved();
}

void MenuSystem::handleExit() {
	// The rows are children of a node on its way out; nothing to tear down beyond dropping our own
	// references to them, which keeps a removed menu from holding its items alive.
	System::handleExit();
}

void MenuSystem::setSource(MenuSource *source) {
	if (_source == source) {
		return;
	}

	if (_source && _owner) {
		_source->removeObserver(this);
	}
	_source = source;
	if (_source && _owner) {
		_source->addObserver(this);
	}

	_itemsDirty = true;
	if (_owner) {
		_owner->markLayoutChildrenDirty();
	}
}

void MenuSystem::setMenuStyle(const MenuStyle &style) {
	if (_style == style) {
		return;
	}
	_style = style;
	if (_owner) {
		_owner->markLayoutChildrenDirty();
	}
}

void MenuSystem::setActivateCallback(ActivateCallback &&cb) { _activateCallback = sp::move(cb); }

void MenuSystem::setWillActivateCallback(ActivateCallback &&cb) {
	_willActivateCallback = sp::move(cb);
}

void MenuSystem::setSubmenuHandler(SubmenuHandler &&handler) {
	_submenuHandler = sp::move(handler);
}

void MenuSystem::setKeyboardEnabled(bool value) {
	if (_keyboardEnabled == value) {
		return;
	}
	_keyboardEnabled = value;
	if (!_owner) {
		return; // handleAdded will build it
	}
	if (value) {
		enableKeyboard();
	} else {
		disableKeyboard();
	}
}

void MenuSystem::enableKeyboard() {
	if (_focus || !_owner) {
		return;
	}

	/* The group first, then the listener: a listener records the nearest group it finds on the
	frame stack as it registers, so one added before the group would come up unaffiliated - and an
	unaffiliated key listener is exactly the bug this whole arrangement exists to avoid.

	Exclusive: the menu that is up owns the keyboard, and the dispatcher re-collects the receivers
	scoped to this group. Propagate: a MenuSourceCustom row may carry a focus group of its own, and
	a search field inside a menu must still be typeable. */
	_focus = _owner->addSystem(Rc<FocusGroup>::create());
	_focus->setEventMask(FocusGroup::EventMask(EventMaskKeyboard));
	_focus->setFlags(FocusGroup::Flags::Exclusive | FocusGroup::Flags::Propagate);

	InputKeyMask keys;
	keys.set(toInt(InputKeyCode::UP));
	keys.set(toInt(InputKeyCode::DOWN));
	keys.set(toInt(InputKeyCode::LEFT));
	keys.set(toInt(InputKeyCode::RIGHT));
	keys.set(toInt(InputKeyCode::HOME));
	keys.set(toInt(InputKeyCode::END));
	keys.set(toInt(InputKeyCode::ENTER));
	keys.set(toInt(InputKeyCode::KP_ENTER));
	keys.set(toInt(InputKeyCode::SPACE));
	keys.set(toInt(InputKeyCode::ESCAPE));

	_keyListener = _owner->addSystem(Rc<InputListener>::create());
	_keyListener->addKeyRecognizer([this](const GestureData &data) { return handleKey(data); },
			InputKeyInfo{sp::move(keys)});

	/* A key event carries the pointer location - the backends fill it in from the last mouse
	position - so the default filter, "is this node under the pointer", would deliver the arrows
	only while the mouse happens to hover the menu. A menu that owns the keyboard owns it wherever
	the pointer is. Same reasoning, and the same seam, as ui::TextInput's. */
	_keyListener->setTouchFilter(
			[](const InputEvent &event, const InputListener::DefaultEventFilter &cb) {
		if (event.data.isKeyEvent()) {
			return true;
		}
		return cb(event);
	});
}

void MenuSystem::disableKeyboard() {
	if (_owner) {
		if (_keyListener) {
			_owner->removeSystem(_keyListener);
		}
		if (_focus) {
			_owner->removeSystem(_focus);
		}
	}
	_keyListener = nullptr;
	_focus = nullptr;

	// The highlight is the keyboard's cursor; with no keyboard there is nothing for it to mean.
	setHighlighted(nullptr);
}

bool MenuSystem::handleKey(const GestureData &data) {
	if (!_keyboardEnabled || !data.input) {
		return false;
	}

	const auto &ev = data.input->data;
	if (ev.event != InputEventName::KeyPressed && ev.event != InputEventName::KeyRepeated) {
		return false;
	}

	switch (ev.key.keycode) {
	case InputKeyCode::UP: return moveHighlight(-1);
	case InputKeyCode::DOWN: return moveHighlight(1);
	case InputKeyCode::HOME: return highlightEdge(false);
	case InputKeyCode::END: return highlightEdge(true);

	case InputKeyCode::ENTER:
	case InputKeyCode::KP_ENTER:
	case InputKeyCode::SPACE: return activateHighlighted();

	case InputKeyCode::RIGHT: {
		// The same navigation a click on a submenu row performs, and through the same handler.
		auto button = (_highlighted && _highlighted->getType() == MenuSourceItem::Type::Button)
				? static_cast<MenuSourceButton *>(_highlighted.get())
				: nullptr;
		if (button && button->hasSubmenu() && _submenuHandler) {
			if (auto node = getNodeForItem(button)) {
				return _submenuHandler(button, node);
			}
		}
		return false;
	}

	case InputKeyCode::LEFT:
		// One level, not the chain: Left in a submenu goes back to the menu that opened it.
		if (auto chain = MenuPopupChain::findForNode(_owner)) {
			if (auto parent = chain->getParent()) {
				parent->dismissChild();
				return true;
			}
		}
		return false;

	case InputKeyCode::ESCAPE:
		/* Only a menu that IS a surface can be closed by Escape. An inline menu has nothing to take
		down, and must not eat the key from whoever put it there. */
		if (auto chain = MenuPopupChain::findForNode(_owner)) {
			chain->dismissChain();
			return true;
		}
		return false;

	default: break;
	}
	return false;
}

bool MenuSystem::isSelectable(const Row &row) const {
	return row.item && row.item->getType() == MenuSourceItem::Type::Button && row.item->isVisible()
			&& row.item->isEnabled();
}

int32_t MenuSystem::indexOfHighlighted() const {
	if (!_highlighted) {
		return -1;
	}
	for (uint32_t i = 0; i < uint32_t(_rows.size()); ++i) {
		if (_rows[i].item == _highlighted) {
			return int32_t(i);
		}
	}
	return -1;
}

void MenuSystem::setHighlighted(MenuSourceItem *item) {
	if (_highlighted == item) {
		return;
	}
	_highlighted = item;
	updateHighlightClasses();
}

void MenuSystem::updateHighlightClasses() {
	for (auto &row : _rows) {
		if (!row.node) {
			continue;
		}
		if (row.item == _highlighted) {
			row.node->addStyleClass("highlighted");
		} else {
			row.node->removeStyleClass("highlighted");
		}
	}
}

bool MenuSystem::moveHighlight(int32_t delta) {
	const int32_t count = int32_t(_rows.size());
	if (count == 0 || delta == 0) {
		return false;
	}

	const int32_t step = delta > 0 ? 1 : -1;
	int32_t index = indexOfHighlighted();
	if (index < 0) {
		// Nothing is on yet: Down starts above the first row, Up below the last one.
		index = step > 0 ? -1 : count;
	}

	for (int32_t i = 0; i < count; ++i) {
		index += step;
		if (index < 0) {
			index = count - 1;
		} else if (index >= count) {
			index = 0;
		}
		if (isSelectable(_rows[index])) {
			setHighlighted(_rows[index].item);
			return true;
		}
	}
	return false;
}

bool MenuSystem::highlightEdge(bool last) {
	if (_rows.empty()) {
		return false;
	}
	if (last) {
		for (uint32_t i = uint32_t(_rows.size()); i > 0; --i) {
			if (isSelectable(_rows[i - 1])) {
				setHighlighted(_rows[i - 1].item);
				return true;
			}
		}
	} else {
		for (auto &row : _rows) {
			if (isSelectable(row)) {
				setHighlighted(row.item);
				return true;
			}
		}
	}
	return false;
}

bool MenuSystem::activateHighlighted() {
	if (!_highlighted) {
		return false;
	}
	for (auto &row : _rows) {
		if (row.item == _highlighted) {
			if (!isSelectable(row)) {
				return false;
			}
			handleItemActivated(_highlighted);
			return true;
		}
	}
	return false;
}

void MenuSystem::handleSourceDirty(MenuSource *source) {
	if (source != _source) {
		return;
	}
	_itemsDirty = true;
	if (_owner) {
		// Directly, not through a scheduled check: an application that renders on demand owes the
		// frame loop a reason to run, and "the menu changed" is one.
		_owner->markLayoutChildrenDirty();
	}
}

bool MenuSystem::handleMeasure(const MeasureConstraints &constraints, Size2 &result) {
	if (!_source || !_owner) {
		return false;
	}

	auto metrics =
			measure(getFontController(), _source, _style, constraints, _owner->getInputDensity());
	if (metrics.size.width <= 0.0f) {
		return false;
	}

	result = metrics.size;
	return true;
}

void MenuSystem::handleLayoutChildren() {
	if (_itemsDirty) {
		rebuild();
	}
	apply();
}

Node *MenuSystem::getNodeForItem(NotNull<MenuSourceItem> item) const {
	for (auto &it : _rows) {
		if (it.item == item) {
			return it.node;
		}
	}
	return nullptr;
}

MenuSourceItem *MenuSystem::getItemForNode(NotNull<Node> node) const {
	for (auto &it : _rows) {
		if (it.node == node) {
			return it.item;
		}
	}
	return nullptr;
}

void MenuSystem::handleItemActivated(NotNull<MenuSourceItem> item) {
	auto button = (item->getType() == MenuSourceItem::Type::Button)
			? static_cast<MenuSourceButton *>(item.get())
			: nullptr;

	// A row that opens a submenu is not a command: it navigates. Nothing is reported, and the
	// item's own callback - which most submenu rows do not have - is not run.
	if (button && button->hasSubmenu() && _submenuHandler) {
		if (auto node = getNodeForItem(item)) {
			if (_submenuHandler(button, node)) {
				return;
			}
		}
	}

	// Before the command: this is where a popup takes its surface down, so that an action opening
	// another surface does not leave the menu standing behind it.
	if (_willActivateCallback) {
		_willActivateCallback(item);
	}

	if (button) {
		if (auto &cb = button->getCallback()) {
			cb(button);
		}
	}

	// After the item's own callback: what the application hears is "this was chosen", once the
	// command it stands for has already run.
	if (_activateCallback) {
		_activateCallback(item);
	}
}

void MenuSystem::handleItemHovered(NotNull<MenuSourceItem> item) {
	// Only while the keyboard is in play: without it there is no highlight to move, and `:hover`
	// alone is what a mouse-driven menu has always shown.
	if (!_keyboardEnabled) {
		return;
	}
	for (auto &row : _rows) {
		if (row.item == item) {
			if (isSelectable(row)) {
				setHighlighted(item);
			}
			return;
		}
	}
}

font::FontController *MenuSystem::getFontController() const {
	auto director = _owner ? _owner->getDirector() : nullptr;
	auto app = director ? director->getApplication() : nullptr;
	return app ? app->getExtension<font::FontController>() : nullptr;
}

void MenuSystem::rebuild() {
	if (!_owner) {
		return;
	}

	_itemsDirty = false;

	Vector<Row> rows;
	if (_source) {
		rows.reserve(_source->countVisible());

		for (auto &it : _source->getItems()) {
			if (!it->isVisible()) {
				continue;
			}

			// Reuse is keyed on item IDENTITY. A name may be empty (separators) or repeated, and
			// rebuilding a row that is still there would drop its hover state and flicker under an
			// open menu.
			Rc<Node> node;
			for (auto &row : _rows) {
				if (row.item == it) {
					node = row.node;
					break;
				}
			}

			if (node) {
				if (auto menuItem = dynamic_cast<MenuItem *>(node.get())) {
					menuItem->updateFromSource();
				}
			} else {
				node = makeNode(it.get());
			}

			if (node) {
				rows.emplace_back(Row{it, sp::move(node)});
			}
		}
	}

	// Whatever is no longer in the model goes.
	for (auto &old : _rows) {
		bool kept = false;
		for (auto &row : rows) {
			if (row.node == old.node) {
				kept = true;
				break;
			}
		}
		if (!kept && old.node) {
			old.node->removeFromParent(true);
		}
	}

	_rows = sp::move(rows);

	// An item that left the model, or one that has just been disabled or hidden, cannot go on
	// carrying the keyboard.
	if (_highlighted) {
		bool live = false;
		for (auto &row : _rows) {
			if (row.item == _highlighted && isSelectable(row)) {
				live = true;
				break;
			}
		}
		if (!live) {
			_highlighted = nullptr;
		}
	}
	// The rows may be new nodes, so the class is stamped after every rebuild rather than only when
	// the highlight moves.
	updateHighlightClasses();

	// Explicit, DISTINCT z-orders: sortAllChildren is an unstable sort, so equal orders would
	// leave the order of the menu up to chance.
	ZOrder z = ZOrder(0);
	for (auto &it : _rows) {
		if (it.node->getParent() != _owner) {
			_owner->addChild(it.node, z);
		} else {
			it.node->setLocalZOrder(z);
		}
		z = ZOrder(z.get() + 1);
	}
}

void MenuSystem::apply() {
	if (!_owner || _inApply) {
		return;
	}

	auto controller = getFontController();
	if (!controller || !_source) {
		return;
	}

	_inApply = true;

	const float width = _owner->getContentSize().width;
	if (width > 0.0f) {
		// The owner already has a box - from its parent, from CSS, or from the popup surface it
		// fills - so the columns are resolved to it rather than negotiated again.
		_metrics = measureAtWidth(controller, _source, _style, width, _owner->getInputDensity());
	} else {
		_metrics = measure(controller, _source, _style, MeasureConstraints{},
				_owner->getInputDensity());
	}

	// Top down, in CSS reading order; the scene's Y grows up, so the cursor counts down from the
	// content box's top.
	float y = _owner->getContentSize().height - _style.paddingVertical;

	size_t index = 0;
	for (auto &row : _rows) {
		if (index >= _metrics.rows.size()) {
			break;
		}
		const auto &metrics = _metrics.rows[index];
		++index;

		if (auto menuItem = dynamic_cast<MenuItem *>(row.node.get())) {
			// Before the size: the row places its children from these numbers, and a size change
			// would otherwise lay it out against the previous menu's columns.
			menuItem->setRowGeometry(_style, _metrics, metrics);
		}

		const Size2 size(_metrics.size.width, metrics.height);
		row.node->setAnchorPoint(Anchor::TopLeft);
		row.node->setPosition(Vec2(0.0f, y));
		row.node->setContentSize(size);

		y -= metrics.height;
	}

	_inApply = false;
}

Rc<Node> MenuSystem::makeNode(NotNull<MenuSourceItem> item) {
	switch (item->getType()) {
	case MenuSourceItem::Type::Button:
		return Rc<MenuItem>::create(this, static_cast<MenuSourceButton *>(item.get()));
	case MenuSourceItem::Type::Separator: return Rc<MenuSeparator>::create(this, item);
	case MenuSourceItem::Type::Custom: {
		auto custom = static_cast<MenuSourceCustom *>(item.get());
		if (auto &factory = custom->getFactory()) {
			auto node = factory(this, custom);
			if (node && node->getName().empty()) {
				node->setName(item->getName());
			}
			return node;
		}
		break;
	}
	}
	return nullptr;
}

} // namespace stappler::xenolith::ui
