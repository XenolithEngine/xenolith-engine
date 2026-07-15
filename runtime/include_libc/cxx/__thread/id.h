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

// sprt overlay for <__thread/id.h>: std::__thread_id and this_thread::get_id()
// retargeted onto sprt's native thread id (sprt::thread::id, a __sprt_pid_t), so
// thread::get_id() (sprt::thread-backed, see <__thread/thread.h>) and
// this_thread::get_id() share one id source and always agree.
//
// The id is an integral type, so __get_underlying_id() keeps satisfying the
// upstream formatter<thread::id> (kept as-is), and ordering is the natural one.

#include <__config>
#include <sprt/c/bits/__sprt_config.h>

#if !defined(SPRT_STD_THREADING_SPRT)

// OPT-IN, DEFAULT OFF (see <mutex> overlay): upstream thread id unless
// SPRT_STD_THREADING_SPRT is defined.
#include_next <__thread/id.h>

#else // sprt-backed std::thread::id (opt-in via SPRT_STD_THREADING_SPRT)

#ifndef _LIBCPP___THREAD_ID_H
#define _LIBCPP___THREAD_ID_H

#include <__compare/ordering.h>
#include <__config>
#include <__fwd/functional.h>
#include <__fwd/ostream.h>

#include <sprt/cxx/thread> // sprt::thread::id, sprt::this_thread::get_id

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

_LIBCPP_BEGIN_NAMESPACE_STD

#if _LIBCPP_HAS_THREADS
class _LIBCPP_EXPORTED_FROM_ABI __thread_id;

namespace this_thread {

_LIBCPP_HIDE_FROM_ABI __thread_id get_id() _NOEXCEPT;

} // namespace this_thread

template <>
struct hash<__thread_id>;

class __thread_id {
  typedef ::sprt::thread::id::__type __id_type; // __sprt_pid_t (integral)
  __id_type __id_;

  static _LIBCPP_HIDE_FROM_ABI bool __lt_impl(__thread_id __x, __thread_id __y) _NOEXCEPT {
    // id == 0 is always less than any other thread id
    if (__x.__id_ == 0)
      return __y.__id_ != 0;
    if (__y.__id_ == 0)
      return false;
    return __x.__id_ < __y.__id_;
  }

public:
  _LIBCPP_HIDE_FROM_ABI __thread_id() _NOEXCEPT : __id_(0) {}

  _LIBCPP_HIDE_FROM_ABI void __reset() { __id_ = 0; }

  friend _LIBCPP_HIDE_FROM_ABI bool operator==(__thread_id __x, __thread_id __y) _NOEXCEPT;
#  if _LIBCPP_STD_VER <= 17
  friend _LIBCPP_HIDE_FROM_ABI bool operator<(__thread_id __x, __thread_id __y) _NOEXCEPT;
#  else
  friend _LIBCPP_HIDE_FROM_ABI strong_ordering operator<=>(__thread_id __x, __thread_id __y) noexcept;
#  endif

  template <class _CharT, class _Traits>
  friend _LIBCPP_HIDE_FROM_ABI basic_ostream<_CharT, _Traits>&
  operator<<(basic_ostream<_CharT, _Traits>& __os, __thread_id __id);

private:
  _LIBCPP_HIDE_FROM_ABI __thread_id(__id_type __id) : __id_(__id) {}

  _LIBCPP_HIDE_FROM_ABI friend __id_type __get_underlying_id(const __thread_id __id) { return __id.__id_; }

  friend __thread_id this_thread::get_id() _NOEXCEPT;
  friend class _LIBCPP_EXPORTED_FROM_ABI thread;
  friend struct hash<__thread_id>;
};

inline _LIBCPP_HIDE_FROM_ABI bool operator==(__thread_id __x, __thread_id __y) _NOEXCEPT {
  return __x.__id_ == __y.__id_;
}

#  if _LIBCPP_STD_VER <= 17

inline _LIBCPP_HIDE_FROM_ABI bool operator!=(__thread_id __x, __thread_id __y) _NOEXCEPT { return !(__x == __y); }

inline _LIBCPP_HIDE_FROM_ABI bool operator<(__thread_id __x, __thread_id __y) _NOEXCEPT {
  return __thread_id::__lt_impl(__x, __y);
}

inline _LIBCPP_HIDE_FROM_ABI bool operator<=(__thread_id __x, __thread_id __y) _NOEXCEPT { return !(__y < __x); }
inline _LIBCPP_HIDE_FROM_ABI bool operator>(__thread_id __x, __thread_id __y) _NOEXCEPT { return __y < __x; }
inline _LIBCPP_HIDE_FROM_ABI bool operator>=(__thread_id __x, __thread_id __y) _NOEXCEPT { return !(__x < __y); }

#  else // _LIBCPP_STD_VER <= 17

inline _LIBCPP_HIDE_FROM_ABI strong_ordering operator<=>(__thread_id __x, __thread_id __y) noexcept {
  if (__x == __y)
    return strong_ordering::equal;
  if (__thread_id::__lt_impl(__x, __y))
    return strong_ordering::less;
  return strong_ordering::greater;
}

#  endif // _LIBCPP_STD_VER <= 17

namespace this_thread {

inline _LIBCPP_HIDE_FROM_ABI __thread_id get_id() _NOEXCEPT { return ::sprt::this_thread::get_id().__native; }

} // namespace this_thread

#endif // _LIBCPP_HAS_THREADS

_LIBCPP_END_NAMESPACE_STD

#endif // _LIBCPP___THREAD_ID_H

#endif // __SPRT_USE_STL
