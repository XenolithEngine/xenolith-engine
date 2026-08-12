/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#ifndef CORE_FONT_SPFONTFORMATTER_H_
#define CORE_FONT_SPFONTFORMATTER_H_

#include "SPFontTextLayout.h"
#include "SPFontHyphenMap.h"

namespace STAPPLER_VERSIONIZED stappler::font {

class FontFaceObject;

class SP_PUBLIC Formatter : public InterfaceObject<mem_std::Interface> {
public:
	struct LinePosition {
		uint16_t offset;
		uint16_t width;
	};

	using LinePositionCallback = Function<LinePosition(uint16_t &, uint16_t &, float density)>;
	using FontCallback = Function<Rc<FontFaceSet>(const FontParameters &f)>;

	enum class ContentRequest {
		Normal,
		Minimize,
		Maximize,
	};

	Formatter();
	Formatter(FontCallback &&, TextLayoutData<mem_std::Interface> *);
	Formatter(FontCallback &&, TextLayoutData<memory::PoolInterface> *);

	void setFontCallback(FontCallback &&);

	void reset(TextLayoutData<mem_std::Interface> *);
	void reset(TextLayoutData<memory::PoolInterface> *);
	void reset();
	void finalize();

	void setLinePositionCallback(const LinePositionCallback &);
	void setWidth(uint16_t w);
	void setTextAlignment(TextAlign align);
	void setTextDirection(TextDirection); // base/paragraph direction (CSS `direction`)
	void setBidiEnabled(bool); // run the Unicode Bidirectional Algorithm (UAX #9) during layout
	void setShapingEnabled(bool); // shape glyphs with HarfBuzz during layout
	void setLineHeightAbsolute(uint16_t);
	void setLineHeightRelative(float);

	void setMaxWidth(uint16_t);
	void setMaxLines(size_t);
	void setOpticalAlignment(bool value);
	void setEmplaceAllChars(bool value);
	void setFillerChar(char32_t);
	void setHyphens(HyphenMap *);
	void setRequest(ContentRequest);

	void begin(uint16_t indent, uint16_t blockMargin = 0);
	bool read(const FontParameters &f, const TextParameters &s, WideStringView str,
			uint16_t front = 0, uint16_t back = 0);
	bool read(const FontParameters &f, const TextParameters &s, const char16_t *str, size_t len,
			uint16_t front = 0, uint16_t back = 0);
	bool read(const FontParameters &f, const TextParameters &s, uint16_t w, uint16_t h);

	uint16_t getHeight() const;
	uint16_t getWidth() const;
	uint16_t getMaxLineX() const;
	uint16_t getLineHeight() const;
	TextDirection getTextDirection() const;
	bool isBidiEnabled() const;
	bool isShapingEnabled() const;

protected:
	bool isSpecial(char32_t) const;
	uint16_t checkBullet(uint16_t first, uint16_t len) const;

	void parseWhiteSpace(WhiteSpace whiteSpacePolicy);
	void parseFontLineHeight(uint16_t);
	bool updatePosition(uint16_t &linePos, uint16_t &lineHeight);

	uint16_t getAdvance(const CharLayoutData &c) const;
	uint16_t getAdvance(uint16_t pos) const;

	inline uint16_t getAdvancePosition(const CharLayoutData &) const;
	inline uint16_t getAdvancePosition(uint16_t pos) const;

	uint16_t getLineAdvancePos(uint16_t lastPos);

	inline uint16_t getOriginPosition(const CharLayoutData &) const;
	inline uint16_t getOriginPosition(uint16_t pos) const;

	bool readWithRange(RangeLayoutData &&, const TextParameters &s, const char16_t *str, size_t len,
			uint16_t front = 0, uint16_t back = 0);
	bool readWithRange(RangeLayoutData &&, const TextParameters &s, uint16_t w, uint16_t h);

	bool readChars(WideStringView &r, const Vector<uint8_t> & = Vector<uint8_t>());
	void pushLineFiller(bool replaceLastChar = false);
	bool pushChar(char32_t);
	bool pushSpace(bool wrap = true);
	bool pushTab();
	bool pushLine(uint16_t first, uint16_t len, bool forceAlign);
	bool pushLine(bool forceAlign);
	bool pushLineBreak();
	bool pushLineBreakChar();

	// CSS-relative text alignment: resolves `start`/`end` against a line's base direction.
	TextAlign resolveTextAlign(TextDirection lineDirection) const;

	// Lay out a finished line in VISUAL order: resolve UAX #9 embedding levels, reorder its runs
	// (rules L1-L2) and assign each char its on-screen x. When shaping is enabled each same-face run
	// is shaped with HarfBuzz (glyph indices + advances/offsets); otherwise chars are repositioned by
	// the advances measured while reading. Returns the line's visual right edge (used for alignment).
	uint16_t layoutLine(uint16_t first, uint16_t len);

	// Shape and place one single-level bidi run for layoutLine: splits it into maximal same-face
	// sub-runs and lays them left-to-right, reversing sub-run order for an RTL run. Returns advanced x.
	int32_t shapeVisualRun(uint16_t runFirst, uint16_t runLen, bool rtl, int32_t x);

	// Loaded face object with the given id within the primary font set, or nullptr.
	FontFaceObject *faceById(uint16_t id) const;

	// Emit a zero-width Unicode bidi control (LRE/RLE/LRO/RLO/LRI/RLI/FSI/PDF/PDI) into the char
	// stream so the resolver applies it; the control renders nothing. Realises CSS `unicode-bidi`.
	void pushBidiControl(char32_t control);

	// Splice the extra glyphs gathered from 1->N shaping decompositions into _output.chars after their
	// source char (as ContinuationChar entries) and re-index lines/ranges to stay consistent.
	void expandGlyphContinuations();

	// Extra advance to add after a grapheme: CSS letter-spacing, plus word-spacing for a space. #9
	int16_t graphemeSpacing(char32_t cp) const;

	void updateLineHeight(uint16_t first, uint16_t last);

	// The uint16_t layout domain is full (charNum, lineX or lineY cannot grow further): raise the
	// output overflow flag, log once per layout, and answer false so the caller stops reading.
	bool reportOverflow(const char *reason);

	struct Output {
		uint16_t *width = nullptr;
		uint16_t *height = nullptr;
		uint16_t *maxAdvance = nullptr;
		bool *overflow = nullptr;

		VectorAdapter<RangeLayoutData> ranges;
		VectorAdapter<CharLayoutData> chars;
		VectorAdapter<LineLayoutData> lines;

		Output() = default;
		Output(TextLayoutData<mem_std::Interface> *);
		Output(TextLayoutData<memory::PoolInterface> *);

		Output &operator=(Output &&) = default;
	};

	Rc<HyphenMap> _hyphens;
	Rc<FontFaceSet> _primaryFontSet;

	Output _output;

	TextParameters _textStyle;

	bool preserveLineBreaks = false;
	bool collapseSpaces = true;
	bool wordWrap = false;
	bool opticalAlignment = true;
	bool emplaceAllChars = false;

	uint16_t faceId = 0;
	char32_t b = 0;
	char32_t c = 0;

	uint16_t defaultWidth = 0;
	uint16_t width = 0;
	uint16_t lineOffset = 0;
	int16_t lineX = 0;
	uint16_t lineY = 0;

	uint16_t maxLineX = 0;

	uint16_t charNum = 0;
	uint16_t lineHeight = 0;
	uint16_t currentLineHeight = 0;
	uint16_t rangeLineHeight = 0;

	float lineHeightMod = 1.0f;
	bool lineHeightIsAbsolute = false;

	uint16_t firstInLine = 0;
	uint16_t wordWrapPos = 0;

	bool bufferedSpace = false;
	bool _overflowReported = false; // one error line per layout, not one per character

	uint16_t maxWidth = 0;
	size_t maxLines = 0;

	char32_t _fillerChar = 0;
	TextAlign alignment = TextAlign::Left;
	TextDirection _defaultDirection = TextDirection::LeftToRight;
	// Resolved base direction of the paragraph being laid out; propagates the first line's base level
	// to wrapped continuation lines under `auto` direction. Reset on each hard break and on reset().
	TextDirection _paragraphDirection = TextDirection::Neutral;
	bool _bidiEnabled = false;
	bool _shapingEnabled = false;

	// Extra shaped glyphs from 1->N decompositions, spliced into _output.chars at finalize().
	struct PendingGlyph {
		uint32_t insertAfter; // source char index this glyph follows
		CharLayoutData data;
	};
	Vector<PendingGlyph> _pendingContinuations;

	ContentRequest request = ContentRequest::Normal;

	FontCallback fontCallback;
	LinePositionCallback linePositionFunc = nullptr;
};

} // namespace stappler::font

#endif /* CORE_FONT_SPFONTFORMATTER_H_ */
