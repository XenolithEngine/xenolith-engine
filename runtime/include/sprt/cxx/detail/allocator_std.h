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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_DETAIL_ALLOCATOR_STD_H_
#define RUNTIME_INCLUDE_SPRT_CXX_DETAIL_ALLOCATOR_STD_H_

#include <sprt/cxx/__memory/allocator_traits.h>
#include <sprt/cxx/__memory/pointer_traits.h>
#include <sprt/cxx/__utility/common.h>

namespace sprt::detail {

// Detects the extended sprt allocator protocol (AllocatorPool / AllocatorMalloc /
// sprt's std::allocator): the EBO container-base hook plus the __allocate /
// __deallocate entry points. A purely standard C++ allocator (only
// allocate/deallocate + allocator_traits, possibly with fancy pointers) does not
// have it and is routed through AllocatorStd below.
template <typename A>
concept __sprt_extended_allocator = requires { typename A::base_class; };

// Empty EBO base for containers instantiated with a purely standard allocator
// (sprt::AllocBase from <sprt/cxx/new> is not used here to keep this header
// free of the <new> dependency; the base only has to be empty).
struct AllocatorStdBase { };

template <typename A>
class AllocatorStd;

template <typename A>
struct __is_allocator_std : false_type { };

template <typename A>
struct __is_allocator_std<AllocatorStd<A>> : true_type { };

// Minimal [container.adaptors]-style allocator detector used to disambiguate the container
// deduction guides (an allocator has a value_type and an allocate(size_t)); a hasher/predicate
// in the same argument position does not, so the allocator-position guides SFINAE out for them.
template <typename A, typename = void>
inline constexpr bool __is_allocator_v = false;
template <typename A>
inline constexpr bool __is_allocator_v<A,
		void_t<typename A::value_type, decltype(sprt::declval<A &>().allocate(size_t(0)))>> = true;

// AllocatorStd adapts a purely standard C++ allocator to the extended sprt
// allocator protocol expected by linear_memory and the node-based containers.
// The container-facing surface works on raw Type* (fancy pointers from the
// underlying allocator are unwrapped on allocate and rebuilt on deallocate,
// like the RbTree standard-allocator path), and the byte-size out-parameters
// of __allocate/__deallocate stay symmetric: __allocate reports exactly
// n * sizeof(T) bytes, __deallocate derives the element count back from them,
// so allocator_traits<A>::deallocate always sees the same n as allocate.
template <typename A>
class AllocatorStd {
public:
	using base_class = AllocatorStdBase;

	using __alloc_traits = sprt::allocator_traits<A>;
	using __alloc_pointer = typename __alloc_traits::pointer;

	using value_type = typename __alloc_traits::value_type;
	using pointer = value_type *;
	using const_pointer = const value_type *;
	using reference = value_type &;
	using const_reference = const value_type &;
	using size_type = size_t;
	using difference_type = ptrdiff_t;

	// Forward the allocator propagation traits from the wrapped allocator so the container's
	// copy/move-assignment and swap propagate it exactly as the user's own allocator would.
	using propagate_on_container_copy_assignment =
			typename __alloc_traits::propagate_on_container_copy_assignment;
	using propagate_on_container_move_assignment =
			typename __alloc_traits::propagate_on_container_move_assignment;
	using propagate_on_container_swap = typename __alloc_traits::propagate_on_container_swap;
	using is_always_equal = typename __alloc_traits::is_always_equal;

	template <typename U>
	struct rebind {
		using other = AllocatorStd<typename __alloc_traits::template rebind_alloc<U>>;
	};

	constexpr AllocatorStd() noexcept = default;
	constexpr AllocatorStd(const A &a) noexcept : _alloc(a) { }
	constexpr AllocatorStd(A &&a) noexcept : _alloc(sprt::move_unsafe(a)) { }

	// Rebinding copy (AllocatorStd<A<U>> -> AllocatorStd<A<T>>), mirrors the
	// converting constructors every standard allocator provides.
	template <typename B>
	requires (is_constructible_v<A, const B &>)
	constexpr AllocatorStd(const AllocatorStd<B> &other) noexcept : _alloc(other.get()) { }

	// Direct wrap of a differently-bound standard allocator (A<U> -> AllocatorStd<A<T>>),
	// so a container can hand its value allocator straight to a node-allocator slot
	// in a single implicit conversion.
	template <typename B>
	requires (!is_same_v<B, A> && !__is_allocator_std<B>::value && is_constructible_v<A, const B &>)
	constexpr AllocatorStd(const B &b) noexcept : _alloc(b) { }

	constexpr const A &get() const noexcept { return _alloc; }

	constexpr value_type *allocate(size_type n) const {
		return sprt::addressof(*__alloc_traits::allocate(_alloc, n));
	}
	constexpr value_type *__allocate(size_type &n) const { return allocate(n); }
	constexpr value_type *__allocate(size_type n, size_type &bytes) const {
		bytes = n * sizeof(value_type);
		return allocate(n);
	}
	constexpr void deallocate(value_type *p, size_type n) const {
		__alloc_traits::deallocate(_alloc, __to_alloc_pointer(p), n);
	}
	constexpr void __deallocate(value_type *p, size_type, size_type bytes) const {
		// The element count is recovered from the byte size reported by
		// __allocate (see the class comment), not from the caller's n: node
		// containers pass logical counts here that may not match the block.
		deallocate(p, bytes / sizeof(value_type));
	}

	template <typename... Args>
	constexpr void construct(value_type *p, Args &&...args) const {
		__alloc_traits::construct(_alloc, p, sprt::forward<Args>(args)...);
	}

	constexpr void destroy(value_type *p) const { __alloc_traits::destroy(_alloc, p); }
	constexpr void destroy(value_type *p, size_type n) const {
		for (size_type i = 0; i < n; ++i) { __alloc_traits::destroy(_alloc, p + i); }
	}

	constexpr pointer address(reference r) const noexcept { return sprt::addressof(r); }
	// For const value_type the two overloads would collapse into one signature.
	constexpr const_pointer address(const_reference r) const noexcept
			requires (!is_same_v<reference, const_reference>)
	{
		return sprt::addressof(r);
	}

	// Forward to the wrapped allocator so a capacity-limited test allocator's max_size is honoured.
	constexpr size_type max_size() const noexcept { return __alloc_traits::max_size(_alloc); }

	template <typename B>
	constexpr bool operator==(const AllocatorStd<B> &other) const noexcept {
		if constexpr (is_same_v<A, B>) {
			return _alloc == other.get();
		} else {
			// Compare through a rebound copy; standard allocators of one family
			// are constructible from and comparable with each other this way.
			return _alloc == A(other.get());
		}
	}

	// The extended protocol requires a validity check (pool allocators can be
	// pool-less); a standard allocator is always usable.
	constexpr explicit operator bool() const noexcept { return true; }

private:
	static constexpr __alloc_pointer __to_alloc_pointer(value_type *p) noexcept {
		if constexpr (is_pointer_v<__alloc_pointer>) {
			return p;
		} else {
			return sprt::pointer_traits<__alloc_pointer>::pointer_to(*p);
		}
	}

	// The extended protocol calls allocation on a const allocator; the standard
	// one takes it by non-const reference.
	SPRT_NO_UNIQUE_ADDRESS
	mutable A _alloc;
};

// The conditional container base: the allocator's own EBO hook for extended
// allocators, an empty base for standard ones.
template <typename A>
struct __allocator_base {
	using type = AllocatorStdBase;
};

template <__sprt_extended_allocator A>
struct __allocator_base<A> {
	using type = typename A::base_class;
};

template <typename A>
using __allocator_base_t = typename __allocator_base<A>::type;

// The allocator a container should actually run its storage machinery on:
// extended allocators pass through, standard ones get wrapped.
template <typename A>
using __select_sprt_allocator = conditional_t<__sprt_extended_allocator<A>, A, AllocatorStd<A>>;

} // namespace sprt::detail

#endif // RUNTIME_INCLUDE_SPRT_CXX_DETAIL_ALLOCATOR_STD_H_
