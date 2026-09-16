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

// WebAssembly POSIX socket / resolver / poll backend.
//
// The browser sandbox exposes no raw kernel socket layer, so every entry point is
// a no-op stub that fails with ENOSYS (or the resolver-specific EAI_FAIL). TLS/QUIC
// byte streams are meant to be driven through OpenSSL's memory BIOs instead of these
// syscalls; the stubs exist only so that code which references the socket BIOs (e.g.
// BIO_s_socket, BIO_s_datagram) still links and degrades predictably at run time.
// Full declarations live in <sys/socket.h> (the __SPRT_WASM block), <netdb.h> and
// <poll.h>. Included by builtin_socket.cpp on wasm, mirroring wasm/mman.cc.

#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h> // sigset_t for pselect()
#include <errno.h>

extern "C" {

// --- <sys/socket.h> -------------------------------------------------------------

// Signatures mirror the <sys/socket.h> umbrella declarations exactly (SOCKET fd/return,
// sockdata_t* buffers, and the noexcept the umbrella funcs carry), so these ENOSYS stubs
// bind to those declarations on the freestanding wasm build. On wasm SOCKET == int and
// sockdata_t == void, so the layouts are the familiar POSIX ones.
SOCKET socket(int, int, int) noexcept { errno = ENOSYS; return -1; }
int socketpair(int, int, int, SOCKET[2]) noexcept { errno = ENOSYS; return -1; }
int bind(SOCKET, const struct sockaddr *, socklen_t) noexcept { errno = ENOSYS; return -1; }
int connect(SOCKET, const struct sockaddr *, socklen_t) noexcept { errno = ENOSYS; return -1; }
int listen(SOCKET, int) noexcept { errno = ENOSYS; return -1; }
SOCKET accept(SOCKET, struct sockaddr *, socklen_t *) noexcept { errno = ENOSYS; return -1; }
SOCKET accept4(SOCKET, struct sockaddr *, socklen_t *, int) noexcept { errno = ENOSYS; return -1; }
int getsockname(SOCKET, struct sockaddr *, socklen_t *) noexcept { errno = ENOSYS; return -1; }
int getpeername(SOCKET, struct sockaddr *, socklen_t *) noexcept { errno = ENOSYS; return -1; }
int shutdown(SOCKET, int) noexcept { errno = ENOSYS; return -1; }
int getsockopt(SOCKET, int, int, sockdata_t *, socklen_t *) noexcept { errno = ENOSYS; return -1; }
int setsockopt(SOCKET, int, int, const sockdata_t *, socklen_t) noexcept {
	errno = ENOSYS;
	return -1;
}

ssize_t send(SOCKET, const sockdata_t *, size_t, int) noexcept { errno = ENOSYS; return -1; }
ssize_t recv(SOCKET, sockdata_t *, size_t, int) noexcept { errno = ENOSYS; return -1; }
ssize_t sendto(SOCKET, const sockdata_t *, size_t, int, const struct sockaddr *,
		socklen_t) noexcept {
	errno = ENOSYS;
	return -1;
}
ssize_t recvfrom(SOCKET, sockdata_t *, size_t, int, struct sockaddr *, socklen_t *) noexcept {
	errno = ENOSYS;
	return -1;
}
ssize_t sendmsg(SOCKET, const struct msghdr *, int) noexcept { errno = ENOSYS; return -1; }
ssize_t recvmsg(SOCKET, struct msghdr *, int) noexcept { errno = ENOSYS; return -1; }
int sendmmsg(SOCKET, struct mmsghdr *, unsigned int, unsigned int) noexcept {
	errno = ENOSYS;
	return -1;
}
int recvmmsg(SOCKET, struct mmsghdr *, unsigned int, unsigned int, struct timespec *) noexcept {
	errno = ENOSYS;
	return -1;
}

struct cmsghdr *__cmsg_nxthdr(struct msghdr *, struct cmsghdr *) { return (struct cmsghdr *)0; }

// --- <netdb.h> ------------------------------------------------------------------

static int s_h_errno = 0;
int *__h_errno_location(void) { return &s_h_errno; }

int getaddrinfo(const char *, const char *, const struct addrinfo *, struct addrinfo **__res) {
	if (__res) {
		*__res = (struct addrinfo *)0;
	}
	return EAI_FAIL;
}
void freeaddrinfo(struct addrinfo *) { }
int getnameinfo(const struct sockaddr *, socklen_t, char *, socklen_t, char *, socklen_t, int) {
	return EAI_FAIL;
}
const char *gai_strerror(int __ecode) {
	switch (__ecode) {
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

struct hostent *gethostbyname(const char *) {
	s_h_errno = NO_RECOVERY;
	return (struct hostent *)0;
}
struct hostent *gethostbyaddr(const void *, socklen_t, int) {
	s_h_errno = NO_RECOVERY;
	return (struct hostent *)0;
}
void sethostent(int) { }
void endhostent(void) { }

struct servent *getservbyname(const char *, const char *) { return (struct servent *)0; }
struct servent *getservbyport(int, const char *) { return (struct servent *)0; }
void setservent(int) { }
void endservent(void) { }

struct protoent *getprotobyname(const char *) { return (struct protoent *)0; }
struct protoent *getprotobynumber(int) { return (struct protoent *)0; }
void setprotoent(int) { }
void endprotoent(void) { }

// --- <poll.h> / <sys/select.h> --------------------------------------------------

int poll(struct pollfd *, nfds_t, int) noexcept { errno = ENOSYS; return -1; }

int select(int, fd_set *, fd_set *, fd_set *, const struct timeval *) noexcept {
	errno = ENOSYS;
	return -1;
}

int pselect(int, fd_set *, fd_set *, fd_set *, const struct timespec *, const sigset_t *) noexcept {
	errno = ENOSYS;
	return -1;
}

} // extern "C"
