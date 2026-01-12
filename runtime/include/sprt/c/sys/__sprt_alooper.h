/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef CORE_RUNTIME_INCLUDE_C_SYS___SPRT_ALOOPER_H_
#define CORE_RUNTIME_INCLUDE_C_SYS___SPRT_ALOOPER_H_

#include <sprt/c/cross/__sprt_config.h>

typedef struct ALooper __SPRT_ID(ALooper);

#define __SPRT_ALOOPER_PREPARE_ALLOW_NON_CALLBACKS (1<<0)
#define __SPRT_ALOOPER_POLL_WAKE (-1)
#define __SPRT_ALOOPER_POLL_CALLBACK (-2)
#define __SPRT_ALOOPER_POLL_TIMEOUT (-3)
#define __SPRT_ALOOPER_POLL_ERROR (-4)

#define __SPRT_ALOOPER_EVENT_INPUT (1 << 0)
#define __SPRT_ALOOPER_EVENT_OUTPUT (1 << 1)
#define __SPRT_ALOOPER_EVENT_ERROR (1 << 2)
#define __SPRT_ALOOPER_EVENT_HANGUP (1 << 3)
#define __SPRT_ALOOPER_EVENT_INVALID (1 << 4)

typedef int (*__SPRT_ID(ALooper_callbackFunc))(int fd, int events, void *data);

#if __SPRT_CONFIG_HAVE_ALOOPER || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS

__SPRT_BEGIN_DECL

SPRT_API __SPRT_ID(ALooper) * __SPRT_ID(ALooper_forThread)();
SPRT_API __SPRT_ID(ALooper) * __SPRT_ID(ALooper_prepare)(int opts);

SPRT_API void __SPRT_ID(ALooper_acquire)(__SPRT_ID(ALooper) * looper);
SPRT_API void __SPRT_ID(ALooper_release)(__SPRT_ID(ALooper) * looper);

SPRT_API int __SPRT_ID(
		ALooper_pollOnce)(int timeoutMillis, int *outFd, int *outEvents, void **outData);

SPRT_API void __SPRT_ID(ALooper_wake)(__SPRT_ID(ALooper) * looper);

SPRT_API int __SPRT_ID(ALooper_addFd)(__SPRT_ID(ALooper) * looper, int fd, int ident, int events,
		__SPRT_ID(ALooper_callbackFunc) callback, void *data);

SPRT_API int __SPRT_ID(ALooper_removeFd)(__SPRT_ID(ALooper) * looper, int fd);

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_C_CROSS_ANDROID_LOOPER_H_
