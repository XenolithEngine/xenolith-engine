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

#include "SPFontFace.h"

#include <harfbuzz/hb.h>
#include <harfbuzz/hb-ft.h>

namespace STAPPLER_VERSIONIZED stappler::font {

FontFaceObject::~FontFaceObject() {
	if (_hbFont) {
		hb_font_destroy(static_cast<hb_font_t *>(_hbFont));
		_hbFont = nullptr;
	}
}

bool FontFaceObject::shape(const char32_t *text, size_t length, TextDirection direction,
		Vector<ShapedGlyph> &out, bool enableLigatures) {
	if (!text || length == 0 || !_face) {
		return false;
	}

	// FT_Face carries mutable size/glyph-slot state; hb_ft reads it during shaping, so lock the face.
	sprt::unique_lock lock(_faceMutex);

	// Cache one HarfBuzz font per face -- creating it per call is expensive. Each FontFaceObject owns
	// its own fixed-pixel-size FT_Face (FontLibrary::newFontFace -> FT_New_Memory_Face), so the
	// hb_font scale stays valid for the face's lifetime. hb_ft returns advances in 26.6 fixed-point
	// pixels.
	// Non-referenced: the hb_font borrows the FT_Face without owning an FT reference, so destroying it
	// never calls back into FreeType (avoids a shutdown crash if FT_Library is already gone). The
	// FT_Face outlives the hb_font -- both are owned by this FontFaceObject and torn down together.
	if (!_hbFont) {
		_hbFont = hb_ft_font_create(_face, nullptr);
	}
	auto hbFont = static_cast<hb_font_t *>(_hbFont);
	if (!hbFont) {
		return false;
	}

	const bool rtl = (direction == TextDirection::RightToLeft);
	hb_unicode_funcs_t *ufuncs = hb_unicode_funcs_get_default();

	// font-variant-ligatures: none -> turn off common/contextual/discretionary ligatures (rlig stays
	// on -- it is required for scripts like Arabic). #9
	hb_feature_t features[3];
	unsigned int featureCount = 0;
	if (!enableLigatures) {
		features[0] = {HB_TAG('l', 'i', 'g', 'a'), 0, 0, (unsigned int)-1};
		features[1] = {HB_TAG('c', 'l', 'i', 'g'), 0, 0, (unsigned int)-1};
		features[2] = {HB_TAG('d', 'l', 'i', 'g'), 0, 0, (unsigned int)-1};
		featureCount = 3;
	}

	// Itemize the run into maximal same-script segments (#5). HarfBuzz shapes one script per buffer;
	// a Common/Inherited/Unknown code point (space, digit, punctuation, combining mark) continues the
	// current script run, so e.g. Latin + Greek on one face each shape with their own rules instead of
	// one guessed script for the whole run.
	struct ScriptRun {
		uint32_t start;
		uint32_t len;
		hb_script_t script;
	};
	Vector<ScriptRun> sruns;
	{
		hb_script_t cur = HB_SCRIPT_INVALID;
		uint32_t runStart = 0;
		for (uint32_t i = 0; i < length; ++i) {
			const hb_script_t s = hb_unicode_script(ufuncs, uint32_t(text[i]));
			if (s == HB_SCRIPT_COMMON || s == HB_SCRIPT_INHERITED || s == HB_SCRIPT_UNKNOWN) {
				continue; // inherit the surrounding script
			}
			if (cur == HB_SCRIPT_INVALID) {
				cur = s; // first strong script also covers any leading common code points
			} else if (s != cur) {
				sruns.emplace_back(ScriptRun{runStart, i - runStart, cur});
				runStart = i;
				cur = s;
			}
		}
		// trailing run; cur may stay INVALID for an all-common run -> let HarfBuzz guess its script
		sruns.emplace_back(ScriptRun{runStart, uint32_t(length) - runStart, cur});
	}

	const size_t before = out.size();

	auto shapeScriptRun = [&](uint32_t rstart, uint32_t rlen, hb_script_t script) {
		hb_buffer_t *buffer = hb_buffer_create();
		hb_buffer_add_utf32(buffer, reinterpret_cast<const uint32_t *>(text) + rstart, int(rlen), 0,
				int(rlen));
		hb_buffer_set_direction(buffer, rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
		if (script != HB_SCRIPT_INVALID) {
			hb_buffer_set_script(buffer, script);
		}
		// fills the language (and the script, when the run was all-common -> INVALID above)
		hb_buffer_guess_segment_properties(buffer);

		hb_shape(hbFont, buffer, featureCount ? features : nullptr, featureCount);

		unsigned int count = 0;
		const hb_glyph_info_t *info = hb_buffer_get_glyph_infos(buffer, &count);
		const hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(buffer, &count);
		out.reserve(out.size() + count);
		for (unsigned int i = 0; i < count; ++i) {
			ShapedGlyph glyph;
			glyph.glyphId =
					info[i].codepoint; // after shaping this is the glyph index, not a code point
			glyph.cluster = info[i].cluster + rstart; // map back to the full run's indices
			glyph.xAdvance = int16_t(pos[i].x_advance >> 6); // 26.6 fixed-point -> pixels
			glyph.yAdvance = int16_t(pos[i].y_advance >> 6);
			glyph.xOffset = int16_t(pos[i].x_offset >> 6);
			glyph.yOffset = int16_t(pos[i].y_offset >> 6);
			out.emplace_back(glyph);
		}

		hb_buffer_destroy(buffer);
	};

	// Lay script runs out left-to-right: logical order for LTR, reversed for RTL (HarfBuzz still
	// reverses glyphs WITHIN each run), so the caller's left-to-right advance accumulation is correct.
	if (!rtl) {
		for (auto &r : sruns) { shapeScriptRun(r.start, r.len, r.script); }
	} else {
		for (size_t i = sruns.size(); i-- > 0;) {
			shapeScriptRun(sruns[i].start, sruns[i].len, sruns[i].script);
		}
	}

	return out.size() > before;
}

} // namespace stappler::font
