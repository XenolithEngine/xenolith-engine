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
#include <sprt/c/bits/__sprt_ssize_t.h>
#include <sprt/c/bits/iovec.h>
#include <sprt/c/cross/__sprt_sysid.h>

// The message / control structs keep the same portable spelling; their layout is
// defined per-platform in cross/<platform>/socket.h (SPRuntimeCSysSocket.cpp
// static_asserts each against the native <sys/socket.h>).
#ifdef __SPRT_BUILD
#define __SPRT_SOCKADDR_NAME __SPRT_ID(sockaddr)
#define __SPRT_SOCKADDR_STORAGE_NAME __SPRT_ID(sockaddr_storage)
#define __SPRT_IN_ADDR_NAME __SPRT_ID(in_addr)
#define __SPRT_IN6_ADDR_NAME __SPRT_ID(in6_addr)
#define __SPRT_SOCKADDR_IN_NAME __SPRT_ID(sockaddr_in)
#define __SPRT_SOCKADDR_IN6_NAME __SPRT_ID(sockaddr_in6)
#define __SPRT_MSGHDR_NAME __SPRT_ID(msghdr)
#define __SPRT_CMSGHDR_NAME __SPRT_ID(cmsghdr)
#define __SPRT_MMSGHDR_NAME __SPRT_ID(mmsghdr)
#define __SPRT_LINGER_NAME __SPRT_ID(linger)
#else
#define __SPRT_SOCKADDR_NAME sockaddr
#define __SPRT_SOCKADDR_STORAGE_NAME sockaddr_storage
#define __SPRT_IN_ADDR_NAME in_addr
#define __SPRT_IN6_ADDR_NAME in6_addr
#define __SPRT_SOCKADDR_IN_NAME sockaddr_in
#define __SPRT_SOCKADDR_IN6_NAME sockaddr_in6
#define __SPRT_MSGHDR_NAME msghdr
#define __SPRT_CMSGHDR_NAME cmsghdr
#define __SPRT_MMSGHDR_NAME mmsghdr
#define __SPRT_LINGER_NAME linger
#endif

struct __SPRT_SOCKADDR_NAME;
struct __SPRT_SOCKADDR_STORAGE_NAME;
struct __SPRT_IN_ADDR_NAME;
struct __SPRT_IN6_ADDR_NAME;
struct __SPRT_SOCKADDR_IN_NAME;
struct __SPRT_SOCKADDR_IN6_NAME;

#include <sprt/c/cross/__sprt_config.h>

// clang-format off
// sockdef.h now carries the portable core socket constants NAMESPACED as __SPRT_* (safe to
// include on every build; the public names are expanded from them in <sys/socket.h>, and the
// wrapper static_asserts each against native). Its remaining platform-specific extras keep
// plain public names and are self-guarded to freestanding inside sockdef.h. So it is always
// included now.
#include SPRT_CROSS_CONFIG_NAME(sprt/c/cross/__SPRT_PLATFORM_NAME/sockdef.h)
#include SPRT_CROSS_CONFIG_NAME(sprt/c/cross/__SPRT_PLATFORM_NAME/socket.h)
// clang-format on

#endif // CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_SOCKET_H_
