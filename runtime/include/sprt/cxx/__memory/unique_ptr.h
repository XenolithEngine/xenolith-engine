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

// [unique.ptr] default_delete + unique_ptr (single object + array) + make_unique family.

#ifndef RUNTIME_INCLUDE_SPRT_CXX___MEMORY_UNIQUE_PTR_H_
#define RUNTIME_INCLUDE_SPRT_CXX___MEMORY_UNIQUE_PTR_H_

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/cxx/cstddef>
#include <sprt/cxx/compare>
#include <sprt/cxx/type_traits>
#include <sprt/cxx/__utility/common.h>
#include <sprt/cxx/detail/hash.h>

// unique_ptr is the standard-conforming owner of `new`/`delete`-allocated objects, so it
// deliberately uses the language new/delete expressions (delete[] needs the array cookie to
// destroy non-trivial arrays, which the sprt::__delete_n helper can't do without a count).
// Silence the freestanding "use sprt::__delete" nudge for exactly this std-layer header.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

namespace sprt {

// [unique.ptr.dltr.dflt]
template <typename _Tp>
struct default_delete {
	constexpr default_delete() noexcept = default;
	template <typename _Up, enable_if_t<is_convertible_v<_Up *, _Tp *>, int> = 0>
	default_delete(const default_delete<_Up> &) noexcept { }
	void operator()(_Tp *__ptr) const noexcept {
		static_assert(sizeof(_Tp) > 0, "default_delete: cannot delete an incomplete type");
		delete __ptr;
	}
};

template <typename _Tp>
struct default_delete<_Tp[]> {
	constexpr default_delete() noexcept = default;
	template <typename _Up, enable_if_t<is_convertible_v<_Up (*)[], _Tp (*)[]>, int> = 0>
	default_delete(const default_delete<_Up[]> &) noexcept { }
	template <typename _Up, enable_if_t<is_convertible_v<_Up (*)[], _Tp (*)[]>, int> = 0>
	void operator()(_Up *__ptr) const noexcept {
		static_assert(sizeof(_Up) > 0, "default_delete: cannot delete an incomplete type");
		delete[] __ptr;
	}
};

// pointer = remove_reference_t<_Dp>::pointer if present, else _Tp*.
template <typename _Tp, typename _Dp, typename = void>
struct __up_pointer {
	using type = _Tp *;
};
template <typename _Tp, typename _Dp>
struct __up_pointer<_Tp, _Dp, void_t<typename remove_reference_t<_Dp>::pointer>> {
	using type = typename remove_reference_t<_Dp>::pointer;
};

// ---------------------------------------------------------- single object
template <typename _Tp, typename _Dp = default_delete<_Tp>>
class unique_ptr {
public:
	using pointer = typename __up_pointer<_Tp, _Dp>::type;
	using element_type = _Tp;
	using deleter_type = _Dp;

private:
	pointer _ptr;
	SPRT_NO_UNIQUE_ADDRESS _Dp _deleter;

public:
	constexpr unique_ptr() noexcept : _ptr(), _deleter() {
		static_assert(!is_pointer_v<_Dp>, "unique_ptr with a pointer deleter needs the deleter");
	}
	constexpr unique_ptr(nullptr_t) noexcept : _ptr(), _deleter() { }
	explicit unique_ptr(pointer __p) noexcept : _ptr(__p), _deleter() { }

	unique_ptr(pointer __p, const _Dp &__d) noexcept : _ptr(__p), _deleter(__d) { }
	unique_ptr(pointer __p, _Dp &&__d) noexcept : _ptr(__p), _deleter(sprt::move_unsafe(__d)) { }

	unique_ptr(unique_ptr &&__u) noexcept
	: _ptr(__u.release()), _deleter(sprt::forward<_Dp>(__u.get_deleter())) { }

	template <typename _Up, typename _Ep,
			enable_if_t<is_convertible_v<typename unique_ptr<_Up, _Ep>::pointer, pointer>
							&& !is_array_v<_Up>
							&& (is_reference_v<_Dp> ? is_same_v<_Ep, _Dp>
													: is_convertible_v<_Ep, _Dp>),
					int> = 0>
	unique_ptr(unique_ptr<_Up, _Ep> &&__u) noexcept
	: _ptr(__u.release()), _deleter(sprt::forward<_Ep>(__u.get_deleter())) { }

	unique_ptr(const unique_ptr &) = delete;
	unique_ptr &operator=(const unique_ptr &) = delete;

	~unique_ptr() {
		if (_ptr) {
			_deleter(_ptr);
		}
	}

	unique_ptr &operator=(unique_ptr &&__u) noexcept {
		reset(__u.release());
		_deleter = sprt::forward<_Dp>(__u.get_deleter());
		return *this;
	}
	template <typename _Up, typename _Ep,
			enable_if_t<is_convertible_v<typename unique_ptr<_Up, _Ep>::pointer, pointer>
							&& !is_array_v<_Up>,
					int> = 0>
	unique_ptr &operator=(unique_ptr<_Up, _Ep> &&__u) noexcept {
		reset(__u.release());
		_deleter = sprt::forward<_Ep>(__u.get_deleter());
		return *this;
	}
	unique_ptr &operator=(nullptr_t) noexcept {
		reset();
		return *this;
	}

	add_lvalue_reference_t<_Tp> operator*() const { return *_ptr; }
	pointer operator->() const noexcept { return _ptr; }
	pointer get() const noexcept { return _ptr; }
	_Dp &get_deleter() noexcept { return _deleter; }
	const _Dp &get_deleter() const noexcept { return _deleter; }
	explicit operator bool() const noexcept { return _ptr != pointer(); }

	pointer release() noexcept {
		pointer __t = _ptr;
		_ptr = pointer();
		return __t;
	}
	void reset(pointer __p = pointer()) noexcept {
		pointer __old = _ptr;
		_ptr = __p;
		if (__old) {
			_deleter(__old);
		}
	}
	void swap(unique_ptr &__u) noexcept {
		pointer __p = _ptr;
		_ptr = __u._ptr;
		__u._ptr = __p;
		sprt::swap(_deleter, __u._deleter);
	}
};

// ---------------------------------------------------------- array
template <typename _Tp, typename _Dp>
class unique_ptr<_Tp[], _Dp> {
public:
	using pointer = typename __up_pointer<_Tp, _Dp>::type;
	using element_type = _Tp;
	using deleter_type = _Dp;

private:
	pointer _ptr;
	SPRT_NO_UNIQUE_ADDRESS _Dp _deleter;

public:
	constexpr unique_ptr() noexcept : _ptr(), _deleter() {
		static_assert(!is_pointer_v<_Dp>, "unique_ptr with a pointer deleter needs the deleter");
	}
	constexpr unique_ptr(nullptr_t) noexcept : _ptr(), _deleter() { }
	explicit unique_ptr(pointer __p) noexcept : _ptr(__p), _deleter() { }

	unique_ptr(pointer __p, const _Dp &__d) noexcept : _ptr(__p), _deleter(__d) { }
	unique_ptr(pointer __p, _Dp &&__d) noexcept : _ptr(__p), _deleter(sprt::move_unsafe(__d)) { }

	unique_ptr(unique_ptr &&__u) noexcept
	: _ptr(__u.release()), _deleter(sprt::forward<_Dp>(__u.get_deleter())) { }

	unique_ptr(const unique_ptr &) = delete;
	unique_ptr &operator=(const unique_ptr &) = delete;

	~unique_ptr() {
		if (_ptr) {
			_deleter(_ptr);
		}
	}

	unique_ptr &operator=(unique_ptr &&__u) noexcept {
		reset(__u.release());
		_deleter = sprt::forward<_Dp>(__u.get_deleter());
		return *this;
	}
	unique_ptr &operator=(nullptr_t) noexcept {
		reset();
		return *this;
	}

	_Tp &operator[](size_t __i) const { return _ptr[__i]; }
	pointer get() const noexcept { return _ptr; }
	_Dp &get_deleter() noexcept { return _deleter; }
	const _Dp &get_deleter() const noexcept { return _deleter; }
	explicit operator bool() const noexcept { return _ptr != pointer(); }

	pointer release() noexcept {
		pointer __t = _ptr;
		_ptr = pointer();
		return __t;
	}
	void reset(pointer __p = pointer()) noexcept {
		pointer __old = _ptr;
		_ptr = __p;
		if (__old) {
			_deleter(__old);
		}
	}
	void swap(unique_ptr &__u) noexcept {
		pointer __p = _ptr;
		_ptr = __u._ptr;
		__u._ptr = __p;
		sprt::swap(_deleter, __u._deleter);
	}
};

// ---------------------------------------------------------- make_unique
template <typename _Tp, typename... _Args>
inline enable_if_t<!is_array_v<_Tp>, unique_ptr<_Tp>> make_unique(_Args &&...__args) {
	return unique_ptr<_Tp>(new _Tp(sprt::forward<_Args>(__args)...));
}
template <typename _Tp>
inline enable_if_t<is_array_v<_Tp> && extent_v<_Tp> == 0, unique_ptr<_Tp>> make_unique(size_t __n) {
	return unique_ptr<_Tp>(new remove_extent_t<_Tp>[__n]());
}
template <typename _Tp, typename... _Args>
enable_if_t<extent_v<_Tp> != 0> make_unique(_Args &&...) = delete;

template <typename _Tp>
inline enable_if_t<!is_array_v<_Tp>, unique_ptr<_Tp>> make_unique_for_overwrite() {
	return unique_ptr<_Tp>(new _Tp);
}
template <typename _Tp>
inline enable_if_t<is_array_v<_Tp> && extent_v<_Tp> == 0, unique_ptr<_Tp>>
make_unique_for_overwrite(size_t __n) {
	return unique_ptr<_Tp>(new remove_extent_t<_Tp>[__n]);
}
template <typename _Tp, typename... _Args>
enable_if_t<extent_v<_Tp> != 0> make_unique_for_overwrite(_Args &&...) = delete;

// ---------------------------------------------------------- swap / compare / hash
template <typename _Tp, typename _Dp>
inline void swap(unique_ptr<_Tp, _Dp> &__a, unique_ptr<_Tp, _Dp> &__b) noexcept {
	__a.swap(__b);
}

template <typename _T1, typename _D1, typename _T2, typename _D2>
inline bool operator==(const unique_ptr<_T1, _D1> &__a, const unique_ptr<_T2, _D2> &__b) {
	return __a.get() == __b.get();
}
template <typename _T1, typename _D1, typename _T2, typename _D2>
inline auto operator<=>(const unique_ptr<_T1, _D1> &__a, const unique_ptr<_T2, _D2> &__b) {
	return __a.get() <=> __b.get();
}
template <typename _Tp, typename _Dp>
inline bool operator==(const unique_ptr<_Tp, _Dp> &__a, nullptr_t) noexcept {
	return !__a;
}
template <typename _Tp, typename _Dp>
inline strong_ordering operator<=>(const unique_ptr<_Tp, _Dp> &__a, nullptr_t) {
	return __a.get() <=> static_cast<typename unique_ptr<_Tp, _Dp>::pointer>(nullptr);
}

template <typename _Tp, typename _Dp>
struct hash<unique_ptr<_Tp, _Dp>> {
	size_t operator()(const unique_ptr<_Tp, _Dp> &__p) const noexcept {
		return hash<typename unique_ptr<_Tp, _Dp>::pointer>()(__p.get());
	}
};

} // namespace sprt

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif // RUNTIME_INCLUDE_SPRT_CXX___MEMORY_UNIQUE_PTR_H_
