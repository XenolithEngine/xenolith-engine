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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SYS_WINAPI_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_WINAPI_H_

#include <sprt/c/sys/__sprt_winapi.h>

#if __SPRT_CONFIG_HAVE_WINAPI || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS

#define _WINAPI_INFINITE __SPRT_WINAPI_INFINITE

__SPRT_BEGIN_DECL

SPRT_FORCEINLINE __SPRT_ID(pid_t) _GetCurrentThreadId() { return __sprt_GetCurrentThreadId(); }

SPRT_FORCEINLINE __SPRT_ID(uint32_t) _GetLastError() { return __sprt_GetLastError(); }

SPRT_FORCEINLINE int _WaitOnAddress(volatile void *Address, void *CompareAddress,
		__SPRT_ID(size_t) AddressSize, __SPRT_ID(uint32_t) dwMilliseconds) {
	return __sprt_WaitOnAddress(Address, CompareAddress, AddressSize, dwMilliseconds);
}

SPRT_FORCEINLINE void _WakeByAddressSingle(void *Address) { __sprt_WakeByAddressSingle(Address); }

SPRT_FORCEINLINE void _WakeByAddressAll(void *Address) { __sprt_WakeByAddressAll(Address); }

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYS_WINAPI_H_
