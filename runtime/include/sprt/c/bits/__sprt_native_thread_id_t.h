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

#ifndef CORE_RUNTIME_INCLUDE_C_BITS___SPRT_NATIVE_THREAD_ID_T_H_
#define CORE_RUNTIME_INCLUDE_C_BITS___SPRT_NATIVE_THREAD_ID_T_H_

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/bits/__sprt_uint64_t.h>

// Native thread identity: pthread_t folded into an integer. The e2k 128-bit
// pointer mode (__ptr128__) makes pthread_t a 16-byte pointer, so the full
// value does not fit into 64 bits there (same first-branch rule as
// __sprt_uintptr_t.h).
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ > 8

typedef unsigned __int128 __SPRT_ID(native_thread_id_t);

#else

typedef __sprt_uint64_t __SPRT_ID(native_thread_id_t);

#endif

#endif /* CORE_RUNTIME_INCLUDE_C_BITS___SPRT_NATIVE_THREAD_ID_T_H_ */
