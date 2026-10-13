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
// Tau user modules: dlopen gives a shared library root linked at the module's start, from its
// preload list or DT_NEEDED (SYS-A-Preload-1); libtau knows the table, this layer is POSIX. The
// lock is recursive: a library's constructor may call dlopen. dl_iterate_phdr walks the same
// table: libunwind of the shared runtime finds unwind tables through it (RT-A-SharedRuntime-1).

#include "../../include/__impl_libc.h"

#include <sprt/c/__sprt_dlfcn.h>
#include <sprt/c/__sprt_link.h>
#include <sprt/c/cross/tau_sprt/sprt_tau.h>
#include <sprt/runtime/thread/rmutex.h>

namespace sprt {

static thread_local const char *s_dlErr = nullptr;
static rmutex s_dlLock;

} // namespace sprt

extern "C" {

void *dlopen(const char *path, int __flags) __SPRT_NOEXCEPT {
	(void)__flags;
	sprt::s_dlLock.lock();
	const char *err = nullptr;
	void *h = __sprt_tau_dl_open(path, &err);
	sprt::s_dlLock.unlock();
	if (!h) {
		sprt::s_dlErr = err;
	}
	return h;
}

void *dlsym(void *__SPRT_RESTRICT __handle, const char *__SPRT_RESTRICT __name) __SPRT_NOEXCEPT {
	if (__handle == __SPRT_RTLD_NEXT) {
		sprt::s_dlErr = "dlsym: RTLD_NEXT is not supported";
		return nullptr;
	}
	sprt::s_dlLock.lock();
	const char *err = nullptr;
	void *p = __sprt_tau_dl_sym(__handle, __name, &err);
	sprt::s_dlLock.unlock();
	if (!p) {
		sprt::s_dlErr = err;
	}
	return p;
}

int dlclose(void *__handle) __SPRT_NOEXCEPT {
	sprt::s_dlLock.lock();
	const char *err = nullptr;
	int ret = __sprt_tau_dl_close(__handle, &err);
	sprt::s_dlLock.unlock();
	if (ret != 0) {
		sprt::s_dlErr = err;
	}
	return ret;
}

char *dlerror(void) __SPRT_NOEXCEPT {
	const char *e = sprt::s_dlErr;
	sprt::s_dlErr = nullptr; // dlerror() clears the error after reporting it
	return (char *)e;
}

int dladdr(const void *__addr, __SPRT_ID(Dl_info) * __info) __SPRT_NOEXCEPT {
	sprt::s_dlLock.lock();
	const char *fname = nullptr, *sname = nullptr;
	void *fbase = nullptr, *saddr = nullptr;
	int found = __sprt_tau_dl_addr(__addr, &fname, &fbase, &sname, &saddr);
	sprt::s_dlLock.unlock();
	if (found) {
		__info->dli_fname = fname;
		__info->dli_fbase = fbase;
		__info->dli_sname = sname;
		__info->dli_saddr = saddr;
	}
	return found;
}

} // extern "C"

// The objects of the module from root's table of libraries; the table does not change, so no lock.
__SPRT_C_FUNC int __SPRT_ID(dl_iterate_phdr)(
		int (*__cb)(struct __SPRT_ID(dl_phdr_info) *, __SPRT_ID(size_t), void *), void *__data) {
	struct __SPRT_ID(dl_phdr_info) info = {};
	const void *phdr = nullptr;
	for (int i = 0; __sprt_tau_dl_object(i, (unsigned long *)&info.dlpi_addr, &info.dlpi_name, &phdr, &info.dlpi_phnum);
			++i) {
		info.dlpi_phdr = (const __SPRT_ID(Elf64_Phdr) *)phdr;
		if (int ret = __cb(&info, sizeof(info), __data)) {
			return ret;
		}
	}
	return 0;
}
