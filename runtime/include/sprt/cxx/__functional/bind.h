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
	return __mem_fn_t<_Res _Class::*> {__pm};
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
	return __not_fn_t<decay_t<_Fn>> {sprt::forward<_Fn>(__f)};
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
		return __invoke_impl(make_index_sequence<sizeof...(_Bound)> {}, sprt::forward<_Call>(__call)...);
	}
	template <typename... _Call>
	constexpr decltype(auto) operator()(_Call &&...__call) {
		return __invoke_impl(make_index_sequence<sizeof...(_Bound)> {}, sprt::forward<_Call>(__call)...);
	}
};

template <typename _Fn, typename... _Args>
constexpr __bind_front_t<decay_t<_Fn>, decay_t<_Args>...> bind_front(_Fn &&__f, _Args &&...__args) {
	return __bind_front_t<decay_t<_Fn>, decay_t<_Args>...> {
			sprt::forward<_Fn>(__f), tuple<decay_t<_Args>...>(sprt::forward<_Args>(__args)...)};
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
		return __invoke_impl(make_index_sequence<sizeof...(_Bound)> {}, sprt::forward<_Call>(__call)...);
	}
	template <typename... _Call>
	constexpr decltype(auto) operator()(_Call &&...__call) {
		return __invoke_impl(make_index_sequence<sizeof...(_Bound)> {}, sprt::forward<_Call>(__call)...);
	}
};

template <typename _Fn, typename... _Args>
constexpr __bind_back_t<decay_t<_Fn>, decay_t<_Args>...> bind_back(_Fn &&__f, _Args &&...__args) {
	return __bind_back_t<decay_t<_Fn>, decay_t<_Args>...> {
			sprt::forward<_Fn>(__f), tuple<decay_t<_Args>...>(sprt::forward<_Args>(__args)...)};
}

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___FUNCTIONAL_BIND_H_
