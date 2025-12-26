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

#ifndef CORE_FONT_SPFONT_H_
#define CORE_FONT_SPFONT_H_

#include "SPMemory.h"
#include "SPGeometry.h"
#include "SPVec2.h"
#include "SPColor.h"

namespace STAPPLER_VERSIONIZED stappler::font {


namespace config {

constexpr bool FontPreloadGroups = false;
constexpr size_t UnicodePlanes = 16;

} // namespace config


struct SP_PUBLIC CharVector final {
	void addChar(char32_t);
	void addString(const StringView &);
	void addString(const WideStringView &);
	void addString(const CharVector &);

	bool empty() const { return !chars.empty(); }

	mem_std::Vector<char32_t> chars;
};

using FontLayoutId = ValueWrapper<uint16_t, class FontLayoutIdTag>;

enum CharAnchor : uint32_t {
	BottomLeft,
	TopLeft,
	TopRight,
	BottomRight
};

struct SP_PUBLIC Metrics final {
	uint16_t size = 0; // font size in pixels
	uint16_t height = 0; // default font line height
	int16_t ascender =
			0; // The distance from the baseline to the highest coordinate used to place an outline point
	int16_t descender =
			0; // The distance from the baseline to the lowest grid coordinate used to place an outline point
	int16_t underlinePosition = 0;
	int16_t underlineThickness = 0;
};

struct SP_PUBLIC CharId final {
	static constexpr uint32_t CharMask = 0x0000'FFFFU;
	static constexpr uint32_t AnchorMask = 0x0003'0000U;
	static constexpr uint32_t SourceMask = 0xFFFC'0000U;
	static constexpr uint32_t SourceMax = (SourceMask >> 18);

	// SourceId is Unique id for FontFace object, that bound with specific unicode plane,
	// so, we can strip plane id from char32_t
	static uint32_t getCharId(uint16_t sourceId, char32_t, CharAnchor);
	static uint32_t rebindCharId(uint32_t, CharAnchor);
	static CharAnchor getAnchorForChar(uint32_t);

	uint32_t layout : 14;
	uint32_t anchor : 2;
	uint32_t value	: 16;

	CharId(uint32_t v) { memcpy(this, &v, sizeof(uint32_t)); }

	CharId(uint16_t l, char16_t ch, CharAnchor a) : layout(l), anchor(toInt(a)), value(ch) { }

	operator uint32_t() const {
		uint32_t ret;
		memcpy(&ret, this, sizeof(uint32_t));
		return ret;
	}
};

struct SP_PUBLIC CharShape16 final {
	char16_t charID = 0;
	uint16_t xAdvance = 0;
};

struct SP_PUBLIC CharShape final {
	char32_t charID = 0;
	uint16_t xAdvance = 0;

	operator char32_t() const { return charID; }
};

struct SP_PUBLIC CharTexture final {
	char32_t charID = 0;

	int16_t x = 0;
	int16_t y = 0;

	uint16_t width = 0;
	uint16_t height = 0;
	uint16_t bitmapWidth = 0;
	uint16_t bitmapRows = 0;
	int16_t pitch = 0;

	uint16_t fontID = 0;

	uint8_t *bitmap;
};

struct SP_PUBLIC FontAtlasValue {
	geom::Vec2 pos;
	geom::Vec2 tex;
};

template <typename Value>
struct SP_PUBLIC FontCharStorage {
	using CellType = std::array<Value, 256>;

	FontCharStorage() { cells.fill(nullptr); }

	~FontCharStorage() {
		for (auto &it : cells) {
			if (it) {
				delete it;
				it = nullptr;
			}
		}
	}

	void emplace(char16_t ch, Value &&value) {
		auto cellId = ch / 256;
		if (!cells[cellId]) {
			cells[cellId] = new CellType;
			memset(cells[cellId]->data(), 0, cells[cellId]->size() * sizeof(Value));
		}

		(*cells[cellId])[ch % 256] = move(value);
	}

	const Value *get(char16_t ch) const {
		auto cellId = ch / 256;
		auto cell = cells[cellId];
		if (!cell) {
			return nullptr;
		}
		return &(cell->at(ch % 256));
	}

	template <typename Callback>
	void foreach (const Callback &cb) {
		static_assert(std::is_invocable_v<Callback, const Value &>, "Invalid callback type");
		for (auto &it : cells) {
			if (it) {
				for (auto &iit : *it) { cb(iit); }
			}
		}
	}

	std::array<CellType *, 256> cells;
};

inline bool operator<(const CharShape &l, const CharShape &c) { return l.charID < c.charID; }
inline bool operator>(const CharShape &l, const CharShape &c) { return l.charID > c.charID; }
inline bool operator<=(const CharShape &l, const CharShape &c) { return l.charID <= c.charID; }
inline bool operator>=(const CharShape &l, const CharShape &c) { return l.charID >= c.charID; }

inline bool operator<(const CharShape &l, const char32_t &c) { return l.charID < c; }
inline bool operator>(const CharShape &l, const char32_t &c) { return l.charID > c; }
inline bool operator<=(const CharShape &l, const char32_t &c) { return l.charID <= c; }
inline bool operator>=(const CharShape &l, const char32_t &c) { return l.charID >= c; }

} // namespace stappler::font

#endif /* CORE_FONT_SPFONT_H_ */
