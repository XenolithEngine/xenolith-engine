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

#ifndef RUNTIME_INCLUDE_CXX___UTILITY_INTEGER_SEQUENCE_H_
#define RUNTIME_INCLUDE_CXX___UTILITY_INTEGER_SEQUENCE_H_

#include <sprt/cxx/__type_traits/types.h>
#include <sprt/cxx/detail/ctypes.h>

namespace sprt {

template <typename _Tp, _Tp... _Ip>
struct integer_sequence {
	typedef _Tp value_type;
	static_assert(is_integral<_Tp>::value,
			"std::integer_sequence can only be instantiated with an integral type");
	static constexpr size_t size() noexcept { return sizeof...(_Ip); }
};

template <size_t... _Ip>
using index_sequence = integer_sequence<size_t, _Ip...>;

#if __has_builtin(__make_integer_seq)

template <typename _Tp, _Tp _Ep>
using make_integer_sequence = __make_integer_seq<integer_sequence, _Tp, _Ep>;

#elif __has_builtin(__integer_pack)

template <typename _Tp, _Tp _SequenceSize>
using make_integer_sequence = integer_sequence<_Tp, __integer_pack(_SequenceSize)...>;

#else
#error "No known way to get an integer pack from the compiler"
#endif

template <size_t _Np>
using make_index_sequence = make_integer_sequence<size_t, _Np>;

} // namespace sprt

#endif // RUNTIME_INCLUDE_CXX___UTILITY_INTEGER_SEQUENCE_H_
