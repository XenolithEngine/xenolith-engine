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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2DPARTICLESYSTEM_H_
#define XENOLITH_RENDERER_BASIC2D_XL2DPARTICLESYSTEM_H_

#include "XL2d.h"
#include "XLCoreMaterial.h"
#include "XLCurveBuffer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

enum class ParticleEmissionType : uint32_t {
	Points,
};

enum class ParticleSystemFlags : uint32_t {
	LocalCoords = 1 << 0,
	AlignWithVelocity = 1 << 1,
	OrderByLifetime = 1 << 2,
	UseLifetimeMax = 1 << 3,
};

struct ParticleSystemData : public Ref {
	ParticleEmitterData data;
	Rc<CurveBuffer> colorCurve;
	Vector<Vec2> emissionPoints;

	ParticleSystemData() { ::__sprt_memset(&data, 0, sizeof(ParticleEmitterData)); }
};

struct ParticleSystemRenderInfo {
	Rc<ParticleSystemData> system;
	core::MaterialId material = 0;
	uint32_t maxFramesPerCall = 0;
	uint32_t transform = 0;
	uint32_t index = 0;
};

SP_DEFINE_ENUM_AS_MASK(ParticleSystemFlags)

class ParticleSystem : public Ref {
public:
	virtual ~ParticleSystem() = default;

	bool init(uint32_t count, uint32_t frameInterval, float lifetime);

	uint64_t getId() const { return _id; }

	void setParticleSize(Size2);

	void setCount(uint32_t);
	uint32_t getCount() const;

	void setExplosiveness(float);
	float getExplosiveness() const;

	void setRandomness(float);
	float getRandomness() const;

	void setFrameInterval(uint32_t);
	uint32_t getFrameInterval() const;
	float getDt() const;

	void setEmissionPoints(SpanView<Vec2>);

	void setNormal(float angle, float rnd = 0.0f);
	float getNormalMin() const;
	float getNormalMax() const;

	void setLifetime(float lifetime, float rnd = 0.0f);
	float getLifetimeMin() const;
	float getLifetimeMax() const;

	void setScale(float scale, float rnd = 0.0f);
	float getScaleMin() const;
	float getScaleMax() const;

	void setAngle(float angle, float rnd = 0.0f);
	float getAngleMin() const;
	float getAngleMax() const;

	void setVelocity(float velocity, float rnd = 0.0f);
	float getVelocityMin() const;
	float getVelocityMax() const;

	void setLinearVelocity(Vec2 velocity, Vec2 rnd = Vec2::ZERO);
	Vec2 getLinearVelocityMin() const;
	Vec2 getLinearVelocityMax() const;

	void setAngularVelocity(float velocity, float rnd = 0.0f);
	float getAngularVelocityMin() const;
	float getAngularVelocityMax() const;

	void setOrbitalVelocity(float velocity, float rnd = 0.0f);
	float getOrbitalVelocityMin() const;
	float getOrbitalVelocityMax() const;

	void setRadialVelocity(float velocity, float rnd = 0.0f);
	float getRadialVelocityMin() const;
	float getRadialVelocityMax() const;

	void setAcceleration(float accel, float rnd = 0.0f);
	float getAccelerationMin() const;
	float getAccelerationMax() const;

	void setLinearAcceleration(Vec2 accel, Vec2 rnd = Vec2::ZERO);
	Vec2 getLinearAccelerationMin() const;
	Vec2 getLinearAccelerationMax() const;

	void setRadialAcceleration(float accel, float rnd = 0.0f);
	float getRadialAccelerationMin() const;
	float getRadialAccelerationMax() const;

	void setTangentialAcceleration(float accel, float rnd = 0.0f);
	float getTangentialAccelerationMin() const;
	float getTangentialAccelerationMax() const;

	void setColorCurve(NotNull<CurveBuffer>);
	CurveBuffer *getColorCurve() const;

	void addFlags(ParticleSystemFlags);
	void clearFlags(ParticleSystemFlags);
	void setFlags(ParticleSystemFlags);
	ParticleSystemFlags getFlags() const;

	Rc<ParticleSystemData> pop();
	Rc<ParticleSystemData> dup();

	bool isDirty() const;

protected:
	void copy();

	bool _copyOnWrite = false;
	uint64_t _id = 0;
	Rc<ParticleSystemData> _data;
};

} // namespace stappler::xenolith::basic2d

#endif /* XENOLITH_RENDERER_BASIC2D_XL2DPARTICLESYSTEM_H_ */
