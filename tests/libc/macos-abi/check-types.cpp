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
// sprt struct layouts <-> Darwin, for every struct that crosses the libSystem
// boundary WITHOUT repacking.
//
// The socket shims hand these to libSystem directly:
//
//     ::bind(fd, (const sockaddr *)sprt_addr, len);
//     ::setsockopt(fd, SOL_SOCKET, SO_LINGER, sprt_linger, sizeof(*sprt_linger));
//     ::sendmsg(fd, (const msghdr *)sprt_msg, flags);
//
// so size, alignment and every field offset must match byte-for-byte.
//
// NOT checked here, on purpose: struct stat, struct dirent and struct statvfs.
// sprt defines its OWN layout for those (bits/stat.h + bits/stat_data.h is 120
// bytes against Darwin's 144) and converts explicitly --  see
// convertStatFromNative() in libc_wrapper/sys/SPRuntimeCSysStat.cpp. Asserting
// equality there would be wrong, not merely redundant. What those paths need is
// field-level round-trip coverage, which is the runtime test suite's job.
//
// Compile-time only; see check.sh.
// ---------------------------------------------------------------------------

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <sys/select.h>

#define SPRT_ABI_HEADER <sprt/c/cross/__sprt_socket.h>
#define SPRT_ABI_HEADER_2 <sprt/c/cross/__sprt_fdset.h>
#include "abi_check.h"

// === the address structs ===================================================
SPRT_SIZE(__sprt_sockaddr, sockaddr);
SPRT_ALIGN(__sprt_sockaddr, sockaddr);
SPRT_FIELD(__sprt_sockaddr, sockaddr, sa_len);
SPRT_FIELD(__sprt_sockaddr, sockaddr, sa_family);
SPRT_FIELD(__sprt_sockaddr, sockaddr, sa_data);

SPRT_SIZE(__sprt_sockaddr_in, sockaddr_in);
SPRT_ALIGN(__sprt_sockaddr_in, sockaddr_in);
SPRT_FIELD(__sprt_sockaddr_in, sockaddr_in, sin_len);
SPRT_FIELD(__sprt_sockaddr_in, sockaddr_in, sin_family);
SPRT_FIELD(__sprt_sockaddr_in, sockaddr_in, sin_port);
SPRT_FIELD(__sprt_sockaddr_in, sockaddr_in, sin_addr);
SPRT_FIELD(__sprt_sockaddr_in, sockaddr_in, sin_zero);

SPRT_SIZE(__sprt_sockaddr_in6, sockaddr_in6);
SPRT_ALIGN(__sprt_sockaddr_in6, sockaddr_in6);
SPRT_FIELD(__sprt_sockaddr_in6, sockaddr_in6, sin6_len);
SPRT_FIELD(__sprt_sockaddr_in6, sockaddr_in6, sin6_family);
SPRT_FIELD(__sprt_sockaddr_in6, sockaddr_in6, sin6_port);
SPRT_FIELD(__sprt_sockaddr_in6, sockaddr_in6, sin6_flowinfo);
SPRT_FIELD(__sprt_sockaddr_in6, sockaddr_in6, sin6_addr);
SPRT_FIELD(__sprt_sockaddr_in6, sockaddr_in6, sin6_scope_id);

SPRT_SIZE(__sprt_in_addr, in_addr);
SPRT_SIZE(__sprt_in6_addr, in6_addr);
SPRT_ALIGN(__sprt_in6_addr, in6_addr);

// The `sa_len` first byte is the single most Darwin-specific thing about these
// structs -- Linux has no such field, so a table cloned from Linux would put
// sa_family at offset 0 and every bind() would see a wrong family.
static_assert(__builtin_offsetof(::sockaddr, sa_len) == 0
				&& __builtin_offsetof(::sockaddr, sa_family) == 1,
		"Darwin sockaddr must start with the BSD sa_len/sa_family pair");

// === scatter/gather and control messages ===================================
SPRT_SIZE(__sprt_iovec, iovec);
SPRT_FIELD(__sprt_iovec, iovec, iov_base);
SPRT_FIELD(__sprt_iovec, iovec, iov_len);

SPRT_SIZE(__sprt_msghdr, msghdr);
SPRT_ALIGN(__sprt_msghdr, msghdr);
SPRT_FIELD(__sprt_msghdr, msghdr, msg_name);
SPRT_FIELD(__sprt_msghdr, msghdr, msg_namelen);
SPRT_FIELD(__sprt_msghdr, msghdr, msg_iov);
SPRT_FIELD(__sprt_msghdr, msghdr, msg_iovlen);
SPRT_FIELD(__sprt_msghdr, msghdr, msg_control);
SPRT_FIELD(__sprt_msghdr, msghdr, msg_controllen);
SPRT_FIELD(__sprt_msghdr, msghdr, msg_flags);

SPRT_SIZE(__sprt_cmsghdr, cmsghdr);
SPRT_ALIGN(__sprt_cmsghdr, cmsghdr);
SPRT_FIELD(__sprt_cmsghdr, cmsghdr, cmsg_len);
SPRT_FIELD(__sprt_cmsghdr, cmsghdr, cmsg_level);
SPRT_FIELD(__sprt_cmsghdr, cmsghdr, cmsg_type);

// === socket options ========================================================
SPRT_SIZE(__sprt_linger, linger);
SPRT_FIELD(__sprt_linger, linger, l_onoff);
SPRT_FIELD(__sprt_linger, linger, l_linger);

// === select() ==============================================================
//
// fd_set is the one struct here where sprt deliberately does NOT mirror the
// native declaration: sprt uses `unsigned long fds_bits[]` (as glibc does),
// Darwin uses `__int32_t fds_bits[]`. So sizeof matches but alignof does not
// (8 vs 4), and asserting alignment equality would be asserting the wrong
// thing.
//
// The contract that actually has to hold is the *bit mapping*: select() reads
// the buffer as 32-bit words, sprt writes it as 64-bit words, and the two agree
// only because Darwin is little-endian -- there, fd N lands in byte N/8, bit
// N%8, whichever word width you slice it with. That is what is pinned.
SPRT_SIZE(__sprt_fd_set, fd_set);
SPRT_CONST(FD_SETSIZE);
static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__,
		"sprt's 64-bit fds_bits is only bit-compatible with Darwin's 32-bit "
		"fds_bits on a little-endian target");
static_assert(alignof(sprt_abi::__sprt_fd_set) >= alignof(::fd_set),
		"sprt's fd_set must be at least as strictly aligned as Darwin's");
static_assert(sizeof(sprt_abi::__sprt_fd_mask) * 8 == 64
				&& sizeof(::fd_mask) * 8 == 32,
		"the fds_bits word widths changed -- re-check the bit-mapping argument above");

// === the scalar typedefs every layout above rests on =======================
SPRT_TYPE_SIZE(__sprt_socklen_t, socklen_t);
SPRT_TYPE_SIGN(__sprt_socklen_t, socklen_t);
SPRT_TYPE_SIZE(__sprt_sa_family_t, sa_family_t);
SPRT_TYPE_SIGN(__sprt_sa_family_t, sa_family_t);
SPRT_TYPE_SIZE(__sprt_in_port_t, in_port_t);
SPRT_TYPE_SIZE(__sprt_in_addr_t, in_addr_t);
