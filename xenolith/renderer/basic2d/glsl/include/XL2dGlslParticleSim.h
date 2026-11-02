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

// The birth point of a shape emitter, drawn from the particle's own generator, spent before
// particleEmit draws the parameters. The shapes are the flat half of the engines the plan
// reads: the disc is Godot's sphere - "Particles will be emitted in the volume of a sphere";
// the rect its box - "Particles will be emitted in the volume of a box", its extents extending
// "in both directions from center"; the ring its ring - "Particles will be emitted in a ring
// or cylinder" with "The inner radius of the ring" and Effekseer's circle angle - "you specify
// just the radius and rotation angle"; the segment Effekseer's line - "Particles are placed on
// the line". Uniform over the area: the fraction u of it lies inside radius r = outer * sqrt(u)
// on the disc and inside r = sqrt(inner * inner + (outer * outer - inner * inner) * u) on the
// ring, so the point density stands flat; the rect draws its two axes apart, the segment one
// fraction between its ends.
SP_GLSL_INLINE vec2 particleEmissionShapePoint(SP_GLSL_IN(ParticleEmitterData) emitter,
		SP_GLSL_INOUT(pcg16_state_t) rng) {
	// The shape block follows the emission points one; both are whole 8-byte blocks, so the
	// words after the last point are its floats
	uint base = 2u + 2u * particleEmissionPointCount();
	if (emitter.emissionType == XL_PARTICLE_EMISSION_DISC) {
		float r = particleExtraFloat(base) * sqrt(pcg16_random_float_r(rng));
		float a = pcg16_random_float_r(rng) * 2.0f * 3.14159265358979323846f;
		return vec2(r * cos(a), r * sin(a));
	}
	if (emitter.emissionType == XL_PARTICLE_EMISSION_RING) {
		float outer = particleExtraFloat(base);
		float inner = particleExtraFloat(base + 1u);
		float from = particleExtraFloat(base + 2u);
		float to = particleExtraFloat(base + 3u);
		float r = sqrt(inner * inner + (outer * outer - inner * inner) * pcg16_random_float_r(rng));
		float a = from + (to - from) * pcg16_random_float_r(rng);
		return vec2(r * cos(a), r * sin(a));
	}
	if (emitter.emissionType == XL_PARTICLE_EMISSION_RECT) {
		float x = particleExtraFloat(base) * (2.0f * pcg16_random_float_r(rng) - 1.0f);
		float y = particleExtraFloat(base + 1u) * (2.0f * pcg16_random_float_r(rng) - 1.0f);
		return vec2(x, y);
	}
	// XL_PARTICLE_EMISSION_SEGMENT
	vec2 a = vec2(particleExtraFloat(base), particleExtraFloat(base + 1u));
	vec2 b = vec2(particleExtraFloat(base + 2u), particleExtraFloat(base + 3u));
	return a + (b - a) * pcg16_random_float_r(rng);
}

// Fraction of the lifetime lived, 0 at birth
SP_GLSL_INLINE float particleLifeFraction(SP_GLSL_IN(ParticleData) particle) {
	if (particle.fullLifetime == 0u) {
		return 0.0f;
	}
	return 1.0f - float(particle.currentLifetime) / float(particle.fullLifetime);
}

// The simulation time the frame stands at once its steps are done: every step t counts from the
// start of cycle frame.cycle - the absolute step is frame.cycle * framesInGen + t, as
// particleUpdate numbers it - and each ages the simulation by dt, so a run that completed
// `done` steps stands at done * dt. genframe still names the frame's first step while the
// update reads it (the caller advances past the call), so done is every step before the frame
// plus nframes; a frame that simulates nothing leaves the last simulated step standing. No
// wall clock is read: the same frame words answer the same time on every run
SP_GLSL_INLINE float particleSimTime(SP_GLSL_IN(ParticleFrameData) frame) {
	uint done = frame.cycle * frame.framesInGen + frame.genframe + frame.nframes;
	return float(done) * frame.dt;
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

	// Godot's damping_min / damping_max - "The maximum rate at which particles lose velocity.
	// For example value of 100 means that the particle will go from 100 velocity to 0 in 1
	// second", the pair's low end the "Minimum equivalent of damping_max" - is drawn last, past
	// every parameter the step already read. The rng order is part of the hash contract, and
	// particleParam draws even at rnd 0, so a zero pair stays out of the stream entirely: a
	// document without damping keeps the births - and the hash - it always had.
	particle.damping = 0.0f;
	if (emitter.damping.init != 0.0f || emitter.damping.rnd != 0.0f) {
		particle.damping = particleParam(emitter.damping, particle.rng);
	}

	// Effekseer's flip, the rendererCommon page's own words - "Specifies the probability of
	// flipping the UV horizontally for each particle." (effekseer.github.io, Help_Tool,
	// rendererCommon) - is drawn per particle at birth, past the damping pair, and lands as a
	// draw word the step never reads. The rng order is part of the hash contract, so the
	// probability's zero guards the draw: an emitter without the key spends no rng and keeps
	// the births - and the hash - it always had
	particle.drawFlags = 0u;
	if (emitter.uvFlip != 0.0f && pcg16_random_float_r(particle.rng) < emitter.uvFlip) {
		particle.drawFlags |= XL_PARTICLE_DRAW_FLAG_FLIP_U;
	}

	if ((emitter.flags & XL_PARTICLE_FLAG_LOCAL_COORDS) == 0u) {
		particle.position = particleTransformPoint(frame, particle.position);
		particle.origin = particleTransformPoint(frame, particle.origin);
		particle.velocity = particleTransformVector(frame, particle.velocity);
		particle.angle = particle.angle + frame.transformRotation;
		particle.scale = particle.scale * frame.transformScale;
	}

	// Godot's inherit_velocity_ratio - "Percentage of the velocity of the respective GPUParticles2D
	// or GPUParticles3D inherited by each particle when spawning."
	// (docs.godotengine.org, ParticleProcessMaterial) - adds the node's share to the velocity the
	// birth chose. The node's velocity rides the frame: its origin displacement over the last step
	// divided by dt, never a wall clock read, so the same frame answers the same sum on every run.
	// A pure sum - the generator stands unread - and the pair's zeros guard it: a zero ratio or a
	// node that never moved leaves the velocity - and the hash - the birth chose
	if (emitter.inheritVelocity != 0.0f
			&& (frame.nodeVelocity.x != 0.0f || frame.nodeVelocity.y != 0.0f)) {
		particle.velocity = particle.velocity + frame.nodeVelocity * emitter.inheritVelocity;
	}

	// The trail starts at the birth point the last transform left: every seat holds it, and the
	// seats part one to a step as the particle walks away. The width's zero guards the writes: a
	// trail-less emitter leaves every seat of every particle untouched
	if (emitter.trailWidth != 0.0f) {
		for (uint i = 0u; i < XL_PARTICLE_TRAIL_SEATS; ++i) {
			particle.trail[i] = particle.position;
		}
	}
}

// One octave of value noise on a 2D integer lattice: the four corner hashes come from the
// engine's own particleHash01, Perlin's quintic fades the fraction between them, and the result
// spans [-1, 1). No texture and no clock - a pure function of the seed and the domain point. The
// domain the turbulence hands it is non-negative by construction: the frequency and the shift
// speed are clamped to it and the lifetime fraction spans [0, 1], so the lattice word conversion
// stays defined.
SP_GLSL_INLINE float particleNoise2(uint seed, vec2 p) {
	vec2 i = floor(p);
	vec2 f = p - i;
	vec2 u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);
	uint x = uint(i.x);
	uint y = uint(i.y);
	float a = particleHash01(seed, x, y);
	float b = particleHash01(seed, x + 1u, y);
	float c = particleHash01(seed, x, y + 1u);
	float d = particleHash01(seed, x + 1u, y + 1u);
	return mix(mix(a, b, u.x), mix(c, d, u.x), u.y) * 2.0f - 1.0f;
}

// The turbulence force, Godot's turbulence block: its noise_strength "The turbulence noise
// strength. Increasing this will result in a stronger, more contrasting, flow pattern.", its
// noise_scale "This value controls the overall scale/frequency of the turbulence noise pattern.
// A small scale will result in smaller features with more detail while a high scale will result
// in smoother noise with larger features." - dividing where this frequency multiplies, its
// noise_speed "A scrolling velocity for the turbulence field. This sets a directional trend for
// the pattern to move in over time." - sliding the index axis as the simulation time grows.
// Godot samples its noise at the particle's position, a 3D texture this model refuses, so the
// pattern lives on the particles instead of the space: the domain's two axes are the particle's
// index and its lifetime fraction. Its influence_min / influence_max - "Minimum turbulence
// influence on each particle.", "Maximum turbulence influence on each particle." - draw the
// actual value at random between the two; the rng order is a hash contract, so the influence
// here ramps from the min at birth to the max at the last step instead. The second component
// rehashes the seed, keeping the two axes of the force decorrelated.
SP_GLSL_INLINE vec2 particleTurbulence(SP_GLSL_IN(ParticleData) particle,
		SP_GLSL_IN(ParticleEmitterData) emitter, uint seed, uint index, float simTime) {
	float life = particleLifeFraction(particle);
	vec2 domain = vec2(float(index) * emitter.turbulenceFrequency
					+ simTime * emitter.turbulenceShiftSpeed,
			life * emitter.turbulenceFrequency);
	vec2 noise = vec2(particleNoise2(seed, domain), particleNoise2(seed ^ 0x9e3779b9u, domain));
	float influence = mix(emitter.turbulenceInfluence.init,
			emitter.turbulenceInfluence.init + emitter.turbulenceInfluence.rnd, life);
	return noise * (emitter.turbulenceStrength * influence);
}

// The attractor force of up to four seats. Godot's own attractors are 3D - its tutorial says
// "Particle attractors are not yet implemented for 2D particle systems", so this seat is this
// model's own - and the image is Effekseer's, whose force field window "can specifies up to 4
// types of force fields" with the attract one moving the particle "towards the position
// specified under" its point. The strength's sign is Godot's, "If strength is negative, particles
// will be pushed in the opposite direction": positive pulls toward the point, negative pushes
// away. The falloff is the shape Godot's attractor tutorial words - "Attenuation controls the
// strength falloff within the attractor's influence region", the strength "weakest at the border
// of this boundary and strongest at its center", and "Particles outside of the boundary are not
// affected by the attractor at all" - the straight line between those two ends:
//
//     strength * (1 - dist / radius)
//
// Godot's directionality - "At 0.0, the attractor is not directional at all: it will attract
// particles towards its center" - stays at that 0 seat: every force points at its own point,
// never along a chosen axis. A pure function of the particle's position, no rng read.
SP_GLSL_INLINE vec2 particleAttractors(SP_GLSL_IN(ParticleData) particle,
		SP_GLSL_IN(ParticleEmitterData) emitter) {
	vec2 force = vec2(0.0f, 0.0f);
	for (uint i = 0u; i < 4u; ++i) {
		if (emitter.attractors[i].strength == 0.0f) {
			continue;
		}
		vec2 toPoint = emitter.attractors[i].point - particle.position;
		float dist = length(toPoint);
		// The boundary sits at the falloff's zero, and a particle at it is the "not affected at
		// all" of one past it; a seat radius of zero leaves no inside
		if (dist < emitter.attractors[i].radius) {
			float falloff = 1.0f - dist / emitter.attractors[i].radius;
			force += particleNormalize(toPoint) * (emitter.attractors[i].strength * falloff);
		}
	}
	return force;
}

// The deflector answer of up to four figures, Wallpaper Engine's own collision operators
// (docs.wallpaperengine.io/en/scene/particles/component/operator.html): its round figure words
// the sphere "Collides particles against a spherical object" - an origin and a radius, a circle
// in a 2D wallpaper - and its line one "Collides particles along a limited line from one side",
// this seat's segment the same line of two points alone, answered from both sides. The answers
// are the two the plan keeps: Bounce, "Particles will bounce off when the collision occurs" -
// the Bounce factor the page tweaks is not carried, the reflection is the full one - and the
// plan's kill, the page's deleting answer: "Particles will be deleted when they collide with the
// object", which "Triggers the death event on the particle, allowing you to spawn child
// particles for example".
//
// The reflection, off the figure the quote words the particle bouncing away from: the wall's
// unit normal n splits the velocity into its normal and its tangential part,
//
//     v = (v . n) n + t
//
// and the bounce keeps the tangential part while it flips the normal one,
//
//     v' = t - (v . n) n = v - 2 (v . n) n
//
// The normal's sign never enters the formula, so the circle's radial line and the segment's
// perpendicular answer alike. The through-half of the step is the wall's own symmetry undone:
// the position mirrors across the boundary - the circle's radial boundary, the segment's line -
// the depth it crossed by, so the bounced particle stands strictly on the side it came from and
// the next step carries it away; a particle left past the wall would meet it again every step,
// the bounce off the quote words never happening.
//
// The hit test is the stepped sim's own: the circle's is the particle inside the ball, a
// distance under the radius; the segment's is the step's path crossing the line - the previous
// position strictly on one side and the new one on the other or on it - with the crossing
// between the segment's ends. The kill is the lifetime pair's own zero, the death signature the
// document cycle reads, and its walk ends at the first killing seat - a dead particle bounces
// off nothing. A pure state write - the generator stands unread - and a seat of shape 0 seats
// nothing: an emitter whose every seat is unused touches no byte, and an old file answers the
// step - and the hash - it always had
SP_GLSL_INLINE void particleDeflectors(SP_GLSL_INOUT(ParticleData) particle,
		SP_GLSL_IN(ParticleEmitterData) emitter, vec2 previous) {
	for (uint i = 0u; i < 4u; ++i) {
		if (emitter.deflectors[i].shape == 0u) {
			continue;
		}
		bool hit = false;
		vec2 normal = vec2(0.0f, 0.0f); // the wall's unit normal, signed as the test found it
		vec2 through = vec2(0.0f, 0.0f); // the offset the mirror takes back off the position
		if (emitter.deflectors[i].shape == XL_PARTICLE_DEFLECTOR_CIRCLE) {
			// The ball's extent alone: a radius of zero or less leaves no inside
			vec2 radial = particle.position - emitter.deflectors[i].a;
			float dist = length(radial);
			if (dist < emitter.deflectors[i].radius) {
				hit = true;
				normal = radial / dist;
				// The depth past the boundary, along the radial unit
				through = normal * (dist - emitter.deflectors[i].radius);
			}
		} else {
			// The side of the line through a and b the point stands on, by the sign of the cross
			vec2 d = emitter.deflectors[i].b - emitter.deflectors[i].a;
			vec2 path = particle.position - previous;
			float s0 = d.x * (previous.y - emitter.deflectors[i].a.y)
					- d.y * (previous.x - emitter.deflectors[i].a.x);
			float s1 = d.x * (particle.position.y - emitter.deflectors[i].a.y)
					- d.y * (particle.position.x - emitter.deflectors[i].a.x);
			float denominator = path.x * d.y - path.y * d.x;
			if (((s0 > 0.0f && s1 <= 0.0f) || (s0 < 0.0f && s1 >= 0.0f))
					&& denominator != 0.0f) {
				// The crossing point, then its place between the segment's ends
				float u = ((emitter.deflectors[i].a.x - previous.x) * d.y
								  - (emitter.deflectors[i].a.y - previous.y) * d.x)
						/ denominator;
				vec2 crossing = previous + path * u;
				vec2 along = crossing - emitter.deflectors[i].a;
				float extent = along.x * d.x + along.y * d.y;
				if (extent >= 0.0f && extent <= d.x * d.x + d.y * d.y) {
					hit = true;
					normal = vec2(-d.y, d.x);
					// s1 is the cross of d with the offset from the line, so the perpendicular
					// offset itself is s1 / (d . d) along the perpendicular
					through = normal * (s1 / (d.x * d.x + d.y * d.y));
				}
			}
		}
		if (!hit) {
			continue;
		}
		if (emitter.deflectors[i].response == XL_PARTICLE_DEFLECTOR_KILL) {
			particle.currentLifetime = 0u;
			return;
		}
		if (emitter.deflectors[i].response == XL_PARTICLE_DEFLECTOR_BOUNCE) {
			vec2 n = particleNormalize(normal);
			particle.velocity = particle.velocity - 2.0f * dot(particle.velocity, n) * n;
			particle.position = particle.position - 2.0f * through;
		}
	}
}

// One step of the cpu_particles_2d model: forces change the velocity, the damping and the speed
// ceiling correct it, the velocity moves the particle, then the orbital and radial velocities move
// it around and away from origin
SP_GLSL_INLINE void particleStep(SP_GLSL_INOUT(ParticleData) particle,
		SP_GLSL_IN(ParticleEmitterData) emitter, float dt, uint seed, uint index,
		float simTime) {
	vec2 radial = particleNormalize(particle.position - particle.origin);
	vec2 tangent = vec2(-radial.y, radial.x);

	vec2 force = particle.linearAcceleration
			+ particleNormalize(particle.velocity) * particle.acceleration
			+ radial * particle.radialAcceleration + tangent * particle.tangentialAcceleration;

	// The turbulence force reads the emitter's words alone - no draw from the particle's
	// generator - and a zero strength touches no byte: an old file answers the step - and the
	// hash - it always had
	if (emitter.turbulenceStrength != 0.0f) {
		force += particleTurbulence(particle, emitter, seed, index, simTime);
	}

	// The attractors read the seats' words the same way, and an empty seating touches no byte
	// either: only a force that is not exactly zero reaches the velocity
	vec2 attracted = particleAttractors(particle, emitter);
	if (attracted.x != 0.0f || attracted.y != 0.0f) {
		force += attracted;
	}

	particle.velocity = particle.velocity + force * dt;

	// Godot's damping - "The maximum rate at which particles lose velocity. For example value of
	// 100 means that the particle will go from 100 velocity to 0 in 1 second" - takes the rate
	// chosen at birth off the speed, never below zero; Wallpaper Engine's Cap velocity - "Lets
	// you set an upper limit for the speed of particles", its Max speed "The maximum speed for
	// each individual particle" - then holds the speed under the emitter's ceiling. Zero damping
	// and a zero ceiling touch no byte: an old file answers the step - and the hash - it always
	// had, with no float noise from a length round trip the values never asked for.
	if (particle.damping != 0.0f) {
		float speed = length(particle.velocity);
		if (speed > 0.0f) {
			float damped = speed - particle.damping * dt;
			particle.velocity = particle.velocity * ((damped > 0.0f ? damped : 0.0f) / speed);
		}
	}
	if (emitter.maxSpeed != 0.0f) {
		float speed = length(particle.velocity);
		if (speed > emitter.maxSpeed) {
			particle.velocity = particle.velocity * (emitter.maxSpeed / speed);
		}
	}

	// The step's path starts where the position last stood; the deflectors read it past every
	// move this step made, the radial one the last
	vec2 previous = particle.position;
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

	particleDeflectors(particle, emitter, previous);

	particle.angle = particle.angle + particle.angularVelocity * dt;
}

// Corners of the particle's quad. With XL_PARTICLE_FLAG_ALIGN_WITH_VELOCITY the quad's Y axis
// follows a non-zero velocity instead of the angle, and the emitter's stretch grows that axis's
// half-length by speed * stretch - Unity 6's Velocity Scale, "Specifies how much particles
// stretch depending on their velocity", on a mode whose particles "still align to the velocity"
// at a zero scale; Godot's scale_over_velocity_curve "scales each particle based on its
// velocity". Zero stretch adds nothing: the quad stays the one the birth scale built.
SP_GLSL_INLINE void particleQuad(SP_GLSL_IN(ParticleData) particle,
		SP_GLSL_IN(ParticleEmitterData) emitter, SP_GLSL_OUT(vec2) bl, SP_GLSL_OUT(vec2) tl,
		SP_GLSL_OUT(vec2) tr, SP_GLSL_OUT(vec2) br) {
	vec2 halfSize = emitter.sizeNormal * (emitter.sizeValue * particle.scale);

	float c = cos(particle.angle);
	float s = sin(particle.angle);
	if ((emitter.flags & XL_PARTICLE_FLAG_ALIGN_WITH_VELOCITY) != 0u) {
		if (emitter.stretch != 0.0f) {
			halfSize.y = halfSize.y + length(particle.velocity) * emitter.stretch;
		}
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

// Godot's scale_curve: the curve multiplies the scale chosen at birth, sampled at the lifetime
// fraction. No curve - the birth scale stands untouched, the state the step kept before the
// curve existed.
SP_GLSL_INLINE void particleApplyScaleCurve(SP_GLSL_INOUT(ParticleData) particle,
		SP_GLSL_IN(ParticleEmitterData) emitter) {
	if (emitter.scaleCurveOffset != 0u) {
		particle.scale = particle.birthScale
				* particleSampleCurve(emitter.scaleCurveOffset, particleLifeFraction(particle)).x;
	}
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

		// The simulation time the turbulence reads. t counts the steps of this frame from the
		// start of cycle frame.cycle, and a cycle holds framesInGen steps, so the absolute step
		// of the simulation is frame.cycle * framesInGen + t; every step ages the simulation by
		// dt, so the time the first `step` steps sum to is step * dt. No wall clock is read: the
		// same document at the same frame answers the same time on every run.
		uint step = frame.cycle * frame.framesInGen + t;
		float simTime = float(step) * frame.dt;

		uint emitStep = particleEmitFrame(index, emitter.count, cycle, frame.seed,
				emitter.randomness, emitter.explosiveness, frame.framesInGen);

		// oneShot is Godot's one_shot - "If true, only one emission cycle occurs": past cycle 0
		// the flagged emitter gives no birth, and the slot ages a living particle instead
		if (emitStep == cycleStep
				&& (((emitter.flags & XL_PARTICLE_FLAG_ONE_SHOT) == 0u) || cycle == 0u)) {
			vec2 point = vec2(0.0f, 0.0f);
			if (emitter.emissionType == XL_PARTICLE_EMISSION_POINTS) {
				uint npoints = particleEmissionPointCount();
				if (npoints > 0u) {
					point = particleEmissionPoint(pcg16_boundedrand_r(particle.rng, npoints));
				}
			} else {
				point = particleEmissionShapePoint(emitter, particle.rng);
			}
			particleEmit(particle, emitter, frame, point);
			particle.birthScale = particle.scale;
			particleApplyScaleCurve(particle, emitter);
			counters.births += 1u;
		} else if (particle.currentLifetime > 0u) {
			// A lifetime of L steps covers the birth step and L - 1 more
			particle.currentLifetime -= 1u;
			counters.steps += 1u;
			if (particle.currentLifetime > 0u) {
				// The trail's history, Godot's trail_enabled - "If true, enables particle trails
				// using a mesh skinning system" - taken without the skinning mesh: the strip this
				// model draws is Effekseer's track, which "is not influenced by them and always
				// faces the camera", not its ribbon, "shaped by connecting lines between
				// (-0.5, 0, 0) and (0.5, 0, 0) of each particle" - a 3D seat this model refuses.
				// The history is the positions the particle stood in: this step's pre-move
				// position becomes the newest seat and the oldest falls out, so no seat repeats
				// the current position. A pure state write - the generator stands unread - and
				// the width's zero guards it: an old file answers the step - and the hash - it
				// always had
				if (emitter.trailWidth != 0.0f) {
					for (uint i = XL_PARTICLE_TRAIL_SEATS - 1u; i > 0u; --i) {
						particle.trail[i] = particle.trail[i - 1u];
					}
					particle.trail[0] = particle.position;
				}
				particleStep(particle, emitter, frame.dt, frame.seed, index, simTime);
				particleApplyScaleCurve(particle, emitter);
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
