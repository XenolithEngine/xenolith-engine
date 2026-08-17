#ifndef CORE_RUNTIME_INCLUDE_C_CROSS_EMBOX_SPRT_NETINETDEF_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS_EMBOX_SPRT_NETINETDEF_H_

// clang-format off
// Embox netinet option numbers. The IPPROTO_* protocol numbers are IANA's and match
// Linux, but the socket options at each protocol level are numbered from 1 in
// Embox's own order - IP_MULTICAST_IF is 1 there, not 32, and IPV6_MULTICAST_IF is
// 20, not 17. setsockopt() gets these untranslated, so the table has to be Embox's.
//
// Options Embox does not implement keep a distinct value above 0x1000, clear of the
// windows it actually uses, so an unsupported request fails with ENOPROTOOPT
// instead of setting a different option; their asserts in SPRuntimeCSysSocket.cpp
// are #ifdef'd on the native spelling.
//
// NB: Embox's own IP_TOS/IP_TTL/IP_HDRINCL (1/2/3) collide with its
// IP_MULTICAST_IF/IP_MULTICAST_TTL/IP_MULTICAST_LOOP (1/2/3). That is the
// platform's numbering, mirrored here as-is - the asserts below pin both sides.

#define __SPRT_INET_ADDRSTRLEN 16
#define __SPRT_INET6_ADDRSTRLEN 46

#define __SPRT_IPPORT_RESERVED 1024

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
#define __SPRT_IPPROTO_UDPLITE  136
#define __SPRT_IPPROTO_MPLS     137
#define __SPRT_IPPROTO_RAW      255
// Not declared by Embox; the IANA numbers are kept.
#define __SPRT_IPPROTO_MH       135
#define __SPRT_IPPROTO_ETHERNET 143
#define __SPRT_IPPROTO_MPTCP    262
// Embox's enum ends one past IPPROTO_RAW.
#define __SPRT_IPPROTO_MAX      256 // Linux: 263

// --- IPPROTO_IP level ---------------------------------------------------------
// NuttX: __SO_PROTOCOL is 16.
#define __SPRT_IP_MULTICAST_IF                  1  // Linux: 32
#define __SPRT_IP_MULTICAST_TTL                 2  // Linux: 33
#define __SPRT_IP_MULTICAST_LOOP                3  // Linux: 34
#define __SPRT_IP_ADD_MEMBERSHIP                4  // Linux: 35
#define __SPRT_IP_DROP_MEMBERSHIP               5  // Linux: 36
#define __SPRT_IP_UNBLOCK_SOURCE                6  // Linux: 37
#define __SPRT_IP_BLOCK_SOURCE                  7  // Linux: 38
#define __SPRT_IP_ADD_SOURCE_MEMBERSHIP         8  // Linux: 39
#define __SPRT_IP_DROP_SOURCE_MEMBERSHIP        9  // Linux: 40
#define __SPRT_IP_MSFILTER                      10 // Linux: 41
#define __SPRT_IP_MULTICAST_ALL                 11 // Linux: 49
#define __SPRT_IP_PKTINFO                       12 // Linux: 8
#define __SPRT_IP_TOS                           0x01 // Linux: 1; same bit-pattern collision as IP_MULTICAST_IF
#define __SPRT_IP_TTL                           0x02 // Linux: 2; collides with IP_MULTICAST_TTL in Embox

#define __SPRT_IP_HDRINCL                0x03 // collides with IP_MULTICAST_LOOP in Embox

// Unimplemented by Embox.
#define __SPRT_IP_OPTIONS              0x1001
#define __SPRT_IP_ROUTER_ALERT         0x1002
#define __SPRT_IP_RECVOPTS             0x1003
#define __SPRT_IP_RETOPTS              0x1004
#define __SPRT_IP_RECVRETOPTS          __SPRT_IP_RETOPTS
#define __SPRT_IP_PKTOPTIONS           0x1005
#define __SPRT_IP_PMTUDISC             0x1006
#define __SPRT_IP_MTU_DISCOVER         __SPRT_IP_PMTUDISC
#define __SPRT_IP_RECVERR              0x1007
#define __SPRT_IP_RECVTTL              0x1008
#define __SPRT_IP_RECVTOS              0x1009
#define __SPRT_IP_MTU                  0x100a
#define __SPRT_IP_FREEBIND             0x100b
#define __SPRT_IP_IPSEC_POLICY         0x100c
#define __SPRT_IP_XFRM_POLICY          0x100d
#define __SPRT_IP_PASSSEC              0x100e
#define __SPRT_IP_TRANSPARENT          0x100f
#define __SPRT_IP_ORIGDSTADDR          0x1010
#define __SPRT_IP_RECVORIGDSTADDR      __SPRT_IP_ORIGDSTADDR
#define __SPRT_IP_MINTTL               0x1011
#define __SPRT_IP_NODEFRAG             0x1012
#define __SPRT_IP_CHECKSUM             0x1013
#define __SPRT_IP_BIND_ADDRESS_NO_PORT 0x1014
#define __SPRT_IP_RECVFRAGSIZE         0x1015
#define __SPRT_IP_RECVERR_RFC4884      0x1016
#define __SPRT_IP_UNICAST_IF           0x1017

#define __SPRT_IP_PMTUDISC_DONT   0
#define __SPRT_IP_PMTUDISC_WANT   1
#define __SPRT_IP_PMTUDISC_DO     2
#define __SPRT_IP_PMTUDISC_PROBE  3
#define __SPRT_IP_PMTUDISC_INTERFACE 4
#define __SPRT_IP_PMTUDISC_OMIT   5

#define __SPRT_IP_DEFAULT_MULTICAST_TTL  1
#define __SPRT_IP_DEFAULT_MULTICAST_LOOP 1
#define __SPRT_IP_MAX_MEMBERSHIPS        20

// MCAST_* (RFC 3678 protocol-independent multicast) - not in Embox.
#define __SPRT_MCAST_JOIN_GROUP         0x1030
#define __SPRT_MCAST_BLOCK_SOURCE       0x1031
#define __SPRT_MCAST_UNBLOCK_SOURCE     0x1032
#define __SPRT_MCAST_LEAVE_GROUP        0x1033
#define __SPRT_MCAST_JOIN_SOURCE_GROUP  0x1034
#define __SPRT_MCAST_LEAVE_SOURCE_GROUP 0x1035
#define __SPRT_MCAST_MSFILTER           0x1036

#define __SPRT_MCAST_EXCLUDE 0
#define __SPRT_MCAST_INCLUDE 1

// --- IPPROTO_IPV6 level -------------------------------------------------------
#define __SPRT_IPV6_JOIN_GROUP            22 // Linux: 20
#define __SPRT_IPV6_LEAVE_GROUP           21
#define __SPRT_IPV6_MULTICAST_HOPS        25 // Linux: 18
#define __SPRT_IPV6_MULTICAST_IF          20 // Linux: 17
#define __SPRT_IPV6_MULTICAST_LOOP        23 // Linux: 19
#define __SPRT_IPV6_UNICAST_HOPS          24 // Linux: 16
#define __SPRT_IPV6_V6ONLY                28 // Linux: 26
#define __SPRT_IPV6_PKTINFO               0x1068 // not in Embox
#define __SPRT_IPV6_RECVPKTINFO           0x1069 // not in Embox
#define __SPRT_IPV6_TCLASS                0x106a // not in Embox
#define __SPRT_IPV6_RECVHOPLIMIT          0x106b // not in Embox
#define __SPRT_IPV6_HOPLIMIT              0x106c // not in Embox

// Embox gives these their own numbers rather than aliasing JOIN/LEAVE_GROUP.
#define __SPRT_IPV6_ADD_MEMBERSHIP  26
#define __SPRT_IPV6_DROP_MEMBERSHIP 27

// Unimplemented by Embox.
#define __SPRT_IPV6_ADDRFORM             0x1040
#define __SPRT_IPV6_2292PKTINFO          0x1041
#define __SPRT_IPV6_2292HOPOPTS          0x1042
#define __SPRT_IPV6_2292DSTOPTS          0x1043
#define __SPRT_IPV6_2292RTHDR            0x1044
#define __SPRT_IPV6_2292PKTOPTIONS       0x1045
#define __SPRT_IPV6_CHECKSUM             0x1046
#define __SPRT_IPV6_2292HOPLIMIT         0x1047
#define __SPRT_IPV6_NEXTHOP              0x1048
#define __SPRT_IPV6_AUTHHDR              0x1049
#define __SPRT_IPV6_ROUTER_ALERT         0x104a
#define __SPRT_IPV6_MTU_DISCOVER         0x104b
#define __SPRT_IPV6_MTU                  0x104c
#define __SPRT_IPV6_RECVERR              0x104d
#define __SPRT_IPV6_JOIN_ANYCAST         0x104e
#define __SPRT_IPV6_LEAVE_ANYCAST        0x104f
#define __SPRT_IPV6_MULTICAST_ALL        0x1050
#define __SPRT_IPV6_ROUTER_ALERT_ISOLATE 0x1051
#define __SPRT_IPV6_IPSEC_POLICY         0x1052
#define __SPRT_IPV6_XFRM_POLICY          0x1053
#define __SPRT_IPV6_HDRINCL              0x1054
#define __SPRT_IPV6_RECVHOPOPTS          0x1055
#define __SPRT_IPV6_HOPOPTS              0x1056
#define __SPRT_IPV6_RXHOPOPTS            __SPRT_IPV6_HOPOPTS
#define __SPRT_IPV6_RTHDRDSTOPTS         0x1057
#define __SPRT_IPV6_RECVRTHDR            0x1058
#define __SPRT_IPV6_RTHDR                0x1059
#define __SPRT_IPV6_RECVDSTOPTS          0x105a
#define __SPRT_IPV6_DSTOPTS              0x105b
#define __SPRT_IPV6_RXDSTOPTS            __SPRT_IPV6_DSTOPTS
#define __SPRT_IPV6_RECVPATHMTU          0x105c
#define __SPRT_IPV6_PATHMTU              0x105d
#define __SPRT_IPV6_DONTFRAG             0x105e
#define __SPRT_IPV6_RECVTCLASS           0x105f
#define __SPRT_IPV6_AUTOFLOWLABEL        0x1060
#define __SPRT_IPV6_ADDR_PREFERENCES     0x1061
#define __SPRT_IPV6_MINHOPCOUNT          0x1062
#define __SPRT_IPV6_ORIGDSTADDR          0x1063
#define __SPRT_IPV6_RECVORIGDSTADDR      __SPRT_IPV6_ORIGDSTADDR
#define __SPRT_IPV6_TRANSPARENT          0x1064
#define __SPRT_IPV6_UNICAST_IF           0x1065
#define __SPRT_IPV6_RECVFRAGSIZE         0x1066
#define __SPRT_IPV6_FREEBIND             0x1067

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

#define __SPRT_IPV6_RTHDR_LOOSE  0
#define __SPRT_IPV6_RTHDR_STRICT 1
#define __SPRT_IPV6_RTHDR_TYPE_0 0

// --- IPPROTO_TCP level --------------------------------------------------------
#define __SPRT_TCP_NODELAY          1
#define __SPRT_TCP_KEEPIDLE         0x10a0 // not in Embox
#define __SPRT_TCP_KEEPINTVL        0x10a1 // not in Embox
#define __SPRT_TCP_KEEPCNT          0x10a2 // not in Embox
#define __SPRT_TCP_MAXSEG           2
#define __SPRT_TCP_CORK             0x10a3 // not in Embox

// Unimplemented by Embox.
#define __SPRT_TCP_SYNCNT               0x1080
#define __SPRT_TCP_LINGER2              0x1081
#define __SPRT_TCP_DEFER_ACCEPT         9 // Linux: 9
#define __SPRT_TCP_WINDOW_CLAMP         0x1083
#define __SPRT_TCP_INFO                 0x1084
#define __SPRT_TCP_QUICKACK             0x1085
#define __SPRT_TCP_CONGESTION           0x1086
#define __SPRT_TCP_MD5SIG               0x1087
#define __SPRT_TCP_MAX_WINSHIFT         0x1088
#define __SPRT_TCP_COOKIE_TRANSACTIONS  0x1089
#define __SPRT_TCP_THIN_LINEAR_TIMEOUTS 0x108a
#define __SPRT_TCP_THIN_DUPACK          0x108b
#define __SPRT_TCP_USER_TIMEOUT         0x108c
#define __SPRT_TCP_REPAIR               0x108d
#define __SPRT_TCP_REPAIR_QUEUE         0x108e
#define __SPRT_TCP_QUEUE_SEQ            0x108f
#define __SPRT_TCP_REPAIR_OPTIONS       0x1090
#define __SPRT_TCP_FASTOPEN             0x1091
#define __SPRT_TCP_TIMESTAMP            0x1092
#define __SPRT_TCP_NOTSENT_LOWAT        0x1093
#define __SPRT_TCP_CC_INFO              0x1094
#define __SPRT_TCP_SAVE_SYN             0x1095
#define __SPRT_TCP_SAVED_SYN            0x1096
#define __SPRT_TCP_REPAIR_WINDOW        0x1097
#define __SPRT_TCP_FASTOPEN_CONNECT     0x1098
#define __SPRT_TCP_ULP                  0x1099
#define __SPRT_TCP_MD5SIG_EXT           0x109a
#define __SPRT_TCP_FASTOPEN_KEY         0x109b
#define __SPRT_TCP_FASTOPEN_NO_COOKIE   0x109c
#define __SPRT_TCP_ZEROCOPY_RECEIVE     0x109d
#define __SPRT_TCP_INQ                  0x109e
#define __SPRT_TCP_CM_INQ               __SPRT_TCP_INQ
#define __SPRT_TCP_TX_DELAY             0x109f

// Not options - protocol constants and enum values, unchanged from the Linux table.
#define __SPRT_TCP_REPAIR_OFF_NO_WP -1
#define __SPRT_TCP_REPAIR_OFF 0
#define __SPRT_TCP_REPAIR_ON 1
#define __SPRT_TCP_COOKIE_IN_ALWAYS 1
#define __SPRT_TCP_COOKIE_OUT_NEVER 2
#define __SPRT_TCP_MD5SIG_FLAG_PREFIX 1
#define __SPRT_TCP_S_DATA_IN 4
#define __SPRT_TCP_S_DATA_OUT 8
#define __SPRT_TCP_COOKIE_MIN 8
#define __SPRT_TCP_COOKIE_MAX 16
#define __SPRT_TCP_COOKIE_PAIR_SIZE 32
#define __SPRT_TCP_MD5SIG_MAXKEYLEN 80
#define __SPRT_TCP_MSS 512
#define __SPRT_TCP_MSS_DEFAULT 536
#define __SPRT_TCP_MSS_DESIRED 1220
#define __SPRT_TCP_MAXWIN 65535
// clang-format on

#endif // CORE_RUNTIME_INCLUDE_C_CROSS_EMBOX_SPRT_NETINETDEF_H_
