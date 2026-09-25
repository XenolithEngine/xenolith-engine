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

#include "XLUiRowSelection.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

ListSelectionOp getListSelectionOp(InputModifier mods) {
#if SPRT_MACOS
	const bool toggle = hasFlag(mods, InputModifier::Command);
#else
	const bool toggle = hasFlag(mods, InputModifier::Ctrl);
#endif
	const bool range = hasFlag(mods, InputModifier::Shift);
	if (toggle && range) {
		return ListSelectionOp::AddRange;
	} else if (range) {
		return ListSelectionOp::Range;
	} else if (toggle) {
		return ListSelectionOp::Toggle;
	}
	return ListSelectionOp::Replace;
}

bool isListSelectionAdditive(ListSelectionOp op) {
	return op == ListSelectionOp::Toggle || op == ListSelectionOp::AddRange;
}

bool ListSelectionState::contains(size_t index) const {
	auto it = sprt::lower_bound(rows.begin(), rows.end(), index);
	return it != rows.end() && *it == index;
}

static void ListSelectionState_insert(ListSelectionState &state, size_t index) {
	auto it = sprt::lower_bound(state.rows.begin(), state.rows.end(), index);
	if (it == state.rows.end() || *it != index) {
		state.rows.insert(it, index);
	}
}

static void ListSelectionState_addRange(ListSelectionState &state, size_t from, size_t to) {
	for (auto i = sprt::min(from, to); i <= sprt::max(from, to); ++i) {
		ListSelectionState_insert(state, i);
	}
}

bool applyListSelection(ListSelectionState &state, size_t index, ListSelectionOp op,
		size_t count) {
	if (index >= count) {
		return false;
	}

	auto prev = state.rows;
	const auto prevCurrent = state.current;
	const auto anchor = state.anchor < count ? state.anchor : index;

	switch (op) {
	case ListSelectionOp::Replace:
		state.rows.clear();
		state.rows.emplace_back(index);
		state.anchor = index;
		break;
	case ListSelectionOp::Toggle:
		if (auto it = sprt::lower_bound(state.rows.begin(), state.rows.end(), index);
				it != state.rows.end() && *it == index) {
			state.rows.erase(it);
		} else {
			ListSelectionState_insert(state, index);
		}
		state.anchor = index;
		break;
	case ListSelectionOp::Range:
		state.rows.clear();
		ListSelectionState_addRange(state, anchor, index);
		state.anchor = anchor;
		break;
	case ListSelectionOp::AddRange:
		ListSelectionState_addRange(state, anchor, index);
		state.anchor = anchor;
		break;
	}
	state.current = index;
	return state.rows != prev || state.current != prevCurrent;
}

bool extendListSelection(ListSelectionState &state, size_t index, size_t count) {
	return applyListSelection(state, index, ListSelectionOp::Range, count);
}

bool selectAllRows(ListSelectionState &state, size_t count) {
	if (count == 0) {
		return false;
	}
	auto prev = state.rows;
	state.rows.clear();
	state.rows.reserve(count);
	for (size_t i = 0; i < count; ++i) { state.rows.emplace_back(i); }
	if (state.current >= count) {
		state.current = 0;
	}
	if (state.anchor >= count) {
		state.anchor = state.current;
	}
	return state.rows != prev;
}

ListSelectionOp getListSweepOp(InputModifier mods) {
	switch (getListSelectionOp(mods)) {
	case ListSelectionOp::Replace: return ListSelectionOp::Replace;
	case ListSelectionOp::Toggle: return ListSelectionOp::Toggle;
	case ListSelectionOp::Range:
	case ListSelectionOp::AddRange: return ListSelectionOp::AddRange;
	}
	return ListSelectionOp::Replace;
}

bool applyListSweep(ListSelectionState &state, SpanView<size_t> hits, ListSelectionOp op,
		size_t count, size_t anchor, size_t current) {
	Vector<size_t> rows;
	rows.reserve(state.rows.size() + hits.size());

	// Both are ascending, so one merge pass: the hits alone, their union, or what only one has
	auto l = state.rows.begin();
	auto r = hits.begin();
	while (l != state.rows.end() || r != hits.end()) {
		if (r != hits.end() && *r >= count) {
			++r;
			continue;
		}
		const bool takeLeft = r == hits.end() || (l != state.rows.end() && *l < *r);
		const bool takeRight = l == state.rows.end() || (r != hits.end() && *r < *l);
		if (takeLeft) {
			if (op != ListSelectionOp::Replace) {
				rows.emplace_back(*l);
			}
			++l;
		} else if (takeRight) {
			rows.emplace_back(*r);
			++r;
		} else {
			if (op != ListSelectionOp::Toggle) {
				rows.emplace_back(*l);
			}
			++l;
			++r;
		}
	}

	const bool changed = rows != state.rows || (current < count && current != state.current);
	state.rows = sp::move(rows);
	if (anchor < count) {
		state.anchor = anchor;
	}
	if (current < count) {
		state.current = current;
	}
	return changed;
}

bool ListSweep::isHit(size_t index) const {
	auto it = sprt::lower_bound(hits.begin(), hits.end(), index);
	return it != hits.end() && *it == index;
}

bool ListSweep::isShown(size_t index, bool selected) const {
	if (!active) {
		return selected;
	}
	switch (op) {
	case ListSelectionOp::Replace: return isHit(index);
	case ListSelectionOp::Toggle: return selected != isHit(index);
	case ListSelectionOp::Range:
	case ListSelectionOp::AddRange: return selected || isHit(index);
	}
	return selected;
}

void RowSelection::setMode(ListSelectionMode mode) { _mode = mode; }

void RowSelection::remap(size_t count, const IdentityFn &fn) {
	_state.rows.clear();
	_state.current = maxOf<size_t>();
	_state.anchor = maxOf<size_t>();
	if (_items.empty() && _current.empty() && _anchor.empty()) {
		return;
	}

	// One pass over the rows, each looked up among the identities in their sorted order
	auto less = [](const RowIdentity &l, const RowIdentity &r) {
		return l.id.get() != r.id.get() ? l.id.get() < r.id.get() : l.offset < r.offset;
	};
	auto sorted = _items;
	sprt::sort(sorted.begin(), sorted.end(), less);

	for (size_t i = 0; i < count; ++i) {
		const auto id = fn(i);
		if (id.empty()) {
			continue;
		}
		auto it = sprt::lower_bound(sorted.begin(), sorted.end(), id, less);
		if (it != sorted.end() && *it == id) {
			_state.rows.emplace_back(i);
		}
		if (id == _current) {
			_state.current = i;
		}
		if (id == _anchor) {
			_state.anchor = i;
		}
	}
}

bool RowSelection::press(size_t index, ListSelectionOp op, size_t count, const IdentityFn &fn) {
	if (_mode == ListSelectionMode::Single) {
		op = ListSelectionOp::Replace;
	}
	auto state = _state;
	applyListSelection(state, index, op, count);
	return commit(sp::move(state), isListSelectionAdditive(op), count, fn);
}

bool RowSelection::extend(size_t index, size_t count, const IdentityFn &fn) {
	if (_mode == ListSelectionMode::Single) {
		return false;
	}
	auto state = _state;
	extendListSelection(state, index, count);
	return commit(sp::move(state), false, count, fn);
}

bool RowSelection::selectAll(size_t count, const IdentityFn &fn) {
	if (_mode == ListSelectionMode::Single) {
		return false;
	}
	auto state = _state;
	selectAllRows(state, count);
	return commit(sp::move(state), false, count, fn);
}

bool RowSelection::set(SpanView<size_t> rows, size_t current, size_t count,
		const IdentityFn &fn) {
	ListSelectionState state;
	for (auto it : rows) {
		if (it < count) {
			ListSelectionState_insert(state, it);
			if (_mode == ListSelectionMode::Single) {
				break;
			}
		}
	}
	if (_mode == ListSelectionMode::Single || current >= count) {
		current = state.rows.empty() ? maxOf<size_t>() : state.rows.front();
	}
	state.current = current;
	state.anchor = _state.anchor < count ? _state.anchor : current;
	return commit(sp::move(state), false, count, fn);
}

bool RowSelection::sweep(SpanView<size_t> hits, ListSelectionOp op, size_t anchor,
		size_t current, size_t count, const IdentityFn &fn) {
	if (_mode == ListSelectionMode::Single) {
		return false;
	}
	auto state = _state;
	applyListSweep(state, hits, op, count, anchor, current);
	return commit(sp::move(state), op != ListSelectionOp::Replace, count, fn);
}

bool RowSelection::setIdentities(SpanView<RowIdentity> ids, size_t count, const IdentityFn &fn) {
	Vector<RowIdentity> items;
	for (auto &it : ids) {
		if (it.empty()) {
			continue;
		}
		bool seen = false;
		for (auto &known : items) { seen = seen || known == it; }
		if (!seen) {
			items.emplace_back(it);
		}
		if (_mode == ListSelectionMode::Single) {
			break;
		}
	}

	const auto current = items.empty() ? RowIdentity() : items.front();
	bool anchored = false;
	for (auto &it : items) { anchored = anchored || it == _anchor; }

	const bool changed = items != _items || current != _current;
	_items = sp::move(items);
	_current = current;
	if (!anchored) {
		_anchor = current;
	}
	remap(count, fn);
	return changed;
}

bool RowSelection::clear() {
	const bool changed = !_items.empty() || !_current.empty();
	_items.clear();
	_current = RowIdentity();
	_anchor = RowIdentity();
	_state = ListSelectionState();
	return changed;
}

bool RowSelection::commit(ListSelectionState &&state, bool keepHidden, size_t count,
		const IdentityFn &fn) {
	Vector<RowIdentity> items;
	items.reserve(state.rows.size());
	for (auto it : state.rows) { items.emplace_back(fn(it)); }

	/* What no row shows now is kept by an additive pick and dropped by any other. Shown is what
	the previous state indexed, which remap() keeps in step with the rows. */
	if (keepHidden && _items.size() > _state.rows.size()) {
		Vector<RowIdentity> shown;
		shown.reserve(_state.rows.size());
		for (auto it : _state.rows) { shown.emplace_back(fn(it)); }
		for (auto &it : _items) {
			bool found = false;
			for (auto &s : shown) { found = found || s == it; }
			if (!found) {
				items.emplace_back(it);
			}
		}
	}

	auto current = state.current < count ? fn(state.current) : RowIdentity();
	auto anchor = state.anchor < count ? fn(state.anchor) : RowIdentity();

	const bool changed = items != _items || current != _current;
	_items = sp::move(items);
	_current = current;
	_anchor = anchor;
	_state = sp::move(state);
	return changed;
}

} // namespace stappler::xenolith::ui
