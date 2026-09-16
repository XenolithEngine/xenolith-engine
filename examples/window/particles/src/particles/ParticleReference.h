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

#ifndef EXAMPLES_WINDOW_PARTICLES_SRC_PARTICLES_PARTICLEREFERENCE_H_
#define EXAMPLES_WINDOW_PARTICLES_SRC_PARTICLES_PARTICLEREFERENCE_H_

#include "XLCommon.h" // IWYU pragma: keep
#include "XL2dParticleSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

// What the renderer takes from an emitter that the simulation depends on
struct ParticleReferenceInput {
	Rc<basic2d::ParticleSystemData> system;
	Size2 defaultSize; // the emitter's texture region, for a system with no particle size
	Mat4 nodeToScene; // identity with LocalCoords, as ParticleEmitter::pushCommands passes it
};

/** The CPU reference of the GPU particle update: the particles of a system restarted with a fixed
seed, after `steps` simulation steps from the restart.

It runs the same XL2dGlslParticleSim.h the update shader compiles, prepared the way vk::ParticlePass
prepares the GPU: zeroed particles seeded by index, the emitter data with the default size and the
extra data buffer. How the steps were spread over frames does not change the result; the node must
not have moved since the restart. False without a seed: particles are then seeded by the OS. */
bool simulateParticleReference(const ParticleReferenceInput &, uint64_t steps, uint32_t count,
		Vector<basic2d::ParticleData> &out);

} // namespace stappler::xenolith::examples

#endif /* EXAMPLES_WINDOW_PARTICLES_SRC_PARTICLES_PARTICLEREFERENCE_H_ */
