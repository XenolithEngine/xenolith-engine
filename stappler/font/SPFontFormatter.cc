/**
 Copyright (c) 2024-2025 Stappler LLC <admin@stappler.dev>

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

#include "SPFontFormatter.h"
#include "SPFontFace.h"
#include "SPFontBidi.h"

namespace STAPPLER_VERSIONIZED stappler::font {

Formatter::Formatter() { }

Formatter::Formatter(FontCallback &&cb, TextLayoutData<mem_std::Interface> *d)
: fontCallback(sp::move(cb)) {
	reset(d);
}

Formatter::Formatter(FontCallback &&cb, TextLayoutData<memory::PoolInterface> *d)
: fontCallback(sp::move(cb)) {
	reset(d);
}

void Formatter::setFontCallback(FontCallback &&cb) { fontCallback = sp::move(cb); }

void Formatter::reset(TextLayoutData<mem_std::Interface> *d) {
	_output = Output(d);
	reset();
}

void Formatter::reset(TextLayoutData<memory::PoolInterface> *d) {
	_output = Output(d);
	reset();
}

void Formatter::reset() {
	b = 0;
	c = 0;

	defaultWidth = 0;
	width = 0;
	lineOffset = 0;
	lineX = 0;
	lineY = 0;

	maxLineX = 0;

	charNum = 0;
	lineHeight = 0;
	currentLineHeight = 0;
	rangeLineHeight = 0;

	lineHeightMod = 1.0f;
	lineHeightIsAbsolute = false;

	firstInLine = 0;
	wordWrapPos = 0;

	bufferedSpace = false;

	_paragraphDirection = TextDirection::Neutral;
	_pendingContinuations.clear();
}

void Formatter::finalize() {
	if (firstInLine < charNum) {
		pushLine(false);
	}

	if (!_output.chars.empty() && _output.chars.back().charID == char32_t(0x0A)) {
		pushLine(false);
	}

	auto chars = _output.chars.size();
	if (chars > 0 && _output.ranges.size() > 0 && _output.lines.size() > 0) {
		auto &lastRange = _output.ranges.back();
		auto &lastLine = _output.lines.back();

		if (lastLine.start + lastLine.count != chars) {
			lastLine.count = uint32_t(chars - lastLine.start);
		}

		if (lastRange.start + lastRange.count != chars) {
			lastRange.count = uint32_t(chars - lastRange.start);
		}
	}

	// #7: splice extra glyphs from 1->N decompositions into the char stream now that it is final
	expandGlyphContinuations();

	*_output.width = getWidth();
	*_output.height = getHeight();
	*_output.maxAdvance = getMaxLineX();
}

// Splice the 1->N decomposition glyphs gathered during layout into the char stream as ContinuationChar
// entries placed right after their source char, then re-index lines and ranges to remain consistent.
// Insertions reference indices in the append-only char stream that stay valid until finalize, so this
// runs exactly once, here.
void Formatter::expandGlyphContinuations() {
	if (_pendingContinuations.empty()) {
		return;
	}

	// stable insertion sort by source index (the number of 1->N glyphs per layout is tiny); equal
	// indices keep their visual (insertion) order so a multi-glyph cluster stays ordered
	for (size_t i = 1; i < _pendingContinuations.size(); ++i) {
		const PendingGlyph key = _pendingContinuations[i];
		size_t j = i;
		while (j > 0 && _pendingContinuations[j - 1].insertAfter > key.insertAfter) {
			_pendingContinuations[j] = _pendingContinuations[j - 1];
			--j;
		}
		_pendingContinuations[j] = key;
	}

	const size_t origChars = _output.chars.size();

	// insBefore[i] = number of continuations to be inserted before original index i
	Vector<uint32_t> insBefore(origChars + 1, 0);
	for (auto &p : _pendingContinuations) {
		if (size_t(p.insertAfter) + 1 <= origChars) {
			insBefore[p.insertAfter + 1] += 1;
		}
	}
	for (size_t i = 1; i <= origChars; ++i) { insBefore[i] += insBefore[i - 1]; }

	// shift each line/range by the insertions before its start, grow it by those within its span
	auto reindex = [&](auto &collection) {
		for (auto &it : collection) {
			const uint32_t s = it.start;
			const uint32_t e = sprt::min(s + it.count, uint32_t(origChars));
			it.start = s + insBefore[s];
			it.count = (e + insBefore[e]) - it.start;
		}
	};
	reindex(_output.lines);
	reindex(_output.ranges);

	// splice the glyphs in ascending order, tracking how many were already inserted
	uint32_t inserted = 0;
	for (auto &p : _pendingContinuations) {
		const size_t at = sprt::min(size_t(p.insertAfter) + 1 + inserted, _output.chars.size());
		_output.chars.insert(at, CharLayoutData(p.data));
		++inserted;
	}

	_pendingContinuations.clear();
}

void Formatter::setLinePositionCallback(const LinePositionCallback &func) {
	linePositionFunc = func;
}

void Formatter::setWidth(uint16_t w) {
	defaultWidth = w;
	width = w;
}

void Formatter::setTextAlignment(TextAlign align) { alignment = align; }

void Formatter::setTextDirection(TextDirection dir) { _defaultDirection = dir; }

void Formatter::setBidiEnabled(bool value) { _bidiEnabled = value; }

void Formatter::setShapingEnabled(bool value) { _shapingEnabled = value; }

TextDirection Formatter::getTextDirection() const { return _defaultDirection; }

bool Formatter::isBidiEnabled() const { return _bidiEnabled; }

bool Formatter::isShapingEnabled() const { return _shapingEnabled; }

// CSS `text-align: start | end` resolves against the line's base direction; the other keywords are
// already absolute. This keeps direction-relative alignment CSS-compatible.
TextAlign Formatter::resolveTextAlign(TextDirection lineDirection) const {
	const bool rtl = (lineDirection == TextDirection::RightToLeft);
	switch (alignment) {
	case TextAlign::Start: return rtl ? TextAlign::Right : TextAlign::Left;
	case TextAlign::End: return rtl ? TextAlign::Left : TextAlign::Right;
	default: return alignment;
	}
}

FontFaceObject *Formatter::faceById(uint16_t id) const {
	if (!_primaryFontSet) {
		return nullptr;
	}
	for (auto &f : _primaryFontSet->getFaces()) {
		if (f && f->getId() == id) {
			return f.get();
		}
	}
	return nullptr;
}

// Emit a zero-width Unicode bidi control into the char stream. It carries its real code point (so the
// resolver in layoutLine applies the embedding/isolate/override) but has no glyph and zero advance, so
// it renders nothing and takes no space. HarfBuzz drops it (default-ignorable) on the shaping path.
void Formatter::pushBidiControl(char32_t control) {
	if (control == 0) {
		return;
	}
	charNum++;
	_output.chars.emplace_back(CharLayoutData{control, lineX, 0, faceId});
}

int16_t Formatter::graphemeSpacing(char32_t cp) const {
	int32_t sp = _textStyle.letterSpacing;
	if (cp == char32_t(' ') || cp == char32_t(0x00A0)) {
		sp += _textStyle.wordSpacing;
	}
	return int16_t(sp);
}

// Lay out a finished line in VISUAL order. Resolves UAX #9 embedding levels, reorders the line's runs
// into visual order (rules L1-L2) and assigns each char its on-screen x. Two paths share the run walk:
//   * shaping on  -> each same-face sub-run is shaped with HarfBuzz (glyph indices + advances/offsets)
//   * shaping off -> chars are repositioned by the advances measured while reading (kerning is kept
//                    only on the fast single-LTR-run path; HarfBuzz handles kerning when shaping is on)
// Returns the line's visual right edge (absolute x) -- the true post-reorder, post-shape width used
// for alignment/justification, not the logical-last char's edge (#3).
uint16_t Formatter::layoutLine(uint16_t first, uint16_t len) {
	if (len == 0 || _output.lines.empty()) {
		return 0;
	}

	auto &line = _output.lines.back();
	const uint16_t lineEnd = uint16_t(first + len);

	// --- Bidi resolution: per-char levels, paragraph base direction, and visual run order ---
	Vector<BidiRun> runs;
	if (_bidiEnabled) {
		// Extract the line's code points (placeholder/filler chars become spaces -> neutral).
		Vector<char32_t> buf;
		buf.reserve(len);
		for (uint16_t i = first; i < lineEnd; ++i) {
			auto ch = _output.chars.at(i).charID;
			buf.emplace_back(ch == CharLayoutData::InvalidChar ? char32_t(0x20) : ch);
		}

		// Base level: explicit `direction` wins; for `auto` reuse the paragraph's already-resolved
		// base so wrapped continuation lines stay consistent (#2), else derive it from this line.
		const TextDirection base = (_defaultDirection == TextDirection::Neutral)
				? _paragraphDirection
				: _defaultDirection;

		// TextBidi allocates from the current pool; run it in a transient pool scope so the Formatter
		// does not depend on an ambient pool context.
		auto pool = memory::pool::create((memory::pool_t *)nullptr);
		memory::perform([&] {
			TextBidi bidi;
			if (!bidi.init(buf.data(), buf.size(), base)) {
				return;
			}
			// resolved embedding levels + the line's paragraph base direction
			bidi.foreachParagraph([&](uint32_t off, uint32_t length, uint8_t baseLevel,
										  SpanView<uint8_t> levels) {
				for (uint32_t i = 0; i < length && i < uint32_t(levels.size()) && (off + i) < len;
						++i) {
					_output.chars.at(uint16_t(first + off + i)).bidiLevel = levels[i];
				}
				if (off == 0) {
					const auto resolved = (baseLevel & 1) ? TextDirection::RightToLeft
														  : TextDirection::LeftToRight;
					line.direction = resolved;
					// remember the paragraph base so the next wrapped line resolves the same way (#2)
					if (_defaultDirection == TextDirection::Neutral
							&& _paragraphDirection == TextDirection::Neutral) {
						_paragraphDirection = resolved;
					}
				}
			});
			// visual run order (rules L1-L2): runs come back already ordered left-to-right
			bidi.foreachVisualRun(0, len, [&](const BidiRun &run) { runs.emplace_back(run); });
		}, pool);
		memory::pool::destroy(pool);
	}

	if (runs.empty()) {
		// bidi disabled (or the resolver failed): a single left-to-right run in logical order
		runs.emplace_back(BidiRun{0, len, 0});
	}

	// Fast path: with no shaping, a single LTR run and no extra spacing, visual order == logical order
	// and the positions computed while reading already stand (they carry kerning and optical
	// alignment) -- leave them. Letter/word-spacing forces the re-layout branch below so the gaps land.
	if (!_shapingEnabled && runs.size() == 1 && !runs.front().isRightToLeft()
			&& _textStyle.letterSpacing == 0 && _textStyle.wordSpacing == 0) {
		return getLineAdvancePos(uint16_t(first + len - 1));
	}

	// --- Visual placement: walk runs left-to-right from the line's left edge ---
	const int16_t lineLeft = _output.chars.at(first).pos;
	if (_shapingEnabled) {
		for (uint16_t i = first; i < lineEnd; ++i) {
			auto &cd = _output.chars.at(i);
			cd.gid = 0;
			cd.advance = 0;
			cd.yOffset = 0;
		}
	}

	int32_t x = lineLeft;
	for (auto &run : runs) {
		const uint16_t runFirst = uint16_t(first + run.offset);
		const uint16_t runEnd = uint16_t(runFirst + run.length);
		const bool rtl = run.isRightToLeft();

		if (_shapingEnabled) {
			x = shapeVisualRun(runFirst, uint16_t(run.length), rtl, x);
		} else if (!rtl) {
			for (uint16_t i = runFirst; i < runEnd; ++i) {
				auto &cd = _output.chars.at(i);
				if (const int16_t sp = graphemeSpacing(cd.charID)) {
					cd.advance =
							uint16_t(cd.advance + sp); // letter/word-spacing folds into the cell
				}
				cd.pos = int16_t(x);
				x += cd.advance;
			}
		} else {
			// RTL run with no shaping: visual order is reverse-logical. Mirror Bidi_Mirrored chars
			// (UAX #9 L4) by swapping in the mirror glyph while keeping the logical code point (so
			// copy/measure still see '('). The shaping path lets HarfBuzz mirror instead.
			for (uint16_t i = runEnd; i-- > runFirst;) {
				auto &cd = _output.chars.at(i);
				if (const char32_t mirror = TextBidi::mirrorCodepoint(cd.charID)) {
					if (auto *face = faceById(cd.face)) {
						if (const uint16_t gi = face->getGlyphIndex(mirror)) {
							cd.gid = gi;
						}
					}
				}
				if (const int16_t sp = graphemeSpacing(cd.charID)) {
					cd.advance = uint16_t(cd.advance + sp);
				}
				cd.pos = int16_t(x);
				x += cd.advance;
			}
		}
	}

	return uint16_t(x < lineLeft ? lineLeft : x);
}

// Shape one single-level bidi run and place it starting at x. The run is split into maximal same-face
// sub-runs; sub-runs lay out left-to-right, but in REVERSE logical order for an RTL run (its first
// logical sub-run is visually rightmost). HarfBuzz reverses glyphs within an RTL sub-run itself. A
// sub-run whose face is unknown, or that fails to shape, leaves its chars at gid 0 (renders nothing).
// Each glyph maps back to its source char by cluster; cluster members with no glyph (e.g. the tail of
// a ligature) keep gid 0 and collapse onto the ligature.
//
// Limitation (later refinement): one glyph per source cluster -- a decomposition that expands one code
// point into N glyphs keeps only the last.
int32_t Formatter::shapeVisualRun(uint16_t runFirst, uint16_t runLen, bool rtl, int32_t x) {
	if (runLen == 0 || !_primaryFontSet) {
		return x;
	}
	const uint16_t runEnd = uint16_t(runFirst + runLen);

	// maximal same-face sub-runs, in logical order
	struct SubRun {
		uint16_t first;
		uint16_t len;
	};
	Vector<SubRun> subs;
	for (uint16_t s = runFirst; s < runEnd;) {
		const uint16_t f = _output.chars.at(s).face;
		uint16_t e = s;
		while (e < runEnd && _output.chars.at(e).face == f) { ++e; }
		subs.emplace_back(SubRun{s, uint16_t(e - s)});
		s = e;
	}

	Vector<char32_t> buf;
	Vector<ShapedGlyph> glyphs;

	auto placeSub = [&](uint16_t subFirst, uint16_t subLen) {
		FontFaceObject *face = faceById(_output.chars.at(subFirst).face);
		if (!face) {
			return;
		}

		buf.clear();
		buf.reserve(subLen);
		for (uint16_t i = subFirst; i < uint16_t(subFirst + subLen); ++i) {
			auto ch = _output.chars.at(i).charID;
			buf.emplace_back(ch == CharLayoutData::InvalidChar ? char32_t(0x20) : ch);
		}

		glyphs.clear();
		if (!face->shape(buf.data(), buf.size(),
					rtl ? TextDirection::RightToLeft : TextDirection::LeftToRight, glyphs,
					_textStyle.enableLigatures)) {
			return;
		}
		// Place one glyph per source char. A cluster that shapes to several glyphs (a 1->N
		// decomposition) keeps its first glyph on the base char and routes the rest to continuation
		// entries spliced in at finalize (#7). HarfBuzz emits a cluster's glyphs contiguously.
		// Letter/word-spacing (#9) is added once per grapheme, after the whole cluster.
		uint32_t prevCluster = 0xFFFF'FFFFu;
		uint16_t prevBaseIdx = 0;
		auto closeGrapheme = [&]() {
			if (prevCluster == 0xFFFF'FFFFu) {
				return;
			}
			auto &pcd = _output.chars.at(prevBaseIdx);
			if (const int16_t sp = graphemeSpacing(pcd.charID)) {
				pcd.advance = uint16_t(pcd.advance + sp);
				x += sp;
			}
		};
		for (auto &g : glyphs) {
			if (g.cluster >= uint32_t(subLen)) {
				continue;
			}
			const uint16_t baseIdx = uint16_t(subFirst + g.cluster);
			if (g.cluster != prevCluster) {
				closeGrapheme(); // letter/word-spacing gap before the next grapheme
				auto &cd = _output.chars.at(baseIdx);
				cd.gid = uint16_t(g.glyphId);
				cd.pos = int16_t(x + g.xOffset);
				cd.advance = uint16_t(g.xAdvance < 0 ? 0 : g.xAdvance);
				cd.yOffset = g.yOffset;
				prevCluster = g.cluster;
				prevBaseIdx = baseIdx;
			} else {
				// extra glyph of the same source char -> continuation (virtual ContinuationChar)
				auto &base = _output.chars.at(baseIdx);
				base.flags |= CharLayoutData::FlagHasContinuation;
				CharLayoutData cont{};
				cont.charID = CharLayoutData::ContinuationChar;
				cont.flags = CharLayoutData::FlagGlyphContinuation;
				cont.face = base.face;
				cont.bidiLevel = base.bidiLevel;
				cont.gid = uint16_t(g.glyphId);
				cont.pos = int16_t(x + g.xOffset);
				cont.advance = uint16_t(g.xAdvance < 0 ? 0 : g.xAdvance);
				cont.yOffset = g.yOffset;
				_pendingContinuations.emplace_back(PendingGlyph{baseIdx, cont});
			}
			x += g.xAdvance;
		}
		closeGrapheme(); // trailing spacing for the last grapheme of the sub-run
	};

	if (!rtl) {
		for (auto &s : subs) { placeSub(s.first, s.len); }
	} else {
		for (size_t i = subs.size(); i-- > 0;) { placeSub(subs[i].first, subs[i].len); }
	}

	return x;
}

void Formatter::setLineHeightAbsolute(uint16_t val) {
	lineHeight = val;
	currentLineHeight = val;
	lineHeightIsAbsolute = true;
	parseFontLineHeight(rangeLineHeight);
}

void Formatter::setLineHeightRelative(float val) {
	lineHeightMod = val;
	lineHeightIsAbsolute = false;
	parseFontLineHeight(rangeLineHeight);
}

void Formatter::setMaxWidth(uint16_t value) { maxWidth = value; }
void Formatter::setMaxLines(size_t value) { maxLines = value; }
void Formatter::setOpticalAlignment(bool value) { opticalAlignment = value; }
void Formatter::setEmplaceAllChars(bool value) { emplaceAllChars = value; }
void Formatter::setFillerChar(char32_t value) { _fillerChar = value; }
void Formatter::setHyphens(HyphenMap *map) { _hyphens = map; }
void Formatter::setRequest(ContentRequest req) { request = req; }

void Formatter::begin(uint16_t ind, uint16_t blockMargin) {
	lineX = ind;

	firstInLine = charNum;
	wordWrapPos = charNum;

	bufferedSpace = false;
	c = 0;
	b = 0;

	if (lineY != 0) {
		lineY += blockMargin;
	}
}

void Formatter::parseWhiteSpace(WhiteSpace whiteSpacePolicy) {
	switch (whiteSpacePolicy) {
	case WhiteSpace::Normal:
		preserveLineBreaks = false;
		collapseSpaces = true;
		wordWrap = true;
		break;
	case WhiteSpace::Nowrap:
		preserveLineBreaks = false;
		collapseSpaces = true;
		wordWrap = false;
		break;
	case WhiteSpace::Pre:
		preserveLineBreaks = true;
		collapseSpaces = false;
		wordWrap = false;
		break;
	case WhiteSpace::PreLine:
		preserveLineBreaks = true;
		collapseSpaces = true;
		wordWrap = true;
		break;
	case WhiteSpace::PreWrap:
		preserveLineBreaks = true;
		collapseSpaces = false;
		wordWrap = true;
		break;
	default:
		preserveLineBreaks = false;
		collapseSpaces = true;
		wordWrap = true;
		break;
	};
}

void Formatter::parseFontLineHeight(uint16_t h) {
	if (!lineHeightIsAbsolute) {
		if (lineHeight == 0) {
			lineHeight = h;
		}
		float fontLineHeight = static_cast<uint16_t>(h * lineHeightMod);
		if (fontLineHeight > currentLineHeight) {
			currentLineHeight = fontLineHeight;
		}
	}
}

bool Formatter::updatePosition(uint16_t &linePos, uint16_t &height) {
	if (linePositionFunc) {
		auto pos = linePositionFunc(linePos, height, _primaryFontSet->getSpec().density);
		lineOffset = pos.offset;
		width = sprt::min(pos.width, defaultWidth);

		uint16_t maxHeight = lineHeight * 16;
		uint16_t extraHeight = 0;

		// skip lines if not enough space
		while (width < _primaryFontSet->getFontHeight() && extraHeight < maxHeight) {
			extraHeight += lineHeight;
			linePos += lineHeight;
			pos = linePositionFunc(linePos, height, _primaryFontSet->getSpec().density);
			lineOffset = pos.offset;
			width = sprt::min(pos.width, defaultWidth);
		}

		if (extraHeight >= maxHeight) {
			return false;
		}
	}
	return true;
}

uint16_t Formatter::getAdvance(const CharLayoutData &ch) const { return ch.advance; }

uint16_t Formatter::getAdvance(uint16_t pos) const {
	if (pos < _output.chars.size()) {
		return getAdvance(_output.chars.at(pos));
	} else {
		return 0;
	}
}

inline uint16_t Formatter::getAdvancePosition(const CharLayoutData &ch) const {
	return ch.pos + ch.advance;
}

inline uint16_t Formatter::getAdvancePosition(uint16_t pos) const {
	return (pos < _output.chars.size()) ? getAdvancePosition(_output.chars.at(pos)) : uint16_t(0);
}

inline uint16_t Formatter::getOriginPosition(const CharLayoutData &ch) const { return ch.pos; }

inline uint16_t Formatter::getOriginPosition(uint16_t pos) const {
	return (pos < _output.chars.size()) ? getOriginPosition(_output.chars.at(pos)) : uint16_t(0);
}

bool Formatter::isSpecial(char32_t ch) const {
	// collapseSpaces can be disabled for manual optical alignment
	if (!opticalAlignment || !collapseSpaces) {
		return false;
	}
	return sprt::chars::CharGroup<char32_t, CharGroupId::OpticalAlignmentSpecial>::match(ch);
}

uint16_t Formatter::checkBullet(uint16_t first, uint16_t len) const {
	// collapseSpaces can be disabled for manual optical alignment
	if (!opticalAlignment || !collapseSpaces) {
		return 0;
	}

	uint16_t offset = 0;
	for (uint16_t i = first; i < first + len - 1; i++) {
		auto ch = _output.chars.at(i).charID;
		if (sprt::chars::CharGroup<char32_t, CharGroupId::OpticalAlignmentBullet>::match(ch)) {
			offset++;
		} else if (sprt::chars::isspace(ch) && offset >= 1) {
			return offset + 1;
		} else {
			break;
		}
	}

	return 0;
}

void Formatter::pushLineFiller(bool replaceLastChar) {
	*_output.overflow = true;
	if (_fillerChar == 0) {
		return;
	}

	auto charDef = _primaryFontSet->getChar(_fillerChar, faceId);
	if (!charDef) {
		return;
	}

	if (replaceLastChar && !_output.chars.empty()) {
		auto &bc = _output.chars.back();
		bc.charID = _fillerChar;
		bc.advance = charDef.xAdvance;
		bc.gid = charDef.glyphIndex;
	} else {
		_output.chars.emplace_back(
				CharLayoutData{_fillerChar, lineX, charDef.xAdvance, faceId, charDef.glyphIndex});
		charNum++;
	}
}

bool Formatter::pushChar(char32_t ch) {
	if (_textStyle.textTransform == TextTransform::Uppercase) {
		ch = sprt::unicode::toupper(ch);
	} else if (_textStyle.textTransform == TextTransform::Lowercase) {
		ch = sprt::unicode::tolower(ch);
	}

	CharShape charDef = _primaryFontSet->getChar(ch, faceId);

	if (charDef.charID == 0) {
		if (ch == char32_t(0x00AD)) {
			charDef = _primaryFontSet->getChar('-', faceId);
		} else {
			log::format(sprt::oslog::Warn, "RichTextFormatter", SP_LOCATION,
					"%s: Attempted to use undefined character: %d '%s'",
					_primaryFontSet->getName().data(), ch, string::toUtf8<Interface>(ch).c_str());
			return true;
		}
	}

	if (charNum == firstInLine && lineOffset > 0) {
		lineX += lineOffset;
	}

	auto posX = lineX;

	CharLayoutData spec{charDef.charID, posX, charDef.xAdvance, faceId, charDef.glyphIndex};

	if (ch == static_cast<char32_t>(0x00AD)) {
		if (_textStyle.hyphens == Hyphens::Manual || _textStyle.hyphens == Hyphens::Auto) {
			wordWrapPos = charNum + 1;
		}
	} else if (ch == u'-' || ch == u'+' || ch == u'*' || ch == u'/' || ch == u'\\') {
		auto pos = charNum;
		while (pos > firstInLine && (!sprt::chars::isspace(_output.chars.at(pos - 1).charID))) {
			pos--;
		}
		if (charNum - pos > 2) {
			wordWrapPos = charNum + 1;
		}
		auto newlineX = lineX + charDef.xAdvance;
		if (maxWidth && lineX > maxWidth) {
			pushLineFiller();
			return false;
		}
		lineX = newlineX;
	} else if (charDef) {
		if (charNum == firstInLine && isSpecial(ch)) {
			spec.pos -= charDef.xAdvance / 2;
			lineX += charDef.xAdvance / 2;
		} else {
			auto newlineX = lineX + charDef.xAdvance;
			if (maxWidth && lineX > maxWidth) {
				pushLineFiller(true);
				return false;
			}
			lineX = newlineX;
		}
	}
	charNum++;
	_output.chars.emplace_back(sp::move(spec));

	return true;
}

bool Formatter::pushSpace(bool wrap) {
	if (pushChar(' ')) {
		if (wordWrap && wrap) {
			wordWrapPos = charNum;
		}
		return true;
	}
	return false;
}

bool Formatter::pushTab() {
	CharShape charDef = _primaryFontSet->getChar(' ', faceId);

	auto posX = lineX;
	if (charDef.xAdvance > 0) {
		auto tabPos = (lineX + charDef.xAdvance) / (charDef.xAdvance * 4) + 1;
		lineX = tabPos * charDef.xAdvance * 4;
	}

	charNum++;
	_output.chars.emplace_back(
			CharLayoutData{char32_t('\t'), posX, uint16_t(lineX - posX), faceId});
	if (wordWrap) {
		wordWrapPos = charNum;
	}

	return true;
}

uint16_t Formatter::getLineAdvancePos(uint16_t lastPos) {
	auto &origChar = _output.chars.at(lastPos);
	auto ch = origChar.charID;
	if (ch == ' ' && lastPos > firstInLine) {
		lastPos--;
	}
	if (lastPos < firstInLine) {
		return 0;
	}

	auto a = getAdvancePosition(lastPos);
	auto &lastChar = _output.chars.at(lastPos);
	ch = lastChar.charID;
	if (isSpecial(ch)) {
		if (ch == '.' || ch == ',') {
			a -= min(a, lastChar.advance);
		} else {
			a -= min(a, uint16_t(lastChar.advance / 2));
		}
	}
	return a;
}

bool Formatter::pushLine(uint16_t first, uint16_t len, bool forceAlign) {
	if (maxLines && _output.lines.size() + 1 == maxLines && forceAlign) {
		pushLineFiller(true);
		return false;
	}

	uint16_t linePos = lineY + currentLineHeight;

	if (len > 0) {
		_output.lines.emplace_back(LineLayoutData{first, len, linePos, currentLineHeight});

		// Resolve the line's base direction (CSS `direction`) and, when bidi is enabled, the UAX #9
		// embedding levels of its characters. `start`/`end` alignment is then resolved against the
		// line's base direction, CSS-style.
		_output.lines.back().direction = (_defaultDirection == TextDirection::Neutral)
				? TextDirection::LeftToRight
				: _defaultDirection;

		// Resolve bidi levels, reorder the line into visual order and (when enabled) shape it; the
		// return value is the line's true visual width, consumed by the alignment/justify below (#3).
		uint16_t advance = layoutLine(first, len);
		const TextAlign align = resolveTextAlign(_output.lines.back().direction);
		uint16_t offsetLeft =
				(advance < (width + lineOffset)) ? ((width + lineOffset) - advance) : 0;
		if (offsetLeft > 0 && align == TextAlign::Right) {
			for (uint16_t i = first; i < first + len; i++) {
				_output.chars.at(i).pos += offsetLeft;
			}
		} else if (offsetLeft > 0 && align == TextAlign::Center) {
			offsetLeft /= 2;
			for (uint16_t i = first; i < first + len; i++) {
				_output.chars.at(i).pos += offsetLeft;
			}
		} else if ((offsetLeft > 0 || (advance > width + lineOffset)) && align == TextAlign::Justify
				&& forceAlign && len > 0) {
			int16_t joffset =
					(advance > width + lineOffset) ? (width + lineOffset - advance) : offsetLeft;
			uint16_t spacesCount = 0;
			if (first == 0) {
				auto bc = checkBullet(first, len);
				first += bc;
				len -= bc;
			}

			for (uint16_t i = first; i < first + len - 1; i++) {
				auto ch = _output.chars.at(i).charID;
				if (sprt::chars::isspace(ch) && ch != '\n') {
					spacesCount++;
				}
			}

			int16_t offset = 0;
			for (uint16_t i = first; i < first + len; i++) {
				auto ch = _output.chars.at(i).charID;
				if (ch != CharLayoutData::InvalidChar && sprt::chars::isspace(ch) && ch != '\n'
						&& spacesCount > 0) {
					offset += joffset / spacesCount;
					joffset -= joffset / spacesCount;
					spacesCount--;
				} else {
					_output.chars.at(i).pos += offset;
				}
			}
		}

		if (advance > maxLineX) {
			maxLineX = advance;
		}
	}

	lineY = linePos;
	firstInLine = charNum;
	wordWrapPos = firstInLine;
	bufferedSpace = false;
	currentLineHeight = min(rangeLineHeight, lineHeight);
	parseFontLineHeight(rangeLineHeight);
	width = defaultWidth;
	if (defaultWidth >= _primaryFontSet->getFontHeight()) {
		if (!updatePosition(lineY, currentLineHeight)) {
			return false;
		}
	}
	b = 0;
	return true;
}

bool Formatter::pushLine(bool forceAlign) {
	uint16_t first = firstInLine;
	if (firstInLine <= charNum) {
		uint16_t len = charNum - firstInLine;
		return pushLine(first, len, forceAlign);
	}
	return true;
}

void Formatter::updateLineHeight(uint16_t first, uint16_t last) {
	if (!lineHeightIsAbsolute) {
		bool found = false;
		for (RangeLayoutData &it : _output.ranges) {
			if (it.start <= first && it.start + it.count > first) {
				found = true;
			} else if (it.start > last) {
				break;
			}
			if (found) {
				parseFontLineHeight(it.height);
			}
		}
	}
}

Formatter::Output::Output(TextLayoutData<mem_std::Interface> *d)
: width(&d->width)
, height(&d->height)
, maxAdvance(&d->maxAdvance)
, overflow(&d->overflow)
, ranges(d->ranges)
, chars(d->chars)
, lines(d->lines) { }

Formatter::Output::Output(TextLayoutData<memory::PoolInterface> *d)
: width(&d->width)
, height(&d->height)
, maxAdvance(&d->maxAdvance)
, overflow(&d->overflow)
, ranges(d->ranges)
, chars(d->chars)
, lines(d->lines) { }

bool Formatter::pushLineBreak() {
	if (sprt::chars::CharGroup<char32_t, CharGroupId::WhiteSpace>::match(
				_output.chars.back().charID)) {
		return true;
	}

	if (firstInLine + 1 >= wordWrapPos && (maxLines != 0 && _output.lines.size() + 1 != maxLines)) {
		return true;
	}

	uint16_t wordStart = wordWrapPos;
	uint16_t wordEnd = charNum - 1;

	if (request == ContentRequest::Normal
			&& (lineX - getOriginPosition(wordWrapPos) > width || wordWrapPos == 0)) {
		if (wordWrap) {
			lineX = lineOffset;
			if (!pushLine(firstInLine, wordEnd - firstInLine, true)) {
				return false;
			}

			firstInLine = wordEnd;
			wordWrapPos = wordEnd;

			auto &ch = _output.chars.at(wordEnd);

			ch.pos = lineX;
			lineX += ch.advance;

			updateLineHeight(wordEnd, charNum);
		}
	} else {
		// we can wrap the word
		auto &ch = _output.chars.at((wordWrapPos - 1));
		if (!sprt::chars::isspace(ch.charID)) {
			if (!pushLine(firstInLine, (wordWrapPos)-firstInLine, true)) {
				return false;
			}
		} else {
			if (!pushLine(firstInLine,
						(wordWrapPos > firstInLine) ? ((wordWrapPos)-firstInLine) : 0, true)) {
				return false;
			}
		}
		firstInLine = wordStart;
		wordWrapPos = wordStart;

		if (wordStart < _output.chars.size()) {
			uint16_t originOffset = getOriginPosition(wordStart);
			auto &bc = _output.chars.at((wordStart));
			if (isSpecial(bc.charID)) {
				originOffset += bc.advance / 2;
			}

			if (originOffset > lineOffset) {
				originOffset -= lineOffset;
			}

			for (uint32_t i = wordStart; i <= wordEnd; i++) {
				_output.chars.at(i).pos -= originOffset;
			}
			lineX -= originOffset;
		} else {
			lineX = 0;
		}
	}
	return true;
}

bool Formatter::pushLineBreakChar() {
	charNum++;
	_output.chars.emplace_back(CharLayoutData{char32_t(0x0A), lineX, 0, 0});

	if (!pushLine(false)) {
		return false;
	}
	lineX = 0;
	_paragraphDirection = TextDirection::Neutral; // a hard break ends the paragraph

	return true;
}

bool Formatter::readChars(WideStringView &r, const Vector<uint8_t> &hyph) {
	size_t wordPos = 0;
	auto hIt = hyph.begin();
	bool startWhitespace = _output.chars.empty();

	auto tmpStr = r.data();
	auto tmpLen = r.size();

	while (tmpLen > 0) {
		uint8_t offset;
		auto c = sprt::unicode::utf16Decode32(tmpStr, tmpLen, offset);

		if (offset <= tmpLen) {
			tmpStr += offset;
			tmpLen -= offset;
		} else {
			break;
		}

		if (hIt != hyph.end() && wordPos == *hIt) {
			pushChar(char32_t(0x00AD));
			++hIt;
		}

		if (c == char32_t('\n')) {
			if (preserveLineBreaks) {
				if (!pushLineBreakChar()) {
					return false;
				}
			} else if (collapseSpaces) {
				if (!startWhitespace) {
					bufferedSpace = true;
				}
			}
			b = 0;
			continue;
		}

		if (c == char32_t('\t') && !collapseSpaces) {
			if (request == ContentRequest::Minimize) {
				wordWrapPos = charNum;
				if (!pushLineBreak()) {
					return false;
				}
			} else if (!pushTab()) {
				return false;
			}
			continue;
		}

		if (c < char32_t(0x20)) {
			if (emplaceAllChars) {
				charNum++;
				_output.chars.emplace_back(
						CharLayoutData{CharLayoutData::InvalidChar, lineX, 0, 0});
			}
			continue;
		}

		if (c != char32_t(0x00A0) && sprt::chars::isspace(c) && collapseSpaces) {
			if (!startWhitespace) {
				bufferedSpace = true;
			}
			b = 0;
			continue;
		}

		if (c == char32_t(0x00A0)) {
			if (!pushSpace(false)) {
				return false;
			}
			bufferedSpace = false;
			continue;
		}

		if (bufferedSpace
				|| (!collapseSpaces && c != char32_t(0x00A0) && sprt::chars::isspace(c))) {
			if (request == ContentRequest::Minimize && charNum > 0) {
				wordWrapPos = charNum;
				auto b = bufferedSpace;
				if (!pushLineBreak()) {
					return false;
				}
				bufferedSpace = b;
			} else if (!pushSpace()) {
				return false;
			}
			if (!bufferedSpace) {
				continue;
			} else {
				bufferedSpace = false;
			}
		}

		auto kerning = _primaryFontSet->getKerningAmount(b, c, faceId);
		if (kerning != 0) {
			lineX += kerning;
		}
		if (!pushChar(c)) {
			return false;
		}
		startWhitespace = false;

		switch (request) {
		case ContentRequest::Minimize:
			if (charNum > 0 && wordWrapPos == charNum && c != char32_t(0x00AD)) {
				if (!pushLineBreak()) {
					return false;
				}
			}
			break;
		case ContentRequest::Maximize: break;
		case ContentRequest::Normal:
			if (width + lineOffset > 0 && lineX > width + lineOffset) {
				if (kerning != 0) {
					lineX -= kerning;
				}
				if (!pushLineBreak()) {
					return false;
				}
			}
			break;
		}

		if (c != char32_t(0x00AD)) {
			b = c;
		}

		++wordPos;
	}
	return true;
}

bool Formatter::read(const FontParameters &f, const TextParameters &s, WideStringView str,
		uint16_t frontOffset, uint16_t backOffset) {
	return read(f, s, str.data(), str.size(), frontOffset, backOffset);
}

bool Formatter::read(const FontParameters &f, const TextParameters &s, const char16_t *str,
		size_t len, uint16_t frontOffset, uint16_t backOffset) {
	if (!str) {
		return false;
	}

	_primaryFontSet = nullptr;

	Rc<FontFaceSet> primaryLayout;
	Rc<FontFaceSet> secondaryLayout;

	if (f.fontVariant == FontVariant::SmallCaps) {
		//secondary = _output.source->getLayout(f.getSmallCaps());

		CharVector primaryStr;
		CharVector secondaryStr;

		auto tmpStr = str;
		auto tmpLen = len;

		while (tmpLen > 0) {
			uint8_t offset;
			auto ch = sprt::unicode::utf16Decode32(tmpStr, tmpLen, offset);

			if (s.textTransform == TextTransform::Uppercase) {
				ch = sprt::unicode::toupper(ch);
			} else if (s.textTransform == TextTransform::Lowercase) {
				ch = sprt::unicode::tolower(ch);
			}
			if (ch != sprt::unicode::toupper(ch)) {
				secondaryStr.addChar(sprt::unicode::toupper(ch));
			} else {
				primaryStr.addChar(ch);
			}

			if (offset <= tmpLen) {
				tmpLen -= offset;
				tmpStr += offset;
			} else {
				break;
			}
		}

		if (_fillerChar) {
			primaryStr.addChar(_fillerChar);
		}
		primaryStr.addChar('-');
		primaryStr.addChar(' ');
		primaryStr.addChar(char32_t(0xAD));

		primaryLayout = fontCallback(f);
		if (primaryLayout) {
			primaryLayout->addString(primaryStr);
		}

		secondaryLayout = fontCallback(f.getSmallCaps());
		if (secondaryLayout) {
			secondaryLayout->addString(secondaryStr);
		}

		if (!secondaryLayout) {
			return false;
		}
	} else {
		CharVector primaryStr;
		if (s.textTransform == TextTransform::None) {
			primaryStr.addString(WideStringView(str, len));
		} else {
			auto tmpStr = str;
			auto tmpLen = len;

			while (tmpLen > 0) {
				uint8_t offset;
				auto ch = sprt::unicode::utf16Decode32(tmpStr, tmpLen, offset);

				if (s.textTransform == TextTransform::Uppercase) {
					ch = sprt::unicode::toupper(ch);
				} else if (s.textTransform == TextTransform::Lowercase) {
					ch = sprt::unicode::tolower(ch);
				}
				primaryStr.addChar(ch);

				if (offset <= tmpLen) {
					tmpLen -= offset;
					tmpStr += offset;
				} else {
					break;
				}
			}
		}
		if (_fillerChar) {
			primaryStr.addChar(_fillerChar);
		}
		primaryStr.addChar('-');
		primaryStr.addChar(' ');
		primaryStr.addChar(char32_t(0xAD));

		primaryLayout = fontCallback(f);
		if (primaryLayout) {
			primaryLayout->addString(primaryStr);
		}
	}

	if (!primaryLayout) {
		return false;
	}

	auto h = primaryLayout->getFontHeight();

	if (f.fontVariant == FontVariant::SmallCaps && s.textTransform != TextTransform::Uppercase) {
		size_t blockStart = 0;
		size_t blockSize = 0;
		bool caps = false;
		TextParameters capsParams = s;
		capsParams.textTransform = TextTransform::Uppercase;

		auto tmpStr = str;
		auto tmpLen = len;

		while (tmpLen > 0) {
			uint8_t offset;
			auto ch = sprt::unicode::utf16Decode32(tmpStr, tmpLen, offset);
			auto c = (s.textTransform == TextTransform::None) ? ch : sprt::unicode::tolower(ch);
			if (sprt::unicode::toupper(c) != c) { // char can be uppercased - use caps
				if (caps != true) {
					caps = true;
					if (blockSize > 0) {
						readWithRange(RangeLayoutData{false, false, s.textDecoration,
										  s.verticalAlign, uint32_t(_output.chars.size()), 0,
										  Color4B(s.color, s.opacity), h,
										  primaryLayout->getMetrics(), primaryLayout},
								s, str + blockStart, blockSize, frontOffset, backOffset);
					}
					blockStart = tmpStr - str;
					blockSize = 0;
				}
			} else {
				if (caps != false) {
					caps = false;
					if (blockSize > 0) {
						readWithRange(RangeLayoutData{false, false, s.textDecoration,
										  s.verticalAlign, uint32_t(_output.chars.size()), 0,
										  Color4B(s.color, s.opacity), h,
										  secondaryLayout->getMetrics(), secondaryLayout},
								capsParams, str + blockStart, blockSize, frontOffset, backOffset);
					}
					blockStart = tmpStr - str;
					blockSize = 0;
				}
			}
			blockSize += offset;
		}
		if (blockSize > 0) {
			if (caps) {
				return readWithRange(RangeLayoutData{false, false, s.textDecoration,
										 s.verticalAlign, uint32_t(_output.chars.size()), 0,
										 Color4B(s.color, s.opacity), h,
										 secondaryLayout->getMetrics(), secondaryLayout},
						capsParams, str + blockStart, blockSize, frontOffset, backOffset);
			} else {
				return readWithRange(RangeLayoutData{false, false, s.textDecoration,
										 s.verticalAlign, uint32_t(_output.chars.size()), 0,
										 Color4B(s.color, s.opacity), h,
										 primaryLayout->getMetrics(), primaryLayout},
						s, str + blockStart, blockSize, frontOffset, backOffset);
			}
		}
	} else {
		return readWithRange(RangeLayoutData{false, false, s.textDecoration, s.verticalAlign,
								 uint32_t(_output.chars.size()), 0, Color4B(s.color, s.opacity), h,
								 primaryLayout->getMetrics(), primaryLayout},
				s, str, len, frontOffset, backOffset);
	}

	return true;
}

bool Formatter::read(const FontParameters &f, const TextParameters &s, uint16_t blockWidth,
		uint16_t blockHeight) {
	_primaryFontSet = nullptr;

	Rc<FontFaceSet> primaryLayout = fontCallback(f);
	if (!primaryLayout) {
		return false;
	}
	return readWithRange(RangeLayoutData{false, false, s.textDecoration, s.verticalAlign,
							 uint32_t(_output.chars.size()), 0, Color4B(s.color, s.opacity),
							 blockHeight, primaryLayout->getMetrics(), primaryLayout},
			s, blockWidth, blockHeight);
}

bool Formatter::readWithRange(RangeLayoutData &&range, const TextParameters &s, const char16_t *str,
		size_t len, uint16_t frontOffset, uint16_t backOffset) {
	_primaryFontSet = range.layout;
	rangeLineHeight = range.height;

	if (bufferedSpace) {
		pushSpace();
		bufferedSpace = false;
	}

	parseFontLineHeight(rangeLineHeight);

	_textStyle = s;
	parseWhiteSpace(_textStyle.whiteSpace);
	if (!updatePosition(lineY, currentLineHeight)) {
		return false;
	}

	if (!_output.chars.empty() && _output.chars.back().charID == ' ' && collapseSpaces) {
		while (len > 0 && ((sprt::chars::isspace(str[0]) && str[0] != 0x00A0) || str[0] < 0x20)) {
			len--;
			str++;
		}
	}

	b = 0;

	lineX += frontOffset;

	// #6: realise CSS `unicode-bidi` by bracketing the span with the Unicode bidi controls the
	// resolver understands. No-op unless bidi is enabled and the mode opens an embedding/isolate.
	char32_t bidiClose = 0, bidiClose2 = 0;
	if (_bidiEnabled && _textStyle.bidi != BidiMode::Normal) {
		const bool rtl = (_textStyle.direction == TextDirection::RightToLeft);
		const bool neutral = (_textStyle.direction == TextDirection::Neutral);
		char32_t open = 0, open2 = 0;
		switch (_textStyle.bidi) {
		case BidiMode::Embed: // LRE / RLE ... PDF
			open = rtl ? 0x202B : 0x202A;
			bidiClose = 0x202C;
			break;
		case BidiMode::BidiOverride: // LRO / RLO ... PDF
			open = rtl ? 0x202E : 0x202D;
			bidiClose = 0x202C;
			break;
		case BidiMode::Isolate: // LRI / RLI / FSI ... PDI
			open = neutral ? 0x2068 : (rtl ? 0x2067 : 0x2066);
			bidiClose = 0x2069;
			break;
		case BidiMode::IsolateOverride: // isolate + override ... PDF PDI
			open = neutral ? 0x2068 : (rtl ? 0x2067 : 0x2066);
			open2 = rtl ? 0x202E : 0x202D;
			bidiClose = 0x202C;
			bidiClose2 = 0x2069;
			break;
		case BidiMode::Plaintext: // FSI ... PDI (base level resolved per isolated run)
			open = 0x2068;
			bidiClose = 0x2069;
			break;
		case BidiMode::Normal: break;
		}
		pushBidiControl(open);
		pushBidiControl(open2);
	}

	WideStringView r(str, len);
	if (_textStyle.hyphens == Hyphens::Auto && _hyphens) {
		while (!r.empty()) {
			WideStringView tmp = r.readUntil<WideStringView::CharGroup<CharGroupId::Latin>,
					WideStringView::CharGroup<CharGroupId::Cyrillic>>();
			if (!tmp.empty()) {
				readChars(tmp);
			}
			tmp = r.readChars<WideStringView::CharGroup<CharGroupId::Latin>,
					WideStringView::CharGroup<CharGroupId::Cyrillic>>();
			if (!tmp.empty()) {
				readChars(tmp, _hyphens->makeWordHyphens(tmp));
			}
		}
	} else {
		readChars(r);
	}

	// close the unicode-bidi embedding/isolate opened above (reverse order)
	pushBidiControl(bidiClose);
	pushBidiControl(bidiClose2);

	range.count = uint32_t(_output.chars.size() - range.start);
	if (range.count > 0) {
		_output.ranges.emplace_back(sp::move(range));
	}
	lineX += backOffset;

	return true;
}
bool Formatter::readWithRange(RangeLayoutData &&range, const TextParameters &s, uint16_t blockWidth,
		uint16_t blockHeight) {
	_primaryFontSet = range.layout;
	rangeLineHeight = range.height;

	if (bufferedSpace) {
		pushSpace();
		bufferedSpace = false;
	}


	_textStyle = s;
	parseWhiteSpace(_textStyle.whiteSpace);

	if (maxWidth && lineX + blockWidth > maxWidth) {
		pushLineFiller(false);
		return false;
	}

	if (width + lineOffset > 0) {
		if (lineX + blockWidth > width + lineOffset) {
			if (!pushLine(true)) {
				return false;
			}
			lineX = 0;
		}
	}

	parseFontLineHeight(rangeLineHeight);
	if (currentLineHeight < blockHeight) {
		currentLineHeight = blockHeight;
	}

	if (!updatePosition(lineY, currentLineHeight)) {
		return false;
	}

	if (charNum == firstInLine && lineOffset > 0) {
		lineX += lineOffset;
	}

	CharLayoutData spec{CharLayoutData::InvalidChar, lineX, blockWidth, 0};
	lineX += spec.advance;
	charNum++;
	_output.chars.emplace_back(sp::move(spec));

	switch (request) {
	case ContentRequest::Minimize:
		wordWrapPos = charNum - 1;
		if (!pushLineBreak()) {
			return false;
		}
		break;
	case ContentRequest::Maximize: break;
	case ContentRequest::Normal:
		if (width + lineOffset > 0 && lineX > width + lineOffset) {
			if (!pushLineBreak()) {
				return false;
			}
		}
		break;
	}


	range.count = uint32_t(_output.chars.size() - range.start);
	_output.ranges.emplace_back(sp::move(range));

	return true;
}

uint16_t Formatter::getHeight() const { return lineY; }
uint16_t Formatter::getWidth() const { return sprt::max(maxLineX, width); }
uint16_t Formatter::getMaxLineX() const { return maxLineX; }
uint16_t Formatter::getLineHeight() const { return lineHeight; }

} // namespace stappler::font
