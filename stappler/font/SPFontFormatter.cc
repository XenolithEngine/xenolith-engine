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

namespace STAPPLER_VERSIONIZED stappler::font {

Formatter::Formatter() { }

Formatter::Formatter(FontCallback &&cb, TextLayoutData<memory::StandartInterface> *d)
: fontCallback(sp::move(cb)) {
	reset(d);
}

Formatter::Formatter(FontCallback &&cb, TextLayoutData<memory::PoolInterface> *d)
: fontCallback(sp::move(cb)) {
	reset(d);
}

void Formatter::setFontCallback(FontCallback &&cb) { fontCallback = sp::move(cb); }

void Formatter::reset(TextLayoutData<memory::StandartInterface> *d) {
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

	*_output.width = getWidth();
	*_output.height = getHeight();
	*_output.maxAdvance = getMaxLineX();
}

void Formatter::setLinePositionCallback(const LinePositionCallback &func) {
	linePositionFunc = func;
}

void Formatter::setWidth(uint16_t w) {
	defaultWidth = w;
	width = w;
}

void Formatter::setTextAlignment(TextAlign align) { alignment = align; }

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
		width = std::min(pos.width, defaultWidth);

		uint16_t maxHeight = lineHeight * 16;
		uint16_t extraHeight = 0;

		// skip lines if not enough space
		while (width < _primaryFontSet->getFontHeight() && extraHeight < maxHeight) {
			extraHeight += lineHeight;
			linePos += lineHeight;
			pos = linePositionFunc(linePos, height, _primaryFontSet->getSpec().density);
			lineOffset = pos.offset;
			width = std::min(pos.width, defaultWidth);
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
	} else {
		_output.chars.emplace_back(CharLayoutData{_fillerChar, lineX, charDef.xAdvance, faceId});
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
			log::format(log::Warn, "RichTextFormatter", SP_LOCATION,
					"%s: Attempted to use undefined character: %d '%s'",
					_primaryFontSet->getName().data(), ch, string::toUtf8<Interface>(ch).c_str());
			return true;
		}
	}

	if (charNum == firstInLine && lineOffset > 0) {
		lineX += lineOffset;
	}

	auto posX = lineX;

	CharLayoutData spec{charDef.charID, posX, charDef.xAdvance, faceId};

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
	auto tabPos = (lineX + charDef.xAdvance) / (charDef.xAdvance * 4) + 1;
	lineX = tabPos * charDef.xAdvance * 4;

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
		uint16_t advance = getLineAdvancePos(first + len - 1);
		uint16_t offsetLeft =
				(advance < (width + lineOffset)) ? ((width + lineOffset) - advance) : 0;
		if (offsetLeft > 0 && alignment == TextAlign::Right) {
			for (uint16_t i = first; i < first + len; i++) {
				_output.chars.at(i).pos += offsetLeft;
			}
		} else if (offsetLeft > 0 && alignment == TextAlign::Center) {
			offsetLeft /= 2;
			for (uint16_t i = first; i < first + len; i++) {
				_output.chars.at(i).pos += offsetLeft;
			}
		} else if ((offsetLeft > 0 || (advance > width + lineOffset))
				&& alignment == TextAlign::Justify && forceAlign && len > 0) {
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

Formatter::Output::Output(TextLayoutData<memory::StandartInterface> *d)
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

	if (firstInLine >= wordWrapPos - 1 && (maxLines != 0 && _output.lines.size() + 1 != maxLines)) {
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
										  geom::Color4B(s.color, s.opacity), h,
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
										  geom::Color4B(s.color, s.opacity), h,
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
										 geom::Color4B(s.color, s.opacity), h,
										 secondaryLayout->getMetrics(), secondaryLayout},
						capsParams, str + blockStart, blockSize, frontOffset, backOffset);
			} else {
				return readWithRange(RangeLayoutData{false, false, s.textDecoration,
										 s.verticalAlign, uint32_t(_output.chars.size()), 0,
										 geom::Color4B(s.color, s.opacity), h,
										 primaryLayout->getMetrics(), primaryLayout},
						s, str + blockStart, blockSize, frontOffset, backOffset);
			}
		}
	} else {
		return readWithRange(RangeLayoutData{false, false, s.textDecoration, s.verticalAlign,
								 uint32_t(_output.chars.size()), 0,
								 geom::Color4B(s.color, s.opacity), h, primaryLayout->getMetrics(),
								 primaryLayout},
				s, str, len, frontOffset, backOffset);
	}

	return true;
}

bool Formatter::read(const FontParameters &f, const TextParameters &s, uint16_t blockWidth,
		uint16_t blockHeight) {
	_primaryFontSet = nullptr;

	Rc<FontFaceSet> primaryLayout = fontCallback(f);
	return readWithRange(RangeLayoutData{false, false, s.textDecoration, s.verticalAlign,
							 uint32_t(_output.chars.size()), 0, geom::Color4B(s.color, s.opacity),
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
uint16_t Formatter::getWidth() const { return std::max(maxLineX, width); }
uint16_t Formatter::getMaxLineX() const { return maxLineX; }
uint16_t Formatter::getLineHeight() const { return lineHeight; }

} // namespace stappler::font
