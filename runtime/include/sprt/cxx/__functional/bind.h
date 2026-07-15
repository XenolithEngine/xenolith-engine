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

// [func.memfn] mem_fn, [func.not.fn] not_fn, [func.bind.front] bind_front / bind_back.
// The classic std::bind with placeholders (_1, _2, ...) is NOT provided — bind_front /
// bind_back / lambdas cover the modern uses.

#ifndef RUNTIME_INCLUDE_SPRT_CXX___FUNCTIONAL_BIND_H_
#define RUNTIME_INCLUDE_SPRT_CXX___FUNCTIONAL_BIND_H_

#include <sprt/cxx/type_traits>
#include <sprt/cxx/tuple>
#include <sprt/cxx/__utility/common.h>
#include <sprt/cxx/__utility/integer_sequence.h>
#include <sprt/cxx/__functional/invoke.h>

namespace sprt {

// ---------------------------------------------------------- mem_fn
template <typename _MemPtr>
struct __mem_fn_t {
	_MemPtr _pm;
	template <typename... _Args>
	constexpr decltype(auto) operator()(_Args &&...__args) const {
		return sprt::__invoke(_pm, sprt::forward<_Args>(__args)...);
	}
};

template <typename _Res, typename _Class>
constexpr __mem_fn_t<_Res _Class::*> mem_fn(_Res _Class::*__pm) noexcept {
	return __mem_fn_t<_Res _Class::*>{__pm};
}

// ---------------------------------------------------------- not_fn
template <typename _Fn>
struct __not_fn_t {
	_Fn _fn;
	template <typename... _Args>
	constexpr decltype(auto) operator()(_Args &&...__args) & {
		return !sprt::__invoke(_fn, sprt::forward<_Args>(__args)...);
	}
	template <typename... _Args>
	constexpr decltype(auto) operator()(_Args &&...__args) const & {
		return !sprt::__invoke(_fn, sprt::forward<_Args>(__args)...);
	}
};

template <typename _Fn>
constexpr __not_fn_t<decay_t<_Fn>> not_fn(_Fn &&__f) {
	return __not_fn_t<decay_t<_Fn>>{sprt::forward<_Fn>(__f)};
}

// ---------------------------------------------------------- bind_front / bind_back
template <typename _Fn, typename... _Bound>
struct __bind_front_t {
	_Fn _fn;
	tuple<_Bound...> _bound;

	template <sprt::size_t... _I, typename... _Call>
	constexpr decltype(auto) __invoke_impl(index_sequence<_I...>, _Call &&...__call) const {
		return sprt::__invoke(_fn, sprt::get<_I>(_bound)..., sprt::forward<_Call>(__call)...);
	}
	template <sprt::size_t... _I, typename... _Call>
	constexpr decltype(auto) __invoke_impl(index_sequence<_I...>, _Call &&...__call) {
		return sprt::__invoke(_fn, sprt::get<_I>(_bound)..., sprt::forward<_Call>(__call)...);
	}
	template <typename... _Call>
	constexpr decltype(auto) operator()(_Call &&...__call) const {
		return __invoke_impl(make_index_sequence<sizeof...(_Bound)>{},
				sprt::forward<_Call>(__call)...);
	}
	template <typename... _Call>
	constexpr decltype(auto) operator()(_Call &&...__call) {
		return __invoke_impl(make_index_sequence<sizeof...(_Bound)>{},
				sprt::forward<_Call>(__call)...);
	}
};

template <typename _Fn, typename... _Args>
constexpr __bind_front_t<decay_t<_Fn>, decay_t<_Args>...> bind_front(_Fn &&__f, _Args &&...__args) {
	return __bind_front_t<decay_t<_Fn>, decay_t<_Args>...>{sprt::forward<_Fn>(__f),
		tuple<decay_t<_Args>...>(sprt::forward<_Args>(__args)...)};
}

template <typename _Fn, typename... _Bound>
struct __bind_back_t {
	_Fn _fn;
	tuple<_Bound...> _bound;

	template <sprt::size_t... _I, typename... _Call>
	constexpr decltype(auto) __invoke_impl(index_sequence<_I...>, _Call &&...__call) const {
		return sprt::__invoke(_fn, sprt::forward<_Call>(__call)..., sprt::get<_I>(_bound)...);
	}
	template <sprt::size_t... _I, typename... _Call>
	constexpr decltype(auto) __invoke_impl(index_sequence<_I...>, _Call &&...__call) {
		return sprt::__invoke(_fn, sprt::forward<_Call>(__call)..., sprt::get<_I>(_bound)...);
	}
	template <typename... _Call>
	constexpr decltype(auto) operator()(_Call &&...__call) const {
		return __invoke_impl(make_index_sequence<sizeof...(_Bound)>{},
				sprt::forward<_Call>(__call)...);
	}
	template <typename... _Call>
	constexpr decltype(auto) operator()(_Call &&...__call) {
		return __invoke_impl(make_index_sequence<sizeof...(_Bound)>{},
				sprt::forward<_Call>(__call)...);
	}
};

template <typename _Fn, typename... _Args>
constexpr __bind_back_t<decay_t<_Fn>, decay_t<_Args>...> bind_back(_Fn &&__f, _Args &&...__args) {
	return __bind_back_t<decay_t<_Fn>, decay_t<_Args>...>{sprt::forward<_Fn>(__f),
		tuple<decay_t<_Args>...>(sprt::forward<_Args>(__args)...)};
}

// ------------------------------------------------------------ classic bind
// [func.bind] std::bind with placeholders _1.._N, is_placeholder / is_bind_expression,
// and nested-bind / reference_wrapper resolution. Modelled on the libstdc++/libc++
// design; the returned closure resolves each bound argument against the call
// arguments via __mu.

template <int _Np>
struct __placeholder { };

namespace placeholders {
inline constexpr __placeholder<1> _1{};
inline constexpr __placeholder<2> _2{};
inline constexpr __placeholder<3> _3{};
inline constexpr __placeholder<4> _4{};
inline constexpr __placeholder<5> _5{};
inline constexpr __placeholder<6> _6{};
inline constexpr __placeholder<7> _7{};
inline constexpr __placeholder<8> _8{};
inline constexpr __placeholder<9> _9{};
inline constexpr __placeholder<10> _10{};
} // namespace placeholders

template <typename _Tp>
struct is_placeholder : integral_constant<int, 0> { };
template <int _Np>
struct is_placeholder<__placeholder<_Np>> : integral_constant<int, _Np> { };
template <typename _Tp>
inline constexpr int is_placeholder_v = is_placeholder<_Tp>::value;

template <typename _Tp>
struct is_bind_expression : false_type { };
template <typename _Tp>
inline constexpr bool is_bind_expression_v = is_bind_expression<_Tp>::value;

template <typename _Fd, typename... _BoundArgs>
class __bind_t;

template <typename _Fd, typename... _BoundArgs>
struct is_bind_expression<__bind_t<_Fd, _BoundArgs...>> : true_type { };

template <typename _Tp>
class reference_wrapper;
template <typename _Tp>
struct __bind_is_refwrap : false_type { };
template <typename _Tp>
struct __bind_is_refwrap<reference_wrapper<_Tp>> : true_type { };

// __mu: resolve one bound argument against the tuple of (forwarded) call arguments.
template <typename _Bound, typename _CallTuple>
constexpr decltype(auto) __bind_mu(_Bound &__bound, _CallTuple &__call) {
	using _Bd = remove_cv_t<remove_reference_t<_Bound>>;
	if constexpr (is_placeholder<_Bd>::value != 0) {
		return sprt::get<is_placeholder<_Bd>::value - 1>(sprt::move_unsafe(__call));
	} else if constexpr (is_bind_expression<_Bd>::value) {
		return sprt::apply(__bound, sprt::move_unsafe(__call));
	} else if constexpr (__bind_is_refwrap<_Bd>::value) {
		return __bound.get();
	} else {
		return (__bound);
	}
}

template <typename _Fd, typename... _BoundArgs>
class __bind_t {
	_Fd _f;
	tuple<_BoundArgs...> _bound;

	template <sprt::size_t... _I, typename _CallTuple>
	constexpr decltype(auto) __call(index_sequence<_I...>, _CallTuple &&__ct) {
		return sprt::__invoke(_f, __bind_mu(sprt::get<_I>(_bound), __ct)...);
	}
	template <sprt::size_t... _I, typename _CallTuple>
	constexpr decltype(auto) __call(index_sequence<_I...>, _CallTuple &&__ct) const {
		return sprt::__invoke(_f, __bind_mu(sprt::get<_I>(_bound), __ct)...);
	}

public:
	template <typename _Gd, typename... _BA>
	requires (!(sizeof...(_BA) == 0 && sprt::is_same_v<sprt::decay_t<_Gd>, __bind_t>))
	constexpr explicit __bind_t(_Gd &&__g, _BA &&...__ba)
	: _f(sprt::forward<_Gd>(__g)), _bound(sprt::forward<_BA>(__ba)...) { }

	constexpr __bind_t(const __bind_t &) = default;
	constexpr __bind_t(__bind_t &&) = default;
	constexpr __bind_t &operator=(const __bind_t &) = default;
	constexpr __bind_t &operator=(__bind_t &&) = default;

	template <typename... _CallArgs>
	constexpr decltype(auto) operator()(_CallArgs &&...__ca) {
		auto __ct = sprt::forward_as_tuple(sprt::forward<_CallArgs>(__ca)...);
		return __call(make_index_sequence<sizeof...(_BoundArgs)>{}, __ct);
	}
	template <typename... _CallArgs>
	constexpr decltype(auto) operator()(_CallArgs &&...__ca) const {
		auto __ct = sprt::forward_as_tuple(sprt::forward<_CallArgs>(__ca)...);
		return __call(make_index_sequence<sizeof...(_BoundArgs)>{}, __ct);
	}
};

template <typename _Fn, typename... _Args>
constexpr __bind_t<decay_t<_Fn>, decay_t<_Args>...> bind(_Fn &&__f, _Args &&...__args) {
	return __bind_t<decay_t<_Fn>, decay_t<_Args>...>(sprt::forward<_Fn>(__f),
			sprt::forward<_Args>(__args)...);
}

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___FUNCTIONAL_BIND_H_
