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

#include "particles/ParticleCurves.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

static constexpr uint32_t s_colorSamples = 32;
static constexpr uint32_t s_animSamples = 64;
static constexpr size_t s_maxDerivedStops = 8;

using namespace interpolation;

// clang-format off
static constexpr ParticleAnimCurveType s_animTypes[] = {
	{"none", Linear, 0, {0, 0, 0, 0}},
	{"linear", Linear, 0, {0, 0, 0, 0}},
	{"easeIn", EaseIn, 1, {2, 0, 0, 0}},
	{"easeOut", EaseOut, 1, {2, 0, 0, 0}},
	{"easeInOut", EaseInOut, 1, {2, 0, 0, 0}},
	{"sineEaseIn", SineEaseIn, 0, {0, 0, 0, 0}},
	{"sineEaseOut", SineEaseOut, 0, {0, 0, 0, 0}},
	{"quadEaseIn", QuadEaseIn, 0, {0, 0, 0, 0}},
	{"quadEaseOut", QuadEaseOut, 0, {0, 0, 0, 0}},
	{"cubicEaseInOut", CubicEaseInOut, 0, {0, 0, 0, 0}},
	{"expoEaseIn", ExpoEaseIn, 0, {0, 0, 0, 0}},
	{"circEaseOut", CircEaseOut, 0, {0, 0, 0, 0}},
	{"elasticEaseOut", ElasticEaseOut, 1, {0.3f, 0, 0, 0}},
	{"backEaseInOut", BackEaseInOut, 0, {0, 0, 0, 0}},
	{"bounceEaseOut", BounceEaseOut, 0, {0, 0, 0, 0}},
	{"bezier", Bezierat, 4, {0.25f, 0.1f, 0.25f, 1.0f}},
};
// clang-format on

Value encodeColorStops(SpanView<ParticleColorStop> stops) {
	Value ret(Value::Type::ARRAY);
	for (auto &it : stops) {
		Value stop;
		stop.setDouble(it.t, "t");
		Value color(Value::Type::ARRAY);
		color.addDouble(it.color.r);
		color.addDouble(it.color.g);
		color.addDouble(it.color.b);
		color.addDouble(it.color.a);
		stop.setValue(sp::move(color), "color");
		ret.addValue(sp::move(stop));
	}
	return ret;
}

Vector<ParticleColorStop> decodeColorStops(const Value &value) {
	Vector<ParticleColorStop> ret;
	if (!value.isArray()) {
		return ret;
	}

	for (auto &it : value.getArray()) {
		const auto &c = it.getValue("color");
		ParticleColorStop stop;
		stop.t = sprt::clamp(float(it.getDouble("t")), 0.0f, 1.0f);
		stop.color = Color4F(float(c.getDouble(0, 1.0)), float(c.getDouble(1, 1.0)),
				float(c.getDouble(2, 1.0)), float(c.getDouble(3, 1.0)));
		ret.emplace_back(stop);
	}

	sprt::stable_sort(ret.begin(), ret.end(),
			[](const ParticleColorStop &a, const ParticleColorStop &b) { return a.t < b.t; });
	return ret;
}

static Color4F sampleStops(SpanView<ParticleColorStop> stops, float t) {
	if (t <= stops.front().t) {
		return stops.front().color;
	}
	for (size_t i = 1; i < stops.size(); ++i) {
		if (t <= stops[i].t) {
			const auto &a = stops[i - 1];
			const auto &b = stops[i];
			const float f = (b.t > a.t) ? (t - a.t) / (b.t - a.t) : 1.0f;
			return a.color * (1.0f - f) + b.color * f;
		}
	}
	return stops.back().color;
}

Value makeColorCurveValue(SpanView<ParticleColorStop> stops) {
	if (stops.empty()) {
		return Value();
	}

	Value values(Value::Type::ARRAY);
	for (uint32_t i = 0; i < s_colorSamples; ++i) {
		auto c = sampleStops(stops, float(i) / float(s_colorSamples));
		values.addDouble(c.r);
		values.addDouble(c.g);
		values.addDouble(c.b);
		values.addDouble(c.a);
	}

	Value ret;
	ret.setString("vec4", "type");
	ret.setValue(sp::move(values), "values");
	return ret;
}

Vector<ParticleColorStop> deriveColorStops(const Value &curve) {
	Vector<ParticleColorStop> ret;
	if (!curve.isDictionary() || curve.getString("type") != "vec4") {
		return ret;
	}

	const auto &values = curve.getValue("values");
	const size_t n = values.size() / 4;
	if (n == 0) {
		return ret;
	}

	const size_t count = sprt::min(n, s_maxDerivedStops);
	for (size_t j = 0; j < count; ++j) {
		size_t k = (count > 1) ? (j * (n - 1)) / (count - 1) : 0;
		ParticleColorStop stop;
		stop.t = float(k) / float(n);
		stop.color = Color4F(float(values.getDouble(k * 4)), float(values.getDouble(k * 4 + 1)),
				float(values.getDouble(k * 4 + 2)), float(values.getDouble(k * 4 + 3)));
		ret.emplace_back(stop);
	}
	return ret;
}

SpanView<ParticleAnimCurveType> getParticleAnimCurveTypes() { return makeSpanView(s_animTypes); }

const ParticleAnimCurveType *getParticleAnimCurveType(StringView name) {
	for (auto &it : s_animTypes) {
		if (it.name == name) {
			return &it;
		}
	}
	return nullptr;
}

Value makeAnimCurveValue(StringView name, SpanView<float> params) {
	auto info = getParticleAnimCurveType(name);
	if (!info || info->name == "none") {
		return Value();
	}

	float p[4];
	for (uint32_t i = 0; i < 4; ++i) { p[i] = (i < params.size()) ? params[i] : info->defaults[i]; }

	Value values(Value::Type::ARRAY);
	for (uint32_t i = 0; i < s_animSamples; ++i) {
		const float t = float(i) / float(s_animSamples - 1);
		auto v = interpolation::interpolateTo(t, info->type, makeSpanView(p, info->params));
		values.addDouble(sprt::clamp(v, 0.0f, 1.0f));
	}

	Value ret;
	ret.setString("float", "type");
	ret.setValue(sp::move(values), "values");
	return ret;
}

} // namespace stappler::xenolith::examples
