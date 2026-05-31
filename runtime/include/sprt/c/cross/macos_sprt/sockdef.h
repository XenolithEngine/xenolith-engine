// clang-format off
#define SOL_SOCKET      0xffff          /* options for socket level */
#define AF_UNSPEC       0               /* unspecified */
#define AF_UNIX         1               /* local to host (pipes) */
#define AF_LOCAL        AF_UNIX         /* backward compatibility */
#define AF_INET         2               /* internetwork: UDP, TCP, etc. */
#define AF_IMPLINK      3               /* arpanet imp addresses */
#define AF_PUP          4               /* pup protocols: e.g. BSP */
#define AF_CHAOS        5               /* mit CHAOS protocols */
#define AF_NS           6               /* XEROX NS protocols */
#define AF_ISO          7               /* ISO protocols */
#define AF_OSI          AF_ISO
#define AF_ECMA         8               /* European computer manufacturers */
#define AF_DATAKIT      9               /* datakit protocols */
#define AF_CCITT        10              /* CCITT protocols, X.25 etc */
#define AF_SNA          11              /* IBM SNA */
#define AF_DECnet       12              /* DECnet */
#define AF_DLI          13              /* DEC Direct data link interface */
#define AF_LAT          14              /* LAT */
#define AF_HYLINK       15              /* NSC Hyperchannel */
#define AF_APPLETALK    16              /* Apple Talk */
#define AF_ROUTE        17              /* Internal Routing Protocol */
#define AF_LINK         18              /* Link layer interface */
#define pseudo_AF_XTP   19              /* eXpress Transfer Protocol (no AF) */
#define AF_COIP         20              /* connection-oriented IP, aka ST II */
#define AF_CNT          21              /* Computer Network Technology */
#define pseudo_AF_RTIP  22              /* Help Identify RTIP packets */
#define AF_IPX          23              /* Novell Internet Protocol */
#define AF_SIP          24              /* Simple Internet Protocol */
#define pseudo_AF_PIP   25              /* Help Identify PIP packets */
#define AF_NDRV         27              /* Network Driver 'raw' access */
#define AF_ISDN         28              /* Integrated Services Digital Network */
#define AF_E164         AF_ISDN         /* CCITT E.164 recommendation */
#define pseudo_AF_KEY   29              /* Internal key-management function */
#define AF_INET6        30              /* IPv6 */
#define AF_NATM         31              /* native ATM access */
#define AF_SYSTEM       32              /* Kernel event messages */
#define AF_NETBIOS      33              /* NetBIOS */
#define AF_PPP          34              /* PPP communication protocol */
#define AF_RESERVED_36  36              /* Reserved for internal usage */
#define AF_IEEE80211    37              /* IEEE 802.11 protocol */
#define AF_UTUN         38
#define AF_VSOCK        40              /* VM Sockets */
#define AF_MAX          41

#define PF_UNSPEC       AF_UNSPEC
#define PF_LOCAL        AF_LOCAL
#define PF_UNIX         PF_LOCAL        /* backward compatibility */
#define PF_INET         AF_INET
#define PF_IMPLINK      AF_IMPLINK
#define PF_PUP          AF_PUP
#define PF_CHAOS        AF_CHAOS
#define PF_NS           AF_NS
#define PF_ISO          AF_ISO
#define PF_OSI          AF_ISO
#define PF_ECMA         AF_ECMA
#define PF_DATAKIT      AF_DATAKIT
#define PF_CCITT        AF_CCITT
#define PF_SNA          AF_SNA
#define PF_DECnet       AF_DECnet
#define PF_DLI          AF_DLI
#define PF_LAT          AF_LAT
#define PF_HYLINK       AF_HYLINK
#define PF_APPLETALK    AF_APPLETALK
#define PF_ROUTE        AF_ROUTE
#define PF_LINK         AF_LINK
#define PF_XTP          pseudo_AF_XTP   /* really just proto family, no AF */
#define PF_COIP         AF_COIP
#define PF_CNT          AF_CNT
#define PF_SIP          AF_SIP
#define PF_IPX          AF_IPX          /* same format as AF_NS */
#define PF_RTIP         pseudo_AF_RTIP  /* same format as AF_INET */
#define PF_PIP          pseudo_AF_PIP
#define PF_NDRV         AF_NDRV
#define PF_ISDN         AF_ISDN
#define PF_KEY          pseudo_AF_KEY
#define PF_INET6        AF_INET6
#define PF_NATM         AF_NATM
#define PF_SYSTEM       AF_SYSTEM
#define PF_NETBIOS      AF_NETBIOS
#define PF_PPP          AF_PPP
#define PF_RESERVED_36  AF_RESERVED_36
#define PF_UTUN         AF_UTUN
#define PF_VSOCK        AF_VSOCK
#define PF_MAX          AF_MAX

#define PF_VLAN         (0x766c616eU)  /* 'vlan' */
#define PF_BOND         (0x626f6e64U)  /* 'bond' */

#define NET_MAXID       AF_MAX
#define NET_RT_DUMP             1       /* dump; may limit to a.f. */
#define NET_RT_FLAGS            2       /* by flags, e.g. RESOLVING */
#define NET_RT_IFLIST           3       /* survey interface list */
#define NET_RT_STAT             4       /* routing statistics */
#define NET_RT_TRASH            5       /* routes not in table but not freed */
#define NET_RT_IFLIST2          6       /* interface list with addresses */
#define NET_RT_DUMP2            7       /* dump; may limit to a.f. */
#define NET_RT_FLAGS_PRIV       10
#define NET_RT_MAXID            11

#define SOMAXCONN       128

#define MSG_OOB         0x1             /* process out-of-band data */
#define MSG_PEEK        0x2             /* peek at incoming message */
#define MSG_DONTROUTE   0x4             /* send without using routing tables */
#define MSG_EOR         0x8             /* data completes record */
#define MSG_TRUNC       0x10            /* data discarded before delivery */
#define MSG_CTRUNC      0x20            /* control data lost before delivery */
#define MSG_WAITALL     0x40            /* wait for full request or error */
#define MSG_DONTWAIT    0x80            /* this message should be nonblocking */
#define MSG_EOF         0x100           /* data completes connection */
#define MSG_FLUSH       0x400           /* Start of 'hold' seq; dump so_temp, deprecated */
#define MSG_HOLD        0x800           /* Hold frag in so_temp, deprecated */
#define MSG_SEND        0x1000          /* Send the packet in so_temp, deprecated */
#define MSG_HAVEMORE    0x2000          /* Data ready to be read */
#define MSG_RCVMORE     0x4000          /* Data remains in current pkt */
#define MSG_NEEDSA      0x10000         /* Fail receive if socket address cannot be allocated */
#define MSG_NOSIGNAL    0x80000         /* do not generate SIGPIPE on EOF */
#define MSG_USEUPCALL   0x80000000      /* Inherit upcall in sock_accept */

#define SCM_RIGHTS                      0x01    /* access rights (array of int) */
#define SCM_TIMESTAMP                   0x02    /* timestamp (struct timeval) */
#define SCM_CREDS                       0x03    /* process creds (struct cmsgcred) */
#define SCM_TIMESTAMP_MONOTONIC         0x04    /* timestamp (uint64_t) */

#define SHUT_RD         0               /* shut down the reading side */
#define SHUT_WR         1               /* shut down the writing side */
#define SHUT_RDWR       2               /* shut down both sides */
// clang-format on
