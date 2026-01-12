/**
Copyright (c) 2016-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#ifndef STAPPLER_CORE_UTILS_SPBUFFER_H_
#define STAPPLER_CORE_UTILS_SPBUFFER_H_

#include "SPIOBuffer.h"
#include "SPStringStream.h"
#include "SPUnicode.h"

#include <sprt/runtime/stack_buffer.h>

namespace STAPPLER_VERSIONIZED stappler {

template <typename Interface>
class BufferTemplate : public Interface::AllocBaseType {
public:
	using byte_type = uint8_t;
	static constexpr size_t defsize = 256;

	BufferTemplate(size_t sz = defsize) {
		_buffer.resize(sz, 0);
		_ptr = (byte_type *)_buffer.data();
		_end = _ptr + sz;
	}
	BufferTemplate(const BufferTemplate &rhs) : _buffer(rhs._buffer) {
		_ptr = (byte_type *)(_buffer.data() + (rhs._ptr - rhs._buffer.data()));
		_end = (byte_type *)(_buffer.data() + _buffer.size());
		_input = rhs._input;
	}
	BufferTemplate(BufferTemplate &&rhs) {
		auto size = rhs._ptr - rhs._buffer.data();
		_buffer = sp::move(rhs._buffer);
		_ptr = (byte_type *)(_buffer.data() + size);
		_end = (byte_type *)(_buffer.data() + _buffer.size());
		_input = rhs._input;
	}

	BufferTemplate &operator=(const BufferTemplate &rhs) {
		auto size = rhs._ptr - rhs._buffer.data();
		_buffer = rhs._buffer;
		_ptr = (byte_type *)(_buffer.data() + size);
		_end = (byte_type *)(_buffer.data() + _buffer.size());
		_input = rhs._input;
		return *this;
	}
	BufferTemplate &operator=(BufferTemplate &&rhs) {
		auto size = rhs._ptr - rhs._buffer.data();
		_buffer = sp::move(rhs._buffer);
		_ptr = (byte_type *)(_buffer.data() + size);
		_end = (byte_type *)(_buffer.data() + _buffer.size());
		_input = rhs._input;
		return *this;
	}

	template <typename CharType, typename std::enable_if<sizeof(CharType) == 1>::type * = nullptr>
	size_t put(const CharType *ptr, size_t len) {
		size_t ret = 0;
		while (ret < len) {
			const size_t bsize = _end - _ptr;
			if (bsize) {
				const size_t remaining = len - ret;
				const size_t nlen = min(bsize, remaining);
				memcpy((void *)_ptr, (const void *)ptr, nlen);
				ret += nlen;
				ptr += nlen;
				_ptr += nlen;
			}

			if (ret < len) {
				overflow(len - ret);
			}
		}
		if (_ptr > _buffer.data() + _input) {
			_input = _ptr - _buffer.data();
		}
		return ret;
	}

	size_t putc(char16_t c) {
		auto len = sprt::unicode::utf8EncodeLength(c);
		if (_end - _ptr < len) {
			overflow();
		}
		sprt::unicode::utf8EncodeBuf((char *)_ptr, _end - _ptr, c);
		_ptr += len;

		if (_ptr > _buffer.data() + _input) {
			_input = _ptr - _buffer.data();
		}
		return len;
	}

	size_t putc(char c) {
		if (_end == _ptr) {
			overflow();
		}
		*_ptr = *((byte_type *)&c);
		++_ptr;

		if (_ptr > _buffer.data() + _input) {
			_input = _ptr - _buffer.data();
		}
		return 1;
	}

	// toString interface adapter
	template <typename... Args,
			std::enable_if_t<string::detail::IsFastToStringAvailable<Args...>::value> * = nullptr>
	StringView putStrings(Args &&...args) {
		auto size = string::detail::getBufferSize(std::forward<Args>(args)...);
		size_t emptyBytes = capacity() - (_ptr - _buffer.data());
		if (emptyBytes < size) {
			overflow(emptyBytes);
		}

		char *start = (char *)_ptr;
		auto s = string::detail::writeBuffer(start, std::forward<Args>(args)...);
		if (s != size) {
			std::cout << "[core]: Invalid buffer size for toString<fast>\n";
			abort();
		}
		_ptr += s;
		if (_ptr > _buffer.data() + _input) {
			_input = _ptr - _buffer.data();
		}

		return StringView(start, s);
	}

	template <typename... Args,
			std::enable_if_t<string::detail::IsFastToStringAvailable<Args...>::value> * = nullptr>
	StringView resetWithStrings(Args &&...args) {
		clear();
		return putStrings(std::forward<Args>(args)...);
	}

	template <typename Reader = StringView>
	Reader get() const {
		return Reader((const typename Reader::CharType *)_buffer.data(), _ptr - _buffer.data());
	}

	template <typename Reader = StringView>
	Reader pop(size_t len) {
		size_t sz = size();
		len = min(len, sz);
		Reader r((const typename Reader::CharType *)(_buffer.data() + sz - len), len);
		_ptr = (byte_type *)(_buffer.data() + sz - len);
		return r;
	}

	template <typename Reader = StringView>
	Reader read(size_t len) {
		size_t input = _input;
		size_t sz = size();
		len = min(len, input - sz);
		Reader r((const typename Reader::CharType *)(_buffer.data() + sz), len);
		_ptr += len;
		return r;
	}

	void clear() {
		_ptr = (byte_type *)_buffer.data();
		_input = 0;
	}

	bool seek(size_t pos) {
		if (pos > _buffer.size()) {
			overflow(pos - _buffer.size());
		}
		_ptr = _buffer.data() + pos;
		if (_ptr > _buffer.data() + _input) {
			_input = _ptr - _buffer.data();
		}
		return true;
	}

	size_t capacity() const { return _buffer.size(); }

	size_t size() const { return _ptr - _buffer.data(); }

	size_t input() const { return _input; }

	bool empty() const { return _ptr == _buffer.data(); }

	auto str() const -> typename Interface::StringType {
		return typename Interface::StringType((char *)_buffer.data(), _ptr - _buffer.data());
	}

	uint8_t *data() { return _buffer.data(); }

	uint8_t *prepare(size_t &size) {
		clear();
		auto c = capacity();
		if (size > c) {
			size = c;
		}
		return _ptr;
	}

	void save(uint8_t *, size_t nbytes) {
		_ptr += nbytes;
		if (_ptr > _buffer.data() + _input) {
			_input = _ptr - _buffer.data();
		}
	}

protected:
	void overflow() {
		auto size = _ptr - _buffer.data();
		_buffer.resize(_buffer.size() * 2, 0);
		_ptr = (byte_type *)(_buffer.data() + size);
		_end = (byte_type *)(_buffer.data() + _buffer.size());
	}

	void overflow(size_t required) {
		if (required < _buffer.size()) {
			overflow();
		} else {
			auto size = _ptr - _buffer.data();
			_buffer.resize(_buffer.size() + required, 0);
			_ptr = (byte_type *)(_buffer.data() + size);
			_end = (byte_type *)(_buffer.data() + _buffer.size());
		}
	}

	typename Interface::BytesType _buffer;
	uint8_t *_ptr = nullptr;
	uint8_t *_end = nullptr;
	size_t _input = 0;
};

using sprt::StackBuffer;

} // namespace STAPPLER_VERSIONIZED stappler

namespace STAPPLER_VERSIONIZED stappler::io {

template <typename Interface>
struct BufferTraits<stappler::BufferTemplate<Interface>> {
	using type = stappler::BufferTemplate<Interface>;

	static uint8_t *PrepareFn(void *ptr, size_t &size) { return ((type *)ptr)->prepare(size); }

	// save written bytes in buffer
	static void SaveFn(void *ptr, uint8_t *buf, size_t prepared, size_t nbytes) {
		((type *)ptr)->save(buf, nbytes);
	}

	static size_t SizeFn(void *ptr) { return ((type *)ptr)->size(); }
	static size_t CapacityFn(void *ptr) { return ((type *)ptr)->capacity(); }
	static uint8_t *DataFn(void *ptr) { return ((type *)ptr)->data(); }
	static void ClearFn(void *ptr) { ((type *)ptr)->clear(); }
};

template <size_t Size>
struct BufferTraits<StackBuffer<Size>> {
	using type = StackBuffer<Size>;

	static uint8_t *PrepareFn(void *ptr, size_t &size) { return ((type *)ptr)->prepare(size); }

	// save written bytes in buffer
	static void SaveFn(void *ptr, uint8_t *buf, size_t source, size_t nbytes) {
		((type *)ptr)->save(buf, nbytes);
	}

	static size_t SizeFn(void *ptr) { return ((type *)ptr)->size(); }
	static size_t CapacityFn(void *ptr) { return ((type *)ptr)->capacity(); }
	static uint8_t *DataFn(void *ptr) { return ((type *)ptr)->data(); }
	static void ClearFn(void *ptr) { ((type *)ptr)->clear(); }
};

} // namespace stappler::io

#endif /* STAPPLER_CORE_UTILS_SPBUFFER_H_ */
