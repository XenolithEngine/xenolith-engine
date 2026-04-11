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
#include <sprt/cxx/cstring>

namespace sprt::_thread {

int thread_t::setname(const char *name) {
	auto nameLen = strlen(name);
	if (nameLen > threadName.size() - 1) {
		return ERANGE;
	}

	unique_lock lock(mutex);
	memcpy(threadName.data(), name, nameLen + 1);

	setname_native(name);

	return 0;
}

int thread_t::getname(char *buf, size_t bufLen) {
	unique_lock lock(mutex);

	auto nameLen = strlen(threadName.data());
	if (bufLen < nameLen) {
		return ERANGE;
	}

	if (nameLen == 0) {
		buf[0] = 0;
	} else {
		memcpy(buf, threadName.data(), nameLen + 1);
	}
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_getaffinity_np)(__SPRT_ID(pthread_t) thread,
		__SPRT_ID(size_t) n, __SPRT_ID(cpu_set_t) * set) {
#if __SPRT_CONFIG_HAVE_PTHREAD_AFFINITY
	if (!thread || !set || n == 0) {
		return EINVAL;
	}

	return reinterpret_cast<_thread::thread_t *>(thread)->getaffinity(n, set);
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_PTHREAD_AFFINITY)");
	return EINVAL;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(pthread_setaffinity_np)(__SPRT_ID(pthread_t) thread,
		__SPRT_ID(size_t) n, const __SPRT_ID(cpu_set_t) * set) {
#if __SPRT_CONFIG_HAVE_PTHREAD_AFFINITY
	if (!thread || !set || n == 0) {
		return EINVAL;
	}

	return reinterpret_cast<_thread::thread_t *>(thread)->setaffinity(n, set);
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_PTHREAD_AFFINITY)");
	return EINVAL;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_getattr_np)(__SPRT_ID(pthread_t) thread, __SPRT_ID(pthread_attr_t) * attr) {
	if (!thread || !attr) {
		return EINVAL;
	}

	auto tthread = reinterpret_cast<_thread::thread_t *>(thread);

	__sprt_memcpy(attr, &tthread->attr, sizeof(attr_t));

	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_setname_np)(__SPRT_ID(pthread_t) thread, const char *name) {
	if (!thread) {
		return EINVAL;
	}

	return reinterpret_cast<_thread::thread_t *>(thread)->setname(name);
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_getname_np)(__SPRT_ID(pthread_t) thread, char *buf, __SPRT_ID(size_t) len) {
	if (!thread || !buf) {
		return EINVAL;
	}

	return reinterpret_cast<_thread::thread_t *>(thread)->getname(buf, len);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_getattr_default_np)(__SPRT_ID(pthread_attr_t) * attr) {
	if (!attr) {
		return EINVAL;
	}

	auto pool = __thread_pool::get();

	unique_lock lock(pool->mutex);
	*reinterpret_cast<attr_t *>(attr) = pool->defaultAttr;
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_setattr_default_np)(const __SPRT_ID(pthread_attr_t) * attr) {
	if (!attr) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<const attr_t *>(attr);
	if (hasFlag(tattr->attr, ThreadAttrFlags::StackPointerCustomized)) {
		return EINVAL;
	}

	auto pool = __thread_pool::get();

	unique_lock lock(pool->mutex);
	pool->defaultAttr = *reinterpret_cast<const attr_t *>(attr);
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_tryjoin_np)(__SPRT_ID(pthread_t) thread, void **ret) {
	return __pthread_join(reinterpret_cast<thread_t *>(thread), ret, Infinite, true);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_timedjoin_np)(__SPRT_ID(pthread_t) thread, void **ret,
		const __SPRT_TIMESPEC_NAME *tv) {
	if (!thread || !tv || tv->tv_nsec < 0 || tv->tv_nsec >= 1'000'000'000) {
		return EINVAL;
	}

	__SPRT_TIMESPEC_NAME curTv;
	__sprt_clock_gettime(__SPRT_CLOCK_REALTIME, &curTv);

	auto diffTv = __sprt_timespec_diff(tv, &curTv);

	if (diffTv.tv_sec < 0) {
		return ETIMEDOUT;
	}

	return __pthread_join(reinterpret_cast<thread_t *>(thread), ret,
			diffTv.tv_nsec + diffTv.tv_sec * 1'000'000'000, false);
}

} // namespace sprt::_thread
