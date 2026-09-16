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

#ifndef XENOLITH_RENDERER_UI_INPUT_XLUITEXTVIEW_H_
#define XENOLITH_RENDERER_UI_INPUT_XLUITEXTVIEW_H_

#include "XLUiTextDocument.h"
#include "XL2dLabel.h"
#include "XL2dLayer.h"
#include "XLDynamicStateSystem.h"
#include "XLSceneInspector.h"
#include "XLUiTextInput.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// Multi-line viewport for TextView, replacing the single-line geometry of the stock
// TextInputContainer through TextInput::makeContainer().
//
// The document is a TextDocument owned by the widget above; this container materializes a Label
// per visible block. Labels are pooled and reused by pointer: the CSS resolver keeps per-Node*
// maps that are not cleaned for deleted nodes.
//
// Geometry rules:
//  * The vertical model is integer rows times a uniform line height, measured from a reference
//    label. Scroll offset and document-space Y are double (document Y reaches millions of px) and
//    become float only after subtracting the viewport position.
//  * Service labels (the measure label, idle pooled labels) stay visible: an invisible node is not
//    visited, so it is never styled and loses its font. An empty label draws nothing.
//  * The caret is re-parented onto the stage and, when its block is outside the window, moved
//    rather than hidden (the blink action re-shows it); the scissor clips it.
//
// Scroll is a document-space offset from the top-left corner, both components >= 0. The
// horizontal range follows the materialized blocks, not the whole document, to avoid O(document)
// measurement per edit.
class SP_PUBLIC TextViewContainer : public TextInputContainer {
public:
	// Blocks kept laid out beyond each edge of the viewport, so a small scroll only positions
	// nodes instead of re-shaping text.
	static constexpr uint32_t kMaterializeMargin = 4;

	virtual ~TextViewContainer() = default;

	virtual bool init() override;

	virtual void update(const UpdateTime &) override;
	virtual void handleContentSizeDirty() override;

	// The whole per-frame pipeline lives here, before the base call: clamp the scroll against
	// the model, follow the caret if it moved, materialize the visible blocks (assign pooled
	// labels, lay them out, slice selection and marked ranges into them), position everything -
	// and only then let the base flush the caret, which by that point reads settled geometry.
	virtual bool visitDraw(FrameInfo &, NodeVisitFlags parentFlags) override;

	virtual void handleLabelChanged() override;

	// The document belongs to the widget above; the container only reads it and writes back
	// measured row counts.
	virtual void setDocument(TextDocument *doc) { _doc = doc; }

	// Document-space scroll from the top-left corner, clamped to getScrollRange(). The Vec2 API
	// is float for the callers' convenience; the authoritative value is double inside.
	virtual Vec2 getScrollOffset() const { return Vec2(float(_scrollX), float(_scrollY)); }
	virtual void setScrollOffset(const Vec2 &);
	virtual void scrollBy(const Vec2 &);
	virtual Size2 getScrollRange() const;

	virtual bool hasHorizontalOverflow() const override;
	virtual bool hasVerticalOverflow() const;

	virtual void moveHorizontalOverflow(float d) override;

	// The width the text wraps at, in px; 0 = no wrapping. Applied to each materialized label via
	// Label::setWidth() (CSS `white-space` only allows breaking). Set by the widget above from the
	// layout pass that computes the viewport.
	virtual void setWrapWidth(float);
	virtual float getWrapWidth() const { return _wrapWidth; }

	// Visual rows of the whole document - the model's row count, not any label's. With wrap off
	// a row is a block; with wrap on a block spans several. See TextView on the
	// logical/visual distinction.
	virtual uint32_t getVisualLineCount() const;
	virtual uint32_t getVisualLineForChar(uint32_t) const;

	// Character on visual row `row` nearest to `goalX` (block-local px). Used by Up/Down, which
	// keep a column across rows of different lengths. Falls back to cell arithmetic when the
	// row's block is not materialized: gestures and synthetic keys run between frames.
	virtual uint32_t getCharForVisualLine(uint32_t row, float goalX) const;

	// First and last caret position on visual row `row`. Blocks never contain '\n' (the line
	// index splits on it), so unlike the single-label design there is no trailing break to
	// exclude.
	virtual Pair<uint32_t, uint32_t> getVisualLineBounds(uint32_t row) const;

	// Block-local caret x for the character, in px - the column memory for vertical motion.
	virtual float getCursorDocX(uint32_t index) const;

	// Uniform line height and monospace cell width, measured from the reference label. Zero
	// until the font loads; every caller treats zero as "not ready yet".
	virtual float getLineHeight() const { return _lineHeight; }
	virtual float getCellWidth() const { return _cellWidth; }
	virtual float getVisibleLineCount() const;

	virtual void setCurrentLineVisible(bool);
	virtual bool isCurrentLineVisible() const { return _currentLineVisible; }

	// Fired when the scroll offset changes; the gutter rides it.
	virtual void setScrollCallback(Function<void(const Vec2 &)> &&);

	// Fired after materialization when anything the gutter depends on changed: the block
	// window, the scroll position or the line structure. Runs inside visitDraw, after the
	// blocks are laid out.
	virtual void setMaterializeCallback(Function<void()> &&);

	// The block range currently holding labels, [first, past-last).
	virtual Pair<uint32_t, uint32_t> getMaterializedBlocks() const {
		return pair(_matFirst, _matLast);
	}
	virtual uint32_t getMaterializedCount() const;
	virtual uint32_t getPoolSize() const { return uint32_t(_slots.size()); }

	// Sum of the selection lengths the materialized labels were told to draw (for checks against
	// the widget's cursor).
	virtual uint32_t getDrawnSelectionLength() const;

	virtual void scrollToCursor();
	virtual void scrollToEnd();

	// In integer rows, not pixels: the pixel offset of a row deep in a huge document does not
	// survive a float round trip through the Vec2 API.
	virtual void scrollToRow(uint64_t row);

	// The viewport follows the caret only when the cursor moves, not on a wheel scroll or
	// scrollToLine(). Recorded here, consumed by the next visitDraw.
	virtual void setCursor(TextCursor, uint32_t activePosition = maxOf<uint32_t>()) override;
	virtual void setMarked(TextCursor) override;

	virtual void setSelectionColor(const Color4F &) override;
	virtual void setMarkedColor(const Color4F &) override;

	virtual TextCursor getCursorForPosition(const Vec2 &,
			font::CharSelectMode = font::CharSelectMode::Best) override;

	// Word under the point, in document indices. Replaces TextInput::getWordForPosition, which
	// is not virtual and reads the base's (empty) label; the widget above overrides the tap and
	// long-press gestures to come here instead.
	virtual TextCursor getWordForPosition(const Vec2 &) const;

protected:
	struct Slot {
		uint32_t block = maxOf<uint32_t>();
		basic2d::Label *label = nullptr;
	};

	virtual void updateCaretPosition() override;

	// Nothing to centre: a multi-line view starts its text at the top and positions its own pooled
	// labels per frame. The base's single-line label is inherited but never shown here.
	virtual void updateLabelPosition() override { }

	// Reads line height and cell width off the reference label; cheap once measured, and
	// re-runs every frame so a font swap or a density change is picked up without a hook.
	void measureFont();

	void clampScroll();
	void materialize();
	Slot *slotForBlock(uint32_t block);
	basic2d::Label *makeSlot();

	// Applies content, width, selection and marked slices to a slot; answers whether the label
	// re-laid out (its row count may have changed).
	void assignSlot(Slot &, uint32_t block);

	// Container-space position of a block's bottom-left corner, from the row model.
	Vec2 blockPosition(uint32_t block) const;

	double docHeight() const;

	TextDocument *_doc = nullptr;

	// Children of the container: the stage holds the pooled labels, the caret and the measure
	// label; the strip lives beside them so the horizontal slide does not take it along.
	Node *_stage = nullptr;
	basic2d::Label *_measure = nullptr;
	basic2d::Layer *_currentLine = nullptr;

	Vector<Slot> _slots;

	Function<void(const Vec2 &)> _scrollCallback;
	Function<void()> _materializeCallback;

	double _scrollX = 0.0;
	double _scrollY = 0.0;
	float _wrapWidth = 0.0f;
	float _lineHeight = 0.0f;
	float _cellWidth = 0.0f;

	// Widest materialized label, px; the basis of the horizontal scroll range (see the class
	// comment on why it follows the window).
	float _maxBlockWidth = 0.0f;

	uint32_t _matFirst = 0;
	uint32_t _matLast = 0;

	// What the gutter last saw; the materialize callback fires only when this changes.
	uint64_t _notifiedState = maxOf<uint64_t>();
	Vec2 _notifiedScroll = Vec2(-1.0f, -1.0f);

	Color4F _selectionColor = Color4F::WHITE;
	Color4F _markedColor = Color4F::WHITE;

	TextCursor _markedGlobal = TextCursor::InvalidCursor;

	bool _currentLineVisible = false;
	bool _followCursor = false;
};

// Monospace multi-line text view for code editors, log panes and consoles. Line numbers, wrapping,
// current-line highlight, read-only and Tab-indents are switches.
//
// CSS: type `text-input` (it is a TextInput - the background, outline, radius and padding come from
// the same applier), plus the classes `text-view` on the widget, `text-view-gutter` /
// `text-view-gutter-label` on the line-number strip, `text-view-current-line` on the current-line
// highlight, and `xl-ui-text-input-label` on every content label. The gutter width in monospace
// cells is the custom property `--gutter-chars`:
//
//   text-input.text-view { font-family:monospace; --gutter-chars:4; }
//   .text-view-gutter { background-color:#1E1E1E; }
//   .text-view-current-line { background-color:rgba(255,255,255,.05); }
//
// The document lives in a TextDocument, rendered as a Label per visible block (see
// TextViewContainer). TextInput's _inputState holds only a window of a few kilobytes around the
// caret, since the IME string travels through every request and echo.
//
// Window protocol: every push carries a serial, and every edit the TextInputProcessor makes
// copies the state it is based on, so each echo carries the serial of the string it edited. The
// widget keeps its live pushes (serial, anchor, base string), diffs the echo against the base of
// its serial, and applies the edit at that push's anchor, so re-anchoring is safe while echoes
// are in flight. The window re-centres when the caret comes within kWindowGuard of an edge, never
// during a composition (dead keys keep their run in cursor.length with compose == Composing).
//
// The global cursor and selection (_gCursor/_gSelAnchor) are authoritative. The base's
// non-virtual cursor helpers (pendingCursor, offsetCursor, moveCursor, getWordForPosition) are
// unused. A selection wider than the window is pushed clipped; an echo that edits the string
// while it stands replaces the whole selection.
//
// Blocks keep every label under the formatter's uint16_t ceilings (charNum: 65535 chars;
// CharLayoutData::pos: 32767 layout units for an unwrapped line, so the unwrapped chunk is computed
// from cell width and density). No document size cap beyond memory.
//
// Not supported: password mode (the block model never masks) and FormAdapters (they use the
// non-virtual window-based accessors).
//
// Keyboard: as on TextInput, except that with TextInputType::MultiLineBit the processor inserts
// Enter as text. Tab arrives as the focusNext hotkey and is turned into an indent here.
class SP_PUBLIC TextView : public TextInput {
public:
	// Chunk of a line one Label holds when wrapping: only the uint16_t char counter binds (the
	// wrap width bounds X), with double headroom for 1->N shaping expansions.
	static constexpr uint32_t kChunkWrapped = 8'000;

	// The IME window: total size, and how close the caret may come to an edge before the window
	// re-centres. The guard leaves room for several autorepeat edits against the old window before
	// a re-anchor's echo returns; the processor silently no-ops a delete at its string's edge.
	static constexpr uint32_t kWindowMax = 4'096;
	static constexpr uint32_t kWindowGuard = 256;

	// encodeState() omits the full text above this size: a state poll must not serialize a
	// multi-megabyte document into every answer.
	static constexpr size_t kStateTextCap = 65'536;

	virtual ~TextView() = default;

	virtual bool init() override;

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;
	virtual void handleContentSizeDirty() override;

	// Reconciles the gutter strip with the gutter label's measured width before drawing. Checked
	// per frame: the font loads asynchronously with no notification.
	virtual bool visitDraw(FrameInfo &, NodeVisitFlags parentFlags) override;

	// The document-level text API. The change callback is fired with an empty view, to avoid an
	// O(document) UTF-8 conversion per keystroke. The using-declaration keeps the base's UTF-8
	// overload visible.
	using TextInput::setText;
	virtual void setText(WideStringView) override;
	virtual StringView getText() const override;

	virtual void focus() override;
	virtual void selectAll() override;

	// Global document indices, here and in every cursor-taking call of this class.
	virtual void setCursor(TextCursor) override;
	virtual TextCursor getGlobalCursor() const { return _gCursor; }

	virtual void setWordWrap(bool);
	virtual bool isWordWrap() const { return _wordWrap; }

	virtual void setGutterVisible(bool);
	virtual bool isGutterVisible() const { return _gutterVisible; }

	virtual void setCurrentLineHighlight(bool);

	// Tab inserts a literal '\t' instead of moving focus. Off by default: a console prompt
	// should still be escapable with Tab.
	virtual void setTabInsertsIndent(bool);
	virtual bool isTabInsertsIndent() const { return _tabInsertsIndent; }

	// Logical lines - lines of the string, not of the layout. Everything a user names
	// ("line 42") is logical; everything a user sees moving (Up, PageDown, the caret) is
	// visual. Both delegate to the document model.
	virtual uint32_t getLineCount() const;
	virtual Pair<uint32_t, uint32_t> getLineColumn(uint32_t index) const;
	virtual uint32_t getIndexForLineColumn(uint32_t line, uint32_t column) const;

	virtual void scrollToLine(uint32_t);
	virtual void scrollToEnd();

	TextViewContainer *getView() const;
	const TextDocument &getDocument() const { return _doc; }

	// -- TextHistoryTarget, over the document rather than over the IME's window --
	//
	// applyHistoryEdit routes through insertGlobal, the single insertion path, so an undo pushes a
	// fresh window, moves the caret and fires the change callback like typing.

	virtual WideStringView sliceForHistory(uint32_t pos, uint32_t len) const override;
	virtual void applyHistoryEdit(uint32_t pos, uint32_t removed, WideStringView) override;
	virtual void setHistoryCursor(TextCursor) override;

	// Every field the inspector reports, in one place so the console can nest its output
	// pane's state inside its own answer.
	virtual Value encodeState() const;

	// Shared by the editor and the console inspector commands.
	virtual bool handleInspectorCommand(StringView action, const Value &args, Value &result);

protected:
	// One live push: what was sent to the IME and where it came from. Kept until an echo with
	// this serial (or a later one) returns, so an in-flight edit of an already-replaced window
	// can still be diffed against its base and applied at its anchor.
	struct WindowPush {
		uint64_t serial = 0;
		uint32_t anchor = 0;
		Rc<TextInputString> base;
	};

	virtual Rc<TextInputContainer> makeContainer() override;

	virtual void handleTextInput(const TextInputState &) override;
	virtual void acquireInput(TextCursor) override;
	virtual void insertText(WideStringView text, TextCursor replaceGlobal) override;

	/* The clipboard hooks; copy/cut/paste/handleTextDrop live in the base. Both cursor hooks answer
	with the global cursor (pendingCursor() is unused here). */
	virtual TextCursor selectionCursor() const override { return _gCursor; }
	virtual TextCursor insertionCursor() const override { return _gCursor; }
	virtual WideStringView getTextForCursor(TextCursor) const override;

	// This class never masks, so a selection may always leave it.
	virtual bool canCopySelection() const override { return true; }

	// The base pours the window into its (empty, service) label here; the document model
	// renders instead, so this must do nothing.
	virtual void updateDisplayString() override { }

	virtual bool handleKey(const GestureData &) override;
	virtual bool handleTextHotkey(HotkeyId, const InputEvent &) override;

	// The base's gestures go through the non-virtual window-based cursor arithmetic; these
	// re-route the word lookup to the container and the cursor flow to the global layer.
	virtual bool handleTap(const GestureTap &) override;
	virtual bool handleLongPress(const GesturePress &) override;
	virtual bool handleSwipeBegin(const Vec2 &) override;
	virtual bool handleSwipe(const Vec2 &, const Vec2 &delta) override;

	virtual bool setStyleValue(const ResolvedStyle &, document::ParameterName,
			const document::StyleValue &) override;

	// Registers through here so handleExit() can remove them; their lambdas capture this widget.
	void addInspectorCommand(Scene *, StringView name, StringView desc);

	// The unwrapped chunk, from the live cell width and density: a chunk drawn as one physical
	// line must fit CharLayoutData::pos (int16_t, 32767 layout units = px * density), and both
	// the cell width (a CSS variable) and the density (the monitor) are runtime values.
	uint32_t computePlainChunk() const;

	void applyChunkSize();

	// Rebuilds the gutter for the materialized window only: numbers for the logical lines whose
	// first block is visible, blanks for wrap continuations and chunk tails.
	void rebuildGutter();

	// Vertical motion keeps the column the caret started from, so a run of Down through short
	// lines comes back to it. Any horizontal move or edit drops the goal.
	void moveCursorVertical(int32_t rows, bool select);
	void moveCursorHorizontal(uint32_t target, bool select);

	// -- the global cursor layer (replaces the base's window-based arithmetic) --

	uint32_t activeGlobal() const;
	uint32_t offsetGlobal(int32_t delta) const;
	void moveGlobal(uint32_t target, bool select);
	void setGlobalCursorInternal(TextCursor);
	void applyGestureGlobal(TextCursor);
	void acquireGlobal(TextCursor);
	void insertGlobal(WideStringView text, TextCursor replaceGlobal);

	// -- the window machinery --

	// CRLF -> LF, control chars except \n\t dropped; `changed` reports whether anything was.
	// Non-const because the per-character filter hook (handleInputChar) is not.
	WideString normalizeInput(WideStringView, bool &changed);

	// doc.apply plus the wrap-row re-estimate for the affected blocks.
	void applyDocEdit(uint32_t pos, uint32_t removed, WideStringView inserted);

	// [anchor, end) of a window centred on the given index. Snapped to line boundaries when
	// they are near, cut mid-line when the line itself is longer than the window.
	Pair<uint32_t, uint32_t> computeWindow(uint32_t center) const;

	static TextCursor clipToWindow(TextCursor global, uint32_t anchor, uint32_t length);

	bool needsReanchor() const;

	// A new window (new serial) centred on the caret; falls back to a local _inputState write
	// when no handler runs, as the base's setText does.
	void pushWindow();

	// The same window (same serial, same string object) with a new relative cursor, so the diff
	// base is unchanged.
	void pushCursorUpdate();

	TextDocument _doc;

	Vector<WindowPush> _pushes;
	uint64_t _windowSerial = 0;
	uint32_t _windowAnchor = 0;
	uint32_t _windowLength = 0;

	// The authoritative cursor/selection/marked state, in document indices. The window's
	// _inputState.cursor is a projection of this, never the other way around - except in
	// handleTextInput, where the echo's answer is translated back through its push's anchor.
	TextCursor _gCursor = TextCursor(0u);
	TextCursor _gMarked = TextCursor::InvalidCursor;
	uint32_t _gSelAnchor = maxOf<uint32_t>();

	basic2d::Layer *_gutter = nullptr;
	basic2d::Label *_gutterLabel = nullptr;

	Vector<String> _inspectorCommands;
	Scene *_inspectorScene = nullptr;

	uint32_t _gutterColumns = 0;
	float _gutterChars = 4.0f;

	// The gutter label width the strip was last sized from; compared in visitDraw. A re-mark from
	// inside handleContentSizeDirty would be lost, as the dirty flag is cleared after it returns.
	float _gutterAppliedWidth = -1.0f;

	// The wrap column count the row estimates were last computed for; the re-estimate runs only
	// when it changes (wrap toggled, viewport resized, font settled).
	uint32_t _estimatedColumns = 0;

	float _goalX = 0.0f;
	bool _goalValid = false;
	bool _wordWrap = false;
	bool _gutterVisible = false;
	bool _tabInsertsIndent = false;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_INPUT_XLUITEXTVIEW_H_
