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

#define __SPRT_BUILD 1

#include <sprt/c/__sprt_assert.h>
#include <sprt/cxx/string>
#include <sprt/runtime/log.h>

#include <stdlib.h>

#if SPRT_APPLE
#include <sprt/cxx/__mutex/unique_lock.h>
#include <sprt/runtime/thread/qmutex.h>

#include <dlfcn.h>
#endif

#if SPRT_APPLE

namespace sprt::libc {

// `quick_exit` / `at_quick_exit` were only added to the macOS libSystem in
// 10.11 (El Capitan). When targeting an older deployment version the symbols
// may be missing at runtime, so we resolve them lazily through `dlsym` and fall
// back to a self-contained implementation when they are unavailable. Resolving
// by name (instead of calling the symbols directly) also avoids emitting a weak
// reference that would crash on launch on systems where the symbol is absent.

using quick_exit_fn = void (*)(int);
using at_quick_exit_fn = int (*)(void (*)(void));

// C requires support for registering at least 32 handlers; that is plenty for
// the rare path where the host libc lacks native support.
static constexpr unsigned QUICK_EXIT_MAX = 32;

struct QuickExitState {
	void (*handlers[QUICK_EXIT_MAX])(void);
	unsigned count;
	qmutex lock;
};

static QuickExitState s_quickExit;

static at_quick_exit_fn nativeAtQuickExit() {
	static auto fn =
			reinterpret_cast<at_quick_exit_fn>(::dlsym(RTLD_DEFAULT, "at_quick_exit"));
	return fn;
}

static quick_exit_fn nativeQuickExit() {
	static auto fn = reinterpret_cast<quick_exit_fn>(::dlsym(RTLD_DEFAULT, "quick_exit"));
	return fn;
}

static int at_quick_exit(void (*cb)(void)) {
	if (auto fn = nativeAtQuickExit()) {
		return fn(cb);
	}

	unique_lock lock(s_quickExit.lock);
	if (s_quickExit.count >= QUICK_EXIT_MAX) {
		return -1;
	}
	s_quickExit.handlers[s_quickExit.count++] = cb;
	return 0;
}

static __SPRT_NORETURN void quick_exit(int ret) {
	if (auto fn = nativeQuickExit()) {
		fn(ret);
		__builtin_unreachable(); // native quick_exit never returns
	}

	// Invoke the registered handlers in reverse order of registration, dropping
	// the lock around each call so a handler may safely re-enter.
	for (;;) {
		void (*cb)(void) = nullptr;
		{
			unique_lock lock(s_quickExit.lock);
			if (s_quickExit.count == 0) {
				break;
			}
			cb = s_quickExit.handlers[--s_quickExit.count];
		}
		cb();
	}

	::_Exit(ret);
}

} // namespace sprt::libc

#endif // SPRT_APPLE

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(atexit_impl)(void (*cb)(void)) { return ::atexit(cb); }

__SPRT_C_FUNC __SPRT_NORETURN void __SPRT_ID(exit_impl)(int ret) { ::exit(ret); }

__SPRT_C_FUNC void __SPRT_ID(_Exit_impl)(int ret) { ::_Exit(ret); }

__SPRT_C_FUNC int __SPRT_ID(at_quick_exit_impl)(void (*cb)(void)) {
#if SPRT_APPLE
	return sprt::libc::at_quick_exit(cb);
#elif SPRT_EMBOX
	(void)cb;
	return 0;
#else
	return ::at_quick_exit(cb);
#endif
}

__SPRT_C_FUNC __SPRT_NORETURN void __SPRT_ID(quick_exit_impl)(int ret) {
#if SPRT_APPLE
	sprt::libc::quick_exit(ret);
#elif SPRT_EMBOX
	::_Exit(ret);
#else
	::quick_exit(ret);
#endif
}

} // namespace sprt
