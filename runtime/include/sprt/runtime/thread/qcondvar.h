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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_QCONDVAR_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_QCONDVAR_H_

#include <sprt/runtime/thread/qmutex.h>
#include <sprt/c/__sprt_sched.h>

namespace sprt {

struct alignas(8) __qcondvar_data {
	uint64_t mutexid = 0;
	qmutex_base::value_type counter = 0;
	qmutex_base::value_type value = 0;
	qmutex_base::value_type previous = 0;
	uint32_t padding = 0;
};

class SPRT_API qcondvar_base : public qmutex_base {
public:
	template < int (*WaitFn)(value_type *, value_type, timeout_type, flags_type),
			timeout_type (*ClockFn)(flags_type), uint64_t (*MutexId)(void *),
			Status (*LockFn)(void *), // mutex lock fn
			Status (*UnlockFn)(void *) // mutex unlock fn
			>
	static Status _wait(__qcondvar_data *data, void *mutex, timeout_type *timeout,
			flags_type flags) {
		uint64_t desired = MutexId(mutex);
		uint64_t expected = 0;
		if (__atomic_compare_exchange_n(&data->mutexid, &expected, desired, false, __ATOMIC_SEQ_CST,
					__ATOMIC_SEQ_CST)) {
			// condition captured by new mutex
			_atomic::fetchAdd(&data->counter, uint32_t(1));
		} else if (expected == desired) {
			// comdition was captured by this mutex
			_atomic::fetchAdd(&data->counter, uint32_t(1));
		} else if (__atomic_load_n(&data->counter, __ATOMIC_SEQ_CST) == 0
				&& __atomic_compare_exchange_n(&data->mutexid, &expected, desired, false,
						__ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
			// The binding is a leftover from a fully-departed waiter generation (the
			// epilogue no longer resets mutexid — see below); with no active waiters
			// the condvar may be re-bound to a new mutex.
			_atomic::fetchAdd(&data->counter, uint32_t(1));
		} else {
			// captured by different mutex with active waiters
			return Status::ErrorInvalidArguemnt;
		}

		value_type v = __atomic_load_n(&data->value, __ATOMIC_SEQ_CST);
		__atomic_store_n(&data->previous, v, __ATOMIC_SEQ_CST);

		Status result = Status::Ok;

		timeout_type now = 0, next = 0;
		if constexpr (ClockFn != nullptr) {
			if (timeout) {
				now = ClockFn(flags);
			}
		}

		UnlockFn(mutex);
		while (v == __atomic_load_n(&data->value, __ATOMIC_SEQ_CST)) {
			if (timeout && *timeout == 0) {
				result = Status::Timeout;
				break;
			}

			int r = WaitFn(&data->value, v, timeout ? *timeout : __SPRT_SPRT_TIMEOUT_INFINITE,
					flags);
			if (r != 0) {
				if (__sprt_errno == ETIMEDOUT) {
					result = Status::Timeout;
					break;
				}
			}

			if constexpr (ClockFn != nullptr) {
				if (timeout) {
					next = ClockFn(flags);
					*timeout -= min((next - now), *timeout);
					now = next;
				}
			}
		}

		// LAST touch of the condvar object, BEFORE re-acquiring the user mutex: the
		// notifier is allowed to destroy the condvar while still holding that mutex
		// (see _destroy), so nothing below may dereference `data` — and _destroy's
		// spin on `counter` is what keeps the object alive for the loop re-reads
		// above. mutexid is deliberately NOT reset here (it would race with a new
		// waiter's registration once we no longer hold the user mutex); a stale
		// binding is re-claimed in the registration path when counter == 0.
		_atomic::fetchSub(&data->counter, 1U);

		// The mutex must be re-acquired before returning, even on timeout.
		auto lockStatus = LockFn(mutex);
		// Preserve a timeout from the wait loop (it would otherwise be lost behind the
		// relock status, making pthread_cond_timedwait report success on timeout).
		// Only surface a relock failure when the wait itself succeeded.
		if (result == Status::Timeout) {
			return result;
		}
		return lockStatus;
	}

	// Contract (as for pthread_cond_signal/broadcast): the caller must hold the
	// associated mutex while signalling. _wait registers `mutexid` and snapshots
	// `previous` while still holding that mutex (it only unlocks at UnlockFn()
	// below), so a signal issued under the same mutex always observes a waiter
	// that has fully published its wait state - there is no lost-wakeup window.
	// Calling _signal without the mutex held breaks that guarantee.
	// Destruction barrier ([thread.condition]/pthread_cond_destroy): a condvar may
	// be destroyed as soon as every waiter has been NOTIFIED — but a notified waiter
	// may still be between the futex wake and its counter decrement (_wait's last
	// touch of the object, done BEFORE re-acquiring the user mutex — the notifier
	// may hold that mutex while destroying, so the barrier must not depend on the
	// relock). Spinning until counter reads zero keeps the memory alive for those
	// waiters' loop re-reads. A cv destroyed with un-notified waiters still inside
	// the futex wait is UB by the standard and spins here forever (loudly, rather
	// than corrupting freed memory silently).
	static void _destroy(__qcondvar_data *data) {
		while (__atomic_load_n(&data->counter, __ATOMIC_SEQ_CST) != 0) {
			__sprt_sched_yield();
		}
	}

	template <int (*WakeFn)(value_type *, flags_type)>
	static Status _signal(__qcondvar_data *data, flags_type flags) {
		// counter, not mutexid: the epilogue no longer clears mutexid, so a stale
		// binding with zero waiters must still take the fast no-op path. A waiter
		// publishes counter (fetchAdd) while holding the mutex the signaller holds
		// now, so an active waiter is always visible here.
		if (__atomic_load_n(&data->counter, __ATOMIC_SEQ_CST) == 0) {
			// no waiters
			return Status::Ok;
		}

		value_type v = 1u + __atomic_load_n(&data->previous, __ATOMIC_SEQ_CST);
		__atomic_store_n(&data->value, v, __ATOMIC_SEQ_CST);
		if (WakeFn(&data->value, flags) != 0) {
			return status::errnoToStatus(__sprt_errno);
		}
		return Status::Ok;
	}
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_QCONDVAR_H_
