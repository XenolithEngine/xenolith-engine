/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons whom the Software is
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

// The CPU reference of the particle emission cycle: the same XL2dGlslParticleSim.h the particle
// update shader compiles, driven step by step and checked against what the cycle must produce.

#define STAPPLER_VERSIONIZED

#include <string.h>

#include <sprt/runtime/platform.h>
#include <sprt/runtime/stream.h>
#include <sprt/cxx/cmath>

#include "XL2dGlslParticleSim.h"

namespace glsl = stappler::glsl;

static constexpr uint32_t MaxParticles = 64;
static constexpr uint32_t MaxPoints = 8;

static glsl::vec2 s_points[MaxPoints];
static uint32_t s_pointCount = 0;

// The extra data buffer, word by word, as ParticleSystemData::writeExtraData lays it out
static constexpr uint32_t MaxExtraWords = 256;
static uint32_t s_extra[MaxExtraWords];

namespace STAPPLER_VERSIONIZED stappler::glsl {

uint particleEmissionPointCount() { return s_pointCount; }

vec2 particleEmissionPoint(uint index) { return s_points[index]; }

uint particleExtraUint(uint word) { return s_extra[word]; }

float particleExtraFloat(uint word) {
	float ret;
	::memcpy(&ret, &s_extra[word], sizeof(float));
	return ret;
}

} // namespace stappler::glsl

static int s_checks = 0;
static int s_failures = 0;

static void check(bool cond, const char *name) {
	++s_checks;
	sprt::cout << (cond ? "[ OK ] " : "[FAIL] ") << name << "\n";
	if (!cond) {
		++s_failures;
	}
}

static void checkEq(int64_t got, int64_t expected, const char *name) {
	++s_checks;
	bool ok = got == expected;
	sprt::cout << (ok ? "[ OK ] " : "[FAIL] ") << name;
	if (!ok) {
		sprt::cout << "  (got " << got << ", expected " << expected << ")";
	}
	sprt::cout << "\n";
	if (!ok) {
		++s_failures;
	}
}

// One emitter as the renderer keeps it: the particle buffer, the clock and the cycle position
struct Emitter {
	glsl::ParticleEmitterData data;
	glsl::ParticleData particles[MaxParticles];

	uint64_t clock = 0;
	uint32_t frame = 0;
	uint32_t cycle = 0;
	uint32_t seed = 0;
	uint32_t maxSteps = 1;

	// Node to scene transform of the frame, identity by default
	glsl::vec4 transformX = glsl::vec4(1.0f, 0.0f, 0.0f, 0.0f);
	glsl::vec4 transformY = glsl::vec4(0.0f, 1.0f, 0.0f, 0.0f);
	float transformRotation = 0.0f;
	float transformScale = 1.0f;

	// Per particle, filled by the last tick: born during it, and whether it would have outlived it
	bool born[MaxParticles];
	bool wasAlive[MaxParticles];

	// Summed ParticleUpdateCounters of the last tick, and the lifetime steps it took per particle
	glsl::ParticleUpdateCounters counters;
	uint32_t aged[MaxParticles];

	Emitter(uint32_t count, float lifetime, uint32_t seed_) : seed(seed_) {
		::memset(&data, 0, sizeof(data));
		data.count = count;
		data.frameInterval = 16'667;
		data.dt = data.frameInterval / 1'000'000.0f;
		data.lifetime.init = lifetime;
		data.scale.init = 1.0f;
		// The zeroed words would seat a phantom second axis of 0..0; the unseated state is the
		// second axis's words equal to the first's, as ParticleSystem::init leaves them
		data.scaleY = data.scale;
		::memset(particles, 0, sizeof(particles));
		reset(0, count);
	}

	void reset(uint32_t first, uint32_t count) {
		for (uint32_t i = first; i < count; ++i) {
			::memset(&particles[i], 0, sizeof(glsl::ParticleData));
			glsl::particleSeedRng(particles[i].rng, seed, i);
		}
	}

	uint32_t cycleFrames() const { return glsl::particleCycleFrames(data); }

	// The renderer's frame: advance the clock to `now`, simulate, move the cycle position
	uint32_t tickTo(uint64_t now) {
		auto framesInGen = glsl::particleCycleFrames(data);
		glsl::particleAdvanceFrame(frame, cycle, framesInGen, 0);

		auto nframes = glsl::particleAdvanceClock(clock, now, data.frameInterval, maxSteps);

		glsl::ParticleFrameData f;
		::memset(&f, 0, sizeof(f));
		f.framesInGen = framesInGen;
		f.genframe = frame;
		f.nframes = nframes;
		f.cycle = cycle;
		f.seed = seed;
		f.dt = data.dt;
		f.transformX = transformX;
		f.transformY = transformY;
		f.transformRotation = transformRotation;
		f.transformScale = transformScale;

		counters = glsl::ParticleUpdateCounters{0, 0};
		for (uint32_t i = 0; i < data.count; ++i) {
			wasAlive[i] = particles[i].currentLifetime > nframes;
			auto before = particles[i].rng;
			auto lifetime = particles[i].currentLifetime;
			glsl::particleUpdate(particles[i], data, f, i, counters);
			// Only an emission draws random numbers
			born[i] = before.state != particles[i].rng.state;
			aged[i] = lifetime - particles[i].currentLifetime;
		}

		glsl::particleAdvanceFrame(frame, cycle, framesInGen, nframes);
		return nframes;
	}

	uint32_t tick(uint32_t steps = 1) {
		return tickTo(clock + uint64_t(steps) * data.frameInterval);
	}

	uint32_t alive() const {
		uint32_t ret = 0;
		for (uint32_t i = 0; i < data.count; ++i) {
			ret += (particles[i].currentLifetime > 0) ? 1 : 0;
		}
		return ret;
	}

	uint32_t births() const {
		uint32_t ret = 0;
		for (uint32_t i = 0; i < data.count; ++i) { ret += born[i] ? 1 : 0; }
		return ret;
	}
};

static void testExplosive() {
	Emitter e(16, 1.0f, 1);
	e.data.explosiveness = 1.0f;
	auto framesInGen = e.cycleFrames();

	e.tick();
	checkEq(e.births(), 16, "explosiveness 1: every particle is born on the first step");
	checkEq(e.alive(), 16, "explosiveness 1: all alive after the first step");

	bool quiet = true;
	bool allAlive = true;
	for (uint32_t k = 1; k < framesInGen; ++k) {
		e.tick();
		quiet = quiet && e.births() == 0;
		allAlive = allAlive && e.alive() == 16;
	}
	check(quiet, "explosiveness 1: no births for the rest of the cycle");
	check(allAlive, "explosiveness 1: nobody dies before the cycle ends");

	e.tick();
	checkEq(e.births(), 16, "explosiveness 1: the next cycle bursts again");
}

static void testUniform() {
	Emitter e(16, 1.0f, 2);
	auto framesInGen = e.cycleFrames();

	bool exact = true;
	uint32_t expectedAlive = 0;
	bool aliveMatches = true;
	for (uint32_t k = 0; k < framesInGen; ++k) {
		e.tick();
		uint32_t expected = 0;
		for (uint32_t i = 0; i < 16; ++i) {
			// i / count of the cycle, in integers
			if ((i * framesInGen) / 16 == k) {
				++expected;
			}
		}
		exact = exact && e.births() == expected;
		expectedAlive += expected;
		aliveMatches = aliveMatches && e.alive() == expectedAlive;
	}
	check(exact, "explosiveness 0, randomness 0: births follow i / count of the cycle");
	check(aliveMatches, "explosiveness 0, randomness 0: the alive count grows with the births");

	bool full = true;
	for (uint32_t k = 0; k < framesInGen * 2; ++k) {
		e.tick();
		full = full && e.alive() == 16;
	}
	check(full, "lifetime equal to the cycle: all alive after the first cycle");
}

static void testHalfExplosive() {
	Emitter e(16, 1.0f, 3);
	e.data.explosiveness = 0.5f;
	auto framesInGen = e.cycleFrames();

	uint32_t total = 0;
	bool early = true;
	for (uint32_t k = 0; k < framesInGen; ++k) {
		e.tick();
		total += e.births();
		if (k * 2 >= framesInGen) {
			early = early && e.births() == 0;
		}
	}
	checkEq(total, 16, "explosiveness 0.5: every particle is born once in a cycle");
	check(early, "explosiveness 0.5: all births are in the first half of the cycle");
}

// Birth step of every particle for `cycles` cycles
// The feedback pipeline sums the same counters over its per-particle records
static void testCounters() {
	Emitter e(32, 0.7f, 5);
	e.data.randomness = 1.0f;
	auto framesInGen = e.cycleFrames();

	bool births = true;
	bool steps = true;
	uint64_t total = 0;
	for (uint32_t k = 0; k < framesInGen * 3; ++k) {
		e.tick();
		births = births && e.counters.births == e.births();
		uint32_t aged = 0;
		for (uint32_t i = 0; i < e.data.count; ++i) { aged += e.born[i] ? 0 : e.aged[i]; }
		steps = steps && e.counters.steps == aged;
		total += e.counters.births;
	}
	check(births, "counters: births match the particles born in the step");
	check(steps, "counters: steps match the lifetime the living particles lost");
	checkEq(int64_t(total), int64_t(e.data.count) * 3, "counters: count births per cycle");

	Emitter burst(16, 1.0f, 2);
	burst.data.explosiveness = 1.0f;
	burst.maxSteps = 2;
	burst.tick(2);
	checkEq(burst.counters.births, 16, "counters: explosiveness 1 bursts in the first step");
	checkEq(burst.counters.steps, 16, "counters: two steps - a birth and one aging step each");
}

static void recordBirths(Emitter &e, uint32_t cycles, uint32_t *out) {
	auto framesInGen = e.cycleFrames();
	for (uint32_t c = 0; c < cycles; ++c) {
		for (uint32_t k = 0; k < framesInGen; ++k) {
			e.tick();
			for (uint32_t i = 0; i < e.data.count; ++i) {
				if (e.born[i]) {
					out[c * MaxParticles + i] = k;
				}
			}
		}
	}
}

static void testRandomness() {
	static constexpr uint32_t Cycles = 3;
	uint32_t a[Cycles * MaxParticles];
	uint32_t b[Cycles * MaxParticles];
	uint32_t c[Cycles * MaxParticles];

	Emitter e2(16, 1.0f, 42);
	e2.data.randomness = 1.0f;
	auto framesInGen = e2.cycleFrames();
	::memset(a, 0xff, sizeof(a));
	uint32_t counts[MaxParticles] = {0};
	for (uint32_t cyc = 0; cyc < Cycles; ++cyc) {
		for (uint32_t k = 0; k < framesInGen; ++k) {
			e2.tick();
			for (uint32_t i = 0; i < 16; ++i) {
				if (e2.born[i]) {
					a[cyc * MaxParticles + i] = k;
					++counts[i];
				}
			}
		}
	}

	bool exactlyOnce = true;
	for (uint32_t i = 0; i < 16; ++i) { exactlyOnce = exactlyOnce && counts[i] == Cycles; }
	check(exactlyOnce, "randomness 1: every particle is born exactly once per cycle");

	bool inSlot = true;
	for (uint32_t cyc = 0; cyc < Cycles; ++cyc) {
		for (uint32_t i = 0; i < 16; ++i) {
			auto k = a[cyc * MaxParticles + i];
			// the shift stays within one slot: [i / count, (i + 1) / count)
			inSlot = inSlot && k >= (i * framesInGen) / 16 && k <= ((i + 1) * framesInGen) / 16;
		}
	}
	check(inSlot, "randomness 1: a birth stays within its own slot of the cycle");

	bool changes = false;
	for (uint32_t i = 0; i < 16; ++i) {
		changes = changes || a[i] != a[MaxParticles + i]
				|| a[MaxParticles + i] != a[2 * MaxParticles + i];
	}
	check(changes, "randomness 1: phases change between cycles");

	Emitter e3(16, 1.0f, 42);
	e3.data.randomness = 1.0f;
	::memset(b, 0xff, sizeof(b));
	recordBirths(e3, Cycles, b);
	check(::memcmp(a, b, sizeof(a)) == 0, "randomness 1: the same seed repeats the sequence");

	Emitter e4(16, 1.0f, 43);
	e4.data.randomness = 1.0f;
	::memset(c, 0xff, sizeof(c));
	recordBirths(e4, Cycles, c);
	check(::memcmp(a, c, sizeof(a)) != 0, "randomness 1: another seed gives another sequence");
}

static void testLifetimeMax() {
	Emitter e(16, 0.5f, 5);
	e.data.lifetime.rnd = 0.5f;

	auto minFrames = e.cycleFrames();
	e.data.flags = XL_PARTICLE_FLAG_USE_LIFETIME_MAX;
	auto maxFrames = e.cycleFrames();

	checkEq(minFrames, uint32_t(sprt::floor(0.5f / e.data.dt)),
			"cycle is the lower lifetime bound without UseLifetimeMax");
	checkEq(maxFrames, uint32_t(sprt::floor(1.0f / e.data.dt)),
			"cycle is the upper lifetime bound with UseLifetimeMax");

	bool neverAlive = true;
	for (uint32_t k = 0; k < maxFrames * 4; ++k) {
		e.tick();
		for (uint32_t i = 0; i < 16; ++i) {
			neverAlive = neverAlive && !(e.born[i] && e.wasAlive[i]);
		}
	}
	check(neverAlive, "UseLifetimeMax: no particle is reborn while alive");

	Emitter f(16, 0.5f, 5);
	f.data.lifetime.rnd = 0.5f;
	bool rebornAlive = false;
	for (uint32_t k = 0; k < minFrames * 4; ++k) {
		f.tick();
		for (uint32_t i = 0; i < 16; ++i) {
			rebornAlive = rebornAlive || (f.born[i] && f.wasAlive[i]);
		}
	}
	check(rebornAlive, "without UseLifetimeMax a particle outliving the cycle is reborn alive");
}

static void testSteps() {
	Emitter single(16, 0.5f, 6);
	single.data.randomness = 0.5f;
	single.data.explosiveness = 0.25f;

	Emitter paired(16, 0.5f, 6);
	paired.data.randomness = 0.5f;
	paired.data.explosiveness = 0.25f;
	paired.maxSteps = 2;

	auto framesInGen = single.cycleFrames();
	bool same = true;
	for (uint32_t k = 0; k < framesInGen * 3; ++k) {
		single.tick();
		single.tick();
		checkEq(paired.tick(2), 2, "two steps in one frame");
		same = same
				&& ::memcmp(single.particles, paired.particles, 16 * sizeof(glsl::ParticleData))
						== 0;
		if (!same) {
			break;
		}
	}
	check(same, "two steps per frame across cycle boundaries equal two frames of one step");
	checkEq(single.cycle, paired.cycle, "the cycle counter agrees");
	checkEq(single.frame, paired.frame, "the cycle position agrees");
}

static void testClock() {
	uint64_t clock = 0;
	auto n = glsl::particleAdvanceClock(clock, 10 * 16'667 + 123, 16'667, 2);
	checkEq(n, 2, "a lag of 10 steps simulates only maxSteps");
	checkEq(int64_t(clock), 10 * 16'667, "the dropped lag leaves the clock on a step boundary");

	clock = 0;
	n = glsl::particleAdvanceClock(clock, 2 * 16'667 + 5, 16'667, 2);
	checkEq(n, 2, "a lag within maxSteps is simulated");
	checkEq(int64_t(clock), 2 * 16'667, "the clock advances by the simulated steps");

	clock = 100;
	n = glsl::particleAdvanceClock(clock, 50, 16'667, 2);
	checkEq(n, 0, "a frame older than the clock simulates nothing");
	checkEq(int64_t(clock), 100, "and does not move the clock");

	uint32_t frame = 70;
	uint32_t cycle = 3;
	glsl::particleAdvanceFrame(frame, cycle, 30, 0);
	checkEq(frame, 10, "a shortened cycle wraps the position");
	checkEq(cycle, 5, "and counts the cycles it passed");
}

static void testCountChange() {
	Emitter e(16, 1.0f, 7);
	auto framesInGen = e.cycleFrames();
	for (uint32_t k = 0; k < framesInGen + 5; ++k) { e.tick(); }
	checkEq(e.alive(), 16, "before resize: all alive");

	// Grow: the renderer copies the first 16 particles and initializes the tail
	e.data.count = 32;
	e.reset(16, 32);

	bool bounded = true;
	bool tailWaits = true;
	for (uint32_t k = 0; k < framesInGen * 2; ++k) {
		auto t = e.frame % framesInGen;
		e.tick();
		bounded = bounded && e.alive() <= 32;
		for (uint32_t i = 16; i < 32; ++i) {
			if (e.born[i]) {
				tailWaits = tailWaits && (i * framesInGen) / 32 == t;
			}
		}
	}
	check(bounded, "grown to 32: never more alive than count");
	check(tailWaits, "grown to 32: a new particle is born at its own phase of the new count");
	checkEq(e.alive(), 32, "grown to 32: all alive after a cycle");

	// Shrink: the renderer keeps the first 8
	e.data.count = 8;
	bool shrunk = true;
	for (uint32_t k = 0; k < framesInGen; ++k) {
		e.tick();
		shrunk = shrunk && e.alive() <= 8;
	}
	check(shrunk, "shrunk to 8: never more alive than count");
	checkEq(e.alive(), 8, "shrunk to 8: all alive");
}

static void testPoints() {
	Emitter e(16, 1.0f, 8);
	auto framesInGen = e.cycleFrames();

	s_points[0] = glsl::vec2(10.0f, 0.0f);
	s_points[1] = glsl::vec2(20.0f, 0.0f);
	s_points[2] = glsl::vec2(30.0f, 0.0f);
	s_pointCount = 3;

	bool onPoint = true;
	uint32_t used = 0;
	for (uint32_t k = 0; k < framesInGen; ++k) {
		e.tick();
		for (uint32_t i = 0; i < 16; ++i) {
			if (!e.born[i]) {
				continue;
			}
			bool found = false;
			for (uint32_t p = 0; p < s_pointCount; ++p) {
				if (e.particles[i].position.x == s_points[p].x
						&& e.particles[i].position.y == s_points[p].y) {
					found = true;
					used |= 1 << p;
				}
			}
			onPoint = onPoint && found;
		}
	}
	check(onPoint, "points: every particle is born on one of the points");
	checkEq(used, 7, "points: all three points are used");

	s_pointCount = 0;
	Emitter z(16, 1.0f, 8);
	z.data.explosiveness = 1.0f;
	z.tick();
	bool origin = true;
	for (uint32_t i = 0; i < 16; ++i) {
		origin = origin && z.particles[i].position.x == 0.0f && z.particles[i].position.y == 0.0f;
	}
	check(origin, "no points: every particle is born at (0, 0)");
}

static void testRandom() {
	glsl::pcg16_state_t rng;
	glsl::pcg16_srandom_r(rng, 12'345, 678);

	bool unit = true;
	bool bounded = true;
	uint32_t hits[3] = {0};
	float maxValue = 0.0f;
	for (uint32_t i = 0; i < 100'000; ++i) {
		auto f = glsl::pcg16_random_float_r(rng);
		unit = unit && f >= 0.0f && f < 1.0f;
		maxValue = sprt::max(maxValue, f);
		auto b = glsl::pcg16_boundedrand_r(rng, 3);
		bounded = bounded && b < 3;
		if (b < 3) {
			++hits[b];
		}
	}
	check(unit, "pcg16_random_float_r stays in [0, 1)");
	check(maxValue > 0.99f, "pcg16_random_float_r covers the whole interval");
	check(bounded, "pcg16_boundedrand_r stays below the bound");
	check(hits[0] > 30'000 && hits[1] > 30'000 && hits[2] > 30'000,
			"pcg16_boundedrand_r is roughly uniform");
}

static void testSeedRng() {
	glsl::pcg16_state_t a, b, c;
	glsl::particleSeedRng(a, 42, 5);
	glsl::particleSeedRng(b, 42, 5);
	glsl::particleSeedRng(c, 42, 6);
	check(a.state == b.state && a.inc == b.inc, "seeded generator depends on seed and index only");
	check(a.state != c.state || a.inc != c.inc, "neighbouring particles get different generators");
}

static bool near(float got, float expected) {
	return sprt::fabs(got - expected)
			<= 1e-4f * sprt::max(sprt::fabs(got), sprt::fabs(expected)) + 1e-3f;
}

static void checkNear(float got, float expected, const char *name) {
	++s_checks;
	bool ok = near(got, expected);
	sprt::cout << (ok ? "[ OK ] " : "[FAIL] ") << name;
	if (!ok) {
		sprt::cout << "  (got " << got << ", expected " << expected << ")";
	}
	sprt::cout << "\n";
	if (!ok) {
		++s_failures;
	}
}

// One long-lived particle in node space, born on the first tick at `start`
struct Single : Emitter {
	Single(glsl::vec2 start = glsl::vec2(0.0f, 0.0f)) : Emitter(1, 10.0f, 11) {
		data.explosiveness = 1.0f;
		data.flags = XL_PARTICLE_FLAG_LOCAL_COORDS;
		point = start;
	}

	glsl::ParticleData &p() { return particles[0]; }

	// Birth, then `steps` steps
	void run(uint32_t steps) {
		s_points[0] = point;
		s_pointCount = 1;
		tick();
		for (uint32_t i = 0; i < steps; ++i) { tick(); }
		s_pointCount = 0;
	}

	glsl::vec2 point;
};

static void testKinematics() {
	static constexpr uint32_t N = 60;
	const float dt = 16'667 / 1'000'000.0f;
	const float n = float(N);

	{
		Single e;
		e.data.normal.init = 0.5f;
		e.data.velocity.init = 100.0f;
		e.run(N);
		checkNear(e.p().position.x, sprt::cos(0.5f) * 100.0f * n * dt,
				"velocity: x = n·v·cos θ·dt");
		checkNear(e.p().position.y, sprt::sin(0.5f) * 100.0f * n * dt,
				"velocity: y = n·v·sin θ·dt");
	}

	{
		Single e;
		e.data.velocity.init = 100.0f;
		e.data.linearVelocity.init = glsl::vec2(0.0f, 50.0f);
		e.run(0);
		checkNear(e.p().velocity.x, 100.0f,
				"initial velocity = normal·velocity + linearVelocity (x)");
		checkNear(e.p().velocity.y, 50.0f,
				"initial velocity = normal·velocity + linearVelocity (y)");
	}

	{
		Single e;
		e.data.velocity.init = 100.0f;
		e.data.linearAcceleration.init = glsl::vec2(0.0f, -300.0f);
		e.run(N);
		checkNear(e.p().position.x, 100.0f * n * dt,
				"linearAcceleration: x keeps the initial velocity");
		checkNear(e.p().position.y, -300.0f * dt * dt * n * (n + 1.0f) / 2.0f,
				"linearAcceleration: y = g·dt²·n(n+1)/2");
	}

	{
		Single e;
		e.data.normal.init = float(M_PI_2);
		e.data.velocity.init = 100.0f;
		e.data.acceleration.init = 50.0f;
		e.run(N);
		checkNear(e.p().velocity.length(), 100.0f + 50.0f * n * dt,
				"acceleration: speed grows by a·dt per step");
		checkNear(e.p().velocity.x / e.p().velocity.length() + 1.0f, 1.0f,
				"acceleration: direction is kept");
	}

	{
		Single e(glsl::vec2(10.0f, 0.0f));
		e.data.radialAcceleration.init = 200.0f;
		e.run(N);
		checkNear(e.p().position.x, 10.0f + 200.0f * dt * dt * n * (n + 1.0f) / 2.0f,
				"radialAcceleration: x = x0 + a·dt²·n(n+1)/2");
		checkNear(e.p().position.y + 1.0f, 1.0f, "radialAcceleration: a straight line from origin");
	}

	{
		Single e(glsl::vec2(10.0f, 0.0f));
		e.data.tangentialAcceleration.init = 300.0f;
		e.run(1);
		checkNear(e.p().position.x, 10.0f,
				"tangentialAcceleration: the first step is across the radius");
		check(e.p().position.y > 0.0f, "tangentialAcceleration: counter-clockwise");
	}

	{
		Single e(glsl::vec2(10.0f, 0.0f));
		e.data.orbitalVelocity.init = 1.5f;
		e.run(N);
		checkNear(e.p().position.length(), 10.0f, "orbitalVelocity: stays on the circle");
		checkNear(sprt::atan2(e.p().position.y, e.p().position.x), 1.5f * n * dt,
				"orbitalVelocity: turns by ω·dt per step");
	}

	{
		Single e(glsl::vec2(10.0f, 0.0f));
		e.data.radialVelocity.init = 40.0f;
		e.run(N);
		checkNear(e.p().position.x, 10.0f + 40.0f * n * dt, "radialVelocity: x = x0 + n·v·dt");
		checkNear(e.p().position.y + 1.0f, 1.0f, "radialVelocity: a straight line from origin");
	}

	{
		Single e;
		e.data.angle.init = 0.3f;
		e.data.angularVelocity.init = 2.0f;
		e.run(N);
		checkNear(e.p().angle, 0.3f + 2.0f * n * dt, "angularVelocity: angle = a0 + n·w·dt");
	}

	{
		// A 20x10 quad, as ParticleSystemData::writeParticleSize stores it
		Single e;
		auto size = glsl::vec2(20.0f, 10.0f);
		e.data.sizeValue = (size / 2.0f).length();
		e.data.sizeNormal = size.getNormalized();
		e.data.scale.init = 2.0f;
		e.data.scaleY = e.data.scale; // the pair moves together, no axis of its own
		e.run(0);

		glsl::vec2 bl, tl, tr, br;
		glsl::particleQuad(e.p(), e.data, bl, tl, tr, br);
		checkNear(bl.x, -20.0f, "quad: scale doubles the half size (x)");
		checkNear(bl.y, -10.0f, "quad: scale doubles the half size (y)");
		checkNear(tr.x, 20.0f, "quad: top right corner (x)");
		checkNear(tr.y, 10.0f, "quad: top right corner (y)");

		e.p().angle = float(M_PI_2);
		glsl::particleQuad(e.p(), e.data, bl, tl, tr, br);
		checkNear(bl.x, 10.0f, "quad: angle rotates the corners (x)");
		checkNear(bl.y, -20.0f, "quad: angle rotates the corners (y)");

		e.data.flags |= XL_PARTICLE_FLAG_ALIGN_WITH_VELOCITY;
		e.p().velocity = glsl::vec2(100.0f, 0.0f);
		glsl::particleQuad(e.p(), e.data, bl, tl, tr, br);
		auto axis = (tl - bl).getNormalized();
		checkNear(axis.x, 1.0f, "AlignWithVelocity: the quad's Y axis follows the velocity");
		checkNear(axis.y + 1.0f, 1.0f, "AlignWithVelocity: and ignores the angle");
	}
}

// Writes a curve at a word offset: header, then the values; returns the byte offset
static uint32_t writeCurve(uint32_t word, uint32_t components,
		std::initializer_list<float> values) {
	s_extra[word] = uint32_t(values.size() / components);
	s_extra[word + 1] = components;
	uint32_t i = 0;
	for (auto v : values) { ::memcpy(&s_extra[word + 2 + i++], &v, sizeof(float)); }
	return word * 4;
}

// The per-axis birth scale: the second axis's own range draws its own number only when seated,
// the words' equality guarding the generator, and the curve's components name the axes - one
// multiplies both, two each its own
static void testScaleAxes() {
	{
		// The unseated pair: the second axis takes the X draw's own value and the stream the
		// single-axis birth always spent. A test that moves `scale` after the ctor moves the
		// pair with it - the harness has no setter, and words left behind would seat a phantom
		Single plain;
		plain.data.scale.init = 2.0f;
		plain.data.scale.rnd = 1.0f;
		plain.data.scaleY = plain.data.scale;
		plain.run(0);

		Single paired;
		paired.data.scale.init = 2.0f;
		paired.data.scale.rnd = 1.0f;
		paired.data.scaleY = paired.data.scale; // equal words, still the unseated state
		paired.run(0);

		checkNear(paired.p().scaleY, paired.p().scale,
				"scale axes: the unseated second axis takes the X draw's value");
		check(paired.p().rng.state == plain.p().rng.state
						&& paired.p().rng.inc == plain.p().rng.inc,
				"scale axes: equal words spend no generator draw");
	}

	{
		// The seated range draws its own number right after the X draw, so every later draw
		// shifts - the stream contract a seated second axis buys
		Single seated;
		seated.data.scale.init = 2.0f;
		seated.data.scale.rnd = 1.0f;
		seated.data.scaleY.init = 3.0f;
		seated.data.scaleY.rnd = 0.5f;
		seated.run(0);

		check(seated.p().scale >= 2.0f && seated.p().scale < 3.0f,
				"scale axes: the X draw keeps its own range");
		check(seated.p().scaleY >= 3.0f && seated.p().scaleY < 3.5f,
				"scale axes: the seated second axis draws from its own range");

		Single plain;
		plain.data.scale.init = 2.0f;
		plain.data.scale.rnd = 1.0f;
		plain.data.scaleY = plain.data.scale;
		plain.run(0);
		check(seated.p().rng.state != plain.p().rng.state,
				"scale axes: the seated draw shifts the stream");
	}

	{
		// A 20x10 quad, as ParticleSystemData::writeParticleSize stores it: each axis's
		// multiplier grows its own half-length
		Single e;
		auto size = glsl::vec2(20.0f, 10.0f);
		e.data.sizeValue = (size / 2.0f).length();
		e.data.sizeNormal = size.getNormalized();
		e.data.scale.init = 2.0f;
		e.data.scaleY.init = 3.0f;
		e.data.scaleY.rnd = 0.0f;
		e.run(0);

		glsl::vec2 bl, tl, tr, br;
		glsl::particleQuad(e.p(), e.data, bl, tl, tr, br);
		checkNear(bl.x, -20.0f, "scale axes: the X multiplier doubles the width");
		checkNear(bl.y, -15.0f, "scale axes: the Y multiplier triples the height");
		checkNear(tr.x, 20.0f, "scale axes: the corner pair agrees (x)");
		checkNear(tr.y, 15.0f, "scale axes: the corner pair agrees (y)");
	}

	{
		// The curve's components: one multiplies both axes, two each its own
		::memset(s_extra, 0, sizeof(s_extra));

		Single one;
		one.data.scaleCurveOffset = writeCurve(2, 1, {0.5f});
		one.run(0);
		checkNear(one.p().scale, 0.5f, "scale axes: a one-component curve multiplies X");
		checkNear(one.p().scaleY, 0.5f, "scale axes: a one-component curve multiplies Y alike");

		Single two;
		two.data.scaleCurveOffset = writeCurve(2, 2, {0.5f, 2.0f});
		two.run(0);
		checkNear(two.p().scale, 0.5f, "scale axes: the first component is X's own");
		checkNear(two.p().scaleY, 2.0f, "scale axes: the second component is Y's own");

		::memset(s_extra, 0, sizeof(s_extra));
	}
}

// The birth interval, Effekseer's GenerationTime: a seated interval puts slot i's birth at the
// step i * interval / dt of every cycle, the schedule the interval's alone - randomness and
// explosiveness stand unread - while the interval's zero keeps the phase schedule byte for byte
static void testInterval() {
	const float dt = 16'667 / 1'000'000.0f;

	{
		// 16 slots over a 59-step cycle, an interval of two steps: slot i is born at its seated
		// step and only there, the second cycle seating the same steps again. The seated step is
		// the engine's own float expression - the two-step ladder the floats round at most one
		// step down, the last slot's 30 arriving as 29
		Emitter e(16, 1.0f, 21);
		e.data.interval = 2.0f * dt;
		auto framesInGen = e.cycleFrames();

		uint32_t seated[16];
		bool ladder = true;
		for (uint32_t i = 0; i < 16; ++i) {
			seated[i] = uint32_t(floor(float(i) * e.data.interval / e.data.dt));
			ladder = ladder && seated[i] + 1u >= 2u * i && seated[i] <= 2u * i;
		}
		check(ladder, "interval: the seated steps are the two-step ladder");

		bool exact = true;
		for (uint32_t k = 0; k < 2 * framesInGen; ++k) {
			e.tick();
			uint32_t step = k % framesInGen;
			for (uint32_t i = 0; i < 16; ++i) {
				exact = exact && e.born[i] == (seated[i] == step);
			}
		}
		check(exact, "interval: slot i is born at its seated step of every cycle");
	}

	{
		// A birth seated past the cycle takes its last step: an interval of 32 steps over four
		// slots seats 0 and 32, the past-cycle pair sharing the cycle's last step
		Emitter e(4, 1.0f, 22);
		e.data.interval = 32.0f * dt;
		auto framesInGen = e.cycleFrames();

		const uint32_t seated[4] = {0, 32, framesInGen - 1, framesInGen - 1};
		bool exact = true;
		for (uint32_t k = 0; k < framesInGen; ++k) {
			e.tick();
			for (uint32_t i = 0; i < 4; ++i) {
				exact = exact && e.born[i] == (seated[i] == k);
			}
		}
		check(exact, "interval: a birth past the cycle takes its last step");
	}

	{
		// The schedule is the interval's alone: explosiveness and randomness stand unread, the
		// births and every particle state equal the plain interval run
		Emitter plain(16, 1.0f, 23);
		plain.data.interval = 2.0f * dt;
		Emitter loaded(16, 1.0f, 23);
		loaded.data.interval = 2.0f * dt;
		loaded.data.explosiveness = 0.75f;
		loaded.data.randomness = 1.0f;

		bool same = true;
		for (uint32_t k = 0; k < plain.cycleFrames(); ++k) {
			plain.tick();
			loaded.tick();
			same = same
					&& 0 == ::memcmp(plain.particles, loaded.particles, sizeof(plain.particles));
		}
		check(same, "interval: explosiveness and randomness stand unread");
	}

	{
		// The zero guard: an interval of zero answers the phase schedule byte for byte, the run
		// that never seated one
		Emitter plain(16, 1.0f, 24);
		plain.data.randomness = 0.5f;
		Emitter zero(16, 1.0f, 24);
		zero.data.randomness = 0.5f;
		zero.data.interval = 0.0f;

		bool same = true;
		for (uint32_t k = 0; k < 2 * plain.cycleFrames(); ++k) {
			plain.tick();
			zero.tick();
			same = same
					&& 0 == ::memcmp(plain.particles, zero.particles, sizeof(plain.particles));
		}
		check(same, "interval: the zero keeps the phase schedule byte for byte");
	}

	checkEq(glsl::particleNewest(16, 0.0f, 2.0f * dt, dt, 60, 0), 0,
			"interval: newest at step 0 is slot 0");
	checkEq(glsl::particleNewest(16, 0.0f, 2.0f * dt, dt, 60, 2), 1,
			"interval: newest at step 2 is slot 1");
	checkEq(glsl::particleNewest(16, 0.0f, 2.0f * dt, dt, 60, 30), 15,
			"interval: newest at step 30 is slot 15");
}

static void testSceneSpace() {	auto setup = [](Single &e) {
		e.data.velocity.init = 100.0f;
		e.data.origin = glsl::vec2(1.0f, 0.0f);
		e.data.angle.init = 0.3f;
		e.data.linearAcceleration.init = glsl::vec2(0.0f, -10.0f);

		// Rotation by 90 degrees, scale 2, translation (100, 50)
		e.transformX = glsl::vec4(0.0f, -2.0f, 100.0f, 0.0f);
		e.transformY = glsl::vec4(2.0f, 0.0f, 50.0f, 0.0f);
		e.transformRotation = float(M_PI_2);
		e.transformScale = 2.0f;
	};

	Single scene(glsl::vec2(10.0f, 0.0f));
	setup(scene);
	scene.data.flags = 0;
	scene.run(0);

	checkNear(scene.p().position.x, 100.0f, "scene space: the birth point is transformed (x)");
	checkNear(scene.p().position.y, 70.0f, "scene space: the birth point is transformed (y)");
	checkNear(scene.p().origin.x, 100.0f, "scene space: origin is transformed (x)");
	checkNear(scene.p().origin.y, 52.0f, "scene space: origin is transformed (y)");
	checkNear(scene.p().velocity.x + 1.0f, 1.0f,
			"scene space: velocity goes through the basis (x)");
	checkNear(scene.p().velocity.y, 200.0f, "scene space: velocity goes through the basis (y)");
	checkNear(scene.p().angle, 0.3f + float(M_PI_2), "scene space: the node's rotation is added");
	checkNear(scene.p().scale, 2.0f, "scene space: the node's scale is applied");
	checkNear(scene.p().linearAcceleration.y, -10.0f, "scene space: gravity stays in scene axes");

	Single local(glsl::vec2(10.0f, 0.0f));
	setup(local);
	local.run(0);

	checkNear(local.p().position.x, 10.0f, "LocalCoords: the birth point is untouched");
	checkNear(local.p().velocity.x, 100.0f, "LocalCoords: velocity is untouched");
	checkNear(local.p().angle, 0.3f, "LocalCoords: angle is untouched");
	checkNear(local.p().scale, 1.0f, "LocalCoords: scale is untouched");
}

static void testCurves() {
	::memset(s_extra, 0, sizeof(s_extra));

	// Four samples at t = 0, 1/4, 2/4, 3/4
	auto floatCurve = writeCurve(2, 1, {0.0f, 1.0f, 3.0f, 7.0f});
	checkNear(glsl::particleSampleCurve(floatCurve, 0.0f).x, 0.0f,
			"curve: the first sample at t = 0");
	checkNear(glsl::particleSampleCurve(floatCurve, 0.25f).x, 1.0f, "curve: a sample at its own t");
	checkNear(glsl::particleSampleCurve(floatCurve, 0.375f).x, 2.0f,
			"curve: linear between neighbouring samples");
	checkNear(glsl::particleSampleCurve(floatCurve, 1.0f).x, 7.0f,
			"curve: holds the last sample at t = 1");
	checkNear(glsl::particleSampleCurve(floatCurve, 2.0f).x, 7.0f, "curve: t is clamped");
	checkNear(glsl::particleSampleCurve(floatCurve, 0.25f).w, 1.0f,
			"curve: a float curve repeats its value in every component");

	auto colorCurve = writeCurve(16, 4, {1.0f, 0.5f, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f, 0.0f});
	auto mid = glsl::particleSampleCurve(colorCurve, 0.25f);
	checkNear(mid.x, 0.5f, "color curve: r halfway");
	checkNear(mid.y, 0.5f, "color curve: g constant");
	checkNear(mid.z, 0.5f, "color curve: b halfway");
	checkNear(mid.w, 0.5f, "color curve: alpha halfway");

	auto single = writeCurve(40, 1, {0.6f});
	checkNear(glsl::particleSampleCurve(single, 0.9f).x, 0.6f, "curve: one sample is a constant");

	// Color over life: at birth, and when the lifetime is half spent
	glsl::ParticleEmitterData emitter;
	::memset(&emitter, 0, sizeof(emitter));
	emitter.colorCurveOffset = colorCurve;

	glsl::ParticleData particle;
	::memset(&particle, 0, sizeof(particle));
	particle.color = glsl::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	particle.fullLifetime = 8;
	particle.currentLifetime = 8;
	checkNear(glsl::particleColor(particle, emitter).z, 0.0f, "color: the curve starts at birth");
	// Two samples at t = 0 and 1/2: half the lifetime reaches the second
	particle.currentLifetime = 4;
	checkNear(glsl::particleColor(particle, emitter).z, 1.0f,
			"color: follows the lifetime fraction");

	// Animation frame: 16 frames, a linear curve over the lifetime whose last sample is 1
	auto frameCurve = writeCurve(60, 1, {0.0f, 1.0f / 3.0f, 2.0f / 3.0f, 1.0f});
	emitter.animFrameCurveOffset = frameCurve;
	particle.fullLifetime = 16;
	bool rising = true;
	uint32_t last = 0;
	for (uint32_t life = 16; life > 0; --life) {
		particle.currentLifetime = life;
		auto frame = glsl::particleAnimFrame(particle, emitter, 16);
		rising = rising && frame >= last && frame < 16;
		last = frame;
	}
	check(rising, "animation frame: rises with the lifetime and stays below the frame count");
	particle.currentLifetime = 16;
	checkEq(glsl::particleAnimFrame(particle, emitter, 16), 0, "animation frame: 0 at birth");
	particle.currentLifetime = 1;
	checkEq(glsl::particleAnimFrame(particle, emitter, 16), 15,
			"animation frame: the last near death");
	emitter.animFrameCurveOffset = 0;
	checkEq(glsl::particleAnimFrame(particle, emitter, 16), 0,
			"animation frame: 0 without a curve");

	glsl::ParticleFrameData frame;
	::memset(&frame, 0, sizeof(frame));
	frame.textureRect = glsl::vec4(0.5f, 0.0f, 0.5f, 1.0f);
	frame.hFrames = 4;
	frame.vFrames = 4;
	glsl::vec2 uv0, uv1;
	glsl::particleFrameCell(frame, 5, uv0, uv1);
	checkNear(uv0.x, 0.625f, "frame cell: column 1 starts a quarter into the rect");
	checkNear(uv1.x, 0.75f, "frame cell: and is a quarter wide");
	checkNear(uv0.y, 0.25f, "frame cell: row 1 from the top of the image");
	checkNear(uv1.y, 0.5f, "frame cell: and a quarter high");
}

static glsl::vec4 hueReference(glsl::vec4 c, float turns) {
	// Godot's hue matrix as base + cos * A + sin * B, its vectors as rows, multiplied the long way
	const float a = turns * 2.0f * float(M_PI);
	const float cs = sprt::cos(a);
	const float sn = sprt::sin(a);
	const float base[3][3] = {{0.299f, 0.587f, 0.114f}, {0.299f, 0.587f, 0.114f},
		{0.299f, 0.587f, 0.114f}};
	const float cosPart[3][3] = {{0.701f, -0.587f, -0.114f}, {-0.299f, 0.413f, -0.114f},
		{-0.300f, -0.588f, 0.886f}};
	const float sinPart[3][3] = {{0.168f, 0.330f, -0.497f}, {-0.328f, 0.035f, 0.292f},
		{1.250f, -1.050f, -0.203f}};
	const float in[3] = {c.x, c.y, c.z};
	float out[3] = {0.0f, 0.0f, 0.0f};
	for (int row = 0; row < 3; ++row) {
		for (int col = 0; col < 3; ++col) {
			out[row] +=
					(base[row][col] + cosPart[row][col] * cs + sinPart[row][col] * sn) * in[col];
		}
	}
	return glsl::vec4(out[0], out[1], out[2], c.w);
}

// Godot's constants are rounded to three digits: a gray drifts by about 1e-3
static void checkHue(float got, float expected, const char *name) {
	++s_checks;
	bool ok = sprt::fabs(got - expected) <= 5e-3f;
	sprt::cout << (ok ? "[ OK ] " : "[FAIL] ") << name;
	if (!ok) {
		sprt::cout << "  (got " << got << ", expected " << expected << ")";
	}
	sprt::cout << "\n";
	if (!ok) {
		++s_failures;
	}
}

static void testHue() {
	auto red = glsl::vec4(0.9f, 0.2f, 0.1f, 0.5f);

	auto same = glsl::particleHueRotate(red, 0.0f);
	checkHue(same.x, red.x, "hue 0: r is kept");
	checkHue(same.y, red.y, "hue 0: g is kept");
	checkHue(same.z, red.z, "hue 0: b is kept");

	auto full = glsl::particleHueRotate(red, 1.0f);
	checkHue(full.x, red.x, "hue 1: a full turn keeps r");
	checkHue(full.y, red.y, "hue 1: a full turn keeps g");

	auto gray = glsl::particleHueRotate(glsl::vec4(0.4f, 0.4f, 0.4f, 1.0f), 0.37f);
	checkHue(gray.x, 0.4f, "hue: gray stays gray (r)");
	checkHue(gray.z, 0.4f, "hue: gray stays gray (b)");

	auto turned = glsl::particleHueRotate(red, 1.0f / 3.0f);
	auto luma = [](glsl::vec4 c) { return 0.299f * c.x + 0.587f * c.y + 0.114f * c.z; };
	checkHue(luma(turned), luma(red), "hue: luminance is kept");
	checkNear(turned.w, 0.5f, "hue: alpha is kept");

	auto reference = hueReference(red, 1.0f / 3.0f);
	checkNear(turned.x, reference.x, "hue 1/3: r matches Godot's matrix");
	checkNear(turned.y, reference.y, "hue 1/3: g matches Godot's matrix");
	checkNear(turned.z, reference.z, "hue 1/3: b matches Godot's matrix");
	check(turned.z > turned.x, "hue 1/3: a positive turn moves red towards blue, as in Godot");
}

static void testNewest() {
	const float dt = 16'667 / 1'000'000.0f;
	checkEq(glsl::particleNewest(16, 1.0f, 0.0f, dt, 60, 0), 15,
			"newest: a burst makes the last index newest");
	checkEq(glsl::particleNewest(16, 0.0f, 0.0f, dt, 60, 0), 0,
			"newest: the first step of a cycle births 0");

	Emitter e(16, 1.0f, 12);
	auto framesInGen = e.cycleFrames();
	bool matches = true;
	for (uint32_t k = 0; k < framesInGen; ++k) {
		auto step = e.frame;
		e.tick();
		for (uint32_t i = 0; i < 16; ++i) {
			if (e.born[i]) {
				matches = matches
						&& glsl::particleNewest(16, 0.0f, e.data.interval, e.data.dt, framesInGen,
								   step)
								== i;
			}
		}
	}
	check(matches, "newest: the particle born on a step is the newest after it");
}

// Effekseer's parent binding, the continuous child of Particle-39: the bit empties the emitter's
// own birth schedule on every cycle - the host that knows the parent seats its births - while the
// step still ages a particle the host seated, so the bound emitter is no second system
static void testParentBound() {
	Emitter e(8, 1.0f, 5);
	auto framesInGen = e.cycleFrames();
	e.data.flags |= XL_PARTICLE_FLAG_PARENT_BOUND;

	// A whole cycle of the suppressed schedule: no birth, nothing alive, the generators untouched
	bool anyBorn = false;
	bool anyDrawn = false;
	for (uint32_t k = 0; k < framesInGen + 2u; ++k) {
		e.tick();
		anyBorn = anyBorn || e.births() != 0;
		anyDrawn = anyDrawn || e.alive() != 0;
	}
	check(!anyBorn, "parent-bound: the own schedule births nothing");
	check(!anyDrawn, "parent-bound: no particle stands alive without a host birth");

	// The step the bit flags still ages what the host seats: one particle born by particleEmit at
	// the point a parent particle stood, aged by the same update until it dies on schedule
	glsl::ParticleFrameData f;
	::memset(&f, 0, sizeof(f));
	f.framesInGen = framesInGen;
	f.genframe = e.frame;
	f.nframes = 0;
	f.cycle = e.cycle;
	f.seed = e.seed;
	f.dt = e.data.dt;
	glsl::particleSeedRng(e.particles[0].rng, e.seed, 0);
	glsl::particleEmit(e.particles[0], e.data, f, glsl::vec2(3.0f, 4.0f));
	e.particles[0].birthScale = e.particles[0].scale;
	e.particles[0].birthScaleY = e.particles[0].scaleY;
	checkEq(e.particles[0].currentLifetime, e.particles[0].fullLifetime,
			"parent-bound: a host-seated birth stands alive");

	// The lifetime 1.0 s covers 59 steps; aging one per tick, the particle dies on the last of
	// them and the run births nothing on its own
	uint32_t ticks = 0;
	bool bornWhileAging = false;
	while (e.particles[0].currentLifetime > 0u) {
		e.tick();
		++ticks;
		bornWhileAging = bornWhileAging || e.births() != 0;
	}
	check(!bornWhileAging, "parent-bound: the aging run births nothing on its own");
	checkEq(ticks, uint32_t(e.particles[0].fullLifetime),
			"parent-bound: the seated particle ages one step per tick and dies on schedule");

	// The bit off is the standalone schedule again: the first cycle births every slot
	e.data.flags &= ~uint32_t(XL_PARTICLE_FLAG_PARENT_BOUND);
	bool bornAgain = false;
	for (uint32_t k = 0; k < framesInGen + 2u && !bornAgain; ++k) {
		e.tick();
		bornAgain = e.births() != 0;
	}
	check(bornAgain, "parent-bound: the bit off restores the own schedule's births");
}

static void testDeflectorSeats() {
	// The stride contract first: the GPU indexes the seats at the std430 stride, so the seat the
	// C++ text writes and the seat the step reads are the same words only while these hold
	checkEq(int64_t(sizeof(glsl::ParticleDeflector)), 32, "deflector seats: the seat is the 32-byte stride");
	checkEq(int64_t(__builtin_offsetof(glsl::ParticleEmitterData, deflectors)), 328,
			"deflector seats: the array opens at its 8-aligned std430 seat");

	auto seat = [](glsl::vec2 a, glsl::vec2 b, uint32_t response) {
		return glsl::ParticleDeflector{a, b, 0.0f, uint32_t(XL_PARTICLE_DEFLECTOR_SEGMENT),
				response, 0u};
	};

	glsl::ParticleEmitterData emitter;
	::memset(&emitter, 0, sizeof(emitter));
	// Two vertical segments, x = 10 and x = 20; a straight step across the second never meets
	// the first, so the second seat answers in its own words
	emitter.deflectors[0] = seat(glsl::vec2(10.0f, -10.0f), glsl::vec2(10.0f, 10.0f),
			XL_PARTICLE_DEFLECTOR_KILL);
	emitter.deflectors[1] = seat(glsl::vec2(20.0f, -10.0f), glsl::vec2(20.0f, 10.0f),
			XL_PARTICLE_DEFLECTOR_KILL);

	auto walk = [&](glsl::vec2 previous, glsl::vec2 position) {
		glsl::ParticleData particle;
		::memset(&particle, 0, sizeof(particle));
		particle.position = position;
		particle.currentLifetime = 16;
		glsl::particleDeflectors(particle, emitter, previous);
		return particle.currentLifetime;
	};

	checkEq(walk(glsl::vec2(9.0f, 0.0f), glsl::vec2(11.0f, 0.0f)), 0,
			"deflector seats: kill at the first seat");
	checkEq(walk(glsl::vec2(19.0f, 0.0f), glsl::vec2(21.0f, 0.0f)), 0,
			"deflector seats: kill at the second seat");
	checkEq(walk(glsl::vec2(0.0f, 0.0f), glsl::vec2(2.0f, 0.0f)), 16,
			"deflector seats: a path past both figures lives");

	// The first seat unused: the second answers alone
	emitter.deflectors[0] = glsl::ParticleDeflector{glsl::vec2(0.0f, 0.0f),
			glsl::vec2(0.0f, 0.0f), 0.0f, 0u, 0u, 0u};
	checkEq(walk(glsl::vec2(19.0f, 0.0f), glsl::vec2(21.0f, 0.0f)), 0,
			"deflector seats: the second seat answers alone");

	// A bounce on the second seat mirrors the velocity and keeps the particle alive
	emitter.deflectors[1] = seat(glsl::vec2(20.0f, -10.0f), glsl::vec2(20.0f, 10.0f),
			XL_PARTICLE_DEFLECTOR_BOUNCE);
	glsl::ParticleData particle;
	::memset(&particle, 0, sizeof(particle));
	particle.position = glsl::vec2(21.0f, 0.0f);
	particle.velocity = glsl::vec2(2.0f, 0.0f);
	particle.currentLifetime = 16;
	glsl::particleDeflectors(particle, emitter, glsl::vec2(19.0f, 0.0f));
	checkEq(particle.currentLifetime, 16, "deflector seats: bounce keeps the particle alive");
	check(particle.velocity.x < 0.0f && particle.velocity.y == 0.0f,
			"deflector seats: bounce mirrors the velocity off the second seat");
	check(particle.position.x < 20.0f,
			"deflector seats: bounce takes the position back across the figure");
}

// Effekseer's ring renderer, the stage's own draw: the pass's vertex seats grow from the
// quad's own six to the ring's count, and the ring's edges seat by the emitter's words - both
// radii multiplied by the scale the curve left standing, the middle edge at the band's half,
// the corner angles the arc's degrees turned by the particle's own angle word
static void testRing() {
	// The seat contract: the constants the pass strides its list by, and the two counts an
	// emitter's words choose between
	checkEq(XL_PARTICLE_RING_SEATS, XL_PARTICLE_RING_CORNERS * 2 * 6,
			"ring: the seats are the corners times two bands times a triangle pair");

	glsl::ParticleEmitterData emitter;
	::memset(&emitter, 0, sizeof(emitter));
	checkEq(glsl::particleVertexSeats(emitter), 6u,
			"ring: the unseated emitter keeps the quad's six seats");

	emitter.ringOuter = 40.0f;
	emitter.ringInner = 20.0f;
	emitter.ringArcFrom = 0.0f;
	emitter.ringArcTo = 360.0f;
	checkEq(glsl::particleVertexSeats(emitter), 192u,
			"ring: a seated ring owns the ring's own seats");

	glsl::ParticleData particle;
	::memset(&particle, 0, sizeof(particle));
	particle.scale = 2.0f;

	// The radii: both multiplied by the drawn scale, the middle edge at the band's half - the
	// even Center Ratio the stage seats beside the corner count
	float inner = 0.0f;
	float middle = 0.0f;
	float outer = 0.0f;
	glsl::particleRingRadii(particle, emitter, inner, middle, outer);
	checkNear(inner, 40.0f, "ring: the inner edge is the emitter's own times the drawn scale");
	checkNear(outer, 80.0f, "ring: the outer edge is the emitter's own times the drawn scale");
	checkNear(middle, 60.0f, "ring: the middle edge stands at the band's half");

	// The full circle's corners: the arc fraction walks the degrees between the two words, so
	// the quarter corner of a 0 to 360 arc points where the quad's own angle word would
	glsl::vec2 at0 = glsl::particleRingPoint(particle, emitter, 0.0f, outer);
	glsl::vec2 atQuarter = glsl::particleRingPoint(particle, emitter, 0.25f, outer);
	glsl::vec2 atHalf = glsl::particleRingPoint(particle, emitter, 0.5f, outer);
	checkNear(at0.x, 80.0f, "ring: the full arc's first corner points along the x axis");
	checkNear(at0.y, 0.0f, "ring: the first corner's y is the axis' own zero");
	checkNear(atQuarter.x, 0.0f, "ring: the quarter corner's x is the turn's own zero");
	checkNear(atQuarter.y, 80.0f, "ring: the quarter corner points along the y axis");
	checkNear(atHalf.x, -80.0f, "ring: the half corner points against the x axis");

	// The particle's own angle word turns the ring with the quad: a quarter-turn rotation
	// moves the first corner onto the y axis
	particle.angle = 1.5707963267948966f;
	at0 = glsl::particleRingPoint(particle, emitter, 0.0f, outer);
	checkNear(at0.x, 0.0f, "ring: the particle's angle turns the first corner's x to zero");
	checkNear(at0.y, 80.0f, "ring: the particle's angle seats the first corner on the y axis");

	// A partial arc: the arc fraction walks the degrees between the two words alone, the birth
	// shape ring's own plain lerp - 90 to 180 spans the second quadrant only
	particle.angle = 0.0f;
	emitter.ringArcFrom = 90.0f;
	emitter.ringArcTo = 180.0f;
	at0 = glsl::particleRingPoint(particle, emitter, 0.0f, outer);
	glsl::vec2 atEnd = glsl::particleRingPoint(particle, emitter, 1.0f, outer);
	checkNear(at0.y, 80.0f, "ring: the partial arc opens at its own first degree");
	checkNear(at0.x, 0.0f, "ring: the partial arc's first corner is on the y axis");
	checkNear(atEnd.x, -80.0f, "ring: the partial arc closes at its own last degree");
	checkNear(atEnd.y, 0.0f, "ring: the partial arc's last corner is against the x axis");
}

// Effekseer's ribbon taken in the screen plane, the strip of Particle-41: the seats stay the
// quad's own six - the segment a particle owns between itself and the next born is a quad of
// the same pair of list triangles - and the segment's corners seat by the width word, each
// end's half the emitter's width multiplied by the scale that end's curve left standing, the
// perpendicular of the segment's own direction the plain band the simplest ribbon draws
static void testStrip() {
	// The seat contract: a seated strip keeps the quad's own seats, and a ring standing ahead
	// of it keeps the ring's own count
	glsl::ParticleEmitterData emitter;
	::memset(&emitter, 0, sizeof(emitter));
	checkEq(glsl::particleVertexSeats(emitter), 6u,
			"strip: the unseated emitter keeps the quad's six seats");

	emitter.stripWidth = 4.0f;
	checkEq(glsl::particleVertexSeats(emitter), 6u,
			"strip: a seated strip keeps the quad's own six seats");

	emitter.ringOuter = 40.0f;
	checkEq(glsl::particleVertexSeats(emitter), 192u,
			"strip: a ring standing ahead of the strip keeps the ring's own seats");

	glsl::ParticleData a;
	glsl::ParticleData b;
	::memset(&a, 0, sizeof(a));
	::memset(&b, 0, sizeof(b));
	a.scale = 1.0f;
	b.scale = 1.0f;
	a.position = glsl::vec2(0.0f, 0.0f);
	b.position = glsl::vec2(10.0f, 0.0f);

	// The horizontal segment: the perpendicular of the x axis is the y axis, both halves the
	// width's own half at a scale of one
	glsl::vec2 bl;
	glsl::vec2 tl;
	glsl::vec2 tr;
	glsl::vec2 br;
	glsl::particleStripSegment(a, b, emitter, bl, tl, tr, br);
	checkNear(bl.x, 0.0f, "strip: the segment's first end seats at its particle");
	checkNear(bl.y, -2.0f, "strip: the bottom left corner sits half the width below");
	checkNear(tl.y, 2.0f, "strip: the top left corner sits half the width above");
	checkNear(tr.x, 10.0f, "strip: the segment's second end seats at the next particle");
	checkNear(tr.y, 2.0f, "strip: the top right corner sits half the width above");
	checkNear(br.y, -2.0f, "strip: the bottom right corner sits half the width below");

	// The width multiplies the scale the curve left standing, each end its own - the ring's
	// own one-scale precedent, the band taking the one scale both ends share
	b.scale = 2.0f;
	glsl::particleStripSegment(a, b, emitter, bl, tl, tr, br);
	checkNear(tl.y, 2.0f, "strip: the first end keeps its own scale's half");
	checkNear(tr.y, 4.0f, "strip: the second end's half is the width times its own scale");

	// A vertical segment turns the perpendicular with it: the band's width lands across the x
	// axis the same way
	a.scale = 1.0f;
	b.scale = 1.0f;
	a.position = glsl::vec2(0.0f, 0.0f);
	b.position = glsl::vec2(0.0f, 8.0f);
	glsl::particleStripSegment(a, b, emitter, bl, tl, tr, br);
	checkNear(tl.x, -2.0f, "strip: the turned segment carries its width across the x axis");
	checkNear(bl.x, 2.0f, "strip: the turned segment's bottom corner mirrors the top");
	checkNear(tl.y, 0.0f, "strip: the turned segment's top corner stays at its end");

	// A segment of zero length leaves particleNormalize's zero cross: every corner at the
	// point, the degenerate quad a dead seat draws
	b.position = glsl::vec2(0.0f, 0.0f);
	glsl::particleStripSegment(a, b, emitter, bl, tl, tr, br);
	checkNear(tl.x, 0.0f, "strip: a zero-length segment folds to its point");
	checkNear(tr.y, 0.0f, "strip: a zero-length segment leaves the far end at the same point");
	checkNear(br.x, 0.0f, "strip: a zero-length segment's bottom corners fold with the top");
}

// Effekseer's position by curve, the start and the end point read as a displacement over the
// lifetime fraction: every step past particleStep adds the walk the fraction made, so the
// offset lands on the birth point's own path and the velocity's walk and the curve's one sum -
// the integrator the plan keeps seated in its place
static void testPositionCurve() {
	// The seat contract: the word sits in the strip block's own hand-carried padding, the word
	// map still 560 under its static_assert
	checkEq(int64_t(__builtin_offsetof(glsl::ParticleEmitterData, positionCurveOffset)), 508,
			"position curve: the offset word seats at the strip block's padding");
	checkEq(int64_t(sizeof(glsl::ParticleEmitterData)), 560,
			"position curve: the word map keeps its 560");

	::memset(s_extra, 0, sizeof(s_extra));

	// The plan's own acceptance shape: a curve giving (10, 0) at the end of life walks the
	// particle to it at zero velocity, k steps in at the sampler's own share - the stops sit at
	// k / count and the last one holds, the same ladder every curve of the extra buffer climbs
	uint32_t curve = writeCurve(2, 2, {0.0f, 0.0f, 10.0f, 0.0f});
	Single seated;
	seated.data.positionCurveOffset = curve;
	seated.run(30);
	checkNear(seated.p().position.x,
			10.0f * sprt::min(float(30) * 2.0f / float(seated.p().fullLifetime), 1.0f),
			"position curve: k steps stand at the curve's own fraction of the lifetime");
	checkNear(seated.p().position.y, 0.0f, "position curve: the second axis walks its own zero");

	// The walk is measured from the first stop: a curve starting away from the origin walks its
	// own delta, not its own value - the start point is the walk's zero
	Single offset;
	offset.data.positionCurveOffset = writeCurve(40, 2, {6.0f, 0.0f, 16.0f, 0.0f});
	offset.run(30);
	checkNear(offset.p().position.x,
			10.0f * sprt::min(float(30) * 2.0f / float(offset.p().fullLifetime), 1.0f),
			"position curve: the walk is measured from the curve's first stop");

	// The generator stands unread: the same seed walks the same stream beside the curve
	Single plain;
	plain.run(30);
	check(plain.p().rng.state == seated.p().rng.state && plain.p().rng.inc == seated.p().rng.inc,
			"position curve: the curve spends no draw of the generator");
	checkNear(plain.p().position.x, 0.0f,
			"position curve: no curve leaves the birth point standing");

	// The file with both sums them - the plan's own letter: the velocity's walk and the curve's
	// one are two paths on one birth point
	Single velocity;
	velocity.data.velocity.init = 30.0f;
	velocity.run(30);
	Single both;
	both.data.velocity.init = 30.0f;
	both.data.positionCurveOffset = curve;
	both.run(30);
	checkNear(both.p().position.x, velocity.p().position.x + seated.p().position.x,
			"position curve: the velocity's walk and the curve's one sum");

	// The word's zero guard: an emitter without the offset walks the particles it always did,
	// byte for byte
	Single unseated;
	unseated.data.positionCurveOffset = 0;
	unseated.run(30);
	check(::memcmp(&unseated.p(), &plain.p(), sizeof(glsl::ParticleData)) == 0,
			"position curve: the unseated word keeps the old particles whole");

	// Two steps of one frame walk the same two fractions the ladder climbs inside
	// particleUpdate's own loop
	Single paird;
	paird.data.positionCurveOffset = curve;
	paird.maxSteps = 2;
	s_points[0] = glsl::vec2(0.0f, 0.0f);
	s_pointCount = 1;
	paird.tick();
	for (uint32_t i = 0; i < 15; ++i) { paird.tick(2); }
	s_pointCount = 0;
	check(::memcmp(&paird.p(), &seated.p(), sizeof(glsl::ParticleData)) == 0,
			"position curve: two steps of one frame equal two frames of one step");

	// A birth stands at the birth point - the curve adds nothing at the birth, the plan's own
	// "after the step" - and a reborn slot restarts its walk from the same zero
	Emitter cycled(4, 0.3f, 9);
	cycled.data.positionCurveOffset = curve;
	auto frames = cycled.cycleFrames();
	bool birthsAtPoint = true;
	for (uint32_t k = 0; k < frames * 3; ++k) {
		cycled.tick();
		for (uint32_t i = 0; i < 4; ++i) {
			if (cycled.born[i]) {
				birthsAtPoint = birthsAtPoint && cycled.particles[i].position.x == 0.0f
						&& cycled.particles[i].position.y == 0.0f;
			}
		}
	}
	check(birthsAtPoint, "position curve: a birth stands at the birth point, the curve off it");
}

int main(int argc, const char *argv[]) {
	int result = 0;
	sprt::initialize(sprt::AppConfig(), result);
	if (result != 0) {
		return result;
	}

	testExplosive();
	testUniform();
	testHalfExplosive();
	testRandomness();
	testCounters();
	testLifetimeMax();
	testSteps();
	testClock();
	testCountChange();
	testPoints();
	testSeedRng();
	testRandom();
	testKinematics();
	testScaleAxes();
	testInterval();
	testSceneSpace();
	testCurves();
	testHue();
	testNewest();
	testParentBound();
	testDeflectorSeats();
	testRing();
	testStrip();
	testPositionCurve();

	sprt::cout << s_checks << " checks, " << s_failures << " failures\n";

	sprt::terminate();
	return s_failures == 0 ? 0 : 1;
}
