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

#include "ft2build.h" // IWYU pragma: keep
#include FT_FREETYPE_H
#include FT_GLYPH_H
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
			/* std::cout << "Variable axis: [" << masters->axis[i].tag << "] "
					<< (masters->axis[i].minimum >> 16) << " - " << (masters->axis[i].maximum >> 16)
					<< " def: "<< (masters->axis[i].def >> 16) << "\n"; */
		}

		FT_Done_MM_Var(lib, masters);
	}

	_params = params;
}

BytesView FontFaceData::getView() const { return _view; }

FontSpecializationVector FontFaceData::getSpecialization(
		const FontSpecializationVector &vec) const {
	return _variations.getSpecialization(vec);
}

bool FontFaceObject::init(StringView name, const Rc<FontFaceData> &data, FT_Library lib,
		FT_Face face, const FontSpecializationVector &spec, uint16_t id, uint16_t plane) {
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

bool FontFaceObject::acquireTexture(char32_t theChar,
		const Callback<void(const CharTexture &)> &cb) {
	std::unique_lock lock(_faceMutex);

	return acquireTextureUnsafe(theChar, cb);
}

bool FontFaceObject::acquireTextureUnsafe(char32_t theChar,
		const Callback<void(const CharTexture &)> &cb) {
	auto plane = ((theChar >> 16) & 0xFFFF);
	if (plane != _plane) {
		return false;
	}

	int glyph_index = FT_Get_Char_Index(_face, theChar);
	if (!glyph_index) {
		return false;
	}

	auto err = FT_Load_Glyph(_face, glyph_index, FT_LOAD_DEFAULT | FT_LOAD_RENDER);
	if (err != FT_Err_Ok) {
		return false;
	}

	//log::format("Texture", "%s: %d '%s'", data.layout->getName().c_str(), theChar.charID, string::toUtf8(theChar.charID).c_str());

	if (_face->glyph->bitmap.buffer != nullptr) {
		if (_face->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
			cb(CharTexture{theChar, static_cast<int16_t>(_face->glyph->metrics.horiBearingX >> 6),
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
		if (!sprt::chars::isspace(theChar) && theChar != char16_t(0x0A)) {
			log::format(log::Warn, "Font", SP_LOCATION, "error: no bitmap for (%d) '%s'", theChar,
					string::toUtf8<Interface>(theChar).data());
		}
	}
	return false;
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
	std::unique_lock lock(_requiredMutex);
	return mem_std::emplace_ordered(_required, ch);
}

auto FontFaceObject::getRequiredChars() const -> Vector<char32_t> {
	std::unique_lock lock(_requiredMutex);
	return _required;
}

size_t FontFaceObject::getRequiredCharsCount() const {
	std::unique_lock lock(_requiredMutex);
	return _required.size();
}

CharShape FontFaceObject::getChar(char32_t c) const {
	auto plane = ((c >> 16) & 0xFFFF);
	if (plane != _plane) {
		return CharShape{0};
	}

	auto ch = char16_t(c & 0xFFFF);
	std::shared_lock lock(_charsMutex);
	auto l = _chars.get(ch);
	if (l && l->charID == ch) {
		return CharShape{char32_t(l->charID) | (char32_t(_plane) << 16), l->xAdvance};
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

	std::shared_lock lock(_charsMutex);
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
		std::shared_lock charsLock(_charsMutex);
		auto value = _chars.get(theChar);
		if (value) {
			if (value->charID == theChar) {
				return true;
			} else if (value->charID == char16_t(0xFFFF)) {
				return false;
			}
		}
	} while (0);

	std::unique_lock charsUniqueLock(_charsMutex);
	auto value = _chars.get(theChar);
	if (value) {
		if (value->charID == theChar) {
			return true;
		} else if (value->charID == char16_t(0xFFFF)) {
			return false;
		}
	}

	std::unique_lock faceLock(_faceMutex);
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
			vec.fontWeight, vec.fontStretch, vec.fontGrade, FontVariant::Normal, false);
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
	std::shared_lock sharedLock(_mutex);

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
		std::unique_lock lock(_mutex);

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
	std::shared_lock lock(_mutex);
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
	std::shared_lock lock(_mutex);
	for (auto &it : _faces) {
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
	std::shared_lock lock(_mutex);

	bool ret = false;
	for (auto &it : chars) {
		if (sprt::chars::isspace(it.charID) || it.charID == char16_t(0x0A)
				|| it.charID == char16_t(0x00AD)) {
			continue;
		}

		for (auto &f : _faces) {
			if (f && f->getId() == it.face) {
				if (f->addRequiredChar(it.charID)) {
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
