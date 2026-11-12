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
	Disc,
	Ring,
	Rect,
	Segment,
};

enum class ParticleSystemFlags : uint32_t {
	None = 0,
	LocalCoords = 1 << 0,
	AlignWithVelocity = 1 << 1,
	OrderByLifetime = 1 << 2,
	UseLifetimeMax = 1 << 3,
	OneShot = 1 << 4,
	// Effekseer's parent binding, the continuous child of Particle-39: suppresses the emitter's
	// own birth schedule, the host seating its births from the living parent particles. Deliber-
	// ately absent from the flag word list below - no document carries it; the host sets the bit
	// on its own copy of the words
	ParentBound = 1 << 5,
};

SP_DEFINE_ENUM_AS_MASK(ParticleSystemFlags)

// Effekseer's ring renderer, the stage's own draw: the outer and inner edges in dp, the arc's
// first and last edge in degrees - the full circle 0 to 360 the arc words' default, saved
// without them - and the three colors of the bands across the width, white the default each.
// An outer of zero is the absence this model keeps: the pass draws the quad it always drew
struct ParticleRing {
	float outer = 0.0f;
	float inner = 0.0f;
	float arcFrom = 0.0f;
	float arcTo = 360.0f;
	Color4F colorOuter = Color4F::WHITE;
	Color4F colorMiddle = Color4F::WHITE;
	Color4F colorInner = Color4F::WHITE;
};

// Immutable once handed to the renderer: ParticleSystem copies it before the next change.
struct SP_PUBLIC ParticleSystemData : public Ref {
	ParticleEmitterData data;
	Rc<CurveBuffer> colorCurve;
	Rc<CurveBuffer> animFrameCurve;
	Rc<CurveBuffer> scaleCurve;
	Rc<CurveBuffer> positionCurve;
	Vector<Vec2> emissionPoints;

	// The numbers of the birth shape data.emissionType names, in the shader's read order:
	// disc [radius]; ring [outer, inner, arc from, arc to]; rect [half width, half height];
	// segment [two points]
	float emissionShape[4] = {0.0f, 0.0f, 0.0f, 0.0f};

	uint64_t systemId = 0;

	// Any parameter change bumps paramsGeneration; restart() and a seed change bump
	// restartGeneration. The renderer compares them to pick the cheapest update.
	uint64_t paramsGeneration = 0;
	uint64_t restartGeneration = 0;

	uint32_t seed = 0;
	bool hasSeed = false;

	// Seconds the system starts as if it had already run, Godot's preprocess; the renderer's own
	// frames start at zero, the CPU-side sim fast-forwards this much before its first hashed frame
	float warm = 0.0f;

	// As set by the user; data.sizeValue and data.sizeNormal are derived from it
	Size2 particleSize;

	ParticleSystemData() { ::__sprt_memset(&data, 0, sizeof(ParticleEmitterData)); }

	// The extra data buffer at ParticleEmitterData::emissionData:
	// [ParticleEmissionPoints][vec2 x count][shape floats][ParticleCurveHeader][float x count *
	// components]..., every block aligned to 8 bytes. The shape floats sit after the emission
	// points block only when emissionType is a shape, in the order listed above.
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

	// The quad mask the draw multiplies its alpha by: the shape is a ParticleQuadMask value,
	// the two numbers the ring's thickness and edge softness. Shape zero keeps the texture
	// path - the emitter's material stays the common sampling one
	uint32_t maskShape = 0;
	float maskThickness = 0.0f;
	float maskSoftness = 0.0f;

	// The node's velocity over the last frame - its origin displacement divided by the system's
	// own dt, never a wall clock read - a newborn inherits its inheritVelocity share of; zero
	// in every frame the node never moved
	Vec2 nodeVelocity = Vec2::ZERO;
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

	// Growth of the velocity-aligned quad's half-length per unit of speed; 0 - no growth, the
	// key encode() writes only when the value is not that default
	void setStretch(float);
	float getStretch() const;

	// Upper limit of the particle speed, 0 - no limit; a negative value clamps to that default,
	// and the key encode() writes only when the value is not it
	void setMaxSpeed(float);
	float getMaxSpeed() const;

	// Turbulence: the strength scales a one-octave 2D noise force, the frequency the noise's
	// variation across its two axes (the fresh system carries 1), the shift speed the pattern's
	// slide per second of simulation time; a negative value clamps to 0, and each key encode()
	// writes only when the value is not that 0
	void setTurbulenceStrength(float);
	float getTurbulenceStrength() const;
	void setTurbulenceFrequency(float);
	float getTurbulenceFrequency() const;
	void setTurbulenceShiftSpeed(float);
	float getTurbulenceShiftSpeed() const;

	// The attractor seats, up to four: each pulls (a positive strength) toward its point or
	// pushes (a negative one) away, the force fallen off to zero at the seat's radius. A
	// strength of zero seats nothing - the entry is dropped from the list, not refused - and the
	// key encode() writes only when a seat is taken
	void setAttractors(SpanView<ParticleAttractor>);
	SpanView<const ParticleAttractor> getAttractors() const;

	// Width of the trail strip the particle's seat history draws, 0 - no trail; a negative value
	// clamps to that default, and the key encode() writes only when the value is not it
	void setTrailWidth(float);
	float getTrailWidth() const;

	// The deflector seats, up to four: each seats a figure - a circle of its center and radius,
	// a segment of its two ends - and the answer a particle that meets the figure earns, a
	// bounce off the figure or the death the onDeath event sees. An entry whose shape or
	// response word the engine does not know seats nothing - dropped from the list, not
	// refused - and the key encode() writes only when a seat is taken
	void setDeflectors(SpanView<ParticleDeflector>);
	SpanView<const ParticleDeflector> getDeflectors() const;

	// Ratio of the node's velocity a newborn particle inherits, 0 - no inheritance; a negative
	// value clamps to that default, and the key encode() writes only when the value is not it
	void setInheritVelocity(float);
	float getInheritVelocity() const;

	// Slide of the sampled UV window over the texture per second of simulation time, in UV
	// units, signed per axis; the pair's zero is the no-slide default, and the key encode()
	// writes only when the value is not it
	void setUvScroll(Vec2);
	Vec2 getUvScroll() const;

	// Probability of the U mirror a newborn particle draws from its own generator, 0 - never;
	// a negative value clamps to that default, and the key encode() writes only when the value
	// is not it
	void setUvFlip(float);
	float getUvFlip() const;

	// Seconds the birth schedule waits before the emitter's own timeline begins, 0 - no wait;
	// a negative value clamps to that default, and the key encode() writes only when the value
	// is not it
	void setStart(float);
	float getStart() const;

	// Seconds between the births of adjacent slots, Effekseer's GenerationTime, 0 - the phase
	// schedule of count over the cycle; a negative value clamps to that default, and the key
	// encode() writes only when the value is not it. A seated interval seats the birth of slot
	// i at the step i * interval / dt of every cycle, and randomness and explosiveness stand
	// unread
	void setInterval(float);
	float getInterval() const;

	// The ring the pass draws in place of the quad, Effekseer's ring renderer in the screen
	// plane: both radii and the arc name themselves into the frame hash the drawing words'
	// own way, and the scale curve multiplies both radii. An outer of zero - or a negative
	// one, clamped to it - seats nothing, and the key encode() writes only when a ring stands.
	// The corner count is the draw's own constant, never a field
	void setRing(const ParticleRing &);
	void clearRing();
	ParticleRing getRing() const;

	// Width of the strip the pass draws through the emitter's particles in their birth order,
	// Effekseer's ribbon taken in the screen plane, 0 - no strip, the quad the pass always
	// drew; a negative value clamps to that default, and the key encode() writes only when
	// the value is not it. The width multiplies the scale the curve left standing at each end
	void setStripWidth(float);
	float getStripWidth() const;

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

	// The shape newborn particles are sampled on, uniform over its area, by the particle's own
	// generator; the type names how many of the four numbers the shape reads. Points leaves the
	// birth on the point list, and setEmissionPoints returns to it
	void setEmissionShape(ParticleEmissionType, SpanView<float>);
	ParticleEmissionType getEmissionType() const;
	const float *getEmissionShape() const;

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

	// The second axis's own birth range, Effekseer's scale by axis. The range seats itself by
	// standing apart from the first axis's words: an unseated second axis (the words equal to
	// `scale`'s) follows every scale edit, while a seated one stands where its own key put it.
	// A seated range equal to the first axis's is the absence again - the birth draws no second
	// number and encode() writes no key
	void setScaleY(float scale, float rnd = 0.0f);
	float getScaleYMin() const;
	float getScaleYMax() const;

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

	// The rate the speed loses per second, chosen at birth; 0 - no loss
	void setDamping(float damping, float rnd = 0.0f);
	float getDampingMin() const;
	float getDampingMax() const;

	// The min and max of the turbulence influence ramp over the particle's lifetime; the pair
	// key encode() always writes
	void setTurbulenceInfluence(float influence, float rnd = 0.0f);
	float getTurbulenceInfluenceMin() const;
	float getTurbulenceInfluenceMax() const;

	// Vec4 curve over the particle's lifetime, multiplied by the color; null removes it
	bool setColorCurve(CurveBuffer *);
	CurveBuffer *getColorCurve() const;

	// Float curve from the lifetime fraction to the animation frame fraction; null removes it
	bool setAnimFrameCurve(CurveBuffer *);
	CurveBuffer *getAnimFrameCurve() const;

	// Curve over the particle's lifetime, multiplies the scale chosen at birth; null removes
	// it. A Float curve multiplies both axes with the one component, a Vec2 one multiplies each
	// axis with its own. Two stops carry what Particle Designer calls start and end size
	bool setScaleCurve(CurveBuffer *);
	CurveBuffer *getScaleCurve() const;

	// Vec2 curve over the particle's lifetime whose walk the step adds to the birth point every
	// step past the step's own moves, Effekseer's position of the start and the end point taken
	// as a displacement - the walk measured from the curve's first stop, so the velocity's path
	// and the curve's one sum. Null removes it; a curve of another type, or one whose every word
	// is zero - a curve that cannot move a particle - seats nothing, the width word's own zero
	// fate
	bool setPositionCurve(CurveBuffer *);
	CurveBuffer *getPositionCurve() const;

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

	// Seconds of preprocess before the first hashed frame; a negative value clamps to the 0 the
	// fresh system carries, and encode() writes the field only when it is not that default
	void setWarm(float);
	float getWarm() const;

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
