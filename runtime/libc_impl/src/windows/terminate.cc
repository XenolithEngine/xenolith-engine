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

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/cxx/detail/ctypes.h>
#include <sprt/cxx/mutex>
#include <sprt/cxx/atomic>

#include <sprt/wrappers/windows/dl_api.h>
#include <sprt/wrappers/windows/thread_api.h>
#include <sprt/wrappers/windows/basic_api.h>

#include "sprt/c/__sprt_stdlib.h"
#include "stdlib.h"
#include "../../include/__impl_libc.h"
#include "../../include/__impl_file.h"

#include "initterm.h"
#include "dll/dllloader.h"

namespace sprt {

static constexpr size_t FUNCTIONS_PER_NODE = 126;

struct FunctionListNode {
	FunctionListNode *next;
	uintptr_t used;
	__funcptr data[126];
};

static_assert(sizeof(FunctionListNode) == 1'024);

static FunctionListNode *s_atexit_list = nullptr;
static FunctionListNode s_atexit_head;

static FunctionListNode *s_at_quick_exit_list = nullptr;
static FunctionListNode s_at_quick_exit_head;

// Serializes mutations of the two process-global lists above (atexit /
// at_quick_exit). The thread-local list below is single-owner and needs no lock.
static sprt::mutex s_atexit_mutex;

// exit() must run teardown exactly once. Holds the id of the thread performing
// it (0 = none); used to distinguish a recursive exit() (from a handler) from a
// concurrent exit() on another thread.
static sprt::atomic<DWORD> s_exitingThread{0};

static __declspec(thread) FunctionListNode *tl_dtors_list = nullptr;
static __declspec(thread) FunctionListNode tl_dtors_head;

static __declspec(allocate(".CRT$XPA")) __funcptr __c_preterm_start[] = {nullptr};
static __declspec(allocate(".CRT$XPZ")) __funcptr __c_preterm_end[] = {nullptr};
static __declspec(allocate(".CRT$XTA")) __funcptr __c_term_start[] = {nullptr};
static __declspec(allocate(".CRT$XTZ")) __funcptr __c_term_end[] = {nullptr};

static bool __addDtor(FunctionListNode **list, FunctionListNode *head, __funcptr fn) {
	if (*list == nullptr) {
		*list = head;
		head->next = nullptr;
		head->used = 0;
	} else if ((*list)->used == FUNCTIONS_PER_NODE) {
		auto nextNode = (FunctionListNode *)__sprt_local_alloc(sizeof(FunctionListNode));
		if (!nextNode) {
			return false;
		}
		nextNode->next = *list;
		nextNode->used = 0;
		*list = nextNode;
	}
	(*list)->data[(*list)->used++] = fn;
	return true;
}

// Walk and run a list previously detached from its head pointer. Runs with NO
// lock held: a handler may legally re-register (atexit) or run for a long time.
static void __runDtorList(FunctionListNode *list) {
	FunctionListNode *plist = nullptr;
	while (list) {
		auto counter = list->used;
		while (counter > 0) {
			__funcptr fn = list->data[--counter];
			if (fn) {
				fn();
			}
		}
		plist = list;
		list = list->next;
		if (plist->next) {
			__sprt_local_free(plist, 0);
		}
	}
}

// Thread-local list: single owner, no lock required.
static bool __callDtors(FunctionListNode **listptr) {
	auto list = *listptr;
	*listptr = nullptr; // prevent recursive loop if called from destructor
	__runDtorList(list);
	return true;
}

// Global lists: detach the list under the lock (atomic w.r.t. atexit's
// __addDtor), then run the handlers with the lock released.
static void __callGlobalDtors(FunctionListNode **listptr) {
	FunctionListNode *list;
	{
		sprt::unique_lock lock(s_atexit_mutex);
		list = *listptr;
		*listptr = nullptr;
	}
	__runDtorList(list);
}

static VOID __callTlsDtors(PVOID DllHandle, DWORD Reason, PVOID Reserved) {
	if (Reason != DLL_THREAD_DETACH && Reason != DLL_PROCESS_DETACH) {
		return;
	}

	__callDtors(&tl_dtors_list);
}

extern const PIMAGE_TLS_CALLBACK __dyn_tls_dtor_callback = __callTlsDtors;

static __declspec(allocate(".CRT$XLC")) PIMAGE_TLS_CALLBACK __tls_dtors_delegate = __callTlsDtors;

__SPRT_C_FUNC int atexit(__funcptr fn) __SPRT_NOEXCEPT {
	sprt::unique_lock lock(s_atexit_mutex);
	if (!__addDtor(&s_atexit_list, &s_atexit_head, fn)) {
		return ENOMEM;
	}
	return 0;
}

__SPRT_C_FUNC int at_quick_exit(__funcptr fn) __SPRT_NOEXCEPT {
	sprt::unique_lock lock(s_atexit_mutex);
	if (!__addDtor(&s_at_quick_exit_list, &s_at_quick_exit_head, fn)) {
		return ENOMEM;
	}
	return 0;
}

void __sprt_libc_thread_exit(bool fromExternalThread) {
	__callDtors(&tl_dtors_list);

	if (fromExternalThread) {
		ExitThread(0);
	}
}

__SPRT_C_FUNC int __tlregdtor(__funcptr fn) __SPRT_NOEXCEPT {
	if (!__addDtor(&tl_dtors_list, &tl_dtors_head, fn)) {
		return ENOMEM;
	}
	return 0;
}

__SPRT_C_FUNC void exit(int result) __SPRT_NOEXCEPT {
	// Run teardown exactly once. A recursive call (an atexit handler that calls
	// exit) must finish via ExitProcess; a concurrent call from another thread
	// must park and let the in-progress exit terminate it — never re-run the
	// handlers or double-tear-down global state.
	DWORD self = GetCurrentThreadId();
	DWORD expected = 0;
	if (!s_exitingThread.compare_exchange_strong(expected, self)) {
		if (expected == self) {
			ExitProcess((UINT)result);
		}
		Sleep(INFINITE); // the in-progress exit() will ExitProcess and terminate us
		ExitProcess((UINT)result); // unreachable safety net
	}

	// Run this thread's TLS destructors, the global atexit handlers, and the C++
	// static destructors, then flush/close streams.
	__sprt_libc_thread_exit(false);

	__callGlobalDtors(&s_atexit_list);

	__initterm(__c_preterm_start, __c_preterm_end, true);
	__initterm(__c_term_start, __c_term_end, true);

	__stdio_exit();

	// Do NOT destroy __libc / unload the loader here: other threads may still be
	// executing libc calls that route through them (use-after-free). ExitProcess
	// terminates every other thread first, after which the OS runs
	// DLL_PROCESS_DETACH and reclaims all process memory, handles, and modules.
	ExitProcess((UINT)result);
}

__SPRT_C_FUNC void quick_exit(int result) __SPRT_NOEXCEPT {
	__callGlobalDtors(&s_at_quick_exit_list);

	ExitProcess((UINT)result);
}

__SPRT_C_FUNC void _Exit(int result) __SPRT_NOEXCEPT {
	ExitProcess((UINT)result); //
}

__SPRT_C_FUNC void __std_terminate() { abort(); }

} // namespace sprt
