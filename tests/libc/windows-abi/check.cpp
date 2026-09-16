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
// Static WINAPI-parity check for the Windows __SPRT_* socket + netinet constants.
//
// On the Windows target the SPRT socket shims (runtime/libc_wrapper) forward
// constants straight through to native Winsock without translation, e.g.
//
//     ::send(fd, buf, n, __flags);      // __flags carries __SPRT_MSG_* bits
//     ::socket(__SPRT_AF_INET, __SPRT_SOCK_STREAM, 0);
//     ::setsockopt(fd, IPPROTO_IPV6, __SPRT_IPV6_V6ONLY, ...);
//
// so every __SPRT_* value that reaches Winsock MUST equal the value in the real
// Windows SDK header. This covers both the core socket table (windows_sprt/
// sockdef.h) and the cross netinet table (__sprt_netinet.h, whose IP_/IPV6_/
// MCAST_/INET*_ADDRSTRLEN values are Windows-selected via SPRT_WINDOWS).
//
// The runtime's own asserts (SPRuntimeCSysSocket.cpp) only compile in the
// *hosted* branch (and only validate the Linux values), so they never see
// Winsock's numbers; this TU closes that gap by pinning the __SPRT_* tables
// against the live SDK headers in runtime/toolchains/src/xwin/splat.
//
// It is compile-time only: parse-check it with
//     clang --target=x86_64-pc-windows-msvc -fsyntax-only ...   (see check.sh)
// A clean compile means every checked constant matches; a failing static_assert
// names the diverging constant.
// ---------------------------------------------------------------------------

#include <winsock2.h>   // core SOCK_/AF_/SO_/MSG_/SOL_SOCKET/SOMAXCONN, SD_*, ws2def
#include <ws2tcpip.h>   // IPv6 / addrinfo level constants
#include <mstcpip.h>    // SO_ORIGINAL_DST, SO_REUSE_*PORT, SO_RECEIVED_*, IP6T_SO_ORIGINAL_DST
#include <ws2ipdef.h>   // IP_* / IPV6_* option numbers
#include <afunix.h>     // AF_UNIX

// The namespaced portable table under test (macros only, no dependencies).
#include <sprt/c/cross/windows_sprt/sockdef.h>
#include <sprt/c/cross/__sprt_netinet.h>

// Assert that __SPRT_<name> equals the identically-named Winsock constant.
#define SPRT_SAME(name) \
	static_assert(__SPRT_##name == (name), "__SPRT_" #name " != Winsock " #name)

// Assert against a differently-named native constant (POSIX name -> Win32 name).
#define SPRT_MAP(sprt, native) \
	static_assert(__SPRT_##sprt == (native), "__SPRT_" #sprt " != Winsock " #native)

// === shutdown() how: POSIX SHUT_* map to Winsock SD_* ======================
SPRT_MAP(SHUT_RD, SD_RECEIVE);
SPRT_MAP(SHUT_WR, SD_SEND);
SPRT_MAP(SHUT_RDWR, SD_BOTH);

// === socket types ==========================================================
SPRT_SAME(SOCK_STREAM);
SPRT_SAME(SOCK_DGRAM);
SPRT_SAME(SOCK_RAW);
SPRT_SAME(SOCK_RDM);
SPRT_SAME(SOCK_SEQPACKET);
// SOCK_CLOEXEC / SOCK_NONBLOCK are Linux accept4()/socket() flag bits that SPRT emulates:
// socket() and accept4() mask them out of the type and apply them with FIONBIO. Winsock
// has no counterpart to pin them against, so what is asserted instead is the property the
// emulation depends on - that they do not collide with the type values Winsock does have.
static_assert(((__SPRT_SOCK_CLOEXEC | __SPRT_SOCK_NONBLOCK)
					  & (SOCK_STREAM | SOCK_DGRAM | SOCK_RAW | SOCK_RDM | SOCK_SEQPACKET))
				== 0,
		"__SPRT_SOCK_CLOEXEC/NONBLOCK overlap a Winsock SOCK_ type value");

// === address / protocol families ==========================================
SPRT_SAME(AF_UNSPEC);
SPRT_SAME(AF_UNIX);
SPRT_MAP(AF_LOCAL, AF_UNIX);   // POSIX alias; Winsock only spells it AF_UNIX
SPRT_SAME(AF_INET);
SPRT_SAME(AF_INET6);
SPRT_SAME(AF_IMPLINK);
SPRT_SAME(AF_PUP);
SPRT_SAME(AF_CHAOS);
SPRT_SAME(AF_NS);
SPRT_SAME(AF_IPX);
SPRT_SAME(AF_ISO);
SPRT_SAME(AF_OSI);
SPRT_SAME(AF_ECMA);
SPRT_SAME(AF_DATAKIT);
SPRT_SAME(AF_CCITT);
SPRT_SAME(AF_SNA);
SPRT_SAME(AF_DECnet);
SPRT_SAME(AF_DLI);
SPRT_SAME(AF_LAT);
SPRT_SAME(AF_HYLINK);
SPRT_SAME(AF_APPLETALK);
SPRT_SAME(AF_NETBIOS);
SPRT_SAME(AF_VOICEVIEW);
SPRT_SAME(AF_FIREFOX);
SPRT_SAME(AF_UNKNOWN1);
SPRT_SAME(AF_BAN);
SPRT_SAME(AF_ATM);
SPRT_SAME(AF_CLUSTER);
SPRT_SAME(AF_12844);
SPRT_SAME(AF_IRDA);
SPRT_SAME(AF_NETDES);
SPRT_SAME(AF_TCNPROCESS);
SPRT_SAME(AF_TCNMESSAGE);
SPRT_SAME(AF_ICLFXBM);
SPRT_SAME(AF_BTH);
SPRT_SAME(AF_LINK);
SPRT_SAME(AF_HYPERV);
SPRT_SAME(AF_MAX);

SPRT_SAME(PF_UNSPEC);
SPRT_SAME(PF_UNIX);
SPRT_SAME(PF_INET);
SPRT_SAME(PF_IMPLINK);
SPRT_SAME(PF_PUP);
SPRT_SAME(PF_CHAOS);
SPRT_SAME(PF_NS);
SPRT_SAME(PF_IPX);
SPRT_SAME(PF_ISO);
SPRT_SAME(PF_OSI);
SPRT_SAME(PF_ECMA);
SPRT_SAME(PF_DATAKIT);
SPRT_SAME(PF_CCITT);
SPRT_SAME(PF_SNA);
SPRT_SAME(PF_DECnet);
SPRT_SAME(PF_DLI);
SPRT_SAME(PF_LAT);
SPRT_SAME(PF_HYLINK);
SPRT_SAME(PF_APPLETALK);
SPRT_SAME(PF_VOICEVIEW);
SPRT_SAME(PF_FIREFOX);
SPRT_SAME(PF_UNKNOWN1);
SPRT_SAME(PF_BAN);
SPRT_SAME(PF_ATM);
SPRT_SAME(PF_INET6);
SPRT_SAME(PF_BTH);
// Winsock provides no PF_LINK / PF_HYPERV alias; pin the SPRT PF alias against
// the AF_ spelling it is defined to equal (AF_LINK / AF_HYPERV are checked above).
SPRT_MAP(PF_LINK, AF_LINK);
SPRT_MAP(PF_HYPERV, AF_HYPERV);
SPRT_SAME(PF_MAX);

// === SOL_ levels ===========================================================
SPRT_SAME(SOL_SOCKET);
// Winsock has no SOL_IP / SOL_IPV6 spelling: the per-protocol setsockopt() level is the
// protocol number itself, which is what these must equal.
SPRT_MAP(SOL_IP, IPPROTO_IP);
SPRT_MAP(SOL_IPV6, IPPROTO_IPV6);

// === SO_ options ===========================================================
SPRT_SAME(SO_REUSEADDR);
SPRT_SAME(SO_TYPE);
SPRT_SAME(SO_ERROR);
SPRT_SAME(SO_DONTROUTE);
SPRT_SAME(SO_BROADCAST);
SPRT_SAME(SO_SNDBUF);
SPRT_SAME(SO_RCVBUF);
SPRT_SAME(SO_KEEPALIVE);
SPRT_SAME(SO_OOBINLINE);
SPRT_SAME(SO_LINGER);
SPRT_SAME(SO_DEBUG);
SPRT_SAME(SO_ACCEPTCONN);
SPRT_SAME(SO_USELOOPBACK);
SPRT_SAME(SO_DONTLINGER);
SPRT_SAME(SO_EXCLUSIVEADDRUSE);
SPRT_SAME(SO_SNDLOWAT);
SPRT_SAME(SO_RCVLOWAT);
SPRT_SAME(SO_SNDTIMEO);
SPRT_SAME(SO_RCVTIMEO);
SPRT_SAME(SO_BSP_STATE);
SPRT_SAME(SO_GROUP_ID);
SPRT_SAME(SO_GROUP_PRIORITY);
SPRT_SAME(SO_MAX_MSG_SIZE);
SPRT_SAME(SO_CONDITIONAL_ACCEPT);
SPRT_SAME(SO_PAUSE_ACCEPT);
SPRT_SAME(SO_COMPARTMENT_ID);
SPRT_SAME(SO_RANDOMIZE_PORT);
SPRT_SAME(SO_PORT_SCALABILITY);
SPRT_SAME(SO_REUSE_UNICASTPORT);
SPRT_SAME(SO_REUSE_MULTICASTPORT);
SPRT_SAME(SO_ORIGINAL_DST);
SPRT_SAME(SO_RECEIVED_HOPLIMIT);
SPRT_SAME(SO_RECEIVED_PROCESSOR);
SPRT_SAME(IP6T_SO_ORIGINAL_DST);

// === MSG_ flags ============================================================
SPRT_SAME(MSG_OOB);
SPRT_SAME(MSG_PEEK);
SPRT_SAME(MSG_DONTROUTE);
SPRT_SAME(MSG_CTRUNC);
SPRT_SAME(MSG_TRUNC);
SPRT_SAME(MSG_WAITALL);
SPRT_SAME(MSG_PARTIAL);
SPRT_SAME(MSG_MAXIOVLEN);
// MSG_NOSIGNAL has no Winsock bit and must not invent one: Winsock fails send() with
// WSAEOPNOTSUPP on any flag it does not recognize, and there is no SIGPIPE here for the
// flag to suppress, so zero is the whole of it. Pin that, since it is what keeps every
// send() carrying the flag - curl's do, via SEND_4TH_ARG - working.
static_assert(__SPRT_MSG_NOSIGNAL == 0,
		"__SPRT_MSG_NOSIGNAL must stay 0: Winsock rejects unknown send() flags");
// MSG_DONTWAIT / MSG_EOR are absent from the Windows table (no per-call non-blocking flag,
// no record boundaries), so there is nothing to check for them.

// === SOMAXCONN / AF_UNIX path length ========================================
SPRT_SAME(SOMAXCONN);
SPRT_SAME(UNIX_PATH_MAX);   // afunix.h; sizes sockaddr_un.sun_path in both tables

// ===========================================================================
// netinet constants from <sprt/c/cross/__sprt_netinet.h>. On this target it
// resolves to its SPRT_WINDOWS values; each is pinned against the live SDK.
// IPPROTO_* are a Winsock enum (not macros) so they are asserted directly.
//
// The macro families use the `defined(__SPRT_X) || defined(X)` guard - the same
// one the runtime's hosted asserts use (SPRuntimeCSysSocket.cpp) - so the check
// runs in BOTH directions: if either side defines a name, the assert has to
// compile. A Linux-only option left in windows_sprt/netinetdef.h therefore fails
// as "use of undeclared identifier 'IP_FREEBIND'" instead of being silently
// skipped, and an option Winsock has but the table misses fails as "use of
// undeclared identifier '__SPRT_IP_FREEBIND'".
//
// That matters because the names are a promise: portable code feature-tests them
// (`#ifdef IP_BIND_ADDRESS_NO_PORT` in curl's cf-socket.c) and calls setsockopt()
// with whatever it finds. A Linux number that Winsock does not have is either
// rejected at runtime or - worse - lands on a different Windows option with the
// same number (24 is IP_RECVIF there), so the table must carry Winsock's surface
// exactly, and nothing else. Names with no Winsock spelling at all are simply
// absent from the Windows table, which is what makes them skip here.
// ===========================================================================

// --- Address string lengths ---
#if defined(__SPRT_INET_ADDRSTRLEN) || defined(INET_ADDRSTRLEN)
SPRT_SAME(INET_ADDRSTRLEN);
#endif
#if defined(__SPRT_INET6_ADDRSTRLEN) || defined(INET6_ADDRSTRLEN)
SPRT_SAME(INET6_ADDRSTRLEN);
#endif

// --- IP protocol numbers (Winsock enum) ---
SPRT_SAME(IPPROTO_IP);
SPRT_SAME(IPPROTO_HOPOPTS);
SPRT_SAME(IPPROTO_ICMP);
SPRT_SAME(IPPROTO_IGMP);
SPRT_SAME(IPPROTO_TCP);
SPRT_SAME(IPPROTO_EGP);
SPRT_SAME(IPPROTO_PUP);
SPRT_SAME(IPPROTO_UDP);
SPRT_SAME(IPPROTO_IDP);
SPRT_SAME(IPPROTO_IPV6);
SPRT_SAME(IPPROTO_ROUTING);
SPRT_SAME(IPPROTO_FRAGMENT);
SPRT_SAME(IPPROTO_ESP);
SPRT_SAME(IPPROTO_AH);
SPRT_SAME(IPPROTO_ICMPV6);
SPRT_SAME(IPPROTO_NONE);
SPRT_SAME(IPPROTO_DSTOPTS);
SPRT_SAME(IPPROTO_PIM);
SPRT_SAME(IPPROTO_SCTP);
SPRT_SAME(IPPROTO_RAW);
SPRT_SAME(IPPROTO_MAX);

// --- Ports / IPv4 address classes ---
#if defined(__SPRT_IPPORT_RESERVED) || defined(IPPORT_RESERVED)
SPRT_SAME(IPPORT_RESERVED);
#endif
#if defined(__SPRT_IN_CLASSA_NET) || defined(IN_CLASSA_NET)
SPRT_SAME(IN_CLASSA_NET);
#endif
#if defined(__SPRT_IN_CLASSA_NSHIFT) || defined(IN_CLASSA_NSHIFT)
SPRT_SAME(IN_CLASSA_NSHIFT);
#endif
#if defined(__SPRT_IN_CLASSA_HOST) || defined(IN_CLASSA_HOST)
SPRT_SAME(IN_CLASSA_HOST);
#endif
#if defined(__SPRT_IN_CLASSA_MAX) || defined(IN_CLASSA_MAX)
SPRT_SAME(IN_CLASSA_MAX);
#endif
#if defined(__SPRT_IN_CLASSB_NET) || defined(IN_CLASSB_NET)
SPRT_SAME(IN_CLASSB_NET);
#endif
#if defined(__SPRT_IN_CLASSB_NSHIFT) || defined(IN_CLASSB_NSHIFT)
SPRT_SAME(IN_CLASSB_NSHIFT);
#endif
#if defined(__SPRT_IN_CLASSB_HOST) || defined(IN_CLASSB_HOST)
SPRT_SAME(IN_CLASSB_HOST);
#endif
#if defined(__SPRT_IN_CLASSB_MAX) || defined(IN_CLASSB_MAX)
SPRT_SAME(IN_CLASSB_MAX);
#endif
#if defined(__SPRT_IN_CLASSC_NET) || defined(IN_CLASSC_NET)
SPRT_SAME(IN_CLASSC_NET);
#endif
#if defined(__SPRT_IN_CLASSC_NSHIFT) || defined(IN_CLASSC_NSHIFT)
SPRT_SAME(IN_CLASSC_NSHIFT);
#endif
#if defined(__SPRT_IN_CLASSC_HOST) || defined(IN_CLASSC_HOST)
SPRT_SAME(IN_CLASSC_HOST);
#endif
// IN_LOOPBACKNET is one of the platform-invariant constants __sprt_netinet.h
// defines for every target, not a per-platform table entry: it is the loopback
// network number (127), never handed to Winsock, and no Windows header spells it.
// Nothing to pin it against, so it is not asserted here.

// --- IPv4 options (IPPROTO_IP level) ---
#if defined(__SPRT_IP_TOS) || defined(IP_TOS)
SPRT_SAME(IP_TOS);
#endif
#if defined(__SPRT_IP_TTL) || defined(IP_TTL)
SPRT_SAME(IP_TTL);
#endif
#if defined(__SPRT_IP_HDRINCL) || defined(IP_HDRINCL)
SPRT_SAME(IP_HDRINCL);
#endif
#if defined(__SPRT_IP_OPTIONS) || defined(IP_OPTIONS)
SPRT_SAME(IP_OPTIONS);
#endif
#if defined(__SPRT_IP_ROUTER_ALERT) || defined(IP_ROUTER_ALERT)
SPRT_SAME(IP_ROUTER_ALERT);
#endif
#if defined(__SPRT_IP_RECVOPTS) || defined(IP_RECVOPTS)
SPRT_SAME(IP_RECVOPTS);
#endif
#if defined(__SPRT_IP_RETOPTS) || defined(IP_RETOPTS)
SPRT_SAME(IP_RETOPTS);
#endif
#if defined(__SPRT_IP_PKTINFO) || defined(IP_PKTINFO)
SPRT_SAME(IP_PKTINFO);
#endif
#if defined(__SPRT_IP_PKTOPTIONS) || defined(IP_PKTOPTIONS)
SPRT_SAME(IP_PKTOPTIONS);
#endif
#if defined(__SPRT_IP_PMTUDISC) || defined(IP_PMTUDISC)
SPRT_SAME(IP_PMTUDISC);
#endif
#if defined(__SPRT_IP_MTU_DISCOVER) || defined(IP_MTU_DISCOVER)
SPRT_SAME(IP_MTU_DISCOVER);
#endif
#if defined(__SPRT_IP_RECVERR) || defined(IP_RECVERR)
SPRT_SAME(IP_RECVERR);
#endif
#if defined(__SPRT_IP_RECVTTL) || defined(IP_RECVTTL)
SPRT_SAME(IP_RECVTTL);
#endif
#if defined(__SPRT_IP_RECVTOS) || defined(IP_RECVTOS)
SPRT_SAME(IP_RECVTOS);
#endif
#if defined(__SPRT_IP_MTU) || defined(IP_MTU)
SPRT_SAME(IP_MTU);
#endif
#if defined(__SPRT_IP_FREEBIND) || defined(IP_FREEBIND)
SPRT_SAME(IP_FREEBIND);
#endif
#if defined(__SPRT_IP_IPSEC_POLICY) || defined(IP_IPSEC_POLICY)
SPRT_SAME(IP_IPSEC_POLICY);
#endif
#if defined(__SPRT_IP_XFRM_POLICY) || defined(IP_XFRM_POLICY)
SPRT_SAME(IP_XFRM_POLICY);
#endif
#if defined(__SPRT_IP_PASSSEC) || defined(IP_PASSSEC)
SPRT_SAME(IP_PASSSEC);
#endif
#if defined(__SPRT_IP_TRANSPARENT) || defined(IP_TRANSPARENT)
SPRT_SAME(IP_TRANSPARENT);
#endif
#if defined(__SPRT_IP_ORIGDSTADDR) || defined(IP_ORIGDSTADDR)
SPRT_SAME(IP_ORIGDSTADDR);
#endif
#if defined(__SPRT_IP_RECVORIGDSTADDR) || defined(IP_RECVORIGDSTADDR)
SPRT_SAME(IP_RECVORIGDSTADDR);
#endif
#if defined(__SPRT_IP_MINTTL) || defined(IP_MINTTL)
SPRT_SAME(IP_MINTTL);
#endif
#if defined(__SPRT_IP_NODEFRAG) || defined(IP_NODEFRAG)
SPRT_SAME(IP_NODEFRAG);
#endif
#if defined(__SPRT_IP_CHECKSUM) || defined(IP_CHECKSUM)
SPRT_SAME(IP_CHECKSUM);
#endif
#if defined(__SPRT_IP_BIND_ADDRESS_NO_PORT) || defined(IP_BIND_ADDRESS_NO_PORT)
SPRT_SAME(IP_BIND_ADDRESS_NO_PORT);
#endif
#if defined(__SPRT_IP_RECVFRAGSIZE) || defined(IP_RECVFRAGSIZE)
SPRT_SAME(IP_RECVFRAGSIZE);
#endif
#if defined(__SPRT_IP_RECVERR_RFC4884) || defined(IP_RECVERR_RFC4884)
SPRT_SAME(IP_RECVERR_RFC4884);
#endif
#if defined(__SPRT_IP_MULTICAST_IF) || defined(IP_MULTICAST_IF)
SPRT_SAME(IP_MULTICAST_IF);
#endif
#if defined(__SPRT_IP_MULTICAST_TTL) || defined(IP_MULTICAST_TTL)
SPRT_SAME(IP_MULTICAST_TTL);
#endif
#if defined(__SPRT_IP_MULTICAST_LOOP) || defined(IP_MULTICAST_LOOP)
SPRT_SAME(IP_MULTICAST_LOOP);
#endif
#if defined(__SPRT_IP_ADD_MEMBERSHIP) || defined(IP_ADD_MEMBERSHIP)
SPRT_SAME(IP_ADD_MEMBERSHIP);
#endif
#if defined(__SPRT_IP_DROP_MEMBERSHIP) || defined(IP_DROP_MEMBERSHIP)
SPRT_SAME(IP_DROP_MEMBERSHIP);
#endif
#if defined(__SPRT_IP_UNBLOCK_SOURCE) || defined(IP_UNBLOCK_SOURCE)
SPRT_SAME(IP_UNBLOCK_SOURCE);
#endif
#if defined(__SPRT_IP_BLOCK_SOURCE) || defined(IP_BLOCK_SOURCE)
SPRT_SAME(IP_BLOCK_SOURCE);
#endif
#if defined(__SPRT_IP_ADD_SOURCE_MEMBERSHIP) || defined(IP_ADD_SOURCE_MEMBERSHIP)
SPRT_SAME(IP_ADD_SOURCE_MEMBERSHIP);
#endif
#if defined(__SPRT_IP_DROP_SOURCE_MEMBERSHIP) || defined(IP_DROP_SOURCE_MEMBERSHIP)
SPRT_SAME(IP_DROP_SOURCE_MEMBERSHIP);
#endif
#if defined(__SPRT_IP_MSFILTER) || defined(IP_MSFILTER)
SPRT_SAME(IP_MSFILTER);
#endif
#if defined(__SPRT_IP_MULTICAST_ALL) || defined(IP_MULTICAST_ALL)
SPRT_SAME(IP_MULTICAST_ALL);
#endif
#if defined(__SPRT_IP_UNICAST_IF) || defined(IP_UNICAST_IF)
SPRT_SAME(IP_UNICAST_IF);
#endif
#if defined(__SPRT_IP_RECVRETOPTS) || defined(IP_RECVRETOPTS)
SPRT_SAME(IP_RECVRETOPTS);
#endif
#if defined(__SPRT_IP_DONTFRAGMENT) || defined(IP_DONTFRAGMENT)
SPRT_SAME(IP_DONTFRAGMENT);
#endif
#if defined(__SPRT_IP_HOPLIMIT) || defined(IP_HOPLIMIT)
SPRT_SAME(IP_HOPLIMIT);
#endif
#if defined(__SPRT_IP_RECEIVE_BROADCAST) || defined(IP_RECEIVE_BROADCAST)
SPRT_SAME(IP_RECEIVE_BROADCAST);
#endif
#if defined(__SPRT_IP_RECVIF) || defined(IP_RECVIF)
SPRT_SAME(IP_RECVIF);
#endif
#if defined(__SPRT_IP_RECVDSTADDR) || defined(IP_RECVDSTADDR)
SPRT_SAME(IP_RECVDSTADDR);
#endif
#if defined(__SPRT_IP_IFLIST) || defined(IP_IFLIST)
SPRT_SAME(IP_IFLIST);
#endif
#if defined(__SPRT_IP_ADD_IFLIST) || defined(IP_ADD_IFLIST)
SPRT_SAME(IP_ADD_IFLIST);
#endif
#if defined(__SPRT_IP_DEL_IFLIST) || defined(IP_DEL_IFLIST)
SPRT_SAME(IP_DEL_IFLIST);
#endif
#if defined(__SPRT_IP_RTHDR) || defined(IP_RTHDR)
SPRT_SAME(IP_RTHDR);
#endif
#if defined(__SPRT_IP_GET_IFLIST) || defined(IP_GET_IFLIST)
SPRT_SAME(IP_GET_IFLIST);
#endif
#if defined(__SPRT_IP_RECVRTHDR) || defined(IP_RECVRTHDR)
SPRT_SAME(IP_RECVRTHDR);
#endif
#if defined(__SPRT_IP_TCLASS) || defined(IP_TCLASS)
SPRT_SAME(IP_TCLASS);
#endif
#if defined(__SPRT_IP_RECVTCLASS) || defined(IP_RECVTCLASS)
SPRT_SAME(IP_RECVTCLASS);
#endif
#if defined(__SPRT_IP_ORIGINAL_ARRIVAL_IF) || defined(IP_ORIGINAL_ARRIVAL_IF)
SPRT_SAME(IP_ORIGINAL_ARRIVAL_IF);
#endif
#if defined(__SPRT_IP_ECN) || defined(IP_ECN)
SPRT_SAME(IP_ECN);
#endif
#if defined(__SPRT_IP_RECVECN) || defined(IP_RECVECN)
SPRT_SAME(IP_RECVECN);
#endif
#if defined(__SPRT_IP_PKTINFO_EX) || defined(IP_PKTINFO_EX)
SPRT_SAME(IP_PKTINFO_EX);
#endif
#if defined(__SPRT_IP_WFP_REDIRECT_RECORDS) || defined(IP_WFP_REDIRECT_RECORDS)
SPRT_SAME(IP_WFP_REDIRECT_RECORDS);
#endif
#if defined(__SPRT_IP_WFP_REDIRECT_CONTEXT) || defined(IP_WFP_REDIRECT_CONTEXT)
SPRT_SAME(IP_WFP_REDIRECT_CONTEXT);
#endif
#if defined(__SPRT_IP_NRT_INTERFACE) || defined(IP_NRT_INTERFACE)
SPRT_SAME(IP_NRT_INTERFACE);
#endif
#if defined(__SPRT_IP_USER_MTU) || defined(IP_USER_MTU)
SPRT_SAME(IP_USER_MTU);
#endif
// IP_MTU_DISCOVER argument values. Winsock declares them as the PMTUD_STATE enum,
// so `defined(IP_PMTUDISC_DO)` is false and only the __SPRT_ side opens the block -
// the assert still compares against the enumerator, which is the point: Windows
// numbers them differently from Linux (DO is 1 there, 2 here).
#if defined(__SPRT_IP_PMTUDISC_NOT_SET) || defined(IP_PMTUDISC_NOT_SET)
SPRT_SAME(IP_PMTUDISC_NOT_SET);
#endif
#if defined(__SPRT_IP_PMTUDISC_DONT) || defined(IP_PMTUDISC_DONT)
SPRT_SAME(IP_PMTUDISC_DONT);
#endif
#if defined(__SPRT_IP_PMTUDISC_WANT) || defined(IP_PMTUDISC_WANT)
SPRT_SAME(IP_PMTUDISC_WANT);
#endif
#if defined(__SPRT_IP_PMTUDISC_DO) || defined(IP_PMTUDISC_DO)
SPRT_SAME(IP_PMTUDISC_DO);
#endif
#if defined(__SPRT_IP_PMTUDISC_PROBE) || defined(IP_PMTUDISC_PROBE)
SPRT_SAME(IP_PMTUDISC_PROBE);
#endif
#if defined(__SPRT_IP_PMTUDISC_INTERFACE) || defined(IP_PMTUDISC_INTERFACE)
SPRT_SAME(IP_PMTUDISC_INTERFACE);
#endif
#if defined(__SPRT_IP_PMTUDISC_OMIT) || defined(IP_PMTUDISC_OMIT)
SPRT_SAME(IP_PMTUDISC_OMIT);
#endif
#if defined(__SPRT_IP_PMTUDISC_MAX) || defined(IP_PMTUDISC_MAX)
SPRT_SAME(IP_PMTUDISC_MAX);
#endif

// IP_DEFAULT_MULTICAST_TTL / IP_DEFAULT_MULTICAST_LOOP / IP_MAX_MEMBERSHIPS are
// Winsock 1.1 names that live in <winsock.h> alone, and winsock2.h defines
// _WINSOCKAPI_ to keep that header out of the TU, so they cannot be pinned from
// here. Their values (1 / 1 / 20) are the ones winsock.h has.

// --- IPv6 options (IPPROTO_IPV6 level) ---
#if defined(__SPRT_IPV6_ADDRFORM) || defined(IPV6_ADDRFORM)
SPRT_SAME(IPV6_ADDRFORM);
#endif
#if defined(__SPRT_IPV6_2292PKTINFO) || defined(IPV6_2292PKTINFO)
SPRT_SAME(IPV6_2292PKTINFO);
#endif
#if defined(__SPRT_IPV6_2292HOPOPTS) || defined(IPV6_2292HOPOPTS)
SPRT_SAME(IPV6_2292HOPOPTS);
#endif
#if defined(__SPRT_IPV6_2292DSTOPTS) || defined(IPV6_2292DSTOPTS)
SPRT_SAME(IPV6_2292DSTOPTS);
#endif
#if defined(__SPRT_IPV6_2292RTHDR) || defined(IPV6_2292RTHDR)
SPRT_SAME(IPV6_2292RTHDR);
#endif
#if defined(__SPRT_IPV6_2292PKTOPTIONS) || defined(IPV6_2292PKTOPTIONS)
SPRT_SAME(IPV6_2292PKTOPTIONS);
#endif
#if defined(__SPRT_IPV6_CHECKSUM) || defined(IPV6_CHECKSUM)
SPRT_SAME(IPV6_CHECKSUM);
#endif
#if defined(__SPRT_IPV6_2292HOPLIMIT) || defined(IPV6_2292HOPLIMIT)
SPRT_SAME(IPV6_2292HOPLIMIT);
#endif
#if defined(__SPRT_IPV6_NEXTHOP) || defined(IPV6_NEXTHOP)
SPRT_SAME(IPV6_NEXTHOP);
#endif
#if defined(__SPRT_IPV6_AUTHHDR) || defined(IPV6_AUTHHDR)
SPRT_SAME(IPV6_AUTHHDR);
#endif
#if defined(__SPRT_IPV6_UNICAST_HOPS) || defined(IPV6_UNICAST_HOPS)
SPRT_SAME(IPV6_UNICAST_HOPS);
#endif
#if defined(__SPRT_IPV6_MULTICAST_IF) || defined(IPV6_MULTICAST_IF)
SPRT_SAME(IPV6_MULTICAST_IF);
#endif
#if defined(__SPRT_IPV6_MULTICAST_HOPS) || defined(IPV6_MULTICAST_HOPS)
SPRT_SAME(IPV6_MULTICAST_HOPS);
#endif
#if defined(__SPRT_IPV6_MULTICAST_LOOP) || defined(IPV6_MULTICAST_LOOP)
SPRT_SAME(IPV6_MULTICAST_LOOP);
#endif
#if defined(__SPRT_IPV6_JOIN_GROUP) || defined(IPV6_JOIN_GROUP)
SPRT_SAME(IPV6_JOIN_GROUP);
#endif
#if defined(__SPRT_IPV6_LEAVE_GROUP) || defined(IPV6_LEAVE_GROUP)
SPRT_SAME(IPV6_LEAVE_GROUP);
#endif
#if defined(__SPRT_IPV6_ROUTER_ALERT) || defined(IPV6_ROUTER_ALERT)
SPRT_SAME(IPV6_ROUTER_ALERT);
#endif
#if defined(__SPRT_IPV6_MTU_DISCOVER) || defined(IPV6_MTU_DISCOVER)
SPRT_SAME(IPV6_MTU_DISCOVER);
#endif
#if defined(__SPRT_IPV6_MTU) || defined(IPV6_MTU)
SPRT_SAME(IPV6_MTU);
#endif
#if defined(__SPRT_IPV6_RECVERR) || defined(IPV6_RECVERR)
SPRT_SAME(IPV6_RECVERR);
#endif
#if defined(__SPRT_IPV6_V6ONLY) || defined(IPV6_V6ONLY)
SPRT_SAME(IPV6_V6ONLY);
#endif
#if defined(__SPRT_IPV6_JOIN_ANYCAST) || defined(IPV6_JOIN_ANYCAST)
SPRT_SAME(IPV6_JOIN_ANYCAST);
#endif
#if defined(__SPRT_IPV6_LEAVE_ANYCAST) || defined(IPV6_LEAVE_ANYCAST)
SPRT_SAME(IPV6_LEAVE_ANYCAST);
#endif
#if defined(__SPRT_IPV6_MULTICAST_ALL) || defined(IPV6_MULTICAST_ALL)
SPRT_SAME(IPV6_MULTICAST_ALL);
#endif
#if defined(__SPRT_IPV6_ROUTER_ALERT_ISOLATE) || defined(IPV6_ROUTER_ALERT_ISOLATE)
SPRT_SAME(IPV6_ROUTER_ALERT_ISOLATE);
#endif
#if defined(__SPRT_IPV6_IPSEC_POLICY) || defined(IPV6_IPSEC_POLICY)
SPRT_SAME(IPV6_IPSEC_POLICY);
#endif
#if defined(__SPRT_IPV6_XFRM_POLICY) || defined(IPV6_XFRM_POLICY)
SPRT_SAME(IPV6_XFRM_POLICY);
#endif
#if defined(__SPRT_IPV6_HDRINCL) || defined(IPV6_HDRINCL)
SPRT_SAME(IPV6_HDRINCL);
#endif
#if defined(__SPRT_IPV6_RECVPKTINFO) || defined(IPV6_RECVPKTINFO)
SPRT_SAME(IPV6_RECVPKTINFO);
#endif
#if defined(__SPRT_IPV6_PKTINFO) || defined(IPV6_PKTINFO)
SPRT_SAME(IPV6_PKTINFO);
#endif
#if defined(__SPRT_IPV6_RECVHOPLIMIT) || defined(IPV6_RECVHOPLIMIT)
SPRT_SAME(IPV6_RECVHOPLIMIT);
#endif
#if defined(__SPRT_IPV6_HOPLIMIT) || defined(IPV6_HOPLIMIT)
SPRT_SAME(IPV6_HOPLIMIT);
#endif
#if defined(__SPRT_IPV6_RECVHOPOPTS) || defined(IPV6_RECVHOPOPTS)
SPRT_SAME(IPV6_RECVHOPOPTS);
#endif
#if defined(__SPRT_IPV6_HOPOPTS) || defined(IPV6_HOPOPTS)
SPRT_SAME(IPV6_HOPOPTS);
#endif
#if defined(__SPRT_IPV6_RTHDRDSTOPTS) || defined(IPV6_RTHDRDSTOPTS)
SPRT_SAME(IPV6_RTHDRDSTOPTS);
#endif
#if defined(__SPRT_IPV6_RECVRTHDR) || defined(IPV6_RECVRTHDR)
SPRT_SAME(IPV6_RECVRTHDR);
#endif
#if defined(__SPRT_IPV6_RTHDR) || defined(IPV6_RTHDR)
SPRT_SAME(IPV6_RTHDR);
#endif
#if defined(__SPRT_IPV6_RECVDSTOPTS) || defined(IPV6_RECVDSTOPTS)
SPRT_SAME(IPV6_RECVDSTOPTS);
#endif
#if defined(__SPRT_IPV6_DSTOPTS) || defined(IPV6_DSTOPTS)
SPRT_SAME(IPV6_DSTOPTS);
#endif
#if defined(__SPRT_IPV6_RECVPATHMTU) || defined(IPV6_RECVPATHMTU)
SPRT_SAME(IPV6_RECVPATHMTU);
#endif
#if defined(__SPRT_IPV6_PATHMTU) || defined(IPV6_PATHMTU)
SPRT_SAME(IPV6_PATHMTU);
#endif
#if defined(__SPRT_IPV6_DONTFRAG) || defined(IPV6_DONTFRAG)
SPRT_SAME(IPV6_DONTFRAG);
#endif
#if defined(__SPRT_IPV6_RECVTCLASS) || defined(IPV6_RECVTCLASS)
SPRT_SAME(IPV6_RECVTCLASS);
#endif
#if defined(__SPRT_IPV6_TCLASS) || defined(IPV6_TCLASS)
SPRT_SAME(IPV6_TCLASS);
#endif
#if defined(__SPRT_IPV6_AUTOFLOWLABEL) || defined(IPV6_AUTOFLOWLABEL)
SPRT_SAME(IPV6_AUTOFLOWLABEL);
#endif
#if defined(__SPRT_IPV6_ADDR_PREFERENCES) || defined(IPV6_ADDR_PREFERENCES)
SPRT_SAME(IPV6_ADDR_PREFERENCES);
#endif
#if defined(__SPRT_IPV6_MINHOPCOUNT) || defined(IPV6_MINHOPCOUNT)
SPRT_SAME(IPV6_MINHOPCOUNT);
#endif
#if defined(__SPRT_IPV6_ORIGDSTADDR) || defined(IPV6_ORIGDSTADDR)
SPRT_SAME(IPV6_ORIGDSTADDR);
#endif
#if defined(__SPRT_IPV6_RECVORIGDSTADDR) || defined(IPV6_RECVORIGDSTADDR)
SPRT_SAME(IPV6_RECVORIGDSTADDR);
#endif
#if defined(__SPRT_IPV6_TRANSPARENT) || defined(IPV6_TRANSPARENT)
SPRT_SAME(IPV6_TRANSPARENT);
#endif
#if defined(__SPRT_IPV6_UNICAST_IF) || defined(IPV6_UNICAST_IF)
SPRT_SAME(IPV6_UNICAST_IF);
#endif
#if defined(__SPRT_IPV6_RECVFRAGSIZE) || defined(IPV6_RECVFRAGSIZE)
SPRT_SAME(IPV6_RECVFRAGSIZE);
#endif
#if defined(__SPRT_IPV6_FREEBIND) || defined(IPV6_FREEBIND)
SPRT_SAME(IPV6_FREEBIND);
#endif
#if defined(__SPRT_IPV6_PROTECTION_LEVEL) || defined(IPV6_PROTECTION_LEVEL)
SPRT_SAME(IPV6_PROTECTION_LEVEL);
#endif
#if defined(__SPRT_IPV6_RECVIF) || defined(IPV6_RECVIF)
SPRT_SAME(IPV6_RECVIF);
#endif
#if defined(__SPRT_IPV6_RECVDSTADDR) || defined(IPV6_RECVDSTADDR)
SPRT_SAME(IPV6_RECVDSTADDR);
#endif
#if defined(__SPRT_IPV6_IFLIST) || defined(IPV6_IFLIST)
SPRT_SAME(IPV6_IFLIST);
#endif
#if defined(__SPRT_IPV6_ADD_IFLIST) || defined(IPV6_ADD_IFLIST)
SPRT_SAME(IPV6_ADD_IFLIST);
#endif
#if defined(__SPRT_IPV6_DEL_IFLIST) || defined(IPV6_DEL_IFLIST)
SPRT_SAME(IPV6_DEL_IFLIST);
#endif
#if defined(__SPRT_IPV6_GET_IFLIST) || defined(IPV6_GET_IFLIST)
SPRT_SAME(IPV6_GET_IFLIST);
#endif
#if defined(__SPRT_IPV6_ECN) || defined(IPV6_ECN)
SPRT_SAME(IPV6_ECN);
#endif
#if defined(__SPRT_IPV6_RECVECN) || defined(IPV6_RECVECN)
SPRT_SAME(IPV6_RECVECN);
#endif
#if defined(__SPRT_IPV6_PKTINFO_EX) || defined(IPV6_PKTINFO_EX)
SPRT_SAME(IPV6_PKTINFO_EX);
#endif
#if defined(__SPRT_IPV6_WFP_REDIRECT_RECORDS) || defined(IPV6_WFP_REDIRECT_RECORDS)
SPRT_SAME(IPV6_WFP_REDIRECT_RECORDS);
#endif
#if defined(__SPRT_IPV6_WFP_REDIRECT_CONTEXT) || defined(IPV6_WFP_REDIRECT_CONTEXT)
SPRT_SAME(IPV6_WFP_REDIRECT_CONTEXT);
#endif
#if defined(__SPRT_IPV6_NRT_INTERFACE) || defined(IPV6_NRT_INTERFACE)
SPRT_SAME(IPV6_NRT_INTERFACE);
#endif
#if defined(__SPRT_IPV6_USER_MTU) || defined(IPV6_USER_MTU)
SPRT_SAME(IPV6_USER_MTU);
#endif
#if defined(__SPRT_IPV6_ADD_MEMBERSHIP) || defined(IPV6_ADD_MEMBERSHIP)
SPRT_SAME(IPV6_ADD_MEMBERSHIP);
#endif
#if defined(__SPRT_IPV6_DROP_MEMBERSHIP) || defined(IPV6_DROP_MEMBERSHIP)
SPRT_SAME(IPV6_DROP_MEMBERSHIP);
#endif
#if defined(__SPRT_IPV6_RXHOPOPTS) || defined(IPV6_RXHOPOPTS)
SPRT_SAME(IPV6_RXHOPOPTS);
#endif
#if defined(__SPRT_IPV6_RXDSTOPTS) || defined(IPV6_RXDSTOPTS)
SPRT_SAME(IPV6_RXDSTOPTS);
#endif
#if defined(__SPRT_IPV6_PMTUDISC_DONT) || defined(IPV6_PMTUDISC_DONT)
SPRT_SAME(IPV6_PMTUDISC_DONT);
#endif
#if defined(__SPRT_IPV6_PMTUDISC_WANT) || defined(IPV6_PMTUDISC_WANT)
SPRT_SAME(IPV6_PMTUDISC_WANT);
#endif
#if defined(__SPRT_IPV6_PMTUDISC_DO) || defined(IPV6_PMTUDISC_DO)
SPRT_SAME(IPV6_PMTUDISC_DO);
#endif
#if defined(__SPRT_IPV6_PMTUDISC_PROBE) || defined(IPV6_PMTUDISC_PROBE)
SPRT_SAME(IPV6_PMTUDISC_PROBE);
#endif
#if defined(__SPRT_IPV6_PMTUDISC_INTERFACE) || defined(IPV6_PMTUDISC_INTERFACE)
SPRT_SAME(IPV6_PMTUDISC_INTERFACE);
#endif
#if defined(__SPRT_IPV6_PMTUDISC_OMIT) || defined(IPV6_PMTUDISC_OMIT)
SPRT_SAME(IPV6_PMTUDISC_OMIT);
#endif
#if defined(__SPRT_IPV6_PREFER_SRC_TMP) || defined(IPV6_PREFER_SRC_TMP)
SPRT_SAME(IPV6_PREFER_SRC_TMP);
#endif
#if defined(__SPRT_IPV6_PREFER_SRC_PUBLIC) || defined(IPV6_PREFER_SRC_PUBLIC)
SPRT_SAME(IPV6_PREFER_SRC_PUBLIC);
#endif
#if defined(__SPRT_IPV6_PREFER_SRC_PUBTMP_DEFAULT) || defined(IPV6_PREFER_SRC_PUBTMP_DEFAULT)
SPRT_SAME(IPV6_PREFER_SRC_PUBTMP_DEFAULT);
#endif
#if defined(__SPRT_IPV6_PREFER_SRC_COA) || defined(IPV6_PREFER_SRC_COA)
SPRT_SAME(IPV6_PREFER_SRC_COA);
#endif
#if defined(__SPRT_IPV6_PREFER_SRC_HOME) || defined(IPV6_PREFER_SRC_HOME)
SPRT_SAME(IPV6_PREFER_SRC_HOME);
#endif
#if defined(__SPRT_IPV6_PREFER_SRC_CGA) || defined(IPV6_PREFER_SRC_CGA)
SPRT_SAME(IPV6_PREFER_SRC_CGA);
#endif
#if defined(__SPRT_IPV6_PREFER_SRC_NONCGA) || defined(IPV6_PREFER_SRC_NONCGA)
SPRT_SAME(IPV6_PREFER_SRC_NONCGA);
#endif
#if defined(__SPRT_IPV6_RTHDR_LOOSE) || defined(IPV6_RTHDR_LOOSE)
SPRT_SAME(IPV6_RTHDR_LOOSE);
#endif
#if defined(__SPRT_IPV6_RTHDR_STRICT) || defined(IPV6_RTHDR_STRICT)
SPRT_SAME(IPV6_RTHDR_STRICT);
#endif
#if defined(__SPRT_IPV6_RTHDR_TYPE_0) || defined(IPV6_RTHDR_TYPE_0)
SPRT_SAME(IPV6_RTHDR_TYPE_0);
#endif

// --- MCAST_* group membership (Winsock macros; the EXCLUDE/INCLUDE enum is not one) ---
#if defined(__SPRT_MCAST_JOIN_GROUP) || defined(MCAST_JOIN_GROUP)
SPRT_SAME(MCAST_JOIN_GROUP);
#endif
#if defined(__SPRT_MCAST_LEAVE_GROUP) || defined(MCAST_LEAVE_GROUP)
SPRT_SAME(MCAST_LEAVE_GROUP);
#endif
#if defined(__SPRT_MCAST_BLOCK_SOURCE) || defined(MCAST_BLOCK_SOURCE)
SPRT_SAME(MCAST_BLOCK_SOURCE);
#endif
#if defined(__SPRT_MCAST_UNBLOCK_SOURCE) || defined(MCAST_UNBLOCK_SOURCE)
SPRT_SAME(MCAST_UNBLOCK_SOURCE);
#endif
#if defined(__SPRT_MCAST_JOIN_SOURCE_GROUP) || defined(MCAST_JOIN_SOURCE_GROUP)
SPRT_SAME(MCAST_JOIN_SOURCE_GROUP);
#endif
#if defined(__SPRT_MCAST_LEAVE_SOURCE_GROUP) || defined(MCAST_LEAVE_SOURCE_GROUP)
SPRT_SAME(MCAST_LEAVE_SOURCE_GROUP);
#endif
// Source-filter mode. Winsock's MULTICAST_MODE_TYPE is an enum, so as with
// PMTUD_STATE only the __SPRT_ side opens these - and they matter: Windows orders
// the enumerators INCLUDE, EXCLUDE, the reverse of Linux's numbering.
#if defined(__SPRT_MCAST_INCLUDE) || defined(MCAST_INCLUDE)
SPRT_SAME(MCAST_INCLUDE);
#endif
#if defined(__SPRT_MCAST_EXCLUDE) || defined(MCAST_EXCLUDE)
SPRT_SAME(MCAST_EXCLUDE);
#endif

// --- TCP options (IPPROTO_TCP level) -----------------------------------------
#if defined(__SPRT_TCP_AO_ADD_KEY) || defined(TCP_AO_ADD_KEY)
SPRT_SAME(TCP_AO_ADD_KEY);
#endif
#if defined(__SPRT_TCP_AO_DEL_KEY) || defined(TCP_AO_DEL_KEY)
SPRT_SAME(TCP_AO_DEL_KEY);
#endif
#if defined(__SPRT_TCP_AO_GET_KEYS) || defined(TCP_AO_GET_KEYS)
SPRT_SAME(TCP_AO_GET_KEYS);
#endif
#if defined(__SPRT_TCP_AO_INFO) || defined(TCP_AO_INFO)
SPRT_SAME(TCP_AO_INFO);
#endif
#if defined(__SPRT_TCP_AO_KEYF_EXCLUDE_OPT) || defined(TCP_AO_KEYF_EXCLUDE_OPT)
SPRT_SAME(TCP_AO_KEYF_EXCLUDE_OPT);
#endif
#if defined(__SPRT_TCP_AO_KEYF_IFINDEX) || defined(TCP_AO_KEYF_IFINDEX)
SPRT_SAME(TCP_AO_KEYF_IFINDEX);
#endif
#if defined(__SPRT_TCP_AO_MAXKEYLEN) || defined(TCP_AO_MAXKEYLEN)
SPRT_SAME(TCP_AO_MAXKEYLEN);
#endif
#if defined(__SPRT_TCP_AO_REPAIR) || defined(TCP_AO_REPAIR)
SPRT_SAME(TCP_AO_REPAIR);
#endif
#if defined(__SPRT_TCP_ATMARK) || defined(TCP_ATMARK)
SPRT_SAME(TCP_ATMARK);
#endif
#if defined(__SPRT_TCP_CA_CWR) || defined(TCP_CA_CWR)
SPRT_SAME(TCP_CA_CWR);
#endif
#if defined(__SPRT_TCP_CA_D) || defined(TCP_CA_D)
SPRT_SAME(TCP_CA_D);
#endif
#if defined(__SPRT_TCP_CA_L) || defined(TCP_CA_L)
SPRT_SAME(TCP_CA_L);
#endif
#if defined(__SPRT_TCP_CA_O) || defined(TCP_CA_O)
SPRT_SAME(TCP_CA_O);
#endif
#if defined(__SPRT_TCP_CA_R) || defined(TCP_CA_R)
SPRT_SAME(TCP_CA_R);
#endif
#if defined(__SPRT_TCP_CC_INFO) || defined(TCP_CC_INFO)
SPRT_SAME(TCP_CC_INFO);
#endif
#if defined(__SPRT_TCP_CLIENT_SND_WND) || defined(TCP_CLIENT_SND_WND)
SPRT_SAME(TCP_CLIENT_SND_WND);
#endif
#if defined(__SPRT_TCP_CM_INQ) || defined(TCP_CM_INQ)
SPRT_SAME(TCP_CM_INQ);
#endif
#if defined(__SPRT_TCP_CONGESTION) || defined(TCP_CONGESTION)
SPRT_SAME(TCP_CONGESTION);
#endif
#if defined(__SPRT_TCP_CONGESTION_ALGORITHM) || defined(TCP_CONGESTION_ALGORITHM)
SPRT_SAME(TCP_CONGESTION_ALGORITHM);
#endif
#if defined(__SPRT_TCP_CONNECTION_INFO) || defined(TCP_CONNECTION_INFO)
SPRT_SAME(TCP_CONNECTION_INFO);
#endif
#if defined(__SPRT_TCP_CONNECTIONTIMEOUT) || defined(TCP_CONNECTIONTIMEOUT)
SPRT_SAME(TCP_CONNECTIONTIMEOUT);
#endif
#if defined(__SPRT_TCP_COOKIE_IN_ALWAYS) || defined(TCP_COOKIE_IN_ALWAYS)
SPRT_SAME(TCP_COOKIE_IN_ALWAYS);
#endif
#if defined(__SPRT_TCP_COOKIE_MAX) || defined(TCP_COOKIE_MAX)
SPRT_SAME(TCP_COOKIE_MAX);
#endif
#if defined(__SPRT_TCP_COOKIE_MIN) || defined(TCP_COOKIE_MIN)
SPRT_SAME(TCP_COOKIE_MIN);
#endif
#if defined(__SPRT_TCP_COOKIE_OUT_NEVER) || defined(TCP_COOKIE_OUT_NEVER)
SPRT_SAME(TCP_COOKIE_OUT_NEVER);
#endif
#if defined(__SPRT_TCP_COOKIE_PAIR_SIZE) || defined(TCP_COOKIE_PAIR_SIZE)
SPRT_SAME(TCP_COOKIE_PAIR_SIZE);
#endif
#if defined(__SPRT_TCP_COOKIE_TRANSACTIONS) || defined(TCP_COOKIE_TRANSACTIONS)
SPRT_SAME(TCP_COOKIE_TRANSACTIONS);
#endif
#if defined(__SPRT_TCP_CORK) || defined(TCP_CORK)
SPRT_SAME(TCP_CORK);
#endif
#if defined(__SPRT_TCP_DEFER_ACCEPT) || defined(TCP_DEFER_ACCEPT)
SPRT_SAME(TCP_DEFER_ACCEPT);
#endif
#if defined(__SPRT_TCP_DELAY_FIN_ACK) || defined(TCP_DELAY_FIN_ACK)
SPRT_SAME(TCP_DELAY_FIN_ACK);
#endif
#if defined(__SPRT_TCP_ENABLE_ECN) || defined(TCP_ENABLE_ECN)
SPRT_SAME(TCP_ENABLE_ECN);
#endif
#if defined(__SPRT_TCP_EXPEDITED_1122) || defined(TCP_EXPEDITED_1122)
SPRT_SAME(TCP_EXPEDITED_1122);
#endif
#if defined(__SPRT_TCP_FAIL_CONNECT_ON_ICMP_ERROR) || defined(TCP_FAIL_CONNECT_ON_ICMP_ERROR)
SPRT_SAME(TCP_FAIL_CONNECT_ON_ICMP_ERROR);
#endif
#if defined(__SPRT_TCP_FASTOPEN) || defined(TCP_FASTOPEN)
SPRT_SAME(TCP_FASTOPEN);
#endif
#if defined(__SPRT_TCP_FASTOPEN_CONNECT) || defined(TCP_FASTOPEN_CONNECT)
SPRT_SAME(TCP_FASTOPEN_CONNECT);
#endif
#if defined(__SPRT_TCP_FASTOPEN_KEY) || defined(TCP_FASTOPEN_KEY)
SPRT_SAME(TCP_FASTOPEN_KEY);
#endif
#if defined(__SPRT_TCP_FASTOPEN_NO_COOKIE) || defined(TCP_FASTOPEN_NO_COOKIE)
SPRT_SAME(TCP_FASTOPEN_NO_COOKIE);
#endif
#if defined(__SPRT_TCP_H) || defined(TCP_H)
SPRT_SAME(TCP_H);
#endif
#if defined(__SPRT_TCP_H_) || defined(TCP_H_)
SPRT_SAME(TCP_H_);
#endif
#if defined(__SPRT_TCP_ICMP_ERROR_INFO) || defined(TCP_ICMP_ERROR_INFO)
SPRT_SAME(TCP_ICMP_ERROR_INFO);
#endif
#if defined(__SPRT_TCP_INFO) || defined(TCP_INFO)
SPRT_SAME(TCP_INFO);
#endif
#if defined(__SPRT_TCP_INITIAL_RTO) || defined(TCP_INITIAL_RTO)
SPRT_SAME(TCP_INITIAL_RTO);
#endif
#if defined(__SPRT_TCP_INITIAL_RTO_DEFAULT_MAX_SYN_RETRANSMISSIONS) || defined(TCP_INITIAL_RTO_DEFAULT_MAX_SYN_RETRANSMISSIONS)
SPRT_SAME(TCP_INITIAL_RTO_DEFAULT_MAX_SYN_RETRANSMISSIONS);
#endif
#if defined(__SPRT_TCP_INITIAL_RTO_DEFAULT_RTT) || defined(TCP_INITIAL_RTO_DEFAULT_RTT)
SPRT_SAME(TCP_INITIAL_RTO_DEFAULT_RTT);
#endif
#if defined(__SPRT_TCP_INITIAL_RTO_NO_SYN_RETRANSMISSIONS) || defined(TCP_INITIAL_RTO_NO_SYN_RETRANSMISSIONS)
SPRT_SAME(TCP_INITIAL_RTO_NO_SYN_RETRANSMISSIONS);
#endif
#if defined(__SPRT_TCP_INITIAL_RTO_UNSPECIFIED_MAX_SYN_RETRANSMISSIONS) || defined(TCP_INITIAL_RTO_UNSPECIFIED_MAX_SYN_RETRANSMISSIONS)
SPRT_SAME(TCP_INITIAL_RTO_UNSPECIFIED_MAX_SYN_RETRANSMISSIONS);
#endif
#if defined(__SPRT_TCP_INITIAL_RTO_UNSPECIFIED_RTT) || defined(TCP_INITIAL_RTO_UNSPECIFIED_RTT)
SPRT_SAME(TCP_INITIAL_RTO_UNSPECIFIED_RTT);
#endif
#if defined(__SPRT_TCP_INQ) || defined(TCP_INQ)
SPRT_SAME(TCP_INQ);
#endif
#if defined(__SPRT_TCP_IPV4) || defined(TCP_IPV4)
SPRT_SAME(TCP_IPV4);
#endif
#if defined(__SPRT_TCP_IPV6) || defined(TCP_IPV6)
SPRT_SAME(TCP_IPV6);
#endif
#if defined(__SPRT_TCP_IS_MPTCP) || defined(TCP_IS_MPTCP)
SPRT_SAME(TCP_IS_MPTCP);
#endif
#if defined(__SPRT_TCP_KEEPALIVE) || defined(TCP_KEEPALIVE)
SPRT_SAME(TCP_KEEPALIVE);
#endif
#if defined(__SPRT_TCP_KEEPCNT) || defined(TCP_KEEPCNT)
SPRT_SAME(TCP_KEEPCNT);
#endif
#if defined(__SPRT_TCP_KEEPIDLE) || defined(TCP_KEEPIDLE)
SPRT_SAME(TCP_KEEPIDLE);
#endif
#if defined(__SPRT_TCP_KEEPINTVL) || defined(TCP_KEEPINTVL)
SPRT_SAME(TCP_KEEPINTVL);
#endif
#if defined(__SPRT_TCP_LINGER2) || defined(TCP_LINGER2)
SPRT_SAME(TCP_LINGER2);
#endif
#if defined(__SPRT_TCP_MAXHLEN) || defined(TCP_MAXHLEN)
SPRT_SAME(TCP_MAXHLEN);
#endif
#if defined(__SPRT_TCP_MAXOLEN) || defined(TCP_MAXOLEN)
SPRT_SAME(TCP_MAXOLEN);
#endif
#if defined(__SPRT_TCP_MAXRT) || defined(TCP_MAXRT)
SPRT_SAME(TCP_MAXRT);
#endif
#if defined(__SPRT_TCP_MAXRTMS) || defined(TCP_MAXRTMS)
SPRT_SAME(TCP_MAXRTMS);
#endif
#if defined(__SPRT_TCP_MAX_SACK) || defined(TCP_MAX_SACK)
SPRT_SAME(TCP_MAX_SACK);
#endif
#if defined(__SPRT_TCP_MAXSEG) || defined(TCP_MAXSEG)
SPRT_SAME(TCP_MAXSEG);
#endif
#if defined(__SPRT_TCP_MAXWIN) || defined(TCP_MAXWIN)
SPRT_SAME(TCP_MAXWIN);
#endif
#if defined(__SPRT_TCP_MAX_WINSHIFT) || defined(TCP_MAX_WINSHIFT)
SPRT_SAME(TCP_MAX_WINSHIFT);
#endif
#if defined(__SPRT_TCP_MD5SIG) || defined(TCP_MD5SIG)
SPRT_SAME(TCP_MD5SIG);
#endif
#if defined(__SPRT_TCP_MD5SIG_EXT) || defined(TCP_MD5SIG_EXT)
SPRT_SAME(TCP_MD5SIG_EXT);
#endif
#if defined(__SPRT_TCP_MD5SIG_FLAG_IFINDEX) || defined(TCP_MD5SIG_FLAG_IFINDEX)
SPRT_SAME(TCP_MD5SIG_FLAG_IFINDEX);
#endif
#if defined(__SPRT_TCP_MD5SIG_FLAG_PREFIX) || defined(TCP_MD5SIG_FLAG_PREFIX)
SPRT_SAME(TCP_MD5SIG_FLAG_PREFIX);
#endif
#if defined(__SPRT_TCP_MD5SIG_MAXKEYLEN) || defined(TCP_MD5SIG_MAXKEYLEN)
SPRT_SAME(TCP_MD5SIG_MAXKEYLEN);
#endif
#if defined(__SPRT_TCP_MINMSS) || defined(TCP_MINMSS)
SPRT_SAME(TCP_MINMSS);
#endif
#if defined(__SPRT_TCP_MSS) || defined(TCP_MSS)
SPRT_SAME(TCP_MSS);
#endif
#if defined(__SPRT_TCP_MSS_DEFAULT) || defined(TCP_MSS_DEFAULT)
SPRT_SAME(TCP_MSS_DEFAULT);
#endif
#if defined(__SPRT_TCP_MSS_DESIRED) || defined(TCP_MSS_DESIRED)
SPRT_SAME(TCP_MSS_DESIRED);
#endif
#if defined(__SPRT_TCP_NODELAY) || defined(TCP_NODELAY)
SPRT_SAME(TCP_NODELAY);
#endif
#if defined(__SPRT_TCP_NOOPT) || defined(TCP_NOOPT)
SPRT_SAME(TCP_NOOPT);
#endif
#if defined(__SPRT_TCP_NOPUSH) || defined(TCP_NOPUSH)
SPRT_SAME(TCP_NOPUSH);
#endif
#if defined(__SPRT_TCP_NOSYNRETRIES) || defined(TCP_NOSYNRETRIES)
SPRT_SAME(TCP_NOSYNRETRIES);
#endif
#if defined(__SPRT_TCP_NOTSENT_LOWAT) || defined(TCP_NOTSENT_LOWAT)
SPRT_SAME(TCP_NOTSENT_LOWAT);
#endif
#if defined(__SPRT_TCP_NOURG) || defined(TCP_NOURG)
SPRT_SAME(TCP_NOURG);
#endif
#if defined(__SPRT_TCP_OFFLOAD_NO_PREFERENCE) || defined(TCP_OFFLOAD_NO_PREFERENCE)
SPRT_SAME(TCP_OFFLOAD_NO_PREFERENCE);
#endif
#if defined(__SPRT_TCP_OFFLOAD_NOT_PREFERRED) || defined(TCP_OFFLOAD_NOT_PREFERRED)
SPRT_SAME(TCP_OFFLOAD_NOT_PREFERRED);
#endif
#if defined(__SPRT_TCP_OFFLOAD_PREFERENCE) || defined(TCP_OFFLOAD_PREFERENCE)
SPRT_SAME(TCP_OFFLOAD_PREFERENCE);
#endif
#if defined(__SPRT_TCP_OFFLOAD_PREFERRED) || defined(TCP_OFFLOAD_PREFERRED)
SPRT_SAME(TCP_OFFLOAD_PREFERRED);
#endif
#if defined(__SPRT_TCP_QUEUE_SEQ) || defined(TCP_QUEUE_SEQ)
SPRT_SAME(TCP_QUEUE_SEQ);
#endif
#if defined(__SPRT_TCP_QUICKACK) || defined(TCP_QUICKACK)
SPRT_SAME(TCP_QUICKACK);
#endif
#if defined(__SPRT_TCP_RECEIVE_ZEROCOPY_FLAG_TLB_CLEAN_HINT) || defined(TCP_RECEIVE_ZEROCOPY_FLAG_TLB_CLEAN_HINT)
SPRT_SAME(TCP_RECEIVE_ZEROCOPY_FLAG_TLB_CLEAN_HINT);
#endif
#if defined(__SPRT_TCP_REPAIR) || defined(TCP_REPAIR)
SPRT_SAME(TCP_REPAIR);
#endif
#if defined(__SPRT_TCP_REPAIR_OFF) || defined(TCP_REPAIR_OFF)
SPRT_SAME(TCP_REPAIR_OFF);
#endif
#if defined(__SPRT_TCP_REPAIR_OFF_NO_WP) || defined(TCP_REPAIR_OFF_NO_WP)
SPRT_SAME(TCP_REPAIR_OFF_NO_WP);
#endif
#if defined(__SPRT_TCP_REPAIR_ON) || defined(TCP_REPAIR_ON)
SPRT_SAME(TCP_REPAIR_ON);
#endif
#if defined(__SPRT_TCP_REPAIR_OPTIONS) || defined(TCP_REPAIR_OPTIONS)
SPRT_SAME(TCP_REPAIR_OPTIONS);
#endif
#if defined(__SPRT_TCP_REPAIR_QUEUE) || defined(TCP_REPAIR_QUEUE)
SPRT_SAME(TCP_REPAIR_QUEUE);
#endif
#if defined(__SPRT_TCP_REPAIR_WINDOW) || defined(TCP_REPAIR_WINDOW)
SPRT_SAME(TCP_REPAIR_WINDOW);
#endif
#if defined(__SPRT_TCP_RXT_CONNDROPTIME) || defined(TCP_RXT_CONNDROPTIME)
SPRT_SAME(TCP_RXT_CONNDROPTIME);
#endif
#if defined(__SPRT_TCP_RXT_FINDROP) || defined(TCP_RXT_FINDROP)
SPRT_SAME(TCP_RXT_FINDROP);
#endif
#if defined(__SPRT_TCP_SAVED_SYN) || defined(TCP_SAVED_SYN)
SPRT_SAME(TCP_SAVED_SYN);
#endif
#if defined(__SPRT_TCP_SAVE_SYN) || defined(TCP_SAVE_SYN)
SPRT_SAME(TCP_SAVE_SYN);
#endif
#if defined(__SPRT_TCP_S_DATA_IN) || defined(TCP_S_DATA_IN)
SPRT_SAME(TCP_S_DATA_IN);
#endif
#if defined(__SPRT_TCP_S_DATA_OUT) || defined(TCP_S_DATA_OUT)
SPRT_SAME(TCP_S_DATA_OUT);
#endif
#if defined(__SPRT_TCP_SENDMOREACKS) || defined(TCP_SENDMOREACKS)
SPRT_SAME(TCP_SENDMOREACKS);
#endif
#if defined(__SPRT_TCP_SET_ACK_FREQUENCY) || defined(TCP_SET_ACK_FREQUENCY)
SPRT_SAME(TCP_SET_ACK_FREQUENCY);
#endif
#if defined(__SPRT_TCP_SET_ICW) || defined(TCP_SET_ICW)
SPRT_SAME(TCP_SET_ICW);
#endif
#if defined(__SPRT_TCP_STDURG) || defined(TCP_STDURG)
SPRT_SAME(TCP_STDURG);
#endif
#if defined(__SPRT_TCP_SYNCNT) || defined(TCP_SYNCNT)
SPRT_SAME(TCP_SYNCNT);
#endif
#if defined(__SPRT_TCP_THIN_DUPACK) || defined(TCP_THIN_DUPACK)
SPRT_SAME(TCP_THIN_DUPACK);
#endif
#if defined(__SPRT_TCP_THIN_LINEAR_TIMEOUTS) || defined(TCP_THIN_LINEAR_TIMEOUTS)
SPRT_SAME(TCP_THIN_LINEAR_TIMEOUTS);
#endif
#if defined(__SPRT_TCP_TIMESTAMP) || defined(TCP_TIMESTAMP)
SPRT_SAME(TCP_TIMESTAMP);
#endif
#if defined(__SPRT_TCP_TIMESTAMPS) || defined(TCP_TIMESTAMPS)
SPRT_SAME(TCP_TIMESTAMPS);
#endif
#if defined(__SPRT_TCP_TX_DELAY) || defined(TCP_TX_DELAY)
SPRT_SAME(TCP_TX_DELAY);
#endif
#if defined(__SPRT_TCP_ULP) || defined(TCP_ULP)
SPRT_SAME(TCP_ULP);
#endif
#if defined(__SPRT_TCP_USER_TIMEOUT) || defined(TCP_USER_TIMEOUT)
SPRT_SAME(TCP_USER_TIMEOUT);
#endif
#if defined(__SPRT_TCP_WINDOW_CLAMP) || defined(TCP_WINDOW_CLAMP)
SPRT_SAME(TCP_WINDOW_CLAMP);
#endif
#if defined(__SPRT_TCP_ZEROCOPY_RECEIVE) || defined(TCP_ZEROCOPY_RECEIVE)
SPRT_SAME(TCP_ZEROCOPY_RECEIVE);
#endif
