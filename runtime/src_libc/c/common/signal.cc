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

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_signal.h>
#include <sprt/cxx/type_traits>

#if __STDC_HOSTED__ == 0

typedef void (*sighandler)(int);

typedef __SPRT_ID(sigset_t) sigset_t;

// Thread-local signal handler state for freestanding environment.
// Each thread maintains its own set of installed handlers and blocked signals.
struct __signal_thread_state {
	// Signal handlers: index is the signal number, value is the handler function pointer.
	// NULL means default handling (terminate), SIG_DFL equivalent.
	sighandler handlers[__SPRT__NSIG];

	// Currently blocked signals mask (stored as a simple bitmask).
	sigset_t blocked = {0};

	// Flag indicating if we are currently inside a signal handler call.
	// Used to prevent recursive signal delivery for the same thread.
	int in_handler_flag = 0;

	// The last signal that was raised and is being delivered.
	int pending_signal = 0;

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
	if (sig < 0 || sig >= __SPRT__NSIG) {
		return nullptr;
	}
	void (*old)(int) = tl_signal_state.handlers[sig];
	tl_signal_state.handlers[sig] = cb;
	return old;
}

// Internal function to deliver a signal to the current thread.
static void __sprt_deliver_signal(int sig) {
	// Prevent recursive delivery of the same signal within the same handler call.
	if (tl_signal_state.in_handler_flag && tl_signal_state.pending_signal == sig) {
		return;
	}

	// Check if the signal is blocked.
	if ((tl_signal_state.blocked.__bits[0] & (1 << sig)) != 0) {
		return;
	}

	// Get the handler for this signal.
	void (*handler)(int) = __sprt_get_handler(sig);

	// Mark that we are inside a signal handler to prevent recursion.
	tl_signal_state.in_handler_flag = 1;
	tl_signal_state.pending_signal = sig;

	if (handler != nullptr && handler != __SPRT_SIG_DFL) {
		// Call the user-installed handler.
		handler(sig);
	} else if (handler == __SPRT_SIG_IGN) {
		// Signal is ignored, do nothing.
	} else {
		// Default action: terminate the program.
		// In a freestanding environment, we call abort or exit.
		__builtin_trap();
	}

	// Clear the pending signal flag.
	tl_signal_state.in_handler_flag = 0;
	tl_signal_state.pending_signal = 0;
}


// Implementations for freestanding environment using tl_signal_state.blocked.

/// Helper to validate signal number and clamp it.
static inline int __sprt_clamp_signal(int s) {
	if (s < 0) {
		return 0;
	}
	if (s >= __SPRT__NSIG) {
		return __SPRT__NSIG - 1;
	}
	return s;
}

/**
 * sigprocmask - Modify the signal mask for the current thread.
 *
 * In a freestanding environment, this manages the blocked signals
 * using the thread-local __signal_thread_state.blocked field.
 *
 * Modes:
 *   SIG_BLOCK   - Add signals in 'set' to the current mask (union).
 *   SIG_UNBLOCK - Remove signals in 'set' from the current mask.
 *   SIG_SETMASK - Replace the entire mask with 'set'.
 */
static int sigprocmask(int mode, const __sprt_sigset_t *__SPRT_RESTRICT set,
		__sprt_sigset_t *__SPRT_RESTRICT oset) {
	// Validate mode parameter.
	if (mode != __SPRT_SIG_BLOCK && mode != __SPRT_SIG_UNBLOCK && mode != __SPRT_SIG_SETMASK) {
		*__sprt___errno_location() = EINVAL;
		return -1;
	}

	// Save the current mask if output buffer provided.
	if (oset != nullptr) {
		oset->__bits[0] = tl_signal_state.blocked.__bits[0];
	}

	switch (mode) {
	case __SPRT_SIG_BLOCK:
		// Union: new_mask = old_mask | set
		tl_signal_state.blocked.__bits[0] |= set->__bits[0];
		break;
	case __SPRT_SIG_UNBLOCK:
		// Difference: new_mask = old_mask & ~set
		tl_signal_state.blocked.__bits[0] &= ~set->__bits[0];
		break;
	case __SPRT_SIG_SETMASK:
		// Replace: new_mask = set
		tl_signal_state.blocked.__bits[0] = set->__bits[0];
		break;
	default: *__sprt___errno_location() = EINVAL; return -1;
	}

	return 0;
}

/**
 * sigsuspend - Suspend execution until a signal is delivered.
 *
 * This temporarily replaces the signal mask with 'set', waits for a signal,
 * then restores the original mask. In freestanding environments without
 * real threading support, this simply updates the blocked mask and returns.
 */
static int sigsuspend(const __sprt_sigset_t *set) {
	// Save the current blocked mask.
	__sprt_sigset_t saved_mask = tl_signal_state.blocked;

	// Temporarily replace the signal mask with the provided set.
	tl_signal_state.blocked.__bits[0] = set->__bits[0];

	// In a freestanding environment without real OS-level suspend,
	// we cannot actually wait for signals. Simply restore the original
	// mask and return -1 with EINTR to indicate interruption.
	tl_signal_state.blocked = saved_mask;

	*__sprt___errno_location() = EINTR;
	return -1;
}

/**
 * sigpending - Get signals pending delivery for the current thread.
 *
 * In a freestanding environment, we track pending signals that are
 * both raised and currently blocked. For now, this returns an empty
 * set since we don't have actual signal queuing in userspace emulation.
 */
static int sigpending(__sprt_sigset_t *set) {
	// In a freestanding environment without real signal delivery,
	// there are no pending signals to report. Return an empty set.
	set->__bits[0] = 0;

#if defined(__SPRT__NSIG_BITS) && __SPRT__NSIG_BITS > 1
	for (size_t i = 1; i < sizeof(set->__bits) / sizeof(set->__bits[0]); ++i) {
		set->__bits[i] = 0;
	}
#endif

	return 0;
}


/**
 * signal() - Install a signal handler for the specified signal.
 *
 * This is a userspace emulation of the POSIX signal() function for freestanding systems.
 * It stores signal handlers in thread-local storage, so each thread has its own set of handlers.
 *
 * Parameters:
 *   sig  - The signal number to handle (e.g., SIGINT, SIGTERM).
 *   func - A pointer to the handler function, or SIG_DFL/SIG_IGN.
 *
 * Returns:
 *   The previous handler for the signal, or a system-defined value on error.
 */
static sighandler signal(int sig, void (*cb)(int)) {
	// Validate signal number.
	if (sig < 0 || sig >= __SPRT__NSIG) {
		return nullptr;
	}

	if (cb == __SPRT_SIG_GET) {
		return __sprt_get_handler(sig);
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
static int raise(int sig) {
	// Validate signal number.
	if (sig < 0 || sig >= __SPRT__NSIG) {
		return EINVAL;
	}

	// Deliver the signal using our userspace emulation.
	__sprt_deliver_signal(sig);

	return 0;
}

#else // __STDC_HOSTED__ != 0

#include <signal.h>

static_assert(SIGHUP == __SPRT_SIGHUP);
static_assert(SIGINT == __SPRT_SIGINT);
static_assert(SIGQUIT == __SPRT_SIGQUIT);
static_assert(SIGILL == __SPRT_SIGILL);
static_assert(SIGTRAP == __SPRT_SIGTRAP);
static_assert(SIGABRT == __SPRT_SIGABRT);
static_assert(SIGIOT == __SPRT_SIGIOT);
#ifdef SIGEMT
static_assert(SIGEMT == __SPRT_SIGEMT);
#endif
static_assert(SIGFPE == __SPRT_SIGFPE);
static_assert(SIGKILL == __SPRT_SIGKILL);
static_assert(SIGBUS == __SPRT_SIGBUS);
static_assert(SIGSEGV == __SPRT_SIGSEGV);
static_assert(SIGSYS == __SPRT_SIGSYS);
static_assert(SIGPIPE == __SPRT_SIGPIPE);
static_assert(SIGALRM == __SPRT_SIGALRM);
static_assert(SIGTERM == __SPRT_SIGTERM);
static_assert(SIGURG == __SPRT_SIGURG);
static_assert(SIGSTOP == __SPRT_SIGSTOP);
static_assert(SIGTSTP == __SPRT_SIGTSTP);
static_assert(SIGCONT == __SPRT_SIGCONT);
static_assert(SIGCHLD == __SPRT_SIGCHLD);
static_assert(SIGTTIN == __SPRT_SIGTTIN);
static_assert(SIGTTOU == __SPRT_SIGTTOU);
static_assert(SIGIO == __SPRT_SIGIO);
static_assert(SIGXCPU == __SPRT_SIGXCPU);
static_assert(SIGXFSZ == __SPRT_SIGXFSZ);
static_assert(SIGVTALRM == __SPRT_SIGVTALRM);
static_assert(SIGPROF == __SPRT_SIGPROF);
static_assert(SIGWINCH == __SPRT_SIGWINCH);
#ifdef SIGINFO
static_assert(SIGINFO == __SPRT_SIGINFO);
#endif
static_assert(SIGUSR1 == __SPRT_SIGUSR1);
static_assert(SIGUSR2 == __SPRT_SIGUSR2);

static_assert(SIG_DFL == __SPRT_SIG_DFL);
//static_assert(SIG_IGN == __SPRT_SIG_IGN);
//static_assert(SIG_HOLD == __SPRT_SIG_HOLD);
//static_assert(SIG_ERR == __SPRT_SIG_ERR);

static_assert(sprt::is_same_v<sig_atomic_t, __sprt_sig_atomic_t>);

#endif // __STDC_HOSTED__

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(sigfillset)(__SPRT_ID(sigset_t) * set) {
#if __STDC_HOSTED__ == 0
	set->__bits[0] = 0xffff'ffff;
	return 0;
#else
	return sigfillset((sigset_t *)set);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(sigaddset)(__SPRT_ID(sigset_t) * set, int s) {
#if __STDC_HOSTED__ == 0
	if (s < __SPRT__NSIG) {
		set->__bits[0] |= (1 << s);
	}
	*__sprt___errno_location() = EINVAL;
	return -1;
#else
	return sigaddset((sigset_t *)set, s);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(sigdelset)(__SPRT_ID(sigset_t) * set, int s) {
#if __STDC_HOSTED__ == 0
	if (s < __SPRT__NSIG) {
		set->__bits[0] &= ~(1 << s);
		return 0;
	}
	*__sprt___errno_location() = EINVAL;
	return -1;
#else
	return sigdelset((sigset_t *)set, s);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(sigismember)(const __SPRT_ID(sigset_t) * set, int s) {
#if __STDC_HOSTED__ == 0
	if (s < __SPRT__NSIG) {
		return !!(set->__bits[0] & (1 << s));
	}
	return 0;
#else
	return sigismember((const sigset_t *)set, s);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(sigprocmask)(int m, const __SPRT_ID(sigset_t) * __SPRT_RESTRICT a,
		__SPRT_ID(sigset_t) * __SPRT_RESTRICT b) {
	return ::sigprocmask(m, (const sigset_t *)a, (sigset_t *)b);
}

__SPRT_C_FUNC int __SPRT_ID(sigsuspend)(const __SPRT_ID(sigset_t) * set) {
	return ::sigsuspend((const sigset_t *)set);
}

__SPRT_C_FUNC int __SPRT_ID(sigpending)(__SPRT_ID(sigset_t) * set) {
	return ::sigpending((sigset_t *)set);
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

// For hosted builds, delegate to the system implementation.
__SPRT_C_FUNC void (*__SPRT_ID(signal)(int sig, void (*cb)(int)))(int) {
	return ::signal(sig, cb); //
}

__SPRT_C_FUNC int __SPRT_ID(raise)(int sig) {
	return ::raise(sig); //
}

} // namespace sprt
