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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_NETINET_IN_H_
#define CORE_RUNTIME_INCLUDE_LIBC_NETINET_IN_H_

/*
	POSIX <netinet/in.h> - Internet address family (IPv4/IPv6).
	- hosted SPRT build -> forwards to the system <netinet/in.h> (#include_next)
	- otherwise         -> SPRT-own declarations below

	The concrete address structures (in_addr, in6_addr, sockaddr_in, sockaddr_in6)
	live in the per-platform cross socket header pulled in via <sys/socket.h>; this
	header adds the family's typedefs, protocol/level constants and helper macros.
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <netinet/in.h>

#else

#include <sys/socket.h>
#include <arpa/inet.h>
#include <inttypes.h>

typedef __SPRT_ID(in_port_t) in_port_t;
typedef __SPRT_ID(in_addr_t) in_addr_t;

struct ip_mreq {
	struct __SPRT_IN_ADDR_NAME imr_multiaddr;
	struct __SPRT_IN_ADDR_NAME imr_interface;
};

struct ip_mreqn {
	struct __SPRT_IN_ADDR_NAME imr_multiaddr;
	struct __SPRT_IN_ADDR_NAME imr_address;
	int imr_ifindex;
};

struct ipv6_mreq {
	struct __SPRT_IN6_ADDR_NAME ipv6mr_multiaddr;
	unsigned ipv6mr_interface;
};

struct in_pktinfo {
	int ipi_ifindex;
	struct __SPRT_IN_ADDR_NAME ipi_spec_dst;
	struct __SPRT_IN_ADDR_NAME ipi_addr;
};

struct in6_pktinfo {
	struct __SPRT_IN6_ADDR_NAME ipi6_addr;
	unsigned ipi6_ifindex;
};

// IPv6 address test macros (IN6_IS_ADDR_*), the IPv4 class macros and the
// IPPROTO_*/IP_*/IPV6_* level constants are materialized from <__sprt_netinet.h>
// via <arpa/inet.h> (included above).

__SPRT_BEGIN_DECL

extern const struct __SPRT_IN6_ADDR_NAME in6addr_any;
extern const struct __SPRT_IN6_ADDR_NAME in6addr_loopback;

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_NETINET_IN_H_
