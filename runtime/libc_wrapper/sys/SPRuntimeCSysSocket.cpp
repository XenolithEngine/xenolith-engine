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

// Expose glibc's full <netinet/in.h> surface (the IPV6_PREFER_SRC_* family and
// other names gated behind __USE_GNU) so the 1-1 netinet asserts below validate
// against the complete native table on Linux. Must precede every libc header, as
// glibc locks __USE_GNU at the first <features.h>. Ignored by non-glibc targets.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#define __SPRT_BUILD 1

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/sys/__sprt_socket.h>
#include <sprt/c/bits/__sprt_time_t.h>
#include <sprt/runtime/log.h>

#if SPRT_WASM

// The browser sandbox has no socket layer; every entry point is an ENOSYS stub. See
// libc_impl/src/wasm/socket.cc for the public symbols; here the __sprt_* backing mirrors
// it so freestanding code that references the socket wrappers still links.

#include <unistd.h>
#include <sys/ioctl.h>

#elif SPRT_WINDOWS

#include <sprt/wrappers/windows/__sprt_winsock.h>

extern "C" {
__SPRT_WIN_IMPORT WINAPI SOCKET socket(int __domain, int __type, int __protocol);
__SPRT_WIN_IMPORT WINAPI int bind(SOCKET __fd, const struct __SPRT_SOCKADDR_NAME *__addr,
		__sprt_socklen_t __len);
__SPRT_WIN_IMPORT WINAPI int connect(SOCKET __fd, const struct __SPRT_SOCKADDR_NAME *__addr,
		__sprt_socklen_t __len);
__SPRT_WIN_IMPORT WINAPI int listen(SOCKET __fd, int __backlog);
__SPRT_WIN_IMPORT WINAPI SOCKET accept(SOCKET __fd, struct __SPRT_SOCKADDR_NAME *__addr,
		__sprt_socklen_t *__len);
__SPRT_WIN_IMPORT WINAPI int getsockname(SOCKET __fd, struct __SPRT_SOCKADDR_NAME *__addr,
		__sprt_socklen_t *__len);
__SPRT_WIN_IMPORT WINAPI int getpeername(SOCKET __fd, struct __SPRT_SOCKADDR_NAME *__addr,
		__sprt_socklen_t *__len);
__SPRT_WIN_IMPORT WINAPI int shutdown(SOCKET __fd, int __how);
__SPRT_WIN_IMPORT WINAPI int getsockopt(SOCKET __fd, int __level, int __optname,
		sockdata_t *__optval, __sprt_socklen_t *__optlen);
__SPRT_WIN_IMPORT WINAPI int setsockopt(SOCKET __fd, int __level, int __optname,
		const sockdata_t *__optval, __sprt_socklen_t __optlen);
__SPRT_WIN_IMPORT WINAPI socksize_t send(SOCKET __fd, const sockdata_t *__buf, int __n,
		int __flags);
__SPRT_WIN_IMPORT WINAPI socksize_t recv(SOCKET __fd, sockdata_t *__buf, int __n, int __flags);
__SPRT_WIN_IMPORT WINAPI socksize_t sendto(SOCKET __fd, const sockdata_t *__buf, int __n,
		int __flags, const struct __SPRT_SOCKADDR_NAME *__addr, __sprt_socklen_t __addr_len);
__SPRT_WIN_IMPORT WINAPI socksize_t recvfrom(SOCKET __fd, sockdata_t *__buf, int __n, int __flags,
		struct __SPRT_SOCKADDR_NAME *__addr, __sprt_socklen_t *__addr_len);

__SPRT_WIN_IMPORT WINAPI int closesocket(SOCKET s);
__SPRT_WIN_IMPORT WINAPI int ioctlsocket(SOCKET s, long cmd, unsigned long *argp);
}

#include <stdlib.h>

#else

#define _GNU_SOURCE 1
// Hosted (Linux / macOS / Android): forward straight to the platform libc, casting the
// SPRT structs to the native ones (validated identical by the static_asserts below).
#include <sys/socket.h>
#include <netinet/in.h> // native sockaddr_in / in_addr / sockaddr_in6 / in6_addr for the asserts
#if SPRT_LINUX
// glibc keeps IPV6_PREFER_SRC_* (and other kernel netinet defines) in the uapi
// <linux/in6.h> rather than <netinet/in.h>; pull it in so the 1-1 asserts can see
// them. It coexists with <netinet/in.h> via glibc's __USE_KERNEL_IPV6_DEFS guard.
#include <linux/in6.h>
#endif
#include <netinet/tcp.h> // native TCP_* option numbers for the asserts
#include <sprt/c/cross/__sprt_netinet.h> // __SPRT_IPPROTO_/IP_/IPV6_/MCAST_ for the asserts
#include <sys/time.h> // native ::timeval for the ILP32 Android SO_*TIMEO payload translation
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>

#if SPRT_APPLE
// macOS ships no accept4() / SOCK_CLOEXEC / SOCK_NONBLOCK; the emulation maps these flag
// bits (Linux values; the path is unused on hosted macOS, where accept4 is not public).
#ifndef SOCK_CLOEXEC
#define SOCK_CLOEXEC 02000000
#endif
#ifndef SOCK_NONBLOCK
#define SOCK_NONBLOCK 04000
#endif
#endif

#endif

#if SPRT_LINUX
// This values is not defined for musl libc
#ifndef TCP_COOKIE_IN_ALWAYS
#define TCP_COOKIE_IN_ALWAYS __SPRT_TCP_COOKIE_IN_ALWAYS
#endif

#ifndef TCP_COOKIE_MAX
#define TCP_COOKIE_MAX __SPRT_TCP_COOKIE_MAX
#endif

#ifndef TCP_COOKIE_MIN
#define TCP_COOKIE_MIN __SPRT_TCP_COOKIE_MIN
#endif

#ifndef TCP_COOKIE_OUT_NEVER
#define TCP_COOKIE_OUT_NEVER __SPRT_TCP_COOKIE_OUT_NEVER
#endif

#ifndef TCP_COOKIE_PAIR_SIZE
#define TCP_COOKIE_PAIR_SIZE __SPRT_TCP_COOKIE_PAIR_SIZE
#endif

#ifndef TCP_COOKIE_TRANSACTIONS
#define TCP_COOKIE_TRANSACTIONS __SPRT_TCP_COOKIE_TRANSACTIONS
#endif

#ifndef TCP_COOKIE_TRANSACTIONS
#define TCP_COOKIE_TRANSACTIONS __SPRT_TCP_COOKIE_TRANSACTIONS
#endif

#ifndef TCP_MAXWIN
#define TCP_MAXWIN __SPRT_TCP_MAXWIN
#endif

#ifndef TCP_MAX_WINSHIFT
#define TCP_MAX_WINSHIFT __SPRT_TCP_MAX_WINSHIFT
#endif

#ifndef TCP_MSS
#define TCP_MSS __SPRT_TCP_MSS
#endif

#ifndef TCP_MSS_DEFAULT
#define TCP_MSS_DEFAULT __SPRT_TCP_MSS_DEFAULT
#endif

#ifndef TCP_MSS_DESIRED
#define TCP_MSS_DESIRED __SPRT_TCP_MSS_DESIRED
#endif

#ifndef TCP_RECEIVE_ZEROCOPY_FLAG_TLB_CLEAN_HINT
#define TCP_RECEIVE_ZEROCOPY_FLAG_TLB_CLEAN_HINT __SPRT_TCP_RECEIVE_ZEROCOPY_FLAG_TLB_CLEAN_HINT
#endif

#ifndef TCP_S_DATA_IN
#define TCP_S_DATA_IN __SPRT_TCP_S_DATA_IN
#endif

#ifndef TCP_S_DATA_OUT
#define TCP_S_DATA_OUT __SPRT_TCP_S_DATA_OUT
#endif

#undef TCP_MD5SIG_FLAG_IFINDEX
#undef TCP_RECEIVE_ZEROCOPY_FLAG_TLB_CLEAN_HINT

#endif

// ---------------------------------------------------------------------------
// ABI validation (hosted). The per-platform cross <sys/socket.h> surface
// (cross/<platform>/socket.h + sockdef.h) is defined to match the native header
// value-for-value and layout-for-layout, so the forwarders below are plain casts.
// ---------------------------------------------------------------------------

#if !SPRT_WASM && !SPRT_WINDOWS

static_assert(sizeof(struct __SPRT_ID(sockaddr)) == sizeof(struct ::sockaddr),
		"sockaddr size differs from native");
static_assert(__builtin_offsetof(struct __SPRT_ID(sockaddr), sa_family)
				== __builtin_offsetof(struct ::sockaddr, sa_family),
		"sockaddr.sa_family offset differs from native");
static_assert(__builtin_offsetof(struct __SPRT_ID(sockaddr), sa_data)
				== __builtin_offsetof(struct ::sockaddr, sa_data),
		"sockaddr.sa_data offset differs from native");
static_assert(sizeof(__SPRT_ID(socklen_t)) == sizeof(::socklen_t),
		"socklen_t size differs from native");

static_assert(sizeof(struct __SPRT_ID(msghdr)) == sizeof(struct ::msghdr),
		"msghdr size differs from native");
static_assert(__builtin_offsetof(struct __SPRT_ID(msghdr), msg_name)
						== __builtin_offsetof(struct ::msghdr, msg_name)
				&& __builtin_offsetof(struct __SPRT_ID(msghdr), msg_namelen)
						== __builtin_offsetof(struct ::msghdr, msg_namelen)
				&& __builtin_offsetof(struct __SPRT_ID(msghdr), msg_iov)
						== __builtin_offsetof(struct ::msghdr, msg_iov)
				&& __builtin_offsetof(struct __SPRT_ID(msghdr), msg_iovlen)
						== __builtin_offsetof(struct ::msghdr, msg_iovlen)
				&& __builtin_offsetof(struct __SPRT_ID(msghdr), msg_control)
						== __builtin_offsetof(struct ::msghdr, msg_control)
				&& __builtin_offsetof(struct __SPRT_ID(msghdr), msg_controllen)
						== __builtin_offsetof(struct ::msghdr, msg_controllen)
				&& __builtin_offsetof(struct __SPRT_ID(msghdr), msg_flags)
						== __builtin_offsetof(struct ::msghdr, msg_flags),
		"msghdr layout differs from native");

static_assert(sizeof(struct __SPRT_ID(cmsghdr)) == sizeof(struct ::cmsghdr),
		"cmsghdr size differs from native");
static_assert(__builtin_offsetof(struct __SPRT_ID(cmsghdr), cmsg_len)
						== __builtin_offsetof(struct ::cmsghdr, cmsg_len)
				&& __builtin_offsetof(struct __SPRT_ID(cmsghdr), cmsg_level)
						== __builtin_offsetof(struct ::cmsghdr, cmsg_level)
				&& __builtin_offsetof(struct __SPRT_ID(cmsghdr), cmsg_type)
						== __builtin_offsetof(struct ::cmsghdr, cmsg_type),
		"cmsghdr layout differs from native");

// Address structures (cross/<platform>/socket.h) vs native <netinet/in.h>: size + the
// field offsets the wrapper / callers actually poke through a `struct sockaddr *` cast.
static_assert(sizeof(struct __SPRT_ID(in_addr)) == sizeof(struct ::in_addr),
		"in_addr size differs from native");
static_assert(sizeof(struct __SPRT_ID(sockaddr_in)) == sizeof(struct ::sockaddr_in),
		"sockaddr_in size differs from native");
static_assert(__builtin_offsetof(struct __SPRT_ID(sockaddr_in), sin_family)
						== __builtin_offsetof(struct ::sockaddr_in, sin_family)
				&& __builtin_offsetof(struct __SPRT_ID(sockaddr_in), sin_port)
						== __builtin_offsetof(struct ::sockaddr_in, sin_port)
				&& __builtin_offsetof(struct __SPRT_ID(sockaddr_in), sin_addr)
						== __builtin_offsetof(struct ::sockaddr_in, sin_addr),
		"sockaddr_in layout differs from native");
static_assert(sizeof(struct __SPRT_ID(in6_addr)) == sizeof(struct ::in6_addr),
		"in6_addr size differs from native");
static_assert(sizeof(struct __SPRT_ID(sockaddr_in6)) == sizeof(struct ::sockaddr_in6),
		"sockaddr_in6 size differs from native");
static_assert(__builtin_offsetof(struct __SPRT_ID(sockaddr_in6), sin6_family)
						== __builtin_offsetof(struct ::sockaddr_in6, sin6_family)
				&& __builtin_offsetof(struct __SPRT_ID(sockaddr_in6), sin6_port)
						== __builtin_offsetof(struct ::sockaddr_in6, sin6_port)
				&& __builtin_offsetof(struct __SPRT_ID(sockaddr_in6), sin6_addr)
						== __builtin_offsetof(struct ::sockaddr_in6, sin6_addr)
				&& __builtin_offsetof(struct __SPRT_ID(sockaddr_in6), sin6_scope_id)
						== __builtin_offsetof(struct ::sockaddr_in6, sin6_scope_id),
		"sockaddr_in6 layout differs from native");

// The __SPRT_-prefixed socket constants (cross/<platform>/sockdef.h) are validated here
// against the native <sys/socket.h>; the plain SOCK_*/AF_*/... alias the __SPRT_ ones.
static_assert(__SPRT_SHUT_RD == SHUT_RD && __SPRT_SHUT_WR == SHUT_WR
				&& __SPRT_SHUT_RDWR == SHUT_RDWR,
		"SHUT_* differ from native");

static_assert(__SPRT_SOCK_STREAM == SOCK_STREAM && __SPRT_SOCK_DGRAM == SOCK_DGRAM
				&& __SPRT_SOCK_RAW == SOCK_RAW && __SPRT_SOCK_SEQPACKET == SOCK_SEQPACKET,
		"SOCK_* type constants differ from native");

static_assert(__SPRT_SOCK_CLOEXEC == SOCK_CLOEXEC && __SPRT_SOCK_NONBLOCK == SOCK_NONBLOCK,
		"SOCK_CLOEXEC/NONBLOCK differ from native");

static_assert(__SPRT_AF_UNSPEC == AF_UNSPEC && __SPRT_AF_UNIX == AF_UNIX
				&& __SPRT_AF_INET == AF_INET && __SPRT_AF_INET6 == AF_INET6,
		"AF_* differ from native");
static_assert(__SPRT_SOL_SOCKET == SOL_SOCKET, "SOL_SOCKET differs from native");
static_assert(__SPRT_SO_REUSEADDR == SO_REUSEADDR && __SPRT_SO_TYPE == SO_TYPE
				&& __SPRT_SO_ERROR == SO_ERROR && __SPRT_SO_DONTROUTE == SO_DONTROUTE
				&& __SPRT_SO_BROADCAST == SO_BROADCAST && __SPRT_SO_SNDBUF == SO_SNDBUF
				&& __SPRT_SO_RCVBUF == SO_RCVBUF && __SPRT_SO_KEEPALIVE == SO_KEEPALIVE
				&& __SPRT_SO_OOBINLINE == SO_OOBINLINE && __SPRT_SO_LINGER == SO_LINGER,
		"SO_* differ from native");

static_assert(__SPRT_SO_REUSEPORT == SO_REUSEPORT, "SO_REUSEPORT differs from native");

static_assert(__SPRT_MSG_OOB == MSG_OOB && __SPRT_MSG_PEEK == MSG_PEEK
				&& __SPRT_MSG_DONTROUTE == MSG_DONTROUTE && __SPRT_MSG_CTRUNC == MSG_CTRUNC
				&& __SPRT_MSG_TRUNC == MSG_TRUNC && __SPRT_MSG_DONTWAIT == MSG_DONTWAIT
				&& __SPRT_MSG_EOR == MSG_EOR && __SPRT_MSG_WAITALL == MSG_WAITALL,
		"MSG_* differ from native");

static_assert(__SPRT_MSG_NOSIGNAL == MSG_NOSIGNAL, "MSG_NOSIGNAL differs from native");

static_assert(__SPRT_SOCK_RDM == SOCK_RDM, "SOCK_RDM/DCCP/PACKET differ from native");

// PF_* family constants
static_assert(__SPRT_PF_UNSPEC == PF_UNSPEC && __SPRT_PF_LOCAL == PF_LOCAL
				&& __SPRT_PF_UNIX == PF_UNIX && __SPRT_PF_INET == PF_INET
				&& __SPRT_PF_INET6 == PF_INET6,
		"PF_* core differ from native");

static_assert(__SPRT_PF_IPX == PF_IPX && __SPRT_PF_APPLETALK == PF_APPLETALK
				&& __SPRT_PF_DECnet == PF_DECnet && __SPRT_PF_KEY == PF_KEY
				&& __SPRT_PF_ROUTE == PF_ROUTE,
		"PF_* extended (A-R) differ from native");

static_assert(__SPRT_PF_SNA == PF_SNA && __SPRT_PF_ISDN == PF_ISDN && __SPRT_PF_VSOCK == PF_VSOCK,
		"PF_* extended (S-MAX) differ from native");

#if SPRT_APPLE

// --- macOS/iOS-specific socket constants -------------------------------------

static_assert(__SPRT_SO_DEBUG == SO_DEBUG, "SO_DEBUG differs from native");
static_assert(__SPRT_SO_ACCEPTCONN == SO_ACCEPTCONN, "SO_ACCEPTCONN differs from native");
static_assert(__SPRT_SO_USELOOPBACK == SO_USELOOPBACK, "SO_USELOOPBACK differs from native");
static_assert(__SPRT_SO_TIMESTAMP == SO_TIMESTAMP, "SO_TIMESTAMP differs from native");
static_assert(__SPRT_SO_SNDLOWAT == SO_SNDLOWAT, "SO_SNDLOWAT differs from native");
static_assert(__SPRT_SO_RCVLOWAT == SO_RCVLOWAT, "SO_RCVLOWAT differs from native");
static_assert(__SPRT_SO_SNDTIMEO == SO_SNDTIMEO, "SO_SNDTIMEO differs from native");
static_assert(__SPRT_SO_RCVTIMEO == SO_RCVTIMEO, "SO_RCVTIMEO differs from native");

// macOS AF_* extended constants (Darwin-specific)
static_assert(__SPRT_AF_LOCAL == AF_LOCAL && __SPRT_AF_UNIX == AF_UNIX,
		"AF_LOCAL/UNIX differ from native");
static_assert(__SPRT_AF_IMPLINK == AF_IMPLINK, "AF_IMPLINK differs from native");
static_assert(__SPRT_AF_PUP == AF_PUP, "AF_PUP differs from native");
static_assert(__SPRT_AF_CHAOS == AF_CHAOS, "AF_CHAOS differs from native");
static_assert(__SPRT_AF_NS == AF_NS, "AF_NS differs from native");
static_assert(__SPRT_AF_ISO == AF_ISO && __SPRT_AF_OSI == AF_OSI, "AF_ISO/OSI differ from native");
static_assert(__SPRT_AF_ECMA == AF_ECMA, "AF_ECMA differs from native");
static_assert(__SPRT_AF_DATAKIT == AF_DATAKIT, "AF_DATAKIT differs from native");
static_assert(__SPRT_AF_CCITT == AF_CCITT, "AF_CCITT differs from native");
static_assert(__SPRT_AF_SNA == AF_SNA && __SPRT_AF_DECnet == AF_DECnet,
		"AF_SNA/DECnet differ from native");
static_assert(__SPRT_AF_DLI == AF_DLI, "AF_DLI differs from native");
static_assert(__SPRT_AF_LAT == AF_LAT, "AF_LAT differs from native");
static_assert(__SPRT_AF_HYLINK == AF_HYLINK, "AF_HYLINK differs from native");
static_assert(__SPRT_AF_APPLETALK == AF_APPLETALK && __SPRT_AF_ROUTE == AF_ROUTE,
		"AF_APPLETALK/ROUTE differ from native");
static_assert(__SPRT_AF_LINK == AF_LINK, "AF_LINK differs from native");
static_assert(__SPRT_AF_COIP == AF_COIP && __SPRT_AF_CNT == AF_CNT,
		"AF_COIP/CNT differ from native");
static_assert(__SPRT_AF_IPX == AF_IPX, "AF_IPX differs from native");
static_assert(__SPRT_AF_SIP == AF_SIP, "AF_SIP differs from native");
static_assert(__SPRT_AF_NDRV == AF_NDRV, "AF_NDRV differs from native");
static_assert(__SPRT_AF_ISDN == AF_ISDN && __SPRT_AF_E164 == AF_E164,
		"AF_ISDN/E164 differ from native");
static_assert(__SPRT_AF_NATM == AF_NATM, "AF_NATM differs from native");
static_assert(__SPRT_AF_SYSTEM == AF_SYSTEM && __SPRT_AF_NETBIOS == AF_NETBIOS,
		"AF_SYSTEM/NETBIOS differ from native");
static_assert(__SPRT_AF_PPP == AF_PPP, "AF_PPP differs from native");
static_assert(__SPRT_AF_IEEE80211 == AF_IEEE80211 && __SPRT_AF_UTUN == AF_UTUN,
		"AF_IEEE80211/UTUN differ from native");
static_assert(__SPRT_AF_VSOCK == AF_VSOCK, "AF_VSOCK differs from native");

static_assert(__SPRT_PF_LOCAL == PF_LOCAL && __SPRT_PF_UNIX == PF_UNIX,
		"PF_LOCAL/UNIX differ from native");
static_assert(__SPRT_PF_IMPLINK == PF_IMPLINK, "PF_IMPLINK differs from native");
static_assert(__SPRT_PF_PUP == PF_PUP, "PF_PUP differs from native");
static_assert(__SPRT_PF_CHAOS == PF_CHAOS, "PF_CHAOS differs from native");
static_assert(__SPRT_PF_NS == PF_NS, "PF_NS differs from native");
static_assert(__SPRT_PF_ISO == PF_ISO && __SPRT_PF_OSI == PF_OSI, "PF_ISO/OSI differ from native");
static_assert(__SPRT_PF_ECMA == PF_ECMA, "PF_ECMA differs from native");
static_assert(__SPRT_PF_DATAKIT == PF_DATAKIT, "PF_DATAKIT differs from native");
static_assert(__SPRT_PF_CCITT == PF_CCITT, "PF_CCITT differs from native");
static_assert(__SPRT_PF_SNA == PF_SNA && __SPRT_PF_DECnet == PF_DECnet,
		"PF_SNA/DECnet differ from native");
static_assert(__SPRT_PF_DLI == PF_DLI, "PF_DLI differs from native");
static_assert(__SPRT_PF_LAT == PF_LAT, "PF_LAT differs from native");
static_assert(__SPRT_PF_HYLINK == PF_HYLINK, "PF_HYLINK differs from native");
static_assert(__SPRT_PF_APPLETALK == PF_APPLETALK && __SPRT_PF_ROUTE == PF_ROUTE,
		"PF_APPLETALK/ROUTE differ from native");
static_assert(__SPRT_PF_LINK == PF_LINK, "PF_LINK differs from native");
static_assert(__SPRT_PF_IPX == PF_IPX, "PF_IPX differs from native");
static_assert(__SPRT_PF_NDRV == PF_NDRV, "PF_NDRV differs from native");
static_assert(__SPRT_PF_ISDN == PF_ISDN, "PF_ISDN differs from native");
static_assert(__SPRT_PF_NATM == PF_NATM, "PF_NATM differs from native");
static_assert(__SPRT_PF_SYSTEM == PF_SYSTEM && __SPRT_PF_NETBIOS == PF_NETBIOS,
		"PF_SYSTEM/NETBIOS differ from native");
static_assert(__SPRT_PF_PPP == PF_PPP, "PF_PPP differs from native");
static_assert(__SPRT_PF_UTUN == PF_UTUN, "PF_UTUN differs from native");
static_assert(__SPRT_PF_VSOCK == PF_VSOCK, "PF_VSOCK differs from native");

// macOS MSG_* extras
static_assert(__SPRT_MSG_EOF == MSG_EOF && __SPRT_MSG_HOLD == MSG_HOLD,
		"MSG_EOF/HOLD differ from native");
static_assert(__SPRT_MSG_FLUSH == MSG_FLUSH, "MSG_FLUSH differs from native");
static_assert(__SPRT_MSG_SEND == MSG_SEND, "MSG_SEND differs from native");
static_assert(__SPRT_MSG_HAVEMORE == MSG_HAVEMORE && __SPRT_MSG_RCVMORE == MSG_RCVMORE,
		"MSG_HAVEMORE/RCVMORE differ from native");

static_assert(__SPRT_SCM_RIGHTS == SCM_RIGHTS, "SCM_RIGHTS differs from native");
static_assert(__SPRT_SCM_TIMESTAMP == SCM_TIMESTAMP, "SCM_TIMESTAMP differs from native");
static_assert(__SPRT_SCM_CREDS == SCM_CREDS, "SCM_CREDS differs from native");
static_assert(__SPRT_NET_MAXID == NET_MAXID, "NET_MAXID differs from native");
static_assert(__SPRT_NET_RT_DUMP == NET_RT_DUMP && __SPRT_NET_RT_FLAGS == NET_RT_FLAGS
				&& __SPRT_NET_RT_IFLIST == NET_RT_IFLIST && __SPRT_NET_RT_STAT == NET_RT_STAT,
		"NET_RT_* core differ from native");

#else

static_assert(__SPRT_PF_ASH == PF_ASH && __SPRT_PF_ECONET == PF_ECONET
				&& __SPRT_PF_ATMSVC == PF_ATMSVC && __SPRT_PF_RDS == PF_RDS
				&& __SPRT_PF_IRDA == PF_IRDA && __SPRT_PF_PPPOX == PF_PPPOX
				&& __SPRT_PF_WANPIPE == PF_WANPIPE && __SPRT_PF_LLC == PF_LLC
				&& __SPRT_PF_CAN == PF_CAN && __SPRT_PF_TIPC == PF_TIPC
				&& __SPRT_PF_BLUETOOTH == PF_BLUETOOTH && __SPRT_PF_IUCV == PF_IUCV
				&& __SPRT_PF_RXRPC == PF_RXRPC && __SPRT_PF_PHONET == PF_PHONET
				&& __SPRT_PF_IEEE802154 == PF_IEEE802154 && __SPRT_PF_CAIF == PF_CAIF
				&& __SPRT_PF_ALG == PF_ALG && __SPRT_PF_NFC == PF_NFC && __SPRT_PF_KCM == PF_KCM
				&& __SPRT_PF_QIPCRTR == PF_QIPCRTR,
		"PF_* extended (S-MAX) differ from native");

static_assert(__SPRT_PF_AX25 == PF_AX25 && __SPRT_PF_NETROM == PF_NETROM
				&& __SPRT_PF_BRIDGE == PF_BRIDGE && __SPRT_PF_ATMPVC == PF_ATMPVC
				&& __SPRT_PF_X25 == PF_X25 && __SPRT_PF_ROSE == PF_ROSE
				&& __SPRT_PF_NETBEUI == PF_NETBEUI && __SPRT_PF_SECURITY == PF_SECURITY
				&& __SPRT_PF_NETLINK == PF_NETLINK && __SPRT_PF_PACKET == PF_PACKET,
		"PF_* extended (A-R) differ from native");

static_assert(__SPRT_SOCK_DCCP == SOCK_DCCP && __SPRT_SOCK_PACKET == SOCK_PACKET,
		"SOCK_RDM/DCCP/PACKET differ from native");

static_assert(__SPRT_AF_AX25 == AF_AX25 && __SPRT_AF_NETROM == AF_NETROM
				&& __SPRT_AF_BRIDGE == AF_BRIDGE && __SPRT_AF_ATMPVC == AF_ATMPVC
				&& __SPRT_AF_X25 == AF_X25 && __SPRT_AF_ROSE == AF_ROSE
				&& __SPRT_AF_NETBEUI == AF_NETBEUI && __SPRT_AF_SECURITY == AF_SECURITY
				&& __SPRT_AF_KEY == AF_KEY && __SPRT_AF_NETLINK == AF_NETLINK
				&& __SPRT_AF_PACKET == AF_PACKET,
		"AF_* extended (A-R) differ from native");

static_assert(__SPRT_AF_ASH == AF_ASH && __SPRT_AF_ECONET == AF_ECONET
				&& __SPRT_AF_ATMSVC == AF_ATMSVC && __SPRT_AF_RDS == AF_RDS
				&& __SPRT_AF_IRDA == AF_IRDA && __SPRT_AF_PPPOX == AF_PPPOX
				&& __SPRT_AF_WANPIPE == AF_WANPIPE && __SPRT_AF_LLC == AF_LLC
				&& __SPRT_AF_CAN == AF_CAN && __SPRT_AF_TIPC == AF_TIPC
				&& __SPRT_AF_BLUETOOTH == AF_BLUETOOTH && __SPRT_AF_IUCV == AF_IUCV
				&& __SPRT_AF_RXRPC == AF_RXRPC && __SPRT_AF_PHONET == AF_PHONET
				&& __SPRT_AF_IEEE802154 == AF_IEEE802154 && __SPRT_AF_CAIF == AF_CAIF
				&& __SPRT_AF_ALG == AF_ALG && __SPRT_AF_NFC == AF_NFC && __SPRT_AF_KCM == AF_KCM
				&& __SPRT_AF_QIPCRTR == AF_QIPCRTR,
		"AF_* extended (S-MAX) differ from native");
static_assert(__SPRT_SO_PEERSEC == SO_PEERSEC, "SO_ACCEPTCONN/PEERSEC differ from native");
static_assert(__SPRT_SO_PEERSEC == SO_PEERSEC, "SO_ACCEPTCONN/PEERSEC differ from native");
static_assert(__SPRT_SO_SNDBUFFORCE == SO_SNDBUFFORCE, "SO_*BUFFORCE differ from native");
static_assert(__SPRT_SO_RCVBUFFORCE == SO_RCVBUFFORCE, "SO_*BUFFORCE differ from native");
static_assert(__SPRT_SO_PROTOCOL == SO_PROTOCOL && __SPRT_SO_DOMAIN == SO_DOMAIN,
		"SO_PROTOCOL/DOMAIN differ from native");
static_assert(__SPRT_SO_NO_CHECK == SO_NO_CHECK,
		"SO_DEBUG/NO_CHECK/PRIORITY/BSDCOMPAT differ from native");
static_assert(__SPRT_SO_PRIORITY == SO_PRIORITY,
		"SO_DEBUG/NO_CHECK/PRIORITY/BSDCOMPAT differ from native");
static_assert(__SPRT_SO_BSDCOMPAT == SO_BSDCOMPAT,
		"SO_DEBUG/NO_CHECK/PRIORITY/BSDCOMPAT differ from native");
static_assert(__SPRT_SO_PASSCRED == SO_PASSCRED,
		"SO_PASSCRED/PEERCRED/RCVLOWAT/SNDLOWAT differ from native");
static_assert(__SPRT_SO_PEERCRED == SO_PEERCRED,
		"SO_PASSCRED/PEERCRED/RCVLOWAT/SNDLOWAT differ from native");
static_assert(__SPRT_SO_TIMESTAMPNS == SO_TIMESTAMPNS && __SPRT_SO_TIMESTAMPING == SO_TIMESTAMPING,
		"SO_TIMESTAMP* differ from native");
static_assert(__SPRT_SO_SECURITY_AUTHENTICATION == SO_SECURITY_AUTHENTICATION
				&& __SPRT_SO_SECURITY_ENCRYPTION_TRANSPORT == SO_SECURITY_ENCRYPTION_TRANSPORT
				&& __SPRT_SO_SECURITY_ENCRYPTION_NETWORK == SO_SECURITY_ENCRYPTION_NETWORK,
		"SO_SECURITY_* differ from native");
static_assert(__SPRT_SO_BINDTODEVICE == SO_BINDTODEVICE, "SO_BINDTODEVICE differs from native");
static_assert(__SPRT_SO_ATTACH_FILTER == SO_ATTACH_FILTER
				&& __SPRT_SO_DETACH_FILTER == SO_DETACH_FILTER
				&& __SPRT_SO_GET_FILTER == SO_GET_FILTER,
		"SO_*_FILTER differ from native");
static_assert(__SPRT_SO_PEERNAME == SO_PEERNAME, "SO_PEERNAME differs from native");
static_assert(__SPRT_SO_PASSSEC == SO_PASSSEC, "SO_PASSSEC differs from native");
static_assert(__SPRT_SCM_TIMESTAMPNS == SCM_TIMESTAMPNS, "SCM_TIMESTAMPNS differs from native");
static_assert(__SPRT_SO_MARK == SO_MARK, "SO_MARK differs from native");
static_assert(__SPRT_SO_RXQ_OVFL == SO_RXQ_OVFL, "SO_RXQ_OVFL differs from native");
static_assert(__SPRT_SO_WIFI_STATUS == SO_WIFI_STATUS && __SPRT_SCM_WIFI_STATUS == SCM_WIFI_STATUS,
		"SO/SCM_WIFI_STATUS differ from native");
static_assert(__SPRT_SO_PEEK_OFF == SO_PEEK_OFF, "SO_PEEK_OFF differs from native");
static_assert(__SPRT_SO_NOFCS == SO_NOFCS, "SO_NOFCS differs from native");
static_assert(__SPRT_SO_LOCK_FILTER == SO_LOCK_FILTER, "SO_LOCK_FILTER differs from native");
static_assert(__SPRT_SO_SELECT_ERR_QUEUE == SO_SELECT_ERR_QUEUE,
		"SO_SELECT_ERR_QUEUE differs from native");
static_assert(__SPRT_SO_BUSY_POLL == SO_BUSY_POLL && __SPRT_SO_MAX_PACING_RATE == SO_MAX_PACING_RATE
				&& __SPRT_SO_BPF_EXTENSIONS == SO_BPF_EXTENSIONS,
		"SO_BUSY_POLL/MAX_PACING_RATE/BPF_EXTENSIONS differ from native");
static_assert(__SPRT_SO_INCOMING_CPU == SO_INCOMING_CPU, "SO_INCOMING_CPU differs from native");
static_assert(__SPRT_SO_ATTACH_BPF == SO_ATTACH_BPF && __SPRT_SO_DETACH_BPF == SO_DETACH_BPF,
		"SO_*_BPF differ from native");
static_assert(__SPRT_SO_CNX_ADVICE == SO_CNX_ADVICE, "SO_CNX_ADVICE differs from native");
static_assert(__SPRT_SCM_TIMESTAMPING_OPT_STATS == SCM_TIMESTAMPING_OPT_STATS,
		"SCM_TIMESTAMPING_OPT_STATS differs from native");
static_assert(__SPRT_SO_MEMINFO == SO_MEMINFO, "SO_MEMINFO differs from native");
static_assert(__SPRT_SO_INCOMING_NAPI_ID == SO_INCOMING_NAPI_ID,
		"SO_INCOMING_NAPI_ID differs from native");
static_assert(__SPRT_SO_COOKIE == SO_COOKIE, "SO_COOKIE differs from native");
static_assert(__SPRT_SCM_TIMESTAMPING_PKTINFO == SCM_TIMESTAMPING_PKTINFO,
		"SCM_TIMESTAMPING_PKTINFO differs from native");
static_assert(__SPRT_SO_PEERGROUPS == SO_PEERGROUPS, "SO_PEERGROUPS differs from native");
static_assert(__SPRT_SO_ZEROCOPY == SO_ZEROCOPY, "SO_ZEROCOPY differs from native");
static_assert(__SPRT_SO_TXTIME == SO_TXTIME && __SPRT_SCM_TXTIME == SCM_TXTIME,
		"SO/SCM_TXTIME differ from native");
static_assert(__SPRT_SO_BINDTOIFINDEX == SO_BINDTOIFINDEX, "SO_BINDTOIFINDEX differs from native");
static_assert(__SPRT_SO_DETACH_REUSEPORT_BPF == SO_DETACH_REUSEPORT_BPF,
		"SO_DETACH_REUSEPORT_BPF differs from native");

static_assert(__SPRT_SOL_IP == SOL_IP && __SPRT_SOL_IPV6 == SOL_IPV6
				&& __SPRT_SOL_ICMPV6 == SOL_ICMPV6,
		"SOL_IP/IPV6/ICMPV6 differ from native");
static_assert(__SPRT_SOL_RAW == SOL_RAW, "SOL_RAW differs from native");
static_assert(__SPRT_SOL_DECNET == SOL_DECNET && __SPRT_SOL_X25 == SOL_X25
				&& __SPRT_SOL_PACKET == SOL_PACKET && __SPRT_SOL_ATM == SOL_ATM
				&& __SPRT_SOL_AAL == SOL_AAL,
		"SOL_DECNET/X25/PACKET/ATM/AAL differ from native");
static_assert(__SPRT_SOL_IRDA == SOL_IRDA && __SPRT_SOL_NETBEUI == SOL_NETBEUI
				&& __SPRT_SOL_LLC == SOL_LLC && __SPRT_SOL_DCCP == SOL_DCCP,
		"SOL_IRDA/NETBEUI/LLC/DCCP differ from native");
static_assert(__SPRT_SOL_NETLINK == SOL_NETLINK && __SPRT_SOL_TIPC == SOL_TIPC
				&& __SPRT_SOL_RXRPC == SOL_RXRPC,
		"SOL_NETLINK/TIPC/RXRPC differ from native");
static_assert(__SPRT_SOL_PPPOL2TP == SOL_PPPOL2TP && __SPRT_SOL_BLUETOOTH == SOL_BLUETOOTH
				&& __SPRT_SOL_PNPIPE == SOL_PNPIPE && __SPRT_SOL_RDS == SOL_RDS,
		"SOL_PPPOL2TP/BLUETOOTH/PNPIPE/RDS differ from native");
static_assert(__SPRT_SOL_IUCV == SOL_IUCV && __SPRT_SOL_CAIF == SOL_CAIF
				&& __SPRT_SOL_ALG == SOL_ALG && __SPRT_SOL_NFC == SOL_NFC,
		"SOL_IUCV/CAIF/ALG/NFC differ from native");
static_assert(__SPRT_SOL_KCM == SOL_KCM && __SPRT_SOL_TLS == SOL_TLS,
		"SOL_KCM/TLS/XDP differ from native");

static_assert(__SPRT_MSG_FIN == MSG_FIN && __SPRT_MSG_SYN == MSG_SYN
				&& __SPRT_MSG_CONFIRM == MSG_CONFIRM,
		"MSG_FIN/SYN/CONFIRM differ from native");
static_assert(__SPRT_MSG_RST == MSG_RST, "MSG_RST differs from native");
static_assert(__SPRT_MSG_ERRQUEUE == MSG_ERRQUEUE && __SPRT_MSG_MORE == MSG_MORE,
		"MSG_ERRQUEUE/MORE differ from native");
static_assert(__SPRT_MSG_WAITFORONE == MSG_WAITFORONE, "MSG_WAITFORONE differs from native");
static_assert(__SPRT_MSG_BATCH == MSG_BATCH, "MSG_BATCH differs from native");

static_assert(__SPRT_MSG_FASTOPEN == MSG_FASTOPEN, "MSG_FASTOPEN differs from native");
static_assert(__SPRT_MSG_CMSG_CLOEXEC == MSG_CMSG_CLOEXEC, "MSG_CMSG_CLOEXEC differs from native");

#endif

// AF_* aliases (those that are not the core four already asserted above)

static_assert(__SPRT_AF_IPX == AF_IPX && __SPRT_AF_APPLETALK == AF_APPLETALK
				&& __SPRT_AF_DECnet == AF_DECnet && __SPRT_AF_ROUTE == AF_ROUTE,
		"AF_* extended (A-R) differ from native");

static_assert(__SPRT_AF_SNA == AF_SNA && __SPRT_AF_ISDN == AF_ISDN && __SPRT_AF_VSOCK == AF_VSOCK,
		"AF_* extended (S-MAX) differ from native");

static_assert(__SPRT_SO_DEBUG == SO_DEBUG,
		"SO_DEBUG/NO_CHECK/PRIORITY/BSDCOMPAT differ from native");
static_assert(__SPRT_SO_RCVLOWAT == SO_RCVLOWAT,
		"SO_PASSCRED/PEERCRED/RCVLOWAT/SNDLOWAT differ from native");
static_assert(__SPRT_SO_SNDLOWAT == SO_SNDLOWAT,
		"SO_PASSCRED/PEERCRED/RCVLOWAT/SNDLOWAT differ from native");
static_assert(__SPRT_SO_ACCEPTCONN == SO_ACCEPTCONN, "SO_ACCEPTCONN/PEERSEC differ from native");
static_assert(__SPRT_SO_RCVTIMEO == SO_RCVTIMEO && __SPRT_SO_SNDTIMEO == SO_SNDTIMEO,
		"SO_*TIMEO differ from native");
static_assert(__SPRT_SO_TIMESTAMP == SO_TIMESTAMP, "SO_TIMESTAMP* differ from native");

static_assert(__SPRT_SCM_TIMESTAMP == SCM_TIMESTAMP, "SCM_TIMESTAMP differs from native");

#if defined(__SPRT_AF_LOCAL) || defined(AF_LOCAL)
static_assert(__SPRT_AF_LOCAL == AF_LOCAL, "AF_LOCAL differs from native");
#endif

#if defined(__SPRT_PF_FILE) || defined(PF_FILE)
static_assert(__SPRT_PF_FILE == PF_FILE, "PF_FILE differs from native");
#endif

#if defined(__SPRT_AF_FILE) || defined(AF_FILE)
static_assert(__SPRT_AF_FILE == AF_FILE, "AF_FILE differs from native");
#endif

#if defined(__SPRT_SOL_XDP) || defined(SOL_XDP)
static_assert(__SPRT_SOL_XDP == SOL_XDP, "SOL_XDP  differs from native");
#endif

#if defined(__SPRT_MSG_PROXY) || defined(MSG_PROXY)
static_assert(__SPRT_MSG_PROXY == MSG_PROXY, "MSG_PROXY differs from native");
#endif

#if defined(__SPRT_MSG_ZEROCOPY) || defined(MSG_ZEROCOPY)
static_assert(__SPRT_MSG_ZEROCOPY == MSG_ZEROCOPY, "MSG_ZEROCOPY differs from native");
#endif

#if defined(__SPRT_AF_IB) || defined(AF_IB)
static_assert(__SPRT_AF_IB == AF_IB, "AF_IB/MPLS differ from native");
#endif

#if defined(__SPRT_AF_MPLS) || defined(AF_MPLS)
static_assert(__SPRT_AF_MPLS == AF_MPLS, "AF_IB/MPLS differ from native");
#endif

#if defined(__SPRT_AF_SMC) || defined(AF_SMC)
static_assert(__SPRT_AF_SMC == AF_SMC, "AF_SMC/XDP differ from native");
#endif

#if defined(__SPRT_AF_IB) || defined(AF_XDP)
static_assert(__SPRT_AF_XDP == AF_XDP, "AF_SMC/XDP differ from native");
#endif

#if defined(__SPRT_PF_IB) || defined(PF_IB)
static_assert(__SPRT_PF_IB == PF_IB, "PF_IB/MPLS differ from native");
#endif

#if defined(__SPRT_PF_MPLS) || defined(PF_MPLS)
static_assert(__SPRT_PF_MPLS == PF_MPLS, "PF_IB/MPLS differ from native");
#endif

#if defined(__SPRT_PF_SMC) || defined(PF_SMC)
static_assert(__SPRT_PF_SMC == PF_SMC, "PF_SMC/XDP differ from native");
#endif

#if defined(__SPRT_PF_XDP) || defined(PF_XDP)
static_assert(__SPRT_PF_XDP == PF_XDP, "PF_SMC/XDP differ from native");
#endif

#if defined(__SPRT_SCM_RIGHTS) || defined(SCM_RIGHTS)
static_assert(__SPRT_SCM_RIGHTS == SCM_RIGHTS, "PF_SMC/XDP differ from native");
#endif

#if defined(__SPRT_SCM_CREDENTIALS) || defined(SCM_CREDENTIALS)
static_assert(__SPRT_SCM_CREDENTIALS == SCM_CREDENTIALS, "PF_SMC/XDP differ from native");
#endif

#if defined(SCM_SECURITY)
static_assert(__SPRT_SCM_SECURITY == SCM_SECURITY, "PF_SMC/XDP differ from native");
#endif

#define __SPRT_SCM_RIGHTS 0x01
#define __SPRT_SCM_CREDENTIALS 0x02
#define __SPRT_SCM_SECURITY 0x03


// netinet constants (<sprt/c/cross/__sprt_netinet.h>) vs native <netinet/in.h>.
// Each hosted platform pulls its own per-platform netinetdef.h (linux/android/
// macos), so the values are validated against the native header on every hosted
// target. The `defined(__SPRT_X) || defined(X)` guard enforces a 1-1 mapping: if
// either side defines a name the assert must compile, so a per-platform table
// that claims an option its libc lacks (or omits one its libc defines) is a build
// error. The per-platform tables are therefore trimmed to each libc's exact
// <netinet/in.h> surface (e.g. bionic lacks IP_PMTUDISC / IPV6_RTHDR_*, glibc
// gates IPV6_PREFER_SRC_* behind _GNU_SOURCE).
// --- string lengths ---
#if defined(__SPRT_INET_ADDRSTRLEN) || defined(INET_ADDRSTRLEN)
static_assert(__SPRT_INET_ADDRSTRLEN == INET_ADDRSTRLEN, "INET_ADDRSTRLEN differs from native");
#endif
#if defined(__SPRT_INET6_ADDRSTRLEN) || defined(INET6_ADDRSTRLEN)
static_assert(__SPRT_INET6_ADDRSTRLEN == INET6_ADDRSTRLEN, "INET6_ADDRSTRLEN differs from native");
#endif
// --- IP protocols ---
#if defined(__SPRT_IPPROTO_IP) || defined(IPPROTO_IP)
static_assert(__SPRT_IPPROTO_IP == IPPROTO_IP, "IPPROTO_IP differs from native");
#endif
#if defined(__SPRT_IPPROTO_HOPOPTS) || defined(IPPROTO_HOPOPTS)
static_assert(__SPRT_IPPROTO_HOPOPTS == IPPROTO_HOPOPTS, "IPPROTO_HOPOPTS differs from native");
#endif
#if defined(__SPRT_IPPROTO_ICMP) || defined(IPPROTO_ICMP)
static_assert(__SPRT_IPPROTO_ICMP == IPPROTO_ICMP, "IPPROTO_ICMP differs from native");
#endif
#if defined(__SPRT_IPPROTO_IGMP) || defined(IPPROTO_IGMP)
static_assert(__SPRT_IPPROTO_IGMP == IPPROTO_IGMP, "IPPROTO_IGMP differs from native");
#endif
#if defined(__SPRT_IPPROTO_IPIP) || defined(IPPROTO_IPIP)
static_assert(__SPRT_IPPROTO_IPIP == IPPROTO_IPIP, "IPPROTO_IPIP differs from native");
#endif
#if defined(__SPRT_IPPROTO_TCP) || defined(IPPROTO_TCP)
static_assert(__SPRT_IPPROTO_TCP == IPPROTO_TCP, "IPPROTO_TCP differs from native");
#endif
#if defined(__SPRT_IPPROTO_EGP) || defined(IPPROTO_EGP)
static_assert(__SPRT_IPPROTO_EGP == IPPROTO_EGP, "IPPROTO_EGP differs from native");
#endif
#if defined(__SPRT_IPPROTO_PUP) || defined(IPPROTO_PUP)
static_assert(__SPRT_IPPROTO_PUP == IPPROTO_PUP, "IPPROTO_PUP differs from native");
#endif
#if defined(__SPRT_IPPROTO_UDP) || defined(IPPROTO_UDP)
static_assert(__SPRT_IPPROTO_UDP == IPPROTO_UDP, "IPPROTO_UDP differs from native");
#endif
#if defined(__SPRT_IPPROTO_IDP) || defined(IPPROTO_IDP)
static_assert(__SPRT_IPPROTO_IDP == IPPROTO_IDP, "IPPROTO_IDP differs from native");
#endif
#if defined(__SPRT_IPPROTO_TP) || defined(IPPROTO_TP)
static_assert(__SPRT_IPPROTO_TP == IPPROTO_TP, "IPPROTO_TP differs from native");
#endif
#if defined(__SPRT_IPPROTO_DCCP) || defined(IPPROTO_DCCP)
static_assert(__SPRT_IPPROTO_DCCP == IPPROTO_DCCP, "IPPROTO_DCCP differs from native");
#endif
#if defined(__SPRT_IPPROTO_IPV6) || defined(IPPROTO_IPV6)
static_assert(__SPRT_IPPROTO_IPV6 == IPPROTO_IPV6, "IPPROTO_IPV6 differs from native");
#endif
#if defined(__SPRT_IPPROTO_ROUTING) || defined(IPPROTO_ROUTING)
static_assert(__SPRT_IPPROTO_ROUTING == IPPROTO_ROUTING, "IPPROTO_ROUTING differs from native");
#endif
#if defined(__SPRT_IPPROTO_FRAGMENT) || defined(IPPROTO_FRAGMENT)
static_assert(__SPRT_IPPROTO_FRAGMENT == IPPROTO_FRAGMENT, "IPPROTO_FRAGMENT differs from native");
#endif
#if defined(__SPRT_IPPROTO_RSVP) || defined(IPPROTO_RSVP)
static_assert(__SPRT_IPPROTO_RSVP == IPPROTO_RSVP, "IPPROTO_RSVP differs from native");
#endif
#if defined(__SPRT_IPPROTO_GRE) || defined(IPPROTO_GRE)
static_assert(__SPRT_IPPROTO_GRE == IPPROTO_GRE, "IPPROTO_GRE differs from native");
#endif
#if defined(__SPRT_IPPROTO_ESP) || defined(IPPROTO_ESP)
static_assert(__SPRT_IPPROTO_ESP == IPPROTO_ESP, "IPPROTO_ESP differs from native");
#endif
#if defined(__SPRT_IPPROTO_AH) || defined(IPPROTO_AH)
static_assert(__SPRT_IPPROTO_AH == IPPROTO_AH, "IPPROTO_AH differs from native");
#endif
#if defined(__SPRT_IPPROTO_ICMPV6) || defined(IPPROTO_ICMPV6)
static_assert(__SPRT_IPPROTO_ICMPV6 == IPPROTO_ICMPV6, "IPPROTO_ICMPV6 differs from native");
#endif
#if defined(__SPRT_IPPROTO_NONE) || defined(IPPROTO_NONE)
static_assert(__SPRT_IPPROTO_NONE == IPPROTO_NONE, "IPPROTO_NONE differs from native");
#endif
#if defined(__SPRT_IPPROTO_DSTOPTS) || defined(IPPROTO_DSTOPTS)
static_assert(__SPRT_IPPROTO_DSTOPTS == IPPROTO_DSTOPTS, "IPPROTO_DSTOPTS differs from native");
#endif
#if defined(__SPRT_IPPROTO_MTP) || defined(IPPROTO_MTP)
static_assert(__SPRT_IPPROTO_MTP == IPPROTO_MTP, "IPPROTO_MTP differs from native");
#endif
#if defined(__SPRT_IPPROTO_BEETPH) || defined(IPPROTO_BEETPH)
static_assert(__SPRT_IPPROTO_BEETPH == IPPROTO_BEETPH, "IPPROTO_BEETPH differs from native");
#endif
#if defined(__SPRT_IPPROTO_ENCAP) || defined(IPPROTO_ENCAP)
static_assert(__SPRT_IPPROTO_ENCAP == IPPROTO_ENCAP, "IPPROTO_ENCAP differs from native");
#endif
#if defined(__SPRT_IPPROTO_PIM) || defined(IPPROTO_PIM)
static_assert(__SPRT_IPPROTO_PIM == IPPROTO_PIM, "IPPROTO_PIM differs from native");
#endif
#if defined(__SPRT_IPPROTO_COMP) || defined(IPPROTO_COMP)
static_assert(__SPRT_IPPROTO_COMP == IPPROTO_COMP, "IPPROTO_COMP differs from native");
#endif
#if defined(__SPRT_IPPROTO_SCTP) || defined(IPPROTO_SCTP)
static_assert(__SPRT_IPPROTO_SCTP == IPPROTO_SCTP, "IPPROTO_SCTP differs from native");
#endif
#if defined(__SPRT_IPPROTO_MH) || defined(IPPROTO_MH)
static_assert(__SPRT_IPPROTO_MH == IPPROTO_MH, "IPPROTO_MH differs from native");
#endif
#if defined(__SPRT_IPPROTO_UDPLITE) || defined(IPPROTO_UDPLITE)
static_assert(__SPRT_IPPROTO_UDPLITE == IPPROTO_UDPLITE, "IPPROTO_UDPLITE differs from native");
#endif
#if defined(__SPRT_IPPROTO_MPLS) || defined(IPPROTO_MPLS)
static_assert(__SPRT_IPPROTO_MPLS == IPPROTO_MPLS, "IPPROTO_MPLS differs from native");
#endif
#if defined(__SPRT_IPPROTO_ETHERNET) || defined(IPPROTO_ETHERNET)
static_assert(__SPRT_IPPROTO_ETHERNET == IPPROTO_ETHERNET, "IPPROTO_ETHERNET differs from native");
#endif
#if defined(__SPRT_IPPROTO_RAW) || defined(IPPROTO_RAW)
static_assert(__SPRT_IPPROTO_RAW == IPPROTO_RAW, "IPPROTO_RAW differs from native");
#endif
#if defined(__SPRT_IPPROTO_MPTCP) || defined(IPPROTO_MPTCP)
static_assert(__SPRT_IPPROTO_MPTCP == IPPROTO_MPTCP, "IPPROTO_MPTCP differs from native");
#endif
#if defined(__SPRT_IPPROTO_MAX) || defined(IPPROTO_MAX)
static_assert(__SPRT_IPPROTO_MAX == IPPROTO_MAX, "IPPROTO_MAX differs from native");
#endif
// --- ports / IPv4 classes ---
#if defined(__SPRT_IPPORT_RESERVED) || defined(IPPORT_RESERVED)
static_assert(__SPRT_IPPORT_RESERVED == IPPORT_RESERVED, "IPPORT_RESERVED differs from native");
#endif
#if defined(__SPRT_IN_CLASSA_NET) || defined(IN_CLASSA_NET)
static_assert(__SPRT_IN_CLASSA_NET == IN_CLASSA_NET, "IN_CLASSA_NET differs from native");
#endif
#if defined(__SPRT_IN_CLASSA_NSHIFT) || defined(IN_CLASSA_NSHIFT)
static_assert(__SPRT_IN_CLASSA_NSHIFT == IN_CLASSA_NSHIFT, "IN_CLASSA_NSHIFT differs from native");
#endif
#if defined(__SPRT_IN_CLASSA_HOST) || defined(IN_CLASSA_HOST)
static_assert(__SPRT_IN_CLASSA_HOST == IN_CLASSA_HOST, "IN_CLASSA_HOST differs from native");
#endif
#if defined(__SPRT_IN_CLASSA_MAX) || defined(IN_CLASSA_MAX)
static_assert(__SPRT_IN_CLASSA_MAX == IN_CLASSA_MAX, "IN_CLASSA_MAX differs from native");
#endif
#if defined(__SPRT_IN_CLASSB_NET) || defined(IN_CLASSB_NET)
static_assert(__SPRT_IN_CLASSB_NET == IN_CLASSB_NET, "IN_CLASSB_NET differs from native");
#endif
#if defined(__SPRT_IN_CLASSB_NSHIFT) || defined(IN_CLASSB_NSHIFT)
static_assert(__SPRT_IN_CLASSB_NSHIFT == IN_CLASSB_NSHIFT, "IN_CLASSB_NSHIFT differs from native");
#endif
#if defined(__SPRT_IN_CLASSB_HOST) || defined(IN_CLASSB_HOST)
static_assert(__SPRT_IN_CLASSB_HOST == IN_CLASSB_HOST, "IN_CLASSB_HOST differs from native");
#endif
#if defined(__SPRT_IN_CLASSB_MAX) || defined(IN_CLASSB_MAX)
static_assert(__SPRT_IN_CLASSB_MAX == IN_CLASSB_MAX, "IN_CLASSB_MAX differs from native");
#endif
#if defined(__SPRT_IN_CLASSC_NET) || defined(IN_CLASSC_NET)
static_assert(__SPRT_IN_CLASSC_NET == IN_CLASSC_NET, "IN_CLASSC_NET differs from native");
#endif
#if defined(__SPRT_IN_CLASSC_NSHIFT) || defined(IN_CLASSC_NSHIFT)
static_assert(__SPRT_IN_CLASSC_NSHIFT == IN_CLASSC_NSHIFT, "IN_CLASSC_NSHIFT differs from native");
#endif
#if defined(__SPRT_IN_CLASSC_HOST) || defined(IN_CLASSC_HOST)
static_assert(__SPRT_IN_CLASSC_HOST == IN_CLASSC_HOST, "IN_CLASSC_HOST differs from native");
#endif
#if defined(__SPRT_IN_LOOPBACKNET) || defined(IN_LOOPBACKNET)
static_assert(__SPRT_IN_LOOPBACKNET == IN_LOOPBACKNET, "IN_LOOPBACKNET differs from native");
#endif
// --- IPv4 options ---
#if defined(__SPRT_IP_TOS) || defined(IP_TOS)
static_assert(__SPRT_IP_TOS == IP_TOS, "IP_TOS differs from native");
#endif
#if defined(__SPRT_IP_TTL) || defined(IP_TTL)
static_assert(__SPRT_IP_TTL == IP_TTL, "IP_TTL differs from native");
#endif
#if defined(__SPRT_IP_HDRINCL) || defined(IP_HDRINCL)
static_assert(__SPRT_IP_HDRINCL == IP_HDRINCL, "IP_HDRINCL differs from native");
#endif
#if defined(__SPRT_IP_OPTIONS) || defined(IP_OPTIONS)
static_assert(__SPRT_IP_OPTIONS == IP_OPTIONS, "IP_OPTIONS differs from native");
#endif
#if defined(__SPRT_IP_ROUTER_ALERT) || defined(IP_ROUTER_ALERT)
static_assert(__SPRT_IP_ROUTER_ALERT == IP_ROUTER_ALERT, "IP_ROUTER_ALERT differs from native");
#endif
#if defined(__SPRT_IP_RECVOPTS) || defined(IP_RECVOPTS)
static_assert(__SPRT_IP_RECVOPTS == IP_RECVOPTS, "IP_RECVOPTS differs from native");
#endif
#if defined(__SPRT_IP_RETOPTS) || defined(IP_RETOPTS)
static_assert(__SPRT_IP_RETOPTS == IP_RETOPTS, "IP_RETOPTS differs from native");
#endif
#if defined(__SPRT_IP_PKTINFO) || defined(IP_PKTINFO)
static_assert(__SPRT_IP_PKTINFO == IP_PKTINFO, "IP_PKTINFO differs from native");
#endif
#if defined(__SPRT_IP_PKTOPTIONS) || defined(IP_PKTOPTIONS)
static_assert(__SPRT_IP_PKTOPTIONS == IP_PKTOPTIONS, "IP_PKTOPTIONS differs from native");
#endif
#if defined(__SPRT_IP_PMTUDISC) || defined(IP_PMTUDISC)
static_assert(__SPRT_IP_PMTUDISC == IP_PMTUDISC, "IP_PMTUDISC differs from native");
#endif
#if defined(__SPRT_IP_MTU_DISCOVER) || defined(IP_MTU_DISCOVER)
static_assert(__SPRT_IP_MTU_DISCOVER == IP_MTU_DISCOVER, "IP_MTU_DISCOVER differs from native");
#endif
#if defined(__SPRT_IP_RECVERR) || defined(IP_RECVERR)
static_assert(__SPRT_IP_RECVERR == IP_RECVERR, "IP_RECVERR differs from native");
#endif
#if defined(__SPRT_IP_RECVTTL) || defined(IP_RECVTTL)
static_assert(__SPRT_IP_RECVTTL == IP_RECVTTL, "IP_RECVTTL differs from native");
#endif
#if defined(__SPRT_IP_RECVTOS) || defined(IP_RECVTOS)
static_assert(__SPRT_IP_RECVTOS == IP_RECVTOS, "IP_RECVTOS differs from native");
#endif
#if defined(__SPRT_IP_MTU) || defined(IP_MTU)
static_assert(__SPRT_IP_MTU == IP_MTU, "IP_MTU differs from native");
#endif
#if defined(__SPRT_IP_FREEBIND) || defined(IP_FREEBIND)
static_assert(__SPRT_IP_FREEBIND == IP_FREEBIND, "IP_FREEBIND differs from native");
#endif
#if defined(__SPRT_IP_IPSEC_POLICY) || defined(IP_IPSEC_POLICY)
static_assert(__SPRT_IP_IPSEC_POLICY == IP_IPSEC_POLICY, "IP_IPSEC_POLICY differs from native");
#endif
#if defined(__SPRT_IP_XFRM_POLICY) || defined(IP_XFRM_POLICY)
static_assert(__SPRT_IP_XFRM_POLICY == IP_XFRM_POLICY, "IP_XFRM_POLICY differs from native");
#endif
#if defined(__SPRT_IP_PASSSEC) || defined(IP_PASSSEC)
static_assert(__SPRT_IP_PASSSEC == IP_PASSSEC, "IP_PASSSEC differs from native");
#endif
#if defined(__SPRT_IP_TRANSPARENT) || defined(IP_TRANSPARENT)
static_assert(__SPRT_IP_TRANSPARENT == IP_TRANSPARENT, "IP_TRANSPARENT differs from native");
#endif
#if defined(__SPRT_IP_ORIGDSTADDR) || defined(IP_ORIGDSTADDR)
static_assert(__SPRT_IP_ORIGDSTADDR == IP_ORIGDSTADDR, "IP_ORIGDSTADDR differs from native");
#endif
#if defined(__SPRT_IP_RECVORIGDSTADDR) || defined(IP_RECVORIGDSTADDR)
static_assert(__SPRT_IP_RECVORIGDSTADDR == IP_RECVORIGDSTADDR,
		"IP_RECVORIGDSTADDR differs from native");
#endif
#if defined(__SPRT_IP_MINTTL) || defined(IP_MINTTL)
static_assert(__SPRT_IP_MINTTL == IP_MINTTL, "IP_MINTTL differs from native");
#endif
#if defined(__SPRT_IP_NODEFRAG) || defined(IP_NODEFRAG)
static_assert(__SPRT_IP_NODEFRAG == IP_NODEFRAG, "IP_NODEFRAG differs from native");
#endif
#if defined(__SPRT_IP_CHECKSUM) || defined(IP_CHECKSUM)
static_assert(__SPRT_IP_CHECKSUM == IP_CHECKSUM, "IP_CHECKSUM differs from native");
#endif
#if defined(__SPRT_IP_BIND_ADDRESS_NO_PORT) || defined(IP_BIND_ADDRESS_NO_PORT)
static_assert(__SPRT_IP_BIND_ADDRESS_NO_PORT == IP_BIND_ADDRESS_NO_PORT,
		"IP_BIND_ADDRESS_NO_PORT differs from native");
#endif
#if defined(__SPRT_IP_RECVFRAGSIZE) || defined(IP_RECVFRAGSIZE)
static_assert(__SPRT_IP_RECVFRAGSIZE == IP_RECVFRAGSIZE, "IP_RECVFRAGSIZE differs from native");
#endif
#if defined(__SPRT_IP_RECVERR_RFC4884) || defined(IP_RECVERR_RFC4884)
static_assert(__SPRT_IP_RECVERR_RFC4884 == IP_RECVERR_RFC4884,
		"IP_RECVERR_RFC4884 differs from native");
#endif
#if defined(__SPRT_IP_MULTICAST_IF) || defined(IP_MULTICAST_IF)
static_assert(__SPRT_IP_MULTICAST_IF == IP_MULTICAST_IF, "IP_MULTICAST_IF differs from native");
#endif
#if defined(__SPRT_IP_MULTICAST_TTL) || defined(IP_MULTICAST_TTL)
static_assert(__SPRT_IP_MULTICAST_TTL == IP_MULTICAST_TTL, "IP_MULTICAST_TTL differs from native");
#endif
#if defined(__SPRT_IP_MULTICAST_LOOP) || defined(IP_MULTICAST_LOOP)
static_assert(__SPRT_IP_MULTICAST_LOOP == IP_MULTICAST_LOOP,
		"IP_MULTICAST_LOOP differs from native");
#endif
#if defined(__SPRT_IP_ADD_MEMBERSHIP) || defined(IP_ADD_MEMBERSHIP)
static_assert(__SPRT_IP_ADD_MEMBERSHIP == IP_ADD_MEMBERSHIP,
		"IP_ADD_MEMBERSHIP differs from native");
#endif
#if defined(__SPRT_IP_DROP_MEMBERSHIP) || defined(IP_DROP_MEMBERSHIP)
static_assert(__SPRT_IP_DROP_MEMBERSHIP == IP_DROP_MEMBERSHIP,
		"IP_DROP_MEMBERSHIP differs from native");
#endif
#if defined(__SPRT_IP_UNBLOCK_SOURCE) || defined(IP_UNBLOCK_SOURCE)
static_assert(__SPRT_IP_UNBLOCK_SOURCE == IP_UNBLOCK_SOURCE,
		"IP_UNBLOCK_SOURCE differs from native");
#endif
#if defined(__SPRT_IP_BLOCK_SOURCE) || defined(IP_BLOCK_SOURCE)
static_assert(__SPRT_IP_BLOCK_SOURCE == IP_BLOCK_SOURCE, "IP_BLOCK_SOURCE differs from native");
#endif
#if defined(__SPRT_IP_ADD_SOURCE_MEMBERSHIP) || defined(IP_ADD_SOURCE_MEMBERSHIP)
static_assert(__SPRT_IP_ADD_SOURCE_MEMBERSHIP == IP_ADD_SOURCE_MEMBERSHIP,
		"IP_ADD_SOURCE_MEMBERSHIP differs from native");
#endif
#if defined(__SPRT_IP_DROP_SOURCE_MEMBERSHIP) || defined(IP_DROP_SOURCE_MEMBERSHIP)
static_assert(__SPRT_IP_DROP_SOURCE_MEMBERSHIP == IP_DROP_SOURCE_MEMBERSHIP,
		"IP_DROP_SOURCE_MEMBERSHIP differs from native");
#endif
#if defined(__SPRT_IP_MSFILTER) || defined(IP_MSFILTER)
static_assert(__SPRT_IP_MSFILTER == IP_MSFILTER, "IP_MSFILTER differs from native");
#endif
#if defined(__SPRT_IP_MULTICAST_ALL) || defined(IP_MULTICAST_ALL)
static_assert(__SPRT_IP_MULTICAST_ALL == IP_MULTICAST_ALL, "IP_MULTICAST_ALL differs from native");
#endif
#if defined(__SPRT_IP_UNICAST_IF) || defined(IP_UNICAST_IF)
static_assert(__SPRT_IP_UNICAST_IF == IP_UNICAST_IF, "IP_UNICAST_IF differs from native");
#endif
#if defined(__SPRT_IP_RECVRETOPTS) || defined(IP_RECVRETOPTS)
static_assert(__SPRT_IP_RECVRETOPTS == IP_RECVRETOPTS, "IP_RECVRETOPTS differs from native");
#endif
#if defined(__SPRT_IP_DONTFRAGMENT) || defined(IP_DONTFRAGMENT)
static_assert(__SPRT_IP_DONTFRAGMENT == IP_DONTFRAGMENT, "IP_DONTFRAGMENT differs from native");
#endif
#if defined(__SPRT_IP_HOPLIMIT) || defined(IP_HOPLIMIT)
static_assert(__SPRT_IP_HOPLIMIT == IP_HOPLIMIT, "IP_HOPLIMIT differs from native");
#endif
#if defined(__SPRT_IP_RECEIVE_BROADCAST) || defined(IP_RECEIVE_BROADCAST)
static_assert(__SPRT_IP_RECEIVE_BROADCAST == IP_RECEIVE_BROADCAST,
		"IP_RECEIVE_BROADCAST differs from native");
#endif
#if defined(__SPRT_IP_RECVIF) || defined(IP_RECVIF)
static_assert(__SPRT_IP_RECVIF == IP_RECVIF, "IP_RECVIF differs from native");
#endif
#if defined(__SPRT_IP_RECVDSTADDR) || defined(IP_RECVDSTADDR)
static_assert(__SPRT_IP_RECVDSTADDR == IP_RECVDSTADDR, "IP_RECVDSTADDR differs from native");
#endif
#if defined(__SPRT_IP_IFLIST) || defined(IP_IFLIST)
static_assert(__SPRT_IP_IFLIST == IP_IFLIST, "IP_IFLIST differs from native");
#endif
#if defined(__SPRT_IP_ADD_IFLIST) || defined(IP_ADD_IFLIST)
static_assert(__SPRT_IP_ADD_IFLIST == IP_ADD_IFLIST, "IP_ADD_IFLIST differs from native");
#endif
#if defined(__SPRT_IP_DEL_IFLIST) || defined(IP_DEL_IFLIST)
static_assert(__SPRT_IP_DEL_IFLIST == IP_DEL_IFLIST, "IP_DEL_IFLIST differs from native");
#endif
#if defined(__SPRT_IP_RTHDR) || defined(IP_RTHDR)
static_assert(__SPRT_IP_RTHDR == IP_RTHDR, "IP_RTHDR differs from native");
#endif
#if defined(__SPRT_IP_GET_IFLIST) || defined(IP_GET_IFLIST)
static_assert(__SPRT_IP_GET_IFLIST == IP_GET_IFLIST, "IP_GET_IFLIST differs from native");
#endif
#if defined(__SPRT_IP_RECVRTHDR) || defined(IP_RECVRTHDR)
static_assert(__SPRT_IP_RECVRTHDR == IP_RECVRTHDR, "IP_RECVRTHDR differs from native");
#endif
#if defined(__SPRT_IP_TCLASS) || defined(IP_TCLASS)
static_assert(__SPRT_IP_TCLASS == IP_TCLASS, "IP_TCLASS differs from native");
#endif
#if defined(__SPRT_IP_RECVTCLASS) || defined(IP_RECVTCLASS)
static_assert(__SPRT_IP_RECVTCLASS == IP_RECVTCLASS, "IP_RECVTCLASS differs from native");
#endif
#if defined(__SPRT_IP_ORIGINAL_ARRIVAL_IF) || defined(IP_ORIGINAL_ARRIVAL_IF)
static_assert(__SPRT_IP_ORIGINAL_ARRIVAL_IF == IP_ORIGINAL_ARRIVAL_IF,
		"IP_ORIGINAL_ARRIVAL_IF differs from native");
#endif
#if defined(__SPRT_IP_ECN) || defined(IP_ECN)
static_assert(__SPRT_IP_ECN == IP_ECN, "IP_ECN differs from native");
#endif
#if defined(__SPRT_IP_RECVECN) || defined(IP_RECVECN)
static_assert(__SPRT_IP_RECVECN == IP_RECVECN, "IP_RECVECN differs from native");
#endif
#if defined(__SPRT_IP_PKTINFO_EX) || defined(IP_PKTINFO_EX)
static_assert(__SPRT_IP_PKTINFO_EX == IP_PKTINFO_EX, "IP_PKTINFO_EX differs from native");
#endif
#if defined(__SPRT_IP_WFP_REDIRECT_RECORDS) || defined(IP_WFP_REDIRECT_RECORDS)
static_assert(__SPRT_IP_WFP_REDIRECT_RECORDS == IP_WFP_REDIRECT_RECORDS,
		"IP_WFP_REDIRECT_RECORDS differs from native");
#endif
#if defined(__SPRT_IP_WFP_REDIRECT_CONTEXT) || defined(IP_WFP_REDIRECT_CONTEXT)
static_assert(__SPRT_IP_WFP_REDIRECT_CONTEXT == IP_WFP_REDIRECT_CONTEXT,
		"IP_WFP_REDIRECT_CONTEXT differs from native");
#endif
#if defined(__SPRT_IP_NRT_INTERFACE) || defined(IP_NRT_INTERFACE)
static_assert(__SPRT_IP_NRT_INTERFACE == IP_NRT_INTERFACE, "IP_NRT_INTERFACE differs from native");
#endif
#if defined(__SPRT_IP_USER_MTU) || defined(IP_USER_MTU)
static_assert(__SPRT_IP_USER_MTU == IP_USER_MTU, "IP_USER_MTU differs from native");
#endif
#if defined(__SPRT_IP_PMTUDISC_DONT) || defined(IP_PMTUDISC_DONT)
static_assert(__SPRT_IP_PMTUDISC_DONT == IP_PMTUDISC_DONT, "IP_PMTUDISC_DONT differs from native");
#endif
#if defined(__SPRT_IP_PMTUDISC_WANT) || defined(IP_PMTUDISC_WANT)
static_assert(__SPRT_IP_PMTUDISC_WANT == IP_PMTUDISC_WANT, "IP_PMTUDISC_WANT differs from native");
#endif
#if defined(__SPRT_IP_PMTUDISC_DO) || defined(IP_PMTUDISC_DO)
static_assert(__SPRT_IP_PMTUDISC_DO == IP_PMTUDISC_DO, "IP_PMTUDISC_DO differs from native");
#endif
#if defined(__SPRT_IP_PMTUDISC_PROBE) || defined(IP_PMTUDISC_PROBE)
static_assert(__SPRT_IP_PMTUDISC_PROBE == IP_PMTUDISC_PROBE,
		"IP_PMTUDISC_PROBE differs from native");
#endif
#if defined(__SPRT_IP_PMTUDISC_INTERFACE) || defined(IP_PMTUDISC_INTERFACE)
static_assert(__SPRT_IP_PMTUDISC_INTERFACE == IP_PMTUDISC_INTERFACE,
		"IP_PMTUDISC_INTERFACE differs from native");
#endif
#if defined(__SPRT_IP_PMTUDISC_OMIT) || defined(IP_PMTUDISC_OMIT)
static_assert(__SPRT_IP_PMTUDISC_OMIT == IP_PMTUDISC_OMIT, "IP_PMTUDISC_OMIT differs from native");
#endif
#if defined(__SPRT_IP_DEFAULT_MULTICAST_TTL) || defined(IP_DEFAULT_MULTICAST_TTL)
static_assert(__SPRT_IP_DEFAULT_MULTICAST_TTL == IP_DEFAULT_MULTICAST_TTL,
		"IP_DEFAULT_MULTICAST_TTL differs from native");
#endif
#if defined(__SPRT_IP_DEFAULT_MULTICAST_LOOP) || defined(IP_DEFAULT_MULTICAST_LOOP)
static_assert(__SPRT_IP_DEFAULT_MULTICAST_LOOP == IP_DEFAULT_MULTICAST_LOOP,
		"IP_DEFAULT_MULTICAST_LOOP differs from native");
#endif
#if defined(__SPRT_IP_MAX_MEMBERSHIPS) || defined(IP_MAX_MEMBERSHIPS)
static_assert(__SPRT_IP_MAX_MEMBERSHIPS == IP_MAX_MEMBERSHIPS,
		"IP_MAX_MEMBERSHIPS differs from native");
#endif
// --- IPv6 options ---
#if defined(__SPRT_IPV6_ADDRFORM) || defined(IPV6_ADDRFORM)
static_assert(__SPRT_IPV6_ADDRFORM == IPV6_ADDRFORM, "IPV6_ADDRFORM differs from native");
#endif
#if defined(__SPRT_IPV6_2292PKTINFO) || defined(IPV6_2292PKTINFO)
static_assert(__SPRT_IPV6_2292PKTINFO == IPV6_2292PKTINFO, "IPV6_2292PKTINFO differs from native");
#endif
#if defined(__SPRT_IPV6_2292HOPOPTS) || defined(IPV6_2292HOPOPTS)
static_assert(__SPRT_IPV6_2292HOPOPTS == IPV6_2292HOPOPTS, "IPV6_2292HOPOPTS differs from native");
#endif
#if defined(__SPRT_IPV6_2292DSTOPTS) || defined(IPV6_2292DSTOPTS)
static_assert(__SPRT_IPV6_2292DSTOPTS == IPV6_2292DSTOPTS, "IPV6_2292DSTOPTS differs from native");
#endif
#if defined(__SPRT_IPV6_2292RTHDR) || defined(IPV6_2292RTHDR)
static_assert(__SPRT_IPV6_2292RTHDR == IPV6_2292RTHDR, "IPV6_2292RTHDR differs from native");
#endif
#if defined(__SPRT_IPV6_2292PKTOPTIONS) || defined(IPV6_2292PKTOPTIONS)
static_assert(__SPRT_IPV6_2292PKTOPTIONS == IPV6_2292PKTOPTIONS,
		"IPV6_2292PKTOPTIONS differs from native");
#endif
#if defined(__SPRT_IPV6_CHECKSUM) || defined(IPV6_CHECKSUM)
static_assert(__SPRT_IPV6_CHECKSUM == IPV6_CHECKSUM, "IPV6_CHECKSUM differs from native");
#endif
#if defined(__SPRT_IPV6_2292HOPLIMIT) || defined(IPV6_2292HOPLIMIT)
static_assert(__SPRT_IPV6_2292HOPLIMIT == IPV6_2292HOPLIMIT,
		"IPV6_2292HOPLIMIT differs from native");
#endif
#if defined(__SPRT_IPV6_NEXTHOP) || defined(IPV6_NEXTHOP)
static_assert(__SPRT_IPV6_NEXTHOP == IPV6_NEXTHOP, "IPV6_NEXTHOP differs from native");
#endif
#if defined(__SPRT_IPV6_AUTHHDR) || defined(IPV6_AUTHHDR)
static_assert(__SPRT_IPV6_AUTHHDR == IPV6_AUTHHDR, "IPV6_AUTHHDR differs from native");
#endif
#if defined(__SPRT_IPV6_UNICAST_HOPS) || defined(IPV6_UNICAST_HOPS)
static_assert(__SPRT_IPV6_UNICAST_HOPS == IPV6_UNICAST_HOPS,
		"IPV6_UNICAST_HOPS differs from native");
#endif
#if defined(__SPRT_IPV6_MULTICAST_IF) || defined(IPV6_MULTICAST_IF)
static_assert(__SPRT_IPV6_MULTICAST_IF == IPV6_MULTICAST_IF,
		"IPV6_MULTICAST_IF differs from native");
#endif
#if defined(__SPRT_IPV6_MULTICAST_HOPS) || defined(IPV6_MULTICAST_HOPS)
static_assert(__SPRT_IPV6_MULTICAST_HOPS == IPV6_MULTICAST_HOPS,
		"IPV6_MULTICAST_HOPS differs from native");
#endif
#if defined(__SPRT_IPV6_MULTICAST_LOOP) || defined(IPV6_MULTICAST_LOOP)
static_assert(__SPRT_IPV6_MULTICAST_LOOP == IPV6_MULTICAST_LOOP,
		"IPV6_MULTICAST_LOOP differs from native");
#endif
#if defined(__SPRT_IPV6_JOIN_GROUP) || defined(IPV6_JOIN_GROUP)
static_assert(__SPRT_IPV6_JOIN_GROUP == IPV6_JOIN_GROUP, "IPV6_JOIN_GROUP differs from native");
#endif
#if defined(__SPRT_IPV6_LEAVE_GROUP) || defined(IPV6_LEAVE_GROUP)
static_assert(__SPRT_IPV6_LEAVE_GROUP == IPV6_LEAVE_GROUP, "IPV6_LEAVE_GROUP differs from native");
#endif
#if defined(__SPRT_IPV6_ROUTER_ALERT) || defined(IPV6_ROUTER_ALERT)
static_assert(__SPRT_IPV6_ROUTER_ALERT == IPV6_ROUTER_ALERT,
		"IPV6_ROUTER_ALERT differs from native");
#endif
#if defined(__SPRT_IPV6_MTU_DISCOVER) || defined(IPV6_MTU_DISCOVER)
static_assert(__SPRT_IPV6_MTU_DISCOVER == IPV6_MTU_DISCOVER,
		"IPV6_MTU_DISCOVER differs from native");
#endif
#if defined(__SPRT_IPV6_MTU) || defined(IPV6_MTU)
static_assert(__SPRT_IPV6_MTU == IPV6_MTU, "IPV6_MTU differs from native");
#endif
#if defined(__SPRT_IPV6_RECVERR) || defined(IPV6_RECVERR)
static_assert(__SPRT_IPV6_RECVERR == IPV6_RECVERR, "IPV6_RECVERR differs from native");
#endif
#if defined(__SPRT_IPV6_V6ONLY) || defined(IPV6_V6ONLY)
static_assert(__SPRT_IPV6_V6ONLY == IPV6_V6ONLY, "IPV6_V6ONLY differs from native");
#endif
#if defined(__SPRT_IPV6_JOIN_ANYCAST) || defined(IPV6_JOIN_ANYCAST)
static_assert(__SPRT_IPV6_JOIN_ANYCAST == IPV6_JOIN_ANYCAST,
		"IPV6_JOIN_ANYCAST differs from native");
#endif
#if defined(__SPRT_IPV6_LEAVE_ANYCAST) || defined(IPV6_LEAVE_ANYCAST)
static_assert(__SPRT_IPV6_LEAVE_ANYCAST == IPV6_LEAVE_ANYCAST,
		"IPV6_LEAVE_ANYCAST differs from native");
#endif
#if defined(__SPRT_IPV6_MULTICAST_ALL) || defined(IPV6_MULTICAST_ALL)
static_assert(__SPRT_IPV6_MULTICAST_ALL == IPV6_MULTICAST_ALL,
		"IPV6_MULTICAST_ALL differs from native");
#endif
#if defined(__SPRT_IPV6_ROUTER_ALERT_ISOLATE) || defined(IPV6_ROUTER_ALERT_ISOLATE)
static_assert(__SPRT_IPV6_ROUTER_ALERT_ISOLATE == IPV6_ROUTER_ALERT_ISOLATE,
		"IPV6_ROUTER_ALERT_ISOLATE differs from native");
#endif
#if defined(__SPRT_IPV6_IPSEC_POLICY) || defined(IPV6_IPSEC_POLICY)
static_assert(__SPRT_IPV6_IPSEC_POLICY == IPV6_IPSEC_POLICY,
		"IPV6_IPSEC_POLICY differs from native");
#endif
#if defined(__SPRT_IPV6_XFRM_POLICY) || defined(IPV6_XFRM_POLICY)
static_assert(__SPRT_IPV6_XFRM_POLICY == IPV6_XFRM_POLICY, "IPV6_XFRM_POLICY differs from native");
#endif
#if defined(__SPRT_IPV6_HDRINCL) || defined(IPV6_HDRINCL)
static_assert(__SPRT_IPV6_HDRINCL == IPV6_HDRINCL, "IPV6_HDRINCL differs from native");
#endif
#if defined(__SPRT_IPV6_RECVPKTINFO) || defined(IPV6_RECVPKTINFO)
static_assert(__SPRT_IPV6_RECVPKTINFO == IPV6_RECVPKTINFO, "IPV6_RECVPKTINFO differs from native");
#endif
#if defined(__SPRT_IPV6_PKTINFO) || defined(IPV6_PKTINFO)
static_assert(__SPRT_IPV6_PKTINFO == IPV6_PKTINFO, "IPV6_PKTINFO differs from native");
#endif
#if defined(__SPRT_IPV6_RECVHOPLIMIT) || defined(IPV6_RECVHOPLIMIT)
static_assert(__SPRT_IPV6_RECVHOPLIMIT == IPV6_RECVHOPLIMIT,
		"IPV6_RECVHOPLIMIT differs from native");
#endif
#if defined(__SPRT_IPV6_HOPLIMIT) || defined(IPV6_HOPLIMIT)
static_assert(__SPRT_IPV6_HOPLIMIT == IPV6_HOPLIMIT, "IPV6_HOPLIMIT differs from native");
#endif
#if defined(__SPRT_IPV6_RECVHOPOPTS) || defined(IPV6_RECVHOPOPTS)
static_assert(__SPRT_IPV6_RECVHOPOPTS == IPV6_RECVHOPOPTS, "IPV6_RECVHOPOPTS differs from native");
#endif
#if defined(__SPRT_IPV6_HOPOPTS) || defined(IPV6_HOPOPTS)
static_assert(__SPRT_IPV6_HOPOPTS == IPV6_HOPOPTS, "IPV6_HOPOPTS differs from native");
#endif
#if defined(__SPRT_IPV6_RTHDRDSTOPTS) || defined(IPV6_RTHDRDSTOPTS)
static_assert(__SPRT_IPV6_RTHDRDSTOPTS == IPV6_RTHDRDSTOPTS,
		"IPV6_RTHDRDSTOPTS differs from native");
#endif
#if defined(__SPRT_IPV6_RECVRTHDR) || defined(IPV6_RECVRTHDR)
static_assert(__SPRT_IPV6_RECVRTHDR == IPV6_RECVRTHDR, "IPV6_RECVRTHDR differs from native");
#endif
#if defined(__SPRT_IPV6_RTHDR) || defined(IPV6_RTHDR)
static_assert(__SPRT_IPV6_RTHDR == IPV6_RTHDR, "IPV6_RTHDR differs from native");
#endif
#if defined(__SPRT_IPV6_RECVDSTOPTS) || defined(IPV6_RECVDSTOPTS)
static_assert(__SPRT_IPV6_RECVDSTOPTS == IPV6_RECVDSTOPTS, "IPV6_RECVDSTOPTS differs from native");
#endif
#if defined(__SPRT_IPV6_DSTOPTS) || defined(IPV6_DSTOPTS)
static_assert(__SPRT_IPV6_DSTOPTS == IPV6_DSTOPTS, "IPV6_DSTOPTS differs from native");
#endif
#if defined(__SPRT_IPV6_RECVPATHMTU) || defined(IPV6_RECVPATHMTU)
static_assert(__SPRT_IPV6_RECVPATHMTU == IPV6_RECVPATHMTU, "IPV6_RECVPATHMTU differs from native");
#endif
#if defined(__SPRT_IPV6_PATHMTU) || defined(IPV6_PATHMTU)
static_assert(__SPRT_IPV6_PATHMTU == IPV6_PATHMTU, "IPV6_PATHMTU differs from native");
#endif
#if defined(__SPRT_IPV6_DONTFRAG) || defined(IPV6_DONTFRAG)
static_assert(__SPRT_IPV6_DONTFRAG == IPV6_DONTFRAG, "IPV6_DONTFRAG differs from native");
#endif
#if defined(__SPRT_IPV6_RECVTCLASS) || defined(IPV6_RECVTCLASS)
static_assert(__SPRT_IPV6_RECVTCLASS == IPV6_RECVTCLASS, "IPV6_RECVTCLASS differs from native");
#endif
#if defined(__SPRT_IPV6_TCLASS) || defined(IPV6_TCLASS)
static_assert(__SPRT_IPV6_TCLASS == IPV6_TCLASS, "IPV6_TCLASS differs from native");
#endif
#if defined(__SPRT_IPV6_AUTOFLOWLABEL) || defined(IPV6_AUTOFLOWLABEL)
static_assert(__SPRT_IPV6_AUTOFLOWLABEL == IPV6_AUTOFLOWLABEL,
		"IPV6_AUTOFLOWLABEL differs from native");
#endif
#if defined(__SPRT_IPV6_ADDR_PREFERENCES) || defined(IPV6_ADDR_PREFERENCES)
static_assert(__SPRT_IPV6_ADDR_PREFERENCES == IPV6_ADDR_PREFERENCES,
		"IPV6_ADDR_PREFERENCES differs from native");
#endif
#if defined(__SPRT_IPV6_MINHOPCOUNT) || defined(IPV6_MINHOPCOUNT)
static_assert(__SPRT_IPV6_MINHOPCOUNT == IPV6_MINHOPCOUNT, "IPV6_MINHOPCOUNT differs from native");
#endif
#if defined(__SPRT_IPV6_ORIGDSTADDR) || defined(IPV6_ORIGDSTADDR)
static_assert(__SPRT_IPV6_ORIGDSTADDR == IPV6_ORIGDSTADDR, "IPV6_ORIGDSTADDR differs from native");
#endif
#if defined(__SPRT_IPV6_RECVORIGDSTADDR) || defined(IPV6_RECVORIGDSTADDR)
static_assert(__SPRT_IPV6_RECVORIGDSTADDR == IPV6_RECVORIGDSTADDR,
		"IPV6_RECVORIGDSTADDR differs from native");
#endif
#if defined(__SPRT_IPV6_TRANSPARENT) || defined(IPV6_TRANSPARENT)
static_assert(__SPRT_IPV6_TRANSPARENT == IPV6_TRANSPARENT, "IPV6_TRANSPARENT differs from native");
#endif
#if defined(__SPRT_IPV6_UNICAST_IF) || defined(IPV6_UNICAST_IF)
static_assert(__SPRT_IPV6_UNICAST_IF == IPV6_UNICAST_IF, "IPV6_UNICAST_IF differs from native");
#endif
#if defined(__SPRT_IPV6_RECVFRAGSIZE) || defined(IPV6_RECVFRAGSIZE)
static_assert(__SPRT_IPV6_RECVFRAGSIZE == IPV6_RECVFRAGSIZE,
		"IPV6_RECVFRAGSIZE differs from native");
#endif
#if defined(__SPRT_IPV6_FREEBIND) || defined(IPV6_FREEBIND)
static_assert(__SPRT_IPV6_FREEBIND == IPV6_FREEBIND, "IPV6_FREEBIND differs from native");
#endif
#if defined(__SPRT_IPV6_PROTECTION_LEVEL) || defined(IPV6_PROTECTION_LEVEL)
static_assert(__SPRT_IPV6_PROTECTION_LEVEL == IPV6_PROTECTION_LEVEL,
		"IPV6_PROTECTION_LEVEL differs from native");
#endif
#if defined(__SPRT_IPV6_RECVIF) || defined(IPV6_RECVIF)
static_assert(__SPRT_IPV6_RECVIF == IPV6_RECVIF, "IPV6_RECVIF differs from native");
#endif
#if defined(__SPRT_IPV6_RECVDSTADDR) || defined(IPV6_RECVDSTADDR)
static_assert(__SPRT_IPV6_RECVDSTADDR == IPV6_RECVDSTADDR, "IPV6_RECVDSTADDR differs from native");
#endif
#if defined(__SPRT_IPV6_IFLIST) || defined(IPV6_IFLIST)
static_assert(__SPRT_IPV6_IFLIST == IPV6_IFLIST, "IPV6_IFLIST differs from native");
#endif
#if defined(__SPRT_IPV6_ADD_IFLIST) || defined(IPV6_ADD_IFLIST)
static_assert(__SPRT_IPV6_ADD_IFLIST == IPV6_ADD_IFLIST, "IPV6_ADD_IFLIST differs from native");
#endif
#if defined(__SPRT_IPV6_DEL_IFLIST) || defined(IPV6_DEL_IFLIST)
static_assert(__SPRT_IPV6_DEL_IFLIST == IPV6_DEL_IFLIST, "IPV6_DEL_IFLIST differs from native");
#endif
#if defined(__SPRT_IPV6_GET_IFLIST) || defined(IPV6_GET_IFLIST)
static_assert(__SPRT_IPV6_GET_IFLIST == IPV6_GET_IFLIST, "IPV6_GET_IFLIST differs from native");
#endif
#if defined(__SPRT_IPV6_ECN) || defined(IPV6_ECN)
static_assert(__SPRT_IPV6_ECN == IPV6_ECN, "IPV6_ECN differs from native");
#endif
#if defined(__SPRT_IPV6_RECVECN) || defined(IPV6_RECVECN)
static_assert(__SPRT_IPV6_RECVECN == IPV6_RECVECN, "IPV6_RECVECN differs from native");
#endif
#if defined(__SPRT_IPV6_PKTINFO_EX) || defined(IPV6_PKTINFO_EX)
static_assert(__SPRT_IPV6_PKTINFO_EX == IPV6_PKTINFO_EX, "IPV6_PKTINFO_EX differs from native");
#endif
#if defined(__SPRT_IPV6_WFP_REDIRECT_RECORDS) || defined(IPV6_WFP_REDIRECT_RECORDS)
static_assert(__SPRT_IPV6_WFP_REDIRECT_RECORDS == IPV6_WFP_REDIRECT_RECORDS,
		"IPV6_WFP_REDIRECT_RECORDS differs from native");
#endif
#if defined(__SPRT_IPV6_WFP_REDIRECT_CONTEXT) || defined(IPV6_WFP_REDIRECT_CONTEXT)
static_assert(__SPRT_IPV6_WFP_REDIRECT_CONTEXT == IPV6_WFP_REDIRECT_CONTEXT,
		"IPV6_WFP_REDIRECT_CONTEXT differs from native");
#endif
#if defined(__SPRT_IPV6_NRT_INTERFACE) || defined(IPV6_NRT_INTERFACE)
static_assert(__SPRT_IPV6_NRT_INTERFACE == IPV6_NRT_INTERFACE,
		"IPV6_NRT_INTERFACE differs from native");
#endif
#if defined(__SPRT_IPV6_USER_MTU) || defined(IPV6_USER_MTU)
static_assert(__SPRT_IPV6_USER_MTU == IPV6_USER_MTU, "IPV6_USER_MTU differs from native");
#endif
#if defined(__SPRT_IPV6_ADD_MEMBERSHIP) || defined(IPV6_ADD_MEMBERSHIP)
static_assert(__SPRT_IPV6_ADD_MEMBERSHIP == IPV6_ADD_MEMBERSHIP,
		"IPV6_ADD_MEMBERSHIP differs from native");
#endif
#if defined(__SPRT_IPV6_DROP_MEMBERSHIP) || defined(IPV6_DROP_MEMBERSHIP)
static_assert(__SPRT_IPV6_DROP_MEMBERSHIP == IPV6_DROP_MEMBERSHIP,
		"IPV6_DROP_MEMBERSHIP differs from native");
#endif
#if defined(__SPRT_IPV6_RXHOPOPTS) || defined(IPV6_RXHOPOPTS)
static_assert(__SPRT_IPV6_RXHOPOPTS == IPV6_RXHOPOPTS, "IPV6_RXHOPOPTS differs from native");
#endif
#if defined(__SPRT_IPV6_RXDSTOPTS) || defined(IPV6_RXDSTOPTS)
static_assert(__SPRT_IPV6_RXDSTOPTS == IPV6_RXDSTOPTS, "IPV6_RXDSTOPTS differs from native");
#endif
#if defined(__SPRT_IPV6_PMTUDISC_DONT) || defined(IPV6_PMTUDISC_DONT)
static_assert(__SPRT_IPV6_PMTUDISC_DONT == IPV6_PMTUDISC_DONT,
		"IPV6_PMTUDISC_DONT differs from native");
#endif
#if defined(__SPRT_IPV6_PMTUDISC_WANT) || defined(IPV6_PMTUDISC_WANT)
static_assert(__SPRT_IPV6_PMTUDISC_WANT == IPV6_PMTUDISC_WANT,
		"IPV6_PMTUDISC_WANT differs from native");
#endif
#if defined(__SPRT_IPV6_PMTUDISC_DO) || defined(IPV6_PMTUDISC_DO)
static_assert(__SPRT_IPV6_PMTUDISC_DO == IPV6_PMTUDISC_DO, "IPV6_PMTUDISC_DO differs from native");
#endif
#if defined(__SPRT_IPV6_PMTUDISC_PROBE) || defined(IPV6_PMTUDISC_PROBE)
static_assert(__SPRT_IPV6_PMTUDISC_PROBE == IPV6_PMTUDISC_PROBE,
		"IPV6_PMTUDISC_PROBE differs from native");
#endif
#if defined(__SPRT_IPV6_PMTUDISC_INTERFACE) || defined(IPV6_PMTUDISC_INTERFACE)
static_assert(__SPRT_IPV6_PMTUDISC_INTERFACE == IPV6_PMTUDISC_INTERFACE,
		"IPV6_PMTUDISC_INTERFACE differs from native");
#endif
#if defined(__SPRT_IPV6_PMTUDISC_OMIT) || defined(IPV6_PMTUDISC_OMIT)
static_assert(__SPRT_IPV6_PMTUDISC_OMIT == IPV6_PMTUDISC_OMIT,
		"IPV6_PMTUDISC_OMIT differs from native");
#endif
#if defined(__SPRT_IPV6_PREFER_SRC_TMP) || defined(IPV6_PREFER_SRC_TMP)
static_assert(__SPRT_IPV6_PREFER_SRC_TMP == IPV6_PREFER_SRC_TMP,
		"IPV6_PREFER_SRC_TMP differs from native");
#endif
#if defined(__SPRT_IPV6_PREFER_SRC_PUBLIC) || defined(IPV6_PREFER_SRC_PUBLIC)
static_assert(__SPRT_IPV6_PREFER_SRC_PUBLIC == IPV6_PREFER_SRC_PUBLIC,
		"IPV6_PREFER_SRC_PUBLIC differs from native");
#endif
#if defined(__SPRT_IPV6_PREFER_SRC_PUBTMP_DEFAULT) || defined(IPV6_PREFER_SRC_PUBTMP_DEFAULT)
static_assert(__SPRT_IPV6_PREFER_SRC_PUBTMP_DEFAULT == IPV6_PREFER_SRC_PUBTMP_DEFAULT,
		"IPV6_PREFER_SRC_PUBTMP_DEFAULT differs from native");
#endif
#if defined(__SPRT_IPV6_PREFER_SRC_COA) || defined(IPV6_PREFER_SRC_COA)
static_assert(__SPRT_IPV6_PREFER_SRC_COA == IPV6_PREFER_SRC_COA,
		"IPV6_PREFER_SRC_COA differs from native");
#endif
#if defined(__SPRT_IPV6_PREFER_SRC_HOME) || defined(IPV6_PREFER_SRC_HOME)
static_assert(__SPRT_IPV6_PREFER_SRC_HOME == IPV6_PREFER_SRC_HOME,
		"IPV6_PREFER_SRC_HOME differs from native");
#endif
#if defined(__SPRT_IPV6_PREFER_SRC_CGA) || defined(IPV6_PREFER_SRC_CGA)
static_assert(__SPRT_IPV6_PREFER_SRC_CGA == IPV6_PREFER_SRC_CGA,
		"IPV6_PREFER_SRC_CGA differs from native");
#endif
#if defined(__SPRT_IPV6_PREFER_SRC_NONCGA) || defined(IPV6_PREFER_SRC_NONCGA)
static_assert(__SPRT_IPV6_PREFER_SRC_NONCGA == IPV6_PREFER_SRC_NONCGA,
		"IPV6_PREFER_SRC_NONCGA differs from native");
#endif
#if defined(__SPRT_IPV6_RTHDR_LOOSE) || defined(IPV6_RTHDR_LOOSE)
static_assert(__SPRT_IPV6_RTHDR_LOOSE == IPV6_RTHDR_LOOSE, "IPV6_RTHDR_LOOSE differs from native");
#endif
#if defined(__SPRT_IPV6_RTHDR_STRICT) || defined(IPV6_RTHDR_STRICT)
static_assert(__SPRT_IPV6_RTHDR_STRICT == IPV6_RTHDR_STRICT,
		"IPV6_RTHDR_STRICT differs from native");
#endif
#if defined(__SPRT_IPV6_RTHDR_TYPE_0) || defined(IPV6_RTHDR_TYPE_0)
static_assert(__SPRT_IPV6_RTHDR_TYPE_0 == IPV6_RTHDR_TYPE_0,
		"IPV6_RTHDR_TYPE_0 differs from native");
#endif
// --- multicast source-filter ---
#if defined(__SPRT_MCAST_JOIN_GROUP) || defined(MCAST_JOIN_GROUP)
static_assert(__SPRT_MCAST_JOIN_GROUP == MCAST_JOIN_GROUP, "MCAST_JOIN_GROUP differs from native");
#endif
#if defined(__SPRT_MCAST_LEAVE_GROUP) || defined(MCAST_LEAVE_GROUP)
static_assert(__SPRT_MCAST_LEAVE_GROUP == MCAST_LEAVE_GROUP,
		"MCAST_LEAVE_GROUP differs from native");
#endif
#if defined(__SPRT_MCAST_JOIN_SOURCE_GROUP) || defined(MCAST_JOIN_SOURCE_GROUP)
static_assert(__SPRT_MCAST_JOIN_SOURCE_GROUP == MCAST_JOIN_SOURCE_GROUP,
		"MCAST_JOIN_SOURCE_GROUP differs from native");
#endif
#if defined(__SPRT_MCAST_LEAVE_SOURCE_GROUP) || defined(MCAST_LEAVE_SOURCE_GROUP)
static_assert(__SPRT_MCAST_LEAVE_SOURCE_GROUP == MCAST_LEAVE_SOURCE_GROUP,
		"MCAST_LEAVE_SOURCE_GROUP differs from native");
#endif
#if defined(__SPRT_MCAST_BLOCK_SOURCE) || defined(MCAST_BLOCK_SOURCE)
static_assert(__SPRT_MCAST_BLOCK_SOURCE == MCAST_BLOCK_SOURCE,
		"MCAST_BLOCK_SOURCE differs from native");
#endif
#if defined(__SPRT_MCAST_UNBLOCK_SOURCE) || defined(MCAST_UNBLOCK_SOURCE)
static_assert(__SPRT_MCAST_UNBLOCK_SOURCE == MCAST_UNBLOCK_SOURCE,
		"MCAST_UNBLOCK_SOURCE differs from native");
#endif
#if defined(__SPRT_MCAST_MSFILTER) || defined(MCAST_MSFILTER)
static_assert(__SPRT_MCAST_MSFILTER == MCAST_MSFILTER, "MCAST_MSFILTER differs from native");
#endif
#if defined(__SPRT_MCAST_EXCLUDE) || defined(MCAST_EXCLUDE)
static_assert(__SPRT_MCAST_EXCLUDE == MCAST_EXCLUDE, "MCAST_EXCLUDE differs from native");
#endif
#if defined(__SPRT_MCAST_INCLUDE) || defined(MCAST_INCLUDE)
static_assert(__SPRT_MCAST_INCLUDE == MCAST_INCLUDE, "MCAST_INCLUDE differs from native");
#endif

// TCP options (<netinet/tcp.h>) vs native. Same 1-1 ||-guard as the netinet block.
// --- TCP options (IPPROTO_TCP level) ---
#if defined(__SPRT_TCP_AO_ADD_KEY) || defined(TCP_AO_ADD_KEY)
static_assert(__SPRT_TCP_AO_ADD_KEY == TCP_AO_ADD_KEY, "TCP_AO_ADD_KEY differs from native");
#endif
#if defined(__SPRT_TCP_AO_DEL_KEY) || defined(TCP_AO_DEL_KEY)
static_assert(__SPRT_TCP_AO_DEL_KEY == TCP_AO_DEL_KEY, "TCP_AO_DEL_KEY differs from native");
#endif
#if defined(__SPRT_TCP_AO_GET_KEYS) || defined(TCP_AO_GET_KEYS)
static_assert(__SPRT_TCP_AO_GET_KEYS == TCP_AO_GET_KEYS, "TCP_AO_GET_KEYS differs from native");
#endif
#if defined(__SPRT_TCP_AO_INFO) || defined(TCP_AO_INFO)
static_assert(__SPRT_TCP_AO_INFO == TCP_AO_INFO, "TCP_AO_INFO differs from native");
#endif
#if defined(__SPRT_TCP_AO_KEYF_EXCLUDE_OPT) || defined(TCP_AO_KEYF_EXCLUDE_OPT)
static_assert(__SPRT_TCP_AO_KEYF_EXCLUDE_OPT == TCP_AO_KEYF_EXCLUDE_OPT,
		"TCP_AO_KEYF_EXCLUDE_OPT differs from native");
#endif
#if defined(__SPRT_TCP_AO_KEYF_IFINDEX) || defined(TCP_AO_KEYF_IFINDEX)
static_assert(__SPRT_TCP_AO_KEYF_IFINDEX == TCP_AO_KEYF_IFINDEX,
		"TCP_AO_KEYF_IFINDEX differs from native");
#endif
#if defined(__SPRT_TCP_AO_MAXKEYLEN) || defined(TCP_AO_MAXKEYLEN)
static_assert(__SPRT_TCP_AO_MAXKEYLEN == TCP_AO_MAXKEYLEN, "TCP_AO_MAXKEYLEN differs from native");
#endif
#if defined(__SPRT_TCP_AO_REPAIR) || defined(TCP_AO_REPAIR)
static_assert(__SPRT_TCP_AO_REPAIR == TCP_AO_REPAIR, "TCP_AO_REPAIR differs from native");
#endif
#if defined(__SPRT_TCP_ATMARK) || defined(TCP_ATMARK)
static_assert(__SPRT_TCP_ATMARK == TCP_ATMARK, "TCP_ATMARK differs from native");
#endif
#if defined(__SPRT_TCP_CA_D) || defined(TCP_CA_D)
static_assert(__SPRT_TCP_CA_D == TCP_CA_D, "TCP_CA_D differs from native");
#endif
#if defined(__SPRT_TCP_CA_L) || defined(TCP_CA_L)
static_assert(__SPRT_TCP_CA_L == TCP_CA_L, "TCP_CA_L differs from native");
#endif
#if defined(__SPRT_TCP_CA_O) || defined(TCP_CA_O)
static_assert(__SPRT_TCP_CA_O == TCP_CA_O, "TCP_CA_O differs from native");
#endif
#if defined(__SPRT_TCP_CA_R) || defined(TCP_CA_R)
static_assert(__SPRT_TCP_CA_R == TCP_CA_R, "TCP_CA_R differs from native");
#endif
#if defined(__SPRT_TCP_CC_INFO) || defined(TCP_CC_INFO)
static_assert(__SPRT_TCP_CC_INFO == TCP_CC_INFO, "TCP_CC_INFO differs from native");
#endif
#if defined(__SPRT_TCP_CLIENT_SND_WND) || defined(TCP_CLIENT_SND_WND)
static_assert(__SPRT_TCP_CLIENT_SND_WND == TCP_CLIENT_SND_WND,
		"TCP_CLIENT_SND_WND differs from native");
#endif
#if defined(__SPRT_TCP_CM_INQ) || defined(TCP_CM_INQ)
static_assert(__SPRT_TCP_CM_INQ == TCP_CM_INQ, "TCP_CM_INQ differs from native");
#endif
#if defined(__SPRT_TCP_CONGESTION) || defined(TCP_CONGESTION)
static_assert(__SPRT_TCP_CONGESTION == TCP_CONGESTION, "TCP_CONGESTION differs from native");
#endif
#if defined(__SPRT_TCP_CONGESTION_ALGORITHM) || defined(TCP_CONGESTION_ALGORITHM)
static_assert(__SPRT_TCP_CONGESTION_ALGORITHM == TCP_CONGESTION_ALGORITHM,
		"TCP_CONGESTION_ALGORITHM differs from native");
#endif
#if defined(__SPRT_TCP_CONNECTION_INFO) || defined(TCP_CONNECTION_INFO)
static_assert(__SPRT_TCP_CONNECTION_INFO == TCP_CONNECTION_INFO,
		"TCP_CONNECTION_INFO differs from native");
#endif
#if defined(__SPRT_TCP_CONNECTIONTIMEOUT) || defined(TCP_CONNECTIONTIMEOUT)
static_assert(__SPRT_TCP_CONNECTIONTIMEOUT == TCP_CONNECTIONTIMEOUT,
		"TCP_CONNECTIONTIMEOUT differs from native");
#endif
#if defined(__SPRT_TCP_COOKIE_IN_ALWAYS) || defined(TCP_COOKIE_IN_ALWAYS)
static_assert(__SPRT_TCP_COOKIE_IN_ALWAYS == TCP_COOKIE_IN_ALWAYS,
		"TCP_COOKIE_IN_ALWAYS differs from native");
#endif
#if defined(__SPRT_TCP_COOKIE_MAX) || defined(TCP_COOKIE_MAX)
static_assert(__SPRT_TCP_COOKIE_MAX == TCP_COOKIE_MAX, "TCP_COOKIE_MAX differs from native");
#endif
#if defined(__SPRT_TCP_COOKIE_MIN) || defined(TCP_COOKIE_MIN)
static_assert(__SPRT_TCP_COOKIE_MIN == TCP_COOKIE_MIN, "TCP_COOKIE_MIN differs from native");
#endif
#if defined(__SPRT_TCP_COOKIE_OUT_NEVER) || defined(TCP_COOKIE_OUT_NEVER)
static_assert(__SPRT_TCP_COOKIE_OUT_NEVER == TCP_COOKIE_OUT_NEVER,
		"TCP_COOKIE_OUT_NEVER differs from native");
#endif
#if defined(__SPRT_TCP_COOKIE_PAIR_SIZE) || defined(TCP_COOKIE_PAIR_SIZE)
static_assert(__SPRT_TCP_COOKIE_PAIR_SIZE == TCP_COOKIE_PAIR_SIZE,
		"TCP_COOKIE_PAIR_SIZE differs from native");
#endif
#if defined(__SPRT_TCP_COOKIE_TRANSACTIONS) || defined(TCP_COOKIE_TRANSACTIONS)
static_assert(__SPRT_TCP_COOKIE_TRANSACTIONS == TCP_COOKIE_TRANSACTIONS,
		"TCP_COOKIE_TRANSACTIONS differs from native");
#endif
#if defined(__SPRT_TCP_CORK) || defined(TCP_CORK)
static_assert(__SPRT_TCP_CORK == TCP_CORK, "TCP_CORK differs from native");
#endif
#if defined(__SPRT_TCP_DEFER_ACCEPT) || defined(TCP_DEFER_ACCEPT)
static_assert(__SPRT_TCP_DEFER_ACCEPT == TCP_DEFER_ACCEPT, "TCP_DEFER_ACCEPT differs from native");
#endif
#if defined(__SPRT_TCP_DELAY_FIN_ACK) || defined(TCP_DELAY_FIN_ACK)
static_assert(__SPRT_TCP_DELAY_FIN_ACK == TCP_DELAY_FIN_ACK,
		"TCP_DELAY_FIN_ACK differs from native");
#endif
#if defined(__SPRT_TCP_ENABLE_ECN) || defined(TCP_ENABLE_ECN)
static_assert(__SPRT_TCP_ENABLE_ECN == TCP_ENABLE_ECN, "TCP_ENABLE_ECN differs from native");
#endif
#if defined(__SPRT_TCP_EXPEDITED_1122) || defined(TCP_EXPEDITED_1122)
static_assert(__SPRT_TCP_EXPEDITED_1122 == TCP_EXPEDITED_1122,
		"TCP_EXPEDITED_1122 differs from native");
#endif
#if defined(__SPRT_TCP_FAIL_CONNECT_ON_ICMP_ERROR) || defined(TCP_FAIL_CONNECT_ON_ICMP_ERROR)
static_assert(__SPRT_TCP_FAIL_CONNECT_ON_ICMP_ERROR == TCP_FAIL_CONNECT_ON_ICMP_ERROR,
		"TCP_FAIL_CONNECT_ON_ICMP_ERROR differs from native");
#endif
#if defined(__SPRT_TCP_FASTOPEN) || defined(TCP_FASTOPEN)
static_assert(__SPRT_TCP_FASTOPEN == TCP_FASTOPEN, "TCP_FASTOPEN differs from native");
#endif
#if defined(__SPRT_TCP_FASTOPEN_CONNECT) || defined(TCP_FASTOPEN_CONNECT)
static_assert(__SPRT_TCP_FASTOPEN_CONNECT == TCP_FASTOPEN_CONNECT,
		"TCP_FASTOPEN_CONNECT differs from native");
#endif
#if defined(__SPRT_TCP_FASTOPEN_KEY) || defined(TCP_FASTOPEN_KEY)
static_assert(__SPRT_TCP_FASTOPEN_KEY == TCP_FASTOPEN_KEY, "TCP_FASTOPEN_KEY differs from native");
#endif
#if defined(__SPRT_TCP_FASTOPEN_NO_COOKIE) || defined(TCP_FASTOPEN_NO_COOKIE)
static_assert(__SPRT_TCP_FASTOPEN_NO_COOKIE == TCP_FASTOPEN_NO_COOKIE,
		"TCP_FASTOPEN_NO_COOKIE differs from native");
#endif
#if defined(__SPRT_TCP_H) || defined(TCP_H)
static_assert(__SPRT_TCP_H == TCP_H, "TCP_H differs from native");
#endif
#if defined(__SPRT_TCP_H_) || defined(TCP_H_)
static_assert(__SPRT_TCP_H_ == TCP_H_, "TCP_H_ differs from native");
#endif
#if defined(__SPRT_TCP_ICMP_ERROR_INFO) || defined(TCP_ICMP_ERROR_INFO)
static_assert(__SPRT_TCP_ICMP_ERROR_INFO == TCP_ICMP_ERROR_INFO,
		"TCP_ICMP_ERROR_INFO differs from native");
#endif
#if defined(__SPRT_TCP_INFO) || defined(TCP_INFO)
static_assert(__SPRT_TCP_INFO == TCP_INFO, "TCP_INFO differs from native");
#endif
#if defined(__SPRT_TCP_INITIAL_RTO) || defined(TCP_INITIAL_RTO)
static_assert(__SPRT_TCP_INITIAL_RTO == TCP_INITIAL_RTO, "TCP_INITIAL_RTO differs from native");
#endif
#if defined(__SPRT_TCP_INITIAL_RTO_DEFAULT_MAX_SYN_RETRANSMISSIONS) \
		|| defined(TCP_INITIAL_RTO_DEFAULT_MAX_SYN_RETRANSMISSIONS)
static_assert(__SPRT_TCP_INITIAL_RTO_DEFAULT_MAX_SYN_RETRANSMISSIONS
				== TCP_INITIAL_RTO_DEFAULT_MAX_SYN_RETRANSMISSIONS,
		"TCP_INITIAL_RTO_DEFAULT_MAX_SYN_RETRANSMISSIONS differs from native");
#endif
#if defined(__SPRT_TCP_INITIAL_RTO_DEFAULT_RTT) || defined(TCP_INITIAL_RTO_DEFAULT_RTT)
static_assert(__SPRT_TCP_INITIAL_RTO_DEFAULT_RTT == TCP_INITIAL_RTO_DEFAULT_RTT,
		"TCP_INITIAL_RTO_DEFAULT_RTT differs from native");
#endif
#if defined(__SPRT_TCP_INITIAL_RTO_NO_SYN_RETRANSMISSIONS) \
		|| defined(TCP_INITIAL_RTO_NO_SYN_RETRANSMISSIONS)
static_assert(__SPRT_TCP_INITIAL_RTO_NO_SYN_RETRANSMISSIONS
				== TCP_INITIAL_RTO_NO_SYN_RETRANSMISSIONS,
		"TCP_INITIAL_RTO_NO_SYN_RETRANSMISSIONS differs from native");
#endif
#if defined(__SPRT_TCP_INITIAL_RTO_UNSPECIFIED_MAX_SYN_RETRANSMISSIONS) \
		|| defined(TCP_INITIAL_RTO_UNSPECIFIED_MAX_SYN_RETRANSMISSIONS)
static_assert(__SPRT_TCP_INITIAL_RTO_UNSPECIFIED_MAX_SYN_RETRANSMISSIONS
				== TCP_INITIAL_RTO_UNSPECIFIED_MAX_SYN_RETRANSMISSIONS,
		"TCP_INITIAL_RTO_UNSPECIFIED_MAX_SYN_RETRANSMISSIONS differs from native");
#endif
#if defined(__SPRT_TCP_INITIAL_RTO_UNSPECIFIED_RTT) || defined(TCP_INITIAL_RTO_UNSPECIFIED_RTT)
static_assert(__SPRT_TCP_INITIAL_RTO_UNSPECIFIED_RTT == TCP_INITIAL_RTO_UNSPECIFIED_RTT,
		"TCP_INITIAL_RTO_UNSPECIFIED_RTT differs from native");
#endif
#if defined(__SPRT_TCP_INQ) || defined(TCP_INQ)
static_assert(__SPRT_TCP_INQ == TCP_INQ, "TCP_INQ differs from native");
#endif
#if defined(__SPRT_TCP_IPV4) || defined(TCP_IPV4)
static_assert(__SPRT_TCP_IPV4 == TCP_IPV4, "TCP_IPV4 differs from native");
#endif
#if defined(__SPRT_TCP_IPV6) || defined(TCP_IPV6)
static_assert(__SPRT_TCP_IPV6 == TCP_IPV6, "TCP_IPV6 differs from native");
#endif
#if defined(__SPRT_TCP_IS_MPTCP) || defined(TCP_IS_MPTCP)
static_assert(__SPRT_TCP_IS_MPTCP == TCP_IS_MPTCP, "TCP_IS_MPTCP differs from native");
#endif
#if defined(__SPRT_TCP_KEEPALIVE) || defined(TCP_KEEPALIVE)
static_assert(__SPRT_TCP_KEEPALIVE == TCP_KEEPALIVE, "TCP_KEEPALIVE differs from native");
#endif
#if defined(__SPRT_TCP_KEEPCNT) || defined(TCP_KEEPCNT)
static_assert(__SPRT_TCP_KEEPCNT == TCP_KEEPCNT, "TCP_KEEPCNT differs from native");
#endif
#if defined(__SPRT_TCP_KEEPIDLE) || defined(TCP_KEEPIDLE)
static_assert(__SPRT_TCP_KEEPIDLE == TCP_KEEPIDLE, "TCP_KEEPIDLE differs from native");
#endif
#if defined(__SPRT_TCP_KEEPINTVL) || defined(TCP_KEEPINTVL)
static_assert(__SPRT_TCP_KEEPINTVL == TCP_KEEPINTVL, "TCP_KEEPINTVL differs from native");
#endif
#if defined(__SPRT_TCP_LINGER2) || defined(TCP_LINGER2)
static_assert(__SPRT_TCP_LINGER2 == TCP_LINGER2, "TCP_LINGER2 differs from native");
#endif
#if defined(__SPRT_TCP_MAXHLEN) || defined(TCP_MAXHLEN)
static_assert(__SPRT_TCP_MAXHLEN == TCP_MAXHLEN, "TCP_MAXHLEN differs from native");
#endif
#if defined(__SPRT_TCP_MAXOLEN) || defined(TCP_MAXOLEN)
static_assert(__SPRT_TCP_MAXOLEN == TCP_MAXOLEN, "TCP_MAXOLEN differs from native");
#endif
#if defined(__SPRT_TCP_MAXRT) || defined(TCP_MAXRT)
static_assert(__SPRT_TCP_MAXRT == TCP_MAXRT, "TCP_MAXRT differs from native");
#endif
#if defined(__SPRT_TCP_MAXRTMS) || defined(TCP_MAXRTMS)
static_assert(__SPRT_TCP_MAXRTMS == TCP_MAXRTMS, "TCP_MAXRTMS differs from native");
#endif
#if defined(__SPRT_TCP_MAX_SACK) || defined(TCP_MAX_SACK)
static_assert(__SPRT_TCP_MAX_SACK == TCP_MAX_SACK, "TCP_MAX_SACK differs from native");
#endif
#if defined(__SPRT_TCP_MAXSEG) || defined(TCP_MAXSEG)
static_assert(__SPRT_TCP_MAXSEG == TCP_MAXSEG, "TCP_MAXSEG differs from native");
#endif
#if defined(__SPRT_TCP_MAXWIN) || defined(TCP_MAXWIN)
static_assert(__SPRT_TCP_MAXWIN == TCP_MAXWIN, "TCP_MAXWIN differs from native");
#endif
#if defined(__SPRT_TCP_MAX_WINSHIFT) || defined(TCP_MAX_WINSHIFT)
static_assert(__SPRT_TCP_MAX_WINSHIFT == TCP_MAX_WINSHIFT, "TCP_MAX_WINSHIFT differs from native");
#endif
#if defined(__SPRT_TCP_MD5SIG) || defined(TCP_MD5SIG)
static_assert(__SPRT_TCP_MD5SIG == TCP_MD5SIG, "TCP_MD5SIG differs from native");
#endif
#if defined(__SPRT_TCP_MD5SIG_EXT) || defined(TCP_MD5SIG_EXT)
static_assert(__SPRT_TCP_MD5SIG_EXT == TCP_MD5SIG_EXT, "TCP_MD5SIG_EXT differs from native");
#endif
#if defined(__SPRT_TCP_MD5SIG_FLAG_IFINDEX) || defined(TCP_MD5SIG_FLAG_IFINDEX)
static_assert(__SPRT_TCP_MD5SIG_FLAG_IFINDEX == TCP_MD5SIG_FLAG_IFINDEX,
		"TCP_MD5SIG_FLAG_IFINDEX differs from native");
#endif
#if defined(__SPRT_TCP_MD5SIG_FLAG_PREFIX) || defined(TCP_MD5SIG_FLAG_PREFIX)
static_assert(__SPRT_TCP_MD5SIG_FLAG_PREFIX == TCP_MD5SIG_FLAG_PREFIX,
		"TCP_MD5SIG_FLAG_PREFIX differs from native");
#endif
#if defined(__SPRT_TCP_MD5SIG_MAXKEYLEN) || defined(TCP_MD5SIG_MAXKEYLEN)
static_assert(__SPRT_TCP_MD5SIG_MAXKEYLEN == TCP_MD5SIG_MAXKEYLEN,
		"TCP_MD5SIG_MAXKEYLEN differs from native");
#endif
#if defined(__SPRT_TCP_MINMSS) || defined(TCP_MINMSS)
static_assert(__SPRT_TCP_MINMSS == TCP_MINMSS, "TCP_MINMSS differs from native");
#endif
#if defined(__SPRT_TCP_MSS) || defined(TCP_MSS)
static_assert(__SPRT_TCP_MSS == TCP_MSS, "TCP_MSS differs from native");
#endif
#if defined(__SPRT_TCP_MSS_DEFAULT) || defined(TCP_MSS_DEFAULT)
static_assert(__SPRT_TCP_MSS_DEFAULT == TCP_MSS_DEFAULT, "TCP_MSS_DEFAULT differs from native");
#endif
#if defined(__SPRT_TCP_MSS_DESIRED) || defined(TCP_MSS_DESIRED)
static_assert(__SPRT_TCP_MSS_DESIRED == TCP_MSS_DESIRED, "TCP_MSS_DESIRED differs from native");
#endif
#if defined(__SPRT_TCP_NODELAY) || defined(TCP_NODELAY)
static_assert(__SPRT_TCP_NODELAY == TCP_NODELAY, "TCP_NODELAY differs from native");
#endif
#if defined(__SPRT_TCP_NOOPT) || defined(TCP_NOOPT)
static_assert(__SPRT_TCP_NOOPT == TCP_NOOPT, "TCP_NOOPT differs from native");
#endif
#if defined(__SPRT_TCP_NOPUSH) || defined(TCP_NOPUSH)
static_assert(__SPRT_TCP_NOPUSH == TCP_NOPUSH, "TCP_NOPUSH differs from native");
#endif
#if defined(__SPRT_TCP_NOSYNRETRIES) || defined(TCP_NOSYNRETRIES)
static_assert(__SPRT_TCP_NOSYNRETRIES == TCP_NOSYNRETRIES, "TCP_NOSYNRETRIES differs from native");
#endif
#if defined(__SPRT_TCP_NOTSENT_LOWAT) || defined(TCP_NOTSENT_LOWAT)
static_assert(__SPRT_TCP_NOTSENT_LOWAT == TCP_NOTSENT_LOWAT,
		"TCP_NOTSENT_LOWAT differs from native");
#endif
#if defined(__SPRT_TCP_NOURG) || defined(TCP_NOURG)
static_assert(__SPRT_TCP_NOURG == TCP_NOURG, "TCP_NOURG differs from native");
#endif
#if defined(__SPRT_TCP_OFFLOAD_NO_PREFERENCE) || defined(TCP_OFFLOAD_NO_PREFERENCE)
static_assert(__SPRT_TCP_OFFLOAD_NO_PREFERENCE == TCP_OFFLOAD_NO_PREFERENCE,
		"TCP_OFFLOAD_NO_PREFERENCE differs from native");
#endif
#if defined(__SPRT_TCP_OFFLOAD_NOT_PREFERRED) || defined(TCP_OFFLOAD_NOT_PREFERRED)
static_assert(__SPRT_TCP_OFFLOAD_NOT_PREFERRED == TCP_OFFLOAD_NOT_PREFERRED,
		"TCP_OFFLOAD_NOT_PREFERRED differs from native");
#endif
#if defined(__SPRT_TCP_OFFLOAD_PREFERENCE) || defined(TCP_OFFLOAD_PREFERENCE)
static_assert(__SPRT_TCP_OFFLOAD_PREFERENCE == TCP_OFFLOAD_PREFERENCE,
		"TCP_OFFLOAD_PREFERENCE differs from native");
#endif
#if defined(__SPRT_TCP_OFFLOAD_PREFERRED) || defined(TCP_OFFLOAD_PREFERRED)
static_assert(__SPRT_TCP_OFFLOAD_PREFERRED == TCP_OFFLOAD_PREFERRED,
		"TCP_OFFLOAD_PREFERRED differs from native");
#endif
#if defined(__SPRT_TCP_QUEUE_SEQ) || defined(TCP_QUEUE_SEQ)
static_assert(__SPRT_TCP_QUEUE_SEQ == TCP_QUEUE_SEQ, "TCP_QUEUE_SEQ differs from native");
#endif
#if defined(__SPRT_TCP_QUICKACK) || defined(TCP_QUICKACK)
static_assert(__SPRT_TCP_QUICKACK == TCP_QUICKACK, "TCP_QUICKACK differs from native");
#endif
#if defined(__SPRT_TCP_RECEIVE_ZEROCOPY_FLAG_TLB_CLEAN_HINT) \
		|| defined(TCP_RECEIVE_ZEROCOPY_FLAG_TLB_CLEAN_HINT)
static_assert(__SPRT_TCP_RECEIVE_ZEROCOPY_FLAG_TLB_CLEAN_HINT
				== TCP_RECEIVE_ZEROCOPY_FLAG_TLB_CLEAN_HINT,
		"TCP_RECEIVE_ZEROCOPY_FLAG_TLB_CLEAN_HINT differs from native");
#endif
#if defined(__SPRT_TCP_REPAIR) || defined(TCP_REPAIR)
static_assert(__SPRT_TCP_REPAIR == TCP_REPAIR, "TCP_REPAIR differs from native");
#endif
#if defined(__SPRT_TCP_REPAIR_OFF) || defined(TCP_REPAIR_OFF)
static_assert(__SPRT_TCP_REPAIR_OFF == TCP_REPAIR_OFF, "TCP_REPAIR_OFF differs from native");
#endif
#if defined(__SPRT_TCP_REPAIR_OFF_NO_WP) || defined(TCP_REPAIR_OFF_NO_WP)
static_assert(__SPRT_TCP_REPAIR_OFF_NO_WP == TCP_REPAIR_OFF_NO_WP,
		"TCP_REPAIR_OFF_NO_WP differs from native");
#endif
#if defined(__SPRT_TCP_REPAIR_ON) || defined(TCP_REPAIR_ON)
static_assert(__SPRT_TCP_REPAIR_ON == TCP_REPAIR_ON, "TCP_REPAIR_ON differs from native");
#endif
#if defined(__SPRT_TCP_REPAIR_OPTIONS) || defined(TCP_REPAIR_OPTIONS)
static_assert(__SPRT_TCP_REPAIR_OPTIONS == TCP_REPAIR_OPTIONS,
		"TCP_REPAIR_OPTIONS differs from native");
#endif
#if defined(__SPRT_TCP_REPAIR_QUEUE) || defined(TCP_REPAIR_QUEUE)
static_assert(__SPRT_TCP_REPAIR_QUEUE == TCP_REPAIR_QUEUE, "TCP_REPAIR_QUEUE differs from native");
#endif
#if defined(__SPRT_TCP_REPAIR_WINDOW) || defined(TCP_REPAIR_WINDOW)
static_assert(__SPRT_TCP_REPAIR_WINDOW == TCP_REPAIR_WINDOW,
		"TCP_REPAIR_WINDOW differs from native");
#endif
#if defined(__SPRT_TCP_RXT_CONNDROPTIME) || defined(TCP_RXT_CONNDROPTIME)
static_assert(__SPRT_TCP_RXT_CONNDROPTIME == TCP_RXT_CONNDROPTIME,
		"TCP_RXT_CONNDROPTIME differs from native");
#endif
#if defined(__SPRT_TCP_RXT_FINDROP) || defined(TCP_RXT_FINDROP)
static_assert(__SPRT_TCP_RXT_FINDROP == TCP_RXT_FINDROP, "TCP_RXT_FINDROP differs from native");
#endif
#if defined(__SPRT_TCP_SAVED_SYN) || defined(TCP_SAVED_SYN)
static_assert(__SPRT_TCP_SAVED_SYN == TCP_SAVED_SYN, "TCP_SAVED_SYN differs from native");
#endif
#if defined(__SPRT_TCP_SAVE_SYN) || defined(TCP_SAVE_SYN)
static_assert(__SPRT_TCP_SAVE_SYN == TCP_SAVE_SYN, "TCP_SAVE_SYN differs from native");
#endif
#if defined(__SPRT_TCP_S_DATA_IN) || defined(TCP_S_DATA_IN)
static_assert(__SPRT_TCP_S_DATA_IN == TCP_S_DATA_IN, "TCP_S_DATA_IN differs from native");
#endif
#if defined(__SPRT_TCP_S_DATA_OUT) || defined(TCP_S_DATA_OUT)
static_assert(__SPRT_TCP_S_DATA_OUT == TCP_S_DATA_OUT, "TCP_S_DATA_OUT differs from native");
#endif
#if defined(__SPRT_TCP_SENDMOREACKS) || defined(TCP_SENDMOREACKS)
static_assert(__SPRT_TCP_SENDMOREACKS == TCP_SENDMOREACKS, "TCP_SENDMOREACKS differs from native");
#endif
#if defined(__SPRT_TCP_SET_ACK_FREQUENCY) || defined(TCP_SET_ACK_FREQUENCY)
static_assert(__SPRT_TCP_SET_ACK_FREQUENCY == TCP_SET_ACK_FREQUENCY,
		"TCP_SET_ACK_FREQUENCY differs from native");
#endif
#if defined(__SPRT_TCP_SET_ICW) || defined(TCP_SET_ICW)
static_assert(__SPRT_TCP_SET_ICW == TCP_SET_ICW, "TCP_SET_ICW differs from native");
#endif
#if defined(__SPRT_TCP_STDURG) || defined(TCP_STDURG)
static_assert(__SPRT_TCP_STDURG == TCP_STDURG, "TCP_STDURG differs from native");
#endif
#if defined(__SPRT_TCP_SYNCNT) || defined(TCP_SYNCNT)
static_assert(__SPRT_TCP_SYNCNT == TCP_SYNCNT, "TCP_SYNCNT differs from native");
#endif
#if defined(__SPRT_TCP_THIN_DUPACK) || defined(TCP_THIN_DUPACK)
static_assert(__SPRT_TCP_THIN_DUPACK == TCP_THIN_DUPACK, "TCP_THIN_DUPACK differs from native");
#endif
#if defined(__SPRT_TCP_THIN_LINEAR_TIMEOUTS) || defined(TCP_THIN_LINEAR_TIMEOUTS)
static_assert(__SPRT_TCP_THIN_LINEAR_TIMEOUTS == TCP_THIN_LINEAR_TIMEOUTS,
		"TCP_THIN_LINEAR_TIMEOUTS differs from native");
#endif
#if defined(__SPRT_TCP_TIMESTAMP) || defined(TCP_TIMESTAMP)
static_assert(__SPRT_TCP_TIMESTAMP == TCP_TIMESTAMP, "TCP_TIMESTAMP differs from native");
#endif
#if defined(__SPRT_TCP_TIMESTAMPS) || defined(TCP_TIMESTAMPS)
static_assert(__SPRT_TCP_TIMESTAMPS == TCP_TIMESTAMPS, "TCP_TIMESTAMPS differs from native");
#endif
#if defined(__SPRT_TCP_TX_DELAY) || defined(TCP_TX_DELAY)
static_assert(__SPRT_TCP_TX_DELAY == TCP_TX_DELAY, "TCP_TX_DELAY differs from native");
#endif
#if defined(__SPRT_TCP_ULP) || defined(TCP_ULP)
static_assert(__SPRT_TCP_ULP == TCP_ULP, "TCP_ULP differs from native");
#endif
#if defined(__SPRT_TCP_USER_TIMEOUT) || defined(TCP_USER_TIMEOUT)
static_assert(__SPRT_TCP_USER_TIMEOUT == TCP_USER_TIMEOUT, "TCP_USER_TIMEOUT differs from native");
#endif
#if defined(__SPRT_TCP_WINDOW_CLAMP) || defined(TCP_WINDOW_CLAMP)
static_assert(__SPRT_TCP_WINDOW_CLAMP == TCP_WINDOW_CLAMP, "TCP_WINDOW_CLAMP differs from native");
#endif
#if defined(__SPRT_TCP_ZEROCOPY_RECEIVE) || defined(TCP_ZEROCOPY_RECEIVE)
static_assert(__SPRT_TCP_ZEROCOPY_RECEIVE == TCP_ZEROCOPY_RECEIVE,
		"TCP_ZEROCOPY_RECEIVE differs from native");
#endif

#endif // hosted

namespace sprt {

#if SPRT_WASM
#define __SPRT_SOCK_ENOSYS() \
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__, \
			" not available on this platform"); \
	*__sprt___errno_location() = ENOSYS; \
	return -1
#endif
__SPRT_C_FUNC SOCKET __SPRT_ID(socket)(int __domain, int __type, int __protocol) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#else
	// SOCKET (int on POSIX, winsock SOCKET on Windows); the native error sentinel passes
	// through - INVALID_SOCKET == (SOCKET)-1 == the POSIX -1.
	return ::socket(__domain, __type, __protocol);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(socketpair)(int __domain, int __type, int __protocol, SOCKET __sv[2]) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	(void)__domain, (void)__type, (void)__protocol, (void)__sv;
	*__sprt___errno_location() = ENOSYS; // winsock has no socketpair()
	return -1;
#else
	return ::socketpair(__domain, __type, __protocol, __sv);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		bind)(SOCKET __fd, const struct __SPRT_ID(sockaddr) * __addr, __SPRT_ID(socklen_t) __len) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	return ::bind(__fd, __addr, __len);
#else
	return ::bind(__fd, (const ::sockaddr *)__addr, (::socklen_t)__len);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(connect)(SOCKET __fd, const struct __SPRT_ID(sockaddr) * __addr,
		__SPRT_ID(socklen_t) __len) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	return ::connect(__fd, __addr, __len);
#else
	return ::connect(__fd, (const ::sockaddr *)__addr, (::socklen_t)__len);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(listen)(SOCKET __fd, int __backlog) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#else
	return ::listen(__fd, __backlog);
#endif
}

__SPRT_C_FUNC SOCKET __SPRT_ID(accept)(SOCKET __fd,
		struct __SPRT_ID(sockaddr) * __SPRT_RESTRICT __addr,
		__SPRT_ID(socklen_t) * __SPRT_RESTRICT __len) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	return ::accept(__fd, __addr, __len);
#else
	return ::accept(__fd, (::sockaddr *)__addr, (::socklen_t *)__len);
#endif
}

__SPRT_C_FUNC SOCKET __SPRT_ID(accept4)(SOCKET __fd,
		struct __SPRT_ID(sockaddr) * __SPRT_RESTRICT __addr,
		__SPRT_ID(socklen_t) * __SPRT_RESTRICT __len, int __flags) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	// winsock has no accept4(): accept() then map SOCK_NONBLOCK to FIONBIO (CLOEXEC is
	// a no-op on Windows).
	SOCKET __s = ::accept(__fd, __addr, __len);
	if (__s == INVALID_SOCKET) {
		return __s;
	}
	if (__flags & __SPRT_SOCK_NONBLOCK) {
		unsigned long __nb = 1;
		::ioctlsocket(__s, (long)FIONBIO, &__nb);
	}
	return __s;
#elif SPRT_APPLE
	// macOS ships no accept4(): emulate with accept() + fcntl() for CLOEXEC/NONBLOCK.
	int __s = ::accept(__fd, (::sockaddr *)__addr, (::socklen_t *)__len);
	if (__s < 0) {
		return -1;
	}
	if (__flags & SOCK_CLOEXEC) {
		::fcntl(__s, F_SETFD, ::fcntl(__s, F_GETFD, 0) | FD_CLOEXEC);
	}
	if (__flags & SOCK_NONBLOCK) {
		::fcntl(__s, F_SETFL, ::fcntl(__s, F_GETFL, 0) | O_NONBLOCK);
	}
	return __s;
#else
	return ::accept4(__fd, (::sockaddr *)__addr, (::socklen_t *)__len, __flags);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(getsockname)(SOCKET __fd,
		struct __SPRT_ID(sockaddr) * __SPRT_RESTRICT __addr,
		__SPRT_ID(socklen_t) * __SPRT_RESTRICT __len) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	return ::getsockname(__fd, __addr, __len);
#else
	return ::getsockname(__fd, (::sockaddr *)__addr, (::socklen_t *)__len);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(getpeername)(SOCKET __fd,
		struct __SPRT_ID(sockaddr) * __SPRT_RESTRICT __addr,
		__SPRT_ID(socklen_t) * __SPRT_RESTRICT __len) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	return ::getpeername(__fd, __addr, __len);
#else
	return ::getpeername(__fd, (::sockaddr *)__addr, (::socklen_t *)__len);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(shutdown)(SOCKET __fd, int __how) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#else
	return ::shutdown(__fd, __how);
#endif
}

#if SPRT_ANDROID && !defined(__LP64__)
// ILP32 Bionic: SO_RCVTIMEO/SO_SNDTIMEO resolve to the _OLD values and the kernel
// expects the native `struct timeval` (32-bit long fields), while the sprt public
// struct carries 64-bit fields - translate the payload at the boundary.
// Note: SO_TIMESTAMP*/SCM_TIMESTAMP* ancillary payloads (recvmsg cmsg) also carry
// the native 32-bit layout on ILP32 and are NOT translated here - rewriting cmsg
// buffers would need a full control-message repack; no engine code consumes them
static bool __sprt_sockopt_is_timeo(int __level, int __optname) {
	return __level == __SPRT_SOL_SOCKET
			&& (__optname == __SPRT_SO_RCVTIMEO || __optname == __SPRT_SO_SNDTIMEO);
}
#endif

__SPRT_C_FUNC int __SPRT_ID(getsockopt)(SOCKET __fd, int __level, int __optname,
		sockdata_t *__SPRT_RESTRICT __optval, __SPRT_ID(socklen_t) * __SPRT_RESTRICT __optlen) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	return ::getsockopt(__fd, __level, __optname, __optval, __optlen);
#else
#if SPRT_ANDROID && !defined(__LP64__)
	if (__sprt_sockopt_is_timeo(__level, __optname) && __optval && __optlen
			&& *__optlen == sizeof(struct __SPRT_TIMEVAL_NAME)) {
		::timeval __ntv;
		::socklen_t __nlen = sizeof(__ntv);
		auto __ret = ::getsockopt(__fd, __level, __optname, &__ntv, &__nlen);
		if (__ret == 0) {
			auto __tv = (struct __SPRT_TIMEVAL_NAME *)__optval;
			__tv->tv_sec = __ntv.tv_sec;
			__tv->tv_usec = __ntv.tv_usec;
		}
		return __ret;
	}
#endif
	return ::getsockopt(__fd, __level, __optname, __optval, (::socklen_t *)__optlen);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(setsockopt)(SOCKET __fd, int __level, int __optname,
		const sockdata_t *__optval, __SPRT_ID(socklen_t) __optlen) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	return ::setsockopt(__fd, __level, __optname, __optval, __optlen);
#else
#if SPRT_ANDROID && !defined(__LP64__)
	if (__sprt_sockopt_is_timeo(__level, __optname) && __optval
			&& __optlen == sizeof(struct __SPRT_TIMEVAL_NAME)) {
		auto __tv = (const struct __SPRT_TIMEVAL_NAME *)__optval;
		::timeval __ntv;
		__ntv.tv_sec = (long)__tv->tv_sec;
		__ntv.tv_usec = (long)__tv->tv_usec;
		return ::setsockopt(__fd, __level, __optname, &__ntv, sizeof(__ntv));
	}
#endif
	return ::setsockopt(__fd, __level, __optname, __optval, (::socklen_t)__optlen);
#endif
}

__SPRT_C_FUNC socksize_t __SPRT_ID(
		send)(SOCKET __fd, const sockdata_t *__buf, __SPRT_ID(size_t) __n, int __flags) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	// winsock send() takes an int length and returns int; the char* buffer matches directly.
	return ::send(__fd, __buf, (int)__n, __flags);
#else
	return ::send(__fd, __buf, __n, __flags);
#endif
}

__SPRT_C_FUNC socksize_t __SPRT_ID(
		recv)(SOCKET __fd, sockdata_t *__buf, __SPRT_ID(size_t) __n, int __flags) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	return ::recv(__fd, __buf, (int)__n, __flags);
#else
	return ::recv(__fd, __buf, __n, __flags);
#endif
}

__SPRT_C_FUNC socksize_t __SPRT_ID(sendto)(SOCKET __fd, const sockdata_t *__buf,
		__SPRT_ID(size_t) __n, int __flags, const struct __SPRT_ID(sockaddr) * __addr,
		__SPRT_ID(socklen_t) __addr_len) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	return ::sendto(__fd, __buf, (int)__n, __flags, __addr, __addr_len);
#else
	return ::sendto(__fd, __buf, __n, __flags, (const ::sockaddr *)__addr, (::socklen_t)__addr_len);
#endif
}

__SPRT_C_FUNC socksize_t __SPRT_ID(recvfrom)(SOCKET __fd, sockdata_t *__SPRT_RESTRICT __buf,
		__SPRT_ID(size_t) __n, int __flags, struct __SPRT_ID(sockaddr) * __SPRT_RESTRICT __addr,
		__SPRT_ID(socklen_t) * __SPRT_RESTRICT __addr_len) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	return ::recvfrom(__fd, __buf, (int)__n, __flags, __addr, __addr_len);
#else
	return ::recvfrom(__fd, __buf, __n, __flags, (::sockaddr *)__addr, (::socklen_t *)__addr_len);
#endif
}

#if SPRT_WINDOWS
// Shared iovec -> WSABUF gather for the msg wrappers. Returns the WSABUF array (either
// the caller's stack buffer or a malloc'd one; *__owned is set when it must be freed).
static WSABUF *__sprt_win_gather(const struct __SPRT_ID(iovec) * __iov, unsigned int __n,
		WSABUF *__stack, unsigned int __stackn, bool *__owned) {
	WSABUF *__bufs =
			__n <= __stackn ? __stack : (WSABUF *)::malloc((__SPRT_ID(size_t))__n * sizeof(WSABUF));
	*__owned = (__bufs != __stack);
	if (__bufs) {
		for (unsigned int __i = 0; __i < __n; ++__i) {
			__bufs[__i].len = (ULONG)__iov[__i].iov_len;
			__bufs[__i].buf = (CHAR *)__iov[__i].iov_base;
		}
	}
	return __bufs;
}
#endif

__SPRT_C_FUNC socksize_t __SPRT_ID(
		sendmsg)(SOCKET __fd, const struct __SPRT_ID(msghdr) * __message, int __flags) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	// winsock has no sendmsg(); WSASendTo carries the scatter/gather iovec (ancillary
	// data in msg_control is not forwarded). A null msg_name behaves like WSASend.
	WSABUF __stack[16];
	bool __owned = false;
	unsigned int __n = (unsigned int)__message->msg_iovlen;
	WSABUF *__bufs = __sprt_win_gather(__message->msg_iov, __n, __stack, 16, &__owned);
	if (__bufs == nullptr) {
		*__sprt___errno_location() = ENOMEM;
		return -1;
	}
	DWORD __sent = 0;
	int __r = ::WSASendTo(__fd, __bufs, (DWORD)__n, &__sent, (DWORD)__flags,
			(const struct __SPRT_ID(sockaddr) *)__message->msg_name, (int)__message->msg_namelen,
			nullptr, nullptr);
	if (__owned) {
		::free(__bufs);
	}
	return __r == 0 ? (socksize_t)__sent : -1;
#else
	return ::sendmsg(__fd, (const ::msghdr *)__message, __flags);
#endif
}

__SPRT_C_FUNC socksize_t __SPRT_ID(
		recvmsg)(SOCKET __fd, struct __SPRT_ID(msghdr) * __message, int __flags) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	WSABUF __stack[16];
	bool __owned = false;
	unsigned int __n = (unsigned int)__message->msg_iovlen;
	WSABUF *__bufs = __sprt_win_gather(__message->msg_iov, __n, __stack, 16, &__owned);
	if (__bufs == nullptr) {
		*__sprt___errno_location() = ENOMEM;
		return -1;
	}
	DWORD __recvd = 0;
	DWORD __wflags = (DWORD)__flags;
	int __fromlen = (int)__message->msg_namelen;
	int __r = ::WSARecvFrom(__fd, __bufs, (DWORD)__n, &__recvd, &__wflags,
			(struct __SPRT_ID(sockaddr) *)__message->msg_name,
			__message->msg_name ? &__fromlen : nullptr, nullptr, nullptr);
	if (__message->msg_name) {
		__message->msg_namelen = (__SPRT_ID(socklen_t))__fromlen;
	}
	__message->msg_flags = (int)__wflags;
	if (__owned) {
		::free(__bufs);
	}
	return __r == 0 ? (socksize_t)__recvd : -1;
#else
	return ::recvmsg(__fd, (::msghdr *)__message, __flags);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(sendmmsg)(SOCKET __fd, struct __SPRT_ID(mmsghdr) * __msgvec,
		unsigned int __vlen, unsigned int __flags) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS || SPRT_APPLE
	// No native sendmmsg(): loop sendmsg() over the batch (Linux semantics - return the
	// count sent, or -1 if the first one fails).
	unsigned int __i = 0;
	for (; __i < __vlen; ++__i) {
		socksize_t __r = __SPRT_ID(sendmsg)(__fd, &__msgvec[__i].msg_hdr, (int)__flags);
		if (__r < 0) {
			break;
		}
		__msgvec[__i].msg_len = (unsigned int)__r;
	}
	if (__i == 0 && __vlen > 0) {
		return -1;
	}
	return (int)__i;
#else
	return ::sendmmsg(__fd, (::mmsghdr *)__msgvec, __vlen, __flags);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(recvmmsg)(SOCKET __fd, struct __SPRT_ID(mmsghdr) * __msgvec,
		unsigned int __vlen, unsigned int __flags, struct __SPRT_TIMESPEC_NAME *__timeout) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS || SPRT_APPLE
	// No native recvmmsg(): loop recvmsg(). The timeout is best-effort (not applied
	// between messages), matching how the batch degrades without kernel support.
	(void)__timeout;
	unsigned int __i = 0;
	for (; __i < __vlen; ++__i) {
		socksize_t __r = __SPRT_ID(recvmsg)(__fd, &__msgvec[__i].msg_hdr, (int)__flags);
		if (__r < 0) {
			break;
		}
		__msgvec[__i].msg_len = (unsigned int)__r;
	}
	if (__i == 0 && __vlen > 0) {
		return -1;
	}
	return (int)__i;
#else
	struct timespec __ts;
	if (__timeout) {
		__ts.tv_sec = __timeout->tv_sec;
		__ts.tv_nsec = __timeout->tv_nsec;
	}
	return ::recvmmsg(__fd, (::mmsghdr *)__msgvec, __vlen, __flags, __timeout ? &__ts : nullptr);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(closesocket)(SOCKET s) {
#if SPRT_WINDOWS
	return closesocket(s);
#else
	return close(s);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(ioctlsocket)(SOCKET s, long cmd, unsigned long *argp) {
#if SPRT_WINDOWS
	return ioctlsocket(s, cmd, argp);
#else
	return ioctl(s, cmd, argp);
#endif
}

} // namespace sprt
