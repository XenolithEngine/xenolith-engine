/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef XENOLITH_RENDERER_BASIC2D_GLSL_INCLUDE_XL2DGLSLPARTICLE_H_
#define XENOLITH_RENDERER_BASIC2D_GLSL_INCLUDE_XL2DGLSLPARTICLE_H_

#include "sprt_glsl.h"

#ifndef SP_GLSL
namespace STAPPLER_VERSIONIZED stappler::glsl {

using namespace sprt::glsl;

#endif

// ParticleEmitterData::flags, the same bits as basic2d::ParticleSystemFlags
#define XL_PARTICLE_FLAG_LOCAL_COORDS 1
#define XL_PARTICLE_FLAG_ALIGN_WITH_VELOCITY 2
#define XL_PARTICLE_FLAG_ORDER_BY_LIFETIME 4
#define XL_PARTICLE_FLAG_USE_LIFETIME_MAX 8
#define XL_PARTICLE_FLAG_ONE_SHOT 16

// ParticleEmitterData::emissionType, the same values as basic2d::ParticleEmissionType
#define XL_PARTICLE_EMISSION_POINTS 0
#define XL_PARTICLE_EMISSION_DISC 1
#define XL_PARTICLE_EMISSION_RING 2
#define XL_PARTICLE_EMISSION_RECT 3
#define XL_PARTICLE_EMISSION_SEGMENT 4

// The trail seats every particle carries. The number is Godot's own - its GPUParticles2D page
// words trail_sections as "The number of sections to use for the particle trail rendering" and
// carries 8 as the default - and no key chooses it: every emitter that draws a trail draws the
// same count
#define XL_PARTICLE_TRAIL_SEATS 8

// ParticleDeflector::shape, the figure a seat carries; zero is the unused seat
#define XL_PARTICLE_DEFLECTOR_CIRCLE 1
#define XL_PARTICLE_DEFLECTOR_SEGMENT 2

// ParticleDeflector::response, the answer a particle that meets the figure earns
#define XL_PARTICLE_DEFLECTOR_BOUNCE 1
#define XL_PARTICLE_DEFLECTOR_KILL 2

// ParticleData::drawFlags, the draw words a birth chose; the bits are the draw's own, the step
// reads none of them
#define XL_PARTICLE_DRAW_FLAG_FLIP_U 1

struct ParticleConstantData {
	uvec2 frameDataPointer;
	uvec2 outVerticesPointer;
	uint emitterIndex;
	uint padding12;
};

// One emitter in the frame data buffer
struct ParticleFrameData {
	// [0-15]
	uvec2 emitterPointer;
	uint vertexOffset;
	uint particleBufferIndex;

	// [16-31]
	uint materialIndex;
	uint framesInGen; // steps in one emission cycle
	uint genframe; // step of the cycle the first simulated step starts at
	uint nframes; // steps to simulate

	// [32-47]
	uint cycle;
	uint seed; // phase hash seed
	float dt;
	float transformRotation; // rotation of the node's transform, radians

	// [48-63]
	vec4 transformX; // node to scene: x' = transformX.x * x + transformX.y * y + transformX.z

	// [64-79]
	vec4 transformY; // node to scene: y' = transformY.x * x + transformY.y * y + transformY.z

	// [80-95]
	float transformScale; // sqrt(|det|) of the node's transform
	uint padding84;
	// ParticleFeedbackRecord per particle, for the feedback pipeline only. At 88: std430 aligns a
	// uvec2 to 8 bytes.
	uvec2 feedbackPointer;

	// [96-111]
	vec4 textureRect; // the emitter's texture region: origin.x, origin.y, width, height

	// [112-127]
	vec4 nodeColor; // the emitter node's displayed color, straight alpha

	// [128-143]
	uint hFrames; // animation frame grid inside textureRect, row 0 at the top of the image
	uint vFrames;
	uint newest; // index of the last born particle, for XL_PARTICLE_FLAG_ORDER_BY_LIFETIME
	uint padding140;

	// [144-151]
	// the node's velocity over the last step - its origin displacement divided by dt, never a
	// wall clock read - the share a newborn with inheritVelocity carries; zero in every frame
	// the node never moved
	vec2 nodeVelocity;

	// [152-159]
	// explicit padding: without it the member-wise size ends at 152 while the C++ side's sizeof
	// rounds to the vec4 alignment's 160, and the frame array's strides part ways
	vec2 padding152;

	// [160]
};

// What one particle did in a frame, written by the feedback pipeline to the particle's own record:
// no two invocations write the same memory, the CPU sums the records
struct ParticleFeedbackRecord {
	uint births;
	uint steps; // steps a living particle aged by
	uint alive; // 1 - the particle is alive after the frame and draws a quad
	uint padding12;
};

struct ParticleUpdateCounters {
	uint births;
	uint steps;
};

struct ParticleIndirectCommand {
	uint vertexCount;
	uint instanceCount;
	uint firstVertex;
	uint firstInstance;
};

struct ParticleFloatParam {
	float init;
	float rnd;
};

struct ParticleVec2Param {
	vec2 init;
	vec2 rnd;
};

struct ParticleEmissionPoints {
	uint count;
	uint padding4;
};

// One attractor seat: the force's point, its strength and its radius. A strength of zero is an
// unused seat; a positive strength pulls toward the point, a negative one pushes away, and past
// the radius the force is zero
struct ParticleAttractor {
	vec2 point;
	float strength;
	float radius;
};

// One deflector seat: the shape names the figure - a circle of `a` as its center and `radius`, a
// segment of `a` to `b` - and the response the answer a particle that meets the figure earns, a
// bounce off the figure or the death the onDeath event sees. A shape of zero is an unused seat
struct ParticleDeflector {
	vec2 a; // the circle's center or the segment's first end
	vec2 b; // the segment's second end, unread by the circle
	float radius; // the circle's radius, unread by the segment
	uint shape; // a XL_PARTICLE_DEFLECTOR_* shape, 0 - unused
	uint response; // a XL_PARTICLE_DEFLECTOR_* response
};

// Header of a curve in the emitter's extra data buffer, followed by count * components floats
struct ParticleCurveHeader {
	uint count;
	uint components;
};

struct ParticleEmitterData {
	// [0-15]
	// 0 - the point list; 1 - disc, 2 - ring, 3 - rect, 4 - segment, the birth shapes whose
	// numbers follow the point list in the extra data buffer
	uint count;
	uint emissionType;
	uvec2 emissionData;

	// [16-31]
	// 0 - localCoords;
	// 1 - alignWithVelocity;
	// 2 - orderByLifetime
	uint flags;
	uint frameInterval;
	vec2 origin;

	// [32-47]
	float sizeValue;
	// growth of the velocity-aligned quad's half-length per unit of speed, 0 - no growth
	float stretch;
	vec2 sizeNormal;

	// [48-63]
	vec4 color;

	// [64-79]
	float explosiveness;
	float randomness;
	ParticleFloatParam normal; // Нормализованное начальное направление движения в радианах

	// [80-95]
	ParticleFloatParam lifetime; // in float seconds
	ParticleFloatParam scale;

	// [96-111]
	ParticleFloatParam angle; // Угол поворота спрайта
	ParticleFloatParam velocity; // скорость вдоль нормали

	// [112-127]
	ParticleVec2Param linearVelocity; // скорость в абсолютном значении

	// [128-143]
	ParticleFloatParam angularVelocity; // начальная скорость вращения, радианы в секунду
	ParticleFloatParam orbitalVelocity; // скорость вращения вокруг origin, радианы в секунду

	// [144-159]
	ParticleFloatParam radialVelocity; // скорость движения от origin, dp в секунду
	ParticleFloatParam acceleration; // ускорение вдоль нормали, dp в секунду^2

	// [160-175]
	ParticleVec2Param linearAcceleration; // ускорение в абсолютном значении, dp в секунду^2

	// [176-191]
	ParticleFloatParam radialAcceleration; // ускорение от origin, dp в секунду^2
	ParticleFloatParam tangentialAcceleration; // ускорение перпендикулярно движению, dp в секунду^2

	// [192-207]
	ParticleFloatParam hue;

	// [208-223]
	uint colorCurveOffset;
	uint animFrameCurveOffset;
	uint scaleCurveOffset;
	float dt;

	// [224-239]
	// the rate the speed loses per second, chosen at birth from the pair; 0 - no loss
	ParticleFloatParam damping;
	// upper limit of the speed, 0 - no limit
	float maxSpeed;
	uint padding236;

	// [240-263]
	// scale of the turbulence force, 0 - no turbulence
	float turbulenceStrength;
	// variations of the one-octave noise across its two axes, 0 - none
	float turbulenceFrequency;
	// slide of the noise pattern per second of simulation time, 0 - a static pattern
	float turbulenceShiftSpeed;
	// min and max of the influence ramp over the lifetime, the min at birth, the max at the last
	// step
	ParticleFloatParam turbulenceInfluence;
	uint padding260;

	// [264-327]
	// the attractor seats, in the order the file's list seats them; a seat of strength 0 is
	// unused
	ParticleAttractor attractors[4];

	// [328-331]
	// width of the trail strip the particle's seat history draws, 0 - no trail
	float trailWidth;

	// [332-459]
	// the deflector seats, in the order the file's list seats them; a seat of shape 0 is unused
	ParticleDeflector deflectors[4];

	// [460-463]
	// ratio of the node's velocity a newborn inherits, 0 - no inheritance
	float inheritVelocity;

	// [464-471]
	// slide of the sampled UV window over the texture per second of simulation time, signed per
	// axis, (0, 0) - no slide
	vec2 uvScroll;
	// probability of the U mirror a birth draws from the particle's own generator, 0 - never
	float uvFlip;

	// [476-479]
	// explicit padding: without it the member-wise size ends at 476 while the C++ side's sizeof
	// rounds to the vec4 alignment's 480, and the emitter words part ways between the two texts
	uint padding476;

	// [480]
};

// State of one particle. Parameters are chosen at birth; positions are in the node's space with
// XL_PARTICLE_FLAG_LOCAL_COORDS, in the scene's otherwise
struct ParticleData {
	// [0-15]
	pcg16_state_t rng;
	vec2 position;

	// [16-31]
	vec2 velocity;
	vec2 origin; // center of the orbital, radial and tangential motion

	// [32-47]
	vec4 color;

	// [48-63]
	float angle; // quad rotation, radians
	float angularVelocity; // radians per second
	float scale; // quad size multiplier
	float hue;

	// [64-79]
	uint fullLifetime; // in steps
	uint currentLifetime; // in steps, 0 - dead
	float orbitalVelocity; // radians per second around origin
	float radialVelocity; // dp per second away from origin

	// [80-95]
	vec2 linearAcceleration; // dp per second^2
	float acceleration; // along the velocity, dp per second^2
	float radialAcceleration; // away from origin, dp per second^2

	// [96-111]
	float tangentialAcceleration; // perpendicular to the direction from origin, dp per second^2
	float birthScale; // the scale chosen at birth; the scale curve multiplies it
	float damping; // the rate the speed loses per second, chosen at birth; 0 - no loss
	// the draw words the birth drew, XL_PARTICLE_DRAW_FLAG_* bits; the step reads none of them
	uint drawFlags;

	// [112-175]
	// the trail's seats, the newest first: seat 0 is the position of the step before the current
	// one, the last seat the oldest of the eight
	vec2 trail[XL_PARTICLE_TRAIL_SEATS];

	// [176]
};

#ifndef SP_GLSL
}
#endif

#endif /* XENOLITH_RENDERER_BASIC2D_GLSL_INCLUDE_XL2DGLSLPARTICLE_H_ */
