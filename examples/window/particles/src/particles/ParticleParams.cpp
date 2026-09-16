/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons whom the Software is
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

#include "XLCommon.h" // IWYU pragma: keep

#include "particles/ParticleParams.h"
#include "particles/ParticleCurves.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

using Kind = ParticleParamKind;

static constexpr double s_deg = 180.0 / M_PI;

// clang-format off
static constexpr ParticleParamInfo s_params[] = {
	{"count", "emission", "@Locale:Particles:Count", Kind::Int, 1, 4096, 1, 1, ""},
	{"explosiveness", "emission", "@Locale:Particles:Explosiveness", Kind::Unit, 0, 1, 0.01, 1, ""},
	{"randomness", "emission", "@Locale:Particles:Randomness", Kind::Unit, 0, 1, 0.01, 1, ""},
	{"origin", "emission", "@Locale:Particles:Origin", Kind::Vec2, -2000, 2000, 1, 1, "dp"},

	{"lifetime", "time", "@Locale:Particles:Lifetime", Kind::Range, 0.05, 30, 0.05, 1, "s"},
	{"frameInterval", "time", "@Locale:Particles:Fps", Kind::Fps, 1, 240, 1, 1, "/s"},
	{"seed", "time", "@Locale:Particles:Seed", Kind::Seed, 0, 1000000, 1, 1, ""},
	{"useLifetimeMax", "time", "@Locale:Particles:UseLifetimeMax", Kind::Flag, 0, 1, 1, 1, ""},

	{"normal", "direction", "@Locale:Particles:Direction", Kind::Direction, -360, 360, 1, s_deg, "°"},
	{"velocity", "direction", "@Locale:Particles:Velocity", Kind::Range, -2000, 2000, 5, 1, "dp/s"},
	{"linearVelocity", "direction", "@Locale:Particles:LinearVelocity", Kind::RangeVec2, -2000, 2000, 5, 1, "dp/s"},

	{"linearAcceleration", "acceleration", "@Locale:Particles:Gravity", Kind::RangeVec2, -2000, 2000, 5, 1, "dp/s²"},
	{"acceleration", "acceleration", "@Locale:Particles:Acceleration", Kind::Range, -2000, 2000, 5, 1, "dp/s²"},
	{"radialAcceleration", "acceleration", "@Locale:Particles:RadialAcceleration", Kind::Range, -2000, 2000, 5, 1, "dp/s²"},
	{"tangentialAcceleration", "acceleration", "@Locale:Particles:TangentialAcceleration", Kind::Range, -2000, 2000, 5, 1, "dp/s²"},
	{"orbitalVelocity", "acceleration", "@Locale:Particles:OrbitalVelocity", Kind::Range, -1440, 1440, 5, s_deg, "°/s"},
	{"radialVelocity", "acceleration", "@Locale:Particles:RadialVelocity", Kind::Range, -2000, 2000, 5, 1, "dp/s"},

	{"angle", "rotation", "@Locale:Particles:Angle", Kind::Range, -360, 360, 1, s_deg, "°"},
	{"angularVelocity", "rotation", "@Locale:Particles:AngularVelocity", Kind::Range, -1440, 1440, 5, s_deg, "°/s"},
	{"scale", "rotation", "@Locale:Particles:Scale", Kind::Range, 0, 10, 0.05, 1, ""},
	{"particleSize", "rotation", "@Locale:Particles:ParticleSize", Kind::Vec2, 0, 512, 1, 1, "dp"},
	{"alignWithVelocity", "rotation", "@Locale:Particles:AlignWithVelocity", Kind::Flag, 0, 1, 1, 1, ""},

	{"color", "color", "@Locale:Particles:Color", Kind::Color, 0, 1, 0.01, 1, ""},
	{"hue", "color", "@Locale:Particles:Hue", Kind::Range, -1, 1, 0.01, 1, ""},

	{"colorStops", "colorCurve", "@Locale:Particles:ColorCurve", Kind::ColorStops, 0, 1, 0.01, 1, ""},

	{"texture", "texture", "@Locale:Particles:Texture", Kind::Texture, 0, 3, 1, 1, ""},
	{"frameGrid", "texture", "@Locale:Particles:FrameGrid", Kind::FrameGrid, 1, 16, 1, 1, ""},
	{"animCurve", "texture", "@Locale:Particles:AnimCurve", Kind::AnimCurve, -10, 10, 0.05, 1, ""},

	{"localCoords", "flags", "@Locale:Particles:LocalCoords", Kind::Flag, 0, 1, 1, 1, ""},
	{"orderByLifetime", "flags", "@Locale:Particles:OrderByLifetime", Kind::Flag, 0, 1, 1, 1, ""},
};
// clang-format on

static constexpr StringView s_sections[] = {"emission", "time", "direction", "acceleration",
	"rotation", "color", "colorCurve", "texture", "flags"};

static constexpr StringView s_sectionTitles[] = {"@Locale:Particles:Section:Emission",
	"@Locale:Particles:Section:Time", "@Locale:Particles:Section:Direction",
	"@Locale:Particles:Section:Acceleration", "@Locale:Particles:Section:Rotation",
	"@Locale:Particles:Section:Color", "@Locale:Particles:Section:ColorCurve",
	"@Locale:Particles:Section:Texture", "@Locale:Particles:Section:Flags"};

static constexpr StringView s_textures[] = {"circle", "square", "spark", "frames"};

SpanView<ParticleParamInfo> getParticleParams() { return makeSpanView(s_params); }

const ParticleParamInfo *getParticleParam(StringView key) {
	for (auto &it : s_params) {
		if (it.key == key) {
			return &it;
		}
	}
	return nullptr;
}

SpanView<StringView> getParticleSections() { return makeSpanView(s_sections); }

StringView getParticleSectionTitle(StringView section) {
	for (size_t i = 0; i < sizeof(s_sections) / sizeof(s_sections[0]); ++i) {
		if (s_sections[i] == section) {
			return s_sectionTitles[i];
		}
	}
	return StringView();
}

SpanView<StringView> getParticleTextureNames() { return makeSpanView(s_textures); }

static bool hasFlag(const Value &system, StringView name) {
	for (auto &it : system.getArray("flags")) {
		if (it.getString() == name) {
			return true;
		}
	}
	return false;
}

static Value makeArray(std::initializer_list<double> values) {
	Value ret(Value::Type::ARRAY);
	for (auto v : values) { ret.addDouble(v); }
	return ret;
}

// Radians shown in degrees and float32 storage leave long tails; the panel shows 1e-4
static double roundUi(double v) { return sprt::round(v * 10'000.0) / 10'000.0; }

static Vector<double> particleParamToUiRaw(const ParticleParamInfo &info, const Value &system,
		const Value &node);

static Vector<double> particleEditorToUi(const ParticleParamInfo &info, const Value &editor) {
	Vector<double> ret;
	if (info.kind == Kind::ColorStops) {
		for (auto &it : decodeColorStops(editor.getValue("colorStops"))) {
			ret.emplace_back(it.t);
			ret.emplace_back(it.color.r);
			ret.emplace_back(it.color.g);
			ret.emplace_back(it.color.b);
			ret.emplace_back(it.color.a);
		}
	} else {
		const auto &curve = editor.getValue("animCurve");
		auto types = getParticleAnimCurveTypes();
		auto name = curve.getString("type");
		size_t index = 0;
		for (size_t i = 0; i < types.size(); ++i) {
			if (types[i].name == name) {
				index = i;
			}
		}
		ret.emplace_back(double(index));
		const auto &params = curve.getValue("params");
		for (uint32_t i = 0; i < 4; ++i) {
			ret.emplace_back(params.getDouble(i, types[index].defaults[i]));
		}
	}
	return ret;
}

Vector<double> particleParamToUi(const ParticleParamInfo &info, const Value &system,
		const Value &node, const Value &editor) {
	auto ret = (info.kind == Kind::ColorStops || info.kind == Kind::AnimCurve)
			? particleEditorToUi(info, editor)
			: particleParamToUiRaw(info, system, node);
	for (auto &it : ret) { it = roundUi(it); }
	return ret;
}

static Vector<double> particleParamToUiRaw(const ParticleParamInfo &info, const Value &system,
		const Value &node) {
	const auto &v = system.getValue(info.key);
	switch (info.kind) {
	case Kind::Int:
	case Kind::Unit: return Vector<double>{v.getDouble() * info.scale};
	case Kind::Range:
		return Vector<double>{v.getDouble("min") * info.scale, v.getDouble("max") * info.scale};
	case Kind::RangeVec2: {
		const auto &min = v.getValue("min");
		const auto &max = v.getValue("max");
		return Vector<double>{min.getDouble(0), min.getDouble(1), max.getDouble(0),
			max.getDouble(1)};
	}
	case Kind::Vec2: return Vector<double>{v.getDouble(0), v.getDouble(1)};
	case Kind::Color:
		return Vector<double>{v.getDouble(0, 1.0), v.getDouble(1, 1.0), v.getDouble(2, 1.0),
			v.getDouble(3, 1.0)};
	case Kind::Flag: return Vector<double>{hasFlag(system, info.key) ? 1.0 : 0.0};
	case Kind::Direction: {
		auto min = v.getDouble("min") * info.scale;
		auto max = v.getDouble("max") * info.scale;
		return Vector<double>{(min + max) / 2.0, (max - min) / 2.0};
	}
	case Kind::Fps: {
		auto interval = system.getInteger("frameInterval", 16'667);
		return Vector<double>{sprt::round(1'000'000.0 / double(sprt::max(interval, int64_t(1))))};
	}
	case Kind::Seed:
		return Vector<double>{system.isInteger("seed") ? 1.0 : 0.0,
			double(system.getInteger("seed"))};
	case Kind::Texture: {
		const auto &name = node.getString("texture");
		for (size_t i = 0; i < sizeof(s_textures) / sizeof(s_textures[0]); ++i) {
			if (s_textures[i] == name) {
				return Vector<double>{double(i)};
			}
		}
		return Vector<double>{0.0};
	}
	case Kind::FrameGrid: {
		const auto &grid = node.getValue("frameGrid");
		return Vector<double>{double(grid.getInteger(0, 1)), double(grid.getInteger(1, 1))};
	}
	case Kind::ColorStops:
	case Kind::AnimCurve: break;
	}
	return Vector<double>();
}

Value particleParamFromUi(const ParticleParamInfo &info, SpanView<double> ui, const Value &system) {
	auto at = [&](size_t i) { return i < ui.size() ? ui[i] : 0.0; };

	Value ret;
	switch (info.kind) {
	case Kind::Int: ret.setInteger(int64_t(sprt::round(at(0))), info.key); break;
	case Kind::Unit: ret.setDouble(at(0) / info.scale, info.key); break;
	case Kind::Range: {
		Value range;
		range.setDouble(at(0) / info.scale, "min");
		range.setDouble(at(1) / info.scale, "max");
		ret.setValue(sp::move(range), info.key);
		break;
	}
	case Kind::RangeVec2: {
		Value range;
		range.setValue(makeArray({at(0), at(1)}), "min");
		range.setValue(makeArray({at(2), at(3)}), "max");
		ret.setValue(sp::move(range), info.key);
		break;
	}
	case Kind::Vec2: ret.setValue(makeArray({at(0), at(1)}), info.key); break;
	case Kind::Color: ret.setValue(makeArray({at(0), at(1), at(2), at(3)}), info.key); break;
	case Kind::Flag: {
		Value flags(Value::Type::ARRAY);
		for (auto &it : system.getArray("flags")) {
			if (it.getString() != info.key) {
				flags.addValue(it);
			}
		}
		if (at(0) != 0.0) {
			flags.addString(info.key);
		}
		ret.setValue(sp::move(flags), "flags");
		break;
	}
	case Kind::Direction: {
		Value range;
		range.setDouble((at(0) - at(1)) / info.scale, "min");
		range.setDouble((at(0) + at(1)) / info.scale, "max");
		ret.setValue(sp::move(range), info.key);
		break;
	}
	case Kind::Fps:
		ret.setInteger(int64_t(sprt::round(1'000'000.0 / sprt::max(at(0), 1.0))), info.key);
		break;
	case Kind::Seed:
		if (at(0) != 0.0) {
			ret.setInteger(int64_t(sprt::round(at(1))), info.key);
		} else {
			ret.setValue(Value(), info.key);
		}
		break;
	case Kind::Texture: {
		auto index = size_t(sprt::clamp(sprt::round(at(0)), 0.0, 3.0));
		ret.setString(s_textures[index], info.key);
		break;
	}
	case Kind::FrameGrid:
		ret.setValue(
				makeArray({sprt::max(sprt::round(at(0)), 1.0), sprt::max(sprt::round(at(1)), 1.0)}),
				info.key);
		break;
	case Kind::ColorStops: {
		Vector<ParticleColorStop> stops;
		for (size_t i = 0; i + 4 < ui.size(); i += 5) {
			stops.emplace_back(ParticleColorStop{float(ui[i]),
				Color4F(float(ui[i + 1]), float(ui[i + 2]), float(ui[i + 3]), float(ui[i + 4]))});
		}
		ret.setValue(encodeColorStops(stops), info.key);
		break;
	}
	case Kind::AnimCurve: {
		auto types = getParticleAnimCurveTypes();
		auto index = size_t(sprt::clamp(sprt::round(at(0)), 0.0, double(types.size() - 1)));
		Value curve;
		curve.setString(types[index].name, "type");
		Value params(Value::Type::ARRAY);
		for (uint32_t i = 0; i < types[index].params; ++i) { params.addDouble(at(1 + i)); }
		curve.setValue(sp::move(params), "params");
		ret.setValue(sp::move(curve), info.key);
		break;
	}
	}
	return ret;
}

} // namespace stappler::xenolith::examples
