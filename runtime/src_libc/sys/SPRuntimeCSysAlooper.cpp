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

#include "private/SPRTSpecific.h"

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/sys/__sprt_alooper.h>

#include <sprt/runtime/log.h>

#if __SPRT_CONFIG_HAVE_ALOOPER
#include <android/looper.h>
#endif

namespace sprt {

#if __SPRT_CONFIG_HAVE_ALOOPER

__SPRT_C_FUNC __SPRT_ID(ALooper) * __SPRT_ID(ALooper_forThread)() { return ALooper_forThread(); }

__SPRT_C_FUNC __SPRT_ID(ALooper) * __SPRT_ID(ALooper_prepare)(int opts) {
	return ALooper_prepare(opts);
}

__SPRT_C_FUNC void __SPRT_ID(ALooper_acquire)(__SPRT_ID(ALooper) * looper) {
	ALooper_acquire(looper);
}
__SPRT_C_FUNC void __SPRT_ID(ALooper_release)(__SPRT_ID(ALooper) * looper) {
	ALooper_release(looper);
}

__SPRT_C_FUNC int __SPRT_ID(
		ALooper_pollOnce)(int timeoutMillis, int *outFd, int *outEvents, void **outData) {
	return ALooper_pollOnce(timeoutMillis, outFd, outEvents, outData);
}

__SPRT_C_FUNC void __SPRT_ID(ALooper_wake)(__SPRT_ID(ALooper) * looper) { ALooper_wake(looper); }

__SPRT_C_FUNC int __SPRT_ID(ALooper_addFd)(__SPRT_ID(ALooper) * looper, int fd, int ident,
		int events, __SPRT_ID(ALooper_callbackFunc) callback, void *data) {
	return ALooper_addFd(looper, fd, ident, events, callback, data);
}

__SPRT_C_FUNC int __SPRT_ID(ALooper_removeFd)(__SPRT_ID(ALooper) * looper, int fd) {
	return ALooper_removeFd(looper, fd);
}

#else

__SPRT_C_FUNC __SPRT_ID(ALooper) * __SPRT_ID(ALooper_forThread)() {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_ALOOPER)");
	return nullptr;
}

__SPRT_C_FUNC __SPRT_ID(ALooper) * __SPRT_ID(ALooper_prepare)(int opts) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_ALOOPER)");
	return nullptr;
}

__SPRT_C_FUNC void __SPRT_ID(ALooper_acquire)(__SPRT_ID(ALooper) * looper) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_ALOOPER)");
	return;
}
__SPRT_C_FUNC void __SPRT_ID(ALooper_release)(__SPRT_ID(ALooper) * looper) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_ALOOPER)");
	return;
}

__SPRT_C_FUNC int __SPRT_ID(
		ALooper_pollOnce)(int timeoutMillis, int *outFd, int *outEvents, void **outData) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_ALOOPER)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

__SPRT_C_FUNC void __SPRT_ID(ALooper_wake)(__SPRT_ID(ALooper) * looper) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_ALOOPER)");
	return;
}

__SPRT_C_FUNC int __SPRT_ID(ALooper_addFd)(__SPRT_ID(ALooper) * looper, int fd, int ident,
		int events, __SPRT_ID(ALooper_callbackFunc) callback, void *data) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_ALOOPER)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(ALooper_removeFd)(__SPRT_ID(ALooper) * looper, int fd) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_ALOOPER)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

#endif

} // namespace sprt
