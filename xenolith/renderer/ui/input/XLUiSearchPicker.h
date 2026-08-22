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

/* Selection for a list nobody can read: a query line, a result list, and the matched characters
lit up in each row.

WHY IT IS NOT ui::Select. That widget's own header says it: a list of hundreds - every registered
component, every member of a large enum - is not a menu. A menu is chosen by looking; this is chosen
by typing, and the two want opposite things from the keyboard. A menu takes the arrow keys because
nothing else wants them. Here the query line holds focus the whole time and the arrows move a
selection somewhere else, which is why this does not reuse MenuSystem's keyboard mode. */

// What a caller can size and colour without writing a stylesheet.
struct SP_PUBLIC SearchPickerStyle {
	float minWidth = 280.0f;
	float maxWidth = 560.0f;

	float queryHeight = 34.0f;
	float rowHeight = 30.0f;

	// How many rows the surface opens with. It does not grow past this; the list scrolls.
	uint32_t maxRows = 10;

	float padding = 6.0f;

	/* The colour of a matched fragment.

	Configuration rather than CSS, and this is not a shortcut: a stylesheet addresses NODES, and a
	character range inside a label is not one. The row, the label and the surface are all styleable
	in the ordinary way; the run of characters inside the text is the one thing that has to be told. */
	Color4B matchColor = Color4B(0xFF, 0xC1, 0x07, 0xFF);
};

/* The comparison, for a caller with no SearchSystem.

A dozen fixed choices do not deserve an index, a configuration and a source registration, and a
widget that demanded them would simply not be used for that case. `out` is filled by the callback,
which is what lets it report its own score and its own highlight ranges rather than being reduced to
a yes or no. */
using SearchMatchFunction = Function<bool(StringView query, StringView target, SearchHit &out)>;

struct SP_PUBLIC SearchPickerConfig {
	/* Where results come from. PASSED, never looked up.

	A popup is a scene of its own: nothing of the opener is above it, so SearchSystem::findForNode
	from inside the surface finds nothing. The same trap the menu code documents for stylesheets,
	and it catches systems for exactly the same reason. */
	SearchSystem *system = nullptr;
	String sourceName;

	// The fallback path: used when `system` is null. `items` is the whole list, `match` decides.
	// An empty `match` with items present means "subsequence", the same default a source has.
	Vector<SearchItem> items;
	SearchMatchFunction match;

	SearchRequestParams params;
	SearchPickerStyle style;

	/* GROUP THE RESULTS UNDER CATEGORIES while nothing is typed.

	Off by default, and the flat path is untouched by it: a picker for a field's value wants the
	best match at the top and has nothing to group by.

	It is on for the case a ranked list cannot serve - a PALETTE. With an empty query there is
	nothing to rank, and a library of a hundred operations shown as a hundred rows in some
	deterministic order is a list nobody reads; the categories ARE the answer then. As soon as
	something is typed the ranking is the answer again, so the same widget collapses to a flat list
	at depth 0. One widget, because they are one interaction: the query line never loses focus and
	the arrows keep moving one selection through whatever is showing.

	The results are rendered by a ui::TreeView in this mode rather than a ui::TableView, since only
	that one has rows at a depth and an expansion state to keep. Nothing else about the widget
	changes: the same hits, the same highlight, the same keys, the same callbacks. */
	bool grouped = false;

	// Which category a hit belongs to. Empty (or an empty answer) puts the hit under a catch-all,
	// because a source is under no obligation to categorize everything and dropping a hit that
	// answered no group would be losing a result to a display decision.
	//
	// Unset with `grouped` on, this reads `SearchHit::data["category"]` - the key SearchItem::data
	// already carries through untouched, so a static list needs no callback at all.
	Function<StringView(const SearchHit &)> group;

	// What an uncategorized hit is filed under. Shown as a category like any other.
	String uncategorized = String("(no category)");

	String placeholder;

	// The id of the current value, so the list opens with it selected rather than at the top.
	String highlight;

	// A native popup is a window of its own and the parent's sheet does not reach into it.
	String stylesheet;
	String stylesheetSource;
	FileCategory stylesheetCategory = FileCategory::Bundled;

	String title;
	String idPrefix;
	bool preferNative = true;
	sprt::window::WindowCreationFlags flags = sprt::window::WindowCreationFlags::None;

	/* The query changed, BEFORE a single item is matched against it.

	For the caller whose own index does the ranking: a palette hands this widget a list it has
	already scored, and the scoring has to happen before `match` is asked about anything. Without
	the hook that caller would have to trigger its own search from inside `match` on a first call it
	could only recognize by remembering the last query - which works and is a trick, and a trick in
	a widget's contract is a thing the next caller gets wrong.

	Runs on both paths, so a source-backed picker can use it to show something of its own. */
	Function<void(StringView query)> onQuery;

	Function<void(const SearchHit &)> onActivate;
	Function<void()> onClose;
};

/** The surface: a query line above a list of results.

Separate from the control that opens it, because it has to work in two places - inside a popup, and
parented straight into a node. The second is not a convenience: it is what lets the widget be driven
and asserted with no window at all. */
class SP_PUBLIC SearchPickerContent : public Panel {
public:
	virtual ~SearchPickerContent();

	virtual bool init(SearchPickerConfig &&);

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;
	virtual void handleContentSizeDirty() override;

	TextInput *getQueryInput() const { return _query; }

	// Null in the grouped mode, where the results are a tree. A caller that only wants to reach the
	// list asks through the row facade below rather than through either of these.
	TableView *getResults() const { return _results; }
	TreeView *getTree() const { return _tree; }

	SpanView<SearchHit> getHits() const { return _hits; }

	// What the FIELD shows. Lags by an echo after a programmatic setText, because editing a
	// TextInput is a request to the platform - so this is what a person sees, not what the list in
	// front of them answers.
	StringView getQuery() const;

	// What the hits in hand ANSWER. Deterministic the moment the list is rebuilt, which is what
	// anything asserting about the surface wants: "the list is showing results for X" is a fact,
	// while "the field has caught up" is a frame away.
	StringView getResultQuery() const { return _resultQuery; }

	/* Replace the local list. For the caller whose OWN index answers the query: set this from
	`onQuery` - which runs before a single item is walked - and the walk then goes over the answer
	to that query rather than over a fixed library filtered a second time.

	Has no effect on the source-backed path, where the list is the source's. */
	virtual void setItems(Vector<SearchItem> &&);
	SpanView<SearchItem> getItems() const { return _config.items; }

	/* ---- the rows, whichever view is carrying them ----

	A DISPLAY row is not a hit: in the grouped mode a category is a row and stands for no hit at
	all, and expanding one shifts every row after it. So the two are counted separately and the
	mapping is asked for rather than assumed - which is the mistake a caller keeping a vector beside
	the model would make, and the one the graph editor's palette documented before this. */
	size_t getRowCount() const;

	// The hit a display row stands for, or maxOf<size_t>() for a category row.
	size_t getHitForRow(size_t row) const;

	// Where a hit is showing, or maxOf<size_t>() when its category is collapsed.
	size_t getRowForHit(size_t hit) const;

	// Open or close a category row. False in the flat mode, and for a row that is not a category.
	virtual bool toggleRow(size_t row);
	bool isRowExpanded(size_t row) const;

	// Index into getHits(), or maxOf<size_t>() when the list is empty. A HIT index in both modes,
	// so a caller that knows what it wants selected does not have to know how it is displayed.
	size_t getSelected() const { return _selected; }
	virtual bool setSelected(size_t);

	// One step through what is VISIBLE, skipping category rows: that is what an arrow key means to
	// a person, and in the grouped mode it is not the same as one step through the hits.
	virtual bool moveSelection(int32_t delta);

	// Reports the selected hit through the activate callback. False when there is nothing selected.
	virtual bool activateSelected();

	// A request is in flight and the list on screen is the previous answer.
	bool isPending() const { return _pending; }

	// Runs the query now, ignoring the system's debounce. What a test drives the widget with.
	virtual void refresh();

	/* The same, for a query the FIELD does not show yet.

	Editing a TextInput is a REQUEST to the platform: the text arrives back by echo, so immediately
	after setText the field still reports the old string. A caller whose model was driven from
	somewhere other than the keyboard - a command, a re-open, a restored session - therefore cannot
	use the field as the source of truth in the same turn, and refresh() would run the list for the
	string the field has not caught up with. This runs it for the string the caller means. */
	virtual void refresh(StringView query);

	// The height this surface wants for `count` rows, before any node exists - which is what a
	// window request needs and what SubWindow::Config::size demands up front.
	static float measureHeight(const SearchPickerStyle &, size_t rowCount);

protected:
	using Panel::init;

	virtual void handleQueryChanged(StringView);
	virtual void handleResult(SearchResult &&);
	virtual void rebuildModel();
	virtual void updateStatus();
	virtual void scrollToSelected();

	virtual bool handleKey(const GestureData &);

	Rc<Node> buildTitleNode(const SearchHit &) const;

	// Which category a hit is filed under, by the config's rule or by the default one.
	StringView groupOf(const SearchHit &) const;

	/* True while the tree is showing categories: grouped, and nothing typed. With a query the tree
	is a flat list at depth 0, because a ranking crosses categories.

	Asked of the query THE CURRENT HITS WERE BUILT FOR, never of the field. Editing a TextInput is a
	request to the platform whose text arrives back by echo, so a widget refreshed with an explicit
	query has hits for one string and a field still showing another - and reading the field there
	renders the right results in the wrong MODE, which is a ranked list drawn as a tree of two
	categories. That is not hypothetical; it is what this was written after. */
	bool isGrouping() const;

	SearchPickerConfig _config;

	TextInput *_query = nullptr;

	// Exactly one of these two is built, decided by `grouped` at init and never changed after: a
	// widget that swapped its list widget on every keystroke would throw away the scroll, the
	// styling and the expansion each time.
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

Deliberately shaped like ui::Select from the outside, so that changing one's mind about which of the
two a field wants is a change of type and not of the code around it. */
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

	virtual void setEnabled(bool);
	bool isEnabled() const override { return isControlEnabled(this); }

	virtual void focus();
	virtual void blur();
	bool isFocused() const { return _focused; }

	virtual bool open();
	virtual void close();
	bool isOpen() const { return _popup != nullptr; }
	SubWindow *getPopup() const { return _popup; }

	basic2d::Label *getLabel() const { return _label; }
	basic2d::IconSprite *getIcon() const { return _icon; }

protected:
	using Panel::init;

	virtual void updateContent();
	virtual void updateInteractiveState();

	bool handleKey(const GestureData &);
	bool handleTap();

	AppWindow *getAppWindow() const;

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

For the cases that are not a field with a value - a command palette, "go to file", the graph's node
palette - where there is nothing to show when the surface is closed. */
SP_PUBLIC Rc<SubWindow> openSearchPicker(NotNull<AppWindow>, NotNull<Node> anchor,
		SearchPickerConfig &&, MenuSide = MenuSide::Below);

} // namespace stappler::xenolith::ui

#endif /* XENOLITH_RENDERER_UI_INPUT_XLUISEARCHPICKER_H_ */
