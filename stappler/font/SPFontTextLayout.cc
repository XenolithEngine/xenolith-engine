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

#include "SPFontTextLayout.h"

namespace STAPPLER_VERSIONIZED stappler::font {

inline static bool isSpaceOrLineBreak(char32_t c) {
	return c == char32_t(0x0A) || sprt::chars::isspace(c);
}

template <typename Interface>
static geom::Rect getLabelLineStartRect(const TextLayoutData<Interface> &f, uint16_t lineId,
		float density, uint32_t c) {
	geom::Rect rect;
	const LineLayoutData &line = f.lines.at(lineId);
	if (line.count > 0) {
		const CharLayoutData &firstChar = f.chars.at(std::max(line.start, c));
		const CharLayoutData &lastChar = f.chars.at(line.start + line.count - 1);
		rect.origin =
				geom::Vec2((firstChar.pos) / density, (line.pos) / density - line.height / density);
		rect.size = geom::Size2((lastChar.pos + lastChar.advance - firstChar.pos) / density,
				line.height / density);
	}

	return rect;
}

template <typename Interface>
static geom::Rect getLabelLineEndRect(const TextLayoutData<Interface> &f, uint16_t lineId,
		float density, uint32_t c) {
	geom::Rect rect;
	const LineLayoutData &line = f.lines.at(lineId);
	if (line.count > 0) {
		const CharLayoutData &firstChar = f.chars.at(line.start);
		const CharLayoutData &lastChar = f.chars.at(std::min(line.start + line.count - 1, c));
		rect.origin =
				geom::Vec2((firstChar.pos) / density, (line.pos) / density - line.height / density);
		rect.size = geom::Size2((lastChar.pos + lastChar.advance - firstChar.pos) / density,
				line.height / density);
	}
	return rect;
}

template <typename Interface>
static geom::Rect getCharsRect(const TextLayoutData<Interface> &f, uint32_t lineId,
		uint32_t firstCharId, uint32_t lastCharId, float density) {
	geom::Rect rect;
	const LineLayoutData &line = f.lines.at(lineId);
	const CharLayoutData &firstChar = f.chars.at(firstCharId);
	const CharLayoutData &lastChar = f.chars.at(lastCharId);
	rect.origin =
			geom::Vec2((firstChar.pos) / density, (line.pos) / density - line.height / density);
	rect.size = geom::Size2((lastChar.pos + lastChar.advance - firstChar.pos) / density,
			line.height / density);
	return rect;
}

template <typename Interface>
static void TextLayoutData_str(const TextLayoutData<Interface> &f,
		const Callback<void(char32_t)> &cb, bool filter) {
	for (auto it = f.begin(); it != f.end(); ++it) {
		const RangeLayoutData &range = *it.range;
		if (!filter || range.align == VerticalAlign::Baseline) {
			size_t end = it.start() + it.count() - 1;
			for (size_t i = it.start(); i <= end; ++i) {
				const auto &spec = f.chars[i];
				if (spec.charID != char32_t(0xAD) && spec.charID != CharLayoutData::InvalidChar) {
					cb(spec.charID);
				}
			}
		}
	}
}

template <typename Interface>
static void TextLayoutData_str(const TextLayoutData<Interface> &f,
		const Callback<void(char32_t)> &cb, uint32_t s_start, uint32_t s_end, size_t maxWords,
		bool ellipsis, bool filter) {
	for (auto it = f.begin(); it != f.end(); ++it) {
		const RangeLayoutData &range = *it.range;
		if (!filter || range.align == VerticalAlign::Baseline) {
			size_t end = it.start() + it.count() - 1;
			for (size_t i = it.start(); i <= end; ++i) {
				const auto &spec = f.chars[i];
				if (spec.charID != char32_t(0xAD) && spec.charID != CharLayoutData::InvalidChar) {
					cb(spec.charID);
				}
			}
		}
	}
}

template <typename Interface>
static Pair<uint32_t, CharSelectMode> TextLayoutData_getChar(const TextLayoutData<Interface> &f,
		int32_t x, int32_t y, CharSelectMode mode) {
	int32_t yDistance = maxOf<int32_t>();
	const LineLayoutData *pLine = nullptr;
	if (!f.lines.empty()) {
		for (auto &l : f.lines) {
			int32_t dst = maxOf<int32_t>();
			switch (mode) {
			case CharSelectMode::Center: dst = abs(y - (l.pos - l.height / 2)); break;
			case CharSelectMode::Best: dst = abs(y - (l.pos - l.height * 3 / 4)); break;
			case CharSelectMode::Prefix:
			case CharSelectMode::Suffix: dst = abs(y - (l.pos - l.height)); break;
			};
			if (dst < yDistance) {
				pLine = &l;
				yDistance = dst;
			} else {
				break;
			}
		}

		if (f.chars.back().charID == char32_t(0x0A) && pLine == &f.lines.back()
				&& (mode == CharSelectMode::Best || mode == CharSelectMode::Suffix)) {
			int32_t dst = maxOf<int32_t>();
			switch (mode) {
			case CharSelectMode::Center: dst = abs(y - (f.height - pLine->height / 2)); break;
			case CharSelectMode::Best: dst = abs(y - (f.height - pLine->height * 3 / 4)); break;
			case CharSelectMode::Prefix:
			case CharSelectMode::Suffix: dst = abs(y - (f.height - pLine->height)); break;
			};
			if (dst < yDistance) {
				return pair(f.chars.size() - 1, CharSelectMode::Suffix);
			}
		}
	}

	if (!pLine) {
		return pair(maxOf<uint32_t>(), mode);
	}

	if (yDistance > pLine->height * 3 / 2 && mode != CharSelectMode::Best) {
		return pair(maxOf<uint32_t>(), mode);
	}

	CharSelectMode nextMode = mode;
	int32_t xDistance = maxOf<int32_t>();
	const CharLayoutData *pChar = nullptr;
	uint32_t charNumber = pLine->start;
	for (uint32_t i = pLine->start; i < pLine->start + pLine->count; ++i) {
		auto &c = f.chars[i];
		if (c.charID != char32_t(0xAD) && !isSpaceOrLineBreak(c.charID)) {
			int32_t dst = maxOf<int32_t>();
			CharSelectMode dstMode = mode;
			switch (mode) {
			case CharSelectMode::Center: dst = abs(x - (c.pos + c.advance / 2)); break;
			case CharSelectMode::Prefix: dst = abs(x - c.pos); break;
			case CharSelectMode::Suffix: dst = abs(x - (c.pos + c.advance)); break;
			case CharSelectMode::Best: {
				int32_t prefixDst = abs(x - c.pos);
				int32_t suffixDst = abs(x - (c.pos + c.advance));
				if (prefixDst <= suffixDst) {
					dst = prefixDst;
					dstMode = CharSelectMode::Prefix;
				} else {
					dst = suffixDst;
					dstMode = CharSelectMode::Suffix;
				}
			} break;
			};
			if (dst < xDistance) {
				pChar = &c;
				xDistance = dst;
				charNumber = i;
				nextMode = dstMode;
			} else {
				break;
			}
		}
	}
	if (pLine->count && f.chars[pLine->start + pLine->count - 1].charID == char32_t(0x0A)) {
		auto &c = f.chars[pLine->start + pLine->count - 1];
		int32_t dst = maxOf<int32_t>();
		switch (mode) {
		case CharSelectMode::Prefix: dst = abs(x - c.pos); break;
		case CharSelectMode::Best: dst = abs(x - c.pos); break;
		default: break;
		};
		if (dst < xDistance) {
			pChar = &c;
			xDistance = dst;
			charNumber = pLine->start + pLine->count - 1;
			nextMode = CharSelectMode::Prefix;
		}
	}

	if ((mode == CharSelectMode::Best || mode == CharSelectMode::Suffix)
			&& pLine == &(f.lines.back())) {
		auto c = f.chars.back();
		int32_t dst = abs(x - (c.pos + c.advance));
		if (dst < xDistance) {
			pChar = &c;
			xDistance = dst;
			charNumber = uint32_t(f.chars.size() - 1);
			nextMode = CharSelectMode::Suffix;
		}
	}

	if (!pChar) {
		return pair(maxOf<uint32_t>(), mode);
	}

	return pair(charNumber, nextMode);
}

template <typename Interface>
const LineLayoutData *TextLayoutData_getLine(const TextLayoutData<Interface> &f, uint32_t idx) {
	const LineLayoutData *ret = nullptr;
	for (const LineLayoutData &it : f.lines) {
		if (it.start <= idx && it.start + it.count > idx) {
			ret = &it;
			break;
		}
	}
	return ret;
}

template <typename Interface>
uint32_t TextLayoutData_getLineNumber(const TextLayoutData<Interface> &f, uint32_t id) {
	uint16_t n = 0;
	for (auto &it : f.lines) {
		if (id >= it.start && id < it.start + it.count) {
			return n;
		}
		n++;
	}
	if (n >= f.lines.size()) {
		n = f.lines.size() - 1;
	}
	return n;
}

template <typename Interface>
float TextLayoutData_getLinePosition(const TextLayoutData<Interface> &f, uint32_t firstCharId,
		uint32_t lastCharId, float density) {
	auto firstLine = TextLayoutData_getLine(f, firstCharId);
	auto lastLine = TextLayoutData_getLine(f, lastCharId);

	return ((firstLine->pos) / density + (lastLine->pos) / density) / 2.0f;
}

template <typename Interface>
Pair<uint32_t, uint32_t> TextLayoutData_selectWord(const TextLayoutData<Interface> &f,
		uint32_t origin) {
	Pair<uint32_t, uint32_t> ret(origin, origin);
	while (ret.second + 1 < f.chars.size() && !isSpaceOrLineBreak(f.chars[ret.second + 1].charID)) {
		++ret.second;
	}
	while (ret.first > 0 && !isSpaceOrLineBreak(f.chars[ret.first - 1].charID)) { --ret.first; }
	return Pair<uint32_t, uint32_t>(ret.first, ret.second + 1 - ret.first);
}

template <typename Interface>
geom::Rect TextLayoutData_getLineRect(const TextLayoutData<Interface> &f, uint32_t lineId,
		float density, const geom::Vec2 &origin) {
	if (lineId >= f.lines.size()) {
		return geom::Rect::ZERO;
	}
	return TextLayoutData_getLineRect(f, f.lines[lineId], density, origin);
}

template <typename Interface>
geom::Rect TextLayoutData_getLineRect(const TextLayoutData<Interface> &f,
		const LineLayoutData &line, float density, const geom::Vec2 &origin) {
	geom::Rect rect;
	if (line.count > 0) {
		const CharLayoutData &firstChar = f.chars.at(line.start);
		const CharLayoutData &lastChar = f.chars.at(line.start + line.count - 1);
		rect.origin = geom::Vec2((firstChar.pos) / density + origin.x,
				(line.pos) / density - line.height / density + origin.y);
		rect.size = geom::Size2((lastChar.pos + lastChar.advance - firstChar.pos) / density,
				line.height / density);
	}
	return rect;
}

template <typename Interface>
void TextLayoutData_getLabelRects(const TextLayoutData<Interface> &f,
		const Callback<void(geom::Rect)> &cb, uint32_t firstCharId, uint32_t lastCharId,
		float density, const geom::Vec2 &origin, const geom::Padding &p) {
	auto firstLine = TextLayoutData_getLineNumber(f, firstCharId);
	auto lastLine = TextLayoutData_getLineNumber(f, lastCharId);

	if (firstLine == lastLine) {
		auto rect = getCharsRect(f, firstLine, firstCharId, lastCharId, density);
		rect.origin.x += origin.x - p.left;
		rect.origin.y += origin.y - p.top;
		rect.size.width += p.left + p.right;
		rect.size.height += p.bottom + p.top;
		if (!rect.equals(geom::Rect::ZERO)) {
			cb(rect);
		}
	} else {
		auto first = getLabelLineStartRect(f, firstLine, density, firstCharId);
		if (!first.equals(geom::Rect::ZERO)) {
			first.origin.x += origin.x;
			first.origin.y += origin.y;
			if (first.origin.x - p.left < 0.0f) {
				first.size.width += (first.origin.x);
				first.origin.x = 0.0f;
			} else {
				first.origin.x -= p.left;
				first.size.width += p.left;
			}
			first.origin.y -= p.top;
			first.size.height += p.bottom + p.top;
			cb(first);
		}

		for (auto i = firstLine + 1; i < lastLine; i++) {
			auto rect = f.getLineRect(i, density);
			rect.origin.x += origin.x;
			rect.origin.y += origin.y - p.top;
			rect.size.height += p.bottom + p.top;
			if (!rect.equals(geom::Rect::ZERO)) {
				cb(rect);
			}
		}

		auto last = getLabelLineEndRect(f, lastLine, density, lastCharId);
		if (!last.equals(geom::Rect::ZERO)) {
			last.origin.x += origin.x;
			last.origin.y += origin.y - p.top;
			last.size.width += p.right;
			last.size.height += p.bottom + p.top;
			cb(last);
		}
	}
}

template <>
void TextLayoutData<memory::StandartInterface>::reserve(size_t nchars, size_t nranges) {
	if (nchars) {
		chars.reserve(nchars);
		lines.reserve(nchars / 60);
	}
	if (nranges) {
		ranges.reserve(nranges);
	}
}

template <>
void TextLayoutData<memory::PoolInterface>::reserve(size_t nchars, size_t nranges) {
	if (nchars) {
		chars.reserve(nchars);
		lines.reserve(nchars / 60);
	}
	if (nranges) {
		ranges.reserve(nranges);
	}
}

template <>
void TextLayoutData<memory::StandartInterface>::str(const Callback<void(char32_t)> &cb,
		bool filter) const {
	TextLayoutData_str(*this, cb, filter);
}

template <>
void TextLayoutData<memory::PoolInterface>::str(const Callback<void(char32_t)> &cb,
		bool filter) const {
	TextLayoutData_str(*this, cb, filter);
}

template <>
void TextLayoutData<memory::StandartInterface>::str(const Callback<void(char32_t)> &cb,
		uint32_t s_start, uint32_t s_end, size_t maxWords, bool ellipsis, bool filter) const {
	TextLayoutData_str(*this, cb, s_start, s_end, maxWords, ellipsis, filter);
}

template <>
void TextLayoutData<memory::PoolInterface>::str(const Callback<void(char32_t)> &cb,
		uint32_t s_start, uint32_t s_end, size_t maxWords, bool ellipsis, bool filter) const {
	TextLayoutData_str(*this, cb, s_start, s_end, maxWords, ellipsis, filter);
}

template <>
Pair<uint32_t, CharSelectMode> TextLayoutData<memory::StandartInterface>::getChar(int32_t x,
		int32_t y, CharSelectMode mode) const {
	return TextLayoutData_getChar(*this, x, y, mode);
}

template <>
Pair<uint32_t, CharSelectMode> TextLayoutData<memory::PoolInterface>::getChar(int32_t x, int32_t y,
		CharSelectMode mode) const {
	return TextLayoutData_getChar(*this, x, y, mode);
}

template <>
const LineLayoutData *TextLayoutData<memory::StandartInterface>::getLine(uint32_t charIndex) const {
	return TextLayoutData_getLine(*this, charIndex);
}

template <>
const LineLayoutData *TextLayoutData<memory::PoolInterface>::getLine(uint32_t charIndex) const {
	return TextLayoutData_getLine(*this, charIndex);
}

template <>
uint32_t TextLayoutData<memory::StandartInterface>::getLineForChar(uint32_t charIndex) const {
	return TextLayoutData_getLineNumber(*this, charIndex);
}

template <>
uint32_t TextLayoutData<memory::PoolInterface>::getLineForChar(uint32_t charIndex) const {
	return TextLayoutData_getLineNumber(*this, charIndex);
}

template <>
float TextLayoutData<memory::StandartInterface>::getLinePosition(uint32_t firstCharId,
		uint32_t lastCharId, float density) const {
	return TextLayoutData_getLinePosition(*this, firstCharId, lastCharId, density);
}

template <>
float TextLayoutData<memory::PoolInterface>::getLinePosition(uint32_t firstCharId,
		uint32_t lastCharId, float density) const {
	return TextLayoutData_getLinePosition(*this, firstCharId, lastCharId, density);
}

template <>
Pair<uint32_t, uint32_t> TextLayoutData<memory::StandartInterface>::selectWord(
		uint32_t originChar) const {
	return TextLayoutData_selectWord(*this, originChar);
}

template <>
Pair<uint32_t, uint32_t> TextLayoutData<memory::PoolInterface>::selectWord(
		uint32_t originChar) const {
	return TextLayoutData_selectWord(*this, originChar);
}

template <>
geom::Rect TextLayoutData<memory::StandartInterface>::getLineRect(uint32_t lineId, float density,
		const geom::Vec2 &origin) const {
	return TextLayoutData_getLineRect(*this, lineId, density, origin);
}

template <>
geom::Rect TextLayoutData<memory::PoolInterface>::getLineRect(uint32_t lineId, float density,
		const geom::Vec2 &origin) const {
	return TextLayoutData_getLineRect(*this, lineId, density, origin);
}

template <>
geom::Rect TextLayoutData<memory::StandartInterface>::getLineRect(const LineLayoutData &line,
		float density, const geom::Vec2 &origin) const {
	return TextLayoutData_getLineRect(*this, line, density, origin);
}

template <>
geom::Rect TextLayoutData<memory::PoolInterface>::getLineRect(const LineLayoutData &line,
		float density, const geom::Vec2 &origin) const {
	return TextLayoutData_getLineRect(*this, line, density, origin);
}

template <>
void TextLayoutData<memory::StandartInterface>::getLabelRects(const Callback<void(geom::Rect)> &cb,
		uint32_t first, uint32_t last, float density, const geom::Vec2 &origin,
		const geom::Padding &p) const {
	return TextLayoutData_getLabelRects(*this, cb, first, last, density, origin, p);
}

template <>
void TextLayoutData<memory::PoolInterface>::getLabelRects(const Callback<void(geom::Rect)> &cb,
		uint32_t first, uint32_t last, float density, const geom::Vec2 &origin,
		const geom::Padding &p) const {
	return TextLayoutData_getLabelRects(*this, cb, first, last, density, origin, p);
}

} // namespace stappler::font
