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

// wasm64 (memory64) checks. The ABI test pins the LP64 data model the headers pick for
// wasm64. The high-memory test pushes the heap past the 4 GiB line a wasm32 module can
// never reach, then makes every kind of boundary crossing happen from up there: plain
// reads and writes, host imports handed a pointer above 4 GiB (they arrive in JS as
// BigInts), and a thread whose stack, TLS block and lock all live above 4 GiB. Anything
// that still squeezes a pointer through 32 bits shows up here as a FAIL or a trap.
//
// Needs the module's memory maximum above 4 GiB (the wasm64 default is 16 GiB). On every
// other target both tests print a SKIP line.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <sys/utsname.h>
#include "../tests.h"

namespace sprt {

#if __SPRT_ARCH_ID == __SPRT_ARCH_ID_WASM64

namespace {

static constexpr uintptr_t FourGiB = uintptr_t(1) << 32;
static constexpr size_t ChunkSize = size_t(256) << 20;
static constexpr size_t MaxChunks = 24; // 6 GiB: the heap starts low, so this is plenty

static int s_failures = 0;

static void check(bool ok, const char *what) {
	printf("%s  %s\n", ok ? "PASS" : sprt::test::failed("FAIL"), what);
	if (!ok) {
		++s_failures;
	}
}

static bool isHigh(const void *p) { return uintptr_t(p) >= FourGiB; }

static void fillPattern(uint8_t *p, size_t n, uint8_t seed) {
	for (size_t i = 0; i < n; ++i) { p[i] = uint8_t(i * 31 + seed); }
}

static bool checkPattern(const uint8_t *p, size_t n, uint8_t seed) {
	for (size_t i = 0; i < n; ++i) {
		if (p[i] != uint8_t(i * 31 + seed)) {
			return false;
		}
	}
	return true;
}

struct HighThreadState {
	pthread_mutex_t *mutex = nullptr;
	pthread_cond_t *cond = nullptr;
	int counter = 0;
	bool ready = false;
	uintptr_t stackAddr = 0;
	uintptr_t tlsAddr = 0;
};

static thread_local int tl_highMarker = 0;

static void *highThreadMain(void *arg) {
	auto state = static_cast<HighThreadState *>(arg);
	int local = 0;
	tl_highMarker = 42;
	state->stackAddr = uintptr_t(&local);
	state->tlsAddr = uintptr_t(&tl_highMarker);

	for (int i = 0; i < 1'000; ++i) {
		pthread_mutex_lock(state->mutex);
		++state->counter;
		pthread_mutex_unlock(state->mutex);
	}

	pthread_mutex_lock(state->mutex);
	state->ready = true;
	pthread_cond_signal(state->cond);
	pthread_mutex_unlock(state->mutex);
	return reinterpret_cast<void *>(uintptr_t(tl_highMarker));
}

} // namespace

void performWasm64AbiTest() {
	printf("--- wasm64 ABI test ---\n");

	// The data model is decided at compile time; a wrong header branch fails the build.
	static_assert(sizeof(void *) == 8);
	static_assert(sizeof(long) == 8);
	static_assert(sizeof(size_t) == 8);
	static_assert(sizeof(ssize_t) == 8);
	static_assert(sizeof(intptr_t) == 8);
	static_assert(sizeof(ptrdiff_t) == 8);
	static_assert(sizeof(off_t) == 8);
	static_assert(sizeof(time_t) == 8);
	static_assert(sizeof(int) == 4);
	static_assert(sizeof(wchar_t) == 4);
	static_assert(alignof(max_align_t) == 16);
	static_assert(sizeof(long double) == 16);

	printf("sizeof: void*=%zu long=%zu size_t=%zu off_t=%zu time_t=%zu long double=%zu\n",
			sizeof(void *), sizeof(long), sizeof(size_t), sizeof(off_t), sizeof(time_t),
			sizeof(long double));

	struct utsname u;
	check(uname(&u) == 0 && strcmp(u.machine, "wasm64") == 0, "uname().machine == wasm64");
	printf("--- wasm64 ABI test done ---\n");
}

void performWasm64HighMemTest() {
	printf("--- wasm64 high-memory test ---\n");
	s_failures = 0;

	// 1. Push the heap past 4 GiB. Huge allocations come straight from sbrk, so they climb
	// monotonically; stop at the first chunk that lies wholly above the line.
	uint8_t *chunks[MaxChunks] = {};
	size_t nchunks = 0;
	uint8_t *high = nullptr;
	uint8_t *low = nullptr;
	while (nchunks < MaxChunks) {
		auto p = static_cast<uint8_t *>(malloc(ChunkSize));
		if (!p) {
			break;
		}
		chunks[nchunks++] = p;
		if (!low) {
			low = p;
		}
		if (isHigh(p)) {
			high = p;
			break;
		}
	}
	printf("allocated %zu chunks of %zu MiB, last at 0x%" PRIxPTR "\n", nchunks, ChunkSize >> 20,
			nchunks ? uintptr_t(chunks[nchunks - 1]) : uintptr_t(0));
	check(high != nullptr, "malloc returns an address above 4 GiB");
	if (!high) {
		for (size_t i = 0; i < nchunks; ++i) { free(chunks[i]); }
		printf("--- wasm64 high-memory test done (failures=%d) ---\n", s_failures);
		return;
	}

	// 2. Plain memory above 4 GiB: fill/verify at both ends of the chunk, copy between the
	// low and the high chunk in both directions, and an overlapping move up there.
	static constexpr size_t Probe = 1 << 20;
	fillPattern(high, Probe, 7);
	fillPattern(high + ChunkSize - Probe, Probe, 11);
	check(checkPattern(high, Probe, 7) && checkPattern(high + ChunkSize - Probe, Probe, 11),
			"write/read pattern at both ends of a chunk above 4 GiB");

	fillPattern(low, Probe, 3);
	memcpy(high + Probe, low, Probe);
	check(checkPattern(high + Probe, Probe, 3), "memcpy low -> high");
	fillPattern(high + 2 * Probe, Probe, 5);
	memcpy(low, high + 2 * Probe, Probe);
	check(checkPattern(low, Probe, 5), "memcpy high -> low");

	fillPattern(high + 4 * Probe, Probe, 9);
	memmove(high + 4 * Probe + 4096, high + 4 * Probe, Probe);
	check(checkPattern(high + 4 * Probe + 4096, Probe, 9), "overlapping memmove above 4 GiB");

	// 3. Host imports handed pointers above 4 GiB.
	static constexpr char Line[] = "wasm64-highmem: fd_write from a buffer above 4 GiB\n";
	auto text = reinterpret_cast<char *>(high + 8 * Probe);
	memcpy(text, Line, sizeof(Line) - 1);
	check(write(1, text, sizeof(Line) - 1) == ssize_t(sizeof(Line) - 1),
			"write() from a buffer above 4 GiB");

	auto rnd = high + 9 * Probe;
	memset(rnd, 0, 4096);
	bool nonzero = false;
	if (getrandom(rnd, 4096, 0) == 4096) {
		for (size_t i = 0; i < 4096; ++i) {
			if (rnd[i]) {
				nonzero = true;
				break;
			}
		}
	}
	check(nonzero, "getrandom() into a buffer above 4 GiB");

	// The Node runner bundles its launch directory; tests/runtime/run-wasm.sh drops this
	// file there. Reading a bundled file copies it through the host import.
	static constexpr char BundleText[] = "sprt wasm64 bundle probe\n";
	int fd = open("/wasm64-bundle.txt", O_RDONLY);
	if (fd >= 0) {
		auto dst = reinterpret_cast<char *>(high + 10 * Probe);
		auto n = read(fd, dst, 4096);
		check(n == ssize_t(sizeof(BundleText) - 1)
						&& memcmp(dst, BundleText, sizeof(BundleText) - 1) == 0,
				"read() of a bundled file into a buffer above 4 GiB");
		close(fd);
	} else {
		printf("SKIP  bundled file read (no /wasm64-bundle.txt in the launch directory)\n");
	}

	auto mapped = static_cast<uint8_t *>(
			mmap(nullptr, 64 << 20, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
	if (mapped != MAP_FAILED) {
		fillPattern(mapped + (32 << 20), Probe, 13);
		check(isHigh(mapped) && checkPattern(mapped + (32 << 20), Probe, 13),
				"anonymous mmap lands above 4 GiB and is writable");
		munmap(mapped, 64 << 20);
	} else {
		check(false, "anonymous mmap of 64 MiB after the heap passed 4 GiB");
	}

	// 4. A thread running on a stack above 4 GiB, with its lock and condition variable placed
	// above 4 GiB as well, so the futex cells are too. The runtime mallocs thread stacks and
	// has no pthread_attr_setstack on wasm; a 512 MiB stack is larger than any hole the
	// chunks above left, so it can only come from the top of the heap. The TLS block is
	// small and usually reuses low memory: its address is reported, not asserted - the
	// BigInt hand-over to __wasm_init_tls is exercised either way.
	auto mutex = reinterpret_cast<pthread_mutex_t *>(high + 11 * Probe);
	auto cond = reinterpret_cast<pthread_cond_t *>(high + 11 * Probe + 256);
	pthread_mutex_init(mutex, nullptr);
	pthread_cond_init(cond, nullptr);

	HighThreadState state;
	state.mutex = mutex;
	state.cond = cond;

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, size_t(512) << 20);

	pthread_t thread;
	if (pthread_create(&thread, &attr, highThreadMain, &state) == 0) {
		for (int i = 0; i < 1'000; ++i) {
			pthread_mutex_lock(mutex);
			++state.counter;
			pthread_mutex_unlock(mutex);
		}
		pthread_mutex_lock(mutex);
		while (!state.ready) { pthread_cond_wait(cond, mutex); }
		pthread_mutex_unlock(mutex);

		void *ret = nullptr;
		pthread_join(thread, &ret);
		printf("thread stack at 0x%" PRIxPTR ", TLS at 0x%" PRIxPTR "\n", state.stackAddr,
				state.tlsAddr);
		check(state.counter == 2'000, "mutex above 4 GiB serializes two threads");
		check(uintptr_t(ret) == 42, "thread returns its TLS value through pthread_join");
		check(state.stackAddr >= FourGiB, "thread stack above 4 GiB");
	} else {
		check(false, "pthread_create after the heap passed 4 GiB");
	}

	pthread_attr_destroy(&attr);
	pthread_cond_destroy(cond);
	pthread_mutex_destroy(mutex);

	for (size_t i = 0; i < nchunks; ++i) { free(chunks[i]); }
	printf("--- wasm64 high-memory test done (failures=%d) ---\n", s_failures);
}

#else

void performWasm64AbiTest() { printf("SKIP  wasm64 ABI test (not a wasm64 build)\n"); }

void performWasm64HighMemTest() { printf("SKIP  wasm64 high-memory test (not a wasm64 build)\n"); }

#endif

} // namespace sprt
