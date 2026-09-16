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

#include "SPCommon.h"
#include "SPFontLibrary.h"
#include "SPFontFace.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

// FontFaceObject::renderTextureUnsafe rasterizes straight into caller-provided memory, which means
// it has to reproduce, by hand, the grid-fitting FreeType applies internally before allocating its
// own slot bitmap (floor/ceil the control box, translate the outline into it). Nothing about that is
// checked by rendering a frame - a one-pixel error there just makes text look slightly off. So it is
// checked here instead, glyph by glyph, against the copying path that three shipping backends use.
//
// Every glyph is compared on both metrics and pixels, at several sizes, over both embedded fonts.

namespace {

struct GlyphImage {
	font::CharTexture texture;
	mem_std::Vector<uint8_t> pixels;
	bool present = false;
};

// The reference: load + render inside FreeType, then copy out, exactly as the Vulkan font queue does.
static GlyphImage Glyph_acquire(font::FontFaceObject *face, char32_t glyphIndex) {
	GlyphImage out;
	face->acquireTextureUnsafe(glyphIndex, [&](const font::CharTexture &tex) {
		out.texture = tex;
		out.present = true;
		out.pixels.resize(size_t(tex.bitmapWidth) * tex.bitmapRows);
		for (uint16_t row = 0; row < tex.bitmapRows; ++row) {
			// a signed pitch is the step between rows, so it indexes them directly
			sprt::memcpy(out.pixels.data() + size_t(tex.bitmapWidth) * row,
					tex.bitmap + ptrdiff_t(tex.pitch) * row, tex.bitmapWidth);
		}
	});
	return out;
}

// The subject: rasterize into our own buffer. The extra padding on the pitch is deliberate - a real
// caller (a glyph slab, a staging buffer) hands out rows wider than the glyph, and a pitch confused
// for a width would go unnoticed with a tight one.
static GlyphImage Glyph_render(font::FontFaceObject *face, char32_t glyphIndex, uint32_t padding) {
	GlyphImage out;
	uint32_t pitch = 0;
	face->renderTextureUnsafe(glyphIndex, [&](const font::CharTexture &tex) -> font::GlyphTarget {
		out.texture = tex;
		out.present = true;
		pitch = tex.bitmapWidth + padding;
		// zeroed: FreeType composites coverage into the target rather than overwriting it
		out.pixels.resize(size_t(pitch) * tex.bitmapRows, uint8_t(0));
		return font::GlyphTarget{out.pixels.data(), int32_t(pitch)};
	});

	if (out.present && padding > 0) {
		// squeeze the padding out so the two images compare as plain w*h buffers
		mem_std::Vector<uint8_t> tight(size_t(out.texture.bitmapWidth) * out.texture.bitmapRows,
				uint8_t(0));
		for (uint16_t row = 0; row < out.texture.bitmapRows; ++row) {
			sprt::memcpy(tight.data() + size_t(out.texture.bitmapWidth) * row,
					out.pixels.data() + size_t(pitch) * row, out.texture.bitmapWidth);
		}
		out.pixels = sp::move(tight);
	}
	return out;
}

static bool Glyph_sameMetrics(const font::CharTexture &l, const font::CharTexture &r) {
	return l.x == r.x && l.y == r.y && l.width == r.width && l.height == r.height
			&& l.bitmapWidth == r.bitmapWidth && l.bitmapRows == r.bitmapRows
			&& l.fontID == r.fontID;
}

struct Report {
	uint32_t compared = 0;
	uint32_t metricsMismatch = 0;
	uint32_t pixelMismatch = 0;
	uint32_t presenceMismatch = 0;
	uint32_t referenceUnstable = 0;
	uint32_t worstDelta = 0;
	char32_t firstBadChar = 0;
};

static void Glyph_compareFace(font::FontFaceObject *face, const char32_t *chars, Report &report) {
	for (; *chars; ++chars) {
		auto ch = *chars;
		auto glyphIndex = face->getGlyphIndex(ch);
		if (!glyphIndex) {
			continue; // the face has no glyph for this code point
		}

		// The padding varies so both the tight and the padded destination layouts are exercised.
		auto expected = Glyph_acquire(face, glyphIndex);
		auto actual = Glyph_render(face, glyphIndex, (glyphIndex % 3) * 5);

		// Loading the same glyph twice must give the same pixels; without this, a face that is
		// simply not reproducible would read as a defect in the path under test.
		auto again = Glyph_acquire(face, glyphIndex);
		if (again.present != expected.present || again.pixels != expected.pixels) {
			++report.referenceUnstable;
		}

		if (expected.present != actual.present) {
			++report.presenceMismatch;
			if (!report.firstBadChar) {
				report.firstBadChar = ch;
			}
			continue;
		}
		if (!expected.present) {
			continue; // whitespace: neither path produces pixels
		}

		++report.compared;

		if (!Glyph_sameMetrics(expected.texture, actual.texture)) {
			++report.metricsMismatch;
			if (!report.firstBadChar) {
				report.firstBadChar = ch;
			}
			continue;
		}

		if (expected.pixels.size() != actual.pixels.size()) {
			++report.pixelMismatch;
			continue;
		}

		uint32_t worst = 0;
		for (size_t i = 0; i < expected.pixels.size(); ++i) {
			auto a = int32_t(expected.pixels[i]);
			auto b = int32_t(actual.pixels[i]);
			worst = sprt::max(worst, uint32_t(a > b ? a - b : b - a));
		}
		if (worst > 0) {
			++report.pixelMismatch;
			report.worstDelta = sprt::max(report.worstDelta, worst);
			if (!report.firstBadChar) {
				report.firstBadChar = ch;
			}
		}
	}
}

static Rc<font::FontFaceObject> Glyph_openFace(font::FontLibrary *lib,
		font::FontLibrary::DefaultFontName name, uint16_t size, bool compressed) {
	font::FontSpecializationVector spec;
	spec.fontSize = font::FontSize(size);

	return lib->openFontFace(font::FontLibrary::getFontName(name), spec, [name, compressed] {
		auto d = font::FontLibrary::getFont(name);
		// The variable fonts ship compressed in the resource blob, the way FontComponent opens
		// them; DejaVu is stored as-is.
		if (compressed) {
			return font::FontLibrary::FontData(
					data::decompress<mem_std::Interface>(d.data(), d.size()));
		}
		return font::FontLibrary::FontData(d, true);
	});
}

} // namespace

void performGlyphTests() {
	sprt::cout << "\n== stappler font glyph rasterization tests (zero-copy vs copy) ==\n";

	auto lib = Rc<font::FontLibrary>::create();
	check(lib != nullptr, "glyph: font library created");
	if (!lib) {
		return;
	}

	// Latin, Cyrillic, digits and punctuation - enough shapes that a systematic grid-fit error
	// cannot hide in the ones that happen to sit on the pixel grid already.
	static constexpr auto Chars = U"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
								  U"0123456789.,;:!?-_()[]{}/\\|@#$%^&*+=<>~"
								  U"АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ"
								  U"абвгдеёжзийклмнопрстуфхцчшщъыьэюя";

	struct FontSource {
		font::FontLibrary::DefaultFontName name;
		bool compressed;
	};

	static constexpr FontSource Fonts[] = {
		{font::FontLibrary::DefaultFontName::DejaVuSans, false},
		{font::FontLibrary::DefaultFontName::RobotoFlex_VariableFont, true},
	};

	// Small sizes are where grid-fitting bites hardest, large ones where an off-by-one in the
	// control box shows up as a clipped edge.
	static constexpr uint16_t Sizes[] = {9, 13, 20, 32, 64};

	Report report;
	uint32_t faces = 0;
	for (auto &font : Fonts) {
		for (auto size : Sizes) {
			auto face = Glyph_openFace(lib, font.name, size, font.compressed);
			if (!face) {
				continue;
			}
			++faces;
			Glyph_compareFace(face, Chars, report);
		}
	}

	check(faces == 10, "glyph: all 2 faces x 5 sizes opened");
	check(report.compared > 500,
			mem_std::toString("glyph: enough glyphs compared (", report.compared, ")"));
	check(report.referenceUnstable == 0,
			mem_std::toString("glyph: the reference path is reproducible (",
					report.referenceUnstable, " glyphs differ between two identical loads)"));
	check(report.presenceMismatch == 0,
			mem_std::toString("glyph: both paths agree on which glyphs have pixels (",
					report.presenceMismatch, " disagreements)"));
	check(report.metricsMismatch == 0,
			mem_std::toString("glyph: metrics identical (", report.metricsMismatch, " mismatches, first at U+",
					uint32_t(report.firstBadChar), ")"));
	check(report.pixelMismatch == 0,
			mem_std::toString("glyph: pixels byte-identical (", report.pixelMismatch,
					" mismatches, worst delta ", report.worstDelta, ")"));

	sprt::cout << "glyph: compared " << report.compared << " glyph rasterizations across " << faces
			   << " faces\n";
}

} // namespace stappler
