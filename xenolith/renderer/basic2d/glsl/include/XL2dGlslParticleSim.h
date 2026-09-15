/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

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

#ifndef XENOLITH_RENDERER_BASIC2D_GLSL_INCLUDE_XL2DGLSLPARTICLESIM_H_
#define XENOLITH_RENDERER_BASIC2D_GLSL_INCLUDE_XL2DGLSLPARTICLESIM_H_

/* Emission and simulation of one particle, compiled both by the particle update shader and by C++
(tests/particles), so the CPU reference runs the same text.

The includer provides the emission points:
	uint particleEmissionPointCount();
	vec2 particleEmissionPoint(uint index);
A shader defines them before including this header; in C++ they are declared here. */

#include "XL2dGlslParticle.h"

#ifndef SP_GLSL
namespace STAPPLER_VERSIONIZED stappler::glsl {

uint particleEmissionPointCount();
vec2 particleEmissionPoint(uint index);

#endif

// PCG-RXS-M-XS 32
// clang-format off
SP_GLSL_INLINE uint particleHash(uint value) {
	uint state = value * 747796405u + 2891336453u;
	uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	return (word >> 22u) ^ word;
}
// clang-format on

// [0, 1) with 24 bits, exact in a float on both sides
SP_GLSL_INLINE float particleHash01(uint seed, uint cycle, uint index) {
	uint h = particleHash(seed ^ particleHash(cycle ^ particleHash(index)));
	return float(h >> 8u) / 16777216.0f;
}

// Step of the emission cycle the particle is born at: its phase i / count, shifted by up to one
// slot with randomness, compressed by explosiveness
SP_GLSL_INLINE uint particleEmitFrame(uint index, uint count, uint cycle, uint seed,
		float randomness, float explosiveness, uint framesInGen) {
	float offset = 0.0f;
	if (randomness > 0.0f) {
		offset = clamp(randomness, 0.0f, 1.0f) * particleHash01(seed, cycle, index);
	}

	float phase =
			(float(index) + offset) / float(count) * (1.0f - clamp(explosiveness, 0.0f, 1.0f));
	uint frame = uint(floor(phase * float(framesInGen)));
	return (frame < framesInGen) ? frame : framesInGen - 1u;
}

// Length of the emission cycle in steps: the lower lifetime bound, or the upper one with
// XL_PARTICLE_FLAG_USE_LIFETIME_MAX
SP_GLSL_INLINE uint particleCycleFrames(SP_GLSL_IN(ParticleEmitterData) emitter) {
	float lifetime = emitter.lifetime.init;
	if ((emitter.flags & XL_PARTICLE_FLAG_USE_LIFETIME_MAX) != 0u && emitter.lifetime.rnd > 0.0f) {
		lifetime = emitter.lifetime.init + emitter.lifetime.rnd;
	}

	float frames = (emitter.dt > 0.0f) ? floor(lifetime / emitter.dt) : 0.0f;
	return (frames >= 1.0f) ? uint(frames) : 1u;
}

SP_GLSL_INLINE float particleParam(SP_GLSL_IN(ParticleFloatParam) param,
		SP_GLSL_INOUT(pcg16_state_t) rng) {
	return param.init + param.rnd * pcg16_random_float_r(rng);
}

SP_GLSL_INLINE vec2 particleParam(SP_GLSL_IN(ParticleVec2Param) param,
		SP_GLSL_INOUT(pcg16_state_t) rng) {
	float x = pcg16_random_float_r(rng);
	float y = pcg16_random_float_r(rng);
	return vec2(param.init.x + param.rnd.x * x, param.init.y + param.rnd.y * y);
}

SP_GLSL_INLINE vec2 particleNormalFromAngle(float angle) { return vec2(cos(angle), sin(angle)); }

// Zero stays zero
SP_GLSL_INLINE vec2 particleNormalize(vec2 v) {
	float l = length(v);
	return (l > 0.0f) ? v / l : vec2(0.0f, 0.0f);
}

SP_GLSL_INLINE vec2 particleRotate(vec2 v, float angle) {
	float c = cos(angle);
	float s = sin(angle);
	return vec2(v.x * c - v.y * s, v.x * s + v.y * c);
}

SP_GLSL_INLINE vec2 particleTransformPoint(SP_GLSL_IN(ParticleFrameData) frame, vec2 p) {
	return vec2(frame.transformX.x * p.x + frame.transformX.y * p.y + frame.transformX.z,
			frame.transformY.x * p.x + frame.transformY.y * p.y + frame.transformY.z);
}

SP_GLSL_INLINE vec2 particleTransformVector(SP_GLSL_IN(ParticleFrameData) frame, vec2 v) {
	return vec2(frame.transformX.x * v.x + frame.transformX.y * v.y,
			frame.transformY.x * v.x + frame.transformY.y * v.y);
}

// Deterministic generator of the particle `index` for a fixed seed
SP_GLSL_INLINE void particleSeedRng(SP_GLSL_INOUT(pcg16_state_t) rng, uint seed, uint index) {
	pcg16_state_t base;
	pcg16_srandom_r(base, seed, 0x5eedu);
	pcg16_advance_r(base, 4u * index);
	uint state = pcg16_random_full_r(base);
	uint inc = pcg16_random_full_r(base);
	pcg16_srandom_r(rng, state, inc);
}

// Every parameter is chosen per particle. Without XL_PARTICLE_FLAG_LOCAL_COORDS the particle is
// moved into the scene by the node's transform; the linear acceleration stays in scene axes.
SP_GLSL_INLINE void particleEmit(SP_GLSL_INOUT(ParticleData) particle,
		SP_GLSL_IN(ParticleEmitterData) emitter, SP_GLSL_IN(ParticleFrameData) frame, vec2 point) {
	float lifetime = floor(particleParam(emitter.lifetime, particle.rng) / emitter.dt);
	particle.fullLifetime = (lifetime >= 1.0f) ? uint(lifetime) : 1u;
	particle.currentLifetime = particle.fullLifetime;

	float speed = particleParam(emitter.velocity, particle.rng);
	vec2 normal = particleNormalFromAngle(particleParam(emitter.normal, particle.rng));
	vec2 linearVelocity = particleParam(emitter.linearVelocity, particle.rng);

	particle.position = point;
	particle.velocity = normal * speed + linearVelocity;
	particle.origin = emitter.origin;
	particle.color = emitter.color;

	particle.angle = particleParam(emitter.angle, particle.rng);
	particle.angularVelocity = particleParam(emitter.angularVelocity, particle.rng);
	particle.scale = particleParam(emitter.scale, particle.rng);
	particle.hue = particleParam(emitter.hue, particle.rng);

	particle.orbitalVelocity = particleParam(emitter.orbitalVelocity, particle.rng);
	particle.radialVelocity = particleParam(emitter.radialVelocity, particle.rng);
	particle.linearAcceleration = particleParam(emitter.linearAcceleration, particle.rng);
	particle.acceleration = particleParam(emitter.acceleration, particle.rng);
	particle.radialAcceleration = particleParam(emitter.radialAcceleration, particle.rng);
	particle.tangentialAcceleration = particleParam(emitter.tangentialAcceleration, particle.rng);

	if ((emitter.flags & XL_PARTICLE_FLAG_LOCAL_COORDS) == 0u) {
		particle.position = particleTransformPoint(frame, particle.position);
		particle.origin = particleTransformPoint(frame, particle.origin);
		particle.velocity = particleTransformVector(frame, particle.velocity);
		particle.angle = particle.angle + frame.transformRotation;
		particle.scale = particle.scale * frame.transformScale;
	}
}

// One step of the cpu_particles_2d model: forces change the velocity, the velocity moves the
// particle, then the orbital and radial velocities move it around and away from origin
SP_GLSL_INLINE void particleStep(SP_GLSL_INOUT(ParticleData) particle, float dt) {
	vec2 radial = particleNormalize(particle.position - particle.origin);
	vec2 tangent = vec2(-radial.y, radial.x);

	vec2 force = particle.linearAcceleration
			+ particleNormalize(particle.velocity) * particle.acceleration
			+ radial * particle.radialAcceleration + tangent * particle.tangentialAcceleration;

	particle.velocity = particle.velocity + force * dt;
	particle.position = particle.position + particle.velocity * dt;

	if (particle.orbitalVelocity != 0.0f) {
		particle.position = particle.origin
				+ particleRotate(particle.position - particle.origin,
						particle.orbitalVelocity * dt);
	}

	if (particle.radialVelocity != 0.0f) {
		particle.position = particle.position
				+ particleNormalize(particle.position - particle.origin)
						* (particle.radialVelocity * dt);
	}

	particle.angle = particle.angle + particle.angularVelocity * dt;
}

// Corners of the particle's quad. With XL_PARTICLE_FLAG_ALIGN_WITH_VELOCITY the quad's Y axis
// follows a non-zero velocity instead of the angle.
SP_GLSL_INLINE void particleQuad(SP_GLSL_IN(ParticleData) particle,
		SP_GLSL_IN(ParticleEmitterData) emitter, SP_GLSL_OUT(vec2) bl, SP_GLSL_OUT(vec2) tl,
		SP_GLSL_OUT(vec2) tr, SP_GLSL_OUT(vec2) br) {
	vec2 halfSize = emitter.sizeNormal * (emitter.sizeValue * particle.scale);

	float c = cos(particle.angle);
	float s = sin(particle.angle);
	if ((emitter.flags & XL_PARTICLE_FLAG_ALIGN_WITH_VELOCITY) != 0u) {
		vec2 dir = particleNormalize(particle.velocity);
		if (dir.x != 0.0f || dir.y != 0.0f) {
			c = dir.y;
			s = -dir.x;
		}
	}

	vec2 ax = vec2(c, s) * halfSize.x;
	vec2 ay = vec2(-s, c) * halfSize.y;

	bl = particle.position - ax - ay;
	tl = particle.position - ax + ay;
	tr = particle.position + ax + ay;
	br = particle.position + ax - ay;
}

// Simulates frame.nframes steps: a particle whose emission step comes is born (again, if it is still
// alive), otherwise a living particle ages by one step and moves if it is still alive
SP_GLSL_INLINE void particleUpdate(SP_GLSL_INOUT(ParticleData) particle,
		SP_GLSL_IN(ParticleEmitterData) emitter, SP_GLSL_IN(ParticleFrameData) frame, uint index) {
	for (uint k = 0u; k < frame.nframes; ++k) {
		uint t = frame.genframe + k;
		uint cycle = frame.cycle + t / frame.framesInGen;
		uint cycleStep = t % frame.framesInGen;

		uint emitStep = particleEmitFrame(index, emitter.count, cycle, frame.seed,
				emitter.randomness, emitter.explosiveness, frame.framesInGen);

		if (emitStep == cycleStep) {
			vec2 point = vec2(0.0f, 0.0f);
			uint npoints = particleEmissionPointCount();
			if (npoints > 0u) {
				point = particleEmissionPoint(pcg16_boundedrand_r(particle.rng, npoints));
			}
			particleEmit(particle, emitter, frame, point);
		} else if (particle.currentLifetime > 0u) {
			// A lifetime of L steps covers the birth step and L - 1 more
			particle.currentLifetime -= 1u;
			if (particle.currentLifetime > 0u) {
				particleStep(particle, frame.dt);
			}
		}
	}
}

#ifndef SP_GLSL

// Steps a frame at `now` simulates. A lag over maxSteps is dropped: the clock snaps to the latest
// step boundary instead of catching up.
inline uint32_t particleAdvanceClock(uint64_t &clock, uint64_t now, uint32_t interval,
		uint32_t maxSteps) {
	if (now <= clock || interval == 0) {
		return 0;
	}

	auto steps = (now - clock) / interval;
	if (steps > maxSteps) {
		clock = now - (now - clock) % interval;
		return maxSteps;
	}

	clock += steps * interval;
	return uint32_t(steps);
}

// Moves the cycle position by `steps`; with 0 it normalizes a position left past a shortened cycle
inline void particleAdvanceFrame(uint &frame, uint &cycle, uint framesInGen, uint steps) {
	frame += steps;
	cycle += frame / framesInGen;
	frame %= framesInGen;
}
}

#endif

#endif /* XENOLITH_RENDERER_BASIC2D_GLSL_INCLUDE_XL2DGLSLPARTICLESIM_H_ */
