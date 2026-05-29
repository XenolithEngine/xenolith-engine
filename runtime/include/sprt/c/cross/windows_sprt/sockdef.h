#define AF_UNSPEC       0               // unspecified
#define AF_UNIX         1               // local to host (pipes, portals)
#define AF_INET         2               // internetwork: UDP, TCP, etc.
#define AF_IMPLINK      3               // arpanet imp addresses
#define AF_PUP          4               // pup protocols: e.g. BSP
#define AF_CHAOS        5               // mit CHAOS protocols
#define AF_NS           6               // XEROX NS protocols
#define AF_IPX          AF_NS           // IPX protocols: IPX, SPX, etc.
#define AF_ISO          7               // ISO protocols
#define AF_OSI          AF_ISO          // OSI is ISO
#define AF_ECMA         8               // european computer manufacturers
#define AF_DATAKIT      9               // datakit protocols
#define AF_CCITT        10              // CCITT protocols, X.25 etc
#define AF_SNA          11              // IBM SNA
#define AF_DECnet       12              // DECnet
#define AF_DLI          13              // Direct data link interface
#define AF_LAT          14              // LAT
#define AF_HYLINK       15              // NSC Hyperchannel
#define AF_APPLETALK    16              // AppleTalk
#define AF_NETBIOS      17              // NetBios-style addresses
#define AF_VOICEVIEW    18              // VoiceView
#define AF_FIREFOX      19              // Protocols from Firefox
#define AF_UNKNOWN1     20              // Somebody is using this!
#define AF_BAN          21              // Banyan
#define AF_ATM          22              // Native ATM Services
#define AF_INET6        23              // Internetwork Version 6
#define AF_CLUSTER      24              // Microsoft Wolfpack
#define AF_12844        25              // IEEE 1284.4 WG AF
#define AF_IRDA         26              // IrDA
#define AF_NETDES       28              // Network Designers OSI & gateway
#define AF_TCNPROCESS   29
#define AF_TCNMESSAGE   30
#define AF_ICLFXBM      31
#define AF_BTH          32              // Bluetooth RFCOMM/L2CAP protocols
#define AF_LINK         33
#define AF_HYPERV       34
#define AF_MAX          35

#define PF_UNSPEC       AF_UNSPEC
#define PF_UNIX         AF_UNIX
#define PF_INET         AF_INET
#define PF_IMPLINK      AF_IMPLINK
#define PF_PUP          AF_PUP
#define PF_CHAOS        AF_CHAOS
#define PF_NS           AF_NS
#define PF_IPX          AF_IPX
#define PF_ISO          AF_ISO
#define PF_OSI          AF_OSI
#define PF_ECMA         AF_ECMA
#define PF_DATAKIT      AF_DATAKIT
#define PF_CCITT        AF_CCITT
#define PF_SNA          AF_SNA
#define PF_DECnet       AF_DECnet
#define PF_DLI          AF_DLI
#define PF_LAT          AF_LAT
#define PF_HYLINK       AF_HYLINK
#define PF_APPLETALK    AF_APPLETALK
#define PF_VOICEVIEW    AF_VOICEVIEW
#define PF_FIREFOX      AF_FIREFOX
#define PF_UNKNOWN1     AF_UNKNOWN1
#define PF_BAN          AF_BAN
#define PF_ATM          AF_ATM
#define PF_INET6        AF_INET6
#define PF_BTH          AF_BTH
#define PF_MAX          AF_MAX

#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define SOCK_RAW        3
#define SOCK_RDM        4
#define SOCK_SEQPACKET  5

#define IPPROTO_IP  0

typedef enum {
	IPPROTO_HOPOPTS = 0, // IPv6 Hop-by-Hop options
	IPPROTO_ICMP = 1,
	IPPROTO_IGMP = 2,
	IPPROTO_GGP = 3,
	IPPROTO_IPV4 = 4,
	IPPROTO_ST = 5,
	IPPROTO_TCP = 6,
	IPPROTO_CBT = 7,
	IPPROTO_EGP = 8,
	IPPROTO_IGP = 9,
	IPPROTO_PUP = 12,
	IPPROTO_UDP = 17,
	IPPROTO_IDP = 22,
	IPPROTO_RDP = 27,
	IPPROTO_IPV6 = 41, // IPv6 header
	IPPROTO_ROUTING = 43, // IPv6 Routing header
	IPPROTO_FRAGMENT = 44, // IPv6 fragmentation header
	IPPROTO_ESP = 50, // encapsulating security payload
	IPPROTO_AH = 51, // authentication header
	IPPROTO_ICMPV6 = 58, // ICMPv6
	IPPROTO_NONE = 59, // IPv6 no next header
	IPPROTO_DSTOPTS = 60, // IPv6 Destination options
	IPPROTO_ND = 77,
	IPPROTO_ICLFXBM = 78,
	IPPROTO_PIM = 103,
	IPPROTO_PGM = 113,
	IPPROTO_L2TP = 115,
	IPPROTO_SCTP = 132,
	IPPROTO_RAW = 255,
	IPPROTO_MAX = 256,
} IPPROTO, *PIPROTO;

#define SOL_SOCKET 0xffff
#define SOL_IP     (SOL_SOCKET-4)
#define SOL_IPV6   (SOL_SOCKET-5)

#define SO_DEBUG        0x0001
#define SO_ACCEPTCONN   0x0002
#define SO_REUSEADDR    0x0004
#define SO_KEEPALIVE    0x0008
#define SO_DONTROUTE    0x0010
#define SO_BROADCAST    0x0020
#define SO_USELOOPBACK  0x0040
#define SO_LINGER       0x0080
#define SO_OOBINLINE    0x0100
#define SO_DONTLINGER   (int)(~SO_LINGER)
#define SO_EXCLUSIVEADDRUSE ((int)(~SO_REUSEADDR))
#define SO_SNDBUF       0x1001
#define SO_RCVBUF       0x1002
#define SO_SNDLOWAT     0x1003
#define SO_RCVLOWAT     0x1004
#define SO_SNDTIMEO     0x1005
#define SO_RCVTIMEO     0x1006
#define SO_ERROR        0x1007
#define SO_TYPE         0x1008
#define SO_BSP_STATE    0x1009
#define SO_GROUP_ID     0x2001
#define SO_GROUP_PRIORITY 0x2002
#define SO_MAX_MSG_SIZE 0x2003
#define SO_CONDITIONAL_ACCEPT 0x3002
#define SO_PAUSE_ACCEPT 0x3003
#define SO_COMPARTMENT_ID 0x3004
#define SO_RANDOMIZE_PORT 0x3005
#define SO_PORT_SCALABILITY 0x3006
#define SO_REUSE_UNICASTPORT 0x3007
#define SO_REUSE_MULTICASTPORT 0x3008
#define SO_ORIGINAL_DST 0x300F
#define IP6T_SO_ORIGINAL_DST SO_ORIGINAL_DST

#define SO_RECEIVED_HOPLIMIT 0x3010
#define SO_RECEIVED_PROCESSOR 0x3011

#define INVALID_SOCKET  (SOCKET)(~0)
#define SOCKET_ERROR            (-1)

#define TCP_NODELAY         0x0001

#define MSG_OOB         0x1
#define MSG_PEEK        0x2
#define MSG_DONTROUTE   0x4
#define MSG_MAXIOVLEN   16
#define MSG_PARTIAL     0x8000
