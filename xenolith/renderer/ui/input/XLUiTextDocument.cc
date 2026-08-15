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

#include "XLUiTextDocument.h"

#include <algorithm>

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

TextDocument::Edit TextDocument::diff(WideStringView oldStr, WideStringView newStr) {
	const size_t os = oldStr.size(), ns = newStr.size();
	size_t p = 0;
	const size_t maxP = std::min(os, ns);
	while (p < maxP && oldStr[p] == newStr[p]) { ++p; }
	size_t s = 0;
	const size_t maxS = std::min(os, ns) - p;
	while (s < maxS && oldStr[os - 1 - s] == newStr[ns - 1 - s]) { ++s; }
	return Edit{uint32_t(p), uint32_t(os - p - s), uint32_t(ns - p - s)};
}

uint32_t TextDocument::countCells(WideStringView str) {
	uint32_t cells = 0;
	for (size_t i = 0; i < str.size(); ++i) {
		const char16_t c = str[i];
		if (c == u'\t') {
			cells += 4; // the formatter advances a tab to the next multiple of 4 space widths
		} else if (c >= 0xDC00 && c <= 0xDFFF) {
			// low surrogate: the pair was counted at its high half
		} else if (c >= 0xD800 && c <= 0xDBFF) {
			cells += 2; // a supplementary-plane char (emoji and rare CJK) is typically wide
		} else if ((c >= 0x1100 && c <= 0x115F) || (c >= 0x2E80 && c <= 0xA4CF)
				|| (c >= 0xAC00 && c <= 0xD7A3) || (c >= 0xF900 && c <= 0xFAFF)
				|| (c >= 0xFE30 && c <= 0xFE4F) || (c >= 0xFF00 && c <= 0xFF60)
				|| (c >= 0xFFE0 && c <= 0xFFE6)) {
			cells += 2; // East Asian wide/fullwidth: two cells in any monospace fallback
		} else {
			cells += 1;
		}
	}
	return cells;
}

TextDocument::TextDocument() {
	_lineStarts.emplace_back(0);
	_blockPrefix.emplace_back(0);
	_blockPrefix.emplace_back(1); // the empty document is one empty line in one empty block
	_blockRows.emplace_back(1);
}

WideStringView TextDocument::slice(uint32_t pos, uint32_t len) const {
	if (pos >= _text.size()) {
		return WideStringView();
	}
	return WideStringView(_text.data() + pos, std::min(size_t(len), _text.size() - pos));
}

void TextDocument::setText(WideStringView str) {
	_text.assign(str.data(), str.size());

	_lineStarts.clear();
	_lineStarts.emplace_back(0);
	for (size_t i = 0; i < _text.size(); ++i) {
		if (_text[i] == u'\n') {
			_lineStarts.emplace_back(uint32_t(i + 1));
		}
	}

	rebuildAllBlocks();
}

void TextDocument::apply(uint32_t pos, uint32_t removed, WideStringView inserted) {
	pos = uint32_t(std::min(size_t(pos), _text.size()));
	removed = uint32_t(std::min(size_t(removed), _text.size() - pos));

	const uint32_t oldEnd = pos + removed;
	const uint32_t firstLine = getLineForIndex(pos);

	// Line starts strictly inside (pos, oldEnd] vanish with the removed range. Everything the
	// edit touches - the old affected span and the block count it owned - is measured BEFORE
	// the arrays change, because both are expressed through the old prefix values.
	auto lo = std::upper_bound(_lineStarts.begin(), _lineStarts.end(), pos);
	auto hi = std::upper_bound(_lineStarts.begin(), _lineStarts.end(), oldEnd);
	const size_t loIdx = size_t(lo - _lineStarts.begin());
	const uint32_t oldLines = uint32_t(hi - lo) + 1; // affected = removed starts + the host line
	const uint32_t oldBlocks = _blockPrefix[firstLine + oldLines] - _blockPrefix[firstLine];

	_text.replace(pos, removed, inserted.data(), inserted.size());
	const int64_t delta = int64_t(inserted.size()) - int64_t(removed);

	Vector<uint32_t> fresh; // starts of the lines the inserted text creates, in NEW coordinates
	for (size_t i = 0; i < inserted.size(); ++i) {
		if (inserted[i] == u'\n') {
			fresh.emplace_back(pos + uint32_t(i) + 1);
		}
	}

	_lineStarts.erase(lo, hi);
	_lineStarts.insert(_lineStarts.begin() + loIdx, fresh.begin(), fresh.end());
	for (size_t i = loIdx + fresh.size(); i < _lineStarts.size(); ++i) {
		_lineStarts[i] = uint32_t(int64_t(_lineStarts[i]) + delta);
	}

	spliceBlocks(firstLine, oldLines, uint32_t(fresh.size()) + 1, oldBlocks);
}

uint32_t TextDocument::getLineStart(uint32_t line) const {
	return _lineStarts[std::min(size_t(line), _lineStarts.size() - 1)];
}

uint32_t TextDocument::getLineLength(uint32_t line) const {
	line = uint32_t(std::min(size_t(line), _lineStarts.size() - 1));
	const uint32_t start = _lineStarts[line];
	const uint32_t end =
			(line + 1 < _lineStarts.size()) ? _lineStarts[line + 1] - 1 : uint32_t(_text.size());
	return end - start;
}

uint32_t TextDocument::getLineForIndex(uint32_t index) const {
	auto it = std::upper_bound(_lineStarts.begin(), _lineStarts.end(), index);
	return uint32_t(it - _lineStarts.begin()) - 1;
}

Pair<uint32_t, uint32_t> TextDocument::getLineColumn(uint32_t index) const {
	index = uint32_t(std::min(size_t(index), _text.size()));
	const uint32_t line = getLineForIndex(index);
	return pair(line, index - _lineStarts[line]);
}

uint32_t TextDocument::getIndexForLineColumn(uint32_t line, uint32_t column) const {
	line = uint32_t(std::min(size_t(line), _lineStarts.size() - 1));
	return _lineStarts[line] + std::min(column, getLineLength(line));
}

void TextDocument::setChunkSize(uint32_t value) {
	value = std::max(value, uint32_t(1));
	if (value != _chunk) {
		_chunk = value;
		rebuildAllBlocks();
	}
}

uint32_t TextDocument::getBlockCount() const { return _blockPrefix.back(); }

uint32_t TextDocument::getBlocksForLine(uint32_t line) const {
	const uint32_t len = getLineLength(line);
	return std::max(uint32_t(1), (len + _chunk - 1) / _chunk);
}

uint32_t TextDocument::getFirstBlockForLine(uint32_t line) const {
	return _blockPrefix[std::min(size_t(line), _blockPrefix.size() - 1)];
}

uint32_t TextDocument::getBlockForIndex(uint32_t index) const {
	index = uint32_t(std::min(size_t(index), _text.size()));
	const uint32_t line = getLineForIndex(index);
	const uint32_t offset = index - _lineStarts[line];
	// An index at the very end of the line (on its '\n', or past a chunk-aligned tail) belongs
	// to the LAST chunk: it is a caret position there, not the start of a chunk that does not
	// exist.
	const uint32_t chunk = std::min(offset / _chunk, getBlocksForLine(line) - 1);
	return _blockPrefix[line] + chunk;
}

TextDocument::BlockSpan TextDocument::getBlock(uint32_t blockIndex) const {
	const uint32_t total = getBlockCount();
	blockIndex = std::min(blockIndex, total - 1);

	auto it = std::upper_bound(_blockPrefix.begin(), _blockPrefix.end(), blockIndex);
	const uint32_t line = uint32_t(it - _blockPrefix.begin()) - 1;
	const uint32_t chunk = blockIndex - _blockPrefix[line];
	const uint32_t len = getLineLength(line);

	BlockSpan span;
	span.line = line;
	span.chunk = chunk;
	span.start = _lineStarts[line] + chunk * _chunk;
	span.length = std::min(_chunk, len - chunk * _chunk);
	return span;
}

uint32_t TextDocument::getBlockRows(uint32_t blockIndex) const {
	return blockIndex < _blockRows.size() ? _blockRows[blockIndex] : 1;
}

void TextDocument::setBlockRows(uint32_t blockIndex, uint32_t rows) {
	rows = std::max(rows, uint32_t(1));
	if (blockIndex < _blockRows.size() && _blockRows[blockIndex] != rows) {
		_blockRows[blockIndex] = rows;
		_rowPrefixValid = std::min(_rowPrefixValid, blockIndex);
	}
}

void TextDocument::estimateRows(uint32_t columns) {
	estimateRowsRange(0, getBlockCount(), columns);
}

void TextDocument::estimateRowsRange(uint32_t firstBlock, uint32_t pastLast, uint32_t columns) {
	if (columns == 0) {
		return;
	}
	pastLast = sprt::min(pastLast, getBlockCount());
	for (uint32_t b = firstBlock; b < pastLast; ++b) {
		const auto span = getBlock(b);
		const auto cells = countCells(slice(span.start, span.length));
		_blockRows[b] = sprt::max(uint32_t(1), (cells + columns - 1) / columns);
	}
	_rowPrefixValid = sprt::min(_rowPrefixValid, firstBlock);
}

uint64_t TextDocument::getRowsBefore(uint32_t blockIndex) const {
	ensureRowPrefix();
	return _rowPrefix[std::min(size_t(blockIndex), _rowPrefix.size() - 1)];
}

uint64_t TextDocument::getTotalRows() const {
	ensureRowPrefix();
	return _rowPrefix.back();
}

uint32_t TextDocument::getBlockForRow(uint64_t row) const {
	ensureRowPrefix();
	auto it = std::upper_bound(_rowPrefix.begin(), _rowPrefix.end(), row);
	if (it == _rowPrefix.begin()) {
		return 0;
	}
	return std::min(uint32_t(it - _rowPrefix.begin()) - 1, uint32_t(_blockRows.size() - 1));
}

void TextDocument::rebuildAllBlocks() {
	const size_t lineCount = _lineStarts.size();
	_blockPrefix.clear();
	_blockPrefix.reserve(lineCount + 1);
	_blockPrefix.emplace_back(0);
	uint32_t acc = 0;
	for (size_t l = 0; l < lineCount; ++l) {
		acc += getBlocksForLine(uint32_t(l));
		_blockPrefix.emplace_back(acc);
	}
	_blockRows.assign(acc, 1);
	_rowPrefix.clear();
	_rowPrefixValid = 0;
}

void TextDocument::spliceBlocks(uint32_t firstLine, uint32_t oldLines, uint32_t newLines,
		uint32_t oldBlocks) {
	const uint32_t base = _blockPrefix[firstLine];

	Vector<uint32_t> counts;
	counts.reserve(newLines);
	uint32_t newBlocks = 0;
	for (uint32_t l = firstLine; l < firstLine + newLines; ++l) {
		const uint32_t b = getBlocksForLine(l);
		counts.emplace_back(b);
		newBlocks += b;
	}

	// Rows of the affected blocks reset to 1: their content changed, any measurement or
	// estimate they carried is void. The view re-estimates or re-measures on the next pass.
	_blockRows.erase(_blockRows.begin() + base, _blockRows.begin() + base + oldBlocks);
	_blockRows.insert(_blockRows.begin() + base, newBlocks, 1u);

	const int64_t deltaBlocks = int64_t(newBlocks) - int64_t(oldBlocks);
	_blockPrefix.erase(_blockPrefix.begin() + firstLine + 1,
			_blockPrefix.begin() + firstLine + 1 + oldLines);
	Vector<uint32_t> prefix;
	prefix.reserve(newLines);
	uint32_t acc = base;
	for (auto c : counts) {
		acc += c;
		prefix.emplace_back(acc);
	}
	_blockPrefix.insert(_blockPrefix.begin() + firstLine + 1, prefix.begin(), prefix.end());
	for (size_t i = firstLine + 1 + newLines; i < _blockPrefix.size(); ++i) {
		_blockPrefix[i] = uint32_t(int64_t(_blockPrefix[i]) + deltaBlocks);
	}

	_rowPrefixValid = std::min(_rowPrefixValid, base);
}

void TextDocument::ensureRowPrefix() const {
	const size_t n = _blockRows.size();
	if (_rowPrefix.size() != n + 1) {
		_rowPrefix.resize(n + 1);
		_rowPrefix[0] = 0;
		if (_rowPrefixValid > n) {
			_rowPrefixValid = 0;
		}
	}
	for (size_t i = _rowPrefixValid; i < n; ++i) {
		_rowPrefix[i + 1] = _rowPrefix[i] + _blockRows[i];
	}
	_rowPrefixValid = uint32_t(n);
}

bool TextDocument::selfTest(String &err) {
	auto fail = [&](StringView what) {
		err = what.str<Interface>();
		return false;
	};

	// -- diff --
	{
		auto d = diff(u"hello", u"hexxllo");
		if (d.pos != 2 || d.removed != 0 || d.inserted != 2) {
			return fail("diff insert");
		}
		d = diff(u"hello", u"hlo");
		if (d.pos != 1 || d.removed != 2 || d.inserted != 0) {
			return fail("diff remove");
		}
		d = diff(u"abc", u"abc");
		if (d.removed != 0 || d.inserted != 0) {
			return fail("diff equal");
		}
		d = diff(u"", u"abc");
		if (d.pos != 0 || d.removed != 0 || d.inserted != 3) {
			return fail("diff from empty");
		}
		d = diff(u"aa", u"aaa");
		if (d.removed != 0 || d.inserted != 1) {
			return fail("diff ambiguous repeat");
		}
	}

	// -- static structure --
	{
		TextDocument doc;
		doc.setChunkSize(4);
		doc.setText(u"ab\nc\n\nlongline\n");
		// lines: "ab", "c", "", "longline", "" (trailing newline opens an empty last line)
		if (doc.getLineCount() != 5) {
			return fail("lineCount");
		}
		if (doc.getLineLength(0) != 2 || doc.getLineLength(2) != 0 || doc.getLineLength(3) != 8
				|| doc.getLineLength(4) != 0) {
			return fail("lineLength");
		}
		// blocks: 1 + 1 + 1 + 2 ("longline" is 8 chars over chunk 4) + 1
		if (doc.getBlockCount() != 6) {
			return fail("blockCount");
		}
		auto b = doc.getBlock(4); // second chunk of "longline"
		if (b.line != 3 || b.chunk != 1 || b.length != 4
				|| doc.slice(b.start, b.length) != WideStringView(u"line")) {
			return fail("block span");
		}
		if (doc.getBlockForIndex(doc.getLineStart(3) + 8) != 4) {
			return fail("blockForIndex at line end");
		}
		if (doc.getBlockForIndex(uint32_t(doc.size())) != 5) {
			return fail("blockForIndex at doc end");
		}
		auto lc = doc.getLineColumn(doc.getIndexForLineColumn(3, 6));
		if (lc.first != 3 || lc.second != 6) {
			return fail("line/column roundtrip");
		}

		// -- rows --
		doc.setBlockRows(3, 2);
		doc.setBlockRows(4, 3);
		if (doc.getTotalRows() != 1 + 1 + 1 + 2 + 3 + 1) {
			return fail("totalRows");
		}
		if (doc.getRowsBefore(4) != 5 || doc.getBlockForRow(5) != 4 || doc.getBlockForRow(0) != 0
				|| doc.getBlockForRow(1'000) != 5) {
			return fail("row mapping");
		}
	}

	// -- incremental vs full rebuild, randomized --
	{
		TextDocument inc;
		inc.setChunkSize(3);
		inc.setText(u"one\ntwo two\n\nthree");

		// xorshift so the sequence is reproducible in a debugger
		uint32_t rnd = 0x1'2345;
		auto next = [&] {
			rnd ^= rnd << 13;
			rnd ^= rnd >> 17;
			rnd ^= rnd << 5;
			return rnd;
		};
		const char16_t alphabet[] = u"ab\ncd\n\nxyz\tw";

		for (int step = 0; step < 300; ++step) {
			const uint32_t pos = next() % uint32_t(inc.size() + 1);
			const uint32_t removed = next() % 5;
			WideString ins;
			for (uint32_t k = next() % 6; k > 0; --k) {
				ins += alphabet[next() % (sizeof(alphabet) / sizeof(char16_t) - 1)];
			}
			inc.apply(pos, removed, ins);

			TextDocument ref;
			ref.setChunkSize(3);
			ref.setText(inc.getView());
			if (inc._lineStarts != ref._lineStarts) {
				return fail(toString("random step ", step, ": lineStarts diverged"));
			}
			if (inc._blockPrefix != ref._blockPrefix) {
				return fail(toString("random step ", step, ": blockPrefix diverged"));
			}
			if (inc._blockRows.size() != ref._blockRows.size()) {
				return fail(toString("random step ", step, ": blockRows size diverged"));
			}
			if (inc.getTotalRows() != ref.getTotalRows()) {
				return fail(toString("random step ", step, ": totalRows diverged"));
			}
		}
	}

	return true;
}

} // namespace stappler::xenolith::ui
