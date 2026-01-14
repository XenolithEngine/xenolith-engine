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
#include <sprt/runtime/unicode.h>
#include <sprt/runtime/halffloat.h>
#include <sprt/runtime/detail/compare.h>
#include <sprt/runtime/mem/pool.h>
#include <sprt/runtime/mem/context.h>
#include <sprt/runtime/mem/string.h>
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
	Self &operator=(const CharType (&c)[Size]);

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
	requires requires(StringViewBase<CharType> v) {
		typename Interface::template BasicStringType<CharType>(v.data(), v.size());
	}
	auto str() const -> typename Interface::template BasicStringType<CharType> {
		if (this->ptr && this->len > 0) {
			return typename Interface::template BasicStringType<CharType>(this->ptr, this->len);
		} else {
			return typename Interface::template BasicStringType<CharType>();
		}
	}

	template <typename StringType>
	requires requires(StringViewBase<CharType> v) { StringType(v.data(), v.size()); }
	auto str() const -> StringType {
		if (this->ptr && this->len > 0) {
			return StringType(this->ptr, this->len);
		} else {
			return StringType();
		}
	}

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

	template <size_t Size>
	constexpr BytesViewTemplate(const uint8_t (&arr)[Size]);

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
	requires requires(StringViewBase<CharType> v) {
		typename Interface::BytesType(v.data(), v.data() + v.size());
	}
	auto bytes() const -> typename Interface::BytesType {
		return typename Interface::BytesType(data(), data() + size());
	}

	template <typename BytesType>
	requires requires(BytesViewTemplate<Endianess> v) { BytesType(v.data(), v.data() + v.size()); }
	auto bytes() const -> BytesType {
		return BytesType(data(), data() + size());
	}

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
using BytesViewNetwork = BytesViewTemplate<endian::network>;
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

	template < typename InputIt, typename = enable_if_t<is_convertible_v<_Type *, InputIt>> >
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
		return *this;
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
	requires requires(SpanView<Type> v) {
		typename Interface::template VectorType<Type>(v.data(), v.data() + v.size());
	}
	auto vec() const -> typename Interface::template VectorType<Type> {
		return typename Interface::template VectorType<Type>(ptr, ptr + len);
	}

	template <typename VectorType>
	requires requires(SpanView<Type> v) { VectorType(v.data(), v.data() + v.size()); }
	auto vec() const -> VectorType {
		return VectorType(data(), data() + size());
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

} // namespace sprt

namespace sprt::unicode {

SPRT_API bool isValidUtf8(StringView);

inline size_t getUtf16Length(char32_t c) { return sprt::unicode::utf16EncodeLength(c); }
SPRT_API size_t getUtf16Length(const StringView &str);
SPRT_API size_t getUtf16HtmlLength(const StringView &str);

inline size_t getUtf8Length(char32_t c) { return sprt::unicode::utf8EncodeLength(c); }
inline size_t getUtf8Length(char16_t c) { return sprt::unicode::utf8EncodeLength(c); }
SPRT_API size_t getUtf8HtmlLength(const StringView &str);
SPRT_API size_t getUtf8Length(const WideStringView &str);
SPRT_API size_t getUtf8Length(const StringViewBase<char32_t> &str);

SPRT_API Status toUtf16(char16_t *buf, size_t bufSize, const StringView &data,
		size_t *ret = nullptr);

SPRT_API Status toUtf16(char16_t *buf, size_t bufSize, char32_t ch, size_t *ret = nullptr);

SPRT_API Status toUtf16Html(char16_t *buf, size_t bufSize, const StringView &data,
		size_t *ret = nullptr);

SPRT_API Status toUtf16(const callback<void(WideStringView)> &, const StringView &data);

SPRT_API Status toUtf16Html(const callback<void(WideStringView)> &, const StringView &data);

SPRT_API Status toUtf8(char *, size_t bufSize, const WideStringView &data, size_t *ret = nullptr);

SPRT_API Status toUtf8(char *, size_t bufSize, char16_t c, size_t *ret = nullptr);

SPRT_API Status toUtf8(char *, size_t bufSize, char32_t c, size_t *ret = nullptr);

SPRT_API Status toUtf8(const callback<void(StringView)> &, const WideStringView &data);

SPRT_API char32_t toupper(char32_t);
SPRT_API char32_t totitle(char32_t);
SPRT_API char32_t tolower(char32_t);

SPRT_API bool toupper(const callback<void(StringView)> &, StringView);
SPRT_API bool totitle(const callback<void(StringView)> &, StringView);
SPRT_API bool tolower(const callback<void(StringView)> &, StringView);

SPRT_API bool toupper(const callback<void(WideStringView)> &, WideStringView);
SPRT_API bool totitle(const callback<void(WideStringView)> &, WideStringView);
SPRT_API bool tolower(const callback<void(WideStringView)> &, WideStringView);

SPRT_API bool compare(StringView l, StringView r, int *result);
SPRT_API bool compare(WideStringView l, WideStringView r, int *result);

SPRT_API bool caseCompare(StringView l, StringView r, int *result);
SPRT_API bool caseCompare(WideStringView l, WideStringView r, int *result);

} // namespace sprt::unicode

namespace sprt {

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

	return __constexpr_strlen(__p);
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
		return __constexpr_strlen(__p);
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
	auto ret = __constexpr_strcompare(lPtr, rPtr, __len);
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
		auto lc = __constexpr_toupper_c(l);
		auto rc = __constexpr_toupper_c(r);
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

template <typename CharType>
inline bool operator<(const StringViewBase<CharType> &l, const StringViewBase<CharType> &r) {
	return sprt::detail::compare_c(l, r) < 0;
}

template <typename CharType>
inline bool operator<=(const StringViewBase<CharType> &l, const StringViewBase<CharType> &r) {
	return sprt::detail::compare_c(l, r) <= 0;
}

template <typename CharType>
inline bool operator>=(const StringViewBase<CharType> &l, const StringViewBase<CharType> &r) {
	return sprt::detail::compare_c(l, r) >= 0;
}

template <typename CharType>
inline bool operator>(const StringViewBase<CharType> &l, const StringViewBase<CharType> &r) {
	return sprt::detail::compare_c(l, r) > 0;
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
template <typename Interface, typename... Args>
void StringViewBase<_CharType>::merge(const callback<void(StringViewBase<CharType>)> &,
		Args &&...args) { }

template <typename _CharType>
template <typename Interface, _CharType c, typename... Args>
void StringViewBase<_CharType>::merge(const callback<void(StringViewBase<CharType>)> &,
		Args &&...args) { }

template <typename _CharType>
template <typename T>
inline size_t StringViewBase<_CharType>::__size(const T &t) {
	return t.size();
}

template <typename _CharType>
inline size_t StringViewBase<_CharType>::__size(const CharType *c) {
	return __constexpr_strlen(c);
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
inline auto StringViewBase<_CharType>::operator=(const CharType (&c)[Size]) -> Self & {
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
		__constexpr_memcpy(buf, this->data(), this->size() * sizeof(_CharType));
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
		for (size_t i = 0; i < this->size(); ++i) { buf[i] = __constexpr_tolower_c(buf[i]); }
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
		for (size_t i = 0; i < this->size(); ++i) { buf[i] = __constexpr_toupper_c(buf[i]); }
		buf[this->size()] = 0;
		return Self(buf, this->size());
	}
	return Self();
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
	return this->prefix(c, __constexpr_strlen(c));
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
	auto ret = detail::readNumber<int64_t>(tmp.ptr, tmp.len, base, offset);
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
inline bool StringViewUtf8::is(const char *c) const { return prefix(c, __constexpr_strlen(c)); }
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
template <size_t Size>
inline constexpr BytesViewTemplate<Endianess>::BytesViewTemplate(const uint8_t (&arr)[Size])
: BytesReader(arr, Size) { }

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
template <typename T>
auto BytesViewTemplate<Endianess>::convert(const uint8_t *data) -> T {
	T res;
	__builtin_memcpy(&res, data, sizeof(T));
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

template <typename T>
requires requires(T x) { SpanView(x.data(), x.size()); }
auto makeSpanView(const T &t) {
	return SpanView<sprt::remove_cv_t<sprt::remove_reference_t<decltype(*t.data())>>>(t);
}

inline auto __convertIntToTwc(int v) {
	if (v < 0) {
		return std::partial_ordering::less;
	} else if (v > 0) {
		return std::partial_ordering::greater;
	} else {
		return std::partial_ordering::equivalent;
	}
}

template <typename Compare>
inline auto __compareDataRanges(const uint8_t *l, size_t __lsize, const uint8_t *r, size_t __rsize,
		const Compare &cmp) {
	auto commonLen = sprt::min(__lsize, __rsize);
	if (commonLen == 0) {
		return __lsize <=> __rsize;
	} else {
		auto ret = __constexpr_memcmp(l, r, sprt::min(__lsize, __rsize));
		if (ret < 0) {
			return sprt::strong_ordering::less;
		} else if (ret > 0) {
			return sprt::strong_ordering::greater;
		} else {
			return __lsize <=> __rsize;
		}
	}
}

template <typename CharType>
inline auto operator<=>(const StringViewBase<CharType> &l, const StringViewBase<CharType> &r) {
	return __convertIntToTwc(sprt::detail::compare_c(l, r));
}

template <typename CharType>
inline auto operator<=>(const StringViewBase<CharType> &l,
		const memory::basic_string<CharType> &r) {
	return __convertIntToTwc(sprt::detail::compare_c(l, r));
}

template <typename CharType>
inline auto operator<=>(const memory::basic_string<CharType> &l,
		const StringViewBase<CharType> &r) {
	return __convertIntToTwc(sprt::detail::compare_c(l, r));
}

template <typename CharType>
inline auto operator<=>(const StringViewBase<CharType> &l, const CharType *r) {
	return __convertIntToTwc(sprt::detail::compare_c(l, StringViewBase<CharType>(r)));
}

template <typename CharType>
inline auto operator<=>(const CharType *l, const StringViewBase<CharType> &r) {
	return __convertIntToTwc(sprt::detail::compare_c(StringViewBase<CharType>(l), r));
}

inline auto operator<=>(const StringViewUtf8 &l, const StringViewUtf8 &r) {
	return __convertIntToTwc(sprt::detail::compare_u(l, r));
}

inline auto operator<=>(const StringViewUtf8 &l, const char *r) {
	return __convertIntToTwc(sprt::detail::compare_u(l, StringViewUtf8(r)));
}

inline auto operator<=>(const char *l, const StringViewUtf8 &r) {
	return __convertIntToTwc(sprt::detail::compare_u(StringViewUtf8(l), r));
}

template <sprt::endian Endianess, size_t Size>
inline auto operator<=>(const BytesViewTemplate<Endianess> &l,
		const sprt::array<uint8_t, Size> &r) {
	return __compareDataRanges(l.data(), l.size(), r.data(), r.size());
}

template <sprt::endian Endianess, size_t Size>
inline auto operator<=>(const sprt::array<uint8_t, Size> &l,
		const BytesViewTemplate<Endianess> &r) {
	return __compareDataRanges(l.data(), l.size(), r.data(), r.size());
}

template <sprt::endian Endianess>
inline auto operator<=>(const BytesViewTemplate<Endianess> &l,
		const BytesViewTemplate<Endianess> &r) {
	return __compareDataRanges(l.data(), l.size(), r.data(), r.size());
}

template <typename _Tp>
inline auto operator<=>(const SpanView<_Tp> &__x, const SpanView<_Tp> &__y) {
	return sprt::lexicographical_compare_three_way(__x.begin(), __x.end(), __y.begin(), __y.end());
}

} // namespace sprt


#endif
