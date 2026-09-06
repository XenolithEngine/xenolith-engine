// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// ---------------------------------------------------------------------------
// cross/macos_sprt/signal.h <-> Darwin <signal.h> parity.
//
// Signal numbers go straight to libSystem's kill()/sigaction()/pthread_kill(),
// so the table must be Darwin's. Note SIGEMT/SIGINFO exist here and not on
// Linux, and SIGCHLD is 20 (Linux 17).
//
// Compile-time only; see check.sh.
// ---------------------------------------------------------------------------

#include <signal.h>
#include <sys/signal.h>

#define SPRT_ABI_HEADER <sprt/c/cross/__sprt_signal.h>
#include "abi_check.h"

// === signal numbers ========================================================
SPRT_CONST(SIGHUP);
SPRT_CONST(SIGINT);
SPRT_CONST(SIGQUIT);
SPRT_CONST(SIGILL);
SPRT_CONST(SIGTRAP);
SPRT_CONST(SIGABRT);
SPRT_CONST(SIGIOT);
SPRT_CONST(SIGEMT);
SPRT_CONST(SIGFPE);
SPRT_CONST(SIGKILL);
SPRT_CONST(SIGBUS);
SPRT_CONST(SIGSEGV);
SPRT_CONST(SIGSYS);
SPRT_CONST(SIGPIPE);
SPRT_CONST(SIGALRM);
SPRT_CONST(SIGTERM);
SPRT_CONST(SIGURG);
SPRT_CONST(SIGSTOP);
SPRT_CONST(SIGTSTP);
SPRT_CONST(SIGCONT);
SPRT_CONST(SIGCHLD);
SPRT_CONST(SIGTTIN);
SPRT_CONST(SIGTTOU);
SPRT_CONST(SIGIO);
SPRT_CONST(SIGXCPU);
SPRT_CONST(SIGXFSZ);
SPRT_CONST(SIGVTALRM);
SPRT_CONST(SIGPROF);
SPRT_CONST(SIGWINCH);
SPRT_CONST(SIGINFO);
SPRT_CONST(SIGUSR1);
SPRT_CONST(SIGUSR2);

// Darwin spells the table bound NSIG; sprt keeps the POSIX-ish _NSIG spelling.
SPRT_CONST_MAP(_NSIG, NSIG);

// SIG_DFL / SIG_IGN / SIG_HOLD / SIG_ERR are function-pointer sentinels
// ((void (*)(int))0, 1, 5, -1). An int-to-pointer cast is not a constant
// expression, so they cannot be static_asserted; the values are mirrored in
// cross/macos_sprt/signal.h and this omission is deliberate. Same reason the
// Windows harness cannot assert _CRTDBG_FILE_STDOUT.

// __SPRT_SIGSET_WORDS is sprt's own spelling of the sigset_t shape; Darwin has
// no such macro, so what is pinned is the resulting type.
SPRT_SIZE(__sprt_sigset_t, sigset_t);
SPRT_ALIGN(__sprt_sigset_t, sigset_t);
