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

#ifndef CORE_RUNTIME_INCLUDE_C_SYS___SPRT_EVENT_H_
#define CORE_RUNTIME_INCLUDE_C_SYS___SPRT_EVENT_H_

#include <sprt/c/cross/__sprt_config.h>

#include <sprt/c/__sprt_stdint.h>
#include <sprt/c/sys/__sprt_types.h>


#define __SPRT_EVFILT_READ             (-1)
#define __SPRT_EVFILT_WRITE            (-2)
#define __SPRT_EVFILT_AIO              (-3)
#define __SPRT_EVFILT_VNODE            (-4)
#define __SPRT_EVFILT_PROC             (-5)
#define __SPRT_EVFILT_SIGNAL           (-6)
#define __SPRT_EVFILT_TIMER            (-7)
#define __SPRT_EVFILT_MACHPORT         (-8)
#define __SPRT_EVFILT_FS               (-9)
#define __SPRT_EVFILT_USER             (-10)
#define __SPRT_EVFILT_VM               (-12)
#define __SPRT_EVFILT_EXCEPT           (-15)

#ifdef __SPRT_BUILD
#define __SPRT_KEVENT_NAME __SPRT_ID(kevent)
#else
#define __SPRT_KEVENT_NAME kevent
#endif

#pragma pack(4)

struct __SPRT_KEVENT_NAME {
	__SPRT_ID(uintptr_t) ident;
	__SPRT_ID(int16_t) filter;
	__SPRT_ID(uint16_t) flags;
	__SPRT_ID(uint32_t) fflags;
	__SPRT_ID(intptr_t) data;
	void *udata;
};

#pragma pack()

#define __SPRT_EV_SET(kevp, a, b, c, d, e, f) do {     \
	struct __SPRT_KEVENT_NAME *__kevp__ = (kevp);       \
	__kevp__->ident = (a);                  \
	__kevp__->filter = (b);                 \
	__kevp__->flags = (c);                  \
	__kevp__->fflags = (d);                 \
	__kevp__->data = (e);                   \
	__kevp__->udata = (f);                  \
} while(0)

// clang-format off
#define __SPRT_KEVENT_FLAG_NONE         0x000000
#define __SPRT_KEVENT_FLAG_IMMEDIATE    0x000001
#define __SPRT_KEVENT_FLAG_ERROR_EVENTS 0x000002
// clang-format on

/* actions */
#define __SPRT_EV_ADD              0x0001
#define __SPRT_EV_DELETE           0x0002
#define __SPRT_EV_ENABLE           0x0004
#define __SPRT_EV_DISABLE          0x0008

/* flags */
#define __SPRT_EV_ONESHOT          0x0010
#define __SPRT_EV_CLEAR            0x0020
#define __SPRT_EV_RECEIPT          0x0040
#define __SPRT_EV_DISPATCH         0x0080
#define __SPRT_EV_UDATA_SPECIFIC   0x0100
#define __SPRT_EV_DISPATCH2        (__SPRT_EV_DISPATCH | __SPRT_EV_UDATA_SPECIFIC)
#define __SPRT_EV_VANISHED         0x0200
#define __SPRT_EV_SYSFLAGS         0xF000
#define __SPRT_EV_FLAG0            0x1000
#define __SPRT_EV_FLAG1            0x2000
#define __SPRT_EV_EOF              0x8000
#define __SPRT_EV_ERROR            0x4000
#define __SPRT_EV_POLL         __SPRT_EV_FLAG0
#define __SPRT_EV_OOBAND       __SPRT_EV_FLAG1

#define __SPRT_NOTE_TRIGGER    0x0100'0000
#define __SPRT_NOTE_FFNOP      0x0000'0000
#define __SPRT_NOTE_FFAND      0x4000'0000
#define __SPRT_NOTE_FFOR       0x8000'0000
#define __SPRT_NOTE_FFCOPY     0xc000'0000
#define __SPRT_NOTE_FFCTRLMASK 0xc000'0000
#define __SPRT_NOTE_FFLAGSMASK 0x00ff'ffff
#define __SPRT_NOTE_LOWAT      0x0000'0001
#define __SPRT_NOTE_OOB        0x0000'0002
#define __SPRT_NOTE_DELETE     0x0000'0001
#define __SPRT_NOTE_WRITE      0x0000'0002
#define __SPRT_NOTE_EXTEND     0x0000'0004
#define __SPRT_NOTE_ATTRIB     0x0000'0008
#define __SPRT_NOTE_LINK       0x0000'0010
#define __SPRT_NOTE_RENAME     0x0000'0020
#define __SPRT_NOTE_REVOKE     0x0000'0040
#define __SPRT_NOTE_NONE       0x0000'0080
#define __SPRT_NOTE_FUNLOCK    0x0000'0100

#define __SPRT_NOTE_LEASE_DOWNGRADE 0x0000'0200
#define __SPRT_NOTE_LEASE_RELEASE 0x0000'0400

#define __SPRT_NOTE_EXIT               0x8000'0000
#define __SPRT_NOTE_FORK               0x4000'0000
#define __SPRT_NOTE_EXEC               0x2000'0000
#define __SPRT_NOTE_SIGNAL             0x0800'0000
#define __SPRT_NOTE_EXITSTATUS         0x0400'0000
#define __SPRT_NOTE_EXIT_DETAIL        0x0200'0000
#define __SPRT_NOTE_PDATAMASK  0x000f'ffff
#define __SPRT_NOTE_PCTRLMASK  (~__SPRT_NOTE_PDATAMASK)

#define __SPRT_NOTE_EXIT_DETAIL_MASK           0x0007'0000
#define __SPRT_NOTE_EXIT_DECRYPTFAIL           0x0001'0000
#define __SPRT_NOTE_EXIT_MEMORY                0x0002'0000
#define __SPRT_NOTE_EXIT_CSERROR               0x0004'0000

#define __SPRT_NOTE_VM_PRESSURE                        0x8000'0000
#define __SPRT_NOTE_VM_PRESSURE_TERMINATE              0x4000'0000
#define __SPRT_NOTE_VM_PRESSURE_SUDDEN_TERMINATE       0x2000'0000
#define __SPRT_NOTE_VM_ERROR                           0x1000'0000

#define __SPRT_NOTE_SECONDS    0x0000'0001
#define __SPRT_NOTE_USECONDS   0x0000'0002
#define __SPRT_NOTE_NSECONDS   0x0000'0004
#define __SPRT_NOTE_ABSOLUTE   0x0000'0008
#define __SPRT_NOTE_LEEWAY     0x0000'0010
#define __SPRT_NOTE_CRITICAL   0x0000'0020
#define __SPRT_NOTE_BACKGROUND 0x0000'0040

#define __SPRT_NOTE_MACH_CONTINUOUS_TIME       0x0000'0080
#define __SPRT_NOTE_MACHTIME   0x0000'0100

#define __SPRT_NOTE_TRACK      0x0000'0001
#define __SPRT_NOTE_TRACKERR   0x0000'0002
#define __SPRT_NOTE_CHILD      0x0000'0004


#if __SPRT_CONFIG_HAVE_KQUEUE || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS

__SPRT_BEGIN_DECL

__SPRT_CONFIG_HAVE_KQUEUE_NOTICE
SPRT_API int __SPRT_ID(kqueue)(void);

__SPRT_CONFIG_HAVE_KQUEUE_NOTICE
SPRT_API int __SPRT_ID(kevent)(int kq, const struct __SPRT_KEVENT_NAME *changelist, int nchanges,
		struct __SPRT_KEVENT_NAME *eventlist, int nevents,
		const struct __SPRT_TIMESPEC_NAME *timeout);

__SPRT_END_DECL

#endif


#endif // CORE_RUNTIME_INCLUDE_C_SYS___SPRT_EVENT_H_
