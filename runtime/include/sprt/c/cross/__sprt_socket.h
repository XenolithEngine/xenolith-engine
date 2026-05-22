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

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/bits/__sprt_int32_t.h>
#include <sprt/c/bits/__sprt_uint32_t.h>
#include <sprt/c/bits/__sprt_uint16_t.h>
#include <sprt/c/bits/__sprt_uint64_t.h>
#include <sprt/c/bits/__sprt_int64_t.h>

#ifdef __SPRT_BUILD
#define __SPRT_SOCKADDR_NAME __SPRT_ID(sockaddr)
#else
#define __SPRT_SOCKADDR_NAME sockaddr
#endif

#ifdef __SPRT_BUILD
#define __SPRT_IN_ADDR_NAME __SPRT_ID(in_addr)
#else
#define __SPRT_IN_ADDR_NAME in_addr
#endif

#ifdef __SPRT_BUILD
#define __SPRT_IN6_ADDR_NAME __SPRT_ID(in6_addr)
#else
#define __SPRT_IN6_ADDR_NAME in6_addr
#endif

#ifdef __SPRT_BUILD
#define __SPRT_SOCKADDR_IN_NAME __SPRT_ID(sockaddr_in)
#else
#define __SPRT_SOCKADDR_IN_NAME sockaddr_in
#endif

#ifdef __SPRT_BUILD
#define __SPRT_SOCKADDR_IN6_NAME __SPRT_ID(sockaddr_in6)
#else
#define __SPRT_SOCKADDR_IN6_NAME sockaddr_in6
#endif

struct __SPRT_SOCKADDR_NAME;
struct __SPRT_IN_ADDR_NAME;
struct __SPRT_IN6_ADDR_NAME;
struct __SPRT_SOCKADDR_IN_NAME;
struct __SPRT_SOCKADDR_IN6_NAME;

#include <sprt/c/cross/__sprt_config.h>

// clang-format off
#include SPRT_CROSS_CONFIG_NAME(sprt/c/cross/__SPRT_PLATFORM_NAME/sockdef.h)
#include SPRT_CROSS_CONFIG_NAME(sprt/c/cross/__SPRT_PLATFORM_NAME/socket.h)
// clang-format on

#endif // CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_SOCKET_H_
