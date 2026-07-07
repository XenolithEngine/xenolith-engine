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

// Minimal libc++-internal <__thread/support.h> shim for building libc++abi
// against the sprt STL. libc++abi includes it unconditionally, but on wasm the
// exception storage / statics use the compiler's `thread_local`, so the
// __libcpp_tls_* / __libcpp_mutex_* helpers below sit on the dead branch and are
// provided only so the header compiles. They map onto the sprt pthread layer
// (uncontended fast paths) rather than doing real work.
#ifndef RUNTIME_INCLUDE_LIBC_STL___THREAD_SUPPORT_H_
#define RUNTIME_INCLUDE_LIBC_STL___THREAD_SUPPORT_H_

#include <sprt/c/__sprt_pthread.h>

namespace std {

// --- thread-local storage keys ---
using __libcpp_tls_key = __SPRT_ID(pthread_key_t);

inline int __libcpp_tls_create(__libcpp_tls_key *__key, void (*__at_exit)(void *)) {
	return __sprt_pthread_key_create(__key, __at_exit);
}
inline void *__libcpp_tls_get(__libcpp_tls_key __key) {
	return __sprt_pthread_getspecific(__key);
}
inline int __libcpp_tls_set(__libcpp_tls_key __key, void *__p) {
	return __sprt_pthread_setspecific(__key, __p);
}

// --- mutexes (used by cxa_guard's non-thread_local fallback) ---
using __libcpp_mutex_t = __SPRT_ID(pthread_mutex_t);
#define _LIBCPP_MUTEX_INITIALIZER {}

inline int __libcpp_mutex_lock(__libcpp_mutex_t *__m) { return __sprt_pthread_mutex_lock(__m); }
inline int __libcpp_mutex_unlock(__libcpp_mutex_t *__m) { return __sprt_pthread_mutex_unlock(__m); }
inline bool __libcpp_mutex_trylock(__libcpp_mutex_t *__m) {
	return __sprt_pthread_mutex_trylock(__m) == 0;
}

} // namespace std

#endif // RUNTIME_INCLUDE_LIBC_STL___THREAD_SUPPORT_H_
