/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_ATOMIC_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_ATOMIC_H_

#include <sprt/runtime/int.h>
#include <sprt/runtime/detail/constructable.h>

namespace sprt {

enum class memory_order : int {
	relaxed = __ATOMIC_RELAXED,
	consume = __ATOMIC_CONSUME,
	acquire = __ATOMIC_ACQUIRE,
	release = __ATOMIC_RELEASE,
	acq_rel = __ATOMIC_ACQ_REL,
	seq_cst = __ATOMIC_SEQ_CST
};

template <typename Type>
struct ___atomic_base_impl {
	constexpr ___atomic_base_impl() noexcept = default;
	constexpr explicit ___atomic_base_impl(const Type &value) noexcept : _value(value) { }

	_Atomic(Type) _value;
};

template <typename Type, bool = is_integral<Type>::value && !is_same<Type, bool>::value>
struct __atomic_base {
	mutable ___atomic_base_impl<Type> _storage;

	using value_type = Type;

	static constexpr bool is_always_lock_free =
			__atomic_always_lock_free(sizeof(_storage), nullptr);

	void store(Type __d, memory_order __m = memory_order::seq_cst) volatile noexcept {
		__c11_atomic_store(addressof(_storage._value), __d, toInt(__m));
	}
	void store(Type __d, memory_order __m = memory_order::seq_cst) noexcept {
		__c11_atomic_store(addressof(_storage._value), __d, toInt(__m));
	}
	Type load(memory_order __m = memory_order::seq_cst) const volatile noexcept {
		return __c11_atomic_load(addressof(_storage._value), toInt(__m));
	}
	Type load(memory_order __m = memory_order::seq_cst) const noexcept {
		return __c11_atomic_load(addressof(_storage._value), toInt(__m));
	}

	operator Type() const volatile noexcept { return load(); }
	operator Type() const noexcept { return load(); }

	Type exchange(Type __d, memory_order __m = memory_order::seq_cst) volatile noexcept {
		return __c11_atomic_exchange(addressof(_storage._value), __d, toInt(__m));
	}
	Type exchange(Type __d, memory_order __m = memory_order::seq_cst) noexcept {
		return __c11_atomic_exchange(addressof(_storage._value), __d, toInt(__m));
	}

	bool compare_exchange_weak(Type &__e, Type __d, memory_order __s,
			memory_order __f) volatile noexcept {
		return __c11_atomic_compare_exchange_weak(addressof(_storage._value), addressof(__e), __d,
				toInt(__s), toInt(__f));
	}
	bool compare_exchange_weak(Type &__e, Type __d, memory_order __s, memory_order __f) noexcept {
		return __c11_atomic_compare_exchange_weak(addressof(_storage._value), addressof(__e), __d,
				toInt(__s), toInt(__f));
	}
	bool compare_exchange_strong(Type &__e, Type __d, memory_order __s,
			memory_order __f) volatile noexcept {
		return __c11_atomic_compare_exchange_strong(addressof(_storage._value), addressof(__e), __d,
				toInt(__s), toInt(__f));
	}
	bool compare_exchange_strong(Type &__e, Type __d, memory_order __s, memory_order __f) noexcept {
		return __c11_atomic_compare_exchange_strong(addressof(_storage._value), addressof(__e), __d,
				toInt(__s), toInt(__f));
	}
	bool compare_exchange_weak(Type &__e, Type __d,
			memory_order __m = memory_order::seq_cst) volatile noexcept {
		return __c11_atomic_compare_exchange_weak(addressof(_storage._value), addressof(__e), __d,
				toInt(__m), toInt(__m));
	}
	bool compare_exchange_weak(Type &__e, Type __d,
			memory_order __m = memory_order::seq_cst) noexcept {
		return __c11_atomic_compare_exchange_weak(addressof(_storage._value), addressof(__e), __d,
				toInt(__m), toInt(__m));
	}
	bool compare_exchange_strong(Type &__e, Type __d,
			memory_order __m = memory_order::seq_cst) volatile noexcept {
		return __c11_atomic_compare_exchange_strong(addressof(_storage._value), addressof(__e), __d,
				toInt(__m), toInt(__m));
	}
	bool compare_exchange_strong(Type &__e, Type __d,
			memory_order __m = memory_order::seq_cst) noexcept {
		return __c11_atomic_compare_exchange_strong(addressof(_storage._value), addressof(__e), __d,
				toInt(__m), toInt(__m));
	}

	__atomic_base() noexcept = default;

	constexpr __atomic_base(const Type &__d) noexcept : _storage(__d) { }

	__atomic_base(const __atomic_base &) = delete;
};

template <typename Type>
struct __atomic_base<Type, true> : public __atomic_base<Type, false> {
	using __base = __atomic_base<Type, false>;
	using difference_type = typename __base::value_type;

	constexpr __atomic_base() noexcept = default;

	constexpr __atomic_base(Type __d) noexcept : __base(__d) { }

	Type fetch_add(Type __op, memory_order __m = memory_order::seq_cst) volatile noexcept {
		return __c11_atomic_fetch_add(addressof(this->_storage._value), __op, toInt(__m));
	}
	Type fetch_add(Type __op, memory_order __m = memory_order::seq_cst) noexcept {
		return __c11_atomic_fetch_add(addressof(this->_storage._value), __op, toInt(__m));
	}
	Type fetch_sub(Type __op, memory_order __m = memory_order::seq_cst) volatile noexcept {
		return __c11_atomic_fetch_sub(addressof(this->_storage._value), __op, toInt(__m));
	}
	Type fetch_sub(Type __op, memory_order __m = memory_order::seq_cst) noexcept {
		return __c11_atomic_fetch_sub(addressof(this->_storage._value), __op, toInt(__m));
	}
	Type fetch_and(Type __op, memory_order __m = memory_order::seq_cst) volatile noexcept {
		return __c11_atomic_fetch_and(addressof(this->_storage._value), __op, toInt(__m));
	}
	Type fetch_and(Type __op, memory_order __m = memory_order::seq_cst) noexcept {
		return __c11_atomic_fetch_and(addressof(this->_storage._value), __op, toInt(__m));
	}
	Type fetch_or(Type __op, memory_order __m = memory_order::seq_cst) volatile noexcept {
		return __c11_atomic_fetch_or(addressof(this->_storage._value), __op, toInt(__m));
	}
	Type fetch_or(Type __op, memory_order __m = memory_order::seq_cst) noexcept {
		return __c11_atomic_fetch_or(addressof(this->_storage._value), __op, toInt(__m));
	}
	Type fetch_xor(Type __op, memory_order __m = memory_order::seq_cst) volatile noexcept {
		return __c11_atomic_fetch_xor(addressof(this->_storage._value), __op, toInt(__m));
	}
	Type fetch_xor(Type __op, memory_order __m = memory_order::seq_cst) noexcept {
		return __c11_atomic_fetch_xor(addressof(this->_storage._value), __op, toInt(__m));
	}

	Type operator++(int) volatile noexcept { return fetch_add(Type(1)); }
	Type operator++(int) noexcept { return fetch_add(Type(1)); }
	Type operator--(int) volatile noexcept { return fetch_sub(Type(1)); }
	Type operator--(int) noexcept { return fetch_sub(Type(1)); }
	Type operator++() volatile noexcept { return fetch_add(Type(1)) + Type(1); }
	Type operator++() noexcept { return fetch_add(Type(1)) + Type(1); }
	Type operator--() volatile noexcept { return fetch_sub(Type(1)) - Type(1); }
	Type operator--() noexcept { return fetch_sub(Type(1)) - Type(1); }
	Type operator+=(Type __op) volatile noexcept { return fetch_add(__op) + __op; }
	Type operator+=(Type __op) noexcept { return fetch_add(__op) + __op; }
	Type operator-=(Type __op) volatile noexcept { return fetch_sub(__op) - __op; }
	Type operator-=(Type __op) noexcept { return fetch_sub(__op) - __op; }
	Type operator&=(Type __op) volatile noexcept { return fetch_and(__op) & __op; }
	Type operator&=(Type __op) noexcept { return fetch_and(__op) & __op; }
	Type operator|=(Type __op) volatile noexcept { return fetch_or(__op) | __op; }
	Type operator|=(Type __op) noexcept { return fetch_or(__op) | __op; }
	Type operator^=(Type __op) volatile noexcept { return fetch_xor(__op) ^ __op; }
	Type operator^=(Type __op) noexcept { return fetch_xor(__op) ^ __op; }
};

template <typename Type>
struct atomic : public __atomic_base<Type> {
	static_assert(is_trivially_copyable<Type>::value,
			"std::atomic<T> requires that 'T' be a trivially copyable type");

	using __base = __atomic_base<Type>;

	atomic() noexcept = default;

	constexpr atomic(Type __d) noexcept : __base(__d) { }

	Type operator=(Type __d) volatile noexcept {
		__base::store(__d);
		return __d;
	}
	Type operator=(Type __d) noexcept {
		__base::store(__d);
		return __d;
	}

	atomic &operator=(const atomic &) = delete;
	atomic &operator=(const atomic &) volatile = delete;
};

template <typename Type>
struct atomic<Type *> : public __atomic_base<Type *> {
	using __base = __atomic_base<Type *>;

	using difference_type = ptrdiff_t;

	atomic() noexcept = default;

	constexpr atomic(Type *__d) noexcept : __base(__d) { }

	Type *operator=(Type *__d) volatile noexcept {
		__base::store(__d);
		return __d;
	}
	Type *operator=(Type *__d) noexcept {
		__base::store(__d);
		return __d;
	}

	Type *fetch_add(ptrdiff_t __op, memory_order __m = memory_order::seq_cst) volatile noexcept {
		// __atomic_fetch_add accepts function pointers, guard against them.
		static_assert(!is_function<remove_pointer_t<Type> >::value,
				"Pointer to function isn't allowed");
		return __c11_atomic_fetch_and(addressof(this->_storage._value), __op, toInt(__m));
	}

	Type *fetch_add(ptrdiff_t __op, memory_order __m = memory_order::seq_cst) noexcept {
		// __atomic_fetch_add accepts function pointers, guard against them.
		static_assert(!is_function<remove_pointer_t<Type> >::value,
				"Pointer to function isn't allowed");
		return __c11_atomic_fetch_and(addressof(this->_storage._value), __op, toInt(__m));
	}

	Type *fetch_sub(ptrdiff_t __op, memory_order __m = memory_order::seq_cst) volatile noexcept {
		// __atomic_fetch_add accepts function pointers, guard against them.
		static_assert(!is_function<remove_pointer_t<Type> >::value,
				"Pointer to function isn't allowed");
		return __c11_atomic_fetch_sub(addressof(this->_storage._value), __op, toInt(__m));
	}

	Type *fetch_sub(ptrdiff_t __op, memory_order __m = memory_order::seq_cst) noexcept {
		// __atomic_fetch_add accepts function pointers, guard against them.
		static_assert(!is_function<remove_pointer_t<Type> >::value,
				"Pointer to function isn't allowed");
		return __c11_atomic_fetch_sub(addressof(this->_storage._value), __op, toInt(__m));
	}

	Type *operator++(int) volatile noexcept { return fetch_add(1); }
	Type *operator++(int) noexcept { return fetch_add(1); }
	Type *operator--(int) volatile noexcept { return fetch_sub(1); }
	Type *operator--(int) noexcept { return fetch_sub(1); }
	Type *operator++() volatile noexcept { return fetch_add(1) + 1; }
	Type *operator++() noexcept { return fetch_add(1) + 1; }
	Type *operator--() volatile noexcept { return fetch_sub(1) - 1; }
	Type *operator--() noexcept { return fetch_sub(1) - 1; }
	Type *operator+=(ptrdiff_t __op) volatile noexcept { return fetch_add(__op) + __op; }
	Type *operator+=(ptrdiff_t __op) noexcept { return fetch_add(__op) + __op; }
	Type *operator-=(ptrdiff_t __op) volatile noexcept { return fetch_sub(__op) - __op; }
	Type *operator-=(ptrdiff_t __op) noexcept { return fetch_sub(__op) - __op; }

	atomic &operator=(const atomic &) = delete;
	atomic &operator=(const atomic &) volatile = delete;
};

struct atomic_flag {
	_Atomic(bool) __a_;

	bool test(memory_order __m = memory_order::seq_cst) const volatile noexcept {
		return true == __c11_atomic_load(&__a_, toInt(__m));
	}
	bool test(memory_order __m = memory_order::seq_cst) const noexcept {
		return true == __c11_atomic_load(&__a_, toInt(__m));
	}

	bool test_and_set(memory_order __m = memory_order::seq_cst) volatile noexcept {
		return __c11_atomic_exchange(&__a_, true, toInt(__m));
	}
	bool test_and_set(memory_order __m = memory_order::seq_cst) noexcept {
		return __c11_atomic_exchange(&__a_, true, toInt(__m));
	}
	void clear(memory_order __m = memory_order::seq_cst) volatile noexcept {
		__c11_atomic_store(&__a_, false, toInt(__m));
	}
	void clear(memory_order __m = memory_order::seq_cst) noexcept {
		__c11_atomic_store(&__a_, false, toInt(__m));
	}

	constexpr atomic_flag() noexcept : __a_(false) { }

	atomic_flag(const atomic_flag &) = delete;
	atomic_flag &operator=(const atomic_flag &) = delete;
	atomic_flag &operator=(const atomic_flag &) volatile = delete;
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_ATOMIC_H_
