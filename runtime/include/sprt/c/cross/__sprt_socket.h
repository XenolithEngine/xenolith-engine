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
#include <sprt/c/bits/__sprt_uint8_t.h>
#include <sprt/c/bits/__sprt_size_t.h>
#include <sprt/c/bits/iovec.h>
#include <sprt/c/cross/__sprt_sysid.h>

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

// The message / control structs keep the same portable spelling; their layout is
// defined per-platform in cross/<platform>/socket.h (SPRuntimeCSysSocket.cpp
// static_asserts each against the native <sys/socket.h>).
#ifdef __SPRT_BUILD
#define __SPRT_MSGHDR_NAME __SPRT_ID(msghdr)
#define __SPRT_CMSGHDR_NAME __SPRT_ID(cmsghdr)
#define __SPRT_MMSGHDR_NAME __SPRT_ID(mmsghdr)
#define __SPRT_LINGER_NAME __SPRT_ID(linger)
#else
#define __SPRT_MSGHDR_NAME msghdr
#define __SPRT_CMSGHDR_NAME cmsghdr
#define __SPRT_MMSGHDR_NAME mmsghdr
#define __SPRT_LINGER_NAME linger
#endif

struct __SPRT_SOCKADDR_NAME;
struct __SPRT_IN_ADDR_NAME;
struct __SPRT_IN6_ADDR_NAME;
struct __SPRT_SOCKADDR_IN_NAME;
struct __SPRT_SOCKADDR_IN6_NAME;

#include <sprt/c/cross/__sprt_config.h>

// clang-format off
// sockdef.h carries the plain-named constants (AF_*/SOCK_*/SO_*/MSG_*). They collide
// with the native <sys/socket.h> enums/macros, so they are pulled only where the SPRT
// libc *is* the libc (freestanding). On a hosted __SPRT_BUILD - the wrapper, which also
// includes the native header for forwarding - the platform header supplies them, and
// only the (namespaced) SPRT struct types below are taken from the cross header.
#if !(defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1)
#include SPRT_CROSS_CONFIG_NAME(sprt/c/cross/__SPRT_PLATFORM_NAME/sockdef.h)
#endif
#include SPRT_CROSS_CONFIG_NAME(sprt/c/cross/__SPRT_PLATFORM_NAME/socket.h)
// clang-format on

#endif // CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_SOCKET_H_
