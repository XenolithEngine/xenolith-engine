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

#ifndef CORE_RUNTIME_INCLUDE_C_CROSS_ANDROID_SPRT_NETINETDEF_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS_ANDROID_SPRT_NETINETDEF_H_

// Android (bionic) netinet option numbers. Differs from glibc: bionic lacks the
// glibc-only IP_PMTUDISC / IPV6_RTHDR_* / IP_MAX_MEMBERSHIPS / IPV6_RX* names and
// keeps IPPORT_RESERVED in <netdb.h>. Validated against native bionic <netinet/in.h>.

#define __SPRT_INET_ADDRSTRLEN 16
#define __SPRT_INET6_ADDRSTRLEN 46

#define __SPRT_IPPROTO_IP       0
#define __SPRT_IPPROTO_HOPOPTS  0
#define __SPRT_IPPROTO_ICMP     1
#define __SPRT_IPPROTO_IGMP     2
#define __SPRT_IPPROTO_IPIP     4
#define __SPRT_IPPROTO_TCP      6
#define __SPRT_IPPROTO_EGP      8
#define __SPRT_IPPROTO_PUP      12
#define __SPRT_IPPROTO_UDP      17
#define __SPRT_IPPROTO_IDP      22
#define __SPRT_IPPROTO_TP       29
#define __SPRT_IPPROTO_DCCP     33
#define __SPRT_IPPROTO_IPV6     41
#define __SPRT_IPPROTO_ROUTING  43
#define __SPRT_IPPROTO_FRAGMENT 44
#define __SPRT_IPPROTO_RSVP     46
#define __SPRT_IPPROTO_GRE      47
#define __SPRT_IPPROTO_ESP      50
#define __SPRT_IPPROTO_AH       51
#define __SPRT_IPPROTO_ICMPV6   58
#define __SPRT_IPPROTO_NONE     59
#define __SPRT_IPPROTO_DSTOPTS  60
#define __SPRT_IPPROTO_MTP      92
#define __SPRT_IPPROTO_BEETPH   94
#define __SPRT_IPPROTO_ENCAP    98
#define __SPRT_IPPROTO_PIM      103
#define __SPRT_IPPROTO_COMP     108
#define __SPRT_IPPROTO_SCTP     132
#define __SPRT_IPPROTO_MH       135
#define __SPRT_IPPROTO_UDPLITE  136
#define __SPRT_IPPROTO_MPLS     137
#define __SPRT_IPPROTO_ETHERNET 143
#define __SPRT_IPPROTO_RAW      255
#define __SPRT_IPPROTO_MPTCP    262
#define __SPRT_IPPROTO_MAX 263

#define __SPRT_IP_TOS 1
#define __SPRT_IP_TTL 2
#define __SPRT_IP_HDRINCL 3
#define __SPRT_IP_OPTIONS 4
#define __SPRT_IP_ROUTER_ALERT    5
#define __SPRT_IP_RECVOPTS        6
#define __SPRT_IP_RETOPTS         7
#define __SPRT_IP_PKTINFO 8
#define __SPRT_IP_PKTOPTIONS      9
#define __SPRT_IP_MTU_DISCOVER 10
#define __SPRT_IP_RECVERR 11
#define __SPRT_IP_RECVTTL 12
#define __SPRT_IP_RECVTOS 13
#define __SPRT_IP_MTU 14
#define __SPRT_IP_FREEBIND        15
#define __SPRT_IP_IPSEC_POLICY    16
#define __SPRT_IP_XFRM_POLICY     17
#define __SPRT_IP_PASSSEC         18
#define __SPRT_IP_TRANSPARENT     19
#define __SPRT_IP_ORIGDSTADDR     20
#define __SPRT_IP_RECVORIGDSTADDR __SPRT_IP_ORIGDSTADDR
#define __SPRT_IP_MINTTL          21
#define __SPRT_IP_NODEFRAG        22
#define __SPRT_IP_CHECKSUM        23
#define __SPRT_IP_BIND_ADDRESS_NO_PORT 24
#define __SPRT_IP_RECVFRAGSIZE    25
#define __SPRT_IP_RECVERR_RFC4884 26
#define __SPRT_IP_MULTICAST_IF 32
#define __SPRT_IP_MULTICAST_TTL 33
#define __SPRT_IP_MULTICAST_LOOP 34
#define __SPRT_IP_ADD_MEMBERSHIP 35
#define __SPRT_IP_DROP_MEMBERSHIP 36
#define __SPRT_IP_UNBLOCK_SOURCE 37
#define __SPRT_IP_BLOCK_SOURCE 38
#define __SPRT_IP_ADD_SOURCE_MEMBERSHIP 39
#define __SPRT_IP_DROP_SOURCE_MEMBERSHIP 40
#define __SPRT_IP_MSFILTER        41
#define __SPRT_IP_MULTICAST_ALL   49
#define __SPRT_IP_UNICAST_IF 50

#define __SPRT_IP_RECVRETOPTS __SPRT_IP_RETOPTS

// Windows-only IPv4 options (winsock.h / ws2ipdef.h); absent from POSIX/Linux.

#define __SPRT_IP_PMTUDISC_DONT   0
#define __SPRT_IP_PMTUDISC_WANT   1
#define __SPRT_IP_PMTUDISC_DO     2
#define __SPRT_IP_PMTUDISC_PROBE  3
#define __SPRT_IP_PMTUDISC_INTERFACE 4
#define __SPRT_IP_PMTUDISC_OMIT   5

#define __SPRT_IP_DEFAULT_MULTICAST_TTL        1
#define __SPRT_IP_DEFAULT_MULTICAST_LOOP       1

// MCAST_* group-membership options: the *_GROUP / *_SOURCE_GROUP numbers differ
// on Windows (winsock ws2ipdef.h); BLOCK/UNBLOCK/MSFILTER and EXCLUDE/INCLUDE match.
#define __SPRT_MCAST_JOIN_GROUP         42
#define __SPRT_MCAST_LEAVE_GROUP        45
#define __SPRT_MCAST_JOIN_SOURCE_GROUP  46
#define __SPRT_MCAST_LEAVE_SOURCE_GROUP 47
#define __SPRT_MCAST_BLOCK_SOURCE       43
#define __SPRT_MCAST_UNBLOCK_SOURCE     44
#define __SPRT_MCAST_MSFILTER           48

#define __SPRT_MCAST_EXCLUDE 0
#define __SPRT_MCAST_INCLUDE 1


#define __SPRT_IPV6_ADDRFORM           1
#define __SPRT_IPV6_2292PKTINFO        2
#define __SPRT_IPV6_2292HOPOPTS        3
#define __SPRT_IPV6_2292DSTOPTS        4
#define __SPRT_IPV6_2292RTHDR          5
#define __SPRT_IPV6_2292PKTOPTIONS     6
#define __SPRT_IPV6_CHECKSUM 7
#define __SPRT_IPV6_2292HOPLIMIT       8
#define __SPRT_IPV6_NEXTHOP            9
#define __SPRT_IPV6_AUTHHDR            10
#define __SPRT_IPV6_UNICAST_HOPS 16
#define __SPRT_IPV6_MULTICAST_IF 17
#define __SPRT_IPV6_MULTICAST_HOPS 18
#define __SPRT_IPV6_MULTICAST_LOOP 19
#define __SPRT_IPV6_JOIN_GROUP 20
#define __SPRT_IPV6_LEAVE_GROUP 21
#define __SPRT_IPV6_ROUTER_ALERT       22
#define __SPRT_IPV6_MTU_DISCOVER 23
#define __SPRT_IPV6_MTU 24
#define __SPRT_IPV6_RECVERR 25
#define __SPRT_IPV6_V6ONLY 26
#define __SPRT_IPV6_JOIN_ANYCAST       27
#define __SPRT_IPV6_LEAVE_ANYCAST      28
#define __SPRT_IPV6_MULTICAST_ALL      29
#define __SPRT_IPV6_ROUTER_ALERT_ISOLATE 30
#define __SPRT_IPV6_IPSEC_POLICY       34
#define __SPRT_IPV6_XFRM_POLICY        35
#define __SPRT_IPV6_HDRINCL 36

#define __SPRT_IPV6_RECVPKTINFO        49
#define __SPRT_IPV6_PKTINFO 50
#define __SPRT_IPV6_RECVHOPLIMIT       51
#define __SPRT_IPV6_HOPLIMIT 52
#define __SPRT_IPV6_RECVHOPOPTS        53
#define __SPRT_IPV6_HOPOPTS 54
#define __SPRT_IPV6_RTHDRDSTOPTS       55
#define __SPRT_IPV6_RECVRTHDR 56
#define __SPRT_IPV6_RTHDR 57
#define __SPRT_IPV6_RECVDSTOPTS        58
#define __SPRT_IPV6_DSTOPTS            59
#define __SPRT_IPV6_RECVPATHMTU        60
#define __SPRT_IPV6_PATHMTU            61
#define __SPRT_IPV6_DONTFRAG 62
#define __SPRT_IPV6_RECVTCLASS 66
#define __SPRT_IPV6_TCLASS 67
#define __SPRT_IPV6_AUTOFLOWLABEL      70
#define __SPRT_IPV6_ADDR_PREFERENCES   72
#define __SPRT_IPV6_MINHOPCOUNT        73
#define __SPRT_IPV6_ORIGDSTADDR        74
#define __SPRT_IPV6_RECVORIGDSTADDR    __SPRT_IPV6_ORIGDSTADDR
#define __SPRT_IPV6_TRANSPARENT        75
#define __SPRT_IPV6_UNICAST_IF 76
#define __SPRT_IPV6_RECVFRAGSIZE       77
#define __SPRT_IPV6_FREEBIND           78

// Windows-only IPv6 options (winsock.h); absent from the POSIX/Linux set.

#define __SPRT_IPV6_ADD_MEMBERSHIP     __SPRT_IPV6_JOIN_GROUP
#define __SPRT_IPV6_DROP_MEMBERSHIP    __SPRT_IPV6_LEAVE_GROUP

#define __SPRT_IPV6_PMTUDISC_DONT      0
#define __SPRT_IPV6_PMTUDISC_WANT      1
#define __SPRT_IPV6_PMTUDISC_DO        2
#define __SPRT_IPV6_PMTUDISC_PROBE     3
#define __SPRT_IPV6_PMTUDISC_INTERFACE 4
#define __SPRT_IPV6_PMTUDISC_OMIT      5

#define __SPRT_IPV6_PREFER_SRC_TMP            0x0001
#define __SPRT_IPV6_PREFER_SRC_PUBLIC         0x0002
#define __SPRT_IPV6_PREFER_SRC_PUBTMP_DEFAULT 0x0100
#define __SPRT_IPV6_PREFER_SRC_COA            0x0004
#define __SPRT_IPV6_PREFER_SRC_HOME           0x0400
#define __SPRT_IPV6_PREFER_SRC_CGA            0x0008
#define __SPRT_IPV6_PREFER_SRC_NONCGA         0x0800




// --- TCP options (IPPROTO_TCP level) -----------------------------------------
#define __SPRT_TCP_REPAIR_OFF_NO_WP -1
#define __SPRT_TCP_REPAIR_OFF 0
#define __SPRT_TCP_AO_KEYF_IFINDEX 1
#define __SPRT_TCP_MD5SIG_FLAG_PREFIX 1
#define __SPRT_TCP_NODELAY 1
#define __SPRT_TCP_RECEIVE_ZEROCOPY_FLAG_TLB_CLEAN_HINT 1
#define __SPRT_TCP_REPAIR_ON 1
#define __SPRT_TCP_AO_KEYF_EXCLUDE_OPT 2
#define __SPRT_TCP_MAXSEG 2
#define __SPRT_TCP_MD5SIG_FLAG_IFINDEX 2
#define __SPRT_TCP_CORK 3
#define __SPRT_TCP_KEEPIDLE 4
#define __SPRT_TCP_KEEPINTVL 5
#define __SPRT_TCP_KEEPCNT 6
#define __SPRT_TCP_SYNCNT 7
#define __SPRT_TCP_LINGER2 8
#define __SPRT_TCP_DEFER_ACCEPT 9
#define __SPRT_TCP_WINDOW_CLAMP 10
#define __SPRT_TCP_INFO 11
#define __SPRT_TCP_QUICKACK 12
#define __SPRT_TCP_CONGESTION 13
#define __SPRT_TCP_MD5SIG 14
#define __SPRT_TCP_THIN_LINEAR_TIMEOUTS 16
#define __SPRT_TCP_THIN_DUPACK 17
#define __SPRT_TCP_USER_TIMEOUT 18
#define __SPRT_TCP_REPAIR 19
#define __SPRT_TCP_REPAIR_QUEUE 20
#define __SPRT_TCP_QUEUE_SEQ 21
#define __SPRT_TCP_REPAIR_OPTIONS 22
#define __SPRT_TCP_FASTOPEN 23
#define __SPRT_TCP_TIMESTAMP 24
#define __SPRT_TCP_NOTSENT_LOWAT 25
#define __SPRT_TCP_CC_INFO 26
#define __SPRT_TCP_SAVE_SYN 27
#define __SPRT_TCP_SAVED_SYN 28
#define __SPRT_TCP_REPAIR_WINDOW 29
#define __SPRT_TCP_FASTOPEN_CONNECT 30
#define __SPRT_TCP_ULP 31
#define __SPRT_TCP_MD5SIG_EXT 32
#define __SPRT_TCP_FASTOPEN_KEY 33
#define __SPRT_TCP_FASTOPEN_NO_COOKIE 34
#define __SPRT_TCP_ZEROCOPY_RECEIVE 35
#define __SPRT_TCP_CM_INQ 36
#define __SPRT_TCP_INQ 36
#define __SPRT_TCP_TX_DELAY 37
#define __SPRT_TCP_AO_ADD_KEY 38
#define __SPRT_TCP_AO_DEL_KEY 39
#define __SPRT_TCP_AO_INFO 40
#define __SPRT_TCP_AO_GET_KEYS 41
#define __SPRT_TCP_AO_REPAIR 42
#define __SPRT_TCP_IS_MPTCP 43
#define __SPRT_TCP_AO_MAXKEYLEN 80
#define __SPRT_TCP_MD5SIG_MAXKEYLEN 80
#define __SPRT_TCP_MSS_DEFAULT 536
#define __SPRT_TCP_MSS_DESIRED 1220

#endif // CORE_RUNTIME_INCLUDE_C_CROSS_ANDROID_SPRT_NETINETDEF_H_
