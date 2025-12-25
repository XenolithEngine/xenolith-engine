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

#ifndef CORE_RUNTIME_INCLUDE_SPRUNTIMEARRAY_H_
#define CORE_RUNTIME_INCLUDE_SPRUNTIMEARRAY_H_

#include "SPRuntimeInt.h"

namespace sprt {

template <typename Type, size_t Size>
struct array {
	using value_type = Type;
	using reference = value_type &;
	using const_reference = const value_type &;
	using pointer = value_type *;
	using const_pointer = const value_type *;
	using iterator = pointer;
	using const_iterator = const_pointer;

	using size_type = size_t;
	using difference_type = ptrdiff_t;

	Type _elems[Size];

	// No explicit construct/copy/destroy for aggregate type
	constexpr void fill(const value_type &v) {
		for (size_t i = 0; i < Size; ++i) { _elems[i] = v; }
	}

	// iterators:
	constexpr iterator begin() noexcept { return iterator(data()); }
	constexpr const_iterator begin() const noexcept { return const_iterator(data()); }
	constexpr iterator end() noexcept { return iterator(data() + Size); }
	constexpr const_iterator end() const noexcept { return const_iterator(data() + Size); }

	constexpr const_iterator cbegin() const noexcept { return begin(); }
	constexpr const_iterator cend() const noexcept { return end(); }

	constexpr size_type size() const noexcept { return Size; }
	constexpr size_type max_size() const noexcept { return Size; }

	[[nodiscard]]
	constexpr bool empty() const noexcept {
		return Size == 0;
	}

	constexpr reference operator[](size_type n) noexcept { return _elems[n]; }
	constexpr const_reference operator[](size_type n) const noexcept { return _elems[n]; }

	constexpr reference at(size_type n) { return _elems[n]; }

	constexpr const_reference at(size_type n) const { return _elems[n]; }

	constexpr reference front() noexcept { return (*this)[0]; }
	constexpr const_reference front() const noexcept { return (*this)[0]; }
	constexpr reference back() noexcept { return (*this)[Size - 1]; }
	constexpr const_reference back() const noexcept { return (*this)[Size - 1]; }

	constexpr value_type *data() noexcept { return _elems; }
	constexpr const value_type *data() const noexcept { return _elems; }
};

} // namespace sprt

#endif // CORE_RUNTIME_INCLUDE_SPRUNTIMEARRAY_H_
