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

// sprt overlay for <__mutex/mutex.h>: retarget std::mutex onto sprt's futex-based
// qmutex instead of libc++'s pthread __libcpp_mutex_t. Shadows the upstream header
// via the include_libc/cxx overlay (searched before libcxx/include), so every
// includer -- <mutex>, <condition_variable>, <shared_mutex> -- sees the same mutex.
//
// The out-of-line qmutex members (~qmutex, lock backend) resolve from the sprt
// runtime; this header only needs to compile against them.
//
// The sprt::qmutex swap is a CONSUMER-side feature (__SPRT_USE_STL==1). When BUILDING
// the ported STL itself -- the runtime / libcxx module, where __SPRT_USE_STL==0 -- the
// sprt runtime headers define std-owned symbols (nothrow_t, pair, ...) that would clash
// with the very libc++ being compiled, so we pass straight through to upstream <mutex>.

#include <__config>
#include <sprt/c/bits/__sprt_config.h>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

#if !defined(SPRT_STD_THREADING_SPRT)

// OPT-IN, DEFAULT OFF (see <mutex> overlay): upstream std::mutex unless
// SPRT_STD_THREADING_SPRT is defined.
#include_next <__mutex/mutex.h>

#else // sprt-backed std::mutex (opt-in via SPRT_STD_THREADING_SPRT)

#ifndef _LIBCPP___MUTEX_MUTEX_H
#define _LIBCPP___MUTEX_MUTEX_H

#include <__type_traits/is_nothrow_constructible.h>

#include <sprt/runtime/thread/qmutex.h>

#if _LIBCPP_HAS_THREADS

_LIBCPP_BEGIN_NAMESPACE_STD

class _LIBCPP_CAPABILITY("mutex") mutex {
  ::sprt::qmutex __m_;

public:
  // [thread.mutex.class]: constexpr mutex() noexcept;
  _LIBCPP_HIDE_FROM_ABI constexpr mutex() noexcept = default;

  mutex(const mutex&)            = delete;
  mutex& operator=(const mutex&) = delete;

  _LIBCPP_HIDE_FROM_ABI ~mutex() = default;

  _LIBCPP_ACQUIRE_CAPABILITY() _LIBCPP_HIDE_FROM_ABI void lock() { __m_.lock(); }
  _LIBCPP_TRY_ACQUIRE_CAPABILITY(true) _LIBCPP_HIDE_FROM_ABI bool try_lock() _NOEXCEPT {
    return __m_.try_lock();
  }
  _LIBCPP_RELEASE_CAPABILITY _LIBCPP_HIDE_FROM_ABI void unlock() _NOEXCEPT { __m_.unlock(); }

  typedef ::sprt::qmutex::native_handle_type native_handle_type;
  _LIBCPP_HIDE_FROM_ABI native_handle_type native_handle() { return __m_.native_handle(); }
};

static_assert(is_nothrow_default_constructible<mutex>::value, "the default constructor for std::mutex must be nothrow");

_LIBCPP_END_NAMESPACE_STD

#endif // _LIBCPP_HAS_THREADS

#endif // _LIBCPP___MUTEX_MUTEX_H

#endif // __SPRT_USE_STL
