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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_THREADS_H_
#define CORE_RUNTIME_INCLUDE_LIBC_THREADS_H_

/*
	Dispatch header for C11 <threads.h>:
	- hosted SPRT build -> forwards to the system <threads.h> (#include_next)
	- otherwise         -> SPRT's own definitions via sprt/c/__sprt_threads.h,
	                       which sit on top of the pthread implementation in
	                       runtime_core (see runtime/core/pthread/c11threads.cc).

	Threads:     thrd_create thrd_equal thrd_current thrd_sleep thrd_yield
	             thrd_exit thrd_detach thrd_join
	Mutexes:     mtx_init mtx_lock mtx_trylock mtx_timedlock mtx_unlock mtx_destroy
	Conditions:  cnd_init cnd_signal cnd_broadcast cnd_wait cnd_timedwait cnd_destroy
	TSS:         tss_create tss_get tss_set tss_delete
	Once:        call_once
	Macros:      thread_local, ONCE_FLAG_INIT, TSS_DTOR_ITERATIONS
	Enums:       thrd_success/busy/error/nomem/timedout, mtx_plain/recursive/timed
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <threads.h>

#else

#include <sprt/c/__sprt_threads.h>
#include <time.h>

#if !defined(__cplusplus) && !defined(thread_local)
#define thread_local _Thread_local
#endif

typedef __SPRT_ID(thrd_t) thrd_t;
typedef __SPRT_ID(mtx_t) mtx_t;
typedef __SPRT_ID(cnd_t) cnd_t;
typedef __SPRT_ID(tss_t) tss_t;
typedef __SPRT_ID(once_flag) once_flag;
typedef __SPRT_ID(thrd_start_t) thrd_start_t;
typedef __SPRT_ID(tss_dtor_t) tss_dtor_t;

enum {
	thrd_success = __SPRT_THRD_SUCCESS,
	thrd_busy = __SPRT_THRD_BUSY,
	thrd_error = __SPRT_THRD_ERROR,
	thrd_nomem = __SPRT_THRD_NOMEM,
	thrd_timedout = __SPRT_THRD_TIMEDOUT,
};

enum {
	mtx_plain = __SPRT_MTX_PLAIN,
	mtx_recursive = __SPRT_MTX_RECURSIVE,
	mtx_timed = __SPRT_MTX_TIMED,
};

#define ONCE_FLAG_INIT __SPRT_ONCE_FLAG_INIT
#define TSS_DTOR_ITERATIONS __SPRT_TSS_DTOR_ITERATIONS

__SPRT_BEGIN_DECL

// --- threads ---

SPRT_UMBRELLA_FUNC
int thrd_create(thrd_t *thr, thrd_start_t func, void *arg) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_thrd_create(thr, func, arg);
}
#endif

SPRT_UMBRELLA_FUNC
int thrd_equal(thrd_t a, thrd_t b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_thrd_equal(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
thrd_t thrd_current(void) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_thrd_current();
}
#endif

SPRT_UMBRELLA_FUNC
int thrd_sleep(const struct __SPRT_TIMESPEC_NAME *duration,
		struct __SPRT_TIMESPEC_NAME *remaining) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_thrd_sleep(duration, remaining);
}
#endif

SPRT_UMBRELLA_FUNC
void thrd_yield(void) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_thrd_yield();
}
#endif

SPRT_UMBRELLA_FUNC
// Forwards to pthread_exit, so it unwinds this thread - see include_libc/pthread.h.
__SPRT_NORETURN void thrd_exit(int res) SPRT_UMBRELLA_END_EXCEPT
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_thrd_exit(res);
}
#endif

SPRT_UMBRELLA_FUNC
int thrd_detach(thrd_t thr) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_thrd_detach(thr);
}
#endif

SPRT_UMBRELLA_FUNC
int thrd_join(thrd_t thr, int *res) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_thrd_join(thr, res);
}
#endif

// --- mutexes ---

SPRT_UMBRELLA_FUNC
int mtx_init(mtx_t *mtx, int type) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_mtx_init(mtx, type);
}
#endif

SPRT_UMBRELLA_FUNC
int mtx_lock(mtx_t *mtx) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_mtx_lock(mtx);
}
#endif

SPRT_UMBRELLA_FUNC
int mtx_trylock(mtx_t *mtx) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_mtx_trylock(mtx);
}
#endif

SPRT_UMBRELLA_FUNC
int mtx_timedlock(mtx_t *__SPRT_RESTRICT mtx,
		const struct __SPRT_TIMESPEC_NAME *__SPRT_RESTRICT ts) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_mtx_timedlock(mtx, ts);
}
#endif

SPRT_UMBRELLA_FUNC
int mtx_unlock(mtx_t *mtx) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_mtx_unlock(mtx);
}
#endif

SPRT_UMBRELLA_FUNC
void mtx_destroy(mtx_t *mtx) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_mtx_destroy(mtx);
}
#endif

// --- condition variables ---

SPRT_UMBRELLA_FUNC
int cnd_init(cnd_t *cond) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cnd_init(cond);
}
#endif

SPRT_UMBRELLA_FUNC
int cnd_signal(cnd_t *cond) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cnd_signal(cond);
}
#endif

SPRT_UMBRELLA_FUNC
int cnd_broadcast(cnd_t *cond) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cnd_broadcast(cond);
}
#endif

SPRT_UMBRELLA_FUNC
int cnd_wait(cnd_t *cond, mtx_t *mtx) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cnd_wait(cond, mtx);
}
#endif

SPRT_UMBRELLA_FUNC
int cnd_timedwait(cnd_t *__SPRT_RESTRICT cond, mtx_t *__SPRT_RESTRICT mtx,
		const struct __SPRT_TIMESPEC_NAME *__SPRT_RESTRICT ts) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cnd_timedwait(cond, mtx, ts);
}
#endif

SPRT_UMBRELLA_FUNC
void cnd_destroy(cnd_t *cond) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_cnd_destroy(cond);
}
#endif

// --- thread-specific storage ---

SPRT_UMBRELLA_FUNC
int tss_create(tss_t *key, tss_dtor_t dtor) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_tss_create(key, dtor);
}
#endif

SPRT_UMBRELLA_FUNC
void *tss_get(tss_t key) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_tss_get(key);
}
#endif

SPRT_UMBRELLA_FUNC
int tss_set(tss_t key, void *val) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_tss_set(key, val);
}
#endif

SPRT_UMBRELLA_FUNC
void tss_delete(tss_t key) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_tss_delete(key);
}
#endif

// --- one-time initialization ---

SPRT_UMBRELLA_FUNC
void call_once(once_flag *flag, void (*func)(void)) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_call_once(flag, func);
}
#endif

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_THREADS_H_
