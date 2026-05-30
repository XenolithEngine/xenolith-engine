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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SYS_EVENT_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_EVENT_H_

/*
	Dispatch header for the BSD/macOS <sys/event.h> (kqueue event notification):
	- hosted SPRT build -> forwards to the system <sys/event.h> (#include_next)
	- otherwise         -> SPRT's own declarations (defined inline below)

	Public surface provided by the SPRT-own path (internal __sprt_* helpers excluded).
	A function tagged [gate: X] is declared only when __SPRT_CONFIG_HAVE_X is set for
	the target (or when __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS forces all of them).
	struct kevent and struct timespec come in via <sprt/c/sys/__sprt_event.h>.

	Macros:
	  filter types (EVFILT_*): EVFILT_READ, EVFILT_WRITE, EVFILT_AIO, EVFILT_VNODE,
	    EVFILT_PROC, EVFILT_SIGNAL, EVFILT_TIMER, EVFILT_MACHPORT, EVFILT_FS,
	    EVFILT_USER, EVFILT_VM, EVFILT_EXCEPT
	  action/flag bits (EV_*): EV_ADD, EV_DELETE, EV_ENABLE, EV_DISABLE, EV_ONESHOT,
	    EV_CLEAR, EV_RECEIPT, EV_DISPATCH, EV_UDATA_SPECIFIC, EV_VANISHED,
	    EV_SYSFLAGS, EV_FLAG0, EV_FLAG1, EV_EOF, EV_ERROR
	  filter-specific fflags (NOTE_*): the full set - user-filter control
	    (NOTE_TRIGGER, NOTE_FF*), vnode (NOTE_DELETE/WRITE/EXTEND/ATTRIB/LINK/RENAME/
	    REVOKE/FUNLOCK, lease notes), proc (NOTE_EXIT and its detail flags, NOTE_FORK,
	    NOTE_EXEC, NOTE_SIGNAL, NOTE_TRACK, NOTE_TRACKERR, NOTE_CHILD),
	    VM pressure (NOTE_VM_ family), and timer units/flags (NOTE_SECONDS/USECONDS/NSECONDS/
	    ABSOLUTE/LEEWAY/CRITICAL/BACKGROUND/MACH_CONTINUOUS_TIME/MACHTIME)
	  EV_SET(...) - helper macro to populate a struct kevent

	Functions  [gate: KQUEUE]:
	  kqueue - create a new kernel event queue, returning a descriptor
	  kevent - register changes on a queue and/or wait for pending events
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <sys/event.h>

#else

#include <sprt/c/sys/__sprt_event.h>

#define EVFILT_READ __SPRT_EVFILT_READ
#define EVFILT_WRITE __SPRT_EVFILT_WRITE
#define EVFILT_AIO __SPRT_EVFILT_AIO
#define EVFILT_VNODE __SPRT_EVFILT_VNODE
#define EVFILT_PROC __SPRT_EVFILT_PROC
#define EVFILT_SIGNAL __SPRT_EVFILT_SIGNAL
#define EVFILT_TIMER __SPRT_EVFILT_TIMER
#define EVFILT_MACHPORT __SPRT_EVFILT_MACHPORT
#define EVFILT_FS __SPRT_EVFILT_FS
#define EVFILT_USER __SPRT_EVFILT_USER
#define EVFILT_VM __SPRT_EVFILT_VM
#define EVFILT_EXCEPT __SPRT_EVFILT_EXCEPT

#define EV_ADD __SPRT_EV_ADD
#define EV_DELETE __SPRT_EV_DELETE
#define EV_ENABLE __SPRT_EV_ENABLE
#define EV_DISABLE __SPRT_EV_DISABLE

#define EV_ONESHOT __SPRT_EV_ONESHOT
#define EV_CLEAR __SPRT_EV_CLEAR
#define EV_RECEIPT __SPRT_EV_RECEIPT
#define EV_DISPATCH __SPRT_EV_DISPATCH
#define EV_UDATA_SPECIFIC __SPRT_EV_UDATA_SPECIFIC
#define EV_VANISHED __SPRT_EV_VANISHED
#define EV_SYSFLAGS __SPRT_EV_SYSFLAGS
#define EV_FLAG0 __SPRT_EV_FLAG0
#define EV_FLAG1 __SPRT_EV_FLAG1
#define EV_EOF __SPRT_EV_EOF
#define EV_ERROR __SPRT_EV_ERROR

#define NOTE_TRIGGER __SPRT_NOTE_TRIGGER
#define NOTE_FFNOP __SPRT_NOTE_FFNOP
#define NOTE_FFAND __SPRT_NOTE_FFAND
#define NOTE_FFOR __SPRT_NOTE_FFOR
#define NOTE_FFCOPY __SPRT_NOTE_FFCOPY
#define NOTE_FFCTRLMASK __SPRT_NOTE_FFCTRLMASK
#define NOTE_FFLAGSMASK __SPRT_NOTE_FFLAGSMASK
#define NOTE_LOWAT __SPRT_NOTE_LOWAT
#define NOTE_OOB __SPRT_NOTE_OOB
#define NOTE_DELETE __SPRT_NOTE_DELETE
#define NOTE_WRITE __SPRT_NOTE_WRITE
#define NOTE_EXTEND __SPRT_NOTE_EXTEND
#define NOTE_ATTRIB __SPRT_NOTE_ATTRIB
#define NOTE_LINK __SPRT_NOTE_LINK
#define NOTE_RENAME __SPRT_NOTE_RENAME
#define NOTE_REVOKE __SPRT_NOTE_REVOKE
#define NOTE_NONE __SPRT_NOTE_NONE
#define NOTE_FUNLOCK __SPRT_NOTE_FUNLOCK

#define NOTE_LEASE_DOWNGRADE __SPRT_NOTE_LEASE_DOWNGRADE
#define NOTE_LEASE_RELEASE __SPRT_NOTE_LEASE_RELEASE

#define NOTE_EXIT __SPRT_NOTE_EXIT
#define NOTE_FORK __SPRT_NOTE_FORK
#define NOTE_EXEC __SPRT_NOTE_EXEC
#define NOTE_SIGNAL __SPRT_NOTE_SIGNAL
#define NOTE_EXITSTATUS __SPRT_NOTE_EXITSTATUS
#define NOTE_EXIT_DETAIL __SPRT_NOTE_EXIT_DETAIL
#define NOTE_PDATAMASK __SPRT_NOTE_PDATAMASK
#define NOTE_PDATAMASK __SPRT_NOTE_PDATAMASK

#define NOTE_EXIT_DETAIL_MASK __SPRT_NOTE_EXIT_DETAIL_MASK
#define NOTE_EXIT_DECRYPTFAIL __SPRT_NOTE_EXIT_DECRYPTFAIL
#define NOTE_EXIT_MEMORY __SPRT_NOTE_EXIT_MEMORY
#define NOTE_EXIT_CSERROR __SPRT_NOTE_EXIT_CSERROR

#define NOTE_VM_PRESSURE __SPRT_NOTE_VM_PRESSURE
#define NOTE_VM_PRESSURE_TERMINATE __SPRT_NOTE_VM_PRESSURE_TERMINATE
#define NOTE_VM_PRESSURE_SUDDEN_TERMINATE __SPRT_NOTE_VM_PRESSURE_SUDDEN_TERMINATE
#define NOTE_VM_ERROR __SPRT_NOTE_VM_ERROR

#define NOTE_SECONDS __SPRT_NOTE_SECONDS
#define NOTE_USECONDS __SPRT_NOTE_USECONDS
#define NOTE_NSECONDS __SPRT_NOTE_NSECONDS
#define NOTE_ABSOLUTE __SPRT_NOTE_ABSOLUTE
#define NOTE_LEEWAY __SPRT_NOTE_LEEWAY
#define NOTE_CRITICAL __SPRT_NOTE_CRITICAL
#define NOTE_BACKGROUND __SPRT_NOTE_BACKGROUND

#define NOTE_MACH_CONTINUOUS_TIME __SPRT_NOTE_MACH_CONTINUOUS_TIME
#define NOTE_MACHTIME __SPRT_NOTE_MACHTIME

#define NOTE_TRACK __SPRT_NOTE_TRACK
#define NOTE_TRACKERR __SPRT_NOTE_TRACKERR
#define NOTE_CHILD __SPRT_NOTE_CHILD

#define EV_SET(...) __SPRT_EV_SET(__VA_ARGS__)

#if __SPRT_CONFIG_HAVE_KQUEUE || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS

__SPRT_BEGIN_DECL

SPRT_UMBRELLA_FUNC
int kqueue(void) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_kqueue();
}
#endif

SPRT_UMBRELLA_FUNC
int kevent(int kq, const struct __SPRT_KEVENT_NAME *changelist, int nchanges,
		struct __SPRT_KEVENT_NAME *eventlist, int nevents,
		const struct __SPRT_TIMESPEC_NAME *timeout) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_kevent(kq, changelist, nchanges, eventlist, nevents, timeout);
}
#endif

__SPRT_END_DECL

#endif // __SPRT_CONFIG_HAVE_KQUEUE

#endif // __SPRT_BUILD

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYS_EVENT_H_
