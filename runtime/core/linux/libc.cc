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

#include <pthread.h>
#include <stdint.h>

#include <sprt/c/bits/__sprt_native_thread_id_t.h>
#include <sprt/c/bits/__sprt_uintptr_t.h>

#include "../include/__plock.h"

#if SPRT_EMBOX || (defined(__e2k__) && defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ > 8)
__SPRT_C_FUNC __sprt_native_thread_id_t __libc_main_thread =
		static_cast<__sprt_native_thread_id_t>(reinterpret_cast<__sprt_uintptr_t>(pthread_self()));
#else
__SPRT_C_FUNC __sprt_native_thread_id_t __libc_main_thread =
		static_cast<__sprt_native_thread_id_t>(pthread_self());
#endif


namespace sprt {

static __plock_storage s_plockStorage;

#if SPRT_EMBOX
// core/embox/emutls.cc: the calling thread's thread_local objects. Embox runs no
// pthread key destructors, so this, the last thing a thread does before it ends,
// is where they are given back.
void __emutls_release_self();
#endif

void __sprt_libc_thread_exit(bool externalThread) {
#if SPRT_EMBOX
	__emutls_release_self();
#endif
	pthread_exit(0);
}

__plock_storage *__libc_get_plock_storage() { return &s_plockStorage; }

} // namespace sprt
