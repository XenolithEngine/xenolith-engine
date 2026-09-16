
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

#include "../include/__el0_syscall.h"

namespace sprt {

// WHAT CHANGED AND WHY. This used to be a linear congruential generator seeded
// from the clock, right here in the process -- which was not merely weak, it was
// undetectably weak: a caller had no way to learn that the bytes it asked the
// operating system for had never left userspace. getrandom(278) does not make
// them strong. /dev/urandom on this board is itself an LCG stirred with clock(),
// and the syscall's own documentation (ABI section 6.2) says so. What it does is
// put the claim where a caller can read it, and put one generator behind every
// asker instead of one per process.
//
// GRND_RANDOM is passed through and the kernel refuses it, for the same reason:
// it asks for the entropy-accounted source specifically, and there is none.

static __SPRT_ID(ssize_t) getrandom(void *__buffer, __SPRT_ID(size_t) __length, unsigned flags) {
	if (!__buffer) {
		__sprt_errno = EFAULT;
		return -1;
	}
	return (__SPRT_ID(ssize_t))__el0_ret(__el0_getrandom(__buffer, __length, flags));
}

static int getentropy(void *__buffer, __SPRT_ID(size_t) __length) {
	if (__length > 256 || __length == 0 || !__buffer) {
		__sprt_errno = EINVAL;
		return -1;
	}
	// getentropy promises all or nothing, and the syscall may answer short.
	__SPRT_ID(size_t) done = 0;
	while (done < __length) {
		auto n = __el0_ret(
				__el0_getrandom((unsigned char *)__buffer + done, __length - done, 0));
		if (n <= 0) {
			return -1;
		}
		done += (__SPRT_ID(size_t))n;
	}
	return 0;
}

} // namespace sprt
