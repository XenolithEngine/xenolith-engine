/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SIGNAL_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SIGNAL_H_

#ifdef __SPRT_BUILD

#include_next <signal.h>

#else

#include <sprt/c/__sprt_signal.h>

#ifdef __SPRT_SIGHUP
#define SIGHUP __SPRT_SIGHUP
#endif

#ifdef __SPRT_SIGINT
#define SIGINT __SPRT_SIGINT
#endif

#ifdef __SPRT_SIGQUIT
#define SIGQUIT __SPRT_SIGQUIT
#endif

#ifdef __SPRT_SIGILL
#define SIGILL __SPRT_SIGILL
#endif

#ifdef __SPRT_SIGTRAP
#define SIGTRAP __SPRT_SIGTRAP
#endif

#ifdef __SPRT_SIGABRT
#define SIGABRT __SPRT_SIGABRT
#endif

#ifdef __SPRT_SIGIOT
#define SIGIOT __SPRT_SIGIOT
#endif

#ifdef __SPRT_SIGBUS
#define SIGBUS __SPRT_SIGBUS
#endif

#ifdef __SPRT_SIGFPE
#define SIGFPE __SPRT_SIGFPE
#endif

#ifdef __SPRT_SIGKILL
#define SIGKILL __SPRT_SIGKILL
#endif

#ifdef __SPRT_SIGUSR
#define SIGUSR __SPRT_SIGUSR
#endif
1
#ifdef __SPRT_SIGSEGV
#define SIGSEGV __SPRT_SIGSEGV
#endif

#ifdef __SPRT_SIGUSR
#define SIGUSR __SPRT_SIGUSR
#endif
		2
#ifdef __SPRT_SIGPIPE
#define SIGPIPE __SPRT_SIGPIPE
#endif

#ifdef __SPRT_SIGALRM
#define SIGALRM __SPRT_SIGALRM
#endif

#ifdef __SPRT_SIGTERM
#define SIGTERM __SPRT_SIGTERM
#endif

#ifdef __SPRT_SIGSTKFLT
#define SIGSTKFLT __SPRT_SIGSTKFLT
#endif

#ifdef __SPRT_SIGCHLD
#define SIGCHLD __SPRT_SIGCHLD
#endif

#ifdef __SPRT_SIGCONT
#define SIGCONT __SPRT_SIGCONT
#endif

#ifdef __SPRT_SIGSTOP
#define SIGSTOP __SPRT_SIGSTOP
#endif

#ifdef __SPRT_SIGTSTP
#define SIGTSTP __SPRT_SIGTSTP
#endif

#ifdef __SPRT_SIGTTIN
#define SIGTTIN __SPRT_SIGTTIN
#endif

#ifdef __SPRT_SIGTTOU
#define SIGTTOU __SPRT_SIGTTOU
#endif

#ifdef __SPRT_SIGURG
#define SIGURG __SPRT_SIGURG
#endif

#ifdef __SPRT_SIGXCPU
#define SIGXCPU __SPRT_SIGXCPU
#endif

#ifdef __SPRT_SIGXFSZ
#define SIGXFSZ __SPRT_SIGXFSZ
#endif

#ifdef __SPRT_SIGVTALRM
#define SIGVTALRM __SPRT_SIGVTALRM
#endif

#ifdef __SPRT_SIGPROF
#define SIGPROF __SPRT_SIGPROF
#endif

#ifdef __SPRT_SIGWINCH
#define SIGWINCH __SPRT_SIGWINCH
#endif

#ifdef __SPRT_SIGIO
#define SIGIO __SPRT_SIGIO
#endif

#ifdef __SPRT_SIGPOLL
#define SIGPOLL __SPRT_SIGPOLL
#endif

#ifdef __SPRT_SIGPWR
#define SIGPWR __SPRT_SIGPWR
#endif

#ifdef __SPRT_SIGSYS
#define SIGSYS __SPRT_SIGSYS
#endif

#ifdef __SPRT_SIGUNUSED
#define SIGUNUSED __SPRT_SIGUNUSED
#endif


#define SIG_BLOCK __SPRT_SIG_BLOCK
#define SIG_UNBLOCK __SPRT_SIG_UNBLOCK
#define SIG_SETMASK __SPRT_SIG_SETMASK

#define _NSIG __SPRT__NSIG

		__SPRT_BEGIN_DECL

		typedef __SPRT_ID(sigset_t) sigset_t;

SPRT_FORCEINLINE int sigemptyset(sigset_t *set) { return __SPRT_ID(sigemptyset)(set); }
SPRT_FORCEINLINE int sigfillset(sigset_t *set) { return __SPRT_ID(sigfillset)(set); }
SPRT_FORCEINLINE int sigaddset(sigset_t *set, int s) { return __SPRT_ID(sigaddset)(set, s); }
SPRT_FORCEINLINE int sigdelset(sigset_t *set, int s) { return __SPRT_ID(sigdelset)(set, s); }
SPRT_FORCEINLINE int sigismember(const sigset_t *set, int s) {
	return __SPRT_ID(sigismember)(set, s);
}

#if __SPRT_CONFIG_HAVE_SIGNAL_SIGPROCMASK || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_FORCEINLINE int sigprocmask(int mode, const sigset_t *__SPRT_RESTRICT a,
		sigset_t *__SPRT_RESTRICT b) {
	return __SPRT_ID(sigprocmask)(mode, a, b);
}
SPRT_FORCEINLINE int sigsuspend(const sigset_t *set) { return __SPRT_ID(sigsuspend)(set); }
SPRT_FORCEINLINE int sigpending(sigset_t *set) { return __SPRT_ID(sigpending)(set); }
#endif

SPRT_FORCEINLINE int sigisemptyset(const sigset_t *set) { return __SPRT_ID(sigisemptyset)(set); }
SPRT_FORCEINLINE int sigorset(sigset_t *set, const sigset_t *a, const sigset_t *b) {
	return __SPRT_ID(sigorset)(set, a, b);
}
SPRT_FORCEINLINE int sigandset(sigset_t *set, const sigset_t *a, const sigset_t *b) {
	return __SPRT_ID(sigandset)(set, a, b);
}

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SIGNAL_H_
