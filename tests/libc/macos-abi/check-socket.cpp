// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// ---------------------------------------------------------------------------
// cross/macos_sprt/sockdef.h <-> Darwin <sys/socket.h> parity.
//
// The socket shims (libc_wrapper/sys/SPRuntimeCSysSocket.cpp) forward these
// constants to libSystem with NO translation:
//
//     ::socket(__SPRT_AF_INET, __SPRT_SOCK_STREAM, 0);
//     ::send(fd, buf, n, __flags);          // __flags carries __SPRT_MSG_* bits
//     ::setsockopt(fd, __SPRT_SOL_SOCKET, __SPRT_SO_LINGER, ...);
//
// so every value must be Darwin's. These differ from Linux far more than most:
// AF_INET6 is 30 (Linux 10), SOL_SOCKET is 0xffff (Linux 1), and the SO_* block
// is a bitmask rather than a dense enumeration.
//
// Compile-time only; see check.sh.
// ---------------------------------------------------------------------------

#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SPRT_ABI_HEADER <sprt/c/cross/__sprt_socket.h>
#include "abi_check.h"

// === shutdown() ===
SPRT_CONST(SHUT_RD);
SPRT_CONST(SHUT_WR);
SPRT_CONST(SHUT_RDWR);

// === socket types ===
SPRT_CONST(SOCK_STREAM);
SPRT_CONST(SOCK_DGRAM);
SPRT_CONST(SOCK_RAW);
SPRT_CONST(SOCK_SEQPACKET);
SPRT_CONST(SOCK_RDM);

// === listen backlog ===
SPRT_CONST(SOMAXCONN);

// === address families ===
SPRT_CONST(AF_UNSPEC);
SPRT_CONST(AF_UNIX);
SPRT_CONST(AF_INET);
SPRT_CONST(AF_INET6);
SPRT_CONST(AF_LOCAL);
SPRT_CONST(AF_IMPLINK);
SPRT_CONST(AF_PUP);
SPRT_CONST(AF_CHAOS);
SPRT_CONST(AF_NS);
SPRT_CONST(AF_ISO);
SPRT_CONST(AF_OSI);
SPRT_CONST(AF_ECMA);
SPRT_CONST(AF_DATAKIT);
SPRT_CONST(AF_CCITT);
SPRT_CONST(AF_SNA);
SPRT_CONST(AF_DECnet);
SPRT_CONST(AF_DLI);
SPRT_CONST(AF_LAT);
SPRT_CONST(AF_HYLINK);
SPRT_CONST(AF_APPLETALK);
SPRT_CONST(AF_ROUTE);
SPRT_CONST(AF_LINK);
SPRT_CONST(AF_COIP);
SPRT_CONST(AF_CNT);
SPRT_CONST(AF_IPX);
SPRT_CONST(AF_SIP);
SPRT_CONST(AF_NDRV);
SPRT_CONST(AF_ISDN);
SPRT_CONST(AF_E164);
SPRT_CONST(AF_NATM);
SPRT_CONST(AF_SYSTEM);
SPRT_CONST(AF_NETBIOS);
SPRT_CONST(AF_PPP);
SPRT_CONST(AF_RESERVED_36);
SPRT_CONST(AF_IEEE80211);
SPRT_CONST(AF_UTUN);
SPRT_CONST(AF_VSOCK);
SPRT_CONST(AF_MAX);

// === protocol families ===
SPRT_CONST(PF_UNSPEC);
SPRT_CONST(PF_LOCAL);
SPRT_CONST(PF_UNIX);
SPRT_CONST(PF_INET);
SPRT_CONST(PF_IMPLINK);
SPRT_CONST(PF_PUP);
SPRT_CONST(PF_CHAOS);
SPRT_CONST(PF_NS);
SPRT_CONST(PF_ISO);
SPRT_CONST(PF_OSI);
SPRT_CONST(PF_ECMA);
SPRT_CONST(PF_DATAKIT);
SPRT_CONST(PF_CCITT);
SPRT_CONST(PF_SNA);
SPRT_CONST(PF_KEY);
SPRT_CONST(PF_DECnet);
SPRT_CONST(PF_DLI);
SPRT_CONST(PF_LAT);
SPRT_CONST(PF_HYLINK);
SPRT_CONST(PF_APPLETALK);
SPRT_CONST(PF_ROUTE);
SPRT_CONST(PF_LINK);
SPRT_CONST(PF_XTP);
SPRT_CONST(PF_COIP);
SPRT_CONST(PF_CNT);
SPRT_CONST(PF_SIP);
SPRT_CONST(PF_IPX);
SPRT_CONST(PF_NDRV);
SPRT_CONST(PF_ISDN);
SPRT_CONST(PF_INET6);
SPRT_CONST(PF_NATM);
SPRT_CONST(PF_SYSTEM);
SPRT_CONST(PF_NETBIOS);
SPRT_CONST(PF_PPP);
SPRT_CONST(PF_RESERVED_36);
SPRT_CONST(PF_UTUN);
SPRT_CONST(PF_VSOCK);
SPRT_CONST(PF_MAX);
SPRT_CONST(PF_VLAN);
SPRT_CONST(PF_BOND);

// === setsockopt levels ===
SPRT_CONST(SOL_SOCKET);

// === socket options ===
SPRT_CONST(SO_REUSEADDR);
SPRT_CONST(SO_TYPE);
SPRT_CONST(SO_ERROR);
SPRT_CONST(SO_DONTROUTE);
SPRT_CONST(SO_BROADCAST);
SPRT_CONST(SO_SNDBUF);
SPRT_CONST(SO_RCVBUF);
SPRT_CONST(SO_KEEPALIVE);
SPRT_CONST(SO_OOBINLINE);
SPRT_CONST(SO_LINGER);
SPRT_CONST(SO_REUSEPORT);
SPRT_CONST(SO_DEBUG);
SPRT_CONST(SO_ACCEPTCONN);
SPRT_CONST(SO_USELOOPBACK);
SPRT_CONST(SO_TIMESTAMP);
SPRT_CONST(SO_SNDLOWAT);
SPRT_CONST(SO_RCVLOWAT);
SPRT_CONST(SO_SNDTIMEO);
SPRT_CONST(SO_RCVTIMEO);

// === POSIX msg flags ===
SPRT_CONST(MSG_OOB);
SPRT_CONST(MSG_PEEK);
SPRT_CONST(MSG_DONTROUTE);
SPRT_CONST(MSG_EOR);
SPRT_CONST(MSG_TRUNC);
SPRT_CONST(MSG_CTRUNC);
SPRT_CONST(MSG_WAITALL);
SPRT_CONST(MSG_DONTWAIT);

// === control message types ===
SPRT_CONST(SCM_RIGHTS);
SPRT_CONST(SCM_TIMESTAMP);
SPRT_CONST(SCM_CREDS);
SPRT_CONST(SCM_TIMESTAMP_MONOTONIC);

// === PF_ROUTE sysctl selectors ===
SPRT_CONST(NET_MAXID);
SPRT_CONST(NET_RT_DUMP);
SPRT_CONST(NET_RT_FLAGS);
SPRT_CONST(NET_RT_IFLIST);
SPRT_CONST(NET_RT_STAT);
SPRT_CONST(NET_RT_TRASH);
SPRT_CONST(NET_RT_IFLIST2);
SPRT_CONST(NET_RT_DUMP2);
SPRT_CONST(NET_RT_FLAGS_PRIV);
SPRT_CONST(NET_RT_MAXID);

// === Apple-specific msg flags ==============================================
// Unlike Winsock, Darwin really does have MSG_NOSIGNAL (0x80000, "do not
// generate SIGPIPE on EOF"), so it is a plain value check, not an emulation.
SPRT_CONST(MSG_NOSIGNAL);
SPRT_CONST(MSG_EOF);
SPRT_CONST(MSG_FLUSH);
SPRT_CONST(MSG_HOLD);
SPRT_CONST(MSG_SEND);
SPRT_CONST(MSG_HAVEMORE);
SPRT_CONST(MSG_RCVMORE);
SPRT_CONST(MSG_NEEDSA);

// === deliberate omissions ==================================================
//
// SOCK_CLOEXEC / SOCK_NONBLOCK are Linux socket()/accept4() type bits. Darwin
// has no such thing; the shims mask them out of the type and apply them through
// fcntl(). There is no Darwin value to pin them against, so what is asserted is
// the property the masking relies on -- that neither can be mistaken for one of
// Darwin's SOCK_* types (1..5), and that they are distinct from each other.
static_assert(__SPRT_SOCK_CLOEXEC > __SPRT_SOCK_RDM
				&& __SPRT_SOCK_NONBLOCK > __SPRT_SOCK_RDM,
		"SOCK_CLOEXEC/SOCK_NONBLOCK must stay clear of Darwin's SOCK_* type range");
static_assert(__SPRT_SOCK_CLOEXEC != __SPRT_SOCK_NONBLOCK, "the two type bits must differ");

// MSG_USEUPCALL (0x80000000, "inherit upcall in sock_accept") is guarded by
// `#ifdef KERNEL` in xnu's <sys/socket.h> and stripped from the published SDK
// altogether -- it is a kernel-only flag with no userspace meaning. It is one of
// the few places the +open sysroot (verbatim xnu) is visibly wider than the SDK,
// so it cannot be asserted against both and is left out on purpose.

// SO_REUSEPORT is asserted above with the rest of the SO_* block: Darwin has it,
// unlike Windows where it had to be dropped from the table entirely.
