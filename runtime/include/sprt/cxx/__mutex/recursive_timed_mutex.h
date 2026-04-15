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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___MUTEX_RECURSIVE_TIMED_MUTEX_H_
#define RUNTIME_INCLUDE_SPRT_CXX___MUTEX_RECURSIVE_TIMED_MUTEX_H_

#include <sprt/runtime/thread/rmutex.h>

namespace sprt {

class recursive_timed_mutex {
public:
	using timeout_type = __sprt_sprt_timeout_t;
	using native_handle_type = __rmutex_data;

	~recursive_timed_mutex();

	recursive_timed_mutex() { }

	recursive_timed_mutex(const recursive_timed_mutex &) = delete;
	recursive_timed_mutex &operator=(const recursive_timed_mutex &) = delete;

	void lock(); // blocking
	bool try_lock() noexcept;

	bool try_lock_for(const timeout_type &rel_time);

	void unlock();

	native_handle_type native_handle() const noexcept { return _mutex; }

protected:
	native_handle_type _mutex;
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___MUTEX_RECURSIVE_TIMED_MUTEX_H_
