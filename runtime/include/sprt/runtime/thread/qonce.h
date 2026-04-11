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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_QONCE_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_QONCE_H_

#include <sprt/runtime/thread/qmutex.h>

namespace sprt {

class qonce final {
public:
	using value_type = __sprt_sprt_qlock_t;
	using flags_type = __sprt_sprt_lock_flags_t;

	template <typename Callback>
	static int perform(value_type *value, const Callback &cb, flags_type f = 0) {
		auto val = _atomic::fetchOr(value, qmutex_base::LOCK_BIT);
		if (val == 0) {
			// The First One
			cb();

			// set complete flag and check for a waiters
			val = _atomic::fetchOr(value, qmutex_base::COMPLETE_BIT);
			if (val & qmutex_base::WAIT_BIT) {
				// wake waiters if any
				return __sprt_sprt_qlock_wake_all(value, f);
			}
		} else if (val & qmutex_base::COMPLETE_BIT) {
			// already complete, return
			return 0;
		} else {
			// set flag to notify The First One that we are waiting for him
			val = _atomic::fetchOr(value, qmutex_base::WAIT_BIT);
			while ((val & qmutex_base::COMPLETE_BIT) == 0) {
				__sprt_sprt_qlock_wait(value, val, __SPRT_SPRT_TIMEOUT_INFINITE, f);
				val = _atomic::loadSeq(value);
			}
		}
		return 0;
	}

	qonce() : _data(0) { }
	~qonce() { }

	template <typename Callback>
	void operator()(const Callback &cb) {
		perform(&_data, cb);
	}

	bool is_set() const {
		auto val = _atomic::loadSeq(&_data);
		return val & qmutex_base::COMPLETE_BIT;
	}

protected:
	value_type _data;
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_QONCE_H_
