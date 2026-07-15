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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_DETAIL_INLINE_BUFFER_H_
#define RUNTIME_INCLUDE_SPRT_CXX_DETAIL_INLINE_BUFFER_H_

// A minimal, dependency-light fixed-size inline buffer that mirrors the subset of the
// std::array / sprt::array interface (data/size/operator[]/iteration/fill) used by the
// FOUNDATIONAL headers — the constexpr ctype tables, the callback SBO storage, the
// linear_memory small/soo storage, and similar.
//
// Why it exists: <sprt/cxx/array> is built on the iterator / algorithm / tuple-protocol
// machinery, which itself sits ABOVE this foundational layer. A foundational header that
// pulled in <sprt/cxx/array> would therefore form an include cycle — and, in particular,
// would block building <array> itself on top of that machinery (e.g. projecting <array>
// from the vendored libc++). inline_buffer carries no such dependency: it needs only a
// size type, so foundational code can keep a fixed inline buffer without dragging array in.
//
// It is an aggregate (no user-declared constructors): brace-initializable, trivially
// copyable, and usable in constant expressions.

#include <sprt/cxx/detail/ctypes.h> // sprt::size_t

namespace sprt {
namespace detail {

template <typename Type, size_t Count>
struct inline_buffer {
	using value_type = Type;
	using size_type = size_t;
	using pointer = Type *;
	using const_pointer = const Type *;
	using reference = Type &;
	using const_reference = const Type &;
	using iterator = pointer;
	using const_iterator = const_pointer;

	Type _elems[Count];

	constexpr reference operator[](size_type __i) { return _elems[__i]; }
	constexpr const_reference operator[](size_type __i) const { return _elems[__i]; }

	constexpr reference front() { return _elems[0]; }
	constexpr const_reference front() const { return _elems[0]; }
	constexpr reference back() { return _elems[Count - 1]; }
	constexpr const_reference back() const { return _elems[Count - 1]; }

	constexpr pointer data() { return _elems; }
	constexpr const_pointer data() const { return _elems; }
	constexpr size_type size() const { return Count; }

	constexpr iterator begin() { return _elems; }
	constexpr const_iterator begin() const { return _elems; }
	constexpr iterator end() { return _elems + Count; }
	constexpr const_iterator end() const { return _elems + Count; }

	constexpr void fill(const value_type &__v) {
		for (size_type __i = 0; __i < Count; ++__i) { _elems[__i] = __v; }
	}
};

} // namespace detail
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX_DETAIL_INLINE_BUFFER_H_
