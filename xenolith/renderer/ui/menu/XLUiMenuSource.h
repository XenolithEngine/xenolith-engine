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

#ifndef XENOLITH_RENDERER_UI_MENU_XLUIMENUSOURCE_H_
#define XENOLITH_RENDERER_UI_MENU_XLUIMENUSOURCE_H_

#include "XLUiMenuTypes.h"
#include "XLHotkey.h"
#include "XL2dIconSprite.h"

#include "SPSubscription.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

class MenuSource;
class MenuSystem;

/** One entry of a menu, as data, independent of any nodes built for it.

A Subscription, so external consumers can use DataListener<MenuSourceItem>. ui::MenuSystem is
notified directly through MenuSource::setDirty instead, so an on-demand renderer repaints without
waiting for a scheduled check.

App-thread only. */
class SP_PUBLIC MenuSourceItem : public Subscription {
public:
	enum class Type {
		Separator,
		Button,
		Custom,
	};

	virtual ~MenuSourceItem() = default;

	virtual bool init();

	virtual Rc<MenuSourceItem> copy() const;

	Type getType() const { return _type; }

	/* Stable identity: the node's name (CSS `#id`), what the activation callback reports and what
	tests drive; the title may be localized. Not required to be unique, but getItem() and node reuse
	pick the first duplicate. */
	virtual void setName(StringView);
	StringView getName() const { return _name; }

	virtual void setFlags(MenuItemFlags);
	MenuItemFlags getFlags() const { return _flags; }

	virtual void setEnabled(bool);
	bool isEnabled() const { return !hasFlag(_flags, MenuItemFlags::Disabled); }

	virtual void setChecked(bool);
	bool isChecked() const { return hasFlag(_flags, MenuItemFlags::Checked); }

	virtual void setVisible(bool);
	bool isVisible() const { return !hasFlag(_flags, MenuItemFlags::Hidden); }

	virtual void setKeepOpen(bool);
	bool isKeepOpen() const { return hasFlag(_flags, MenuItemFlags::KeepOpen); }

	// Free-form payload for the application. Ignored by the kit.
	virtual void setData(Value &&);
	const Value &getData() const { return _data; }

	/* Hides Subscription::setDirty: an item change is also its source's change. Call
	Subscription::setDirty explicitly for the flags-only form. */
	void setDirty(Flags flags = Initial);

	// Set by MenuSource when the item is added, cleared when it is removed. Non-owning: the source
	// owns the item, never the other way round.
	MenuSource *getSource() const { return _source; }

protected:
	friend class MenuSource;

	// Shared tail of every copy(): copies what MenuSourceItem itself owns, and nothing else.
	void copyTo(MenuSourceItem *) const;

	Type _type = Type::Separator;
	String _name;
	MenuItemFlags _flags = MenuItemFlags::None;
	Value _data;
	MenuSource *_source = nullptr;
};

/** A command: two texts, two icons, an accelerator and either a callback or a submenu.

Title, subtitle, leading and trailing icon are independent slots; an absent one takes no column. */
class SP_PUBLIC MenuSourceButton : public MenuSourceItem {
public:
	using ActionCallback = Function<void(NotNull<MenuSourceButton>)>;

	/* Builds the submenu the first time it is opened, for contents that depend on later state
	(recent files, devices). Runs once per item; the result is cached. */
	using SubmenuFactory = Function<Rc<MenuSource>(NotNull<MenuSourceButton>)>;

	virtual ~MenuSourceButton();

	virtual bool init() override;
	virtual bool init(StringView name, StringView title, ActionCallback &&);
	virtual bool init(StringView name, StringView title, IconName, ActionCallback &&);

	virtual Rc<MenuSourceItem> copy() const override;

	virtual void setTitle(StringView);
	StringView getTitle() const { return _title; }

	// The second line, under the title. Wrapped like the title and measured into the same column.
	virtual void setSubtitle(StringView);
	StringView getSubtitle() const { return _subtitle; }

	virtual void setLeadingIcon(IconName);
	IconName getLeadingIcon() const { return _leadingIcon; }

	virtual void setTrailingIcon(IconName);
	IconName getTrailingIcon() const { return _trailingIcon; }

	// The registered hotkey this item is the visible face of.
	virtual void setHotkey(HotkeyId);
	HotkeyId getHotkey() const { return _hotkey; }

	/* Register the combination and set the id in one step. Idempotent by name, like
	HotkeyRegistry::add. */
	virtual HotkeyId setHotkey(StringView name, StringView combo,
			StringView description = StringView(), HotkeyOptions = HotkeyOptions::None);

	/* Override the accelerator text. HotkeyCombo::encode prints the engine spelling (`Mod3+S`,
	`LEFT`), which suits keymap files but not, e.g., macOS menus. Does not change the binding. */
	virtual void setShortcutText(StringView);
	StringView getShortcutText() const { return _shortcutText; }

	// True when there is anything to draw in the accelerator column.
	bool hasShortcut() const;

	// Streams the accelerator label: the override if there is one, otherwise the registry's
	// spelling of the bound combination. Emits nothing when there is neither.
	void encodeShortcut(const Callback<void(StringView)> &) const;

	virtual void setCallback(ActionCallback &&);
	const ActionCallback &getCallback() const { return _callback; }

	virtual void setSubmenu(Rc<MenuSource> &&);
	virtual void setSubmenu(SubmenuFactory &&);

	// The submenu, running the factory on first use. Null when this item has none.
	MenuSource *getSubmenu();

	// The submenu only if already built; does not run a lazy factory.
	MenuSource *getBuiltSubmenu() const { return _submenu; }

	bool hasSubmenu() const { return _submenu || _submenuFactory != nullptr; }

protected:
	String _title;
	String _subtitle;
	String _shortcutText;
	IconName _leadingIcon = IconName::None;
	IconName _trailingIcon = IconName::None;
	HotkeyId _hotkey = HotkeyId(0);
	ActionCallback _callback;
	Rc<MenuSource> _submenu;
	SubmenuFactory _submenuFactory;
};

/** An arbitrary node in the menu, built by a factory.

Measurement must work without the factory, since the menu size is settled before any node exists
(a popup needs its Extent2 up front). */
class SP_PUBLIC MenuSourceCustom : public MenuSourceItem {
public:
	// Handed the system that is building the row, so one factory can serve several menus.
	using FactoryFunction = Function<Rc<Node>(NotNull<MenuSystem>, NotNull<MenuSourceCustom>)>;

	/* MaxContent asks for the preferred width; Normal with a bounded maxWidth asks for the height
	at that width, the same contract as Label. */
	using MeasureFunction = Function<Size2(NotNull<MenuSourceCustom>, const MeasureConstraints &)>;

	virtual ~MenuSourceCustom() = default;

	virtual bool init() override;
	virtual bool init(FactoryFunction &&, MeasureFunction &&);

	// A node of a fixed size; both dimensions are kept by copy().
	virtual bool init(FactoryFunction &&, Size2);

	virtual Rc<MenuSourceItem> copy() const override;

	Size2 measure(const MeasureConstraints &) const;

	const FactoryFunction &getFactory() const { return _factory; }

protected:
	FactoryFunction _factory;
	MeasureFunction _measure;
	Size2 _fixedSize;
};

/** An ordered list of items: the menu, as data.

Outlives the nodes built from it. Mutating it marks it dirty; every ui::MenuSystem showing it
rebuilds on the next visit.

App-thread only. */
class SP_PUBLIC MenuSource : public Subscription {
public:
	virtual ~MenuSource();

	virtual bool init() { return true; }

	MenuSourceItem *addItem(Rc<MenuSourceItem> &&);
	MenuSourceItem *insertItem(size_t index, Rc<MenuSourceItem> &&);

	MenuSourceButton *addButton(StringView name, StringView title,
			MenuSourceButton::ActionCallback && = nullptr);
	MenuSourceButton *addButton(StringView name, StringView title, IconName,
			MenuSourceButton::ActionCallback && = nullptr);

	MenuSourceButton *addSubmenu(StringView name, StringView title, Rc<MenuSource> &&);
	MenuSourceButton *addSubmenu(StringView name, StringView title, IconName, Rc<MenuSource> &&);
	MenuSourceButton *addSubmenu(StringView name, StringView title, IconName,
			MenuSourceButton::SubmenuFactory &&);

	MenuSourceCustom *addCustom(MenuSourceCustom::FactoryFunction &&,
			MenuSourceCustom::MeasureFunction &&, StringView name = StringView());
	MenuSourceCustom *addCustom(MenuSourceCustom::FactoryFunction &&, Size2,
			StringView name = StringView());

	MenuSourceItem *addSeparator(StringView name = StringView());

	bool removeItem(MenuSourceItem *);
	bool removeItem(StringView name);

	MenuSourceItem *getItem(StringView name) const;

	SpanView<Rc<MenuSourceItem>> getItems() const { return _items; }
	size_t count() const { return _items.size(); }

	// Visible items only, in source order - what a consumer actually builds.
	size_t countVisible() const;

	void clear();

	// A deep copy; no state is shared. An unbuilt lazy submenu is copied as its factory, a built
	// one as a copy of the source.
	Rc<MenuSource> copy() const;

	// See MenuSourceItem::setDirty.
	void setDirty(Flags flags = Initial);

	// Systems watching this source; non-owning both ways, a system unregisters in handleExit.
	void addObserver(NotNull<MenuSystem>);
	void removeObserver(NotNull<MenuSystem>);

protected:
	Vector<Rc<MenuSourceItem>> _items;
	Vector<MenuSystem *> _observers;
};

/** Subscribe `listener` to every hotkey the menu declares. Returns the number of subscriptions.

A subscription runs the item's callback and consumes the key; a disabled item declines, so the
combination continues down the dispatcher's walk. `recursive` descends only into submenus already
built: a lazy SubmenuFactory is not run, so commands behind one must be bound where declared. */
SP_PUBLIC size_t bindMenuHotkeys(NotNull<InputListener>, NotNull<MenuSource>,
		HotkeyFlags = HotkeyFlags::None, bool recursive = true);

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_MENU_XLUIMENUSOURCE_H_
