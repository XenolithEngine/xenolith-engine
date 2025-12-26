/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "MaterialColorScheme.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

CorePalette::CorePalette(const Color4F &color, bool isContentColor)
: CorePalette(geom::Cam16::create(color), isContentColor) { }

CorePalette::CorePalette(const geom::Cam16 &cam, bool isContentColor)
: CorePalette(cam.hue, cam.chroma, isContentColor) { }

CorePalette::CorePalette(geom::Cam16Float hue, geom::Cam16Float chroma, bool is_content)
: primary(hue, is_content ? chroma : std::fmax(chroma, geom::Cam16Float(48)))
, secondary(hue, is_content ? chroma / 3 : 16)
, tertiary(hue + 60, is_content ? chroma / 2 : 24)
, neutral(hue, is_content ? std::fmin(chroma / geom::Cam16Float(12), geom::Cam16Float(4)) : 4)
, neutralVariant(hue, is_content ? std::fmin(chroma / geom::Cam16Float(6), geom::Cam16Float(8)) : 8)
, error(25, 84) { }

ColorRole ColorScheme::getColorRoleOn(ColorRole role, ThemeType type) {
	switch (role) {
	case ColorRole::Primary: return ColorRole::OnPrimary; break;
	case ColorRole::OnPrimary: return ColorRole::Primary; break;
	case ColorRole::PrimaryContainer: return ColorRole::OnPrimaryContainer; break;
	case ColorRole::OnPrimaryContainer: return ColorRole::PrimaryContainer; break;
	case ColorRole::Secondary: return ColorRole::OnSecondary; break;
	case ColorRole::OnSecondary: return ColorRole::Secondary; break;
	case ColorRole::SecondaryContainer: return ColorRole::OnSecondaryContainer; break;
	case ColorRole::OnSecondaryContainer: return ColorRole::SecondaryContainer; break;
	case ColorRole::Tertiary: return ColorRole::OnTertiary; break;
	case ColorRole::OnTertiary: return ColorRole::Tertiary; break;
	case ColorRole::TertiaryContainer: return ColorRole::OnTertiaryContainer; break;
	case ColorRole::OnTertiaryContainer: return ColorRole::TertiaryContainer; break;
	case ColorRole::Error: return ColorRole::OnError; break;
	case ColorRole::OnError: return ColorRole::Error; break;
	case ColorRole::ErrorContainer: return ColorRole::OnErrorContainer; break;
	case ColorRole::OnErrorContainer: return ColorRole::ErrorContainer; break;
	case ColorRole::Background: return ColorRole::OnBackground; break;
	case ColorRole::OnBackground: return ColorRole::Background; break;
	case ColorRole::Surface: return ColorRole::OnSurface; break;
	case ColorRole::OnSurface: return ColorRole::Surface; break;
	case ColorRole::SurfaceVariant: return ColorRole::OnSurfaceVariant; break;
	case ColorRole::OnSurfaceVariant: return ColorRole::SurfaceVariant; break;
	case ColorRole::Outline: return ColorRole::OnBackground; break;
	case ColorRole::OutlineVariant: return ColorRole::OnBackground; break;
	case ColorRole::Shadow: return (type == ThemeType::LightTheme) ? ColorRole::Background : ColorRole::OnBackground; break;
	case ColorRole::Scrim: return (type == ThemeType::LightTheme) ? ColorRole::Background : ColorRole::OnBackground; break;
	case ColorRole::InverseSurface: return ColorRole::InverseOnSurface; break;
	case ColorRole::InverseOnSurface: return ColorRole::InverseSurface; break;
	case ColorRole::InversePrimary: return ColorRole::OnBackground; break;
	case ColorRole::Undefined: break;
	case ColorRole::Max: break;
	}
	return role;
}

ColorScheme::ColorScheme(ThemeType t, const CorePalette &p) : type(t), palette(p) {
	set(type, palette);
}

ColorScheme::ColorScheme(ThemeType t, const Color4F &color, bool isContent)
: ColorScheme(t, CorePalette(color, isContent)) { }

ColorScheme::ColorScheme(ThemeType t, const ColorHCT &color, bool isContent)
: ColorScheme(t, CorePalette(color.data.hue, color.data.chroma, isContent)) { }

void ColorScheme::set(ThemeType t, const CorePalette &p) {
	type = t;
	switch (type) {
	case ThemeType::LightTheme:
	case ThemeType::Custom:
		colors[toInt(ColorRole::Primary)] = Color4F(p.primary.get(40));
		colors[toInt(ColorRole::OnPrimary)] = Color4F(p.primary.get(100));
		colors[toInt(ColorRole::PrimaryContainer)] = Color4F(p.primary.get(90));
		colors[toInt(ColorRole::OnPrimaryContainer)] = Color4F(p.primary.get(10));
		colors[toInt(ColorRole::Secondary)] = Color4F(p.secondary.get(40));
		colors[toInt(ColorRole::OnSecondary)] = Color4F(p.secondary.get(100));
		colors[toInt(ColorRole::SecondaryContainer)] = Color4F(p.secondary.get(90));
		colors[toInt(ColorRole::OnSecondaryContainer)] = Color4F(p.secondary.get(10));
		colors[toInt(ColorRole::Tertiary)] = Color4F(p.tertiary.get(40));
		colors[toInt(ColorRole::OnTertiary)] = Color4F(p.tertiary.get(100));
		colors[toInt(ColorRole::TertiaryContainer)] = Color4F(p.tertiary.get(90));
		colors[toInt(ColorRole::OnTertiaryContainer)] = Color4F(p.tertiary.get(10));
		colors[toInt(ColorRole::Error)] = Color4F(p.error.get(40));
		colors[toInt(ColorRole::OnError)] = Color4F(p.error.get(100));
		colors[toInt(ColorRole::ErrorContainer)] = Color4F(p.error.get(90));
		colors[toInt(ColorRole::OnErrorContainer)] = Color4F(p.error.get(10));
		colors[toInt(ColorRole::Background)] = Color4F(p.neutral.get(99));
		colors[toInt(ColorRole::OnBackground)] = Color4F(p.neutral.get(10));
		colors[toInt(ColorRole::Surface)] = Color4F(p.neutral.get(99));
		colors[toInt(ColorRole::OnSurface)] = Color4F(p.neutral.get(10));
		colors[toInt(ColorRole::SurfaceVariant)] = Color4F(p.neutralVariant.get(90));
		colors[toInt(ColorRole::OnSurfaceVariant)] = Color4F(p.neutralVariant.get(30));
		colors[toInt(ColorRole::Outline)] = Color4F(p.neutralVariant.get(50));
		colors[toInt(ColorRole::OutlineVariant)] = Color4F(p.neutralVariant.get(80));
		colors[toInt(ColorRole::Shadow)] = Color4F(p.neutral.get(0));
		colors[toInt(ColorRole::Scrim)] = Color4F(p.neutral.get(0));
		colors[toInt(ColorRole::InverseSurface)] = Color4F(p.neutral.get(20));
		colors[toInt(ColorRole::InverseOnSurface)] = Color4F(p.neutral.get(95));
		colors[toInt(ColorRole::InversePrimary)] = Color4F(p.primary.get(80));
		break;
	case ThemeType::DarkTheme:
		colors[toInt(ColorRole::Primary)] = Color4F(p.primary.get(80));
		colors[toInt(ColorRole::OnPrimary)] = Color4F(p.primary.get(20));
		colors[toInt(ColorRole::PrimaryContainer)] = Color4F(p.primary.get(30));
		colors[toInt(ColorRole::OnPrimaryContainer)] = Color4F(p.primary.get(90));
		colors[toInt(ColorRole::Secondary)] = Color4F(p.secondary.get(80));
		colors[toInt(ColorRole::OnSecondary)] = Color4F(p.secondary.get(20));
		colors[toInt(ColorRole::SecondaryContainer)] = Color4F(p.secondary.get(30));
		colors[toInt(ColorRole::OnSecondaryContainer)] = Color4F(p.secondary.get(90));
		colors[toInt(ColorRole::Tertiary)] = Color4F(p.tertiary.get(80));
		colors[toInt(ColorRole::OnTertiary)] = Color4F(p.tertiary.get(20));
		colors[toInt(ColorRole::TertiaryContainer)] = Color4F(p.tertiary.get(30));
		colors[toInt(ColorRole::OnTertiaryContainer)] = Color4F(p.tertiary.get(90));
		colors[toInt(ColorRole::Error)] = Color4F(p.error.get(80));
		colors[toInt(ColorRole::OnError)] = Color4F(p.error.get(20));
		colors[toInt(ColorRole::ErrorContainer)] = Color4F(p.error.get(30));
		colors[toInt(ColorRole::OnErrorContainer)] = Color4F(p.error.get(80));
		colors[toInt(ColorRole::Background)] = Color4F(p.neutral.get(10));
		colors[toInt(ColorRole::OnBackground)] = Color4F(p.neutral.get(90));
		colors[toInt(ColorRole::Surface)] = Color4F(p.neutral.get(10));
		colors[toInt(ColorRole::OnSurface)] = Color4F(p.neutral.get(90));
		colors[toInt(ColorRole::SurfaceVariant)] = Color4F(p.neutralVariant.get(30));
		colors[toInt(ColorRole::OnSurfaceVariant)] = Color4F(p.neutralVariant.get(80));
		colors[toInt(ColorRole::Outline)] = Color4F(p.neutralVariant.get(60));
		colors[toInt(ColorRole::OutlineVariant)] = Color4F(p.neutralVariant.get(30));
		colors[toInt(ColorRole::Shadow)] = Color4F(p.neutral.get(0));
		colors[toInt(ColorRole::Scrim)] = Color4F(p.neutral.get(0));
		colors[toInt(ColorRole::InverseSurface)] = Color4F(p.neutral.get(90));
		colors[toInt(ColorRole::InverseOnSurface)] = Color4F(p.neutral.get(20));
		colors[toInt(ColorRole::InversePrimary)] = Color4F(p.primary.get(40));
		break;
	}

	this->palette = p;
}

void ColorScheme::set(ThemeType t, const Color4F &color, bool isContent) {
	set(t, CorePalette(color, isContent));
}

void ColorScheme::set(ThemeType t, const ColorHCT &color, bool isContent) {
	set(t, CorePalette(color.data.hue, color.data.chroma, isContent));
}

ColorHCT ColorScheme::hct(ColorRole name, float alpha) const {
	switch (type) {
	case ThemeType::LightTheme:
		switch (name) {
		case ColorRole::Primary: return palette.primary.hct(40, alpha); break;
		case ColorRole::OnPrimary: return palette.primary.hct(100, alpha); break;
		case ColorRole::PrimaryContainer: return palette.primary.hct(90, alpha); break;
		case ColorRole::OnPrimaryContainer: return palette.primary.hct(10, alpha); break;
		case ColorRole::Secondary: return palette.secondary.hct(40, alpha); break;
		case ColorRole::OnSecondary: return palette.secondary.hct(100, alpha); break;
		case ColorRole::SecondaryContainer: return palette.secondary.hct(90, alpha); break;
		case ColorRole::OnSecondaryContainer: return palette.secondary.hct(10, alpha); break;
		case ColorRole::Tertiary: return palette.tertiary.hct(40, alpha); break;
		case ColorRole::OnTertiary: return palette.tertiary.hct(100, alpha); break;
		case ColorRole::TertiaryContainer: return palette.tertiary.hct(90, alpha); break;
		case ColorRole::OnTertiaryContainer: return palette.tertiary.hct(10, alpha); break;
		case ColorRole::Error: return palette.error.hct(40, alpha); break;
		case ColorRole::OnError: return palette.error.hct(100, alpha); break;
		case ColorRole::ErrorContainer: return palette.error.hct(90, alpha); break;
		case ColorRole::OnErrorContainer: return palette.error.hct(10, alpha); break;
		case ColorRole::Background: return palette.neutral.hct(99, alpha); break;
		case ColorRole::OnBackground: return palette.neutral.hct(10, alpha); break;
		case ColorRole::Surface: return palette.neutral.hct(99, alpha); break;
		case ColorRole::OnSurface: return palette.neutral.hct(10, alpha); break;
		case ColorRole::SurfaceVariant: return palette.neutralVariant.hct(90, alpha); break;
		case ColorRole::OnSurfaceVariant: return palette.neutralVariant.hct(30, alpha); break;
		case ColorRole::Outline: return palette.neutralVariant.hct(50, alpha); break;
		case ColorRole::OutlineVariant: return palette.neutralVariant.hct(80, alpha); break;
		case ColorRole::Shadow: return palette.neutral.hct(0, alpha); break;
		case ColorRole::Scrim: return palette.neutral.hct(0, alpha); break;
		case ColorRole::InverseSurface: return palette.neutral.hct(20, alpha); break;
		case ColorRole::InverseOnSurface: return palette.neutral.hct(95, alpha); break;
		case ColorRole::InversePrimary: return palette.primary.hct(80, alpha); break;
		case ColorRole::Undefined: break;
		case ColorRole::Max: break;
		}
		break;
	case ThemeType::DarkTheme:
		switch (name) {
		case ColorRole::Primary: return palette.primary.hct(80, alpha); break;
		case ColorRole::OnPrimary: return palette.primary.hct(20, alpha); break;
		case ColorRole::PrimaryContainer: return palette.primary.hct(30, alpha); break;
		case ColorRole::OnPrimaryContainer: return palette.primary.hct(90, alpha); break;
		case ColorRole::Secondary: return palette.secondary.hct(80, alpha); break;
		case ColorRole::OnSecondary: return palette.secondary.hct(20, alpha); break;
		case ColorRole::SecondaryContainer: return palette.secondary.hct(30, alpha); break;
		case ColorRole::OnSecondaryContainer: return palette.secondary.hct(90, alpha); break;
		case ColorRole::Tertiary: return palette.tertiary.hct(80, alpha); break;
		case ColorRole::OnTertiary: return palette.tertiary.hct(20, alpha); break;
		case ColorRole::TertiaryContainer: return palette.tertiary.hct(30, alpha); break;
		case ColorRole::OnTertiaryContainer: return palette.tertiary.hct(90, alpha); break;
		case ColorRole::Error: return palette.error.hct(80, alpha); break;
		case ColorRole::OnError: return palette.error.hct(20, alpha); break;
		case ColorRole::ErrorContainer: return palette.error.hct(30, alpha); break;
		case ColorRole::OnErrorContainer: return palette.error.hct(80, alpha); break;
		case ColorRole::Background: return palette.neutral.hct(10, alpha); break;
		case ColorRole::OnBackground: return palette.neutral.hct(90, alpha); break;
		case ColorRole::Surface: return palette.neutral.hct(10, alpha); break;
		case ColorRole::OnSurface: return palette.neutral.hct(90, alpha); break;
		case ColorRole::SurfaceVariant: return palette.neutralVariant.hct(30, alpha); break;
		case ColorRole::OnSurfaceVariant: return palette.neutralVariant.hct(80, alpha); break;
		case ColorRole::Outline: return palette.neutralVariant.hct(60, alpha); break;
		case ColorRole::OutlineVariant: return palette.neutralVariant.hct(30, alpha); break;
		case ColorRole::Shadow: return palette.neutral.hct(0, alpha); break;
		case ColorRole::Scrim: return palette.neutral.hct(0, alpha); break;
		case ColorRole::InverseSurface: return palette.neutral.hct(90, alpha); break;
		case ColorRole::InverseOnSurface: return palette.neutral.hct(20, alpha); break;
		case ColorRole::InversePrimary: return palette.primary.hct(40, alpha); break;
		case ColorRole::Undefined: break;
		case ColorRole::Max: break;
		}
		break;
	case ThemeType::Custom:
		return ColorHCT(colors[toInt(name)], alpha);
		break;
	}
	return ColorHCT();
}

ColorHCT::Values ColorScheme::values(ColorRole name, float alpha) const {
	switch (type) {
	case ThemeType::LightTheme:
		switch (name) {
		case ColorRole::Primary: return palette.primary.values(40, alpha); break;
		case ColorRole::OnPrimary: return palette.primary.values(100, alpha); break;
		case ColorRole::PrimaryContainer: return palette.primary.values(90, alpha); break;
		case ColorRole::OnPrimaryContainer: return palette.primary.values(10, alpha); break;
		case ColorRole::Secondary: return palette.secondary.values(40, alpha); break;
		case ColorRole::OnSecondary: return palette.secondary.values(100, alpha); break;
		case ColorRole::SecondaryContainer: return palette.secondary.values(90, alpha); break;
		case ColorRole::OnSecondaryContainer: return palette.secondary.values(10, alpha); break;
		case ColorRole::Tertiary: return palette.tertiary.values(40, alpha); break;
		case ColorRole::OnTertiary: return palette.tertiary.values(100, alpha); break;
		case ColorRole::TertiaryContainer: return palette.tertiary.values(90, alpha); break;
		case ColorRole::OnTertiaryContainer: return palette.tertiary.values(10, alpha); break;
		case ColorRole::Error: return palette.error.values(40, alpha); break;
		case ColorRole::OnError: return palette.error.values(100, alpha); break;
		case ColorRole::ErrorContainer: return palette.error.values(90, alpha); break;
		case ColorRole::OnErrorContainer: return palette.error.values(10, alpha); break;
		case ColorRole::Background: return palette.neutral.values(99, alpha); break;
		case ColorRole::OnBackground: return palette.neutral.values(10, alpha); break;
		case ColorRole::Surface: return palette.neutral.values(99, alpha); break;
		case ColorRole::OnSurface: return palette.neutral.values(10, alpha); break;
		case ColorRole::SurfaceVariant: return palette.neutralVariant.values(90, alpha); break;
		case ColorRole::OnSurfaceVariant: return palette.neutralVariant.values(30, alpha); break;
		case ColorRole::Outline: return palette.neutralVariant.values(50, alpha); break;
		case ColorRole::OutlineVariant: return palette.neutralVariant.values(80, alpha); break;
		case ColorRole::Shadow: return palette.neutral.values(0, alpha); break;
		case ColorRole::Scrim: return palette.neutral.values(0, alpha); break;
		case ColorRole::InverseSurface: return palette.neutral.values(20, alpha); break;
		case ColorRole::InverseOnSurface: return palette.neutral.values(95, alpha); break;
		case ColorRole::InversePrimary: return palette.primary.values(80, alpha); break;
		case ColorRole::Undefined: break;
		case ColorRole::Max: break;
		}
		break;
	case ThemeType::DarkTheme:
		switch (name) {
		case ColorRole::Primary: return palette.primary.values(80, alpha); break;
		case ColorRole::OnPrimary: return palette.primary.values(20, alpha); break;
		case ColorRole::PrimaryContainer: return palette.primary.values(30, alpha); break;
		case ColorRole::OnPrimaryContainer: return palette.primary.values(90, alpha); break;
		case ColorRole::Secondary: return palette.secondary.values(80, alpha); break;
		case ColorRole::OnSecondary: return palette.secondary.values(20, alpha); break;
		case ColorRole::SecondaryContainer: return palette.secondary.values(30, alpha); break;
		case ColorRole::OnSecondaryContainer: return palette.secondary.values(90, alpha); break;
		case ColorRole::Tertiary: return palette.tertiary.values(80, alpha); break;
		case ColorRole::OnTertiary: return palette.tertiary.values(20, alpha); break;
		case ColorRole::TertiaryContainer: return palette.tertiary.values(30, alpha); break;
		case ColorRole::OnTertiaryContainer: return palette.tertiary.values(90, alpha); break;
		case ColorRole::Error: return palette.error.values(80, alpha); break;
		case ColorRole::OnError: return palette.error.values(20, alpha); break;
		case ColorRole::ErrorContainer: return palette.error.values(30, alpha); break;
		case ColorRole::OnErrorContainer: return palette.error.values(80, alpha); break;
		case ColorRole::Background: return palette.neutral.values(10, alpha); break;
		case ColorRole::OnBackground: return palette.neutral.values(90, alpha); break;
		case ColorRole::Surface: return palette.neutral.values(10, alpha); break;
		case ColorRole::OnSurface: return palette.neutral.values(90, alpha); break;
		case ColorRole::SurfaceVariant: return palette.neutralVariant.values(30, alpha); break;
		case ColorRole::OnSurfaceVariant: return palette.neutralVariant.values(80, alpha); break;
		case ColorRole::Outline: return palette.neutralVariant.values(60, alpha); break;
		case ColorRole::OutlineVariant: return palette.neutralVariant.values(30, alpha); break;
		case ColorRole::Shadow: return palette.neutral.values(0, alpha); break;
		case ColorRole::Scrim: return palette.neutral.values(0, alpha); break;
		case ColorRole::InverseSurface: return palette.neutral.values(90, alpha); break;
		case ColorRole::InverseOnSurface: return palette.neutral.values(20, alpha); break;
		case ColorRole::InversePrimary: return palette.primary.values(40, alpha); break;
		case ColorRole::Undefined: break;
		case ColorRole::Max: break;
		}
		break;
	case ThemeType::Custom:
		return ColorHCT(colors[toInt(name)], alpha).data;
		break;
	}
	return ColorHCT::Values{0.0f, 50.0f, 0.0f, alpha};
}

}
