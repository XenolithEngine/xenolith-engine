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
#include <sprt/c/sys/__sprt_event.h>
#include <sprt/runtime/log.h>

#if __SPRT_CONFIG_HAVE_KQUEUE
#include <sys/event.h>
#include <time.h>
#endif

#include "private/SPRTSpecific.h"

namespace sprt {

#if __SPRT_CONFIG_HAVE_KQUEUE

static_assert(sizeof(struct kevent) == sizeof(struct __SPRT_KEVENT_NAME));

static_assert(EVFILT_READ == __SPRT_EVFILT_READ);
static_assert(EVFILT_WRITE == __SPRT_EVFILT_WRITE);
static_assert(EVFILT_AIO == __SPRT_EVFILT_AIO);
static_assert(EVFILT_VNODE == __SPRT_EVFILT_VNODE);
static_assert(EVFILT_PROC == __SPRT_EVFILT_PROC);
static_assert(EVFILT_SIGNAL == __SPRT_EVFILT_SIGNAL);
static_assert(EVFILT_TIMER == __SPRT_EVFILT_TIMER);
static_assert(EVFILT_MACHPORT == __SPRT_EVFILT_MACHPORT);
static_assert(EVFILT_FS == __SPRT_EVFILT_FS);
static_assert(EVFILT_USER == __SPRT_EVFILT_USER);
static_assert(EVFILT_VM == __SPRT_EVFILT_VM);
static_assert(EVFILT_EXCEPT == __SPRT_EVFILT_EXCEPT);

static_assert(EV_ADD == __SPRT_EV_ADD);
static_assert(EV_DELETE == __SPRT_EV_DELETE);
static_assert(EV_ENABLE == __SPRT_EV_ENABLE);
static_assert(EV_DISABLE == __SPRT_EV_DISABLE);

static_assert(EV_ONESHOT == __SPRT_EV_ONESHOT);
static_assert(EV_CLEAR == __SPRT_EV_CLEAR);
static_assert(EV_RECEIPT == __SPRT_EV_RECEIPT);
static_assert(EV_DISPATCH == __SPRT_EV_DISPATCH);
static_assert(EV_UDATA_SPECIFIC == __SPRT_EV_UDATA_SPECIFIC);
static_assert(EV_UDATA_SPECIFIC == __SPRT_EV_UDATA_SPECIFIC);
static_assert(EV_VANISHED == __SPRT_EV_VANISHED);
static_assert(EV_SYSFLAGS == __SPRT_EV_SYSFLAGS);
static_assert(EV_FLAG0 == __SPRT_EV_FLAG0);
static_assert(EV_FLAG1 == __SPRT_EV_FLAG1);
static_assert(EV_EOF == __SPRT_EV_EOF);
static_assert(EV_ERROR == __SPRT_EV_ERROR);
static_assert(EV_FLAG0 == __SPRT_EV_FLAG0);
static_assert(EV_FLAG1 == __SPRT_EV_FLAG1);

static_assert(NOTE_TRIGGER == __SPRT_NOTE_TRIGGER);
static_assert(NOTE_FFNOP == __SPRT_NOTE_FFNOP);
static_assert(NOTE_FFAND == __SPRT_NOTE_FFAND);
static_assert(NOTE_FFOR == __SPRT_NOTE_FFOR);
static_assert(NOTE_FFCOPY == __SPRT_NOTE_FFCOPY);
static_assert(NOTE_FFCTRLMASK == __SPRT_NOTE_FFCTRLMASK);
static_assert(NOTE_FFLAGSMASK == __SPRT_NOTE_FFLAGSMASK);
static_assert(NOTE_LOWAT == __SPRT_NOTE_LOWAT);
static_assert(NOTE_OOB == __SPRT_NOTE_OOB);
static_assert(NOTE_DELETE == __SPRT_NOTE_DELETE);
static_assert(NOTE_WRITE == __SPRT_NOTE_WRITE);
static_assert(NOTE_EXTEND == __SPRT_NOTE_EXTEND);
static_assert(NOTE_ATTRIB == __SPRT_NOTE_ATTRIB);
static_assert(NOTE_LINK == __SPRT_NOTE_LINK);
static_assert(NOTE_RENAME == __SPRT_NOTE_RENAME);
static_assert(NOTE_REVOKE == __SPRT_NOTE_REVOKE);
static_assert(NOTE_NONE == __SPRT_NOTE_NONE);
static_assert(NOTE_FUNLOCK == __SPRT_NOTE_FUNLOCK);

static_assert(NOTE_LEASE_DOWNGRADE == __SPRT_NOTE_LEASE_DOWNGRADE);
static_assert(NOTE_LEASE_RELEASE == __SPRT_NOTE_LEASE_RELEASE);

static_assert(NOTE_EXIT == __SPRT_NOTE_EXIT);
static_assert(NOTE_FORK == __SPRT_NOTE_FORK);
static_assert(NOTE_EXEC == __SPRT_NOTE_EXEC);
static_assert(NOTE_SIGNAL == __SPRT_NOTE_SIGNAL);
static_assert(NOTE_EXITSTATUS == __SPRT_NOTE_EXITSTATUS);
static_assert(NOTE_EXIT_DETAIL == __SPRT_NOTE_EXIT_DETAIL);
static_assert(NOTE_PDATAMASK == __SPRT_NOTE_PDATAMASK);
static_assert(NOTE_PDATAMASK == __SPRT_NOTE_PDATAMASK);

static_assert(NOTE_EXIT_DETAIL_MASK == __SPRT_NOTE_EXIT_DETAIL_MASK);
static_assert(NOTE_EXIT_DECRYPTFAIL == __SPRT_NOTE_EXIT_DECRYPTFAIL);
static_assert(NOTE_EXIT_MEMORY == __SPRT_NOTE_EXIT_MEMORY);
static_assert(NOTE_EXIT_CSERROR == __SPRT_NOTE_EXIT_CSERROR);

static_assert(NOTE_VM_PRESSURE == __SPRT_NOTE_VM_PRESSURE);
static_assert(NOTE_VM_PRESSURE_TERMINATE == __SPRT_NOTE_VM_PRESSURE_TERMINATE);
static_assert(NOTE_VM_PRESSURE_SUDDEN_TERMINATE == __SPRT_NOTE_VM_PRESSURE_SUDDEN_TERMINATE);
static_assert(NOTE_VM_ERROR == __SPRT_NOTE_VM_ERROR);

static_assert(NOTE_SECONDS == __SPRT_NOTE_SECONDS);
static_assert(NOTE_USECONDS == __SPRT_NOTE_USECONDS);
static_assert(NOTE_NSECONDS == __SPRT_NOTE_NSECONDS);
static_assert(NOTE_ABSOLUTE == __SPRT_NOTE_ABSOLUTE);
static_assert(NOTE_LEEWAY == __SPRT_NOTE_LEEWAY);
static_assert(NOTE_CRITICAL == __SPRT_NOTE_CRITICAL);
static_assert(NOTE_BACKGROUND == __SPRT_NOTE_BACKGROUND);

static_assert(NOTE_MACH_CONTINUOUS_TIME == __SPRT_NOTE_MACH_CONTINUOUS_TIME);
static_assert(NOTE_MACHTIME == __SPRT_NOTE_MACHTIME);

static_assert(NOTE_TRACK == __SPRT_NOTE_TRACK);
static_assert(NOTE_TRACKERR == __SPRT_NOTE_TRACKERR);
static_assert(NOTE_CHILD == __SPRT_NOTE_CHILD);

__SPRT_C_FUNC int __SPRT_ID(kqueue)(void) { return ::kqueue(); }

__SPRT_C_FUNC int __SPRT_ID(kevent)(int kq, const struct __SPRT_KEVENT_NAME *changelist,
		int nchanges, struct __SPRT_KEVENT_NAME *eventlist, int nevents,
		const struct __SPRT_TIMESPEC_NAME *timeout) {
	struct timespec _timeout;
	if (timeout) {
		_timeout.tv_sec = timeout->tv_sec;
		_timeout.tv_nsec = timeout->tv_nsec;
	}

	return ::kevent(kq, (const struct kevent *)changelist, nchanges, (struct kevent *)eventlist,
			nevents, timeout ? &_timeout : nullptr);
}

#else

__SPRT_C_FUNC int __SPRT_ID(kqueue)(void) {
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_KQUEUE)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(kevent)(int kq, const struct __SPRT_KEVENT_NAME *changelist,
		int nchanges, struct __SPRT_KEVENT_NAME *eventlist, int nevents,
		const struct __SPRT_TIMESPEC_NAME *timeout) {
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_KQUEUE)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

#endif

} // namespace sprt
