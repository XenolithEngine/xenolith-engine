// clang-format off
// Embox socket constants. Like NuttX, Embox does not share the Linux SO_* table
// (SO_REUSEADDR is 11, not 2), and it assigns its own numbers to two of the
// address families it does support (PF_NETLINK is 18, PF_RDS is 19). SHUT_*,
// MSG_* and the IP-protocol SOL_* levels DO match Linux.
//
// __sprt_socket()/__sprt_setsockopt()/__sprt_shutdown() forward their arguments
// untranslated, and outside __SPRT_BUILD these macros ARE the application's SO_*/
// AF_*/MSG_*, so the numbers have to be Embox's own. Every one below is pinned in
// SPRuntimeCSysSocket.cpp against the native <sys/socket.h>.
//
// Names Embox does not implement are still defined (the <sys/socket.h> umbrella
// re-exports them unconditionally) but are moved to 0x1000+, clear of every range
// Embox assigns meaning to, so setsockopt() answers ENOPROTOOPT rather than acting
// on a different option. Their asserts are #ifdef'd on the native spelling.

#define __SPRT_SHUT_RD        0
#define __SPRT_SHUT_WR        1
#define __SPRT_SHUT_RDWR      2

#define __SPRT_SOCK_STREAM    1
#define __SPRT_SOCK_DGRAM     2
#define __SPRT_SOCK_RAW       3
#define __SPRT_SOCK_SEQPACKET 5
#define __SPRT_SOCK_PACKET    10
#define __SPRT_SOCK_RDM       20 // Linux: 4
// Embox has no SOCK_DCCP.
#define __SPRT_SOCK_DCCP      0x1000

// Embox's socket() takes a bare type, with no room for the Linux type flags. The
// bits below are outside its enum, so socket() rejects them - which is the right
// answer: a caller asking for a non-blocking socket must not silently get a
// blocking one.
#define __SPRT_SOCK_CLOEXEC   0x10000000
#define __SPRT_SOCK_NONBLOCK  0x20000000

#define __SPRT_SOL_SOCKET     1

// Socket-level options - Embox orders these alphabetically from 0, the same idea
// NuttX has but with a different tail.
#define __SPRT_SO_ACCEPTCONN   0  // Linux: 30
#define __SPRT_SO_BROADCAST    1  // Linux: 6
#define __SPRT_SO_DEBUG        2  // Linux: 1
#define __SPRT_SO_DONTROUTE    3  // Linux: 5
#define __SPRT_SO_ERROR        4
#define __SPRT_SO_KEEPALIVE    5  // Linux: 9
#define __SPRT_SO_LINGER       6  // Linux: 13
#define __SPRT_SO_OOBINLINE    7  // Linux: 10
#define __SPRT_SO_RCVBUF       8
#define __SPRT_SO_RCVLOWAT     9  // Linux: 18
#define __SPRT_SO_RCVTIMEO     10 // Linux: 20
#define __SPRT_SO_REUSEADDR    11 // Linux: 2
#define __SPRT_SO_SNDBUF       12 // Linux: 7
#define __SPRT_SO_SNDLOWAT     13 // Linux: 19
#define __SPRT_SO_SNDTIMEO     14 // Linux: 21
#define __SPRT_SO_TYPE         15 // Linux: 3
#define __SPRT_SO_BINDTODEVICE 16 // Linux: 25
#define __SPRT_SO_DOMAIN       17 // Linux: 39
#define __SPRT_SO_PROTOCOL     18 // Linux: 38
#define __SPRT_SO_SNDBUFFORCE  32
#define __SPRT_SO_RCVBUFFORCE  33
#define __SPRT_SO_TIMESTAMP    34 // Linux: 29
#define __SPRT_SO_RXQ_OVFL     35 // Linux: 40

// Unimplemented by Embox.
#define __SPRT_SO_REUSEPORT            0x1000
#define __SPRT_SO_NO_CHECK             0x1001
#define __SPRT_SO_PRIORITY             0x1002
#define __SPRT_SO_BSDCOMPAT            0x1003
#define __SPRT_SO_PASSCRED             0x1004
#define __SPRT_SO_PEERCRED             0x1005
#define __SPRT_SO_PEERSEC              0x1006
#define __SPRT_SO_TIMESTAMPNS          0x1007
#define __SPRT_SO_TIMESTAMPING         0x1008
#define __SPRT_SO_SECURITY_AUTHENTICATION       0x1009
#define __SPRT_SO_SECURITY_ENCRYPTION_TRANSPORT 0x100a
#define __SPRT_SO_SECURITY_ENCRYPTION_NETWORK   0x100b
#define __SPRT_SO_ATTACH_FILTER        0x100c
#define __SPRT_SO_DETACH_FILTER        0x100d
#define __SPRT_SO_GET_FILTER           __SPRT_SO_ATTACH_FILTER
#define __SPRT_SO_PEERNAME             0x100e
#define __SPRT_SO_PASSSEC              0x100f
#define __SPRT_SO_MARK                 0x1010
#define __SPRT_SO_WIFI_STATUS          0x1011
#define __SPRT_SO_PEEK_OFF             0x1012
#define __SPRT_SO_NOFCS                0x1013
#define __SPRT_SO_LOCK_FILTER          0x1014
#define __SPRT_SO_SELECT_ERR_QUEUE     0x1015
#define __SPRT_SO_BUSY_POLL            0x1016
#define __SPRT_SO_MAX_PACING_RATE      0x1017
#define __SPRT_SO_BPF_EXTENSIONS       0x1018
#define __SPRT_SO_INCOMING_CPU         0x1019
#define __SPRT_SO_ATTACH_BPF           0x101a
#define __SPRT_SO_DETACH_BPF           __SPRT_SO_DETACH_FILTER
#define __SPRT_SO_ATTACH_REUSEPORT_CBPF 0x101b
#define __SPRT_SO_ATTACH_REUSEPORT_EBPF 0x101c
#define __SPRT_SO_CNX_ADVICE           0x101d
#define __SPRT_SO_MEMINFO              0x101e
#define __SPRT_SO_INCOMING_NAPI_ID     0x101f
#define __SPRT_SO_COOKIE               0x1020
#define __SPRT_SO_PEERGROUPS           0x1021
#define __SPRT_SO_ZEROCOPY             0x1022
#define __SPRT_SO_TXTIME               0x1023
#define __SPRT_SO_BINDTOIFINDEX        0x1024
#define __SPRT_SO_DETACH_REUSEPORT_BPF 0x1025

// Address / protocol families. Embox matches Linux for the classic families but
// renumbers NETLINK and RDS.
#define __SPRT_PF_UNSPEC       0
#define __SPRT_PF_LOCAL        1
#define __SPRT_PF_UNIX         __SPRT_PF_LOCAL
#define __SPRT_PF_FILE         __SPRT_PF_LOCAL
#define __SPRT_PF_INET         2
#define __SPRT_PF_INET6        10
#define __SPRT_PF_PACKET       17
#define __SPRT_PF_NETLINK      18 // Linux: 16
#define __SPRT_PF_ROUTE        __SPRT_PF_NETLINK
#define __SPRT_PF_RDS          19 // Linux: 21
#define __SPRT_PF_CAN          29
#define __SPRT_PF_MAX          30 // one past PF_CAN, as Embox's enum ends

// Families Embox does not implement. Their Linux numbers are kept where they are
// free: unlike the SO_* options these are not "do something else" hazards -
// socket() rejects an unsupported domain outright - and keeping them makes the
// table readable next to linux_sprt/sockdef.h. The two whose Linux numbers land
// on a family Embox DOES support (ASH would be NETLINK, ECONET would be RDS) are
// moved out of range instead.
#define __SPRT_PF_AX25         3
#define __SPRT_PF_IPX          4
#define __SPRT_PF_APPLETALK    5
#define __SPRT_PF_NETROM       6
#define __SPRT_PF_BRIDGE       7
#define __SPRT_PF_ATMPVC       8
#define __SPRT_PF_X25          9
#define __SPRT_PF_ROSE         11
#define __SPRT_PF_DECnet       12
#define __SPRT_PF_NETBEUI      13
#define __SPRT_PF_SECURITY     14
#define __SPRT_PF_KEY          15
#define __SPRT_PF_ASH          0x1100 // Linux: 18, which is Embox's PF_NETLINK
#define __SPRT_PF_ECONET       0x1101 // Linux: 19, which is Embox's PF_RDS
#define __SPRT_PF_ATMSVC       20
#define __SPRT_PF_SNA          22
#define __SPRT_PF_IRDA         23
#define __SPRT_PF_PPPOX        24
#define __SPRT_PF_WANPIPE      25
#define __SPRT_PF_LLC          26
#define __SPRT_PF_IB           27
#define __SPRT_PF_MPLS         28
#define __SPRT_PF_BLUETOOTH    31
#define __SPRT_PF_TIPC         0x1102 // Linux: 30, which is Embox's PF_MAX
#define __SPRT_PF_IUCV         32
#define __SPRT_PF_RXRPC        33
#define __SPRT_PF_ISDN         34
#define __SPRT_PF_PHONET       35
#define __SPRT_PF_IEEE802154   36
#define __SPRT_PF_CAIF         37
#define __SPRT_PF_ALG          38
#define __SPRT_PF_NFC          39
#define __SPRT_PF_VSOCK        40
#define __SPRT_PF_KCM          41
#define __SPRT_PF_QIPCRTR      42
#define __SPRT_PF_SMC          43
#define __SPRT_PF_XDP          44

#define __SPRT_AF_UNSPEC       __SPRT_PF_UNSPEC
#define __SPRT_AF_UNIX         __SPRT_PF_UNIX
#define __SPRT_AF_LOCAL        __SPRT_PF_LOCAL
#define __SPRT_AF_FILE         __SPRT_PF_FILE
#define __SPRT_AF_INET         __SPRT_PF_INET
#define __SPRT_AF_INET6        __SPRT_PF_INET6
#define __SPRT_AF_NETLINK      __SPRT_PF_NETLINK
#define __SPRT_AF_ROUTE        __SPRT_PF_ROUTE
#define __SPRT_AF_PACKET       __SPRT_PF_PACKET
#define __SPRT_AF_CAN          __SPRT_PF_CAN
#define __SPRT_AF_BLUETOOTH    __SPRT_PF_BLUETOOTH
#define __SPRT_AF_IEEE802154   __SPRT_PF_IEEE802154
#define __SPRT_AF_VSOCK        __SPRT_PF_VSOCK
#define __SPRT_AF_MAX          __SPRT_PF_MAX
#define __SPRT_AF_AX25         __SPRT_PF_AX25
#define __SPRT_AF_IPX          __SPRT_PF_IPX
#define __SPRT_AF_APPLETALK    __SPRT_PF_APPLETALK
#define __SPRT_AF_NETROM       __SPRT_PF_NETROM
#define __SPRT_AF_BRIDGE       __SPRT_PF_BRIDGE
#define __SPRT_AF_ATMPVC       __SPRT_PF_ATMPVC
#define __SPRT_AF_X25          __SPRT_PF_X25
#define __SPRT_AF_ROSE         __SPRT_PF_ROSE
#define __SPRT_AF_DECnet       __SPRT_PF_DECnet
#define __SPRT_AF_NETBEUI      __SPRT_PF_NETBEUI
#define __SPRT_AF_SECURITY     __SPRT_PF_SECURITY
#define __SPRT_AF_KEY          __SPRT_PF_KEY
#define __SPRT_AF_ASH          __SPRT_PF_ASH
#define __SPRT_AF_ECONET       __SPRT_PF_ECONET
#define __SPRT_AF_ATMSVC       __SPRT_PF_ATMSVC
#define __SPRT_AF_RDS          __SPRT_PF_RDS
#define __SPRT_AF_SNA          __SPRT_PF_SNA
#define __SPRT_AF_IRDA         __SPRT_PF_IRDA
#define __SPRT_AF_PPPOX        __SPRT_PF_PPPOX
#define __SPRT_AF_WANPIPE      __SPRT_PF_WANPIPE
#define __SPRT_AF_LLC          __SPRT_PF_LLC
#define __SPRT_AF_IB           __SPRT_PF_IB
#define __SPRT_AF_MPLS         __SPRT_PF_MPLS
#define __SPRT_AF_TIPC         __SPRT_PF_TIPC
#define __SPRT_AF_IUCV         __SPRT_PF_IUCV
#define __SPRT_AF_RXRPC        __SPRT_PF_RXRPC
#define __SPRT_AF_ISDN         __SPRT_PF_ISDN
#define __SPRT_AF_PHONET       __SPRT_PF_PHONET
#define __SPRT_AF_CAIF         __SPRT_PF_CAIF
#define __SPRT_AF_ALG          __SPRT_PF_ALG
#define __SPRT_AF_NFC          __SPRT_PF_NFC
#define __SPRT_AF_KCM          __SPRT_PF_KCM
#define __SPRT_AF_QIPCRTR      __SPRT_PF_QIPCRTR
#define __SPRT_AF_SMC          __SPRT_PF_SMC
#define __SPRT_AF_XDP          __SPRT_PF_XDP

// send()/recv() flags - Embox mirrors Linux (it spells 0x10 MSG_PROBE where Linux
// says MSG_PROXY, and adds MSG_TRYHARD as an alias of MSG_DONTROUTE).
#define __SPRT_MSG_OOB          0x000001
#define __SPRT_MSG_PEEK         0x000002
#define __SPRT_MSG_DONTROUTE    0x000004
#define __SPRT_MSG_CTRUNC       0x000008
#define __SPRT_MSG_PROXY        0x000010
#define __SPRT_MSG_TRUNC        0x000020
#define __SPRT_MSG_DONTWAIT     0x000040
#define __SPRT_MSG_EOR          0x000080
#define __SPRT_MSG_WAITALL      0x000100
#define __SPRT_MSG_FIN          0x000200
#define __SPRT_MSG_SYN          0x000400
#define __SPRT_MSG_CONFIRM      0x000800
#define __SPRT_MSG_RST          0x001000
#define __SPRT_MSG_ERRQUEUE     0x002000
#define __SPRT_MSG_NOSIGNAL     0x004000
#define __SPRT_MSG_MORE         0x008000

// Unimplemented by Embox; kept at their Linux bits (unused there, and a stray flag
// bit is ignored rather than misread).
#define __SPRT_MSG_WAITFORONE   0x10000
#define __SPRT_MSG_BATCH        0x40000
#define __SPRT_MSG_ZEROCOPY     0x4000000
#define __SPRT_MSG_FASTOPEN     0x20000000
#define __SPRT_MSG_CMSG_CLOEXEC 0x40000000

// listen() backlog hint. Embox declares no SOMAXCONN at all; keep the value musl,
// bionic and Darwin use - listen() only takes it as advice.
#define __SPRT_SOMAXCONN 128

// Protocol levels. The IP-protocol-numbered ones agree with Linux; Embox has none
// of the Linux link-layer levels.
#define __SPRT_SOL_IP          0
#define __SPRT_SOL_TCP         6
#define __SPRT_SOL_UDP         17
#define __SPRT_SOL_IPV6        41
#define __SPRT_SOL_ICMPV6      58
#define __SPRT_SOL_RAW         255

#define __SPRT_SOL_PACKET      0x1100 // Linux: 263
#define __SPRT_SOL_DECNET      0x1101
#define __SPRT_SOL_X25         0x1102
#define __SPRT_SOL_ATM         0x1103
#define __SPRT_SOL_AAL         0x1104
#define __SPRT_SOL_IRDA        0x1105
#define __SPRT_SOL_NETBEUI     0x1106
#define __SPRT_SOL_LLC         0x1107
#define __SPRT_SOL_DCCP        0x1108
#define __SPRT_SOL_NETLINK     0x1109
#define __SPRT_SOL_TIPC        0x110a
#define __SPRT_SOL_RXRPC       0x110b
#define __SPRT_SOL_PPPOL2TP    0x110c
#define __SPRT_SOL_BLUETOOTH   0x110d
#define __SPRT_SOL_PNPIPE      0x110e
#define __SPRT_SOL_RDS         0x110f
#define __SPRT_SOL_IUCV        0x1110
#define __SPRT_SOL_CAIF        0x1111
#define __SPRT_SOL_ALG         0x1112
#define __SPRT_SOL_NFC         0x1113
#define __SPRT_SOL_KCM         0x1114
#define __SPRT_SOL_TLS         0x1115
#define __SPRT_SOL_XDP         0x1116

// Control-message types. Embox declares none of these; the values are sprt's own.
#define __SPRT_SCM_RIGHTS       0x01
#define __SPRT_SCM_CREDENTIALS  0x02
#define __SPRT_SCM_SECURITY     0x03
#define __SPRT_SCM_TIMESTAMP    __SPRT_SO_TIMESTAMP
#define __SPRT_SCM_TIMESTAMPNS  __SPRT_SO_TIMESTAMPNS
#define __SPRT_SCM_TIMESTAMPING __SPRT_SO_TIMESTAMPING
#define __SPRT_SCM_WIFI_STATUS  __SPRT_SO_WIFI_STATUS
#define __SPRT_SCM_TXTIME       __SPRT_SO_TXTIME
#define __SPRT_SCM_TIMESTAMPING_OPT_STATS 0x1200
#define __SPRT_SCM_TIMESTAMPING_PKTINFO   0x1201
// clang-format on
