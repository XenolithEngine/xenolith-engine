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

#ifndef XENOLITH_RENDERER_UI_MENU_XLUIMENUSYSTEM_H_
#define XENOLITH_RENDERER_UI_MENU_XLUIMENUSYSTEM_H_

#include "XLUiMenuSource.h"
#include "XLSystem.h"
#include "XLFocusGroup.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

class MenuItem;

/** Builds and places the nodes of a ui::MenuSource on the node it is attached to.

	auto menu = node->addSystem(Rc<ui::MenuSystem>::create(source));
	menu->setActivateCallback([](NotNull<ui::MenuSourceItem> item) { run(item->getName()); });

Not popup-specific: a popup is this system on a ui::SubWindow's content (see XLUiMenuPopup.h), and
it works equally on a panel or a scrolled node.

It owns its children's geometry: the owner must not carry a ui::LayoutSystem and is marked with
SystemManagedLayout. Colours, fonts, corners and backgrounds come from CSS.

Rows are placed arithmetically from the metrics, not by nested flex: a flex column measures its
items' main axis with MaxContent, which for a Label means no wrapping, so wrapped rows would come
out too short.

App-thread only. */
class SP_PUBLIC MenuSystem : public System {
public:
	static uint64_t Id;

	// Same band as LayoutSystem/DockSystem: after styling, before anything that reads geometry.
	static constexpr uint32_t MenuDefaultPriority = System::DefaultPriority - 100;

	// Fired after the menu has acted on the item. See setActivateCallback.
	using ActivateCallback = Function<void(NotNull<MenuSourceItem>)>;

	/* Opens `item`'s submenu anchored on the row's node; return true if it did. Opening is
	navigation, so the item's callback does not run and no activation is reported. Unset, a submenu
	row acts as a command. Must be idempotent for the already open item (MenuPopupChain::openSubmenu
	is). */
	using SubmenuHandler = Function<bool(NotNull<MenuSourceButton>, NotNull<Node>)>;

	/* Closes this menu's open submenu because the pointer moved to another row; may be called with
	nothing open. */
	using SubmenuCloseHandler = Function<void()>;

	// The nearest MenuSystem at or above `node`; there is no acquireForNode.
	static MenuSystem *findForNode(Node *);

	/* Resolve a menu's geometry without building anything: the popup extent (needed before any node
	exists), the fit-content answer for an inline menu, and each row's wrapped height. `density`
	must be the density labels are shaped at; measureForNode reads it off the node. */
	static MenuMetrics measure(font::FontController *, NotNull<MenuSource>, const MenuStyle &,
			const MeasureConstraints &, float density = 1.0f);

	/* measure() for an already decided width (an inline menu sized by its parent, or a popup at the
	extent it was given); shares the row pass with measure(). A too narrow width yields a zero text
	column. */
	static MenuMetrics measureAtWidth(font::FontController *, NotNull<MenuSource>,
			const MenuStyle &, float width, float density = 1.0f);

	// measure() with the font controller and the density taken from the node's director.
	static MenuMetrics measureForNode(NotNull<Node>, NotNull<MenuSource>, const MenuStyle &,
			const MeasureConstraints &);

	virtual ~MenuSystem();

	virtual bool init() override;
	virtual bool init(MenuSource *);
	virtual bool init(MenuSource *, const MenuStyle &);

	virtual void handleAdded(Node *) override;
	virtual void handleRemoved() override;
	virtual void handleExit() override;

	// Report the menu's natural size, so an inline menu works as a fit-content item.
	virtual bool handleMeasure(const MeasureConstraints &, Size2 &result) override;

	// Rebuild (if the model changed) and place the rows. Own size and child order are fixed here.
	virtual void handleLayoutChildren() override;

	virtual void setSource(MenuSource *);
	MenuSource *getSource() const { return _source; }

	virtual void setMenuStyle(const MenuStyle &);
	const MenuStyle &getMenuStyle() const { return _style; }

	// The geometry the last layout pass resolved. Empty before the first one.
	const MenuMetrics &getMetrics() const { return _metrics; }

	/* Fires after the item's callback has run and the menu has decided whether to stay open, so a
	popup wrapper can close and report in one place. */
	virtual void setActivateCallback(ActivateCallback &&);
	const ActivateCallback &getActivateCallback() const { return _activateCallback; }

	/* Runs before the item's callback; a popup wrapper closes its surface here, so an action that
	opens another surface does not leave the menu behind it. */
	virtual void setWillActivateCallback(ActivateCallback &&);

	/* Called when the pointer enters this menu anywhere, including separators, padding and gaps
	between rows. A chain level uses it to cancel the close its parent armed when the pointer left
	the opener row. */
	using PointerEnterHandler = Function<void()>;
	virtual void setPointerEnterHandler(PointerEnterHandler &&);

	virtual void setSubmenuHandler(SubmenuHandler &&);
	virtual void setSubmenuCloseHandler(SubmenuCloseHandler &&);

	/* Cancel a pending submenu open or close; safe with nothing armed. Called by a chain level on
	the levels above it (MenuPopupChain::handlePointerEntered). */
	virtual void cancelSubmenuDelay();

	// --- the pointer ----------------------------------------------------------------------------

	/* Hover behaviour; see MenuHoverConfig. A popup passes it down the chain (MenuConfig::hover).
	*/
	virtual void setHoverConfig(const MenuHoverConfig &);
	const MenuHoverConfig &getHoverConfig() const { return _hover; }

	// --- the keyboard ---------------------------------------------------------------------------

	/* Make this menu the keyboard owner: a FocusGroup and a key listener on the owner node. Off by
	default. The group is Exclusive, so while enabled all keys in the window go to this menu and
	unused ones are swallowed; openMenu enables it for popups, inline menus enable it on demand.
	Flags::Propagate lets a MenuSourceCustom row with its own focus group receive keys. */
	virtual void setKeyboardEnabled(bool);
	bool isKeyboardEnabled() const { return _keyboardEnabled; }

	// The group, or null while the keyboard is off.
	FocusGroup *getFocusGroup() const { return _focus; }

	/* The row the keyboard is on, marked with the `highlighted` style class. Hovering a row moves
	it, so there is one current row. */
	virtual void setHighlighted(MenuSourceItem *);
	MenuSourceItem *getHighlighted() const { return _highlighted; }

	// Step the highlight by `delta` rows, skipping separators, custom rows and disabled items, and
	// wrapping at the ends. With nothing highlighted, a positive delta starts at the top.
	virtual bool moveHighlight(int32_t delta);

	// First / last row that can be highlighted at all.
	virtual bool highlightEdge(bool last);

	virtual bool activateHighlighted();

	// The node built for an item, or null when the item is hidden or the menu has not been built.
	Node *getNodeForItem(NotNull<MenuSourceItem>) const;

	// The item a node was built for, or null.
	MenuSourceItem *getItemForNode(NotNull<Node>) const;

	// Called by MenuSource when anything in it changed.
	void handleSourceDirty(MenuSource *);

	// Called by a row when the user picked it. Runs the item's callback, then the activate
	// callback.
	virtual void handleItemActivated(NotNull<MenuSourceItem>);

	/* Called by a row on pointer enter. The highlight follows only while the keyboard is enabled.
	Independently, a submenu row arms the open timer and any other row arms the close timer; arming
	replaces whatever was armed. */
	virtual void handleItemHovered(NotNull<MenuSourceItem>);

protected:
	struct Row {
		Rc<MenuSourceItem> item;
		Rc<Node> node;
	};

	// Action tag of the pending submenu timer, so only it is cancelled.
	static constexpr uint32_t SubmenuDelayActionTag = "XLUiMenuSubmenu"_tag;

	// The group is added before the listener, which takes the nearest group from the frame stack
	// when it registers.
	void enableKeyboard();
	void disableKeyboard();

	/* Arm a submenu timer: `item` to open, null to close. Cancels the previous one; a zero delay
	fires immediately. */
	void armSubmenu(MenuSourceButton *item, TimeInterval delay);
	void fireSubmenu();

	// Listener for the pointer over the whole menu; built only when setPointerEnterHandler is set.
	void enablePointerListener();
	void disablePointerListener();

	bool handleKey(const GestureData &);

	// a visible, enabled command; separators and custom rows are not selectable
	bool isSelectable(const Row &) const;

	// -1 when nothing is highlighted or the highlighted item is no longer a row.
	int32_t indexOfHighlighted() const;

	// The only writer of the `highlighted` style class.
	void updateHighlightClasses();

	// Bring _rows in line with the model, reusing nodes keyed by item identity (names may be empty
	// or shared); recreating a row under an open menu would drop hover and flicker.
	void rebuild();

	// Place the rows from the metrics. The only thing here that writes a node's geometry.
	void apply();

	Rc<Node> makeNode(NotNull<MenuSourceItem>);

	font::FontController *getFontController() const;

	Rc<MenuSource> _source;
	MenuStyle _style;
	MenuMetrics _metrics;
	Vector<Row> _rows;
	ActivateCallback _activateCallback;
	ActivateCallback _willActivateCallback;
	SubmenuHandler _submenuHandler;
	SubmenuCloseHandler _submenuCloseHandler;
	MenuHoverConfig _hover;

	/* Target of the armed timer: the row to open, or null to close. Not a record of what is open;
	the chain holds that, and a cache here would go stale when a level closes by other means. */
	Rc<MenuSourceButton> _pendingSubmenu;
	bool _submenuArmed = false;

	bool _itemsDirty = true;

	// Guards against our own commits re-arming the pass through child content-size events.
	bool _inApply = false;

	// owned by the owner node, like its other systems
	FocusGroup *_focus = nullptr;
	InputListener *_keyListener = nullptr;

	PointerEnterHandler _pointerEnterHandler;
	InputListener *_pointerListener = nullptr;
	Rc<MenuSourceItem> _highlighted;
	bool _keyboardEnabled = false;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_MENU_XLUIMENUSYSTEM_H_
