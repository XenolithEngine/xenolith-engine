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

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class AppThread;

}

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

enum class ParticleEmissionType : uint32_t {
	Points,
};

enum class ParticleSystemFlags : uint32_t {
	None = 0,
	LocalCoords = 1 << 0,
	AlignWithVelocity = 1 << 1,
	OrderByLifetime = 1 << 2,
	UseLifetimeMax = 1 << 3,
};

SP_DEFINE_ENUM_AS_MASK(ParticleSystemFlags)

// Immutable once handed to the renderer: ParticleSystem copies it before the next change.
struct SP_PUBLIC ParticleSystemData : public Ref {
	ParticleEmitterData data;
	Rc<CurveBuffer> colorCurve;
	Rc<CurveBuffer> animFrameCurve;
	Vector<Vec2> emissionPoints;

	uint64_t systemId = 0;

	// Any parameter change bumps paramsGeneration; restart() and a seed change bump
	// restartGeneration. The renderer compares them to pick the cheapest update.
	uint64_t paramsGeneration = 0;
	uint64_t restartGeneration = 0;

	uint32_t seed = 0;
	bool hasSeed = false;

	// As set by the user; data.sizeValue and data.sizeNormal are derived from it
	Size2 particleSize;

	ParticleSystemData() { ::__sprt_memset(&data, 0, sizeof(ParticleEmitterData)); }

	// The extra data buffer at ParticleEmitterData::emissionData:
	// [ParticleEmissionPoints][vec2 x count][ParticleCurveHeader][float x count * components]...,
	// every block aligned to 8 bytes.
	size_t getExtraDataSize() const;

	// Writes the buffer and sets the curve offsets (0 - no curve) in `target`, a copy of `data`
	void writeExtraData(uint8_t *, size_t, ParticleEmitterData &target) const;

	static void writeParticleSize(ParticleEmitterData &, Size2);
};

// The emitter's simulation after one rendered frame
struct SP_PUBLIC ParticleFeedback {
	uint64_t sequence = 0; // order of the frame in the renderer, never goes back; 0 - no report yet
	uint64_t restartGeneration = 0;
	uint32_t cycle = 0; // emission cycle the next frame starts in
	uint32_t cycleFrame = 0; // step of that cycle
	uint32_t framesInGen = 0; // steps in one cycle
	uint32_t nframes = 0; // steps this frame simulated

	// Filled only by the feedback pipeline (XL_PARTICLE_FEEDBACK=1), summed over the particles
	bool counters = false;
	uint32_t births = 0;
	uint32_t steps = 0; // steps living particles aged by
	uint32_t alive = 0; // particles drawn after the frame
};

// The first particles of the emitter's buffer after one rendered frame
struct SP_PUBLIC ParticleSnapshot {
	bool success = false; // false - the emitter left the scene before a frame took the snapshot
	ParticleFeedback feedback; // of the frame the snapshot is taken after
	Vector<ParticleData> particles;
};

/** Carries what the renderer learns about an emitter back to the application thread.

The renderer calls deliver* from its own threads; results are handed to the application thread,
where the rest of the object lives. After detach() nothing more is accepted. */
class SP_PUBLIC ParticleFeedbackReceiver : public Ref {
public:
	using SnapshotCallback = Function<void(ParticleSnapshot &&)>;

	struct SnapshotRequest {
		uint32_t id = 0;
		uint32_t count = 0;
		SnapshotCallback callback;
	};

	virtual ~ParticleFeedbackReceiver() = default;

	bool init(AppThread *);

	// Any thread
	void deliverFeedback(const ParticleFeedback &);
	void deliverSnapshot(uint32_t id, ParticleSnapshot &&);

	// Application thread
	void attach();

	// Pending snapshots complete with success = false
	void detach();

	uint32_t requestSnapshot(uint32_t count, SnapshotCallback &&);
	const SnapshotRequest *getPendingSnapshot() const;

	const ParticleFeedback &getFeedback() const { return _feedback; }
	uint64_t getTotalBirths() const { return _totalBirths; }
	uint64_t getTotalSteps() const { return _totalSteps; }

protected:
	AppThread *_application = nullptr;
	bool _attached = false;
	ParticleFeedback _feedback;
	uint64_t _totalBirths = 0;
	uint64_t _totalSteps = 0;
	uint32_t _nextSnapshotId = 1;
	Vector<SnapshotRequest> _snapshots;
};

struct ParticleSystemRenderInfo {
	Rc<ParticleSystemData> system;
	core::MaterialId material = 0;
	uint32_t maxFramesPerCall = 0;
	uint32_t transform = 0;
	uint32_t index = 0;

	// Quad size used when the system sets none: the emitter's texture region, in pixels
	Size2 defaultSize;

	// Node to scene content transform for newborn particles; identity with LocalCoords
	Mat4 nodeToScene;

	Rect textureRect = Rect(0.0f, 0.0f, 1.0f, 1.0f);
	UVec2 frameGrid = UVec2(1, 1);
	Color4F color = Color4F::WHITE; // the node's displayed color

	// Null - the emitter asks for nothing back
	Rc<ParticleFeedbackReceiver> feedback;
	uint32_t snapshotId = 0; // 0 - no snapshot this frame
	uint32_t snapshotCount = 0;
};

class SP_PUBLIC ParticleSystem : public Ref {
public:
	virtual ~ParticleSystem() = default;

	// Godot defaults: 8 particles, 1 s lifetime, 60 steps per second, white, direction 0 with a
	// 45 degree spread
	bool init();
	bool init(uint32_t count, uint32_t frameInterval, float lifetime);
	bool init(const Value &);

	uint64_t getId() const { return _id; }

	// Sets only the keys present in the value, see encode() for the format
	void apply(const Value &);
	Value encode() const;

	// Zero size means the size of the emitter's texture region
	void setParticleSize(Size2);
	Size2 getParticleSize() const;

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
	SpanView<Vec2> getEmissionPoints() const;

	void setOrigin(Vec2);
	Vec2 getOrigin() const;

	void setColor(const Color4F &);
	Color4F getColor() const;

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

	// Hue shift in turns, chosen at birth
	void setHue(float hue, float rnd = 0.0f);
	float getHueMin() const;
	float getHueMax() const;

	// Vec4 curve over the particle's lifetime, multiplied by the color; null removes it
	bool setColorCurve(CurveBuffer *);
	CurveBuffer *getColorCurve() const;

	// Float curve from the lifetime fraction to the animation frame fraction; null removes it
	bool setAnimFrameCurve(CurveBuffer *);
	CurveBuffer *getAnimFrameCurve() const;

	void addFlags(ParticleSystemFlags);
	void clearFlags(ParticleSystemFlags);
	void setFlags(ParticleSystemFlags);
	ParticleSystemFlags getFlags() const;

	// Reinitializes every particle of every emitter that uses this system
	void restart();

	// Fixed seed for the particle random generators; takes effect as a restart
	void setSeed(uint32_t);
	void clearSeed();
	uint32_t getSeed() const;
	bool hasSeed() const;

	uint64_t getParamsGeneration() const;
	uint64_t getRestartGeneration() const;

	Rc<ParticleSystemData> pop();
	Rc<ParticleSystemData> dup();

	bool isDirty() const;

protected:
	// Copy-on-write, then marks the parameters as changed
	ParticleSystemData &mutate();
	void copy();

	bool _copyOnWrite = false;
	uint64_t _id = 0;
	Rc<ParticleSystemData> _data;
};

} // namespace stappler::xenolith::basic2d

#endif /* XENOLITH_RENDERER_BASIC2D_XL2DPARTICLESYSTEM_H_ */
