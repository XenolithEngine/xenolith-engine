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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_STRINGVIEW_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_STRINGVIEW_H_

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_STRINGVIEW_H_
#include <sprt/runtime/stringview.h>
#endif

#include <sprt/runtime/unicode.h>
#include <sprt/runtime/compare.h>
#include <sprt/runtime/halffloat.h>

namespace sprt::detail {

// read number from string and offset pointers

template <typename L, typename R,
		typename CharType = typename enable_if<
				is_same< typename L::value_type, typename R::value_type >::value,
				typename L::value_type>::type>
inline int compare_c(const L &l, const R &r);

template <typename L, typename R,
		typename CharType = typename enable_if<
				is_same< typename L::value_type, typename R::value_type >::value,
				typename L::value_type>::type>
inline int compare_u(const L &l, const R &r);

template <typename L, typename R,
		typename CharType = typename enable_if<
				is_same< typename L::value_type, typename R::value_type >::value,
				typename L::value_type>::type>
inline int caseCompare_c(const L &l, const R &r);

template <typename L, typename R,
		typename CharType = typename enable_if<
				is_same< typename L::value_type, typename R::value_type >::value,
				typename L::value_type>::type>
inline int caseCompare_u(const L &l, const R &r);

template <typename _CharT>
constexpr size_t length(const _CharT *__p) {
	if (!__p) {
		return 0;
	}

	return strlen(__p);
}

// Limited length function
// This function can be used to safely obtain the length of a string when its potential maximum length is known
// For such a case, this function will not perform the comparison of infinity but will limit it to the maximum number of characters transmitted
template <typename _CharT>
constexpr size_t length(const _CharT *__p, size_t max) {
	if (!__p) {
		return 0;
	}

	if (max == Max<size_t>) {
		return strlen(__p);
	} else {
		size_t __i = 0;
		while (__i < max && __p[__i] != _CharT(0)) { ++__i; }
		return __i;
	}
}

template <>
constexpr size_t length(const uint8_t *__p, size_t max) {
	if (!__p) {
		return 0;
	}

	return max;
}

template <typename T, typename Char>
inline auto readNumber(const Char *ptr, size_t len, int base, uint8_t &offset) -> Result<T> {
	// prevent to read out of bounds, copy symbols to stack buffer
	char buf[32] = {0}; // int64_t/scientific double character length max
	size_t m = min(size_t(31), len);
	size_t i = 0;
	for (; i < m; i++) {
		auto c = ptr[i];
		if (c < 127) {
			buf[i] = c;
		} else {
			break;
		}
	}

	// read number from internal buffer
	char *ret = nullptr;
	auto val = StringToNumber<T>(buf, &ret, base);
	if (*ret == 0) {
		// while string was used
		offset = i;
	} else if (ret && ret != buf) {
		// part of string was used
		offset = ret - buf;
	} else {
		// fail to read number
		offset = 0;
		return Result<T>();
	}
	return Result<T>(val);
}

template <typename CharType>
inline int compare_c(const CharType *lPtr, size_t lSize, const CharType *rPtr, size_t rSize) {
	auto __len = min(lSize, rSize);
	auto ret = strcompare(lPtr, rPtr, __len);
	if (!ret) {
		if (lSize < rSize) {
			return -1;
		} else if (lSize == rSize) {
			return 0;
		} else {
			return 1;
		}
	}
	return ret;
}

template <typename L, typename R, typename CharType>
inline int compare_c(const L &l, const R &r) {
	return compare_c(l.data(), l.size(), r.data(), r.size());
}

template <typename L, typename R, typename CharType>
inline int compare_u(const L &l, const R &r) {
	int ret = 0;
	if (unicode::compare(StringViewBase<CharType>(l), StringViewBase<CharType>(r), &ret)) {
		return ret;
	}
	return compare_c(l, r);
}

template <typename L, typename R, typename CharType>
inline int caseCompare_c(const L &l, const R &r) {
	auto ret = sprt::lexicographical_compare_pointer(l.data(), l.data() + l.size(), r.data(),
			r.data() + r.size(), [&](const CharType &l, const CharType &r) -> int {
		auto lc = toupper_c(l);
		auto rc = toupper_c(r);
		if (lc == rc) {
			return 0;
		} else if (lc < rc) {
			return -1;
		} else {
			return 1;
		}
	});
	return ret;
}

template <typename L, typename R, typename CharType>
inline int caseCompare_u(const L &l, const R &r) {
	int ret = 0;
	if (unicode::caseCompare(StringViewBase<CharType>(l), StringViewBase<CharType>(r), &ret)) {
		return ret;
	}
	return caseCompare_c(l, r);
}

} // namespace sprt::detail

namespace sprt {

struct StringComparator {
	template <typename CharT>
	static int compare(const CharT *l, const CharT *r, size_t len) {
		return detail::compare_c(BytesReader<CharT>(l, len), BytesReader<CharT>(r, len));
	}
};

struct StringCaseComparator {
	template <typename CharT>
	static int compare(const CharT *l, const CharT *r, size_t len) {
		return detail::caseCompare_c(BytesReader<CharT>(l, len), BytesReader<CharT>(r, len));
	}
};

struct StringUnicodeComparator {
	template <typename CharT>
	static int compare(const CharT *l, const CharT *r, size_t len) {
		return detail::compare_u(BytesReader<CharT>(l, len), BytesReader<CharT>(r, len));
	}
};

struct StringUnicodeCaseComparator {
	template <typename CharT>
	static int compare(const CharT *l, const CharT *r, size_t len) {
		return detail::caseCompare_u(BytesReader<CharT>(l, len), BytesReader<CharT>(r, len));
	}
};

template <typename C>
inline bool operator==(const StringViewBase<C> &l, const StringViewBase<C> &r) {
	return detail::compare_c(l, r) == 0;
}

template <typename C>
inline bool operator!=(const StringViewBase<C> &l, const StringViewBase<C> &r) {
	return detail::compare_c(l, r) != 0;
}

template <typename C>
inline bool operator==(const StringViewBase<C> &l, const C *r) {
	return detail::compare_c(l, StringViewBase<C>(r)) == 0;
}
template <typename C>
inline bool operator!=(const StringViewBase<C> &l, const C *r) {
	return detail::compare_c(l, StringViewBase<C>(r)) != 0;
}

template <typename C>
inline bool operator==(const C *l, const StringViewBase<C> &r) {
	return detail::compare_c(StringViewBase<C>(l), r) == 0;
}
template <typename C>
inline bool operator!=(const C *l, const StringViewBase<C> &r) {
	return detail::compare_c(StringViewBase<C>(l), r) != 0;
}

inline bool operator==(const StringViewUtf8 &l, const StringViewUtf8 &r) {
	return detail::compare_c(l, r) == 0;
}

inline bool operator!=(const StringViewUtf8 &l, const StringViewUtf8 &r) {
	return detail::compare_c(l, r) != 0;
}

inline bool operator==(const StringViewUtf8 &l, const char *r) {
	return detail::compare_c(l, StringViewUtf8(r)) == 0;
}
inline bool operator!=(const StringViewUtf8 &l, const char *r) {
	return detail::compare_c(l, StringViewUtf8(r)) != 0;
}

inline bool operator==(const char *l, const StringViewUtf8 &r) {
	return detail::compare_c(StringViewUtf8(l), r) == 0;
}
inline bool operator!=(const char *l, const StringViewUtf8 &r) {
	return detail::compare_c(StringViewUtf8(l), r) != 0;
}

template <typename CharT>
template <typename Comparator>
inline bool BytesReader<CharT>::equals(const CharType *d, size_t l) const {
	return (l == len && Comparator::compare(ptr, d, l) == 0);
}

template <typename CharT>
template <typename Comparator>
inline bool BytesReader<CharT>::equals(const CharType *d) const {
	// Use limited-length for safety
	auto l = detail::length(d, len + 1);
	if (l != len) {
		return false;
	}
	return equals<Comparator>(d, len);
}

template <typename CharT>
template <typename Comparator>
inline bool BytesReader<CharT>::prefix(const CharType *d, size_t l) const {
	return (l <= len && Comparator::compare(ptr, d, l) == 0);
}

template <typename CharT>
template <typename Comparator>
inline bool BytesReader<CharT>::starts_with(const CharType *d) const {
	// Use limited-length for safety
	auto l = detail::length(d, len + 1);
	if (l > len) {
		return false;
	}
	return prefix<Comparator>(d, l);
}

template <typename CharT>
template <size_t Count, typename Comparator>
inline bool BytesReader<CharT>::starts_with(const CharType d[Count]) const {
	if (Count > len) {
		return false;
	}
	return prefix<Comparator>(d, Count);
}

template <typename CharT>
template <typename Comparator>
inline bool BytesReader<CharT>::ends_with(const CharType *d, size_t l) const {
	return (l <= len && Comparator::compare(ptr + (len - l), d, l) == 0);
}

template <typename CharT>
template <typename Comparator>
inline bool BytesReader<CharT>::ends_with(const CharType *d) const {
	auto l = detail::length(d, len + 1);
	if (l > len) {
		return false;
	}
	return ends_with<Comparator>(d, l);
}

template <typename CharT>
template <size_t Count, typename Comparator>
inline bool BytesReader<CharT>::ends_with(const CharType d[Count]) const {
	if (Count > len) {
		return false;
	}
	return ends_with<Comparator>(d, Count);
}

template <typename _CharType>
template <typename Interface, typename... Args>
auto StringViewBase<_CharType>::merge(Args &&...args) ->
		typename Interface::template BasicStringType<CharType> {
	using StringType = typename Interface::template BasicStringType<CharType>;

	StringType ret;
	ret.reserve(_size(forward<Args>(args)...));
	_merge(ret, forward<Args>(args)...);
	return ret;
}

template <typename _CharType>
template <typename Interface, _CharType C, typename... Args>
auto StringViewBase<_CharType>::merge(Args &&...args) ->
		typename Interface::template BasicStringType<CharType> {
	using StringType = typename Interface::template BasicStringType<CharType>;

	StringType ret;
	ret.reserve(_size(forward<Args>(args)...) + sizeof...(Args));
	_mergeWithSep<StringType, C, true>(ret, forward<Args>(args)...);
	return ret;
}


template <typename _CharType>
template <typename T>
inline size_t StringViewBase<_CharType>::__size(const T &t) {
	return t.size();
}

template <typename _CharType>
inline size_t StringViewBase<_CharType>::__size(const CharType *c) {
	return strlen(c);
}

template <typename _CharType>
template <typename T, typename... Args>
inline size_t StringViewBase<_CharType>::_size(T &&t) {
	return __size(t);
}

template <typename _CharType>
template <typename T, typename... Args>
inline size_t StringViewBase<_CharType>::_size(T &&t, Args &&...args) {
	return __size(t) + _size(forward<Args>(args)...);
}

template <typename _CharType>
template <typename Buf, typename T>
inline void StringViewBase<_CharType>::__merge(Buf &buf, T &&t) {
	if (!t.empty()) {
		buf.append(t.data(), t.size());
	}
}

template <typename _CharType>
template <typename Buf>
inline void StringViewBase<_CharType>::__merge(Buf &buf, const CharType *c) {
	if (c) {
		buf.append(c);
	}
}

template <typename _CharType>
template <typename Buf, typename T, typename... Args>
inline void StringViewBase<_CharType>::_merge(Buf &buf, T &&t, Args &&...args) {
	__merge(buf, sprt::forward<T>(t));
	_merge(buf, forward<Args>(args)...);
}

template <typename _CharType>
template <typename Buf, typename T>
inline void StringViewBase<_CharType>::_merge(Buf &buf, T &&t) {
	__merge(buf, sprt::forward<T>(t));
}

template <typename _CharType>
template <typename Buf, _CharType C, bool Front, typename T>
inline void StringViewBase<_CharType>::__mergeWithSep(Buf &buf, T &&t) {
	Self tmp(t);
	tmp.trimChars<typename Self::template Chars<C>>();
	if (!tmp.empty()) {
		if constexpr (Front) {
			tmp = Self(t);
			tmp.backwardSkipChars<typename Self::template Chars<C>>();

			buf.append(tmp.data(), tmp.size());
		} else {
			tmp = Self(t);
			tmp.trimChars<typename Self::template Chars<C>>();

			if (!buf.empty()) {
				buf.push_back(C);
			}

			buf.append(tmp.data(), tmp.size());
		}
	}
}

template <typename _CharType>
template <typename Buf, _CharType C, bool Front, typename T, typename... Args>
inline void StringViewBase<_CharType>::_mergeWithSep(Buf &buf, T &&t, Args &&...args) {
	__mergeWithSep<Buf, C, Front>(buf, sprt::forward<T>(t));
	_mergeWithSep<Buf, C, false>(buf, sprt::forward<Args>(args)...);
}

template <typename _CharType>
template <typename Buf, _CharType C, bool Front, typename T>
inline void StringViewBase<_CharType>::_mergeWithSep(Buf &buf, T &&t) {
	__mergeWithSep<Buf, C, Front>(buf, sprt::forward<T>(t));
}


template <typename _CharType>
inline constexpr StringViewBase<_CharType>::StringViewBase(const CharType *ptr, size_t len)
: BytesReader<_CharType>(ptr, detail::length(ptr, len)) { }

template <typename _CharType>
inline constexpr StringViewBase<_CharType>::StringViewBase(const CharType *ptr, size_t pos,
		size_t len)
: BytesReader<_CharType>(ptr + pos, detail::length(ptr + pos, len)) { }

template <typename _CharType>
inline constexpr StringViewBase<_CharType>::StringViewBase(const Self &ptr, size_t pos, size_t len)
: BytesReader<_CharType>(ptr.data() + pos, min(len, ptr.size() - pos)) { }

template <typename _CharType>
inline constexpr StringViewBase<_CharType>::StringViewBase(const Self &ptr, size_t len)
: BytesReader<_CharType>(ptr.data(), min(len, ptr.size())) { }

template <typename _CharType>
inline auto StringViewBase<_CharType>::operator=(const CharType *ptr) -> Self & {
	this->set(StringViewBase<_CharType>(ptr));
	return *this;
}

template <typename _CharType>
template <size_t Size>
inline auto StringViewBase<_CharType>::operator=(const CharType c[Size]) -> Self & {
	this->set(StringViewBase<_CharType>(c, Size));
	return *this;
}

template <typename _CharType>
template <size_t Size>
inline constexpr auto StringViewBase<_CharType>::operator=(const sprt::array<CharType, Size> &str)
		-> Self & {
	this->set(str);
	return *this;
}

template <typename _CharType>
auto StringViewBase<_CharType>::set(const CharType *p, size_t l) -> Self & {
	this->ptr = p;
	this->len = l;
	return *this;
}

template <typename _CharType>
auto StringViewBase<_CharType>::pdup(memory::pool_t *p) const -> Self {
	if (!p) {
		p = sprt::memory::pool::acquire();
	}
	if (this->size() > 0) {
		auto buf =
				(_CharType *)sprt::memory::pool::palloc(p, (this->size() + 1) * sizeof(_CharType));
		memcpy(buf, this->data(), this->size() * sizeof(_CharType));
		buf[this->size()] = 0;
		return Self(buf, this->size());
	}
	return Self();
}

template <typename _CharType>
auto StringViewBase<_CharType>::ptolower_c(memory::pool_t *p) const -> Self {
	if (!p) {
		p = sprt::memory::pool::acquire();
	}
	if (this->size() > 0) {
		auto buf =
				(_CharType *)sprt::memory::pool::palloc(p, (this->size() + 1) * sizeof(_CharType));
		memcpy(buf, this->data(), this->size() * sizeof(_CharType));
		for (size_t i = 0; i < this->size(); ++i) { buf[i] = tolower_c(buf[i]); }
		buf[this->size()] = 0;
		return Self(buf, this->size());
	}
	return Self();
}

template <typename _CharType>
auto StringViewBase<_CharType>::ptoupper_c(memory::pool_t *p) const -> Self {
	if (!p) {
		p = sprt::memory::pool::acquire();
	}
	if (this->size() > 0) {
		auto buf =
				(_CharType *)sprt::memory::pool::palloc(p, (this->size() + 1) * sizeof(_CharType));
		memcpy(buf, this->data(), this->size() * sizeof(_CharType));
		for (size_t i = 0; i < this->size(); ++i) { buf[i] = toupper_c(buf[i]); }
		buf[this->size()] = 0;
		return Self(buf, this->size());
	}
	return Self();
}

template <typename _CharType>
template <typename Interface>
auto StringViewBase<_CharType>::str() const ->
		typename Interface::template BasicStringType<CharType> {
	if (this->ptr && this->len > 0) {
		return typename Interface::template BasicStringType<CharType>(this->ptr, this->len);
	} else {
		return typename Interface::template BasicStringType<CharType>();
	}
}

template <typename _CharType>
auto StringViewBase<_CharType>::operator++() -> Self & {
	if (!this->empty()) {
		this->ptr++;
		this->len--;
	}
	return *this;
}

template <typename _CharType>
auto StringViewBase<_CharType>::operator++(int) -> Self {
	auto tmp = *this;
	if (!this->empty()) {
		this->ptr++;
		this->len--;
	}
	return tmp;
}

template <typename _CharType>
auto StringViewBase<_CharType>::operator+=(size_t l) -> Self & {
	this->offset(l);
	return *this;
}

template <typename _CharType>
auto StringViewBase<_CharType>::is(const CharType &c) const -> bool {
	return this->len > 0 && *this->ptr == c;
}

template <typename _CharType>
auto StringViewBase<_CharType>::is(const CharType *c) const -> bool {
	return this->prefix(c, strlen(c));
}

template <typename _CharType>
auto StringViewBase<_CharType>::is(const Self &c) const -> bool {
	return this->prefix(c.data(), c.size());
}

template <typename _CharType>
template <_CharType C>
auto StringViewBase<_CharType>::is() const -> bool {
	return this->len > 0 && *this->ptr == C;
}

template <typename _CharType>
template <CharGroupId G>
auto StringViewBase<_CharType>::is() const -> bool {
	return this->len > 0 && MatchCharGroup<G>::match(*this->ptr);
}

template <typename _CharType>
template <typename M>
auto StringViewBase<_CharType>::is() const -> bool {
	return this->len > 0 && M::match(*this->ptr);
}

template <typename _CharType>
auto StringViewBase<_CharType>::begin() const -> iterator {
	return iterator(this->ptr);
}

template <typename _CharType>
auto StringViewBase<_CharType>::end() const -> iterator {
	return iterator(this->ptr + this->len);
}

template <typename _CharType>
auto StringViewBase<_CharType>::operator-(const Self &other) const -> Self {
	if (this->ptr > other.ptr && size_t(this->ptr - other.ptr) < this->len) {
		return Self(this->ptr, this->ptr - other.ptr);
	}
	return Self();
}

template <typename _CharType>
auto StringViewBase<_CharType>::operator-=(const Self &other) const -> Self & {
	if (this->ptr > other.ptr && size_t(this->ptr - other.ptr) < this->len) {
		this->len = this->ptr - other.ptr;
		return *this;
	}
	return *this;
}

template <typename _CharType>
auto StringViewBase<_CharType>::readFloat() -> Result<float> {
	Self tmp = *this;
	tmp.skipChars<typename Self::template CharGroup<CharGroupId::WhiteSpace>>();
	uint8_t offset = 0;
	auto ret = detail::readNumber<float>(tmp.ptr, tmp.len, 0, offset);
	this->ptr += offset;
	this->len -= offset;
	return ret;
}

template <typename _CharType>
auto StringViewBase<_CharType>::readDouble() -> Result<double> {
	Self tmp = *this;
	tmp.skipChars<typename Self::template CharGroup<CharGroupId::WhiteSpace>>();
	uint8_t offset = 0;
	auto ret = detail::readNumber<double>(tmp.ptr, tmp.len, 0, offset);
	this->ptr += offset;
	this->len -= offset;
	return ret;
}

template <typename _CharType>
auto StringViewBase<_CharType>::readInteger(int base) -> Result<int64_t> {
	Self tmp = *this;
	tmp.skipChars<typename Self::template CharGroup<CharGroupId::WhiteSpace>>();
	uint8_t offset = 0;
	auto ret = detail::readNumber<int64_t>(tmp.ptr, tmp.len, 0, offset);
	this->ptr += offset;
	this->len -= offset;
	return ret;
}

template <typename _CharType>
template <typename... Args>
auto StringViewBase<_CharType>::skipChars() -> void {
	size_t offset = 0;
	while (this->len > offset && match<Args...>(this->ptr[offset])) { ++offset; }
	auto off = sprt::min(offset, this->len);
	this->len -= off;
	this->ptr += off;
}

template <typename _CharType>
template <typename... Args>
auto StringViewBase<_CharType>::skipUntil() -> void {
	size_t offset = 0;
	while (this->len > offset && !match<Args...>(this->ptr[offset])) { ++offset; }
	auto off = sprt::min(offset, this->len);
	this->len -= off;
	this->ptr += off;
}

template <typename _CharType>
template <typename... Args>
auto StringViewBase<_CharType>::backwardSkipChars() -> void {
	while (this->len > 0 && match<Args...>(this->ptr[this->len - 1])) { --this->len; }
}

template <typename _CharType>
template <typename... Args>
auto StringViewBase<_CharType>::backwardSkipUntil() -> void {
	while (this->len > 0 && !match<Args...>(this->ptr[this->len - 1])) { --this->len; }
}

template <typename _CharType>
auto StringViewBase<_CharType>::skipString(const Self &str) -> bool {
	if (!this->ptr) {
		return false;
	}
	if (this->prefix(str.data(), str.size())) {
		auto s = sprt::min(str.size(), this->len);
		this->ptr += s;
		this->len -= s;
		return true;
	}
	return false;
}

template <typename _CharType>
auto StringViewBase<_CharType>::skipUntilString(const Self &str, bool stopBeforeString) -> bool {
	if (!this->ptr) {
		return false;
	}

	while (this->len > 0 && !this->prefix(str.data(), str.size())) {
		this->ptr += 1;
		this->len -= 1;
	}
	if (this->len > 0 && *this->ptr != 0 && !stopBeforeString) {
		skipString(str);
	}

	return this->len > 0 && *this->ptr != 0;
}

template <typename _CharType>
template <typename... Args>
auto StringViewBase<_CharType>::readChars() -> Self {
	auto tmp = *this;
	skipChars<Args...>();
	return Self(tmp.data(), tmp.size() - this->size());
}

template <typename _CharType>
template <typename... Args>
auto StringViewBase<_CharType>::readUntil() -> Self {
	auto tmp = *this;
	skipUntil<Args...>();
	return Self(tmp.data(), tmp.size() - this->size());
}

template <typename _CharType>
template <typename... Args>
auto StringViewBase<_CharType>::backwardReadChars() -> Self {
	auto tmp = *this;
	backwardSkipChars<Args...>();
	return Self(this->data() + this->size(), tmp.size() - this->size());
}

template <typename _CharType>
template <typename... Args>
auto StringViewBase<_CharType>::backwardReadUntil() -> Self {
	auto tmp = *this;
	backwardSkipUntil<Args...>();
	return Self(this->data() + this->size(), tmp.size() - this->size());
}

template <typename _CharType>
auto StringViewBase<_CharType>::readUntilString(const Self &str) -> Self {
	auto tmp = *this;
	skipUntilString(str);
	return Self(tmp.data(), tmp.size() - this->size());
}

template <typename _CharType>
template <typename Separator, typename Callback>
auto StringViewBase<_CharType>::split(const Callback &cb) const -> void {
	Self str(*this);
	while (!str.empty()) {
		str.skipChars<Separator>();
		auto tmp = str.readUntil<Separator>();
		if (!tmp.empty()) {
			cb(tmp);
		}
	}
}

template <typename _CharType>
template <typename... Args>
auto StringViewBase<_CharType>::trimChars() -> void {
	this->skipChars<Args...>();
	if (!this->empty()) {
		this->backwardSkipChars<Args...>();
	}
}

template <typename _CharType>
template <typename... Args>
auto StringViewBase<_CharType>::trimUntil() -> void {
	this->skipUntil<Args...>();
	if (!this->empty()) {
		this->backwardSkipUntil<Args...>();
	}
}

template <typename _CharType>
template <typename... Args>
auto StringViewBase<_CharType>::match(CharType c) -> bool {
	return chars::Compose<CharType, Args...>::match(c);
}


inline StringViewUtf8::StringViewUtf8() : BytesReader(nullptr, 0) { }

inline StringViewUtf8::StringViewUtf8(const char *ptr, size_t len)
: BytesReader(ptr, detail::length(ptr, len)) { }

inline StringViewUtf8::StringViewUtf8(const char *ptr, size_t pos, size_t len)
: BytesReader(ptr + pos, detail::length(ptr + pos, len)) { }

inline StringViewUtf8::StringViewUtf8(const StringViewUtf8 &ptr, size_t len)
: StringViewUtf8(ptr, 0, len) { }

inline StringViewUtf8::StringViewUtf8(const StringViewUtf8 &ptr, size_t pos, size_t len)
: BytesReader(ptr.data() + pos, min(len, ptr.size() - pos)) { }

inline auto StringViewUtf8::set(const char *p, size_t l) -> Self & {
	ptr = p;
	len = l;
	return *this;
}

inline bool StringViewUtf8::is(const char &c) const { return len > 0 && *ptr == c; }
inline bool StringViewUtf8::is(const char16_t &c) const {
	return len > 0 && len >= sprt::unicode::utf8_length_data[uint8_t(*ptr)]
			&& sprt::unicode::utf8Decode32(ptr, len) == char32_t(c);
}
inline bool StringViewUtf8::is(const char32_t &c) const {
	return len > 0 && len >= sprt::unicode::utf8_length_data[uint8_t(*ptr)]
			&& sprt::unicode::utf8Decode32(ptr, len) == c;
}
inline bool StringViewUtf8::is(const char *c) const { return prefix(c, strlen(c)); }
inline bool StringViewUtf8::is(const Self &c) const { return prefix(c.data(), c.size()); }

template <char32_t C>
inline bool StringViewUtf8::is() const {
	return len > 0 && len >= sprt::unicode::utf8_length_data[uint8_t(*ptr)]
			&& sprt::unicode::utf8Decode32(ptr, len) == C;
}

template <CharGroupId G>
inline bool StringViewUtf8::is() const {
	return len > 0 && len >= sprt::unicode::utf8_length_data[uint8_t(*ptr)]
			&& chars::CharGroup<MatchCharType, G>::match(sprt::unicode::utf8Decode32(ptr, len));
}

template <typename M>
inline bool StringViewUtf8::is() const {
	return len > 0 && len >= sprt::unicode::utf8_length_data[uint8_t(*ptr)]
			&& M::match(sprt::unicode::utf8Decode32(ptr, len));
}

inline char32_t StringViewUtf8::getChar() const {
	if (!empty()) {
		uint8_t off = 0;
		auto ret = sprt::unicode::utf8Decode32(this->ptr, this->len, off);
		if (off > len) {
			// invalid codepoint in view
			return 0;
		}
		return ret;
	} else {
		return 0;
	}
}

inline char32_t StringViewUtf8::readChar() {
	if (!empty()) {
		uint8_t off = 0;
		auto ret = sprt::unicode::utf8Decode32(this->ptr, this->len, off);
		if (off > len) {
			// invalid codepoint in view
			offset(len);
			return 0;
		}
		offset(off);
		return ret;
	} else {
		return 0;
	}
}

inline auto StringViewUtf8::letter() const -> Self {
	if (this->len > 0) {
		return Self(this->ptr,
				sprt::min(this->len, size_t(sprt::unicode::utf8_length_data[uint8_t(*ptr)])));
	}
	return Self();
}
template <typename Interface>
inline auto StringViewUtf8::str() const -> typename Interface::StringType {
	if (this->ptr && this->len > 0) {
		return typename Interface::StringType(this->ptr, this->len);
	}
	return typename Interface::StringType();
}

// extend offset functions with unicode support
inline void StringViewUtf8::offset(size_t l) {
	while (l > 0 && len > 0) {
		++(*this);
		--l;
	}
}
inline auto StringViewUtf8::operator++() -> Self & {
	if (len > 0) {
		auto l = sprt::min(size_t(sprt::unicode::utf8_length_data[uint8_t(*ptr)]), len);
		ptr += l;
		len -= l;
	}
	return *this;
}
inline auto StringViewUtf8::operator++(int) -> Self {
	auto tmp = *this;
	++(*this);
	return tmp;
}
inline auto StringViewUtf8::operator+=(size_t l) -> Self & {
	offset(l);
	return *this;
}

inline bool StringViewUtf8::isSpace() const {
	auto tmp = *this;
	tmp.skipChars<WhiteSpace>();
	return tmp.empty();
}

inline auto StringViewUtf8::begin() const -> Self { return Self(this->ptr, this->len); }
inline auto StringViewUtf8::end() const -> Self { return Self(this->ptr + this->len, 0); }
inline auto StringViewUtf8::operator-(const Self &other) const -> Self {
	if (this->ptr > other.ptr && size_t(this->ptr - other.ptr) < this->len) {
		return Self(this->ptr, this->ptr - other.ptr);
	}
	return Self();
}
inline auto StringViewUtf8::operator-=(const Self &other) -> Self & {
	if (this->ptr > other.ptr && size_t(this->ptr - other.ptr) < this->len) {
		this->len = this->ptr - other.ptr;
		return *this;
	}
	return *this;
}
inline auto StringViewUtf8::operator*() const -> MatchCharType {
	return sprt::unicode::utf8Decode32(ptr, len);
}

template <typename Callback>
inline void StringViewUtf8::foreach (const Callback &cb) const {
	static_assert(sprt::is_invocable_v<Callback, MatchCharType>);
	auto p = ptr;
	const auto e = ptr + len;
	while (p < e) {
		const uint8_t mask = sprt::unicode::utf8_length_mask[uint8_t(*p)];
		const uint8_t len = sprt::unicode::utf8_length_data[uint8_t(*p)];
		uint32_t ret = *p++ & mask;
		for (uint8_t c = 1; c < len; ++c) {
			const auto ch = *p++;
			if ((ch & 0xc0) != 0x80) {
				ret = 0;
				break;
			}
			ret <<= 6;
			ret |= (ch & 0x3f);
		}
		cb(MatchCharType(ret));
	}
}

inline size_t StringViewUtf8::code_size() const {
	size_t ret = 0;
	auto p = ptr;
	const auto e = ptr + len;
	while (p < e) {
		++ret;
		p += sprt::unicode::utf8_length_data[uint8_t(*p)];
	}
	return ret;
}

inline StringViewUtf8::operator StringViewBase<char>() const {
	return StringViewBase<char>(ptr, len);
}

inline Result<float> StringViewUtf8::readFloat() {
	Self tmp = *this;
	tmp.skipChars<CharGroup<CharGroupId::WhiteSpace>>();
	uint8_t offset = 0;
	auto ret = detail::readNumber<float>(tmp.ptr, tmp.len, 0, offset);
	this->ptr += offset;
	this->len -= offset;
	return ret;
}
inline Result<double> StringViewUtf8::readDouble() {
	Self tmp = *this;
	tmp.skipChars<CharGroup<CharGroupId::WhiteSpace>>();
	uint8_t offset = 0;
	auto ret = detail::readNumber<double>(tmp.ptr, tmp.len, 0, offset);
	this->ptr += offset;
	this->len -= offset;
	return ret;
}
inline Result<int64_t> StringViewUtf8::readInteger(int base) {
	Self tmp = *this;
	tmp.skipChars<CharGroup<CharGroupId::WhiteSpace>>();
	uint8_t offset = 0;
	auto ret = detail::readNumber<int64_t>(tmp.ptr, tmp.len, 0, offset);
	this->ptr += offset;
	this->len -= offset;
	return ret;
}

template <typename... Args>
inline void StringViewUtf8::skipChars() {
	uint8_t clen = 0;
	size_t offset = 0;
	while (len > offset
			&& match<Args...>(sprt::unicode::utf8Decode32(ptr + offset, len - offset, clen))
			&& clen > 0) {
		offset += clen;
	}
	auto off = sprt::min(offset, len);
	len -= off;
	ptr += off;
}

template <typename... Args>
inline void StringViewUtf8::skipUntil() {
	uint8_t clen = 0;
	size_t offset = 0;
	while (len > offset
			&& !match<Args...>(sprt::unicode::utf8Decode32(ptr + offset, len - offset, clen))
			&& clen > 0) {
		offset += clen;
	}
	auto off = sprt::min(offset, len);
	len -= off;
	ptr += off;
}

template <typename... Args>
inline void StringViewUtf8::backwardSkipChars() {
	uint8_t clen = 0;
	while (this->len > 0 && rv_match_utf8<Args...>(this->ptr, this->len, clen)) {
		if (clen > 0) {
			this->len -= sprt::min(size_t(clen), this->len);
		} else {
			return;
		}
	}
}

template <typename... Args>
inline void StringViewUtf8::backwardSkipUntil() {
	uint8_t clen = 0;
	while (this->len > 0 && !rv_match_utf8<Args...>(this->ptr, this->len, clen)) {
		if (clen > 0) {
			this->len -= sprt::min(size_t(clen), this->len);
			;
		} else {
			return;
		}
	}
}

inline bool StringViewUtf8::skipString(const Self &str) {
	if (!ptr) {
		return false;
	}
	if (this->prefix(str.data(), str.size())) {
		auto s = sprt::min(len, str.size());
		ptr += s;
		len -= s;
		return true;
	}
	return false;
}
inline bool StringViewUtf8::skipUntilString(const Self &str, bool stopBeforeString) {
	if (!ptr) {
		return false;
	}

	while (this->len > 0 && !this->prefix(str.data(), str.size())) {
		this->ptr += 1;
		this->len -= 1;
	}
	if (this->len > 0 && *this->ptr != 0 && !stopBeforeString) {
		skipString(str);
	}

	return this->len > 0 && *this->ptr != 0;
}

template <typename... Args>
inline auto StringViewUtf8::readChars() -> Self {
	auto tmp = *this;
	skipChars<Args...>();
	return Self(tmp.data(), tmp.size() - this->size());
}

template <typename... Args>
inline auto StringViewUtf8::readUntil() -> Self {
	auto tmp = *this;
	skipUntil<Args...>();
	return Self(tmp.data(), tmp.size() - this->size());
}

template <typename... Args>
inline auto StringViewUtf8::backwardReadChars() -> Self {
	auto tmp = *this;
	backwardSkipChars<Args...>();
	return Self(this->data() + this->size(), tmp.size() - this->size());
}

template <typename... Args>
inline auto StringViewUtf8::backwardReadUntil() -> Self {
	auto tmp = *this;
	backwardSkipUntil<Args...>();
	return Self(this->data() + this->size(), tmp.size() - this->size());
}

inline auto StringViewUtf8::readUntilString(const Self &str) -> Self {
	auto tmp = *this;
	skipUntilString(str);
	return Self(tmp.data(), tmp.size() - this->size());
}

template <typename Separator, typename Callback>
inline void StringViewUtf8::split(const Callback &cb) const {
	Self str(*this);
	while (!str.empty()) {
		str.skipChars<Separator>();
		auto tmp = str.readUntil<Separator>();
		if (!tmp.empty()) {
			cb(tmp);
		}
	}
}

template <typename... Args>
inline void StringViewUtf8::trimChars() {
	this->skipChars<Args...>();
	if (!this->empty()) {
		this->backwardSkipChars<Args...>();
	}
}

template <typename... Args>
inline void StringViewUtf8::trimUntil() {
	this->skipUntil<Args...>();
	if (!this->empty()) {
		this->backwardSkipUntil<Args...>();
	}
}

template <typename... Args>
inline bool StringViewUtf8::rv_match_utf8(const CharType *ptr, size_t len, uint8_t &offset) {
	size_t nchars = 0;
	while (len > 0) {
		if (!sprt::unicode::isUtf8Surrogate(ptr[len - 1])) {
			++nchars;
			return match<Args...>(sprt::unicode::utf8Decode32(ptr + len - 1, nchars, offset));
		} else {
			--len;
			++nchars;
		}
	}
	offset = 0;
	return false;
}

template <typename... Args>
inline bool StringViewUtf8::match(MatchCharType c) {
	return chars::Compose<MatchCharType, Args...>::match(c);
}

template <endian Endianess>
inline constexpr BytesViewTemplate<Endianess>::BytesViewTemplate(const uint8_t *p, size_t l)
: BytesReader(p, l) { }

template <endian Endianess>
inline constexpr BytesViewTemplate<Endianess>::BytesViewTemplate(StringView str)
: BytesReader((const uint8_t *)str.data(), str.size()) { }

template <endian Endianess>
template <size_t Size>
inline constexpr BytesViewTemplate<Endianess>::BytesViewTemplate(
		const sprt::array<uint8_t, Size> &arr)
: BytesReader(arr.data(), Size) { }

template <endian Endianess>
template <endian OtherEndianess>
inline constexpr BytesViewTemplate<Endianess>::BytesViewTemplate(
		const BytesViewTemplate<OtherEndianess> &data)
: BytesReader(data.data(), data.size()) { }

template <endian Endianess>
template <endian OtherEndianess>
inline constexpr BytesViewTemplate<Endianess>::BytesViewTemplate(
		const BytesViewTemplate<OtherEndianess> ptr, size_t len)
: BytesReader(ptr.data(), min(len, ptr.size())) { }

template <endian Endianess>
template <endian OtherEndianess>
inline constexpr BytesViewTemplate<Endianess>::BytesViewTemplate(
		const BytesViewTemplate<OtherEndianess> ptr, size_t pos, size_t len)
: BytesReader(ptr.data() + pos, min(len, ptr.size() - pos)) { }

template <endian Endianess>
auto BytesViewTemplate<Endianess>::set(const uint8_t *p, size_t l) -> Self & {
	ptr = p;
	len = l;
	return *this;
}

template <endian Endianess>
auto BytesViewTemplate<Endianess>::operator++() -> Self & {
	if (len > 0) {
		ptr++;
		len--;
	}
	return *this;
}

template <endian Endianess>
auto BytesViewTemplate<Endianess>::operator++(int) -> Self & {
	if (len > 0) {
		ptr++;
		len--;
	}
	return *this;
}

template <endian Endianess>
auto BytesViewTemplate<Endianess>::operator+=(size_t l) -> Self & {
	if (len > 0) {
		offset(l);
	}
	return *this;
}

template <endian Endianess>
auto BytesViewTemplate<Endianess>::operator==(const Self &other) const -> bool {
	return len == other.len
			&& (ptr == other.ptr || memcmp(ptr, other.ptr, len * sizeof(CharType)) == 0);
}

template <endian Endianess>
auto BytesViewTemplate<Endianess>::operator!=(const Self &other) const -> bool {
	return !(*this == other);
}

template <endian Endianess>
auto BytesViewTemplate<Endianess>::pdup(memory::pool_t *p) const -> Self {
	if (!p) {
		p = sprt::memory::pool::acquire();
	}
	auto buf = (uint8_t *)sprt::memory::pool::palloc(p, this->size() * sizeof(uint8_t));
	memcpy(buf, this->data(), this->size() * sizeof(uint8_t));
	return Self(buf, this->size());
}

template <endian Endianess>
template <typename Interface>
auto BytesViewTemplate<Endianess>::bytes() const -> typename Interface::BytesType {
	return typename Interface::BytesType(data(), data() + size());
}

template <endian Endianess>
template <typename T>
auto BytesViewTemplate<Endianess>::convert(const uint8_t *data) -> T {
	T res;
	memcpy(&res, data, sizeof(T));
	return Converter<T>::Swap(res);
};

template <endian Endianess>
template <uint8_t... Args>
auto BytesViewTemplate<Endianess>::skipChars() -> void {
	size_t offset = 0;
	while (this->len > offset && match<Args...>(this->ptr[offset])) { ++offset; }
	auto off = sprt::min(offset, this->len);
	this->len -= off;
	this->ptr += off;
}

template <endian Endianess>
template <uint8_t... Args>
auto BytesViewTemplate<Endianess>::skipUntil() -> void {
	size_t offset = 0;
	while (this->len > offset && !match<Args...>(this->ptr[offset])) { ++offset; }
	auto off = sprt::min(offset, this->len);
	this->len -= off;
	this->ptr += off;
}

template <endian Endianess>
template <uint8_t... Args>
auto BytesViewTemplate<Endianess>::backwardSkipChars() -> void {
	while (this->len > 0 && match<Args...>(this->ptr[this->len - 1])) { --this->len; }
}

template <endian Endianess>
template <uint8_t... Args>
auto BytesViewTemplate<Endianess>::backwardSkipUntil() -> void {
	while (this->len > 0 && !match<Args...>(this->ptr[this->len - 1])) { --this->len; }
}

template <endian Endianess>
template <uint8_t... Args>
auto BytesViewTemplate<Endianess>::readChars() -> Self {
	auto tmp = *this;
	skipChars<Args...>();
	return Self(tmp.data(), tmp.size() - this->size());
}

template <endian Endianess>
template <uint8_t... Args>
auto BytesViewTemplate<Endianess>::readUntil() -> Self {
	auto tmp = *this;
	skipUntil<Args...>();
	return Self(tmp.data(), tmp.size() - this->size());
}

template <endian Endianess>
template <uint8_t... Args>
auto BytesViewTemplate<Endianess>::backwardReadChars() -> Self {
	auto tmp = *this;
	backwardSkipChars<Args...>();
	return Self(this->data() + this->size(), tmp.size() - this->size());
}

template <endian Endianess>
template <uint8_t... Args>
auto BytesViewTemplate<Endianess>::backwardReadUntil() -> Self {
	auto tmp = *this;
	backwardSkipUntil<Args...>();
	return Self(this->data() + this->size(), tmp.size() - this->size());
}

template <endian Endianess>
template <typename Separator, typename Callback>
auto BytesViewTemplate<Endianess>::split(const Callback &cb) const -> void {
	Self str(*this);
	while (!str.empty()) {
		str.skipChars<Separator>();
		auto tmp = str.readUntil<Separator>();
		if (!tmp.empty()) {
			cb(tmp);
		}
	}
}

template <endian Endianess>
template <uint8_t... Args>
auto BytesViewTemplate<Endianess>::trimChars() -> void {
	this->skipChars<Args...>();
	if (!this->empty()) {
		this->backwardSkipChars<Args...>();
	}
}

template <endian Endianess>
template <uint8_t... Args>
auto BytesViewTemplate<Endianess>::trimUntil() -> void {
	this->skipUntil<Args...>();
	if (!this->empty()) {
		this->backwardSkipUntil<Args...>();
	}
}

template <endian Endianess>
template <uint8_t Arg, uint8_t... Args>
auto BytesViewTemplate<Endianess>::match(CharType c) -> bool {
	if (c == Arg) {
		return true;
	}
	if constexpr (sizeof...(Args) > 0) {
		return match<Args...>(c);
	}
	return false;
}

template <endian Endianess>
auto BytesViewTemplate<Endianess>::readUnsigned64() -> uint64_t {
	uint64_t ret = 0;
	if (len >= 8) {
		ret = convert<uint64_t>(ptr);
		ptr += 8;
		len -= 8;
	}
	return ret;
}

template <endian Endianess>
auto BytesViewTemplate<Endianess>::readUnsigned32() -> uint32_t {
	uint32_t ret = 0;
	if (len >= 4) {
		ret = convert<uint32_t>(ptr);
		ptr += 4;
		len -= 4;
	}
	return ret;
}

template <endian Endianess>
auto BytesViewTemplate<Endianess>::readUnsigned24() -> uint32_t {
	uint32_t ret = 0;
	if (len >= 3) {
		ret = (*ptr << 16) + (*(ptr + 1) << 8) + *(ptr + 2);
		ptr += 3;
		len -= 3;
	}
	return ret;
}

template <endian Endianess>
auto BytesViewTemplate<Endianess>::readUnsigned16() -> uint16_t {
	uint16_t ret = 0;
	if (len >= 2) {
		ret = convert<uint16_t>(ptr);
		ptr += 2;
		len -= 2;
	}
	return ret;
}

template <endian Endianess>
auto BytesViewTemplate<Endianess>::readUnsigned() -> uint8_t {
	uint8_t ret = 0;
	if (len > 0) {
		ret = *ptr;
		++ptr;
		--len;
	}
	return ret;
}

template <endian Endianess>
auto BytesViewTemplate<Endianess>::readFloat64() -> double {
	double ret = 0;
	if (len >= 8) {
		ret = convert<double>(ptr);
		ptr += 8;
		len -= 8;
	}
	return ret;
}

template <endian Endianess>
auto BytesViewTemplate<Endianess>::readFloat32() -> float {
	float ret = 0;
	if (len >= 4) {
		ret = convert<float>(ptr);
		ptr += 4;
		len -= 4;
	}
	return ret;
}

template <endian Endianess>
auto BytesViewTemplate<Endianess>::readFloat16() -> float {
	return halffloat::decode(readUnsigned16());
}

// read null-terminated string
template <endian Endianess>
auto BytesViewTemplate<Endianess>::readString() -> StringView {
	size_t offset = 0;
	while (len - offset && ptr[offset]) { offset++; }
	StringView ret((const char *)ptr, offset);
	ptr += offset;
	len -= offset;
	if (len && *ptr == 0) {
		++ptr;
		--len;
	}
	return ret;
}

// read fixed-size string
template <endian Endianess>
auto BytesViewTemplate<Endianess>::readString(size_t s) -> StringView {
	if (len < s) {
		s = len;
	}
	StringView ret((const char *)ptr, s);
	ptr += s;
	len -= s;
	return ret;
}

template <endian Endianess>
auto BytesViewTemplate<Endianess>::toStringView() const -> StringView {
	return StringView((const char *)ptr, len);
}

template <endian Endianess>
template <endian Target>
auto BytesViewTemplate<Endianess>::readBytes(size_t s) -> BytesViewTemplate<Target> {
	if (len < s) {
		s = len;
	}
	BytesViewTemplate<Target> ret(ptr, s);
	ptr += s;
	len -= s;
	return ret;
}

template <endian Endianess>
template <typename T>
auto BytesViewTemplate<Endianess>::readSpan(size_t s) -> SpanView<T> {
	if (len < s * sizeof(T)) {
		s = len / sizeof(T);
	}

	SpanView<T> ret(reinterpret_cast<const T *>(ptr), s);
	ptr += s * sizeof(T);
	len -= s * sizeof(T);
	return ret;
}

template <typename Type, size_t Size>
auto makeSpanView(const sprt::array<Type, Size> &vec) -> SpanView<Type> {
	return SpanView<Type>(vec);
}

template <typename Type>
auto makeSpanView(const Type *ptr, size_t size) -> SpanView<Type> {
	return SpanView<Type>(ptr, size);
}

template <typename Type, size_t Size>
auto makeSpanView(const Type (&array)[Size]) -> SpanView<Type> {
	return SpanView<Type>(array, Size);
}

} // namespace sprt


#endif
