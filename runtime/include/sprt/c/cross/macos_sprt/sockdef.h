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
#define __SPRT_AF_INET6       30
#define __SPRT_SOL_SOCKET     0xffff
#define __SPRT_SO_REUSEADDR   0x0004
#define __SPRT_SO_TYPE        0x1008
#define __SPRT_SO_ERROR       0x1007
#define __SPRT_SO_DONTROUTE   0x0010
#define __SPRT_SO_BROADCAST   0x0020
#define __SPRT_SO_SNDBUF      0x1001
#define __SPRT_SO_RCVBUF      0x1002
#define __SPRT_SO_KEEPALIVE   0x0008
#define __SPRT_SO_OOBINLINE   0x0100
#define __SPRT_SO_LINGER      0x0080
#define __SPRT_SO_REUSEPORT   0x0200
#define __SPRT_MSG_OOB        0x1
#define __SPRT_MSG_PEEK       0x2
#define __SPRT_MSG_DONTROUTE  0x4
#define __SPRT_MSG_EOR        0x8
#define __SPRT_MSG_TRUNC      0x10
#define __SPRT_MSG_CTRUNC     0x20
#define __SPRT_MSG_WAITALL    0x40
#define __SPRT_MSG_DONTWAIT   0x80
#define __SPRT_MSG_NOSIGNAL   0x80000

// Platform-specific extras (namespaced with __SPRT_*) so they never collide with the
// native <sys/socket.h>. These are always visible; the wrapper static_asserts them
// against the native values in SPRuntimeCSysSocket.cpp.

/* getsockopt/setsockopt option names (Darwin values; mirror the __SPRT_SO_* above) */
#define __SPRT_SO_DEBUG        0x0001          /* turn on debugging info recording */
#define __SPRT_SO_ACCEPTCONN   0x0002          /* socket has had listen() */
#define __SPRT_SO_USELOOPBACK  0x0040          /* bypass hardware when possible */
#define __SPRT_SO_TIMESTAMP    0x0400          /* timestamp received dgram traffic */
#define __SPRT_SO_SNDLOWAT     0x1003          /* send low-water mark */
#define __SPRT_SO_RCVLOWAT     0x1004          /* receive low-water mark */
#define __SPRT_SO_SNDTIMEO     0x1005          /* send timeout */
#define __SPRT_SO_RCVTIMEO     0x1006          /* receive timeout */

#define __SPRT_AF_LOCAL        __SPRT_AF_UNIX         /* backward compatibility */
#define __SPRT_AF_IMPLINK      3               /* arpanet imp addresses */
#define __SPRT_AF_PUP          4               /* pup protocols: e.g. BSP */
#define __SPRT_AF_CHAOS        5               /* mit CHAOS protocols */
#define __SPRT_AF_NS           6               /* XEROX NS protocols */
#define __SPRT_AF_ISO          7               /* ISO protocols */
#define __SPRT_AF_OSI          __SPRT_AF_ISO
#define __SPRT_AF_ECMA         8               /* European computer manufacturers */
#define __SPRT_AF_DATAKIT      9               /* datakit protocols */
#define __SPRT_AF_CCITT        10              /* CCITT protocols, X.25 etc */
#define __SPRT_AF_SNA          11              /* IBM SNA */
#define __SPRT_AF_DECnet       12              /* DECnet */
#define __SPRT_AF_DLI          13              /* DEC Direct data link interface */
#define __SPRT_AF_LAT          14              /* LAT */
#define __SPRT_AF_HYLINK       15              /* NSC Hyperchannel */
#define __SPRT_AF_APPLETALK    16              /* Apple Talk */
#define __SPRT_AF_ROUTE        17              /* Internal Routing Protocol */
#define __SPRT_AF_LINK         18              /* Link layer interface */
#define __SPRT__AF_XTP         19              /* eXpress Transfer Protocol (no AF) */
#define __SPRT_AF_COIP         20              /* connection-oriented IP, aka ST II */
#define __SPRT_AF_CNT          21              /* Computer Network Technology */
#define __SPRT_AF_IPX          23              /* Novell Internet Protocol */
#define __SPRT_AF_SIP          24              /* Simple Internet Protocol */
#define __SPRT_AF_NDRV         27              /* Network Driver 'raw' access */
#define __SPRT_AF_ISDN         28              /* Integrated Services Digital Network */
#define __SPRT_AF_E164         __SPRT_AF_ISDN  /* CCITT E.164 recommendation */
#define __SPRT_AF_NATM         31              /* native ATM access */
#define __SPRT_AF_SYSTEM       32              /* Kernel event messages */
#define __SPRT_AF_NETBIOS      33              /* NetBIOS */
#define __SPRT_AF_PPP          34              /* PPP communication protocol */
#define __SPRT_AF_RESERVED_36  36              /* Reserved for internal usage */
#define __SPRT_AF_IEEE80211    37              /* IEEE 802.11 protocol */
#define __SPRT_AF_UTUN         38
#define __SPRT_AF_VSOCK        40              /* VM Sockets */
#define __SPRT_AF_MAX          41

#define __SPRT_PF_UNSPEC       __SPRT_AF_UNSPEC
#define __SPRT_PF_LOCAL        __SPRT_AF_LOCAL
#define __SPRT_PF_UNIX         __SPRT_PF_LOCAL   /* backward compatibility */
#define __SPRT_PF_INET         __SPRT_AF_INET
#define __SPRT_PF_IMPLINK      __SPRT_AF_IMPLINK
#define __SPRT_PF_PUP          __SPRT_AF_PUP
#define __SPRT_PF_CHAOS        __SPRT_AF_CHAOS
#define __SPRT_PF_NS           __SPRT_AF_NS
#define __SPRT_PF_ISO          __SPRT_AF_ISO
#define __SPRT_PF_OSI          __SPRT_AF_ISO
#define __SPRT_PF_ECMA         __SPRT_AF_ECMA
#define __SPRT_PF_DATAKIT      __SPRT_AF_DATAKIT
#define __SPRT_PF_CCITT        __SPRT_AF_CCITT
#define __SPRT_PF_SNA          __SPRT_AF_SNA
#define __SPRT_PF_KEY 29
#define __SPRT_PF_DECnet       __SPRT_AF_DECnet
#define __SPRT_PF_DLI          __SPRT_AF_DLI
#define __SPRT_PF_LAT          __SPRT_AF_LAT
#define __SPRT_PF_HYLINK       __SPRT_AF_HYLINK
#define __SPRT_PF_APPLETALK    __SPRT_AF_APPLETALK
#define __SPRT_PF_ROUTE        __SPRT_AF_ROUTE
#define __SPRT_PF_LINK         __SPRT_AF_LINK
#define __SPRT_PF_XTP          __SPRT__AF_XTP    /* really just proto family, no AF */
#define __SPRT_PF_COIP         __SPRT_AF_COIP
#define __SPRT_PF_CNT          __SPRT_AF_CNT
#define __SPRT_PF_SIP          __SPRT_AF_SIP
#define __SPRT_PF_IPX          __SPRT_AF_IPX     /* same format as AF_NS */
#define __SPRT_PF_NDRV         __SPRT_AF_NDRV
#define __SPRT_PF_ISDN         __SPRT_AF_ISDN
#define __SPRT_PF_INET6        __SPRT_AF_INET6
#define __SPRT_PF_NATM         __SPRT_AF_NATM
#define __SPRT_PF_SYSTEM       __SPRT_AF_SYSTEM
#define __SPRT_PF_NETBIOS      __SPRT_AF_NETBIOS
#define __SPRT_PF_PPP          __SPRT_AF_PPP
#define __SPRT_PF_RESERVED_36  __SPRT_AF_RESERVED_36
#define __SPRT_PF_UTUN         __SPRT_AF_UTUN
#define __SPRT_PF_VSOCK        __SPRT_AF_VSOCK
#define __SPRT_PF_MAX          __SPRT_AF_MAX

#define __SPRT_PF_VLAN         (0x766c616eU)  /* 'vlan' */
#define __SPRT_PF_BOND         (0x626f6e64U)  /* 'bond' */

#define __SPRT_NET_MAXID       __SPRT_AF_MAX
#define __SPRT_NET_RT_DUMP             1       /* dump; may limit to a.f. */
#define __SPRT_NET_RT_FLAGS            2       /* by flags, e.g. RESOLVING */
#define __SPRT_NET_RT_IFLIST           3       /* survey interface list */
#define __SPRT_NET_RT_STAT             4       /* routing statistics */
#define __SPRT_NET_RT_TRASH            5       /* routes not in table but not freed */
#define __SPRT_NET_RT_IFLIST2          6       /* interface list with addresses */
#define __SPRT_NET_RT_DUMP2            7       /* dump; may limit to a.f. */
#define __SPRT_NET_RT_FLAGS_PRIV       10
#define __SPRT_NET_RT_MAXID            11

#define __SPRT_MSG_EOF         0x100           /* data completes connection */
#define __SPRT_MSG_FLUSH       0x400           /* Start of 'hold' seq; dump so_temp, deprecated */
#define __SPRT_MSG_HOLD        0x800           /* Hold frag in so_temp, deprecated */
#define __SPRT_MSG_SEND        0x1000          /* Send the packet in so_temp, deprecated */
#define __SPRT_MSG_HAVEMORE    0x2000          /* Data ready to be read */
#define __SPRT_MSG_RCVMORE     0x4000          /* Data remains in current pkt */
#define __SPRT_MSG_NEEDSA      0x10000         /* Fail receive if socket address cannot be allocated */
#define __SPRT_MSG_USEUPCALL   0x80000000      /* Inherit upcall in sock_accept */

#define __SPRT_SCM_RIGHTS                      0x01    /* access rights (array of int) */
#define __SPRT_SCM_TIMESTAMP                   0x02    /* timestamp (struct timeval) */
#define __SPRT_SCM_CREDS                       0x03    /* process creds (struct cmsgcred) */
#define __SPRT_SCM_TIMESTAMP_MONOTONIC         0x04    /* timestamp (uint64_t) */

#define __SPRT_SOCK_RDM        4               /* reliably-delivered message */
// clang-format on
