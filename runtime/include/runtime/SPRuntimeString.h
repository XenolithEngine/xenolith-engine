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

#ifndef CORE_RUNTIME_INCLUDE_SPRUNTIMESTRING_H_
#define CORE_RUNTIME_INCLUDE_SPRUNTIMESTRING_H_

#include "SPRuntimeInt.h"
#include "SPRuntimeArray.h"
#include "SPRuntimeNotNull.h"

#include <c/__sprt_string.h>

namespace sprt {

SPRT_API char *strappend(char SPRT_NONNULL *buf, size_t *bufSize, const char SPRT_NONNULL *str,
		size_t strSize);

SPRT_API char *strappend(char SPRT_NONNULL *buf, size_t *bufSize, int64_t);
SPRT_API char *strappend(char SPRT_NONNULL *buf, size_t *bufSize, uint64_t);
SPRT_API char *strappend(char SPRT_NONNULL *buf, size_t *bufSize, double);

// fast itoa implementation
// data will be written at the end of buffer, no trailing zero (do not try to use strlen on it!)
// designed to be used with StringView: StringView(buf + bufSize - ret, ret)

// use nullptr buffer to calculate expected buffer length

SPRT_API size_t itoa(int64_t number, char *buffer, size_t bufSize);
SPRT_API size_t itoa(uint64_t number, char *buffer, size_t bufSize);

SPRT_API size_t itoa(int64_t number, char16_t *buffer, size_t bufSize);
SPRT_API size_t itoa(uint64_t number, char16_t *buffer, size_t bufSize);

// fast dtoa implementation
// data will be written from beginning, no trailing zero (do not try to use strlen on it!)
// designed to be used with StringView: StringView(buf, ret)

// use nullptr buffer to calculate expected buffer length

static constexpr size_t INT_MAX_DIGITS = 22;
static constexpr size_t DOUBLE_MAX_DIGITS = 27;

SPRT_API size_t dtoa(double number, char *buffer, size_t bufSize);
SPRT_API size_t dtoa(double number, char16_t *buffer, size_t bufSize);

SPRT_API bool ispunct(char c);
SPRT_API bool isdigit(char c);
SPRT_API bool isalpha(char c);
SPRT_API bool isspace(char c);
SPRT_API bool islower(char c);
SPRT_API bool isupper(char c);
SPRT_API bool isalnum(char c);
SPRT_API bool isxdigit(char c);
SPRT_API bool isbase64(char c);
SPRT_API bool istpunct(char c);

SPRT_API size_t strlen(const char *);
SPRT_API size_t strlen(const char16_t *);

SPRT_API char *_makeCharBuffer(size_t);
SPRT_API char16_t *_makeChar16Buffer(size_t);

template <typename Type>
auto newCharBuffer(size_t) -> Type *;

template <>
inline auto newCharBuffer<char>(size_t size) -> char * {
	return _makeCharBuffer(size);
}

template <>
inline auto newCharBuffer<char16_t>(size_t size) -> char16_t * {
	return _makeChar16Buffer(size);
}

SPRT_API void freeCharBuffer(char *);
SPRT_API void freeCharBuffer(char16_t *);

template <typename Type>
struct SpanView {
	const Type *ptr = nullptr;
	size_t length = 0;

	constexpr SpanView() noexcept = default;

	constexpr SpanView(const Type *p, size_t len) noexcept : ptr(p), length(len) { }

	template <auto Count>
	constexpr SpanView(const Type (&c)[Count]) noexcept {
		this->ptr = c;
		this->length = Count;
	}

	template <auto Count>
	constexpr SpanView(const array<Type, Count> &v) noexcept {
		this->ptr = v.data();
		this->length = v.size();
	}

	constexpr bool is(const Type &c) const { return length > 0 && ptr[0] == c; }

	constexpr const Type *data() const { return ptr; }
	constexpr size_t size() const { return length; }
	constexpr bool empty() const { return length == 0; }

	constexpr const Type *begin() const { return ptr; }
	constexpr const Type *end() const { return ptr + length; }

	constexpr const Type front() const { return ptr[0]; }

	constexpr const Type &at(size_t idx) const { return ptr[idx]; }

	constexpr void offset(size_t l) {
		if (l > length) {
			length = 0;
		} else {
			ptr += l;
			length -= l;
		}
	}
};

template <typename Char>
struct StringViewBase : public SpanView<Char> {
	using SpanView<Char>::SpanView;

	StringViewBase(const Char *ptr) {
		this->ptr = ptr;
		this->length = strlen(ptr);
	}

	StringViewBase(const Char *p, size_t l) {
		this->ptr = p;
		this->length = l;
	}

	constexpr bool terminated() const { return this->ptr[this->length] == 0; }

	template <typename Callback>
	void performWithTerminated(const Callback &cb) const {
		if (terminated()) {
			cb(this->ptr, this->length);
		} else {
			Char data[this->length + 1];

			for (size_t i = 0; i < this->length; ++i) { data[i] = this->ptr[i]; }
			data[this->length] = 0;

			cb(data, this->length);
		}
	}

	const Char &operator[](size_t idx) const { return this->ptr[idx]; }

	bool equals(StringViewBase str) const {
		return this->length == str.size()
				&& ::__sprt_memcmp(this->ptr, str.data(), sizeof(Char) * this->length) == 0;
	}

	bool starts_with(StringViewBase str) const {
		return this->length >= str.size()
				&& ::__sprt_memcmp(this->ptr, str.data(), sizeof(Char) * str.size()) == 0;
	}

	StringViewBase sub(size_t pos) const {
		return StringViewBase(this->ptr + pos, this->length - pos);
	}

	StringViewBase sub(size_t pos, size_t len) const {
		return StringViewBase(this->ptr + pos, len);
	}

	size_t find(Char c) const {
		auto l = this->length;
		auto p = this->ptr;

		while (l > 0 && *p != c) {
			++p;
			--l;
		}

		if (l > 0) {
			return p - this->ptr;
		}
		return size_t(0) - 1;
	}

	size_t rfind(Char c) const {
		auto l = this->length;
		auto p = this->ptr + this->length - 1;

		while (l > 0 && *p != c) {
			--p;
			--l;
		}
		if (l > 0) {
			return p - this->ptr;
		}
		return size_t(0) - 1;
	}
};

using StringView = StringViewBase<char>;
using WideStringView = StringViewBase<char16_t>;

struct BytesView : public SpanView<uint8_t> {
	using SpanView::SpanView;

	template <typename T>
	constexpr auto readSpan(size_t s) -> SpanView<T> {
		if (length < s * sizeof(T)) {
			s = length / sizeof(T);
		}

		SpanView<T> ret{reinterpret_cast<const T *>(ptr), s};
		ptr += s * sizeof(T);
		length -= s * sizeof(T);
		return ret;
	}

	auto readString(size_t s) -> StringView {
		if (length < s) {
			s = length;
		}
		StringView ret((const char *)ptr, s);
		ptr += s;
		length -= s;
		return ret;
	}

	auto readString() -> StringView {
		size_t offset = 0;
		while (length - offset && ptr[offset]) { offset++; }
		StringView ret((const char *)ptr, offset);
		ptr += offset;
		length -= offset;
		if (length && *ptr == 0) {
			++ptr;
			--length;
		}
		return ret;
	}
};

} // namespace sprt

#endif // CORE_RUNTIME_INCLUDE_SPRUNTIMESTRING_H_
