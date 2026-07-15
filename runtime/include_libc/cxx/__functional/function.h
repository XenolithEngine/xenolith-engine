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

// sprt overlay for <__functional/function.h>: back std::function with sprt's
// allocator-aware type-erasure engine (sprt::__malloc_function).
//
// OPT-IN, DEFAULT OFF: define SPRT_STD_FUNCTION_SPRT to enable the sprt-backed
// std::function. By default this passes through to upstream libc++ (zero regression).
//
// std::function must be a class template (primary undefined + signature partial-spec,
// so it only accepts R(Args...)), so this embeds sprt::__malloc_function and forwards
// to it, presenting the exact [func.wrap.func] surface. bad_function_call is provided
// here as upstream does (it lives in this header).

#include <__config>

#if !defined(SPRT_STD_FUNCTION_SPRT)

#include_next <__functional/function.h>

#else // sprt-backed std::function (opt-in via SPRT_STD_FUNCTION_SPRT)

#ifndef _LIBCPP___FUNCTIONAL_FUNCTION_H
#define _LIBCPP___FUNCTIONAL_FUNCTION_H

#include <__exception/exception.h>
#include <__type_traits/decay.h>
#include <__type_traits/enable_if.h>
#include <__type_traits/invoke.h>
#include <__type_traits/is_constructible.h>
#include <__type_traits/is_same.h>
#include <__type_traits/remove_cvref.h>
#include <__utility/forward.h>
#include <__utility/move.h>
#include <__verbose_abort>
#include <cstddef>
#include <typeinfo>

#include <sprt/cxx/function>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

_LIBCPP_PUSH_MACROS
#include <__undef_macros>

_LIBCPP_BEGIN_NAMESPACE_STD

// [func.wrap.badcall] bad_function_call — kept identical to upstream (it is defined in
// this header). sprt's engine asserts on an empty call, but the type must still exist.
class _LIBCPP_EXPORTED_FROM_ABI bad_function_call : public exception {
public:
  _LIBCPP_HIDE_FROM_ABI bad_function_call() _NOEXCEPT                                    = default;
  _LIBCPP_HIDE_FROM_ABI bad_function_call(const bad_function_call&) _NOEXCEPT            = default;
  _LIBCPP_HIDE_FROM_ABI bad_function_call& operator=(const bad_function_call&) _NOEXCEPT = default;
  _LIBCPP_HIDE_FROM_ABI_VIRTUAL ~bad_function_call() _NOEXCEPT override {}
  _LIBCPP_HIDE_FROM_ABI_VIRTUAL const char* what() const _NOEXCEPT override { return "std::bad_function_call"; }
};

[[__noreturn__]] inline _LIBCPP_HIDE_FROM_ABI void __throw_bad_function_call() {
#if _LIBCPP_HAS_EXCEPTIONS
  throw bad_function_call();
#else
  _LIBCPP_VERBOSE_ABORT("bad_function_call was thrown in -fno-exceptions mode");
#endif
}

template <class _Fp>
class function; // undefined

template <class _Rp, class... _ArgTypes>
class function<_Rp(_ArgTypes...)> {
  ::sprt::__malloc_function<_Rp(_ArgTypes...)> __f_;

  // A target _Fp is acceptable iff it is Lvalue-Callable for _Rp(_ArgTypes...), is
  // copy-constructible, and is not itself a std::function (so wrapping never double-wraps).
  template <class _Fp>
  using _EnableIfLValueCallable = __enable_if_t<
      !is_same<__remove_cvref_t<_Fp>, function>::value &&
      __is_invocable_r_v<_Rp, __decay_t<_Fp>&, _ArgTypes...> &&
      is_copy_constructible<__decay_t<_Fp> >::value>;

public:
  typedef _Rp result_type;

  _LIBCPP_HIDE_FROM_ABI function() _NOEXCEPT {}
  _LIBCPP_HIDE_FROM_ABI function(nullptr_t) _NOEXCEPT {}
  _LIBCPP_HIDE_FROM_ABI function(const function&)            = default;
  _LIBCPP_HIDE_FROM_ABI function(function&&) _NOEXCEPT       = default;

  template <class _Fp, class = _EnableIfLValueCallable<_Fp> >
  _LIBCPP_HIDE_FROM_ABI function(_Fp __f) : __f_(std::move(__f)) {}

  _LIBCPP_HIDE_FROM_ABI function& operator=(const function&)      = default;
  _LIBCPP_HIDE_FROM_ABI function& operator=(function&&) _NOEXCEPT = default;
  _LIBCPP_HIDE_FROM_ABI function& operator=(nullptr_t) _NOEXCEPT {
    __f_ = nullptr;
    return *this;
  }
  template <class _Fp, class = _EnableIfLValueCallable<__decay_t<_Fp> > >
  _LIBCPP_HIDE_FROM_ABI function& operator=(_Fp&& __f) {
    __f_ = std::forward<_Fp>(__f);
    return *this;
  }

  _LIBCPP_HIDE_FROM_ABI ~function() = default;

  _LIBCPP_HIDE_FROM_ABI void swap(function& __other) _NOEXCEPT { __f_.swap(__other.__f_); }

  _LIBCPP_HIDE_FROM_ABI explicit operator bool() const _NOEXCEPT { return static_cast<bool>(__f_); }

  _LIBCPP_HIDE_FROM_ABI _Rp operator()(_ArgTypes... __args) const {
    return __f_(std::forward<_ArgTypes>(__args)...);
  }

  _LIBCPP_HIDE_FROM_ABI const std::type_info& target_type() const _NOEXCEPT { return __f_.target_type(); }

  template <class _Tp>
  _LIBCPP_HIDE_FROM_ABI _Tp* target() _NOEXCEPT {
    return __f_.template target<_Tp>();
  }
  template <class _Tp>
  _LIBCPP_HIDE_FROM_ABI const _Tp* target() const _NOEXCEPT {
    return __f_.template target<_Tp>();
  }

  friend _LIBCPP_HIDE_FROM_ABI bool operator==(const function& __f, nullptr_t) _NOEXCEPT { return !__f; }
#if _LIBCPP_STD_VER <= 17
  friend _LIBCPP_HIDE_FROM_ABI bool operator!=(const function& __f, nullptr_t) _NOEXCEPT { return static_cast<bool>(__f); }
  friend _LIBCPP_HIDE_FROM_ABI bool operator==(nullptr_t, const function& __f) _NOEXCEPT { return !__f; }
  friend _LIBCPP_HIDE_FROM_ABI bool operator!=(nullptr_t, const function& __f) _NOEXCEPT { return static_cast<bool>(__f); }
#endif
};

// [func.wrap.func.con] deduction guides
template <class _Rp, class... _Ap>
function(_Rp (*)(_Ap...)) -> function<_Rp(_Ap...)>;

template <class _Fp, class _Stripped = typename ::sprt::__function_guide_helper<decltype(&_Fp::operator())>::type>
function(_Fp) -> function<_Stripped>;

template <class _Rp, class... _ArgTypes>
_LIBCPP_HIDE_FROM_ABI void swap(function<_Rp(_ArgTypes...)>& __x, function<_Rp(_ArgTypes...)>& __y) _NOEXCEPT {
  __x.swap(__y);
}

_LIBCPP_END_NAMESPACE_STD

_LIBCPP_POP_MACROS

#endif // _LIBCPP___FUNCTIONAL_FUNCTION_H

#endif // !SPRT_STD_FUNCTION_SPRT
