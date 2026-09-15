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
	uint padding88;
	uint padding92;

	// [96]
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

// Header of a curve in the emitter's extra data buffer, followed by count * components floats
struct ParticleCurveHeader {
	uint count;
	uint components;
};

struct ParticleEmitterData {
	// [0-15]
	// 0 - points
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
	uint padding36;
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
	uint padding216;
	float dt;

	// [224]
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
	uint padding100;
	uint padding104;
	uint padding108;

	// [112]
};

#ifndef SP_GLSL
}
#endif

#endif /* XENOLITH_RENDERER_BASIC2D_GLSL_INCLUDE_XL2DGLSLPARTICLE_H_ */
