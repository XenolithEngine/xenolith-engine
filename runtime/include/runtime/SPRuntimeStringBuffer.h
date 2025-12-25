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

#ifndef CORE_RUNTIME_PRIVATE_SPRTSTRINGBUFFER_H_
#define CORE_RUNTIME_PRIVATE_SPRTSTRINGBUFFER_H_

#include "SPRuntimeString.h"

namespace sprt {

template <typename CharType>
struct StringBuffer {
	template <typename... Args>
	static StringBuffer<CharType> create(Args &&...args) {
		StringBuffer<CharType> ret;

		size_t targetSize = 0;
		__processArgs<CharType>([&](StringView str) { targetSize += str.size(); },
				forward<Args>(args)...);

		targetSize += 1; // nullterm

		auto target = ret.prepare(targetSize);

		__processArgs<CharType>([&](StringView str) {
			target = strappend(target, &targetSize, str.data(), str.size());
		}, forward<Args>(args)...);

		ret.commit(target - ret.data());
		return ret;
	}

	~StringBuffer() {
		if (_buffer) {
			freeCharBuffer(_buffer);
		}
	}

	StringBuffer() = default;

	StringBuffer(StringView str) {
		if (str.empty()) {
			return;
		}

		size_t targetSize = str.size();
		auto target = prepare(targetSize);

		target = strappend(target, &targetSize, str.data(), str.size());

		commit(target - data());
	}

	StringBuffer(const StringBuffer &other) = delete;
	StringBuffer &operator=(const StringBuffer &other) = delete;

	StringBuffer(StringBuffer &&other) {
		_buffer = other._buffer;
		_length = other._length;
		_capacity = other._capacity;

		other._buffer = nullptr;
		other._length = 0;
		other._capacity = 0;
	}

	StringBuffer &operator=(StringBuffer &&other) {
		clear();

		_buffer = other._buffer;
		_length = other._length;
		_capacity = other._capacity;

		other._buffer = nullptr;
		other._length = 0;
		other._capacity = 0;

		return *this;
	}

	void clear() {
		if (_buffer) {
			freeCharBuffer(_buffer);
			_buffer = nullptr;
			_capacity = 0;
			_length = 0;
		}
	}

	CharType *prepare(size_t &size) {
		if (_capacity < size) {
			clear();
			_buffer = newCharBuffer<CharType>(size + 1);
			_capacity = size;
			_length = 0;
		}
		_buffer[0] = 0;
		size = _capacity;
		return _buffer;
	}

	void commit(size_t size) { _length = min(size, _capacity); }

	size_t capacity() const { return _capacity; }
	size_t size() const { return _length; }
	CharType *data() const { return _buffer; }

	operator StringViewBase<CharType>() const { return StringViewBase<CharType>(_buffer, _length); }

	size_t _length = 0;
	size_t _capacity = 0;
	CharType *_buffer = nullptr;
};

} // namespace sprt

#endif // CORE_RUNTIME_PRIVATE_SPRTSTRINGBUFFER_H_
