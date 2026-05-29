/**
 Copyright (c) 2026 Xenolith Team Team <admin@xenolith.studio>

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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___MEMORY_ALLOCATOR_TRAITS_H_
#define RUNTIME_INCLUDE_SPRT_CXX___MEMORY_ALLOCATOR_TRAITS_H_

#include <sprt/cxx/__memory/common.h>
#include <sprt/cxx/__memory/pointer_traits.h>
#include <sprt/cxx/__type_traits/modifications.h>
#include <sprt/cxx/__type_traits/constants.h>

namespace sprt {

template <typename _Tp>
using __pointer_member = typename _Tp::pointer;

template <typename _Tp, typename _Alloc>
using __pointer = __detected_or_t<_Tp *, __pointer_member, remove_reference_t<_Alloc> >;

template <typename _Alloc, template <typename> typename _Alias, typename _Ptr, typename _Tp,
		typename = void>
struct __rebind_or_alias_pointer {
	using type = typename pointer_traits<_Ptr>::template rebind<_Tp>::other;
};

template <typename _Ptr, typename _Alloc, typename _Tp, template <typename> typename _Alias>
struct __rebind_or_alias_pointer<_Alloc, _Alias, _Ptr, _Tp, void_t<_Alias<_Alloc> > > {
	using type = _Alias<_Alloc>;
};

template <typename _Alloc>
using __const_pointer_member = typename _Alloc::const_pointer;

template <typename _Tp, typename _Ptr, typename _Alloc>
using __const_pointer_t =
		typename __rebind_or_alias_pointer<_Alloc, __const_pointer_member, _Ptr, const _Tp>::type;

template <typename _Alloc>
using __void_pointer_member = typename _Alloc::void_pointer;

template <typename _Ptr, typename _Alloc>
using __void_pointer_t =
		typename __rebind_or_alias_pointer<_Alloc, __void_pointer_member, _Ptr, void>::type;

template <typename _Alloc>
using __const_void_pointer_member = typename _Alloc::const_void_pointer;

template <typename _Ptr, typename _Alloc>
using __const_void_pointer_t = typename __rebind_or_alias_pointer<_Alloc,
		__const_void_pointer_member, _Ptr, const void>::type;

// __size_type
template <typename _Tp>
using __size_type_member = typename _Tp::size_type;

template <typename _Alloc, typename _DiffType>
using __size_type = __detected_or_t<make_unsigned_t<_DiffType>, __size_type_member, _Alloc>;

// __alloc_traits_difference_type
template <typename _Alloc, typename _Ptr, typename = void>
struct __alloc_traits_difference_type {
	using type = typename pointer_traits<_Ptr>::difference_type;
};

template <typename _Alloc, typename _Ptr>
struct __alloc_traits_difference_type<_Alloc, _Ptr, void_t<typename _Alloc::difference_type> > {
	using type = typename _Alloc::difference_type;
};

// __propagate_on_container_copy_assignment
template <typename _Tp>
using __propagate_on_container_copy_assignment_member =
		typename _Tp::propagate_on_container_copy_assignment;

template <typename _Alloc>
using __propagate_on_container_copy_assignment =
		__detected_or_t<false_type, __propagate_on_container_copy_assignment_member, _Alloc>;

// __propagate_on_container_move_assignment
template <typename _Tp>
using __propagate_on_container_move_assignment_member =
		typename _Tp::propagate_on_container_move_assignment;

template <typename _Alloc>
using __propagate_on_container_move_assignment =
		__detected_or_t<false_type, __propagate_on_container_move_assignment_member, _Alloc>;

// __propagate_on_container_swap
template <typename _Tp>
using __propagate_on_container_swap_member = typename _Tp::propagate_on_container_swap;

template <typename _Alloc>
using __propagate_on_container_swap =
		__detected_or_t<false_type, __propagate_on_container_swap_member, _Alloc>;

// __allocator_traits_rebind
template <typename _Tp, typename _Up, typename = void>
inline const bool __has_rebind_other_v = false;
template <typename _Tp, typename _Up>
inline const bool
		__has_rebind_other_v<_Tp, _Up, void_t<typename _Tp::template rebind<_Up>::other> > = true;

template <typename _Tp, typename _Up, bool = __has_rebind_other_v<_Tp, _Up> >
struct __allocator_traits_rebind {
	static_assert(__has_rebind_other_v<_Tp, _Up>, "This allocator has to implement rebind");
	using type = typename _Tp::template rebind<_Up>::other;
};
template <template <typename, typename...> typename _Alloc, typename _Tp, typename... _Args,
		typename _Up>
struct __allocator_traits_rebind<_Alloc<_Tp, _Args...>, _Up, true> {
	using type = typename _Alloc<_Tp, _Args...>::template rebind<_Up>::other;
};
template <template <typename, typename...> typename _Alloc, typename _Tp, typename... _Args,
		typename _Up>
struct __allocator_traits_rebind<_Alloc<_Tp, _Args...>, _Up, false> {
	using type = _Alloc<_Up, _Args...>;
};

template <typename _Alloc, typename _Tp>
using __allocator_traits_rebind_t = typename __allocator_traits_rebind<_Alloc, _Tp>::type;


// __has_allocate_hint_v
template <typename _Alloc, typename _SizeType, typename _ConstVoidPtr, typename = void>
inline const bool __has_allocate_hint_v = false;

template <typename _Alloc, typename _SizeType, typename _ConstVoidPtr>
inline const bool __has_allocate_hint_v< _Alloc, _SizeType, _ConstVoidPtr,
		decltype((void)sprt::declval<_Alloc>().allocate(sprt::declval<_SizeType>(),
				sprt::declval<_ConstVoidPtr>()))> = true;

// __has_construct_v
template <typename, typename _Alloc, typename... _Args>
inline const bool __has_construct_impl = false;

template <typename _Alloc, typename... _Args>
inline const bool __has_construct_impl<decltype((void)sprt::declval<_Alloc>().construct(
											   sprt::declval<_Args>()...)),
		_Alloc, _Args...> = true;

template <typename _Alloc, typename... _Args>
inline const bool __has_construct_v = __has_construct_impl<void, _Alloc, _Args...>;

// __has_destroy_v
template <typename _Alloc, typename _Pointer, typename = void>
inline const bool __has_destroy_v = false;

template <typename _Alloc, typename _Pointer>
inline const bool __has_destroy_v<_Alloc, _Pointer,
		decltype((void)sprt::declval<_Alloc>().destroy(sprt::declval<_Pointer>()))> = true;

// __has_max_size_v
template <typename _Alloc, typename = void>
inline const bool __has_max_size_v = false;

template <typename _Alloc>
inline const bool __has_max_size_v<_Alloc, decltype((void)sprt::declval<_Alloc &>().max_size())> =
		true;

// __has_select_on_container_copy_construction_v
template <typename _Alloc, typename = void>
inline const bool __has_select_on_container_copy_construction_v = false;

template <typename _Alloc>
inline const bool __has_select_on_container_copy_construction_v< _Alloc,
		decltype((void)sprt::declval<_Alloc>().select_on_container_copy_construction())> = true;

template <typename _Alloc>
struct allocator_traits {
	using allocator_type = _Alloc;
	using value_type = typename allocator_type::value_type;
	using pointer = __pointer<value_type, allocator_type>;
	using const_pointer = __const_pointer_t<value_type, pointer, allocator_type>;
	using void_pointer = __void_pointer_t<pointer, allocator_type>;
	using const_void_pointer = __const_void_pointer_t<pointer, allocator_type>;
	using difference_type = typename __alloc_traits_difference_type<allocator_type, pointer>::type;
	using size_type = __size_type<allocator_type, difference_type>;
	using propagate_on_container_copy_assignment =
			__propagate_on_container_copy_assignment<allocator_type>;
	using propagate_on_container_move_assignment =
			__propagate_on_container_move_assignment<allocator_type>;
	using propagate_on_container_swap = __propagate_on_container_swap<allocator_type>;

	template <typename _Tp>
	using rebind_alloc = __allocator_traits_rebind_t<allocator_type, _Tp>;
	template <typename _Tp>
	using rebind_traits = allocator_traits<rebind_alloc<_Tp> >;

	[[nodiscard]]
	constexpr static pointer allocate(allocator_type &__a, size_type __n) {
		return __a.allocate(__n);
	}

	template <typename _Ap = _Alloc,
			enable_if_t<__has_allocate_hint_v<_Ap, size_type, const_void_pointer>, int> = 0>
	[[nodiscard]]
	constexpr static pointer allocate(allocator_type &__a, size_type __n,
			const_void_pointer __hint) {
		return __a.allocate(__n, __hint);
	}
	template <typename _Ap = _Alloc,
			enable_if_t<!__has_allocate_hint_v<_Ap, size_type, const_void_pointer>, int> = 0>
	[[nodiscard]]
	constexpr static pointer allocate(allocator_type &__a, size_type __n, const_void_pointer) {
		return __a.allocate(__n);
	}

	constexpr static void deallocate(allocator_type &__a, pointer __p, size_type __n) noexcept {
		__a.deallocate(__p, __n);
	}

	template <typename _Tp, typename... _Args,
			enable_if_t<__has_construct_v<allocator_type, _Tp *, _Args...>, int> = 0>
	constexpr static void construct(allocator_type &__a, _Tp *__p, _Args &&...__args) {
		__a.construct(__p, sprt::forward<_Args>(__args)...);
	}
	template <typename _Tp, typename... _Args,
			enable_if_t<!__has_construct_v<allocator_type, _Tp *, _Args...>, int> = 0>
	constexpr static void construct(allocator_type &, _Tp *__p, _Args &&...__args) {
		sprt::construct_at(__p, sprt::forward<_Args>(__args)...);
	}

	template <typename _Tp, enable_if_t<__has_destroy_v<allocator_type, _Tp *>, int> = 0>
	constexpr static void destroy(allocator_type &__a, _Tp *__p) {
		__a.destroy(__p);
	}
	template <typename _Tp, enable_if_t<!__has_destroy_v<allocator_type, _Tp *>, int> = 0>
	constexpr static void destroy(allocator_type &, _Tp *__p) {
		sprt::destroy_at(__p);
	}

	template <typename _Ap = _Alloc, enable_if_t<__has_max_size_v<const _Ap>, int> = 0>
	constexpr static size_type max_size(const allocator_type &__a) noexcept {
		return __a.max_size();
	}
	template <typename _Ap = _Alloc, enable_if_t<!__has_max_size_v<const _Ap>, int> = 0>
	constexpr static size_type max_size(const allocator_type &) noexcept {
		return Max<size_type> / sizeof(value_type);
	}

	template <typename _Ap = _Alloc,
			enable_if_t<__has_select_on_container_copy_construction_v<const _Ap>, int> = 0>
	constexpr static allocator_type select_on_container_copy_construction(
			const allocator_type &__a) {
		return __a.select_on_container_copy_construction();
	}
	template <typename _Ap = _Alloc,
			enable_if_t<!__has_select_on_container_copy_construction_v<const _Ap>, int> = 0>
	constexpr static allocator_type select_on_container_copy_construction(
			const allocator_type &__a) {
		return __a;
	}
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___MEMORY_ALLOCATOR_TRAITS_H_
