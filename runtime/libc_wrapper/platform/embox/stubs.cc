/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/

// Embox already ships dlfcn, BSD sockets, locale, and C99 libm (often as
// clang __builtin_* macros). Do not redeclare those — Embox <dlfcn.h> has no
// extern "C" guard, and <math.h> macros expand cbrt/exp2/hypot into builtins.
//
// __cxa_thread_atexit: libc++abi drops cxa_thread_atexit.cpp when
// CMAKE_SYSTEM_NAME=Generic. Embox pthread_key_create ignores the destructor,
// so this list is drained only if something calls the key dtor by hand; it
// still has to exist for libc++ static locals.

#include <pthread.h>
#include <stdlib.h>
#include <sprt/c/sys/__sprt_socket.h>

// libunwind built with LIBUNWIND_IS_BAREMETAL reads the DWARF tables through
// __eh_frame_{start,end}. They are IMAGE linker script symbols, and the engine
// only ever emits a relocatable (-Wl,-r), so they stay undefined here — which
// is fine, -r keeps undefined symbols, and the image link is where they belong.
//
// They are deliberately NOT defined as weak placeholder objects here. That was
// the earlier shape and it is worse in both directions: with placeholders in
// force dwarf_section_length is 0, so findUnwindSections() silently answers "no
// unwind info" and every unwind stops at the first frame (a throw reaches
// std::terminate, the destructor-running longjmp in runtime_core_setjmp.cpp
// aborts) — a silent loss of the guarantee instead of a link error naming the
// missing symbol.
//
// Embox's mk/image.lds.S marks the region as `_eh_frame_begin` (one underscore,
// no end symbol at all), so the board build patches the two assignments in:
// xenolith-os/board/embox-qemu/patches/image.lds-eh-frame-bounds.py.

// The .eh_frame_hdr binary-search index does not exist in this image: the engine
// links with -Wl,-r, which produces no such section, and Embox's own image
// script does not build one either. libunwind takes "absent" to mean
// `&__eh_frame_hdr_start == 0` and then falls back to a linear FDE scan
// (UnwindCursor.hpp: `if (!foundFDE && (sects.dwarf_index_section != 0))`) —
// slower per frame, but correct.
//
// So these two cannot be weak .bss OBJECTS the way the pair above is: an object
// has a real address, libunwind reads one byte at it as a header and every
// single unwind step prints
//     libunwind: unsupported .eh_frame_hdr at <addr>: need at least 4 bytes ...
// They have to be absolute zero, which only an assembler symbol assignment can
// express. Still weak, so a linker script that does produce a real .eh_frame_hdr
// overrides them.
__asm__(".weak __eh_frame_hdr_start\n"
		".set __eh_frame_hdr_start, 0\n"
		".weak __eh_frame_hdr_end\n"
		".set __eh_frame_hdr_end, 0\n");

extern "C" struct __SPRT_CMSGHDR_NAME *__cmsg_nxthdr(struct __SPRT_MSGHDR_NAME *__mhdr,
		struct __SPRT_CMSGHDR_NAME *__cmsg) {
	if ((__SPRT_ID(size_t))__cmsg->cmsg_len < sizeof(struct __SPRT_CMSGHDR_NAME)) {
		return nullptr;
	}

	auto *__next = (struct __SPRT_CMSGHDR_NAME *)((unsigned char *)__cmsg
			+ __SPRT_CMSG_ALIGN(__cmsg->cmsg_len));
	auto *__end = (unsigned char *)__mhdr->msg_control + __mhdr->msg_controllen;

	// Both the header itself and the aligned payload have to fit: a truncated
	// control buffer must end the walk, not hand out a header to read past.
	if ((unsigned char *)(__next + 1) > __end
			|| (unsigned char *)__next + __SPRT_CMSG_ALIGN(__next->cmsg_len) > __end) {
		return nullptr;
	}
	return __next;
}

__attribute__((weak)) extern "C" int sendmmsg(SOCKET __fd, struct __SPRT_MMSGHDR_NAME *__msgvec,
		unsigned int __vlen, unsigned int __flags) {
	return __SPRT_ID(sendmmsg)(__fd, __msgvec, __vlen, __flags);
}

__attribute__((weak)) extern "C" int recvmmsg(SOCKET __fd, struct __SPRT_MMSGHDR_NAME *__msgvec,
		unsigned int __vlen, unsigned int __flags, struct __SPRT_TIMESPEC_NAME *__timeout) {
	return __SPRT_ID(recvmmsg)(__fd, __msgvec, __vlen, __flags, __timeout);
}

namespace {

struct CxaAtexitEntry {
	void (*dtor)(void *);
	void *arg;
	CxaAtexitEntry *next;
};

pthread_key_t s_cxaThreadKey;
pthread_once_t s_cxaThreadOnce = PTHREAD_ONCE_INIT;

void cxaThreadAtexitDrain(void *head) {
	auto *entry = static_cast<CxaAtexitEntry *>(head);
	while (entry) {
		if (entry->dtor) {
			entry->dtor(entry->arg);
		}
		auto *next = entry->next;
		free(entry);
		entry = next;
	}
}

void cxaThreadAtexitInit() { pthread_key_create(&s_cxaThreadKey, cxaThreadAtexitDrain); }

} // namespace

extern "C" int __cxa_thread_atexit(void (*dtor)(void *), void *arg, void * /*dsoHandle*/) {
	pthread_once(&s_cxaThreadOnce, cxaThreadAtexitInit);

	auto *entry = static_cast<CxaAtexitEntry *>(malloc(sizeof(CxaAtexitEntry)));
	if (!entry) {
		return -1;
	}
	entry->dtor = dtor;
	entry->arg = arg;
	entry->next = static_cast<CxaAtexitEntry *>(pthread_getspecific(s_cxaThreadKey));
	pthread_setspecific(s_cxaThreadKey, entry);
	return 0;
}
