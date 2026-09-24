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


#ifndef XENOLITH_RENDERER_UI_INPUT_XLUISEARCHPICKER_H_
#define XENOLITH_RENDERER_UI_INPUT_XLUISEARCHPICKER_H_

#include "XLUiPanel.h"
#include "XLUiTextInput.h"
#include "XLUiTableView.h"
#include "XLUiTreeView.h"
#include "XLUiSubWindow.h"
#include "XLUiMenuPopup.h" // MenuSide and placementForNode: a picker drops out of a node like a menu
#include "XLUiSearchSystem.h"
#include "XL2dIconSprite.h"
#include "XL2dLabel.h"
#include "XLUiControlLock.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/* Selection from a long list: a query line, a result list, and the matched characters lit up in
each row. The query line keeps focus while the arrows move the list selection, so this does not
use MenuSystem's keyboard mode; for short lists use ui::Select. */

// What a caller can size and colour without writing a stylesheet.
struct SP_PUBLIC SearchPickerStyle {
	float minWidth = 280.0f;
	float maxWidth = 560.0f;

	float queryHeight = 34.0f;
	float rowHeight = 30.0f;

	// How many rows the surface opens with. It does not grow past this; the list scrolls.
	uint32_t maxRows = 10;

	float padding = 6.0f;

	// The colour of a matched fragment. Set here, not in CSS: a character range inside a label is
	// not a node a stylesheet can address.
	Color4B matchColor = Color4B(0xFF, 0xC1, 0x07, 0xFF);
};

// The comparison, for a caller with no SearchSystem. The callback fills `out` with its own score
// and highlight ranges.
using SearchMatchFunction = Function<bool(StringView query, StringView target, SearchHit &out)>;

struct SP_PUBLIC SearchPickerConfig {
	// Where results come from. Must be passed: a popup is a separate scene, so
	// SearchSystem::findForNode from inside the surface finds nothing.
	SearchSystem *system = nullptr;
	String sourceName;

	// The fallback path: used when `system` is null. `items` is the whole list, `match` decides.
	// An empty `match` with items present means "subsequence", the same default a source has.
	Vector<SearchItem> items;
	SearchMatchFunction match;

	SearchRequestParams params;
	SearchPickerStyle style;

	/* Group the results under categories while the query is empty (a palette); with a query the
	list is ranked and shown flat at depth 0. Rendered by a ui::TreeView instead of a TableView;
	hits, highlight, keys and callbacks are the same in both modes. Off by default. */
	bool grouped = false;

	// Which category a hit belongs to. An empty answer files the hit under `uncategorized`.
	// Unset with `grouped` on, this reads `SearchHit::data["category"]`.
	Function<StringView(const SearchHit &)> group;

	// What an uncategorized hit is filed under. Shown as a category like any other.
	String uncategorized = String("(no category)");

	String placeholder;

	// The id of the current value, so the list opens with it selected rather than at the top.
	String highlight;

	// A sheet of the list's own, for the native path. Left empty, the surface inherits the sheet
	// of the control it drops out of (PopupSurfaceConfig::styleSource).
	String stylesheet;
	String stylesheetSource;
	FileCategory stylesheetCategory = FileCategory::Bundled;

	String title;
	String idPrefix;
	bool preferNative = true;
	sprt::window::WindowCreationFlags flags = sprt::window::WindowCreationFlags::None;

	// The query changed, before any item is matched against it. Lets a caller with its own index
	// rank and call setItems() first. Runs on both paths.
	Function<void(StringView query)> onQuery;

	Function<void(const SearchHit &)> onActivate;
	Function<void()> onClose;
};

/** The surface: a query line above a list of results.

Separate from the control that opens it, so it works both inside a popup and parented straight into
a node (which lets it be driven with no window). */
class SP_PUBLIC SearchPickerContent : public Panel {
public:
	virtual ~SearchPickerContent();

	virtual bool init(SearchPickerConfig &&);

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;
	virtual void handleContentSizeDirty() override;

	TextInput *getQueryInput() const { return _query; }

	// Only one is non-null: the table in flat mode, the tree in grouped mode. Prefer the row
	// accessors below.
	TableView *getResults() const { return _results; }
	TreeView *getTree() const { return _tree; }

	SpanView<SearchHit> getHits() const { return _hits; }

	// What the field shows. Lags by an echo after a programmatic setText.
	StringView getQuery() const;

	// The query the current hits answer; up to date as soon as the list is rebuilt.
	StringView getResultQuery() const { return _resultQuery; }

	/* Replace the local list. Call from `onQuery` to supply the caller's own answer to the query
	before matching runs. No effect on the source-backed path. */
	virtual void setItems(Vector<SearchItem> &&);
	SpanView<SearchItem> getItems() const { return _config.items; }

	/* ---- the rows, whichever view is carrying them ----

	A display row is not a hit: in grouped mode a category is a row with no hit, and expanding one
	shifts the rows after it. Use the mapping below rather than assuming one. */
	size_t getRowCount() const;

	// The hit a display row stands for, or maxOf<size_t>() for a category row.
	size_t getHitForRow(size_t row) const;

	// A hit's title or a category's name. Empty for no such row.
	StringView getRowTitle(size_t row) const;

	// Where a hit is showing, or maxOf<size_t>() when its category is collapsed.
	size_t getRowForHit(size_t hit) const;

	// Open or close a category row. False in the flat mode, and for a row that is not a category.
	virtual bool toggleRow(size_t row);
	bool isRowExpanded(size_t row) const;

	/* True while the tree is showing categories: grouped, and the result query is empty. Decided by
	the query the current hits were built for, not by the field text, which lags by an echo. */
	bool isGrouping() const;

	/* Make a hit visible, expanding its category. True when it is showing afterwards. Needed to
	select a hit in grouped mode, where categories open collapsed. */
	bool revealHit(size_t hit);

	// Index into getHits(), or maxOf<size_t>() when the list is empty. A hit index in both modes.
	size_t getSelected() const { return _selected; }
	virtual bool setSelected(size_t);

	// One step through visible hit rows, skipping category rows.
	virtual bool moveSelection(int32_t delta);

	// Reports the selected hit through the activate callback. False when there is nothing selected.
	virtual bool activateSelected();

	// A request is in flight and the list on screen is the previous answer.
	bool isPending() const { return _pending; }

	// Runs the query now, ignoring the system's debounce. What a test drives the widget with.
	virtual void refresh();

	/* Runs the given query now. Use after a programmatic setText: the field reports the old string
	until the platform echoes the edit back. */
	virtual void refresh(StringView query);

	// The height this surface wants for `count` rows, before any node exists (for
	// SubWindow::Config::size).
	static float measureHeight(const SearchPickerStyle &, size_t rowCount);

protected:
	using Panel::init;

	virtual void handleQueryChanged(StringView);
	virtual void handleResult(SearchResult &&);
	virtual void rebuildModel();
	virtual void updateStatus();
	virtual void scrollToSelected();

	virtual bool handleKey(const GestureData &);

	/* One result's title with highlight. A tree row takes the label bare (a flex row measures
	a Label and cannot measure a Panel); a table cell takes it wrapped (a table sizes the cell and
	leaves a bare label at zero width). */
	Rc<basic2d::Label> buildTitleLabel(const SearchHit &) const;
	Rc<Node> buildTitleNode(const SearchHit &) const;

	// Which category a hit is filed under, by the config's rule or by the default one.
	StringView groupOf(const SearchHit &) const;

	SearchPickerConfig _config;

	TextInput *_query = nullptr;

	// Exactly one is built, chosen by `grouped` at init and never changed.
	TableView *_results = nullptr;
	TreeView *_tree = nullptr;

	basic2d::Label *_status = nullptr;

	InputListener *_keyListener = nullptr;

	Rc<data::Model> _model;
	Vector<SearchHit> _hits;

	// The query the hits in hand answer. Not the field's text - see isGrouping().
	String _resultQuery;

	size_t _selected = maxOf<size_t>();
	uint64_t _request = 0;
	bool _pending = false;
};

/** The control that opens it: shows the chosen value, opens the surface on click or on Enter.

Has the same outer interface as ui::Select, so a field can switch between the two by type. */
class SP_PUBLIC SearchPicker : public Panel, public EditLockTarget {
public:
	virtual ~SearchPicker();

	virtual bool init() override;

	virtual void handleExit() override;
	virtual void handleContentSizeDirty() override;

	virtual void setConfig(SearchPickerConfig &&);
	const SearchPickerConfig &getConfig() const { return _config; }

	// `title` is what is shown; `id` is what getValue() reports and what a form collects.
	virtual void setValue(StringView id, StringView title, bool silent = false);
	StringView getValue() const { return _value; }
	StringView getValueTitle() const { return _title; }

	virtual void setPlaceholder(StringView);
	StringView getPlaceholder() const { return _placeholder; }

	virtual void setChangeCallback(Function<void(const SearchHit &)> &&);

	virtual void setEnabled(bool) override;
	bool isEnabled() const override { return isControlEnabled(this); }

	virtual void focus();
	virtual void blur();
	bool isFocused() const { return _focused; }

	virtual bool open();
	virtual void close();
	bool isOpen() const { return _popup != nullptr; }
	SubWindow *getPopup() const { return _popup; }

	// The open list surface, or null while the picker is closed.
	SearchPickerContent *getContent() const;

	basic2d::Label *getLabel() const { return _label; }
	basic2d::IconSprite *getIcon() const { return _icon; }

protected:
	using Panel::init;

	virtual void updateContent();
	virtual void updateInteractiveState();

	bool handleKey(const GestureData &);
	bool handleTap();

	core::RenderServerChannel *getParentWindow() const;

	SearchPickerConfig _config;

	String _value;
	String _title;
	String _placeholder;

	basic2d::Label *_label = nullptr;
	basic2d::IconSprite *_icon = nullptr;

	InputListener *_listener = nullptr;
	InputListener *_focusListener = nullptr;

	Rc<SubWindow> _popup;

	Function<void(const SearchHit &)> _changeCallback;

	bool _focused = false;
	bool _hoverApplied = false;
};

/** Opens a picker surface over `anchor`, without a SearchPicker control in front of it.

For uses with no value to show when closed: a command palette, "go to file", a node palette. */
SP_PUBLIC Rc<SubWindow> openSearchPicker(NotNull<core::RenderServerChannel>, NotNull<Node> anchor,
		SearchPickerConfig &&, MenuSide = MenuSide::Below);

} // namespace stappler::xenolith::ui

#endif /* XENOLITH_RENDERER_UI_INPUT_XLUISEARCHPICKER_H_ */
