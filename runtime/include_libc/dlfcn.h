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

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <dlfcn.h>

#else

#include <sprt/c/__sprt_dlfcn.h>

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

SPRT_UMBRELLA_FUNC
int dlclose(void *h) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_dlclose(h);
}
#endif

SPRT_UMBRELLA_FUNC
char *dlerror(void) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_dlerror();
}
#endif

SPRT_UMBRELLA_FUNC
void *dlopen(const char *path, int flags) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_dlopen(path, flags);
}
#endif

SPRT_UMBRELLA_FUNC
void *dlsym(void *__SPRT_RESTRICT h, const char *__SPRT_RESTRICT sym) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_dlsym(h, sym);
}
#endif

SPRT_UMBRELLA_FUNC
int dladdr(const void *h, Dl_info *info) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_dladdr(h, info);
}
#endif

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_DLFCN_H_
