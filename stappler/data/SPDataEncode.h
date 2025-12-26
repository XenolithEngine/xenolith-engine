/**
Copyright (c) 2017-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>

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

#ifndef STAPPLER_DATA_SPDATAENCODE_H_
#define STAPPLER_DATA_SPDATAENCODE_H_

#include "SPDataEncodeCbor.h"
#include "SPDataEncodeJson.h"
#include "SPDataEncodeSerenity.h"
#include "SPFilepath.h"

#ifdef MODULE_STAPPLER_FILESYSTEM
#include "SPFilesystem.h"
#endif

namespace STAPPLER_VERSIONIZED stappler::data {

struct SP_PUBLIC EncodeFormat {
	static int EncodeStreamIndex;

	enum Format {
		Json				= 0b0000, // Raw JSON data, with no whitespace
		Pretty				= 0b0001, // Pretty-printed JSON data
		Cbor				= 0b0010, // CBOR data (http://cbor.io/, http://tools.ietf.org/html/rfc7049)
		DefaultFormat		= 0b0011,
		Serenity			= 0b0100,
		SerenityPretty		= 0b0101,
		PrettyTime			= 0b1001, // Pretty-printed JSON data (with time markers comment)
	};

	// We use LZ4 for compression, it's very fast to decode
	enum Compression {
		NoCompression			= 0b0000 << 4,
		LZ4Compression			= 0b0001 << 4,
		LZ4HCCompression		= 0b0011 << 4,

#ifdef MODULE_STAPPLER_BROTLI_LIB
		Brotli					= 0b0100 << 4,
#endif

		DefaultCompress = NoCompression
	};

	enum Encryption {
		Unencrypted			= 0b0000 << 8,
		Encrypted			= 0b0001 << 8
	};

	static EncodeFormat CborCompressed;
	static EncodeFormat JsonCompressed;

	constexpr EncodeFormat(Format fmt = DefaultFormat, Compression cmp = DefaultCompress, Encryption enc = Unencrypted, StringView = StringView())
	: format(fmt), compression(cmp), encryption(enc) { }

	constexpr explicit EncodeFormat(int flag)
	: format((Format)(flag & 0x0F)), compression((Compression)(flag & 0xF0))
	, encryption((Encryption)(flag &0xF00)) { }

	EncodeFormat(const EncodeFormat & other) : format(other.format), compression(other.compression)
	, encryption(other.encryption) { }

	EncodeFormat & operator=(const EncodeFormat & other) {
		format = other.format;
		compression = other.compression;
		encryption = other.encryption;
		return *this;
	}

	bool isRaw() const {
		return compression == NoCompression && encryption == Unencrypted;
	}

	bool isTextual() const {
		return isRaw() && (format == Json || format == Pretty);
	}

	int flag() const {
		return (int)format | (int)compression | (int)encryption;
	}

	Format format;
	Compression compression;
	Encryption encryption;
};

SP_PUBLIC uint8_t *getLZ4EncodeState();
SP_PUBLIC size_t compressData(const uint8_t *src, size_t srcSize, uint8_t *dest, size_t destSize, EncodeFormat::Compression c);
SP_PUBLIC void writeCompressionMark(uint8_t *data, size_t sourceSize, EncodeFormat::Compression c, uint8_t padding = 0);

template <typename Interface>
SP_PUBLIC auto compress(const uint8_t *, size_t, EncodeFormat::Compression, bool conditional) -> typename Interface::BytesType;

template <typename Interface>
SP_PUBLIC auto compress(BytesView, EncodeFormat::Compression, bool conditional) -> typename Interface::BytesType;

SP_PUBLIC size_t getCompressBounds(size_t, EncodeFormat::Compression);

template <typename Interface>
struct EncodeTraits {
	using InterfaceType = Interface;
	using ValueType = ValueTemplate<Interface>;
	using BytesType = typename ValueType::BytesType;
	using StringType = typename ValueType::StringType;

	static BytesType write(const ValueType &data, EncodeFormat fmt, size_t reserve = 0) {
		BytesType ret;
		switch (fmt.format) {
		case EncodeFormat::Json:
		case EncodeFormat::Pretty:
		case EncodeFormat::PrettyTime: {
			StringType s = json::write(data, (fmt.format == EncodeFormat::Pretty), (fmt.format == EncodeFormat::PrettyTime));
			ret.reserve(s.length());
			ret.assign(s.begin(), s.end());
			break;
		}
		case EncodeFormat::Cbor:
		case EncodeFormat::DefaultFormat:
			ret = cbor::write(data, reserve);
			break;

		case EncodeFormat::Serenity:
		case EncodeFormat::SerenityPretty: {
			StringType s = serenity::write(data, (fmt.format == EncodeFormat::SerenityPretty));
			ret.reserve(s.length());
			ret.assign(s.begin(), s.end());
			break;
		}
		}

		if (fmt.compression != EncodeFormat::NoCompression) {
			auto tmp = compress<Interface>(ret.data(), ret.size(), fmt.compression, true);
			if (!tmp.empty()) {
				return tmp;
			}
		}
		return ret;
	}

	static bool write(const Callback<void(StringView)> &stream, const ValueType &data, EncodeFormat fmt) {
		if (fmt.isRaw()) {
			switch (fmt.format) {
			case EncodeFormat::Json: json::write(stream, data, false); return true; break;
			case EncodeFormat::Pretty: json::write(stream, data, true); return true; break;
			case EncodeFormat::PrettyTime: json::write(stream, data, true, true); return true; break;
			case EncodeFormat::Cbor:
			case EncodeFormat::DefaultFormat:
				return cbor::write([&] (BytesView bytes) {
					StringView str; str.set((const char *)bytes.data(), bytes.size());
					stream(str);
				}, data);
				break;
			case EncodeFormat::Serenity: serenity::write(stream, data, false); return true; break;
			case EncodeFormat::SerenityPretty: serenity::write(stream, data, true); return true; break;
			}
		} else {
			auto ret = write(data, fmt);
			if (!ret.empty()) {
				StringView str; str.set((const char *)ret.data(), ret.size());
				stream << str;
				return true;
			}
			return false;
		}
		return false;
	}

#ifdef MODULE_STAPPLER_FILESYSTEM
	static bool save(const ValueType &data, const FileInfo &info, EncodeFormat fmt) {
		if (fmt.format == EncodeFormat::DefaultFormat) {
			auto ext = filepath::lastExtension(info.path);
			if (ext == "json") {
				fmt.format = EncodeFormat::Json;
			} else {
				fmt.format = EncodeFormat::Cbor;
			}
		}
		if (fmt.isRaw()) {
			switch (fmt.format) {
			case EncodeFormat::Json: return json::save(data, info, false); break;
			case EncodeFormat::Pretty: return json::save(data, info, true); break;
			case EncodeFormat::PrettyTime: return json::save(data, info, true, true); break;
			case EncodeFormat::Cbor:
			case EncodeFormat::DefaultFormat:
				return cbor::save(data, info);
				break;
			case EncodeFormat::Serenity: return serenity::save(data, info, false); break;
			case EncodeFormat::SerenityPretty: return serenity::save(data, info, true); break;
			}
		} else {
			auto ret = write(data, fmt);
			if (!ret.empty()) {
				filesystem::write(info, ret);
				return true;
			}
			return false;
		}
		return false;
	}
#endif
};

template <typename Interface> inline auto
write(const ValueTemplate<Interface> &data, EncodeFormat fmt = EncodeFormat(), size_t reserve = 0) -> typename ValueTemplate<Interface>::BytesType {
	return EncodeTraits<Interface>::write(data, fmt, reserve);
}

template <typename Interface> inline bool
write(const Callback<void(StringView)> &stream, const ValueTemplate<Interface> &data, EncodeFormat fmt = EncodeFormat()) {
	return EncodeTraits<Interface>::write(stream, data, fmt);
}

template <typename Interface> inline bool
save(const ValueTemplate<Interface> &data, const FileInfo &file, EncodeFormat fmt = EncodeFormat()) {
	return EncodeTraits<Interface>::save(data, file, fmt);
}

template <typename Interface> inline auto
toString(const ValueTemplate<Interface> &data, bool pretty = false) -> typename ValueTemplate<Interface>::StringType  {
	return json::write<Interface>(data, pretty);
}
template <typename Interface> inline auto
toString(const ValueTemplate<Interface> &data, EncodeFormat::Format fmt) -> typename ValueTemplate<Interface>::StringType  {
	switch (fmt) {
	case EncodeFormat::Json:
		return json::write<Interface>(data, false);
		break;
	case EncodeFormat::Pretty:
		return json::write<Interface>(data, true);
		break;
	case EncodeFormat::PrettyTime:
		return json::write<Interface>(data, true, true);
		break;
	case EncodeFormat::Cbor:
		return base64::encode<Interface>(cbor::write(data));
		break;
	case EncodeFormat::DefaultFormat:
		return json::write<Interface>(data, false);
		break;
	case EncodeFormat::Serenity:
		return serenity::write<Interface>(data, false);
		break;
	case EncodeFormat::SerenityPretty:
		return serenity::write<Interface>(data, true);
		break;
	}
	return typename ValueTemplate<Interface>::StringType();
}

template<typename CharT, typename Traits> inline std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits> & stream, EncodeFormat f) {
	stream.iword( EncodeFormat::EncodeStreamIndex ) = f.flag();
	return stream;
}

template<typename CharT, typename Traits> inline std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits> & stream, EncodeFormat::Format f) {
	EncodeFormat fmt(f);
	stream << fmt;
	return stream;
}

template<typename CharT, typename Traits, typename Interface> inline std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits> & stream, const ValueTemplate<Interface> &val) {
	EncodeFormat fmt(int(stream.iword( EncodeFormat::EncodeStreamIndex )));
	write<Interface>([&] (StringViewBase<CharT> str) {
		stream << str;
	}, val, fmt);
	return stream;
}

}

#endif /* STAPPLER_DATA_SPDATAENCODE_H_ */
