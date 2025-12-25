/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_DLFCN_H_
#define CORE_RUNTIME_INCLUDE_LIBC_DLFCN_H_

#ifdef __SPRT_BUILD

#include_next <dlfcn.h>

#else

#include <c/__sprt_dlfcn.h>

#define RTLD_LAZY __SPRT_RTLD_LAZY
#define RTLD_NOW __SPRT_RTLD_NOW
#define RTLD_NOLOAD __SPRT_RTLD_NOLOAD
#define RTLD_NODELETE __SPRT_RTLD_NODELETE
#define RTLD_GLOBAL __SPRT_RTLD_GLOBAL
#define RTLD_LOCAL __SPRT_RTLD_LOCAL
#define RTLD_NEXT __SPRT_RTLD_NEXT
#define RTLD_DEFAULT __SPRT_RTLD_DEFAULT
#define RTLD_DI_LINKMAP __SPRT_RTLD_DI_LINKMAP

__SPRT_BEGIN_DECL

typedef __SPRT_ID(Dl_info) Dl_info;

SPRT_FORCEINLINE inline int dlclose(void *h) { return __sprt_dlclose(h); }
SPRT_FORCEINLINE inline char *dlerror(void) { return __sprt_dlerror(); }

SPRT_FORCEINLINE inline void *dlopen(const char *path, int flags) {
	return __sprt_dlopen(path, flags);
}

SPRT_FORCEINLINE inline void *dlsym(void *__SPRT_RESTRICT h, const char *__SPRT_RESTRICT sym) {
	return __sprt_dlsym(h, sym);
}

SPRT_FORCEINLINE inline int dladdr(const void *h, Dl_info *info) { return __sprt_dladdr(h, info); }

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_DLFCN_H_
