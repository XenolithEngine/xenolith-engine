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

// sprt overlay for <__condition_variable/condition_variable.h>: retarget
// std::condition_variable onto sprt's futex condvar (qcondvar_base) driving the
// sprt-backed std::mutex family. Fully header-inline (no out-of-line TU): notify_*,
// wait, and the timed waits all delegate to qcondvar_base::_wait / _signal, so this
// composes with the mutex overlay's std::mutex == sprt::qmutex.
//
// std::condition_variable_any and notify_all_at_thread_exit stay upstream (the
// <condition_variable> umbrella): they use only this class's public interface.
//
// __safe_nanosecond_cast and cv_status are kept identical to upstream -- other
// headers (the umbrella, <future>) reference std::__safe_nanosecond_cast by name.

#include <__config>
#include <sprt/c/bits/__sprt_config.h>

#if !defined(SPRT_STD_THREADING_SPRT)

// OPT-IN, DEFAULT OFF (see <mutex> overlay): upstream condition_variable unless
// SPRT_STD_THREADING_SPRT is defined.
#include_next <__condition_variable/condition_variable.h>

#else // sprt-backed std::condition_variable (opt-in via SPRT_STD_THREADING_SPRT)

#ifndef _LIBCPP___CONDITION_VARIABLE_CONDITION_VARIABLE_H
#define _LIBCPP___CONDITION_VARIABLE_CONDITION_VARIABLE_H

#include <__chrono/duration.h>
#include <__chrono/steady_clock.h>
#include <__chrono/system_clock.h>
#include <__chrono/time_point.h>
#include <__config>
#include <__mutex/mutex.h>
#include <__mutex/unique_lock.h>
#include <__system_error/throw_system_error.h>
#include <__type_traits/enable_if.h>
#include <__type_traits/is_floating_point.h>
#include <__utility/move.h>
#include <limits>
#include <ratio>

#include <sprt/runtime/thread/qcondvar.h>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

_LIBCPP_PUSH_MACROS
#include <__undef_macros>

_LIBCPP_BEGIN_NAMESPACE_STD

#if _LIBCPP_HAS_THREADS

// enum class cv_status
_LIBCPP_DECLARE_STRONG_ENUM(cv_status){no_timeout, timeout};
_LIBCPP_DECLARE_STRONG_ENUM_EPILOG(cv_status)

template <class _Rep, class _Period, __enable_if_t<is_floating_point<_Rep>::value, int> = 0>
inline _LIBCPP_HIDE_FROM_ABI chrono::nanoseconds __safe_nanosecond_cast(chrono::duration<_Rep, _Period> __d) {
  using namespace chrono;
  using __ratio       = ratio_divide<_Period, nano>;
  using __ns_rep      = nanoseconds::rep;
  _Rep __result_float = __d.count() * __ratio::num / __ratio::den;

  _Rep __result_max = numeric_limits<__ns_rep>::max();
  if (__result_float >= __result_max) {
    return nanoseconds::max();
  }

  _Rep __result_min = numeric_limits<__ns_rep>::min();
  if (__result_float <= __result_min) {
    return nanoseconds::min();
  }

  return nanoseconds(static_cast<__ns_rep>(__result_float));
}

template <class _Rep, class _Period, __enable_if_t<!is_floating_point<_Rep>::value, int> = 0>
inline _LIBCPP_HIDE_FROM_ABI chrono::nanoseconds __safe_nanosecond_cast(chrono::duration<_Rep, _Period> __d) {
  using namespace chrono;
  if (__d.count() == 0) {
    return nanoseconds(0);
  }

  using __ratio         = ratio_divide<_Period, nano>;
  using __ns_rep        = nanoseconds::rep;
  __ns_rep __result_max = numeric_limits<__ns_rep>::max();
  if (__d.count() > 0 && __d.count() > __result_max / __ratio::num) {
    return nanoseconds::max();
  }

  __ns_rep __result_min = numeric_limits<__ns_rep>::min();
  if (__d.count() < 0 && __d.count() < __result_min / __ratio::num) {
    return nanoseconds::min();
  }

  __ns_rep __result = __d.count() * __ratio::num / __ratio::den;
  if (__result == 0) {
    return nanoseconds(1);
  }

  return nanoseconds(__result);
}

class condition_variable {
  ::sprt::__qcondvar_data __cv_;

  typedef ::sprt::qcondvar_base __base;
  typedef unique_lock<mutex> __unique_lock;

  // qcondvar_base adapters over std::unique_lock<std::mutex>: identity by the owned
  // std::mutex address, lock/unlock through the lock's public interface.
  static uint64_t __mutex_id(void* __m) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(static_cast<__unique_lock*>(__m)->mutex()));
  }
  static ::sprt::Status __mutex_lock(void* __m) {
    static_cast<__unique_lock*>(__m)->mutex()->lock();
    return ::sprt::Status::Ok;
  }
  static ::sprt::Status __mutex_unlock(void* __m) {
    static_cast<__unique_lock*>(__m)->mutex()->unlock();
    return ::sprt::Status::Ok;
  }

  static ::__sprt_sprt_timeout_t __to_ns(chrono::nanoseconds __ns) {
    return __ns.count() <= 0 ? ::__sprt_sprt_timeout_t(0)
                             : static_cast<::__sprt_sprt_timeout_t>(__ns.count());
  }

public:
  _LIBCPP_HIDE_FROM_ABI _LIBCPP_CONSTEXPR condition_variable() _NOEXCEPT = default;
  // Destruction is permitted once every waiter has been notified, even if they
  // have not yet returned from wait ([thread.condition]) — _destroy blocks until
  // the last woken waiter has left the epilogue that touches this object.
  _LIBCPP_HIDE_FROM_ABI ~condition_variable() { __base::_destroy(&__cv_); }

  condition_variable(const condition_variable&)            = delete;
  condition_variable& operator=(const condition_variable&) = delete;

  _LIBCPP_HIDE_FROM_ABI void notify_one() _NOEXCEPT {
    __base::_signal<__sprt_sprt_qlock_wake_one>(&__cv_, 0);
  }
  _LIBCPP_HIDE_FROM_ABI void notify_all() _NOEXCEPT {
    __base::_signal<__sprt_sprt_qlock_wake_all>(&__cv_, 0);
  }

  _LIBCPP_HIDE_FROM_ABI void wait(__unique_lock& __lk) _NOEXCEPT {
    __base::_wait<__sprt_sprt_qlock_wait, nullptr, __mutex_id, __mutex_lock, __mutex_unlock>(
        &__cv_, &__lk, nullptr, 0);
  }

  template <class _Predicate>
  _LIBCPP_HIDE_FROM_ABI void wait(__unique_lock& __lk, _Predicate __pred) {
    while (!__pred())
      wait(__lk);
  }

  _LIBCPP_HIDE_FROM_ABI cv_status __wait_for_ns(__unique_lock& __lk, chrono::nanoseconds __ns) {
    ::__sprt_sprt_timeout_t __t = __to_ns(__ns);
    ::sprt::Status __st =
        __base::_wait<__sprt_sprt_qlock_wait, __sprt_sprt_qlock_now, __mutex_id, __mutex_lock, __mutex_unlock>(
            &__cv_, &__lk, &__t, 0);
    return __st == ::sprt::Status::Timeout ? cv_status::timeout : cv_status::no_timeout;
  }

  template <class _Clock, class _Duration>
  _LIBCPP_HIDE_FROM_ABI cv_status wait_until(__unique_lock& __lk, const chrono::time_point<_Clock, _Duration>& __t) {
    typename _Clock::time_point __now = _Clock::now();
    if (__t <= __now)
      return cv_status::timeout;
    __wait_for_ns(__lk, std::__safe_nanosecond_cast(__t - __now));
    return _Clock::now() < __t ? cv_status::no_timeout : cv_status::timeout;
  }

  template <class _Clock, class _Duration, class _Predicate>
  _LIBCPP_HIDE_FROM_ABI bool
  wait_until(__unique_lock& __lk, const chrono::time_point<_Clock, _Duration>& __t, _Predicate __pred) {
    while (!__pred()) {
      if (wait_until(__lk, __t) == cv_status::timeout)
        return __pred();
    }
    return true;
  }

  template <class _Rep, class _Period>
  _LIBCPP_HIDE_FROM_ABI cv_status wait_for(__unique_lock& __lk, const chrono::duration<_Rep, _Period>& __d) {
    if (__d <= __d.zero())
      return cv_status::timeout;
    return __wait_for_ns(__lk, std::__safe_nanosecond_cast(__d));
  }

  template <class _Rep, class _Period, class _Predicate>
  _LIBCPP_HIDE_FROM_ABI bool
  wait_for(__unique_lock& __lk, const chrono::duration<_Rep, _Period>& __d, _Predicate __pred) {
    return wait_until(__lk, chrono::steady_clock::now() + __d, std::move(__pred));
  }

  typedef ::sprt::__qcondvar_data* native_handle_type;
  _LIBCPP_HIDE_FROM_ABI native_handle_type native_handle() { return &__cv_; }
};

#endif // _LIBCPP_HAS_THREADS

_LIBCPP_END_NAMESPACE_STD

_LIBCPP_POP_MACROS

#endif // _LIBCPP___CONDITION_VARIABLE_CONDITION_VARIABLE_H

#endif // __SPRT_USE_STL
