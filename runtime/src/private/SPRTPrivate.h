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

#ifndef CORE_RUNTIME_PRIVATE_SPRTPRIVATE_H_
#define CORE_RUNTIME_PRIVATE_SPRTPRIVATE_H_

#include "SPRuntimeInt.h"

#if SPRT_ANDROID
#include <time.h>
#endif

namespace sprt::platform {

SPRT_LOCAL bool initialize(int &);
SPRT_LOCAL void terminate();

} // namespace sprt::platform

namespace sprt::backtrace {

SPRT_LOCAL void initialize();
SPRT_LOCAL void terminate();

} // namespace sprt::backtrace

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

} // namespace sprt::platform

#endif

#endif // CORE_RUNTIME_PRIVATE_SPRTPRIVATE_H_
