/**
Copyright (c) 2017-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef STAPPLER_DATA_SPDATADECODE_H_
#define STAPPLER_DATA_SPDATADECODE_H_

#include "SPDataDecodeCbor.h"
#include "SPDataDecodeJson.h"
#include "SPDataDecodeSerenity.h"

#ifdef MODULE_STAPPLER_FILESYSTEM
#include "SPFilesystem.h"
#endif

namespace STAPPLER_VERSIONIZED stappler::data {

enum class DataFormat {
	Unknown,
	Json,
	Cbor,
	Serenity,

	CborBase64,

	LZ4_Short,
	LZ4_Word,
#ifdef MODULE_STAPPLER_BROTLI_LIB
	Brotli_Short,
	Brotli_Word,
#endif

	// for future implementations
	// Encrypt,
};

inline DataFormat detectDataFormat(const uint8_t *ptr, size_t size, uint8_t &padding) {
	if (size > 3 && ptr[0] == 0xd9 && ptr[1] == 0xd9 && ptr[2] == 0xf7) {
		return DataFormat::Cbor;
	} else if (size > 4 && ptr[0] == '2' && ptr[1] == 'd' && ptr[2] == 'n' && ptr[3] == '3') {
		return DataFormat::CborBase64;
	} else if (size > 3 && ptr[0] == 'L' && ptr[1] == 'Z' && ptr[2] == '4') {
		switch (ptr[3]) {
		case 'S': padding = 0; return DataFormat::LZ4_Short; break;
		case 'T': padding = 1; return DataFormat::LZ4_Short; break;
		case 'U': padding = 2; return DataFormat::LZ4_Short; break;
		case 'V': padding = 3; return DataFormat::LZ4_Short; break;
		case 'W': padding = 0; return DataFormat::LZ4_Word; break;
		case 'X': padding = 1; return DataFormat::LZ4_Word; break;
		case 'Y': padding = 2; return DataFormat::LZ4_Word; break;
		case 'Z': padding = 3; return DataFormat::LZ4_Word; break;
		}
#ifdef MODULE_STAPPLER_BROTLI_LIB
	} else if (size > 3 && ptr[0] == 'S' && ptr[1] == 'B' && ptr[2] == 'r') {
		switch (ptr[3]) {
		case 'S': padding = 0; return DataFormat::Brotli_Short; break;
		case 'T': padding = 1; return DataFormat::Brotli_Short; break;
		case 'U': padding = 2; return DataFormat::Brotli_Short; break;
		case 'V': padding = 3; return DataFormat::Brotli_Short; break;
		case 'W': padding = 0; return DataFormat::Brotli_Word; break;
		case 'X': padding = 1; return DataFormat::Brotli_Word; break;
		case 'Y': padding = 2; return DataFormat::Brotli_Word; break;
		case 'Z': padding = 3; return DataFormat::Brotli_Word; break;
		}
#endif
	} else if (ptr[0] == '(') {
		return DataFormat::Serenity;
	} else {
		return DataFormat::Json;
	}
	return DataFormat::Unknown;
}

SP_PUBLIC size_t decompress(const uint8_t *srcData, size_t srcSize, uint8_t *dstData, size_t dstSize);

SP_PUBLIC size_t getDecompressedSize(const uint8_t *, size_t);

template <typename Interface>
SP_PUBLIC auto decompressLZ4(const uint8_t *, size_t, bool sh) -> ValueTemplate<Interface>;

template <typename Interface>
SP_PUBLIC auto decompressBrotli(const uint8_t *, size_t, bool sh) -> ValueTemplate<Interface>;

template <typename Interface>
auto decompress(const uint8_t *d, size_t size) -> typename Interface::BytesType {
	if (auto s = decompress(d, size, nullptr, 0)) {
		typename Interface::BytesType res; res.resize(s);
		if (decompress(d, size, res.data(), res.size()) == s) {
			return res;
		}
	}
	return typename Interface::BytesType();
}

template <typename Interface, typename StringType>
auto read(const StringType &data, const StringView &key = StringView()) -> ValueTemplate<Interface> {
	if (data.size() == 0) {
		return ValueTemplate<Interface>();
	}
	uint8_t padding = 0;
	auto ff = detectDataFormat((const uint8_t *)data.data(), data.size(), padding);
	switch (ff) {
	case DataFormat::Cbor:
		return cbor::read<Interface>(data);
		break;
	case DataFormat::Json:
		return json::read<Interface>(StringView((char *)data.data(), data.size()));
		break;
	case DataFormat::Serenity:
		return serenity::read<Interface>(StringView((char *)data.data(), data.size()));
		break;
	case DataFormat::CborBase64:
		return read<Interface>(base64::decode<Interface>(CoderSource(data)), key);
		break;
	case DataFormat::LZ4_Short:
		return decompressLZ4<Interface>((const uint8_t *)data.data() + 4, data.size() - 4 - padding, true);
		break;
	case DataFormat::LZ4_Word:
		return decompressLZ4<Interface>((const uint8_t *)data.data() + 4, data.size() - 4 - padding, false);
		break;
#ifdef MODULE_STAPPLER_BROTLI_LIB
	case DataFormat::Brotli_Short:
		return decompressBrotli<Interface>((const uint8_t *)data.data() + 4, data.size() - 4 - padding, true);
		break;
	case DataFormat::Brotli_Word:
		return decompressBrotli<Interface>((const uint8_t *)data.data() + 4, data.size() - 4 - padding, false);
		break;
#endif
	default:
		break;
	}
	return ValueTemplate<Interface>();
}

#ifdef MODULE_STAPPLER_FILESYSTEM
template <typename Interface>
auto readFile(const FileInfo &filename, const StringView &key = StringView()) -> ValueTemplate<Interface> {
	return read<Interface>(filesystem::readIntoMemory<Interface>(filename));
}
#endif
}

#endif /* STAPPLER_DATA_SPDATADECODE_H_ */
