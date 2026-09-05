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

#include "XLUiTextView.h"

#include "SPString.h"
#include "XLAppThread.h"
#include "XLDirector.h"
#include "XLHotkey.h"
#include "XLInputListener.h"
#include "XLScene.h"
#include "XLSceneContent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// A wheel notch moves three lines, the same amount every desktop editor uses. Expressed in lines
// rather than pixels so it stays right at any font size.
static constexpr float kWheelLines = 3.0f;

// How close to an edge the caret may come before the viewport follows it. Horizontally it is capped
// against the box, because a margin wider than a quarter of the viewport would leave the caret
// permanently re-centring.
static constexpr float kScrollMarginX = 48.0f;

// Speed of the pull when a drag-selection parks the pointer outside the box, px/s. Matches the
// stock single-line field, so the two feel the same.
static constexpr float kAutoScrollSpeed = 300.0f;

// Distance from an edge at which that pull starts.
static constexpr float kAutoScrollEdge = 48.0f;

static constexpr float kCaretWidth = 1.5f;

// Global selection or marked range cut down to one block: the piece the block's label draws, in
// the label's own indices. Blocks never contain the '\n' between lines, so a selection crossing
// it simply ends at one label and begins at the next.
static TextCursor sliceCursorToSpan(TextCursor c, const TextDocument::BlockSpan &span) {
	if (c == TextCursor::InvalidCursor || c.length == 0) {
		return TextCursor::InvalidCursor;
	}
	const uint64_t selStart = c.start;
	const uint64_t selEnd = uint64_t(c.start) + c.length;
	const uint64_t spanStart = span.start;
	const uint64_t spanEnd = uint64_t(span.start) + span.length;
	const uint64_t s = sprt::max(selStart, spanStart);
	const uint64_t e = sprt::min(selEnd, spanEnd);
	if (e <= s) {
		return TextCursor::InvalidCursor;
	}
	return TextCursor(uint32_t(s - spanStart), uint32_t(e - s));
}

// ===========================================================================
// TextViewContainer
// ===========================================================================

bool TextViewContainer::init() {
	if (!TextInputContainer::init()) {
		return false;
	}

	// A child of the container rather than of a label, so the horizontal slide does not take the
	// strip with it: the highlight marks a line across the whole viewport, not a run of glyphs.
	// ZOrder(-1) puts it under the text; the scissor is ApplyForAll, so it is still clipped.
	_currentLine = addChild(Rc<basic2d::Layer>::create(Color::White), ZOrder(-1));
	_currentLine->setAnchorPoint(Anchor::BottomLeft);
	_currentLine->addStyleClass("text-view-current-line");
	_currentLine->setVisible(false);

	_stage = addChild(Rc<Node>::create());
	_stage->setName("text-view-stage");
	_stage->setAnchorPoint(Anchor::BottomLeft);

	// The caret was created by the base as a child of ITS label so a single-line slide would move
	// both for free. Here labels come and go with the scroll, so the caret moves to the stage and
	// is positioned explicitly. The local Rc keeps it alive across the swap.
	Rc<basic2d::Layer> caret(_caret);
	_caret->removeFromParent(false);
	_stage->addChild(sp::move(caret), ZOrder(2));

	// The reference for the uniform line height and the monospace cell width. Same type and class
	// as the content labels, so the CSS resolver styles it identically; VISIBLE, because an
	// invisible node is never visited and therefore never styled or laid out - it merely sits
	// above the viewport where the scissor clips it. See handleContentSizeDirty.
	_measure = _stage->addChild(Rc<basic2d::Label>::create());
	_measure->setAnchorPoint(Anchor::BottomLeft);
	_measure->setType("label");
	_measure->addStyleClass("xl-ui-text-input-label");
	_measure->setString("0");
	_measure->setPersistentGlyphData(true);

	return true;
}

void TextViewContainer::update(const UpdateTime &time) {
	// Not TextInputContainer::update: it pulls on X only, and its early-out asks
	// hasHorizontalOverflow(), so a vertical drag past the bottom edge of a non-overflowing-wide
	// document would never move. Node::update is what the base would have called first anyway.
	Node::update(time);

	// Vec2::INVALID is a pair of NaNs, and NaN compares equal to nothing - "is a target set" has to
	// be asked as isValid(), never as == Vec2::INVALID.
	if (!_autoScrollTarget.isValid()) {
		return;
	}

	// The pointer is parked outside the box, so no further gesture event is coming and the motion
	// has to come from the clock.
	const auto local = convertToNodeSpace(_autoScrollTarget);
	const auto edgeX = sprt::min(kAutoScrollEdge, _contentSize.width / 3.0f);
	const auto edgeY = sprt::min(kAutoScrollEdge, _contentSize.height / 3.0f);

	Vec2 delta;
	if (local.x < edgeX) {
		delta.x = -(1.0f - math::clamp(local.x / edgeX, 0.0f, 1.0f));
	} else if (local.x > _contentSize.width - edgeX) {
		delta.x = 1.0f - math::clamp((_contentSize.width - local.x) / edgeX, 0.0f, 1.0f);
	}

	// Node space is Y-up and the scroll offset is Y-down, so the sign flips: below the bottom edge
	// (small local.y) means "scroll further down the document".
	if (local.y < edgeY) {
		delta.y = 1.0f - math::clamp(local.y / edgeY, 0.0f, 1.0f);
	} else if (local.y > _contentSize.height - edgeY) {
		delta.y = -(1.0f - math::clamp((_contentSize.height - local.y) / edgeY, 0.0f, 1.0f));
	}

	if (delta != Vec2::ZERO) {
		scrollBy(delta * kAutoScrollSpeed * time.dt);
	}
}

void TextViewContainer::handleContentSizeDirty() {
	// Node, not TextInputContainer: the base pins the placeholder to the bottom of the box and
	// gives the caret the full box height, and a multi-line view wants the top and one line.
	Node::handleContentSizeDirty();

	// Text starts at the top of a multi-line view, so the placeholder does too.
	_placeholder->setPosition(
			Vec2(0.0f, _contentSize.height - _placeholder->getContentSize().height));

	_stage->setContentSize(_contentSize);

	// Above the box, inside the scissor: visited (so styled and measured), never on screen.
	_measure->setPosition(Vec2(0.0f, _contentSize.height + 16.0f));

	_caretDirty = true;
}

bool TextViewContainer::visitDraw(FrameInfo &frame, NodeVisitFlags parentFlags) {
	if (!_visible) {
		return false;
	}

	// The order is the contract. The font is measured first because every geometry below is in
	// its units; the caret-follow runs before materialization so the window is built around the
	// final offset; and all of it runs before the base, whose visitDraw flushes _caretDirty into
	// updateCaretPosition against the labels this pass has just settled.
	measureFont();
	if (_doc && _lineHeight > 0.0f) {
		if (_followCursor) {
			_followCursor = false;
			scrollToCursor();
		}
		materialize();
	}

	return TextInputContainer::visitDraw(frame, parentFlags);
}

void TextViewContainer::handleLabelChanged() {
	// The base resets caret geometry state for ITS label; the model view recomputes everything
	// per frame, so only the flag matters.
	TextInputContainer::handleLabelChanged();
	_caretDirty = true;
}

void TextViewContainer::measureFont() {
	_measure->tryUpdateLabel();
	const auto size = _measure->getContentSize();
	if (size.height > 0.0f && (size.height != _lineHeight || size.width != _cellWidth)) {
		// The measure label answers both numbers at once: its height is one line box, its width
		// the advance of "0" - the monospace cell. Re-read every frame because the font loads
		// asynchronously and a density change re-lays the label out with no notification.
		_lineHeight = size.height;
		_cellWidth = size.width;
		_caretDirty = true;
	}
}

double TextViewContainer::docHeight() const {
	return _doc ? double(_doc->getTotalRows()) * double(_lineHeight) : 0.0;
}

Size2 TextViewContainer::getScrollRange() const {
	if (!_doc || _lineHeight <= 0.0f) {
		return Size2::ZERO;
	}
	return Size2(float(sprt::max(double(_maxBlockWidth) - double(_contentSize.width), 0.0)),
			float(sprt::max(docHeight() - double(_contentSize.height), 0.0)));
}

void TextViewContainer::clampScroll() {
	const double maxY = sprt::max(docHeight() - double(_contentSize.height), 0.0);
	const double maxX = sprt::max(double(_maxBlockWidth) - double(_contentSize.width), 0.0);
	_scrollY = math::clamp(_scrollY, 0.0, maxY);
	_scrollX = math::clamp(_scrollX, 0.0, maxX);
}

void TextViewContainer::setScrollOffset(const Vec2 &offset) {
	const auto prev = getScrollOffset();
	_scrollX = double(offset.x);
	_scrollY = double(offset.y);
	clampScroll();
	if (getScrollOffset() != prev) {
		_caretDirty = true;
	}
}

void TextViewContainer::scrollBy(const Vec2 &d) {
	// The delta path adds in double before clamping, so a long wheel session deep in a huge
	// document does not accumulate float error.
	const auto prev = getScrollOffset();
	_scrollX += double(d.x);
	_scrollY += double(d.y);
	clampScroll();
	if (getScrollOffset() != prev) {
		_caretDirty = true;
	}
}

void TextViewContainer::scrollToRow(uint64_t row) {
	_scrollY = double(row) * double(_lineHeight);
	clampScroll();
	_caretDirty = true;
}

void TextViewContainer::scrollToEnd() {
	// Past the end on purpose: the clamp knows the real extent in double, which a float round
	// trip through getScrollRange() would land 2 px short of at the bottom of a huge document.
	_scrollY = docHeight();
	clampScroll();
	_caretDirty = true;
}

bool TextViewContainer::hasHorizontalOverflow() const { return getScrollRange().width > 0.0f; }

bool TextViewContainer::hasVerticalOverflow() const { return getScrollRange().height > 0.0f; }

void TextViewContainer::moveHorizontalOverflow(float d) {
	// The base drags the label directly and stops an easing action that this class never starts.
	// `d` is a pointer delta, and the offset runs the other way.
	scrollBy(Vec2(-d, 0.0f));
}

void TextViewContainer::setWrapWidth(float width) {
	if (_wrapWidth == width) {
		return;
	}
	_wrapWidth = width;
	// Nothing else to do here: the width is applied per label at materialization, and the block
	// structure change that goes with a wrap toggle is the widget's business (the chunk size).
	_caretDirty = true;
}

TextViewContainer::Slot *TextViewContainer::slotForBlock(uint32_t block) {
	Slot *free = nullptr;
	for (auto &slot : _slots) {
		if (slot.block == block) {
			return &slot;
		}
		if (!free && slot.block == maxOf<uint32_t>()) {
			free = &slot;
		}
	}
	if (free) {
		return free;
	}
	makeSlot();
	return &_slots.back();
}

basic2d::Label *TextViewContainer::makeSlot() {
	auto label = Rc<basic2d::Label>::create();
	label->setAnchorPoint(Anchor::BottomLeft);
	// Same selectors as the stock single-line label, so the stylesheet that styles one styles
	// the other - and a dynamically added node is styled on its first visit, before drawing.
	label->setType("label");
	label->addStyleClass("xl-ui-text-input-label");
	label->setSelectionColor(_selectionColor);
	label->setMarkedColor(_markedColor);

	auto ptr = _stage->addChild(sp::move(label), ZOrder(1));
	_slots.emplace_back(Slot{maxOf<uint32_t>(), ptr});
	return ptr;
}

void TextViewContainer::assignSlot(Slot &slot, uint32_t block) {
	const auto span = _doc->getBlock(block);
	auto label = slot.label;
	slot.block = block;

	// Width first, then string: both are equality-guarded upstream, so an unchanged block costs
	// two comparisons and no layout at all - which is what makes "reassign the window every
	// frame" affordable.
	label->setWidth(_wrapWidth > 0.0f ? _wrapWidth : 0.0f);
	label->setString(_doc->slice(span.start, span.length));
	label->tryUpdateLabel();

	if (_wrapWidth > 0.0f) {
		// The measured truth replaces the estimate; the row prefix shifts accordingly.
		_doc->setBlockRows(block, uint32_t(sprt::max(size_t(1), label->getLinesCount())));
	}

	// The label draws only its slice of the global selection and marked ranges. Guarded by
	// comparison because setSelectionCursor rebuilds its rects unconditionally on every call.
	const auto sel = sliceCursorToSpan(_cursor, span);
	if (label->getSelectionCursor() != sel) {
		label->setSelectionCursor(sel);
	}
	const auto mk = sliceCursorToSpan(_markedGlobal, span);
	if (label->getMarkedCursor() != mk) {
		label->setMarkedCursor(mk);
	}
}

Vec2 TextViewContainer::blockPosition(uint32_t block) const {
	// Double until the viewport-relative subtraction: the block top in document space reaches
	// millions of px, where float's step is already 2 px and labels would jitter against the
	// gutter at the bottom of a large file.
	const double top = double(_doc->getRowsBefore(block)) * double(_lineHeight);
	const double height = double(_doc->getBlockRows(block)) * double(_lineHeight);
	return Vec2(float(-_scrollX), float(double(_contentSize.height) - (top - _scrollY) - height));
}

void TextViewContainer::materialize() {
	clampScroll();

	const uint32_t blockCount = _doc->getBlockCount();
	const double marginPx = double(kMaterializeMargin) * double(_lineHeight);

	uint32_t first = _doc->getBlockForRow(uint64_t(sprt::max(_scrollY, 0.0) / double(_lineHeight)));
	first = first > kMaterializeMargin ? first - kMaterializeMargin : 0;

	// Everything above the window frees first, so the walk below finds its slots free. Slots
	// below the window are freed after the walk - transiently holding both windows caps the pool
	// at roughly twice the viewport, which is the price of never re-shaping on a small scroll.
	for (auto &slot : _slots) {
		if (slot.block != maxOf<uint32_t>() && slot.block < first) {
			slot.block = maxOf<uint32_t>();
			slot.label->setString(WideStringView());
			slot.label->setSelectionCursor(TextCursor::InvalidCursor);
			slot.label->setMarkedCursor(TextCursor::InvalidCursor);
		}
	}

	// Walk down, assigning and measuring, until the viewport plus margin is covered. Measuring
	// happens inside the walk because with wrapping the height of a block is only known after
	// its label laid out - and the walk's own cursor advances by the measured value.
	uint32_t b = first;
	double y = double(_doc->getRowsBefore(first)) * double(_lineHeight);
	const double yEnd = _scrollY + double(_contentSize.height) + marginPx;
	float maxWidth = 0.0f;

	while (b < blockCount && y < yEnd) {
		auto slot = slotForBlock(b);
		const auto oldRows = _doc->getBlockRows(b);
		assignSlot(*slot, b);
		const auto newRows = _doc->getBlockRows(b);

		// A block above the viewport top that measured taller than its estimate would push
		// everything the user is looking at; moving the anchor with it keeps the view still.
		if (newRows != oldRows && y < _scrollY) {
			_scrollY += double(int64_t(newRows) - int64_t(oldRows)) * double(_lineHeight);
		}

		maxWidth = sprt::max(maxWidth, slot->label->getContentSize().width);
		y += double(_doc->getBlockRows(b)) * double(_lineHeight);
		++b;
	}

	_matFirst = first;
	_matLast = b;

	for (auto &slot : _slots) {
		if (slot.block != maxOf<uint32_t>() && slot.block >= _matLast) {
			slot.block = maxOf<uint32_t>();
			slot.label->setString(WideStringView());
			slot.label->setSelectionCursor(TextCursor::InvalidCursor);
			slot.label->setMarkedCursor(TextCursor::InvalidCursor);
		}
	}

	_maxBlockWidth = maxWidth;
	clampScroll();

	// Position after the measuring walk: rows may have shifted during it, and the prefix the
	// positions are computed from is only final now.
	for (auto &slot : _slots) {
		if (slot.block != maxOf<uint32_t>()) {
			slot.label->setPosition(blockPosition(slot.block));
		}
	}

	const auto offset = getScrollOffset();
	if (offset != _notifiedScroll && _scrollCallback) {
		_scrollCallback(offset);
	}

	// The gutter depends on the window, the scroll and the row structure; pack those and notify
	// only on change, so a static frame costs no string building above.
	const uint64_t state = uint64_t(_matFirst) ^ (uint64_t(_matLast) << 20)
			^ (uint64_t(_doc->getLineCount()) << 40) ^ (_doc->getTotalRows() << 52);
	if ((state != _notifiedState || offset != _notifiedScroll) && _materializeCallback) {
		_materializeCallback();
	}
	_notifiedState = state;
	_notifiedScroll = offset;

	_caretDirty = true;
}

uint32_t TextViewContainer::getMaterializedCount() const {
	uint32_t count = 0;
	for (auto &slot : _slots) {
		if (slot.block != maxOf<uint32_t>()) {
			++count;
		}
	}
	return count;
}

uint32_t TextViewContainer::getDrawnSelectionLength() const {
	uint32_t total = 0;
	for (auto &slot : _slots) {
		if (slot.block != maxOf<uint32_t>()) {
			const auto c = slot.label->getSelectionCursor();
			if (c != TextCursor::InvalidCursor) {
				total += c.length;
			}
		}
	}
	return total;
}

uint32_t TextViewContainer::getVisualLineCount() const {
	return _doc ? uint32_t(_doc->getTotalRows()) : 0;
}

uint32_t TextViewContainer::getVisualLineForChar(uint32_t index) const {
	if (!_doc) {
		return 0;
	}
	const auto block = _doc->getBlockForIndex(index);
	uint64_t row = _doc->getRowsBefore(block);

	if (_wrapWidth > 0.0f) {
		// Inside a wrapped block the row split is the label's layout; without the label the
		// block's first row is the honest answer (gestures run between frames).
		for (auto &slot : _slots) {
			if (slot.block == block && slot.label->getLinesCount() > 0) {
				const auto span = _doc->getBlock(block);
				const auto local = index - span.start;
				const auto count = uint32_t(slot.label->getLinesCount());
				uint32_t lo = 0, hi = count - 1;
				while (lo < hi) {
					const auto mid = (lo + hi + 1) / 2;
					if (slot.label->getLine(mid).start <= local) {
						lo = mid;
					} else {
						hi = mid - 1;
					}
				}
				row += lo;
				break;
			}
		}
	}
	return uint32_t(row);
}

Pair<uint32_t, uint32_t> TextViewContainer::getVisualLineBounds(uint32_t row) const {
	if (!_doc) {
		return pair(0u, 0u);
	}
	const auto block = _doc->getBlockForRow(row);
	const auto span = _doc->getBlock(block);

	if (_wrapWidth > 0.0f) {
		for (auto &slot : _slots) {
			if (slot.block == block && slot.label->getLinesCount() > 0) {
				const auto rows = uint32_t(slot.label->getLinesCount());
				const auto rowIn =
						sprt::min(uint32_t(uint64_t(row) - _doc->getRowsBefore(block)), rows - 1);
				const auto l = slot.label->getLine(rowIn);
				return pair(span.start + l.start,
						span.start + sprt::min(l.start + l.count, span.length));
			}
		}
	}
	return pair(span.start, span.start + span.length);
}

uint32_t TextViewContainer::getCharForVisualLine(uint32_t row, float goalX) const {
	if (!_doc) {
		return 0;
	}
	const auto block = _doc->getBlockForRow(row);
	const auto span = _doc->getBlock(block);

	for (auto &slot : _slots) {
		if (slot.block == block && !slot.label->empty()
				&& slot.label->getCharsCount() == span.length) {
			const auto rows = uint32_t(sprt::max(size_t(1), slot.label->getLinesCount()));
			const auto rowIn =
					sprt::min(uint32_t(uint64_t(row) - _doc->getRowsBefore(block)), rows - 1);
			const auto l = slot.label->getLine(rowIn);
			const auto last = sprt::min(l.start + l.count, span.length);

			// Linear from the left: a row is short and the positions are monotonic within a bidi
			// run, which a binary search would need but cannot assume across runs.
			uint32_t best = l.start;
			float bestDist = maxOf<float>();
			for (uint32_t i = l.start; i <= last; ++i) {
				const auto dist = sprt::fabs(slot.label->getCursorPosition(i).x - goalX);
				if (dist < bestDist) {
					bestDist = dist;
					best = i;
				}
			}
			return span.start + best;
		}
	}

	// No label: the monospace fallback. Exact for ASCII, close enough for one frame of a gesture
	// on a block that has not been laid out yet.
	const auto col = _cellWidth > 0.0f
			? uint32_t(math::clamp(std::round(goalX / _cellWidth), 0.0f, float(span.length)))
			: 0;
	return span.start + col;
}

float TextViewContainer::getCursorDocX(uint32_t index) const {
	if (!_doc) {
		return 0.0f;
	}
	const auto block = _doc->getBlockForIndex(index);
	const auto span = _doc->getBlock(block);
	for (auto &slot : _slots) {
		if (slot.block == block && !slot.label->empty()) {
			return slot.label->getCursorPosition(index - span.start).x;
		}
	}
	return float(index - span.start) * _cellWidth;
}

float TextViewContainer::getVisibleLineCount() const {
	return _lineHeight > 0.0f ? _contentSize.height / _lineHeight : 0.0f;
}

void TextViewContainer::setCurrentLineVisible(bool value) {
	if (_currentLineVisible == value) {
		return;
	}
	_currentLineVisible = value;
	_caretDirty = true;
}

void TextViewContainer::setScrollCallback(Function<void(const Vec2 &)> &&cb) {
	_scrollCallback = sp::move(cb);
}

void TextViewContainer::setMaterializeCallback(Function<void()> &&cb) {
	_materializeCallback = sp::move(cb);
}

void TextViewContainer::setCursor(TextCursor cursor, uint32_t activePosition) {
	const auto prev = _cursor;
	const auto prevActive = _cursorActive;

	// The base stores the state and pushes the range into ITS label, which is empty here and
	// guarded; the materialized labels get their slices re-cut on the next frame's pass.
	TextInputContainer::setCursor(cursor, activePosition);

	// Only a real move arms the follow. The base is equality-guarded, so this also stays quiet
	// when the same cursor is re-pushed - which the IME echo does on every keystroke that only
	// changed the string.
	if (_cursor != prev || _cursorActive != prevActive) {
		_followCursor = true;
	}
}

void TextViewContainer::setMarked(TextCursor cursor) {
	TextInputContainer::setMarked(cursor);
	_markedGlobal = cursor;
	_caretDirty = true;
}

void TextViewContainer::setSelectionColor(const Color4F &color) {
	TextInputContainer::setSelectionColor(color);
	_selectionColor = color;
	// The color lives on every Label separately - the pool has to be told, or a recycled label
	// would draw the next selection in whatever color it had last.
	for (auto &slot : _slots) { slot.label->setSelectionColor(color); }
}

void TextViewContainer::setMarkedColor(const Color4F &color) {
	TextInputContainer::setMarkedColor(color);
	_markedColor = color;
	for (auto &slot : _slots) { slot.label->setMarkedColor(color); }
}

void TextViewContainer::scrollToCursor() {
	if (!_doc || _lineHeight <= 0.0f) {
		return;
	}

	const auto idx = _cursorActive;
	const auto block = _doc->getBlockForIndex(idx);

	// The caret's row within a wrapped block needs the block's layout; materialize it on demand
	// so a far jump (a goto, an end-of-document paste) still lands exactly.
	if (_wrapWidth > 0.0f) {
		auto slot = slotForBlock(block);
		if (slot->block != block) {
			assignSlot(*slot, block);
		}
	}

	const auto span = _doc->getBlock(block);
	uint64_t row = _doc->getRowsBefore(block);
	float docX = float(idx - span.start) * _cellWidth;
	for (auto &slot : _slots) {
		if (slot.block == block && !slot.label->empty()) {
			const auto cpos = slot.label->getCursorPosition(idx - span.start);
			docX = cpos.x;
			if (_wrapWidth > 0.0f && slot.label->getLinesCount() > 1) {
				// y from the label runs bottom-up; the row index runs top-down.
				const auto localRow = getVisualLineForChar(idx) - uint32_t(row);
				row += localRow;
			}
			break;
		}
	}

	const double rowTop = double(row) * double(_lineHeight);
	if (rowTop < _scrollY) {
		_scrollY = rowTop;
	} else if (rowTop + double(_lineHeight) > _scrollY + double(_contentSize.height)) {
		_scrollY = rowTop + double(_lineHeight) - double(_contentSize.height);
	}

	const auto marginX = sprt::min(_contentSize.width / 4.0f, kScrollMarginX);
	if (double(docX) - marginX < _scrollX) {
		_scrollX = sprt::max(double(docX) - marginX, 0.0);
	} else if (double(docX) + marginX > _scrollX + double(_contentSize.width)) {
		_scrollX = double(docX) + marginX - double(_contentSize.width);
	}

	clampScroll();
	_caretDirty = true;
}

TextCursor TextViewContainer::getCursorForPosition(const Vec2 &loc, font::CharSelectMode) {
	if (!_doc || _lineHeight <= 0.0f || _doc->empty()) {
		return TextCursor(0);
	}

	const auto p = convertToNodeSpace(loc);

	// Clamped rather than rejected, so a click above or below the text lands on the first or
	// last row instead of nowhere.
	const double docY = _scrollY + (double(_contentSize.height) - double(p.y));
	uint64_t row = docY <= 0.0 ? 0 : uint64_t(docY / double(_lineHeight));
	const auto total = _doc->getTotalRows();
	if (row >= total) {
		row = total - 1;
	}

	return TextCursor(getCharForVisualLine(uint32_t(row), p.x + float(_scrollX)));
}

TextCursor TextViewContainer::getWordForPosition(const Vec2 &loc) const {
	if (!_doc || _lineHeight <= 0.0f) {
		return TextCursor::InvalidCursor;
	}

	const auto p = convertToNodeSpace(loc);
	const double docY = _scrollY + (double(_contentSize.height) - double(p.y));
	if (docY < 0.0) {
		return TextCursor::InvalidCursor;
	}
	const uint64_t row = uint64_t(docY / double(_lineHeight));
	if (row >= _doc->getTotalRows()) {
		return TextCursor::InvalidCursor;
	}

	const auto block = _doc->getBlockForRow(row);
	const auto span = _doc->getBlock(block);
	for (auto &slot : _slots) {
		if (slot.block == block && !slot.label->empty()) {
			// getCharIndex with Center picks the glyph the pointer is over, not the nearest
			// boundary, which is what "the word I am pointing at" means. Its space-skipping is
			// right here (a word lookup in an indent should find nothing), unlike in
			// getCursorForPosition where it would snap the caret out of the indent.
			const auto lp = p - slot.label->getPosition().xy();
			const auto idx = slot.label->getCharIndex(lp, font::CharSelectMode::Center);
			if (idx.first == maxOf<uint32_t>()) {
				return TextCursor::InvalidCursor;
			}
			const auto word = slot.label->selectWord(idx.first);
			if (word == TextCursor::InvalidCursor) {
				return TextCursor::InvalidCursor;
			}
			return TextCursor(span.start + word.start, word.length);
		}
	}
	return TextCursor::InvalidCursor;
}

void TextViewContainer::updateCaretPosition() {
	if (!_doc || _lineHeight <= 0.0f) {
		return;
	}

	const auto idx = _cursorActive == maxOf<uint32_t>() ? _cursor.start : _cursorActive;
	const auto block = _doc->getBlockForIndex(idx);
	const auto span = _doc->getBlock(block);
	const auto base = blockPosition(block);

	// The caret is positioned even when its block is outside the window - the scissor clips it
	// there. Hiding it instead would fight the blink action, which turns visibility back on
	// every half second.
	Vec2 cpos(base.x + float(idx - span.start) * _cellWidth,
			base.y + float(_doc->getBlockRows(block) - 1) * _lineHeight);
	for (auto &slot : _slots) {
		if (slot.block == block && !slot.label->empty()) {
			cpos = base + slot.label->getCursorPosition(idx - span.start);
			break;
		}
	}

	_caret->setContentSize(Size2(kCaretWidth, _lineHeight));
	_caret->setPosition(cpos);

	// Drawn in container space so it spans the viewport whatever the horizontal slide is. Hidden
	// while a selection is up: two overlapping highlights on the same line read as a rendering
	// bug.
	_currentLine->setVisible(_currentLineVisible && _cursor.length == 0);
	if (_currentLine->isVisible()) {
		_currentLine->setContentSize(Size2(_contentSize.width, _lineHeight));
		_currentLine->setPositionY(cpos.y);
	}
}

// ===========================================================================
// TextView
// ===========================================================================

bool TextView::init() {
	if (!TextInput::init()) {
		return false;
	}

	addStyleClass("text-view");

	// ON here, unlike a plain field: a multi-line editor is the thing a person expects Ctrl+Z from,
	// and this one owns its document outright, so there is no other history it could be stealing
	// the chord from. CodeEditor inherits the decision.
	setUndoEnabled(true);

	// The bit that makes this multi-line at the platform level: TextInputProcessor stops declining
	// ENTER and inserts it as text ('\r' remapped to '\n'). Without it Enter is "submit" and no
	// amount of local editing would produce a newline the IME agrees exists.
	setInputType(
			TextInputType(toInt(TextInputType::Text_Text) | toInt(TextInputType::MultiLineBit)));

	// TextInput's own filter answers `_focused` for every key event, and a read-only view is never
	// focused - it acquires no input handler at all - so arrows and PageUp would never reach it.
	// Scrolling a read-only pane with the keyboard is exactly what a console output is for, so a
	// read-only view takes key events on the ordinary hit test instead.
	_listener->setTouchFilter(
			[this](const InputEvent &event, const InputListener::DefaultEventFilter &cb) {
		if (event.data.isKeyEvent()) {
			return _focused || (isReadOnly() && cb(event));
		}
		return cb(event);
	});

	// basic2d::ScrollView is single-axis and owns its own gesture set, so the wheel is wired
	// directly onto the viewport offset instead.
	_listener->addScrollRecognizer([this](const GestureScroll &scroll) {
		auto lh = getView()->getLineHeight();
		if (lh <= 0.0f) {
			lh = 16.0f; // the font has not measured yet; any sane step beats a dead wheel
		}
		const auto step = lh * kWheelLines;
		Vec2 delta(-scroll.amount.x * step, -scroll.amount.y * step);

		// Shift+wheel is horizontal on every desktop, but only on backends that do not already
		// report it as an x amount - so it is a fallback, not an override.
		if (delta.x == 0.0f && hasFlag(scroll.input->data.input.modifiers, InputModifier::Shift)) {
			delta.x = delta.y;
			delta.y = 0.0f;
		}

		getView()->scrollBy(delta);
		return true;
	});

	// The gutter is a sibling of the viewport, not a child: it must not be clipped by the
	// viewport's scissor nor ride its horizontal slide, only its vertical one.
	_gutter = addChild(Rc<basic2d::Layer>::create(Color::Black), ZOrder(1));
	_gutter->addStyleClass("text-view-gutter");
	_gutter->setAnchorPoint(Anchor::BottomLeft);
	_gutter->setVisible(false);

	auto scissor =
			_gutter->addSystem(Rc<DynamicStateSystem>::create(DynamicStateApplyMode::ApplyForAll));
	scissor->enableScissor();

	_gutterLabel = _gutter->addChild(Rc<basic2d::Label>::create());
	_gutterLabel->setAnchorPoint(Anchor::BottomLeft);
	_gutterLabel->setType("label");
	_gutterLabel->addStyleClass("text-view-gutter-label");
	// Digits and spaces only, so the glyph set is tiny and worth pinning.
	_gutterLabel->setPersistentGlyphData(true);

	// The gutter is rebuilt when the materialized window, the scroll or the row structure
	// changes - and from the container's own pass, AFTER the blocks are laid out: the widget's
	// visitDraw runs before its children's and would see the previous frame.
	getView()->setMaterializeCallback([this] { rebuildGutter(); });

	return true;
}

Rc<TextInputContainer> TextView::makeContainer() {
	auto container = Rc<TextViewContainer>::create();
	// The document outlives every frame concern: it is a plain member of this widget, and the
	// container only reads it and writes back measured row counts.
	container->setDocument(&_doc);
	return container;
}

TextViewContainer *TextView::getView() const {
	return static_cast<TextViewContainer *>(_container);
}

void TextView::handleEnter(Scene *scene) {
	TextInput::handleEnter(scene);
	_inspectorScene = scene;
	markContentSizeDirty();
}

void TextView::handleExit() {
	// Before the base call: Node::handleExit() clears _scene at its very end, and a command whose
	// lambda captured a destroyed widget is a dangling call from the inspector socket. Demos are
	// swapped by removing them, so this really does run.
	if (_inspectorScene) {
		if (auto content = _inspectorScene->getContent()) {
			if (auto i = inspector::get(content)) {
				for (auto &it : _inspectorCommands) { i->removeCommand(it); }
			}
		}
		_inspectorScene = nullptr;
	}
	_inspectorCommands.clear();

	TextInput::handleExit();
}

bool TextView::visitDraw(FrameInfo &frame, NodeVisitFlags parentFlags) {
	// The gutter label's width changes on its own the first time its font finishes LOADING, and
	// again whenever the numbers grow a digit - with no notification either time. Marking here
	// rather than resizing directly keeps handleContentSizeDirty the single writer of the
	// geometry, and marking from OUTSIDE it is what makes the mark survive: the flag is cleared
	// once the handler returns, so a re-mark issued inside it is lost.
	const auto measured = _gutterVisible ? _gutterLabel->getContentSize().width : 0.0f;
	if (measured != _gutterAppliedWidth) {
		markContentSizeDirty();
	}

	// The chunk follows the measured cell width and the density, and both settle asynchronously
	// (the font loads, the window lands on a monitor); setChunkSize no-ops on the same value.
	applyChunkSize();

	// The first wrap estimate needs the cell width, which the measure label answers only after
	// its font loads - re-checked here per frame for the same reason as the gutter width above.
	if (_wordWrap) {
		auto view = getView();
		const auto cell = view->getCellWidth();
		const auto width = view->getWrapWidth();
		if (cell > 0.0f && width > 0.0f) {
			const auto columns = uint32_t(sprt::max(width / cell, 1.0f));
			if (columns != _estimatedColumns) {
				_estimatedColumns = columns;
				_doc.estimateRows(columns);
			}
		}
	} else {
		_estimatedColumns = 0;
	}

	return TextInput::visitDraw(frame, parentFlags);
}

uint32_t TextView::computePlainChunk() const {
	const auto cell = getView()->getCellWidth();
	float density = 1.0f;
	if (_scene) {
		density = _scene->getFrameConstraints().density;
	}
	if (cell <= 0.0f || density <= 0.0f) {
		// Nothing measured yet: the conservative floor keeps even a density-3 monitor safe until
		// the first real measurement replaces it.
		return 512;
	}

	// A chunk drawn as ONE physical line must fit CharLayoutData::pos: int16_t, 32767 layout
	// units, units being px * density. The *4 is the worst cell: a tab advances to the next
	// multiple of four space widths, CJK is two cells - and 30000 rather than 32767 keeps slack
	// for kerning and letter-spacing.
	const auto worst = double(cell) * double(density) * 4.0;
	return uint32_t(math::clamp(std::floor(30000.0 / worst), 512.0, 2000.0));
}

void TextView::applyChunkSize() {
	_doc.setChunkSize(_wordWrap ? kChunkWrapped : computePlainChunk());
}

void TextView::handleContentSizeDirty() {
	// Sizes the container to the whole padded box and rebuilds the background image.
	TextInput::handleContentSizeDirty();

	// Then carve the gutter strip off its left. Done after the base rather than instead of it so
	// the padding arithmetic stays in one place; setContentSize is equality-guarded downstream, so
	// the container is not laid out twice for real.
	const auto pos = _container->getPosition().xy();
	const auto size = _container->getContentSize();

	_gutterAppliedWidth = _gutterVisible ? _gutterLabel->getContentSize().width : 0.0f;

	float strip = 0.0f;
	if (_gutterColumns > 0 && _gutterAppliedWidth > 0.0f) {
		// Every gutter line is padded to exactly _gutterColumns monospace cells, so one cell is a
		// division away - no font metrics API needed - and the strip gets one cell of gap.
		strip = _gutterAppliedWidth + (_gutterAppliedWidth / float(_gutterColumns));
	}

	// Visible whenever the gutter is ON, even at zero width. A hidden node is not visited, a Label
	// that is not visited never lays out, and a Label that never lays out measures zero - so hiding
	// the strip until it has a width is a deadlock: it would never acquire one. A zero-wide layer
	// draws nothing anyway.
	_gutter->setVisible(_gutterVisible);
	_gutter->setPosition(pos);
	_gutter->setContentSize(Size2(strip, size.height));

	const auto inner = sprt::max(size.width - strip, 0.0f);

	_container->setPosition(Vec2(pos.x + strip, pos.y));
	_container->setContentSize(Size2(inner, size.height));

	// The one place the wrap width is decided, and it is pushed rather than pulled. Deriving it
	// inside the container would mean reading a viewport width that markContentSizeDirty() has
	// just invalidated - which is exactly the race that made a wrap toggle take effect only
	// sometimes. Here `inner` was computed two lines up and is by construction current.
	getView()->setWrapWidth(_wordWrap ? inner : 0.0f);
	applyChunkSize();
}

void TextView::setWordWrap(bool value) {
	if (_wordWrap == value) {
		return;
	}
	_wordWrap = value;

	// Only the intent is recorded. handleContentSizeDirty() is where the viewport width exists
	// and is therefore where the wrap width and the chunk size are applied.
	markContentSizeDirty();
}

void TextView::setGutterVisible(bool value) {
	if (_gutterVisible == value) {
		return;
	}
	_gutterVisible = value;
	rebuildGutter();
	markContentSizeDirty();
}

void TextView::setCurrentLineHighlight(bool value) { getView()->setCurrentLineVisible(value); }

void TextView::setTabInsertsIndent(bool value) { _tabInsertsIndent = value; }

uint32_t TextView::getLineCount() const { return _doc.getLineCount(); }

Pair<uint32_t, uint32_t> TextView::getLineColumn(uint32_t index) const {
	return _doc.getLineColumn(index);
}

uint32_t TextView::getIndexForLineColumn(uint32_t line, uint32_t column) const {
	return _doc.getIndexForLineColumn(line, column);
}

void TextView::scrollToLine(uint32_t line) {
	// Through the row model, in integer rows: the pixel offset of a deep line does not survive a
	// float round trip.
	getView()->scrollToRow(_doc.getRowsBefore(_doc.getFirstBlockForLine(line)));
}

void TextView::scrollToEnd() { getView()->scrollToEnd(); }

// ===========================================================================
// TextView: the global cursor layer and the IME window
// ===========================================================================

WideString TextView::normalizeInput(WideStringView str, bool &changed) {
	changed = false;
	WideString out;
	out.reserve(str.size());
	for (size_t i = 0; i < str.size(); ++i) {
		auto c = str[i];
		if (c == u'\r') {
			// CRLF collapses to one LF; a lone CR becomes one too. The runtime's processor
			// already does this for typed keys, but pasted and dropped text arrives unfiltered.
			changed = true;
			if (i + 1 < str.size() && str[i + 1] == u'\n') {
				continue;
			}
			c = u'\n';
		}
		// Anything else below 0x20 is dropped by the formatter anyway (Formatter::readChars),
		// and a character present in the string but absent from the layout breaks the identity
		// the cursor depends on: index in the string == index in the layout.
		if ((c < 0x20 && c != u'\n' && c != u'\t') || !handleInputChar(c)) {
			changed = true;
			continue;
		}
		out.push_back(c);
	}
	return out;
}

void TextView::applyDocEdit(uint32_t pos, uint32_t removed, WideStringView inserted) {
	/* The one point every change to this document passes through - insertGlobal and everything
	under it, setText, and the IME echo, which is where typing actually arrives. Recording here
	rather than at a widget command is not tidiness: a typed character never reaches insertText at
	all, because the platform owns the input and the widget only ever sees the echo.

	Before _doc.apply, because after it the removed text is gone. _gCursor is still the caret as it
	was: every caller assigns it AFTER this returns. */
	if (_history.isEnabled() && !_history.isApplying()) {
		_history.recordEdit(pos, _doc.slice(pos, removed), inserted, _gCursor, _historyEditName,
				_historyClock);
	}

	_doc.apply(pos, removed, inserted);

	// apply() reset the affected blocks' rows to 1; with wrapping on, restore honest estimates
	// immediately - a scrollToEnd right after an append (the console's tail pinning) aims at
	// the model height, and it must not aim short.
	if (_wordWrap && _estimatedColumns > 0) {
		const auto firstLine = _doc.getLineForIndex(pos);
		const auto lastLine = _doc.getLineForIndex(pos + uint32_t(inserted.size()));
		_doc.estimateRowsRange(_doc.getFirstBlockForLine(firstLine),
				_doc.getFirstBlockForLine(lastLine) + _doc.getBlocksForLine(lastLine),
				_estimatedColumns);
	}
}

Pair<uint32_t, uint32_t> TextView::computeWindow(uint32_t center) const {
	const auto size = uint32_t(_doc.size());
	center = sprt::min(center, size);

	uint32_t lo = center > kWindowMax / 2 ? center - kWindowMax / 2 : 0;
	uint32_t hi = sprt::min(size, center + kWindowMax / 2);

	// Snap to line boundaries when they are near - a window that starts mid-word confuses a
	// real IME's context lookup for nothing - but never let the snap blow the window up: a
	// single line longer than the window (a minified file) must be cut mid-line.
	const auto loSnap = _doc.getLineStart(_doc.getLineForIndex(lo));
	if (lo - loSnap <= kWindowMax) {
		lo = loSnap;
	}
	const auto hiLine = _doc.getLineForIndex(hi);
	const auto hiSnap = sprt::min(size, _doc.getLineStart(hiLine) + _doc.getLineLength(hiLine) + 1);
	if (hiSnap - hi <= kWindowMax) {
		hi = hiSnap; // includes the line's own '\n', so deleteForward at its end has room
	}

	return pair(lo, hi);
}

TextCursor TextView::clipToWindow(TextCursor global, uint32_t anchor, uint32_t length) {
	const uint64_t winEnd = uint64_t(anchor) + length;
	const auto s = math::clamp(uint64_t(global.start), uint64_t(anchor), winEnd);
	const auto e = math::clamp(uint64_t(global.start) + global.length, uint64_t(anchor), winEnd);
	return TextCursor(uint32_t(s - anchor), uint32_t(e - s));
}

bool TextView::needsReanchor() const {
	const auto active = activeGlobal();
	const uint32_t focus = active != maxOf<uint32_t>() ? active : _gCursor.start + _gCursor.length;
	const auto size = uint32_t(_doc.size());
	const uint64_t winEnd = uint64_t(_windowAnchor) + _windowLength;

	if (_windowAnchor > 0 && focus < uint64_t(_windowAnchor) + kWindowGuard) {
		return true;
	}
	if (winEnd < size && uint64_t(focus) + kWindowGuard > winEnd) {
		return true;
	}
	return focus < _windowAnchor || focus > winEnd;
}

void TextView::pushWindow() {
	const auto active = activeGlobal();
	const uint32_t center = active != maxOf<uint32_t>() ? active : _gCursor.start + _gCursor.length;

	const auto window = computeWindow(center);
	_windowAnchor = window.first;
	_windowLength = window.second - window.first;

	auto string = TextInputString::create(_doc.slice(_windowAnchor, _windowLength));
	const auto cursor = clipToWindow(_gCursor, _windowAnchor, _windowLength);
	const auto marked = _gMarked == TextCursor::InvalidCursor
			? TextCursor::InvalidCursor
			: clipToWindow(_gMarked, _windowAnchor, _windowLength);

	// The manager clamps cursor.start silently, which would MASK a global index leaking into a
	// window request - so the leak is reported here, where it is still attributable.
	if (uint64_t(cursor.start) + cursor.length > string->size()) {
		slog().error("TextView", "global cursor leaked into a window request: ", cursor.start, "+",
				cursor.length, " > ", string->size());
	}

	TextInputRequest req;
	req.string = string;
	req.cursor = cursor;
	req.marked = marked;
	req.serial = ++_windowSerial;
	req.type = _inputType;

	if (_handler.isActive()) {
		_pushes.emplace_back(WindowPush{req.serial, _windowAnchor, string});
		_handler.update(sp::move(req));
	} else {
		// No platform authority to defer to - the same local-write rule as the base's setText.
		auto state = req.getState();
		state.enabled = _inputState.enabled;
		state.compose = _inputState.compose;
		_inputState = sp::move(state);
	}
}

void TextView::pushCursorUpdate() {
	if (!_handler.isActive() || _pushes.empty()) {
		return;
	}

	// The base string did not change, so neither does the serial: an edit the processor makes
	// on top of this still diffs against the same recorded base.
	auto &last = _pushes.back();
	TextInputRequest req;
	req.string = last.base;
	req.cursor = clipToWindow(_gCursor, _windowAnchor, _windowLength);
	req.marked = _gMarked == TextCursor::InvalidCursor
			? TextCursor::InvalidCursor
			: clipToWindow(_gMarked, _windowAnchor, _windowLength);
	req.serial = last.serial;
	req.type = _inputType;
	_handler.update(sp::move(req));
}

uint32_t TextView::activeGlobal() const {
	if (_gCursor.length == 0 || _gSelAnchor == maxOf<uint32_t>()) {
		return maxOf<uint32_t>();
	}
	// the anchor is the end that stays put; the user is moving the other one
	return _gSelAnchor <= _gCursor.start ? _gCursor.start + _gCursor.length : _gCursor.start;
}

uint32_t TextView::offsetGlobal(int32_t delta) const {
	const auto size = int64_t(_doc.size());

	int64_t from = _gCursor.start;
	if (_gCursor.length > 0) {
		if (_gSelAnchor == maxOf<uint32_t>()) {
			// nothing is being extended: moving off a selection collapses it to the edge you
			// are moving towards, as every editor does
			return uint32_t(math::clamp(
					int64_t(delta < 0 ? _gCursor.start : _gCursor.start + _gCursor.length),
					int64_t(0), size));
		}
		from = activeGlobal();
	}

	return uint32_t(math::clamp(from + delta, int64_t(0), size));
}

void TextView::moveGlobal(uint32_t target, bool select) {
	if (select) {
		if (_gSelAnchor == maxOf<uint32_t>()) {
			// anchor the selection at the end the caret is moving away from
			_gSelAnchor = _gCursor.length > 0 ? _gCursor.start + _gCursor.length : _gCursor.start;
		}
		const auto from = sprt::min(_gSelAnchor, target);
		const auto to = sprt::max(_gSelAnchor, target);
		setGlobalCursorInternal(TextCursor(from, to - from));
	} else {
		_gSelAnchor = maxOf<uint32_t>();
		setGlobalCursorInternal(TextCursor(target));
	}
}

void TextView::setGlobalCursorInternal(TextCursor cursor) {
	const auto size = uint32_t(_doc.size());
	cursor.start = sprt::min(cursor.start, size);
	cursor.length = sprt::min(cursor.length, size - cursor.start);

	_gCursor = cursor;
	_container->setCursor(cursor, activeGlobal());

	if (_handler.isActive()) {
		if (needsReanchor()) {
			pushWindow();
		} else {
			pushCursorUpdate();
		}
	} else {
		_inputState.cursor = clipToWindow(cursor, _windowAnchor, _windowLength);
	}
}

void TextView::applyGestureGlobal(TextCursor cursor) {
	if (!_focused && !isReadOnly()) {
		acquireGlobal(cursor);
	} else {
		setGlobalCursorInternal(cursor);
	}
}

void TextView::acquireGlobal(TextCursor cursor) {
	if (!_director) {
		return;
	}

	const auto size = uint32_t(_doc.size());
	cursor.start = sprt::min(cursor.start, size);
	cursor.length = sprt::min(cursor.length, size - cursor.start);
	_gCursor = cursor;
	_container->setCursor(cursor, activeGlobal());

	const auto window = computeWindow(cursor.start + cursor.length);
	_windowAnchor = window.first;
	_windowLength = window.second - window.first;

	auto string = TextInputString::create(_doc.slice(_windowAnchor, _windowLength));

	TextInputRequest req;
	req.string = string;
	req.cursor = clipToWindow(cursor, _windowAnchor, _windowLength);
	req.marked = TextCursor::InvalidCursor;
	req.serial = ++_windowSerial;
	req.type = _inputType;

	_pushes.clear();
	_pushes.emplace_back(WindowPush{req.serial, _windowAnchor, string});
	_handler.run(_director->getTextInputManager(), sp::move(req));
	_focusListener->setEnabled(true);
}

void TextView::acquireInput(TextCursor cursor) { acquireGlobal(cursor); }

void TextView::insertGlobal(WideStringView text, TextCursor replace) {
	const auto size = uint32_t(_doc.size());
	replace.start = sprt::min(replace.start, size);
	replace.length = sprt::min(replace.length, size - replace.start);

	bool filtered = false;
	auto norm = normalizeInput(text, filtered);

	applyDocEdit(replace.start, replace.length, norm);

	_gSelAnchor = maxOf<uint32_t>();
	_gMarked = TextCursor::InvalidCursor;
	_gCursor = TextCursor(replace.start + uint32_t(norm.size()));

	_container->setCursor(_gCursor, maxOf<uint32_t>());
	_container->setMarked(TextCursor::InvalidCursor);
	_container->setPlaceholderVisible(_doc.empty() && !_focused);

	// A fresh window either way: the edit may be megabytes (a paste), and the processor must
	// never see more than the window - the whole point of the design.
	pushWindow();

	if (_callback) {
		_callback(StringView());
	}
}

void TextView::insertText(WideStringView text, TextCursor replaceGlobal) {
	insertGlobal(text, replaceGlobal);
}

void TextView::setText(WideStringView str) {
	bool filtered = false;
	auto norm = normalizeInput(str, filtered);

	/* Replacing the whole document is not an edit of it - it is a different document. A history
	kept across this would undo into a file nobody has open, which is why a load drops it - and why
	the replacement itself is not recorded either, which would put the old document back in the new
	one's history one Ctrl+Z away. */
	_history.clear();
	_history.setRecording(false);

	applyDocEdit(0, uint32_t(_doc.size()), norm);

	_history.setRecording(true);

	_gSelAnchor = maxOf<uint32_t>();
	_gMarked = TextCursor::InvalidCursor;
	_gCursor = TextCursor(uint32_t(norm.size()));

	_container->setCursor(_gCursor, maxOf<uint32_t>());
	_container->setMarked(TextCursor::InvalidCursor);
	_container->setPlaceholderVisible(_doc.empty() && !_focused);

	pushWindow();

	if (_callback) {
		_callback(StringView());
	}
}

StringView TextView::getText() const {
	_textCache = string::toUtf8<Interface>(_doc.getView());
	return _textCache;
}

void TextView::focus() {
	if (!isEnabled() || isReadOnly() || _handler.isActive()) {
		return;
	}
	acquireGlobal(TextCursor(uint32_t(_doc.size())));
}

void TextView::selectAll() {
	const auto count = uint32_t(_doc.size());
	if (count == 0) {
		return;
	}
	_gSelAnchor = maxOf<uint32_t>();
	setGlobalCursorInternal(TextCursor(0u, count));
}

WideStringView TextView::getTextForCursor(TextCursor cursor) const {
	return _doc.slice(cursor.start, cursor.length);
}

void TextView::setCursor(TextCursor cursor) { setGlobalCursorInternal(cursor); }

void TextView::handleTextInput(const TextInputState &data) {
	const bool wasFocused = _focused;
	const bool wasComposing = _inputState.marked.length > 0;

	// Focus follows what the platform actually granted, not what was asked for - which is what
	// makes `:focus` in CSS mean something.
	if (_focused != data.enabled) {
		_focused = data.enabled;
		if (!_focused) {
			_focusListener->setEnabled(false);
			_gSelAnchor = maxOf<uint32_t>();
		}
		updateInteractiveState();
	}

	// The push this echo descends from. Everything before it in the list is superseded: the
	// channel is ordered, so no echo for an older serial can still arrive.
	WindowPush *push = nullptr;
	for (auto &p : _pushes) {
		if (p.serial == data.serial) {
			push = &p;
			break;
		}
	}
	if (push) {
		while (!_pushes.empty() && _pushes.front().serial < data.serial) {
			_pushes.erase(_pushes.begin());
			push = &_pushes.front(); // the erase shifted it
		}
	}

	_inputState = data;

	_container->setEnabled(_focused);

	if (!push) {
		// An echo with no base: from before the first push of this focus session, or after a
		// cancel dropped the list. Nothing can be diffed against it - resync authoritatively.
		if (data.enabled && _handler.isActive()) {
			pushWindow();
		}
		_container->setPlaceholderVisible(_doc.empty() && !_focused);
		return;
	}

	const auto anchor = push->anchor;
	const auto baseView = WideStringView(push->base->string);
	const auto echoRaw = data.getStringView();
	const auto echoView = WideStringView(echoRaw.data(), echoRaw.size());

	bool docChanged = false;
	bool needRepush = false;
	bool cursorFromEdit = false;

	if (echoView != baseView) {
		const auto d = TextDocument::diff(baseView, echoView);

		bool filtered = false;
		auto ins = normalizeInput(WideStringView(echoView.data() + d.pos, d.inserted), filtered);
		needRepush = filtered;

		// A selection wider than the window was pushed as its clip; an edit that replaces the
		// clip means "replace the WHOLE selection" - typing over a select-all must not leave
		// the out-of-window part standing.
		const uint64_t selEnd = uint64_t(_gCursor.start) + _gCursor.length;
		const bool selWider = _gCursor.length > 0
				&& (_gCursor.start < anchor || selEnd > uint64_t(anchor) + push->base->size());
		if (selWider) {
			applyDocEdit(_gCursor.start, _gCursor.length, ins);
			_gCursor = TextCursor(_gCursor.start + uint32_t(ins.size()));
			cursorFromEdit = true;
		} else {
			applyDocEdit(anchor + d.pos, d.removed, ins);
		}
		docChanged = true;

		// Later echoes of this same serial diff against what the processor now holds. A deleted
		// last character comes back as a null string - "the window emptied", not "no base".
		push->base = data.string ? data.string : TextInputString::create(WideStringView());
	}

	if (!cursorFromEdit) {
		if (docChanged) {
			// An edit moved the caret; the echo is authoritative.
			_gCursor = TextCursor(anchor + data.cursor.start, data.cursor.length);
		} else {
			// A pure cursor echo. If it is exactly OUR OWN clip coming back, the global cursor
			// must stand: the projection is lossy (a selection wider than the window clips),
			// and overwriting the original with its own clip would silently shrink it - which
			// is precisely how typing over a select-all would end up replacing only the
			// window. Anything else is a platform-side cursor move and wins.
			const auto expected = clipToWindow(_gCursor, anchor, uint32_t(push->base->size()));
			if (data.cursor != expected) {
				_gCursor = TextCursor(anchor + data.cursor.start, data.cursor.length);
			}
		}
	}
	_gMarked = data.marked == TextCursor::InvalidCursor
			? TextCursor::InvalidCursor
			: TextCursor(anchor + data.marked.start, data.marked.length);

	_container->setCursor(_gCursor, activeGlobal());
	_container->setMarked(_gMarked);
	_container->setPlaceholderVisible(_doc.empty() && !_focused);

	// A marked range is a composition in progress, not committed text: reporting it as a
	// change would make an autocomplete widget fire on every syllable being assembled.
	if (_callback
			&& ((docChanged && _gMarked.length == 0)
					|| (wasComposing && _gMarked.length == 0 && !docChanged))) {
		_callback(StringView());
	}

	if (data.enabled && _handler.isActive()) {
		if (needRepush) {
			// The correction has to travel back, or the platform keeps editing the string it
			// thinks it has and the next keystroke reverts it.
			pushWindow();
		} else if (_gMarked.length == 0 && data.compose != InputKeyComposeState::Composing
				&& needsReanchor()) {
			// Never during a composition: the processor's compose run lives in cursor.length
			// (dead keys) or in marked (an IME), and moving the origin under either would
			// desynchronize it.
			pushWindow();
		}
	}

	if (wasFocused && !_focused) {
		_pushes.clear();
		_container->setPlaceholderVisible(_doc.empty());
	}
}

void TextView::rebuildGutter() {
	if (!_gutterVisible) {
		if (_gutterColumns != 0) {
			_gutterColumns = 0;
			markContentSizeDirty();
		}
		return;
	}

	// Wide enough for the largest number it will ever show, but never narrower than the
	// stylesheet asked for - so the strip does not twitch as the document crosses a power of
	// ten.
	uint32_t digits = 1;
	for (auto n = _doc.getLineCount(); n >= 10; n /= 10) { ++digits; }
	const auto columns = sprt::max(uint32_t(_gutterChars), digits);

	auto view = getView();
	const auto range = view->getMaterializedBlocks();

	// Numbers only for the window: a whole-document gutter string is O(document) per change,
	// which is the exact cost the block design removes. A logical line is numbered on the first
	// row of its first block; wrap continuations and chunk tails stay blank, which is what an
	// editor shows.
	StringStream out;
	bool firstRow = true;
	for (uint32_t b = range.first; b < range.second; ++b) {
		const auto span = _doc.getBlock(b);
		const auto rows = _doc.getBlockRows(b);
		for (uint32_t r = 0; r < rows; ++r) {
			if (!firstRow) {
				out << "\n";
			}
			firstRow = false;
			if (r == 0 && span.chunk == 0) {
				auto num = mem_std::toString(span.line + 1);
				for (size_t p = num.size(); p < columns; ++p) { out << " "; }
				out << num;
			} else {
				for (uint32_t p = 0; p < columns; ++p) { out << " "; }
			}
		}
	}

	_gutterLabel->setString(out.str());
	_gutterLabel->tryUpdateLabel();

	// Align the gutter window's first row with the block window's first row. The distance is
	// computed in double and becomes float only after the subtraction - the same rule as the
	// block positions, or the two columns would drift apart at the bottom of a large file.
	const double topPx = double(_doc.getRowsBefore(range.first)) * double(view->getLineHeight())
			- double(view->getScrollOffset().y);
	_gutterLabel->setPosition(Vec2(0.0f,
			_gutter->getContentSize().height - float(topPx)
					- _gutterLabel->getContentSize().height));

	// Only the column count is this function's business; the measured width is reconciled in
	// visitDraw, which is the one place a re-mark actually survives.
	if (_gutterColumns != columns) {
		_gutterColumns = columns;
		markContentSizeDirty();
	}
}

void TextView::moveCursorHorizontal(uint32_t target, bool select) {
	_goalValid = false;
	moveGlobal(target, select);
}

void TextView::moveCursorVertical(int32_t rows, bool select) {
	auto view = getView();
	const auto active = activeGlobal();
	const auto from = (active == maxOf<uint32_t>()) ? _gCursor.start : active;

	if (!_goalValid) {
		_goalX = view->getCursorDocX(from);
		_goalValid = true;
	}

	const auto count = view->getVisualLineCount();
	if (count == 0) {
		return;
	}

	const auto row = int64_t(view->getVisualLineForChar(from)) + rows;
	const auto clamped = uint32_t(math::clamp(row, int64_t(0), int64_t(count) - 1));

	// moveGlobal(), not moveCursorHorizontal(): the goal column has to survive the step, which
	// is the entire point of keeping it.
	moveGlobal(view->getCharForVisualLine(clamped, _goalX), select);
}

bool TextView::handleKey(const GestureData &data) {
	if ((!_focused && !isReadOnly()) || !data.input) {
		return false;
	}

	const auto &ev = data.input->data;
	if (ev.event != InputEventName::KeyPressed && ev.event != InputEventName::KeyRepeated) {
		return false;
	}

	auto view = getView();
	const bool select = hasFlag(ev.input.modifiers, InputModifier::Shift);

	// A read-only pane has no caret to move, so the same keys scroll it instead - which is the
	// only way to read a long console log without a mouse.
	if (isReadOnly()) {
		const auto lh = view->getLineHeight();
		switch (ev.key.keycode) {
		case InputKeyCode::UP: view->scrollBy(Vec2(0.0f, -lh)); return true;
		case InputKeyCode::DOWN: view->scrollBy(Vec2(0.0f, lh)); return true;
		case InputKeyCode::LEFT: view->scrollBy(Vec2(-lh, 0.0f)); return true;
		case InputKeyCode::RIGHT: view->scrollBy(Vec2(lh, 0.0f)); return true;
		case InputKeyCode::PAGE_UP:
			view->scrollBy(Vec2(0.0f, -view->getContentSize().height));
			return true;
		case InputKeyCode::PAGE_DOWN:
			view->scrollBy(Vec2(0.0f, view->getContentSize().height));
			return true;
		case InputKeyCode::HOME: view->setScrollOffset(Vec2(0.0f, 0.0f)); return true;
		case InputKeyCode::END: view->scrollToEnd(); return true;
		default: return false;
		}
	}

	// One less than the visible count, so a page keeps a line of overlap and the eye can follow.
	const auto page = sprt::max(int32_t(view->getVisibleLineCount()) - 1, 1);

	switch (ev.key.keycode) {
	case InputKeyCode::LEFT: moveCursorHorizontal(offsetGlobal(-1), select); return true;
	case InputKeyCode::RIGHT: moveCursorHorizontal(offsetGlobal(1), select); return true;
	case InputKeyCode::UP: moveCursorVertical(-1, select); return true;
	case InputKeyCode::DOWN: moveCursorVertical(1, select); return true;
	case InputKeyCode::PAGE_UP: moveCursorVertical(-page, select); return true;
	case InputKeyCode::PAGE_DOWN: moveCursorVertical(page, select); return true;
	case InputKeyCode::HOME:
	case InputKeyCode::END: {
		// Visual, not logical: with wrap on, Home means the start of the row the caret is on,
		// which is what the user is looking at.
		const auto active = activeGlobal();
		const auto from = (active == maxOf<uint32_t>()) ? _gCursor.start : active;
		const auto bounds = view->getVisualLineBounds(view->getVisualLineForChar(from));
		moveCursorHorizontal(ev.key.keycode == InputKeyCode::HOME ? bounds.first : bounds.second,
				select);
		return true;
	}
	default: break;
	}
	return false;
}

bool TextView::handleTextHotkey(HotkeyId id, const InputEvent &ev) {
	auto &hk = EngineHotkeys::get();

	// A read-only view is never `_focused` - it acquires no input handler at all - and the base
	// declines every hotkey in that state, which would make the one thing a read-only view exists
	// for, selecting and copying, impossible.
	if (isReadOnly()) {
		if (id == hk.textSelectAll) {
			selectAll();
			return true;
		}
		if (id == hk.textCopy) {
			return copy();
		}
		return false;
	}

	if (!_focused) {
		return false;
	}

	// The runtime's processor always declines TAB (it is navigation everywhere, multi-line fields
	// included), so Tab arrives here as the focusNext hotkey and is turned back into an indent.
	// Shift+Tab is a separate id and keeps its meaning, which is how the field stays escapable.
	if (_tabInsertsIndent && id == hk.focusNext) {
		insertGlobal(WideStringView(u"\t"), _gCursor);
		return true;
	}

	// Defensive: canHandleInputEvent only claims a key that carries a keychar, so on a backend
	// where ENTER has none the processor cannot insert it even with MultiLineBit set and the key
	// falls through to this binding instead. Untestable on Linux, which does carry one.
	if (id == hk.textAccept || id == hk.textAcceptKeypad) {
		insertGlobal(WideStringView(u"\n"), _gCursor);
		return true;
	}

	return TextInput::handleTextHotkey(id, ev);
}

bool TextView::handleTap(const GestureTap &tap) {
	// The base's shape, with one substitution: the word lookup goes to the container, because
	// TextInput::getWordForPosition is not virtual and reads the base's empty label.
	if (!isEnabled()) {
		return false;
	}

	if (_longPressApplied) {
		_longPressApplied = false;
		return true;
	}

	_gSelAnchor = maxOf<uint32_t>();

	switch (tap.count) {
	case 1: applyGestureGlobal(_container->getCursorForPosition(tap.location())); return true;
	case 2: {
		const auto word = getView()->getWordForPosition(tap.location());
		if (word != TextCursor::InvalidCursor) {
			applyGestureGlobal(word);
			return true;
		}
		break;
	}
	case 3: applyGestureGlobal(TextCursor(0u, uint32_t(_doc.size()))); return true;
	default: break;
	}
	return false;
}

bool TextView::handleLongPress(const GesturePress &press) {
	// Same substitution as handleTap, same reason.
	if (!isEnabled() || _dragSelecting || _panning || _doc.empty()) {
		return true;
	}

	switch (press.tickCount) {
	case 1: {
		const auto word = getView()->getWordForPosition(press.location());
		if (word != TextCursor::InvalidCursor) {
			_gSelAnchor = word.start;
			_longPressApplied = true;
			applyGestureGlobal(word);
		}
		break;
	}
	case 2:
		_gSelAnchor = 0;
		_longPressApplied = true;
		applyGestureGlobal(TextCursor(0u, uint32_t(_doc.size())));
		break;
	default: break;
	}

	return true;
}

bool TextView::handleSwipeBegin(const Vec2 &pt) {
	// The base's shape, on the global layer: the container answers in document indices, and
	// the base's moveCursor would misread them as window ones.
	if (!isEnabled() || !isTouched(pt, 8.0f)) {
		return false;
	}

	if (_focused && !isReadOnly()) {
		const auto cursor = _container->getCursorForPosition(pt);
		_gSelAnchor = cursor.start;
		_dragSelecting = true;
		_listener->setExclusive();
		return true;
	}

	// Not focused: a drag pans the viewport so the text can be read without editing it. Both
	// axes - a multi-line view overflows vertically far more often than horizontally.
	if (getView()->hasHorizontalOverflow() || getView()->hasVerticalOverflow()) {
		_panning = true;
		_listener->setExclusive();
		return true;
	}
	return false;
}

bool TextView::handleSwipe(const Vec2 &pt, const Vec2 &delta) {
	if (_dragSelecting) {
		const auto cursor = _container->getCursorForPosition(pt);
		moveGlobal(cursor.start, true);
		// keep pulling while the pointer sits outside the box
		_container->setAutoScrollTarget(_container->isTouched(pt) ? Vec2::INVALID : pt);
		return true;
	}

	if (_panning) {
		getView()->scrollBy(Vec2(-delta.x, delta.y));
		return true;
	}
	return false;
}

bool TextView::setStyleValue(const ResolvedStyle &style, document::ParameterName name,
		const document::StyleValue &value) {
	auto ret = TextInput::setStyleValue(style, name, value);

	// CmdReset is the one call that carries the whole ResolvedStyle, and custom properties are
	// never delivered as parameters - which is why the base reads --caret-color here too.
	if (name == document::ParameterName::CmdReset) {
		auto prop = style.getCustomProperty("--gutter-chars");
		float chars = 0.0f;
		if (!prop.empty() && prop.readFloat().grab(chars) && chars > 0.0f) {
			if (_gutterChars != chars) {
				_gutterChars = chars;
				rebuildGutter();
			}
		}
	}

	return ret;
}

void TextView::addInspectorCommand(Scene *scene, StringView name, StringView desc) {
	auto content = scene->getContent();
	if (!content) {
		return;
	}

	auto action = name.pdup();
	if (inspector::addCommand(content, name, desc,
				[this, action](Value &&args, Function<void(Value &&)> &&done) {
		Value result;
		result.setBool(handleInspectorCommand(action, args, result), "ok");
		done(sp::move(result));
	})) {
		_inspectorCommands.emplace_back(name.str<Interface>());
	}
}

Value TextView::encodeState() const {
	auto view = getView();
	const auto cursor = _gCursor;
	const auto lineColumn = getLineColumn(cursor.start);
	const auto offset = view->getScrollOffset();
	const auto range = view->getScrollRange();
	const auto lineHeight = view->getLineHeight();
	const auto materialized = view->getMaterializedBlocks();

	Value ret;
	// Capped: a state poll must not serialize a multi-megabyte document into every answer. The
	// size is always reported; a caller that needs the text of a huge document reads it in
	// slices through the `lines` command.
	if (_doc.size() <= kStateTextCap) {
		ret.setString(getText(), "text");
	} else {
		ret.setBool(true, "textOmitted");
	}
	ret.setInteger(int64_t(_doc.size()), "charCount");
	ret.setInteger(int64_t(getLineCount()), "lineCount");
	ret.setBool(isReadOnly(), "readOnly");
	ret.setBool(_focused, "focused");
	ret.setBool(_wordWrap, "wordWrap");
	ret.setBool(_gutterVisible, "gutterVisible");

	ret.setInteger(int64_t(cursor.start), "cursorStart");
	ret.setInteger(int64_t(cursor.length), "cursorLength");
	ret.setInteger(int64_t(lineColumn.first), "line");
	ret.setInteger(int64_t(lineColumn.second), "column");
	if (cursor.length > 0 && cursor.length <= kStateTextCap) {
		ret.setString(string::toUtf8<Interface>(_doc.slice(cursor.start, cursor.length)),
				"selectionText");
	}

	ret.setInteger(int64_t(_gMarked.start), "markedStart");
	ret.setInteger(int64_t(_gMarked.length), "markedLength");

	// The IME window, laid bare: the document stays here, the platform only ever sees this
	// slice. windowLength staying small while charCount grows is the entire point.
	ret.setInteger(int64_t(_windowAnchor), "windowAnchor");
	ret.setInteger(int64_t(_windowLength), "windowLength");
	ret.setInteger(int64_t(_windowSerial), "windowSerial");
	ret.setInteger(int64_t(_pushes.size()), "windowPushes");

	ret.setInteger(int64_t(view->getVisualLineCount()), "visualLineCount");
	ret.setInteger(int64_t(lineHeight > 0.0f ? offset.y / lineHeight : 0.0f), "firstVisibleLine");
	ret.setDouble(double(view->getVisibleLineCount()), "visibleLineCount");
	ret.setDouble(double(lineHeight), "lineHeight");
	ret.setDouble(double(view->getCellWidth()), "cellWidth");

	ret.setDouble(double(offset.x), "scrollX");
	ret.setDouble(double(offset.y), "scrollY");
	ret.setDouble(double(range.width), "scrollRangeX");
	ret.setDouble(double(range.height), "scrollRangeY");

	// The viewport and the document model it shows. Reported because every scroll and wrap
	// question is really a question about these two, and a zero viewport - a widget asked about
	// before it was ever laid out - otherwise looks like a wrap bug.
	ret.setDouble(double(view->getContentSize().width), "viewWidth");
	ret.setDouble(double(view->getContentSize().height), "viewHeight");
	ret.setDouble(double(view->getWrapWidth()), "wrapWidth");
	ret.setDouble(double(_doc.getTotalRows()) * double(lineHeight), "docHeight");

	// The virtualization, laid bare: how many blocks exist, how many hold labels, how big the
	// pool got. materializedCount ≈ visible + margins is the health check; poolSize growing
	// without bound would mean the recycling broke.
	ret.setInteger(int64_t(_doc.getBlockCount()), "blockCount");
	ret.setInteger(int64_t(_doc.getChunkSize()), "chunkSize");
	ret.setInteger(int64_t(materialized.first), "firstMaterializedBlock");
	ret.setInteger(int64_t(view->getMaterializedCount()), "materializedCount");
	ret.setInteger(int64_t(view->getPoolSize()), "poolSize");

	const auto caret = view->getCaret();
	ret.setDouble(double(caret->getPosition().x), "caretX");
	ret.setDouble(double(caret->getPosition().y), "caretY");
	ret.setBool(caret->isVisible(), "caretVisible");

	// The widget's cursor and the highlight the labels were actually told to draw are two
	// different things, and only reporting both tells them apart when one of them is wrong.
	// Selection outside the materialized window is (correctly) not drawn, so this equals
	// cursorLength only when the selection fits the window.
	ret.setInteger(int64_t(view->getDrawnSelectionLength()), "drawnSelectionLength");

	ret.setString(string::toUtf8<Interface>(_gutterLabel->getString()), "gutterText");
	ret.setInteger(int64_t(_gutterColumns), "gutterColumns");

	// The history. `undoName` rather than only `canUndo`, because what a menu has to show is WHAT
	// would be taken back; `historyDepth` counts COMMITTED entries, so a word still being typed is
	// not in it yet - which is exactly why canUndo is asked separately and answers true anyway.
	ret.setBool(_history.isEnabled(), "undoEnabled");
	ret.setBool(canUndo(), "canUndo");
	ret.setBool(canRedo(), "canRedo");
	ret.setString(getUndoName(), "undoName");
	ret.setString(getRedoName(), "redoName");
	ret.setInteger(int64_t(_history.getDepth()), "historyDepth");
	ret.setInteger(int64_t(_history.getPosition()), "historyPosition");

	return ret;
}

bool TextView::handleInspectorCommand(StringView action, const Value &args, Value &result) {
	auto view = getView();

	if (action.ends_with("doc-selftest")) {
		// The document model is pure index arithmetic; this runs its own synthetic checks so the
		// model is falsifiable over the socket without rendering a single frame.
		String err;
		if (!TextDocument::selfTest(err)) {
			result.setString(err, "error");
			return false;
		}
		return true;
	} else if (action.ends_with("state")) {
		result.setValue(encodeState(), "state");
		return true;
	} else if (action.ends_with("set-text")) {
		setText(args.getString("text"));
		return true;
	} else if (action.ends_with("insert")) {
		const auto at = uint32_t(sprt::max(args.getInteger("at"), int64_t(0)));
		insertText(string::toUtf16<Interface>(args.getString("text")),
				TextCursor(sprt::min(at, uint32_t(_doc.size()))));
		return true;
	} else if (action.ends_with("set-cursor")) {
		const auto start = uint32_t(sprt::max(args.getInteger("start"), int64_t(0)));
		const auto length = uint32_t(sprt::max(args.getInteger("length"), int64_t(0)));
		setCursor(TextCursor(start, length));
		return true;
	} else if (action.ends_with("select")) {
		const auto from = getIndexForLineColumn(uint32_t(args.getInteger("line")),
				uint32_t(args.getInteger("column")));
		const auto to = getIndexForLineColumn(uint32_t(args.getInteger("endLine")),
				uint32_t(args.getInteger("endColumn")));
		setCursor(TextCursor(sprt::min(from, to),
				uint32_t(sprt::max(from, to) - sprt::min(from, to))));
		return true;
	} else if (action.ends_with("select-all")) {
		selectAll();
		return true;
	} else if (action.ends_with("undo")) {
		return undo();
	} else if (action.ends_with("redo")) {
		return redo();
	} else if (action.ends_with("undo-enabled")) {
		setUndoEnabled(args.getBool("value"));
		return true;
	} else if (action.ends_with("history-break")) {
		// End the run in progress without waiting for its idle window: what a caller does when it
		// knows the thought is over, and what lets a check script assert coalescing without
		// sleeping through a real one.
		_history.breakRun();
		return true;
	} else if (action.ends_with("history-idle")) {
		_history.setCoalesceIdle(uint64_t(sprt::max(args.getInteger("value"), int64_t(0))));
		return true;
	} else if (action.ends_with("history-clear")) {
		_history.clear();
		return true;
	} else if (action.ends_with("copy")) {
		return copy();
	} else if (action.ends_with("cut")) {
		return cut();
	} else if (action.ends_with("paste")) {
		return paste();
	} else if (action.ends_with("goto")) {
		const auto index = getIndexForLineColumn(uint32_t(args.getInteger("line")),
				uint32_t(args.getInteger("column")));
		setCursor(TextCursor(index));
		scrollToLine(uint32_t(args.getInteger("line")));
		return true;
	} else if (action.ends_with("scroll-to-line")) {
		scrollToLine(uint32_t(sprt::max(args.getInteger("line"), int64_t(0))));
		return true;
	} else if (action.ends_with("scroll-to-end")) {
		scrollToEnd();
		return true;
	} else if (action.ends_with("scroll")) {
		view->setScrollOffset(Vec2(float(args.getDouble("x")), float(args.getDouble("y"))));
		return true;
	} else if (action.ends_with("wrap")) {
		setWordWrap(args.getBool("value"));
		return true;
	} else if (action.ends_with("gutter")) {
		setGutterVisible(args.getBool("value"));
		return true;
	} else if (action.ends_with("caret-blink")) {
		setCaretBlink(args.getBool("value"));
		return true;
	} else if (action.ends_with("focus")) {
		focus();
		return true;
	} else if (action.ends_with("key")) {
		// A synthetic key press, so vertical motion and the goal column are assertable without a
		// real event and without a window manager.
		const auto name = args.getString("name");
		const bool select = args.getBool("shift");
		if (name == "UP") {
			moveCursorVertical(-1, select);
		} else if (name == "DOWN") {
			moveCursorVertical(1, select);
		} else if (name == "LEFT") {
			moveCursorHorizontal(offsetGlobal(-1), select);
		} else if (name == "RIGHT") {
			moveCursorHorizontal(offsetGlobal(1), select);
		} else if (name == "PAGE_UP" || name == "PAGE_DOWN") {
			const auto page = sprt::max(int32_t(view->getVisibleLineCount()) - 1, 1);
			moveCursorVertical(name == "PAGE_UP" ? -page : page, select);
		} else if (name == "HOME" || name == "END") {
			const auto bounds =
					view->getVisualLineBounds(view->getVisualLineForChar(_gCursor.start));
			moveCursorHorizontal(name == "HOME" ? bounds.first : bounds.second, select);
		} else {
			return false;
		}
		return true;
	} else if (action.ends_with("lines")) {
		const auto offset = size_t(sprt::max(args.getInteger("offset"), int64_t(0)));
		const auto requested = args.getInteger("limit");
		const auto limit = size_t(requested > 0 ? requested : 40);

		Value lines(Value::Type::ARRAY);
		for (size_t i = offset; i < _doc.getLineCount() && i < offset + limit; ++i) {
			const auto start = _doc.getLineStart(uint32_t(i));
			const auto length = _doc.getLineLength(uint32_t(i));
			Value line;
			line.setInteger(int64_t(i), "index");
			line.setInteger(int64_t(start), "start");
			line.setInteger(int64_t(length), "length");
			line.setString(string::toUtf8<Interface>(_doc.slice(start, length)), "text");
			lines.addValue(sp::move(line));
		}

		result.setInteger(int64_t(_doc.getLineCount()), "count");
		result.setValue(sp::move(lines), "lines");
		return true;
	}

	return false;
}

WideStringView TextView::sliceForHistory(uint32_t pos, uint32_t len) const {
	return _doc.slice(pos, len);
}

void TextView::applyHistoryEdit(uint32_t pos, uint32_t removed, WideStringView inserted) {
	insertGlobal(inserted, TextCursor(pos, removed));
}

void TextView::setHistoryCursor(TextCursor cursor) {
	setGlobalCursorInternal(cursor);
	pushCursorUpdate();
}

} // namespace stappler::xenolith::ui
