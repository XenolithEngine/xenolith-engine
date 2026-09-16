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

#ifndef EXAMPLES_WINDOW_PARTICLES_SRC_PARTICLES_PARTICLECURVES_H_
#define EXAMPLES_WINDOW_PARTICLES_SRC_PARTICLES_PARTICLECURVES_H_

#include "XLCommon.h" // IWYU pragma: keep
#include "XLInterpolation.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

/* The editable form of the two curves, kept by the example beside ParticleSystem::encode:

	colorStops: [{t, color: [r, g, b, a]}, ...]    empty - no color curve
	animCurve:  {type, params: [...]}              type "none" - no frame curve

A system only stores sampled curves, so the editors keep these and resample on every change. */
struct ParticleColorStop {
	float t = 0.0f;
	Color4F color = Color4F::WHITE;
};

struct ParticleAnimCurveType {
	StringView name;
	interpolation::Type type;
	uint32_t params;
	float defaults[4];
};

Value encodeColorStops(SpanView<ParticleColorStop>);

// Sorted by t, clamped to [0, 1]
Vector<ParticleColorStop> decodeColorStops(const Value &);

// A vec4 curve value for ParticleSystem::apply: samples at i / n, linear between the stops
Value makeColorCurveValue(SpanView<ParticleColorStop>);

// Up to 8 stops at evenly spaced samples of a curve that came without stops
Vector<ParticleColorStop> deriveColorStops(const Value &curve);

// "none" first, then the interpolation types an animation curve can use
SpanView<ParticleAnimCurveType> getParticleAnimCurveTypes();
const ParticleAnimCurveType *getParticleAnimCurveType(StringView name);

// A float curve value from 0 to 1 over the lifetime, the last sample exactly 1; null for "none"
Value makeAnimCurveValue(StringView type, SpanView<float> params);

} // namespace stappler::xenolith::examples

#endif /* EXAMPLES_WINDOW_PARTICLES_SRC_PARTICLES_PARTICLECURVES_H_ */
