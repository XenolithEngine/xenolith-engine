/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/

// POSIX symbols the sprt wrappers reference but the qemu-armv8a flat export
// does not define: CONFIG_NET, CONFIG_LIBC_DLFCN and CONFIG_LIBC_LOCALE are
// off, so NuttX ships the headers and no libc.a bodies. Weak ENOSYS / C-locale
// stubs match the wasm port's posture (runtime/libc_impl/src/wasm/{dlfcn,socket}.cc).
// Enabling those Kconfig options later supplies strong definitions that win.
//
// Do NOT stub pipe(2): the Xenolith poll reactor still needs a real self-pipe
// (CONFIG_PIPES). The NuttX Queue reactor no longer writes that pipe — NuttX
// write() blocks once the 1 KiB buffer fills.
//
// Do NOT define stdin/stdout/stderr: NuttX <stdio.h> maps them to
// lib_get_stream(fd) macros, not FILE* globals.
//
// __cxa_thread_atexit: libc++abi drops cxa_thread_atexit.cpp when
// CMAKE_SYSTEM_NAME=Generic (neither UNIX nor FUCHSIA). NuttX is hosted-POSIX
// with pthread_key_t, so the libc++abi fallback (a key drained on thread exit)
// lives here.

// The CONFIG_* set of the export this build targets. NuttX's own headers pull it
// in internally, but the guards below need it in this TU by name: with
// CONFIG_NET the socket block must not be compiled at all.
#include <nuttx/config.h>

#include <dlfcn.h>
#include <errno.h>
#include <locale.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <syslog.h>

// The __sprt_* socket surface (struct __sprt_mmsghdr, __SPRT_CMSG_ALIGN, the
// __SPRT_ID(sendmmsg)/(recvmmsg) wrappers) for the out-of-line helpers below.
// Inside the runtime (__SPRT_BUILD) these structs carry __sprt_-prefixed tags,
// so they never collide with NuttX's own <sys/socket.h> definitions above.
#include <sprt/c/sys/__sprt_socket.h>

namespace {
const char *s_dlErr = nullptr;
} // namespace

extern "C" {

__attribute__((weak)) void *dlopen(const char * /*path*/, int /*mode*/) {
	s_dlErr = "dlopen: dynamic loading is not supported in the NuttX flat build";
	return nullptr;
}

__attribute__((weak)) void *dlsym(void * /*handle*/, const char * /*name*/) {
	s_dlErr = "dlsym: dynamic loading is not supported in the NuttX flat build";
	return nullptr;
}

__attribute__((weak)) int dlclose(void * /*handle*/) { return 0; }

__attribute__((weak)) char *dlerror(void) {
	const char *e = s_dlErr;
	s_dlErr = nullptr;
	return const_cast<char *>(e);
}

// CONFIG_NET off: NuttX ships <sys/socket.h> but no libc.a bodies, so the sprt
// wrappers would not link. These stubs fill that gap.
//
// They MUST vanish when CONFIG_NET is on. A weak definition is not "overridden"
// by a strong one in an archive: the two-stage NuttX link pulls this object into
// the engine relocatable first, which leaves `socket` defined, so the linker
// never has a reason to pull socket.o out of NuttX's libc.a. The weak ENOSYS
// body then wins and networking fails at runtime with nothing to see at link
// time. (Verified with lld: the image keeps `mov w0, #-1`.)
#ifndef CONFIG_NET

__attribute__((weak)) int socket(int, int, int) {
	errno = ENOSYS;
	return -1;
}
__attribute__((weak)) int bind(int, const struct sockaddr *, socklen_t) {
	errno = ENOSYS;
	return -1;
}
__attribute__((weak)) int listen(int, int) {
	errno = ENOSYS;
	return -1;
}
__attribute__((weak)) int connect(int, const struct sockaddr *, socklen_t) {
	errno = ENOSYS;
	return -1;
}
__attribute__((weak)) int accept(int, struct sockaddr *, socklen_t *) {
	errno = ENOSYS;
	return -1;
}
__attribute__((weak)) int getsockname(int, struct sockaddr *, socklen_t *) {
	errno = ENOSYS;
	return -1;
}
__attribute__((weak)) int getpeername(int, struct sockaddr *, socklen_t *) {
	errno = ENOSYS;
	return -1;
}
__attribute__((weak)) int setsockopt(int, int, int, const void *, socklen_t) {
	errno = ENOSYS;
	return -1;
}
__attribute__((weak)) int getsockopt(int, int, int, void *, socklen_t *) {
	errno = ENOSYS;
	return -1;
}
__attribute__((weak)) int shutdown(int, int) {
	errno = ENOSYS;
	return -1;
}
__attribute__((weak)) ssize_t send(int, const void *, size_t, int) {
	errno = ENOSYS;
	return -1;
}
__attribute__((weak)) ssize_t recv(int, void *, size_t, int) {
	errno = ENOSYS;
	return -1;
}
__attribute__((weak)) ssize_t sendto(int, const void *, size_t, int, const struct sockaddr *,
		socklen_t) {
	errno = ENOSYS;
	return -1;
}
__attribute__((weak)) ssize_t recvfrom(int, void *, size_t, int, struct sockaddr *, socklen_t *) {
	errno = ENOSYS;
	return -1;
}

#endif // !CONFIG_NET

// Out-of-line pieces of the socket surface that NuttX's libc does not carry at
// any CONFIG_NET setting. Third-party code (OpenSSL's bss_dgram.c - the BIO the
// DTLS/QUIC stack runs on - and curl) reaches them through the sprt umbrella:
// deps compile with -ffreestanding, so they take the sprt-own branch of
// include_libc/sys/socket.h, where these are declarations, not inline bodies.

// CMSG_NXTHDR() maps to an out-of-line helper in the glibc shape the umbrella
// follows. NuttX has its own __cmsg_nxthdr(), but it is a 3-argument
// `static inline` and emits no symbol, so the name is free - reached here
// through an asm label, since two extern "C" declarations of one name cannot
// differ in signature.
extern "C" struct __SPRT_CMSGHDR_NAME *__sprt_nuttx_cmsg_nxthdr(
		struct __SPRT_MSGHDR_NAME *__mhdr, struct __SPRT_CMSGHDR_NAME *__cmsg) __asm__(
		"__cmsg_nxthdr");

extern "C" struct __SPRT_CMSGHDR_NAME *__sprt_nuttx_cmsg_nxthdr(
		struct __SPRT_MSGHDR_NAME *__mhdr, struct __SPRT_CMSGHDR_NAME *__cmsg) {
	if ((__SPRT_ID(size_t))__cmsg->cmsg_len < sizeof(struct __SPRT_CMSGHDR_NAME)) {
		return nullptr;
	}

	auto *__next = (struct __SPRT_CMSGHDR_NAME *)((unsigned char *)__cmsg
			+ __SPRT_CMSG_ALIGN(__cmsg->cmsg_len));
	auto *__end = (unsigned char *)__mhdr->msg_control + __mhdr->msg_controllen;

	// Both the header itself and the aligned payload have to fit: a truncated
	// control buffer must end the walk, not hand out a header to read past.
	if ((unsigned char *)(__next + 1) > __end
			|| (unsigned char *)__next + __SPRT_CMSG_ALIGN(__next->cmsg_len) > __end) {
		return nullptr;
	}
	return __next;
}

#ifdef CONFIG_NET
// NuttX has no sendmmsg()/recvmmsg() (they are Linux extensions). The wrappers
// already emulate them by looping sendmsg()/recvmsg() on this platform - see the
// SPRT_NUTTX branch in SPRuntimeCSysSocket.cpp - but the plain-name symbol is
// only a header inline on targets whose libc provides one, so it has to be
// emitted here. Weak: a future NuttX gaining the real calls takes over.
__attribute__((weak)) int sendmmsg(SOCKET __fd, struct __SPRT_MMSGHDR_NAME *__msgvec,
		unsigned int __vlen, unsigned int __flags) {
	return __SPRT_ID(sendmmsg)(__fd, __msgvec, __vlen, __flags);
}

__attribute__((weak)) int recvmmsg(SOCKET __fd, struct __SPRT_MMSGHDR_NAME *__msgvec,
		unsigned int __vlen, unsigned int __flags, struct __SPRT_TIMESPEC_NAME *__timeout) {
	return __SPRT_ID(recvmmsg)(__fd, __msgvec, __vlen, __flags, __timeout);
}
#endif // CONFIG_NET

// BIO_s_log()'s backend. NuttX "supports" these as do-nothing MACROS in
// <syslog.h> (openlog() is documented there as not implemented), so no symbol
// exists in libc.a - while the deps, compiling against the sprt umbrella, see
// ordinary prototypes and emit calls. Drop the macros and provide the bodies;
// syslog() itself works, the identity/facility are simply ignored, exactly as
// NuttX's own macros do.
#undef openlog
#undef closelog

__attribute__((weak)) void openlog(const char * /*ident*/, int /*option*/, int /*facility*/) { }
__attribute__((weak)) void closelog(void) { }

__attribute__((weak)) locale_t newlocale(int, const char *, locale_t) { return LC_GLOBAL_LOCALE; }
__attribute__((weak)) locale_t uselocale(locale_t) { return LC_GLOBAL_LOCALE; }
__attribute__((weak)) void freelocale(locale_t) {}

} // extern "C"

// The C99 <math.h> entries NuttX libm drops are NOT stubbed here any more.
// exp2/cbrt/fdim/hypot used to be approximated in this file (exp(x*ln2),
// pow(x,1/3), fmax(x-y,0), sqrt(x*x+y*y)) — each losing precision, overflowing,
// or getting the NaN/errno edges wrong — and the rest of the gap (fma, ilogb,
// log1p, remquo, nextafter, the long double family, ...) was simply missing.
// They all come from musl now; see c/SPRuntimeCMathMusl.c.

extern "C" {

struct hostent;

static int s_h_errno = 0;
extern "C" int *__h_errno_location(void) { return &s_h_errno; }

#ifndef CONFIG_LIBC_NETDB
// Same rule as the socket block above: with CONFIG_LIBC_NETDB the resolver comes
// from NuttX's libc, and a weak stub here would shadow it.
__attribute__((weak)) struct hostent *gethostbyname(const char * /*name*/) {
	s_h_errno = 2; // HOST_NOT_FOUND
	return nullptr;
}
#endif

} // extern "C"

extern "C" {
__attribute__((weak)) char __eh_frame_start;
__attribute__((weak)) char __eh_frame_end;
__attribute__((weak)) char __eh_frame_hdr_start;
__attribute__((weak)) char __eh_frame_hdr_end;
} // extern "C"

namespace {

struct CxaAtexitEntry {
	void (*dtor)(void *);
	void *arg;
	CxaAtexitEntry *next;
};

pthread_key_t s_cxaThreadKey;
pthread_once_t s_cxaThreadOnce = PTHREAD_ONCE_INIT;

void cxaThreadAtexitDrain(void *head) {
	auto *entry = static_cast<CxaAtexitEntry *>(head);
	while (entry) {
		if (entry->dtor) {
			entry->dtor(entry->arg);
		}
		auto *next = entry->next;
		free(entry);
		entry = next;
	}
}

void cxaThreadAtexitInit() { pthread_key_create(&s_cxaThreadKey, cxaThreadAtexitDrain); }

} // namespace

extern "C" int __cxa_thread_atexit(void (*dtor)(void *), void *arg, void * /*dsoHandle*/) {
	pthread_once(&s_cxaThreadOnce, cxaThreadAtexitInit);

	auto *entry = static_cast<CxaAtexitEntry *>(malloc(sizeof(CxaAtexitEntry)));
	if (!entry) {
		return -1;
	}
	entry->dtor = dtor;
	entry->arg = arg;
	entry->next = static_cast<CxaAtexitEntry *>(pthread_getspecific(s_cxaThreadKey));
	pthread_setspecific(s_cxaThreadKey, entry);
	return 0;
}
