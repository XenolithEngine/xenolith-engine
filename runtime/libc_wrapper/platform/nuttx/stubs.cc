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

#include <dlfcn.h>
#include <errno.h>
#include <locale.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/socket.h>

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

__attribute__((weak)) locale_t newlocale(int, const char *, locale_t) { return LC_GLOBAL_LOCALE; }
__attribute__((weak)) locale_t uselocale(locale_t) { return LC_GLOBAL_LOCALE; }
__attribute__((weak)) void freelocale(locale_t) {}

} // extern "C"

// NuttX libm on this board drops a handful of C99 symbols the wrappers call.
#define WEAK_STUB __attribute__((weak))

extern "C" {

WEAK_STUB double exp2(double x) { return exp(x * 6.93147180559945286227e-01); }
WEAK_STUB float exp2f(float x) { return expf(x * 6.93147180559945286227e-01f); }
WEAK_STUB long double exp2l(long double x) { return expl(x * 6.93147180559945286227e-01L); }

WEAK_STUB double cbrt(double x) {
	return (x >= 0.0) ? pow(x, 1.0 / 3.0) : -pow(-x, 1.0 / 3.0);
}
WEAK_STUB float cbrtf(float x) {
	return (x >= 0.0f) ? powf(x, 1.0f / 3.0f) : -powf(-x, 1.0f / 3.0f);
}
WEAK_STUB long double cbrtl(long double x) {
	return (x >= 0.0L) ? powl(x, 1.0L / 3.0L) : -powl(-x, 1.0L / 3.0L);
}

WEAK_STUB double fdim(double x, double y) { return fmax(x - y, 0.0); }
WEAK_STUB float fdimf(float x, float y) { return fmaxf(x - y, 0.0f); }
WEAK_STUB long double fdiml(long double x, long double y) { return fmaxl(x - y, 0.0L); }

WEAK_STUB float hypotf(float x, float y) { return (float)hypot((double)x, (double)y); }
WEAK_STUB double hypot(double x, double y) { return sqrt(x * x + y * y); }

} // extern "C"

extern "C" {

struct hostent;

static int s_h_errno = 0;
extern "C" int *__h_errno_location(void) { return &s_h_errno; }

__attribute__((weak)) struct hostent *gethostbyname(const char * /*name*/) {
	s_h_errno = 2; // HOST_NOT_FOUND
	return nullptr;
}

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
