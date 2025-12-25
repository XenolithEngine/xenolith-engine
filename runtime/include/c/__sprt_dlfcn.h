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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_DLFCN_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_DLFCN_H_

#include <c/bits/__sprt_def.h>

__SPRT_BEGIN_DECL

#define __SPRT_RTLD_LAZY   1
#define __SPRT_RTLD_NOW    2
#define __SPRT_RTLD_NOLOAD 4
#define __SPRT_RTLD_NODELETE 4'096
#define __SPRT_RTLD_GLOBAL 256
#define __SPRT_RTLD_LOCAL  0

#define __SPRT_RTLD_NEXT    ((void *)-1)
#define __SPRT_RTLD_DEFAULT ((void *)0)

#define __SPRT_RTLD_DI_LINKMAP 2

typedef struct {
	const char *dli_fname;
	void *dli_fbase;
	const char *dli_sname;
	void *dli_saddr;
} __SPRT_ID(Dl_info);

SPRT_API int __SPRT_ID(dlclose)(void *);
SPRT_API char *__SPRT_ID(dlerror)(void);
SPRT_API void *__SPRT_ID(dlopen)(const char *, int);
SPRT_API void *__SPRT_ID(dlsym)(void *__SPRT_RESTRICT, const char *__SPRT_RESTRICT);
SPRT_API int __SPRT_ID(dladdr)(const void *, __SPRT_ID(Dl_info) *);

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_DLFCN_H_
