
// Embox EL0 entropy backend.
//
// THIS IS NOT A CSPRNG, and nothing here can make it one: getrandom(278) is M2,
// and even once it lands Embox has no entropy pool behind it. What follows is a
// clock-seeded LCG -- adequate for hash seeds and jitter, NOT for keys, nonces or
// anything an adversary sees.
//
// It is the same construction the hosted Embox target already uses
// (runtime_core_random.cpp), kept identical so that both Embox models have the
// same (weak) properties rather than two different weak ones.
//
// The honest alternative -- failing -- was rejected because getentropy() failing
// takes down every hash table in the runtime. A caller that needs real entropy
// has to be told by a human, not by an errno.

#ifndef __SPRT_BUILD
#define __SPRT_BUILD 1
#endif

#include <sprt/c/sys/__sprt_random.h>
#include <sprt/c/__sprt_time.h>
#include <sprt/c/__sprt_errno.h>

namespace sprt {

// Mixed into the seed so two calls in the same clock tick do not repeat.
static __SPRT_ID(uint32_t) s_el0_random_counter = 0;

static void __el0_fill_random(void *buffer, __SPRT_ID(size_t) length) {
	auto p = static_cast<unsigned char *>(buffer);
	struct __SPRT_TIMESPEC_NAME ts = {0, 0};
	__sprt_clock_gettime(__SPRT_CLOCK_MONOTONIC, &ts);
	auto seed = static_cast<__SPRT_ID(uint32_t)>(ts.tv_nsec)
			^ static_cast<__SPRT_ID(uint32_t)>(ts.tv_sec) ^ 0xA5A5'A5A5u
			^ (++s_el0_random_counter * 2'654'435'761u);
	for (__SPRT_ID(size_t) i = 0; i < length; ++i) {
		seed = seed * 1'664'525u + 1'013'904'223u;
		// The high bits of an LCG are the least bad ones.
		p[i] = static_cast<unsigned char>(seed >> 16);
	}
}

// The two entry points runtime_core_random.cpp forwards to, with the same
// signatures the wasm sibling provides.

static __SPRT_ID(ssize_t) getrandom(void *__buffer, __SPRT_ID(size_t) __length, unsigned flags) {
	(void)flags; // GRND_RANDOM / GRND_NONBLOCK are meaningless without a pool
	if (!__buffer) {
		__sprt_errno = EFAULT;
		return -1;
	}
	__el0_fill_random(__buffer, __length);
	return static_cast<__SPRT_ID(ssize_t)>(__length);
}

static int getentropy(void *__buffer, __SPRT_ID(size_t) __length) {
	if (__length > 256 || __length == 0 || !__buffer) {
		__sprt_errno = EINVAL;
		return -1;
	}
	__el0_fill_random(__buffer, __length);
	return 0;
}

} // namespace sprt
