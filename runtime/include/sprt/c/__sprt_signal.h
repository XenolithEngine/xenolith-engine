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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_SIGNAL_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_SIGNAL_H_

#include <sprt/c/cross/__sprt_signal.h>
#include <sprt/c/bits/__sprt_int.h>

#define __SPRT_SIG_BLOCK 0
#define __SPRT_SIG_UNBLOCK 1
#define __SPRT_SIG_SETMASK 2

__SPRT_BEGIN_DECL

SPRT_API int __SPRT_ID(sigemptyset)(__SPRT_ID(sigset_t) * set);
SPRT_API int __SPRT_ID(sigfillset)(__SPRT_ID(sigset_t) * set);
SPRT_API int __SPRT_ID(sigaddset)(__SPRT_ID(sigset_t) * set, int);
SPRT_API int __SPRT_ID(sigdelset)(__SPRT_ID(sigset_t) * set, int);
SPRT_API int __SPRT_ID(sigismember)(const __SPRT_ID(sigset_t) * set, int);

#if __SPRT_CONFIG_HAVE_SIGNAL_SIGPROCMASK || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_API int __SPRT_ID(sigprocmask)(int, const __SPRT_ID(sigset_t) * __SPRT_RESTRICT,
		__SPRT_ID(sigset_t) * __SPRT_RESTRICT);
SPRT_API int __SPRT_ID(sigsuspend)(const __SPRT_ID(sigset_t) *);
SPRT_API int __SPRT_ID(sigpending)(__SPRT_ID(sigset_t) *);
#endif

SPRT_API int __SPRT_ID(sigisemptyset)(const __SPRT_ID(sigset_t) *);
SPRT_API int __SPRT_ID(
		sigorset)(__SPRT_ID(sigset_t) *, const __SPRT_ID(sigset_t) *, const __SPRT_ID(sigset_t) *);
SPRT_API int __SPRT_ID(
		sigandset)(__SPRT_ID(sigset_t) *, const __SPRT_ID(sigset_t) *, const __SPRT_ID(sigset_t) *);

SPRT_API void (*__SPRT_ID(signal)(int, void (*)(int)))(int);
SPRT_API int __SPRT_ID(raise)(int);

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_SIGNAL_H_
