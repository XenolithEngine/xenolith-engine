/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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
#include "SPFontLibrary.h"
#include "SPLog.h"

#include <sprt/runtime/hash.h>

#include "ft2build.h" // IWYU pragma: keep
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
#include FT_MULTIPLE_MASTERS_H
#include FT_TRUETYPE_TABLES_H
#include FT_SFNT_NAMES_H
#include FT_ADVANCES_H

namespace STAPPLER_VERSIONIZED stappler::font {

static constexpr uint32_t getAxisTag(char c1, char c2, char c3, char c4) {
	return uint32_t(c1 & 0xFF) << 24 | uint32_t(c2 & 0xFF) << 16 | uint32_t(c3 & 0xFF) << 8
			| uint32_t(c4 & 0xFF);
}

static constexpr uint32_t getAxisTag(const char c[4]) { return getAxisTag(c[0], c[1], c[2], c[3]); }

static CharGroupId getCharGroupForChar(char32_t c) {
	using namespace sprt::chars;
	if (CharGroup<char32_t, CharGroupId::Numbers>::match(c)) {
		return CharGroupId::Numbers;
	} else if (CharGroup<char32_t, CharGroupId::Latin>::match(c)) {
		return CharGroupId::Latin;
	} else if (CharGroup<char32_t, CharGroupId::Cyrillic>::match(c)) {
		return CharGroupId::Cyrillic;
	} else if (CharGroup<char32_t, CharGroupId::Currency>::match(c)) {
		return CharGroupId::Currency;
	} else if (CharGroup<char32_t, CharGroupId::GreekBasic>::match(c)) {
		return CharGroupId::GreekBasic;
	} else if (CharGroup<char32_t, CharGroupId::Math>::match(c)) {
		return CharGroupId::Math;
	} else if (CharGroup<char32_t, CharGroupId::TextPunctuation>::match(c)) {
		return CharGroupId::TextPunctuation;
	}
	return CharGroupId::None;
}

bool FontFaceData::init(StringView name, BytesView data, bool persistent) {
	if (persistent) {
		_view = data;
		_persistent = true;
		_name = name.str<Interface>();
		return true;
	} else {
		return init(name, data.bytes<Interface>());
	}
}

bool FontFaceData::init(StringView name, Bytes &&data) {
	_persistent = false;
	_data = sp::move(data);
	_view = _data;
	_name = name.str<Interface>();
	return true;
}

bool FontFaceData::init(StringView name, Function<Bytes()> &&cb) {
	_persistent = true;
	_data = cb();
	_view = _data;
	_name = name.str<Interface>();
	return true;
}

FontLayoutParameters FontFaceData::acquireDefaultParams(FT_Face face) {
	FontLayoutParameters sfnt;

	if (face->style_flags & FT_STYLE_FLAG_ITALIC) {
		sfnt.fontStyle = FontStyle::Italic;
	}

	if (face->style_flags & FT_STYLE_FLAG_BOLD) {
		sfnt.fontWeight = FontWeight::Bold;
	}

	auto table = (TT_OS2 *)FT_Get_Sfnt_Table(face, FT_SFNT_OS2);
	if (table) {
		sfnt.fontWeight = FontWeight(table->usWeightClass);
		switch (table->usWidthClass) {
		case 1: sfnt.fontStretch = FontStretch::UltraCondensed; break;
		case 2: sfnt.fontStretch = FontStretch::ExtraCondensed; break;
		case 3: sfnt.fontStretch = FontStretch::Condensed; break;
		case 4: sfnt.fontStretch = FontStretch::SemiCondensed; break;
		case 5: sfnt.fontStretch = FontStretch::Normal; break;
		case 6: sfnt.fontStretch = FontStretch::SemiExpanded; break;
		case 7: sfnt.fontStretch = FontStretch::Expanded; break;
		case 8: sfnt.fontStretch = FontStretch::ExtraExpanded; break;
		case 9: sfnt.fontStretch = FontStretch::UltraExpanded; break;
		default: break;
		}

		if (table->panose[0] == 2) {
			// only for Latin Text
			FontLayoutParameters panose;
			switch (table->panose[2]) {
			case 2: panose.fontWeight = FontWeight::ExtraLight; break;
			case 3: panose.fontWeight = FontWeight::Light; break;
			case 4: panose.fontWeight = FontWeight::Thin; break;
			case 5: panose.fontWeight = FontWeight::Normal; break;
			case 6: panose.fontWeight = FontWeight::Medium; break;
			case 7: panose.fontWeight = FontWeight::SemiBold; break;
			case 8: panose.fontWeight = FontWeight::Bold; break;
			case 9: panose.fontWeight = FontWeight::ExtraBold; break;
			case 10: panose.fontWeight = FontWeight::Heavy; break;
			case 11: panose.fontWeight = FontWeight::Black; break;
			default: break;
			}

			switch (table->panose[3]) {
			case 2: panose.fontStretch = FontStretch::Normal; break;
			case 5: panose.fontStretch = FontStretch::Expanded; break;
			case 6: panose.fontStretch = FontStretch::Condensed; break;
			case 7: panose.fontStretch = FontStretch::ExtraExpanded; break;
			case 8: panose.fontStretch = FontStretch::ExtraCondensed; break;
			default: break;
			}

			switch (table->panose[7]) {
			case 5: panose.fontStyle = FontStyle::Oblique; break;
			case 9: panose.fontStyle = FontStyle::Oblique; break;
			case 10: panose.fontStyle = FontStyle::Oblique; break;
			case 11: panose.fontStyle = FontStyle::Oblique; break;
			case 12: panose.fontStyle = FontStyle::Oblique; break;
			case 13: panose.fontStyle = FontStyle::Oblique; break;
			case 14: panose.fontStyle = FontStyle::Oblique; break;
			default: break;
			}

			if (panose.fontWeight != sfnt.fontWeight) {
				if (panose.fontWeight != FontWeight::Normal) {
					sfnt.fontWeight = panose.fontWeight;
				}
			}

			if (panose.fontStretch != sfnt.fontStretch) {
				if (panose.fontStretch != FontStretch::Normal) {
					sfnt.fontStretch = panose.fontStretch;
				}
			}

			if (sfnt.fontStyle == FontStyle::Normal && panose.fontStyle != sfnt.fontStyle) {
				sfnt.fontStyle = panose.fontStyle;
			}
		}
	} else {
		log::source().error("font::FontFaceData",
				"No preconfigured style or OS/2 table for font: ", _name);
	}
	return sfnt;
}

void FontFaceData::inspectVariableFont(FontLayoutParameters params, FT_Library lib, FT_Face face) {
	FT_MM_Var *masters = nullptr;
	FT_Get_MM_Var(face, &masters);

	_variations.weight = params.fontWeight;
	_variations.stretch = params.fontStretch;
	_variations.opticalSize = uint32_t(0);
	_variations.italic = uint32_t(params.fontStyle == FontStyle::Italic ? 1 : 0);
	_variations.slant = params.fontStyle;
	_variations.grade = params.fontGrade;

	if (masters) {
		for (FT_UInt i = 0; i < masters->num_axis; ++i) {
			auto tag = masters->axis[i].tag;
			if (tag == getAxisTag("wght")) {
				_variations.axisMask |= FontVariableAxis::Weight;
				_variations.weight.min = FontWeight(masters->axis[i].minimum >> 16);
				_variations.weight.max = FontWeight(masters->axis[i].maximum >> 16);
			} else if (tag == getAxisTag("wdth")) {
				_variations.axisMask |= FontVariableAxis::Width;
				_variations.stretch.min = FontStretch(masters->axis[i].minimum >> 15);
				_variations.stretch.max = FontStretch(masters->axis[i].maximum >> 15);
			} else if (tag == getAxisTag("ital")) {
				_variations.axisMask |= FontVariableAxis::Italic;
				_variations.italic.min = uint32_t(masters->axis[i].minimum);
				_variations.italic.max = uint32_t(masters->axis[i].maximum);
			} else if (tag == getAxisTag("slnt")) {
				_variations.axisMask |= FontVariableAxis::Slant;
				_variations.slant.min = FontStyle(masters->axis[i].minimum >> 10);
				_variations.slant.max = FontStyle(masters->axis[i].maximum >> 10);
			} else if (tag == getAxisTag("opsz")) {
				_variations.axisMask |= FontVariableAxis::OpticalSize;
				_variations.opticalSize.min = uint32_t(masters->axis[i].minimum);
				_variations.opticalSize.max = uint32_t(masters->axis[i].maximum);
			} else if (tag == getAxisTag("GRAD")) {
				_variations.axisMask |= FontVariableAxis::Grade;
				_variations.grade.min = FontGrade(masters->axis[i].minimum >> 16);
				_variations.grade.max = FontGrade(masters->axis[i].maximum >> 16);
			}
			/* sprt::cout << "Variable axis: [" << masters->axis[i].tag << "] "
					<< (masters->axis[i].minimum >> 16) << " - " << (masters->axis[i].maximum >> 16)
					<< " def: "<< (masters->axis[i].def >> 16) << "\n"; */
		}

		FT_Done_MM_Var(lib, masters);
	}

	_params = params;
}

BytesView FontFaceData::getView() const { return _view; }

uint64_t FontFaceData::getContentHash() const {
	auto v = getView();
	if (v.empty()) {
		return 0;
	}
	return sprt::xxh64::hash(reinterpret_cast<const char *>(v.data()), v.size(), 0);
}

FontSpecializationVector FontFaceData::getSpecialization(
		const FontSpecializationVector &vec) const {
	return _variations.getSpecialization(vec);
}

bool FontFaceObject::init(StringView name, const Rc<FontFaceData> &data, FT_Library lib,
		FT_Face face, const FontSpecializationVector &spec, uint16_t id, uint16_t plane) {
	if (!face) {
		// newFontFace() returns null on malformed/corrupt font data
		return false;
	}
	auto err = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
	if (err != FT_Err_Ok) {
		return false;
	}

	auto &var = data->getVariations();
	if (var.axisMask != FontVariableAxis::None) {
		Vector<FT_Fixed> vector;

		FT_MM_Var *masters;
		FT_Get_MM_Var(face, &masters);

		if (masters) {
			for (FT_UInt i = 0; i < masters->num_axis; ++i) {
				auto tag = masters->axis[i].tag;
				if (tag == getAxisTag("wght")) {
					vector.emplace_back(var.weight.clamp(spec.fontWeight).get() << 16);
				} else if (tag == getAxisTag("wdth")) {
					vector.emplace_back(var.stretch.clamp(spec.fontStretch).get() << 15);
				} else if (tag == getAxisTag("ital")) {
					if (spec.fontStyle.get() == FontStyle::Normal.get()) {
						vector.emplace_back(var.italic.min);
					} else if (spec.fontStyle.get() == FontStyle::Italic.get()) {
						vector.emplace_back(var.italic.max);
					} else {
						if ((var.axisMask & FontVariableAxis::Slant) != FontVariableAxis::None) {
							vector.emplace_back(var.italic.min); // has true oblique
						} else {
							vector.emplace_back(var.italic.max);
						}
					}
				} else if (tag == getAxisTag("slnt")) {
					if (spec.fontStyle.get() == FontStyle::Normal.get()) {
						vector.emplace_back(0);
					} else if (spec.fontStyle.get() == FontStyle::Italic.get()) {
						if ((var.axisMask & FontVariableAxis::Italic) != FontVariableAxis::None) {
							vector.emplace_back(masters->axis[i].def);
						} else {
							vector.emplace_back(var.slant.clamp(FontStyle::Oblique).get() << 10);
						}
					} else {
						vector.emplace_back(var.slant.clamp(spec.fontStyle).get() << 10);
					}
				} else if (tag == getAxisTag("opsz")) {
					auto opticalSize = uint32_t(floorf(spec.fontSize.get() / spec.density)) << 16;
					vector.emplace_back(var.opticalSize.clamp(opticalSize));
				} else if (tag == getAxisTag("GRAD")) {
					vector.emplace_back(var.grade.clamp(spec.fontGrade).get() << 16);
				} else {
					vector.emplace_back(masters->axis[i].def);
				}
			}

			FT_Set_Var_Design_Coordinates(face, FT_UInt(vector.size()), vector.data());
			FT_Done_MM_Var(lib, masters);
		}
	}

	// set the requested font size
	err = FT_Set_Pixel_Sizes(face, spec.fontSize.get(), spec.fontSize.get());
	if (err != FT_Err_Ok) {
		return false;
	}

	_spec = spec;
	_metrics.size = spec.fontSize.get();
	_metrics.height = face->size->metrics.height >> 6;
	_metrics.ascender = face->size->metrics.ascender >> 6;
	_metrics.descender = face->size->metrics.descender >> 6;
	_metrics.underlinePosition = face->underline_position >> 6;
	_metrics.underlineThickness = face->underline_thickness >> 6;

	_name = name.str<Interface>();
	_id = id;
	_data = data;
	_face = face;
	_plane = plane;

	return true;
}

char16_t FontFaceObject::getCharId(char32_t theChar) const {
	auto plane = ((theChar >> 16) & 0xFFFF);
	if (plane != _plane) {
		return 0;
	}

	return char16_t(theChar & 0xFFFF);
}


// `glyphIndex` is a FreeType glyph index (the glyph HarfBuzz/the layout selected for rendering), not
// a code point -- the codepoint->glyph mapping already happened during layout (FontFaceObject::getChar
// or HarfBuzz shaping). Rasterize the glyph directly and key the resulting texture by its glyph index.
bool FontFaceObject::acquireTextureUnsafe(char32_t glyphIndex,
		const Callback<void(const CharTexture &)> &cb) {
	if (!glyphIndex) {
		return false;
	}

	auto err = FT_Load_Glyph(_face, FT_UInt(glyphIndex), FT_LOAD_DEFAULT | FT_LOAD_RENDER);
	if (err != FT_Err_Ok) {
		return false;
	}

	if (_face->glyph->bitmap.buffer != nullptr) {
		if (_face->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
			cb(CharTexture{glyphIndex,
				static_cast<int16_t>(_face->glyph->metrics.horiBearingX >> 6),
				static_cast<int16_t>(-(_face->glyph->metrics.horiBearingY >> 6)),
				static_cast<uint16_t>(_face->glyph->metrics.width >> 6),
				static_cast<uint16_t>(_face->glyph->metrics.height >> 6),
				static_cast<uint16_t>(_face->glyph->bitmap.width),
				static_cast<uint16_t>(_face->glyph->bitmap.rows),
				_face->glyph->bitmap.pitch ? int16_t(_face->glyph->bitmap.pitch)
										   : int16_t(_face->glyph->bitmap.width),
				_id, _face->glyph->bitmap.buffer});
			return true;
		}
	} else {
		log::format(sprt::oslog::Warn, "Font", SP_LOCATION, "error: no bitmap for glyph %u",
				uint32_t(glyphIndex));
	}
	return false;
}

// Same glyph as acquireTextureUnsafe, rasterized straight into the caller's storage.
//
// FreeType does not expose the grid-fitting its own renderer applies before allocating the slot
// bitmap, so it is repeated here: floor the control box down to the pixel grid, ceil it up, and
// translate the outline into the resulting box. Getting this wrong does not fail - it shifts the
// glyph by a pixel or clips it - which is why the equality test in tests/font exists.
bool FontFaceObject::renderTextureUnsafe(char32_t glyphIndex,
		const Callback<GlyphTarget(const CharTexture &)> &cb) {
	if (!glyphIndex) {
		return false;
	}

	// FT_LOAD_DEFAULT, not FT_LOAD_NO_BITMAP: a face with embedded strikes must resolve the same
	// way it does under FT_LOAD_RENDER, or the two paths would disagree on such fonts.
	auto err = FT_Load_Glyph(_face, FT_UInt(glyphIndex), FT_LOAD_DEFAULT);
	if (err != FT_Err_Ok) {
		return false;
	}

	auto slot = _face->glyph;

	auto makeTexture = [&](uint16_t bitmapWidth, uint16_t bitmapRows) {
		return CharTexture{glyphIndex, static_cast<int16_t>(slot->metrics.horiBearingX >> 6),
			static_cast<int16_t>(-(slot->metrics.horiBearingY >> 6)),
			static_cast<uint16_t>(slot->metrics.width >> 6),
			static_cast<uint16_t>(slot->metrics.height >> 6), bitmapWidth, bitmapRows,
			int16_t(bitmapWidth), _id, nullptr};
	};

	// An outline flagged as self-overlapping is rendered by FreeType with oversampling ("quadruples
	// the rendering time", per FT_OUTLINE_OVERLAP) - and that happens in the renderer module, not in
	// the raster FT_Outline_Get_Bitmap reaches. Rasterizing such a glyph ourselves produces visibly
	// different pixels (measured: up to 73/255 on RobotoFlex, which sets the flag on nearly every
	// glyph, while DejaVu never does). Reproducing the oversampling by hand would be guesswork tied
	// to a FreeType version, so those glyphs go through the renderer and pay for one copy.
	const bool canRasterizeDirect = slot->format == FT_GLYPH_FORMAT_OUTLINE
			&& (slot->outline.flags & FT_OUTLINE_OVERLAP) == 0;

	if (canRasterizeDirect) {
		FT_Pos originX = 0;
		FT_Pos originY = 0;
		uint16_t bitmapWidth = 0;
		uint16_t bitmapRows = 0;

		if (slot->bitmap.width > 0 && slot->bitmap.rows > 0) {
			// FreeType presets the bitmap geometry while loading the glyph, and its own renderer
			// then uses exactly these numbers. Taking them verbatim is the only way to be sure the
			// two paths agree - a hand-rolled control box matches for some faces and drifts by a
			// fraction of a pixel for others (measured: identical on DejaVu, off on RobotoFlex).
			bitmapWidth = uint16_t(slot->bitmap.width);
			bitmapRows = uint16_t(slot->bitmap.rows);
			originX = FT_Pos(slot->bitmap_left) * 64;
			originY = FT_Pos(slot->bitmap_top - int(bitmapRows)) * 64;
		} else {
			// No preset (older FreeType, or a renderer that does not fill it in): fall back to the
			// grid-fitted control box, which is what the preset is computed from.
			FT_BBox cbox;
			FT_Outline_Get_CBox(&slot->outline, &cbox);

			cbox.xMin &= ~63; // FT_PIX_FLOOR
			cbox.yMin &= ~63;
			cbox.xMax = (cbox.xMax + 63) & ~63; // FT_PIX_CEIL
			cbox.yMax = (cbox.yMax + 63) & ~63;

			bitmapWidth = uint16_t((cbox.xMax - cbox.xMin) >> 6);
			bitmapRows = uint16_t((cbox.yMax - cbox.yMin) >> 6);
			originX = cbox.xMin;
			originY = cbox.yMin;
		}

		if (bitmapWidth == 0 || bitmapRows == 0) {
			return false; // whitespace: nothing to store and nothing to draw
		}

		auto target = cb(makeTexture(bitmapWidth, bitmapRows));
		if (!target.buffer) {
			return false;
		}

		FT_Outline_Translate(&slot->outline, -originX, -originY);

		FT_Bitmap bitmap;
		sprt::memset(&bitmap, 0, sizeof(bitmap));
		bitmap.buffer = target.buffer;
		bitmap.width = bitmapWidth;
		bitmap.rows = bitmapRows;
		bitmap.pitch = target.pitch ? target.pitch : int(bitmapWidth);
		bitmap.pixel_mode = FT_PIXEL_MODE_GRAY;
		bitmap.num_grays = 256;

		// Composites into the target, so it has to arrive zeroed - see GlyphTarget.
		err = FT_Outline_Get_Bitmap(slot->library, &slot->outline, &bitmap);
		if (err != FT_Err_Ok) {
			log::format(sprt::oslog::Warn, "Font", SP_LOCATION,
					"error: fail to rasterize glyph %u", uint32_t(glyphIndex));
			return false;
		}
		return true;
	}

	// The copying path: overlapping outlines (above) and bitmap-only faces (embedded strikes,
	// colour fonts). FreeType owns the pixels here, so this is the one case that still copies -
	// once, when the glyph is first rasterized, not per frame. Keeping it inside this function
	// means callers never branch on the glyph format.
	if (slot->format != FT_GLYPH_FORMAT_BITMAP) {
		err = FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);
		if (err != FT_Err_Ok) {
			return false;
		}
	}

	if (!slot->bitmap.buffer || slot->bitmap.pixel_mode != FT_PIXEL_MODE_GRAY) {
		return false;
	}

	auto bitmapWidth = uint16_t(slot->bitmap.width);
	auto bitmapRows = uint16_t(slot->bitmap.rows);
	if (bitmapWidth == 0 || bitmapRows == 0) {
		return false;
	}

	auto target = cb(makeTexture(bitmapWidth, bitmapRows));
	if (!target.buffer) {
		return false;
	}

	auto pitch = target.pitch ? target.pitch : int32_t(bitmapWidth);
	auto srcPitch = slot->bitmap.pitch ? ptrdiff_t(slot->bitmap.pitch) : ptrdiff_t(bitmapWidth);

	// The pitch is signed and is the step from one row to the next, so it indexes rows directly -
	// a negative one just walks the buffer backwards.
	for (uint16_t row = 0; row < bitmapRows; ++row) {
		sprt::memcpy(target.buffer + ptrdiff_t(pitch) * row, slot->bitmap.buffer + srcPitch * row,
				bitmapWidth);
	}
	return true;
}

bool FontFaceObject::addChars(const Vector<char32_t> &chars, bool expand,
		Vector<char32_t> *failed) {
	bool updated = false;
	uint32_t mask = 0;

	if constexpr (!config::FontPreloadGroups) {
		expand = false;
	}

	for (auto &c : chars) {
		auto plane = ((c >> 16) & 0xFFFF);
		if (plane != _plane) {
			mem_std::emplace_ordered(*failed, c);
			continue;
		}

		if (expand) {
			// for some chars, we add full group, not only requested char
			auto g = getCharGroupForChar(c);
			if (g != CharGroupId::None) {
				if ((mask & toInt(g)) == 0) {
					mask |= toInt(g);
					if (addCharGroup(g, failed)) {
						updated = true;
					}
					continue;
				}
			}
		}

		if (!addChar(char16_t(c & 0xFFFF), updated) && failed) {
			mem_std::emplace_ordered(*failed, c);
		}
	}
	return updated;
}

bool FontFaceObject::addCharGroup(CharGroupId g, Vector<char32_t> *failed) {
	bool updated = false;
	using namespace sprt::chars;
	auto f = [&, this](char32_t c) {
		auto plane = ((c >> 16) & 0xFFFF);
		if ((plane != _plane || !addChar(char16_t(c & 0xFFFF), updated)) && failed) {
			mem_std::emplace_ordered(*failed, c);
		}
	};

	switch (g) {
	case CharGroupId::Numbers: CharGroup<char32_t, CharGroupId::Numbers>::foreach (f); break;
	case CharGroupId::Latin: CharGroup<char32_t, CharGroupId::Latin>::foreach (f); break;
	case CharGroupId::Cyrillic: CharGroup<char32_t, CharGroupId::Cyrillic>::foreach (f); break;
	case CharGroupId::Currency: CharGroup<char32_t, CharGroupId::Currency>::foreach (f); break;
	case CharGroupId::GreekBasic: CharGroup<char32_t, CharGroupId::GreekBasic>::foreach (f); break;
	case CharGroupId::Math: CharGroup<char32_t, CharGroupId::Math>::foreach (f); break;
	case CharGroupId::TextPunctuation:
		CharGroup<char32_t, CharGroupId::TextPunctuation>::foreach (f);
		break;
	default: break;
	}
	return updated;
}

bool FontFaceObject::addRequiredChar(char32_t ch) {
	sprt::unique_lock lock(_requiredMutex);
	return mem_std::emplace_ordered(_required, ch);
}

auto FontFaceObject::getRequiredChars() const -> Vector<char32_t> {
	sprt::unique_lock lock(_requiredMutex);
	return _required;
}

size_t FontFaceObject::getRequiredCharsCount() const {
	sprt::unique_lock lock(_requiredMutex);
	return _required.size();
}

bool FontFaceObject::hasPendingChars() const {
	sprt::unique_lock lock(_requiredMutex);
	return _required.size() > _submittedChars;
}

void FontFaceObject::setCharsSubmitted(size_t count) {
	sprt::unique_lock lock(_requiredMutex);
	// Never move it backwards: two batches can be assembled from overlapping snapshots, and the
	// later one to be recorded is not necessarily the larger.
	if (count > _submittedChars) {
		_submittedChars = count;
	}
}

void FontFaceObject::resetCharsSubmitted() {
	sprt::unique_lock lock(_requiredMutex);
	_submittedChars = 0;
}

uint16_t FontFaceObject::getGlyphIndex(char32_t theChar) {
	if (!_face) {
		return 0;
	}
	sprt::unique_lock lock(_faceMutex);
	return uint16_t(FT_Get_Char_Index(_face, theChar));
}

CharShape FontFaceObject::getChar(char32_t c) const {
	auto plane = ((c >> 16) & 0xFFFF);
	if (plane != _plane) {
		return CharShape{0};
	}

	auto ch = char16_t(c & 0xFFFF);
	sprt::shared_lock lock(_charsMutex);
	auto l = _chars.get(ch);
	if (l && l->charID == ch) {
		return CharShape{char32_t(l->charID) | (char32_t(_plane) << 16), l->xAdvance,
			l->glyphIndex};
	}
	return CharShape{0};
}

int16_t FontFaceObject::getKerningAmount(char32_t first, char32_t second) const {
	auto planeA = ((first >> 16) & 0xFFFF);
	auto planeB = ((second >> 16) & 0xFFFF);
	if (planeA != _plane || planeB != _plane) {
		return 0;
	}

	auto firstCh = first & 0xFFFF;
	auto secondCh = second & 0xFFFF;

	sprt::shared_lock lock(_charsMutex);
	uint32_t key = (firstCh << 16) | (secondCh & 0xffff);
	auto it = _kerning.find(key);
	if (it != _kerning.end()) {
		return it->second;
	}
	return 0;
}

bool FontFaceObject::addChar(char16_t theChar, bool &updated) {
	do {
		// try to get char with shared lock
		sprt::shared_lock charsLock(_charsMutex);
		auto value = _chars.get(theChar);
		if (value) {
			if (value->charID == theChar) {
				return true;
			} else if (value->charID == char16_t(0xFFFF)) {
				return false;
			}
		}
	} while (0);

	sprt::unique_lock charsUniqueLock(_charsMutex);
	auto value = _chars.get(theChar);
	if (value) {
		if (value->charID == theChar) {
			return true;
		} else if (value->charID == char16_t(0xFFFF)) {
			return false;
		}
	}

	sprt::unique_lock faceLock(_faceMutex);
	FT_UInt cIdx = FT_Get_Char_Index(_face, theChar);
	if (!cIdx) {
		_chars.emplace(theChar, CharShape16{char16_t(0xFFFF)});
		return false;
	}

	FT_Fixed advance;
	auto err = FT_Get_Advance(_face, cIdx, FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP, &advance);
	if (err != FT_Err_Ok) {
		_chars.emplace(theChar, CharShape16{char16_t(0xFFFF)});
		return false;
	}

	/*auto err = FT_Load_Glyph(_face, cIdx, FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP);
	if (err != FT_Err_Ok) {
		_chars.emplace(theChar, CharLayout{char16_t(0xFFFF)});
		return false;
	}*/

	// store result in the passed rectangle
	_chars.emplace(theChar,
			CharShape16{
				char16_t(theChar),
				static_cast<uint16_t>(advance >> 16),
				static_cast<uint16_t>(cIdx),
			});

	if (!sprt::chars::isspace(theChar)) {
		updated = true;
	}

	if (FT_HAS_KERNING(_face)) {
		_chars.foreach ([&, this](const CharShape16 &it) {
			if (it.charID == 0 || it.charID == char16_t(0xFFFF)) {
				return;
			}

			if (it.charID != theChar) {
				FT_Vector kerning;
				auto err = FT_Get_Kerning(_face, cIdx, cIdx, FT_KERNING_DEFAULT, &kerning);
				if (err == FT_Err_Ok) {
					auto value = int16_t(kerning.x >> 6);
					if (value != 0) {
						_kerning.emplace(theChar << 16 | (it.charID & 0xffff), value);
					}
				}
			} else {
				auto kIdx = FT_Get_Char_Index(_face, it.charID);

				FT_Vector kerning;
				auto err = FT_Get_Kerning(_face, cIdx, kIdx, FT_KERNING_DEFAULT, &kerning);
				if (err == FT_Err_Ok) {
					auto value = int16_t(kerning.x >> 6);
					if (value != 0) {
						_kerning.emplace(theChar << 16 | (it.charID & 0xffff), value);
					}
				}

				err = FT_Get_Kerning(_face, kIdx, cIdx, FT_KERNING_DEFAULT, &kerning);
				if (err == FT_Err_Ok) {
					auto value = int16_t(kerning.x >> 6);
					if (value != 0) {
						_kerning.emplace(it.charID << 16 | (theChar & 0xffff), value);
					}
				}
			}
		});
	}
	return true;
}

auto FontFaceSet::constructName(StringView family, const FontSpecializationVector &vec) -> String {
	return FontParameters::getFontConfigName<Interface>(family, vec.fontSize, vec.fontStyle,
			vec.fontWeight, vec.fontStretch, vec.fontGrade, FontVariant::Normal, vec.density,
			false);
}

bool FontFaceSet::init(String &&name, StringView family, FontSpecializationVector &&spec,
		Rc<FontFaceData> &&data, FontLibrary *c) {
	_name = sp::move(name);
	_family = family.str<Interface>();
	_spec = sp::move(spec);
	_sources.emplace_back(move(data));
	_library = c;
	_faces.resize(_sources.size(), nullptr);
	if (auto face = _library->openFontFace(_sources.front(), _spec)) {
		_faces[0] = face;
		_metrics = _faces.front()->getMetrics();
	}
	return true;
}

bool FontFaceSet::init(String &&name, StringView family, FontSpecializationVector &&spec,
		Vector<Rc<FontFaceData>> &&data, FontLibrary *c) {
	_name = sp::move(name);
	_family = family.str<Interface>();
	_spec = sp::move(spec);
	_sources = sp::move(data);
	_faces.resize(_sources.size(), nullptr);
	_library = c;
	if (auto face = _library->openFontFace(_sources.front(), _spec)) {
		_faces[0] = face;
		_metrics = _faces.front()->getMetrics();
	}
	return true;
}

void FontFaceSet::touch(uint64_t clock, bool persistent) {
	_accessTime = clock;
	_persistent = persistent;
}

bool FontFaceSet::addString(const CharVector &str) {
	Vector<char32_t> failed;
	return addString(str, failed);
}

bool FontFaceSet::addString(const CharVector &str, Vector<char32_t> &failed) {
	sprt::shared_lock sharedLock(_mutex);

	bool shouldOpenFonts = false;
	bool updated = false;
	size_t i = 0;

	for (auto &it : _faces) {
		if (i == 0) {
			if (it->addChars(str.chars, i == 0, &failed)) {
				updated = true;
			}
		} else {
			// font was not loaded - try to load then add chars
			if (it == nullptr) {
				shouldOpenFonts = true;
				break;
			}

			auto tmp = sp::move(failed);
			failed.clear();

			if (it->addChars(tmp, i == 0, &failed)) {
				updated = true;
			}
		}

		if (failed.empty()) {
			break;
		}

		++i;
	}

	if (shouldOpenFonts) {
		sharedLock.unlock();
		sprt::unique_lock lock(_mutex);

		for (; i < _faces.size(); ++i) {
			if (_faces[i] == nullptr) {
				_faces[i] = _library->openFontFace(_sources[i], _spec);
			}

			auto tmp = sp::move(failed);
			failed.clear();

			if (_faces[i]->addChars(tmp, i == 0, &failed)) {
				updated = true;
			}

			if (failed.empty()) {
				break;
			}
		}
	}

	return updated;
}

uint16_t FontFaceSet::getFontHeight() const { return _metrics.height; }

int16_t FontFaceSet::getKerningAmount(char32_t first, char32_t second, uint16_t face) const {
	sprt::shared_lock lock(_mutex);
	for (auto &it : _faces) {
		if (it) {
			if (it->getId() == face) {
				return it->getKerningAmount(first, second);
			}
		} else {
			return 0;
		}
	}
	return 0;
}

Metrics FontFaceSet::getMetrics() const { return _metrics; }

CharShape FontFaceSet::getChar(char32_t ch, uint16_t &face) const {
	sprt::shared_lock lock(_mutex);
	for (auto &it : _faces) {
		if (!it) {
			continue;
		}
		auto l = it->getChar(ch);
		if (l.charID != 0) {
			face = it->getId();
			return l;
		}
	}
	return CharShape();
}

size_t FontFaceSet::getRequiredCharsCount() const {
	size_t count = 0;
	for (auto &face : _faces) {
		if (face) {
			count += face->getRequiredCharsCount();
		}
	}
	return count;
}

bool FontFaceSet::addTextureChars(SpanView<CharLayoutData> chars) const {
	sprt::shared_lock lock(_mutex);

	bool ret = false;
	for (auto &it : chars) {
		if (sprt::chars::isspace(it.charID) || it.charID == char16_t(0x0A)
				|| it.charID == char16_t(0x00AD)) {
			continue;
		}

		for (auto &f : _faces) {
			if (f && f->getId() == it.face) {
				if (f->addRequiredChar(it.gid)) {
					++_texturesCount;
					ret = true;
					break;
				}
			}
		}
	}
	return ret;
}

auto FontFaceSet::getFaces() const -> const Vector<Rc<FontFaceObject>> & { return _faces; }

size_t FontFaceSet::getFaceCount() const { return _sources.size(); }

Rc<FontFaceData> FontFaceSet::getSource(size_t idx) const {
	if (idx < _sources.size()) {
		return _sources[idx];
	}
	return nullptr;
}

} // namespace stappler::font
