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

// sprt overlay for <__thread/thread.h>: std::thread wraps sprt's native thread class
// (sprt::thread), so creation/join/detach and the id go through sprt's own thread
// management rather than libc++'s pthread wrappers. thread::id == this_thread's id
// (both from sprt, see <__thread/id.h> overlay).
//
// The rest of upstream is kept verbatim: __thread_struct / __thread_specific_ptr /
// __thread_local_data (used by <future> and for the per-thread cleanup slot), and
// hash<__thread_id> / operator<<(ostream, __thread_id). The thread ctor still seeds
// __thread_local_data on the new thread so <future>'s *_at_thread_exit keeps working.

#include <__config>
#include <sprt/c/bits/__sprt_config.h>

#if !defined(SPRT_STD_THREADING_SPRT)

// OPT-IN, DEFAULT OFF (see <mutex> overlay): upstream std::thread unless
// SPRT_STD_THREADING_SPRT is defined.
#include_next <__thread/thread.h>

#else // sprt-backed std::thread (opt-in via SPRT_STD_THREADING_SPRT)

#ifndef _LIBCPP___THREAD_THREAD_H
#define _LIBCPP___THREAD_THREAD_H

#include <__assert>
#include <__condition_variable/condition_variable.h>
#include <__config>
#include <__exception/terminate.h>
#include <__functional/hash.h>
#include <__functional/invoke.h>
#include <__functional/unary_function.h>
#include <__memory/addressof.h>
#include <__memory/unique_ptr.h>
#include <__mutex/mutex.h>
#include <__system_error/throw_system_error.h>
#include <__thread/id.h>
#include <__thread/support.h>
#include <__type_traits/decay.h>
#include <__type_traits/enable_if.h>
#include <__type_traits/is_same.h>
#include <__type_traits/remove_cvref.h>
#include <__utility/forward.h>
#include <__utility/move.h>

#include <sprt/cxx/thread>

#if _LIBCPP_HAS_LOCALIZATION
#  include <sstream>
#endif

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

_LIBCPP_PUSH_MACROS
#include <__undef_macros>

_LIBCPP_BEGIN_NAMESPACE_STD

#if _LIBCPP_HAS_THREADS

template <class _Tp>
class __thread_specific_ptr;
class _LIBCPP_EXPORTED_FROM_ABI __thread_struct;
class _LIBCPP_HIDDEN __thread_struct_imp;
class __assoc_sub_state;

_LIBCPP_EXPORTED_FROM_ABI __thread_specific_ptr<__thread_struct>& __thread_local_data();

class _LIBCPP_EXPORTED_FROM_ABI __thread_struct {
  __thread_struct_imp* __p_;

  __thread_struct(const __thread_struct&);
  __thread_struct& operator=(const __thread_struct&);

public:
  __thread_struct();
  ~__thread_struct();

  void notify_all_at_thread_exit(condition_variable*, mutex*);
  void __make_ready_at_thread_exit(__assoc_sub_state*);
};

template <class _Tp>
class __thread_specific_ptr {
  __libcpp_tls_key __key_;

  // Only __thread_local_data() may construct a __thread_specific_ptr
  // and only with _Tp == __thread_struct.
  static_assert(is_same<_Tp, __thread_struct>::value, "");
  __thread_specific_ptr();
  friend _LIBCPP_EXPORTED_FROM_ABI __thread_specific_ptr<__thread_struct>& __thread_local_data();

  _LIBCPP_HIDDEN static void _LIBCPP_TLS_DESTRUCTOR_CC __at_thread_exit(void*);

public:
  typedef _Tp* pointer;

  __thread_specific_ptr(const __thread_specific_ptr&)            = delete;
  __thread_specific_ptr& operator=(const __thread_specific_ptr&) = delete;
  ~__thread_specific_ptr();

  _LIBCPP_HIDE_FROM_ABI pointer get() const { return static_cast<_Tp*>(__libcpp_tls_get(__key_)); }
  _LIBCPP_HIDE_FROM_ABI pointer operator*() const { return *get(); }
  _LIBCPP_HIDE_FROM_ABI pointer operator->() const { return get(); }
  void set_pointer(pointer __p);
};

template <class _Tp>
void _LIBCPP_TLS_DESTRUCTOR_CC __thread_specific_ptr<_Tp>::__at_thread_exit(void* __p) {
  delete static_cast<pointer>(__p);
}

template <class _Tp>
__thread_specific_ptr<_Tp>::__thread_specific_ptr() {
  int __ec = __libcpp_tls_create(&__key_, &__thread_specific_ptr::__at_thread_exit);
  if (__ec)
    std::__throw_system_error(__ec, "__thread_specific_ptr construction failed");
}

template <class _Tp>
__thread_specific_ptr<_Tp>::~__thread_specific_ptr() {
  // __thread_specific_ptr is only created with a static storage duration
  // so this destructor is only invoked during program termination. Invoking
  // pthread_key_delete(__key_) may prevent other threads from deleting their
  // thread local data. For this reason we leak the key.
}

template <class _Tp>
void __thread_specific_ptr<_Tp>::set_pointer(pointer __p) {
  _LIBCPP_ASSERT_INTERNAL(get() == nullptr, "Attempting to overwrite thread local data");
  std::__libcpp_tls_set(__key_, __p);
}

template <>
struct hash<__thread_id> : public __unary_function<__thread_id, size_t> {
  _LIBCPP_HIDE_FROM_ABI size_t operator()(__thread_id __v) const _NOEXCEPT {
    return hash<decltype(__get_underlying_id(__v))>()(__get_underlying_id(__v));
  }
};

#  if _LIBCPP_HAS_LOCALIZATION
template <class _CharT, class _Traits>
_LIBCPP_HIDE_FROM_ABI basic_ostream<_CharT, _Traits>&
operator<<(basic_ostream<_CharT, _Traits>& __os, __thread_id __id) {
  // [thread.thread.id]/9: an unspecified but stable text representation. Route through
  // a classic-locale temporary so stream flags do not perturb it, matching upstream.
  basic_ostringstream<_CharT, _Traits> __sstr;
  __sstr.imbue(locale::classic());
  __sstr << __get_underlying_id(__id);
  return __os << __sstr.str();
}
#  endif // _LIBCPP_HAS_LOCALIZATION

class _LIBCPP_EXPORTED_FROM_ABI thread {
  ::sprt::thread __t_;

  thread(const thread&);
  thread& operator=(const thread&);

public:
  typedef __thread_id id;
  typedef ::sprt::thread::native_handle_type native_handle_type;

  _LIBCPP_HIDE_FROM_ABI thread() _NOEXCEPT = default;

  template <class _Fp, class... _Args, __enable_if_t<!is_same<__remove_cvref_t<_Fp>, thread>::value, int> = 0>
  _LIBCPP_HIDE_FROM_ABI explicit thread(_Fp&& __f, _Args&&... __args)
      : __t_([__tsp   = unique_ptr<__thread_struct>(new __thread_struct),
              __fn    = __decay_t<_Fp>(std::forward<_Fp>(__f)),
              ... __a = __decay_t<_Args>(std::forward<_Args>(__args))]() mutable {
          // Seed the per-thread cleanup slot so <future>'s *_at_thread_exit works,
          // then run the user callable -- mirrors upstream's __thread_proxy.
          __thread_local_data().set_pointer(__tsp.release());
          std::__invoke(std::move(__fn), std::move(__a)...);
        }) {}

  // [thread.thread.destr]/[thread.thread.assign]: a joinable thread must not be
  // destroyed or assigned over -- std::terminate(), regardless of what sprt::thread
  // itself would do.
  _LIBCPP_HIDE_FROM_ABI ~thread() {
    if (__t_.joinable())
      terminate();
  }

  _LIBCPP_HIDE_FROM_ABI thread(thread&& __t) _NOEXCEPT : __t_(std::move(__t.__t_)) {}

  _LIBCPP_HIDE_FROM_ABI thread& operator=(thread&& __t) _NOEXCEPT {
    if (__t_.joinable())
      terminate();
    __t_ = std::move(__t.__t_);
    return *this;
  }

  _LIBCPP_HIDE_FROM_ABI void swap(thread& __t) _NOEXCEPT { __t_.swap(__t.__t_); }

  _LIBCPP_HIDE_FROM_ABI bool joinable() const _NOEXCEPT { return __t_.joinable(); }
  _LIBCPP_HIDE_FROM_ABI void join() { __t_.join(); }
  _LIBCPP_HIDE_FROM_ABI void detach() { __t_.detach(); }
  _LIBCPP_HIDE_FROM_ABI id get_id() const _NOEXCEPT { return id(__t_.get_id().__native); }
  _LIBCPP_HIDE_FROM_ABI native_handle_type native_handle() _NOEXCEPT { return __t_.native_handle(); }

  static _LIBCPP_HIDE_FROM_ABI unsigned hardware_concurrency() _NOEXCEPT {
    return ::sprt::thread::hardware_concurrency();
  }
};

inline _LIBCPP_HIDE_FROM_ABI void swap(thread& __x, thread& __y) _NOEXCEPT { __x.swap(__y); }

#endif // _LIBCPP_HAS_THREADS

_LIBCPP_END_NAMESPACE_STD

_LIBCPP_POP_MACROS

#endif // _LIBCPP___THREAD_THREAD_H

#endif // __SPRT_USE_STL
