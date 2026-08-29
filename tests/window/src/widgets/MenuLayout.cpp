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

#include "widgets/MenuLayout.h"
#include "XLUiMenuItem.h"
#include "XLUiStyleResolver.h"
#include "XLAppWindow.h"
#include "XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

static constexpr auto s_menuCss = StringView(R"css(
menu {
	background-color: #202026;
	border-radius: 6px;
	outline-color: #3d3d3d;
	outline-width: 1px;
}
menu-item {
	background-color: #00000000;
}
menu-item:hover {
	background-color: #2f2f38;
}
menu-item:checked {
	background-color: #2a2a44;
}
menu-item.highlighted {
	background-color: #3a3a5c;
}
menu-item > label {
	color: #e8e8e8;
	font-size: 14px;
}
menu-item-subtitle {
	color: #9a9aa4;
}
menu-item-shortcut {
	color: #9a9aa4;
}
menu-separator {
	background-color: #3d3d3d;
}
button {
	width: 180px;
	height: 36px;
	background-color: #3a3a3a;
	outline-color: #5a5a5a;
	outline-width: 1px;
	border-radius: 6px;
}
button > label {
	color: #e8e8e8;
	font-size: 15px;
}
text-input {
	width: 220px;
	height: 34px;
	background-color: #292929;
	outline-width: 1px;
	outline-color: rgba(255,255,255,.15);
	border-radius: 6px;
	padding: 0 10px;
	color: #e8e8e8;
	font-size: 14px;
}
)css");

Value ackValue(bool ok) {
	Value ret;
	ret.setBool(ok, "ok");
	return ret;
}

Value encodeRect(Node *node) {
	Value ret;
	if (!node) {
		return ret;
	}
	auto pos = node->getPosition();
	auto size = node->getContentSize();
	ret.setDouble(double(pos.x), "x");
	ret.setDouble(double(pos.y), "y");
	ret.setDouble(double(size.width), "width");
	ret.setDouble(double(size.height), "height");
	return ret;
}

} // namespace

bool MenuLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_menuCss);
	addSystem(Rc<ui::StyleResolver>::create(true));

	buildSource();

	// The inline menu. A plain Panel with a MenuSystem on it - which is the whole of "a menu that is
	// not a popup", and the same object the popup puts inside its surface.
	_menuPanel = addChild(Rc<ui::Panel>::create(), ZOrder(1));
	_menuPanel->setName("inline-menu");
	_menuPanel->setType("menu");
	_menuPanel->setAnchorPoint(Anchor::TopLeft);

	ui::MenuStyle style;
	// Pinned, so that where the long title wraps is a property of the test rather than of the host's
	// font: the width negotiation is exercised by the popup, which is not pinned.
	style.minWidth = _menuWidth;
	style.maxWidth = _menuWidth;

	_menu = _menuPanel->addSystem(Rc<ui::MenuSystem>::create(_source, style));
	_menu->setActivateCallback([this](NotNull<ui::MenuSourceItem> item) {
		++_activations;
		_lastActivated = item->getName().str<Interface>();
		_activationLog.emplace_back(_lastActivated);
	});

	/* An inline menu has no chain to open a surface in, so its handlers only WRITE DOWN what was
	asked of them. That is the point: what the pointer decides is a property of MenuSystem, and here
	it is readable with no window in play at all - which is also the only place the disabled seam
	(`hover.openSubmenu = false`) can be told apart from a menu that simply has nowhere to open. */
	_menu->setSubmenuHandler([this](NotNull<ui::MenuSourceButton> item, NotNull<Node>) -> bool {
		_activationLog.emplace_back(toString("submenu-open:", item->getName()));
		return true;
	});
	_menu->setSubmenuCloseHandler([this] { _activationLog.emplace_back("submenu-close"); });
	_menu->setHoverConfig(_hover);

	_openButton = addChild(TestLayout::makeButton("Open popup",
								   [this] {
		if (_popup) {
			_popup->dismiss();
			_popup = nullptr;
			return;
		}
		openPopup();
	}),
			ZOrder(2));
	_openButton->setName("open-popup");

	/* Somebody else in this window who wants the arrows. It is what makes "the menu owns the
	keyboard" a checkable statement rather than a claim: with the inline menu's keyboard off the
	arrows move this caret, and with it on they must not. */
	_neighbour = addChild(Rc<ui::TextInput>::create(), ZOrder(2));
	_neighbour->setName("neighbour");
	_neighbour->setText("abcdef");
	_neighbour->setCaretBlink(false);

	return true;
}

bool MenuLayout::openPopup() {
	if (_popup || !_openButton) {
		return _popup != nullptr;
	}

	auto window = getAppWindow();
	if (!window) {
		return false;
	}

	ui::MenuConfig config;
	config.idPrefix = String("menu-test");
	config.title = String("Menu test");
	// A native popup is a scene of its own: the layout's sheet does not reach it, so the same CSS
	// travels with the menu.
	config.stylesheetSource = s_menuCss.str<Interface>();
	// Carried down the whole chain by MenuPopupChain, so every level answers the pointer alike.
	config.hover = _hover;
	config.onActivate = [this](NotNull<ui::MenuSourceItem> item) {
		++_activations;
		_lastActivated = item->getName().str<Interface>();
		_activationLog.emplace_back(_lastActivated);
	};
	config.onClose = [this] { _popup = nullptr; };

	_popup = ui::openMenuForNode(window, _openButton, _source, sp::move(config));
	return _popup != nullptr;
}

void MenuLayout::buildSource() {
	_submenu = Rc<ui::MenuSource>::create();
	_submenu->addButton("sub-one", "Submenu one", [this](NotNull<ui::MenuSourceButton> item) {
		_activationLog.emplace_back(toString("callback:", item->getName()));
	});
	_submenu->addButton("sub-two", "Submenu two", [this](NotNull<ui::MenuSourceButton> item) {
		_activationLog.emplace_back(toString("callback:", item->getName()));
	});

	/* A third level, appended AFTER the two rows the geometry checks stand on so that none of them
	moves. It is what makes "nested levels" plural: a submenu opening a submenu is the case where a
	level has both a parent and a child, and only the third one has that. */
	_deepSubmenu = Rc<ui::MenuSource>::create();
	_deepSubmenu->addButton("deep-one", "Deeper one", [this](NotNull<ui::MenuSourceButton> item) {
		_activationLog.emplace_back(toString("callback:", item->getName()));
	});
	_deepSubmenu->addButton("deep-two", "Deeper two", [this](NotNull<ui::MenuSourceButton> item) {
		_activationLog.emplace_back(toString("callback:", item->getName()));
	});
	_submenu->addSubmenu("sub-more", "Deeper still", basic2d::IconName::None,
			Rc<ui::MenuSource>(_deepSubmenu));

	_source = Rc<ui::MenuSource>::create();

	auto record = [this](NotNull<ui::MenuSourceButton> item) {
		_activationLog.emplace_back(toString("callback:", item->getName()));
	};

	_source->addButton("plain", "Plain command", record);

	_source->addButton("with-icon", "With an icon", basic2d::IconName::Content_save_solid, record);

	auto sub = _source->addButton("with-subtitle", "With a subtitle", record);
	sub->setSubtitle("The second line, wrapped into the same column");

	auto save = _source->addButton("with-hotkey", "Save", basic2d::IconName::Content_save_solid,
			record);
	// Declared here, which is the point of the seam: the menu is where the command is written down.
	save->setHotkey("org.stappler.xenolith.tests.menu.save", "Ctrl+S", "Save the document");

	// Long enough that it cannot fit the pinned width on any reasonable font: the row must grow.
	_source
			->addButton("long-title",
					"A command with a title long enough that it has to wrap onto several lines "
					"inside the " "text column",
					record);

	_source->addSeparator("separator");

	auto toggle = _source->addButton("toggle", "A toggle that keeps the menu open",
			[this](NotNull<ui::MenuSourceButton> item) {
		item->setChecked(!item->isChecked());
		_activationLog.emplace_back(toString("callback:", item->getName()));
	});
	toggle->setKeepOpen(true);

	_source->addCustom(
			[this](NotNull<ui::MenuSystem>, NotNull<ui::MenuSourceCustom> item) -> Rc<Node> {
		++_customBuilds;
		auto node = Rc<ui::Panel>::create();
		node->setType("menu-custom");
		node->setPathColor(Color4B(0x30, 0x30, 0x50, 0xFF), false);
		return node;
	}, Size2(200.0f, 28.0f), "custom");

	auto disabled = _source->addButton("disabled", "A disabled command", record);
	disabled->setEnabled(false);

	auto hidden = _source->addButton("hidden", "A hidden command", record);
	hidden->setVisible(false);

	_source->addSubmenu("submenu", "Open a submenu", basic2d::IconName::None,
			Rc<ui::MenuSource>(_submenu));
}

void MenuLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	if (_menuPanel) {
		_menuPanel->setPosition(Vec2(40.0f, getWorkTop() - 20.0f));
		updateInlineMenu();
	}

	if (_openButton) {
		_openButton->setAnchorPoint(Anchor::TopLeft);
		_openButton->setPosition(Vec2(40.0f + _menuWidth + 40.0f, getWorkTop() - 20.0f));
	}

	if (_neighbour) {
		_neighbour->setAnchorPoint(Anchor::TopLeft);
		_neighbour->setPosition(Vec2(40.0f + _menuWidth + 40.0f, getWorkTop() - 80.0f));
	}
}

void MenuLayout::updateInlineMenu() {
	if (!_menuPanel) {
		return;
	}
	// Through the measurement protocol rather than by assigning a size: this is exactly what a
	// fit-content ancestor does to an inline menu, so the test exercises that path too.
	_menuPanel->markMeasureDirty();
	_menuPanel->markLayoutChildrenDirty();
}

AppWindow *MenuLayout::getAppWindow() const {
	return _director ? dynamic_cast<AppWindow *>(_director->getRenderServer()) : nullptr;
}

ui::MenuSourceItem *MenuLayout::getItem(const Value &args) const {
	return _source ? _source->getItem(args.getString("item")) : nullptr;
}

ui::MenuPopupChain *MenuLayout::getChainForLevel(int64_t level) const {
	if (level < 1 || !_popup) {
		return nullptr;
	}

	// getPanel() answers on both materializations, which is what makes this work in headless (where
	// a popup is a real window) and in an overlay build alike.
	auto chain = ui::MenuPopupChain::findForNode(_popup->getPanel());
	for (int64_t i = 1; chain && i < level; ++i) {
		auto child = chain->getChild();
		chain = child ? ui::MenuPopupChain::findForNode(child->getPanel()) : nullptr;
	}
	return chain;
}

ui::MenuSystem *MenuLayout::getMenuForLevel(int64_t level) const {
	if (level <= 0) {
		return _menu;
	}
	auto chain = getChainForLevel(level);
	auto surface = chain ? chain->getSurface() : nullptr;
	return surface ? ui::MenuSystem::findForNode(surface->getPanel()) : nullptr;
}

void MenuLayout::applyHoverConfig() {
	if (_menu) {
		_menu->setHoverConfig(_hover);
	}
	// Every level that is already up. A level opened later takes it from MenuConfig::hover, which
	// openPopup fills in from the same field.
	for (int64_t level = 1; auto menu = getMenuForLevel(level); ++level) {
		menu->setHoverConfig(_hover);
		if (level > 8) {
			break; // a chain this deep is a bug, not a menu
		}
	}
}

Value MenuLayout::encodeMetrics() const {
	Value ret;
	if (!_menu) {
		return ret;
	}

	auto &metrics = _menu->getMetrics();
	ret.setDouble(double(metrics.size.width), "width");
	ret.setDouble(double(metrics.size.height), "height");
	ret.setDouble(double(metrics.leadingColumn), "leadingColumn");
	ret.setDouble(double(metrics.textColumn), "textColumn");
	ret.setDouble(double(metrics.shortcutColumn), "shortcutColumn");
	ret.setDouble(double(metrics.trailingColumn), "trailingColumn");

	Value rows;
	for (auto &it : metrics.rows) {
		Value row;
		row.setString(it.item ? it.item->getName() : StringView(), "item");
		row.setDouble(double(it.height), "height");
		row.setDouble(double(it.titleHeight), "titleHeight");
		row.setDouble(double(it.subtitleHeight), "subtitleHeight");
		rows.addValue(sp::move(row));
	}
	ret.setValue(sp::move(rows), "rows");

	ret.setValue(encodeRect(_menuPanel), "panel");
	return ret;
}

Value MenuLayout::encodeState() const {
	Value ret;
	ret.setInteger(int64_t(_activations), "activations");
	ret.setString(_lastActivated, "lastActivated");
	ret.setInteger(int64_t(_customBuilds), "customBuilds");
	ret.setBool(_popup != nullptr, "popupOpen");
	ret.setBool(_menu && _menu->isKeyboardEnabled(), "keyboard");
	if (_menu) {
		auto highlighted = _menu->getHighlighted();
		ret.setString(highlighted ? highlighted->getName() : StringView(), "highlighted");
	}
	if (_neighbour) {
		ret.setString(_neighbour->getText(), "neighbourText");
		ret.setInteger(int64_t(_neighbour->getCursor().start), "neighbourCursor");
		ret.setBool(_neighbour->isFocused(), "neighbourFocused");
	}
	if (_popup) {
		ret.setBool(_popup->isNative(), "popupNative");
		ret.setString(_popup->getId(), "popupId");
	}

	Value log;
	for (auto &it : _activationLog) { log.addString(it); }
	ret.setValue(sp::move(log), "log");

	Value items;
	if (_source) {
		for (auto &it : _source->getItems()) {
			Value item;
			item.setString(it->getName(), "name");
			item.setBool(it->isVisible(), "visible");
			item.setBool(it->isEnabled(), "enabled");
			item.setBool(it->isChecked(), "checked");
			item.setBool(it->isKeepOpen(), "keepOpen");

			switch (it->getType()) {
			case ui::MenuSourceItem::Type::Button: {
				auto button = static_cast<ui::MenuSourceButton *>(it.get());
				item.setString("button", "type");
				item.setString(button->getTitle(), "title");
				item.setString(button->getSubtitle(), "subtitle");
				item.setBool(button->hasSubmenu(), "submenu");
				if (button->hasShortcut()) {
					StringStream shortcut;
					button->encodeShortcut([&](StringView str) { shortcut << str; });
					item.setString(shortcut.str(), "shortcut");
				}
				break;
			}
			case ui::MenuSourceItem::Type::Custom: item.setString("custom", "type"); break;
			case ui::MenuSourceItem::Type::Separator: item.setString("separator", "type"); break;
			}

			if (_menu) {
				if (auto node = _menu->getNodeForItem(it.get())) {
					item.setValue(encodeRect(node), "node");
				}
			}

			items.addValue(sp::move(item));
		}
	}
	ret.setValue(sp::move(items), "items");
	return ret;
}

void MenuLayout::registerCommands() {
	addCommand("metrics", "Report the resolved menu geometry: columns, size and every row height",
			[this](Value &&) { return encodeMetrics(); });

	addCommand("state", "Report the model, the built rows and what has been activated",
			[this](Value &&) { return encodeState(); });

	addCommand("activate", "Activate an item by name: {item}", [this](Value &&args) {
		auto item = getItem(args);
		if (item && _menu) {
			_menu->handleItemActivated(item);
		}
		return ackValue(item != nullptr);
	});

	/* THE POINTER, reported through the very call ui::MenuItem makes on the entering edge of a real
	hover. It cannot be an injected MouseMove: the mouse-over recognizer gates on
	WindowState::Pointer, and headless has no way to put the pointer over a row. So what this drives
	is everything ABOVE that call - which is all of what this change is. */
	addCommand("hover", "Report a pointer entering a row: {item, level} (0 inline, 1 popup, 2 its "
					 "submenu, ...)",
			[this](Value &&args) {
		const Value &a = args;
		auto menu = getMenuForLevel(a.getInteger("level"));
		auto source = menu ? menu->getSource() : nullptr;
		auto item = source ? source->getItem(a.getString("item")) : nullptr;
		if (menu && item) {
			menu->handleItemHovered(item);
			return ackValue(true);
		}
		return ackValue(false);
	});

	addCommand("hover-config",
			"What a hover does, at every level: {enabled, open, close} - delays in milliseconds",
			[this](Value &&args) {
		const Value &a = args;
		if (a.isBool("enabled")) {
			_hover.openSubmenu = a.getBool("enabled");
		}
		if (a.isInteger("open")) {
			_hover.openDelay = TimeInterval::milliseconds(uint64_t(a.getInteger("open")));
		}
		if (a.isInteger("close")) {
			_hover.closeDelay = TimeInterval::milliseconds(uint64_t(a.getInteger("close")));
		}
		applyHoverConfig();
		return ackValue(true);
	});

	addCommand("chain",
			"The open menu chain: one entry per level, with the row its child hangs off",
			[this](Value &&) {
		Value ret;
		Value levels;
		if (_popup) {
			for (int64_t level = 1; auto chain = getChainForLevel(level); ++level) {
				auto surface = chain->getSurface();
				Value entry;
				entry.setInteger(level, "level");
				entry.setString(surface ? surface->getId() : StringView(), "id");
				// Which row THIS level has a child open for - null when it is the last one.
				auto item = chain->getChildItem();
				entry.setString(item ? item->getName() : StringView(), "child");
				levels.addValue(sp::move(entry));
				if (level > 8) {
					break;
				}
			}
		}
		ret.setInteger(int64_t(levels.size()), "depth");
		ret.setValue(sp::move(levels), "levels");
		return ret;
	});

	addCommand("set-checked", "Toggle an item: {item, value}", [this](Value &&args) {
		auto item = getItem(args);
		if (item) {
			item->setChecked(static_cast<const Value &>(args).getBool("value"));
			updateInlineMenu();
		}
		return ackValue(item != nullptr);
	});

	addCommand("set-enabled", "Enable or disable an item: {item, value}", [this](Value &&args) {
		auto item = getItem(args);
		if (item) {
			item->setEnabled(static_cast<const Value &>(args).getBool("value"));
			updateInlineMenu();
		}
		return ackValue(item != nullptr);
	});

	addCommand("set-visible", "Show or hide an item: {item, value}", [this](Value &&args) {
		auto item = getItem(args);
		if (item) {
			item->setVisible(static_cast<const Value &>(args).getBool("value"));
			updateInlineMenu();
		}
		return ackValue(item != nullptr);
	});

	addCommand("set-title", "Replace an item's title: {item, value}", [this](Value &&args) {
		auto item = getItem(args);
		if (item && item->getType() == ui::MenuSourceItem::Type::Button) {
			static_cast<ui::MenuSourceButton *>(item)->setTitle(
					static_cast<const Value &>(args).getString("value"));
			updateInlineMenu();
			return ackValue(true);
		}
		return ackValue(false);
	});

	addCommand("set-width", "Pin the inline menu to a width: {value}", [this](Value &&args) {
		const auto value = float(static_cast<const Value &>(args).getDouble("value"));
		if (value <= 0.0f || !_menu) {
			return ackValue(false);
		}
		_menuWidth = value;
		auto style = _menu->getMenuStyle();
		style.minWidth = value;
		style.maxWidth = value;
		_menu->setMenuStyle(style);
		updateInlineMenu();
		return ackValue(true);
	});

	addCommand("open", "Open the popup form of the same menu", [this](Value &&) {
		openPopup();
		return ackValue(_popup != nullptr);
	});

	addCommand("close", "Dismiss the popup and everything it opened", [this](Value &&) {
		if (_popup) {
			_popup->dismiss();
			_popup = nullptr;
			return ackValue(true);
		}
		return ackValue(false);
	});

	addCommand("keyboard", "Give the inline menu the keyboard, or take it away: {value}",
			[this](Value &&args) {
		if (!_menu) {
			return ackValue(false);
		}
		_menu->setKeyboardEnabled(static_cast<const Value &>(args).getBool("value"));
		return ackValue(true);
	});

	addCommand("highlight", "Move the inline menu's highlight: {item} by name, or {delta} by step",
			[this](Value &&args) {
		if (!_menu) {
			return ackValue(false);
		}
		const Value &a = args;
		if (a.isInteger("delta")) {
			return ackValue(_menu->moveHighlight(int32_t(a.getInteger("delta"))));
		}
		_menu->setHighlighted(getItem(a));
		return ackValue(_menu->getHighlighted() != nullptr);
	});

	addCommand("focus-neighbour", "Put the caret in the field beside the menu", [this](Value &&) {
		if (_neighbour) {
			_neighbour->focus();
			return ackValue(true);
		}
		return ackValue(false);
	});

	addCommand("reset-counters", "Zero the activation log and the build counter", [this](Value &&) {
		_activations = 0;
		_customBuilds = 0;
		_lastActivated.clear();
		_activationLog.clear();
		return ackValue(true);
	});
}

} // namespace stappler::xenolith::app
