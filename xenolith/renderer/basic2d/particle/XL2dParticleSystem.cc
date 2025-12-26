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

#include "XL2dParticleSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

static std::atomic<uint64_t> s_particleSystemId = 1;

bool ParticleSystem::init(uint32_t count, uint32_t frameInterval, float lifetime) {
	_id = s_particleSystemId.fetch_add(1);
	_data = Rc<ParticleSystemData>::alloc();

	_data->data.count = count;
	_data->data.frameInterval = frameInterval;
	_data->data.dt = frameInterval / 1'000'000.0f;
	_data->data.lifetime.init = lifetime;
	_data->data.explosiveness = 0.0f;

	return true;
}

void ParticleSystem::setParticleSize(Size2 size) {
	if (_copyOnWrite) {
		copy();
	}

	_data->data.sizeValue = Vec2(size / 2.0f).length();
	_data->data.sizeNormal = Vec2(size).getNormalized();
}

void ParticleSystem::setCount(uint32_t c) {
	if (_copyOnWrite) {
		copy();
	}

	_data->data.count = c;
}

uint32_t ParticleSystem::getCount() const { return _data->data.count; }

void ParticleSystem::setExplosiveness(float f) {
	if (_copyOnWrite) {
		copy();
	}

	_data->data.explosiveness = f;
}

float ParticleSystem::getExplosiveness() const { return _data->data.explosiveness; }

void ParticleSystem::setFrameInterval(uint32_t f) {
	if (_copyOnWrite) {
		copy();
	}

	_data->data.frameInterval = f;
	_data->data.dt = f / 1'000'000.0f;
}

void ParticleSystem::setRandomness(float v) {
	if (_copyOnWrite) {
		copy();
	}

	_data->data.randomness = v;
}

float ParticleSystem::getRandomness() const { return _data->data.randomness; }

uint32_t ParticleSystem::getFrameInterval() const { return _data->data.frameInterval; }

float ParticleSystem::getDt() const { return _data->data.dt; }

void ParticleSystem::setEmissionPoints(SpanView<Vec2>) {
	if (_copyOnWrite) {
		copy();
	}

	_data->data.emissionType = toInt(ParticleEmissionType::Points);
}

void ParticleSystem::setNormal(float angle, float rnd) {
	if (_copyOnWrite) {
		copy();
	}

	_data->data.normal.init = angle;
	_data->data.normal.rnd = rnd;
}

float ParticleSystem::getNormalMin() const { return _data->data.normal.init; }

float ParticleSystem::getNormalMax() const {
	return _data->data.normal.init + _data->data.normal.rnd;
}

void ParticleSystem::setLifetime(float lifetime, float rnd) {
	if (_copyOnWrite) {
		copy();
	}

	_data->data.lifetime.init = lifetime;
	_data->data.lifetime.rnd = rnd;
}

float ParticleSystem::getLifetimeMin() const { return _data->data.lifetime.init; }

float ParticleSystem::getLifetimeMax() const {
	return _data->data.lifetime.init + _data->data.lifetime.rnd;
}

void ParticleSystem::setScale(float scale, float rnd) {
	if (_copyOnWrite) {
		copy();
	}

	_data->data.scale.init = scale;
	_data->data.scale.rnd = rnd;
}

float ParticleSystem::getScaleMin() const { return _data->data.scale.init; }

float ParticleSystem::getScaleMax() const { return _data->data.scale.init + _data->data.scale.rnd; }

void ParticleSystem::setAngle(float angle, float rnd) {
	if (_copyOnWrite) {
		copy();
	}

	_data->data.angle.init = angle;
	_data->data.angle.rnd = rnd;
}

float ParticleSystem::getAngleMin() const { return _data->data.angle.init; }

float ParticleSystem::getAngleMax() const { return _data->data.angle.init + _data->data.angle.rnd; }

void ParticleSystem::setVelocity(float velocity, float rnd) {
	if (_copyOnWrite) {
		copy();
	}

	_data->data.velocity.init = velocity;
	_data->data.velocity.rnd = rnd;
}

float ParticleSystem::getVelocityMin() const { return _data->data.velocity.init; }

float ParticleSystem::getVelocityMax() const {
	return _data->data.velocity.init + _data->data.velocity.rnd;
}

void ParticleSystem::setLinearVelocity(Vec2 velocity, Vec2 rnd) {
	if (_copyOnWrite) {
		copy();
	}

	_data->data.linearVelocity.init = velocity;
	_data->data.linearVelocity.rnd = rnd;
}

Vec2 ParticleSystem::getLinearVelocityMin() const { return _data->data.linearVelocity.init; }

Vec2 ParticleSystem::getLinearVelocityMax() const {
	return _data->data.linearVelocity.init + _data->data.linearVelocity.rnd;
}

void ParticleSystem::setAngularVelocity(float velocity, float rnd) {
	if (_copyOnWrite) {
		copy();
	}

	_data->data.angularVelocity.init = velocity;
	_data->data.angularVelocity.rnd = rnd;
}

float ParticleSystem::getAngularVelocityMin() const { return _data->data.angularVelocity.init; }

float ParticleSystem::getAngularVelocityMax() const {
	return _data->data.angularVelocity.init + _data->data.angularVelocity.rnd;
}

void ParticleSystem::setOrbitalVelocity(float velocity, float rnd) {
	if (_copyOnWrite) {
		copy();
	}

	_data->data.orbitalVelocity.init = velocity;
	_data->data.orbitalVelocity.rnd = rnd;
}

float ParticleSystem::getOrbitalVelocityMin() const { return _data->data.orbitalVelocity.init; }

float ParticleSystem::getOrbitalVelocityMax() const {
	return _data->data.orbitalVelocity.init + _data->data.orbitalVelocity.rnd;
}

void ParticleSystem::setRadialVelocity(float velocity, float rnd) {
	if (_copyOnWrite) {
		copy();
	}

	_data->data.radialVelocity.init = velocity;
	_data->data.radialVelocity.rnd = rnd;
}

float ParticleSystem::getRadialVelocityMin() const { return _data->data.radialVelocity.init; }

float ParticleSystem::getRadialVelocityMax() const {
	return _data->data.radialVelocity.init + _data->data.radialVelocity.rnd;
}

void ParticleSystem::setAcceleration(float accel, float rnd) {
	if (_copyOnWrite) {
		copy();
	}

	_data->data.acceleration.init = accel;
	_data->data.acceleration.rnd = rnd;
}

float ParticleSystem::getAccelerationMin() const { return _data->data.acceleration.init; }

float ParticleSystem::getAccelerationMax() const {
	return _data->data.acceleration.init + _data->data.acceleration.rnd;
}

void ParticleSystem::setLinearAcceleration(Vec2 accel, Vec2 rnd) {
	if (_copyOnWrite) {
		copy();
	}

	_data->data.linearAcceleration.init = accel;
	_data->data.linearAcceleration.rnd = rnd;
}

Vec2 ParticleSystem::getLinearAccelerationMin() const {
	return _data->data.linearAcceleration.init;
}

Vec2 ParticleSystem::getLinearAccelerationMax() const {
	return _data->data.linearAcceleration.init + _data->data.linearAcceleration.rnd;
}

void ParticleSystem::setRadialAcceleration(float accel, float rnd) {
	if (_copyOnWrite) {
		copy();
	}

	_data->data.radialAcceleration.init = accel;
	_data->data.radialAcceleration.rnd = rnd;
}

float ParticleSystem::getRadialAccelerationMin() const {
	return _data->data.radialAcceleration.init;
}
float ParticleSystem::getRadialAccelerationMax() const {
	return _data->data.radialAcceleration.init + _data->data.radialAcceleration.rnd;
}

void ParticleSystem::setTangentialAcceleration(float accel, float rnd) {
	if (_copyOnWrite) {
		copy();
	}

	_data->data.tangentialAcceleration.init = accel;
	_data->data.tangentialAcceleration.rnd = rnd;
}

float ParticleSystem::getTangentialAccelerationMin() const {
	return _data->data.tangentialAcceleration.init;
}

float ParticleSystem::getTangentialAccelerationMax() const {
	return _data->data.tangentialAcceleration.init + _data->data.tangentialAcceleration.rnd;
}

void ParticleSystem::setColorCurve(NotNull<CurveBuffer> curve) {
	if (_copyOnWrite) {
		copy();
	}

	_data->colorCurve = curve;
}

CurveBuffer *ParticleSystem::getColorCurve() const { return _data->colorCurve; }

void ParticleSystem::addFlags(ParticleSystemFlags flags) { _data->data.flags |= toInt(flags); }

void ParticleSystem::clearFlags(ParticleSystemFlags flags) { _data->data.flags &= ~toInt(flags); }

void ParticleSystem::setFlags(ParticleSystemFlags flags) { _data->data.flags = toInt(flags); }

ParticleSystemFlags ParticleSystem::getFlags() const {
	return ParticleSystemFlags(_data->data.flags);
}

Rc<ParticleSystemData> ParticleSystem::pop() {
	_copyOnWrite = true;
	return _data;
}

Rc<ParticleSystemData> ParticleSystem::dup() {
	auto data = Rc<ParticleSystemData>::alloc();
	data->data = _data->data;
	data->colorCurve = _data->colorCurve;
	data->emissionPoints = _data->emissionPoints;
	return data;
}

bool ParticleSystem::isDirty() const { return !_copyOnWrite; }

void ParticleSystem::copy() {
	if (_copyOnWrite) {
		_data = dup();
		_copyOnWrite = false;
	}
}

} // namespace stappler::xenolith::basic2d
