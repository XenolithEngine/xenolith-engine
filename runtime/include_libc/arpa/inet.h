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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_ARPA_INET_H_
#define CORE_RUNTIME_INCLUDE_LIBC_ARPA_INET_H_

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <arpa/inet.h>

#else

#include <sprt/c/cross/__sprt_socket.h>
#include <stdint.h>

typedef __SPRT_ID(socklen_t) socklen_t;
typedef __SPRT_ID(in_addr_t) in_addr_t;

// clang-format off
#define INADDR_ANY              (__SPRT_ID(uint32_t))0x00000000
#define INADDR_LOOPBACK         (__SPRT_ID(uint32_t))0x7f000001
#define INADDR_BROADCAST        (__SPRT_ID(uint32_t))0xffffffff
#define INADDR_NONE             (__SPRT_ID(uint32_t))0xffffffff
// clang-format on

__SPRT_BEGIN_DECL

uint32_t htonl(uint32_t) SPRT_NOEXCEPT;
uint16_t htons(uint16_t) SPRT_NOEXCEPT;
uint32_t ntohl(uint32_t) SPRT_NOEXCEPT;
uint16_t ntohs(uint16_t) SPRT_NOEXCEPT;

in_addr_t inet_addr(const char *) SPRT_NOEXCEPT;
char *inet_ntoa(struct __SPRT_IN_ADDR_NAME) SPRT_NOEXCEPT;
int inet_pton(int, const char *__SPRT_RESTRICT, void *__SPRT_RESTRICT) SPRT_NOEXCEPT;
const char *inet_ntop(int, const void *__SPRT_RESTRICT, char *__SPRT_RESTRICT,
		socklen_t) SPRT_NOEXCEPT;

// Legacy functions:
in_addr_t inet_network(const char *) SPRT_NOEXCEPT;
int inet_aton(const char *, struct __SPRT_IN_ADDR_NAME *) SPRT_NOEXCEPT;
struct __SPRT_IN_ADDR_NAME inet_makeaddr(in_addr_t, in_addr_t) SPRT_NOEXCEPT;

in_addr_t inet_lnaof(struct __SPRT_IN_ADDR_NAME) SPRT_NOEXCEPT;
in_addr_t inet_netof(struct __SPRT_IN_ADDR_NAME) SPRT_NOEXCEPT;

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_ASSERT_H_
