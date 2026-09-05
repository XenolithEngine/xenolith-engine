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

// Embox EL0 process and thread teardown.
//
// atexit / at_quick_exit / __cxa_atexit registries plus the Itanium
// __cxa_thread_atexit path. exit() drains the process registry in reverse,
// flushes stdio and leaves through exit_group(94) -- which the kernel answers in
// the trap handler itself, unwinding the EL0 thread rather than returning to it.
//
// _exit and _Exit use the same syscall, so a program that exits the hard way
// still tears the task down properly; the difference is only what runs first.
//
// The registries are fixed-capacity arrays rather than a growable list for the
// same reason as on the other freestanding targets: they have to work before the
// allocator does, since a static constructor can register a destructor.

#include "../../include/__impl_libc.h"
#include "../../include/__impl_file.h"

#include <sprt/c/__sprt_errno.h>
#include <sprt/cxx/mutex>
#include <sprt/cxx/atomic>

#include "../../../core/include/__el0_syscall.h"

namespace sprt {

// A registered destructor. `arg == kNoArg` marks a plain atexit()/at_quick_exit()
// callback (void()); otherwise it is a __cxa_atexit callback (void(*)(void*)).
struct __dtor_entry {
	void (*fn)(void *);
	void *arg;
};

static constexpr void *kNoArg = nullptr;

// Fixed-capacity registries. 4096 process-level handlers is far beyond what any
// real program registers; overflow drops the handler (and would be a leak, never
// a crash). A growable list is a later refinement.
static constexpr int kMaxDtors = 4096;

static __dtor_entry s_atexit[kMaxDtors];
static int s_atexitCount = 0;
static __dtor_entry s_quickExit[kMaxDtors];
static int s_quickExitCount = 0;
static sprt::mutex s_atexitMutex;

static sprt::atomic<int> s_exiting{0};

// Per-thread destructor list (Itanium __cxa_thread_atexit + TLS dtors).
static constexpr int kMaxThreadDtors = 1024;
static thread_local __dtor_entry tl_dtors[kMaxThreadDtors];
static thread_local int tl_dtorsCount = 0;

static int __push(__dtor_entry *list, int *count, int cap, void (*fn)(void *), void *arg) {
	if (*count >= cap) {
		return ENOMEM;
	}
	list[*count].fn = fn;
	list[*count].arg = arg;
	++*count;
	return 0;
}

static void __run(__dtor_entry *list, int count) {
	// LIFO, as the C standard requires for atexit handlers.
	for (int i = count - 1; i >= 0; --i) {
		if (list[i].fn) {
			list[i].fn(list[i].arg);
		}
	}
}

// Trampoline turning a void() callback into the void(void*) slot.
static void __call_void(void *fn) { reinterpret_cast<void (*)(void)>(fn)(); }

} // namespace sprt

extern "C" {

int atexit(void (*fn)(void)) __SPRT_NOEXCEPT {
	sprt::unique_lock lock(sprt::s_atexitMutex);
	return sprt::__push(sprt::s_atexit, &sprt::s_atexitCount, sprt::kMaxDtors, sprt::__call_void,
			(void *)fn);
}

int at_quick_exit(void (*fn)(void)) __SPRT_NOEXCEPT {
	sprt::unique_lock lock(sprt::s_atexitMutex);
	return sprt::__push(sprt::s_quickExit, &sprt::s_quickExitCount, sprt::kMaxDtors,
			sprt::__call_void, (void *)fn);
}

// Itanium static-object destructor registration (dso_handle ignored — single
// module, no dlclose yet).
int __cxa_atexit(void (*fn)(void *), void *arg, void *dso) __SPRT_NOEXCEPT {
	(void)dso;
	sprt::unique_lock lock(sprt::s_atexitMutex);
	return sprt::__push(sprt::s_atexit, &sprt::s_atexitCount, sprt::kMaxDtors, fn, arg);
}

// Thread-local object destructor registration (no lock: the list is per-thread).
int __cxa_thread_atexit(void (*fn)(void *), void *arg, void *dso) __SPRT_NOEXCEPT {
	(void)dso;
	return sprt::__push(sprt::tl_dtors, &sprt::tl_dtorsCount, sprt::kMaxThreadDtors, fn, arg);
}

// Legacy name used by some native layers; forwards a void() dtor to the TLS list.
int __tlregdtor(void (*fn)(void)) __SPRT_NOEXCEPT {
	return sprt::__push(sprt::tl_dtors, &sprt::tl_dtorsCount, sprt::kMaxThreadDtors,
			sprt::__call_void, (void *)fn);
}

} // extern "C"

namespace sprt {

void __sprt_libc_thread_exit(bool fromExternalThread) {
	__run(tl_dtors, tl_dtorsCount);
	tl_dtorsCount = 0;
	(void)fromExternalThread;
}

} // namespace sprt

extern "C" {

__SPRT_NORETURN void exit(int result) __SPRT_NOEXCEPT {
	int expected = 0;
	if (!sprt::s_exiting.compare_exchange_strong(expected, 1)) {
		// A concurrent exit() is already tearing the process down.
		__el0_exit_group(result);
		__builtin_unreachable();
	}
	sprt::__sprt_libc_thread_exit(false);
	// Snapshot the count under the lock, then run the handlers WITHOUT holding it:
	// the registered handlers include this translation unit's own static-dtor
	// (which destroys s_atexitMutex), and destroying a held qmutex aborts. Exit is
	// terminal and single-threaded here, so no further registration can race.
	int atexitCount;
	{
		sprt::unique_lock lock(sprt::s_atexitMutex);
		atexitCount = sprt::s_atexitCount;
	}
	sprt::__run(sprt::s_atexit, atexitCount);
	__stdio_exit();
	__el0_exit_group(result);
	__builtin_unreachable();
}

__SPRT_NORETURN void _Exit(int result) __SPRT_NOEXCEPT {
	// No atexit handlers, no stdio flush.
	__el0_exit_group(result);
	__builtin_unreachable();
}

__SPRT_NORETURN void _exit(int result) __SPRT_NOEXCEPT {
	__el0_exit_group(result);
	__builtin_unreachable();
}

__SPRT_NORETURN void quick_exit(int result) __SPRT_NOEXCEPT {
	int expected = 0;
	if (sprt::s_exiting.compare_exchange_strong(expected, 1)) {
		int n;
		{
			sprt::unique_lock lock(sprt::s_atexitMutex);
			n = sprt::s_quickExitCount;
		}
		sprt::__run(sprt::s_quickExit, n);
	}
	__el0_exit_group(result);
	__builtin_unreachable();
}

} // extern "C"
