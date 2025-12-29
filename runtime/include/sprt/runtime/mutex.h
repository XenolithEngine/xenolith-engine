/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_MUTEX_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_MUTEX_H_

#include <sprt/runtime/int.h>
#include <sprt/c/__sprt_assert.h>

namespace sprt {

#if SPRT_LINUX
struct __qmutex_data_t {
	uint32_t value = 0;
};

struct _recursive_qmutex_data_t {
	uint32_t value = 0;
	uint32_t counter = 0;
};
#else
#error TODO
#endif

class qmutex final {
public:
	~qmutex();

	void lock();
	bool try_lock();

	// returns true if we have some waiters
	bool unlock();

protected:
	__qmutex_data_t _data;
};

class recursive_qmutex final {
public:
	~recursive_qmutex();

	void lock();
	bool try_lock();

	// returns true if we have some waiters
	bool unlock();

protected:
	_recursive_qmutex_data_t _data;
};


template <class _Mutex>
class unique_lock {
public:
	typedef _Mutex mutex_type;

private:
	mutex_type *__m_;
	bool __owns_;

public:
	[[nodiscard]]
	unique_lock() noexcept
	: __m_(nullptr), __owns_(false) { }

	[[nodiscard]]
	explicit unique_lock(mutex_type &__m)
	: __m_(addressof(__m)), __owns_(true) {
		__m_->lock();
	}

	~unique_lock() {
		if (__owns_) {
			__m_->unlock();
		}
	}

	unique_lock(unique_lock const &) = delete;
	unique_lock &operator=(unique_lock const &) = delete;

	[[nodiscard]]
	unique_lock(unique_lock &&__u) noexcept
	: __m_(__u.__m_), __owns_(__u.__owns_) {
		__u.__m_ = nullptr;
		__u.__owns_ = false;
	}

	unique_lock &operator=(unique_lock &&__u) noexcept {
		if (__owns_) {
			__m_->unlock();
		}

		__m_ = __u.__m_;
		__owns_ = __u.__owns_;
		__u.__m_ = nullptr;
		__u.__owns_ = false;
		return *this;
	}

	void lock() noexcept {
		sprt_passert(__m_ == nullptr, "unique_lock::lock: references null mutex");
		sprt_passert(!__owns_, "unique_lock::lock: already locked");

		__m_->lock();
		__owns_ = true;
	}

	bool try_lock() noexcept {
		sprt_passert(__m_ == nullptr, "unique_lock::lock: references null mutex");
		sprt_passert(!__owns_, "unique_lock::lock: already locked");

		__owns_ = __m_->try_lock();
		return __owns_;
	}

	void unlock() {
		sprt_passert(__owns_, "unique_lock::unlock: not locked");

		__m_->unlock();
		__owns_ = false;
	}

	mutex_type *release() noexcept {
		mutex_type *__m = __m_;
		__m_ = nullptr;
		__owns_ = false;
		return __m;
	}

	bool owns_lock() const noexcept { return __owns_; }
	explicit operator bool() const noexcept { return __owns_; }
	mutex_type *mutex() const noexcept { return __m_; }
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_MUTEX_H_
