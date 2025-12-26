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

#ifndef XENOLITH_RENDERER_MATERIAL2D_BASE_MATERIALSURFACESTYLE_H_
#define XENOLITH_RENDERER_MATERIAL2D_BASE_MATERIALSURFACESTYLE_H_

#include "MaterialColorScheme.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

class StyleContainer;
class SurfaceInterior;

enum class Elevation {
	Level0,	// 0dp
	Level1, // 1dp 5%
	Level2, // 3dp 8%
	Level3, // 6dp 11%
	Level4, // 8dp 12%
	Level5, // 12dp 14%
};

enum class ShapeFamily {
	RoundedCorners,
	CutCorners,
};

enum class ShapeStyle {
	None, // 0dp
	ExtraSmall, // 4dp
	Small, // 8dp
	Medium, // 12dp
	Large, // 16dp
	ExtraLarge, // 28dp
	Full // .
};

enum class NodeStyle {
	SurfaceTonal,
	SurfaceTonalElevated,
	Elevated,
	Filled,
	FilledElevated,
	FilledTonal,
	FilledTonalElevated,
	Outlined,
	Text
};

enum class ActivityState {
	Enabled,
	Disabled,
	Hovered,
	Focused,
	Pressed
};

struct SurfaceStyleData;

struct SP_PUBLIC SurfaceStyle {
	static SurfaceStyle Background;
	static constexpr uint32_t PrimarySchemeTag = maxOf<uint32_t>();

	constexpr SurfaceStyle() = default;

	template<typename ... Args>
	constexpr SurfaceStyle(Args && ... args) {
		define(std::forward<Args>(args)...);
	}

	constexpr void setup(const SurfaceStyle &value) { *this = value; }
	constexpr void setup(uint32_t value) { schemeTag = value; }
	constexpr void setup(ColorRole value) { colorRole = value; }
	constexpr void setup(Elevation value) { elevation = value; }
	constexpr void setup(ShapeFamily value) { shapeFamily = value; }
	constexpr void setup(ShapeStyle value) { shapeStyle = value; }
	constexpr void setup(NodeStyle value) { nodeStyle = value; }
	constexpr void setup(ActivityState value) { activityState = value; }

	template <typename T>
	constexpr void setup(const T &) { static_assert("Invalid type"); }


	template <typename T>
	constexpr void define(T && t) {
		setup(std::forward<T>(t));
	}

	template <typename T, typename ... Args>
	constexpr void define(T && t, Args && ... args) {
		define(std::forward<T>(t));
		define(std::forward<Args>(args)...);
	}

	bool apply(SurfaceStyleData &data, const Size2 &contentSize, const StyleContainer *, const SurfaceInterior *interior);

	constexpr bool operator==(const SurfaceStyle &) const = default;
	constexpr bool operator!=(const SurfaceStyle &) const = default;

	uint32_t schemeTag = PrimarySchemeTag;
	ColorRole colorRole = ColorRole::Primary;
	Elevation elevation = Elevation::Level0;
	ShapeFamily shapeFamily = ShapeFamily::RoundedCorners;
	ShapeStyle shapeStyle = ShapeStyle::None;
	NodeStyle nodeStyle = NodeStyle::SurfaceTonal;
	ActivityState activityState = ActivityState::Enabled;
};

struct SP_PUBLIC SurfaceStyleData {
	static SurfaceStyleData progress(const SurfaceStyleData &, const SurfaceStyleData &, float p);

	uint32_t schemeTag = SurfaceStyle::PrimarySchemeTag;
	ShapeFamily shapeFamily = ShapeFamily::RoundedCorners;
	ThemeType themeType = ThemeType::LightTheme;
	Color4F colorScheme;
	Color4F colorElevation;
	ColorHCT colorHCT;
	ColorHCT colorBackground;
	ColorHCT colorOn;
	float cornerRadius = 0.0f;
	float elevationValue = 0.0f;
	float shadowValue = 0.0f;
	float outlineValue = 0.0f;

	bool operator==(const SurfaceStyleData &) const = default;
	bool operator!=(const SurfaceStyleData &) const = default;
};

}

namespace STAPPLER_VERSIONIZED stappler {

inline auto progress(const xenolith::material2d::SurfaceStyleData &l, const xenolith::material2d::SurfaceStyleData &r, float p) {
	return xenolith::material2d::SurfaceStyleData::progress(l, r, p);
}

}

#endif /* XENOLITH_RENDERER_MATERIAL2D_BASE_MATERIALSURFACESTYLE_H_ */
