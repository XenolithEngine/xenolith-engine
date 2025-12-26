/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "MaterialMenuSource.h"
#include "XLNode.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

bool MenuSourceItem::init() {
	return true;
}

Rc<MenuSourceItem> MenuSourceItem::copy() const {
	auto ret = Rc<MenuSourceItem>::create();
	ret->setCustomData(_customData);
	return ret;
}

void MenuSourceItem::setCustomData(const Value &val) {
	_customData = val;
	setDirty();
}

void MenuSourceItem::setCustomData(Value &&val) {
	_customData = sp::move(val);
	setDirty();
}

const Value &MenuSourceItem::getCustomData() const {
	return _customData;
}

MenuSourceItem * MenuSourceItem::setAttachCallback(const AttachCallback &cb) {
	_attachCallback = cb;
	return this;
}

MenuSourceItem * MenuSourceItem::setDetachCallback(const AttachCallback &cb) {
	_detachCallback = cb;
	return this;
}

MenuSourceItem::Type MenuSourceItem::getType() const {
	return _type;
}

void MenuSourceItem::handleNodeAttached(Node *n) {
	if (_attachCallback) {
		_attachCallback(this, n);
	}
}

void MenuSourceItem::handleNodeDetached(Node *n) {
	if (_detachCallback) {
		_detachCallback(this, n);
	}
}

void MenuSourceItem::setDirty() {
	Subscription::setDirty();
}

MenuSourceButton::~MenuSourceButton() { }

bool MenuSourceButton::init(StringView str, IconName name, Callback &&cb) {
	if (!init()) {
		return false;
	}

	_name = str.str<Interface>();
	_nameIcon = name;
	_callback = sp::move(cb);
	return true;
}

bool MenuSourceButton::init(StringView str, IconName name, Rc<MenuSource> &&menu) {
	if (!init()) {
		return false;
	}

	_name = str.str<Interface>();
	_nameIcon = name;
	_nextMenu = move(menu);
	return true;
}

bool MenuSourceButton::init() {
	if (!MenuSourceItem::init()) {
		return false;
	}

	_type = Type::Button;
	return true;
}

Rc<MenuSourceItem> MenuSourceButton::copy() const {
	auto ret = Rc<MenuSourceButton>::create();
	ret->setName(_name);
	ret->setNameIcon(_nameIcon);
	ret->setValue(_name);
	ret->setValueIcon(_nameIcon);
	ret->setSelected(_selected);
	ret->setNextMenu(_nextMenu);
	ret->setCallback(Callback(_callback));
	ret->setCustomData(_customData);
	return ret;
}

void MenuSourceButton::setName(StringView val) {
	if (_name != val) {
		_name = val.str<Interface>();
		setDirty();
	}
}
const String & MenuSourceButton::getName() const {
	return _name;
}

void MenuSourceButton::setValue(StringView val) {
	if (_value != val) {
		_value = val.str<Interface>();
		setDirty();
	}
}
const String & MenuSourceButton::getValue() const {
	return _value;
}

void MenuSourceButton::setNameIcon(IconName icon) {
	if (_nameIcon != icon) {
		_nameIcon = icon;
		setDirty();
	}
}
IconName MenuSourceButton::getNameIcon() const {
	return _nameIcon;
}

void MenuSourceButton::setValueIcon(IconName icon) {
	if (_valueIcon != icon) {
		_valueIcon = icon;
		setDirty();
	}
}
IconName MenuSourceButton::getValueIcon() const {
	return _valueIcon;
}

void MenuSourceButton::setCallback(Callback &&cb) {
	_callback = sp::move(cb);
	setDirty();
}
const MenuSourceButton::Callback & MenuSourceButton::getCallback() const {
	return _callback;
}

void MenuSourceButton::setNextMenu(MenuSource *menu) {
	if (menu != _nextMenu) {
		_nextMenu = menu;
		setDirty();
	}
}
MenuSource * MenuSourceButton::getNextMenu() const {
	return _nextMenu ? _nextMenu.get() : nullptr;
}

void MenuSourceButton::setSelected(bool value) {
	if (_selected != value) {
		_selected = value;
		setDirty();
	}
}
bool MenuSourceButton::isSelected() const {
	return _selected;
}

bool MenuSourceCustom::init() {
	if (!MenuSourceItem::init()) {
		return false;
	}

	_type = Type::Custom;
	return true;
}

bool MenuSourceCustom::init(float h, const FactoryFunction &func, float minWidth) {
	return init([h] (Node *, float w) { return h; }, func);
}

bool MenuSourceCustom::init(const HeightFunction &h, const FactoryFunction &func, float minWidth) {
	if (!init()) {
		return false;
	}

	_minWidth = minWidth;
	_heightFunction = h;
	_function = func;
	return true;
}

Rc<MenuSourceItem> MenuSourceCustom::copy() const {
	auto ret = Rc<MenuSourceCustom>::create(_heightFunction, _function);
	ret->setCustomData(_customData);
	return ret;
}

float MenuSourceCustom::getMinWidth() const {
	return _minWidth;
}

float MenuSourceCustom::getHeight(Node *n, float w) const {
	return _heightFunction(n, w);
}

const MenuSourceCustom::HeightFunction & MenuSourceCustom::getHeightFunction() const {
	return _heightFunction;
}

const MenuSourceCustom::FactoryFunction & MenuSourceCustom::getFactoryFunction() const {
	return _function;
}

MenuSource::~MenuSource() { }

void MenuSource::setHintCount(size_t h) {
	_hintCount = h;
}

size_t MenuSource::getHintCount() const {
	return _hintCount;
}

Rc<MenuSource> MenuSource::copy() const {
	auto ret = Rc<MenuSource>::create();
	for (auto &it : _items) {
		ret->addItem(it->copy());
	}
	ret->setHintCount(_hintCount);
	return ret;
}

void MenuSource::addItem(MenuSourceItem *item) {
	if (item) {
		_items.emplace_back(item);
		setDirty();
	}
}

MenuSourceButton *MenuSource::addButton(StringView str, Callback &&cb) {
	auto item = Rc<MenuSourceButton>::create(str, IconName::None, sp::move(cb));
	addItem(item);
	return item;
}

MenuSourceButton *MenuSource::addButton(StringView str, IconName name, Callback &&cb) {
	auto item = Rc<MenuSourceButton>::create(str, name, sp::move(cb));
	addItem(item);
	return item;
}

MenuSourceButton *MenuSource::addButton(StringView str, IconName name, Rc<MenuSource> &&source) {
	auto item = Rc<MenuSourceButton>::create(str, name, move(source));
	addItem(item);
	return item;
}

MenuSourceCustom *MenuSource::addCustom(float h, const MenuSourceCustom::FactoryFunction &func, float w) {
	auto item = Rc<MenuSourceCustom>::create(h, func, w);
	addItem(item);
	return item;
}

MenuSourceCustom *MenuSource::addCustom(const HeightFunction &h, const FactoryFunction &func, float w) {
	auto item = Rc<MenuSourceCustom>::create(h, func, w);
	addItem(item);
	return item;
}

MenuSourceItem *MenuSource::addSeparator() {
	auto item = Rc<MenuSourceItem>::create();
	addItem(item);
	return item;
}

void MenuSource::clear() {
	_items.clear();
	setDirty();
}

uint32_t MenuSource::count() {
	return (uint32_t)_items.size();
}

const Vector<Rc<MenuSourceItem>> &MenuSource::getItems() const {
	return _items;
}

}
