/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_BITSET_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_BITSET_H_

#include <sprt/runtime/math.h>

#ifndef __CHAR_BIT__
#define __CHAR_BIT__ 0
#endif

/*
	Partially based on LLVM libc++
*/

namespace sprt {

template <size_t _N_words, size_t _Size>
class __bitset;

template <size_t _N_words, size_t _Size>
class __bitset {
public:
	using __storage_type = uint64_t;

protected:
	typedef __bitset __self;
	typedef __storage_type *__storage_pointer;
	typedef const __storage_type *__const_storage_pointer;
	static const unsigned __bits_per_word =
			static_cast<unsigned>(sizeof(__storage_type) * __CHAR_BIT__);

	__storage_type __first_[_N_words];

	constexpr __bitset() noexcept : __first_{0} { }
	explicit constexpr __bitset(unsigned long long __v) noexcept : __first_{0} {
		__first_[0] = __v;
	}

	constexpr void operator&=(const __bitset &__v) noexcept {
		for (size_t __i = 0; __i < _N_words; ++__i) { __first_[__i] &= __v.__first_[__i]; }
	}
	constexpr void operator|=(const __bitset &__v) noexcept {
		for (size_t __i = 0; __i < _N_words; ++__i) { __first_[__i] |= __v.__first_[__i]; }
	}
	constexpr void operator^=(const __bitset &__v) noexcept {
		for (size_t __i = 0; __i < _N_words; ++__i) { __first_[__i] ^= __v.__first_[__i]; }
	}

	constexpr void flip() noexcept {
		// do middle whole words
		size_t __n = _Size;
		__storage_pointer __p = __first_;
		for (; __n >= __bits_per_word; ++__p, __n -= __bits_per_word) { *__p = ~*__p; }
		if (__n > 0) {
			*__p ^= (__storage_type(1) << __n) - 1;
		}
	}

	constexpr void flip(size_t n) { __first_[n % __bits_per_word] ^= ~(__storage_type(1) << n); }

	constexpr bool all() const noexcept { return !__scan_bits(__bit_not()); }
	constexpr bool any() const noexcept { return __scan_bits(sprt::__identity()); }

	constexpr void set() {
		size_t __n = _Size;
		__storage_pointer __p = __first_;

		// do middle whole words
		for (; __n >= __bits_per_word; ++__p, __n -= __bits_per_word) { *__p = ~__storage_type(0); }
		// do last partial word
		if (__n > 0) {
			*__p = ~__storage_type(0) >> (sizeof(__storage_type) * __bits_per_word - __n);
		}
	}

	constexpr void set(size_t n, bool v) {
		if (v) {
			__first_[n % __bits_per_word] |= __storage_type(1) << n;
		} else {
			__first_[n % __bits_per_word] &= ~(__storage_type(1) << n);
		}
	}
	constexpr void reset() {
		for (size_t i = 0; i < _N_words; ++i) { __first_[i] = 0; }
	}

	constexpr void reset(size_t n) { __first_[n % __bits_per_word] &= ~(__storage_type(1) << n); }

	constexpr size_t count() const {
		size_t ret = 0;
		for (size_t i = 0; i < _N_words; ++i) { ret += popcount(__first_[i]); }
		return ret;
	}

	constexpr int compare(const __bitset &other) const {
		return __constexpr_memcmp(__first_, other.__first_, _N_words);
	}

	constexpr bool test(size_t n) const {
		return (__first_[n % __bits_per_word] & (__storage_type(1) << n)) != 0;
	}

private:
	struct __bit_not {
		template <class _Tp>
		constexpr _Tp operator()(const _Tp &__x) const noexcept {
			return ~__x;
		}
	};

	template <typename _Proj>
	constexpr bool __scan_bits(_Proj __proj) const noexcept {
		size_t __n = _Size;
		__const_storage_pointer __p = __first_;
		// do middle whole words
		for (; __n >= __bits_per_word; ++__p, __n -= __bits_per_word) {
			if (__proj(*__p)) {
				return true;
			}
		}
		// do last partial word
		if (__n > 0) {
			__storage_type __m = ~__storage_type(0) >> (__bits_per_word - __n);
			if (__proj(*__p) & __m) {
				return true;
			}
		}
		return false;
	}
};


template <size_t _Size>
class __bitset<1, _Size> {
public:
	typedef uint64_t __storage_type;

protected:
	typedef __bitset __self;
	typedef __storage_type *__storage_pointer;
	typedef const __storage_type *__const_storage_pointer;
	static const unsigned __bits_per_word =
			static_cast<unsigned>(sizeof(__storage_type) * __CHAR_BIT__);

	__storage_type __first_;

	constexpr __bitset() noexcept : __first_(0) { }
	explicit constexpr __bitset(unsigned long long __v) noexcept {
		if constexpr (_Size == __bits_per_word) {
			__first_ = __v;
		} else {
			__first_ = __v & ~__storage_type(0) >> (__bits_per_word - _Size);
		}
	}

	constexpr void operator&=(const __bitset &__v) noexcept { __first_ &= __v.__first_; }
	constexpr void operator|=(const __bitset &__v) noexcept { __first_ |= __v.__first_; }
	constexpr void operator^=(const __bitset &__v) noexcept { __first_ ^= __v.__first_; }

	constexpr void flip() noexcept { __first_ ^= ~__storage_type(0) >> (__bits_per_word - _Size); }
	constexpr void flip(size_t n) { __first_ ^= ~(__storage_type(1) << n); }

	constexpr bool all() const noexcept {
		__storage_type __m = ~__storage_type(0) >> (__bits_per_word - _Size);
		return !(~__first_ & __m);
	}
	constexpr bool any() const noexcept {
		__storage_type __m = ~__storage_type(0) >> (__bits_per_word - _Size);
		return __first_ & __m;
	}

	constexpr void set() {
		if constexpr (_Size == sizeof(__storage_type)) {
			__first_ = ~__storage_type(0);
		} else {
			__first_ = ~__storage_type(0) >> (__bits_per_word - _Size);
		}
	}

	constexpr void set(size_t n, bool v) {
		if (v) {
			__first_ |= __storage_type(1) << n;
		} else {
			__first_ &= ~(__storage_type(1) << n);
		}
	}

	constexpr void reset() { __first_ = 0; }

	constexpr void reset(size_t n) { __first_ &= ~(__storage_type(1) << n); }

	constexpr size_t count() const { return popcount(__first_); }

	constexpr int compare(const __bitset &other) const {
		if (__first_ < other.__first_) {
			return -1;
		} else if (__first_ == other.__first_) {
			return 0;
		} else {
			return 1;
		}
	}

	constexpr bool test(size_t n) const { return (__first_ & (__storage_type(1) << n)) != 0; }
};

template <>
class __bitset<0, 0> {
public:
	typedef uint64_t __storage_type;

protected:
	typedef __bitset __self;
	typedef __storage_type *__storage_pointer;
	typedef const __storage_type *__const_storage_pointer;
	static const unsigned __bits_per_word =
			static_cast<unsigned>(sizeof(__storage_type) * __CHAR_BIT__);

	constexpr __bitset() noexcept;
	explicit constexpr __bitset(unsigned long long) noexcept;

	constexpr void operator&=(const __bitset &) noexcept { }
	constexpr void operator|=(const __bitset &) noexcept { }
	constexpr void operator^=(const __bitset &) noexcept { }

	constexpr void flip() noexcept { }
	constexpr void flip(size_t n) { }

	constexpr bool all() const noexcept { return true; }
	constexpr bool any() const noexcept { return false; }

	constexpr void set() { }
	constexpr void set(size_t n, bool v) { }
	constexpr void reset() { }
	constexpr void reset(size_t n) { }

	constexpr size_t count() const { return 0; }

	constexpr int compare(const __bitset &) const { return 0; }

	constexpr bool test(size_t n) const { return false; }
};

inline constexpr __bitset<0, 0>::__bitset() noexcept { }

inline constexpr __bitset<0, 0>::__bitset(unsigned long long) noexcept { }

template <size_t _Size>
class bitset;

template <size_t _Size>
class bitset
: private __bitset<_Size == 0 ? 0 : (_Size - 1) / (sizeof(uint64_t) * __CHAR_BIT__) + 1, _Size> {
public:
	static const unsigned __n_words =
			_Size == 0 ? 0 : (_Size - 1) / (sizeof(size_t) * __CHAR_BIT__) + 1;
	typedef __bitset<__n_words, _Size> __base;

	constexpr bitset() noexcept { }
	constexpr bitset(unsigned long long __v) noexcept
	: __base(sizeof(unsigned long long) * __CHAR_BIT__ <= _Size ? __v
																: __v & ((1ULL << _Size) - 1)) { }

	// 23.3.5.2 bitset operations:
	constexpr bitset &operator&=(const bitset &__rhs) noexcept;
	constexpr bitset &operator|=(const bitset &__rhs) noexcept;
	constexpr bitset &operator^=(const bitset &__rhs) noexcept;
	constexpr bitset &set() noexcept;
	constexpr bitset &set(size_t __pos, bool __val = true);
	constexpr bitset &reset() noexcept;
	constexpr bitset &reset(size_t __pos);
	constexpr bitset operator~() const noexcept;
	constexpr bitset &flip() noexcept;
	constexpr bitset &flip(size_t __pos);

	constexpr size_t count() const noexcept;
	constexpr size_t size() const noexcept { return _Size; }
	constexpr bool operator==(const bitset &__rhs) const noexcept;

	constexpr bool test(size_t __pos) const;
	constexpr bool all() const noexcept { return __base::all(); }
	constexpr bool any() const noexcept { return __base::any(); }
	constexpr bool none() const noexcept { return !any(); }
};

template <size_t _Size>
inline constexpr bitset<_Size> &bitset<_Size>::operator&=(const bitset &__rhs) noexcept {
	__base::operator&=(__rhs);
	return *this;
}

template <size_t _Size>
inline constexpr bitset<_Size> &bitset<_Size>::operator|=(const bitset &__rhs) noexcept {
	__base::operator|=(__rhs);
	return *this;
}

template <size_t _Size>
inline constexpr bitset<_Size> &bitset<_Size>::operator^=(const bitset &__rhs) noexcept {
	__base::operator^=(__rhs);
	return *this;
}

template <size_t _Size>
inline constexpr bitset<_Size> &bitset<_Size>::set() noexcept {
	__base::set();
	return *this;
}

template <size_t _Size>
constexpr bitset<_Size> &bitset<_Size>::set(size_t __pos, bool __val) {
	if (__pos >= _Size) {
		return *this;
	}

	__base::set(__pos, __val);
	return *this;
}

template <size_t _Size>
inline constexpr bitset<_Size> &bitset<_Size>::reset() noexcept {
	__base::reset();
	return *this;
}

template <size_t _Size>
constexpr bitset<_Size> &bitset<_Size>::reset(size_t __pos) {
	if (__pos >= _Size) {
		return *this;
	}

	__base::reset(__pos);
	return *this;
}

template <size_t _Size>
inline constexpr bitset<_Size> bitset<_Size>::operator~() const noexcept {
	bitset __x(*this);
	__x.flip();
	return __x;
}

template <size_t _Size>
inline constexpr bitset<_Size> &bitset<_Size>::flip() noexcept {
	__base::flip();
	return *this;
}

template <size_t _Size>
constexpr bitset<_Size> &bitset<_Size>::flip(size_t __pos) {
	if (__pos >= _Size) {
		return *this;
	}

	__base::flip(__pos);
	return *this;
}

template <size_t _Size>
inline constexpr size_t bitset<_Size>::count() const noexcept {
	return __base::count();
}

template <size_t _Size>
inline constexpr bool bitset<_Size>::operator==(const bitset &__rhs) const noexcept {
	return __base::compare(__rhs) == 0;
}

template <size_t _Size>
constexpr bool bitset<_Size>::test(size_t __pos) const {
	if (__pos >= _Size) {
		return false;
	}

	return __base::test(__pos);
}

template <size_t _Size>
inline constexpr bitset<_Size> operator&(const bitset<_Size> &__x,
		const bitset<_Size> &__y) noexcept {
	bitset<_Size> __r = __x;
	__r &= __y;
	return __r;
}

template <size_t _Size>
inline constexpr bitset<_Size> operator|(const bitset<_Size> &__x,
		const bitset<_Size> &__y) noexcept {
	bitset<_Size> __r = __x;
	__r |= __y;
	return __r;
}

template <size_t _Size>
inline constexpr bitset<_Size> operator^(const bitset<_Size> &__x,
		const bitset<_Size> &__y) noexcept {
	bitset<_Size> __r = __x;
	__r ^= __y;
	return __r;
}

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_BITSET_H_
