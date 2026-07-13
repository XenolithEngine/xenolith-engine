// clang-format off
// Portable core socket constants, namespaced (__SPRT_*) so they are safe to include on
// every build; <sys/socket.h> expands the public names from these, and each is
// static_asserted against the native header in SPRuntimeCSysSocket.cpp.
#define __SPRT_SHUT_RD        0
#define __SPRT_SHUT_WR        1
#define __SPRT_SHUT_RDWR      2
#define __SPRT_SOCK_STREAM    1
#define __SPRT_SOCK_DGRAM     2
#define __SPRT_SOCK_RAW       3
#define __SPRT_SOCK_SEQPACKET 5
#define __SPRT_SOCK_CLOEXEC   02000000
#define __SPRT_SOCK_NONBLOCK  04000
#define __SPRT_AF_UNSPEC      0
#define __SPRT_AF_UNIX        1
#define __SPRT_AF_INET        2
#define __SPRT_AF_INET6       10
#define __SPRT_SOL_SOCKET     1
#define __SPRT_SO_REUSEADDR   2
#define __SPRT_SO_TYPE        3
#define __SPRT_SO_ERROR       4
#define __SPRT_SO_DONTROUTE   5
#define __SPRT_SO_BROADCAST   6
#define __SPRT_SO_SNDBUF      7
#define __SPRT_SO_RCVBUF      8
#define __SPRT_SO_KEEPALIVE   9
#define __SPRT_SO_OOBINLINE   10
#define __SPRT_SO_LINGER      13
#define __SPRT_SO_REUSEPORT   15
#define __SPRT_MSG_OOB        0x0001
#define __SPRT_MSG_PEEK       0x0002
#define __SPRT_MSG_DONTROUTE  0x0004
#define __SPRT_MSG_CTRUNC     0x0008
#define __SPRT_MSG_TRUNC      0x0020
#define __SPRT_MSG_DONTWAIT   0x0040
#define __SPRT_MSG_EOR        0x0080
#define __SPRT_MSG_WAITALL    0x0100
#define __SPRT_MSG_NOSIGNAL   0x4000
#define __SPRT_SOMAXCONN      128

// Platform-specific extras (namespaced with __SPRT_*) so they never collide with the
// native <sys/socket.h>. These are always visible; the wrapper static_asserts them
// against the native values in SPRuntimeCSysSocket.cpp.
#define __SPRT_SOCK_RDM       4
#define __SPRT_SOCK_DCCP      6
#define __SPRT_SOCK_PACKET    10


#define __SPRT_PF_UNSPEC       0
#define __SPRT_PF_LOCAL        1
#define __SPRT_PF_UNIX         __SPRT_PF_LOCAL
#define __SPRT_PF_FILE         __SPRT_PF_LOCAL
#define __SPRT_PF_INET         2
#define __SPRT_PF_AX25         3
#define __SPRT_PF_IPX          4
#define __SPRT_PF_APPLETALK    5
#define __SPRT_PF_NETROM       6
#define __SPRT_PF_BRIDGE       7
#define __SPRT_PF_ATMPVC       8
#define __SPRT_PF_X25          9
#define __SPRT_PF_INET6        10
#define __SPRT_PF_ROSE         11
#define __SPRT_PF_DECnet       12
#define __SPRT_PF_NETBEUI      13
#define __SPRT_PF_SECURITY     14
#define __SPRT_PF_KEY          15
#define __SPRT_PF_NETLINK      16
#define __SPRT_PF_ROUTE        __SPRT_PF_NETLINK
#define __SPRT_PF_PACKET       17
#define __SPRT_PF_ASH          18
#define __SPRT_PF_ECONET       19
#define __SPRT_PF_ATMSVC       20
#define __SPRT_PF_RDS          21
#define __SPRT_PF_SNA          22
#define __SPRT_PF_IRDA         23
#define __SPRT_PF_PPPOX        24
#define __SPRT_PF_WANPIPE      25
#define __SPRT_PF_LLC          26
#define __SPRT_PF_IB           27
#define __SPRT_PF_MPLS         28
#define __SPRT_PF_CAN          29
#define __SPRT_PF_TIPC         30
#define __SPRT_PF_BLUETOOTH    31
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
#define __SPRT_PF_MAX          45

#define __SPRT_AF_LOCAL        __SPRT_PF_LOCAL
#define __SPRT_AF_FILE         __SPRT_AF_LOCAL
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
#define __SPRT_AF_NETLINK      __SPRT_PF_NETLINK
#define __SPRT_AF_ROUTE        __SPRT_PF_ROUTE
#define __SPRT_AF_PACKET       __SPRT_PF_PACKET
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
#define __SPRT_AF_CAN          __SPRT_PF_CAN
#define __SPRT_AF_TIPC         __SPRT_PF_TIPC
#define __SPRT_AF_BLUETOOTH    __SPRT_PF_BLUETOOTH
#define __SPRT_AF_IUCV         __SPRT_PF_IUCV
#define __SPRT_AF_RXRPC        __SPRT_PF_RXRPC
#define __SPRT_AF_ISDN         __SPRT_PF_ISDN
#define __SPRT_AF_PHONET       __SPRT_PF_PHONET
#define __SPRT_AF_IEEE802154   __SPRT_PF_IEEE802154
#define __SPRT_AF_CAIF         __SPRT_PF_CAIF
#define __SPRT_AF_ALG          __SPRT_PF_ALG
#define __SPRT_AF_NFC          __SPRT_PF_NFC
#define __SPRT_AF_VSOCK        __SPRT_PF_VSOCK
#define __SPRT_AF_KCM          __SPRT_PF_KCM
#define __SPRT_AF_QIPCRTR      __SPRT_PF_QIPCRTR
#define __SPRT_AF_SMC          __SPRT_PF_SMC
#define __SPRT_AF_XDP          __SPRT_PF_XDP
#define __SPRT_AF_MAX          __SPRT_PF_MAX

#ifndef __SPRT_SO_DEBUG
#define __SPRT_SO_DEBUG        1
#define __SPRT_SO_NO_CHECK     11
#define __SPRT_SO_PRIORITY     12
#define __SPRT_SO_BSDCOMPAT    14
#define __SPRT_SO_PASSCRED     16
#define __SPRT_SO_PEERCRED     17
#define __SPRT_SO_RCVLOWAT     18
#define __SPRT_SO_SNDLOWAT     19
#define __SPRT_SO_ACCEPTCONN   30
#define __SPRT_SO_PEERSEC      31
#define __SPRT_SO_SNDBUFFORCE  32
#define __SPRT_SO_RCVBUFFORCE  33
#define __SPRT_SO_PROTOCOL     38
#define __SPRT_SO_DOMAIN       39
#endif

#ifndef __SPRT_SO_RCVTIMEO
#if __LONG_MAX == 0x7fffffff
#define __SPRT_SO_RCVTIMEO     66
#define __SPRT_SO_SNDTIMEO     67
#else
#define __SPRT_SO_RCVTIMEO     20
#define __SPRT_SO_SNDTIMEO     21
#endif
#endif

#ifndef __SPRT_SO_TIMESTAMP
#if __LONG_MAX == 0x7fffffff
#define __SPRT_SO_TIMESTAMP    63
#define __SPRT_SO_TIMESTAMPNS  64
#define __SPRT_SO_TIMESTAMPING 65
#else
#define __SPRT_SO_TIMESTAMP    29
#define __SPRT_SO_TIMESTAMPNS  35
#define __SPRT_SO_TIMESTAMPING 37
#endif
#endif

#define __SPRT_SO_SECURITY_AUTHENTICATION              22
#define __SPRT_SO_SECURITY_ENCRYPTION_TRANSPORT        23
#define __SPRT_SO_SECURITY_ENCRYPTION_NETWORK          24

#define __SPRT_SO_BINDTODEVICE 25

#define __SPRT_SO_ATTACH_FILTER        26
#define __SPRT_SO_DETACH_FILTER        27
#define __SPRT_SO_GET_FILTER           __SPRT_SO_ATTACH_FILTER

#define __SPRT_SO_PEERNAME             28
#define __SPRT_SCM_TIMESTAMP           __SPRT_SO_TIMESTAMP
#define __SPRT_SO_PASSSEC              34
#define __SPRT_SCM_TIMESTAMPNS         __SPRT_SO_TIMESTAMPNS
#define __SPRT_SO_MARK                 36
#define __SPRT_SCM_TIMESTAMPING        __SPRT_SO_TIMESTAMPING
#define __SPRT_SO_RXQ_OVFL             40
#define __SPRT_SO_WIFI_STATUS          41
#define __SPRT_SCM_WIFI_STATUS         __SPRT_SO_WIFI_STATUS
#define __SPRT_SO_PEEK_OFF             42
#define __SPRT_SO_NOFCS                43
#define __SPRT_SO_LOCK_FILTER          44
#define __SPRT_SO_SELECT_ERR_QUEUE     45
#define __SPRT_SO_BUSY_POLL            46
#define __SPRT_SO_MAX_PACING_RATE      47
#define __SPRT_SO_BPF_EXTENSIONS       48
#define __SPRT_SO_INCOMING_CPU         49
#define __SPRT_SO_ATTACH_BPF           50
#define __SPRT_SO_DETACH_BPF           __SPRT_SO_DETACH_FILTER
#define __SPRT_SO_ATTACH_REUSEPORT_CBPF 51
#define __SPRT_SO_ATTACH_REUSEPORT_EBPF 52
#define __SPRT_SO_CNX_ADVICE           53
#define __SPRT_SCM_TIMESTAMPING_OPT_STATS 54
#define __SPRT_SO_MEMINFO              55
#define __SPRT_SO_INCOMING_NAPI_ID     56
#define __SPRT_SO_COOKIE               57
#define __SPRT_SCM_TIMESTAMPING_PKTINFO 58
#define __SPRT_SO_PEERGROUPS           59
#define __SPRT_SO_ZEROCOPY             60
#define __SPRT_SO_TXTIME               61
#define __SPRT_SCM_TXTIME              __SPRT_SO_TXTIME
#define __SPRT_SO_BINDTOIFINDEX        62
#define __SPRT_SO_DETACH_REUSEPORT_BPF 68


#define __SPRT_SOL_IP          0
#define __SPRT_SOL_IPV6        41
#define __SPRT_SOL_ICMPV6      58

#define __SPRT_SOL_RAW         255
#define __SPRT_SOL_DECNET      261
#define __SPRT_SOL_X25         262
#define __SPRT_SOL_PACKET      263
#define __SPRT_SOL_ATM         264
#define __SPRT_SOL_AAL         265
#define __SPRT_SOL_IRDA        266
#define __SPRT_SOL_NETBEUI     267
#define __SPRT_SOL_LLC         268
#define __SPRT_SOL_DCCP        269
#define __SPRT_SOL_NETLINK     270
#define __SPRT_SOL_TIPC        271
#define __SPRT_SOL_RXRPC       272
#define __SPRT_SOL_PPPOL2TP    273
#define __SPRT_SOL_BLUETOOTH   274
#define __SPRT_SOL_PNPIPE      275
#define __SPRT_SOL_RDS         276
#define __SPRT_SOL_IUCV        277
#define __SPRT_SOL_CAIF        278
#define __SPRT_SOL_ALG         279
#define __SPRT_SOL_NFC         280
#define __SPRT_SOL_KCM         281
#define __SPRT_SOL_TLS         282
#define __SPRT_SOL_XDP         283


#define __SPRT_MSG_PROXY     0x0010
#define __SPRT_MSG_FIN       0x0200
#define __SPRT_MSG_SYN       0x0400
#define __SPRT_MSG_CONFIRM   0x0800
#define __SPRT_MSG_RST       0x1000
#define __SPRT_MSG_ERRQUEUE  0x2000
#define __SPRT_MSG_MORE      0x8000
#define __SPRT_MSG_WAITFORONE 0x10000
#define __SPRT_MSG_BATCH     0x40000
#define __SPRT_MSG_ZEROCOPY  0x4000000
#define __SPRT_MSG_FASTOPEN  0x20000000
#define __SPRT_MSG_CMSG_CLOEXEC 0x40000000

#define __SPRT_SCM_RIGHTS 0x01
#define __SPRT_SCM_CREDENTIALS 0x02

// clang-format on
