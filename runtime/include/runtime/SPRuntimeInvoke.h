/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef CORE_RUNTIME_INCLUDE_SPRUNTIMEINVOKE_H_
#define CORE_RUNTIME_INCLUDE_SPRUNTIMEINVOKE_H_

#include "SPRuntimeInit.h"

namespace sprt {

/*
	Invokable
	(based on llvm-libc++ code)
*/

template <typename _Tp, typename _Up, typename = void>
struct __is_core_convertible : public false_type { };

template <typename _Tp, typename _Up>
struct __is_core_convertible<_Tp, _Up,
		decltype(static_cast<void (*)(_Up)>(0)(static_cast<_Tp (*)()>(0)()))> : public true_type {
};

template <typename _Tp>
_Tp &&__declval(int);
template <typename _Tp>
_Tp __declval(long);

template <typename _Tp>
decltype(sprt::__declval<_Tp>(0)) declval() noexcept {
	static_assert(!__is_same(_Tp, _Tp),
                "std::declval can only be used in an unevaluated context. "
                "It's likely that your current usage is trying to extract a value from the function.");
}

template <typename _Tp>
using __decay_t = __decay(_Tp);

template <typename _Tp>
struct decay {
	using type = __decay_t<_Tp>;
};

template <typename _DecayedFp>
struct __member_pointer_typename_type { };

template <typename _Ret, typename _ClassType>
struct __member_pointer_typename_type<_Ret _ClassType::*> {
	typedef _ClassType type;
};

template <typename _Tp>
struct is_member_pointer {
	static constexpr bool value = __is_member_pointer(_Tp);
};

template <typename _Tp>
struct is_member_object_pointer {
	static constexpr bool value = __is_member_object_pointer(_Tp);
};

template <typename _Tp>
struct is_member_function_pointer {
	static constexpr bool value = __is_member_function_pointer(_Tp);
};

template <typename _Tp>
inline constexpr bool is_member_pointer_v = __is_member_pointer(_Tp);

template <typename _Tp>
inline constexpr bool is_member_object_pointer_v = __is_member_object_pointer(_Tp);

template <typename _Tp>
inline constexpr bool is_member_function_pointer_v = __is_member_function_pointer(_Tp);

template <typename _Tp>
class reference_wrapper;

template <typename _Tp>
struct __is_reference_wrapper_impl {
	static constexpr bool value = false;
};

template <typename _Tp>
struct __is_reference_wrapper_impl<reference_wrapper<_Tp> > {
	static constexpr bool value = true;
};

template <typename _Tp>
struct __is_reference_wrapper : public __is_reference_wrapper_impl<remove_cv_t<_Tp> > { };


template <typename _Fp, typename _A0, typename _DecayFp = __decay_t<_Fp>,
		typename _DecayA0 = __decay_t<_A0>,
		typename _ClassT = typename __member_pointer_typename_type<_DecayFp>::type>
using __enable_if_bullet1 = enable_if_t<is_member_function_pointer<_DecayFp>::value
		&& (is_same<_ClassT, _DecayA0>::value || is_base_of<_ClassT, _DecayA0>::value)>;

template <typename _Fp, typename _A0, typename _DecayFp = __decay_t<_Fp>,
		typename _DecayA0 = __decay_t<_A0> >
using __enable_if_bullet2 = enable_if_t<is_member_function_pointer<_DecayFp>::value
		&& __is_reference_wrapper<_DecayA0>::value>;

template <typename _Fp, typename _A0, typename _DecayFp = __decay_t<_Fp>,
		typename _DecayA0 = __decay_t<_A0>,
		typename _ClassT = typename __member_pointer_typename_type<_DecayFp>::type>
using __enable_if_bullet3 = enable_if_t<is_member_function_pointer<_DecayFp>::value
		&& !(is_same<_ClassT, _DecayA0>::value || is_base_of<_ClassT, _DecayA0>::value)
		&& !__is_reference_wrapper<_DecayA0>::value>;

template <typename _Fp, typename _A0, typename _DecayFp = __decay_t<_Fp>,
		typename _DecayA0 = __decay_t<_A0>,
		typename _ClassT = typename __member_pointer_typename_type<_DecayFp>::type>
using __enable_if_bullet4 = enable_if_t<is_member_object_pointer<_DecayFp>::value
		&& (is_same<_ClassT, _DecayA0>::value || is_base_of<_ClassT, _DecayA0>::value)>;

template <typename _Fp, typename _A0, typename _DecayFp = __decay_t<_Fp>,
		typename _DecayA0 = __decay_t<_A0> >
using __enable_if_bullet5 = enable_if_t<is_member_object_pointer<_DecayFp>::value
		&& __is_reference_wrapper<_DecayA0>::value>;

template <typename _Fp, typename _A0, typename _DecayFp = __decay_t<_Fp>,
		typename _DecayA0 = __decay_t<_A0>,
		typename _ClassT = typename __member_pointer_typename_type<_DecayFp>::type>
using __enable_if_bullet6 = enable_if_t<is_member_object_pointer<_DecayFp>::value
		&& !(is_same<_ClassT, _DecayA0>::value || is_base_of<_ClassT, _DecayA0>::value)
		&& !__is_reference_wrapper<_DecayA0>::value>;

struct __nat {
	__nat() = delete;
	__nat(const __nat &) = delete;
	__nat &operator=(const __nat &) = delete;
	~__nat() = delete;
};

template <typename... _Args>
__nat __invoke(_Args &&...__args);

// bullets 1, 2 and 3

// clang-format off
template <typename _Fp, typename _A0, typename... _Args, typename = __enable_if_bullet1<_Fp, _A0> >
inline  constexpr
decltype((sprt::declval<_A0>().*sprt::declval<_Fp>())(sprt::declval<_Args>()...))
__invoke(_Fp&& __f, _A0&& __a0, _Args&&... __args)
    noexcept(noexcept((static_cast<_A0&&>(__a0).*__f)(static_cast<_Args&&>(__args)...)))
               { return (static_cast<_A0&&>(__a0).*__f)(static_cast<_Args&&>(__args)...); }

template <typename _Fp, typename _A0, typename... _Args, typename = __enable_if_bullet2<_Fp, _A0> >
inline  constexpr
decltype((sprt::declval<_A0>().get().*sprt::declval<_Fp>())(sprt::declval<_Args>()...))
__invoke(_Fp&& __f, _A0&& __a0, _Args&&... __args)
    noexcept(noexcept((__a0.get().*__f)(static_cast<_Args&&>(__args)...)))
               { return (__a0.get().*__f)(static_cast<_Args&&>(__args)...); }

template <typename _Fp, typename _A0, typename... _Args, typename = __enable_if_bullet3<_Fp, _A0> >
inline  constexpr
decltype(((*sprt::declval<_A0>()).*sprt::declval<_Fp>())(sprt::declval<_Args>()...))
__invoke(_Fp&& __f, _A0&& __a0, _Args&&... __args)
    noexcept(noexcept(((*static_cast<_A0&&>(__a0)).*__f)(static_cast<_Args&&>(__args)...)))
               { return ((*static_cast<_A0&&>(__a0)).*__f)(static_cast<_Args&&>(__args)...); }

// bullets 4, 5 and 6

template <typename _Fp, typename _A0, typename = __enable_if_bullet4<_Fp, _A0> >
inline  constexpr
decltype(sprt::declval<_A0>().*sprt::declval<_Fp>())
__invoke(_Fp&& __f, _A0&& __a0)
    noexcept(noexcept(static_cast<_A0&&>(__a0).*__f))
               { return static_cast<_A0&&>(__a0).*__f; }

template <typename _Fp, typename _A0, typename = __enable_if_bullet5<_Fp, _A0> >
inline constexpr
decltype(sprt::declval<_A0>().get().*sprt::declval<_Fp>())
__invoke(_Fp&& __f, _A0&& __a0)
    noexcept(noexcept(__a0.get().*__f))
               { return __a0.get().*__f; }

template <typename _Fp, typename _A0, typename = __enable_if_bullet6<_Fp, _A0> >
inline constexpr
decltype((*sprt::declval<_A0>()).*sprt::declval<_Fp>())
__invoke(_Fp&& __f, _A0&& __a0)
    noexcept(noexcept((*static_cast<_A0&&>(__a0)).*__f))
               { return (*static_cast<_A0&&>(__a0)).*__f; }

// bullet 7

template <typename _Fp, typename... _Args>
inline constexpr
decltype(sprt::declval<_Fp>()(sprt::declval<_Args>()...))
__invoke(_Fp&& __f, _Args&&... __args)
    noexcept(noexcept(static_cast<_Fp&&>(__f)(static_cast<_Args&&>(__args)...)))
               { return static_cast<_Fp&&>(__f)(static_cast<_Args&&>(__args)...); }

template <typename _Tp>
struct is_void {
	static constexpr bool value = __is_same(__remove_cv(_Tp), void);
};

template <typename _Tp>
inline constexpr bool is_void_v = __is_same(__remove_cv(_Tp), void);

template <typename _Ret, bool = is_void<_Ret>::value>
struct __invoke_void_return_wrapper {
	template <typename... _Args>
	constexpr static _Ret __call(_Args &&...__args) {
		return sprt::__invoke(forward<_Args>(__args)...);
	}
};

template <typename _Ret>
struct __invoke_void_return_wrapper<_Ret, true> {
	template <typename... _Args>
	constexpr static void __call(_Args &&...__args) {
		sprt::__invoke(forward<_Args>(__args)...);
	}
};

template <typename _Ret, typename _Fp, typename... _Args>
struct __invokable_r {
  template <typename _XFp, typename... _XArgs>
  static decltype(sprt::__invoke(sprt::declval<_XFp>(), sprt::declval<_XArgs>()...)) __try_call(int);
  template <typename _XFp, typename... _XArgs>
  static __nat __try_call(...);

  // FIXME: Check that _Ret, _Fp, and _Args... are all complete types, cv void,
  // or incomplete array types as required by the standard.
  using _Result = decltype(__try_call<_Fp, _Args...>(0));

  using type              = conditional_t<_IsNotSame<_Result, __nat>::value,
                                           conditional_t<is_void<_Ret>::value, true_type, __is_core_convertible<_Result, _Ret> >,
                                            false_type>;
  static const bool value = type::value;
};
template <typename _Fp, typename... _Args>
using __is_invocable = __invokable_r<void, _Fp, _Args...>;

template <typename _Func, typename... _Args>
inline const bool __is_invocable_v = __is_invocable<_Func, _Args...>::value;

template <typename _Ret, typename _Func, typename... _Args>
inline const bool __is_invocable_r_v = __invokable_r<_Ret, _Func, _Args...>::value;

template <typename _Func, typename... _Args>
struct __invoke_result : enable_if<__is_invocable_v<_Func, _Args...>,
								 typename __invokable_r<void, _Func, _Args...>::_Result> { };

template <typename _Func, typename... _Args>
using __invoke_result_t = typename __invoke_result<_Func, _Args...>::type;

template <typename _Ret, typename... _Args>
constexpr _Ret __invoke_r(_Args &&...__args) {
	return __invoke_void_return_wrapper<_Ret>::__call(forward<_Args>(__args)...);
}

template <typename _Fn, typename... _Args>
struct is_invocable {
	static constexpr bool value = __is_invocable_v<_Fn, _Args...>;
};

template <typename _Ret, typename _Fn, typename... _Args>
struct is_invocable_r {
	static constexpr bool value = __is_invocable_r_v<_Ret, _Fn, _Args...>;
};

template <typename _Fn, typename... _Args>
inline constexpr bool is_invocable_v = __is_invocable_v<_Fn, _Args...>;

template <typename _Ret, typename _Fn, typename... _Args>
inline constexpr bool is_invocable_r_v = __is_invocable_r_v<_Ret, _Fn, _Args...>;

}

#endif // CORE_RUNTIME_INCLUDE_SPRUNTIMEINVOKE_H_