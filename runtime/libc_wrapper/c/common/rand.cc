/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#include <sprt/c/__sprt_stdlib.h>
#include <sprt/cxx/__mutex/unique_lock.h>
#include <sprt/runtime/hash.h>
#include <sprt/runtime/thread/qmutex.h>
#include <sprt/thirdparty/pcg_random.hpp>

namespace sprt::libc {

// from musl-libc
static unsigned temper(unsigned x) {
	x ^= x >> 11;
	x ^= x << 7 & 0x9D2C'5680;
	x ^= x << 15 & 0xEFC6'0000;
	x ^= x >> 18;
	return x;
}

// from musl-libc
static int rand_r(unsigned *seed) { return temper(*seed = *seed * 1'103'515'245 + 12'345) / 2; }


template <size_t IntSize>
struct _RandState;

template <>
struct _RandState<4> {
	template <typename IntType>
	static uint64_t toSeed(IntType base) {
		return hash64((const char *)&base, sizeof(unsigned));
	}

	pcg32 rng;
	qmutex lock;
};

template <>
struct _RandState<8> {
	template <typename IntType>
	static uint64_t toSeed(IntType base) {
		return hash64((const char *)&base, sizeof(unsigned));
	}

	pcg64 rng;
	qmutex lock;
};

static _RandState<sizeof(unsigned)> s_randState;

static int rand(void) {
	unique_lock lock(s_randState.lock);
	auto value = s_randState.rng();
	return static_cast<int>(value & __SPRT_RAND_MAX);
}

static void srand(unsigned base) {
	unique_lock lock(s_randState.lock);
	s_randState.rng.seed(_RandState<sizeof(unsigned)>::toSeed(base));
}

static _RandState<sizeof(unsigned long)> s_randomState;

static long int random(void) {
	unique_lock lock(s_randomState.lock);
	auto value = s_randomState.rng();
	return static_cast<long int>(value & __SPRT_LONG_MAX);
}

static void srandom(unsigned int base) {
	unique_lock lock(s_randomState.lock);
	s_randomState.rng.seed(_RandState<sizeof(unsigned long)>::toSeed(base));
}

} // namespace sprt::libc

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(rand_impl)(void) { return sprt::libc::rand(); }

__SPRT_C_FUNC void __SPRT_ID(srand_impl)(unsigned base) { return sprt::libc::srand(base); }

__SPRT_C_FUNC int __SPRT_ID(rand_r)(unsigned *v) { return sprt::libc::rand_r(v); }

__SPRT_C_FUNC long int __SPRT_ID(random)(void) { return sprt::libc::random(); }
__SPRT_C_FUNC void __SPRT_ID(srandom)(unsigned int seed) { sprt::libc::srandom(seed); }

} // namespace sprt
