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


#include "XLUiSearchPicker.h"
#include "XLUiMenuPopup.h"
#include "XLUiPopupSurface.h"
#include "XLUiSubWindowSession.h"
#include "XLUiStyleSystem.h"
#include "XLUiStyleResolver.h"
#include "XLUiLayoutSystem.h"
#include "XLInputListener.h"
#include "XLAppWindow.h"
#include "XLDirector.h"
#include "XLScene.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

static constexpr IconName s_searchPickerIcon = IconName::Action_search_outline;
static constexpr float s_searchPickerPadding = 10.0f;
static constexpr float s_searchPickerGap = 8.0f;

// The surface's own background, for the same reason a menu has one: a SceneLayout2d paints nothing.
static constexpr Color4B s_searchPickerSurfaceColor = Color4B(0xFA, 0xFA, 0xFA, 0xFF);

// ---- SearchPickerContent ----------------------------------------------------------------------

SearchPickerContent::~SearchPickerContent() { }

float SearchPickerContent::measureHeight(const SearchPickerStyle &style, size_t rowCount) {
	auto rows = sprt::max(size_t(1), sprt::min(size_t(style.maxRows), rowCount));
	return style.padding * 2.0f + style.queryHeight + style.rowHeight * float(rows);
}

bool SearchPickerContent::init(SearchPickerConfig &&config) {
	if (!Panel::init()) {
		return false;
	}

	_config = sp::move(config);

	setType("search-picker-content");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-search-picker-content");

	_query = addChild(Rc<TextInput>::create(), ZOrder(1));
	_query->setName("search-picker-query");
	_query->addStyleClass("xl-ui-search-picker-query");
	if (!_config.placeholder.empty()) {
		_query->setPlaceholder(_config.placeholder);
	}

	/* The query hook. It is already composition-safe: TextInput withholds the change while an IME
	is assembling a character, so this does not fire once per keystroke of a syllable. */
	_query->setCallback([this](StringView value) { handleQueryChanged(value); });

	if (_config.grouped) {
		/* The grouped list is a TREE, because only a tree has a depth and an expansion state to
		keep. Everything else about it is the flat list's: the same hits, the same title node, the
		same highlight, and the same two callbacks - which is the point of doing it here rather than
		in a second widget. */
		_tree = addChild(Rc<TreeView>::create(), ZOrder(1));
		_tree->setName("search-picker-results");
		_tree->addStyleClass("xl-ui-search-picker-results");
		_tree->setRowHeight(_config.style.rowHeight);
		_tree->setSelectionEnabled(true);

		_tree->setRowCallback([this](TreeView::RowBuilder &builder) {
			// A row is addressed by what its own Value SAYS, never by its index into a list built
			// beside the model: expanding a category shifts every row after it, and a parallel
			// vector would then describe the wrong rows.
			const auto &data = builder.getData();
			if (!data.hasValue("index")) {
				// A category. The standard decorated row draws it, so it gets the expander and the
				// indent for nothing.
				builder.setLabel(data.getString("name"));
				builder.setName(toString("search-picker-category-", builder.getIndex()));
				return;
			}

			auto index = size_t(data.getInteger("index"));
			if (index < _hits.size()) {
				builder.setContent(buildTitleNode(_hits[index]));
			}
			builder.setName(toString("search-picker-row-", builder.getIndex()));
		});

		_tree->setSelectCallback([this](size_t index, const TreeView::Row &) {
			// The tree is the other writer of the selection; keeping our own copy in step is what
			// lets the arrow keys and the mouse agree about what "selected" means. A category row
			// stands for no hit, so selecting one selects nothing.
			_selected = getHitForRow(index);
		});
		_tree->setActivateCallback([this](size_t index, const TreeView::Row &row) {
			// Activating a category OPENS it. It is not a result and there is nothing to report.
			if (row.isCategory()) {
				_tree->toggleRow(index);
				return;
			}
			_selected = getHitForRow(index);
			activateSelected();
		});
	} else {
		_results = addChild(Rc<TableView>::create(), ZOrder(1));
		_results->setName("search-picker-results");
		_results->addStyleClass("xl-ui-search-picker-results");
		_results->setHeaderVisible(false);
		_results->setRowHeight(_config.style.rowHeight);
		_results->setSelectionEnabled(true);
		_results->setColumns(Vector<TableView::Column>{
			{String("title"), String(), String("search-picker-cell"), GridTrack()},
		});
		_results->setCellCallback([this](TableView::CellBuilder &builder) {
			if (builder.isHeader()) {
				return;
			}
			auto row = builder.getRow();
			if (!row) {
				return;
			}
			auto index = size_t(row->getData().getInteger("index"));
			if (index < _hits.size()) {
				builder.setNode(buildTitleNode(_hits[index]));
			}
		});
		_results->setSelectCallback([this](size_t index, const TableView::Row &) {
			// The table is the other writer of the selection; keeping our own copy in step is what
			// lets the arrow keys and the mouse agree about what "selected" means.
			_selected = index;
		});
		_results->setActivateCallback([this](size_t index, const TableView::Row &) {
			_selected = index;
			activateSelected();
		});
	}

	_status = addChild(Rc<basic2d::Label>::create(), ZOrder(2));
	_status->setName("search-picker-status");
	_status->setType("label");
	_status->addStyleClass("xl-ui-search-picker-status");
	_status->setAlignment(font::TextAlign::Center);
	_status->setVisible(false);

	/* Priority 1 puts this in the dispatcher's PRE-SCENE band, so it sees the arrows before the
	query line's own listener - which binds Up and Down as "go to the start/end of the line" and
	would otherwise swallow them. Everything it does not claim falls straight through to the field.

	This is the point where the widget stops resembling ui::Select: that one hands the keyboard over
	to MenuSystem while its list is open, because nothing there is typing. Here the query line has
	to keep focus for the whole interaction, so the surface takes only the keys that move a
	selection and leaves the rest to the text field. */
	_keyListener = addSystem(Rc<InputListener>::create());
	_keyListener->setPriority(1);

	InputKeyMask keys;
	keys.set(toInt(InputKeyCode::UP));
	keys.set(toInt(InputKeyCode::DOWN));
	keys.set(toInt(InputKeyCode::PAGE_UP));
	keys.set(toInt(InputKeyCode::PAGE_DOWN));
	keys.set(toInt(InputKeyCode::ENTER));
	keys.set(toInt(InputKeyCode::KP_ENTER));
	_keyListener->addKeyRecognizer([this](const GestureData &data) { return handleKey(data); },
			InputKeyInfo{sp::move(keys)});

	/* Escape is a HOTKEY here, not a key: the engine registers it as `back` ("Back / close") and
	the hotkey pass consumes it before any key recognizer runs. Bound as a raw keycode it simply
	never arrives - which is how a picker ends up being the one popup a person cannot dismiss. */
	_keyListener->addHotkey(EngineHotkeys::get().back, [this](HotkeyId, const InputEvent &) {
		/* Gated on having somewhere to close TO, not on the query line holding focus.

		Focus is the wrong test twice over: a surface in a popup may not have been given the
		keyboard yet when the user hits Escape, and a surface parented into a panel among other
		widgets has no business claiming Escape at all. A configuration with an onClose is exactly
		the surface that was opened as a dismissable thing. */
		if (_config.onClose) {
			_config.onClose();
			return true;
		}
		return false;
	}, HotkeyFlags::None);

	// A key event carries a pointer location, so the default filter would answer only while the
	// mouse happens to be over the surface.
	_keyListener->setTouchFilter(
			[](const InputEvent &event, const InputListener::DefaultEventFilter &cb) {
		if (event.data.isKeyEvent()) {
			return true;
		}
		return cb(event);
	});

	return true;
}

void SearchPickerContent::handleEnter(Scene *scene) {
	Panel::handleEnter(scene);

	if (_query) {
		_query->focus();
	}

	// The list starts full rather than empty: a palette that shows nothing until something is typed
	// hides the very thing the user opened it to look through.
	refresh();
}

void SearchPickerContent::handleExit() {
	if (_config.system && _request) {
		_config.system->cancel(_request);
		_request = 0;
		_pending = false;
	}
	Panel::handleExit();
}

void SearchPickerContent::handleContentSizeDirty() {
	Panel::handleContentSizeDirty();

	if (getSystemByType<LayoutSystem>()) {
		// A LayoutSystem owns the children's geometry; this placement would be a second writer of
		// the same positions. Same rule as ui::Select's.
		return;
	}

	const float width = _contentSize.width;
	const float height = _contentSize.height;
	if (width <= 0.0f || height <= 0.0f) {
		return;
	}

	const float padding = _config.style.padding;
	const float inner = sprt::max(width - padding * 2.0f, 0.0f);

	if (_query) {
		_query->setAnchorPoint(Anchor::TopLeft);
		_query->setPosition(Vec2(padding, height - padding));
		_query->setContentSize(Size2(inner, _config.style.queryHeight));
	}

	const float listTop = height - padding - _config.style.queryHeight;
	const float listHeight = sprt::max(listTop - padding, 0.0f);

	if (auto list = _results ? static_cast<Node *>(_results) : static_cast<Node *>(_tree)) {
		list->setAnchorPoint(Anchor::TopLeft);
		list->setPosition(Vec2(padding, listTop));
		list->setContentSize(Size2(inner, listHeight));
	}

	if (_status) {
		_status->setAnchorPoint(Anchor::MiddleTop);
		_status->setPosition(Vec2(width / 2.0f, listTop));
		_status->setWidth(inner);
	}
}

StringView SearchPickerContent::getQuery() const {
	return _query ? _query->getText() : StringView();
}

void SearchPickerContent::setItems(Vector<SearchItem> &&items) { _config.items = sp::move(items); }

void SearchPickerContent::handleQueryChanged(StringView value) {
	// What the hits about to be built answer. Everything downstream that needs to know the query -
	// the display mode above all - reads this rather than the field, which may not have echoed yet.
	_resultQuery = value.str<Interface>();

	// Before anything is matched: a caller whose own index does the ranking scores the list here,
	// and `match` below is then only asked to report what that ranking said.
	if (_config.onQuery) {
		_config.onQuery(value);
	}

	if (_config.system && !_config.sourceName.empty()) {
		if (_request) {
			_config.system->cancel(_request);
		}
		_pending = true;
		_request = _config.system->query(_config.sourceName, value, _config.params,
				[this](SearchResult &&result) { handleResult(sp::move(result)); });
		updateStatus();
		return;
	}

	// No system: the local list, compared by the caller's function or by the same subsequence
	// matcher a source would have used.
	SearchResult result;
	result.query = value.str<Interface>();

	for (auto &item : _config.items) {
		if (_config.params.filter && !_config.params.filter(item.id, item.tag)) {
			continue;
		}

		SearchHit hit;
		hit.id = item.id;
		hit.tag = item.tag;
		hit.title = item.title;
		hit.subtitle = item.subtitle;
		hit.data = item.data;

		bool matched = false;
		if (_config.match) {
			matched = _config.match(value, item.title, hit);
		} else {
			search::FuzzyMatch match;
			search::fuzzyMatch(value, item.title, match);
			if (match.matched) {
				matched = true;
				hit.score = float(match.score);
				search::makeHighlightRanges(item.title, match.indices,
						[&](size_t start, size_t length) {
					hit.ranges.emplace_back(uint32_t(start), uint32_t(length));
				});
			}
		}

		if (matched) {
			result.hits.emplace_back(sp::move(hit));
		}
	}

	sprt::sort(result.hits.begin(), result.hits.end(), [](const SearchHit &l, const SearchHit &r) {
		if (l.score != r.score) {
			return l.score > r.score;
		}
		return sprt::unicode::compareCodepoints(StringView(l.title), StringView(r.title)) < 0;
	});

	if (_config.params.limit && result.hits.size() > _config.params.limit) {
		result.hits.resize(_config.params.limit);
		result.partial = true;
	}

	handleResult(sp::move(result));
}

void SearchPickerContent::handleResult(SearchResult &&result) {
	_pending = false;
	_request = 0;
	_hits = sp::move(result.hits);

	rebuildModel();

	// The current value if it is still in the list, the first row otherwise. Not "keep the previous
	// index": after a query narrows the list, index 3 is a different thing than it was.
	size_t selected = _hits.empty() ? maxOf<size_t>() : 0;
	if (!_config.highlight.empty()) {
		for (uint32_t i = 0; i < _hits.size(); ++i) {
			if (_hits[i].data.getString("id") == _config.highlight
					|| StringView(_hits[i].title) == StringView(_config.highlight)) {
				selected = i;
				break;
			}
		}
	}

	setSelected(selected);
	updateStatus();
}

StringView SearchPickerContent::groupOf(const SearchHit &hit) const {
	auto name = _config.group ? _config.group(hit) : hit.data.getString("category");
	return name.empty() ? StringView(_config.uncategorized) : name;
}

bool SearchPickerContent::isGrouping() const { return _config.grouped && _resultQuery.empty(); }

void SearchPickerContent::rebuildModel() {
	// A fresh model rather than a cleared one: setSource early-outs on the same pointer, and for a
	// list this size building a new one is cheaper than reasoning about what a partial update
	// leaves behind.
	_model = Rc<data::Model>::create();

	auto root = _model->getRoot();

	auto addHit = [&](data::Model::Node *parent, uint32_t i) {
		Value value;
		// The index, not the payload: the row has to find its way back to the hit, and the hit's
		// own id is the caller's and need not be unique.
		value.setInteger(int64_t(i), "index");
		value.setString(_hits[i].title, "title");
		value.setString(_hits[i].title, "name"); // the tree's standard label key
		_model->emplaceItem(parent, maxOf<size_t>(), sp::move(value));
	};

	if (isGrouping()) {
		/* Categories in FIRST-APPEARANCE order, walked once per category over the hits.

		That keeps the order INSIDE a category the one the source produced - which for a palette is
		a deterministic order somebody's golden dump asserts - and it means the categories
		themselves come out in the order the source first mentions them rather than in a collation
		order, which is a matter of convention and is not promised stable across Unicode versions. */
		Vector<StringView> categories;
		for (auto &hit : _hits) {
			auto name = groupOf(hit);
			bool seen = false;
			for (auto &c : categories) {
				if (c == name) {
					seen = true;
					break;
				}
			}
			if (!seen) {
				categories.emplace_back(name);
			}
		}

		for (auto &category : categories) {
			Value value;
			value.setString(category, "name");
			auto node = _model->emplaceCategory(root, maxOf<size_t>(), sp::move(value));
			for (uint32_t i = 0; i < _hits.size(); ++i) {
				if (groupOf(_hits[i]) == category) {
					addHit(node, i);
				}
			}
		}
	} else {
		for (uint32_t i = 0; i < _hits.size(); ++i) { addHit(root, i); }
	}

	if (_results) {
		_results->setSource(_model);
	}
	if (_tree) {
		_tree->setSource(_model);
	}
}

// ---- the rows, whichever view is carrying them ---------------------------------------------------

size_t SearchPickerContent::getRowCount() const {
	if (_tree) {
		return _tree->getRowCount();
	}
	return _hits.size();
}

size_t SearchPickerContent::getHitForRow(size_t row) const {
	if (!_tree) {
		return row < _hits.size() ? row : maxOf<size_t>();
	}
	auto r = _tree->getRow(row);
	if (!r || r->isCategory()) {
		return maxOf<size_t>();
	}
	const auto &data = r->getData();
	if (!data.hasValue("index")) {
		return maxOf<size_t>();
	}
	auto index = size_t(data.getInteger("index"));
	return index < _hits.size() ? index : maxOf<size_t>();
}

size_t SearchPickerContent::getRowForHit(size_t hit) const {
	if (!_tree) {
		return hit < _hits.size() ? hit : maxOf<size_t>();
	}
	for (size_t i = 0; i < _tree->getRowCount(); ++i) {
		if (getHitForRow(i) == hit) {
			return i;
		}
	}
	// Its category is collapsed, so it is not showing at all - which is an answer, and a different
	// one from "there is no such hit".
	return maxOf<size_t>();
}

bool SearchPickerContent::toggleRow(size_t row) {
	if (!_tree) {
		return false;
	}
	auto r = _tree->getRow(row);
	return (r && r->isCategory()) ? _tree->toggleRow(row) : false;
}

bool SearchPickerContent::isRowExpanded(size_t row) const {
	return _tree ? _tree->isRowExpanded(row) : false;
}

void SearchPickerContent::updateStatus() {
	if (!_status) {
		return;
	}

	if (!_hits.empty()) {
		_status->setVisible(false);
		return;
	}

	_status->setVisible(true);
	_status->setString(_pending ? StringView("…") : StringView("Nothing found"));
}

bool SearchPickerContent::setSelected(size_t index) {
	if (index != maxOf<size_t>() && index >= _hits.size()) {
		return false;
	}

	_selected = index;
	if (_results) {
		_results->setSelectedRow(index);
	}
	if (_tree) {
		// A hit whose category is collapsed is showing nowhere, and the tree is told to select
		// nothing rather than a row that stands for something else.
		_tree->setSelectedRow(getRowForHit(index));
	}
	scrollToSelected();
	return true;
}

bool SearchPickerContent::moveSelection(int32_t delta) {
	if (_hits.empty()) {
		return false;
	}

	if (!_tree) {
		int64_t next = (_selected == maxOf<size_t>()) ? 0 : int64_t(_selected) + delta;
		if (next < 0) {
			next = 0;
		}
		if (next >= int64_t(_hits.size())) {
			next = int64_t(_hits.size()) - 1;
		}
		return setSelected(size_t(next));
	}

	/* One step through what is VISIBLE, skipping the category rows.

	That is what an arrow key means to a person, and in this mode it is not one step through the
	hits: a category sits between two of them, and hits under a collapsed category are not there to
	step onto at all. Walking the DISPLAY and mapping back is the only way to get both right. */
	const size_t rows = _tree->getRowCount();
	if (rows == 0) {
		return false;
	}

	const int32_t step = delta < 0 ? -1 : 1;
	int64_t at = int64_t(getRowForHit(_selected));
	if (_selected == maxOf<size_t>() || at < 0) {
		// Nothing selected: start just outside, so the first step lands on the first (or last) hit.
		at = step > 0 ? -1 : int64_t(rows);
	}

	for (int32_t taken = 0; taken < (delta < 0 ? -delta : delta); ++taken) {
		int64_t next = at + step;
		while (next >= 0 && next < int64_t(rows) && getHitForRow(size_t(next)) == maxOf<size_t>()) {
			next += step;
		}
		if (next < 0 || next >= int64_t(rows)) {
			break; // the ends hold, exactly as they do in the flat list
		}
		at = next;
	}

	const auto hit = (at >= 0 && at < int64_t(rows)) ? getHitForRow(size_t(at)) : maxOf<size_t>();
	return hit != maxOf<size_t>() ? setSelected(hit) : false;
}

void SearchPickerContent::scrollToSelected() {
	if (_selected == maxOf<size_t>() || (!_results && !_tree)) {
		return;
	}

	auto scroll = _results ? _results->getScroll() : _tree->getScroll();
	if (!scroll) {
		return;
	}

	/* Arithmetic on the row height rather than on the row's node: the selected row may not be built
	at all - that is what virtualization means - and the position it WOULD have is what the scroller
	needs to be told anyway. Correct because the picker sets one fixed row height; a table with a
	per-row height callback would have to ask the controller instead. */
	const float rowHeight = _config.style.rowHeight;
	// The DISPLAY row, which in the grouped mode is not the hit index: the categories above it are
	// rows too.
	const auto row = getRowForHit(_selected);
	if (row == maxOf<size_t>()) {
		return;
	}
	const float top = float(row) * rowHeight;
	const float bottom = top + rowHeight;

	const float position = scroll->getScrollPosition();
	const float size = scroll->getScrollSize();
	if (size <= 0.0f) {
		return;
	}

	if (top < position) {
		scroll->setScrollPosition(top);
	} else if (bottom > position + size) {
		scroll->setScrollPosition(bottom - size);
	}
}

bool SearchPickerContent::activateSelected() {
	if (_selected == maxOf<size_t>() || _selected >= _hits.size()) {
		return false;
	}

	if (_config.onActivate) {
		_config.onActivate(_hits[_selected]);
	}
	return true;
}

void SearchPickerContent::refresh() { handleQueryChanged(getQuery()); }

void SearchPickerContent::refresh(StringView query) { handleQueryChanged(query); }

bool SearchPickerContent::handleKey(const GestureData &data) {
	if (!data.input) {
		return false;
	}

	const auto &ev = data.input->data;
	// Repeats included: holding Down has to keep walking the list, which is the one place in this
	// widget where auto-repeat is the expected behaviour rather than an accident.
	if (ev.event != InputEventName::KeyPressed && ev.event != InputEventName::KeyRepeated) {
		return false;
	}

	// Only while the query line holds focus. Embedded in a panel among other widgets, this surface
	// has no claim on the arrow keys when the user is somewhere else.
	if (!_query || !_query->isFocused()) {
		return false;
	}

	switch (ev.key.keycode) {
	case InputKeyCode::UP: return moveSelection(-1);
	case InputKeyCode::DOWN: return moveSelection(1);
	case InputKeyCode::PAGE_UP: return moveSelection(-int32_t(_config.style.maxRows));
	case InputKeyCode::PAGE_DOWN: return moveSelection(int32_t(_config.style.maxRows));
	case InputKeyCode::ENTER:
	case InputKeyCode::KP_ENTER: return activateSelected();
	default: break;
	}
	return false;
}

Rc<Node> SearchPickerContent::buildTitleNode(const SearchHit &hit) const {
	/* The same shape TableView builds for a plain cell - a `table-cell` Panel with a `label`
	inside - rather than a bare Label. The cell is what the table sizes; a label handed over
	directly is positioned and then left at zero width, which is a row that renders nothing. */
	auto panel = Rc<Panel>::create();
	panel->setType("table-cell");
	panel->removeStyleClass("xl-ui-panel");
	panel->addStyleClass("xl-ui-table-cell");
	panel->addStyleClass("xl-ui-search-picker-cell");
	Panel::registerStyleAppliers("table-cell");

	auto label = panel->addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	label->setType("label");
	label->addStyleClass("table-label");
	label->addStyleClass("xl-ui-search-picker-title");
	label->setAlignment(font::TextAlign::Left);
	label->setString(hit.title);

	/* The matched characters, in the units the label counts in. The conversion happened where the
	match was produced (search::makeHighlightRanges); by the time a range reaches here it is
	already a pair a label can be handed, which is the whole reason that arithmetic lives in the
	engine instead of at every call site. */
	const auto &color = _config.style.matchColor;
	for (auto &range : hit.ranges) {
		label->setTextRangeStyle(range.first, range.second,
				basic2d::Label::Style(Color3B(color.r, color.g, color.b)));
	}

	return panel;
}

// ---- SearchPicker -----------------------------------------------------------------------------

SearchPicker::~SearchPicker() { }

bool SearchPicker::init() {
	if (!Panel::init()) {
		return false;
	}

	setType("search-picker");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-search-picker");
	registerStyleAppliers("search-picker");

	_icon = addChild(Rc<basic2d::IconSprite>::create(), ZOrder(1));
	_icon->setType("icon");
	_icon->addStyleClass("xl-ui-search-picker-icon");
	_icon->setIconName(s_searchPickerIcon);

	_label = addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	_label->setType("label");
	_label->addStyleClass("xl-ui-search-picker-label");
	_label->setAlignment(font::TextAlign::Left);

	_listener = addSystem(Rc<InputListener>::create());

	_listener->addTapRecognizer([this](const GestureTap &tap) {
		if (tap.event == GestureEvent::Activated) {
			return handleTap();
		}
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}), 1});

	_listener->addMouseOverRecognizer([this](const GestureData &data) {
		switch (data.event) {
		case GestureEvent::Began: _hoverApplied = true; break;
		case GestureEvent::Ended:
		case GestureEvent::Cancelled: _hoverApplied = false; break;
		default: break;
		}
		updateInteractiveState();
		return true;
	}, false);

	InputKeyMask keys;
	keys.set(toInt(InputKeyCode::ENTER));
	keys.set(toInt(InputKeyCode::KP_ENTER));
	keys.set(toInt(InputKeyCode::SPACE));
	keys.set(toInt(InputKeyCode::DOWN));
	_listener->addKeyRecognizer([this](const GestureData &data) { return handleKey(data); },
			InputKeyInfo{sp::move(keys)});

	_listener->setTouchFilter(
			[this](const InputEvent &event, const InputListener::DefaultEventFilter &cb) {
		if (event.data.isKeyEvent()) {
			return _focused;
		}
		return cb(event);
	});

	_focusListener = addSystem(Rc<InputListener>::create());
	_focusListener->setPriority(1);
	_focusListener->addTapRecognizer([this](const GestureTap &) {
		// Not while the surface is up: the tap that picks a row lands in another window.
		if (!isOpen()) {
			blur();
		}
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}), 1});
	_focusListener->setTouchFilter(
			[this](const InputEvent &event, const InputListener::DefaultEventFilter &) {
		return !isTouched(event.currentLocation, 0.0f);
	});
	_focusListener->setEnabled(false);

	updateContent();
	return true;
}

void SearchPicker::handleExit() {
	close();
	Panel::handleExit();
}

void SearchPicker::handleContentSizeDirty() {
	Panel::handleContentSizeDirty();

	if (getSystemByType<LayoutSystem>()) {
		return;
	}

	const float height = _contentSize.height;
	const float width = _contentSize.width;
	if (height <= 0.0f || width <= 0.0f) {
		return;
	}

	float left = s_searchPickerPadding;
	if (_icon && _icon->isVisible()) {
		_icon->setAnchorPoint(Anchor::MiddleLeft);
		_icon->setPosition(Vec2(left, height / 2.0f));
		left += _icon->getContentSize().width + s_searchPickerGap;
	}

	if (_label) {
		_label->setAnchorPoint(Anchor::MiddleLeft);
		_label->setPosition(Vec2(left, height / 2.0f));
		_label->setWidth(sprt::max(width - s_searchPickerPadding - left, 0.0f));
	}
}

void SearchPicker::setConfig(SearchPickerConfig &&config) {
	_config = sp::move(config);
	if (isOpen()) {
		// The surface was built from the previous configuration; rebuilding it under the user would
		// move the row they were about to click.
		close();
	}
	updateContent();
}

void SearchPicker::setValue(StringView id, StringView title, bool silent) {
	_value = id.str<Interface>();
	_title = title.str<Interface>();
	_config.highlight = _value;
	updateContent();

	if (!silent && _changeCallback) {
		SearchHit hit;
		hit.title = _title;
		hit.data.setString(_value, "id");
		_changeCallback(hit);
	}
}

void SearchPicker::setPlaceholder(StringView value) {
	_placeholder = value.str<Interface>();
	updateContent();
}

void SearchPicker::setChangeCallback(Function<void(const SearchHit &)> &&cb) {
	_changeCallback = sp::move(cb);
}

void SearchPicker::setEnabled(bool value) {
	// The lock has the last word, and remembers what was asked for so unlocking can give it
	// back. A no-op, and one pointer test, on a control nobody locked.
	value = resolveEditLock(this, value);
	if (_enabled == value) {
		return;
	}
	_enabled = value;
	if (!_enabled) {
		close();
		blur();
	}
	applyControlEnabled(this, _enabled);
	updateInteractiveState();
}

void SearchPicker::focus() {
	if (_focused || !_enabled) {
		return;
	}
	_focused = true;
	if (_focusListener) {
		_focusListener->setEnabled(true);
	}
	updateInteractiveState();
}

void SearchPicker::blur() {
	if (!_focused) {
		return;
	}
	_focused = false;
	if (_focusListener) {
		_focusListener->setEnabled(false);
	}
	updateInteractiveState();
}

void SearchPicker::updateContent() {
	if (!_label) {
		return;
	}

	if (!_title.empty()) {
		_label->setString(_title);
		_label->removeStyleClass("placeholder");
	} else {
		_label->setString(_placeholder);
		_label->addStyleClass("placeholder");
	}
}

void SearchPicker::updateInteractiveState() {
	setOrUpdateComponent<InteractiveComponent>([this](NotNull<InteractiveComponent> state) {
		// The Enabled bit and the `disabled` class are applyControlEnabled's, from setEnabled.
		bool dirty = false;
		// The counters are cumulative, so each flag is pushed on an edge and never twice.
		const bool hover = _hoverApplied && _enabled;
		if (hover != sprt::hasFlag(state->state, InteractiveState::Hover)) {
			dirty = state->handleHover(hover ? 1 : -1) || dirty;
		}
		const bool focus = _focused && _enabled;
		if (focus != sprt::hasFlag(state->state, InteractiveState::Focus)) {
			dirty = state->handleFocus(focus ? 1 : -1) || dirty;
		}
		return dirty;
	});
}

bool SearchPicker::handleTap() {
	if (!_enabled) {
		return false;
	}
	focus();
	if (isOpen()) {
		close();
	} else {
		open();
	}
	return true;
}

bool SearchPicker::handleKey(const GestureData &data) {
	if (!_focused || !_enabled || !data.input) {
		return false;
	}

	const auto &ev = data.input->data;
	if (ev.event != InputEventName::KeyPressed && ev.event != InputEventName::KeyRepeated) {
		return false;
	}

	// While the surface is up, the keyboard belongs to its query line, in its own window.
	if (isOpen()) {
		return false;
	}

	switch (ev.key.keycode) {
	case InputKeyCode::ENTER:
	case InputKeyCode::KP_ENTER:
	case InputKeyCode::SPACE:
	case InputKeyCode::DOWN: return open();
	default: break;
	}
	return false;
}

AppWindow *SearchPicker::getAppWindow() const {
	auto scene = getScene();
	auto director = scene ? scene->getDirector() : nullptr;
	auto server = director ? director->getRenderServer() : nullptr;
	return server ? dynamic_cast<AppWindow *>(server) : nullptr;
}

bool SearchPicker::open() {
	if (!_enabled || isOpen()) {
		return false;
	}

	auto window = getAppWindow();
	if (!window) {
		return false;
	}

	SearchPickerConfig config = _config;
	config.highlight = _value;

	// The surface is at least as wide as the control it drops out of.
	if (config.style.minWidth < _contentSize.width) {
		config.style.minWidth = _contentSize.width;
	}
	if (config.style.maxWidth < config.style.minWidth) {
		config.style.maxWidth = config.style.minWidth;
	}

	config.onActivate = [this, inner = _config.onActivate](const SearchHit &hit) {
		// Close first: an activation is free to open something else in this surface's place, and a
		// picker still standing behind it is one the user has to dismiss by hand.
		close();
		setValue(hit.data.getString("id"), hit.title);
		if (inner) {
			inner(hit);
		}
	};

	config.onClose = [this, inner = _config.onClose] {
		close();
		if (inner) {
			inner();
		}
	};

	_popup = openSearchPicker(window, this, sp::move(config), MenuSide::Below);
	if (!_popup) {
		return false;
	}

	addStyleClass("open");
	return true;
}

void SearchPicker::close() {
	if (auto popup = sp::move(_popup)) {
		_popup = nullptr;
		removeStyleClass("open");
		popup->dismiss();
	}
}

// ---- the surface ------------------------------------------------------------------------------

Rc<SubWindow> openSearchPicker(NotNull<AppWindow> window, NotNull<Node> anchor,
		SearchPickerConfig &&config, MenuSide side) {
	/* The extent has to be settled BEFORE any node exists - it is what the window request carries.
	The list is opened at its full height rather than at the height of the current answer: a surface
	that resized itself on every keystroke would jump under the pointer while the user typed. */
	const Extent2 size(uint32_t(std::lround(config.style.minWidth)),
			uint32_t(std::lround(
					SearchPickerContent::measureHeight(config.style, config.style.maxRows))));

	PopupSurfaceConfig surfaceConfig;
	surfaceConfig.stylesheet = config.stylesheet;
	surfaceConfig.stylesheetCategory = config.stylesheetCategory;
	surfaceConfig.stylesheetSource = config.stylesheetSource;
	surfaceConfig.title = config.title.empty() ? String("Search") : config.title;
	surfaceConfig.idPrefix = config.idPrefix.empty() ? String("search-picker") : config.idPrefix;
	surfaceConfig.size = size;
	surfaceConfig.layoutName = String("search-picker-layout");
	surfaceConfig.panelName = String("search-picker");
	surfaceConfig.fallbackColor = s_searchPickerSurfaceColor;
	surfaceConfig.flags = config.flags;
	surfaceConfig.preferNative = config.preferNative;
	// COPIED, not moved: SearchPickerContent reads onClose itself - it is what Escape calls - so
	// taking it out of the config here would leave the surface as the one popup that cannot be
	// dismissed from inside.
	surfaceConfig.onClose = config.onClose;

	/* The surface IS the content: it types and classes itself in init, so nothing here names it
	beyond the node name the inspector finds it by.

	Captured BY COPY, not moved: on the native path this does not run until the popup's scene
	exists, by which time whatever opened the picker may be gone. */
	surfaceConfig.makePanel = [config = config](NotNull<SubWindow>, Extent2) mutable -> Rc<Panel> {
		return Rc<SearchPickerContent>::create(sp::move(config));
	};

	return openPopupSurface(window, placementForNode(anchor, side), sp::move(surfaceConfig));
}

} // namespace stappler::xenolith::ui
