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

#ifndef CORE_RUNTIME_INCLUDE_C_CROSS_MACOS_SPRT_NETINETDEF_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS_MACOS_SPRT_NETINETDEF_H_

// macOS (Darwin/BSD) netinet option numbers, extracted from the SDK
// <netinet/in.h>. Options Linux exposes but Darwin lacks are omitted.

#define __SPRT_INET_ADDRSTRLEN 16
#define __SPRT_INET6_ADDRSTRLEN 46

#define __SPRT_IPPORT_RESERVED 1024

#define __SPRT_IPPROTO_IP 0
#define __SPRT_IPPROTO_HOPOPTS 0
#define __SPRT_IPPROTO_ICMP 1
#define __SPRT_IPPROTO_IGMP 2
#define __SPRT_IPPROTO_IPIP 4
#define __SPRT_IPPROTO_TCP 6
#define __SPRT_IPPROTO_EGP 8
#define __SPRT_IPPROTO_PUP 12
#define __SPRT_IPPROTO_UDP 17
#define __SPRT_IPPROTO_IDP 22
#define __SPRT_IPPROTO_TP 29
#define __SPRT_IPPROTO_IPV6 41
#define __SPRT_IPPROTO_ROUTING 43
#define __SPRT_IPPROTO_FRAGMENT 44
#define __SPRT_IPPROTO_RSVP 46
#define __SPRT_IPPROTO_GRE 47
#define __SPRT_IPPROTO_ESP 50
#define __SPRT_IPPROTO_AH 51
#define __SPRT_IPPROTO_ICMPV6 58
#define __SPRT_IPPROTO_NONE 59
#define __SPRT_IPPROTO_DSTOPTS 60
#define __SPRT_IPPROTO_MTP 92
#define __SPRT_IPPROTO_ENCAP 98
#define __SPRT_IPPROTO_PIM 103
#define __SPRT_IPPROTO_SCTP 132
#define __SPRT_IPPROTO_RAW 255
#define __SPRT_IPPROTO_MAX 256

#define __SPRT_IP_TOS 3
#define __SPRT_IP_TTL 4
#define __SPRT_IP_HDRINCL 2
#define __SPRT_IP_OPTIONS 1
#define __SPRT_IP_RECVOPTS 5
#define __SPRT_IP_RETOPTS 8
#define __SPRT_IP_PKTINFO 26
#define __SPRT_IP_RECVTTL 24
#define __SPRT_IP_RECVTOS 27
#define __SPRT_IP_IPSEC_POLICY 21
#define __SPRT_IP_MULTICAST_IF 9
#define __SPRT_IP_MULTICAST_TTL 10
#define __SPRT_IP_MULTICAST_LOOP 11
#define __SPRT_IP_ADD_MEMBERSHIP 12
#define __SPRT_IP_DROP_MEMBERSHIP 13
#define __SPRT_IP_UNBLOCK_SOURCE 73
#define __SPRT_IP_BLOCK_SOURCE 72
#define __SPRT_IP_ADD_SOURCE_MEMBERSHIP 70
#define __SPRT_IP_DROP_SOURCE_MEMBERSHIP 71
#define __SPRT_IP_MSFILTER 74

#define __SPRT_IP_RECVRETOPTS 6
#define __SPRT_IP_RECVDSTADDR 7
#define __SPRT_IP_RECVIF 20

#define __SPRT_IP_DEFAULT_MULTICAST_TTL 1
#define __SPRT_IP_DEFAULT_MULTICAST_LOOP 1
#define __SPRT_IP_MAX_MEMBERSHIPS 4095

// MCAST_* group-membership options: the *_GROUP / *_SOURCE_GROUP numbers differ
// on Windows (winsock ws2ipdef.h); BLOCK/UNBLOCK/MSFILTER and EXCLUDE/INCLUDE match.
#define __SPRT_MCAST_JOIN_GROUP 80
#define __SPRT_MCAST_LEAVE_GROUP 81
#define __SPRT_MCAST_JOIN_SOURCE_GROUP 82
#define __SPRT_MCAST_LEAVE_SOURCE_GROUP 83
#define __SPRT_MCAST_BLOCK_SOURCE 84
#define __SPRT_MCAST_UNBLOCK_SOURCE 85

#define __SPRT_MCAST_EXCLUDE 2
#define __SPRT_MCAST_INCLUDE 1

#define __SPRT_IPV6_2292PKTINFO 19
#define __SPRT_IPV6_2292HOPOPTS 22
#define __SPRT_IPV6_2292DSTOPTS 23
#define __SPRT_IPV6_2292RTHDR 24
#define __SPRT_IPV6_2292PKTOPTIONS 25
#define __SPRT_IPV6_CHECKSUM 26
#define __SPRT_IPV6_2292HOPLIMIT 20
#define __SPRT_IPV6_UNICAST_HOPS 4
#define __SPRT_IPV6_MULTICAST_IF 9
#define __SPRT_IPV6_MULTICAST_HOPS 10
#define __SPRT_IPV6_MULTICAST_LOOP 11
#define __SPRT_IPV6_JOIN_GROUP 12
#define __SPRT_IPV6_LEAVE_GROUP 13
#define __SPRT_IPV6_V6ONLY 27
#define __SPRT_IPV6_IPSEC_POLICY 28

#define __SPRT_IPV6_RECVTCLASS 35
#define __SPRT_IPV6_TCLASS 36

#define __SPRT_IPV6_RTHDR_LOOSE 0
#define __SPRT_IPV6_RTHDR_STRICT 1

#define __SPRT_IPV6_RTHDR_TYPE_0 0


// --- TCP options (IPPROTO_TCP level) -----------------------------------------
#define __SPRT_TCP_NODELAY 1
#define __SPRT_TCP_MAXSEG 2
#define __SPRT_TCP_MAX_SACK 4
#define __SPRT_TCP_NOPUSH 4
#define __SPRT_TCP_NOOPT 8
#define __SPRT_TCP_MAX_WINSHIFT 14
#define __SPRT_TCP_KEEPALIVE 16
#define __SPRT_TCP_CONNECTIONTIMEOUT 32
#define __SPRT_TCP_MAXOLEN 40
#define __SPRT_TCP_MAXHLEN 60
#define __SPRT_TCP_RXT_CONNDROPTIME 128
#define __SPRT_TCP_MINMSS 216
#define __SPRT_TCP_RXT_FINDROP 256
#define __SPRT_TCP_KEEPINTVL 257
#define __SPRT_TCP_KEEPCNT 258
#define __SPRT_TCP_SENDMOREACKS 259
#define __SPRT_TCP_ENABLE_ECN 260
#define __SPRT_TCP_FASTOPEN 261
#define __SPRT_TCP_CONNECTION_INFO 262
#define __SPRT_TCP_MSS 512
#define __SPRT_TCP_NOTSENT_LOWAT 513
#define __SPRT_TCP_MAXWIN 65535

#endif // CORE_RUNTIME_INCLUDE_C_CROSS_MACOS_SPRT_NETINETDEF_H_
