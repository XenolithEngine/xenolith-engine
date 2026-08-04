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

#include "XLSoftGlyphStore.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft {

bool GlyphStore::init() { return true; }

uint32_t GlyphStore::getGlyphId(uint32_t objectId) {
	return font::CharId::rebindCharId(objectId, font::CharAnchor::BottomLeft);
}

uint8_t *GlyphStore::allocate(size_t size) {
	if (size == 0) {
		return nullptr;
	}

	auto addPage = [&](size_t pageSize) -> uint8_t * {
		Bytes page;
		page.resize(pageSize, uint8_t(0)); // zeroed: FreeType composites into what it is given
		_pages.emplace_back(sp::move(page));
		return _pages.back().data();
	};

	_used += size;

	// A glyph bigger than a page gets a page of its own. The bump page is tracked by index rather
	// than as "the last one" precisely so that this case does not abandon the rest of it.
	if (size > PageSize) {
		return addPage(size);
	}

	if (_bumpPage == maxOf<size_t>() || _pageOffset + size > PageSize) {
		addPage(PageSize);
		_bumpPage = _pages.size() - 1;
		_pageOffset = 0;
	}

	auto ptr = _pages[_bumpPage].data() + _pageOffset;
	_pageOffset += size;
	return ptr;
}

bool GlyphStore::hasGlyph(uint32_t glyphId) const {
	sprt::unique_lock lock(_mutex);
	return _glyphs.find(glyphId) != _glyphs.end();
}

auto GlyphStore::getGlyph(uint32_t glyphId) const -> const Glyph * {
	sprt::unique_lock lock(_mutex);
	auto it = _glyphs.find(glyphId);
	return (it != _glyphs.end()) ? &it->second : nullptr;
}

font::GlyphTarget GlyphStore::emplaceGlyph(uint32_t glyphId, const font::CharTexture &tex) {
	if (tex.bitmapWidth == 0 || tex.bitmapRows == 0) {
		return font::GlyphTarget();
	}

	sprt::unique_lock lock(_mutex);
	if (_glyphs.find(glyphId) != _glyphs.end()) {
		return font::GlyphTarget(); // already rasterized; nothing to do
	}

	auto pitch = uint32_t(tex.bitmapWidth);
	auto pixels = allocate(size_t(pitch) * tex.bitmapRows);
	if (!pixels) {
		return font::GlyphTarget();
	}

	_glyphs.emplace(glyphId,
			Glyph{pixels, pitch, tex.bitmapWidth, tex.bitmapRows, tex.x, tex.y, tex.width,
				tex.height});

	return font::GlyphTarget{pixels, int32_t(pitch)};
}

void GlyphStore::emplaceWhitePixel() {
	auto glyphId = font::CharId::getCharId(font::CharId::SourceMax, 0, font::CharAnchor::BottomLeft);

	sprt::unique_lock lock(_mutex);
	if (_glyphs.find(glyphId) != _glyphs.end()) {
		return;
	}

	auto pixels = allocate(1);
	if (!pixels) {
		return;
	}
	*pixels = 255;

	_glyphs.emplace(glyphId, Glyph{pixels, 1, 1, 1, 0, 0, 1, 1});
}

uint32_t GlyphStore::getGlyphCount() const {
	sprt::unique_lock lock(_mutex);
	return uint32_t(_glyphs.size());
}

uint64_t GlyphStore::getMemoryUsage() const {
	sprt::unique_lock lock(_mutex);
	return _used;
}

void GlyphStore::foreachGlyph(const Callback<void(uint32_t, const Glyph &)> &cb) const {
	sprt::unique_lock lock(_mutex);
	for (auto &it : _glyphs) { cb(it.first, it.second); }
}

} // namespace stappler::xenolith::soft
