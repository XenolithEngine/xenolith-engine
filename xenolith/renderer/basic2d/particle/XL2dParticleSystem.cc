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

static sprt::atomic<uint64_t> s_particleSystemId = 1;

// The shader reads the flags through the XL_PARTICLE_FLAG_* defines
static_assert(toInt(ParticleSystemFlags::LocalCoords) == XL_PARTICLE_FLAG_LOCAL_COORDS);
static_assert(
		toInt(ParticleSystemFlags::AlignWithVelocity) == XL_PARTICLE_FLAG_ALIGN_WITH_VELOCITY);
static_assert(toInt(ParticleSystemFlags::OrderByLifetime) == XL_PARTICLE_FLAG_ORDER_BY_LIFETIME);
static_assert(toInt(ParticleSystemFlags::UseLifetimeMax) == XL_PARTICLE_FLAG_USE_LIFETIME_MAX);

static constexpr size_t ParticleExtraAlign = 8;

static size_t ParticleSystem_align(size_t size) {
	return (size + ParticleExtraAlign - 1) & ~(ParticleExtraAlign - 1);
}

static size_t ParticleSystem_getCurveSize(const CurveBuffer *curve) {
	if (!curve) {
		return 0;
	}
	return ParticleSystem_align(sizeof(ParticleCurveHeader)
			+ curve->getSize() * curve->getElementSize() * sizeof(float));
}

size_t ParticleSystemData::getExtraDataSize() const {
	return ParticleSystem_align(
				   sizeof(ParticleEmissionPoints) + emissionPoints.size() * sizeof(Vec2))
			+ ParticleSystem_getCurveSize(colorCurve) + ParticleSystem_getCurveSize(animFrameCurve);
}

void ParticleSystemData::writeExtraData(uint8_t *buf, size_t size,
		ParticleEmitterData &target) const {
	::__sprt_memset(buf, 0, size);

	size_t offset = 0;

	auto points = reinterpret_cast<ParticleEmissionPoints *>(buf);
	points->count = uint32_t(emissionPoints.size());
	::__sprt_memcpy(buf + sizeof(ParticleEmissionPoints), emissionPoints.data(),
			emissionPoints.size() * sizeof(Vec2));
	offset += ParticleSystem_align(
			sizeof(ParticleEmissionPoints) + emissionPoints.size() * sizeof(Vec2));

	auto writeCurve = [&](const CurveBuffer *curve) -> uint32_t {
		if (!curve) {
			return 0;
		}

		auto start = offset;
		auto header = reinterpret_cast<ParticleCurveHeader *>(buf + offset);
		header->count = uint32_t(curve->getSize());
		header->components = curve->getElementSize();
		::__sprt_memcpy(buf + offset + sizeof(ParticleCurveHeader), curve->getData(),
				header->count * header->components * sizeof(float));
		offset += ParticleSystem_getCurveSize(curve);
		return uint32_t(start);
	};

	target.colorCurveOffset = writeCurve(colorCurve);
	target.animFrameCurveOffset = writeCurve(animFrameCurve);
}

void ParticleSystemData::writeParticleSize(ParticleEmitterData &data, Size2 size) {
	auto v = Vec2(size.width, size.height);
	data.sizeValue = (v / 2.0f).length();
	data.sizeNormal = (data.sizeValue > 0.0f) ? v.getNormalized() : Vec2::ZERO;
}

bool ParticleSystem::init() {
	_id = s_particleSystemId.fetch_add(1);
	_data = Rc<ParticleSystemData>::alloc();
	_data->systemId = _id;

	auto &d = _data->data;
	d.count = 8;
	d.emissionType = toInt(ParticleEmissionType::Points);
	d.frameInterval = 16'667;
	d.dt = d.frameInterval / 1'000'000.0f;
	d.lifetime.init = 1.0f;
	d.scale.init = 1.0f;
	d.color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
	d.normal.init = -float(M_PI_4);
	d.normal.rnd = float(M_PI_2);

	return true;
}

bool ParticleSystem::init(uint32_t count, uint32_t frameInterval, float lifetime) {
	if (!init()) {
		return false;
	}

	auto &d = _data->data;
	d.count = count;
	d.frameInterval = frameInterval;
	d.dt = frameInterval / 1'000'000.0f;
	d.lifetime.init = lifetime;
	return true;
}

bool ParticleSystem::init(const Value &value) {
	if (!init()) {
		return false;
	}

	apply(value);
	return true;
}

static Vec2 ParticleSystem_readVec2(const Value &v, Vec2 def) {
	if (!v.isArray() || v.size() < 2) {
		return def;
	}
	return Vec2(float(v.getDouble(0, def.x)), float(v.getDouble(1, def.y)));
}

static Value ParticleSystem_writeVec2(Vec2 v) {
	Value ret(Value::Type::ARRAY);
	ret.addDouble(v.x);
	ret.addDouble(v.y);
	return ret;
}

static void ParticleSystem_readRange(const Value &value, StringView key, float min, float max,
		const Callback<void(float, float)> &setter) {
	// Keyed checks: an absent key reads as Value::Null, and that counts as a basic type
	if (value.isDictionary(key)) {
		const auto &v = value.getValue(key);
		auto newMin = float(v.getDouble("min", min));
		auto newMax = float(v.getDouble("max", newMin + (max - min)));
		setter(newMin, newMax - newMin);
	} else if (value.isInteger(key) || value.isDouble(key)) {
		setter(float(value.getDouble(key)), 0.0f);
	}
}

static void ParticleSystem_readRange(const Value &value, StringView key, Vec2 min, Vec2 max,
		const Callback<void(Vec2, Vec2)> &setter) {
	const auto &v = value.getValue(key);
	if (v.isDictionary()) {
		auto newMin = ParticleSystem_readVec2(v.getValue("min"), min);
		auto newMax = ParticleSystem_readVec2(v.getValue("max"), newMin + (max - min));
		setter(newMin, newMax - newMin);
	} else if (v.isArray()) {
		setter(ParticleSystem_readVec2(v, min), Vec2::ZERO);
	}
}

static Value ParticleSystem_writeRange(float min, float max) {
	Value ret;
	ret.setDouble(min, "min");
	ret.setDouble(max, "max");
	return ret;
}

static Value ParticleSystem_writeRange(Vec2 min, Vec2 max) {
	Value ret;
	ret.setValue(ParticleSystem_writeVec2(min), "min");
	ret.setValue(ParticleSystem_writeVec2(max), "max");
	return ret;
}

static Rc<CurveBuffer> ParticleSystem_readCurve(const Value &v) {
	const auto &typeName = v.getString("type");
	CurveBufferType type = CurveBufferType::Float;
	if (typeName == "float") {
		type = CurveBufferType::Float;
	} else if (typeName == "vec2") {
		type = CurveBufferType::Vec2;
	} else if (typeName == "vec3") {
		type = CurveBufferType::Vec3;
	} else if (typeName == "vec4") {
		type = CurveBufferType::Vec4;
	} else {
		return nullptr;
	}

	Vector<float> values;
	for (auto &it : v.getArray("values")) { values.emplace_back(float(it.getDouble())); }
	return Rc<CurveBuffer>::create(type, SpanView<float>(values));
}

static Value ParticleSystem_writeCurve(const CurveBuffer *curve) {
	Value ret;
	switch (curve->getType()) {
	case CurveBufferType::Float: ret.setString("float", "type"); break;
	case CurveBufferType::Vec2: ret.setString("vec2", "type"); break;
	case CurveBufferType::Vec3: ret.setString("vec3", "type"); break;
	case CurveBufferType::Vec4: ret.setString("vec4", "type"); break;
	}

	Value values(Value::Type::ARRAY);
	auto data = curve->getData();
	auto n = curve->getSize() * curve->getElementSize();
	for (size_t i = 0; i < n; ++i) { values.addDouble(data[i]); }
	ret.setValue(sp::move(values), "values");
	return ret;
}

struct ParticleSystemFlagName {
	ParticleSystemFlags flag;
	StringView name;
};

static constexpr ParticleSystemFlagName s_particleFlagNames[] = {
	{ParticleSystemFlags::LocalCoords, "localCoords"},
	{ParticleSystemFlags::AlignWithVelocity, "alignWithVelocity"},
	{ParticleSystemFlags::OrderByLifetime, "orderByLifetime"},
	{ParticleSystemFlags::UseLifetimeMax, "useLifetimeMax"},
};

void ParticleSystem::apply(const Value &value) {
	if (!value.isDictionary()) {
		return;
	}

	if (value.isBasicType("count")) {
		setCount(uint32_t(sprt::max(value.getInteger("count"), int64_t(0))));
	}
	if (value.isBasicType("frameInterval")) {
		setFrameInterval(uint32_t(sprt::max(value.getInteger("frameInterval"), int64_t(1))));
	}
	if (value.isBasicType("explosiveness")) {
		setExplosiveness(float(value.getDouble("explosiveness")));
	}
	if (value.isBasicType("randomness")) {
		setRandomness(float(value.getDouble("randomness")));
	}
	if (value.isArray("particleSize")) {
		auto s = ParticleSystem_readVec2(value.getValue("particleSize"), Vec2::ZERO);
		setParticleSize(Size2(s.x, s.y));
	}
	if (value.isArray("origin")) {
		setOrigin(ParticleSystem_readVec2(value.getValue("origin"), getOrigin()));
	}
	if (value.isArray("color")) {
		const auto &c = value.getValue("color");
		auto cur = getColor();
		setColor(Color4F(float(c.getDouble(0, cur.r)), float(c.getDouble(1, cur.g)),
				float(c.getDouble(2, cur.b)), float(c.getDouble(3, cur.a))));
	}
	if (value.isArray("emissionPoints")) {
		Vector<Vec2> points;
		for (auto &it : value.getArray("emissionPoints")) {
			points.emplace_back(ParticleSystem_readVec2(it, Vec2::ZERO));
		}
		setEmissionPoints(points);
	}

	// clang-format off
	ParticleSystem_readRange(value, "normal", getNormalMin(), getNormalMax(),
			[&](float v, float r) { setNormal(v, r); });
	ParticleSystem_readRange(value, "lifetime", getLifetimeMin(), getLifetimeMax(),
			[&](float v, float r) { setLifetime(v, r); });
	ParticleSystem_readRange(value, "scale", getScaleMin(), getScaleMax(),
			[&](float v, float r) { setScale(v, r); });
	ParticleSystem_readRange(value, "angle", getAngleMin(), getAngleMax(),
			[&](float v, float r) { setAngle(v, r); });
	ParticleSystem_readRange(value, "velocity", getVelocityMin(), getVelocityMax(),
			[&](float v, float r) { setVelocity(v, r); });
	ParticleSystem_readRange(value, "linearVelocity", getLinearVelocityMin(), getLinearVelocityMax(),
			[&](Vec2 v, Vec2 r) { setLinearVelocity(v, r); });
	ParticleSystem_readRange(value, "angularVelocity", getAngularVelocityMin(),
			getAngularVelocityMax(), [&](float v, float r) { setAngularVelocity(v, r); });
	ParticleSystem_readRange(value, "orbitalVelocity", getOrbitalVelocityMin(),
			getOrbitalVelocityMax(), [&](float v, float r) { setOrbitalVelocity(v, r); });
	ParticleSystem_readRange(value, "radialVelocity", getRadialVelocityMin(),
			getRadialVelocityMax(), [&](float v, float r) { setRadialVelocity(v, r); });
	ParticleSystem_readRange(value, "acceleration", getAccelerationMin(), getAccelerationMax(),
			[&](float v, float r) { setAcceleration(v, r); });
	ParticleSystem_readRange(value, "linearAcceleration", getLinearAccelerationMin(),
			getLinearAccelerationMax(), [&](Vec2 v, Vec2 r) { setLinearAcceleration(v, r); });
	ParticleSystem_readRange(value, "radialAcceleration", getRadialAccelerationMin(),
			getRadialAccelerationMax(), [&](float v, float r) { setRadialAcceleration(v, r); });
	ParticleSystem_readRange(value, "tangentialAcceleration", getTangentialAccelerationMin(),
			getTangentialAccelerationMax(), [&](float v, float r) { setTangentialAcceleration(v, r); });
	ParticleSystem_readRange(value, "hue", getHueMin(), getHueMax(),
			[&](float v, float r) { setHue(v, r); });
	// clang-format on

	if (value.hasValue("colorCurve")) {
		const auto &v = value.getValue("colorCurve");
		setColorCurve(v.isDictionary() ? ParticleSystem_readCurve(v).get() : nullptr);
	}
	if (value.hasValue("animFrameCurve")) {
		const auto &v = value.getValue("animFrameCurve");
		setAnimFrameCurve(v.isDictionary() ? ParticleSystem_readCurve(v).get() : nullptr);
	}

	if (value.isArray("flags")) {
		auto flags = ParticleSystemFlags::None;
		for (auto &it : value.getArray("flags")) {
			for (auto &f : s_particleFlagNames) {
				if (it.getString() == f.name) {
					flags |= f.flag;
				}
			}
		}
		setFlags(flags);
	}

	if (value.isBasicType("seed")) {
		setSeed(uint32_t(value.getInteger("seed")));
	} else if (value.hasValue("seed")) {
		clearSeed();
	}
}

Value ParticleSystem::encode() const {
	Value ret;
	ret.setInteger(getCount(), "count");
	ret.setInteger(getFrameInterval(), "frameInterval");
	ret.setDouble(getExplosiveness(), "explosiveness");
	ret.setDouble(getRandomness(), "randomness");

	auto size = getParticleSize();
	ret.setValue(ParticleSystem_writeVec2(Vec2(size.width, size.height)), "particleSize");
	ret.setValue(ParticleSystem_writeVec2(getOrigin()), "origin");

	auto color = getColor();
	Value c(Value::Type::ARRAY);
	c.addDouble(color.r);
	c.addDouble(color.g);
	c.addDouble(color.b);
	c.addDouble(color.a);
	ret.setValue(sp::move(c), "color");

	Value points(Value::Type::ARRAY);
	for (auto &it : _data->emissionPoints) { points.addValue(ParticleSystem_writeVec2(it)); }
	ret.setValue(sp::move(points), "emissionPoints");

	ret.setValue(ParticleSystem_writeRange(getNormalMin(), getNormalMax()), "normal");
	ret.setValue(ParticleSystem_writeRange(getLifetimeMin(), getLifetimeMax()), "lifetime");
	ret.setValue(ParticleSystem_writeRange(getScaleMin(), getScaleMax()), "scale");
	ret.setValue(ParticleSystem_writeRange(getAngleMin(), getAngleMax()), "angle");
	ret.setValue(ParticleSystem_writeRange(getVelocityMin(), getVelocityMax()), "velocity");
	ret.setValue(ParticleSystem_writeRange(getLinearVelocityMin(), getLinearVelocityMax()),
			"linearVelocity");
	ret.setValue(ParticleSystem_writeRange(getAngularVelocityMin(), getAngularVelocityMax()),
			"angularVelocity");
	ret.setValue(ParticleSystem_writeRange(getOrbitalVelocityMin(), getOrbitalVelocityMax()),
			"orbitalVelocity");
	ret.setValue(ParticleSystem_writeRange(getRadialVelocityMin(), getRadialVelocityMax()),
			"radialVelocity");
	ret.setValue(ParticleSystem_writeRange(getAccelerationMin(), getAccelerationMax()),
			"acceleration");
	ret.setValue(ParticleSystem_writeRange(getLinearAccelerationMin(), getLinearAccelerationMax()),
			"linearAcceleration");
	ret.setValue(ParticleSystem_writeRange(getRadialAccelerationMin(), getRadialAccelerationMax()),
			"radialAcceleration");
	ret.setValue(ParticleSystem_writeRange(getTangentialAccelerationMin(),
						 getTangentialAccelerationMax()),
			"tangentialAcceleration");
	ret.setValue(ParticleSystem_writeRange(getHueMin(), getHueMax()), "hue");

	if (_data->colorCurve) {
		ret.setValue(ParticleSystem_writeCurve(_data->colorCurve), "colorCurve");
	}
	if (_data->animFrameCurve) {
		ret.setValue(ParticleSystem_writeCurve(_data->animFrameCurve), "animFrameCurve");
	}

	Value flags(Value::Type::ARRAY);
	for (auto &f : s_particleFlagNames) {
		if (hasFlag(getFlags(), f.flag)) {
			flags.addString(f.name);
		}
	}
	ret.setValue(sp::move(flags), "flags");

	if (_data->hasSeed) {
		ret.setInteger(_data->seed, "seed");
	}
	return ret;
}

void ParticleSystem::setParticleSize(Size2 size) {
	auto &d = mutate();
	d.particleSize = size;
	ParticleSystemData::writeParticleSize(d.data, size);
}

Size2 ParticleSystem::getParticleSize() const { return _data->particleSize; }

void ParticleSystem::setCount(uint32_t c) { mutate().data.count = c; }

uint32_t ParticleSystem::getCount() const { return _data->data.count; }

void ParticleSystem::setExplosiveness(float f) { mutate().data.explosiveness = f; }

float ParticleSystem::getExplosiveness() const { return _data->data.explosiveness; }

void ParticleSystem::setRandomness(float v) { mutate().data.randomness = v; }

float ParticleSystem::getRandomness() const { return _data->data.randomness; }

void ParticleSystem::setFrameInterval(uint32_t f) {
	auto &d = mutate();
	d.data.frameInterval = f;
	d.data.dt = f / 1'000'000.0f;
}

uint32_t ParticleSystem::getFrameInterval() const { return _data->data.frameInterval; }

float ParticleSystem::getDt() const { return _data->data.dt; }

void ParticleSystem::setEmissionPoints(SpanView<Vec2> points) {
	auto &d = mutate();
	d.data.emissionType = toInt(ParticleEmissionType::Points);
	d.emissionPoints = points.vec<Interface>();
}

SpanView<Vec2> ParticleSystem::getEmissionPoints() const { return _data->emissionPoints; }

void ParticleSystem::setOrigin(Vec2 origin) { mutate().data.origin = origin; }

Vec2 ParticleSystem::getOrigin() const { return _data->data.origin; }

void ParticleSystem::setColor(const Color4F &color) {
	mutate().data.color = Vec4(color.r, color.g, color.b, color.a);
}

Color4F ParticleSystem::getColor() const {
	auto &c = _data->data.color;
	return Color4F(c.x, c.y, c.z, c.w);
}

#define XL_PARTICLE_FLOAT_PARAM(Name, field) \
	void ParticleSystem::set##Name(float value, float rnd) { \
		auto &d = mutate(); \
		d.data.field.init = value; \
		d.data.field.rnd = rnd; \
	} \
	float ParticleSystem::get##Name##Min() const { return _data->data.field.init; } \
	float ParticleSystem::get##Name##Max() const { \
		return _data->data.field.init + _data->data.field.rnd; \
	}

#define XL_PARTICLE_VEC2_PARAM(Name, field) \
	void ParticleSystem::set##Name(Vec2 value, Vec2 rnd) { \
		auto &d = mutate(); \
		d.data.field.init = value; \
		d.data.field.rnd = rnd; \
	} \
	Vec2 ParticleSystem::get##Name##Min() const { return _data->data.field.init; } \
	Vec2 ParticleSystem::get##Name##Max() const { \
		return _data->data.field.init + _data->data.field.rnd; \
	}

XL_PARTICLE_FLOAT_PARAM(Normal, normal)
XL_PARTICLE_FLOAT_PARAM(Lifetime, lifetime)
XL_PARTICLE_FLOAT_PARAM(Scale, scale)
XL_PARTICLE_FLOAT_PARAM(Angle, angle)
XL_PARTICLE_FLOAT_PARAM(Velocity, velocity)
XL_PARTICLE_VEC2_PARAM(LinearVelocity, linearVelocity)
XL_PARTICLE_FLOAT_PARAM(AngularVelocity, angularVelocity)
XL_PARTICLE_FLOAT_PARAM(OrbitalVelocity, orbitalVelocity)
XL_PARTICLE_FLOAT_PARAM(RadialVelocity, radialVelocity)
XL_PARTICLE_FLOAT_PARAM(Acceleration, acceleration)
XL_PARTICLE_VEC2_PARAM(LinearAcceleration, linearAcceleration)
XL_PARTICLE_FLOAT_PARAM(RadialAcceleration, radialAcceleration)
XL_PARTICLE_FLOAT_PARAM(TangentialAcceleration, tangentialAcceleration)
XL_PARTICLE_FLOAT_PARAM(Hue, hue)

#undef XL_PARTICLE_FLOAT_PARAM
#undef XL_PARTICLE_VEC2_PARAM

bool ParticleSystem::setColorCurve(CurveBuffer *curve) {
	if (curve && curve->getType() != CurveBufferType::Vec4) {
		log::source().error("ParticleSystem", "Color curve must be of Vec4 type");
		return false;
	}

	mutate().colorCurve = curve;
	return true;
}

CurveBuffer *ParticleSystem::getColorCurve() const { return _data->colorCurve; }

bool ParticleSystem::setAnimFrameCurve(CurveBuffer *curve) {
	if (curve && curve->getType() != CurveBufferType::Float) {
		log::source().error("ParticleSystem", "Animation frame curve must be of Float type");
		return false;
	}

	mutate().animFrameCurve = curve;
	return true;
}

CurveBuffer *ParticleSystem::getAnimFrameCurve() const { return _data->animFrameCurve; }

void ParticleSystem::addFlags(ParticleSystemFlags flags) { setFlags(getFlags() | flags); }

void ParticleSystem::clearFlags(ParticleSystemFlags flags) { setFlags(getFlags() & ~flags); }

void ParticleSystem::setFlags(ParticleSystemFlags flags) {
	const auto changed = getFlags() ^ flags;

	auto &d = mutate();
	d.data.flags = toInt(flags);

	// Living particles are in the old space
	if (hasFlag(changed, ParticleSystemFlags::LocalCoords)) {
		++d.restartGeneration;
	}
}

ParticleSystemFlags ParticleSystem::getFlags() const {
	return ParticleSystemFlags(_data->data.flags);
}

void ParticleSystem::restart() {
	copy();
	++_data->restartGeneration;
}

void ParticleSystem::setSeed(uint32_t seed) {
	copy();
	_data->seed = seed;
	_data->hasSeed = true;
	++_data->restartGeneration;
}

void ParticleSystem::clearSeed() {
	copy();
	_data->seed = 0;
	_data->hasSeed = false;
	++_data->restartGeneration;
}

uint32_t ParticleSystem::getSeed() const { return _data->seed; }

bool ParticleSystem::hasSeed() const { return _data->hasSeed; }

uint64_t ParticleSystem::getParamsGeneration() const { return _data->paramsGeneration; }

uint64_t ParticleSystem::getRestartGeneration() const { return _data->restartGeneration; }

Rc<ParticleSystemData> ParticleSystem::pop() {
	_copyOnWrite = true;
	return _data;
}

Rc<ParticleSystemData> ParticleSystem::dup() {
	auto data = Rc<ParticleSystemData>::alloc();
	data->data = _data->data;
	data->colorCurve = _data->colorCurve;
	data->animFrameCurve = _data->animFrameCurve;
	data->emissionPoints = _data->emissionPoints;
	data->systemId = _data->systemId;
	data->paramsGeneration = _data->paramsGeneration;
	data->restartGeneration = _data->restartGeneration;
	data->seed = _data->seed;
	data->hasSeed = _data->hasSeed;
	data->particleSize = _data->particleSize;
	return data;
}

bool ParticleSystem::isDirty() const { return !_copyOnWrite; }

ParticleSystemData &ParticleSystem::mutate() {
	copy();
	++_data->paramsGeneration;
	return *_data;
}

void ParticleSystem::copy() {
	if (_copyOnWrite) {
		_data = dup();
		_copyOnWrite = false;
	}
}

} // namespace stappler::xenolith::basic2d
