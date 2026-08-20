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
#include "XLUiSubWindow.h"
#include "XLUiMenuPopup.h" // MenuSide and placementForNode: a picker drops out of a node like a menu
#include "XLUiSearchSystem.h"
#include "XL2dIconSprite.h"
#include "XL2dLabel.h"
#include "XLUiEditLock.h"

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
	TableView *getResults() const { return _results; }

	SpanView<SearchHit> getHits() const { return _hits; }
	StringView getQuery() const;

	// Index into getHits(), or maxOf<size_t>() when the list is empty.
	size_t getSelected() const { return _selected; }
	virtual bool setSelected(size_t);
	virtual bool moveSelection(int32_t delta);

	// Reports the selected hit through the activate callback. False when there is nothing selected.
	virtual bool activateSelected();

	// A request is in flight and the list on screen is the previous answer.
	bool isPending() const { return _pending; }

	// Runs the query now, ignoring the system's debounce. What a test drives the widget with.
	virtual void refresh();

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

	SearchPickerConfig _config;

	TextInput *_query = nullptr;
	TableView *_results = nullptr;
	basic2d::Label *_status = nullptr;

	InputListener *_keyListener = nullptr;

	Rc<data::Model> _model;
	Vector<SearchHit> _hits;

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
	bool isEnabled() const { return _enabled; }

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

	bool _enabled = true;
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
