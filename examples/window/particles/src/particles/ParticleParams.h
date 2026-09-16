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

#ifndef EXAMPLES_WINDOW_PARTICLES_SRC_PARTICLES_PARTICLEPARAMS_H_
#define EXAMPLES_WINDOW_PARTICLES_SRC_PARTICLES_PARTICLEPARAMS_H_

#include "XLCommon.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

/* The parameters the panel edits, one table for building the panel, reading values into it and
turning its values back into a patch for ParticleSystem::apply.

A parameter's UI values are a list of doubles in the units the panel shows (degrees, steps per
second, turns); `particleParamToUi` and `particleParamFromUi` are the only places that convert. */
enum class ParticleParamKind {
	Int, // [v]
	Unit, // [v], 0..1 with a slider
	Range, // [min, max]
	RangeVec2, // [minX, minY, maxX, maxY]
	Vec2, // [x, y]
	Color, // [r, g, b, a], 0..1
	Flag, // [0 | 1], an element of `flags`
	Direction, // [direction, spread] in degrees, the `normal` range
	Fps, // [steps per second], `frameInterval`
	Seed, // [fixed 0 | 1, seed]
	Texture, // [index into getParticleTextureNames], a node property
	FrameGrid, // [h, v], a node property
	ColorStops, // [t, r, g, b, a, ...], editor data
	AnimCurve, // [type index, p0, p1, p2, p3], editor data
};

struct ParticleParamInfo {
	StringView key; // the encode() key; the flag name for Flag
	StringView section;
	StringView title; // locale tag
	ParticleParamKind kind;
	double min;
	double max;
	double step;
	double scale; // UI value = stored value * scale
	StringView unit;
};

SpanView<ParticleParamInfo> getParticleParams();
const ParticleParamInfo *getParticleParam(StringView key);

// Section ids in panel order, and their locale tags
SpanView<StringView> getParticleSections();
StringView getParticleSectionTitle(StringView section);

// Textures an emitter can use, by name
SpanView<StringView> getParticleTextureNames();

// `system` is ParticleSystem::encode(), `node` holds {texture, frameGrid}, `editor` holds
// {colorStops, animCurve}
Vector<double> particleParamToUi(const ParticleParamInfo &, const Value &system, const Value &node,
		const Value &editor);

// A patch with this parameter only; flags need the current `system` to keep the other flags
Value particleParamFromUi(const ParticleParamInfo &, SpanView<double>, const Value &system);

} // namespace stappler::xenolith::examples

#endif /* EXAMPLES_WINDOW_PARTICLES_SRC_PARTICLES_PARTICLEPARAMS_H_ */
