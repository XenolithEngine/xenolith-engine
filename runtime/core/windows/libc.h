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

#ifndef RUNTIME_CORE_WINDOWS_LIBC_H_
#define RUNTIME_CORE_WINDOWS_LIBC_H_

#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_time.h>
#include <sprt/c/__sprt_string.h>
#include <sprt/c/__sprt_dlfcn.h>

extern "C" {
typedef __SPRT_ID(Dl_info) Dl_info;

extern __SPRT_ID(FILE) * stdin;
extern __SPRT_ID(FILE) * stdout;
extern __SPRT_ID(FILE) * stderr;

size_t fread(void *__restrict, size_t, size_t, __SPRT_ID(FILE) *__restrict);
size_t fwrite(const void *__restrict, size_t, size_t, __SPRT_ID(FILE) *__restrict);

int fcntl(int __fd, int __cmd, ...);

void perror(const char *);

int fflush(__SPRT_ID(FILE) *);

int dlclose(void *h);
char *dlerror(void);

void *dlopen(const char *path, int flags);

void *dlsym(void *__SPRT_RESTRICT h, const char *__SPRT_RESTRICT sym);

int dladdr(const void *h, Dl_info *info);

int usleep(__SPRT_ID(time_t));

double log2(double);
float log2f(float);
long double log2l(long double);

char *strerror(int);

int strerror_r(int err, char *buf, size_t buflen);

void qsort(void *a, size_t b, size_t c, int (*value)(const void *, const void *));

void *malloc(size_t value);
void *calloc(size_t a, size_t value);
void *realloc(void *ptr, size_t value);
void free(void *value);
void free_sized(void *value, size_t s);
void free_aligned_sized(void *value, size_t al, size_t s);

void *local_alloc(size_t value);
void local_free(void *value, size_t s);

void abort();
}

#endif // RUNTIME_CORE_WINDOWS_LIBC_H_
