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

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

class MenuItem;

/** Builds and places the nodes of a ui::MenuSource on the node it is attached to.

	auto menu = node->addSystem(Rc<ui::MenuSystem>::create(source));
	menu->setActivateCallback([](NotNull<ui::MenuSourceItem> item) { run(item->getName()); });

This is the whole consumer: a popup is this system on the content of a ui::SubWindow (see
XLUiMenuPopup.h), a menu bar drop-down is this system on a panel, a sidebar list is this system on
a scrolled node. Nothing about it is popup-specific.

IT OWNS ITS CHILDREN'S GEOMETRY, the way ui::DockSystem does. The owner therefore must not also
carry a ui::LayoutSystem - two systems writing every child's ContentSize is not a layout, it is a
fight - and the owner is marked with SystemManagedLayout so the style resolver keeps out of the
same business. Colours, fonts, corners and backgrounds remain entirely CSS's.

WHY THE ROWS ARE NOT FLEX CONTAINERS. A row is placed arithmetically from the metrics, not by a
nested flex pass, and that is a correctness requirement rather than an optimization: a flex column
measures its items' MAIN axis with MeasureMode::MaxContent, and for a Label max-content means "do
not wrap at all". A title stacked over a subtitle inside a column flex would therefore be measured
as one line each and the row would come out too short for the text it is about to draw. The metrics
wrap the text at the resolved column width and the row is built to the answer.

App-thread only. */
class SP_PUBLIC MenuSystem : public System {
public:
	static uint64_t Id;

	// Same band as LayoutSystem/DockSystem: after styling, before anything that reads geometry.
	static constexpr uint32_t MenuDefaultPriority = System::DefaultPriority - 100;

	// Fired after the menu has acted on the item. See setActivateCallback.
	using ActivateCallback = Function<void(NotNull<MenuSourceItem>)>;

	/* Asked to open `item`'s submenu, anchored on the row's node. Return true when it did: opening a
	submenu is navigation rather than a choice, so the item's own callback does NOT run and no
	activation is reported. Unset, a submenu row behaves like any other command. */
	using SubmenuHandler = Function<bool(NotNull<MenuSourceButton>, NotNull<Node>)>;

	// The nearest MenuSystem at or above `node`. There is no acquireForNode: a menu is something an
	// application builds deliberately, not something a widget can conjure over itself.
	static MenuSystem *findForNode(Node *);

	/* Resolve a menu's geometry without building anything.

	This is the single place a menu width or a row height is decided, and it answers three
	questions that must never disagree: how large a popup surface to ask the window system for
	(which has to be settled BEFORE any node exists), what a fit-content ancestor should be told
	about an inline menu, and how tall each row has to be once its text has wrapped.

	`density` must be the density the labels will actually be shaped at, or the measurement and the
	drawing part company on a HiDPI display - measureForNode reads it off the node. */
	static MenuMetrics measure(font::FontController *, NotNull<MenuSource>, const MenuStyle &,
			const MeasureConstraints &, float density = 1.0f);

	/* measure() for a menu whose width is already decided - an inline menu sized by its parent, or
	a popup surface being rebuilt at the extent the window system actually gave it.

	This is the half of measure() that resolves the rows, so the two can never disagree about where
	the text wraps. A width narrower than the columns need yields a zero text column, not a negative
	one. */
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

	/* Fires after the item's own callback has run and after the menu has decided whether to stay
	open - which is what lets a popup wrapper close the surface and report the choice in one place
	without every item's callback having to know it is in a popup. */
	virtual void setActivateCallback(ActivateCallback &&);
	const ActivateCallback &getActivateCallback() const { return _activateCallback; }

	/* Runs BEFORE the item's own callback, which is what a popup wrapper takes the surface down in:
	an action is free to put another surface up in its place, and a menu still on screen behind it
	is one the user then has to dismiss by hand. */
	virtual void setWillActivateCallback(ActivateCallback &&);

	virtual void setSubmenuHandler(SubmenuHandler &&);

	// The node built for an item, or null when the item is hidden or the menu has not been built.
	Node *getNodeForItem(NotNull<MenuSourceItem>) const;

	// The item a node was built for, or null.
	MenuSourceItem *getItemForNode(NotNull<Node>) const;

	// Called by MenuSource when anything in it changed.
	void handleSourceDirty(MenuSource *);

	// Called by a row when the user picked it. Runs the item's callback, then the activate
	// callback.
	virtual void handleItemActivated(NotNull<MenuSourceItem>);

protected:
	struct Row {
		Rc<MenuSourceItem> item;
		Rc<Node> node;
	};

	// Bring _rows in line with the model, reusing the node of every item that is still there. Node
	// reuse is keyed on item IDENTITY, not on the name: a name may be empty (separators) or shared,
	// and a rebuild that recreated the row under an open menu would drop hover and flicker.
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

	bool _itemsDirty = true;

	// Guards against our own commits: setContentSize on a row would otherwise come back as a child
	// content-size event and re-arm the pass we are inside.
	bool _inApply = false;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_MENU_XLUIMENUSYSTEM_H_
