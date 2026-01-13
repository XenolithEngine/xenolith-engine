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

#define SIGHUP __SPRT_SIGHUP
#define SIGINT __SPRT_SIGINT
#define SIGQUIT __SPRT_SIGQUIT
#define SIGILL __SPRT_SIGILL
#define SIGTRAP __SPRT_SIGTRAP
#define SIGABRT __SPRT_SIGABRT
#define SIGIOT __SPRT_SIGIOT
#define SIGBUS __SPRT_SIGBUS
#define SIGFPE __SPRT_SIGFPE
#define SIGKILL __SPRT_SIGKILL
#define SIGUSR1 __SPRT_SIGUSR1
#define SIGSEGV __SPRT_SIGSEGV
#define SIGUSR2 __SPRT_SIGUSR2
#define SIGPIPE __SPRT_SIGPIPE
#define SIGALRM __SPRT_SIGALRM
#define SIGTERM __SPRT_SIGTERM
#define SIGSTKFLT __SPRT_SIGSTKFLT
#define SIGCHLD __SPRT_SIGCHLD
#define SIGCONT __SPRT_SIGCONT
#define SIGSTOP __SPRT_SIGSTOP
#define SIGTSTP __SPRT_SIGTSTP
#define SIGTTIN __SPRT_SIGTTIN
#define SIGTTOU __SPRT_SIGTTOU
#define SIGURG __SPRT_SIGURG
#define SIGXCPU __SPRT_SIGXCPU
#define SIGXFSZ __SPRT_SIGXFSZ
#define SIGVTALRM __SPRT_SIGVTALRM
#define SIGPROF __SPRT_SIGPROF
#define SIGWINCH __SPRT_SIGWINCH
#define SIGIO __SPRT_SIGIO
#define SIGPOLL __SPRT_SIGPOLL
#define SIGPWR __SPRT_SIGPWR
#define SIGSYS __SPRT_SIGSYS
#define SIGUNUSED __SPRT_SIGUNUSED

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
SPRT_FORCEINLINE int sigprocmask(int mode, const sigset_t *__SPRT_RESTRICT a,
		sigset_t *__SPRT_RESTRICT b) {
	return __SPRT_ID(sigprocmask)(mode, a, b);
}
SPRT_FORCEINLINE int sigsuspend(const sigset_t *set) { return __SPRT_ID(sigsuspend)(set); }
SPRT_FORCEINLINE int sigpending(sigset_t *set) { return __SPRT_ID(sigpending)(set); }

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
