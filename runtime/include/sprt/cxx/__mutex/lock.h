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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___MUTEX_LOCK_H_
#define RUNTIME_INCLUDE_SPRT_CXX___MUTEX_LOCK_H_

// [thread.lock.algorithm] std::lock / std::try_lock and [thread.lock.scoped]
// std::scoped_lock. Implemented over the raw Lockable requirements
// (lock()/try_lock()/unlock()) so they work with any of sprt's mutex flavours.

#include <sprt/cxx/__mutex/lock_guard.h> // adopt_lock_t and friends
#include <sprt/cxx/tuple>
#include <sprt/c/__sprt_sched.h> // __sprt_sched_yield for the lock() back-off

namespace sprt {
inline namespace __cxx_mutex {

// ---- try_lock ---------------------------------------------------------------
// Returns -1 when every lock was acquired; otherwise the 0-based index of the
// first lock that could not be acquired (having released any it already took).
template <typename _L0, typename _L1>
int try_lock(_L0 &__l0, _L1 &__l1) {
	if (!__l0.try_lock()) {
		return 0;
	}
	if (!__l1.try_lock()) {
		__l0.unlock();
		return 1;
	}
	return -1;
}

template <typename _L0, typename _L1, typename _L2, typename... _L3>
int try_lock(_L0 &__l0, _L1 &__l1, _L2 &__l2, _L3 &...__l3) {
	if (!__l0.try_lock()) {
		return 0;
	}
	int __r = sprt::try_lock(__l1, __l2, __l3...);
	if (__r == -1) {
		return -1;
	}
	__l0.unlock();
	return __r + 1;
}

// ---- lock -------------------------------------------------------------------
// Deadlock-avoiding acquisition of all the given lockables ([thread.lock.algorithm]).
template <typename _L0, typename _L1>
void lock(_L0 &__l0, _L1 &__l1) {
	while (true) {
		__l0.lock();
		if (__l1.try_lock()) {
			return; // both held
		}
		__l0.unlock();
		__SPRT_ID(sched_yield)();
		__l1.lock();
		if (__l0.try_lock()) {
			return; // both held
		}
		__l1.unlock();
		__SPRT_ID(sched_yield)();
	}
}

template <typename _L0, typename _L1, typename _L2, typename... _L3>
void __lock_first(int __i, _L0 &__l0, _L1 &__l1, _L2 &__l2, _L3 &...__l3) {
	// Mirrors libc++'s __lock_first: lock the current "first" lockable, try_lock
	// the rest; on failure rotate so a different lockable leads, yielding between
	// attempts. Guaranteed to make progress and never deadlock.
	while (true) {
		switch (__i) {
		case 0: {
			__l0.lock();
			__i = sprt::try_lock(__l1, __l2, __l3...);
			if (__i == -1) {
				return; // __l0 + the rest are all held
			}
			__l0.unlock();
		}
			++__i;
			__SPRT_ID(sched_yield)();
			break;
		case 1: {
			__l1.lock();
			__i = sprt::try_lock(__l2, __l3..., __l0);
			if (__i == -1) {
				return; // __l1 + the rest are all held
			}
			__l1.unlock();
		}
			if (__i == static_cast<int>(sizeof...(_L3)) + 1) {
				__i = 0;
			} else {
				__i += 2;
			}
			__SPRT_ID(sched_yield)();
			break;
		default: sprt::__lock_first(__i - 2, __l2, __l3..., __l0, __l1); return;
		}
	}
}

template <typename _L0, typename _L1, typename _L2, typename... _L3>
void lock(_L0 &__l0, _L1 &__l1, _L2 &__l2, _L3 &...__l3) {
	sprt::__lock_first(0, __l0, __l1, __l2, __l3...);
}

// ---- scoped_lock ------------------------------------------------------------
template <typename... _MutexTypes>
class scoped_lock;

// zero mutexes: does nothing
template <>
class scoped_lock<> {
public:
	explicit scoped_lock() = default;
	explicit scoped_lock(adopt_lock_t) noexcept { }
	~scoped_lock() = default;

	scoped_lock(const scoped_lock &) = delete;
	scoped_lock &operator=(const scoped_lock &) = delete;
};

// one mutex: behaves like lock_guard
template <typename _Mutex>
class scoped_lock<_Mutex> {
public:
	typedef _Mutex mutex_type;

	explicit scoped_lock(mutex_type &__m) : __m_(__m) { __m_.lock(); }
	explicit scoped_lock(adopt_lock_t, mutex_type &__m) noexcept : __m_(__m) { }
	~scoped_lock() { __m_.unlock(); }

	scoped_lock(const scoped_lock &) = delete;
	scoped_lock &operator=(const scoped_lock &) = delete;

private:
	mutex_type &__m_;
};

// two or more mutexes: acquired together via sprt::lock (deadlock-free)
template <typename... _MutexTypes>
class scoped_lock {
public:
	explicit scoped_lock(_MutexTypes &...__m) : __t_(__m...) { sprt::lock(__m...); }
	explicit scoped_lock(adopt_lock_t, _MutexTypes &...__m) noexcept : __t_(__m...) { }
	~scoped_lock() {
		sprt::apply([](_MutexTypes &...__m) { (__m.unlock(), ...); }, __t_);
	}

	scoped_lock(const scoped_lock &) = delete;
	scoped_lock &operator=(const scoped_lock &) = delete;

private:
	sprt::tuple<_MutexTypes &...> __t_;
};

} // namespace __cxx_mutex
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___MUTEX_LOCK_H_
