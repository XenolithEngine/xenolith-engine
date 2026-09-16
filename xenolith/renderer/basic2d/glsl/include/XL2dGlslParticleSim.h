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

The includer provides the emission points and the words of the extra data buffer (byte offset / 4):
	uint particleEmissionPointCount();
	vec2 particleEmissionPoint(uint index);
	uint particleExtraUint(uint word);
	float particleExtraFloat(uint word);
A shader defines them before including this header; in C++ they are declared here. */

#include "XL2dGlslParticle.h"

#ifndef SP_GLSL
namespace STAPPLER_VERSIONIZED stappler::glsl {

uint particleEmissionPointCount();
vec2 particleEmissionPoint(uint index);
uint particleExtraUint(uint word);
float particleExtraFloat(uint word);

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

// Fraction of the lifetime lived, 0 at birth
SP_GLSL_INLINE float particleLifeFraction(SP_GLSL_IN(ParticleData) particle) {
	if (particle.fullLifetime == 0u) {
		return 0.0f;
	}
	return 1.0f - float(particle.currentLifetime) / float(particle.fullLifetime);
}

SP_GLSL_INLINE float particleCurveComponent(uint base, uint i0, uint i1, float f, uint components,
		uint c) {
	uint k = (c < components) ? c : components - 1u;
	float a = particleExtraFloat(base + i0 * components + k);
	float b = particleExtraFloat(base + i1 * components + k);
	return a + (b - a) * f;
}

// A curve of the extra data buffer at a byte offset, linearly interpolated at t in [0, 1]. Missing
// components repeat the last one, so a float curve reads as (v, v, v, v).
SP_GLSL_INLINE vec4 particleSampleCurve(uint offset, float t) {
	uint word = offset / 4u;
	uint count = particleExtraUint(word);
	uint components = particleExtraUint(word + 1u);
	if (count == 0u || components == 0u) {
		return vec4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	float pos = clamp(t, 0.0f, 1.0f) * float(count);
	uint i0 = uint(floor(pos));
	if (i0 > count - 1u) {
		i0 = count - 1u;
	}
	uint i1 = (i0 + 1u < count) ? i0 + 1u : i0;
	float f = clamp(pos - float(i0), 0.0f, 1.0f);

	uint base = word + 2u;
	return vec4(particleCurveComponent(base, i0, i1, f, components, 0u),
			particleCurveComponent(base, i0, i1, f, components, 1u),
			particleCurveComponent(base, i0, i1, f, components, 2u),
			particleCurveComponent(base, i0, i1, f, components, 3u));
}

// Hue rotation by `turns` with the matrix of Godot's ParticleProcessMaterial, its vectors taken as
// rows: luminance and grays are kept, alpha is untouched
SP_GLSL_INLINE vec4 particleHueRotate(vec4 color, float turns) {
	float angle = turns * 2.0f * 3.14159265358979323846f;
	float c = cos(angle);
	float s = sin(angle);
	return vec4((0.299f + 0.701f * c + 0.168f * s) * color.x
					+ (0.587f - 0.587f * c + 0.330f * s) * color.y
					+ (0.114f - 0.114f * c - 0.497f * s) * color.z,
			(0.299f - 0.299f * c - 0.328f * s) * color.x
					+ (0.587f + 0.413f * c + 0.035f * s) * color.y
					+ (0.114f - 0.114f * c + 0.292f * s) * color.z,
			(0.299f - 0.300f * c + 1.250f * s) * color.x
					+ (0.587f - 0.588f * c - 1.050f * s) * color.y
					+ (0.114f + 0.886f * c - 0.203f * s) * color.z,
			color.w);
}

// color · colorCurve(t), then the hue rotation chosen at birth
SP_GLSL_INLINE vec4 particleColor(SP_GLSL_IN(ParticleData) particle,
		SP_GLSL_IN(ParticleEmitterData) emitter) {
	vec4 color = particle.color;
	if (emitter.colorCurveOffset != 0u) {
		vec4 curve = particleSampleCurve(emitter.colorCurveOffset, particleLifeFraction(particle));
		color = vec4(color.x * curve.x, color.y * curve.y, color.z * curve.z, color.w * curve.w);
	}
	if (particle.hue != 0.0f) {
		color = particleHueRotate(color, particle.hue);
	}
	return color;
}

// Animation frame of `frames`: the animation curve maps the lifetime fraction to [0, 1] of them
SP_GLSL_INLINE uint particleAnimFrame(SP_GLSL_IN(ParticleData) particle,
		SP_GLSL_IN(ParticleEmitterData) emitter, uint frames) {
	if (emitter.animFrameCurveOffset == 0u || frames <= 1u) {
		return 0u;
	}

	float value =
			particleSampleCurve(emitter.animFrameCurveOffset, particleLifeFraction(particle)).x;
	float frame = floor(clamp(value, 0.0f, 1.0f) * float(frames));
	uint ret = uint(frame);
	return (ret < frames) ? ret : frames - 1u;
}

// Texture coordinates of an animation frame: the frame's cell of the grid inside textureRect,
// uv0 at the top left of the image cell, uv1 at the bottom right
SP_GLSL_INLINE void particleFrameCell(SP_GLSL_IN(ParticleFrameData) frame, uint index,
		SP_GLSL_OUT(vec2) uv0, SP_GLSL_OUT(vec2) uv1) {
	uint h = (frame.hFrames > 0u) ? frame.hFrames : 1u;
	uint v = (frame.vFrames > 0u) ? frame.vFrames : 1u;
	float cw = frame.textureRect.z / float(h);
	float ch = frame.textureRect.w / float(v);
	uint col = index % h;
	uint row = (index / h) % v;
	uv0 = vec2(frame.textureRect.x + float(col) * cw, frame.textureRect.y + float(row) * ch);
	uv1 = vec2(uv0.x + cw, uv0.y + ch);
}

// Simulates frame.nframes steps: a particle whose emission step comes is born (again, if it is still
// alive), otherwise a living particle ages by one step and moves if it is still alive. The counters
// grow by the births and the aging steps.
SP_GLSL_INLINE void particleUpdate(SP_GLSL_INOUT(ParticleData) particle,
		SP_GLSL_IN(ParticleEmitterData) emitter, SP_GLSL_IN(ParticleFrameData) frame, uint index,
		SP_GLSL_INOUT(ParticleUpdateCounters) counters) {
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
			counters.births += 1u;
		} else if (particle.currentLifetime > 0u) {
			// A lifetime of L steps covers the birth step and L - 1 more
			particle.currentLifetime -= 1u;
			counters.steps += 1u;
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

// The particle born last by the step `cycleStep` of the cycle, from the nominal phases (no
// randomness): the largest index whose emission step is not after it
inline uint32_t particleNewest(uint count, float explosiveness, uint framesInGen, uint cycleStep) {
	if (count == 0) {
		return 0;
	}

	uint32_t lo = 0;
	uint32_t hi = count - 1;
	while (lo < hi) {
		auto mid = lo + (hi - lo + 1) / 2;
		if (particleEmitFrame(mid, count, 0, 0, 0.0f, explosiveness, framesInGen) <= cycleStep) {
			lo = mid;
		} else {
			hi = mid - 1;
		}
	}
	return lo;
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
