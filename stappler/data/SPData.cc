/**
Copyright (c) 2016-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#include "SPData.h"
#include "SPMemInterface.h"
#include "SPString.h"
#include "SPStringView.h"

#ifdef MODULE_STAPPLER_FILESYSTEM
#include "SPFilesystem.h"
#endif

#include <sprt/runtime/compress.h>

#ifdef MODULE_STAPPLER_BROTLI_LIB
#include "brotli/encode.h"
#include "brotli/decode.h"
#else
#warning Module 'stappler_brotli_lib' is not enabled, data::Value built without Brotli compression support
#endif

namespace STAPPLER_VERSIONIZED stappler::data {

EncodeFormat EncodeFormat::CborCompressed(EncodeFormat::Cbor, EncodeFormat::LZ4HCCompression);
EncodeFormat EncodeFormat::JsonCompressed(EncodeFormat::Json, EncodeFormat::LZ4HCCompression);

int EncodeFormat::EncodeStreamIndex = std::ios_base::xalloc();

namespace serenity {

// clang-format off
bool shouldEncodePercent(char c) {
#define V16 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
	static uint8_t s_decTable[256] = {
		V16, V16, // 0-1, 0-F
		1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 0, // 2, 0-F
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, // 3, 0-F
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 4, 0-F
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, // 5, 0-F
		1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 6, 0-F
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, // 7, 0-F
		V16, V16, V16, V16, V16, V16, V16, V16,
	};

	return bool(s_decTable[*((uint8_t *)(&c))]);
}
// clang-format on

} // namespace serenity

template <>
template <>
auto ValueTemplate<memory::PoolInterface>::convert<memory::PoolInterface>() const
		-> ValueTemplate<memory::PoolInterface> {
	return ValueTemplate<memory::PoolInterface>(*this);
}

template <>
template <>
auto ValueTemplate<memory::StandartInterface>::convert<memory::StandartInterface>() const
		-> ValueTemplate<memory::StandartInterface> {
	return ValueTemplate<memory::StandartInterface>(*this);
}

template <>
template <>
auto ValueTemplate<memory::PoolInterface>::convert<memory::StandartInterface>() const
		-> ValueTemplate<memory::StandartInterface> {
	switch (_type) {
	case Type::INTEGER: return ValueTemplate<memory::StandartInterface>(intVal); break;
	case Type::DOUBLE: return ValueTemplate<memory::StandartInterface>(doubleVal); break;
	case Type::BOOLEAN: return ValueTemplate<memory::StandartInterface>(boolVal); break;
	case Type::CHARSTRING:
		return ValueTemplate<memory::StandartInterface>(
				memory::StandartInterface::StringType(strVal->data(), strVal->size()));
		break;
	case Type::BYTESTRING:
		return ValueTemplate<memory::StandartInterface>(memory::StandartInterface::BytesType(
				bytesVal->data(), bytesVal->data() + bytesVal->size()));
		break;
	case Type::ARRAY: {
		ValueTemplate<memory::StandartInterface> ret(
				ValueTemplate<memory::StandartInterface>::Type::ARRAY);
		auto &arr = ret.asArray();
		arr.reserve(arrayVal->size());
		for (auto &it : *arrayVal) { arr.emplace_back(it.convert<memory::StandartInterface>()); }
		return ret;
		break;
	}
	case Type::DICTIONARY: {
		ValueTemplate<memory::StandartInterface> ret(
				ValueTemplate<memory::StandartInterface>::Type::DICTIONARY);
		auto &dict = ret.asDict();
		for (auto &it : *dictVal) {
			dict.emplace(StringView(it.first).str<memory::StandartInterface>(),
					it.second.convert<memory::StandartInterface>());
		}
		return ret;
		break;
	}
	default: break;
	}
	return ValueTemplate<memory::StandartInterface>();
}

template <>
template <>
auto ValueTemplate<memory::StandartInterface>::convert<memory::PoolInterface>() const
		-> ValueTemplate<memory::PoolInterface> {
	switch (_type) {
	case Type::INTEGER: return ValueTemplate<memory::PoolInterface>(intVal); break;
	case Type::DOUBLE: return ValueTemplate<memory::PoolInterface>(doubleVal); break;
	case Type::BOOLEAN: return ValueTemplate<memory::PoolInterface>(boolVal); break;
	case Type::CHARSTRING:
		return ValueTemplate<memory::PoolInterface>(
				memory::PoolInterface::StringType(strVal->data(), strVal->size()));
		break;
	case Type::BYTESTRING:
		return ValueTemplate<memory::PoolInterface>(memory::PoolInterface::BytesType(
				bytesVal->data(), bytesVal->data() + bytesVal->size()));
		break;
	case Type::ARRAY: {
		ValueTemplate<memory::PoolInterface> ret(ValueTemplate<memory::PoolInterface>::Type::ARRAY);
		auto &arr = ret.asArray();
		arr.reserve(arrayVal->size());
		for (auto &it : *arrayVal) { arr.emplace_back(it.convert<memory::PoolInterface>()); }
		return ret;
		break;
	}
	case Type::DICTIONARY: {
		ValueTemplate<memory::PoolInterface> ret(
				ValueTemplate<memory::PoolInterface>::Type::DICTIONARY);
		auto &dict = ret.asDict();
		dict.reserve(dictVal->size());
		for (auto &it : *dictVal) {
			dict.emplace(StringView(it.first).str<memory::PoolInterface>(),
					it.second.convert<memory::PoolInterface>());
		}
		return ret;
		break;
	}
	default: break;
	}
	return ValueTemplate<memory::PoolInterface>();
}

size_t getCompressBounds(size_t size, EncodeFormat::Compression c) {
	switch (c) {
	case EncodeFormat::LZ4Compression:
	case EncodeFormat::LZ4HCCompression: {
		return sprt::lz4_getCompressBounds(size) + +((size <= 0xFFFF) ? 2 : 4);
		break;
	}
#ifdef MODULE_STAPPLER_BROTLI_LIB
	case EncodeFormat::Brotli:
		return BrotliEncoderMaxCompressedSize(size) + ((size <= 0xFFFF) ? 2 : 4);
		break;
#endif
	case EncodeFormat::NoCompression: break;
	}
	return 0;
}

thread_local uint8_t tl_compressBuffer[128_KiB];

size_t compressData(const uint8_t *src, size_t srcSize, uint8_t *dest, size_t destSize,
		EncodeFormat::Compression c) {
	switch (c) {
	case EncodeFormat::LZ4Compression: {
		const int offSize = ((srcSize <= 0xFFFF) ? 2 : 4);
		const auto ret = sprt::lz4_compressData(src, srcSize, dest + offSize, destSize - offSize);
		if (ret > 0) {
			if (srcSize <= 0xFFFF) {
				uint16_t sz = srcSize;
				memcpy(dest, &sz, sizeof(sz));
			} else {
				uint32_t sz = uint32_t(srcSize);
				memcpy(dest, &sz, sizeof(sz));
			}
			return ret + offSize;
		}
		break;
	}
	case EncodeFormat::LZ4HCCompression: {
		const int offSize = ((srcSize <= 0xFFFF) ? 2 : 4);
		const auto ret = sprt::lz4hc_compressData(src, srcSize, dest + offSize, destSize - offSize);
		if (ret > 0) {
			if (srcSize <= 0xFFFF) {
				uint16_t sz = srcSize;
				memcpy(dest, &sz, sizeof(sz));
			} else {
				uint32_t sz = uint32_t(srcSize);
				memcpy(dest, &sz, sizeof(sz));
			}
			return ret + offSize;
		}
		break;
	}
#ifdef MODULE_STAPPLER_BROTLI_LIB
	case EncodeFormat::Brotli: {
		const int offSize = ((srcSize <= 0xFFFF) ? 2 : 4);
		size_t ret = destSize - offSize;
		if (BrotliEncoderCompress(10, BROTLI_MAX_WINDOW_BITS, BROTLI_DEFAULT_MODE, srcSize,
					(const uint8_t *)src, &ret, dest + offSize)
				== BROTLI_TRUE) {
			if (srcSize <= 0xFFFF) {
				uint16_t sz = srcSize;
				memcpy(dest, &sz, sizeof(sz));
			} else {
				uint32_t sz = uint32_t(srcSize);
				memcpy(dest, &sz, sizeof(sz));
			}
			return ret + offSize;
		}
		break;
	}
#endif
	case EncodeFormat::NoCompression: break;
	}
	return 0;
}

void writeCompressionMark(uint8_t *data, size_t sourceSize, EncodeFormat::Compression c,
		uint8_t padding) {
	switch (c) {
	case EncodeFormat::LZ4Compression:
	case EncodeFormat::LZ4HCCompression:
		if (sourceSize <= 0xFFFF) {
			switch (padding) {
			case 0: memcpy(data, "LZ4S", 4); break;
			case 1: memcpy(data, "LZ4T", 4); break;
			case 2: memcpy(data, "LZ4U", 4); break;
			case 3: memcpy(data, "LZ4V", 4); break;
			}
		} else {
			switch (padding) {
			case 0: memcpy(data, "LZ4W", 4); break;
			case 1: memcpy(data, "LZ4X", 4); break;
			case 2: memcpy(data, "LZ4Y", 4); break;
			case 3: memcpy(data, "LZ4Z", 4); break;
			}
		}
		break;
#ifdef MODULE_STAPPLER_BROTLI_LIB
	case EncodeFormat::Brotli:
		if (sourceSize <= 0xFFFF) {
			switch (padding) {
			case 0: memcpy(data, "SBrS", 4); break;
			case 1: memcpy(data, "SBrT", 4); break;
			case 2: memcpy(data, "SBrU", 4); break;
			case 3: memcpy(data, "SBrV", 4); break;
			}
		} else {
			switch (padding) {
			case 0: memcpy(data, "SBrW", 4); break;
			case 1: memcpy(data, "SBrX", 4); break;
			case 2: memcpy(data, "SBrY", 4); break;
			case 3: memcpy(data, "SBrZ", 4); break;
			}
		}
		break;
#endif
	case EncodeFormat::NoCompression: break;
	}
}

template <typename Interface>
static inline auto doCompress(const uint8_t *src, size_t size, EncodeFormat::Compression c,
		bool conditional) -> typename Interface::BytesType {
	auto bufferSize = getCompressBounds(size, c);
	if (bufferSize == 0) {
		return typename Interface::BytesType();
	} else if (bufferSize <= sizeof(tl_compressBuffer)) {
		auto encodeSize = compressData(src, size, tl_compressBuffer, sizeof(tl_compressBuffer), c);
		if (encodeSize == 0 || (conditional && encodeSize + 4 > size)) {
			return typename Interface::BytesType();
		}
		auto targetSize = encodeSize + 4;
		auto targetExtra = 4 - (targetSize) % sizeof(uint32_t);
		targetSize += ((targetExtra == 4) ? 0 : targetExtra);
		typename Interface::BytesType ret;
		ret.resize(targetSize);
		writeCompressionMark(ret.data(), size, c, (targetExtra == 4) ? 0 : targetExtra);
		memcpy(ret.data() + 4, tl_compressBuffer, encodeSize);
		return ret;
	} else {
		typename Interface::BytesType ret;
		ret.resize(bufferSize + 4);
		auto encodeSize = compressData(src, size, ret.data() + 4, bufferSize, c);
		if (encodeSize == 0 || (conditional && encodeSize + 4 > size)) {
			return typename Interface::BytesType();
		}
		auto targetSize = encodeSize + 4;
		auto targetExtra = 4 - (targetSize) % sizeof(uint32_t);
		writeCompressionMark(ret.data(), size, c, (targetExtra == 4) ? 0 : targetExtra);
		targetSize += ((targetExtra == 4) ? 0 : targetExtra);
		ret.resize(targetSize);
		ret.shrink_to_fit();
		return ret;
	}
	return typename Interface::BytesType();
}

template <>
auto compress<memory::PoolInterface>(const uint8_t *src, size_t size, EncodeFormat::Compression c,
		bool conditional) -> memory::PoolInterface::BytesType {
	return doCompress<memory::PoolInterface>(src, size, c, conditional);
}

template <>
auto compress<memory::StandartInterface>(const uint8_t *src, size_t size,
		EncodeFormat::Compression c, bool conditional) -> memory::StandartInterface::BytesType {
	return doCompress<memory::StandartInterface>(src, size, c, conditional);
}

template <>
auto compress<memory::PoolInterface>(BytesView src, EncodeFormat::Compression c, bool conditional)
		-> memory::PoolInterface::BytesType {
	return doCompress<memory::PoolInterface>(src.data(), src.size(), c, conditional);
}

template <>
auto compress<memory::StandartInterface>(BytesView src, EncodeFormat::Compression c,
		bool conditional) -> memory::StandartInterface::BytesType {
	return doCompress<memory::StandartInterface>(src.data(), src.size(), c, conditional);
}

using decompress_ptr = const uint8_t *;

template <typename Interface>
static inline auto doDecompressLZ4(BytesView data, bool sh) -> ValueTemplate<Interface> {
	size_t size = sh ? data.readUnsigned16() : data.readUnsigned32();

	ValueTemplate<Interface> ret;
	if (size <= sizeof(tl_compressBuffer)) {
		if (sprt::lz4_decompressData(data.data(), data.size(), tl_compressBuffer, size)) {
			ret = data::read<Interface>(BytesView(tl_compressBuffer, size));
		}
	} else {
		typename Interface::BytesType res;
		res.resize(size);
		if (sprt::lz4_decompressData(data.data(), data.size(), res.data(), size)) {
			ret = data::read<Interface>(res);
		}
	}
	return ret;
}

template <>
auto decompressLZ4(const uint8_t *srcPtr, size_t srcSize, bool sh)
		-> ValueTemplate<memory::PoolInterface> {
	return doDecompressLZ4<memory::PoolInterface>(BytesView(srcPtr, srcSize), sh);
}

template <>
auto decompressLZ4(const uint8_t *srcPtr, size_t srcSize, bool sh)
		-> ValueTemplate<memory::StandartInterface> {
	return doDecompressLZ4<memory::StandartInterface>(BytesView(srcPtr, srcSize), sh);
}

#ifdef MODULE_STAPPLER_BROTLI_LIB
static bool doDecompressBrotliFrame(const uint8_t *src, size_t srcSize, uint8_t *dest,
		size_t destSize) {
	size_t ret = destSize;
	return BrotliDecoderDecompress(srcSize, src, &ret, dest) == BROTLI_DECODER_RESULT_SUCCESS;
}
template <typename Interface>
static inline auto doDecompressBrotli(BytesView data, bool sh) -> ValueTemplate<Interface> {
	size_t size = sh ? data.readUnsigned16() : data.readUnsigned32();

	ValueTemplate<Interface> ret;
	if (size <= sizeof(tl_compressBuffer)) {
		if (doDecompressBrotliFrame(data.data(), data.size(), tl_compressBuffer, size)) {
			ret = data::read<Interface>(BytesView(tl_compressBuffer, size));
		}
	} else {
		typename Interface::BytesType res;
		res.resize(size);
		if (doDecompressBrotliFrame(data.data(), data.size(), res.data(), size)) {
			ret = data::read<Interface>(res);
		}
	}
	return ret;
}

template <>
auto decompressBrotli(const uint8_t *srcPtr, size_t srcSize, bool sh)
		-> ValueTemplate<memory::PoolInterface> {
	return doDecompressBrotli<memory::PoolInterface>(BytesView(srcPtr, srcSize), sh);
}

template <>
auto decompressBrotli(const uint8_t *srcPtr, size_t srcSize, bool sh)
		-> ValueTemplate<memory::StandartInterface> {
	return doDecompressBrotli<memory::StandartInterface>(BytesView(srcPtr, srcSize), sh);
}

#endif

size_t decompress(const uint8_t *d, size_t size, uint8_t *dstData, size_t dstSize) {
	size_t ret = 0;
	BytesView data(d, size);
	uint8_t padding = 0;
	auto ff = detectDataFormat(data.data(), data.size(), padding);
	switch (ff) {
	case DataFormat::LZ4_Short: {
		data += 4;
		ret = data.readUnsigned16();
		if (dstData) {
			if (dstSize >= ret) {
				if (!sprt::lz4_decompressData(data.data(), data.size() - padding, dstData, ret)) {
					ret = 0;
				}
			} else {
				ret = 0;
			}
		}
		break;
	}
	case DataFormat::LZ4_Word: {
		data += 4;
		ret = data.readUnsigned32();
		if (dstData) {
			if (dstSize >= ret) {
				if (!sprt::lz4_decompressData(data.data(), data.size() - padding, dstData, ret)) {
					ret = 0;
				}
			} else {
				ret = 0;
			}
		}
		break;
	}
#ifdef MODULE_STAPPLER_BROTLI_LIB
	case DataFormat::Brotli_Short: {
		data += 4;
		ret = data.readUnsigned16();
		if (dstData) {
			if (dstSize >= ret) {
				if (!doDecompressBrotliFrame(data.data(), data.size() - padding, dstData, ret)) {
					ret = 0;
				}
			} else {
				ret = 0;
			}
		}
		break;
	}
	case DataFormat::Brotli_Word: {
		data += 4;
		ret = data.readUnsigned32();
		if (dstData) {
			if (dstSize >= ret) {
				if (!doDecompressBrotliFrame(data.data(), data.size() - padding, dstData, ret)) {
					ret = 0;
				}
			} else {
				ret = 0;
			}
		}
		break;
	}
#endif
	default: break;
	}
	return ret;
}

size_t getDecompressedSize(const uint8_t *d, size_t size) {
	return decompress(d, size, nullptr, 0);
}

template <>
const typename ValueTemplate<memory::StandartInterface>::StringType
		ValueTemplate<memory::StandartInterface>::StringNull{};

template <>
const typename ValueTemplate<memory::StandartInterface>::BytesType
		ValueTemplate<memory::StandartInterface>::BytesNull{};

template <>
const typename ValueTemplate<memory::StandartInterface>::ArrayType
		ValueTemplate<memory::StandartInterface>::ArrayNull{};

template <>
const typename ValueTemplate<memory::StandartInterface>::DictionaryType
		ValueTemplate<memory::StandartInterface>::DictionaryNull{};


template <>
const typename ValueTemplate<memory::PoolInterface>::StringType
		ValueTemplate<memory::PoolInterface>::StringNull(sprt::memory::get_zero_pool());

template <>
const typename ValueTemplate<memory::PoolInterface>::BytesType
		ValueTemplate<memory::PoolInterface>::BytesNull(sprt::memory::get_zero_pool());

template <>
const typename ValueTemplate<memory::PoolInterface>::ArrayType
		ValueTemplate<memory::PoolInterface>::ArrayNull(sprt::memory::get_zero_pool());

template <>
const typename ValueTemplate<memory::PoolInterface>::DictionaryType
		ValueTemplate<memory::PoolInterface>::DictionaryNull(sprt::memory::get_zero_pool());

template <>
auto ValueTemplate<memory::StandartInterface>::getStringNullConst() -> const StringType & {
	return StringNull;
}

template <>
auto ValueTemplate<memory::StandartInterface>::getBytesNullConst() -> const BytesType & {
	return BytesNull;
}

template <>
auto ValueTemplate<memory::StandartInterface>::getArrayNullConst() -> const ArrayType & {
	return ArrayNull;
}

template <>
auto ValueTemplate<memory::StandartInterface>::getDictionaryNullConst() -> const DictionaryType & {
	return DictionaryNull;
}

template <>
auto ValueTemplate<memory::PoolInterface>::getStringNullConst() -> const StringType & {
	return StringNull;
}

template <>
auto ValueTemplate<memory::PoolInterface>::getBytesNullConst() -> const BytesType & {
	return BytesNull;
}

template <>
auto ValueTemplate<memory::PoolInterface>::getArrayNullConst() -> const ArrayType & {
	return ArrayNull;
}

template <>
auto ValueTemplate<memory::PoolInterface>::getDictionaryNullConst() -> const DictionaryType & {
	return DictionaryNull;
}

template <>
auto ValueTemplate<memory::StandartInterface>::getStringNull() -> StringType & {
	return const_cast<StringType &>(StringNull);
}

template <>
auto ValueTemplate<memory::StandartInterface>::getBytesNull() -> BytesType & {
	return const_cast<BytesType &>(BytesNull);
}

template <>
auto ValueTemplate<memory::StandartInterface>::getArrayNull() -> ArrayType & {
	return const_cast<ArrayType &>(ArrayNull);
}

template <>
auto ValueTemplate<memory::StandartInterface>::getDictionaryNull() -> DictionaryType & {
	return const_cast<DictionaryType &>(DictionaryNull);
}

template <>
auto ValueTemplate<memory::PoolInterface>::getStringNull() -> StringType & {
	return const_cast<StringType &>(StringNull);
}

template <>
auto ValueTemplate<memory::PoolInterface>::getBytesNull() -> BytesType & {
	return const_cast<BytesType &>(BytesNull);
}

template <>
auto ValueTemplate<memory::PoolInterface>::getArrayNull() -> ArrayType & {
	return const_cast<ArrayType &>(ArrayNull);
}

template <>
auto ValueTemplate<memory::PoolInterface>::getDictionaryNull() -> DictionaryType & {
	return const_cast<DictionaryType &>(DictionaryNull);
}

} // namespace stappler::data
