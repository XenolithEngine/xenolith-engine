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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_NETDB_H_
#define CORE_RUNTIME_INCLUDE_LIBC_NETDB_H_

/*
	POSIX <netdb.h> - network database (name/address resolution).
	- hosted SPRT build -> forwards to the system <netdb.h> (#include_next)
	- otherwise         -> SPRT-own declarations below (Linux/musl layout).

	Name resolution is unavailable on freestanding wasm (there is no resolver), so
	the sprt libc implements these as no-op stubs: getaddrinfo()/getnameinfo() report
	EAI_FAIL and the by-name lookups return NULL. The full struct/prototype surface
	is provided so consumers (e.g. OpenSSL's socket BIOs) compile unchanged.
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <netdb.h>

#else

#include <sys/socket.h>
#include <netinet/in.h>

// On NuttX the sprt libc shim stands in for the platform libc at compile time
// (deps build against sprt include_libc, not the NuttX sysroot — see the NUTTX
// branch of common/configure.mk), so the network database type/prototype
// surface must be visible there too. The Linux/musl ABI values match NuttX's
// own <netdb.h>, so the same definitions work for both freestanding targets.
#if defined(SPRT_WASM) || defined(SPRT_HOSTED_RTOS)

struct hostent {
	char *h_name; // official name of host
	char **h_aliases; // alias list
	int h_addrtype; // host address type
	int h_length; // length of address
	char **h_addr_list; // list of addresses
};
#define h_addr h_addr_list[0] // for backward compatibility

struct netent {
	char *n_name; // official name of net
	char **n_aliases; // alias list
	int n_addrtype; // net address type
	__SPRT_ID(uint32_t) n_net; // network number
};

struct servent {
	char *s_name; // official service name
	char **s_aliases; // alias list
	int s_port; // port number
	char *s_proto; // protocol to use
};

struct protoent {
	char *p_name; // official protocol name
	char **p_aliases; // alias list
	int p_proto; // protocol number
};

struct addrinfo {
	int ai_flags;
	int ai_family;
	int ai_socktype;
	int ai_protocol;
	socklen_t ai_addrlen;
	struct __SPRT_SOCKADDR_NAME *ai_addr;
	char *ai_canonname;
	struct addrinfo *ai_next;
};

// clang-format off
// getaddrinfo() ai_flags
#define AI_PASSIVE      0x01
#define AI_CANONNAME    0x02
#define AI_NUMERICHOST  0x04
#define AI_V4MAPPED     0x08
#define AI_ALL          0x10
#define AI_ADDRCONFIG   0x20
#define AI_NUMERICSERV  0x400

// getnameinfo() flags + limits
#define NI_NUMERICHOST  0x01
#define NI_NUMERICSERV  0x02
#define NI_NOFQDN       0x04
#define NI_NAMEREQD     0x08
#define NI_DGRAM        0x10
#define NI_NUMERICSCOPE 0x100
#define NI_MAXHOST      255
#define NI_MAXSERV      32

// getaddrinfo()/getnameinfo() error codes
#define EAI_BADFLAGS   -1
#define EAI_NONAME     -2
#define EAI_AGAIN      -3
#define EAI_FAIL       -4
#define EAI_NODATA     -5
#define EAI_FAMILY     -6
#define EAI_SOCKTYPE   -7
#define EAI_SERVICE    -8
#define EAI_ADDRFAMILY -9
#define EAI_MEMORY     -10
#define EAI_SYSTEM     -11
#define EAI_OVERFLOW   -12

// legacy gethostbyname() h_errno values
#define HOST_NOT_FOUND 1
#define TRY_AGAIN      2
#define NO_RECOVERY    3
#define NO_DATA        4
#define NO_ADDRESS     NO_DATA
// clang-format on

__SPRT_BEGIN_DECL

int *__h_errno_location(void);
#define h_errno (*__h_errno_location())

int getaddrinfo(const char *__SPRT_RESTRICT __node, const char *__SPRT_RESTRICT __service,
		const struct addrinfo *__SPRT_RESTRICT __hints, struct addrinfo **__SPRT_RESTRICT __res);
void freeaddrinfo(struct addrinfo *__res);
int getnameinfo(const struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr, socklen_t __addrlen,
		char *__SPRT_RESTRICT __host, socklen_t __hostlen, char *__SPRT_RESTRICT __serv,
		socklen_t __servlen, int __flags);
const char *gai_strerror(int __ecode);

struct hostent *gethostbyname(const char *__name);
struct hostent *gethostbyaddr(const void *__addr, socklen_t __len, int __type);
void sethostent(int __stayopen);
void endhostent(void);

struct servent *getservbyname(const char *__name, const char *__proto);
struct servent *getservbyport(int __port, const char *__proto);
void setservent(int __stayopen);
void endservent(void);

struct protoent *getprotobyname(const char *__name);
struct protoent *getprotobynumber(int __proto);
void setprotoent(int __stayopen);
void endprotoent(void);

__SPRT_END_DECL

#endif // SPRT_WASM || SPRT_HOSTED_RTOS

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_NETDB_H_
