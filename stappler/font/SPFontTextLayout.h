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

#ifndef CORE_FONT_SPFONTLAYOUTDATA_H_
#define CORE_FONT_SPFONTLAYOUTDATA_H_

#include "SPFontStyle.h"
#include "SPPadding.h"

namespace STAPPLER_VERSIONIZED stappler::font {

class FontFaceSet;

enum class CharSelectMode {
	Center,
	Prefix,
	Suffix,
	Best,
};

struct SP_PUBLIC CharLayoutData final {
	static constexpr char32_t InvalidChar = char32_t(0xFFFF'FFFF);

	char32_t charID = 0;
	int16_t pos = 0;
	uint16_t advance = 0;
	uint16_t face = 0;
	uint16_t padding = 0;
};

struct SP_PUBLIC LineLayoutData final {
	uint32_t start = 0;
	uint32_t count = 0;
	uint16_t pos = 0;
	uint16_t height = 0;
};

struct SP_PUBLIC RangeLayoutData final {
	bool colorDirty = false;
	bool opacityDirty = false;
	TextDecoration decoration = TextDecoration::None;
	VerticalAlign align = VerticalAlign::Baseline;

	uint32_t start = 0;
	uint32_t count = 0;

	geom::Color4B color;
	uint16_t height = 0;

	Metrics metrics;
	FontFaceSet *layout = nullptr;
};

struct SP_PUBLIC RangeLineIterator {
	const RangeLayoutData *range;
	const LineLayoutData *line;

	uint32_t start() const { return std::max(range->start, line->start); }
	uint32_t count() const {
		return std::min(range->start + range->count, line->start + line->count) - start();
	}
	uint32_t end() const {
		return std::min(range->start + range->count, line->start + line->count);
	}

	RangeLineIterator &operator++() {
		const auto rangeEnd = range->start + range->count;
		const auto lineEnd = line->start + line->count;
		if (rangeEnd < lineEnd) {
			++range;
		} else if (rangeEnd > lineEnd) {
			++line;
		} else {
			++range;
			++line;
		}

		return *this;
	}

	bool operator==(const RangeLineIterator &other) const {
		return other.line == line && other.range == range;
	}
	bool operator!=(const RangeLineIterator &other) const { return !(*this == other); }
};

template <typename Interface>
struct SP_PUBLIC TextLayoutData : public Interface::AllocBaseType {
	template <typename Value>
	using Vector = typename Interface::template VectorType<Value>;

	Vector< RangeLayoutData > ranges;
	Vector< CharLayoutData > chars;
	Vector< LineLayoutData > lines;

	uint16_t width = 0;
	uint16_t height = 0;
	uint16_t maxAdvance = 0;
	bool overflow = false;

	void reserve(size_t nchars, size_t nranges = 0);

	RangeLineIterator begin() const { return RangeLineIterator{&*ranges.begin(), &*lines.begin()}; }

	RangeLineIterator end() const {
		// Pass-the-ed pointer acquisition
		// Ugly trick, but all others is forbidden in msvc debug mode
		return RangeLineIterator{&ranges.at(ranges.size() - 1) + 1,
			&lines.at(lines.size() - 1) + 1};
	}

	void clear() {
		ranges.clear();
		chars.clear();
		lines.clear();
		width = 0;
		height = 0;
		maxAdvance = 0;
		overflow = false;
	}

	void str(const Callback<void(char32_t)> &, bool filterAlign = true) const;
	void str(const Callback<void(char32_t)> &, uint32_t, uint32_t,
			size_t maxWords = maxOf<size_t>(), bool ellipsis = true, bool filterAlign = true) const;

	float getTextIndent(float density) const { return chars.front().pos / density; }

	// on error maxOf<uint32_t> returned
	Pair<uint32_t, CharSelectMode> getChar(int32_t x, int32_t y,
			CharSelectMode = CharSelectMode::Center) const;
	const LineLayoutData *getLine(uint32_t charIndex) const;
	uint32_t getLineForChar(uint32_t charIndex) const;

	float getLinePosition(uint32_t firstCharId, uint32_t lastCharId, float density) const;

	Pair<uint32_t, uint32_t> selectWord(uint32_t originChar) const;

	geom::Rect getLineRect(uint32_t lineId, float density, const geom::Vec2 & = geom::Vec2()) const;
	geom::Rect getLineRect(const LineLayoutData &, float density,
			const geom::Vec2 & = geom::Vec2()) const;

	void getLabelRects(const Callback<void(geom::Rect)> &, uint32_t first, uint32_t last,
			float density, const geom::Vec2 & = geom::Vec2(),
			const geom::Padding &p = geom::Padding()) const;
};

} // namespace stappler::font

#endif /* CORE_FONT_SPFONTLAYOUTDATA_H_ */
