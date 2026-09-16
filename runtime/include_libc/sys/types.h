/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>
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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SYS_TYPES_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_TYPES_H_

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <sys/types.h>

#else

#include <sprt/c/__sprt_pthread.h>
#include <sprt/c/cross/__sprt_socket.h>
#include <sprt/c/cross/__sprt_signal.h>
#include <sprt/c/cross/__sprt_fstypes.h> // mode_t / nlink_t / ino_t / dev_t / blk*_t
#include <sprt/c/cross/__sprt_sysid.h> // uid_t / gid_t / pid_t
#include <sprt/c/bits/__sprt_size_t.h>
#include <sprt/c/bits/__sprt_time_t.h>
#include <sprt/c/bits/__sprt_ssize_t.h>
#include <sprt/c/bits/__sprt_time_t.h>
#include <sprt/c/cross/__sprt_fdset.h>

#include <inttypes.h>

typedef __SPRT_ID(size_t) size_t;
typedef __SPRT_ID(rsize_t) rsize_t;
typedef __SPRT_ID(off_t) off_t;
typedef __SPRT_ID(off_t) off64_t;
typedef __SPRT_ID(ssize_t) ssize_t;
typedef __SPRT_ID(time_t) time_t;
typedef __SPRT_ID(clock_t) clock_t;
typedef __SPRT_ID(clockid_t) clockid_t;

// POSIX id / filesystem types. <sys/types.h> is the canonical home for these;
// several headers (sys/stat.h, fcntl.h, ...) also re-typedef the ones they need,
// which is a legal identical redefinition in C/C++. The identifiers resolve to the
// same __sprt_* aliases from __sprt_fstypes.h / __sprt_sysid.h included above.
typedef __SPRT_ID(mode_t) mode_t;
typedef __SPRT_ID(nlink_t) nlink_t;
typedef __SPRT_ID(ino_t) ino_t;
typedef __SPRT_ID(ino_t) ino64_t;
typedef __SPRT_ID(dev_t) dev_t;
typedef __SPRT_ID(blksize_t) blksize_t;
typedef __SPRT_ID(blkcnt_t) blkcnt_t;
typedef __SPRT_ID(blkcnt_t) blkcnt64_t;
typedef __SPRT_ID(uid_t) uid_t;
typedef __SPRT_ID(gid_t) gid_t;
typedef __SPRT_ID(pid_t) pid_t;
typedef unsigned int id_t;
typedef __SPRT_ID(pthread_t) pthread_t;
typedef __SPRT_ID(pthread_once_t) pthread_once_t;
typedef __SPRT_ID(pthread_key_t) pthread_key_t;
typedef __SPRT_ID(pthread_spinlock_t) pthread_spinlock_t;
typedef __SPRT_ID(pthread_mutexattr_t) pthread_mutexattr_t;
typedef __SPRT_ID(pthread_cond_t) pthread_cond_t;
typedef __SPRT_ID(pthread_condattr_t) pthread_condattr_t;
typedef __SPRT_ID(pthread_rwlockattr_t) pthread_rwlockattr_t;
typedef __SPRT_ID(pthread_barrierattr_t) pthread_barrierattr_t;
typedef __SPRT_ID(pthread_mutex_t) pthread_mutex_t;
typedef __SPRT_ID(pthread_attr_t) pthread_attr_t;
typedef __SPRT_ID(pthread_rwlock_t) pthread_rwlock_t;
typedef __SPRT_ID(pthread_barrier_t) pthread_barrier_t;

typedef __SPRT_ID(fd_set) fd_set;

typedef __SPRT_ID(socklen_t) socklen_t;
typedef __SPRT_ID(sa_family_t) sa_family_t;

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYS_TYPES_H_
