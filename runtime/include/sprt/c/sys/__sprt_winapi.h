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

#ifndef CORE_RUNTIME_INCLUDE_C_SYS___SPRT_WINAPI_H_
#define CORE_RUNTIME_INCLUDE_C_SYS___SPRT_WINAPI_H_

#include <sprt/c/cross/__sprt_config.h>
#include <sprt/c/cross/__sprt_sysid.h>
#include <sprt/c/bits/__sprt_uint32_t.h>
#include <sprt/c/bits/__sprt_size_t.h>

/*
	This defines some WinAPI umbrella methods available for use in the Stappler Runtime

	Methods are defined in libc style, without WinAPI-specific variable types.
*/

// clang-format off
#define __SPRT_WINAPI_INFINITE 0xFFFFFFFF
// clang-format on

#if __SPRT_CONFIG_HAVE_WINAPI || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS

__SPRT_BEGIN_DECL

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API __SPRT_ID(pid_t) __SPRT_ID(GetCurrentThreadId)();

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API __SPRT_ID(uint32_t) __SPRT_ID(GetLastError)();

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API int __SPRT_ID(WaitOnAddress)(volatile void *Address, void *CompareAddress,
		__SPRT_ID(size_t) AddressSize, __SPRT_ID(uint32_t) dwMilliseconds);

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API void __SPRT_ID(WakeByAddressSingle)(void *Address);

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API void __SPRT_ID(WakeByAddressAll)(void *Address);

__SPRT_END_DECL

#endif

#endif
