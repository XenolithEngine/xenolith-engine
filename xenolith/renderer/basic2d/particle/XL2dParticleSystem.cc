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
static_assert(toInt(ParticleSystemFlags::OneShot) == XL_PARTICLE_FLAG_ONE_SHOT);

static_assert(toInt(ParticleEmissionType::Points) == XL_PARTICLE_EMISSION_POINTS);
static_assert(toInt(ParticleEmissionType::Disc) == XL_PARTICLE_EMISSION_DISC);
static_assert(toInt(ParticleEmissionType::Ring) == XL_PARTICLE_EMISSION_RING);
static_assert(toInt(ParticleEmissionType::Rect) == XL_PARTICLE_EMISSION_RECT);
static_assert(toInt(ParticleEmissionType::Segment) == XL_PARTICLE_EMISSION_SEGMENT);

static constexpr size_t ParticleExtraAlign = 8;

static size_t ParticleSystem_align(size_t size) {
	return (size + ParticleExtraAlign - 1) & ~(ParticleExtraAlign - 1);
}

// The floats the shape reads from its extra data block: disc 1, ring 4, rect 2, segment 4
static size_t ParticleSystem_shapeParamCount(ParticleEmissionType type) {
	switch (type) {
	case ParticleEmissionType::Disc: return 1;
	case ParticleEmissionType::Ring: return 4;
	case ParticleEmissionType::Rect: return 2;
	case ParticleEmissionType::Segment: return 4;
	case ParticleEmissionType::Points: break;
	}
	return 0;
}

static size_t ParticleSystem_getCurveSize(const CurveBuffer *curve) {
	if (!curve) {
		return 0;
	}
	return ParticleSystem_align(sizeof(ParticleCurveHeader)
			+ curve->getSize() * curve->getElementSize() * sizeof(float));
}

size_t ParticleSystemData::getExtraDataSize() const {
	size_t ret = ParticleSystem_align(
			sizeof(ParticleEmissionPoints) + emissionPoints.size() * sizeof(Vec2));
	if (data.emissionType != toInt(ParticleEmissionType::Points)) {
		ret += ParticleSystem_align(
				ParticleSystem_shapeParamCount(ParticleEmissionType(data.emissionType))
				* sizeof(float));
	}
	return ret + ParticleSystem_getCurveSize(colorCurve) + ParticleSystem_getCurveSize(animFrameCurve)
			+ ParticleSystem_getCurveSize(scaleCurve);
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

	// The birth shape's numbers, right after the points block - the words past the last point,
	// the base the shader computes on its own
	if (data.emissionType != toInt(ParticleEmissionType::Points)) {
		auto count = ParticleSystem_shapeParamCount(ParticleEmissionType(data.emissionType));
		::__sprt_memcpy(buf + offset, emissionShape, count * sizeof(float));
		offset += ParticleSystem_align(count * sizeof(float));
	}

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
	target.scaleCurveOffset = writeCurve(scaleCurve);
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
	// The fresh system's second axis is no axis of its own: the words equal to the first's are
	// the unseated state every later edit of the pair keeps until `scaleY` stands apart
	d.scaleY = d.scale;
	d.color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
	d.normal.init = -float(M_PI_4);
	d.normal.rnd = float(M_PI_2);

	// The fresh system's turbulence: no force, one variation of the noise per lifetime, a static
	// pattern, full influence
	d.turbulenceFrequency = 1.0f;
	d.turbulenceInfluence.init = 1.0f;

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
	{ParticleSystemFlags::OneShot, "oneShot"},
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
	if (value.isDictionary("emissionShape")) {
		const auto &shape = value.getValue("emissionShape");
		auto type = shape.getString("type");
		float params[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		ParticleEmissionType parsed = ParticleEmissionType::Points;
		if (type == "disc") {
			parsed = ParticleEmissionType::Disc;
			params[0] = float(shape.getDouble("radius", 0.0));
		} else if (type == "ring") {
			parsed = ParticleEmissionType::Ring;
			params[0] = float(shape.getDouble("outer", 0.0));
			params[1] = float(shape.getDouble("inner", 0.0));
			params[2] = float(shape.getDouble("from", 0.0));
			params[3] = float(shape.getDouble("to", 0.0));
		} else if (type == "rect") {
			parsed = ParticleEmissionType::Rect;
			auto half = ParticleSystem_readVec2(shape.getValue("half"), Vec2::ZERO);
			params[0] = half.x;
			params[1] = half.y;
		} else if (type == "segment") {
			parsed = ParticleEmissionType::Segment;
			auto from = ParticleSystem_readVec2(shape.getValue("from"), Vec2::ZERO);
			auto to = ParticleSystem_readVec2(shape.getValue("to"), Vec2::ZERO);
			params[0] = from.x;
			params[1] = from.y;
			params[2] = to.x;
			params[3] = to.y;
		}
		// A shape word the engine does not know leaves the system untouched, so encode()
		// drops the key and the studio door refuses the document
		if (parsed != ParticleEmissionType::Points) {
			setEmissionShape(parsed, params);
		}
	}

	// clang-format off
	ParticleSystem_readRange(value, "normal", getNormalMin(), getNormalMax(),
			[&](float v, float r) { setNormal(v, r); });
	ParticleSystem_readRange(value, "lifetime", getLifetimeMin(), getLifetimeMax(),
			[&](float v, float r) { setLifetime(v, r); });
	ParticleSystem_readRange(value, "scale", getScaleMin(), getScaleMax(),
			[&](float v, float r) { setScale(v, r); });
	// The second axis's own key: absent, the setter is never called and the words stay equal to
	// the first axis's - the unseated state the birth and encode() keep
	ParticleSystem_readRange(value, "scaleY", getScaleYMin(), getScaleYMax(),
			[&](float v, float r) { setScaleY(v, r); });
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
	ParticleSystem_readRange(value, "damping", getDampingMin(), getDampingMax(),
			[&](float v, float r) { setDamping(v, r); });
	ParticleSystem_readRange(value, "turbulenceInfluence", getTurbulenceInfluenceMin(),
			getTurbulenceInfluenceMax(),
			[&](float v, float r) { setTurbulenceInfluence(v, r); });
	// clang-format on

	if (value.hasValue("colorCurve")) {
		const auto &v = value.getValue("colorCurve");
		setColorCurve(v.isDictionary() ? ParticleSystem_readCurve(v).get() : nullptr);
	}
	if (value.hasValue("animFrameCurve")) {
		const auto &v = value.getValue("animFrameCurve");
		setAnimFrameCurve(v.isDictionary() ? ParticleSystem_readCurve(v).get() : nullptr);
	}
	if (value.hasValue("scaleCurve")) {
		const auto &v = value.getValue("scaleCurve");
		setScaleCurve(v.isDictionary() ? ParticleSystem_readCurve(v).get() : nullptr);
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

	if (value.isBasicType("warm")) {
		setWarm(float(value.getDouble("warm")));
	}

	if (value.isBasicType("stretch")) {
		setStretch(float(value.getDouble("stretch")));
	}

	if (value.isBasicType("maxSpeed")) {
		setMaxSpeed(float(value.getDouble("maxSpeed")));
	}

	if (value.isBasicType("turbulenceStrength")) {
		setTurbulenceStrength(float(value.getDouble("turbulenceStrength")));
	}
	if (value.isBasicType("turbulenceFrequency")) {
		setTurbulenceFrequency(float(value.getDouble("turbulenceFrequency")));
	}
	if (value.isBasicType("turbulenceShiftSpeed")) {
		setTurbulenceShiftSpeed(float(value.getDouble("turbulenceShiftSpeed")));
	}

	if (value.isArray("attractors")) {
		// The seats take one entry each, in list order: a strength of zero seats nothing - the
		// entry is dropped, not refused - and the fourth seat is the last, a fifth entry is the
		// studio door's own refusal before it ever reaches this loop
		ParticleAttractor seated[4] = {};
		uint32_t count = 0;
		for (auto &it : value.getArray("attractors")) {
			if (!it.isDictionary()) {
				continue;
			}
			auto strength = it.getDouble("strength", 0.0);
			if (strength == 0.0) {
				continue;
			}
			if (count == 4) {
				break;
			}
			seated[count] = ParticleAttractor{ParticleSystem_readVec2(it.getValue("point"),
												  Vec2::ZERO),
					float(strength), float(it.getDouble("radius", 0.0))};
			++count;
		}
			setAttractors(SpanView<ParticleAttractor>(seated, count));
		}

		if (value.isBasicType("trailWidth")) {
			setTrailWidth(float(value.getDouble("trailWidth")));
		}

		if (value.isArray("deflectors")) {
			// The seats take one entry each, in list order: an entry whose shape or response word
			// the engine does not know seats nothing - dropped, not refused - and the fourth seat
			// is the last, a fifth entry is the studio door's own refusal before it ever reaches
			// this loop
			ParticleDeflector seated[4] = {};
			uint32_t count = 0;
			for (auto &it : value.getArray("deflectors")) {
				if (!it.isDictionary()) {
					continue;
				}
				const auto type = it.getString("type");
				const auto response = it.getString("response");
				uint32_t shape = 0;
				if (type == StringView("circle")) {
					shape = XL_PARTICLE_DEFLECTOR_CIRCLE;
				} else if (type == StringView("segment")) {
					shape = XL_PARTICLE_DEFLECTOR_SEGMENT;
				} else {
					continue;
				}
				uint32_t answer = 0;
				if (response == StringView("bounce")) {
					answer = XL_PARTICLE_DEFLECTOR_BOUNCE;
				} else if (response == StringView("kill")) {
					answer = XL_PARTICLE_DEFLECTOR_KILL;
				} else {
					continue;
				}
				if (count == 4) {
					break;
				}
				ParticleDeflector seat =
						ParticleDeflector{Vec2::ZERO, Vec2::ZERO, 0.0f, shape, answer};
				if (shape == XL_PARTICLE_DEFLECTOR_CIRCLE) {
					seat.a = ParticleSystem_readVec2(it.getValue("center"), Vec2::ZERO);
					seat.radius = float(it.getDouble("radius", 0.0));
				} else {
					seat.a = ParticleSystem_readVec2(it.getValue("from"), Vec2::ZERO);
					seat.b = ParticleSystem_readVec2(it.getValue("to"), Vec2::ZERO);
				}
				seated[count] = seat;
				++count;
			}
			setDeflectors(SpanView<ParticleDeflector>(seated, count));
		}

		if (value.isBasicType("inheritVelocity")) {
			setInheritVelocity(float(value.getDouble("inheritVelocity")));
		}

		if (value.isArray("uvScroll")) {
			setUvScroll(ParticleSystem_readVec2(value.getValue("uvScroll"), getUvScroll()));
		}

		if (value.isBasicType("uvFlip")) {
			setUvFlip(float(value.getDouble("uvFlip")));
		}

		if (value.isBasicType("start")) {
			setStart(float(value.getDouble("start")));
		}

		if (value.isBasicType("interval")) {
			setInterval(float(value.getDouble("interval")));
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

	// The birth shape, only when one stands - the fresh system's point birth writes no key, and
	// a shape's numbers ride inside its own word
	if (getEmissionType() != ParticleEmissionType::Points) {
		Value shape;
		switch (getEmissionType()) {
		case ParticleEmissionType::Disc:
			shape.setString("disc", "type");
			shape.setDouble(_data->emissionShape[0], "radius");
			break;
		case ParticleEmissionType::Ring:
			shape.setString("ring", "type");
			shape.setDouble(_data->emissionShape[0], "outer");
			shape.setDouble(_data->emissionShape[1], "inner");
			shape.setDouble(_data->emissionShape[2], "from");
			shape.setDouble(_data->emissionShape[3], "to");
			break;
		case ParticleEmissionType::Rect:
			shape.setString("rect", "type");
			shape.setValue(
					ParticleSystem_writeVec2(Vec2(_data->emissionShape[0], _data->emissionShape[1])),
					"half");
			break;
		case ParticleEmissionType::Segment:
			shape.setString("segment", "type");
			shape.setValue(
					ParticleSystem_writeVec2(Vec2(_data->emissionShape[0], _data->emissionShape[1])),
					"from");
			shape.setValue(
					ParticleSystem_writeVec2(Vec2(_data->emissionShape[2], _data->emissionShape[3])),
					"to");
			break;
		case ParticleEmissionType::Points: break;
		}
		ret.setValue(sp::move(shape), "emissionShape");
	}

	ret.setValue(ParticleSystem_writeRange(getNormalMin(), getNormalMax()), "normal");
	ret.setValue(ParticleSystem_writeRange(getLifetimeMin(), getLifetimeMax()), "lifetime");
	ret.setValue(ParticleSystem_writeRange(getScaleMin(), getScaleMax()), "scale");
	// The second axis's own range, only when its words stand apart from the first axis's - a
	// range equal to `scale` is the unseated state the birth keeps, and the key stays out so an
	// old file saves byte for byte
	if (_data->data.scaleY.init != _data->data.scale.init
			|| _data->data.scaleY.rnd != _data->data.scale.rnd) {
		ret.setValue(ParticleSystem_writeRange(getScaleYMin(), getScaleYMax()), "scaleY");
	}
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
	ret.setValue(ParticleSystem_writeRange(getDampingMin(), getDampingMax()), "damping");
	ret.setValue(ParticleSystem_writeRange(getTurbulenceInfluenceMin(),
						 getTurbulenceInfluenceMax()),
			"turbulenceInfluence");

	if (_data->colorCurve) {
		ret.setValue(ParticleSystem_writeCurve(_data->colorCurve), "colorCurve");
	}
	if (_data->animFrameCurve) {
		ret.setValue(ParticleSystem_writeCurve(_data->animFrameCurve), "animFrameCurve");
	}
	if (_data->scaleCurve) {
		ret.setValue(ParticleSystem_writeCurve(_data->scaleCurve), "scaleCurve");
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
	if (_data->warm != 0.0f) {
		ret.setDouble(_data->warm, "warm");
	}
	if (_data->data.stretch != 0.0f) {
		ret.setDouble(_data->data.stretch, "stretch");
	}
	if (_data->data.maxSpeed != 0.0f) {
		ret.setDouble(_data->data.maxSpeed, "maxSpeed");
	}
	if (_data->data.turbulenceStrength != 0.0f) {
		ret.setDouble(_data->data.turbulenceStrength, "turbulenceStrength");
	}
	if (_data->data.turbulenceFrequency != 0.0f) {
		ret.setDouble(_data->data.turbulenceFrequency, "turbulenceFrequency");
	}
	if (_data->data.turbulenceShiftSpeed != 0.0f) {
		ret.setDouble(_data->data.turbulenceShiftSpeed, "turbulenceShiftSpeed");
	}
	// The taken seats alone, each as the full triple the apply reads; no seat taken is the fresh
	// default, and the key stays out
	Value attractors(Value::Type::ARRAY);
	for (const auto &seat : _data->data.attractors) {
		if (seat.strength == 0.0f) {
			continue;
		}
		Value entry(Value::Type::DICTIONARY);
		entry.setValue(ParticleSystem_writeVec2(seat.point), "point");
		entry.setDouble(seat.strength, "strength");
		entry.setDouble(seat.radius, "radius");
		attractors.addValue(sp::move(entry));
	}
	if (!attractors.empty()) {
		ret.setValue(sp::move(attractors), "attractors");
	}
	if (_data->data.trailWidth != 0.0f) {
		ret.setDouble(_data->data.trailWidth, "trailWidth");
	}
	// The taken seats alone, each as the full entry the apply reads; no seat taken is the fresh
	// default, and the key stays out
	Value deflectors(Value::Type::ARRAY);
	for (const auto &seat : _data->data.deflectors) {
		if (seat.shape == 0u) {
			continue;
		}
		Value entry(Value::Type::DICTIONARY);
		if (seat.shape == XL_PARTICLE_DEFLECTOR_CIRCLE) {
			entry.setString("circle", "type");
			entry.setValue(ParticleSystem_writeVec2(seat.a), "center");
			entry.setDouble(seat.radius, "radius");
		} else {
			entry.setString("segment", "type");
			entry.setValue(ParticleSystem_writeVec2(seat.a), "from");
			entry.setValue(ParticleSystem_writeVec2(seat.b), "to");
		}
		entry.setString(seat.response == XL_PARTICLE_DEFLECTOR_BOUNCE ? "bounce" : "kill",
				"response");
		deflectors.addValue(sp::move(entry));
	}
	if (!deflectors.empty()) {
		ret.setValue(sp::move(deflectors), "deflectors");
	}
	if (_data->data.inheritVelocity != 0.0f) {
		ret.setDouble(_data->data.inheritVelocity, "inheritVelocity");
	}
	if (_data->data.uvScroll.x != 0.0f || _data->data.uvScroll.y != 0.0f) {
		ret.setValue(ParticleSystem_writeVec2(_data->data.uvScroll), "uvScroll");
	}
	if (_data->data.uvFlip != 0.0f) {
		ret.setDouble(_data->data.uvFlip, "uvFlip");
	}
	if (_data->data.start != 0.0f) {
		ret.setDouble(_data->data.start, "start");
	}
	if (_data->data.interval != 0.0f) {
		ret.setDouble(_data->data.interval, "interval");
	}
	return ret;
}

void ParticleSystem::setParticleSize(Size2 size) {
	auto &d = mutate();
	d.particleSize = size;
	ParticleSystemData::writeParticleSize(d.data, size);
}

Size2 ParticleSystem::getParticleSize() const { return _data->particleSize; }

void ParticleSystem::setStretch(float stretch) {
	// Inside ParticleEmitterData, so the emitter re-uploads and the drawn quads follow
	mutate().data.stretch = stretch;
}

float ParticleSystem::getStretch() const { return _data->data.stretch; }

void ParticleSystem::setMaxSpeed(float maxSpeed) {
	// Inside ParticleEmitterData, so the emitter re-uploads and the step follows; a negative
	// ceiling is the no-ceiling default
	mutate().data.maxSpeed = sprt::max(maxSpeed, 0.0f);
}

float ParticleSystem::getMaxSpeed() const { return _data->data.maxSpeed; }

void ParticleSystem::setTurbulenceStrength(float strength) {
	// Inside ParticleEmitterData, so the emitter re-uploads and the step follows; a negative
	// strength is the no-turbulence default
	mutate().data.turbulenceStrength = sprt::max(strength, 0.0f);
}

float ParticleSystem::getTurbulenceStrength() const {
	return _data->data.turbulenceStrength;
}

void ParticleSystem::setTurbulenceFrequency(float frequency) {
	// A negative frequency collapses the noise's axes the way a zero one does; clamped to that 0
	mutate().data.turbulenceFrequency = sprt::max(frequency, 0.0f);
}

float ParticleSystem::getTurbulenceFrequency() const {
	return _data->data.turbulenceFrequency;
}

void ParticleSystem::setTurbulenceShiftSpeed(float shiftSpeed) {
	// Inside ParticleEmitterData, so the emitter re-uploads and the step follows; a negative
	// shift speed would drag the noise domain below the lattice's non-negative words, so it
	// clamps to the static-pattern default
	mutate().data.turbulenceShiftSpeed = sprt::max(shiftSpeed, 0.0f);
}

float ParticleSystem::getTurbulenceShiftSpeed() const {
	return _data->data.turbulenceShiftSpeed;
}

void ParticleSystem::setAttractors(SpanView<ParticleAttractor> attractors) {
	// Inside ParticleEmitterData, so the emitter re-uploads and the step follows. The seats past
	// the given list are cleared, so a shorter list unseats what a longer one had
	auto &data = mutate().data;
	size_t i = 0;
	for (; i < attractors.size() && i < 4; ++i) {
		data.attractors[i] = attractors[i];
	}
	for (; i < 4; ++i) {
		data.attractors[i] = ParticleAttractor{Vec2::ZERO, 0.0f, 0.0f};
	}
}

SpanView<const ParticleAttractor> ParticleSystem::getAttractors() const {
	return SpanView<const ParticleAttractor>(_data->data.attractors, 4);
}

void ParticleSystem::setTrailWidth(float width) {
	// Inside ParticleEmitterData, so the emitter re-uploads and the step follows; a negative
	// width is the no-trail default
	mutate().data.trailWidth = sprt::max(width, 0.0f);
}

float ParticleSystem::getTrailWidth() const { return _data->data.trailWidth; }

void ParticleSystem::setDeflectors(SpanView<ParticleDeflector> deflectors) {
	// Inside ParticleEmitterData, so the emitter re-uploads and the step follows. The seats past
	// the given list are cleared, so a shorter list unseats what a longer one had
	auto &data = mutate().data;
	size_t i = 0;
	for (; i < deflectors.size() && i < 4; ++i) {
		data.deflectors[i] = deflectors[i];
	}
	for (; i < 4; ++i) {
		data.deflectors[i] = ParticleDeflector{Vec2::ZERO, Vec2::ZERO, 0.0f, 0u, 0u};
	}
}

SpanView<const ParticleDeflector> ParticleSystem::getDeflectors() const {
	return SpanView<const ParticleDeflector>(_data->data.deflectors, 4);
}

void ParticleSystem::setInheritVelocity(float ratio) {
	// Inside ParticleEmitterData, so the emitter re-uploads and the birth follows; a negative
	// ratio is the no-inheritance default
	mutate().data.inheritVelocity = sprt::max(ratio, 0.0f);
}

float ParticleSystem::getInheritVelocity() const { return _data->data.inheritVelocity; }

void ParticleSystem::setUvScroll(Vec2 scroll) {
	// Inside ParticleEmitterData, so the emitter re-uploads and the pass follows; the speed is
	// signed per axis - the slide's direction is the sign's own - and no component is clamped
	mutate().data.uvScroll = scroll;
}

Vec2 ParticleSystem::getUvScroll() const { return _data->data.uvScroll; }

void ParticleSystem::setUvFlip(float flip) {
	// Inside ParticleEmitterData, so the emitter re-uploads and the birth follows; a negative
	// probability is the never-flip default
	mutate().data.uvFlip = sprt::max(flip, 0.0f);
}

float ParticleSystem::getUvFlip() const { return _data->data.uvFlip; }

void ParticleSystem::setStart(float seconds) {
	// Inside ParticleEmitterData, so the emitter re-uploads and the birth schedule follows; a
	// negative delay is the no-delay default
	mutate().data.start = sprt::max(seconds, 0.0f);
}

float ParticleSystem::getStart() const { return _data->data.start; }

void ParticleSystem::setInterval(float seconds) {
	// Inside ParticleEmitterData, so the emitter re-uploads and the birth schedule follows; a
	// negative interval is the phase-schedule default
	mutate().data.interval = sprt::max(seconds, 0.0f);
}

float ParticleSystem::getInterval() const { return _data->data.interval; }

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

void ParticleSystem::setEmissionShape(ParticleEmissionType type, SpanView<float> params) {
	auto &d = mutate();
	d.data.emissionType = toInt(type);
	for (size_t i = 0; i < 4; ++i) {
		d.emissionShape[i] = (i < params.size()) ? params[i] : 0.0f;
	}
}

ParticleEmissionType ParticleSystem::getEmissionType() const {
	return ParticleEmissionType(_data->data.emissionType);
}

const float *ParticleSystem::getEmissionShape() const { return _data->emissionShape; }

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

// The first axis's range carries the pair's tracking: an unseated second axis (its words equal
// to the first's) follows every scale edit, so a document that never named `scaleY` keeps its
// two axes one, while a seated one stands where its own key put it
void ParticleSystem::setScale(float value, float rnd) {
	auto &d = mutate();
	const bool tracking = d.data.scaleY.init == d.data.scale.init
			&& d.data.scaleY.rnd == d.data.scale.rnd;
	d.data.scale.init = value;
	d.data.scale.rnd = rnd;
	if (tracking) {
		d.data.scaleY = d.data.scale;
	}
}

float ParticleSystem::getScaleMin() const { return _data->data.scale.init; }

float ParticleSystem::getScaleMax() const {
	return _data->data.scale.init + _data->data.scale.rnd;
}

XL_PARTICLE_FLOAT_PARAM(Normal, normal)
XL_PARTICLE_FLOAT_PARAM(Lifetime, lifetime)
XL_PARTICLE_FLOAT_PARAM(ScaleY, scaleY)
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
XL_PARTICLE_FLOAT_PARAM(Damping, damping)
XL_PARTICLE_FLOAT_PARAM(TurbulenceInfluence, turbulenceInfluence)

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

bool ParticleSystem::setScaleCurve(CurveBuffer *curve) {
	// The Float curve is both axes with the one component; the Vec2 one is Effekseer's scale by
	// axis, each axis its own component - anything wider has no axis to ride
	if (curve && curve->getType() != CurveBufferType::Float
			&& curve->getType() != CurveBufferType::Vec2) {
		log::source().error("ParticleSystem", "Scale curve must be of Float or Vec2 type");
		return false;
	}

	mutate().scaleCurve = curve;
	return true;
}

CurveBuffer *ParticleSystem::getScaleCurve() const { return _data->scaleCurve; }

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

void ParticleSystem::setWarm(float seconds) {
	// No generation bump: warm lives outside ParticleEmitterData, so the renderer neither
	// restarts nor re-uploads for it - only the CPU-side sim reads the field
	copy();
	_data->warm = sprt::max(seconds, 0.0f);
}

float ParticleSystem::getWarm() const { return _data->warm; }

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
	data->scaleCurve = _data->scaleCurve;
	data->emissionPoints = _data->emissionPoints;
	::__sprt_memcpy(data->emissionShape, _data->emissionShape, sizeof(data->emissionShape));
	data->systemId = _data->systemId;
	data->paramsGeneration = _data->paramsGeneration;
	data->restartGeneration = _data->restartGeneration;
	data->seed = _data->seed;
	data->hasSeed = _data->hasSeed;
	data->warm = _data->warm;
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
