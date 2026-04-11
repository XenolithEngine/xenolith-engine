/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_QBARRIER_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_QBARRIER_H_

#include <sprt/runtime/thread/qmutex.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_sched.h>

namespace sprt {

class qbarrier_base {
public:
	using value_type = __sprt_sprt_qlock_t;
	using timeout_type = __sprt_sprt_timeout_t;
	using flags_type = __sprt_sprt_lock_flags_t;

	static constexpr value_type ValueMask = 0x7FFF'FFFF;
	static constexpr value_type PassFlag = 0x8000'0000;

	struct barrier_data {
		value_type nthreads = 0; // number of threads in single exit batch
		value_type nIn = 0; // number of input threads
		value_type nOut = 0; // number of outgoing threads
		value_type generations = 0; // counter to drop nOut only single time per batch
	};

	/*
		Returns:
		- Status::Ok - normal return
		- Status::Done - PTHREAD_BARRIER_SERIAL_THREAD
	*/
	template <
			// Function to wait on lock
			int (*WaitFn)(value_type *, value_type, timeout_type, flags_type),

			// Function to wake single waiter
			int (*WakeFn)(value_type *, flags_type)>
	static Status _wait(barrier_data *__data, flags_type flags) {
		uint32_t expected = __data->nthreads & ValueMask;
		uint32_t gen = _atomic::loadSeq(&__data->generations);
		auto ticket = _atomic::fetchAdd(&__data->nIn, uint32_t(1));

		if ((ticket % (expected)) == expected - 1) {
			// golden ticket - we can set PassFlag and signal others
			// but only if PassFlag is NOT set

			// decrement In value to exclude overflow
			_atomic::fetchSub(&__data->nIn, expected);

			// we expect that flag is set for our WaitOnAddress
			expected |= PassFlag;

			auto v = _atomic::fetchOr(&__data->nthreads, PassFlag);
			while ((v & PassFlag) != 0) {
				// flag was set - waiting
				// this can happen, if some other thread with golden ticket set flag before us
				// in this case, we wait until the last thread in group will wake us
				auto st = WaitFn(&__data->nthreads, expected, __SPRT_SPRT_TIMEOUT_INFINITE, flags);
				if (st != 0 && __sprt_errno != EAGAIN) {
					return status::errnoToStatus(__sprt_errno);
				}

				// try to set flag again
				v = _atomic::fetchOr(&__data->nthreads, PassFlag);
			}

			// clear Out counter
			_atomic::storeSeq(&__data->nOut, uint32_t(0));
			_atomic::fetchAdd(&__data->generations, uint32_t(1));

			WakeFn(&__data->nthreads, flags);
			return Status::Done;
		} else {
			do {
				// wait until PassFlag is set
				do {
					auto st = WaitFn(&__data->nthreads, expected, __SPRT_SPRT_TIMEOUT_INFINITE,
							flags);
					if (st != 0 && __sprt_errno != EAGAIN) {
						return status::errnoToStatus(__sprt_errno);
					}
				} while (_atomic::loadSeq(&__data->nthreads) == expected);

				uint32_t newGen;
				// spinlock for generation increment
				// if generation was incremented - Out was cleared
				while ((newGen = _atomic::loadSeq(&__data->generations)) == gen) {
					__sprt_sched_yield();
				}
				gen = newGen;

				// pass flag was set
				// last exiting thread (expected - 2 thread) should remove flag; others ( > expected - 2) should wait again

				auto exitTicket = _atomic::fetchAdd(&__data->nOut, uint32_t(1));
				if (exitTicket < expected - 2) {
					return Status::Ok;
				} else if (exitTicket == expected - 2) {
					// black ticket - remove PassFlag
					_atomic::fetchAnd(&__data->nthreads, ValueMask);

					WakeFn(&__data->nthreads, flags);

					return Status::Ok;
				} else {
					// we fail to exit
				}
			} while (1);
		}

		__builtin_unreachable();
		return Status::Ok;
	}
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_QBARRIER_H_
