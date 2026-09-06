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

#ifndef XENOLITH_RENDERER_UI_INPUT_XLUITEXTDOCUMENT_H_
#define XENOLITH_RENDERER_UI_INPUT_XLUITEXTDOCUMENT_H_

#include "XLCommon.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// The text model behind TextView: the document itself, its logical lines, and the "blocks"
// the view materializes - one block per chunk of a line, one Label per block.
//
// This exists because the widget stopped keeping the document inside TextInput's _inputState.
// The IME contract carries a single string whose layout, echo and re-echo are all O(size of that
// string), so the widget keeps only a small window of the document there - and something else has
// to be the authority on the whole text. This is that something: plain data and index arithmetic,
// no nodes, no fonts, testable over the inspector socket without a single frame rendered.
//
// LINES AND BLOCKS. A logical line is what the string says (split on '\n'). A block is what one
// Label is allowed to hold: lines longer than the chunk size are cut into chunk-sized blocks,
// because the font formatter lays out in a uint16_t domain (charNum caps one label at 65535
// chars, CharLayoutData::pos caps an unwrapped line at 32767 layout units) and a block is the
// unit that provably fits. A chunk boundary therefore acts as a forced line break on screen.
//
// INDEX MAINTENANCE IS INCREMENTAL. Every keystroke lands here as apply(); rebuilding the line
// index from scratch would be O(document) per keystroke, which is the exact cost this design
// removes. Instead the tail of the index is shifted and only the inserted text is scanned. The
// tail shift itself (a memmove of the arrays past the edit) is the accepted price: ~4 MB per
// million lines, fractions of a millisecond.
//
// ROWS. The vertical model counts visual rows per block: 1 without wrapping, measured or
// estimated with it. Row prefix sums answer "which block is at scroll offset Y" and "how tall is
// the document"; they are rebuilt lazily from the first dirty block. Rows are integers on
// purpose - multiplying by the (uniform, monospace) line height happens in the view, in double:
// document Y coordinates reach millions of pixels, where float's 1-ulp step is already 2 px.
class SP_PUBLIC TextDocument {
public:
	// One single-range replacement: `removed` code units at `pos` replaced by `inserted` ones.
	// Also the shape of a window diff - every operation the runtime's TextInputProcessor
	// performs is a single-range edit, so a prefix/suffix diff reconstructs it exactly.
	struct Edit {
		uint32_t pos = 0;
		uint32_t removed = 0;
		uint32_t inserted = 0;
	};

	// Where a block lives: its line, its chunk ordinal on that line, and its slice of the
	// document. The slice never includes the line's trailing '\n'.
	struct BlockSpan {
		uint32_t line = 0;
		uint32_t chunk = 0;
		uint32_t start = 0;
		uint32_t length = 0;
	};

	// Diff of two versions of the same window by common prefix/suffix. Exact for single-range
	// edits; for equal strings answers {size, 0, 0}. The ambiguity of repeated characters
	// ("aa" -> "aaa" could be an insert at 0, 1 or 2) is content-equivalent, and the cursor is
	// taken from the echo itself, never derived from the diff position.
	static Edit diff(WideStringView oldStr, WideStringView newStr);

	// Visual cells of a monospace slice: tab counts as 4 (the formatter advances to the next
	// multiple of 4 space widths), wide (CJK and fullwidth) as 2, everything else as 1. Feeds
	// the row ESTIMATE for blocks that were never laid out; the measured value from a real
	// Label replaces it once the block is materialized.
	static uint32_t countCells(WideStringView);

	// Runs the index arithmetic against synthetic documents and answers whether it held.
	// Wired to an inspector command so the model is checkable headless, without rendering.
	static bool selfTest(String &err);

	TextDocument();

	WideStringView getView() const { return WideStringView(_text); }
	size_t size() const { return _text.size(); }
	bool empty() const { return _text.empty(); }
	WideStringView slice(uint32_t pos, uint32_t len) const;

	// Full replacement: the only O(document) rebuild, used by setText/loadFile.
	void setText(WideStringView);

	// The incremental path: replace `removed` units at `pos` with `inserted`.
	void apply(uint32_t pos, uint32_t removed, WideStringView inserted);

	// -- logical lines --

	uint32_t getLineCount() const { return uint32_t(_lineStarts.size()); }
	uint32_t getLineStart(uint32_t line) const;

	// Length without the trailing '\n' - the text a Label of this line would show.
	uint32_t getLineLength(uint32_t line) const;

	uint32_t getLineForIndex(uint32_t index) const;
	Pair<uint32_t, uint32_t> getLineColumn(uint32_t index) const;
	uint32_t getIndexForLineColumn(uint32_t line, uint32_t column) const;

	// -- blocks --

	// The chunk size defines the block structure wholesale, so changing it rebuilds the block
	// index and resets every row to 1 (the caller re-estimates). It changes when wrapping is
	// toggled: an unwrapped block must fit the formatter's X ceiling, a wrapped one only its
	// char-count ceiling, so the two modes want very different chunks.
	void setChunkSize(uint32_t);
	uint32_t getChunkSize() const { return _chunk; }

	uint32_t getBlockCount() const;
	uint32_t getBlocksForLine(uint32_t line) const;
	uint32_t getFirstBlockForLine(uint32_t line) const;
	uint32_t getBlockForIndex(uint32_t index) const;
	BlockSpan getBlock(uint32_t blockIndex) const;

	// -- rows (vertical model) --

	uint32_t getBlockRows(uint32_t blockIndex) const;
	void setBlockRows(uint32_t blockIndex, uint32_t rows);

	// Estimates every block's rows for wrapping at `columns` cells: ceil(cells / columns), cells
	// counted by countCells(). One O(document) pass, run when wrapping turns on or the width
	// changes. The estimate is monospace-exact for ASCII; the measured value from a materialized
	// label replaces it block by block.
	void estimateRows(uint32_t columns);

	// The incremental counterpart: re-estimates [firstBlock, pastLast) after an edit reset the
	// affected blocks' rows to 1. Without this a scroll pinned to the bottom would aim at a
	// document whose freshly edited tail claims one row per block until it is measured.
	void estimateRowsRange(uint32_t firstBlock, uint32_t pastLast, uint32_t columns);

	uint64_t getRowsBefore(uint32_t blockIndex) const;
	uint64_t getTotalRows() const;

	// Block containing the given row; rows past the end answer the last block.
	uint32_t getBlockForRow(uint64_t row) const;

protected:
	// Recomputes the whole block index; the O(lines) path behind setText and setChunkSize.
	void rebuildAllBlocks();

	// Replaces the block entries of `oldLines` lines starting at firstLine with entries for
	// `newLines` lines (the affected span of an apply()), shifting the tail - the incremental
	// counterpart of rebuildAllBlocks. `oldBlocks` is the block count of the replaced span,
	// measured against the prefix BEFORE the line index changed.
	void spliceBlocks(uint32_t firstLine, uint32_t oldLines, uint32_t newLines, uint32_t oldBlocks);

	void ensureRowPrefix() const;

	WideString _text;

	// Index of the first character of each logical line; always non-empty (line 0 starts at 0).
	Vector<uint32_t> _lineStarts;

	// _blockPrefix[i] = number of blocks before line i; size == lineCount + 1, so the last
	// entry is the total block count.
	Vector<uint32_t> _blockPrefix;

	// Rows per block, indexed by block; size == block count. 1 means "one visual row", which
	// is exact without wrapping and the floor of any estimate with it.
	Vector<uint32_t> _blockRows;

	// Lazy prefix sums over _blockRows; entries below _rowPrefixValid are current.
	mutable Vector<uint64_t> _rowPrefix;
	mutable uint32_t _rowPrefixValid = 0;

	uint32_t _chunk = 2'000;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_INPUT_XLUITEXTDOCUMENT_H_
