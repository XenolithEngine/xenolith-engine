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

// ParticleEmitterData::flags, Effekseer's parent binding, the continuous child of Particle-39: the
// bit suppresses the emitter's own birth schedule - its births are seated by the host that knows
// the parent, one generation per living parent particle, so the step the bit flags never births on
// its own. The bit is host-set, unnamed in ParticleSystem's flag word list, so no document carries
// it: the child's parenting rides the document's envelope, not the system's words
#define XL_PARTICLE_FLAG_PARENT_BOUND 32

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

// The vertex seats one particle owns in the pass's vertex list: the quad's own pair of list
// triangles
#define XL_PARTICLE_QUAD_SEATS 6

// The ring's corner count, Effekseer's own ring page numbering its Vertex Count - "The number
// of corner vertices around the ring ... 3 makes a triangle, 4 a diamond" - the default the
// count word's zero carries: the emitter's own field, 3..64, seats its corners over it, and
// this word is the unseated state the old files' buffers still read
#define XL_PARTICLE_RING_CORNERS 16

struct ParticleConstantData {
	uvec2 frameDataPointer;
	uvec2 outVerticesPointer;
	uint emitterIndex;
	// 1 - the strip's own dispatch, the draw alone: the update it reads already ran, the
	// barrier between the dispatches seats every particle's standing state, so the segment a
	// particle owns may read the next particle. 0 - the update dispatch, simulation and draw
	uint stripPass;
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
	// explicit padding: the member-wise size ends at 28 while std430 strides an array of the
	// struct at its vec2 alignment's 32, so the seat carries the word itself
	uint padding28;
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

	// [192-199]
	ParticleFloatParam hue;

	// [200-215]
	uint colorCurveOffset;
	uint animFrameCurveOffset;
	uint scaleCurveOffset;
	float dt;

	// [216-231]
	// the rate the speed loses per second, chosen at birth from the pair; 0 - no loss
	ParticleFloatParam damping;
	// upper limit of the speed, 0 - no limit
	float maxSpeed;
	uint padding228;

	// [232-251]
	// scale of the turbulence force, 0 - no turbulence
	float turbulenceStrength;
	// variations of the one-octave noise across its two axes, 0 - none
	float turbulenceFrequency;
	// slide of the noise pattern per second of simulation time, 0 - a static pattern
	float turbulenceShiftSpeed;
	// min and max of the influence ramp over the lifetime, the min at birth, the max at the last
	// step
	ParticleFloatParam turbulenceInfluence;

	// [252-255]
	// seconds between the births of adjacent slots, Effekseer's GenerationTime, 0 - the phase
	// schedule of count over the cycle
	float interval;

	// [256-319]
	// the attractor seats, in the order the file's list seats them; a seat of strength 0 is
	// unused
	ParticleAttractor attractors[4];

	// [320-323]
	// width of the trail strip the particle's seat history draws, 0 - no trail
	float trailWidth;

	// [324]
	// explicit padding: the deflector array's std430 seat is 8-aligned and would open at 328,
	// four bytes past the member-wise end, so the word is carried by hand
	uint padding324;

	// [328-455]
	// the deflector seats, in the order the file's list seats them; a seat of shape 0 is unused
	ParticleDeflector deflectors[4];

	// [456-459]
	// ratio of the node's velocity a newborn inherits, 0 - no inheritance
	float inheritVelocity;

	// [460]
	// explicit padding: uvScroll's std430 seat is 8-aligned and would open at 464, not 460
	uint padding460;

	// [464-475]
	// slide of the sampled UV window over the texture per second of simulation time, signed per
	// axis, (0, 0) - no slide
	vec2 uvScroll;
	// probability of the U mirror a birth draws from the particle's own generator, 0 - never
	float uvFlip;

	// [476-479]
	// seconds the birth schedule waits before the emitter's own timeline begins, 0 - no wait
	float start;

	// [480-487]
	// the second axis's own birth range, Effekseer's scale by axis; words equal to `scale` are
	// the absence this model keeps - the second axis takes the first's draw, the generator
	// stands unread, and the file writes no key
	ParticleFloatParam scaleY;

	// [488-503]
	// Effekseer's ring renderer, the stage's own draw: the ring's outer edge in dp, 0 - no
	// ring, the quad the pass always drew; the inner edge in dp, read only beside a seated
	// outer; the arc's first and last edge in degrees, the full circle 0 to 360
	float ringOuter;
	float ringInner;
	float ringArcFrom;
	float ringArcTo;

	// [504-507]
	// width of the strip the pass draws through the emitter's particles in their birth order,
	// Effekseer's ribbon taken in the screen plane, 0 - no strip, the quad the pass always drew
	float stripWidth;

	// [508-511]
	// offset of the position curve in the extra data buffer, a vec2 curve over the lifetime
	// fraction whose walk the step adds to the birth point every step past particleStep,
	// Effekseer's position of the start and the end point summed beside the velocity rather than
	// seated in its place; 0 - no curve, the position the step always left. The word sits in the
	// hand-carried padding between the strip and the ring's colors: the seat the strip's own
	// stage left, the strip word's own precedent
	uint positionCurveOffset;

	// [512-559]
	// the ring's three color bands, Effekseer's own "a triple structure of outside, center and
	// inside": the outer and inner colors fill their edges, the middle one blends between them
	// across the band's half at the fraction the ratio curve names, the even 0.5 the absent
	// curve keeps
	vec4 ringColorOuter;
	vec4 ringColorMiddle;
	vec4 ringColorInner;

	// [560-579]
	// offsets of the ring's five scalar curves in the extra data buffer, each a Float curve
	// over the lifetime fraction whose sample the draw reads in the ring word's place - the
	// outer and inner edges in dp, the arc's two degrees, the middle edge's fraction over the
	// band; 0 - no curve, the ring word the draw always read. The curves are draw words: the
	// step and every particle field stand untouched, the sample lands where the pass and the
	// shared helpers read the words, and the whole ring object's own words seat the fallbacks
	// the old files' buffers carry
	uint ringOuterCurveOffset;
	uint ringInnerCurveOffset;
	uint ringArcFromCurveOffset;
	uint ringArcToCurveOffset;
	uint ringRatioCurveOffset;

	// [580-583]
	// the ring's corner count, Effekseer's Vertex Count, 3..64; 0 - the default sixteen the
	// constant above carries, the unseated word the old buffers keep
	uint ringVertexCount;

	// [584-591]
	// explicit padding: the member-wise size ends at 584 while std430 strides the struct at
	// the color vec4's alignment of 16, so the tail is carried in the struct itself
	uvec2 padding584;

	// [592]
};

#ifndef SP_GLSL
// The word map above is the compiler's own, locked where it is declared: both texts share this
// struct, and the seating of a new word reads these offsets. The offsets past the attractors are
// the std430 seats first - a vec2-aligned array or member opens on an 8-byte boundary, and the
// explicit padding members hold the C++ text to the same numbers
static_assert(sizeof(ParticleEmitterData) == 592
		&& __builtin_offsetof(ParticleEmitterData, colorCurveOffset) == 200
		&& __builtin_offsetof(ParticleEmitterData, interval) == 252
		&& __builtin_offsetof(ParticleEmitterData, attractors) == 256
		&& __builtin_offsetof(ParticleEmitterData, deflectors) == 328
		&& __builtin_offsetof(ParticleEmitterData, inheritVelocity) == 456
		&& __builtin_offsetof(ParticleEmitterData, uvScroll) == 464
		&& __builtin_offsetof(ParticleEmitterData, uvFlip) == 472
		&& __builtin_offsetof(ParticleEmitterData, start) == 476
		&& __builtin_offsetof(ParticleEmitterData, scaleY) == 480
		&& __builtin_offsetof(ParticleEmitterData, ringOuter) == 488
		&& __builtin_offsetof(ParticleEmitterData, ringArcTo) == 500
		&& __builtin_offsetof(ParticleEmitterData, stripWidth) == 504
		&& __builtin_offsetof(ParticleEmitterData, positionCurveOffset) == 508
		&& __builtin_offsetof(ParticleEmitterData, ringColorOuter) == 512
		&& __builtin_offsetof(ParticleEmitterData, ringColorInner) == 544
		&& __builtin_offsetof(ParticleEmitterData, ringOuterCurveOffset) == 560
		&& __builtin_offsetof(ParticleEmitterData, ringInnerCurveOffset) == 564
		&& __builtin_offsetof(ParticleEmitterData, ringArcFromCurveOffset) == 568
		&& __builtin_offsetof(ParticleEmitterData, ringArcToCurveOffset) == 572
		&& __builtin_offsetof(ParticleEmitterData, ringRatioCurveOffset) == 576
		&& __builtin_offsetof(ParticleEmitterData, ringVertexCount) == 580);
// The deflector seat is the std430 array stride of the struct: 32, the vec2 alignment's own
// rounding of the member-wise 28
static_assert(sizeof(ParticleDeflector) == 32
		&& __builtin_offsetof(ParticleDeflector, radius) == 16
		&& __builtin_offsetof(ParticleDeflector, response) == 24);
#endif

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
	float scale; // quad size multiplier, the X axis's own; the pair's Y is scaleY
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
	// the X scale chosen at birth; the scale curve multiplies it, the pair's Y is birthScaleY
	float birthScale;
	float damping; // the rate the speed loses per second, chosen at birth; 0 - no loss
	// the draw words the birth drew, XL_PARTICLE_DRAW_FLAG_* bits; the step reads none of them
	uint drawFlags;

	// [112-119]
	// the second axis's pair since the per-axis scale: the multiplier its birth chose and the
	// drawn one the curve left standing. A birth the emitter seated no own second axis for
	// takes the X draw's own value in both, so the quad - and the hash - answer what they
	// always had
	float birthScaleY;
	float scaleY;

	// [120-183]
	// the trail's seats, the newest first: seat 0 is the position of the step before the current
	// one, the last seat the oldest of the eight
	vec2 trail[XL_PARTICLE_TRAIL_SEATS];

	// [184-191]
	// explicit padding: the member-wise size ends at 184 while std430 strides the particle
	// array at the color vec4's alignment of 192, so the tail is carried in the struct itself
	uvec2 padding184;

	// [192]
};

#ifndef SP_GLSL
// The particle's own seat, locked the same way: the CPU both seeds and reads the buffer the
// step strides over, so the two texts cannot part ways on the array stride
static_assert(sizeof(ParticleData) == 192
		&& __builtin_offsetof(ParticleData, trail) == 120);
#endif

#ifndef SP_GLSL
}
#endif

#endif /* XENOLITH_RENDERER_BASIC2D_GLSL_INCLUDE_XL2DGLSLPARTICLE_H_ */
