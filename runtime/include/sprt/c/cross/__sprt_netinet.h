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

#ifndef CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_NETINET_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_NETINET_H_

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/cross/__sprt_config.h>
#include <sprt/c/cross/__sprt_sysid.h>

// Platform-varying netinet option numbers (INET*_ADDRSTRLEN, IPPROTO_*, IP_*,
// IPV6_*, MCAST_*) live in the per-platform netinetdef.h, exactly as the socket
// constants live in the per-platform sockdef.h. <netinet/in.h> / <arpa/inet.h>
// expand the public names from these __SPRT_* macros; SPRuntimeCSysSocket.cpp
// static_asserts them against the native header.
#include SPRT_CROSS_CONFIG_NAME(sprt/c/cross/__SPRT_PLATFORM_NAME/netinetdef.h)

// Platform-invariant constants and address-classification macros (same on every
// target): INADDR_*, IPPORT_RESERVED, the IN6_IS_ADDR_* / IN_CLASS* test macros
// and their masks, and IN_LOOPBACKNET.
// clang-format off
#define __SPRT_INADDR_ANY              ((__sprt_in_addr_t)0x00000000)
#define __SPRT_INADDR_LOOPBACK         ((__sprt_in_addr_t)0x7f000001)
#define __SPRT_INADDR_BROADCAST        ((__sprt_in_addr_t)0xffffffff)
#define __SPRT_INADDR_NONE             ((__sprt_in_addr_t)0xffffffff)

#define __SPRT_INADDR_UNSPEC_GROUP    ((__sprt_in_addr_t)0xe0000000)
#define __SPRT_INADDR_ALLHOSTS_GROUP  ((__sprt_in_addr_t)0xe0000001)
#define __SPRT_INADDR_ALLRTRS_GROUP   ((__sprt_in_addr_t)0xe0000002)
#define __SPRT_INADDR_MAX_LOCAL_GROUP ((__sprt_in_addr_t)0xe00000ff)

#define __SPRT_IN6_IS_ADDR_UNSPECIFIED(a) \
        (((__sprt_uint32_t *) (a))[0] == 0 && ((__sprt_uint32_t *) (a))[1] == 0 && \
         ((__sprt_uint32_t *) (a))[2] == 0 && ((__sprt_uint32_t *) (a))[3] == 0)

#define __SPRT_IN6_IS_ADDR_LOOPBACK(a) \
        (((__sprt_uint32_t *) (a))[0] == 0 && ((__sprt_uint32_t *) (a))[1] == 0 && \
         ((__sprt_uint32_t *) (a))[2] == 0 && \
         ((__sprt_uint8_t *) (a))[12] == 0 && ((__sprt_uint8_t *) (a))[13] == 0 && \
         ((__sprt_uint8_t *) (a))[14] == 0 && ((__sprt_uint8_t *) (a))[15] == 1 )

#define __SPRT_IN6_IS_ADDR_MULTICAST(a) (((__sprt_uint8_t *) (a))[0] == 0xff)

#define __SPRT_IN6_IS_ADDR_LINKLOCAL(a) \
        ((((__sprt_uint8_t *) (a))[0]) == 0xfe && (((__sprt_uint8_t *) (a))[1] & 0xc0) == 0x80)

#define __SPRT_IN6_IS_ADDR_SITELOCAL(a) \
        ((((__sprt_uint8_t *) (a))[0]) == 0xfe && (((__sprt_uint8_t *) (a))[1] & 0xc0) == 0xc0)

#define __SPRT_IN6_IS_ADDR_V4MAPPED(a) \
        (((__sprt_uint32_t *) (a))[0] == 0 && ((__sprt_uint32_t *) (a))[1] == 0 && \
         ((__sprt_uint8_t *) (a))[8] == 0 && ((__sprt_uint8_t *) (a))[9] == 0 && \
         ((__sprt_uint8_t *) (a))[10] == 0xff && ((__sprt_uint8_t *) (a))[11] == 0xff)

#define __SPRT_IN6_IS_ADDR_V4COMPAT(a) \
        (((__sprt_uint32_t *) (a))[0] == 0 && ((__sprt_uint32_t *) (a))[1] == 0 && \
         ((__sprt_uint32_t *) (a))[2] == 0 && \
         !IN6_IS_ADDR_UNSPECIFIED(a) && !IN6_IS_ADDR_LOOPBACK(a))

#define __SPRT_IN6_IS_ADDR_MC_NODELOCAL(a) \
        (IN6_IS_ADDR_MULTICAST(a) && ((((__sprt_uint8_t *) (a))[1] & 0xf) == 0x1))

#define __SPRT_IN6_IS_ADDR_MC_LINKLOCAL(a) \
        (IN6_IS_ADDR_MULTICAST(a) && ((((__sprt_uint8_t *) (a))[1] & 0xf) == 0x2))

#define __SPRT_IN6_IS_ADDR_MC_SITELOCAL(a) \
        (IN6_IS_ADDR_MULTICAST(a) && ((((__sprt_uint8_t *) (a))[1] & 0xf) == 0x5))

#define __SPRT_IN6_IS_ADDR_MC_ORGLOCAL(a) \
        (IN6_IS_ADDR_MULTICAST(a) && ((((__sprt_uint8_t *) (a))[1] & 0xf) == 0x8))

#define __SPRT_IN6_IS_ADDR_MC_GLOBAL(a) \
        (IN6_IS_ADDR_MULTICAST(a) && ((((__sprt_uint8_t *) (a))[1] & 0xf) == 0xe))

#define __SPRT_ARE_4_EQUAL(a, b) \
	(!( (0[a]-0[b]) | (1[a]-1[b]) | (2[a]-2[b]) | (3[a]-3[b]) ))
#define __SPRT_IN6_ARE_ADDR_EQUAL(a, b) \
	__SPRT_ARE_4_EQUAL((const __sprt_uint32_t *)(a), (const __sprt_uint32_t *)(b))

#define __SPRT_IN_CLASSA(a)		((((__sprt_in_addr_t)(a)) & 0x80000000) == 0)
#define __SPRT_IN_CLASSA_NET		0xff000000
#define __SPRT_IN_CLASSA_NSHIFT	24
#define __SPRT_IN_CLASSA_HOST		(0xffffffff & ~__SPRT_IN_CLASSA_NET)
#define __SPRT_IN_CLASSA_MAX		128
#define __SPRT_IN_CLASSB(a)		((((__sprt_in_addr_t)(a)) & 0xc0000000) == 0x80000000)
#define __SPRT_IN_CLASSB_NET		0xffff0000
#define __SPRT_IN_CLASSB_NSHIFT	16
#define __SPRT_IN_CLASSB_HOST		(0xffffffff & ~__SPRT_IN_CLASSB_NET)
#define __SPRT_IN_CLASSB_MAX		65536
#define __SPRT_IN_CLASSC(a)		((((__sprt_in_addr_t)(a)) & 0xe0000000) == 0xc0000000)
#define __SPRT_IN_CLASSC_NET		0xffffff00
#define __SPRT_IN_CLASSC_NSHIFT	8
#define __SPRT_IN_CLASSC_HOST		(0xffffffff & ~__SPRT_IN_CLASSC_NET)
#define __SPRT_IN_CLASSD(a)		((((__sprt_in_addr_t)(a)) & 0xf0000000) == 0xe0000000)
#define __SPRT_IN_MULTICAST(a)		IN_CLASSD(a)
#define __SPRT_IN_EXPERIMENTAL(a)	((((__sprt_in_addr_t)(a)) & 0xe0000000) == 0xe0000000)
#define __SPRT_IN_BADCLASS(a)		((((__sprt_in_addr_t)(a)) & 0xf0000000) == 0xf0000000)
// clang-format on

#define __SPRT_IN_LOOPBACKNET 127

#endif // CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_NETINET_H_
