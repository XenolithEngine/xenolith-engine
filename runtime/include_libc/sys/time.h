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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SYS_TIME_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_TIME_H_

#ifdef __SPRT_BUILD

#include_next <sys/time.h>

#else

#include <c/sys/__sprt_time.h>

__SPRT_BEGIN_DECL

SPRT_FORCEINLINE inline int gettimeofday(struct timeval *__SPRT_RESTRICT __tv,
		struct timezone *__SPRT_RESTRICT __tz) {
	return __sprt_gettimeofday(__tv, __tz);
}

SPRT_FORCEINLINE inline int settimeofday(const timeval *__tv, const struct timezone *__tz) {
	return __sprt_settimeofday(__tv, __tz);
}

SPRT_FORCEINLINE inline int getitimer(int __w, struct itimerval *__tv) {
	return __sprt_getitimer(__w, __tv);
}

SPRT_FORCEINLINE inline int setitimer(int __w, const struct itimerval *__SPRT_RESTRICT __tv,
		struct itimerval *__SPRT_RESTRICT __atv) {
	return __sprt_setitimer(__w, __tv, __atv);
}

SPRT_FORCEINLINE inline int utimes(const char *path, const __SPRT_TIMEVAL_NAME ts[2]) {
	return __sprt_utimes(path, ts);
}

SPRT_FORCEINLINE inline int futimes(int fd, const __SPRT_TIMEVAL_NAME ts[2]) {
	return __sprt_futimes(fd, ts);
}

SPRT_FORCEINLINE inline int futimesat(int fd, const char *path, const __SPRT_TIMEVAL_NAME ts[2]) {
	return __sprt_futimesat(fd, path, ts);
}

SPRT_FORCEINLINE inline int lutimes(const char *path, const __SPRT_TIMEVAL_NAME ts[2]) {
	return __sprt_lutimes(path, ts);
}

#if __SPRT_CONFIG_HAVE_ADJTIME || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_FORCEINLINE inline int adjtime(const __SPRT_TIMEVAL_NAME *__tv, __SPRT_TIMEVAL_NAME *__otv) {
	return __sprt_adjtime(__tv, __otv);
}
#endif

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYS_TIME_H_
