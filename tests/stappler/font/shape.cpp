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

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

// Exercises HarfBuzz shaping via FontFaceObject::shape(): the glyphs HarfBuzz proposes (the RENDERING
// set -- glyph indices) together with their advances (the POSITIONING set), over the embedded DejaVu
// Sans face. This is the foundation for feeding HarfBuzz glyphs to the renderer.
void performShapeTests() {
	sprt::cout << "\n== stappler font shaping tests (HarfBuzz) ==\n";

	auto lib = Rc<font::FontLibrary>::create();
	check(lib != nullptr, "shape: font library created");
	if (!lib) {
		return;
	}

	font::FontSpecializationVector spec;
	spec.fontSize = font::FontSize(uint16_t(32));

	auto face = lib->openFontFace(
			font::FontLibrary::getFontName(font::FontLibrary::DefaultFontName::DejaVuSans), spec, [] {
		return font::FontLibrary::FontData(
				font::FontLibrary::getFont(font::FontLibrary::DefaultFontName::DejaVuSans), true);
	});
	check(face != nullptr, "shape: DejaVu Sans face opened");
	if (!face) {
		return;
	}

	// --- basic Latin shaping: the glyphs HarfBuzz proposes for rendering ---
	const char32_t latin[] = {'A', 'V', 'A'};
	mem_std::Vector<font::ShapedGlyph> glyphs;
	bool ok = face->shape(latin, 3, font::TextDirection::LeftToRight, glyphs);
	check(ok && !glyphs.empty(), "shape: produces glyphs for 'AVA'");
	check(glyphs.size() == 3, "shape: 3 glyphs for 3 Latin code points (1:1, no ligatures)");

	bool allGid = true, allAdvance = true, clustersOk = true;
	for (uint32_t i = 0; i < glyphs.size(); ++i) {
		if (glyphs[i].glyphId == 0) {
			allGid = false;
		}
		if (glyphs[i].xAdvance <= 0) {
			allAdvance = false;
		}
		if (glyphs[i].cluster != i) {
			clustersOk = false;
		}
	}
	check(allGid, "shape: every glyph has a non-zero glyph index (the render glyph)");
	check(allAdvance, "shape: every glyph has a positive advance (positioning)");
	check(clustersOk, "shape: clusters map 1:1 back to source code points");
	check(glyphs[0].glyphId != uint32_t('A'), "shape: glyph index is a GID, not the code point");

	// --- advance parity: HarfBuzz advance for a lone 'A' equals the FreeType getChar advance ---
	mem_std::Vector<char32_t> pre;
	pre.emplace_back('A');
	mem_std::Vector<char32_t> failed;
	face->addChars(pre, false, &failed);
	auto csA = face->getChar('A');
	mem_std::Vector<font::ShapedGlyph> one;
	face->shape(pre.data(), 1, font::TextDirection::LeftToRight, one);
	int advDiff = !one.empty() ? (int(one[0].xAdvance) - int(csA.xAdvance)) : 1000;
	if (advDiff < 0) {
		advDiff = -advDiff;
	}
	check(csA.xAdvance > 0 && advDiff <= 1,
			"shape: HarfBuzz advance matches FreeType getChar advance for 'A'");

	// --- RTL direction is accepted (Hebrew aleph, bet) ---
	const char32_t rtl[] = {0x05D0, 0x05D1};
	mem_std::Vector<font::ShapedGlyph> rtlGlyphs;
	bool rtlOk = face->shape(rtl, 2, font::TextDirection::RightToLeft, rtlGlyphs);
	check(rtlOk, "shape: RTL run shaped without error");

	// --- empty input is rejected ---
	mem_std::Vector<font::ShapedGlyph> none;
	check(!face->shape(latin, 0, font::TextDirection::LeftToRight, none),
			"shape: zero-length run returns false");

	// --- PROBE: find a code point DejaVu decomposes 1 -> N glyphs (same cluster) ---
	// --- 1 -> N decomposition: one code point shaping to several glyphs sharing a cluster ---
	// U+06C0 (ARABIC LETTER HEH WITH YEH ABOVE) has no single glyph in DejaVu Sans, so HarfBuzz
	// decomposes it into a base + mark -- two glyphs both at cluster 0. The Formatter routes the extra
	// glyph to a ContinuationChar entry so every glyph renders (Formatter::expandGlyphContinuations).
	const char32_t decomp = 0x06C0;
	mem_std::Vector<font::ShapedGlyph> dg;
	face->shape(&decomp, 1, font::TextDirection::RightToLeft, dg);
	check(dg.size() >= 2, "shape: U+06C0 decomposes one code point into multiple glyphs (1->N)");
	check(dg.size() >= 2 && dg[0].cluster == 0 && dg[1].cluster == 0,
			"shape: the decomposed glyphs share the source cluster");
	check(dg.size() >= 2 && dg[0].glyphId != 0 && dg[1].glyphId != 0,
			"shape: both decomposed glyphs carry real glyph indices");

	// --- #9: font-variant-ligatures toggle ---
	const char32_t ffi[] = {'f', 'f', 'i'};
	mem_std::Vector<font::ShapedGlyph> ligOn, ligOff;
	face->shape(ffi, 3, font::TextDirection::LeftToRight, ligOn, true);
	face->shape(ffi, 3, font::TextDirection::LeftToRight, ligOff, false);
	check(ligOff.size() == 3, "shape: ligatures disabled keeps 'ffi' as 3 separate glyphs");
	check(ligOff.size() >= ligOn.size(),
			"shape: disabling ligatures never yields fewer glyphs than enabling");
	sprt::cout << "  ligatures: 'ffi' enabled=" << ligOn.size() << " disabled=" << ligOff.size()
			   << "\n";

	// --- #8: bidi-aware selection rectangles + continuation/skip ---
	auto mk = [](char32_t id, int16_t pos, uint16_t adv, uint8_t flags, uint8_t level = 0) {
		font::CharLayoutData c;
		c.charID = id;
		c.pos = pos;
		c.advance = adv;
		c.gid = 1;
		c.flags = flags;
		c.bidiLevel = level;
		return c;
	};

	// one line: [a,b] LTR at x 0..20, [c,d] RTL reversed at x 90..110 -> a visual gap in the middle
	font::TextLayoutData<memory::StandartInterface> tl;
	tl.chars.emplace_back(mk('a', 0, 10, 0));
	tl.chars.emplace_back(mk('b', 10, 10, 0));
	tl.chars.emplace_back(mk('c', 100, 10, 0));
	tl.chars.emplace_back(mk('d', 90, 10, 0));
	font::LineLayoutData line;
	line.start = 0;
	line.count = 4;
	line.pos = 20;
	line.height = 20;
	tl.lines.emplace_back(line);

	int rects = 0;
	tl.getLabelRects([&](font::Rect) { ++rects; }, 0, 3, 1.0f, font::Vec2::ZERO, font::Padding());
	check(rects == 2, "selection: a bidi range yields 2 visual rects (one per direction run)");

	// the line rect (which the selection quads build on) spans the full VISUAL extent of a bidi line:
	// min(pos)=0 .. max(pos+advance)=110, not logical-first(0)..logical-last(100)
	auto lineRect = tl.getLineRect(tl.lines[0], 1.0f, font::Vec2::ZERO);
	check(lineRect.origin.x == 0.0f && lineRect.size.width == 110.0f,
			"line rect: a bidi line spans its full visual extent (min..max), not logical first/last");

	// a direction change inside the selection splits into 2 rects even when the runs are ADJACENT
	// (no visual gap): [a,b] level 0 at x 0..20, [c,d] level 1 (RTL) at x 20..40
	font::TextLayoutData<memory::StandartInterface> td;
	td.chars.emplace_back(mk('a', 0, 10, 0, 0));
	td.chars.emplace_back(mk('b', 10, 10, 0, 0));
	td.chars.emplace_back(mk('c', 30, 10, 0, 1));
	td.chars.emplace_back(mk('d', 20, 10, 0, 1));
	font::LineLayoutData l3;
	l3.start = 0;
	l3.count = 4;
	l3.pos = 20;
	l3.height = 20;
	td.lines.emplace_back(l3);

	int rects3 = 0;
	td.getLabelRects([&](font::Rect) { ++rects3; }, 0, 3, 1.0f, font::Vec2::ZERO, font::Padding());
	check(rects3 == 2,
			"selection: a direction change splits into 2 rects even when the runs are adjacent");

	// a glyph-continuation entry is skipped by selection rects and by text extraction
	font::TextLayoutData<memory::StandartInterface> tc;
	tc.chars.emplace_back(mk('x', 0, 10, 0));
	tc.chars.emplace_back(mk(font::CharLayoutData::ContinuationChar, 5, 0,
			font::CharLayoutData::FlagGlyphContinuation));
	tc.chars.emplace_back(mk('y', 10, 10, 0));
	font::LineLayoutData l2;
	l2.start = 0;
	l2.count = 3;
	l2.pos = 20;
	l2.height = 20;
	tc.lines.emplace_back(l2);
	font::RangeLayoutData rng;
	rng.start = 0;
	rng.count = 3;
	rng.align = font::VerticalAlign::Baseline;
	tc.ranges.emplace_back(rng);

	int rects2 = 0;
	tc.getLabelRects([&](font::Rect) { ++rects2; }, 0, 2, 1.0f, font::Vec2::ZERO, font::Padding());
	check(rects2 == 1, "selection: a glyph-continuation does not split the selection rect");

	mem_std::String s;
	tc.str([&](char32_t c) { s += char(c < 128 ? char(c) : '?'); });
	check(s == "xy", "selection: text extraction skips the continuation virtual code point");
}

} // namespace stappler
