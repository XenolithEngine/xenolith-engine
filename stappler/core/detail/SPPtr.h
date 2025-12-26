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

#ifndef CORE_CORE_DETAIL_SPPTR_H_
#define CORE_CORE_DETAIL_SPPTR_H_

#include "SPLogInit.h"
#include <algorithm>

namespace STAPPLER_VERSIONIZED stappler {

template <typename T, typename Finalizer>
concept PtrFinalizer = requires(T *ptr, const Finalizer &f) { f(ptr); };

template <typename T>
struct PtrFinalizerDefault {
	void operator()(T *t) const { ::free(t); }
};

// RAII Pointer type for C code interoperability (it's not an std;:unique_ptr replacement!)
// Created to call `::free` or other finalizer when value become out of scope
//
// Stappler framework types should provide their own RAII types or use Ref + Rc<>
template <typename T, typename Finalizer = PtrFinalizerDefault<T>>
requires PtrFinalizer<T, Finalizer>
class Ptr final {
public:
	using element_type = T *;

	~Ptr() noexcept { clear(); }

	Ptr(T *t, Finalizer &&f = Finalizer()) noexcept : _base(t), _finalizer(std::move(f)) { }

	Ptr(const Ptr &other) noexcept = delete;
	Ptr &operator=(const Ptr &other) noexcept = delete;

	Ptr(Ptr &&other) noexcept : _base(other._base), _finalizer(std::move(other._finalizer)) {
		other._base = nullptr;
	}
	Ptr &operator=(Ptr &&other) noexcept {
		clear();

		_base = other._base;
		_finalizer = std::move(other._finalizer);
		other._base = nullptr;
		return *this;
	}

	constexpr auto get() const noexcept { return _base; }

	void clear() noexcept {
		if (_base) {
			_finalizer(_base);
			_base = nullptr;
		}
	}

	constexpr explicit operator bool() const noexcept { return get() != nullptr; }

	constexpr operator element_type() const noexcept { return get(); }
	constexpr decltype(auto) operator->() const noexcept { return get(); }
	constexpr decltype(auto) operator*() const noexcept { return *get(); }

	Ptr &operator++() = delete;
	Ptr &operator--() = delete;
	Ptr operator++(int) = delete;
	Ptr operator--(int) = delete;
	Ptr &operator+=(std::ptrdiff_t) = delete;
	Ptr &operator-=(std::ptrdiff_t) = delete;
	void operator[](std::ptrdiff_t) const = delete;

private:
	element_type _base;
	Finalizer _finalizer;
};

} // namespace STAPPLER_VERSIONIZED stappler

#endif /* CORE_CORE_DETAIL_SPPTR_H_ */
