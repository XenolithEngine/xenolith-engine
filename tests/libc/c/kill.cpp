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

// POSIX kill(). Only the cases that do not end the process: the liveness probe
// (signal 0), delivery to self through an installed handler, and the two error
// paths. Signal numbers and errno values differ between glibc and the
// freestanding libc_impl, so every result is printed as a symbolic outcome or a
// boolean and the two runs diff identically.
//
// Every call result and the errno it left behind are captured into locals before
// the printf: the order in which printf's arguments are evaluated is unspecified,
// so reading errno (or a handler's counter) in an argument next to the call that
// sets it is a race the two toolchains resolve differently.

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

namespace sprt::test {

namespace {

volatile sig_atomic_t g_killHits = 0;

void onKilled(int) { g_killHits += 1; }

// The outcome is what is contractual; the exact -1 is not worth printing.
const char *fails(int r) { return r == -1 ? "fail" : "ok"; }

const char *yesNo(bool b) { return b ? "yes" : "no"; }

} // namespace

void performKillTest() {
	// Signal 0 performs no delivery - it only reports whether the target exists
	// and is signalable, which is the standard liveness probe.
	int r = kill(getpid(), 0);
	printf("kill(self,0)=%s\n", fails(r));

	// pid 0 means "the caller's process group", which always includes the caller.
	r = kill(0, 0);
	printf("kill(0,0)=%s\n", fails(r));

	// A pid far above any live process. Both libcs answer ESRCH - glibc because
	// it is past pid_max, the Windows backend because OpenProcess cannot find it.
	errno = 0;
	r = kill(0x3fff'fffe, 0);
	bool esrch = errno == ESRCH;
	printf("kill(nobody,0)=%s ESRCH=%s\n", fails(r), yesNo(esrch));

	// Delivery to self. POSIX requires an unblocked signal to reach the handler
	// before kill() returns, so the counter is already up by the time it does.
	g_killHits = 0;
	signal(SIGINT, &onKilled);
	r = kill(getpid(), SIGINT);
	int hits = (int)g_killHits;
	signal(SIGINT, SIG_DFL);
	printf("kill(self,SIGINT)=%s hits=%d\n", fails(r), hits);

	// An out-of-range signal number is EINVAL, and nothing is delivered.
	g_killHits = 0;
	errno = 0;
	r = kill(getpid(), -1);
	bool einval = errno == EINVAL;
	hits = (int)g_killHits;
	printf("kill(self,bad-signo)=%s EINVAL=%s hits=%d\n", fails(r), yesNo(einval), hits);
}

} // namespace sprt::test
