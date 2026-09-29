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

// Two small libc surfaces Mesa reaches for (xenolith-os docs/EMBOX-LAVAPIPE.md,
// R4 and R5), checked where the platform has them -- Embox EL1 here:
//
//   sched_affinity   sched_getcpu, sched_getaffinity and sched_setaffinity on
//                    the calling thread, and the CPU_*_S macros, of which
//                    CPU_CLR_S and CPU_ISSET_S used to expand to CPU_SET_S
//   flock            <sys/file.h>: an exclusive lock refuses a second
//                    descriptor, shared locks share, LOCK_UN lets go

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <sys/file.h>
#include <unistd.h>
#include "../tests.h"

namespace sprt {

namespace {

struct Checker {
	int failures = 0;
	void operator()(bool cond, const char *msg) {
		printf("  %s: %s\n", cond ? "PASS" : sprt::test::failed("FAIL"), msg);
		if (!cond) {
			++failures;
		}
	}
};

} // namespace

void performSchedAffinityTest() {
	Checker check;

	// The macros, on any platform.
	cpu_set_t s;
	CPU_ZERO_S(sizeof(s), &s);
	CPU_SET_S(3, sizeof(s), &s);
	check(CPU_ISSET_S(3, sizeof(s), &s) != 0 && CPU_ISSET_S(2, sizeof(s), &s) == 0,
			"CPU_ISSET_S tells a set bit from a clear one");
	check(CPU_COUNT_S(sizeof(s), &s) == 1, "... and CPU_ISSET_S did not set the bit it asked about");
	CPU_CLR_S(3, sizeof(s), &s);
	check(CPU_ISSET_S(3, sizeof(s), &s) == 0 && CPU_COUNT_S(sizeof(s), &s) == 0,
			"CPU_CLR_S clears");

#if defined(__EMBOX__) && !defined(__EMBOX_USER__)
	const long n = sysconf(_SC_NPROCESSORS_ONLN);
	const int cpu = sched_getcpu();
	printf("sched_affinity: %ld core(s) online, running on %d\n", n, cpu);
	check(n >= 1 && cpu >= 0 && cpu < n, "sched_getcpu is a core that is online");

	cpu_set_t orig;
	CPU_ZERO(&orig);
	check(sched_getaffinity(0, sizeof(orig), &orig) == 0, "sched_getaffinity(0) answers");
	check(CPU_COUNT(&orig) == n, "... with every online core in the mask");

	errno = 0;
	check(sched_getaffinity(0, 0, &orig) == -1 && errno == EINVAL,
			"a mask too small for the answer is EINVAL");

	int stayed = 0;
	for (int target = 0; target < n; ++target) {
		cpu_set_t one;
		CPU_ZERO(&one);
		CPU_SET(target, &one);
		if (sched_setaffinity(0, sizeof(one), &one) != 0) {
			break;
		}
		int here = 0;
		for (int i = 0; i < 100; ++i) {
			sched_yield();
			here += sched_getcpu() == target ? 1 : 0;
		}
		stayed += here == 100 ? 1 : 0;
	}
	printf("sched_affinity: pinned to each of %ld core(s) in turn, stayed on %d\n", n, stayed);
	check(stayed == n, "sched_setaffinity to one core keeps the thread there");

	check(sched_setaffinity(0, sizeof(orig), &orig) == 0, "the original mask goes back");
	cpu_set_t back;
	CPU_ZERO(&back);
	sched_getaffinity(0, sizeof(back), &back);
	check(CPU_EQUAL(&back, &orig), "... and reads back as it was");
#else
	printf("sched_affinity: the calls are Embox EL1's here; macros only\n");
#endif

	printf("sched_affinity: %s (%d failure(s))\n", check.failures ? "FAILED" : "PASS",
			check.failures);
}

void performFlockTest() {
	Checker check;

	check(LOCK_SH == 1 && LOCK_EX == 2 && LOCK_NB == 4 && LOCK_UN == 8,
			"LOCK_* have the values every libc gives them");

	errno = 0;
	check(flock(-1, LOCK_EX) == -1 && errno == EBADF, "a bad descriptor is EBADF");

#if defined(__EMBOX__) && !defined(__EMBOX_USER__)
	const char *path = "/tmp/runtimetest-flock";
	int a = open(path, O_RDWR | O_CREAT, 0644);
	int b = open(path, O_RDWR);
	if (a < 0 || b < 0) {
		printf("flock: cannot open %s (errno %d): no writable /tmp on this board\n", path, errno);
		check(false, "two descriptors on a file in /tmp");
	} else {
		check(flock(a, LOCK_EX | LOCK_NB) == 0, "LOCK_EX on the first descriptor");
		errno = 0;
		check(flock(b, LOCK_EX | LOCK_NB) == -1 && errno == EWOULDBLOCK,
				"... refuses LOCK_EX|LOCK_NB on the second with EWOULDBLOCK");
		errno = 0;
		check(flock(b, LOCK_SH | LOCK_NB) == -1 && errno == EWOULDBLOCK,
				"... and LOCK_SH|LOCK_NB too");
		check(flock(a, LOCK_UN) == 0, "LOCK_UN lets go");
		check(flock(b, LOCK_SH | LOCK_NB) == 0 && flock(a, LOCK_SH | LOCK_NB) == 0,
				"two shared locks share");
		errno = 0;
		check(flock(a, LOCK_EX | LOCK_NB) == -1 && errno == EWOULDBLOCK,
				"an exclusive one waits for the other shared holder");
		errno = 0;
		check(flock(a, 0) == -1 && errno == EINVAL, "an operation that is none is EINVAL");
	}
	if (a >= 0) {
		close(a);
	}
	if (b >= 0) {
		close(b);
	}
	unlink(path);
#else
	printf("flock: the file checks are Embox EL1's here\n");
#endif

	printf("flock: %s (%d failure(s))\n", check.failures ? "FAILED" : "PASS", check.failures);
}

} // namespace sprt
