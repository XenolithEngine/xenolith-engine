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
#include "XLUiSelect.h" // SelectOption
#include "XLUiMenuPopup.h"
#include "XLUiControlLock.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// One member of the set. `id` is the identity reported to callbacks and forms; `title` is
// presentation and may be localized (as in ui::SelectOption).
struct SP_PUBLIC ChipItem {
	String id;
	String title;
	IconName icon = IconName::None;
	bool removable = true;
};

// An entry the "+" menu offers; ui::makeSelectOptions builds lists of these too.
using ChipOption = SelectOption;

/** A row of chips holding one list value (tags, flags, an element chain).

In a ui::FormSystem it is one field collecting one array of ids. The chips keep their own gestures,
since the form admits listeners at or below the focused field's node
(FormSystem::isWithinFocusedField).

Focus is a flag this node sets (as in ui::Select), not platform text input.

Keyboard: Left/Right/Home/End move the selection, Delete removes the selected chip. The row is one
tab stop; Shift+Tab into it selects the last chip. Backspace with nothing selected selects the last
chip, a second Backspace removes it. While the "+" menu is open, its MenuSystem owns the keyboard.

Limits: at setMaxCount(n) the "+" is disabled and addItem refuses; with setUniqueIds(true) options
already present are disabled in the menu. Duplicates are allowed by default.

Height follows the wrap: the row answers the measurement protocol with the wrapped height and the
measure phase commits it (as ui::TableView's auto height), overriding a height from CSS or the owner
while auto height is on. setIntrinsicHeightCallback reports changes.

CSS: type `chip-row`, class `xl-ui-chip-row`, states `.open`, `.full`, `:disabled`. Children are
`chip-row > chip` and `chip-row > button` (the "+", named `add`). With `display:flex;
flex-wrap:wrap` the flex pass owns placement and measurement. */
class SP_PUBLIC ChipRow : public Panel, public EditLockTarget {
public:
	// The whole row, on every accepted change.
	using ChangeCallback = Function<void(SpanView<ChipItem>)>;

	// The row took or lost the keyboard. The form adapter uses it to focus this field on a chip tap
	// (forms/ depends on input/, not the other way).
	using FocusCallback = Function<void(bool focused)>;

	// Tab navigation, as in ui::TextInput.
	using NavigateCallback = Function<bool(bool backwards)>;

	// Opens a caller's surface instead of the built-in menu; returns true if it opened something.
	using AddCallback = Function<bool(NotNull<ChipRow>)>;

	virtual ~ChipRow();

	virtual bool init() override;

	virtual void handleExit() override;
	virtual void handleContentSizeDirty() override;

	// ---- the value -------------------------------------------------------------------------

	virtual void setItems(SpanView<ChipItem>, bool silent = false);
	SpanView<ChipItem> getItems() const { return _items; }
	size_t getItemCount() const { return _items.size(); }

	/* Returns false without changes for an empty id, at the maximum count, or for a duplicate while
	ids are unique. */
	virtual bool addItem(const ChipItem &, bool silent = false);

	// Adds the declared option carrying `id`. False when no option does.
	virtual bool addById(StringView id, bool silent = false);

	virtual bool removeItem(uint32_t index, bool silent = false);
	virtual bool removeById(StringView id, bool silent = false);
	virtual void clearItems(bool silent = false);

	// Index of the first item with the id, or -1; address duplicates by index.
	int32_t indexOf(StringView id) const;

	// Named `At` so it does not hide Node's own accessors.
	Chip *getChipAt(uint32_t) const;

	// ---- what the "+" offers ------------------------------------------------------------------

	virtual void setOptions(SpanView<ChipOption>);
	SpanView<ChipOption> getOptions() const { return _options; }
	const ChipOption *getOption(StringView id) const;

	// Takes precedence over the built-in menu. With neither, the "+" is hidden.
	virtual void setAddCallback(AddCallback &&);

	// ---- declared limits ----------------------------------------------------------------------

	virtual void setMaxCount(uint32_t); // 0 - no limit
	uint32_t getMaxCount() const { return _maxCount; }
	bool isFull() const;

	// Off by default; enable for sets such as flags.
	virtual void setUniqueIds(bool);
	bool isUniqueIds() const { return _unique; }

	virtual void setWrapEnabled(bool);
	bool isWrapEnabled() const { return _wrap; }

	virtual void setEnabled(bool) override;
	bool isEnabled() const override { return isControlEnabled(this); }

	// ---- the height the wrap asks for -----------------------------------------------------------

	/* Report the wrapped height through the measurement protocol and let the measure phase commit
	it. On by default; while on, a height from CSS or the owner is replaced. */
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

	/* Enter the row from navigation: selects the last chip when backwards, the first otherwise.
	No-op when the row already has focus (a tap already chose the selection). */
	virtual void focusFromNavigation(bool backwards);

	// ---- the "+" ------------------------------------------------------------------------------

	// False when the row is disabled, full, already open, has nothing to offer, or has no window.
	virtual bool open();
	virtual void close();
	bool isOpen() const { return _popup != nullptr; }
	SubWindow *getPopup() const { return _popup; }

	virtual void setMenuStyle(const MenuStyle &);
	const MenuStyle &getMenuStyle() const { return _menuStyle; }

	/* The template the menu is opened with. The stylesheet is optional: ui::openPopupSurface passes
	the sheet in force at the opener. Callbacks and placement are filled in by open(). */
	virtual void setPopupConfig(MenuConfig &&);
	const MenuConfig &getPopupConfig() const { return _popupConfig; }

	virtual void setChangeCallback(ChangeCallback &&);
	virtual void setFocusCallback(FocusCallback &&);
	virtual void setNavigateCallback(NavigateCallback &&);

	Button *getAddButton() const { return _addButton; }

protected:
	using Panel::init;

	// Chip nodes are rebuilt from the model whenever it changes.
	virtual void rebuildChips();

	virtual void updateSelection();
	virtual void updateAddButton();
	virtual void updateInteractiveState();
	virtual void notifyChange();

	/* Walks the chips and the "+" in order, breaking a line when the next one does not fit, and
	returns the total height. With `commit` it also writes positions and sizes; measurement and
	placement share it so the reported height matches the drawn one. */
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

	// Last height reported to the listener; NaN until the first report.
	float _reportedHeight = nan();
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_INPUT_XLUICHIPROW_H_
