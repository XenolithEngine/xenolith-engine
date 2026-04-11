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

__SPRT_C_FUNC int __SPRT_ID(pthread_attr_init)(__SPRT_ID(pthread_attr_t) * attr) {
	new (reinterpret_cast<attr_t *>(attr), sprt::nothrow) attr_t;
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_attr_destroy)(__SPRT_ID(pthread_attr_t) * attr) {
	if (!attr) {
		return EINVAL;
	}

	reinterpret_cast<attr_t *>(attr)->~attr_t();
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_attr_getguardsize)(const __SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT attr,
		__SPRT_ID(size_t) * __SPRT_RESTRICT ret) {
	if (!attr || !ret) {
		return EINVAL;
	}

	*ret = reinterpret_cast<const attr_t *>(attr)->guardSize;
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_attr_setguardsize)(__SPRT_ID(pthread_attr_t) * attr, __SPRT_ID(size_t) v) {
	if (!attr || v >= Max<uint32_t> || !native::validate_attr_setguardsize(v)) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<attr_t *>(attr);
	tattr->guardSize = static_cast<uint32_t>(v);
	tattr->attr |= ThreadAttrFlags::GuardSizeCustomized;
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_attr_getstacksize)(const __SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT attr,
		__SPRT_ID(size_t) * __SPRT_RESTRICT ret) {
	if (!ret || !attr) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<const attr_t *>(attr);
	*ret = tattr->stackSize;
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_attr_setstacksize)(__SPRT_ID(pthread_attr_t) * attr, __SPRT_ID(size_t) v) {
	if (!attr || v >= Max<uint32_t> || v < __SPRT_PTHREAD_STACK_MIN
			|| !native::validate_attr_setstacksize(v)) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<attr_t *>(attr);
	tattr->stackSize = static_cast<uint32_t>(v);
	tattr->attr |= ThreadAttrFlags::StackSizeCustomized;
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_attr_getdetachstate)(const __SPRT_ID(pthread_attr_t) * attr, int *ret) {
	if (!ret || !attr) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<const attr_t *>(attr);
	if (hasFlag(tattr->attr, ThreadAttrFlags::Detached)) {
		*ret = __SPRT_PTHREAD_CREATE_DETACHED;
	} else {
		*ret = __SPRT_PTHREAD_CREATE_JOINABLE;
	}

	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_attr_setdetachstate)(__SPRT_ID(pthread_attr_t) * attr, int v) {
	if (!attr) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<attr_t *>(attr);
	switch (v) {
	case __SPRT_PTHREAD_CREATE_JOINABLE: tattr->attr &= ~ThreadAttrFlags::Detached; break;
	case __SPRT_PTHREAD_CREATE_DETACHED: tattr->attr |= ThreadAttrFlags::Detached; break;
	default: return EINVAL;
	}
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_attr_getstack)(const __SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT attr,
		void **__SPRT_RESTRICT ret, __SPRT_ID(size_t) * __SPRT_RESTRICT sz) {
	if (!attr) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<const attr_t *>(attr);
	if (ret) {
		*ret = tattr->stack;
	}
	if (sz) {
		*sz = tattr->stackSize;
	}
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_attr_setstack)(__SPRT_ID(pthread_attr_t) * attr, void *ptr, __SPRT_ID(size_t) sz) {
	if (!attr || !ptr || sz < __SPRT_PTHREAD_STACK_MIN
			|| !native::validate_attr_setstack(ptr, sz)) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<attr_t *>(attr);
	tattr->stack = ptr;
	tattr->stackSize = sz;
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_attr_getscope)(
		const __SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	if (!ret || !attr) {
		return EINVAL;
	}

	*ret = __SPRT_PTHREAD_SCOPE_SYSTEM;
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_attr_setscope)(__SPRT_ID(pthread_attr_t) * attr, int v) {
	if (!attr) {
		return EINVAL;
	}

	if (v == __SPRT_PTHREAD_SCOPE_SYSTEM) {
		return 0;
	} else {
		return ENOTSUP;
	}
}

__SPRT_C_FUNC int __SPRT_ID(pthread_attr_getschedpolicy)(
		const __SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	if (!ret || !attr) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<const attr_t *>(attr);
	auto policy = tattr->attr & ThreadAttrFlags::PrioMask;
	switch (policy) {
	case ThreadAttrFlags::PrioFifo: *ret = __SPRT_SCHED_FIFO; break;
	case ThreadAttrFlags::PrioRR: *ret = __SPRT_SCHED_RR; break;
	default: *ret = __SPRT_SCHED_OTHER; break;
	}
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_attr_setschedpolicy)(__SPRT_ID(pthread_attr_t) * attr, int v) {
	if (!attr) {
		return EINVAL;
	}

	if (!native::validate_attr_setschedpolicy(v)) {
		return ENOTSUP;
	}

	auto tattr = reinterpret_cast<attr_t *>(attr);
	switch (v) {
	case __SPRT_SCHED_OTHER: tattr->attr &= ~ThreadAttrFlags::PrioMask; break;
	case __SPRT_SCHED_FIFO:
		tattr->attr &= ~ThreadAttrFlags::PrioMask;
		tattr->attr |= ThreadAttrFlags::PrioFifo;
		break;
	case __SPRT_SCHED_RR:
		tattr->attr &= ~ThreadAttrFlags::PrioMask;
		tattr->attr |= ThreadAttrFlags::PrioRR;
		break;
	default: return ENOTSUP;
	}

	auto pool = __thread_pool::get();
	if (!pool->isPrioValid(tattr->attr, tattr->prio)) {
		tattr->prio = 0;
	}
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_attr_getschedparam)(const __SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT attr,
		struct __SPRT_ID(sched_param) * __SPRT_RESTRICT ret) {
	if (!ret || !attr) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<const attr_t *>(attr);
	ret->sched_priority = static_cast<int>(tattr->prio);
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_attr_setschedparam)(__SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT attr,
		const struct __SPRT_ID(sched_param) * __SPRT_RESTRICT val) {
	if (!val || !attr) {
		return EINVAL;
	}

	if (!native::validate_attr_setschedparam(val->sched_priority)) {
		return ENOTSUP;
	}

	auto pool = __thread_pool::get();
	auto tattr = reinterpret_cast<attr_t *>(attr);
	if (!pool->isPrioValid(tattr->attr, val->sched_priority)) {
		return EINVAL;
	}
	tattr->prio = val->sched_priority;
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_attr_getinheritsched)(
		const __SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	if (!ret || !attr) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<const attr_t *>(attr);
	if (hasFlag(tattr->attr, ThreadAttrFlags::ExplicitSched)) {
		*ret = __SPRT_PTHREAD_EXPLICIT_SCHED;
	} else {
		*ret = __SPRT_PTHREAD_INHERIT_SCHED;
	}
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_attr_setinheritsched)(__SPRT_ID(pthread_attr_t) * attr, int v) {
	if (!attr) {
		return EINVAL;
	}

	if (!native::validate_attr_setinheritsched(v)) {
		return ENOTSUP;
	}

	auto tattr = reinterpret_cast<attr_t *>(attr);
	switch (v) {
	case __SPRT_PTHREAD_INHERIT_SCHED: tattr->attr &= ~ThreadAttrFlags::ExplicitSched; break;
	case __SPRT_PTHREAD_EXPLICIT_SCHED: tattr->attr |= ThreadAttrFlags::ExplicitSched; break;
	default: return ENOTSUP;
	}
	return 0;
}

} // namespace sprt::_thread
