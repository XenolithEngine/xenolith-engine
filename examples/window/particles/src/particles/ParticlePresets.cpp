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

#include "particles/ParticlePresets.h"
#include "XL2dParticleSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

static constexpr StringView s_presetNames[] = {"fountain", "snow", "vortex"};

// Angles in radians, distances in dp, times in seconds; the emitter sits at the origin
static constexpr StringView s_fountain = R"json({
	"count": 160,
	"lifetime": {"min": 1.6, "max": 2.2},
	"randomness": 1,
	"particleSize": [12, 12],
	"normal": {"min": 1.37, "max": 1.77},
	"velocity": {"min": 320, "max": 400},
	"linearAcceleration": {"min": [0, -400], "max": [0, -400]},
	"scale": {"min": 0.6, "max": 1.2}
})json";

static constexpr StringView s_snow = R"json({
	"count": 120,
	"lifetime": {"min": 6, "max": 6},
	"randomness": 1,
	"particleSize": [10, 10],
	"normal": {"min": -1.87, "max": -1.27},
	"velocity": {"min": 30, "max": 60},
	"linearAcceleration": {"min": [0, -8], "max": [0, -8]},
	"angularVelocity": {"min": -2, "max": 2},
	"scale": {"min": 0.5, "max": 1.2}
})json";

static constexpr StringView s_vortex = R"json({
	"count": 240,
	"lifetime": {"min": 3, "max": 3},
	"randomness": 1,
	"particleSize": [10, 10],
	"orbitalVelocity": {"min": 1.5, "max": 2.5},
	"radialVelocity": {"min": -70, "max": -50},
	"scale": {"min": 0.5, "max": 1}
})json";

static Value makePoints(uint32_t count, const Callback<Vec2(uint32_t)> &cb) {
	Value ret(Value::Type::ARRAY);
	for (uint32_t i = 0; i < count; ++i) {
		auto p = cb(i);
		Value point(Value::Type::ARRAY);
		point.addDouble(p.x);
		point.addDouble(p.y);
		ret.addValue(sp::move(point));
	}
	return ret;
}

SpanView<StringView> getParticlePresetNames() { return makeSpanView(s_presetNames); }

Value getParticlePreset(StringView name) {
	Value preset;
	if (name == "fountain") {
		preset = data::read<Interface>(s_fountain);
	} else if (name == "snow") {
		preset = data::read<Interface>(s_snow);
		// A line of points above the emitter
		preset.setValue(
				makePoints(13, [](uint32_t i) { return Vec2(-480.0f + 80.0f * i, 280.0f); }),
				"emissionPoints");
	} else if (name == "vortex") {
		preset = data::read<Interface>(s_vortex);
		// A ring around the emitter
		preset.setValue(makePoints(16,
								[](uint32_t i) {
			auto a = float(M_PI * 2.0) * i / 16.0f;
			return Vec2(sprt::cos(a) * 220.0f, sprt::sin(a) * 220.0f);
		}),
				"emissionPoints");
	} else {
		return Value();
	}

	if (!preset.isDictionary()) {
		return Value();
	}

	auto ret = Rc<basic2d::ParticleSystem>::create()->encode();
	const Value &src = preset;
	for (auto &it : src.getDict()) { ret.setValue(it.second, it.first); }
	return ret;
}

} // namespace stappler::xenolith::examples
