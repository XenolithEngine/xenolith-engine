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

#include "SPGlsl.h"

#ifndef SP_GLSL
namespace STAPPLER_VERSIONIZED stappler::glsl {
#endif

struct ParticleConstantData {
	uvec2 outVerticesPointer;
	uvec2 outCommandPointer;
	uvec2 emitterPointer;
	uvec2 feedbackPointer;
	uint materialIndex;
	uint particleBufferIndex;

	float timeline; // (1.0 - explosiveness) * lifetime / count

	uint framesInGen;
	uint genframe;
	float gentime;
	float gendt;
	float dt;
	uint nframes;
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

struct ParticleData {
	// [0-15]
	pcg16_state_t rng;
	vec2 position;

	// [16-31]
	vec2 sizeNormal;
	vec2 angle;

	// [32-47]
	vec4 color;

	// [48-63]
	float sizeValue;
	uint fullLifetime;
	uint currentLifetime; // in frames
	uint padding60;

	// [64-79]
	vec2 normal;
	vec2 linearVelocity;

	// [80-95]
	float velocity;
	float hue;
	float animFrame;
	float qAcceleration; // квантованное по dt ускорение

	// [96-111]
	vec2 qAngularVelocity; // квантованная по dt скорость вращения
	float orbitalVelocity; // скорость вращения вокруг origin, радианы в секунду
	float radialVelocity; // скорость движения от origin, dp в секунду

	// [112-127]
	vec2 qLinearAcceleration; // квантованное по dt ускорение
	float radialAcceleration; // ускорение от origin, dp в секунду^2
	float tangentialAcceleration; // ускорение перпендикулярно движению, dp в секунду^2

	// [128]
};

struct ParticleFeedback {
	uint emissionCount;
	uint simulationCount;
	uint skippedCount;
	uint nframes;
	ParticleData emitted;
	uint written;
};

#ifndef SP_GLSL
}
#endif

#endif /* XENOLITH_RENDERER_BASIC2D_GLSL_INCLUDE_XL2DGLSLPARTICLE_H_ */
