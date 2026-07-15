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

// sprt overlay for <__mutex/lock_guard.h>: sprt's [thread.lock.guard] std::lock_guard.
// A pure library wrapper over the Lockable requirements, so it composes with the
// sprt-backed std::mutex family from this overlay as well as any user mutex.

#include <sprt/c/bits/__sprt_config.h>

#if !defined(SPRT_STD_THREADING_SPRT)

// OPT-IN, DEFAULT OFF (see <mutex> overlay): upstream lock_guard unless
// SPRT_STD_THREADING_SPRT is defined.
#include_next <__mutex/lock_guard.h>

#else // sprt-backed std::lock_guard (opt-in via SPRT_STD_THREADING_SPRT)

#ifndef _LIBCPP___MUTEX_LOCK_GUARD_H
#define _LIBCPP___MUTEX_LOCK_GUARD_H

#include <__config>
#include <__mutex/tag_types.h>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

_LIBCPP_BEGIN_NAMESPACE_STD

template <class _Mutex>
class _LIBCPP_SCOPED_LOCKABLE lock_guard {
public:
  typedef _Mutex mutex_type;

private:
  mutex_type& __m_;

public:
  [[__nodiscard__]] _LIBCPP_HIDE_FROM_ABI explicit lock_guard(mutex_type& __m) _LIBCPP_ACQUIRE_CAPABILITY(__m)
      : __m_(__m) {
    __m_.lock();
  }

  [[__nodiscard__]] _LIBCPP_HIDE_FROM_ABI lock_guard(mutex_type& __m, adopt_lock_t) _LIBCPP_REQUIRES_CAPABILITY(__m)
      : __m_(__m) {}

  _LIBCPP_RELEASE_CAPABILITY _LIBCPP_HIDE_FROM_ABI ~lock_guard() { __m_.unlock(); }

  lock_guard(lock_guard const&)            = delete;
  lock_guard& operator=(lock_guard const&) = delete;
};
_LIBCPP_CTAD_SUPPORTED_FOR_TYPE(lock_guard);

_LIBCPP_END_NAMESPACE_STD

#endif // _LIBCPP___MUTEX_LOCK_GUARD_H

#endif // SPRT_STD_THREADING_SPRT
