/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "MaterialSurfaceStyle.h"
#include "MaterialStyleContainer.h"
#include "MaterialSurfaceInterior.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

SurfaceStyle SurfaceStyle::Background = SurfaceStyle{SurfaceStyle::PrimarySchemeTag, ColorRole::Background, Elevation::Level0};

bool SurfaceStyle::apply(SurfaceStyleData &data, const Size2 &contentSize, const StyleContainer *style, const SurfaceInterior *interior) {
	bool dirty = false;

	if (data.schemeTag != schemeTag) {
		data.schemeTag = schemeTag;
		dirty = true;
	}

	// HCT resolve is expensive, compare only HCT values before it
	ColorHCT::Values targetColorHCT, targetColorBackground, targetColorOn;

	float targetElevationValue = 0.0f;
	float targetShadowValue = 0.0f;
	float targetOutlineValue = 0.0f;

	const ColorScheme *scheme = nullptr;

	if (auto s = style->getScheme(schemeTag)) {
		scheme = s;
	} else {
		scheme = &style->getPrimaryScheme();
	}

	data.themeType = scheme->type;

	bool hasShadow = false;
	bool hasBlendElevation = false;

	auto targetElevation = elevation;

	switch (nodeStyle) {
	case NodeStyle::SurfaceTonal:
		targetColorHCT = scheme->values(colorRole);
		targetColorBackground = scheme->values(ColorRole::Surface);
		targetColorOn = scheme->values(ColorScheme::getColorRoleOn(ColorRole::Surface, scheme->type));
		hasBlendElevation = true;
		switch (activityState) {
		case ActivityState::Enabled: break;
		case ActivityState::Disabled:
			targetColorHCT = scheme->values(ColorRole::OnSurface);
			targetColorBackground = scheme->values(ColorRole::Surface);
			targetColorOn = scheme->values(ColorRole::OnSurface, 0.34f);
			targetElevationValue = 0.12f;
			targetElevation = Elevation(toInt(targetElevation) - 1);
			break;
		case ActivityState::Hovered:
			targetElevation = Elevation(toInt(targetElevation) + 1);
			targetElevationValue = 0.08f;
			break;
		case ActivityState::Focused:
			targetElevationValue = 0.12f;
			break;
		case ActivityState::Pressed:
			targetElevationValue = 0.12f;
			break;
		}
		break;
	case NodeStyle::SurfaceTonalElevated:
		targetColorHCT = scheme->values(colorRole);
		targetColorBackground = scheme->values(ColorRole::Surface);
		targetColorOn = scheme->values(ColorScheme::getColorRoleOn(ColorRole::Surface, scheme->type));
		hasBlendElevation = hasShadow = true;
		switch (activityState) {
		case ActivityState::Enabled: break;
		case ActivityState::Disabled:
			targetColorHCT = scheme->values(ColorRole::OnSurface);
			targetColorBackground = scheme->values(ColorRole::Surface);
			targetColorOn = scheme->values(ColorRole::OnSurface, 0.34f);
			targetElevation = Elevation(toInt(targetElevation) - 1);
			targetElevationValue = 0.12f;
			break;
		case ActivityState::Hovered:
			targetElevation = Elevation(toInt(targetElevation) + 1);
			targetElevationValue = 0.08f;
			break;
		case ActivityState::Focused:
			targetElevationValue = 0.12f;
			break;
		case ActivityState::Pressed:
			targetElevationValue = 0.12f;
			break;
		}
		break;
	case NodeStyle::Elevated:
		targetColorHCT = scheme->values(ColorRole::Surface);
		targetColorBackground = scheme->values(ColorRole::Surface);
		targetColorOn = scheme->values(colorRole);
		switch (activityState) {
		case ActivityState::Enabled: break;
		case ActivityState::Disabled:
			targetColorHCT = scheme->values(ColorRole::OnSurface);
			targetColorBackground = scheme->values(ColorRole::Surface);
			targetColorOn = scheme->values(ColorRole::OnSurface, 0.34f);
			targetElevation = Elevation(toInt(targetElevation) - 1);
			targetElevationValue = 0.12f;
			break;
		case ActivityState::Hovered:
			targetColorHCT = scheme->values(ColorRole::Primary);
			targetElevation = Elevation(toInt(targetElevation) + 1);
			targetElevationValue = 0.08f;
			break;
		case ActivityState::Focused:
			targetColorHCT = scheme->values(ColorRole::Primary);
			targetElevationValue = 0.12f;
			break;
		case ActivityState::Pressed:
			targetColorHCT = scheme->values(ColorRole::Primary);
			targetElevationValue = 0.12f;
			break;
		}
		hasShadow = true;
		break;
	case NodeStyle::Filled:
		targetColorHCT = scheme->values(colorRole);
		targetColorBackground = scheme->values(colorRole);
		targetColorOn = scheme->values(ColorScheme::getColorRoleOn(colorRole, scheme->type));
		switch (activityState) {
		case ActivityState::Enabled: break;
		case ActivityState::Disabled:
			targetColorHCT = scheme->values(ColorRole::OnSurface);
			targetColorBackground = scheme->values(ColorRole::Surface);
			targetColorOn = scheme->values(ColorRole::OnSurface, 0.34f);
			targetElevationValue = 0.12f;
			break;
		case ActivityState::Hovered:
			targetColorHCT = scheme->values(ColorScheme::getColorRoleOn(colorRole, scheme->type));
			targetElevation = Elevation(toInt(targetElevation) + 1);
			targetElevationValue = 0.08f;
			hasShadow = true;
			break;
		case ActivityState::Focused:
			targetColorHCT = scheme->values(ColorScheme::getColorRoleOn(colorRole, scheme->type));
			targetElevationValue = 0.12f;
			break;
		case ActivityState::Pressed:
			targetColorHCT = scheme->values(ColorScheme::getColorRoleOn(colorRole, scheme->type));
			targetElevationValue = 0.12f;
			break;
		}
		break;
	case NodeStyle::FilledElevated:
		targetColorHCT = scheme->values(colorRole);
		targetColorBackground = scheme->values(colorRole);
		targetColorOn = scheme->values(ColorScheme::getColorRoleOn(colorRole, scheme->type));
		hasShadow = true;
		switch (activityState) {
		case ActivityState::Enabled: break;
		case ActivityState::Disabled:
			targetColorHCT = scheme->values(ColorRole::OnSurface);
			targetColorBackground = scheme->values(ColorRole::Surface);
			targetColorOn = scheme->values(ColorRole::OnSurface, 0.34f);
			targetElevation = Elevation(toInt(targetElevation) - 1);
			targetElevationValue = 0.12f;
			break;
		case ActivityState::Hovered:
			targetColorHCT = scheme->values(ColorScheme::getColorRoleOn(colorRole, scheme->type));
			targetElevation = Elevation(toInt(targetElevation) + 1);
			targetElevationValue = 0.08f;
			break;
		case ActivityState::Focused:
			targetColorHCT = scheme->values(ColorScheme::getColorRoleOn(colorRole, scheme->type));
			targetElevationValue = 0.12f;
			break;
		case ActivityState::Pressed:
			targetColorHCT = scheme->values(ColorScheme::getColorRoleOn(colorRole, scheme->type));
			targetElevationValue = 0.12f;
			break;
		}
		break;
	case NodeStyle::FilledTonal:
		targetColorHCT = scheme->values(ColorRole::SecondaryContainer);
		targetColorBackground = scheme->values(ColorRole::SecondaryContainer);
		targetColorOn = scheme->values(ColorScheme::getColorRoleOn(ColorRole::SecondaryContainer, scheme->type));
		switch (activityState) {
		case ActivityState::Enabled: break;
		case ActivityState::Disabled:
			targetColorHCT = scheme->values(ColorRole::OnSurface);
			targetColorBackground = scheme->values(ColorRole::Surface);
			targetColorOn = scheme->values(ColorRole::OnSurface, 0.34f);
			targetElevationValue = 0.12f;
			targetElevation = Elevation(toInt(targetElevation) - 1);
			break;
		case ActivityState::Hovered:
			targetColorHCT = scheme->values(ColorRole::OnSecondaryContainer);
			targetElevation = Elevation(toInt(targetElevation) + 1);
			targetElevationValue = 0.08f;
			break;
		case ActivityState::Focused:
			targetColorHCT = scheme->values(ColorRole::OnSecondaryContainer);
			targetElevationValue = 0.12f;
			break;
		case ActivityState::Pressed:
			targetColorHCT = scheme->values(ColorRole::OnSecondaryContainer);
			targetElevationValue = 0.12f;
			break;
		}
		break;
	case NodeStyle::FilledTonalElevated:
		targetColorHCT = scheme->values(ColorRole::SecondaryContainer);
		targetColorBackground = scheme->values(ColorRole::SecondaryContainer);
		targetColorOn = scheme->values(ColorScheme::getColorRoleOn(ColorRole::SecondaryContainer, scheme->type));
		hasShadow = true;
		switch (activityState) {
		case ActivityState::Enabled: break;
		case ActivityState::Disabled:
			targetColorHCT = scheme->values(ColorRole::OnSurface);
			targetColorBackground = scheme->values(ColorRole::Surface);
			targetColorOn = scheme->values(ColorRole::OnSurface, 0.34f);
			targetElevation = Elevation(toInt(targetElevation) - 1);
			targetElevationValue = 0.12f;
			break;
		case ActivityState::Hovered:
			targetColorHCT = scheme->values(ColorRole::OnSecondaryContainer);
			targetElevation = Elevation(toInt(targetElevation) + 1);
			targetElevationValue = 0.08f;
			break;
		case ActivityState::Focused:
			targetColorHCT = scheme->values(ColorRole::OnSecondaryContainer);
			targetElevationValue = 0.12f;
			break;
		case ActivityState::Pressed:
			targetColorHCT = scheme->values(ColorRole::OnSecondaryContainer);
			targetElevationValue = 0.12f;
			break;
		}
		break;
	case NodeStyle::Outlined:
		targetColorHCT = scheme->values(ColorRole::Outline, 0.0f);
		targetColorBackground = scheme->values(ColorRole::Outline, 0.0f);
		targetColorOn = interior ? interior->getStyle().colorOn.data : scheme->values(colorRole);
		targetOutlineValue = 1.0f;
		switch (activityState) {
		case ActivityState::Enabled: break;
		case ActivityState::Disabled:
			targetColorHCT = scheme->values(ColorRole::OnSurface, 0.0f);
			targetColorBackground = scheme->values(ColorRole::Surface, 0.0f);
			targetColorOn = scheme->values(ColorRole::OnSurface, 0.34f);
			targetElevation = Elevation(toInt(targetElevation) - 1);
			targetElevationValue = 0.12f;
			break;
		case ActivityState::Hovered:
			targetElevationValue = 0.08f;
			targetColorHCT = scheme->values(colorRole, 1.0f);
			targetElevation = Elevation(toInt(targetElevation) + 1);
			break;
		case ActivityState::Focused:
			targetElevationValue = 0.12f;
			targetColorHCT = scheme->values(colorRole, 1.0f);
			break;
		case ActivityState::Pressed:
			targetElevationValue = 0.12f;
			targetColorHCT = scheme->values(colorRole, 1.0f);
			break;
		}
		break;
	case NodeStyle::Text:
		targetColorHCT = scheme->values(ColorRole::Surface, 0.0f);
		targetColorBackground = scheme->values(colorRole, 0.0f);
		targetColorOn = interior ? interior->getStyle().colorOn.data : scheme->values(colorRole);
		switch (activityState) {
		case ActivityState::Enabled: break;
		case ActivityState::Disabled:
			targetColorOn = scheme->values(ColorRole::OnSurface, 0.34f);
			targetElevation = Elevation(toInt(targetElevation) - 1);
			break;
		case ActivityState::Hovered:
			targetElevationValue = 0.08f;
			targetColorHCT = scheme->values(ColorRole::Surface, 1.0f);
			targetColorHCT = scheme->values(colorRole, 1.0f);
			targetElevation = Elevation(toInt(targetElevation) + 1);
			break;
		case ActivityState::Focused:
			targetElevationValue = 0.12f;
			targetColorHCT = scheme->values(ColorRole::Surface, 1.0f);
			targetColorHCT = scheme->values(colorRole, 1.0f);
			break;
		case ActivityState::Pressed:
			targetElevationValue = 0.12f;
			targetColorHCT = scheme->values(ColorRole::Surface, 1.0f);
			targetColorHCT = scheme->values(colorRole, 1.0f);
			break;
		}
		break;
	}

	switch (activityState) {
	case ActivityState::Focused:
	case ActivityState::Hovered:
	case ActivityState::Pressed:
		targetColorOn.alpha = 1.0f;
		break;
	case ActivityState::Disabled:
		targetColorOn.alpha = 0.34f;
		break;
	default:
		targetColorOn.alpha = 0.85f;
		break;
	}

	if (targetColorHCT != data.colorHCT.data) {
		data.colorHCT = targetColorHCT;
		data.colorScheme = data.colorHCT.asColor4F();
		dirty = true;
	}
	if (targetColorBackground != data.colorBackground.data) {
		data.colorBackground = targetColorBackground;
		dirty = true;
	}
	if (targetOutlineValue != data.outlineValue) {
		data.outlineValue = targetOutlineValue;
		dirty = true;
	}

	if (hasBlendElevation) {
		switch (targetElevation) {
		case Elevation::Level0: targetElevationValue += 0.0f; break;	// 0dp
		case Elevation::Level1: targetElevationValue += 0.05f; break; // 1dp 5%
		case Elevation::Level2: targetElevationValue += 0.08f; break; // 3dp 8%
		case Elevation::Level3: targetElevationValue += 0.11f; break; // 6dp 11%
		case Elevation::Level4: targetElevationValue += 0.12f; break; // 8dp 12%
		case Elevation::Level5: targetElevationValue += 0.14f; break; // 12dp 14%
		}
	}

	if (hasShadow) {
		switch (targetElevation) {
		case Elevation::Level0: targetShadowValue = 0.0f; break; // 0dp
		case Elevation::Level1: targetShadowValue = 1.75f; break; // 1dp 5%
		case Elevation::Level2: targetShadowValue = 3.0f; break; // 3dp 8%
		case Elevation::Level3: targetShadowValue = 5.0f; break; // 6dp 11%
		case Elevation::Level4: targetShadowValue = 7.0f; break; // 8dp 12%
		case Elevation::Level5: targetShadowValue = 9.0f; break; // 12dp 14%
		default: targetShadowValue = 10.0f + 3.0f * (toInt(targetElevation) - 5); break;
		}
	}

	if (targetElevationValue != data.elevationValue) {
		data.elevationValue = targetElevationValue;
		dirty = true;
	}

	if (dirty) {
		data.colorElevation = data.colorBackground.asColor4F() * (1.0f - data.elevationValue) + data.colorScheme * data.elevationValue;
	}

	if (targetColorOn != data.colorOn.data) {
		data.colorOn = targetColorOn;
		dirty = true;
	}

	if (targetShadowValue != data.shadowValue) {
		data.shadowValue = targetShadowValue;
		dirty = true;
	}

	float targetCornerRadius = 0.0f;

	switch (shapeStyle) {
	case ShapeStyle::None: targetCornerRadius = 0.0f; break; // 0dp
	case ShapeStyle::ExtraSmall: targetCornerRadius = 4.0f; break; // 4dp
	case ShapeStyle::Small: targetCornerRadius = 8.0f; break; // 8dp
	case ShapeStyle::Medium: targetCornerRadius = 12.0f; break; // 12dp
	case ShapeStyle::Large: targetCornerRadius = 16.0f; break; // 16dp
	case ShapeStyle::ExtraLarge: targetCornerRadius = 28.0f; break; // 28dp
	case ShapeStyle::Full: targetCornerRadius = std::min(contentSize.width, contentSize.height) / 2.0f; break;
	}

	if (data.shapeFamily != shapeFamily) {
		data.shapeFamily = shapeFamily;
		dirty = true;
	}

	if (targetCornerRadius != data.cornerRadius) {
		data.cornerRadius = targetCornerRadius;
		dirty = true;
	}

	return dirty;
}

SurfaceStyleData SurfaceStyleData::progress(const SurfaceStyleData &l, const SurfaceStyleData &r, float p) {
	SurfaceStyleData ret(r);
	ret.schemeTag =  (p < 0.5f) ? l.schemeTag : r.schemeTag;
	ret.colorHCT = stappler::progress(l.colorHCT, r.colorHCT, p);
	ret.colorBackground = stappler::progress(l.colorBackground, r.colorBackground, p);
	ret.colorOn = stappler::progress(l.colorOn, r.colorOn, p);

	ret.colorScheme = ret.colorHCT;
	ret.elevationValue = stappler::progress(l.elevationValue, r.elevationValue, p);
	ret.outlineValue = stappler::progress(l.outlineValue, r.outlineValue, p);
	if (ret.outlineValue < 0.1f) {
		ret.outlineValue = 0.0f;
	}

	ret.colorElevation = ret.colorBackground.asColor4F() * (1.0f - ret.elevationValue) + ret.colorScheme * ret.elevationValue;

	if (l.shapeFamily == r.shapeFamily) {
		ret.cornerRadius = stappler::progress(l.cornerRadius, r.cornerRadius, p);
	} else {
		auto scale = l.cornerRadius / (l.cornerRadius + r.cornerRadius);
		if (p < scale) {
			ret.cornerRadius = stappler::progress(l.cornerRadius, 0.0f, p / scale);
		} else {
			ret.cornerRadius = stappler::progress(0.0f, r.cornerRadius, p / (1.0f - scale));
		}
	}

	ret.shadowValue = stappler::progress(l.shadowValue, r.shadowValue, p);
	return ret;
}

}
