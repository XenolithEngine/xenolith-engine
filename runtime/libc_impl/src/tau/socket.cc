// IPv4 sockets of a Tau module over the network stack's client in libtau (sprt_tau.h): each
// socket is an fd whose O_NONBLOCK and timeouts pick how its calls wait. Names resolve through
// the stack; addresses and services are parsed here. select() is poll() over the fd sets.
// Included by builtin_socket.cpp.

#include "../../include/__impl_libc.h"

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/__sprt_string.h>
#include <sprt/c/sys/__sprt_ioctl.h>
#include <sprt/c/cross/tau_sprt/sprt_tau.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>

namespace sprt {

struct __tau_socket {
	__sprt_tau_sock *sock;
	int type;
	unsigned long long rcvtimeo; // ns
	unsigned long long sndtimeo;
};

static constexpr unsigned long long TAU_RESOLVE_NS = 10'000'000'000ull;
static constexpr int TAU_FIONBIO = 0x5421; // the Linux number, which the headers do not spell

static __tau_socket *__tau_sock_of(const __fd_slot *fp) { return static_cast<__tau_socket *>(fp->handle); }

static long __tau_sock_ret(long r) {
	if (r < 0) {
		__sprt_errno = (int)-r;
		return -1;
	}
	return r;
}

static unsigned __tau_sock_wait(const __fd_slot *fp, int msgFlags) {
	return ((fp->flags & __SPRT_O_NONBLOCK) || (msgFlags & MSG_DONTWAIT)) ? __SPRT_TAU_IO_NONBLOCK : 0;
}

static ssize_t __tau_sock_read(__fd_slot *fp, void *buf, size_t n, off64_t *, uint32_t) {
	auto s = __tau_sock_of(fp);
	unsigned flags = __tau_sock_wait(fp, 0);
	return __tau_sock_ret(__sprt_tau_sock_recv(s->sock, buf, n, nullptr, &flags, s->rcvtimeo));
}

static ssize_t __tau_sock_write(__fd_slot *fp, const void *buf, size_t n, off64_t *, uint32_t) {
	auto s = __tau_sock_of(fp);
	return __tau_sock_ret(__sprt_tau_sock_send(s->sock, buf, n, nullptr, __tau_sock_wait(fp, 0), s->sndtimeo));
}

static ssize_t __tau_sock_readv(__fd_slot *fp, const __SPRT_IOVEC_NAME *iov, int iovcnt) {
	ssize_t total = 0;
	for (int i = 0; i < iovcnt; ++i) {
		auto n = __tau_sock_read(fp, iov[i].iov_base, iov[i].iov_len, nullptr, 0);
		if (n < 0) {
			return total ? total : n;
		}
		total += n;
		if ((size_t)n < iov[i].iov_len) {
			break;
		}
	}
	return total;
}

static ssize_t __tau_sock_writev(__fd_slot *fp, const __SPRT_IOVEC_NAME *iov, int iovcnt) {
	ssize_t total = 0;
	for (int i = 0; i < iovcnt; ++i) {
		auto n = __tau_sock_write(fp, iov[i].iov_base, iov[i].iov_len, nullptr, 0);
		if (n < 0) {
			return total ? total : n;
		}
		total += n;
		if ((size_t)n < iov[i].iov_len) {
			break;
		}
	}
	return total;
}

static off_t __tau_sock_seek(__fd_slot *, off_t, int) {
	__sprt_errno = ESPIPE;
	return -1;
}

static int __tau_sock_stat(__fd_slot *, struct __SPRT_STAT_NAME *st) {
	__builtin_memset(st, 0, sizeof(*st));
	st->st_mode = __SPRT_S_IFSOCK | 0'777;
	return 0;
}

static int __tau_sock_close(__fd_slot *fp) {
	auto s = __tau_sock_of(fp);
	__sprt_tau_sock_close(s->sock);
	__sprt_free(s);
	fp->handle = nullptr;
	return 0;
}

static int __tau_sock_dup(__fd_slot *, int *, uint32_t) {
	__sprt_errno = ENOSYS;
	return -1;
}

static void __tau_sock_nonblock(__fd_slot *fp, bool on) {
	fp->flags = on ? (fp->flags | __SPRT_O_NONBLOCK) : (fp->flags & ~uint32_t(__SPRT_O_NONBLOCK));
}

static int __tau_sock_ioctl(__fd_slot *fp, int, int cmd, intptr_t arg, __fd_ctl_mode mode) {
	if (mode == __fd_ctl_mode::fnctl) {
		switch (cmd) {
		case __SPRT_F_GETFL: return (int)fp->flags;
		case __SPRT_F_SETFL: __tau_sock_nonblock(fp, arg & __SPRT_O_NONBLOCK); return 0;
		case __SPRT_F_GETFD:
		case __SPRT_F_SETFD: return 0;
		default: __sprt_errno = EINVAL; return -1;
		}
	}
	if (cmd == TAU_FIONBIO && arg) {
		__tau_sock_nonblock(fp, *reinterpret_cast<const int *>(arg) != 0);
		return 0;
	}
	__sprt_errno = ENOTTY;
	return -1;
}

const __fd_ops *__tau_sock_ops() {
	static __fd_ops ops = [] {
		__fd_ops o;
		o.fo_read = &__tau_sock_read;
		o.fo_write = &__tau_sock_write;
		o.fo_ioctl = &__tau_sock_ioctl;
		o.fo_dup = &__tau_sock_dup;
		o.fo_close = &__tau_sock_close;
		o.fo_readv = &__tau_sock_readv;
		o.fo_writev = &__tau_sock_writev;
		o.fo_seek = &__tau_sock_seek;
		o.fo_stat = &__tau_sock_stat;
		return o;
	}();
	return &ops;
}

static __fd_slot *__tau_sock_slot(int fd) {
	auto slot = __libc::get()->get_fd_slot(fd);
	if (!slot || !slot->handle) {
		__sprt_errno = EBADF;
		return nullptr;
	}
	if (slot->ops != __tau_sock_ops()) {
		__sprt_errno = ENOTSOCK;
		return nullptr;
	}
	return slot;
}

static int __tau_sock_fd(__sprt_tau_sock *sock, int type, bool nonblock) {
	auto s = static_cast<__tau_socket *>(__sprt_malloc(sizeof(__tau_socket)));
	if (!s) {
		__sprt_tau_sock_close(sock);
		__sprt_errno = ENOMEM;
		return -1;
	}
	*s = __tau_socket{sock, type, __SPRT_TAU_TIMEOUT_INFINITE, __SPRT_TAU_TIMEOUT_INFINITE};
	int fd = __libc::get()->create_fd(s, __tau_sock_ops(), __SPRT_O_RDWR | (nonblock ? __SPRT_O_NONBLOCK : 0), 0);
	if (fd < 0) {
		__sprt_tau_sock_close(sock);
		__sprt_free(s);
		__sprt_errno = EMFILE;
	}
	return fd;
}

static bool __tau_inet_of(const struct sockaddr *a, socklen_t len, __sprt_tau_inet &out) {
	if (!a || len < sizeof(struct sockaddr_in)) {
		__sprt_errno = EINVAL;
		return false;
	}
	if (a->sa_family != AF_INET) {
		__sprt_errno = EAFNOSUPPORT;
		return false;
	}
	struct sockaddr_in in;
	__builtin_memcpy(&in, a, sizeof(in));
	out.addr = ntohl(in.sin_addr.s_addr);
	out.port = ntohs(in.sin_port);
	return true;
}

static void __tau_inet_to(const __sprt_tau_inet &in, struct sockaddr *a, socklen_t *len) {
	if (!a || !len) {
		return;
	}
	struct sockaddr_in s;
	__builtin_memset(&s, 0, sizeof(s));
	s.sin_family = AF_INET;
	s.sin_port = htons(in.port);
	s.sin_addr.s_addr = htonl(in.addr);
	__builtin_memcpy(a, &s, *len < sizeof(s) ? *len : sizeof(s));
	*len = sizeof(s);
}

static unsigned long long __tau_timeout_ns(const struct timeval &tv) {
	if (tv.tv_sec == 0 && tv.tv_usec == 0) {
		return __SPRT_TAU_TIMEOUT_INFINITE;
	}
	return (unsigned long long)tv.tv_sec * 1'000'000'000ull + (unsigned long long)tv.tv_usec * 1'000ull;
}

static struct timeval __tau_timeval(unsigned long long ns) {
	struct timeval tv;
	__builtin_memset(&tv, 0, sizeof(tv));
	if (ns != __SPRT_TAU_TIMEOUT_INFINITE) {
		tv.tv_sec = (time_t)(ns / 1'000'000'000ull);
		tv.tv_usec = (decltype(tv.tv_usec))(ns % 1'000'000'000ull / 1'000ull);
	}
	return tv;
}

static long __tau_recv(int fd, void *buf, size_t n, int flags, struct sockaddr *addr, socklen_t *len) {
	auto slot = __tau_sock_slot(fd);
	if (!slot) {
		return -1;
	}
	auto s = __tau_sock_of(slot);
	size_t done = 0;
	for (;;) {
		unsigned f = __tau_sock_wait(slot, flags) | ((flags & MSG_PEEK) ? __SPRT_TAU_MSG_PEEK : 0);
		__sprt_tau_inet from{0, 0};
		long r = __sprt_tau_sock_recv(s->sock, static_cast<char *>(buf) + done, n - done, &from, &f, s->rcvtimeo);
		if (r < 0) {
			return done ? (long)done : __tau_sock_ret(r);
		}
		__tau_inet_to(from, addr, len);
		done += (size_t)r;
		bool more = (flags & MSG_WAITALL) && s->type == SOCK_STREAM && r > 0 && done < n && !(flags & MSG_PEEK);
		if (!more) {
			return (long)done;
		}
	}
}

// Parses a.b.c.d with each part 0-255 in decimal.
static bool __tau_parse_ipv4(const char *s, unsigned &out) {
	unsigned v = 0;
	for (int part = 0; part < 4; ++part) {
		unsigned n = 0;
		int digits = 0;
		while (*s >= '0' && *s <= '9' && digits < 4) {
			n = n * 10 + unsigned(*s++ - '0');
			++digits;
		}
		if (!digits || digits > 3 || n > 255 || (part < 3 ? *s++ != '.' : *s != 0)) {
			return false;
		}
		v = (v << 8) | n;
	}
	out = v;
	return true;
}

static int __tau_hex(char c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return -1;
}

// RFC 4291 text: groups of hex, one "::", an IPv4 tail.
static bool __tau_parse_ipv6(const char *s, uint8_t out[16]) {
	uint16_t words[8] = {};
	int count = 0, gap = -1;
	if (s[0] == ':' && s[1] == ':') {
		gap = 0;
		s += 2;
		if (!*s) {
			__builtin_memset(out, 0, 16);
			return true;
		}
	} else if (*s == ':') {
		return false;
	}
	for (;;) {
		const char *start = s;
		unsigned v = 0;
		int digits = 0;
		while (__tau_hex(*s) >= 0 && digits < 5) {
			v = (v << 4) | unsigned(__tau_hex(*s++));
			++digits;
		}
		if (*s == '.' && count <= 6) {
			unsigned v4 = 0;
			if (!__tau_parse_ipv4(start, v4)) {
				return false;
			}
			words[count++] = uint16_t(v4 >> 16);
			words[count++] = uint16_t(v4);
			break;
		}
		if (!digits || digits > 4 || count == 8) {
			return false;
		}
		words[count++] = uint16_t(v);
		if (!*s) {
			break;
		}
		if (*s++ != ':') {
			return false;
		}
		if (*s == ':') {
			if (gap >= 0) {
				return false;
			}
			gap = count;
			if (!*++s) {
				break;
			}
		}
	}
	if (gap < 0 ? count != 8 : count > 7) {
		return false;
	}
	uint16_t full[8] = {};
	int tail = gap < 0 ? 0 : count - gap;
	for (int i = 0; i < count - tail; ++i) {
		full[i] = words[i];
	}
	for (int i = 0; i < tail; ++i) {
		full[8 - tail + i] = words[gap + i];
	}
	for (int i = 0; i < 8; ++i) {
		out[2 * i] = uint8_t(full[i] >> 8);
		out[2 * i + 1] = uint8_t(full[i]);
	}
	return true;
}

static char *__tau_put_dec(char *p, unsigned v) {
	char tmp[10];
	int n = 0;
	do {
		tmp[n++] = char('0' + v % 10);
		v /= 10;
	} while (v);
	while (n) {
		*p++ = tmp[--n];
	}
	return p;
}

static void __tau_format_ipv4(unsigned v, char out[16]) {
	char *p = out;
	for (int i = 3; i >= 0; --i) {
		p = __tau_put_dec(p, (v >> (8 * i)) & 0xff);
		if (i) {
			*p++ = '.';
		}
	}
	*p = 0;
}

static void __tau_format_ipv6(const uint8_t a[16], char out[46]) {
	uint16_t w[8];
	for (int i = 0; i < 8; ++i) {
		w[i] = uint16_t(a[2 * i] << 8 | a[2 * i + 1]);
	}
	int best = -1, bestLen = 1;
	for (int i = 0; i < 8;) {
		int j = i;
		while (j < 8 && !w[j]) {
			++j;
		}
		if (j - i > bestLen) {
			best = i;
			bestLen = j - i;
		}
		i = j == i ? i + 1 : j;
	}
	char *p = out;
	bool v4 = best == 0 && (bestLen == 6 || (bestLen == 5 && w[5] == 0xffff));
	for (int i = 0; i < 8; ++i) {
		if (i == best) {
			*p++ = ':';
			if (i == 0) {
				*p++ = ':';
			}
			i += bestLen - 1;
			continue;
		}
		if (v4 && i == 6) {
			__tau_format_ipv4(unsigned(a[12]) << 24 | unsigned(a[13]) << 16 | unsigned(a[14]) << 8 | a[15], p);
			return;
		}
		static const char hex[] = "0123456789abcdef";
		bool lead = true;
		for (int k = 12; k >= 0; k -= 4) {
			unsigned d = (w[i] >> k) & 0xf;
			if (d || !lead || k == 0) {
				*p++ = hex[d];
				lead = false;
			}
		}
		if (i < 7) {
			*p++ = ':';
		}
	}
	*p = 0;
}

static int __tau_service(const char *service, int flags, unsigned &port) {
	if (!service || !*service) {
		port = 0;
		return 0;
	}
	unsigned v = 0;
	const char *s = service;
	while (*s >= '0' && *s <= '9' && v <= 65'535) {
		v = v * 10 + unsigned(*s++ - '0');
	}
	if (!*s && v <= 65'535) {
		port = v;
		return 0;
	}
	if (flags & AI_NUMERICSERV) {
		return EAI_NONAME;
	}
	static const struct {
		const char *name;
		unsigned port;
	} known[] = {{"echo", 7}, {"ftp", 21}, {"ssh", 22}, {"domain", 53}, {"http", 80}, {"ntp", 123}, {"https", 443}};
	for (auto &k : known) {
		if (__builtin_strcmp(k.name, service) == 0) {
			port = k.port;
			return 0;
		}
	}
	return EAI_SERVICE;
}

static int __tau_lookup(const char *node, int flags, unsigned &addr) {
	if (!node) {
		addr = (flags & AI_PASSIVE) ? INADDR_ANY : INADDR_LOOPBACK;
		return 0;
	}
	if (__tau_parse_ipv4(node, addr)) {
		return 0;
	}
	if (flags & AI_NUMERICHOST) {
		return EAI_NONAME;
	}
	if (__builtin_strcmp(node, "localhost") == 0) {
		addr = INADDR_LOOPBACK;
		return 0;
	}
	int r = __sprt_tau_resolve(node, &addr, TAU_RESOLVE_NS);
	if (r == 0) {
		return 0;
	}
	if (r == -__SPRT_TAU_ENOENT || r == -__SPRT_TAU_EINVAL) {
		return EAI_NONAME;
	}
	return r == -__SPRT_TAU_ENOMEM ? EAI_MEMORY : EAI_AGAIN;
}

struct __tau_addrinfo {
	struct addrinfo ai;
	struct sockaddr_in sa;
};

} // namespace sprt

using namespace sprt;

extern "C" {

SOCKET socket(int domain, int type, int protocol) noexcept {
	if (domain != AF_INET) {
		__sprt_errno = EAFNOSUPPORT;
		return -1;
	}
	int kind = type & ~(SOCK_NONBLOCK | SOCK_CLOEXEC);
	bool ok = (kind == SOCK_STREAM && (protocol == 0 || protocol == IPPROTO_TCP))
			|| (kind == SOCK_DGRAM && (protocol == 0 || protocol == IPPROTO_UDP));
	if (!ok) {
		__sprt_errno = EPROTONOSUPPORT;
		return -1;
	}
	__sprt_tau_sock *s = nullptr;
	int r = __sprt_tau_sock_open(kind == SOCK_STREAM ? __SPRT_TAU_SOCK_STREAM : __SPRT_TAU_SOCK_DGRAM, &s);
	if (r < 0) {
		return (int)__tau_sock_ret(r);
	}
	return __tau_sock_fd(s, kind, type & SOCK_NONBLOCK);
}

int socketpair(int, int, int, SOCKET[2]) noexcept {
	__sprt_errno = EOPNOTSUPP;
	return -1;
}

int bind(SOCKET fd, const struct sockaddr *addr, socklen_t len) noexcept {
	auto slot = __tau_sock_slot(fd);
	__sprt_tau_inet a;
	if (!slot || !__tau_inet_of(addr, len, a)) {
		return -1;
	}
	return (int)__tau_sock_ret(__sprt_tau_sock_bind(__tau_sock_of(slot)->sock, &a));
}

int connect(SOCKET fd, const struct sockaddr *addr, socklen_t len) noexcept {
	auto slot = __tau_sock_slot(fd);
	__sprt_tau_inet a;
	if (!slot || !__tau_inet_of(addr, len, a)) {
		return -1;
	}
	auto s = __tau_sock_of(slot);
	return (int)__tau_sock_ret(__sprt_tau_sock_connect(s->sock, &a, __tau_sock_wait(slot, 0), s->sndtimeo));
}

int listen(SOCKET fd, int backlog) noexcept {
	auto slot = __tau_sock_slot(fd);
	return slot ? (int)__tau_sock_ret(__sprt_tau_sock_listen(__tau_sock_of(slot)->sock, backlog)) : -1;
}

SOCKET accept4(SOCKET fd, struct sockaddr *addr, socklen_t *len, int flags) noexcept {
	auto slot = __tau_sock_slot(fd);
	if (!slot) {
		return -1;
	}
	auto s = __tau_sock_of(slot);
	__sprt_tau_sock *n = nullptr;
	__sprt_tau_inet peer{0, 0};
	int r = __sprt_tau_sock_accept(s->sock, __tau_sock_wait(slot, 0), s->rcvtimeo, &n, &peer);
	if (r < 0) {
		return (int)__tau_sock_ret(r);
	}
	__tau_inet_to(peer, addr, len);
	return __tau_sock_fd(n, SOCK_STREAM, flags & SOCK_NONBLOCK);
}

SOCKET accept(SOCKET fd, struct sockaddr *addr, socklen_t *len) noexcept { return accept4(fd, addr, len, 0); }

static int __tau_name(SOCKET fd, int peer, struct sockaddr *addr, socklen_t *len) {
	auto slot = __tau_sock_slot(fd);
	if (!slot) {
		return -1;
	}
	if (!addr || !len) {
		__sprt_errno = EFAULT;
		return -1;
	}
	__sprt_tau_inet a{0, 0};
	int r = __sprt_tau_sock_name(__tau_sock_of(slot)->sock, peer, &a);
	if (r < 0) {
		return (int)__tau_sock_ret(r);
	}
	__tau_inet_to(a, addr, len);
	return 0;
}

int getsockname(SOCKET fd, struct sockaddr *addr, socklen_t *len) noexcept { return __tau_name(fd, 0, addr, len); }

int getpeername(SOCKET fd, struct sockaddr *addr, socklen_t *len) noexcept { return __tau_name(fd, 1, addr, len); }

int shutdown(SOCKET fd, int how) noexcept {
	auto slot = __tau_sock_slot(fd);
	if (!slot) {
		return -1;
	}
	int bits = how == SHUT_RD ? __SPRT_TAU_SHUT_RD
			: how == SHUT_WR  ? __SPRT_TAU_SHUT_WR
			: how == SHUT_RDWR ? __SPRT_TAU_SHUT_RD | __SPRT_TAU_SHUT_WR
							   : 0;
	if (!bits) {
		__sprt_errno = EINVAL;
		return -1;
	}
	return (int)__tau_sock_ret(__sprt_tau_sock_shutdown(__tau_sock_of(slot)->sock, bits));
}

// Options the stack has no knob for are taken and kept by nobody: buffers, keepalive,
// reuse, Nagle.
int setsockopt(SOCKET fd, int level, int name, const sockdata_t *val, socklen_t len) noexcept {
	auto slot = __tau_sock_slot(fd);
	if (!slot) {
		return -1;
	}
	auto s = __tau_sock_of(slot);
	if (level == SOL_SOCKET) {
		switch (name) {
		case SO_RCVTIMEO:
		case SO_SNDTIMEO: {
			struct timeval tv;
			if (!val || len < sizeof(tv)) {
				__sprt_errno = EINVAL;
				return -1;
			}
			__builtin_memcpy(&tv, val, sizeof(tv));
			(name == SO_RCVTIMEO ? s->rcvtimeo : s->sndtimeo) = __tau_timeout_ns(tv);
			return 0;
		}
		case SO_REUSEADDR:
#ifdef SO_REUSEPORT
		case SO_REUSEPORT:
#endif
		case SO_KEEPALIVE:
		case SO_BROADCAST:
		case SO_SNDBUF:
		case SO_RCVBUF:
		case SO_LINGER:
		case SO_OOBINLINE: return 0;
		default: break;
		}
	} else if (level == IPPROTO_TCP && s->type == SOCK_STREAM) {
		return 0;
	}
	__sprt_errno = ENOPROTOOPT;
	return -1;
}

int getsockopt(SOCKET fd, int level, int name, sockdata_t *val, socklen_t *len) noexcept {
	auto slot = __tau_sock_slot(fd);
	if (!slot) {
		return -1;
	}
	auto s = __tau_sock_of(slot);
	auto putInt = [&](int v) {
		if (!val || !len || *len < sizeof(int)) {
			__sprt_errno = EINVAL;
			return -1;
		}
		__builtin_memcpy(val, &v, sizeof(v));
		*len = sizeof(v);
		return 0;
	};
	if (level == SOL_SOCKET) {
		switch (name) {
		case SO_ERROR: return putInt(-__sprt_tau_sock_error(s->sock));
		case SO_TYPE: return putInt(s->type);
		case SO_RCVTIMEO:
		case SO_SNDTIMEO: {
			struct timeval tv = __tau_timeval(name == SO_RCVTIMEO ? s->rcvtimeo : s->sndtimeo);
			if (!val || !len || *len < sizeof(tv)) {
				__sprt_errno = EINVAL;
				return -1;
			}
			__builtin_memcpy(val, &tv, sizeof(tv));
			*len = sizeof(tv);
			return 0;
		}
		default: break;
		}
	}
	__sprt_errno = ENOPROTOOPT;
	return -1;
}

ssize_t sendto(SOCKET fd, const sockdata_t *buf, size_t n, int flags, const struct sockaddr *addr, socklen_t len) noexcept {
	auto slot = __tau_sock_slot(fd);
	if (!slot) {
		return -1;
	}
	__sprt_tau_inet to;
	if (addr && !__tau_inet_of(addr, len, to)) {
		return -1;
	}
	auto s = __tau_sock_of(slot);
	return __tau_sock_ret(
			__sprt_tau_sock_send(s->sock, buf, n, addr ? &to : nullptr, __tau_sock_wait(slot, flags), s->sndtimeo));
}

ssize_t send(SOCKET fd, const sockdata_t *buf, size_t n, int flags) noexcept {
	return sendto(fd, buf, n, flags, nullptr, 0);
}

ssize_t recvfrom(SOCKET fd, sockdata_t *buf, size_t n, int flags, struct sockaddr *addr, socklen_t *len) noexcept {
	return __tau_recv(fd, buf, n, flags, addr, len);
}

ssize_t recv(SOCKET fd, sockdata_t *buf, size_t n, int flags) noexcept {
	return __tau_recv(fd, buf, n, flags, nullptr, nullptr);
}

// One datagram or one write of the gathered bytes.
ssize_t sendmsg(SOCKET fd, const struct msghdr *m, int flags) noexcept {
	size_t total = 0;
	for (size_t i = 0; i < (size_t)m->msg_iovlen; ++i) {
		total += m->msg_iov[i].iov_len;
	}
	auto buf = static_cast<char *>(__sprt_malloc(total ? total : 1));
	if (!buf) {
		__sprt_errno = ENOMEM;
		return -1;
	}
	size_t at = 0;
	for (size_t i = 0; i < (size_t)m->msg_iovlen; ++i) {
		__builtin_memcpy(buf + at, m->msg_iov[i].iov_base, m->msg_iov[i].iov_len);
		at += m->msg_iov[i].iov_len;
	}
	auto r = sendto(fd, buf, total, flags, static_cast<const struct sockaddr *>(m->msg_name), m->msg_namelen);
	__sprt_free(buf);
	return r;
}

ssize_t recvmsg(SOCKET fd, struct msghdr *m, int flags) noexcept {
	size_t total = 0;
	for (size_t i = 0; i < (size_t)m->msg_iovlen; ++i) {
		total += m->msg_iov[i].iov_len;
	}
	auto buf = static_cast<char *>(__sprt_malloc(total ? total : 1));
	if (!buf) {
		__sprt_errno = ENOMEM;
		return -1;
	}
	socklen_t namelen = m->msg_namelen;
	auto r = __tau_recv(fd, buf, total, flags, static_cast<struct sockaddr *>(m->msg_name), &namelen);
	if (r >= 0) {
		m->msg_namelen = m->msg_name ? namelen : 0;
		m->msg_controllen = 0;
		m->msg_flags = 0;
		size_t at = 0;
		for (size_t i = 0; i < (size_t)m->msg_iovlen && at < (size_t)r; ++i) {
			size_t k = (size_t)r - at < m->msg_iov[i].iov_len ? (size_t)r - at : m->msg_iov[i].iov_len;
			__builtin_memcpy(m->msg_iov[i].iov_base, buf + at, k);
			at += k;
		}
	}
	__sprt_free(buf);
	return r;
}

int sendmmsg(SOCKET, struct mmsghdr *, unsigned int, unsigned int) noexcept {
	__sprt_errno = ENOSYS;
	return -1;
}

int recvmmsg(SOCKET, struct mmsghdr *, unsigned int, unsigned int, struct timespec *) noexcept {
	__sprt_errno = ENOSYS;
	return -1;
}

struct cmsghdr *__cmsg_nxthdr(struct msghdr *, struct cmsghdr *) { return (struct cmsghdr *)0; }

// The entries the inline calls of the public headers reach (sprt/c/sys/__sprt_socket.h).
SOCKET __SPRT_ID(socket)(int d, int t, int p) { return socket(d, t, p); }
int __SPRT_ID(socketpair)(int d, int t, int p, SOCKET sv[2]) { return socketpair(d, t, p, sv); }
int __SPRT_ID(bind)(SOCKET fd, const struct __SPRT_SOCKADDR_NAME *a, __SPRT_ID(socklen_t) l) { return bind(fd, a, l); }
int __SPRT_ID(connect)(SOCKET fd, const struct __SPRT_SOCKADDR_NAME *a, __SPRT_ID(socklen_t) l) {
	return connect(fd, a, l);
}
int __SPRT_ID(listen)(SOCKET fd, int b) { return listen(fd, b); }
SOCKET __SPRT_ID(accept)(SOCKET fd, struct __SPRT_SOCKADDR_NAME *a, __SPRT_ID(socklen_t) *l) { return accept(fd, a, l); }
SOCKET __SPRT_ID(accept4)(SOCKET fd, struct __SPRT_SOCKADDR_NAME *a, __SPRT_ID(socklen_t) *l, int f) {
	return accept4(fd, a, l, f);
}
int __SPRT_ID(getsockname)(SOCKET fd, struct __SPRT_SOCKADDR_NAME *a, __SPRT_ID(socklen_t) *l) {
	return getsockname(fd, a, l);
}
int __SPRT_ID(getpeername)(SOCKET fd, struct __SPRT_SOCKADDR_NAME *a, __SPRT_ID(socklen_t) *l) {
	return getpeername(fd, a, l);
}
int __SPRT_ID(shutdown)(SOCKET fd, int how) { return shutdown(fd, how); }
int __SPRT_ID(getsockopt)(SOCKET fd, int lv, int n, sockdata_t *v, __SPRT_ID(socklen_t) *l) {
	return getsockopt(fd, lv, n, v, l);
}
int __SPRT_ID(setsockopt)(SOCKET fd, int lv, int n, const sockdata_t *v, __SPRT_ID(socklen_t) l) {
	return setsockopt(fd, lv, n, v, l);
}
socksize_t __SPRT_ID(send)(SOCKET fd, const sockdata_t *b, __SPRT_ID(size_t) n, int f) { return send(fd, b, n, f); }
socksize_t __SPRT_ID(recv)(SOCKET fd, sockdata_t *b, __SPRT_ID(size_t) n, int f) { return recv(fd, b, n, f); }
socksize_t __SPRT_ID(sendto)(SOCKET fd, const sockdata_t *b, __SPRT_ID(size_t) n, int f,
		const struct __SPRT_SOCKADDR_NAME *a, __SPRT_ID(socklen_t) l) {
	return sendto(fd, b, n, f, a, l);
}
socksize_t __SPRT_ID(recvfrom)(SOCKET fd, sockdata_t *b, __SPRT_ID(size_t) n, int f, struct __SPRT_SOCKADDR_NAME *a,
		__SPRT_ID(socklen_t) *l) {
	return recvfrom(fd, b, n, f, a, l);
}
socksize_t __SPRT_ID(sendmsg)(SOCKET fd, const struct __SPRT_MSGHDR_NAME *m, int f) { return sendmsg(fd, m, f); }
socksize_t __SPRT_ID(recvmsg)(SOCKET fd, struct __SPRT_MSGHDR_NAME *m, int f) { return recvmsg(fd, m, f); }
int __SPRT_ID(sendmmsg)(SOCKET fd, struct __SPRT_MMSGHDR_NAME *v, unsigned int n, unsigned int f) {
	return sendmmsg(fd, v, n, f);
}
int __SPRT_ID(recvmmsg)(SOCKET fd, struct __SPRT_MMSGHDR_NAME *v, unsigned int n, unsigned int f,
		struct __SPRT_TIMESPEC_NAME *t) {
	return recvmmsg(fd, v, n, f, t);
}

// --- <arpa/inet.h> ---------------------------------------------------------------

int inet_pton(int af, const char *__restrict src, void *__restrict dst) noexcept {
	if (af == AF_INET) {
		unsigned v = 0;
		if (!__tau_parse_ipv4(src, v)) {
			return 0;
		}
		uint32_t n = htonl(v);
		__builtin_memcpy(dst, &n, 4);
		return 1;
	}
	if (af == AF_INET6) {
		return __tau_parse_ipv6(src, static_cast<uint8_t *>(dst)) ? 1 : 0;
	}
	__sprt_errno = EAFNOSUPPORT;
	return -1;
}

const char *inet_ntop(int af, const void *__restrict src, char *__restrict dst, socklen_t size) noexcept {
	char text[46];
	if (af == AF_INET) {
		uint32_t n;
		__builtin_memcpy(&n, src, 4);
		__tau_format_ipv4(ntohl(n), text);
	} else if (af == AF_INET6) {
		__tau_format_ipv6(static_cast<const uint8_t *>(src), text);
	} else {
		__sprt_errno = EAFNOSUPPORT;
		return nullptr;
	}
	size_t len = __builtin_strlen(text);
	if (len >= size) {
		__sprt_errno = ENOSPC;
		return nullptr;
	}
	__builtin_memcpy(dst, text, len + 1);
	return dst;
}

int inet_aton(const char *cp, struct in_addr *inp) noexcept {
	unsigned v = 0;
	if (!__tau_parse_ipv4(cp, v)) {
		return 0;
	}
	if (inp) {
		inp->s_addr = htonl(v);
	}
	return 1;
}

in_addr_t inet_addr(const char *cp) noexcept {
	unsigned v = 0;
	return __tau_parse_ipv4(cp, v) ? htonl(v) : INADDR_NONE;
}

char *inet_ntoa(struct in_addr in) noexcept {
	static char text[16];
	__tau_format_ipv4(ntohl(in.s_addr), text);
	return text;
}

// --- <netdb.h> ------------------------------------------------------------------

static int s_h_errno = 0;
int *__h_errno_location(void) { return &s_h_errno; }

int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) {
	if (!res) {
		return EAI_FAIL;
	}
	*res = nullptr;
	int family = hints ? hints->ai_family : AF_UNSPEC;
	int flags = hints ? hints->ai_flags : 0;
	int socktype = hints ? hints->ai_socktype : 0;
	int protocol = hints ? hints->ai_protocol : 0;
	if (family != AF_UNSPEC && family != AF_INET) {
		return EAI_FAMILY;
	}
	if (socktype != 0 && socktype != SOCK_STREAM && socktype != SOCK_DGRAM) {
		return EAI_SOCKTYPE;
	}
	if (!node && !service) {
		return EAI_NONAME;
	}
	unsigned port = 0, addr = 0;
	int r = __tau_service(service, flags, port);
	r = r ? r : __tau_lookup(node, flags, addr);
	if (r) {
		return r;
	}
	const int kinds[] = {SOCK_STREAM, SOCK_DGRAM};
	struct addrinfo *first = nullptr, **next = &first;
	for (int kind : kinds) {
		if (socktype && socktype != kind) {
			continue;
		}
		auto e = static_cast<__tau_addrinfo *>(__sprt_malloc(sizeof(__tau_addrinfo)));
		if (!e) {
			freeaddrinfo(first);
			return EAI_MEMORY;
		}
		__builtin_memset(e, 0, sizeof(*e));
		e->sa.sin_family = AF_INET;
		e->sa.sin_port = htons(uint16_t(port));
		e->sa.sin_addr.s_addr = htonl(addr);
		e->ai.ai_flags = flags;
		e->ai.ai_family = AF_INET;
		e->ai.ai_socktype = kind;
		e->ai.ai_protocol = protocol ? protocol : kind == SOCK_STREAM ? IPPROTO_TCP : IPPROTO_UDP;
		e->ai.ai_addrlen = sizeof(e->sa);
		e->ai.ai_addr = reinterpret_cast<struct sockaddr *>(&e->sa);
		if (!first && (flags & AI_CANONNAME) && node) {
			size_t len = __builtin_strlen(node);
			e->ai.ai_canonname = static_cast<char *>(__sprt_malloc(len + 1));
			if (e->ai.ai_canonname) {
				__builtin_memcpy(e->ai.ai_canonname, node, len + 1);
			}
		}
		*next = &e->ai;
		next = &e->ai.ai_next;
	}
	*res = first;
	return 0;
}

void freeaddrinfo(struct addrinfo *ai) {
	while (ai) {
		struct addrinfo *next = ai->ai_next;
		if (ai->ai_canonname) {
			__sprt_free(ai->ai_canonname);
		}
		__sprt_free(ai);
		ai = next;
	}
}

int getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, socklen_t hostlen, char *serv,
		socklen_t servlen, int flags) {
	__sprt_tau_inet a;
	if (!__tau_inet_of(sa, salen, a)) {
		return EAI_FAMILY;
	}
	if (host && hostlen) {
		if (flags & NI_NAMEREQD) {
			return EAI_NONAME;
		}
		char text[16];
		__tau_format_ipv4(a.addr, text);
		size_t len = __builtin_strlen(text);
		if (len >= hostlen) {
			return EAI_OVERFLOW;
		}
		__builtin_memcpy(host, text, len + 1);
	}
	if (serv && servlen) {
		char text[8];
		*__tau_put_dec(text, a.port) = 0;
		size_t len = __builtin_strlen(text);
		if (len >= servlen) {
			return EAI_OVERFLOW;
		}
		__builtin_memcpy(serv, text, len + 1);
	}
	return 0;
}

const char *gai_strerror(int code) {
	switch (code) {
	case 0: return "Success";
	case EAI_BADFLAGS: return "Invalid flags";
	case EAI_NONAME: return "Name or service not known";
	case EAI_AGAIN: return "Temporary failure in name resolution";
	case EAI_FAIL: return "Non-recoverable failure in name resolution";
	case EAI_FAMILY: return "Address family not supported";
	case EAI_SOCKTYPE: return "Socket type not supported";
	case EAI_SERVICE: return "Service not supported for socket type";
	case EAI_MEMORY: return "Memory allocation failure";
	case EAI_SYSTEM: return "System error";
	case EAI_OVERFLOW: return "Argument buffer overflow";
	default: return "Unknown error";
	}
}

struct hostent *gethostbyname(const char *name) {
	static struct in_addr addr;
	static char *list[2];
	static char *aliases[1];
	static char hostName[256];
	static struct hostent h;
	unsigned v = 0;
	int r = __tau_lookup(name, 0, v);
	if (r) {
		s_h_errno = r == EAI_NONAME ? HOST_NOT_FOUND : TRY_AGAIN;
		return nullptr;
	}
	addr.s_addr = htonl(v);
	list[0] = reinterpret_cast<char *>(&addr);
	list[1] = nullptr;
	aliases[0] = nullptr;
	size_t len = __builtin_strlen(name);
	len = len < sizeof(hostName) - 1 ? len : sizeof(hostName) - 1;
	__builtin_memcpy(hostName, name, len);
	hostName[len] = 0;
	h.h_name = hostName;
	h.h_aliases = aliases;
	h.h_addrtype = AF_INET;
	h.h_length = 4;
	h.h_addr_list = list;
	return &h;
}

struct hostent *gethostbyaddr(const void *, socklen_t, int) {
	s_h_errno = NO_RECOVERY;
	return nullptr;
}
void sethostent(int) { }
void endhostent(void) { }

struct servent *getservbyname(const char *, const char *) { return nullptr; }
struct servent *getservbyport(int, const char *) { return nullptr; }
void setservent(int) { }
void endservent(void) { }

struct protoent *getprotobyname(const char *) { return nullptr; }
struct protoent *getprotobynumber(int) { return nullptr; }
void setprotoent(int) { }
void endprotoent(void) { }

// --- <sys/select.h> -------------------------------------------------------------

static int __tau_select(int n, fd_set *r, fd_set *w, fd_set *e, int timeoutMs) {
	if (n < 0 || n > FD_SETSIZE) {
		__sprt_errno = EINVAL;
		return -1;
	}
	int count = 0;
	for (int fd = 0; fd < n; ++fd) {
		count += (r && FD_ISSET(fd, r)) || (w && FD_ISSET(fd, w)) || (e && FD_ISSET(fd, e)) ? 1 : 0;
	}
	auto fds = static_cast<struct pollfd *>(__sprt_malloc(sizeof(struct pollfd) * (count ? count : 1)));
	if (!fds) {
		__sprt_errno = ENOMEM;
		return -1;
	}
	int k = 0;
	for (int fd = 0; fd < n; ++fd) {
		short ev = short(((r && FD_ISSET(fd, r)) ? POLLIN : 0) | ((w && FD_ISSET(fd, w)) ? POLLOUT : 0));
		if (ev || (e && FD_ISSET(fd, e))) {
			fds[k].fd = fd;
			fds[k].events = ev;
			fds[k].revents = 0;
			++k;
		}
	}
	int ready = poll(fds, (nfds_t)count, timeoutMs);
	if (ready < 0) {
		__sprt_free(fds);
		return -1;
	}
	for (int i = 0; i < count; ++i) {
		if (fds[i].revents & POLLNVAL) {
			__sprt_free(fds);
			__sprt_errno = EBADF;
			return -1;
		}
	}
	int bits = 0;
	for (int i = 0; i < count; ++i) {
		int fd = fds[i].fd;
		short rv = fds[i].revents;
		bool in = r && FD_ISSET(fd, r) && (rv & (POLLIN | POLLHUP | POLLERR));
		bool out = w && FD_ISSET(fd, w) && (rv & (POLLOUT | POLLERR));
		bool ex = e && FD_ISSET(fd, e) && (rv & POLLPRI);
		if (r && FD_ISSET(fd, r) && !in) {
			FD_CLR(fd, r);
		}
		if (w && FD_ISSET(fd, w) && !out) {
			FD_CLR(fd, w);
		}
		if (e && FD_ISSET(fd, e) && !ex) {
			FD_CLR(fd, e);
		}
		bits += (in ? 1 : 0) + (out ? 1 : 0) + (ex ? 1 : 0);
	}
	__sprt_free(fds);
	return bits;
}

int select(int n, fd_set *r, fd_set *w, fd_set *e, const struct timeval *tv) noexcept {
	int ms = tv ? int(tv->tv_sec * 1'000 + (tv->tv_usec + 999) / 1'000) : -1;
	return __tau_select(n, r, w, e, ms);
}

int pselect(int n, fd_set *r, fd_set *w, fd_set *e, const struct timespec *ts, const sigset_t *) noexcept {
	int ms = ts ? int(ts->tv_sec * 1'000 + (ts->tv_nsec + 999'999) / 1'000'000) : -1;
	return __tau_select(n, r, w, e, ms);
}

} // extern "C"
