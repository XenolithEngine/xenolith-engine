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

#include "XLUiMenuSource.h"
#include "XLUiMenuSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// --- MenuSourceItem ----------------------------------------------------------------------------

bool MenuSourceItem::init() { return true; }

Rc<MenuSourceItem> MenuSourceItem::copy() const {
	auto ret = Rc<MenuSourceItem>::create();
	copyTo(ret);
	return ret;
}

void MenuSourceItem::copyTo(MenuSourceItem *target) const {
	if (!target) {
		return;
	}
	target->_type = _type;
	target->_name = _name;
	target->_flags = _flags;
	target->_data = _data;
	// _source is deliberately NOT copied: a copy belongs to whichever menu adopts it.
}

void MenuSourceItem::setName(StringView value) {
	if (_name == value) {
		return;
	}
	_name = value.str<Interface>();
	setDirty();
}

void MenuSourceItem::setFlags(MenuItemFlags value) {
	if (_flags == value) {
		return;
	}
	_flags = value;
	setDirty();
}

void MenuSourceItem::setEnabled(bool value) {
	setFlags(value ? (_flags & ~MenuItemFlags::Disabled) : (_flags | MenuItemFlags::Disabled));
}

void MenuSourceItem::setChecked(bool value) {
	setFlags(value ? (_flags | MenuItemFlags::Checked) : (_flags & ~MenuItemFlags::Checked));
}

void MenuSourceItem::setVisible(bool value) {
	setFlags(value ? (_flags & ~MenuItemFlags::Hidden) : (_flags | MenuItemFlags::Hidden));
}

void MenuSourceItem::setKeepOpen(bool value) {
	setFlags(value ? (_flags | MenuItemFlags::KeepOpen) : (_flags & ~MenuItemFlags::KeepOpen));
}

void MenuSourceItem::setData(Value &&value) {
	if (_data == value) {
		return;
	}
	_data = sp::move(value);
	setDirty();
}

void MenuSourceItem::setDirty(Flags flags) {
	Subscription::setDirty(flags);
	if (_source) {
		_source->setDirty(flags);
	}
}

// --- MenuSourceButton --------------------------------------------------------------------------

MenuSourceButton::~MenuSourceButton() { }

bool MenuSourceButton::init() {
	if (!MenuSourceItem::init()) {
		return false;
	}
	_type = Type::Button;
	return true;
}

bool MenuSourceButton::init(StringView name, StringView title, ActionCallback &&cb) {
	return init(name, title, IconName::None, sp::move(cb));
}

bool MenuSourceButton::init(StringView name, StringView title, IconName icon, ActionCallback &&cb) {
	if (!init()) {
		return false;
	}
	_name = name.str<Interface>();
	_title = title.str<Interface>();
	_leadingIcon = icon;
	_callback = sp::move(cb);
	return true;
}

Rc<MenuSourceItem> MenuSourceButton::copy() const {
	auto ret = Rc<MenuSourceButton>::create();
	copyTo(ret);
	ret->_title = _title;
	ret->_subtitle = _subtitle;
	ret->_shortcutText = _shortcutText;
	ret->_leadingIcon = _leadingIcon;
	ret->_trailingIcon = _trailingIcon;
	ret->_hotkey = _hotkey;
	ret->_callback = _callback;
	// A built submenu is copied as a menu of its own, so the two items do not share state; an
	// unbuilt one is copied as its factory, so the copy stays as lazy as the original.
	if (_submenu) {
		ret->_submenu = _submenu->copy();
	}
	ret->_submenuFactory = _submenuFactory;
	return ret;
}

void MenuSourceButton::setTitle(StringView value) {
	if (_title == value) {
		return;
	}
	_title = value.str<Interface>();
	setDirty();
}

void MenuSourceButton::setSubtitle(StringView value) {
	if (_subtitle == value) {
		return;
	}
	_subtitle = value.str<Interface>();
	setDirty();
}

void MenuSourceButton::setLeadingIcon(IconName value) {
	if (_leadingIcon == value) {
		return;
	}
	_leadingIcon = value;
	setDirty();
}

void MenuSourceButton::setTrailingIcon(IconName value) {
	if (_trailingIcon == value) {
		return;
	}
	_trailingIcon = value;
	setDirty();
}

void MenuSourceButton::setHotkey(HotkeyId value) {
	if (_hotkey == value) {
		return;
	}
	_hotkey = value;
	setDirty();
}

HotkeyId MenuSourceButton::setHotkey(StringView name, StringView combo, StringView description,
		HotkeyOptions options) {
	auto id = HotkeyRegistry::getInstance()->add(name, HotkeyCombo::parse(combo), description,
			options);
	setHotkey(id);
	return id;
}

void MenuSourceButton::setShortcutText(StringView value) {
	if (_shortcutText == value) {
		return;
	}
	_shortcutText = value.str<Interface>();
	setDirty();
}

bool MenuSourceButton::hasShortcut() const {
	if (!_shortcutText.empty()) {
		return true;
	}
	return !_hotkey.empty() && HotkeyRegistry::getInstance()->getCombo(_hotkey).isValid();
}

void MenuSourceButton::encodeShortcut(const Callback<void(StringView)> &out) const {
	if (!_shortcutText.empty()) {
		out(_shortcutText);
		return;
	}
	if (_hotkey.empty()) {
		return;
	}
	auto combo = HotkeyRegistry::getInstance()->getCombo(_hotkey);
	if (combo.isValid()) {
		combo.encode(out);
	}
}

void MenuSourceButton::setCallback(ActionCallback &&cb) {
	_callback = sp::move(cb);
	// Not equality-guarded: a Function is not comparable, and a rebound command is a change even
	// when the closure looks the same.
	setDirty();
}

void MenuSourceButton::setSubmenu(Rc<MenuSource> &&value) {
	if (_submenu == value) {
		return;
	}
	_submenu = sp::move(value);
	_submenuFactory = nullptr;
	setDirty();
}

void MenuSourceButton::setSubmenu(SubmenuFactory &&factory) {
	_submenu = nullptr;
	_submenuFactory = sp::move(factory);
	setDirty();
}

MenuSource *MenuSourceButton::getSubmenu() {
	if (!_submenu && _submenuFactory) {
		_submenu = _submenuFactory(this);
		// The factory has done its job; keeping it would make a second call rebuild a menu the
		// user may have scrolled or expanded.
		_submenuFactory = nullptr;
	}
	return _submenu;
}

// --- MenuSourceCustom --------------------------------------------------------------------------

bool MenuSourceCustom::init() {
	if (!MenuSourceItem::init()) {
		return false;
	}
	_type = Type::Custom;
	return true;
}

bool MenuSourceCustom::init(FactoryFunction &&factory, MeasureFunction &&measure) {
	if (!init()) {
		return false;
	}
	_factory = sp::move(factory);
	_measure = sp::move(measure);
	return true;
}

bool MenuSourceCustom::init(FactoryFunction &&factory, Size2 size) {
	if (!init()) {
		return false;
	}
	_factory = sp::move(factory);
	_fixedSize = size;
	return true;
}

Rc<MenuSourceItem> MenuSourceCustom::copy() const {
	auto ret = Rc<MenuSourceCustom>::create();
	copyTo(ret);
	ret->_factory = _factory;
	ret->_measure = _measure;
	ret->_fixedSize = _fixedSize;
	return ret;
}

Size2 MenuSourceCustom::measure(const MeasureConstraints &constraints) const {
	if (_measure) {
		return _measure(const_cast<MenuSourceCustom *>(this), constraints);
	}
	return _fixedSize;
}

// --- MenuSource --------------------------------------------------------------------------------

MenuSource::~MenuSource() {
	for (auto &it : _items) { it->_source = nullptr; }
}

MenuSourceItem *MenuSource::addItem(Rc<MenuSourceItem> &&item) {
	return insertItem(_items.size(), sp::move(item));
}

MenuSourceItem *MenuSource::insertItem(size_t index, Rc<MenuSourceItem> &&item) {
	if (!item) {
		return nullptr;
	}
	auto ret = item.get();
	ret->_source = this;
	_items.emplace(_items.begin() + int64_t(sprt::min(index, _items.size())), sp::move(item));
	setDirty();
	return ret;
}

MenuSourceButton *MenuSource::addButton(StringView name, StringView title,
		MenuSourceButton::ActionCallback &&cb) {
	return addButton(name, title, IconName::None, sp::move(cb));
}

MenuSourceButton *MenuSource::addButton(StringView name, StringView title, IconName icon,
		MenuSourceButton::ActionCallback &&cb) {
	auto item = Rc<MenuSourceButton>::create(name, title, icon, sp::move(cb));
	addItem(item);
	return item;
}

MenuSourceButton *MenuSource::addSubmenu(StringView name, StringView title,
		Rc<MenuSource> &&source) {
	return addSubmenu(name, title, IconName::None, sp::move(source));
}

MenuSourceButton *MenuSource::addSubmenu(StringView name, StringView title, IconName icon,
		Rc<MenuSource> &&source) {
	auto item = Rc<MenuSourceButton>::create(name, title, icon, nullptr);
	item->setSubmenu(sp::move(source));
	addItem(item);
	return item;
}

MenuSourceButton *MenuSource::addSubmenu(StringView name, StringView title, IconName icon,
		MenuSourceButton::SubmenuFactory &&factory) {
	auto item = Rc<MenuSourceButton>::create(name, title, icon, nullptr);
	item->setSubmenu(sp::move(factory));
	addItem(item);
	return item;
}

MenuSourceCustom *MenuSource::addCustom(MenuSourceCustom::FactoryFunction &&factory,
		MenuSourceCustom::MeasureFunction &&measure, StringView name) {
	auto item = Rc<MenuSourceCustom>::create(sp::move(factory), sp::move(measure));
	item->setName(name);
	addItem(item);
	return item;
}

MenuSourceCustom *MenuSource::addCustom(MenuSourceCustom::FactoryFunction &&factory, Size2 size,
		StringView name) {
	auto item = Rc<MenuSourceCustom>::create(sp::move(factory), size);
	item->setName(name);
	addItem(item);
	return item;
}

MenuSourceItem *MenuSource::addSeparator(StringView name) {
	auto item = Rc<MenuSourceItem>::create();
	item->setName(name);
	return addItem(sp::move(item));
}

bool MenuSource::removeItem(MenuSourceItem *item) {
	for (auto it = _items.begin(); it != _items.end(); ++it) {
		if (it->get() == item) {
			item->_source = nullptr;
			_items.erase(it);
			setDirty();
			return true;
		}
	}
	return false;
}

bool MenuSource::removeItem(StringView name) { return removeItem(getItem(name)); }

MenuSourceItem *MenuSource::getItem(StringView name) const {
	if (name.empty()) {
		return nullptr;
	}
	for (auto &it : _items) {
		if (it->getName() == name) {
			return it.get();
		}
	}
	return nullptr;
}

size_t MenuSource::countVisible() const {
	size_t ret = 0;
	for (auto &it : _items) {
		if (it->isVisible()) {
			++ret;
		}
	}
	return ret;
}

void MenuSource::clear() {
	if (_items.empty()) {
		return;
	}
	for (auto &it : _items) { it->_source = nullptr; }
	_items.clear();
	setDirty();
}

Rc<MenuSource> MenuSource::copy() const {
	auto ret = Rc<MenuSource>::create();
	ret->_items.reserve(_items.size());
	for (auto &it : _items) { ret->addItem(it->copy()); }
	return ret;
}

void MenuSource::setDirty(Flags flags) {
	Subscription::setDirty(flags);

	// A copy, because a system may take itself off the list from inside handleSourceDirty (a
	// rebuild that drops the node the system lives on).
	auto observers = _observers;
	for (auto &it : observers) { it->handleSourceDirty(this); }
}

void MenuSource::addObserver(NotNull<MenuSystem> system) {
	for (auto &it : _observers) {
		if (it == system) {
			return;
		}
	}
	_observers.emplace_back(system);
}

void MenuSource::removeObserver(NotNull<MenuSystem> system) {
	for (auto it = _observers.begin(); it != _observers.end(); ++it) {
		if (*it == system) {
			_observers.erase(it);
			return;
		}
	}
}

// --- hotkeys -----------------------------------------------------------------------------------

size_t bindMenuHotkeys(NotNull<InputListener> listener, NotNull<MenuSource> source,
		HotkeyFlags flags, bool recursive) {
	size_t ret = 0;
	for (auto &it : source->getItems()) {
		if (it->getType() != MenuSourceItem::Type::Button) {
			continue;
		}

		auto button = static_cast<MenuSourceButton *>(it.get());
		if (auto id = button->getHotkey(); !id.empty()) {
			// Rc: the binding outlives whatever built the menu, and a subscription that dangles is
			// worse than one that keeps a few strings alive.
			listener->addHotkey(id,
					[item = Rc<MenuSourceButton>(button)](HotkeyId, const InputEvent &) -> bool {
				// A greyed-out command declines rather than swallowing the key: the same
				// combination may mean something else further along the dispatcher's walk.
				if (!item->isEnabled()) {
					return false;
				}
				if (auto &cb = item->getCallback()) {
					cb(item);
					return true;
				}
				return false;
			}, flags);
			++ret;
		}

		if (recursive) {
			// getBuiltSubmenu, not getSubmenu: binding a key must never run a lazy factory and
			// materialize a menu nobody opened.
			if (auto sub = button->getBuiltSubmenu()) {
				ret += bindMenuHotkeys(listener, sub, flags, true);
			}
		}
	}
	return ret;
}

} // namespace stappler::xenolith::ui
