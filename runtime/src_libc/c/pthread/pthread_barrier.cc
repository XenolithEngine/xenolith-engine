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

#define __SPRT_BUILD 1

#include "pthread_native.h"

namespace sprt::_thread {

int barrier_t::wait() {
	__sprt_sprt_lock_flags_t fl = 0;
	if (hasFlag(flags, BarrierAttrFlags::Shared)) {
		fl |= __SPRT_SPRT_LOCK_FLAG_SHARED;
	}

	auto st = qbarrier_base::_wait<__sprt_sprt_qlock_wait, __sprt_sprt_qlock_wake_all>(&data, fl);
	switch (st) {
	case Status::Ok: return 0; break;
	case Status::Done: return __SPRT_PTHREAD_BARRIER_SERIAL_THREAD; break;
	default: return status::toErrno(st); break;
	}
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_barrierattr_init)(__SPRT_ID(pthread_barrierattr_t) * attr) {
	if (!attr) {
		return EINVAL;
	}

	new (reinterpret_cast<_thread::barrierattr_t *>(attr), nothrow) _thread::barrierattr_t();
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_barrierattr_destroy)(__SPRT_ID(pthread_barrierattr_t) * attr) {
	if (!attr) {
		return EINVAL;
	}

	reinterpret_cast<_thread::barrierattr_t *>(attr)->~barrierattr_t();
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_barrierattr_setpshared)(__SPRT_ID(pthread_barrierattr_t) * attr, int v) {
	if (!attr || !_thread::native::validate_barrierattr_setpshared(v)) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<_thread::barrierattr_t *>(attr);

	switch (v) {
	case __SPRT_PTHREAD_PROCESS_PRIVATE: tattr->flags &= ~_thread::BarrierAttrFlags::Shared; break;
	case __SPRT_PTHREAD_PROCESS_SHARED: tattr->flags |= _thread::BarrierAttrFlags::Shared; break;
	default: return ENOTSUP; break;
	}

	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_barrierattr_getpshared)(
		const __SPRT_ID(pthread_barrierattr_t) * __SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	if (!attr || !ret) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<const _thread::barrierattr_t *>(attr);
	if (hasFlag(tattr->flags, _thread::BarrierAttrFlags::Shared)) {
		*ret = __SPRT_PTHREAD_PROCESS_SHARED;
	} else {
		*ret = __SPRT_PTHREAD_PROCESS_PRIVATE;
	}
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_barrier_init)(__SPRT_ID(pthread_barrier_t) * __SPRT_RESTRICT bar,
		const __SPRT_ID(pthread_barrierattr_t) * __SPRT_RESTRICT attr, unsigned v) {
	if (!bar || v == 0) {
		return EINVAL;
	}

	auto tbar = new (reinterpret_cast<_thread::barrier_t *>(bar), nothrow) barrier_t{{v}};
	if (attr) {
		tbar->flags = reinterpret_cast<const _thread::barrier_t *>(attr)->flags;
	}
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_barrier_destroy)(__SPRT_ID(pthread_barrier_t) * bar) {
	if (!bar) {
		return EINVAL;
	}

	auto tbar = reinterpret_cast<_thread::barrier_t *>(bar);

	if (_atomic::loadSeq(&tbar->data.nOut) != 0 || _atomic::loadSeq(&tbar->data.nIn) != 0
			|| (_atomic::loadSeq(&tbar->data.nthreads) & barrier_t::PassFlag)) {
		return EBUSY;
	}

	tbar->~barrier_t();
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_barrier_wait)(__SPRT_ID(pthread_barrier_t) * bar) {
	if (!bar) {
		return EINVAL;
	}

	return reinterpret_cast<_thread::barrier_t *>(bar)->wait();
}

} // namespace sprt::_thread
