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

// CSS Grid placement, plus the track-list parsers and the track-sizing algorithm the table backend
// shares. A subunit of XLUi.scu.cpp - see XLUiLayoutInternal.h.

#include "XLUiLayoutInternal.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

namespace {

// ---- grid working structs -------------------------------------------------

// resolved half-open interval of 0-based track indices [start, end)
struct GridSpan {
	uint32_t start = 0;
	uint32_t end = 0;
	bool definite = false;

	uint32_t span() const { return end - start; }
};

struct GridItem {
	Node *node = nullptr;
	GridItemInfo cfg;

	GridSpan col;
	GridSpan row;

	float natW = 0.0f; // natural (max-content) size along X
	float natH = 0.0f; // natural size along Y

	// filled during positioning (top-left content-box coordinates)
	float boxX = 0.0f;
	float boxY = 0.0f;
	float boxW = 0.0f;
	float boxH = 0.0f;
};
// Read a leading number from `r` (advances r past it). Returns false if there is
// no number at the head.
static bool readNumber(StringView &r, float &out) {
	r.skipChars<StringView::WhiteSpace>();
	auto res = r.readFloat();
	if (!res.valid()) {
		return false;
	}
	out = res.get();
	return true;
}

// Parse one whitespace-delimited grid track token into `out`. Returns false for
// an unsupported token (named lines in brackets, minmax(), fit-content(), ...).
static bool parseGridTrackToken(StringView token, GridTrack &out) {
	token.trimChars<StringView::WhiteSpace>();
	if (token.empty()) {
		return false;
	}
	if (token == "auto" || token == "min-content" || token == "max-content") {
		out.type = GridTrack::Auto;
		out.value = 0.0f;
		return true;
	}
	StringView r(token);
	float num = 0.0f;
	if (!readNumber(r, num)) {
		return false; // named line "[...]" or unsupported function
	}
	r.trimChars<StringView::WhiteSpace>();
	if (r == "fr") {
		out.type = GridTrack::Fraction;
		out.value = num;
	} else if (r.is('%')) {
		out.type = GridTrack::Percent;
		out.value = num;
	} else { // "px", unit-less, or an unsupported length unit -> best-effort px
		out.type = GridTrack::Fixed;
		out.value = num;
	}
	return true;
}

} // namespace

// Parse a track list, expanding repeat(). Recursion depth is bounded by the
// nesting of repeat(), which CSS does not allow, so this is effectively flat.
Vector<GridTrack> parseGridTemplate(StringView input) {
	Vector<GridTrack> ret;
	StringView r(input);
	while (true) {
		r.skipChars<StringView::WhiteSpace>();
		if (r.empty()) {
			break;
		}
		if (r.starts_with("repeat(")) {
			r += 7; // strlen("repeat(")
			r.skipChars<StringView::WhiteSpace>();
			auto cres = r.readFloat();
			const uint32_t n = cres.valid() ? uint32_t(sprt::max(cres.get(), 0.0f)) : 0u;
			r.skipChars<StringView::WhiteSpace>();
			if (r.is(',')) {
				++r;
			}
			// inner track list up to the matching ')'
			const char *innerStart = r.data();
			int depth = 1;
			while (!r.empty() && depth > 0) {
				if (r.is('(')) {
					++depth;
				} else if (r.is(')')) {
					--depth;
					if (depth == 0) {
						break;
					}
				}
				++r;
			}
			StringView inner(innerStart, size_t(r.data() - innerStart));
			if (r.is(')')) {
				++r;
			}
			auto innerTracks = parseGridTemplate(inner);
			for (uint32_t i = 0; i < n; ++i) {
				for (auto &t : innerTracks) { ret.emplace_back(t); }
			}
			continue;
		}
		if (r.is('[')) {
			// named-line block: skip (unsupported)
			while (!r.empty() && !r.is(']')) { ++r; }
			if (r.is(']')) {
				++r;
			}
			continue;
		}
		// read a single whitespace-delimited token
		auto token = r.readUntil<StringView::WhiteSpace>();
		GridTrack track;
		if (parseGridTrackToken(token, track)) {
			ret.emplace_back(track);
		}
	}
	return ret;
}

// helper for parseGridLine: parse "span N" or "N" or "auto"/empty
static void parseLineToken(StringView token, uint32_t &line, uint32_t &span, bool &isSpan) {
	token.trimChars<StringView::WhiteSpace>();
	line = 0;
	span = 0;
	isSpan = false;
	if (token.empty() || token == "auto") {
		return;
	}
	if (token.starts_with("span")) {
		isSpan = true;
		StringView r = token;
		r += 4; // strlen("span")
		float n = 0.0f;
		if (readNumber(r, n)) {
			span = uint32_t(sprt::max(n, 1.0f));
		} else {
			span = 1;
		}
		return;
	}
	StringView r(token);
	float n = 0.0f;
	if (readNumber(r, n)) {
		line = uint32_t(sprt::max(n, 1.0f));
	}
}

bool parseGridLine(StringView input, uint32_t &start, uint32_t &end, uint32_t &span) {
	StringView r(input);
	r.trimChars<StringView::WhiteSpace>();
	if (r.empty()) {
		return false;
	}

	StringView left = r;
	StringView right;
	auto slash = r.find('/');
	if (slash < r.size()) {
		left = r.sub(0, slash);
		right = r.sub(slash + 1);
	}

	uint32_t lLine = 0, lSpan = 0, rLine = 0, rSpan = 0;
	bool lIsSpan = false, rIsSpan = false;
	parseLineToken(left, lLine, lSpan, lIsSpan);

	if (right.empty()) {
		// single value: start line or bare span
		if (lIsSpan) {
			start = 0;
			end = 0;
			span = lSpan;
		} else {
			start = lLine;
			end = 0;
			span = 1;
		}
		return true;
	}

	parseLineToken(right, rLine, rSpan, rIsSpan);
	if (lIsSpan) {
		// "span N / M"
		start = 0;
		end = rLine;
		span = sprt::max(lSpan, 1u);
	} else if (rIsSpan) {
		// "N / span M"
		start = lLine;
		end = 0;
		span = sprt::max(rSpan, 1u);
	} else {
		start = lLine;
		end = rLine;
		span = 1;
	}
	return true;
}

namespace {

// resolve a per-axis line/span placement into a 0-based half-open track interval
static GridSpan resolveGridSpan(uint32_t startLine, uint32_t endLine, uint32_t span) {
	GridSpan out;
	const uint32_t sp = sprt::max(span, 1u);
	if (startLine >= 1 && endLine >= 1) {
		uint32_t s = startLine - 1;
		uint32_t e = endLine - 1;
		if (e < s) {
			const uint32_t t = s;
			s = e;
			e = t;
		}
		if (e <= s) {
			e = s + 1;
		}
		out = {s, e, true};
	} else if (startLine >= 1) {
		const uint32_t s = startLine - 1;
		out = {s, s + sp, true};
	} else if (endLine >= 1) {
		uint32_t e = endLine - 1;
		if (e < 1) {
			e = 1;
		}
		const uint32_t s = (e > sp) ? (e - sp) : 0u;
		out = {s, sprt::max(e, s + 1), true};
	} else {
		out = {0, sp, false};
	}
	return out;
}

// distribution of leftover space along one axis (justify/align-content)
static void gridContentDistribution(GridAlign align, float freeSpace, size_t count, float &offset,
		float &between) {
	offset = 0.0f;
	between = 0.0f;
	if (freeSpace <= 0.0f || count == 0) {
		return;
	}
	const float n = static_cast<float>(count);
	switch (align) {
	case GridAlign::End: offset = freeSpace; break;
	case GridAlign::Center: offset = freeSpace / 2.0f; break;
	case GridAlign::SpaceBetween: between = (count > 1) ? freeSpace / (n - 1.0f) : 0.0f; break;
	case GridAlign::SpaceAround: {
		const float space = freeSpace / n;
		offset = space / 2.0f;
		between = space;
		break;
	}
	case GridAlign::SpaceEvenly: {
		const float space = freeSpace / (n + 1.0f);
		offset = space;
		between = space;
		break;
	}
	default: break; // Start / Stretch / Auto
	}
}

} // namespace

// The track-sizing algorithm, shared with the table backend (declared in XLUiLayoutInternal.h).
// It knows nothing about grid items or table cells - only about TrackContributions - which is why
// two placement models can share it.
void resolveTrackSizes(Vector<GridTrackSize> &tracks, SpanView<TrackContribution> items,
		float axisContent, float gap) {
	const size_t n = tracks.size();
	if (n == 0) {
		return;
	}
	const float gapsTotal = gap * static_cast<float>(n - 1);

	// base sizing for Fixed / Percent; Auto and Fraction start at 0
	for (auto &t : tracks) {
		switch (t.def.type) {
		case GridTrack::Fixed: t.base = sprt::max(t.def.value, 0.0f); break;
		case GridTrack::Percent: t.base = sprt::max(t.def.value / 100.0f * axisContent, 0.0f); break;
		default: t.base = 0.0f; break;
		}
	}

	// content sizing of Auto tracks: single-track contributions first
	for (auto &it : items) {
		if (it.span == 1 && it.start < n && tracks[it.start].def.type == GridTrack::Auto) {
			tracks[it.start].base = sprt::max(tracks[it.start].base, it.size);
		}
	}
	// spanning contributions: grow the Auto tracks they cover to cover the deficit
	for (auto &it : items) {
		const uint32_t end = it.start + it.span;
		if (it.span <= 1 || end > n) {
			continue;
		}
		float covered = gap * static_cast<float>(it.span - 1);
		uint32_t autoCount = 0;
		for (uint32_t t = it.start; t < end; ++t) {
			covered += tracks[t].base;
			if (tracks[t].def.type == GridTrack::Auto) {
				++autoCount;
			}
		}
		const float deficit = it.size - covered;
		if (deficit > 0.0f && autoCount > 0) {
			const float add = deficit / static_cast<float>(autoCount);
			for (uint32_t t = it.start; t < end; ++t) {
				if (tracks[t].def.type == GridTrack::Auto) {
					tracks[t].base += add;
				}
			}
		}
	}

	// distribute positive free space across fr tracks
	float sumBase = 0.0f;
	float sumFr = 0.0f;
	for (auto &t : tracks) {
		sumBase += t.base;
		if (t.def.type == GridTrack::Fraction) {
			sumFr += sprt::max(t.def.value, 0.0f);
		}
	}
	const float freeSpace = axisContent - gapsTotal - sumBase;
	if (freeSpace > 0.0f && sumFr > 0.0f) {
		for (auto &t : tracks) {
			if (t.def.type == GridTrack::Fraction) {
				t.base = freeSpace * sprt::max(t.def.value, 0.0f) / sumFr;
			}
		}
	}
	for (auto &t : tracks) { t.base = sprt::max(t.base, 0.0f); }
}

void positionTrackSizes(Vector<GridTrackSize> &tracks, float axisContent, float gap,
		GridAlign contentAlign) {
	const size_t n = tracks.size();
	if (n == 0) {
		return;
	}
	float sumBase = 0.0f;
	for (auto &t : tracks) { sumBase += t.base; }
	const float gapsTotal = gap * static_cast<float>(n - 1);
	const float freeSpace = axisContent - sumBase - gapsTotal;

	float offset = 0.0f, between = 0.0f;
	gridContentDistribution(contentAlign, freeSpace, n, offset, between);

	float pos = offset;
	for (auto &t : tracks) {
		t.position = pos;
		pos += t.base + gap + between;
	}
}

void LayoutSystem::layoutGrid() {
	auto infoPtr = _owner->getComponent<GridLayoutInfo>();
	const GridLayoutInfo info = infoPtr ? *infoPtr : GridLayoutInfo();

	const Size2 containerSize = _owner->getContentSize();
	const float contentW = sprt::max(containerSize.width - info.padding.horizontal(), 0.0f);
	const float contentH = sprt::max(containerSize.height - info.padding.vertical(), 0.0f);

	// 1. Collect items.
	Vector<GridItem> items;
	for (auto &child : _owner->getChildren()) {
		// collapsed when explicitly invisible or `display: none`; a `visibility: hidden`
		// child keeps its layout box (isDisplayed stays true)
		if (!child->isDisplayed()) {
			continue;
		}
		// out of the grid flow entirely, like an absolutely positioned box (OutOfFlowComponent)
		if (child->getComponent<OutOfFlowComponent>()) {
			continue;
		}
		GridItem item;
		item.node = child;
		if (auto cfg = child->getComponent<GridItemInfo>()) {
			item.cfg = *cfg;
		}
		const Size2 cs = intrinsicSize(child);
		item.natW = (item.cfg.width >= 0.0f) ? item.cfg.width : cs.width;
		item.natH = (item.cfg.height >= 0.0f) ? item.cfg.height : cs.height;
		item.col = resolveGridSpan(item.cfg.gridColumnStart, item.cfg.gridColumnEnd,
				item.cfg.columnSpan);
		item.row = resolveGridSpan(item.cfg.gridRowStart, item.cfg.gridRowEnd, item.cfg.rowSpan);
		items.emplace_back(item);
	}
	if (items.empty()) {
		return;
	}

	// stable-sort by order (insertion sort; tiny counts)
	for (size_t i = 1; i < items.size(); ++i) {
		GridItem key = items[i];
		size_t j = i;
		while (j > 0 && items[j - 1].cfg.order > key.cfg.order) {
			items[j] = items[j - 1];
			--j;
		}
		items[j] = key;
	}

	// 2. Placement. Work in minor (fixed, wrapping) / major (growing) axes.
	const bool rowFlow =
			info.autoFlow == GridAutoFlow::Row || info.autoFlow == GridAutoFlow::RowDense;
	const bool dense =
			info.autoFlow == GridAutoFlow::RowDense || info.autoFlow == GridAutoFlow::ColumnDense;

	const uint32_t minorBase = uint32_t(rowFlow ? info.columnTracks.size() : info.rowTracks.size());
	const uint32_t majorBase = uint32_t(rowFlow ? info.rowTracks.size() : info.columnTracks.size());

	// per-item minor / major spans (as pointers into col/row by flow)
	auto minorOf = [&](GridItem &it) -> GridSpan & { return rowFlow ? it.col : it.row; };
	auto majorOf = [&](GridItem &it) -> GridSpan & { return rowFlow ? it.row : it.col; };

	// finalize the minor track count (the wrap dimension)
	uint32_t W = minorBase;
	for (auto &it : items) {
		auto &mn = minorOf(it);
		W = sprt::max(W, mn.definite ? mn.end : mn.span());
	}
	W = sprt::max(W, 1u);

	uint32_t M = majorBase;
	Vector<uint8_t> occ;
	occ.resize(size_t(M) * W, 0);
	auto ensureMajor = [&](uint32_t m) {
		if (m > M) {
			occ.resize(size_t(m) * W, 0);
			M = m;
		}
	};
	auto isFree = [&](uint32_t maj, uint32_t majSpan, uint32_t mn, uint32_t mnSpan) -> bool {
		for (uint32_t a = maj; a < maj + majSpan; ++a) {
			for (uint32_t b = mn; b < mn + mnSpan; ++b) {
				if (occ[size_t(a) * W + b]) {
					return false;
				}
			}
		}
		return true;
	};
	auto mark = [&](uint32_t maj, uint32_t majSpan, uint32_t mn, uint32_t mnSpan) {
		for (uint32_t a = maj; a < maj + majSpan; ++a) {
			for (uint32_t b = mn; b < mn + mnSpan; ++b) { occ[size_t(a) * W + b] = 1; }
		}
	};
	auto commit = [&](GridItem &it, uint32_t maj, uint32_t mn) {
		auto &mnAxis = minorOf(it);
		auto &mjAxis = majorOf(it);
		// capture spans BEFORE mutating start (span() == end - start)
		const uint32_t mnSp = mnAxis.span();
		const uint32_t mjSp = mjAxis.span();
		mnAxis.start = mn;
		mnAxis.end = mn + mnSp;
		mjAxis.start = maj;
		mjAxis.end = maj + mjSp;
	};

	// Phase 1: items definite in both axes.
	for (auto &it : items) {
		auto &mn = minorOf(it);
		auto &mj = majorOf(it);
		if (mn.definite && mj.definite) {
			ensureMajor(mj.end);
			// clamp minor into the grid (definite W already covers it)
			mark(mj.start, mj.span(), mn.start, mn.span());
		}
	}

	// Phases 2 & 3: everything else in DOM/order sequence, with a shared cursor
	// for the fully-auto items.
	uint32_t curMajor = 0, curMinor = 0;
	for (auto &it : items) {
		auto &mn = minorOf(it);
		auto &mj = majorOf(it);
		if (mn.definite && mj.definite) {
			continue; // placed in phase 1
		}
		const uint32_t mnSpan = mn.span();
		const uint32_t mjSpan = mj.span();

		if (mn.definite && !mj.definite) {
			// minor-locked: scan the major axis for a free band
			uint32_t maj = 0;
			while (true) {
				ensureMajor(maj + mjSpan);
				if (isFree(maj, mjSpan, mn.start, mnSpan)) {
					break;
				}
				++maj;
			}
			mark(maj, mjSpan, mn.start, mnSpan);
			commit(it, maj, mn.start);
		} else if (!mn.definite && mj.definite) {
			// major-locked: scan the minor axis within the fixed major band
			ensureMajor(mj.end);
			bool placed = false;
			for (uint32_t b = 0; mnSpan <= W && b + mnSpan <= W; ++b) {
				if (isFree(mj.start, mjSpan, b, mnSpan)) {
					mark(mj.start, mjSpan, b, mnSpan);
					commit(it, mj.start, b);
					placed = true;
					break;
				}
			}
			if (!placed) {
				// overflow: pin to minor 0
				mark(mj.start, mjSpan, 0, mnSpan);
				commit(it, mj.start, 0);
			}
		} else {
			// fully auto: cursor packing
			if (dense) {
				curMajor = 0;
				curMinor = 0;
			}
			while (true) {
				if (curMinor + mnSpan > W) {
					curMinor = 0;
					++curMajor;
				}
				ensureMajor(curMajor + mjSpan);
				if (isFree(curMajor, mjSpan, curMinor, mnSpan)) {
					break;
				}
				++curMinor;
			}
			mark(curMajor, mjSpan, curMinor, mnSpan);
			commit(it, curMajor, curMinor);
			if (!dense) {
				curMinor += mnSpan;
			}
		}
	}

	const uint32_t colCount = rowFlow ? W : M;
	const uint32_t rowCount = rowFlow ? M : W;

	// 3. Track sizing (independent per axis).
	auto buildTracks = [&](uint32_t count, const Vector<GridTrack> &explicitTracks,
							   const GridTrack &autoDefault) {
		Vector<GridTrackSize> tracks;
		tracks.resize(count);
		for (uint32_t i = 0; i < count; ++i) {
			tracks[i].def = (i < explicitTracks.size()) ? explicitTracks[i] : autoDefault;
		}
		return tracks;
	};
	Vector<GridTrackSize> cols = buildTracks(colCount, info.columnTracks, info.autoColumn);
	Vector<GridTrackSize> rows = buildTracks(rowCount, info.rowTracks, info.autoRow);

	// Project the placed items onto each axis as track contributions - the only thing the sizing
	// algorithm needs from them, and what lets the table backend reuse it verbatim.
	auto contributions = [&](bool isColumn) {
		Vector<TrackContribution> ret;
		ret.reserve(items.size());
		for (auto &it : items) {
			const GridSpan &s = isColumn ? it.col : it.row;
			ret.emplace_back(TrackContribution{s.start, s.span(), isColumn ? it.natW : it.natH});
		}
		return ret;
	};
	resolveTrackSizes(cols, contributions(true), contentW, info.columnGap);
	resolveTrackSizes(rows, contributions(false), contentH, info.rowGap);

	// 4. Positioning: track offsets + content distribution.
	positionTrackSizes(cols, contentW, info.columnGap, info.justifyContent);
	positionTrackSizes(rows, contentH, info.rowGap, info.alignContent);

	// 5. Per-item cell rect, self-alignment, and projection to bottom-left space.
	auto selfAlign = [](GridAlign a, GridAlign fallback, float cellStart, float cellSize,
							 float natural, float &outStart, float &outSize) {
		GridAlign use = (a == GridAlign::Auto) ? fallback : a;
		if (use == GridAlign::Stretch || use == GridAlign::Auto) {
			outStart = cellStart;
			outSize = cellSize;
			return;
		}
		outSize = natural;
		switch (use) {
		case GridAlign::End: outStart = cellStart + (cellSize - natural); break;
		case GridAlign::Center: outStart = cellStart + (cellSize - natural) / 2.0f; break;
		default: outStart = cellStart; break; // Start
		}
	};

	for (auto &it : items) {
		if (it.col.end == 0 || it.row.end == 0 || it.col.end > cols.size()
				|| it.row.end > rows.size()) {
			continue; // safety: unplaced / out of range
		}
		const float cellX = cols[it.col.start].position;
		const float cellRight = cols[it.col.end - 1].position + cols[it.col.end - 1].base;
		const float cellY = rows[it.row.start].position;
		const float cellBottom = rows[it.row.end - 1].position + rows[it.row.end - 1].base;

		// inset the cell by the item margin
		float availX = cellX + it.cfg.margin.left;
		float availW = sprt::max((cellRight - cellX) - it.cfg.margin.horizontal(), 0.0f);
		float availY = cellY + it.cfg.margin.top;
		float availH = sprt::max((cellBottom - cellY) - it.cfg.margin.vertical(), 0.0f);

		float x = availX, w = availW, y = availY, h = availH;
		selfAlign(it.cfg.justifySelf, info.justifyItems, availX, availW, it.natW, x, w);
		selfAlign(it.cfg.alignSelf, info.alignItems, availY, availH, it.natH, y, h);
		w = sprt::max(w, 0.0f);
		h = sprt::max(h, 0.0f);

		it.boxX = x;
		it.boxY = y;
		it.boxW = w;
		it.boxH = h;

		Vec2 bottomLeft;
		bottomLeft.x = info.padding.left + it.boxX;
		bottomLeft.y = containerSize.height - info.padding.top - it.boxY - it.boxH;

		it.node->setContentSize(Size2(it.boxW, it.boxH));
		const Vec2 anchor = it.node->getAnchorPoint();
		it.node->setPosition(bottomLeft + Vec2(anchor.x * it.boxW, anchor.y * it.boxH));
	}
}

} // namespace stappler::xenolith::ui
