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

	// TextInput withholds the change while an IME composes, so this fires once per character.
	_query->setCallback([this](StringView value) { handleQueryChanged(value); });

	if (_config.grouped) {
		// Grouped results use a tree for depth and expansion state; hits, title node, highlight and
		// callbacks are shared with the flat list.
		_tree = addChild(Rc<TreeView>::create(), ZOrder(1));
		_tree->setName("search-picker-results");
		_tree->addStyleClass("xl-ui-search-picker-results");
		_tree->setRowHeight(_config.style.rowHeight);
		_tree->setSelectionEnabled(true);

		_tree->setRowCallback([this](TreeView::RowBuilder &builder) {
			// A row is identified by its own Value, not by its position: expanding a category
			// shifts the rows after it.
			const auto &data = builder.getData();
			if (!data.hasValue("index")) {
				// A category, drawn by the standard decorated row (expander and indent).
				builder.setLabel(data.getString("name"));
				builder.setName(toString("search-picker-category-", builder.getIndex()));
				return;
			}

			auto index = size_t(data.getInteger("index"));
			if (index < _hits.size()) {
				/* A bare Label, not a `table-cell` wrapper: a tree row is a flex row, which can
				measure a Label but leaves a layout-less Panel at zero size. It also keeps
				`tree-row > label` selectors matching. */
				builder.setContent(buildTitleLabel(_hits[index]));
			}
			builder.setName(toString("search-picker-row-", builder.getIndex()));
		});

		_tree->setSelectCallback([this](size_t index, const TreeView::Row &row) {
			/* A tap picks the row. Runs only for taps (TreeView::setSelectedRow reports nothing),
			so there is no activate callback on this tree. */
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
			// A tap picks the row, as in the grouped mode.
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

	/* Priority 1 puts this in the dispatcher's pre-scene band, ahead of the query line's listener
	(which binds Up/Down to line start/end). The query line keeps focus; this takes only the
	selection keys and the rest falls through to the field. */
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

	/* Escape is bound as the `back` hotkey: the hotkey pass consumes it before any key recognizer
	runs, so a raw keycode binding never fires. */
	_keyListener->addHotkey(EngineHotkeys::get().back, [this](HotkeyId, const InputEvent &) {
		/* Gated on having an onClose, not on focus: a popup may not have the keyboard yet, and an
		embedded surface without onClose must not claim Escape. */
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

	// The list starts full, with results for the empty query.
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
		// A LayoutSystem owns the children's geometry.
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
	// The query the hits about to be built answer. Downstream code (the display mode) reads this,
	// not the field, which may not have echoed yet.
	_resultQuery = value.str<Interface>();

	// Before matching, so a caller with its own index can replace the list first.
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

	// The current value if it is still in the list, the first row otherwise.
	size_t selected = _hits.empty() ? maxOf<size_t>() : 0;
	bool highlighted = false;
	if (!_config.highlight.empty()) {
		for (uint32_t i = 0; i < _hits.size(); ++i) {
			// Through a const reference: `id` is optional in `data`, and the non-const getString
			// asserts on a missing key.
			const auto &hit = _hits[i];
			if (hit.data.getString("id") == _config.highlight
					|| StringView(hit.title) == StringView(_config.highlight)) {
				selected = i;
				highlighted = true;
				break;
			}
		}
	}

	/* Revealed before it is selected, and only when `highlight` named it: grouped categories open
	collapsed, so the hit has no row until revealed. The fallback to hit 0 reveals nothing. */
	if (highlighted) {
		revealHit(selected);
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
	// A fresh model rather than a cleared one: setSource early-outs on the same pointer.
	_model = Rc<data::Model>::create();

	auto root = _model->getRoot();

	auto addHit = [&](data::Model::Node *parent, uint32_t i) {
		Value value;
		// The hit index, since the caller's id need not be unique.
		value.setInteger(int64_t(i), "index");
		value.setString(_hits[i].title, "title");
		value.setString(_hits[i].title, "name"); // the tree's standard label key
		_model->emplaceItem(parent, maxOf<size_t>(), sp::move(value));
	};

	if (isGrouping()) {
		/* Categories in first-appearance order, and hits inside each in source order, so the
		output is deterministic and independent of collation. */
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

StringView SearchPickerContent::getRowTitle(size_t row) const {
	if (!_tree) {
		return row < _hits.size() ? StringView(_hits[row].title) : StringView();
	}
	auto r = _tree->getRow(row);
	if (!r) {
		return StringView();
	}
	if (!r->isCategory()) {
		const auto hit = getHitForRow(row);
		return hit < _hits.size() ? StringView(_hits[hit].title) : StringView();
	}
	return r->getData().getString("name");
}

size_t SearchPickerContent::getRowForHit(size_t hit) const {
	if (hit >= _hits.size()) {
		/* No selection shows nowhere. Checked here because getHitForRow returns maxOf for category
		rows, which the walk below would otherwise match. */
		return maxOf<size_t>();
	}
	if (!_tree) {
		return hit;
	}
	for (size_t i = 0; i < _tree->getRowCount(); ++i) {
		if (getHitForRow(i) == hit) {
			return i;
		}
	}
	// Its category is collapsed, so it is not showing.
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

bool SearchPickerContent::revealHit(size_t hit) {
	if (hit >= _hits.size()) {
		return false;
	}
	if (getRowForHit(hit) != maxOf<size_t>()) {
		return true; // flat, or its category is already open
	}
	if (!_tree) {
		return false;
	}

	// By name: a collapsed category has no child rows to walk.
	const auto category = groupOf(_hits[hit]);
	for (size_t i = 0; i < _tree->getRowCount(); ++i) {
		auto r = _tree->getRow(i);
		if (!r || !r->isCategory() || r->getData().getString("name") != category) {
			continue;
		}
		if (!_tree->isRowExpanded(i)) {
			_tree->toggleRow(i);
		}
		return getRowForHit(hit) != maxOf<size_t>();
	}
	return false;
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
		// A hit in a collapsed category selects no row.
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

	// Walks the displayed rows, skipping category rows, and maps the result back to a hit.
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
			break; // clamp at the ends, as in the flat list
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

	/* Computed from the fixed row height: the selected row may not be built (virtualized). */
	const float rowHeight = _config.style.rowHeight;
	// The display row, which in grouped mode differs from the hit index.
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
	// Repeats included, so holding Down keeps walking the list.
	if (ev.event != InputEventName::KeyPressed && ev.event != InputEventName::KeyRepeated) {
		return false;
	}

	// Only while the query line holds focus, so an embedded surface leaves other keys alone.
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

Rc<basic2d::Label> SearchPickerContent::buildTitleLabel(const SearchHit &hit) const {
	auto label = Rc<basic2d::Label>::create();
	label->setType("label");
	// Both classes: `table-label` for the flat list, `tree-label` to match TreeView's own labels.
	label->addStyleClass("table-label");
	label->addStyleClass("tree-label");
	label->addStyleClass("xl-ui-search-picker-title");
	label->setAlignment(font::TextAlign::Left);
	label->setString(hit.title);

	// Ranges are already in label units (converted by search::makeHighlightRanges).
	const auto &color = _config.style.matchColor;
	for (auto &range : hit.ranges) {
		label->setTextRangeStyle(range.first, range.second,
				basic2d::Label::Style(Color3B(color.r, color.g, color.b)));
	}

	return label;
}

Rc<Node> SearchPickerContent::buildTitleNode(const SearchHit &hit) const {
	/* The shape TableView builds for a plain cell: a `table-cell` Panel with a label inside. A
	table sizes the cell and leaves a bare label at zero width; tree rows take the bare label. */
	auto panel = Rc<Panel>::create();
	panel->setType("table-cell");
	panel->removeStyleClass("xl-ui-panel");
	panel->addStyleClass("xl-ui-table-cell");
	panel->addStyleClass("xl-ui-search-picker-cell");
	Panel::registerStyleAppliers("table-cell");
	// Transparent like the cells TableView builds itself; Panel's default opaque white would cover
	// the row background and selection (TableView_paintCellDefaults).
	panel->setPathColor(Color4B(0, 0, 0, 0), true);

	panel->addChild(buildTitleLabel(hit), ZOrder(1));

	return panel;
}

// ---- SearchPicker -----------------------------------------------------------------------------

SearchPicker::~SearchPicker() { }

bool SearchPicker::init() {
	if (!Panel::init()) {
		return false;
	}

	/* The InteractiveComponent must exist from the start: without one the state reads as 0, so
	`:disabled` would match and isEnabled() would report false. */
	applyControlEnabled(this, true);

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
		// The open surface was built from the previous configuration.
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
	// The edit lock overrides the request and remembers it for unlock.
	value = resolveEditLock(this, value);
	if (isEnabled() == value) {
		return;
	}
	applyControlEnabled(this, value);
	if (!value) {
		close();
		blur();
	}
	updateInteractiveState();
}

void SearchPicker::focus() {
	if (_focused || !isEnabled()) {
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
		const bool hover = _hoverApplied && sprt::hasFlag(state->state, InteractiveState::Enabled);
		if (hover != sprt::hasFlag(state->state, InteractiveState::Hover)) {
			dirty = state->handleHover(hover ? 1 : -1) || dirty;
		}
		const bool focus = _focused && sprt::hasFlag(state->state, InteractiveState::Enabled);
		if (focus != sprt::hasFlag(state->state, InteractiveState::Focus)) {
			dirty = state->handleFocus(focus ? 1 : -1) || dirty;
		}
		return dirty;
	});
}

bool SearchPicker::handleTap() {
	if (!isEnabled()) {
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
	if (!_focused || !isEnabled() || !data.input) {
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
	if (!isEnabled() || isOpen()) {
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
		// Close first: the activation may open something else in this surface's place.
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

SearchPickerContent *SearchPicker::getContent() const {
	return _popup ? dynamic_cast<SearchPickerContent *>(_popup->getPanel()) : nullptr;
}

Rc<SubWindow> openSearchPicker(NotNull<AppWindow> window, NotNull<Node> anchor,
		SearchPickerConfig &&config, MenuSide side) {
	/* The extent is part of the window request, so it is settled before any node exists. Full
	height, so the surface does not resize on every keystroke. */
	const Extent2 size(uint32_t(std::lround(config.style.minWidth)),
			uint32_t(std::lround(
					SearchPickerContent::measureHeight(config.style, config.style.maxRows))));

	PopupSurfaceConfig surfaceConfig;
	surfaceConfig.stylesheet = config.stylesheet;
	surfaceConfig.stylesheetCategory = config.stylesheetCategory;
	surfaceConfig.stylesheetSource = config.stylesheetSource;
	// The style source when no sheet is named; read before this call returns, never kept.
	surfaceConfig.styleSource = anchor;
	surfaceConfig.title = config.title.empty() ? String("Search") : config.title;
	surfaceConfig.idPrefix = config.idPrefix.empty() ? String("search-picker") : config.idPrefix;
	surfaceConfig.size = size;
	surfaceConfig.layoutName = String("search-picker-layout");
	surfaceConfig.panelName = String("search-picker");
	surfaceConfig.fallbackColor = s_searchPickerSurfaceColor;
	surfaceConfig.flags = config.flags;
	surfaceConfig.preferNative = config.preferNative;
	// Copied, not moved: SearchPickerContent calls onClose itself on Escape.
	surfaceConfig.onClose = config.onClose;

	/* Captured by copy: on the native path this runs once the popup's scene exists, when the
	opener may be gone. */
	surfaceConfig.makePanel = [config = config](NotNull<SubWindow>, Extent2) mutable -> Rc<Panel> {
		return Rc<SearchPickerContent>::create(sp::move(config));
	};

	return openPopupSurface(window, placementForNode(anchor, side), sp::move(surfaceConfig));
}

} // namespace stappler::xenolith::ui
