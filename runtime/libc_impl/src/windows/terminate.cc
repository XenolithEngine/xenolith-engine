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

#include <sprt/wrappers/windows/dl_api.h>
#include <sprt/wrappers/windows/thread_api.h>

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

static bool __callDtors(FunctionListNode **listptr) {
	auto list = *listptr;
	*listptr = nullptr; // prevent recursive loop if called from destructor

	__funcptr fn = 0;
	FunctionListNode *plist = nullptr;
	while (list) {
		auto counter = list->used;
		while (counter > 0) {
			fn = list->data[--counter];
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
	return true;
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
	if (!__addDtor(&s_atexit_list, &s_atexit_head, fn)) {
		return ENOMEM;
	}
	return 0;
}

__SPRT_C_FUNC int at_quick_exit(__funcptr fn) __SPRT_NOEXCEPT {
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
	// call atexit registrations

	__sprt_libc_thread_exit(false);

	__callDtors(&s_atexit_list);

	__initterm(__c_preterm_start, __c_preterm_end, true);
	__initterm(__c_term_start, __c_term_end, true);

	__stdio_exit();

	sprt::destroy_at(sprt::__libc::get());

	auto loader = sprt::DllLoader::get();
	loader->unload();

	sprt::destroy_at(loader);

	ExitProcess((UINT)result);
}

__SPRT_C_FUNC void quick_exit(int result) __SPRT_NOEXCEPT {
	__callDtors(&s_at_quick_exit_list);

	ExitProcess((UINT)result);
}

__SPRT_C_FUNC void _Exit(int result) __SPRT_NOEXCEPT {
	ExitProcess((UINT)result); //
}

__SPRT_C_FUNC void __std_terminate() { abort(); }

} // namespace sprt
