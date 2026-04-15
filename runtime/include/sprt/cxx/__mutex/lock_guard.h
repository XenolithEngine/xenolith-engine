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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___MUTEX_LOCK_GUARD_H_
#define RUNTIME_INCLUDE_SPRT_CXX___MUTEX_LOCK_GUARD_H_

#include <sprt/c/__sprt_assert.h>

namespace sprt {

struct defer_lock_t {
	explicit defer_lock_t() = default;
};

struct try_to_lock_t {
	explicit try_to_lock_t() = default;
};

struct adopt_lock_t {
	explicit adopt_lock_t() = default;
};

inline constexpr defer_lock_t defer_lock = defer_lock_t();
inline constexpr try_to_lock_t try_to_lock = try_to_lock_t();
inline constexpr adopt_lock_t adopt_lock = adopt_lock_t();

template <typename _Mutex>
class lock_guard {
public:
	typedef _Mutex mutex_type;

private:
	mutex_type &__m_;

public:
	[[nodiscard]]
	explicit lock_guard(mutex_type &__m)
	: __m_(__m) {
		__m_.lock();
	}

	[[nodiscard]]
	lock_guard(mutex_type &__m, adopt_lock_t)
	: __m_(__m) { }
	~lock_guard() { __m_.unlock(); }

	lock_guard(lock_guard const &) = delete;
	lock_guard &operator=(lock_guard const &) = delete;
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___MUTEX_LOCK_GUARD_H_
