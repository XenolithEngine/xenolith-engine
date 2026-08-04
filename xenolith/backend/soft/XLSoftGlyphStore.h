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

#ifndef XENOLITH_BACKEND_SOFT_XLSOFTGLYPHSTORE_H_
#define XENOLITH_BACKEND_SOFT_XLSOFTGLYPHSTORE_H_

#include "XLSoft.h"
#include "SPFont.h"

#include <sprt/cxx/mutex>

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft {

// Rasterized glyphs, kept as they came out of FreeType.
//
// The GPU backends pack glyphs into an atlas image so that a run of text costs one texture binding.
// A software rasterizer has no bindings to save - picking a different source pointer per glyph is
// free - so the atlas would buy nothing and cost a great deal: every time a new character appears
// on screen, the atlas is re-packed, re-allocated and every previously rasterized glyph is copied
// into it again. And that is not a start-up cost: FontFaceObject::_required only ever grows, so the
// whole set is resubmitted on every update.
//
// Here glyphs are simply appended to a slab and never moved. A glyph is rasterized once, in place,
// by FreeType itself (font::FontFaceObject::renderTextureUnsafe writes into the slot this class
// hands out), so it is never copied at all. Pages arrive zeroed, which is also what makes the
// in-place rasterization legal - FreeType composites coverage into its target.
//
// Shared between the font-rendering worker threads and the render thread, hence the lock; it is
// taken per glyph while filling the cache, never while drawing.
class SP_PUBLIC GlyphStore : public Ref {
public:
	struct Glyph {
		const uint8_t *pixels = nullptr;
		uint32_t pitch = 0;

		// the rasterized coverage bitmap
		uint16_t width = 0;
		uint16_t rows = 0;

		// placement relative to the pen, in the same units and signs as font::CharTexture
		int16_t x = 0;
		int16_t y = 0;

		// grid-fitted metrics; equal to width/rows for every well-formed glyph, and compared
		// against them before the blit path is taken
		uint16_t metricWidth = 0;
		uint16_t metricHeight = 0;
	};

	virtual ~GlyphStore() = default;

	bool init();

	// Glyph ids are font::CharId values with the anchor bits cleared: the four corners of a quad
	// name the same glyph.
	static uint32_t getGlyphId(uint32_t objectId);

	bool hasGlyph(uint32_t glyphId) const;
	const Glyph *getGlyph(uint32_t glyphId) const;

	// Reserve storage for a glyph and register it. Returns where to rasterize, or an empty target
	// if the glyph is already known or cannot be stored.
	font::GlyphTarget emplaceGlyph(uint32_t glyphId, const font::CharTexture &);

	// The white texel underlines are drawn with (font::CharId::SourceMax). Not a glyph, but it
	// reaches the renderer through the same path, so it lives in the same table.
	void emplaceWhitePixel();

	uint32_t getGlyphCount() const;
	uint64_t getMemoryUsage() const;

	// Iteration is for building the atlas of quad geometry, which happens on one thread while the
	// store is not being filled.
	void foreachGlyph(const Callback<void(uint32_t, const Glyph &)> &) const;

protected:
	static constexpr size_t PageSize = 64_KiB;

	uint8_t *allocate(size_t size);

	// Pages are never freed or moved while the store lives, so the pointers in _glyphs stay valid.
	Vector<Bytes> _pages;
	size_t _bumpPage = maxOf<size_t>();
	size_t _pageOffset = 0;
	uint64_t _used = 0;
	Map<uint32_t, Glyph> _glyphs;
	mutable sprt::mutex _mutex;
};

} // namespace stappler::xenolith::soft

#endif /* XENOLITH_BACKEND_SOFT_XLSOFTGLYPHSTORE_H_ */
