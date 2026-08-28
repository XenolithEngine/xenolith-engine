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

// setjmp/longjmp, and the guarantee sprt adds on top of C: the destructors of
// every frame between the longjmp and the setjmp run, in reverse order, on the
// way out (runtime/include/sprt/c/__sprt_setjmp.h).
//
// A plain libc longjmp restores registers and jumps; the frames in between are
// simply forgotten, which for C++ (or Rust, or Go) means leaked mutexes, leaked
// allocations and unclosed descriptors. sprt implements the jump as a forced
// unwind instead: setjmp records the CFA of ITS OWN caller alongside the native
// buffer, longjmp drives _Unwind_ForcedUnwind, and the stop function performs
// the native jump only once the unwinder reaches that CFA — so every frame
// below it has already had its cleanups run by the personality routine.
//
// That machinery is also what pthread_exit() is built on (thread_t::exit()
// longjmps to the buffer runthread() saved), so the last check here is the one
// that matters on a fresh port: a thread that exits early still destroys its
// locals.
//
// Platforms where the unwinder is missing are expected to degrade to the plain
// jump, not to break: the value checks below must pass everywhere, and only the
// destructor checks depend on a working unwinder.

#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <sprt/runtime/log.h>

namespace sprt {

// Every check here performs a real longjmp, which traps on wasm (see the SKIP in
// performSetjmpTest below), so none of the machinery is built for that target.
#if !SPRT_WASM

namespace {

// Running destructors during an unwind needs cleanup landing pads, and the
// compiler only emits those under -fexceptions. The OS presets differ: Embox and
// NuttX build -fexceptions (their libc++abi is the image's C++ ABI), the desktop
// ones build -fno-exceptions. Where they are absent the destructor half of the
// contract cannot hold by construction - the header says as much - so it is
// reported as skipped rather than failed. The value half is checked everywhere.
#if defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#define SPRT_TEST_HAS_CLEANUPS 1
#else
#define SPRT_TEST_HAS_CLEANUPS 0
#endif

int s_failures = 0;
int s_skipped = 0;

void check(bool cond, const char *msg) {
	printf("  %s: %s\n", cond ? "PASS" : "FAIL", msg);
	if (!cond) {
		++s_failures;
	}
}

// Destructor order is recorded into a global: the stack that held the objects is
// gone by the time anything can be inspected.
char s_trace[64];
unsigned s_traceLen = 0;

void trace(char c) {
	if (s_traceLen + 1 < sizeof(s_trace)) {
		s_trace[s_traceLen++] = c;
	}
}

const char *traceStr() {
	s_trace[s_traceLen] = '\0';
	return s_trace;
}

// Compares the recorded destructor order against what the unwind should have
// produced, and prints the actual trace when it does not match - "expected cba,
// got cb" localises the break far better than a bare FAIL.
void checkTrace(const char *expected, const char *msg) {
#if SPRT_TEST_HAS_CLEANUPS
	bool ok = strcmp(traceStr(), expected) == 0;
	check(ok, msg);
	if (!ok) {
		printf("    destructor trace: \"%s\" (expected \"%s\")\n", traceStr(), expected);
	}
#else
	(void)expected;
	// The trace is still printed: it must be EMPTY here. Anything else would mean
	// destructors ran without landing pads, which is not a thing.
	printf("  SKIP: %s (built -fno-exceptions, trace \"%s\")\n", msg, traceStr());
	++s_skipped;
#endif
}

struct Marker {
	char id;

	explicit Marker(char i) : id(i) { }
	~Marker() { trace(id); }
};

jmp_buf s_buf;
sigjmp_buf s_sigbuf;

// noinline keeps the three frames real: inlined into the caller they would share
// one CFA and the unwind would have nothing to walk.
__attribute__((noinline)) void level3() {
	Marker m('c');
	longjmp(s_buf, 7);
}

__attribute__((noinline)) void level2() {
	Marker m('b');
	level3();
}

__attribute__((noinline)) void level1() {
	Marker m('a');
	level2();
}

__attribute__((noinline)) void sigLevel2() {
	Marker m('y');
	siglongjmp(s_sigbuf, 3);
}

__attribute__((noinline)) void sigLevel1() {
	Marker m('x');
	sigLevel2();
}

// --- the basic contract ---------------------------------------------------

void runValueChecks() {
	// The direct call returns 0.
	volatile int direct = setjmp(s_buf);
	if (direct == 0) {
		check(true, "setjmp returns 0 on the direct call");
		longjmp(s_buf, 42);
		check(false, "longjmp returned to its caller");
	} else {
		check(direct == 42, "longjmp(buf, 42) makes setjmp return 42");
	}

	// ISO C 7.13.2.1: a zero value is reported as 1.
	volatile int zero = setjmp(s_buf);
	if (zero == 0) {
		longjmp(s_buf, 0);
		check(false, "longjmp(buf, 0) returned to its caller");
	} else {
		check(zero == 1, "longjmp(buf, 0) makes setjmp return 1");
	}

	// A volatile local of the setjmp frame survives the jump.
	volatile int keepsValue = 0x5A5A;
	volatile int again = setjmp(s_buf);
	if (again == 0) {
		longjmp(s_buf, 5);
	}
	check(keepsValue == 0x5A5A, "volatile local of the setjmp frame survives");
}

// --- the sprt guarantee ---------------------------------------------------

void runUnwindChecks() {
	s_traceLen = 0;

	volatile int r = setjmp(s_buf);
	if (r == 0) {
		level1();
		check(false, "longjmp did not return control to setjmp");
		return;
	}

	check(r == 7, "longjmp across three frames delivers its value");
	// Reverse order: the innermost frame is unwound first. The setjmp frame
	// itself is jumped INTO, so its own locals are untouched - that is why there
	// is no fourth letter here.
	checkTrace("cba", "destructors of the frames between longjmp and setjmp run, innermost first");
}

void runSigUnwindChecks() {
	s_traceLen = 0;

	volatile int r = sigsetjmp(s_sigbuf, 1);
	if (r == 0) {
		sigLevel1();
		check(false, "siglongjmp did not return control to sigsetjmp");
		return;
	}

	check(r == 3, "siglongjmp delivers its value");
	checkTrace("yx", "siglongjmp runs the intervening destructors");
}

// --- the same thing, off the main thread ----------------------------------
//
// A spawned thread runs on a stack the OS handed out, and its outermost frames
// belong to the platform's thread trampoline rather than to the program. The
// unwinder has to cope with that, so the plain jump is checked here before the
// pthread_exit one below, which additionally depends on the buffer the runtime's
// own thread entry point saved.

jmp_buf s_threadBuf;

__attribute__((noinline)) void threadLevel2() {
	Marker m('q');
	longjmp(s_threadBuf, 9);
}

__attribute__((noinline)) void threadLevel1() {
	Marker m('p');
	threadLevel2();
}

int s_threadJumpValue = 0;

void *threadJumpBody(void *) {
	volatile int r = setjmp(s_threadBuf);
	if (r == 0) {
		threadLevel1();
		return nullptr;
	}
	s_threadJumpValue = r;
	return nullptr;
}

void runThreadJumpChecks() {
	s_traceLen = 0;
	s_threadJumpValue = 0;

	pthread_t thread;
	if (pthread_create(&thread, nullptr, &threadJumpBody, nullptr) != 0) {
		check(false, "pthread_create for the off-main-thread longjmp check");
		return;
	}
	pthread_join(thread, nullptr);

	check(s_threadJumpValue == 9, "longjmp on a spawned thread delivers its value");
	checkTrace("qp", "longjmp on a spawned thread runs the intervening destructors");
}

// --- what the guarantee is actually for -----------------------------------
//
// pthread_exit() from inside a nested call is a longjmp to the buffer the
// thread entry point saved. Without the unwind the two Markers below would
// never be destroyed - the failure mode that leaks a held mutex on every early
// thread exit.

__attribute__((noinline)) void threadInner() {
	Marker m('n');
	pthread_exit(nullptr);
}

void *threadBody(void *) {
	Marker m('o');
	threadInner();
	trace('!'); // must not be reached
	return nullptr;
}

void runThreadExitChecks() {
	s_traceLen = 0;

	pthread_t thread;
	if (pthread_create(&thread, nullptr, &threadBody, nullptr) != 0) {
		check(false, "pthread_create for the pthread_exit check");
		return;
	}
	pthread_join(thread, nullptr);

	checkTrace("no", "pthread_exit destroys the locals of the frames it leaves");
}

} // namespace

#endif // !SPRT_WASM

void performSetjmpTest() {
#if SPRT_WASM
	// wasm has no stack the module can save and restore: a jump out of a live frame
	// needs the clang SjLj lowering built on wasm exception handling, which the
	// runtime is not yet compiled with. Until then setjmp is the no-op that returns
	// 0 and longjmp traps (runtime_core_setjmp.cpp), so every check would kill the
	// module rather than fail.
	printf("performSetjmpTest: SKIP (wasm setjmp/longjmp needs the -fwasm-exceptions "
		   "SjLj lowering)\n");
#else
	s_failures = 0;
	s_skipped = 0;

	runValueChecks();
	runUnwindChecks();
	runSigUnwindChecks();
	runThreadJumpChecks();
	runThreadExitChecks();

	printf("performSetjmpTest: %s (%d failures, %d skipped)\n",
			s_failures == 0 ? "ALL PASS" : "FAILED", s_failures, s_skipped);
#endif
}

} // namespace sprt
