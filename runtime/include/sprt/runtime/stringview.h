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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_STRINGVIEW_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_STRINGVIEW_H_

#include <sprt/runtime/math.h>
#include <sprt/runtime/array.h>
#include <sprt/runtime/notnull.h>
#include <sprt/runtime/chargroup.h>
#include <sprt/runtime/string.h>
#include <sprt/runtime/hash.h>
#include <sprt/runtime/endian.h>
#include <sprt/runtime/detail/operations.h>
#include <sprt/runtime/detail/constexpr.h>
#include <sprt/runtime/mem/pool.h>
#include <sprt/runtime/mem/context.h>
#include <sprt/runtime/mem/detail/pointer_iterator.h>

#include <sprt/c/__sprt_string.h>

namespace sprt {

template <typename _Type>
class SpanView;

// Compares data blocks byte by byte
struct BytesComparator {
	template <typename CharT>
	static int compare(const CharT *l, const CharT *r, size_t size) {
		return ::__sprt_memcmp(l, r, size * sizeof(CharT));
	}
};

// Compares strings based on C locale
struct StringComparator;

// Compares strings based on C locale and is case insensitive
struct StringCaseComparator;

// Compares strings as UTF8 or UTF16
struct StringUnicodeComparator;

// Compares strings as UTF8 or UTF16, case insensitive
struct StringUnicodeCaseComparator;

template <typename _CharType>
class BytesReader {
public:
	using CharType = _CharType;
	using value_type = CharType;

	template <CharType... Args>
	using Chars = chars::Chars<CharType, Args...>;

	template <CharType First, CharType Last>
	using Range = chars::Range<CharType, First, Last>;

	template <CharGroupId Group>
	using CharGroup = chars::CharGroup<CharType, Group>;

	template <typename... Args>
	using Compose = chars::Compose<CharType, Args...>;

	constexpr BytesReader() = default;
	constexpr BytesReader(const CharType *p, size_t l) : ptr(p), len(l) { }

	BytesReader &set(const CharType *p, size_t l) {
		ptr = p;
		len = l;
		return *this;
	}

	void offset(size_t l) {
		if (l > len) {
			len = 0;
		} else {
			ptr += l;
			len -= l;
		}
	}

	template <typename Comparator = BytesComparator>
	bool equals(const CharType *d, size_t l) const;

	template <typename Comparator = BytesComparator>
	bool equals(const CharType *d) const;

	template <typename Comparator = BytesComparator>
	bool equals(const BytesReader<CharType> &str) const {
		return equals<Comparator>(str.data(), str.size());
	}

	template <typename Comparator = BytesComparator>
	bool prefix(const CharType *d, size_t l) const;

	template <typename Comparator = BytesComparator>
	bool starts_with(const BytesReader<CharType> &str) const {
		return prefix<Comparator>(str.data(), str.size());
	}

	template <typename Comparator = BytesComparator>
	bool starts_with(const CharType *d, size_t l) const {
		return prefix<Comparator>(d, l);
	}

	template <typename Comparator = BytesComparator>
	bool starts_with(const CharType *d) const;

	template <size_t Count, typename Comparator = BytesComparator>
	bool starts_with(const CharType d[Count]) const;

	bool starts_with(CharType c) const { return is(c); }

	template <typename Comparator = BytesComparator>
	bool ends_with(const BytesReader<CharType> &str) const {
		return ends_with<Comparator>(str.data(), str.size());
	}

	template <typename Comparator = BytesComparator>
	bool ends_with(const CharType *d, size_t l) const;

	template <typename Comparator = BytesComparator>
	bool ends_with(const CharType *d) const;

	template <size_t Count, typename Comparator = BytesComparator>
	bool ends_with(const CharType d[Count]) const;

	bool ends_with(CharType c) const { return len > 0 && ptr[len - 1] == c; }

	constexpr const CharType *data() const { return ptr; }

	constexpr size_t size() const { return len; }

	constexpr size_t find(const CharType *s, size_t pos, size_t n) const;
	constexpr size_t find(CharType c, size_t pos = 0) const;

	constexpr size_t rfind(const CharType *s, size_t pos, size_t n) const;
	constexpr size_t rfind(CharType c, size_t pos = Max<size_t>) const;

	constexpr size_t find(const BytesReader<CharType> &str, size_t pos = 0) const {
		return this->find(str.data(), pos, str.size());
	}
	constexpr size_t find(const CharType *s, size_t pos = 0) const {
		return this->find(s, pos, __constexpr_strlen(s));
	}

	constexpr size_t rfind(const BytesReader<CharType> &str, size_t pos = Max<size_t>) const {
		return this->rfind(str.data(), pos, str.size());
	}
	constexpr size_t rfind(const CharType *s, size_t pos = Max<size_t>) const {
		return this->rfind(s, pos, __constexpr_strlen(s));
	}

	bool is(const CharType &c) const { return len > 0 && *ptr == c; };

	bool operator>(const size_t &val) const { return len > val; }
	bool operator>=(const size_t &val) const { return len >= val; }
	bool operator<(const size_t &val) const { return len < val; }
	bool operator<=(const size_t &val) const { return len <= val; }

	CharType front() const { return *ptr; }
	CharType back() const { return ptr[len - 1]; }

	CharType at(const size_t &s) const { return s < len ? ptr[s] : 0; }
	CharType operator[](const size_t &s) const { return s < len ? ptr[s] : 0; }
	CharType operator*() const { return *ptr; }

	void clear() { len = 0; }
	bool empty() const { return len == 0 || !ptr; }

	bool terminated() const { return ptr[len] == 0; }

	template <typename Callback>
	void performWithTerminated(const Callback &cb) {
		static_assert(is_invocable_v<Callback, const CharType *, size_t>,
				"Invalid callback type (expected (const Char *, size_t))");
		if (terminated()) {
			cb(data(), size());
		} else {
			auto buf = (char *)__sprt_malloca((size() + 1) * sizeof(CharType));
			__sprt_memcpy(buf, data(), size() * sizeof(CharType));
			buf[size()] = 0;
			cb((const CharType *)buf, size());
			__sprt_freea(buf);
		}
	}

protected:
	const CharType *ptr = nullptr;
	size_t len = 0;
};


template <typename _CharType>
class StringViewBase : public BytesReader<_CharType> {
public:
	using Self = StringViewBase;
	using MatchCharType = _CharType;
	using CharType = _CharType;
	using value_type = _CharType;

	using iterator =
			memory::detail::pointer_iterator<const _CharType, const _CharType *, const _CharType &>;
	using reverse_iterator = memory::detail::pointer_reverse_iterator<iterator>;

	template <CharType... Args>
	using MatchChars = chars::Chars<CharType, Args...>;

	template <char First, char Last>
	using MatchRange = chars::Range<CharType, First, Last>;

	template <CharGroupId Group>
	using MatchCharGroup = chars::CharGroup<CharType, Group>;

	// CharGroup shortcuts
	using Numbers = MatchCharGroup<CharGroupId::Numbers>;
	using Latin = MatchCharGroup<CharGroupId::Latin>;
	using WhiteSpace = MatchCharGroup<CharGroupId::WhiteSpace>;
	using LatinLowercase = MatchCharGroup<CharGroupId::LatinLowercase>;
	using LatinUppercase = MatchCharGroup<CharGroupId::LatinUppercase>;
	using Alphanumeric = MatchCharGroup<CharGroupId::Alphanumeric>;
	using Hexadecimial = MatchCharGroup<CharGroupId::Hexadecimial>;
	using Base64 = MatchCharGroup<CharGroupId::Base64>;

	template <typename Interface, typename... Args>
	static auto merge(Args &&...args) -> typename Interface::template BasicStringType<CharType>;

	template <typename Interface, _CharType c, typename... Args>
	static auto merge(Args &&...args) -> typename Interface::template BasicStringType<CharType>;

	template <typename Interface, typename... Args>
	static void merge(const callback<void(StringViewBase<CharType>)> &, Args &&...args);

	template <typename Interface, _CharType c, typename... Args>
	static void merge(const callback<void(StringViewBase<CharType>)> &, Args &&...args);

	constexpr StringViewBase() = default;
	constexpr StringViewBase(const CharType *ptr, size_t len = Max<size_t>);
	constexpr StringViewBase(const CharType *ptr, size_t pos, size_t len);
	constexpr StringViewBase(const Self &, size_t pos, size_t len);
	constexpr StringViewBase(const Self &, size_t len);

	template <typename T>
	requires requires(T x) { StringViewBase(x.data(), x.size()); }
	constexpr StringViewBase(const T &str) : StringViewBase(str.data(), str.size()) { }

	constexpr Self &operator=(const Self &str) = default;

	template <typename T>
	requires requires(T x) { StringViewBase(x.data(), x.size()); }
	constexpr Self &operator=(const T &str) {
		set(str.data(), str.size());
		return *this;
	}

	Self &operator=(const CharType *);

	template <size_t Size>
	Self &operator=(const CharType c[Size]);

	template <size_t Size>
	constexpr Self &operator=(const array<CharType, Size> &);

	constexpr operator sprt::StringViewBase<CharType>() const {
		return sprt::StringViewBase<CharType>(this->data(), this->size());
	}

	// unsafe set, without length-check
	Self &set(const CharType *p, size_t l);

	bool is(const CharType &c) const;
	bool is(const CharType *c) const;
	bool is(const Self &c) const;

	template <_CharType C>
	bool is() const;
	template <CharGroupId G>
	bool is() const;
	template <typename M>
	bool is() const;

	Self sub(size_t pos = 0, size_t len = Max<size_t>) const {
		if (pos > this->size()) {
			return Self();
		}

		return StringViewBase(*this, pos, len);
	}

	Self pdup(memory::pool_t * = nullptr) const;

	Self ptolower_c(memory::pool_t * = nullptr) const;
	Self ptoupper_c(memory::pool_t * = nullptr) const;

	template <typename Interface>
	auto str() const -> typename Interface::template BasicStringType<CharType>;

	Self &operator++();
	Self operator++(int);
	Self &operator+=(size_t l);

	iterator begin() const;
	iterator end() const;

	Self operator-(const Self &) const;
	Self &operator-=(const Self &) const;

	constexpr uint64_t hash() const {
		return sprt::hash64((const char *)this->data(), this->size() * sizeof(CharType));
	}

	constexpr uint32_t hash32() const {
		return sprt::hash32((const char *)this->data(), uint32_t(this->size() * sizeof(CharType)));
	}

public:
	Result<float> readFloat();
	Result<double> readDouble();
	Result<int64_t> readInteger(int base = 0);

public:
	template <typename... Args>
	void skipChars();
	template <typename... Args>
	void skipUntil();

	template <typename... Args>
	void backwardSkipChars();
	template <typename... Args>
	void backwardSkipUntil();

	bool skipString(const Self &str);
	bool skipUntilString(const Self &str, bool stopBeforeString = true);

	template <typename... Args>
	Self readChars();
	template <typename... Args>
	Self readUntil();

	template <typename... Args>
	Self backwardReadChars();
	template <typename... Args>
	Self backwardReadUntil();

	Self readUntilString(const Self &str);

	template <typename Separator, typename Callback>
	void split(const Callback &cb) const;

	template <typename... Args>
	void trimChars();
	template <typename... Args>
	void trimUntil();

protected:
	template <typename T>
	static size_t __size(const T &);

	static size_t __size(const CharType *);

	template <typename T, typename... Args>
	static size_t _size(T &&);

	template <typename T, typename... Args>
	static size_t _size(T &&, Args &&...args);

	template <typename Buf, typename T>
	static void __merge(Buf &, T &&t);

	template <typename Buf>
	static void __merge(Buf &, const CharType *);

	template <typename Buf, typename T, typename... Args>
	static void _merge(Buf &, T &&, Args &&...args);

	template <typename Buf, typename T>
	static void _merge(Buf &, T &&);

	template <typename Buf, _CharType C, bool Front, typename T>
	static void __mergeWithSep(Buf &, T &&t);

	template <typename Buf, _CharType C, bool Front, typename T, typename... Args>
	static void _mergeWithSep(Buf &, T &&, Args &&...args);

	template <typename Buf, _CharType C, bool Front, typename T>
	static void _mergeWithSep(Buf &, T &&);

	template <typename... Args>
	bool match(CharType c);
};

class StringViewUtf8 : public BytesReader<char> {
public:
	using Self = StringViewUtf8;
	using MatchCharType = char32_t;
	using CharType = char;
	using value_type = char;

	template <MatchCharType... Args>
	using MatchChars = chars::Chars<MatchCharType, Args...>;

	template <char First, char Last>
	using MatchRange = chars::Range<MatchCharType, First, Last>;

	template <CharGroupId Group>
	using MatchCharGroup = chars::CharGroup<MatchCharType, Group>;

	template <typename... Args>
	using MatchCompose = chars::Compose<MatchCharType, Args...>;

	template <MatchCharType... Args>
	using Chars = chars::Chars<MatchCharType, Args...>;

	template <char First, char Last>
	using Range = chars::Range<MatchCharType, First, Last>;

	template <CharGroupId Group>
	using CharGroup = chars::CharGroup<MatchCharType, Group>;

	template <typename... Args>
	using Compose = chars::Compose<MatchCharType, Args...>;

	// CharGroup shortcuts
	using Numbers = MatchCharGroup<CharGroupId::Numbers>;
	using Latin = MatchCharGroup<CharGroupId::Latin>;
	using WhiteSpace = MatchCharGroup<CharGroupId::WhiteSpace>;
	using LatinLowercase = MatchCharGroup<CharGroupId::LatinLowercase>;
	using LatinUppercase = MatchCharGroup<CharGroupId::LatinUppercase>;
	using Alphanumeric = MatchCharGroup<CharGroupId::Alphanumeric>;
	using Hexadecimial = MatchCharGroup<CharGroupId::Hexadecimial>;
	using Base64 = MatchCharGroup<CharGroupId::Base64>;

	StringViewUtf8();
	StringViewUtf8(const char *ptr, size_t len = Max<size_t>);
	StringViewUtf8(const char *ptr, size_t pos, size_t len);
	StringViewUtf8(const StringViewUtf8 &, size_t len);
	StringViewUtf8(const StringViewUtf8 &, size_t pos, size_t len);

	template <typename T>
	requires requires(T x) { StringViewUtf8(x.data(), x.size()); }
	constexpr StringViewUtf8(const T &str) : StringViewUtf8(str.data(), str.size()) { }

	constexpr Self &operator=(const Self &str) = default;

	template <typename T>
	requires requires(T x) { StringViewUtf8(x.data(), x.size()); }
	constexpr Self &operator=(const T &str) {
		set(str.data(), str.size());
		return *this;
	}

	Self &set(const char *p, size_t l);

	bool is(const char &c) const;
	bool is(const char16_t &c) const;
	bool is(const char32_t &c) const;
	bool is(const char *c) const;
	bool is(const Self &c) const;

	template <char32_t C>
	bool is() const;
	template <CharGroupId G>
	bool is() const;
	template <typename M>
	bool is() const;

	Self sub(size_t pos = 0, size_t len = Max<size_t>) const {
		return StringViewUtf8(*this, pos, len);
	}

	char32_t getChar() const;
	char32_t readChar();

	Self letter() const;

	template <typename Interface>
	auto str() const -> typename Interface::StringType;

	void offset(size_t l);
	Self &operator++();
	Self operator++(int);
	Self &operator+=(size_t l);

	bool isSpace() const;

	Self begin() const;
	Self end() const;

	Self operator-(const Self &) const;
	Self &operator-=(const Self &);

	MatchCharType operator*() const;

	template <typename Callback>
	void foreach (const Callback &cb) const;

	size_t code_size() const;

	operator StringViewBase<char>() const;

	constexpr uint64_t hash() const { return sprt::hash64(data(), size() * sizeof(CharType)); }

	constexpr uint64_t hash32() const {
		return sprt::hash32(data(), uint32_t(size() * sizeof(CharType)));
	}

public:
	Result<float> readFloat();
	Result<double> readDouble();
	Result<int64_t> readInteger(int base = 0);

public:
	template <typename... Args>
	void skipChars();
	template <typename... Args>
	void skipUntil();

	template <typename... Args>
	void backwardSkipChars();
	template <typename... Args>
	void backwardSkipUntil();

	bool skipString(const Self &str);
	bool skipUntilString(const Self &str, bool stopBeforeString = true);

	template <typename... Args>
	Self readChars();
	template <typename... Args>
	Self readUntil();

	template <typename... Args>
	Self backwardReadChars();
	template <typename... Args>
	Self backwardReadUntil();

	Self readUntilString(const Self &str);
	template <typename Separator, typename Callback>
	void split(const Callback &cb) const;

	template <typename... Args>
	void trimChars();
	template <typename... Args>
	void trimUntil();

protected: // char-matching inline functions
	template <typename... Args>
	bool rv_match_utf8(const CharType *ptr, size_t len, uint8_t &offset);
	template <typename... Args>
	bool match(MatchCharType c);
};

template <endian Endianess = endian::network>
class BytesViewTemplate : public BytesReader<uint8_t> {
public:
	template <class T>
	using Converter = byteorder::ConverterTraits<Endianess, T>;

	using Self = BytesViewTemplate<Endianess>;

	constexpr BytesViewTemplate() = default;
	constexpr BytesViewTemplate(const uint8_t *p, size_t l);
	constexpr BytesViewTemplate(StringViewBase<char>);

	template <typename T>
	requires requires(T x) { BytesViewTemplate(x.data(), x.size()); }
	constexpr BytesViewTemplate(const T &str) : BytesViewTemplate(str.data(), str.size()) { }

	template <size_t Size>
	constexpr BytesViewTemplate(const array<uint8_t, Size> &arr);

	constexpr BytesViewTemplate(const Self &vec) = default;

	template <endian OtherEndianess>
	constexpr BytesViewTemplate(const BytesViewTemplate<OtherEndianess> &vec);

	template <endian OtherEndianess>
	constexpr BytesViewTemplate(const BytesViewTemplate<OtherEndianess>, size_t len);

	template <endian OtherEndianess>
	constexpr BytesViewTemplate(const BytesViewTemplate<OtherEndianess>, size_t pos, size_t len);

	Self &operator=(const Self &b) = default;

	template <typename T>
	requires requires(T x) { BytesViewTemplate(x.data(), x.size()); }
	constexpr Self &operator=(const T &str) {
		set(str.data(), str.size());
		return *this;
	}

	Self &set(const uint8_t *p, size_t l);

	Self &operator++();
	Self &operator++(int);
	Self &operator+=(size_t l);

	bool operator==(const Self &other) const;
	bool operator!=(const Self &other) const;

	Self pdup(memory::pool_t * = nullptr) const;

	template <typename Interface>
	auto bytes() const -> typename Interface::BytesType;

	constexpr Self sub(size_t pos = 0, size_t len = Max<size_t>) const {
		return Self(*this, pos, len);
	}

private:
	template <typename T>
	auto convert(const uint8_t *data) -> T;

	bool match(CharType c) { return false; }
	template <uint8_t Arg, uint8_t... Args>
	bool match(CharType c);

public:
	template <uint8_t... Args>
	void skipChars();
	template <uint8_t... Args>
	void skipUntil();

	template <uint8_t... Args>
	void backwardSkipChars();
	template <uint8_t... Args>
	void backwardSkipUntil();

	template <uint8_t... Args>
	Self readChars();
	template <uint8_t... Args>
	Self readUntil();

	template <uint8_t... Args>
	Self backwardReadChars();
	template <uint8_t... Args>
	Self backwardReadUntil();

	template <uint8_t... Args>
	void trimChars();
	template <uint8_t... Args>
	void trimUntil();

	template <typename Separator, typename Callback>
	void split(const Callback &cb) const;

	uint64_t readUnsigned64();
	uint32_t readUnsigned32();
	uint32_t readUnsigned24();
	uint16_t readUnsigned16();
	uint8_t readUnsigned();

	double readFloat64();
	float readFloat32();
	float readFloat16();

	StringViewBase<char> readString(); // read null-terminated string
	StringViewBase<char> readString(size_t s); // read fixed-size string

	StringViewBase<char> toStringView() const;

	template <endian OtherEndianess = Endianess>
	auto readBytes(size_t s) -> BytesViewTemplate<OtherEndianess>; // read fixed-size string

	template <typename T>
	auto readSpan(size_t) -> SpanView<T>;
};

using StringView = StringViewBase<char>;
using WideStringView = StringViewBase<char16_t>;

using BytesView = BytesViewTemplate<endian::native>;
using BytesViewNetwork = BytesViewTemplate<endian::native>;
using BytesViewHost = BytesViewTemplate<endian::native>;

template <typename _Type>
class SpanView {
public:
	using Type = _Type;
	using Self = SpanView<Type>;
	using iterator = memory::detail::pointer_iterator<const Type, const Type *, const Type &>;
	using reverse_iterator = memory::detail::pointer_reverse_iterator<iterator>;

	constexpr SpanView() = default;
	constexpr SpanView(const Type *p, size_t l) : ptr(p), len(l) { }
	constexpr SpanView(const Type *begin, const Type *end) : ptr(begin), len(end - begin) { }

	static Self alloc(memory::pool_t *pool, size_t count) {
		auto mem = (Type *)sprt::memory::pool::palloc(pool, count * sizeof(Type));
		return Self(mem, count);
	}

	template <size_t N>
	SpanView(const _Type value[N]) : ptr(&value[0]), len(N) { }

	template < typename InputIt, typename = enable_if_t<is_convertible<_Type *, InputIt>::value> >
	SpanView(InputIt first, InputIt last) : ptr(&(*first)), len(sprt::distance(first, last)) { }

	template <typename T>
	requires requires(T x) { SpanView(x.data(), x.size()); }
	constexpr SpanView(const T &vec) : SpanView(vec.data(), vec.size()) { }

	template <typename T>
	requires requires(T x) { SpanView(x.data(), x.size()); }
	constexpr SpanView(const T &vec, size_t count)
	: SpanView(vec.data(), min(vec.size(), count)) { }

	template <typename T>
	requires requires(T x) { SpanView(x.data(), x.size()); }
	constexpr SpanView(const T &vec, size_t off, size_t count)
	: SpanView(vec.data() + min(off, vec.size()),
			  min(vec.size() > off ? vec.size() - off : 0, count)) { }

	template <size_t Size>
	SpanView(const Type (&array)[Size]) : ptr(&array[0]), len(Size) { }

	SpanView(const Self &v) = default;
	SpanView(const Self &v, size_t len) : ptr(v.data()), len(min(len, v.size())) { }
	SpanView(const Self &v, size_t off, size_t count)
	: ptr(v.data() + min(off, v.size())), len(min(v.size() > off ? v.size() - off : 0, count)) { }

	template <typename T>
	requires requires(T x) { SpanView(x.data(), x.size()); }
	Self &operator=(const T &vec) {
		set(vec.data(), vec.size());
	}

	constexpr Self &operator=(const Self &v) = default;

	Self &set(const Type *p, size_t l) {
		ptr = p;
		len = l;
		return *this;
	}

	void offset(size_t l) {
		if (l > len) {
			len = 0;
		} else {
			ptr += l;
			len -= l;
		}
	}

	const Type *data() const { return ptr; }
	size_t size() const { return len; }

	iterator begin() const noexcept { return iterator(ptr); }
	iterator end() const noexcept { return iterator(ptr + len); }

	reverse_iterator rbegin() const noexcept { return reverse_iterator(end()); }
	reverse_iterator rend() const noexcept { return reverse_iterator(begin()); }

	bool operator>(const size_t &val) const { return len > val; }
	bool operator>=(const size_t &val) const { return len >= val; }
	bool operator<(const size_t &val) const { return len < val; }
	bool operator<=(const size_t &val) const { return len <= val; }

	Self &operator++() {
		if (len > 0) {
			ptr++;
			len--;
		}
		return *this;
	}
	Self operator++(int) {
		auto tmp = *this;
		if (len > 0) {
			ptr++;
			len--;
		}
		return tmp;
	}
	Self &operator+=(size_t l) {
		if (len > 0) {
			offset(l);
		}
		return *this;
	}

	bool operator==(const Self &other) const {
		if (len == other.size()) {
			auto it1 = begin();
			auto it2 = other.begin();
			auto itEnd = end();

			while (it1 != itEnd) {
				if (*it1++ != *it2++) {
					return false;
				}
			}
			return true;
		}
		return false;
	}
	bool operator!=(const Self &other) const {
		if (len == other.size()) {
			auto it1 = begin();
			auto it2 = other.begin();
			auto itEnd = end();

			while (it1 != itEnd) {
				if (*it1++ != *it2++) {
					return true;
				}
			}
			return false;
		}
		return true;
	}

	const Type &front() const { return *ptr; }
	const Type &back() const { return ptr[len - 1]; }

	const Type &at(const size_t &s) const { return ptr[s]; }
	const Type &operator[](const size_t &s) const { return ptr[s]; }
	const Type &operator*() const { return *ptr; }

	void clear() { len = 0; }
	bool empty() const { return len == 0 || !ptr; }

	Self first(size_t count) const { return Self(ptr, min(count, len)); }
	Self last(size_t count) const {
		return (count < len) ? Self(ptr + len - count, count) : Self(ptr, len);
	}

	Self pop_front(size_t count = 1) {
		auto ret = first(count);
		offset(count);
		return ret;
	}
	Self pop_back(size_t count = 1) {
		auto ret = last(count);
		len -= ret.size();
		return ret;
	}

	template <typename Interface>
	auto vec() const -> typename Interface::template VectorType<Type> {
		return typename Interface::template VectorType<Type>(ptr, ptr + len);
	}

	BytesView bytes() const { return BytesView((uint8_t *)ptr, len * sizeof(Type)); }

	Self pdup(memory::pool_t *p = nullptr) const {
		if (!p) {
			p = memory::pool::acquire();
		}
		auto buf = (Type *)sprt::memory::pool::palloc(p, this->size() * sizeof(Type));
		memcpy((void *)buf, this->data(), this->size() * sizeof(Type));
		return Self(buf, this->size());
	}

	constexpr size_t hash() const {
		if constexpr (sizeof(size_t) == 4) {
			return sprt::hash32((const char *)data(), size() * sizeof(_Type));
		} else {
			return sprt::hash64((const char *)data(), size() * sizeof(_Type));
		}
	}

	Self sub(size_t pos = 0, size_t len = Max<size_t>) const { return Self(*this, pos, len); }

protected:
	const Type *ptr = nullptr;
	size_t len = 0;
};

using StringView = StringViewBase<char>;
using WideStringView = StringViewBase<char16_t>;

namespace status {

SPRT_API StringView getStatusName(Status status);

SPRT_API void getStatusDescription(Status st, const callback<void(StringView)> &cb);

} // namespace status

//
// Implementation
//

template <typename CharType>
constexpr inline size_t BytesReader<CharType>::find(const CharType *__s, size_t __pos,
		size_t __n) const {
	const size_t __size = this->size();
	const CharType *__data = data();

	if (__n == 0) {
		return __pos <= __size ? __pos : Max<size_t>;
	} else if (__n <= __size) {
		for (; __pos <= __size - __n; ++__pos) {
			if (__constexpr_chareq(__data[__pos], __s[0])
					&& __constexpr_strcompare(__data + __pos + 1, __s + 1, __n - 1) == 0) {
				return __pos;
			}
		}
	}
	return Max<size_t>;
}

template <typename CharType>
constexpr inline size_t BytesReader<CharType>::find(CharType __c, size_t __pos) const {
	size_t __ret = Max<size_t>;
	const size_t __size = this->size();
	if (__pos < __size) {
		const CharType *__data = data();
		const size_t __n = __size - __pos;
		const CharType *__p = __constexpr_strfind(__data + __pos, __n, __c);
		if (__p) {
			__ret = __p - __data;
		}
	}
	return __ret;
}

template <typename CharType>
constexpr inline size_t BytesReader<CharType>::rfind(const CharType *__s, size_t __pos,
		size_t __n) const {
	const size_t __size = this->size();
	if (__n <= __size) {
		__pos = min(size_t(__size - __n), __pos);
		const CharType *__data = data();
		do {
			if (__constexpr_strcompare(__data + __pos, __s, __n) == 0) {
				return __pos;
			}
		} while (__pos-- > 0);
	}
	return Max<size_t>;
}

template <typename CharType>
constexpr inline size_t BytesReader<CharType>::rfind(CharType __c, size_t __pos) const {
	size_t __size = this->size();
	if (__size) {
		if (--__size > __pos) {
			__size = __pos;
		}
		for (++__size; __size-- > 0;) {
			if (__constexpr_chareq(data()[__size], __c)) {
				return __size;
			}
		}
	}
	return Max<size_t>;
}

} // namespace sprt

#include <sprt/runtime/detail/stringview.h>

#endif
