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

#ifndef EXAMPLES_WINDOW_PARTICLES_SRC_PARTICLES_PARTICLEPRESETS_H_
#define EXAMPLES_WINDOW_PARTICLES_SRC_PARTICLES_PARTICLEPRESETS_H_

#include "XLCommon.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

// Names of the presets, in the order they are offered
SpanView<StringView> getParticlePresetNames();

// The preset over the ParticleSystem::init defaults, in the ParticleSystem::encode format, so
// ParticleSystem::apply sets every parameter. Empty for an unknown name.
//
// Keys beyond the system: the node's "texture" and "frameGrid", and the editable curves
// "colorStops" and "animCurve" (see ParticleCurves.h), which replace sampled curves.
Value getParticlePreset(StringView name);

} // namespace stappler::xenolith::examples

#endif /* EXAMPLES_WINDOW_PARTICLES_SRC_PARTICLES_PARTICLEPRESETS_H_ */
