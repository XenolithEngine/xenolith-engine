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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_STACK_BUFFER_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_STACK_BUFFER_H_

#include <sprt/runtime/detail/constexpr.h>
#include <sprt/runtime/array.h>
#include <sprt/runtime/stringview.h>

namespace sprt {

template <size_t Size>
class StackBuffer {
public:
	StackBuffer() { }
	StackBuffer(const StackBuffer &rhs) : _size(rhs._size), _buf(rhs._buf) { }
	StackBuffer(StackBuffer &&rhs) : _size(rhs._size), _buf(rhs._buf) { }

	StackBuffer &operator=(const StackBuffer &rhs) {
		_buf = rhs._buf;
		_size = rhs._size;
		return *this;
	}
	StackBuffer &operator=(StackBuffer &&rhs) {
		_buf = rhs._buf;
		_size = rhs._size;
		return *this;
	}

	uint8_t &operator[](size_t n) { return _buf[n]; }
	const uint8_t &operator[](size_t n) const { return _buf[n]; }

	uint8_t &at(size_t n) { return _buf.at(n); }
	const uint8_t &at(size_t n) const { return _buf.at(n); }

	size_t size() const { return _size; }
	size_t capacity() const { return Size; }
	size_t remains() const { return Size - _size; }

	bool empty() const { return _size == 0; }
	bool full() const { return _size == Size; }

	void soft_clear() { _size = 0; }

	void clear() {
		memset(_buf.data(), 0, Size);
		_size = 0;
	}

	uint8_t *data() { return _buf.data(); }
	const uint8_t *data() const { return _buf.data(); }

	size_t put(const uint8_t *ptr, size_t s) {
		if (s + _size > Size) {
			s = Size - _size;
		}

		__constexpr_memcpy(_buf.data() + _size, ptr, s);
		_size += s;
		return s;
	}

	size_t putc(uint8_t c) { return put(&c, 1); }

	template <typename Reader = StringView>
	Reader get() const {
		return Reader((const typename Reader::CharType *)_buf.data(), _size);
	}

	uint8_t *prepare(size_t &size) {
		clear();
		return prepare_preserve(size);
	}

	uint8_t *prepare_preserve(size_t &size) {
		auto r = remains();
		if (r < size) {
			size = r;
		}
		return _buf.data() + _size;
	}

	void save(uint8_t *data, size_t nbytes) { _size += nbytes; }

protected:
	size_t _size = 0;
	array<uint8_t, Size> _buf;
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_STACK_BUFFER_H_
