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

#ifndef CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_SOCKET_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_SOCKET_H_

#include <c/bits/__sprt_def.h>
#include <c/bits/__sprt_int32_t.h>
#include <c/bits/__sprt_uint32_t.h>

#ifdef __SPRT_BUILD
#define __SPRT_SOCKADDR_NAME __SPRT_ID(sockaddr)
#else
#define __SPRT_SOCKADDR_NAME sockaddr
#endif

struct __SPRT_SOCKADDR_NAME;

#if SPRT_LINUX

typedef __SPRT_ID(uint32_t) __SPRT_ID(socklen_t);
typedef unsigned short __SPRT_ID(sa_family_t);

struct __SPRT_SOCKADDR_NAME {
	__SPRT_ID(sa_family_t) sa_family;
	char sa_data[14];
};

#elif SPRT_ANDROID

#if !defined(__LP64__)
typedef __SPRT_ID(int32_t) socklen_t;
#else
typedef __SPRT_ID(uint32_t) socklen_t;
#endif

struct __SPRT_SOCKADDR_NAME {
	sa_family_t sa_family;
	char sa_data[14];
};

#elif SPRT_WINDOWS
#error "Unknown OS"
#elif SPRT_MACOS
#error "Unknown OS"
#else
#error "Unknown OS"
#endif

#endif // CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_SOCKET_H_
