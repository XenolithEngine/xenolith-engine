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

#include <sprt/c/__sprt_errno.h>

#include "signal.h"
#include "stdlib.h"

typedef void (*sighandler)(int);

// Thread-local signal handler state for freestanding environment.
// Each thread maintains its own set of installed handlers and blocked signals.
struct __signal_thread_state {
	// Signal handlers: index is the signal number, value is the handler function pointer.
	// NULL means default handling (terminate), SIG_DFL equivalent.
	sighandler handlers[__SPRT__NSIG];

	// Currently blocked signals mask (stored as a simple bitmask).
	sigset_t blocked = {0};

	sigset_t pending = {0};

	// Flag indicating if we are currently inside a signal handler call.
	// Used to prevent recursive signal delivery for the same thread.
	int in_handler_flag = 0;

	__signal_thread_state() {
		// Initialize all handlers to default (terminate on unhandled signals).
		for (int i = 0; i < __SPRT__NSIG; ++i) {
			handlers[i] = nullptr; // null means default action: terminate
		}
	}
};

// Thread-local storage for signal state.
thread_local __signal_thread_state tl_signal_state;

// Helper to get the handler for a signal.
static inline sighandler __sprt_get_handler(int sig) { return tl_signal_state.handlers[sig]; }

// Helper to set the handler for a signal. Returns the old handler.
static inline sighandler __sprt_set_handler(int sig, void (*cb)(int)) {
	void (*old)(int) = tl_signal_state.handlers[sig];
	tl_signal_state.handlers[sig] = cb;
	return old;
}

static void __sprt_default_signal_handler(int sig) { __builtin_trap(); }

// Internal function to deliver a signal to the current thread.
static void __sprt_deliver_signal(int sig) {
	if (tl_signal_state.in_handler_flag) {
		return;
	}

	// Check if the signal is blocked.
	if (sigismember(&tl_signal_state.blocked, sig)) {
		sigaddset(&tl_signal_state.pending, sig);
		return;
	}

	// Get the handler for this signal.
	void (*handler)(int) = __sprt_get_handler(sig);

	// Mark that we are inside a signal handler to prevent recursion.
	tl_signal_state.in_handler_flag = 1;

	if (handler == __SPRT_SIG_IGN) {
		return;
	} else if (handler == __SPRT_SIG_DFL) {
		__sprt_default_signal_handler(sig);
	} else if (handler) {
		sigaddset(&tl_signal_state.blocked, sig);

		handler(sig);

		sigdelset(&tl_signal_state.blocked, sig);
	}

	// Clear the pending signal flag.
	tl_signal_state.in_handler_flag = 0;
}

// Implementations for freestanding environment using tl_signal_state.blocked.

extern "C" {

int sigfillset(sigset_t *set) __SPRT_NOEXCEPT {
	set->__bits[0] = 0xffff'ffff;
	return 0;
}

int sigaddset(sigset_t *set, int s) __SPRT_NOEXCEPT {
	if (s < __SPRT__NSIG) {
		set->__bits[0] |= (1 << s);
	}
	*__sprt___errno_location() = EINVAL;
	return -1;
}

int sigdelset(sigset_t *set, int s) __SPRT_NOEXCEPT {
	if (s < __SPRT__NSIG) {
		set->__bits[0] &= ~(1 << s);
		return 0;
	}
	*__sprt___errno_location() = EINVAL;
	return -1;
}

int sigismember(const sigset_t *set, int s) __SPRT_NOEXCEPT {
	if (s < __SPRT__NSIG) {
		return !!(set->__bits[0] & (1 << s));
	}
	return 0;
}

int sigprocmask(int mode, const __sprt_sigset_t *__SPRT_RESTRICT set,
		__sprt_sigset_t *__SPRT_RESTRICT oset) __SPRT_NOEXCEPT {
	// Validate mode parameter.
	if (mode != __SPRT_SIG_BLOCK && mode != __SPRT_SIG_UNBLOCK && mode != __SPRT_SIG_SETMASK) {
		*__sprt___errno_location() = EINVAL;
		return -1;
	}

	// Save the current mask if output buffer provided.
	if (oset != nullptr) {
		*oset = tl_signal_state.blocked;
	}

	switch (mode) {
	case __SPRT_SIG_BLOCK:
		// Union: new_mask = old_mask | set
		__sprt_sigorset(&tl_signal_state.blocked, &tl_signal_state.blocked, set);
		break;
	case __SPRT_SIG_UNBLOCK:
		// Difference: new_mask = old_mask & ~set
		for (size_t i = 0; i < sizeof(set->__bits) / sizeof(set->__bits[0]); ++i) {
			tl_signal_state.blocked.__bits[i] &= ~set->__bits[i];
		}
		break;
	case __SPRT_SIG_SETMASK:
		// Replace: new_mask = set
		tl_signal_state.blocked = *set;
		break;
	default: *__sprt___errno_location() = EINVAL; return -1;
	}

	return 0;
}

int sigsuspend(const __sprt_sigset_t *set) __SPRT_NOEXCEPT {
	*__sprt___errno_location() = EINTR;
	return -1;
}

int sigpending(__sprt_sigset_t *set) __SPRT_NOEXCEPT {
	if (!set) {
		*__sprt___errno_location() = EFAULT;
	}

	*set = tl_signal_state.pending;
	return 0;
}

sighandler signal(int sig, void (*cb)(int)) __SPRT_NOEXCEPT {
	// Validate signal number.
	if (sig < 0 || sig >= __SPRT__NSIG) {
		return nullptr;
	}

	if (cb == __SPRT_SIG_GET) {
		return __sprt_get_handler(sig);
	}

	if (cb == __SPRT_SIG_ERR || cb == __SPRT_SIG_ACK) {
		return __SPRT_SIG_ERR;
	}

	// SIGABRT can not be SIG_IGN
	if (sig == __SPRT_SIGABRT && cb == __SPRT_SIG_IGN) {
		return __SPRT_SIG_ERR;
	}

	return __sprt_set_handler(sig, cb);
}

/**
 * raise() - Send a signal to the calling thread.
 *
 * This is a userspace emulation of the POSIX raise() function for freestanding systems.
 * It delivers the signal directly within the current thread using the stored handler.
 *
 * Parameters:
 *   sig - The signal number to send.
 *
 * Returns:
 *   0 on success, non-zero error code on failure.
 */
int raise(int sig) __SPRT_NOEXCEPT {
	// Validate signal number.
	if (sig < 0 || sig >= __SPRT__NSIG) {
		return EINVAL;
	}

	// Deliver the signal using our userspace emulation.
	__sprt_deliver_signal(sig);

	return 0;
}

_Noreturn void abort() __SPRT_NOEXCEPT {
	// Mimic Linux behavior (see https://man7.org/linux/man-pages/man3/abort.3.html)

	// unblock SIGABRT
	sigset_t set;
	sigaddset(&set, __SPRT_SIGABRT);

	sigprocmask(__SPRT_SIG_UNBLOCK, &set, nullptr);

	// Deliver signal to current handler
	__sprt_deliver_signal(__SPRT_SIGABRT);

	// Reset handler to default
	__sprt_set_handler(__SPRT_SIGABRT, __SPRT_SIG_DFL);

	// Re-deliver signal
	__sprt_deliver_signal(__SPRT_SIGABRT);

	__builtin_unreachable();
}
}

__SPRT_C_FUNC int __SPRT_ID(sigisemptyset)(const __SPRT_ID(sigset_t) * set) {
	for (auto &it : set->__bits) {
		if (it != 0) {
			return 0;
		}
	}
	return 1;
}

__SPRT_C_FUNC int __SPRT_ID(sigorset)(__SPRT_ID(sigset_t) * set, const __SPRT_ID(sigset_t) * a,
		const __SPRT_ID(sigset_t) * b) {
	auto s = sizeof(set->__bits) / sizeof(set->__bits[0]);
	for (size_t i = 0; i < s; ++i) { set->__bits[i] = a->__bits[i] | b->__bits[i]; }
	return 0;
}
__SPRT_C_FUNC int __SPRT_ID(sigandset)(__SPRT_ID(sigset_t) * set, const __SPRT_ID(sigset_t) * a,
		const __SPRT_ID(sigset_t) * b) {
	auto s = sizeof(set->__bits) / sizeof(set->__bits[0]);
	for (size_t i = 0; i < s; ++i) { set->__bits[i] = a->__bits[i] & b->__bits[i]; }
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(sigemptyset)(__SPRT_ID(sigset_t) * set) {
	for (auto &it : set->__bits) { it = 0; }
	return 0;
}

__SPRT_C_FUNC __SPRT_NORETURN void __SPRT_ID(abort_impl)(void) { ::abort(); }
