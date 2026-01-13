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

#ifndef RUNTIME_SRC_PRIVATE_SPRTSPECIFIC_H_
#define RUNTIME_SRC_PRIVATE_SPRTSPECIFIC_H_

#include <sprt/runtime/int.h>

#if SPRT_LINUX || SPRT_ANDROID

#ifndef __SPRT_BUILD
__SPRT_C_FUNC long int syscall(long int __sysno, ...) noexcept;
#endif

#if defined(__x86_64__)

static constexpr int SYS_GETTID = 186;
static constexpr int SYS_FUTEX_V1 = 202;

#elif defined(__i386__)

static constexpr int SYS_GETTID = 224;
static constexpr int SYS_FUTEX_V1 = 422;

#elif defined(__aarch64__)

static constexpr int SYS_GETTID = 178;
static constexpr int SYS_FUTEX_V1 = 98;

#elif defined(__arm__)

static constexpr int SYS_GETTID = 224;
static constexpr int SYS_FUTEX_V1 = 422;

#elif defined(__riscv)

static constexpr int SYS_GETTID = 178;
static constexpr int SYS_FUTEX_V1 = 422;

#endif

static constexpr int SYS_FUTEX_V2_WAKE = 454;
static constexpr int SYS_FUTEX_V2_WAIT = 455;
static constexpr int SYS_EPOLL_PWAIT2 = 441;
static constexpr int SYS_IO_URING_SETUP = 425;
static constexpr int SYS_IO_URING_ENTER = 426;
static constexpr int SYS_IO_URING_REGISTER = 427;

namespace sprt {

struct _linux_timespec {
	int64_t tv_sec; /* seconds */
	int64_t tv_nsec; /* nanoseconds */
};

struct _linux_itimerspec {
	_linux_timespec it_interval; /* timer period */
	_linux_timespec it_value; /* timer expiration */
};

void setFutexVersion(int v);

} // namespace sprt

#endif

#if SPRT_ANDROID

namespace sprt::platform {

extern int (*_timespec_get)(struct timespec *__ts, int __base);
extern int (*_timespec_getres)(struct timespec *__ts, int __base);
extern int (*_getlogin_r)(char *__buffer, size_t __buffer_size);
extern ssize_t (*_copy_file_range)(int __fd_in, off_t *__off_in, int __fd_out, off_t *__off_out,
		size_t __length, unsigned int __flags);

extern int (*_futimes)(int __fd, const struct timeval __times[2]);
extern int (*_lutimes)(const char *__path, const struct timeval __times[2]);
extern int (*_futimesat)(int __dir_fd, const char *__path, const struct timeval __times[2]);

extern int (*_sync_file_range)(int __fd, off64_t __offset, off64_t __length, unsigned int __flags);

extern int (*_mlock2)(const void *__addr, size_t __size, int __flags);
extern int (*_memfd_create)(const char *__name, unsigned __flags);

extern char *(*_ctermid)(char *__buf);

extern int (*_getsubopt)(char **__option, char *const *__tokens, char **__value_ptr);

extern int (*_getentropy)(void *__buffer, size_t __buffer_size);

extern ssize_t (*_getrandom)(void *__buffer, size_t __buffer_size, unsigned int __flags);

} // namespace sprt::platform

#endif

#endif // RUNTIME_SRC_PRIVATE_SPRTSPECIFIC_H_
