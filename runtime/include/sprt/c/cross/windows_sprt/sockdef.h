// clang-format off
// Portable core socket constants, namespaced (__SPRT_*) so they are safe to include on
// every build; <sys/socket.h> expands the public names from these, and each is
// static_asserted against the native header in SPRuntimeCSysSocket.cpp (hosted targets)
// or against the Windows SDK in tests/libc/windows-abi (this one).
#define __SPRT_SHUT_RD        0
#define __SPRT_SHUT_WR        1
#define __SPRT_SHUT_RDWR      2

#define __SPRT_SOCK_STREAM    1
#define __SPRT_SOCK_DGRAM     2
#define __SPRT_SOCK_RAW       3
#define __SPRT_SOCK_RDM       4
#define __SPRT_SOCK_SEQPACKET 5
#define __SPRT_SOCK_CLOEXEC   02000000
#define __SPRT_SOCK_NONBLOCK  04000

#define __SPRT_AF_UNSPEC      0
#define __SPRT_AF_UNIX        1
#define __SPRT_AF_INET        2
#define __SPRT_AF_INET6       23

// Platform-specific constants (Windows), namespaced with __SPRT_ prefix.
#define __SPRT_AF_LOCAL        __SPRT_AF_UNIX  // alias used on POSIX
#define __SPRT_AF_IMPLINK      3               // arpanet imp addresses
#define __SPRT_AF_PUP          4               // pup protocols: e.g. BSP
#define __SPRT_AF_CHAOS        5               // mit CHAOS protocols
#define __SPRT_AF_NS           6               // XEROX NS protocols
#define __SPRT_AF_IPX          __SPRT_AF_NS    // IPX protocols: IPX, SPX, etc.
#define __SPRT_AF_ISO          7               // ISO protocols
#define __SPRT_AF_OSI          __SPRT_AF_ISO   // OSI is ISO
#define __SPRT_AF_ECMA         8               // european computer manufacturers
#define __SPRT_AF_DATAKIT      9               // datakit protocols
#define __SPRT_AF_CCITT        10              // CCITT protocols, X.25 etc
#define __SPRT_AF_SNA          11              // IBM SNA
#define __SPRT_AF_DECnet       12              // DECnet
#define __SPRT_AF_DLI          13              // Direct data link interface
#define __SPRT_AF_LAT          14              // LAT
#define __SPRT_AF_HYLINK       15              // NSC Hyperchannel
#define __SPRT_AF_APPLETALK    16              // AppleTalk
#define __SPRT_AF_NETBIOS      17              // NetBios-style addresses
#define __SPRT_AF_VOICEVIEW    18              // VoiceView
#define __SPRT_AF_FIREFOX      19              // Protocols from Firefox
#define __SPRT_AF_UNKNOWN1     20              // Somebody is using this!
#define __SPRT_AF_BAN          21              // Banyan
#define __SPRT_AF_ATM          22              // Native ATM Services
#define __SPRT_AF_CLUSTER      24              // Microsoft Wolfpack
#define __SPRT_AF_12844        25              // IEEE 1284.4 WG AF
#define __SPRT_AF_IRDA         26              // IrDA
#define __SPRT_AF_NETDES       28              // Network Designers OSI & gateway
#define __SPRT_AF_TCNPROCESS   29
#define __SPRT_AF_TCNMESSAGE   30
#define __SPRT_AF_ICLFXBM      31
#define __SPRT_AF_BTH          32              // Bluetooth RFCOMM/L2CAP protocols
#define __SPRT_AF_LINK         33
#define __SPRT_AF_HYPERV       34
#define __SPRT_AF_MAX          35

#define __SPRT_PF_UNSPEC       __SPRT_AF_UNSPEC
#define __SPRT_PF_UNIX         __SPRT_AF_UNIX
#define __SPRT_PF_INET         __SPRT_AF_INET
#define __SPRT_PF_IMPLINK      __SPRT_AF_IMPLINK
#define __SPRT_PF_PUP          __SPRT_AF_PUP
#define __SPRT_PF_CHAOS        __SPRT_AF_CHAOS
#define __SPRT_PF_NS           __SPRT_AF_NS
#define __SPRT_PF_IPX          __SPRT_AF_IPX
#define __SPRT_PF_ISO          __SPRT_AF_ISO
#define __SPRT_PF_OSI          __SPRT_AF_OSI
#define __SPRT_PF_ECMA         __SPRT_AF_ECMA
#define __SPRT_PF_DATAKIT      __SPRT_AF_DATAKIT
#define __SPRT_PF_CCITT        __SPRT_AF_CCITT
#define __SPRT_PF_SNA          __SPRT_AF_SNA
#define __SPRT_PF_DECnet       __SPRT_AF_DECnet
#define __SPRT_PF_DLI          __SPRT_AF_DLI
#define __SPRT_PF_LAT          __SPRT_AF_LAT
#define __SPRT_PF_HYLINK       __SPRT_AF_HYLINK
#define __SPRT_PF_APPLETALK    __SPRT_AF_APPLETALK
#define __SPRT_PF_VOICEVIEW    __SPRT_AF_VOICEVIEW
#define __SPRT_PF_FIREFOX      __SPRT_AF_FIREFOX
#define __SPRT_PF_UNKNOWN1     __SPRT_AF_UNKNOWN1
#define __SPRT_PF_BAN          __SPRT_AF_BAN
#define __SPRT_PF_ATM          __SPRT_AF_ATM
#define __SPRT_PF_INET6        __SPRT_AF_INET6
#define __SPRT_PF_BTH          __SPRT_AF_BTH
#define __SPRT_PF_LINK         __SPRT_AF_LINK
#define __SPRT_PF_HYPERV       __SPRT_AF_HYPERV
#define __SPRT_PF_MAX          __SPRT_AF_MAX

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

#define __SPRT_SO_DEBUG            0x0001
#define __SPRT_SO_ACCEPTCONN       0x0002
#define __SPRT_SO_USELOOPBACK      0x0040
#define __SPRT_SO_DONTLINGER       (int)(~__SPRT_SO_LINGER)
#define __SPRT_SO_EXCLUSIVEADDRUSE ((int)(~__SPRT_SO_REUSEADDR))
#define __SPRT_SO_SNDLOWAT         0x1003
#define __SPRT_SO_RCVLOWAT         0x1004
#define __SPRT_SO_SNDTIMEO         0x1005
#define __SPRT_SO_RCVTIMEO         0x1006
#define __SPRT_SO_BSP_STATE        0x1009
#define __SPRT_SO_GROUP_ID         0x2001
#define __SPRT_SO_GROUP_PRIORITY   0x2002
#define __SPRT_SO_MAX_MSG_SIZE     0x2003
#define __SPRT_SO_CONDITIONAL_ACCEPT 0x3002
#define __SPRT_SO_PAUSE_ACCEPT     0x3003
#define __SPRT_SO_COMPARTMENT_ID   0x3004
#define __SPRT_SO_RANDOMIZE_PORT   0x3005
#define __SPRT_SO_PORT_SCALABILITY 0x3006
#define __SPRT_SO_REUSE_UNICASTPORT    0x3007
#define __SPRT_SO_REUSE_MULTICASTPORT  0x3008
#define __SPRT_SO_ORIGINAL_DST   0x300F
#define __SPRT_SO_RECEIVED_HOPLIMIT    0x3010
#define __SPRT_SO_RECEIVED_PROCESSOR   0x3011

#define __SPRT_MSG_OOB        0x1
#define __SPRT_MSG_PEEK       0x2
#define __SPRT_MSG_DONTROUTE  0x4
#define __SPRT_MSG_CTRUNC     0x0200        // native Winsock value (ws2def.h)
#define __SPRT_MSG_TRUNC      0x0100        // native Winsock value (ws2def.h)
#define __SPRT_MSG_WAITALL    0x8           // native Winsock value (winsock2.h)
#define __SPRT_MSG_NOSIGNAL   0
#define __SPRT_MSG_MAXIOVLEN   16
#define __SPRT_MSG_PARTIAL     0x8000

#define __SPRT_SOMAXCONN      0x7fffffff

#define __SPRT_SOL_SOCKET 0xffff
#define __SPRT_SOL_IP          0
#define __SPRT_SOL_IPV6        41

#define __SPRT_IP6T_SO_ORIGINAL_DST __SPRT_SO_ORIGINAL_DST

#define __SPRT_UNIX_PATH_MAX 108

// clang-format on
