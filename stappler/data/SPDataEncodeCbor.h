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

#ifndef STAPPLER_DATA_SPDATAENCODECBOR_H_
#define STAPPLER_DATA_SPDATAENCODECBOR_H_

#include "SPDataCbor.h"
#include "SPDataValue.h"

#if MODULE_STAPPLER_FILESYSTEM
#include "SPFilepath.h"
#include "SPFilesystem.h"
#endif

namespace STAPPLER_VERSIONIZED stappler::data::cbor {

template <typename Interface>
struct Encoder : public Interface::AllocBaseType {
	using InterfaceType = Interface;
	using ValueType = ValueTemplate<Interface>;

	enum Type {
		None,
		File,
		Buffered,
		Vector,
		Stream,
	};

	static typename Interface::BytesType encode(const typename ValueType::ArrayType &arr) {
		Encoder<Interface> enc(false);
		for (auto &it : arr) { it.encode(enc); }
		return enc.data();
	}

	static typename Interface::BytesType encode(const typename ValueType::DictionaryType &arr) {
		Encoder<Interface> enc(false);
		for (auto &it : arr) { it.encode(enc); }
		return enc.data();
	}

#if MODULE_STAPPLER_FILESYSTEM
	Encoder(const FileInfo &info) : type(File) {
		filesystem::enumerateWritablePaths(info, filesystem::Access::None,
				[&](StringView filename, FileFlags) {
			auto path = filesystem::native::posixToNative<Interface>(filename);
			file = new std::ofstream(path.data(), std::ios::binary);
			if (isOpen()) {
				cbor::_writeId(*this);
			}
			return false;
		});
	}
#endif

	Encoder(const Callback<void(BytesView)> *s) : type(Stream) {
		stream = s;
		if (isOpen()) {
			cbor::_writeId(*this);
		}
	}

	Encoder(bool prefix = false, size_t reserve = 1_KiB)
	: type(Interface::usesMemoryPool() ? Vector : Buffered) {
		static thread_local typename ValueType::BytesType tl_buffer;
		if constexpr (Interface::usesMemoryPool()) {
			buffer = new (std::nothrow) typename ValueType::BytesType();
		} else {
			if (reserve <= 1_KiB) {
				buffer = &tl_buffer;
				buffer->clear();
				reserve = 1_KiB;
			} else {
				type = Vector;
				buffer = new (std::nothrow) typename ValueType::BytesType();
			}
		}
		buffer->reserve(reserve);
		if (prefix && isOpen()) {
			cbor::_writeId(*this);
		}
	}

	~Encoder() {
		switch (type) {
		case File:
			file->flush();
			file->close();
			delete file;
			break;
		case Stream: break;
		case Vector: delete buffer; break;
		default: break;
		}
	}

	void emplace(uint8_t c) {
		switch (type) {
		case File: file->put(c); break;
		case Stream: (*stream) << c; break;
		case Buffered:
		case Vector: buffer->emplace_back(c); break;
		default: break;
		}
	}

	void emplace(const uint8_t *buf, size_t size) {
		if (type == Buffered) {
			switchBuffer(buffer->size() + size);
		}

		size_t tmpSize;
		switch (type) {
		case File: file->write((const std::ofstream::char_type *)buf, size); break;
		case Stream: (*stream) << BytesView(buf, size); break;
		case Buffered:
		case Vector:
			tmpSize = buffer->size();
			buffer->resize(tmpSize + size);
			memcpy(buffer->data() + tmpSize, buf, size);
			break;
		default: break;
		}
	}

	void switchBuffer(size_t newSize) {
		if (type == Buffered && newSize > 100_KiB) {
			type = Vector;
			auto newVec = new (std::nothrow) typename ValueType::BytesType();
			newVec->resize(buffer->size());
			memcpy(newVec->data(), buffer->data(), buffer->size());
			buffer = newVec;
		}
	}

	bool isOpen() const {
		switch (type) {
		case None: return false; break;
		case File: return file->is_open(); break;
		default: return true; break;
		}
		return false;
	}

	typename ValueType::BytesType data() {
		if (type == Buffered) {
			typename ValueType::BytesType ret;
			ret.resize(buffer->size());
			memcpy(ret.data(), buffer->data(), buffer->size());
			return ret;
		} else if (type == Vector) {
			typename ValueType::BytesType ret(sp::move(*buffer));
			return ret;
		}
		return typename ValueType::BytesType();
	}

public: // CBOR format impl
	inline void write(nullptr_t n) { _writeNull(*this, n); }
	inline void write(bool value) { _writeBool(*this, value); }
	inline void write(int64_t value) { _writeInt(*this, value); }
	inline void write(double value) { _writeFloat(*this, value); }
	inline void write(const typename ValueType::StringType &str) { _writeString(*this, str); }
	inline void write(const StringView &str) { _writeString(*this, str); }
	inline void write(const typename ValueType::BytesType &data) { _writeBytes(*this, data); }
	inline void onBeginArray(const typename ValueType::ArrayType &arr) {
		_writeArrayStart(*this, arr.size());
	}
	inline void onBeginDict(const typename ValueType::DictionaryType &dict) {
		_writeMapStart(*this, dict.size());
	}

private:
	union {
		typename ValueType::BytesType *buffer;
		std::ofstream *file;
		const Callback<void(BytesView)> *stream;
	};

	Type type;
};


template <typename Interface>
inline auto writeArray(const typename ValueTemplate<Interface>::ArrayType &arr) ->
		typename Interface::BytesType {
	return Encoder<Interface>::encode(arr);
}
template <typename Interface>
inline auto writeObject(const typename ValueTemplate<Interface>::DictionaryType &dict) ->
		typename Interface::BytesType {
	return Encoder<Interface>::encode(dict);
}

template <typename Interface>
inline auto write(const ValueTemplate<Interface> &data, size_t reserve = 1_KiB) ->
		typename Interface::BytesType {
	Encoder<Interface> enc(true, reserve);
	if (enc.isOpen()) {
		data.encode(enc);
		return enc.data();
	}
	return typename Interface::BytesType();
}

template <typename Interface>
inline bool write(const Callback<void(BytesView)> &stream, const ValueTemplate<Interface> &data) {
	Encoder<Interface> enc(&stream);
	if (enc.isOpen()) {
		data.encode(enc);
		return true;
	}
	return false;
}

#if MODULE_STAPPLER_FILESYSTEM
template <typename Interface>
inline bool save(const ValueTemplate<Interface> &data, const FileInfo &file) {
	Encoder<Interface> enc(file);
	if (enc.isOpen()) {
		data.encode(enc);
		return true;
	}
	return false;
}
#endif

} // namespace stappler::data::cbor

#endif /* STAPPLER_DATA_SPDATAENCODECBOR_H_ */
