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
#include "signal.h"
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
	return sigfillset((sigset_t *)set);
}

__SPRT_C_FUNC int __SPRT_ID(sigaddset)(__SPRT_ID(sigset_t) * set, int s) {
	return sigaddset((sigset_t *)set, s);
}
__SPRT_C_FUNC int __SPRT_ID(sigdelset)(__SPRT_ID(sigset_t) * set, int s) {
	return sigdelset((sigset_t *)set, s);
}
__SPRT_C_FUNC int __SPRT_ID(sigismember)(const __SPRT_ID(sigset_t) * set, int s) {
	return sigismember((const sigset_t *)set, s);
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

// For hosted builds, delegate to the system implementation.
__SPRT_C_FUNC void (*__SPRT_ID(signal)(int sig, void (*cb)(int)))(int) {
	return ::signal(sig, cb); //
}

__SPRT_C_FUNC int __SPRT_ID(raise)(int sig) {
	return ::raise(sig); //
}

} // namespace sprt
