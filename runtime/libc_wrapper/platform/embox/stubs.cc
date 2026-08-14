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

extern "C" {
__attribute__((weak)) char __eh_frame_start;
__attribute__((weak)) char __eh_frame_end;
__attribute__((weak)) char __eh_frame_hdr_start;
__attribute__((weak)) char __eh_frame_hdr_end;
} // extern "C"

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
