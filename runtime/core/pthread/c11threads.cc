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

// C11 <threads.h> implemented on top of SPRT's pthread layer. Most entries forward
// to the matching __sprt_pthread_* function and translate the errno-style return
// into a thrd_* status code. thrd_create goes straight to thread_t::create (the
// same path as __sprt_pthread_create / _beginthreadex), stashing the C11 entry
// point and its argument in the new thread's embedded storage block — so it needs
// no separate heap allocation. pthread start routines return void* while C11 ones
// return int, hence the int<->void* round-trip recovered by thrd_join.

#define __SPRT_BUILD 1

#include "pthread_impl.h" // thread_t::create, __sprt_pthread_*, bare errno (EBUSY, ...)

#include <sprt/c/__sprt_threads.h>
#include <sprt/c/__sprt_sched.h> // __sprt_sched_yield
#include <sprt/c/bits/__sprt_intptr_t.h>

namespace sprt::_thread {

namespace {

// The C11 entry point and its argument, placement-constructed into the new
// thread's embedded storage block (see thread_t::create), so thrd_create avoids a
// heap allocation just as _beginthreadex / __sprt_pthread_create do.
struct c11_thrd_storage {
	__SPRT_ID(thrd_start_t) func;
	void *arg;
};

static_assert(sizeof(c11_thrd_storage) <= THREAD_STORAGE_BLOCK_SIZE);

} // namespace

// --- threads ---

__SPRT_C_FUNC int __SPRT_ID(thrd_create)(__SPRT_ID(thrd_t) *thr, __SPRT_ID(thrd_start_t) func,
		void *arg) {
	thread_t *t = nullptr;
	int r = thread_t::create(&t, nullptr, [](thread_base_t *th) -> void * {
		auto *s = reinterpret_cast<c11_thrd_storage *>(th->storage);
		return reinterpret_cast<void *>(static_cast<__SPRT_ID(intptr_t)>(s->func(s->arg)));
	}, arg, [&](uint8_t *buf, size_t) { new (buf) c11_thrd_storage{func, arg}; });

	if (r != 0) {
		// Leave a well-defined (null) handle on failure. C11 does not require it,
		// but callers that unconditionally thrd_join() the array (and platforms
		// where creation always fails, e.g. wasm with ENOSYS threads) would then
		// join an uninitialised handle; a null thrd_t makes join return an error
		// (via __pthread_join's `if (!thread) return ESRCH`) instead of trapping.
		*thr = nullptr;
		return (r == EAGAIN) ? __SPRT_THRD_NOMEM : __SPRT_THRD_ERROR;
	}
	*thr = reinterpret_cast<__SPRT_ID(thrd_t)>(t);
	return __SPRT_THRD_SUCCESS;
}

__SPRT_C_FUNC int __SPRT_ID(thrd_equal)(__SPRT_ID(thrd_t) a, __SPRT_ID(thrd_t) b) {
	return __sprt_pthread_equal(a, b);
}

__SPRT_C_FUNC __SPRT_ID(thrd_t) __SPRT_ID(thrd_current)(void) { return __sprt_pthread_self(); }

__SPRT_C_FUNC int __SPRT_ID(thrd_sleep)(const struct __SPRT_TIMESPEC_NAME *duration,
		struct __SPRT_TIMESPEC_NAME *remaining) {
	// C11 wants 0 on success and a negative value otherwise; nanosleep already
	// reports -1 on interruption/error.
	return __sprt_nanosleep(duration, remaining) == 0 ? 0 : -1;
}

__SPRT_C_FUNC void __SPRT_ID(thrd_yield)(void) { __sprt_sched_yield(); }

__SPRT_C_FUNC __SPRT_NORETURN void __SPRT_ID(thrd_exit)(int res) {
	__sprt_pthread_exit(reinterpret_cast<void *>(static_cast<__SPRT_ID(intptr_t)>(res)));
}

__SPRT_C_FUNC int __SPRT_ID(thrd_detach)(__SPRT_ID(thrd_t) thr) {
	return __sprt_pthread_detach(thr) == 0 ? __SPRT_THRD_SUCCESS : __SPRT_THRD_ERROR;
}

__SPRT_C_FUNC int __SPRT_ID(thrd_join)(__SPRT_ID(thrd_t) thr, int *res) {
	void *result = nullptr;
	if (__sprt_pthread_join(thr, &result) != 0) {
		return __SPRT_THRD_ERROR;
	}
	if (res) {
		*res = static_cast<int>(reinterpret_cast<__SPRT_ID(intptr_t)>(result));
	}
	return __SPRT_THRD_SUCCESS;
}

// --- mutexes ---

__SPRT_C_FUNC int __SPRT_ID(mtx_init)(__SPRT_ID(mtx_t) *mtx, int type) {
	if (type & __SPRT_MTX_RECURSIVE) {
		__SPRT_ID(pthread_mutexattr_t) attr;
		__sprt_pthread_mutexattr_init(&attr);
		__sprt_pthread_mutexattr_settype(&attr, __SPRT_PTHREAD_MUTEX_RECURSIVE);
		int r = __sprt_pthread_mutex_init(mtx, &attr);
		__sprt_pthread_mutexattr_destroy(&attr);
		return r == 0 ? __SPRT_THRD_SUCCESS : __SPRT_THRD_ERROR;
	}
	return __sprt_pthread_mutex_init(mtx, nullptr) == 0 ? __SPRT_THRD_SUCCESS : __SPRT_THRD_ERROR;
}

__SPRT_C_FUNC int __SPRT_ID(mtx_lock)(__SPRT_ID(mtx_t) *mtx) {
	return __sprt_pthread_mutex_lock(mtx) == 0 ? __SPRT_THRD_SUCCESS : __SPRT_THRD_ERROR;
}

__SPRT_C_FUNC int __SPRT_ID(mtx_trylock)(__SPRT_ID(mtx_t) *mtx) {
	int r = __sprt_pthread_mutex_trylock(mtx);
	if (r == 0) {
		return __SPRT_THRD_SUCCESS;
	}
	return (r == EBUSY) ? __SPRT_THRD_BUSY : __SPRT_THRD_ERROR;
}

__SPRT_C_FUNC int __SPRT_ID(mtx_timedlock)(__SPRT_ID(mtx_t) * __SPRT_RESTRICT mtx,
		const struct __SPRT_TIMESPEC_NAME * __SPRT_RESTRICT ts) {
	int r = __sprt_pthread_mutex_timedlock(mtx, ts);
	if (r == 0) {
		return __SPRT_THRD_SUCCESS;
	}
	return (r == ETIMEDOUT) ? __SPRT_THRD_TIMEDOUT : __SPRT_THRD_ERROR;
}

__SPRT_C_FUNC int __SPRT_ID(mtx_unlock)(__SPRT_ID(mtx_t) *mtx) {
	return __sprt_pthread_mutex_unlock(mtx) == 0 ? __SPRT_THRD_SUCCESS : __SPRT_THRD_ERROR;
}

__SPRT_C_FUNC void __SPRT_ID(mtx_destroy)(__SPRT_ID(mtx_t) *mtx) {
	__sprt_pthread_mutex_destroy(mtx);
}

// --- condition variables ---

__SPRT_C_FUNC int __SPRT_ID(cnd_init)(__SPRT_ID(cnd_t) *cond) {
	return __sprt_pthread_cond_init(cond, nullptr) == 0 ? __SPRT_THRD_SUCCESS : __SPRT_THRD_ERROR;
}

__SPRT_C_FUNC int __SPRT_ID(cnd_signal)(__SPRT_ID(cnd_t) *cond) {
	return __sprt_pthread_cond_signal(cond) == 0 ? __SPRT_THRD_SUCCESS : __SPRT_THRD_ERROR;
}

__SPRT_C_FUNC int __SPRT_ID(cnd_broadcast)(__SPRT_ID(cnd_t) *cond) {
	return __sprt_pthread_cond_broadcast(cond) == 0 ? __SPRT_THRD_SUCCESS : __SPRT_THRD_ERROR;
}

__SPRT_C_FUNC int __SPRT_ID(cnd_wait)(__SPRT_ID(cnd_t) *cond, __SPRT_ID(mtx_t) *mtx) {
	return __sprt_pthread_cond_wait(cond, mtx) == 0 ? __SPRT_THRD_SUCCESS : __SPRT_THRD_ERROR;
}

__SPRT_C_FUNC int __SPRT_ID(cnd_timedwait)(__SPRT_ID(cnd_t) * __SPRT_RESTRICT cond,
		__SPRT_ID(mtx_t) * __SPRT_RESTRICT mtx,
		const struct __SPRT_TIMESPEC_NAME * __SPRT_RESTRICT ts) {
	int r = __sprt_pthread_cond_timedwait(cond, mtx, ts);
	if (r == 0) {
		return __SPRT_THRD_SUCCESS;
	}
	return (r == ETIMEDOUT) ? __SPRT_THRD_TIMEDOUT : __SPRT_THRD_ERROR;
}

__SPRT_C_FUNC void __SPRT_ID(cnd_destroy)(__SPRT_ID(cnd_t) *cond) {
	__sprt_pthread_cond_destroy(cond);
}

// --- thread-specific storage ---

__SPRT_C_FUNC int __SPRT_ID(tss_create)(__SPRT_ID(tss_t) *key, __SPRT_ID(tss_dtor_t) dtor) {
	return __sprt_pthread_key_create(key, dtor) == 0 ? __SPRT_THRD_SUCCESS : __SPRT_THRD_ERROR;
}

__SPRT_C_FUNC void *__SPRT_ID(tss_get)(__SPRT_ID(tss_t) key) {
	return __sprt_pthread_getspecific(key);
}

__SPRT_C_FUNC int __SPRT_ID(tss_set)(__SPRT_ID(tss_t) key, void *val) {
	return __sprt_pthread_setspecific(key, val) == 0 ? __SPRT_THRD_SUCCESS : __SPRT_THRD_ERROR;
}

__SPRT_C_FUNC void __SPRT_ID(tss_delete)(__SPRT_ID(tss_t) key) { __sprt_pthread_key_delete(key); }

// --- one-time initialization ---

__SPRT_C_FUNC void __SPRT_ID(call_once)(__SPRT_ID(once_flag) *flag, void (*func)(void)) {
	__sprt_pthread_once(flag, func);
}

} // namespace sprt::_thread
