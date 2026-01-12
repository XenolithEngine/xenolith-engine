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

// Original source code from LLVM libc++

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_INITIALIZER_LIST_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_INITIALIZER_LIST_H_

#include <sprt/runtime/int.h>

#if __SPRT_USE_STL

#include <initializer_list>

#else

namespace std {

template <typename _Ep>
class initializer_list {
	const _Ep *__begin_;
	sprt::size_t __size_;

	constexpr initializer_list(const _Ep *__b, sprt::size_t __s) noexcept
	: __begin_(__b), __size_(__s) { }

public:
	typedef _Ep value_type;
	typedef const _Ep &reference;
	typedef const _Ep &const_reference;
	typedef sprt::size_t size_type;

	typedef const _Ep *iterator;
	typedef const _Ep *const_iterator;

	constexpr initializer_list() noexcept : __begin_(nullptr), __size_(0) { }

	constexpr sprt::size_t size() const noexcept { return __size_; }

	constexpr const _Ep *begin() const noexcept { return __begin_; }

	constexpr const _Ep *end() const noexcept { return __begin_ + __size_; }
};

template <class _Ep>
inline constexpr const _Ep *begin(initializer_list<_Ep> __il) noexcept {
	return __il.begin();
}

template <class _Ep>
inline constexpr const _Ep *end(initializer_list<_Ep> __il) noexcept {
	return __il.end();
}

} // namespace std

#endif

namespace sprt {
using std::initializer_list;
}

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_INITIALIZER_LIST_H_
