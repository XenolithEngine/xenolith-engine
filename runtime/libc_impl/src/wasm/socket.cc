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
#include <errno.h>

extern "C" {

// --- <sys/socket.h> -------------------------------------------------------------

int socket(int, int, int) { errno = ENOSYS; return -1; }
int socketpair(int, int, int, int[2]) { errno = ENOSYS; return -1; }
int bind(int, const struct sockaddr *, socklen_t) { errno = ENOSYS; return -1; }
int connect(int, const struct sockaddr *, socklen_t) { errno = ENOSYS; return -1; }
int listen(int, int) { errno = ENOSYS; return -1; }
int accept(int, struct sockaddr *, socklen_t *) { errno = ENOSYS; return -1; }
int accept4(int, struct sockaddr *, socklen_t *, int) { errno = ENOSYS; return -1; }
int getsockname(int, struct sockaddr *, socklen_t *) { errno = ENOSYS; return -1; }
int getpeername(int, struct sockaddr *, socklen_t *) { errno = ENOSYS; return -1; }
int shutdown(int, int) { errno = ENOSYS; return -1; }
int getsockopt(int, int, int, void *, socklen_t *) { errno = ENOSYS; return -1; }
int setsockopt(int, int, int, const void *, socklen_t) { errno = ENOSYS; return -1; }

ssize_t send(int, const void *, size_t, int) { errno = ENOSYS; return -1; }
ssize_t recv(int, void *, size_t, int) { errno = ENOSYS; return -1; }
ssize_t sendto(int, const void *, size_t, int, const struct sockaddr *, socklen_t) {
	errno = ENOSYS;
	return -1;
}
ssize_t recvfrom(int, void *, size_t, int, struct sockaddr *, socklen_t *) {
	errno = ENOSYS;
	return -1;
}
ssize_t sendmsg(int, const struct msghdr *, int) { errno = ENOSYS; return -1; }
ssize_t recvmsg(int, struct msghdr *, int) { errno = ENOSYS; return -1; }
int sendmmsg(int, struct mmsghdr *, unsigned int, unsigned int) { errno = ENOSYS; return -1; }
int recvmmsg(int, struct mmsghdr *, unsigned int, unsigned int, struct timespec *) {
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

int poll(struct pollfd *, nfds_t, int) { errno = ENOSYS; return -1; }

int select(int, fd_set *, fd_set *, fd_set *, struct timeval *) { errno = ENOSYS; return -1; }

} // extern "C"
