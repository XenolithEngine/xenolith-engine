#ifndef CORE_RUNTIME_INCLUDE_LIBC_SYS___SOCKDEF_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SYS___SOCKDEF_H_

#include <sprt/c/sys/__sprt_socket.h>

// Portable socket constants: expand the public POSIX names from the namespaced __SPRT_*
// that the per-platform sockdef.h defines. This branch is the freestanding / non-__SPRT_BUILD
// surface (a hosted __SPRT_BUILD takes the #include_next above and gets these from native).
#define SHUT_RD        __SPRT_SHUT_RD
#define SHUT_WR        __SPRT_SHUT_WR
#define SHUT_RDWR      __SPRT_SHUT_RDWR
#define SOCK_STREAM    __SPRT_SOCK_STREAM
#define SOCK_DGRAM     __SPRT_SOCK_DGRAM
#define SOCK_RAW       __SPRT_SOCK_RAW
#define SOCK_SEQPACKET __SPRT_SOCK_SEQPACKET
#define SOCK_CLOEXEC   __SPRT_SOCK_CLOEXEC
#define SOCK_NONBLOCK  __SPRT_SOCK_NONBLOCK
#define AF_UNSPEC      __SPRT_AF_UNSPEC
#define AF_UNIX        __SPRT_AF_UNIX
#define AF_INET        __SPRT_AF_INET
#define AF_INET6       __SPRT_AF_INET6
#define SOL_SOCKET     __SPRT_SOL_SOCKET
#define SO_REUSEADDR   __SPRT_SO_REUSEADDR
#define SO_TYPE        __SPRT_SO_TYPE
#define SO_ERROR       __SPRT_SO_ERROR
#define SO_DONTROUTE   __SPRT_SO_DONTROUTE
#define SO_BROADCAST   __SPRT_SO_BROADCAST
#define SO_SNDBUF      __SPRT_SO_SNDBUF
#define SO_RCVBUF      __SPRT_SO_RCVBUF
#define SO_KEEPALIVE   __SPRT_SO_KEEPALIVE
#define SO_OOBINLINE   __SPRT_SO_OOBINLINE
#define SO_LINGER      __SPRT_SO_LINGER
#define SO_REUSEPORT   __SPRT_SO_REUSEPORT
#define MSG_OOB        __SPRT_MSG_OOB
#define MSG_PEEK       __SPRT_MSG_PEEK
#define MSG_DONTROUTE  __SPRT_MSG_DONTROUTE
#define MSG_CTRUNC     __SPRT_MSG_CTRUNC
#define MSG_TRUNC      __SPRT_MSG_TRUNC
#define MSG_DONTWAIT   __SPRT_MSG_DONTWAIT
#define MSG_EOR        __SPRT_MSG_EOR
#define MSG_WAITALL    __SPRT_MSG_WAITALL
#define MSG_NOSIGNAL   __SPRT_MSG_NOSIGNAL
#define SOMAXCONN      __SPRT_SOMAXCONN

// Platform-specific constants: materialize plain names from __SPRT_* where defined.
// Every mapping is guarded so cross-platform code only gets what the target supports.

// --- AF_* extended (all platforms) ----------------------------------------
#ifdef __SPRT_AF_LOCAL
#ifndef AF_LOCAL
#define AF_LOCAL       __SPRT_AF_LOCAL
#endif
#endif
#ifdef __SPRT_AF_IMPLINK
#ifndef AF_IMPLINK
#define AF_IMPLINK     __SPRT_AF_IMPLINK
#endif
#endif
#ifdef __SPRT_AF_PUP
#ifndef AF_PUP
#define AF_PUP         __SPRT_AF_PUP
#endif
#endif
#ifdef __SPRT_AF_CHAOS
#ifndef AF_CHAOS
#define AF_CHAOS       __SPRT_AF_CHAOS
#endif
#endif
#ifdef __SPRT_AF_NS
#ifndef AF_NS
#define AF_NS          __SPRT_AF_NS
#endif
#endif
#ifdef __SPRT_AF_IPX
#ifndef AF_IPX
#define AF_IPX         __SPRT_AF_IPX
#endif
#endif
#ifdef __SPRT_AF_ISO
#ifndef AF_ISO
#define AF_ISO         __SPRT_AF_ISO
#endif
#endif
#ifdef __SPRT_AF_OSI
#ifndef AF_OSI
#define AF_OSI         __SPRT_AF_OSI
#endif
#endif
#ifdef __SPRT_AF_ECMA
#ifndef AF_ECMA
#define AF_ECMA        __SPRT_AF_ECMA
#endif
#endif
#ifdef __SPRT_AF_DATAKIT
#ifndef AF_DATAKIT
#define AF_DATAKIT     __SPRT_AF_DATAKIT
#endif
#endif
#ifdef __SPRT_AF_CCITT
#ifndef AF_CCITT
#define AF_CCITT       __SPRT_AF_CCITT
#endif
#endif
#ifdef __SPRT_AF_SNA
#ifndef AF_SNA
#define AF_SNA         __SPRT_AF_SNA
#endif
#endif
#ifdef __SPRT_AF_DECnet
#ifndef AF_DECnet
#define AF_DECnet      __SPRT_AF_DECnet
#endif
#endif
#ifdef __SPRT_AF_DLI
#ifndef AF_DLI
#define AF_DLI         __SPRT_AF_DLI
#endif
#endif
#ifdef __SPRT_AF_LAT
#ifndef AF_LAT
#define AF_LAT         __SPRT_AF_LAT
#endif
#endif
#ifdef __SPRT_AF_HYLINK
#ifndef AF_HYLINK
#define AF_HYLINK      __SPRT_AF_HYLINK
#endif
#endif
#ifdef __SPRT_AF_APPLETALK
#ifndef AF_APPLETALK
#define AF_APPLETALK   __SPRT_AF_APPLETALK
#endif
#endif
#ifdef __SPRT_AF_ROUTE
#ifndef AF_ROUTE
#define AF_ROUTE       __SPRT_AF_ROUTE
#endif
#endif
#ifdef __SPRT_AF_LINK
#ifndef AF_LINK
#define AF_LINK        __SPRT_AF_LINK
#endif
#endif
#ifdef __SPRT_AF_NETBIOS
#ifndef AF_NETBIOS
#define AF_NETBIOS     __SPRT_AF_NETBIOS
#endif
#endif
#ifdef __SPRT_AF_AX25
#ifndef AF_AX25
#define AF_AX25        __SPRT_AF_AX25
#endif
#endif
#ifdef __SPRT_AF_NETROM
#ifndef AF_NETROM
#define AF_NETROM      __SPRT_AF_NETROM
#endif
#endif
#ifdef __SPRT_AF_BRIDGE
#ifndef AF_BRIDGE
#define AF_BRIDGE      __SPRT_AF_BRIDGE
#endif
#endif
#ifdef __SPRT_AF_ATMPVC
#ifndef AF_ATMPVC
#define AF_ATMPVC      __SPRT_AF_ATMPVC
#endif
#endif
#ifdef __SPRT_AF_X25
#ifndef AF_X25
#define AF_X25         __SPRT_AF_X25
#endif
#endif
#ifdef __SPRT_AF_ROSE
#ifndef AF_ROSE
#define AF_ROSE        __SPRT_AF_ROSE
#endif
#endif
#ifdef __SPRT_AF_NETBEUI
#ifndef AF_NETBEUI
#define AF_NETBEUI     __SPRT_AF_NETBEUI
#endif
#endif
#ifdef __SPRT_AF_SECURITY
#ifndef AF_SECURITY
#define AF_SECURITY    __SPRT_AF_SECURITY
#endif
#endif
#ifdef __SPRT_AF_KEY
#ifndef AF_KEY
#define AF_KEY         __SPRT_AF_KEY
#endif
#endif
#ifdef __SPRT_AF_NETLINK
#ifndef AF_NETLINK
#define AF_NETLINK     __SPRT_AF_NETLINK
#endif
#endif
#ifdef __SPRT_AF_PACKET
#ifndef AF_PACKET
#define AF_PACKET      __SPRT_AF_PACKET
#endif
#endif
#ifdef __SPRT_AF_ASH
#ifndef AF_ASH
#define AF_ASH         __SPRT_AF_ASH
#endif
#endif
#ifdef __SPRT_AF_ECONET
#ifndef AF_ECONET
#define AF_ECONET      __SPRT_AF_ECONET
#endif
#endif
#ifdef __SPRT_AF_ATMSVC
#ifndef AF_ATMSVC
#define AF_ATMSVC      __SPRT_AF_ATMSVC
#endif
#endif
#ifdef __SPRT_AF_RDS
#ifndef AF_RDS
#define AF_RDS         __SPRT_AF_RDS
#endif
#endif
#ifdef __SPRT_AF_IRDA
#ifndef AF_IRDA
#define AF_IRDA        __SPRT_AF_IRDA
#endif
#endif
#ifdef __SPRT_AF_PPPOX
#ifndef AF_PPPOX
#define AF_PPPOX       __SPRT_AF_PPPOX
#endif
#endif
#ifdef __SPRT_AF_WANPIPE
#ifndef AF_WANPIPE
#define AF_WANPIPE     __SPRT_AF_WANPIPE
#endif
#endif
#ifdef __SPRT_AF_LLC
#ifndef AF_LLC
#define AF_LLC         __SPRT_AF_LLC
#endif
#endif
#ifdef __SPRT_AF_CAN
#ifndef AF_CAN
#define AF_CAN         __SPRT_AF_CAN
#endif
#endif
#ifdef __SPRT_AF_TIPC
#ifndef AF_TIPC
#define AF_TIPC        __SPRT_AF_TIPC
#endif
#endif
#ifdef __SPRT_AF_BLUETOOTH
#ifndef AF_BLUETOOTH
#define AF_BLUETOOTH   __SPRT_AF_BLUETOOTH
#endif
#endif
#ifdef __SPRT_AF_IUCV
#ifndef AF_IUCV
#define AF_IUCV        __SPRT_AF_IUCV
#endif
#endif
#ifdef __SPRT_AF_RXRPC
#ifndef AF_RXRPC
#define AF_RXRPC       __SPRT_AF_RXRPC
#endif
#endif
#ifdef __SPRT_AF_PHONET
#ifndef AF_PHONET
#define AF_PHONET      __SPRT_AF_PHONET
#endif
#endif
#ifdef __SPRT_AF_IEEE802154
#ifndef AF_IEEE802154
#define AF_IEEE802154  __SPRT_AF_IEEE802154
#endif
#endif
#ifdef __SPRT_AF_CAIF
#ifndef AF_CAIF
#define AF_CAIF        __SPRT_AF_CAIF
#endif
#endif
#ifdef __SPRT_AF_ALG
#ifndef AF_ALG
#define AF_ALG         __SPRT_AF_ALG
#endif
#endif
#ifdef __SPRT_AF_NFC
#ifndef AF_NFC
#define AF_NFC         __SPRT_AF_NFC
#endif
#endif
#ifdef __SPRT_AF_KCM
#ifndef AF_KCM
#define AF_KCM         __SPRT_AF_KCM
#endif
#endif
#ifdef __SPRT_AF_QIPCRTR
#ifndef AF_QIPCRTR
#define AF_QIPCRTR     __SPRT_AF_QIPCRTR
#endif
#endif
#ifdef __SPRT_AF_VSOCK
#ifndef AF_VSOCK
#define AF_VSOCK       __SPRT_AF_VSOCK
#endif
#endif
#ifdef __SPRT_AF_MAX
#ifndef AF_MAX
#define AF_MAX         __SPRT_AF_MAX
#endif
#endif

// --- PF_* extended (all platforms) ----------------------------------------
#ifdef __SPRT_PF_UNSPEC
#ifndef PF_UNSPEC
#define PF_UNSPEC      __SPRT_PF_UNSPEC
#endif
#endif
#ifdef __SPRT_PF_UNIX
#ifndef PF_UNIX
#define PF_UNIX        __SPRT_PF_UNIX
#endif
#endif
#ifdef __SPRT_PF_LOCAL
#ifndef PF_LOCAL
#define PF_LOCAL       __SPRT_PF_LOCAL
#endif
#endif
#ifdef __SPRT_PF_INET
#ifndef PF_INET
#define PF_INET        __SPRT_PF_INET
#endif
#endif
#ifdef __SPRT_PF_INET6
#ifndef PF_INET6
#define PF_INET6       __SPRT_PF_INET6
#endif
#endif
#ifdef __SPRT_PF_IMPLINK
#ifndef PF_IMPLINK
#define PF_IMPLINK     __SPRT_PF_IMPLINK
#endif
#endif
#ifdef __SPRT_PF_PUP
#ifndef PF_PUP
#define PF_PUP         __SPRT_PF_PUP
#endif
#endif
#ifdef __SPRT_PF_CHAOS
#ifndef PF_CHAOS
#define PF_CHAOS       __SPRT_PF_CHAOS
#endif
#endif
#ifdef __SPRT_PF_NS
#ifndef PF_NS
#define PF_NS          __SPRT_PF_NS
#endif
#endif
#ifdef __SPRT_PF_IPX
#ifndef PF_IPX
#define PF_IPX         __SPRT_PF_IPX
#endif
#endif
#ifdef __SPRT_PF_ISO
#ifndef PF_ISO
#define PF_ISO         __SPRT_PF_ISO
#endif
#endif
#ifdef __SPRT_PF_OSI
#ifndef PF_OSI
#define PF_OSI         __SPRT_PF_OSI
#endif
#endif
#ifdef __SPRT_PF_ECMA
#ifndef PF_ECMA
#define PF_ECMA        __SPRT_PF_ECMA
#endif
#endif
#ifdef __SPRT_PF_DATAKIT
#ifndef PF_DATAKIT
#define PF_DATAKIT     __SPRT_PF_DATAKIT
#endif
#endif
#ifdef __SPRT_PF_CCITT
#ifndef PF_CCITT
#define PF_CCITT       __SPRT_PF_CCITT
#endif
#endif
#ifdef __SPRT_PF_SNA
#ifndef PF_SNA
#define PF_SNA         __SPRT_PF_SNA
#endif
#endif
#ifdef __SPRT_PF_DECnet
#ifndef PF_DECnet
#define PF_DECnet      __SPRT_PF_DECnet
#endif
#endif
#ifdef __SPRT_PF_DLI
#ifndef PF_DLI
#define PF_DLI         __SPRT_PF_DLI
#endif
#endif
#ifdef __SPRT_PF_LAT
#ifndef PF_LAT
#define PF_LAT         __SPRT_PF_LAT
#endif
#endif
#ifdef __SPRT_PF_HYLINK
#ifndef PF_HYLINK
#define PF_HYLINK      __SPRT_PF_HYLINK
#endif
#endif
#ifdef __SPRT_PF_APPLETALK
#ifndef PF_APPLETALK
#define PF_APPLETALK   __SPRT_PF_APPLETALK
#endif
#endif
#ifdef __SPRT_PF_ROUTE
#ifndef PF_ROUTE
#define PF_ROUTE       __SPRT_PF_ROUTE
#endif
#endif
#ifdef __SPRT_PF_LINK
#ifndef PF_LINK
#define PF_LINK        __SPRT_PF_LINK
#endif
#endif
#ifdef __SPRT_PF_NETBIOS
#ifndef PF_NETBIOS
#define PF_NETBIOS     __SPRT_PF_NETBIOS
#endif
#endif
#ifdef __SPRT_PF_KEY
#ifndef PF_KEY
#define PF_KEY         __SPRT_PF_KEY
#endif
#endif
#ifdef __SPRT_PF_ASH
#ifndef PF_ASH
#define PF_ASH         __SPRT_PF_ASH
#endif
#endif
#ifdef __SPRT_PF_ECONET
#ifndef PF_ECONET
#define PF_ECONET      __SPRT_PF_ECONET
#endif
#endif
#ifdef __SPRT_PF_ATMSVC
#ifndef PF_ATMSVC
#define PF_ATMSVC      __SPRT_PF_ATMSVC
#endif
#endif
#ifdef __SPRT_PF_RDS
#ifndef PF_RDS
#define PF_RDS         __SPRT_PF_RDS
#endif
#endif
#ifdef __SPRT_PF_IRDA
#ifndef PF_IRDA
#define PF_IRDA        __SPRT_PF_IRDA
#endif
#endif
#ifdef __SPRT_PF_PPPOX
#ifndef PF_PPPOX
#define PF_PPPOX       __SPRT_PF_PPPOX
#endif
#endif
#ifdef __SPRT_PF_WANPIPE
#ifndef PF_WANPIPE
#define PF_WANPIPE     __SPRT_PF_WANPIPE
#endif
#endif
#ifdef __SPRT_PF_LLC
#ifndef PF_LLC
#define PF_LLC         __SPRT_PF_LLC
#endif
#endif
#ifdef __SPRT_PF_CAN
#ifndef PF_CAN
#define PF_CAN         __SPRT_PF_CAN
#endif
#endif
#ifdef __SPRT_PF_TIPC
#ifndef PF_TIPC
#define PF_TIPC        __SPRT_PF_TIPC
#endif
#endif
#ifdef __SPRT_PF_BLUETOOTH
#ifndef PF_BLUETOOTH
#define PF_BLUETOOTH   __SPRT_PF_BLUETOOTH
#endif
#endif
#ifdef __SPRT_PF_IUCV
#ifndef PF_IUCV
#define PF_IUCV        __SPRT_PF_IUCV
#endif
#endif
#ifdef __SPRT_PF_RXRPC
#ifndef PF_RXRPC
#define PF_RXRPC       __SPRT_PF_RXRPC
#endif
#endif
#ifdef __SPRT_PF_PHONET
#ifndef PF_PHONET
#define PF_PHONET      __SPRT_PF_PHONET
#endif
#endif
#ifdef __SPRT_PF_IEEE802154
#ifndef PF_IEEE802154
#define PF_IEEE802154  __SPRT_PF_IEEE802154
#endif
#endif
#ifdef __SPRT_PF_CAIF
#ifndef PF_CAIF
#define PF_CAIF        __SPRT_PF_CAIF
#endif
#endif
#ifdef __SPRT_PF_ALG
#ifndef PF_ALG
#define PF_ALG         __SPRT_PF_ALG
#endif
#endif
#ifdef __SPRT_PF_NFC
#ifndef PF_NFC
#define PF_NFC         __SPRT_PF_NFC
#endif
#endif
#ifdef __SPRT_PF_KCM
#ifndef PF_KCM
#define PF_KCM         __SPRT_PF_KCM
#endif
#endif
#ifdef __SPRT_PF_QIPCRTR
#ifndef PF_QIPCRTR
#define PF_QIPCRTR     __SPRT_PF_QIPCRTR
#endif
#endif
#ifdef __SPRT_PF_VSOCK
#ifndef PF_VSOCK
#define PF_VSOCK       __SPRT_PF_VSOCK
#endif
#endif
#ifdef __SPRT_PF_MAX
#ifndef PF_MAX
#define PF_MAX         __SPRT_PF_MAX
#endif
#endif

// --- SOL_* extended -------------------------------------------------------
#ifdef __SPRT_SOL_IP
#ifndef SOL_IP
#define SOL_IP         __SPRT_SOL_IP
#endif
#endif
#ifdef __SPRT_SOL_IPV6
#ifndef SOL_IPV6
#define SOL_IPV6       __SPRT_SOL_IPV6
#endif
#endif
#ifdef __SPRT_SOL_ICMPV6
#ifndef SOL_ICMPV6
#define SOL_ICMPV6     __SPRT_SOL_ICMPV6
#endif
#endif
#ifdef __SPRT_SOL_RAW
#ifndef SOL_RAW
#define SOL_RAW        __SPRT_SOL_RAW
#endif
#endif
#ifdef __SPRT_SOL_DECNET
#ifndef SOL_DECNET
#define SOL_DECNET     __SPRT_SOL_DECNET
#endif
#endif
#ifdef __SPRT_SOL_X25
#ifndef SOL_X25
#define SOL_X25        __SPRT_SOL_X25
#endif
#endif
#ifdef __SPRT_SOL_PACKET
#ifndef SOL_PACKET
#define SOL_PACKET     __SPRT_SOL_PACKET
#endif
#endif
#ifdef __SPRT_SOL_ATM
#ifndef SOL_ATM
#define SOL_ATM        __SPRT_SOL_ATM
#endif
#endif
#ifdef __SPRT_SOL_AAL
#ifndef SOL_AAL
#define SOL_AAL        __SPRT_SOL_AAL
#endif
#endif
#ifdef __SPRT_SOL_IRDA
#ifndef SOL_IRDA
#define SOL_IRDA       __SPRT_SOL_IRDA
#endif
#endif
#ifdef __SPRT_SOL_NETBEUI
#ifndef SOL_NETBEUI
#define SOL_NETBEUI    __SPRT_SOL_NETBEUI
#endif
#endif
#ifdef __SPRT_SOL_LLC
#ifndef SOL_LLC
#define SOL_LLC        __SPRT_SOL_LLC
#endif
#endif
#ifdef __SPRT_SOL_DCCP
#ifndef SOL_DCCP
#define SOL_DCCP       __SPRT_SOL_DCCP
#endif
#endif
#ifdef __SPRT_SOL_NETLINK
#ifndef SOL_NETLINK
#define SOL_NETLINK    __SPRT_SOL_NETLINK
#endif
#endif
#ifdef __SPRT_SOL_TIPC
#ifndef SOL_TIPC
#define SOL_TIPC       __SPRT_SOL_TIPC
#endif
#endif
#ifdef __SPRT_SOL_RXRPC
#ifndef SOL_RXRPC
#define SOL_RXRPC      __SPRT_SOL_RXRPC
#endif
#endif
#ifdef __SPRT_SOL_PPPOL2TP
#ifndef SOL_PPPOL2TP
#define SOL_PPPOL2TP   __SPRT_SOL_PPPOL2TP
#endif
#endif
#ifdef __SPRT_SOL_BLUETOOTH
#ifndef SOL_BLUETOOTH
#define SOL_BLUETOOTH  __SPRT_SOL_BLUETOOTH
#endif
#endif
#ifdef __SPRT_SOL_PNPIPE
#ifndef SOL_PNPIPE
#define SOL_PNPIPE     __SPRT_SOL_PNPIPE
#endif
#endif
#ifdef __SPRT_SOL_RDS
#ifndef SOL_RDS
#define SOL_RDS        __SPRT_SOL_RDS
#endif
#endif
#ifdef __SPRT_SOL_IUCV
#ifndef SOL_IUCV
#define SOL_IUCV       __SPRT_SOL_IUCV
#endif
#endif
#ifdef __SPRT_SOL_CAIF
#ifndef SOL_CAIF
#define SOL_CAIF       __SPRT_SOL_CAIF
#endif
#endif
#ifdef __SPRT_SOL_ALG
#ifndef SOL_ALG
#define SOL_ALG        __SPRT_SOL_ALG
#endif
#endif
#ifdef __SPRT_SOL_NFC
#ifndef SOL_NFC
#define SOL_NFC        __SPRT_SOL_NFC
#endif
#endif
#ifdef __SPRT_SOL_KCM
#ifndef SOL_KCM
#define SOL_KCM        __SPRT_SOL_KCM
#endif
#endif
#ifdef __SPRT_SOL_TLS
#ifndef SOL_TLS
#define SOL_TLS        __SPRT_SOL_TLS
#endif
#endif
#ifdef __SPRT_SOL_XDP
#ifndef SOL_XDP
#define SOL_XDP        __SPRT_SOL_XDP
#endif
#endif

// --- SO_* platform-specific -----------------------------------------------
#ifdef __SPRT_SO_DEBUG
#ifndef SO_DEBUG
#define SO_DEBUG       __SPRT_SO_DEBUG
#endif
#endif
#ifdef __SPRT_SO_ACCEPTCONN
#ifndef SO_ACCEPTCONN
#define SO_ACCEPTCONN  __SPRT_SO_ACCEPTCONN
#endif
#endif
#ifdef __SPRT_SO_USELOOPBACK
#ifndef SO_USELOOPBACK
#define SO_USELOOPBACK __SPRT_SO_USELOOPBACK
#endif
#endif
#ifdef __SPRT_SO_SNDLOWAT
#ifndef SO_SNDLOWAT
#define SO_SNDLOWAT    __SPRT_SO_SNDLOWAT
#endif
#endif
#ifdef __SPRT_SO_RCVLOWAT
#ifndef SO_RCVLOWAT
#define SO_RCVLOWAT    __SPRT_SO_RCVLOWAT
#endif
#endif
#ifdef __SPRT_SO_SNDTIMEO
#ifndef SO_SNDTIMEO
#define SO_SNDTIMEO    __SPRT_SO_SNDTIMEO
#endif
#endif
#ifdef __SPRT_SO_RCVTIMEO
#ifndef SO_RCVTIMEO
#define SO_RCVTIMEO    __SPRT_SO_RCVTIMEO
#endif
#endif
#ifdef __SPRT_SO_TIMESTAMP
#ifndef SO_TIMESTAMP
#define SO_TIMESTAMP   __SPRT_SO_TIMESTAMP
#endif
#endif
#ifdef __SPRT_SO_DONTLINGER
#ifndef SO_DONTLINGER
#define SO_DONTLINGER  __SPRT_SO_DONTLINGER
#endif
#endif
#ifdef __SPRT_SO_EXCLUSIVEADDRUSE
#ifndef SO_EXCLUSIVEADDRUSE
#define SO_EXCLUSIVEADDRUSE __SPRT_SO_EXCLUSIVEADDRUSE
#endif
#endif
#ifdef __SPRT_SO_PEERSEC
#ifndef SO_PEERSEC
#define SO_PEERSEC     __SPRT_SO_PEERSEC
#endif
#endif
#ifdef __SPRT_SO_SNDBUFFORCE
#ifndef SO_SNDBUFFORCE
#define SO_SNDBUFFORCE __SPRT_SO_SNDBUFFORCE
#endif
#endif
#ifdef __SPRT_SO_RCVBUFFORCE
#ifndef SO_RCVBUFFORCE
#define SO_RCVBUFFORCE __SPRT_SO_RCVBUFFORCE
#endif
#endif
#ifdef __SPRT_SO_PROTOCOL
#ifndef SO_PROTOCOL
#define SO_PROTOCOL    __SPRT_SO_PROTOCOL
#endif
#endif
#ifdef __SPRT_SO_DOMAIN
#ifndef SO_DOMAIN
#define SO_DOMAIN      __SPRT_SO_DOMAIN
#endif
#endif
#ifdef __SPRT_SO_NO_CHECK
#ifndef SO_NO_CHECK
#define SO_NO_CHECK    __SPRT_SO_NO_CHECK
#endif
#endif
#ifdef __SPRT_SO_PRIORITY
#ifndef SO_PRIORITY
#define SO_PRIORITY    __SPRT_SO_PRIORITY
#endif
#endif
#ifdef __SPRT_SO_BSDCOMPAT
#ifndef SO_BSDCOMPAT
#define SO_BSDCOMPAT   __SPRT_SO_BSDCOMPAT
#endif
#endif
#ifdef __SPRT_SO_PASSCRED
#ifndef SO_PASSCRED
#define SO_PASSCRED    __SPRT_SO_PASSCRED
#endif
#endif
#ifdef __SPRT_SO_PEERCRED
#ifndef SO_PEERCRED
#define SO_PEERCRED    __SPRT_SO_PEERCRED
#endif
#endif
#ifdef __SPRT_SO_TIMESTAMPNS
#ifndef SO_TIMESTAMPNS
#define SO_TIMESTAMPNS __SPRT_SO_TIMESTAMPNS
#endif
#endif
#ifdef __SPRT_SO_TIMESTAMPING
#ifndef SO_TIMESTAMPING
#define SO_TIMESTAMPING __SPRT_SO_TIMESTAMPING
#endif
#endif
#ifdef __SPRT_SO_SECURITY_AUTHENTICATION
#ifndef SO_SECURITY_AUTHENTICATION
#define SO_SECURITY_AUTHENTICATION __SPRT_SO_SECURITY_AUTHENTICATION
#endif
#endif
#ifdef __SPRT_SO_SECURITY_ENCRYPTION_TRANSPORT
#ifndef SO_SECURITY_ENCRYPTION_TRANSPORT
#define SO_SECURITY_ENCRYPTION_TRANSPORT __SPRT_SO_SECURITY_ENCRYPTION_TRANSPORT
#endif
#endif
#ifdef __SPRT_SO_SECURITY_ENCRYPTION_NETWORK
#ifndef SO_SECURITY_ENCRYPTION_NETWORK
#define SO_SECURITY_ENCRYPTION_NETWORK __SPRT_SO_SECURITY_ENCRYPTION_NETWORK
#endif
#endif
#ifdef __SPRT_SO_BINDTODEVICE
#ifndef SO_BINDTODEVICE
#define SO_BINDTODEVICE __SPRT_SO_BINDTODEVICE
#endif
#endif
#ifdef __SPRT_SO_ATTACH_FILTER
#ifndef SO_ATTACH_FILTER
#define SO_ATTACH_FILTER __SPRT_SO_ATTACH_FILTER
#endif
#endif
#ifdef __SPRT_SO_DETACH_FILTER
#ifndef SO_DETACH_FILTER
#define SO_DETACH_FILTER __SPRT_SO_DETACH_FILTER
#endif
#endif
#ifdef __SPRT_SO_GET_FILTER
#ifndef SO_GET_FILTER
#define SO_GET_FILTER  __SPRT_SO_GET_FILTER
#endif
#endif
#ifdef __SPRT_SO_PEERNAME
#ifndef SO_PEERNAME
#define SO_PEERNAME    __SPRT_SO_PEERNAME
#endif
#endif
#ifdef __SPRT_SO_PASSSEC
#ifndef SO_PASSSEC
#define SO_PASSSEC     __SPRT_SO_PASSSEC
#endif
#endif
#ifdef __SPRT_SO_MARK
#ifndef SO_MARK
#define SO_MARK        __SPRT_SO_MARK
#endif
#endif
#ifdef __SPRT_SO_RXQ_OVFL
#ifndef SO_RXQ_OVFL
#define SO_RXQ_OVFL    __SPRT_SO_RXQ_OVFL
#endif
#endif
#ifdef __SPRT_SO_WIFI_STATUS
#ifndef SO_WIFI_STATUS
#define SO_WIFI_STATUS __SPRT_SO_WIFI_STATUS
#endif
#endif
#ifdef __SPRT_SO_PEEK_OFF
#ifndef SO_PEEK_OFF
#define SO_PEEK_OFF    __SPRT_SO_PEEK_OFF
#endif
#endif
#ifdef __SPRT_SO_NOFCS
#ifndef SO_NOFCS
#define SO_NOFCS       __SPRT_SO_NOFCS
#endif
#endif
#ifdef __SPRT_SO_LOCK_FILTER
#ifndef SO_LOCK_FILTER
#define SO_LOCK_FILTER __SPRT_SO_LOCK_FILTER
#endif
#endif
#ifdef __SPRT_SO_SELECT_ERR_QUEUE
#ifndef SO_SELECT_ERR_QUEUE
#define SO_SELECT_ERR_QUEUE __SPRT_SO_SELECT_ERR_QUEUE
#endif
#endif
#ifdef __SPRT_SO_BUSY_POLL
#ifndef SO_BUSY_POLL
#define SO_BUSY_POLL   __SPRT_SO_BUSY_POLL
#endif
#endif
#ifdef __SPRT_SO_MAX_PACING_RATE
#ifndef SO_MAX_PACING_RATE
#define SO_MAX_PACING_RATE __SPRT_SO_MAX_PACING_RATE
#endif
#endif
#ifdef __SPRT_SO_BPF_EXTENSIONS
#ifndef SO_BPF_EXTENSIONS
#define SO_BPF_EXTENSIONS __SPRT_SO_BPF_EXTENSIONS
#endif
#endif
#ifdef __SPRT_SO_INCOMING_CPU
#ifndef SO_INCOMING_CPU
#define SO_INCOMING_CPU __SPRT_SO_INCOMING_CPU
#endif
#endif
#ifdef __SPRT_SO_ATTACH_BPF
#ifndef SO_ATTACH_BPF
#define SO_ATTACH_BPF  __SPRT_SO_ATTACH_BPF
#endif
#endif
#ifdef __SPRT_SO_DETACH_BPF
#ifndef SO_DETACH_BPF
#define SO_DETACH_BPF  __SPRT_SO_DETACH_BPF
#endif
#endif
#ifdef __SPRT_SO_CNX_ADVICE
#ifndef SO_CNX_ADVICE
#define SO_CNX_ADVICE  __SPRT_SO_CNX_ADVICE
#endif
#endif
#ifdef __SPRT_SO_MEMINFO
#ifndef SO_MEMINFO
#define SO_MEMINFO     __SPRT_SO_MEMINFO
#endif
#endif
#ifdef __SPRT_SO_INCOMING_NAPI_ID
#ifndef SO_INCOMING_NAPI_ID
#define SO_INCOMING_NAPI_ID __SPRT_SO_INCOMING_NAPI_ID
#endif
#endif
#ifdef __SPRT_SO_COOKIE
#ifndef SO_COOKIE
#define SO_COOKIE      __SPRT_SO_COOKIE
#endif
#endif
#ifdef __SPRT_SO_PEERGROUPS
#ifndef SO_PEERGROUPS
#define SO_PEERGROUPS  __SPRT_SO_PEERGROUPS
#endif
#endif
#ifdef __SPRT_SO_ZEROCOPY
#ifndef SO_ZEROCOPY
#define SO_ZEROCOPY    __SPRT_SO_ZEROCOPY
#endif
#endif
#ifdef __SPRT_SO_TXTIME
#ifndef SO_TXTIME
#define SO_TXTIME      __SPRT_SO_TXTIME
#endif
#endif
#ifdef __SPRT_SO_BINDTOIFINDEX
#ifndef SO_BINDTOIFINDEX
#define SO_BINDTOIFINDEX __SPRT_SO_BINDTOIFINDEX
#endif
#endif
#ifdef __SPRT_SO_DETACH_REUSEPORT_BPF
#ifndef SO_DETACH_REUSEPORT_BPF
#define SO_DETACH_REUSEPORT_BPF __SPRT_SO_DETACH_REUSEPORT_BPF
#endif
#endif

// --- SOCK_* extended ------------------------------------------------------
#ifdef __SPRT_SOCK_RDM
#ifndef SOCK_RDM
#define SOCK_RDM       __SPRT_SOCK_RDM
#endif
#endif
#ifdef __SPRT_SOCK_DCCP
#ifndef SOCK_DCCP
#define SOCK_DCCP      __SPRT_SOCK_DCCP
#endif
#endif
#ifdef __SPRT_SOCK_PACKET
#ifndef SOCK_PACKET
#define SOCK_PACKET    __SPRT_SOCK_PACKET
#endif
#endif

// --- MSG_* platform-specific ----------------------------------------------
#ifdef __SPRT_MSG_EOF
#ifndef MSG_EOF
#define MSG_EOF        __SPRT_MSG_EOF
#endif
#endif
#ifdef __SPRT_MSG_HOLD
#ifndef MSG_HOLD
#define MSG_HOLD       __SPRT_MSG_HOLD
#endif
#endif
#ifdef __SPRT_MSG_FLUSH
#ifndef MSG_FLUSH
#define MSG_FLUSH      __SPRT_MSG_FLUSH
#endif
#endif
#ifdef __SPRT_MSG_SEND
#ifndef MSG_SEND
#define MSG_SEND       __SPRT_MSG_SEND
#endif
#endif
#ifdef __SPRT_MSG_HAVEMORE
#ifndef MSG_HAVEMORE
#define MSG_HAVEMORE   __SPRT_MSG_HAVEMORE
#endif
#endif
#ifdef __SPRT_MSG_RCVMORE
#ifndef MSG_RCVMORE
#define MSG_RCVMORE    __SPRT_MSG_RCVMORE
#endif
#endif
#ifdef __SPRT_MSG_NEEDSA
#ifndef MSG_NEEDSA
#define MSG_NEEDSA     __SPRT_MSG_NEEDSA
#endif
#endif
#ifdef __SPRT_MSG_FIN
#ifndef MSG_FIN
#define MSG_FIN        __SPRT_MSG_FIN
#endif
#endif
#ifdef __SPRT_MSG_SYN
#ifndef MSG_SYN
#define MSG_SYN        __SPRT_MSG_SYN
#endif
#endif
#ifdef __SPRT_MSG_CONFIRM
#ifndef MSG_CONFIRM
#define MSG_CONFIRM    __SPRT_MSG_CONFIRM
#endif
#endif
#ifdef __SPRT_MSG_RST
#ifndef MSG_RST
#define MSG_RST        __SPRT_MSG_RST
#endif
#endif
#ifdef __SPRT_MSG_ERRQUEUE
#ifndef MSG_ERRQUEUE
#define MSG_ERRQUEUE   __SPRT_MSG_ERRQUEUE
#endif
#endif
#ifdef __SPRT_MSG_MORE
#ifndef MSG_MORE
#define MSG_MORE       __SPRT_MSG_MORE
#endif
#endif
#ifdef __SPRT_MSG_WAITFORONE
#ifndef MSG_WAITFORONE
#define MSG_WAITFORONE __SPRT_MSG_WAITFORONE
#endif
#endif
#ifdef __SPRT_MSG_BATCH
#ifndef MSG_BATCH
#define MSG_BATCH      __SPRT_MSG_BATCH
#endif
#endif
#ifdef __SPRT_MSG_FASTOPEN
#ifndef MSG_FASTOPEN
#define MSG_FASTOPEN   __SPRT_MSG_FASTOPEN
#endif
#endif
#ifdef __SPRT_MSG_CMSG_CLOEXEC
#ifndef MSG_CMSG_CLOEXEC
#define MSG_CMSG_CLOEXEC __SPRT_MSG_CMSG_CLOEXEC
#endif
#endif
#ifdef __SPRT_MSG_PROXY
#ifndef MSG_PROXY
#define MSG_PROXY      __SPRT_MSG_PROXY
#endif
#endif
#ifdef __SPRT_MSG_ZEROCOPY
#ifndef MSG_ZEROCOPY
#define MSG_ZEROCOPY   __SPRT_MSG_ZEROCOPY
#endif
#endif
#ifdef __SPRT_MSG_PARTIAL
#ifndef MSG_PARTIAL
#define MSG_PARTIAL    __SPRT_MSG_PARTIAL
#endif
#endif
#ifdef __SPRT_MSG_MAXIOVLEN
#ifndef MSG_MAXIOVLEN
#define MSG_MAXIOVLEN  __SPRT_MSG_MAXIOVLEN
#endif
#endif

// --- SCM_* platform-specific ----------------------------------------------
#ifdef __SPRT_SCM_RIGHTS
#ifndef SCM_RIGHTS
#define SCM_RIGHTS     __SPRT_SCM_RIGHTS
#endif
#endif
#ifdef __SPRT_SCM_TIMESTAMP
#ifndef SCM_TIMESTAMP
#define SCM_TIMESTAMP  __SPRT_SCM_TIMESTAMP
#endif
#endif
#ifdef __SPRT_SCM_CREDS
#ifndef SCM_CREDS
#define SCM_CREDS      __SPRT_SCM_CREDS
#endif
#endif
#ifdef __SPRT_SCM_TIMESTAMPNS
#ifndef SCM_TIMESTAMPNS
#define SCM_TIMESTAMPNS __SPRT_SCM_TIMESTAMPNS
#endif
#endif
#ifdef __SPRT_SCM_WIFI_STATUS
#ifndef SCM_WIFI_STATUS
#define SCM_WIFI_STATUS __SPRT_SCM_WIFI_STATUS
#endif
#endif
#ifdef __SPRT_SCM_TIMESTAMPING_OPT_STATS
#ifndef SCM_TIMESTAMPING_OPT_STATS
#define SCM_TIMESTAMPING_OPT_STATS __SPRT_SCM_TIMESTAMPING_OPT_STATS
#endif
#endif
#ifdef __SPRT_SCM_TIMESTAMPING_PKTINFO
#ifndef SCM_TIMESTAMPING_PKTINFO
#define SCM_TIMESTAMPING_PKTINFO __SPRT_SCM_TIMESTAMPING_PKTINFO
#endif
#endif
#ifdef __SPRT_SCM_TXTIME
#ifndef SCM_TXTIME
#define SCM_TXTIME     __SPRT_SCM_TXTIME
#endif
#endif

// --- NET_RT_* (macOS/iOS) -------------------------------------------------
#ifdef __SPRT_NET_MAXID
#ifndef NET_MAXID
#define NET_MAXID      __SPRT_NET_MAXID
#endif
#endif
#ifdef __SPRT_NET_RT_DUMP
#ifndef NET_RT_DUMP
#define NET_RT_DUMP    __SPRT_NET_RT_DUMP
#endif
#endif
#ifdef __SPRT_NET_RT_FLAGS
#ifndef NET_RT_FLAGS
#define NET_RT_FLAGS   __SPRT_NET_RT_FLAGS
#endif
#endif
#ifdef __SPRT_NET_RT_IFLIST
#ifndef NET_RT_IFLIST
#define NET_RT_IFLIST  __SPRT_NET_RT_IFLIST
#endif
#endif
#ifdef __SPRT_NET_RT_STAT
#ifndef NET_RT_STAT
#define NET_RT_STAT    __SPRT_NET_RT_STAT
#endif
#endif

// --- Windows SO_* extras --------------------------------------------------
#ifdef __SPRT_SO_BSP_STATE
#ifndef SO_BSP_STATE
#define SO_BSP_STATE   __SPRT_SO_BSP_STATE
#endif
#endif
#ifdef __SPRT_SO_GROUP_ID
#ifndef SO_GROUP_ID
#define SO_GROUP_ID    __SPRT_SO_GROUP_ID
#endif
#endif
#ifdef __SPRT_SO_GROUP_PRIORITY
#ifndef SO_GROUP_PRIORITY
#define SO_GROUP_PRIORITY __SPRT_SO_GROUP_PRIORITY
#endif
#endif
#ifdef __SPRT_SO_MAX_MSG_SIZE
#ifndef SO_MAX_MSG_SIZE
#define SO_MAX_MSG_SIZE __SPRT_SO_MAX_MSG_SIZE
#endif
#endif
#ifdef __SPRT_SO_CONDITIONAL_ACCEPT
#ifndef SO_CONDITIONAL_ACCEPT
#define SO_CONDITIONAL_ACCEPT __SPRT_SO_CONDITIONAL_ACCEPT
#endif
#endif
#ifdef __SPRT_SO_PAUSE_ACCEPT
#ifndef SO_PAUSE_ACCEPT
#define SO_PAUSE_ACCEPT __SPRT_SO_PAUSE_ACCEPT
#endif
#endif
#ifdef __SPRT_SO_COMPARTMENT_ID
#ifndef SO_COMPARTMENT_ID
#define SO_COMPARTMENT_ID __SPRT_SO_COMPARTMENT_ID
#endif
#endif
#ifdef __SPRT_SO_RANDOMIZE_PORT
#ifndef SO_RANDOMIZE_PORT
#define SO_RANDOMIZE_PORT __SPRT_SO_RANDOMIZE_PORT
#endif
#endif
#ifdef __SPRT_SO_PORT_SCALABILITY
#ifndef SO_PORT_SCALABILITY
#define SO_PORT_SCALABILITY __SPRT_SO_PORT_SCALABILITY
#endif
#endif
#ifdef __SPRT_SO_REUSE_UNICASTPORT
#ifndef SO_REUSE_UNICASTPORT
#define SO_REUSE_UNICASTPORT __SPRT_SO_REUSE_UNICASTPORT
#endif
#endif
#ifdef __SPRT_SO_REUSE_MULTICASTPORT
#ifndef SO_REUSE_MULTICASTPORT
#define SO_REUSE_MULTICASTPORT __SPRT_SO_REUSE_MULTICASTPORT
#endif
#endif
#ifdef __SPRT_SO_ORIGINAL_DST
#ifndef SO_ORIGINAL_DST
#define SO_ORIGINAL_DST __SPRT_SO_ORIGINAL_DST
#endif
#endif
#ifdef __SPRT_IP6T_SO_ORIGINAL_DST
#ifndef IP6T_SO_ORIGINAL_DST
#define IP6T_SO_ORIGINAL_DST __SPRT_IP6T_SO_ORIGINAL_DST
#endif
#endif
#ifdef __SPRT_SO_RECEIVED_HOPLIMIT
#ifndef SO_RECEIVED_HOPLIMIT
#define SO_RECEIVED_HOPLIMIT __SPRT_SO_RECEIVED_HOPLIMIT
#endif
#endif
#ifdef __SPRT_SO_RECEIVED_PROCESSOR
#ifndef SO_RECEIVED_PROCESSOR
#define SO_RECEIVED_PROCESSOR __SPRT_SO_RECEIVED_PROCESSOR
#endif
#endif

// --- AF_FILE / PF_FILE aliases (POSIX) ------------------------------------
#ifdef __SPRT_PF_FILE
#ifndef PF_FILE
#define PF_FILE        __SPRT_PF_FILE
#endif
#endif
#ifdef __SPRT_AF_FILE
#ifndef AF_FILE
#define AF_FILE        __SPRT_AF_FILE
#endif
#endif

// --- Linux Android SCM_CREDENTIALS (already above but ensure coverage) ----
#ifdef __SPRT_SCM_CREDENTIALS
#ifndef SCM_CREDENTIALS
#define SCM_CREDENTIALS __SPRT_SCM_CREDENTIALS
#endif
#endif

// === Additional platform constants (full-coverage sweep) ==================

// --- AF_* additional (macOS/Windows/Linux) --------------------------------
#ifdef __SPRT_AF_12844
#ifndef AF_12844
#define AF_12844 __SPRT_AF_12844
#endif
#endif
#ifdef __SPRT_AF_ATM
#ifndef AF_ATM
#define AF_ATM __SPRT_AF_ATM
#endif
#endif
#ifdef __SPRT_AF_BAN
#ifndef AF_BAN
#define AF_BAN __SPRT_AF_BAN
#endif
#endif
#ifdef __SPRT_AF_BTH
#ifndef AF_BTH
#define AF_BTH __SPRT_AF_BTH
#endif
#endif
#ifdef __SPRT_AF_CLUSTER
#ifndef AF_CLUSTER
#define AF_CLUSTER __SPRT_AF_CLUSTER
#endif
#endif
#ifdef __SPRT_AF_CNT
#ifndef AF_CNT
#define AF_CNT __SPRT_AF_CNT
#endif
#endif
#ifdef __SPRT_AF_COIP
#ifndef AF_COIP
#define AF_COIP __SPRT_AF_COIP
#endif
#endif
#ifdef __SPRT_AF_E164
#ifndef AF_E164
#define AF_E164 __SPRT_AF_E164
#endif
#endif
#ifdef __SPRT_AF_FIREFOX
#ifndef AF_FIREFOX
#define AF_FIREFOX __SPRT_AF_FIREFOX
#endif
#endif
#ifdef __SPRT_AF_HYPERV
#ifndef AF_HYPERV
#define AF_HYPERV __SPRT_AF_HYPERV
#endif
#endif
#ifdef __SPRT_AF_IB
#ifndef AF_IB
#define AF_IB __SPRT_AF_IB
#endif
#endif
#ifdef __SPRT_AF_ICLFXBM
#ifndef AF_ICLFXBM
#define AF_ICLFXBM __SPRT_AF_ICLFXBM
#endif
#endif
#ifdef __SPRT_AF_IEEE80211
#ifndef AF_IEEE80211
#define AF_IEEE80211 __SPRT_AF_IEEE80211
#endif
#endif
#ifdef __SPRT_AF_ISDN
#ifndef AF_ISDN
#define AF_ISDN __SPRT_AF_ISDN
#endif
#endif
#ifdef __SPRT_AF_MPLS
#ifndef AF_MPLS
#define AF_MPLS __SPRT_AF_MPLS
#endif
#endif
#ifdef __SPRT_AF_NATM
#ifndef AF_NATM
#define AF_NATM __SPRT_AF_NATM
#endif
#endif
#ifdef __SPRT_AF_NDRV
#ifndef AF_NDRV
#define AF_NDRV __SPRT_AF_NDRV
#endif
#endif
#ifdef __SPRT_AF_NETDES
#ifndef AF_NETDES
#define AF_NETDES __SPRT_AF_NETDES
#endif
#endif
#ifdef __SPRT_AF_PPP
#ifndef AF_PPP
#define AF_PPP __SPRT_AF_PPP
#endif
#endif
#ifdef __SPRT_AF_RESERVED_36
#ifndef AF_RESERVED_36
#define AF_RESERVED_36 __SPRT_AF_RESERVED_36
#endif
#endif
#ifdef __SPRT_AF_SIP
#ifndef AF_SIP
#define AF_SIP __SPRT_AF_SIP
#endif
#endif
#ifdef __SPRT_AF_SMC
#ifndef AF_SMC
#define AF_SMC __SPRT_AF_SMC
#endif
#endif
#ifdef __SPRT_AF_SYSTEM
#ifndef AF_SYSTEM
#define AF_SYSTEM __SPRT_AF_SYSTEM
#endif
#endif
#ifdef __SPRT_AF_TCNMESSAGE
#ifndef AF_TCNMESSAGE
#define AF_TCNMESSAGE __SPRT_AF_TCNMESSAGE
#endif
#endif
#ifdef __SPRT_AF_TCNPROCESS
#ifndef AF_TCNPROCESS
#define AF_TCNPROCESS __SPRT_AF_TCNPROCESS
#endif
#endif
#ifdef __SPRT_AF_UNKNOWN1
#ifndef AF_UNKNOWN1
#define AF_UNKNOWN1 __SPRT_AF_UNKNOWN1
#endif
#endif
#ifdef __SPRT_AF_UTUN
#ifndef AF_UTUN
#define AF_UTUN __SPRT_AF_UTUN
#endif
#endif
#ifdef __SPRT_AF_VOICEVIEW
#ifndef AF_VOICEVIEW
#define AF_VOICEVIEW __SPRT_AF_VOICEVIEW
#endif
#endif
#ifdef __SPRT_AF_XDP
#ifndef AF_XDP
#define AF_XDP __SPRT_AF_XDP
#endif
#endif

// --- PF_* additional (macOS/Windows/Linux/Android) ------------------------
#ifdef __SPRT_PF_ATM
#ifndef PF_ATM
#define PF_ATM __SPRT_PF_ATM
#endif
#endif
#ifdef __SPRT_PF_ATMPVC
#ifndef PF_ATMPVC
#define PF_ATMPVC __SPRT_PF_ATMPVC
#endif
#endif
#ifdef __SPRT_PF_AX25
#ifndef PF_AX25
#define PF_AX25 __SPRT_PF_AX25
#endif
#endif
#ifdef __SPRT_PF_BAN
#ifndef PF_BAN
#define PF_BAN __SPRT_PF_BAN
#endif
#endif
#ifdef __SPRT_PF_BOND
#ifndef PF_BOND
#define PF_BOND __SPRT_PF_BOND
#endif
#endif
#ifdef __SPRT_PF_BRIDGE
#ifndef PF_BRIDGE
#define PF_BRIDGE __SPRT_PF_BRIDGE
#endif
#endif
#ifdef __SPRT_PF_BTH
#ifndef PF_BTH
#define PF_BTH __SPRT_PF_BTH
#endif
#endif
#ifdef __SPRT_PF_CNT
#ifndef PF_CNT
#define PF_CNT __SPRT_PF_CNT
#endif
#endif
#ifdef __SPRT_PF_COIP
#ifndef PF_COIP
#define PF_COIP __SPRT_PF_COIP
#endif
#endif
#ifdef __SPRT_PF_FIREFOX
#ifndef PF_FIREFOX
#define PF_FIREFOX __SPRT_PF_FIREFOX
#endif
#endif
#ifdef __SPRT_PF_HYPERV
#ifndef PF_HYPERV
#define PF_HYPERV __SPRT_PF_HYPERV
#endif
#endif
#ifdef __SPRT_PF_IB
#ifndef PF_IB
#define PF_IB __SPRT_PF_IB
#endif
#endif
#ifdef __SPRT_PF_ISDN
#ifndef PF_ISDN
#define PF_ISDN __SPRT_PF_ISDN
#endif
#endif
#ifdef __SPRT_PF_MPLS
#ifndef PF_MPLS
#define PF_MPLS __SPRT_PF_MPLS
#endif
#endif
#ifdef __SPRT_PF_NATM
#ifndef PF_NATM
#define PF_NATM __SPRT_PF_NATM
#endif
#endif
#ifdef __SPRT_PF_NDRV
#ifndef PF_NDRV
#define PF_NDRV __SPRT_PF_NDRV
#endif
#endif
#ifdef __SPRT_PF_NETBEUI
#ifndef PF_NETBEUI
#define PF_NETBEUI __SPRT_PF_NETBEUI
#endif
#endif
#ifdef __SPRT_PF_NETLINK
#ifndef PF_NETLINK
#define PF_NETLINK __SPRT_PF_NETLINK
#endif
#endif
#ifdef __SPRT_PF_NETROM
#ifndef PF_NETROM
#define PF_NETROM __SPRT_PF_NETROM
#endif
#endif
#ifdef __SPRT_PF_PACKET
#ifndef PF_PACKET
#define PF_PACKET __SPRT_PF_PACKET
#endif
#endif
#ifdef __SPRT_PF_PPP
#ifndef PF_PPP
#define PF_PPP __SPRT_PF_PPP
#endif
#endif
#ifdef __SPRT_PF_RESERVED_36
#ifndef PF_RESERVED_36
#define PF_RESERVED_36 __SPRT_PF_RESERVED_36
#endif
#endif
#ifdef __SPRT_PF_ROSE
#ifndef PF_ROSE
#define PF_ROSE __SPRT_PF_ROSE
#endif
#endif
#ifdef __SPRT_PF_SECURITY
#ifndef PF_SECURITY
#define PF_SECURITY __SPRT_PF_SECURITY
#endif
#endif
#ifdef __SPRT_PF_SIP
#ifndef PF_SIP
#define PF_SIP __SPRT_PF_SIP
#endif
#endif
#ifdef __SPRT_PF_SMC
#ifndef PF_SMC
#define PF_SMC __SPRT_PF_SMC
#endif
#endif
#ifdef __SPRT_PF_SYSTEM
#ifndef PF_SYSTEM
#define PF_SYSTEM __SPRT_PF_SYSTEM
#endif
#endif
#ifdef __SPRT_PF_UNKNOWN1
#ifndef PF_UNKNOWN1
#define PF_UNKNOWN1 __SPRT_PF_UNKNOWN1
#endif
#endif
#ifdef __SPRT_PF_UTUN
#ifndef PF_UTUN
#define PF_UTUN __SPRT_PF_UTUN
#endif
#endif
#ifdef __SPRT_PF_VLAN
#ifndef PF_VLAN
#define PF_VLAN __SPRT_PF_VLAN
#endif
#endif
#ifdef __SPRT_PF_VOICEVIEW
#ifndef PF_VOICEVIEW
#define PF_VOICEVIEW __SPRT_PF_VOICEVIEW
#endif
#endif
#ifdef __SPRT_PF_X25
#ifndef PF_X25
#define PF_X25 __SPRT_PF_X25
#endif
#endif
#ifdef __SPRT_PF_XDP
#ifndef PF_XDP
#define PF_XDP __SPRT_PF_XDP
#endif
#endif
#ifdef __SPRT_PF_XTP
#ifndef PF_XTP
#define PF_XTP __SPRT_PF_XTP
#endif
#endif

// --- SOL_* additional (Android/Linux) -------------------------------------
#ifdef __SPRT_SOL_ATALK
#ifndef SOL_ATALK
#define SOL_ATALK __SPRT_SOL_ATALK
#endif
#endif
#ifdef __SPRT_SOL_AX25
#ifndef SOL_AX25
#define SOL_AX25 __SPRT_SOL_AX25
#endif
#endif
#ifdef __SPRT_SOL_IPX
#ifndef SOL_IPX
#define SOL_IPX __SPRT_SOL_IPX
#endif
#endif
#ifdef __SPRT_SOL_NETROM
#ifndef SOL_NETROM
#define SOL_NETROM __SPRT_SOL_NETROM
#endif
#endif
#ifdef __SPRT_SOL_ROSE
#ifndef SOL_ROSE
#define SOL_ROSE __SPRT_SOL_ROSE
#endif
#endif
#ifdef __SPRT_SOL_SCTP
#ifndef SOL_SCTP
#define SOL_SCTP __SPRT_SOL_SCTP
#endif
#endif
#ifdef __SPRT_SOL_TCP
#ifndef SOL_TCP
#define SOL_TCP __SPRT_SOL_TCP
#endif
#endif
#ifdef __SPRT_SOL_UDP
#ifndef SOL_UDP
#define SOL_UDP __SPRT_SOL_UDP
#endif
#endif

// --- SO_* additional (Linux/Android) --------------------------------------
#ifdef __SPRT_SO_ATTACH_REUSEPORT_CBPF
#ifndef SO_ATTACH_REUSEPORT_CBPF
#define SO_ATTACH_REUSEPORT_CBPF __SPRT_SO_ATTACH_REUSEPORT_CBPF
#endif
#endif
#ifdef __SPRT_SO_ATTACH_REUSEPORT_EBPF
#ifndef SO_ATTACH_REUSEPORT_EBPF
#define SO_ATTACH_REUSEPORT_EBPF __SPRT_SO_ATTACH_REUSEPORT_EBPF
#endif
#endif
#ifdef __SPRT_SO_BUF_LOCK
#ifndef SO_BUF_LOCK
#define SO_BUF_LOCK __SPRT_SO_BUF_LOCK
#endif
#endif
#ifdef __SPRT_SO_BUSY_POLL_BUDGET
#ifndef SO_BUSY_POLL_BUDGET
#define SO_BUSY_POLL_BUDGET __SPRT_SO_BUSY_POLL_BUDGET
#endif
#endif
#ifdef __SPRT_SO_DEVMEM_DMABUF
#ifndef SO_DEVMEM_DMABUF
#define SO_DEVMEM_DMABUF __SPRT_SO_DEVMEM_DMABUF
#endif
#endif
#ifdef __SPRT_SO_DEVMEM_DONTNEED
#ifndef SO_DEVMEM_DONTNEED
#define SO_DEVMEM_DONTNEED __SPRT_SO_DEVMEM_DONTNEED
#endif
#endif
#ifdef __SPRT_SO_DEVMEM_LINEAR
#ifndef SO_DEVMEM_LINEAR
#define SO_DEVMEM_LINEAR __SPRT_SO_DEVMEM_LINEAR
#endif
#endif
#ifdef __SPRT_SO_NETNS_COOKIE
#ifndef SO_NETNS_COOKIE
#define SO_NETNS_COOKIE __SPRT_SO_NETNS_COOKIE
#endif
#endif
#ifdef __SPRT_SO_PASSPIDFD
#ifndef SO_PASSPIDFD
#define SO_PASSPIDFD __SPRT_SO_PASSPIDFD
#endif
#endif
#ifdef __SPRT_SO_PEERPIDFD
#ifndef SO_PEERPIDFD
#define SO_PEERPIDFD __SPRT_SO_PEERPIDFD
#endif
#endif
#ifdef __SPRT_SO_PREFER_BUSY_POLL
#ifndef SO_PREFER_BUSY_POLL
#define SO_PREFER_BUSY_POLL __SPRT_SO_PREFER_BUSY_POLL
#endif
#endif
#ifdef __SPRT_SO_RCVMARK
#ifndef SO_RCVMARK
#define SO_RCVMARK __SPRT_SO_RCVMARK
#endif
#endif
#ifdef __SPRT_SO_RCVTIMEO_NEW
#ifndef SO_RCVTIMEO_NEW
#define SO_RCVTIMEO_NEW __SPRT_SO_RCVTIMEO_NEW
#endif
#endif
#ifdef __SPRT_SO_RCVTIMEO_OLD
#ifndef SO_RCVTIMEO_OLD
#define SO_RCVTIMEO_OLD __SPRT_SO_RCVTIMEO_OLD
#endif
#endif
#ifdef __SPRT_SO_RESERVE_MEM
#ifndef SO_RESERVE_MEM
#define SO_RESERVE_MEM __SPRT_SO_RESERVE_MEM
#endif
#endif
#ifdef __SPRT_SO_SNDTIMEO_NEW
#ifndef SO_SNDTIMEO_NEW
#define SO_SNDTIMEO_NEW __SPRT_SO_SNDTIMEO_NEW
#endif
#endif
#ifdef __SPRT_SO_SNDTIMEO_OLD
#ifndef SO_SNDTIMEO_OLD
#define SO_SNDTIMEO_OLD __SPRT_SO_SNDTIMEO_OLD
#endif
#endif
#ifdef __SPRT_SO_TIMESTAMPING_NEW
#ifndef SO_TIMESTAMPING_NEW
#define SO_TIMESTAMPING_NEW __SPRT_SO_TIMESTAMPING_NEW
#endif
#endif
#ifdef __SPRT_SO_TIMESTAMPING_OLD
#ifndef SO_TIMESTAMPING_OLD
#define SO_TIMESTAMPING_OLD __SPRT_SO_TIMESTAMPING_OLD
#endif
#endif
#ifdef __SPRT_SO_TIMESTAMP_NEW
#ifndef SO_TIMESTAMP_NEW
#define SO_TIMESTAMP_NEW __SPRT_SO_TIMESTAMP_NEW
#endif
#endif
#ifdef __SPRT_SO_TIMESTAMPNS_NEW
#ifndef SO_TIMESTAMPNS_NEW
#define SO_TIMESTAMPNS_NEW __SPRT_SO_TIMESTAMPNS_NEW
#endif
#endif
#ifdef __SPRT_SO_TIMESTAMPNS_OLD
#ifndef SO_TIMESTAMPNS_OLD
#define SO_TIMESTAMPNS_OLD __SPRT_SO_TIMESTAMPNS_OLD
#endif
#endif
#ifdef __SPRT_SO_TIMESTAMP_OLD
#ifndef SO_TIMESTAMP_OLD
#define SO_TIMESTAMP_OLD __SPRT_SO_TIMESTAMP_OLD
#endif
#endif
#ifdef __SPRT_SO_TXREHASH
#ifndef SO_TXREHASH
#define SO_TXREHASH __SPRT_SO_TXREHASH
#endif
#endif

// --- MSG_* additional (Android/macOS) -------------------------------------
#ifdef __SPRT_MSG_CMSG_COMPAT
#ifndef MSG_CMSG_COMPAT
#define MSG_CMSG_COMPAT __SPRT_MSG_CMSG_COMPAT
#endif
#endif
#ifdef __SPRT_MSG_PROBE
#ifndef MSG_PROBE
#define MSG_PROBE __SPRT_MSG_PROBE
#endif
#endif
#ifdef __SPRT_MSG_TRYHARD
#ifndef MSG_TRYHARD
#define MSG_TRYHARD __SPRT_MSG_TRYHARD
#endif
#endif
#ifdef __SPRT_MSG_USEUPCALL
#ifndef MSG_USEUPCALL
#define MSG_USEUPCALL __SPRT_MSG_USEUPCALL
#endif
#endif

// --- SCM_* additional (Linux/Android/macOS) -------------------------------
#ifdef __SPRT_SCM_DEVMEM_DMABUF
#ifndef SCM_DEVMEM_DMABUF
#define SCM_DEVMEM_DMABUF __SPRT_SCM_DEVMEM_DMABUF
#endif
#endif
#ifdef __SPRT_SCM_DEVMEM_LINEAR
#ifndef SCM_DEVMEM_LINEAR
#define SCM_DEVMEM_LINEAR __SPRT_SCM_DEVMEM_LINEAR
#endif
#endif
#ifdef __SPRT_SCM_SECURITY
#ifndef SCM_SECURITY
#define SCM_SECURITY __SPRT_SCM_SECURITY
#endif
#endif
#ifdef __SPRT_SCM_TIMESTAMPING
#ifndef SCM_TIMESTAMPING
#define SCM_TIMESTAMPING __SPRT_SCM_TIMESTAMPING
#endif
#endif
#ifdef __SPRT_SCM_TIMESTAMP_MONOTONIC
#ifndef SCM_TIMESTAMP_MONOTONIC
#define SCM_TIMESTAMP_MONOTONIC __SPRT_SCM_TIMESTAMP_MONOTONIC
#endif
#endif

// --- NET_RT_* additional (macOS/iOS) --------------------------------------
#ifdef __SPRT_NET_RT_DUMP2
#ifndef NET_RT_DUMP2
#define NET_RT_DUMP2 __SPRT_NET_RT_DUMP2
#endif
#endif
#ifdef __SPRT_NET_RT_FLAGS_PRIV
#ifndef NET_RT_FLAGS_PRIV
#define NET_RT_FLAGS_PRIV __SPRT_NET_RT_FLAGS_PRIV
#endif
#endif
#ifdef __SPRT_NET_RT_IFLIST2
#ifndef NET_RT_IFLIST2
#define NET_RT_IFLIST2 __SPRT_NET_RT_IFLIST2
#endif
#endif
#ifdef __SPRT_NET_RT_MAXID
#ifndef NET_RT_MAXID
#define NET_RT_MAXID __SPRT_NET_RT_MAXID
#endif
#endif
#ifdef __SPRT_NET_RT_TRASH
#ifndef NET_RT_TRASH
#define NET_RT_TRASH __SPRT_NET_RT_TRASH
#endif
#endif

// --- IPX_* additional (Android) -------------------------------------------
#ifdef __SPRT_IPX_TYPE
#ifndef IPX_TYPE
#define IPX_TYPE __SPRT_IPX_TYPE
#endif
#endif
// clang-format on

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYS___SOCKDEF_H_
