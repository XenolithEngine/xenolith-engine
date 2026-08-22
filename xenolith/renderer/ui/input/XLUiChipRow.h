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

#ifndef XENOLITH_RENDERER_UI_INPUT_XLUICHIPROW_H_
#define XENOLITH_RENDERER_UI_INPUT_XLUICHIPROW_H_

#include "XLUiChip.h"
#include "XLUiSelect.h" // SelectOption: what "one option, as data" already means in this kit
#include "XLUiMenuPopup.h"
#include "XLUiControlLock.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// One member of the set, as DATA. `id` is the identity - what the callback reports, what a form
// collects, what a test drives the row by - and `title` is presentation, which may be localized out
// from under you. Exactly ui::SelectOption's split, for exactly its reason.
struct SP_PUBLIC ChipItem {
	String id;
	String title;
	IconName icon = IconName::None;
	bool removable = true;
};

// What the "+" offers. One option, as data, is already ui::SelectOption; a second identical type
// would only give the two somewhere to drift apart.
//
// Because it is the same type, ui::makeSelectOptions builds a list for this widget too - the
// id==title case, which is what a set of tag names or element names already is.
using ChipOption = SelectOption;

/** Several values that are one value: the row of chips.

An element chain, a set of flags, a list of tags, the filters of an explorer. What makes it a widget
rather than a habit of putting badges beside each other is that everything outside it treats it as
ONE thing: one form field, one value, one stop in the tab ring.

ONE FORM FIELD, NOT N. Inside a ui::FormSystem this collects ONE array of ids under one name, is
validated once and refused once. The chips keep their own gestures underneath - the form never sees
them - because ui::FormSystem admits a listener whose owner IS the focused field's node or sits
BELOW it (FormSystem::isWithinFocusedField). That one rule is what makes composition possible with
no new machinery.

ITS FOCUS IS ITS OWN, and that is why this is simpler than ui::VectorField. A row of number fields
holds the keyboard through the platform's text input, so focus arrives late, by echo, and the widget
has to remember what it ASKED for. Here focus is a flag this node sets - the same model as
ui::Select - so there is no window in which nobody is focused and nothing to reconcile.

THE KEYBOARD IS SPLIT BY PLACE, like ui::Select's. Inside the row, Left/Right/Home/End move the
SELECTION and Delete takes the selected chip off. Tab is not navigation inside the row: the row is
one stop, so Tab leaves it, and a Shift+Tab that ENTERS it selects the LAST chip - that is what
FormFieldSlots::setFocused's `backwards` argument is for. While the "+" menu is up, its own
MenuSystem owns the keyboard in its own window and this node answers nothing.

Backspace with nothing selected SELECTS the last chip instead of removing it. Removal always has a
visible target: the second Backspace does take it off, so the two-key sequence still reads the way
it does in a field of tags, without ever deleting something the user could not see.

LIMITS ARE DECLARED, NOT DISCOVERED. At setMaxCount(n) the "+" is disabled and opens nothing, and
addItem refuses; with setUniqueIds(true) an option already in the row comes up disabled in the menu.
An interface that lets you press a thing and then refuses is lying about what is possible. Duplicates
are ALLOWED by default, because a row is a list: an element chain of Array<Array<Int>> is the chips
Array, Array, Int, and a set of flags is the case that has to ask for uniqueness.

HEIGHT FOLLOWS THE WRAP. Chips wrap to as many lines as they need, so the height is a function of
the width and of the model, and the row answers the content measurement protocol with it - the same
contract as ui::TableView's auto height. The measure phase COMMITS that answer, which is the point:
a row that wraps and is not resized clips. A height declared by CSS or by an owner is therefore
overridden while auto height is on, and setAutoHeight(false) is how a fixed box asks to keep its
own. setIntrinsicHeightCallback reports the changes to an owner that has to place the row itself.

CSS: type `chip-row`, class `xl-ui-chip-row`, states `.open`, `.full`, `.disabled`. Children are
`chip-row > chip` and `chip-row > button` (the "+", named `add`). A styled row lays its chips out
with `display:flex; flex-wrap:wrap`, and then the flex pass owns both the placement and the
measurement - this widget's own arithmetic steps aside, as every other widget's does. */
class SP_PUBLIC ChipRow : public Panel, public EditLockTarget {
public:
	// The whole row, on every accepted change. A consumer holds a set, so that is what it hears.
	using ChangeCallback = Function<void(SpanView<ChipItem>)>;

	// The row took or lost the keyboard. The FORM ADAPTER listens here: a tap that selects a chip
	// has to make the form focus this FIELD, or the form goes on filtering keys to whatever it
	// focused last. The widget cannot do that itself - forms/ knows about input/, never the other
	// way round.
	using FocusCallback = Function<void(bool focused)>;

	// Tab. Exactly ui::TextInput's seam, for exactly its reason.
	using NavigateCallback = Function<bool(bool backwards)>;

	// A surface of the caller's own instead of the built-in menu - a search palette for a list too
	// long to be a menu. Return true for "I opened something".
	using AddCallback = Function<bool(NotNull<ChipRow>)>;

	virtual ~ChipRow();

	virtual bool init() override;

	virtual void handleExit() override;
	virtual void handleContentSizeDirty() override;

	// ---- the value -------------------------------------------------------------------------

	virtual void setItems(SpanView<ChipItem>, bool silent = false);
	SpanView<ChipItem> getItems() const { return _items; }
	size_t getItemCount() const { return _items.size(); }

	/* False and NOTHING changes: an empty id (a member the collected value could not name), the
	declared maximum, or a duplicate while ids are unique. */
	virtual bool addItem(const ChipItem &, bool silent = false);

	// Adds the declared option carrying `id`. False when no option does.
	virtual bool addById(StringView id, bool silent = false);

	virtual bool removeItem(uint32_t index, bool silent = false);
	virtual bool removeById(StringView id, bool silent = false);
	virtual void clearItems(bool silent = false);

	// -1 when nothing carries that id. The FIRST, which is what duplicates make ambiguous - address
	// a repeated id by index.
	int32_t indexOf(StringView id) const;

	// `At` because a Node ALREADY has getComponent<T>()-style accessors of its own, and the same
	// lesson was learned in ui::VectorField: a member named like the node system's hides it.
	Chip *getChipAt(uint32_t) const;

	// ---- what the "+" offers ------------------------------------------------------------------

	virtual void setOptions(SpanView<ChipOption>);
	SpanView<ChipOption> getOptions() const { return _options; }
	const ChipOption *getOption(StringView id) const;

	// Takes precedence over the built-in menu. With neither one there is no "+" at all: a button
	// that opens nothing is worse than no button.
	virtual void setAddCallback(AddCallback &&);

	// ---- declared limits ----------------------------------------------------------------------

	virtual void setMaxCount(uint32_t); // 0 - no limit
	uint32_t getMaxCount() const { return _maxCount; }
	bool isFull() const;

	// Off by default: a row is a list. On for a set of flags, where the same member twice is not a
	// value anyone means.
	virtual void setUniqueIds(bool);
	bool isUniqueIds() const { return _unique; }

	virtual void setWrapEnabled(bool);
	bool isWrapEnabled() const { return _wrap; }

	virtual void setEnabled(bool);
	bool isEnabled() const override { return isControlEnabled(this); }

	// ---- the height the wrap asks for -----------------------------------------------------------

	/* Report the wrapped height through the measurement protocol, and let the measure phase commit
	it. On by default: a row that wraps and is not resized is a row that clips.

	The consequence is in the class comment: while this is on, a height from a stylesheet or from an
	owner is replaced by the wrapped one. Turn it off for a row that must keep the box it was
	given. */
	virtual void setAutoHeight(bool);
	bool isAutoHeight() const { return _autoHeight; }

	// The height the current model needs at the current width, and at any width.
	float getIntrinsicHeight() const;
	float measureHeight(float width) const;

	// How many lines the last placement produced. 0 before the first one.
	uint32_t getLineCount() const { return _lineCount; }

	// Fires when the model or the width changed the answer getIntrinsicHeight() gives.
	virtual void setIntrinsicHeightCallback(Function<void(float)> &&);

	// ---- selection, focus, navigation -------------------------------------------------------

	// The chip Delete would take off, or -1.
	int32_t getSelected() const { return _selected; }
	virtual void select(int32_t index);

	virtual void focus();
	virtual void blur();
	bool isFocused() const { return _focused; }

	/* Enter the row from a navigation: the LAST chip when it came backwards, the first otherwise -
	and nothing at all when the row already holds the keyboard, because then a tap has already
	decided what is selected and the form is only catching up. */
	virtual void focusFromNavigation(bool backwards);

	// ---- the "+" ------------------------------------------------------------------------------

	// False when the row is disabled, full, already open, has nothing to offer, or has no window.
	virtual bool open();
	virtual void close();
	bool isOpen() const { return _popup != nullptr; }
	SubWindow *getPopup() const { return _popup; }

	virtual void setMenuStyle(const MenuStyle &);
	const MenuStyle &getMenuStyle() const { return _menuStyle; }

	/* The template the menu is opened with. DECLARING THE STYLESHEET IS NOT OPTIONAL for a styled
	application: a native popup is a scene of its own and the application's sheet does not reach it.
	The callbacks and the placement are filled in by open(). */
	virtual void setPopupConfig(MenuConfig &&);
	const MenuConfig &getPopupConfig() const { return _popupConfig; }

	virtual void setChangeCallback(ChangeCallback &&);
	virtual void setFocusCallback(FocusCallback &&);
	virtual void setNavigateCallback(NavigateCallback &&);

	Button *getAddButton() const { return _addButton; }

protected:
	using Panel::init;

	// Chip nodes are rebuilt from the model whenever it changes: the model is the vector, and a
	// node list kept in step with it by hand would be a second copy of the same order.
	virtual void rebuildChips();

	virtual void updateSelection();
	virtual void updateAddButton();
	virtual void updateInteractiveState();
	virtual void notifyChange();

	/* The wrap, in one place. Walks the chips and the "+" in order, breaks a line when the next one
	would not fit, and returns the total height. With `commit` it also writes the positions and the
	sizes; without it, it is the measurement. Two callers, one arithmetic - which is the only way
	the height a row reports can be the height it draws at. */
	float layoutRow(float width, bool commit);

	// Recomputes the reported height and tells the listener when the answer moved.
	virtual void updateIntrinsicHeight();

	Rc<MenuSource> makeSource();

	bool handleKey(const GestureData &);
	bool handleChipTap(uint32_t index);
	bool handleChipRemove(uint32_t index);

	AppWindow *getAppWindow() const;

	Vector<ChipItem> _items;
	Vector<ChipOption> _options;
	Vector<Chip *> _chips;

	Button *_addButton = nullptr;
	InputListener *_listener = nullptr;
	InputListener *_focusListener = nullptr;

	MenuStyle _menuStyle;
	MenuConfig _popupConfig;
	Rc<SubWindow> _popup;

	ChangeCallback _changeCallback;
	FocusCallback _focusCallback;
	NavigateCallback _navigateCallback;
	AddCallback _addCallback;
	Function<void(float)> _intrinsicHeightCallback;

	uint32_t _maxCount = 0;
	uint32_t _lineCount = 0;
	int32_t _selected = -1;

	bool _unique = false;
	bool _wrap = true;
	bool _autoHeight = true;
	bool _focused = false;

	// Edge trackers for InteractiveComponent's cumulative counters.
	bool _hoverApplied = false;
	bool _focusApplied = false;

	// What the listener was last told, so an unchanged answer is not reported twice. NaN until the
	// first report - the same guard ui::TableView uses.
	float _reportedHeight = nan();
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_INPUT_XLUICHIPROW_H_
