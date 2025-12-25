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

#ifndef CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_SIGNAL_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_SIGNAL_H_

#include <c/bits/__sprt_def.h>
#include <c/bits/__sprt_int.h>

#define __SPRT_SIGHUP    1
#define __SPRT_SIGINT    2
#define __SPRT_SIGQUIT   3
#define __SPRT_SIGILL    4
#define __SPRT_SIGTRAP   5
#define __SPRT_SIGABRT   6
#define __SPRT_SIGIOT    __SPRT_SIGABRT
#define __SPRT_SIGBUS    7
#define __SPRT_SIGFPE    8
#define __SPRT_SIGKILL   9
#define __SPRT_SIGUSR1   10
#define __SPRT_SIGSEGV   11
#define __SPRT_SIGUSR2   12
#define __SPRT_SIGPIPE   13
#define __SPRT_SIGALRM   14
#define __SPRT_SIGTERM   15
#define __SPRT_SIGSTKFLT 16
#define __SPRT_SIGCHLD   17
#define __SPRT_SIGCONT   18
#define __SPRT_SIGSTOP   19
#define __SPRT_SIGTSTP   20
#define __SPRT_SIGTTIN   21
#define __SPRT_SIGTTOU   22
#define __SPRT_SIGURG    23
#define __SPRT_SIGXCPU   24
#define __SPRT_SIGXFSZ   25
#define __SPRT_SIGVTALRM 26
#define __SPRT_SIGPROF   27
#define __SPRT_SIGWINCH  28
#define __SPRT_SIGIO     29
#define __SPRT_SIGPOLL   29
#define __SPRT_SIGPWR    30
#define __SPRT_SIGSYS    31
#define __SPRT_SIGUNUSED __SPRT_SIGSYS

#if SPRT_LINUX

#define __SPRT_SIG_ERR  ((void (*)(int))-1)
#define __SPRT_SIG_DFL  ((void (*)(int)) 0)
#define __SPRT_SIG_IGN  ((void (*)(int)) 1)

typedef int __SPRT_ID(sig_atomic_t);

#define __SPRT_SIG_ATOMIC_MIN __SPRT_INT_MAX
#define __SPRT_SIG_ATOMIC_MAX (-1-__SPRT_INT_MAX)

#if defined(__x86_64__) || defined(_M_X64)
#include <c/cross/linux/x86_64/signal.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <c/cross/linux/aarch64/signal.h>
#else
#error "Unknown Linux arch"
#endif

#elif SPRT_WINDOWS

#define __SPRT_SIG_DFL ((void (*)(int))0)     // default signal action
#define __SPRT_SIG_IGN ((void (*)(int))1)     // ignore signal
#define __SPRT_SIG_GET ((void (*)(int))2)     // return current value
#define __SPRT_SIG_SGE ((void (*)(int))3)     // signal gets error
#define __SPRT_SIG_ACK ((void (*)(int))4)
#define __SPRT_SIG_ERR __SPRT_SIG_SGE

typedef int __SPRT_ID(sig_atomic_t);

#define __SPRT_SIG_ATOMIC_MIN __SPRT_INT_MAX
#define __SPRT_SIG_ATOMIC_MAX (-1-__SPRT_INT_MAX)

#define __SPRT__NSIG 23

#elif SPRT_ANDROID

#define __SPRT_SIG_ERR  ((void (*)(int))-1)
#define __SPRT_SIG_DFL  ((void (*)(int)) 0)
#define __SPRT_SIG_IGN  ((void (*)(int)) 1)

typedef int __SPRT_ID(sig_atomic_t);

#define __SPRT_SIG_ATOMIC_MIN __SPRT_INT_MAX
#define __SPRT_SIG_ATOMIC_MAX (-1-__SPRT_INT_MAX)

#define __SPRT__NSIG 65

#elif SPRT_MACOS

#error "Unknown OS"

#if defined(__x86_64__) || defined(_M_X64)
#include <c/cross/macos/x86_64/signal.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <c/cross/macos/aarch64/signal.h>
#else
#error "Unknown Macos arch"
#endif

#else
#error "Unknown OS"
#endif


#endif
