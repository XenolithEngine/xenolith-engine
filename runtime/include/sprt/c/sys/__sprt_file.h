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

#ifndef CORE_RUNTIME_INCLUDE_C_SYS___SPRT_FILE_H_
#define CORE_RUNTIME_INCLUDE_C_SYS___SPRT_FILE_H_

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/cross/__sprt_config.h>

// The BSD values, which glibc, musl, bionic, Darwin and Embox all share.
#define __SPRT_LOCK_SH 1
#define __SPRT_LOCK_EX 2
#define __SPRT_LOCK_NB 4
#define __SPRT_LOCK_UN 8

__SPRT_BEGIN_DECL

#if __SPRT_CONFIG_HAVE_FLOCK || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
__SPRT_CONFIG_HAVE_FLOCK_NOTICE
SPRT_API int __SPRT_ID(flock)(int, int);
#endif

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C_SYS___SPRT_FILE_H_
