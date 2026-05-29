/**
 Copyright (c) 2026 Xenolith Team Team <admin@xenolith.studio>

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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___MUTEX_TIMED_MUTEX_H_
#define RUNTIME_INCLUDE_SPRT_CXX___MUTEX_TIMED_MUTEX_H_

#include <sprt/runtime/thread/qmutex.h>

namespace sprt {

class timed_mutex {
public:
	using timeout_type = __sprt_sprt_timeout_t;
	using native_handle_type = qmutex_base::value_type;

	timed_mutex() { }
	~timed_mutex() { }

	timed_mutex(const timed_mutex &) = delete;
	timed_mutex &operator=(const timed_mutex &) = delete;

	void lock() { qmutex_base::_lock<__sprt_sprt_qlock_wait, nullptr>(&_mutex, 0, 0); }

	bool try_lock() { return qmutex_base::_try_lock(&_mutex) == Status::Ok; }

	bool try_lock_for(timeout_type rel_time) {
		return qmutex_base::_lock<__sprt_sprt_qlock_wait, __sprt_sprt_qlock_now>(&_mutex, &rel_time,
					   0)
				== Status::Ok;
	}

	void unlock() { qmutex_base::_unlock<__sprt_sprt_qlock_wake_one>(&_mutex, 0); }

	native_handle_type native_handle() const noexcept { return _mutex; }

protected:
	native_handle_type _mutex = 0;
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___MUTEX_TIMED_MUTEX_H_
