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

#ifndef RUNTIME_SRC_LIBC_PLATFORM_WINDOWS_PTHREAD_WIN_H_
#define RUNTIME_SRC_LIBC_PLATFORM_WINDOWS_PTHREAD_WIN_H_

#ifndef __SPRT_BUILD
#define __SPRT_BUILD
#endif

#include <sprt/c/__sprt_pthread.h>
#include <sprt/c/__sprt_sched.h>
#include <sprt/c/__sprt_time.h>
#include <sprt/runtime/log.h>
#include <sprt/runtime/new.h>
#include <sprt/runtime/mutex.h>
#include <sprt/runtime/atomic.h>
#include <sprt/runtime/mem/set.h>
#include <sprt/runtime/mem/map.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <process.h>
#include <setjmp.h>

#include "private/SPRTSpecific.h" // IWYU pragma: keep

#include "sched_win.h"

/* Windows pthread implementation notes

This implementation is based on WinAPI functions from scratch and is not a reworking of
pthread4w, w32-pthread or any other library. The conventions for those implementations
may not apply to this one.

For full operation of this implementation, a Windows version of at least Windows 11 (Build 22000)
or Windows Server 2022 (Build 20348) is required. The main dependency is QueueUserAPC2 for a full
implementation of pthread_cancel.

Primitives based on WaitOnAddress (qmutex and waitable_address) are used for synchronization.

= Priorities =

Unlike other implementations, for thread prioritization to work you need:

1. Disable priority inheritance (pthread_attr_setinheritsched(t, PTHREAD_EXPLICIT_SCHED).
Otherwise, the thread will always inherit the priority of the creator thread.

2. Switch priority mode (pthread_attr_setschedpolicy(t, SCHED_RR)). In the default mode
SCHED_OTHER, priorities are not applied because the way they work is not described in POSIX.

3. Assign priority to a thread using pthread_attr_setschedparam

Similar rules apply to pthread_setschedparam, priorities are applied only in SCHED_RR mode and
only if priority inheritance is disabled.

For Windows, the standard thread switching algorithm is defined as SCHED_OTHER, and the
algorithm with a specific priority is defined as SCHED_RR, which reflects the description of
the thread prioritization mechanism in MSDN.

Priorities are determined by values ​​from 0 to 6, which directly maps to the values ​​in the
Windows API:

0 - THREAD_PRIORITY_IDLE
1 - THREAD_PRIORITY_LOWEST
2 - THREAD_PRIORITY_BELOW_NORMAL
3 - THREAD_PRIORITY_NORMAL (default)
4 - THREAD_PRIORITY_ABOVE_NORMAL
5 - THREAD_PRIORITY_HIGHEST
6 - THREAD_PRIORITY_TIME_CRITICAL

= Exit and cancel =

To force a thread to terminate, SPRT will exit to the thread's initial function using jongjmp.
This method, if C++ exceptions are enabled by the compiler, allows SPRT to correctly call all
destructors and unwind the stack.

There is no way to gracefully terminate a thread created outside of pthread (that is, not
pthread_create) using pthread_exit. Such a thread, however, will be terminated using
_endthreadex. In this case, data leaks, deadlocks and race conditions are possible, since
destructors will not be called.

pthread_cancel is not disabled in runtme configuration

= Thread attaching =

You can use the pthread API with threads not created with pthread_create, with a number of
restrictions:

* You can get pthread_t using pthread_self()
* Calling pthread_exit leads to data leaks and other problems (see above)
* pthread_join not possible


= Mutexes =

Mutexes implemented with WaitOnAddress. Keynotes:

* Function unavailability is reported by the pthread_mutexattr_* functions by setting errno to ENOTSUP.
pthread_mutex_init cannot fail due to invalid attributes.

* pthread_mutexattr_get* always reports the actual attribute value

* Inter-process mutexes (PTHREAD_PROCESS_SHARED) not available.

* Priority options not available, only PTHREAD_PRIO_NONE is supported as protocol. You can set and get
prioceiling, but it's noop.

* Only mutex type is PTHREAD_MUTEX_RECURSIVE. PTHREAD_MUTEX_DEFAULT maps to PTHREAD_MUTEX_RECURSIVE

* PTHREAD_MUTEX_ROBUST is supported with POSIX semantics. WaitOnAddress is not robust
by default, so, PTHREAD_MUTEX_STALLED is default for mutex.

* pthread_mutex_timedwait works against CLOCK_REALTIME, see notes on clocks for condward below


= Condvars =

* Implemented on top of WaitOnAddress

* Inter-process condvars (PTHREAD_PROCESS_SHARED) not available.

* Only the same mutex can be used for simultaneous waits.

pthread_condattr_setclock restrictions:
* CLOCK_REALTIME by default. Also available are CLOCK_REALTIME_COARSE, CLOCK_MONOTONIC, CLOCK_MONOTONIC_COARSE
* The clock parameter is used only to calculate the timeout for WaitOnAddress, not as a direct kernel limit
* The timeout for WaitOnAddress is expressed in milliseconds. The minimum wait time is 1 millisecond.


= Spinlock =

* No specific, just works


= RW Locks =

* Writer starvation should be partially prevented (gatekeeper algorithm)

* Robust (locks will be safely released when thread dies)


= Barrier =

* Experimental algorithm, based on in/out tickets with WaitOnAddress (needs confirmation in high loads)


= Keys =

* pthread_key_delete when specific values exists is safe

* PTHREAD_KEYS_MAX is UINT32_t MAX - 1

* PTHREAD_DESTRUCTOR_ITERATIONS is 4


= Other =

* pthread_atfork noop
* pthread_getconcurrency noop
* pthread_setconcurrency noop
* pthread_getcpuclockid emulated in clock_* side, you need to call pthread_getcpuclockid to actually create clockid

*/

namespace sprt {

static_assert(sizeof(pthread_once_t) == sizeof(__sprt_pthread_once_t));

} // namespace sprt

#endif // RUNTIME_SRC_LIBC_PLATFORM_WINDOWS_PTHREAD_WIN_H_
