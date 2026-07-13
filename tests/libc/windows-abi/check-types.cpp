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
// Static WINAPI-parity check for the Windows __SPRT_* socket *types*.
//
// The SPRT socket wrappers forward address/option structs straight through to
// native Winsock without repacking, e.g.
//
//     ::bind(fd, (const sockaddr *)sprt_addr, len);
//     ::setsockopt(fd, SOL_SOCKET, SO_LINGER, sprt_linger, sizeof(*sprt_linger));
//
// so the SPRT struct layouts (cross/windows_sprt/socket.h) MUST match Winsock's
// byte-for-byte. This TU pins them against the live SDK headers.
//
// The SPRT windows socket.h is a full winsock replacement (it also defines
// IN_ADDR, SOCKET, hostent, addrinfo, WSADATA, ...), so it cannot share global
// scope with <winsock2.h>. __SPRT_BUILD namespaces the core structs as __sprt_*
// tags; wrapping the SPRT include in a C++ namespace isolates the remaining
// helper types too, letting both live in one TU for comparison.
//
// Compile-time only; see check.sh.
// ---------------------------------------------------------------------------

#include <winsock2.h>   // native sockaddr / sockaddr_in / in_addr / linger / ...
#include <ws2tcpip.h>   // socklen_t, sockaddr_in6, in6_addr
#include <ws2ipdef.h>

// The SPRT libc's winsock-shaped types, isolated so their helper typedefs
// (IN_ADDR, SOCKET, hostent, ...) do not collide with the real winsock ones.
namespace sprt_abi {
#define __SPRT_BUILD 1
#include <sprt/c/cross/__sprt_socket.h>
#undef __SPRT_BUILD
}

#define SPRT_TYPE(t) sprt_abi::__sprt_##t

// Size + selected field offsets must match the native winsock struct.
#define SPRT_SIZE(sprt_t, native_t) \
	static_assert(sizeof(SPRT_TYPE(sprt_t)) == sizeof(::native_t), \
			"sizeof(__sprt_" #sprt_t ") != winsock " #native_t)
#define SPRT_OFFSET(sprt_t, native_t, field) \
	static_assert(__builtin_offsetof(SPRT_TYPE(sprt_t), field) \
					== __builtin_offsetof(::native_t, field), \
			"__sprt_" #sprt_t "." #field " offset != winsock " #native_t)

// === socklen_t =============================================================
static_assert(sizeof(sprt_abi::__SPRT_ID(socklen_t)) == sizeof(::socklen_t),
		"socklen_t size differs from winsock");

// === struct sockaddr =======================================================
SPRT_SIZE(sockaddr, sockaddr);
SPRT_OFFSET(sockaddr, sockaddr, sa_family);
SPRT_OFFSET(sockaddr, sockaddr, sa_data);

// === struct sockaddr_in / in_addr ==========================================
SPRT_SIZE(in_addr, in_addr);
SPRT_SIZE(sockaddr_in, sockaddr_in);
SPRT_OFFSET(sockaddr_in, sockaddr_in, sin_family);
SPRT_OFFSET(sockaddr_in, sockaddr_in, sin_port);
SPRT_OFFSET(sockaddr_in, sockaddr_in, sin_addr);
SPRT_OFFSET(sockaddr_in, sockaddr_in, sin_zero);

// === struct sockaddr_in6 / in6_addr ========================================
SPRT_SIZE(in6_addr, in6_addr);
SPRT_SIZE(sockaddr_in6, sockaddr_in6);
SPRT_OFFSET(sockaddr_in6, sockaddr_in6, sin6_family);
SPRT_OFFSET(sockaddr_in6, sockaddr_in6, sin6_port);
SPRT_OFFSET(sockaddr_in6, sockaddr_in6, sin6_flowinfo);
SPRT_OFFSET(sockaddr_in6, sockaddr_in6, sin6_addr);
SPRT_OFFSET(sockaddr_in6, sockaddr_in6, sin6_scope_id);

// === struct linger (SO_LINGER; winsock uses u_short fields) =================
SPRT_SIZE(linger, linger);
SPRT_OFFSET(linger, linger, l_onoff);
SPRT_OFFSET(linger, linger, l_linger);

// The remaining structs keep winsock's plain spelling (not __sprt_-prefixed), so
// they are reachable only through the namespace: sprt_abi::<name> vs ::<name>.
#define SPRT_RAW_SIZE(t) \
	static_assert(sizeof(sprt_abi::t) == sizeof(::t), "sizeof(" #t ") != winsock")
#define SPRT_RAW_OFFSET(t, field) \
	static_assert(__builtin_offsetof(sprt_abi::t, field) == __builtin_offsetof(::t, field), \
			#t "." #field " offset != winsock")

// === SCOPE_ID (sockaddr_in6 scope union) ===================================
SPRT_RAW_SIZE(SCOPE_ID);

// === struct sockaddr_storage ===============================================
SPRT_RAW_SIZE(sockaddr_storage);
SPRT_RAW_OFFSET(sockaddr_storage, ss_family);

// === struct hostent ========================================================
SPRT_RAW_SIZE(hostent);
SPRT_RAW_OFFSET(hostent, h_name);
SPRT_RAW_OFFSET(hostent, h_aliases);
SPRT_RAW_OFFSET(hostent, h_addrtype);
SPRT_RAW_OFFSET(hostent, h_length);
SPRT_RAW_OFFSET(hostent, h_addr_list);

// === struct netent =========================================================
SPRT_RAW_SIZE(netent);
SPRT_RAW_OFFSET(netent, n_name);
SPRT_RAW_OFFSET(netent, n_aliases);
SPRT_RAW_OFFSET(netent, n_addrtype);
SPRT_RAW_OFFSET(netent, n_net);

// === struct servent (winsock puts s_proto before s_port on Win64) ==========
SPRT_RAW_SIZE(servent);
SPRT_RAW_OFFSET(servent, s_name);
SPRT_RAW_OFFSET(servent, s_aliases);
SPRT_RAW_OFFSET(servent, s_proto);
SPRT_RAW_OFFSET(servent, s_port);

// === struct protoent =======================================================
SPRT_RAW_SIZE(protoent);
SPRT_RAW_OFFSET(protoent, p_name);
SPRT_RAW_OFFSET(protoent, p_aliases);
SPRT_RAW_OFFSET(protoent, p_proto);

// === struct addrinfo (ADDRINFOA) ===========================================
SPRT_RAW_SIZE(addrinfo);
SPRT_RAW_OFFSET(addrinfo, ai_flags);
SPRT_RAW_OFFSET(addrinfo, ai_family);
SPRT_RAW_OFFSET(addrinfo, ai_socktype);
SPRT_RAW_OFFSET(addrinfo, ai_protocol);
SPRT_RAW_OFFSET(addrinfo, ai_addrlen);
SPRT_RAW_OFFSET(addrinfo, ai_canonname);
SPRT_RAW_OFFSET(addrinfo, ai_addr);
SPRT_RAW_OFFSET(addrinfo, ai_next);

// === struct WSAData (WSADATA) ==============================================
SPRT_RAW_SIZE(WSAData);
SPRT_RAW_OFFSET(WSAData, wVersion);
SPRT_RAW_OFFSET(WSAData, wHighVersion);
SPRT_RAW_OFFSET(WSAData, iMaxSockets);
SPRT_RAW_OFFSET(WSAData, iMaxUdpDg);
SPRT_RAW_OFFSET(WSAData, lpVendorInfo);
SPRT_RAW_OFFSET(WSAData, szDescription);
SPRT_RAW_OFFSET(WSAData, szSystemStatus);

// msghdr / cmsghdr / mmsghdr have no winsock counterpart (winsock uses the
// differently shaped WSAMSG / WSABUF), so they are validated only for internal
// self-consistency by the runtime, not against the SDK here.
