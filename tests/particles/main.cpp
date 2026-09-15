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

namespace STAPPLER_VERSIONIZED stappler::glsl {

uint particleEmissionPointCount() { return s_pointCount; }

vec2 particleEmissionPoint(uint index) { return s_points[index]; }

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

	Emitter(uint32_t count, float lifetime, uint32_t seed_) : seed(seed_) {
		::memset(&data, 0, sizeof(data));
		data.count = count;
		data.frameInterval = 16'667;
		data.dt = data.frameInterval / 1'000'000.0f;
		data.lifetime.init = lifetime;
		data.scale.init = 1.0f;
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

		for (uint32_t i = 0; i < data.count; ++i) {
			wasAlive[i] = particles[i].currentLifetime > nframes;
			auto before = particles[i].rng;
			glsl::particleUpdate(particles[i], data, f, i);
			// Only an emission draws random numbers
			born[i] = before.state != particles[i].rng.state;
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

static void testSceneSpace() {
	auto setup = [](Single &e) {
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
	testLifetimeMax();
	testSteps();
	testClock();
	testCountChange();
	testPoints();
	testSeedRng();
	testRandom();
	testKinematics();
	testSceneSpace();

	sprt::cout << s_checks << " checks, " << s_failures << " failures\n";

	sprt::terminate();
	return s_failures == 0 ? 0 : 1;
}
