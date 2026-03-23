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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_STRING_H_
#define RUNTIME_INCLUDE_SPRT_CXX_STRING_H_

#include <sprt/cxx/memory/allocator_malloc.h>
#include <sprt/cxx/memory/allocator_pool.h>
#include <sprt/cxx/memory/storage.h>
#include <sprt/cxx/hash.h>

#include <sprt/runtime/string.h>
#include <sprt/runtime/detail/constexpr.h>

#include <sprt/cxx/compare.h>
#include <sprt/cxx/initializer_list.h>

namespace sprt {

struct char_pointer_test;
template <typename T>
struct is_char_pointer { };
template <>
struct is_char_pointer<const char *> {
	using Type = char_pointer_test *;
};
template <>
struct is_char_pointer<char *> {
	using Type = char_pointer_test;
};
template <>
struct is_char_pointer<const char16_t *> {
	using Type = char_pointer_test;
};
template <>
struct is_char_pointer<char16_t *> {
	using Type = char_pointer_test;
};
template <>
struct is_char_pointer<const char32_t *> {
	using Type = char_pointer_test;
};
template <>
struct is_char_pointer<char32_t *> {
	using Type = char_pointer_test;
};

template <typename CharType, typename InputIterator, bool IsIntegral, typename Allocator>
struct __basic_string_fill;

template <typename CharType, typename Allocator>
class __basic_string : public Allocator::base_class {
public:
	using allocator_type = Allocator;

	using pointer = CharType *;
	using const_pointer = const CharType *;
	using reference = CharType &;
	using const_reference = const CharType &;

	using size_type = size_t;
	using charT = CharType;
	using value_type = CharType;
	using mem_type = memory::linear_memory<CharType, size_t(1), allocator_type>;
	using self = __basic_string<CharType, allocator_type>;

	using iterator = typename mem_type::iterator;
	using const_iterator = typename mem_type::const_iterator;
	using reverse_iterator = typename mem_type::reverse_iterator;
	using const_reverse_iterator = typename mem_type::const_reverse_iterator;

	static const size_type npos;

	__basic_string() noexcept : _mem() { }
	explicit __basic_string(const allocator_type &alloc) noexcept : _mem(alloc) { }

	__basic_string(const self &str) noexcept : _mem(str._mem) { }
	__basic_string(const self &str, const allocator_type &alloc) noexcept
	: _mem(str._mem, alloc) { }

	__basic_string(const self &str, size_type pos, size_type len = npos,
			const allocator_type &alloc = allocator_type()) noexcept
	: _mem(str._mem, pos, len, alloc) { }

	__basic_string(const charT *s, const allocator_type &alloc = allocator_type()) noexcept
	: _mem(s, (s ? __constexpr_strlen(s) : 0), alloc) { }

	__basic_string(const charT *s, size_type n,
			const allocator_type &alloc = allocator_type()) noexcept
	: _mem(s, n, alloc) { }

	__basic_string(size_type n, charT c, const allocator_type &alloc = allocator_type()) noexcept
	: _mem(alloc) {
		_mem.fill(n, c);
	}

	template <typename InputIterator>
	__basic_string(InputIterator first, InputIterator last,
			const allocator_type &alloc = allocator_type()) noexcept;

	__basic_string(initializer_list<charT> il,
			const allocator_type &alloc = allocator_type()) noexcept
	: _mem(alloc) {
		_mem.reserve(il.size());
		for (auto it = il.begin(); it != il.end(); it++) { _mem.emplace_back_unsafe(*it); }
	}

	__basic_string(__basic_string &&str) noexcept : _mem(sprt::move_unsafe(str._mem)) { }
	__basic_string(__basic_string &&str, const allocator_type &alloc) noexcept
	: _mem(sprt::move_unsafe(str._mem), alloc) { }

	__basic_string &operator=(const __basic_string &str) noexcept {
		_mem = str._mem;
		return *this;
	}

	__basic_string &operator=(__basic_string &&str) noexcept {
		_mem = sprt::move_unsafe(str._mem);
		return *this;
	}

	__basic_string &operator=(const charT *s) noexcept {
		_mem.assign(s, __constexpr_strlen(s));
		return *this;
	}

	__basic_string &operator=(charT ch) noexcept {
		_mem.assign(&ch, 1);
		return *this;
	}

	__basic_string &operator=(initializer_list<charT> ilist) noexcept {
		_mem.clear();
		_mem.reserve(ilist.size());
		for (auto &it : ilist) { _mem.emplace_back_unsafe(it); }
		return *this;
	}

	allocator_type get_allocator() const noexcept { return _mem.get_allocator(); }

	iterator begin() noexcept { return _mem.begin(); }
	iterator end() noexcept { return _mem.end(); }

	const_iterator begin() const noexcept { return _mem.begin(); }
	const_iterator end() const noexcept { return _mem.end(); }

	const_iterator cbegin() const noexcept { return _mem.cbegin(); }
	const_iterator cend() const noexcept { return _mem.cend(); }

	reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
	reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

	const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
	const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

	const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }
	const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

	size_type size() const noexcept { return _mem.size(); }
	size_type length() const noexcept { return _mem.size(); }
	size_type capacity() const noexcept { return _mem.capacity(); }

	void reserve(size_type n = 0) { _mem.reserve(n); }

	void resize(size_type n) { _mem.resize(n); }
	void resize(size_type n, charT c) { _mem.resize(n, c); }

	void clear() { _mem.clear(); }
	bool empty() const noexcept { return _mem.empty(); }

	void shrink_to_fit() noexcept { _mem.shrink_to_fit(); }

	reference operator[](size_type pos) noexcept { return _mem.at(pos); }
	const_reference operator[](size_type pos) const noexcept { return _mem.at(pos); }

	reference at(size_type pos) noexcept { return _mem.at(pos); }
	const_reference at(size_type pos) const noexcept { return _mem.at(pos); }

	charT &back() noexcept { return _mem.back(); }
	const charT &back() const noexcept { return _mem.back(); }

	charT &front() noexcept { return _mem.front(); }
	const charT &front() const noexcept { return _mem.front(); }

	__basic_string &append(const __basic_string &str) {
		_mem.insert_back(str._mem);
		return *this;
	}
	__basic_string &append(const __basic_string &str, size_type subpos, size_type sublen = npos) {
		_mem.insert_back(str._mem, subpos, sublen);
		return *this;
	}
	__basic_string &append(const charT *s) {
		_mem.insert_back(s, __constexpr_strlen(s));
		return *this;
	}
	__basic_string &append(const charT *s, size_type n) {
		_mem.insert_back(s, n);
		return *this;
	}
	__basic_string &append(size_type n, charT c) {
		_mem.resize(_mem.size() + n, c);
		return *this;
	}
	template <typename InputIterator>
	__basic_string &append(InputIterator first, InputIterator last) {
		auto diff = sprt::distance(first, last);
		_mem.reserve(_mem.size() + diff, true);
		for (auto it = first; it != last; ++it) {
			if (*it != value_type(0)) {
				_mem.emplace_back_unsafe(*it);
			} else {
				break;
			}
		}
		return *this;
	}
	__basic_string &append(initializer_list<charT> il) {
		_mem.reserve(il.size() + _mem.size(), true);
		for (auto it = il.begin(); it != il.end(); it++) { _mem.emplace_back_unsafe(*it); }
		return *this;
	}

	__basic_string &operator+=(const __basic_string &str) { return append(str); }
	__basic_string &operator+=(const charT *s) { return append(s); }
	__basic_string &operator+=(charT c) {
		_mem.emplace_back(c);
		return *this;
	}
	__basic_string &operator+=(initializer_list<charT> il) { return append(il); }

	__basic_string &assign(const __basic_string &str) {
		_mem.assign(str._mem);
		return *this;
	}
	__basic_string &assign(const __basic_string &str, size_type subpos, size_type sublen = npos) {
		_mem.assign(str._mem, subpos, sublen);
		return *this;
	}
	__basic_string &assign(const charT *s) {
		_mem.assign(s, __constexpr_strlen(s));
		return *this;
	}
	__basic_string &assign(const charT *s, size_type n) {
		_mem.assign(s, n);
		return *this;
	}
	__basic_string &assign(size_type n, charT c) {
		_mem.fill(n, c);
		return *this;
	}

	template <typename InputIterator>
	__basic_string &assign(InputIterator first, InputIterator last) {
		_mem.clear();
		auto diff = last - first;
		_mem.reserve(diff);
		for (auto it = first; it != last; it++) { _mem.emplace_back_unsafe(*it); }
		return *this;
	}

	__basic_string &assign(initializer_list<charT> il) {
		_mem.clear();
		_mem.reserve(il.size());
		for (auto it = il.begin(); it != il.end(); it++) { _mem.emplace_back_unsafe(*it); }
		return *this;
	}
	__basic_string &assign(__basic_string &&str) noexcept {
		_mem = sprt::move_unsafe(str._mem);
		return *this;
	}

	void push_back(CharType c) noexcept { _mem.emplace_back(c); }

	void emplace_back(CharType c) noexcept { _mem.emplace_back(c); }

	void pop_back() noexcept { _mem.pop_back(); }

	__basic_string &insert(size_type pos, const __basic_string &str) {
		_mem.insert(pos, str._mem);
		return *this;
	}
	__basic_string &insert(size_type pos, const __basic_string &str, size_type subpos,
			size_type sublen = npos) {
		_mem.insert(pos, str._mem, subpos, sublen);
		return *this;
	}

	__basic_string &insert(size_type pos, const charT *s) {
		_mem.insert(pos, s, __constexpr_strlen(s));
		return *this;
	}

	__basic_string &insert(size_type pos, const charT *s, size_type n) {
		_mem.insert(pos, s, n);
		return *this;
	}

	__basic_string &insert(size_type pos, size_type n, charT c) {
		_mem.insert(pos, n, c);
		return *this;
	}
	iterator insert(const_iterator p, size_type n, charT c) { return _mem.insert(p, n, c); }

	iterator insert(const_iterator p, charT c) { return _mem.insert(p, 1, c); }

	template < typename InputIt >
	iterator insert(const_iterator pos, InputIt first, InputIt last) {
		return _mem.insert(pos, first, last);
	}

	iterator insert(const_iterator pos, initializer_list<charT> init) {
		return _mem.insert(pos, init.begin(), init.end());
	}

	__basic_string &erase(size_type pos = 0, size_type len = npos) {
		_mem.erase(pos, len);
		return *this;
	}

	iterator erase(const_iterator p) { return _mem.erase(p); }

	iterator erase(const_iterator first, const_iterator last) { return _mem.erase(first, last); }

	__basic_string &replace(size_type pos, size_type len, const __basic_string &str) {
		_mem.replace(pos, len, str._mem);
		return *this;
	}
	__basic_string &replace(const_iterator i1, const_iterator i2, const __basic_string &str) {
		_mem.replace(i1 - _mem.data(), i2 - i1, str._mem);
		return *this;
	}

	__basic_string &replace(size_type pos, size_type len, const __basic_string &str,
			size_type subpos, size_type sublen = npos) {
		_mem.replace(pos, len, str._mem, subpos, sublen);
		return *this;
	}

	__basic_string &replace(size_type pos, size_type len, const charT *s) {
		_mem.replace(pos, len, s, __constexpr_strlen(s));
		return *this;
	}
	__basic_string &replace(const_iterator i1, const_iterator i2, const charT *s) {
		_mem.replace(i1 - _mem.data(), i2 - i1, s, __constexpr_strlen(s));
		return *this;
	}

	__basic_string &replace(size_type pos, size_type len, const charT *s, size_type n) {
		_mem.replace(pos, len, s, n);
		return *this;
	}
	__basic_string &replace(const_iterator i1, const_iterator i2, const charT *s, size_type n) {
		_mem.replace(i1 - _mem.data(), i2 - i1, s, n);
		return *this;
	}

	__basic_string &replace(size_type pos, size_type len, size_type n, charT c) {
		_mem.replace(pos, len, n, c);
		return *this;
	}
	__basic_string &replace(const_iterator i1, const_iterator i2, size_type n, charT c) {
		_mem.replace(i1 - _mem.data(), i2 - i1, n, c);
		return *this;
	}

	template < typename InputIt >
	__basic_string &replace(const_iterator first, const_iterator last, InputIt first2,
			InputIt last2) {
		_mem.replace(first, last, first2, last2);
		return *this;
	}

	__basic_string &replace(const_iterator first, const_iterator last,
			initializer_list<charT> init) {
		_mem.replace(first, last, init.begin(), init.end());
		return *this;
	}

	const charT *c_str() const noexcept { return _mem.data(); }
	const charT *data() const noexcept { return _mem.data(); }
	charT *data() noexcept { return _mem.data(); }

	size_type copy(charT *s, size_type len, size_type pos = 0) const {
		len = min(len, _mem.size() - pos);
		__sprt_memcpy(s, _mem.data() + pos, len * sizeof(charT));
		return len;
	}

	__basic_string substr(size_type pos = 0, size_type len = npos) const {
		return __basic_string(*this, pos, len);
	}


	size_type find(const charT *__s, size_type __pos, size_type __n) const;
	size_type find(charT __c, size_type __pos = 0) const;
	size_type rfind(const charT *__s, size_type __pos, size_type __n) const;
	size_type rfind(charT __c, size_type __pos = npos) const;
	size_type find_first_of(const charT *__s, size_type __pos, size_type __n) const;
	size_type find_last_of(const charT *__s, size_type __pos, size_type __n) const;
	size_type find_first_not_of(const charT *__s, size_type __pos, size_type __n) const;
	size_type find_first_not_of(charT __c, size_type __pos = 0) const;
	size_type find_last_not_of(const charT *__s, size_type __pos, size_type __n) const;
	size_type find_last_not_of(charT __c, size_type __pos = npos) const;

	size_type find(const __basic_string &__str, size_type __pos = 0) const {
		return this->find(__str.data(), __pos, __str.size());
	}

	size_type find(const charT *__s, size_type __pos = 0) const {
		return this->find(__s, __pos, __constexpr_strlen(__s));
	}

	size_type rfind(const __basic_string &__str, size_type __pos = npos) const {
		return this->rfind(__str.data(), __pos, __str.size());
	}

	size_type rfind(const charT *__s, size_type __pos = npos) const {
		return this->rfind(__s, __pos, __constexpr_strlen(__s));
	}

	size_type find_first_of(const __basic_string &__str, size_type __pos = 0) const {
		return this->find_first_of(__str.data(), __pos, __str.size());
	}

	size_type find_first_of(const charT *__s, size_type __pos = 0) const {
		return this->find_first_of(__s, __pos, __constexpr_strlen(__s));
	}

	size_type find_first_of(charT __c, size_type __pos = 0) const { return this->find(__c, __pos); }

	size_type find_last_of(const __basic_string &__str, size_type __pos = npos) const {
		return this->find_last_of(__str.data(), __pos, __str.size());
	}

	size_type find_last_of(const charT *__s, size_type __pos = npos) const {
		return this->find_last_of(__s, __pos, __constexpr_strlen(__s));
	}

	size_type find_last_of(charT __c, size_type __pos = npos) const {
		return this->rfind(__c, __pos);
	}

	size_type find_first_not_of(const __basic_string &__str, size_type __pos = 0) const {
		return this->find_first_not_of(__str.data(), __pos, __str.size());
	}

	size_type find_first_not_of(const charT *__s, size_type __pos = 0) const {
		return this->find_first_not_of(__s, __pos, __constexpr_strlen(__s));
	}

	size_type find_last_not_of(const __basic_string &__str, size_type __pos = npos) const {
		return this->find_last_not_of(__str.data(), __pos, __str.size());
	}

	size_type find_last_not_of(const charT *__s, size_type __pos = npos) const {
		return this->find_last_not_of(__s, __pos, __constexpr_strlen(__s));
	}

	int compare(const __basic_string &str) const noexcept {
		size_type lhs_sz = size();
		size_type rhs_sz = str.size();
		return compare(data(), lhs_sz, str.data(), rhs_sz);
	}

	int compare(size_type pos, size_type len, const __basic_string &str) const {
		size_type lhs_sz = min(size() - pos, len);
		size_type rhs_sz = str.size();
		return compare(data() + pos, lhs_sz, str.data(), rhs_sz);
	}
	int compare(size_type pos, size_type len, const __basic_string &str, size_type subpos,
			size_type sublen = npos) const {
		size_type lhs_sz = min(size() - pos, len);
		size_type rhs_sz = min(str.size() - subpos, sublen);
		return compare(data() + pos, lhs_sz, str.data() + subpos, rhs_sz);
	}

	int compare(const charT *s) const {
		size_type lhs_sz = size();
		size_type rhs_sz = __constexpr_strlen(s);
		return compare(data(), lhs_sz, s, rhs_sz);
	}
	int compare(size_type pos, size_type len, const charT *s) const {
		size_type lhs_sz = min(size() - pos, len);
		size_type rhs_sz = __constexpr_strlen(s);
		return compare(data() + pos, lhs_sz, s, rhs_sz);
	}

	int compare(size_type pos, size_type len, const charT *s, size_type n) const {
		size_type lhs_sz = min(size() - pos, len);
		size_type rhs_sz = n;
		return compare(data() + pos, lhs_sz, s, rhs_sz);
	}

protected:
	static int compare(const charT *s1, size_type len1, const charT *s2, size_type len2) {
		int result = (s1 && s2) ? __constexpr_strcompare(s1, s2, min(len1, len2)) : 0;
		if (result != 0) {
			return result;
		}
		if (len1 < len2) {
			return -1;
		}
		if (len1 > len2) {
			return 1;
		}
		return 0;
	}

	template <typename C, typename I, bool B, typename A>
	friend struct __basic_string_fill;

	mem_type _mem;
};

template <typename CharType, typename InputIterator, typename Allocator>
struct __basic_string_fill<CharType, InputIterator, true, Allocator> {
	static void fill(__basic_string<CharType, Allocator> &str, InputIterator first,
			InputIterator last) noexcept {
		str._mem.fill(size_t(first), CharType(last));
	}
};

template <typename CharType, typename InputIterator, typename Allocator>
struct __basic_string_fill<CharType, InputIterator, false, Allocator> {
	static void fill(__basic_string<CharType, Allocator> &str, InputIterator first,
			InputIterator last) noexcept {
		auto size = distance(first, last);
		str.reserve(size);
		for (auto it = first; it != last; it++) { str._mem.emplace_back_unsafe(*it); }
	}
};

template <typename CharT, typename Allocator>
template <typename InputIterator>
__basic_string<CharT, Allocator>::__basic_string(InputIterator first, InputIterator last,
		const allocator_type &alloc) noexcept
: _mem(alloc) {
	__basic_string_fill<CharT, InputIterator, sprt::is_integral_v<InputIterator>, Allocator>::fill(
			*this, first, last);
}

template <typename CharT, typename Allocator>
__basic_string<CharT, Allocator> operator+(const __basic_string<CharT, Allocator> &lhs,
		const __basic_string<CharT, Allocator> &rhs) {
	__basic_string<CharT, Allocator> ret;
	ret.reserve(lhs.size() + rhs.size());
	ret.assign(lhs);
	ret.append(rhs);
	return ret;
}

template <typename CharT, typename Allocator>
__basic_string<CharT, Allocator> operator+(const CharT *lhs,
		const __basic_string<CharT, Allocator> &rhs) {
	__basic_string<CharT, Allocator> ret;
	ret.reserve(__constexpr_strlen(lhs) + rhs.size());
	ret.assign(lhs);
	ret.append(rhs);
	return ret;
}

template <typename CharT, typename Allocator>
__basic_string<CharT, Allocator> operator+(CharT lhs, const __basic_string<CharT, Allocator> &rhs) {
	__basic_string<CharT, Allocator> ret;
	ret.reserve(rhs.size() + 1);
	ret.assign(1, lhs);
	ret.append(rhs);
	return ret;
}

template <typename CharT, typename Allocator>
__basic_string<CharT, Allocator> operator+(const __basic_string<CharT, Allocator> &lhs,
		const CharT *rhs) {
	__basic_string<CharT, Allocator> ret;
	ret.reserve(lhs.size() + __constexpr_strlen(rhs));
	ret.assign(lhs);
	ret.append(rhs);
	return ret;
}

template <typename CharT, typename Allocator>
__basic_string<CharT, Allocator> operator+(const __basic_string<CharT, Allocator> &lhs, CharT rhs) {
	__basic_string<CharT, Allocator> ret;
	ret.reserve(rhs.size() + 1);
	ret.assign(lhs);
	ret.append(1, rhs);
	return ret;
}

template <typename CharT, typename Allocator>
__basic_string<CharT, Allocator> operator+(__basic_string<CharT, Allocator> &&lhs,
		const __basic_string<CharT, Allocator> &rhs) {
	return sprt::move_unsafe(lhs.append(rhs));
}

template <typename CharT, typename Allocator>
__basic_string<CharT, Allocator> operator+(const __basic_string<CharT, Allocator> &lhs,
		__basic_string<CharT, Allocator> &&rhs) {
	return sprt::move_unsafe(rhs.insert(0, lhs));
}

template <typename CharT, typename Allocator>
__basic_string<CharT, Allocator> operator+(__basic_string<CharT, Allocator> &&lhs,
		__basic_string<CharT, Allocator> &&rhs) {
	return sprt::move_unsafe(lhs.append(rhs));
}

template <typename CharT, typename Allocator>
__basic_string<CharT, Allocator> operator+(const CharT *lhs,
		__basic_string<CharT, Allocator> &&rhs) {
	return sprt::move_unsafe(rhs.insert(0, lhs));
}

template <typename CharT, typename Allocator>
__basic_string<CharT, Allocator> operator+(CharT lhs, __basic_string<CharT, Allocator> &&rhs) {
	return sprt::move_unsafe(rhs.insert(0, 1, lhs));
}

template <typename CharT, typename Allocator>
__basic_string<CharT, Allocator> operator+(__basic_string<CharT, Allocator> &&lhs,
		const CharT *rhs) {
	return sprt::move_unsafe(lhs.append(rhs));
}

template <typename CharT, typename Allocator>
__basic_string<CharT, Allocator> operator+(__basic_string<CharT, Allocator> &&lhs, CharT rhs) {
	return sprt::move_unsafe(lhs.append(1, rhs));
}

template <typename CharT, typename Allocator>
bool operator==(const __basic_string<CharT, Allocator> &lhs,
		const __basic_string<CharT, Allocator> &rhs) {
	return lhs.compare(rhs) == 0;
}

template <typename CharT, typename Allocator>
auto operator<=>(const __basic_string<CharT, Allocator> &lhs,
		const __basic_string<CharT, Allocator> &rhs) {
	auto ret = lhs.compare(rhs);
	if (ret == 0) {
		return std::strong_ordering::equal;
	} else if (ret < 0) {
		return std::strong_ordering::less;
	} else {
		return std::strong_ordering::greater;
	}
}

template <typename CharT, typename Allocator>
bool operator==(const __basic_string<CharT, Allocator> &lhs, const CharT *rhs) {
	return lhs.compare(rhs) == 0;
}

template <typename CharT, typename Allocator>
auto operator<=>(const __basic_string<CharT, Allocator> &lhs, const CharT *rhs) {
	auto ret = lhs.compare(rhs);
	if (ret == 0) {
		return std::strong_ordering::equal;
	} else if (ret < 0) {
		return std::strong_ordering::less;
	} else {
		return std::strong_ordering::greater;
	}
}

template <typename CharT, typename Allocator>
bool operator==(const CharT *lhs, const __basic_string<CharT, Allocator> &rhs) {
	return rhs.compare(lhs) == 0;
}

template <typename CharT, typename Allocator>
auto operator<=>(const CharT *lhs, const __basic_string<CharT, Allocator> &rhs) {
	auto ret = rhs.compare(lhs);
	if (ret == 0) {
		return std::strong_ordering::equal;
	} else if (ret < 0) {
		return std::strong_ordering::greater;
	} else {
		return std::strong_ordering::less;
	}
}

template <typename CharT, typename Allocator>
auto __basic_string<CharT, Allocator>::find(const charT *__s, size_type __pos, size_type __n) const
		-> size_type {
	const size_type __size = this->size();
	const CharT *__data = _mem.data();

	if (__n == 0) {
		return __pos <= __size ? __pos : npos;
	} else if (__n <= __size) {
		for (; __pos <= __size - __n; ++__pos) {
			if (__constexpr_chareq(__data[__pos], __s[0])
					&& __constexpr_strcompare(__data + __pos + 1, __s + 1, __n - 1) == 0) {
				return __pos;
			}
		}
	}
	return npos;
}

template <typename CharT, typename Allocator>
auto __basic_string<CharT, Allocator>::find(charT __c, size_type __pos) const -> size_type {
	size_type __ret = npos;
	const size_type __size = this->size();
	if (__pos < __size) {
		const CharT *__data = _mem.data();
		const size_type __n = __size - __pos;
		const CharT *__p = __constexpr_strfind(__data + __pos, __n, __c);
		if (__p) {
			__ret = __p - __data;
		}
	}
	return __ret;
}

template <typename CharT, typename Allocator>
auto __basic_string<CharT, Allocator>::rfind(const charT *__s, size_type __pos, size_type __n) const
		-> size_type {
	const size_type __size = this->size();
	if (__n <= __size) {
		__pos = min(size_type(__size - __n), __pos);
		const CharT *__data = _mem.data();
		do {
			if (__constexpr_strcompare(__data + __pos, __s, __n) == 0) {
				return __pos;
			}
		} while (__pos-- > 0);
	}
	return npos;
}

template <typename CharT, typename Allocator>
auto __basic_string<CharT, Allocator>::rfind(charT __c, size_type __pos) const -> size_type {
	size_type __size = this->size();
	if (__size) {
		if (--__size > __pos) {
			__size = __pos;
		}
		for (++__size; __size-- > 0;) {
			if (__constexpr_chareq(_mem.data()[__size], __c)) {
				return __size;
			}
		}
	}
	return npos;
}

template <typename CharT, typename Allocator>
auto __basic_string<CharT, Allocator>::find_first_of(const charT *__s, size_type __pos,
		size_type __n) const -> size_type {
	for (; __n && __pos < this->size(); ++__pos) {
		const CharT *__p = __constexpr_strfind(__s, __n, _mem.data()[__pos]);
		if (__p) {
			return __pos;
		}
	}
	return npos;
}

template <typename CharT, typename Allocator>
auto __basic_string<CharT, Allocator>::find_last_of(const charT *__s, size_type __pos,
		size_type __n) const -> size_type {
	size_type __size = this->size();
	if (__size && __n) {
		if (--__size > __pos) {
			__size = __pos;
		}
		do {
			if (__constexpr_strfind(__s, __n, _mem.data()[__size])) {
				return __size;
			}
		} while (__size-- != 0);
	}
	return npos;
}

template <typename CharT, typename Allocator>
auto __basic_string<CharT, Allocator>::find_first_not_of(const charT *__s, size_type __pos,
		size_type __n) const -> size_type {
	for (; __pos < this->size(); ++__pos) {
		if (!__constexpr_strfind(__s, __n, _mem.data()[__pos])) {
			return __pos;
		}
	}
	return npos;
}

template <typename CharT, typename Allocator>
auto __basic_string<CharT, Allocator>::find_first_not_of(charT __c, size_type __pos) const
		-> size_type {
	for (; __pos < this->size(); ++__pos) {
		if (!__constexpr_chareq(_mem.data()[__pos], __c)) {
			return __pos;
		}
	}
	return npos;
}

template <typename CharT, typename Allocator>
auto __basic_string<CharT, Allocator>::find_last_not_of(const charT *__s, size_type __pos,
		size_type __n) const -> size_type {
	size_type __size = this->size();
	if (__size) {
		if (--__size > __pos) {
			__size = __pos;
		}
		do {
			if (!__constexpr_strfind(__s, __n, _mem.data()[__size])) {
				return __size;
			}
		} while (__size--);
	}
	return npos;
}

template <typename CharT, typename Allocator>
auto __basic_string<CharT, Allocator>::find_last_not_of(charT __c, size_type __pos) const
		-> size_type {
	size_type __size = this->size();
	if (__size) {
		if (--__size > __pos) {
			__size = __pos;
		}
		do {
			if (!__constexpr_chareq(_mem.data()[__size], __c)) {
				return __size;
			}
		} while (__size--);
	}
	return npos;
}

template <typename CharT, typename Allocator>
constexpr const typename __basic_string<CharT, Allocator>::size_type
		__basic_string<CharT, Allocator>::npos =
				Max<typename __basic_string<CharT, Allocator>::size_type>;

using __pool_string = __basic_string<char, memory::AllocatorPool<char>>;
using __pool_u16string = __basic_string<char16_t, memory::AllocatorPool<char16_t>>;
using __pool_u32string = __basic_string<char32_t, memory::AllocatorPool<char32_t>>;

using __malloc_string = __basic_string<char, memory::AllocatorMalloc<char>>;
using __malloc_u16string = __basic_string<char16_t, memory::AllocatorMalloc<char16_t>>;
using __malloc_u32string = __basic_string<char32_t, memory::AllocatorMalloc<char32_t>>;

template <typename CharType, typename Allocator>
struct hash<__basic_string<CharType, Allocator>> {
	constexpr size_t operator()(const sprt::__basic_string<CharType, Allocator> &s) const noexcept {
		if constexpr (sizeof(size_t) == 8) {
			return sprt::hash64(s.data(), s.size() * sizeof(CharType));
		} else {
			return sprt::hash32(s.data(), s.size() * sizeof(CharType));
		}
	}
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX_STRING_H_
