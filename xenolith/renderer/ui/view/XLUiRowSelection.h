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

#ifndef XENOLITH_RENDERER_UI_VIEW_XLUIROWSELECTION_H_
#define XENOLITH_RENDERER_UI_VIEW_XLUIROWSELECTION_H_

#include "XLUiConfig.h"
#include "XLInput.h"
#include "SPDataModel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/** The selection of a list of rows: which are picked out, the current one the keyboard and an
activation act on, and the anchor a range runs from. Shared by TableView and TreeView; the index
half below is for any list of its own, such as a grid of cards. */

enum class ListSelectionMode : uint8_t {
	Single, // one row; every pick replaces it
	Multiple, // a set; see ListSelectionOp
};

// What a pick does to a multiple selection.
enum class ListSelectionOp : uint8_t {
	Replace, // the row alone
	Toggle, // the row in or out, the rest kept
	Range, // the rows from the anchor to this one, instead of the rest
	AddRange, // the same run, added to the rest
};

/* The operation a press asks for: the toggle modifier (Ctrl, Command on macOS) toggles, Shift
takes a range, both add the range; no modifier replaces. */
SP_PUBLIC ListSelectionOp getListSelectionOp(InputModifier);

// Whether an operation keeps what was picked out before it, rows a list does not show included.
SP_PUBLIC bool isListSelectionAdditive(ListSelectionOp);

struct SP_PUBLIC ListSelectionState {
	Vector<size_t> rows; // ascending, no repeats
	size_t anchor = maxOf<size_t>();
	size_t current = maxOf<size_t>();

	bool contains(size_t) const;
};

/* A pick of row `index` of `count` rows. Replace and Toggle move the anchor to the row, a range
leaves it where it is; the current row is always the one picked. False when nothing changed. */
SP_PUBLIC bool applyListSelection(ListSelectionState &, size_t index, ListSelectionOp,
		size_t count);

// A keyboard step with Shift: the run from the anchor to `index`, which becomes current.
SP_PUBLIC bool extendListSelection(ListSelectionState &, size_t index, size_t count);

// Every row; the current and the anchor stay where they are, or go to the first row.
SP_PUBLIC bool selectAllRows(ListSelectionState &, size_t count);

/* The operation a band swept over a list asks for: the toggle modifier toggles what it covers,
Shift with or without it adds, no modifier replaces. A band has no run from the anchor, so there
is no Range. */
SP_PUBLIC ListSelectionOp getListSweepOp(InputModifier);

/* A band over `hits` (ascending, below `count`) against the selection it started from: Replace
takes the hits alone, Toggle flips each of them, Range and AddRange add them. The anchor and the
current row go to the hits the caller names, and stay where they were with no hits. False when
nothing changed. */
SP_PUBLIC bool applyListSweep(ListSelectionState &, SpanView<size_t> hits, ListSelectionOp,
		size_t count, size_t anchor, size_t current);

/* A band in progress: what it covers and how the release applies it. The selection itself is
untouched until then; isShown() tells how a row is drawn meanwhile. */
struct SP_PUBLIC ListSweep {
	Vector<size_t> hits; // ascending
	ListSelectionOp op = ListSelectionOp::Replace;
	size_t anchor = maxOf<size_t>();
	size_t current = maxOf<size_t>();
	bool active = false;

	bool isHit(size_t) const;

	// Whether a row that is `selected` now would be after the release.
	bool isShown(size_t, bool selected) const;
};

// A row as a model knows it: its node, and for a span row the index in the span.
struct SP_PUBLIC RowIdentity {
	data::Model::ItemId id = data::Model::ItemId(0);
	uint64_t offset = 0;

	bool empty() const { return id == data::Model::ItemId(0); }
	bool operator==(const RowIdentity &other) const {
		return id == other.id && offset == other.offset;
	}
	bool operator!=(const RowIdentity &other) const { return !(*this == other); }
};

/* A selection held by identity, so it follows its rows across rebuilds. The indices are derived by
remap(). An identity no row shows now - a row in a collapsed branch - stays selected with no index,
and an additive pick keeps it. In the single mode every pick is a Replace. */
class SP_PUBLIC RowSelection {
public:
	using IdentityFn = Callback<RowIdentity(size_t)>;

	void setMode(ListSelectionMode);
	ListSelectionMode getMode() const { return _mode; }

	const ListSelectionState &getState() const { return _state; }
	size_t getCurrent() const { return _state.current; }
	size_t getAnchor() const { return _state.anchor; }
	SpanView<size_t> getRows() const { return _state.rows; }
	bool isSelected(size_t index) const { return _state.contains(index); }
	bool empty() const { return _items.empty(); }

	const RowIdentity &getCurrentIdentity() const { return _current; }
	SpanView<RowIdentity> getIdentities() const { return _items; }

	// The indices from the identities, after the rows changed.
	void remap(size_t count, const IdentityFn &);

	// Each answers whether the selected rows or the current one changed.
	bool press(size_t index, ListSelectionOp, size_t count, const IdentityFn &);
	bool extend(size_t index, size_t count, const IdentityFn &);
	bool selectAll(size_t count, const IdentityFn &);

	// Exactly these rows; the anchor stays while its row is among the rows shown.
	bool set(SpanView<size_t> rows, size_t current, size_t count, const IdentityFn &);

	/* A band released over `hits`, see applyListSweep. Toggle and the adding ops keep what no row
	shows now; Replace drops it. False in the single mode. */
	bool sweep(SpanView<size_t> hits, ListSelectionOp, size_t anchor, size_t current, size_t count,
			const IdentityFn &);

	// Exactly these identities, the first of them current.
	bool setIdentities(SpanView<RowIdentity>, size_t count, const IdentityFn &);

	bool clear();

protected:
	bool commit(ListSelectionState &&, bool keepHidden, size_t count, const IdentityFn &);

	ListSelectionMode _mode = ListSelectionMode::Single;
	ListSelectionState _state;
	Vector<RowIdentity> _items;
	RowIdentity _current;
	RowIdentity _anchor;
};

} // namespace stappler::xenolith::ui

#endif /* XENOLITH_RENDERER_UI_VIEW_XLUIROWSELECTION_H_ */
