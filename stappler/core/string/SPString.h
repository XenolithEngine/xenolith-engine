/**
Copyright (c) 2016-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#ifndef STAPPLER_CORE_STRING_SPSTRING_H_
#define STAPPLER_CORE_STRING_SPSTRING_H_

#include "SPCoreCrypto.h"
#include "SPStringStream.h"

namespace STAPPLER_VERSIONIZED stappler {

inline uint32_t SP_MAKE_API_VERSION(StringView version) {
	uint32_t ver[4];
	uint32_t i = 0;
	version.split<StringView::Chars<'.'>>([&](StringView str) {
		if (i < 4) {
			ver[i++] = uint32_t(str.readInteger(10).get(0));
		}
	});

	uint32_t verCode = 0;
	switch (i) {
	case 0: verCode = SP_MAKE_API_VERSION(0, 0, 1, 0); break;
	case 1: verCode = SP_MAKE_API_VERSION(0, ver[0], 0, 0); break;
	case 2: verCode = SP_MAKE_API_VERSION(0, ver[0], ver[1], 0); break;
	case 3: verCode = SP_MAKE_API_VERSION(0, ver[0], ver[1], ver[2]); break;
	case 4: verCode = SP_MAKE_API_VERSION(ver[0], ver[1], ver[2], ver[3]); break;
	}
	return verCode;
}

template <typename Interface>
inline auto getVersionDescription(uint32_t version) {
	return string::toString<Interface>(version >> 29, ".", version >> 22, ".",
			(version >> 12) & 0b11'1111'1111, ".", version & 0b1111'1111'1111);
}

} // namespace STAPPLER_VERSIONIZED stappler

namespace STAPPLER_VERSIONIZED stappler::string {

SP_PUBLIC char charToKoi8r(char16_t c);

template <typename StringType>
struct InterfaceForString;

template <>
struct InterfaceForString<typename memory::StandartInterface::StringType> {
	using Type = memory::StandartInterface;
};

template <>
struct InterfaceForString<typename memory::StandartInterface::WideStringType> {
	using Type = memory::StandartInterface;
};

template <>
struct InterfaceForString<typename memory::PoolInterface::StringType> {
	using Type = memory::PoolInterface;
};

template <>
struct InterfaceForString<typename memory::PoolInterface::WideStringType> {
	using Type = memory::PoolInterface;
};


template <>
struct InterfaceForString<const typename memory::StandartInterface::StringType> {
	using Type = memory::StandartInterface;
};

template <>
struct InterfaceForString<const typename memory::StandartInterface::WideStringType> {
	using Type = memory::StandartInterface;
};

template <>
struct InterfaceForString<const typename memory::PoolInterface::StringType> {
	using Type = memory::PoolInterface;
};

template <>
struct InterfaceForString<const typename memory::PoolInterface::WideStringType> {
	using Type = memory::PoolInterface;
};

template <typename Interface>
auto toupper(const StringView &str) -> typename Interface::StringType;

template <typename Interface>
auto toupper(const WideStringView &str) -> typename Interface::WideStringType;

template <typename Interface>
auto tolower(const StringView &str) -> typename Interface::StringType;

template <typename Interface>
auto tolower(const WideStringView &str) -> typename Interface::WideStringType;

template <typename Interface>
auto totitle(const StringView &str) -> typename Interface::StringType;

template <typename Interface>
auto totitle(const WideStringView &str) -> typename Interface::WideStringType;

template <typename Interface>
auto urlencode(const StringView &data) -> typename Interface::StringType;

template <typename Storage>
void urldecode(Storage &, const StringView &str);

template <typename Interface>
auto urldecode(const StringView &data) -> typename Interface::StringType;

template <typename Interface>
auto toKoi8r(const WideStringView &data) -> typename Interface::StringType;

template <typename T>
void split(const StringView &str, const StringView &delim, T &&callback);

template <typename Interface>
auto decodeHtml(const StringView &data) -> typename Interface::StringType;

template <typename Container>
void apply(Container &c, const Callback<void(typename Container::value_type &)> &cb) {
	for (auto &it : c) { cb(it); }
}

// fast tolower for C locale
template <typename Container>
void apply_tolower_c(Container &c) {
	stappler::string::apply(c, [](typename Container::value_type &ch) { ch = std::tolower(ch); });
}

// fast toupper for C locale
template <typename Container>
void apply_toupper_c(Container &c) {
	stappler::string::apply(c, [](typename Container::value_type &ch) { ch = std::toupper(ch); });
}

template <typename Interface>
struct StringTraits final {
	using String = typename Interface::StringType;
	using WideString = typename Interface::WideStringType;
	using StringStream = typename Interface::StringStreamType;

	template <typename Value>
	using Vector = typename Interface::template VectorType<Value>;

	template <typename Value>
	using Set = typename Interface::template SetType<Value>;

	template <typename T>
	static void split(const String &str, const String &delim, T &&callback);

	static String urlencode(const StringView &data);
	static String urldecode(const StringView &str);

	static WideString toUtf16(char32_t);
	static WideString toUtf16(const StringView &str);
	static WideString toUtf16Html(const StringView &str);
	static String toUtf8(char32_t c);
	static String toUtf8(char16_t c);
	static String toUtf8(const WideStringView &str);
	static String toUtf8(const StringViewBase<char32_t> &str);

	static String toKoi8r(const WideStringView &str);

	static WideString tolower(const WideStringView &str);
	static WideString toupper(const WideStringView &str);
	static WideString totitle(const WideStringView &str);

	static String tolower(const StringView &str);
	static String toupper(const StringView &str);
	static String totitle(const StringView &str);

	static String decodeHtml(const StringView &);

	static bool isUrlencodeChar(char c);
};

} // namespace stappler::string


namespace STAPPLER_VERSIONIZED stappler::string {

using Sha256 = crypto::Sha256;
using Sha512 = crypto::Sha512;

/* Very simple and quick hasher, do NOT use it in collision-sensative cases */
inline uint32_t hash32(const StringView &key) {
	return hash::hash32(key.data(), uint32_t(key.size()));
}
inline uint64_t hash64(const StringView &key) { return hash::hash64(key.data(), key.size()); }

/* default stdlib hash 32/64-bit, platform depended, unsigned variant (do NOT use for storage) */
template <typename StringType>
inline uint64_t stdlibHashUnsigned(const StringType &key) {
	std::hash<StringType> hasher;
	return hasher(key);
}

/* default stdlib hash 32/64-bit, platform depended, signed variant, can be used for storage */
template <typename StringType>
inline int64_t stdlibHashSigned(const StringType &key) {
	return reinterpretValue<int64_t>(stdlibHashUnsigned(key));
}

} // namespace stappler::string


namespace STAPPLER_VERSIONIZED stappler::base16 {

SP_PUBLIC const char *charToHex(const char &c, bool upper = false);
SP_PUBLIC uint8_t hexToChar(const char &c);
SP_PUBLIC uint8_t hexToChar(const char &c, const char &d);

SP_PUBLIC size_t encodeSize(size_t);
SP_PUBLIC size_t decodeSize(size_t);

template <typename Interface>
SP_PUBLIC auto encode(const CoderSource &source, bool upper = false) ->
		typename Interface::StringType;

SP_PUBLIC void encode(std::basic_ostream<char> &stream, const CoderSource &source,
		bool upper = false);
SP_PUBLIC void encode(const Callback<void(char)> &cb, const CoderSource &source,
		bool upper = false);
SP_PUBLIC size_t encode(char *, size_t bsize, const CoderSource &source, bool upper = false);

template <typename Interface>
SP_PUBLIC auto decode(const CoderSource &source) -> typename Interface::BytesType;

SP_PUBLIC void decode(std::basic_ostream<char> &stream, const CoderSource &source);
SP_PUBLIC void decode(const Callback<void(uint8_t)> &cb, const CoderSource &source);
SP_PUBLIC size_t decode(uint8_t *, size_t bsize, const CoderSource &source);

} // namespace stappler::base16


namespace STAPPLER_VERSIONIZED stappler::base64 {

SP_PUBLIC size_t encodeSize(size_t);
SP_PUBLIC size_t decodeSize(size_t);

template <typename Interface>
SP_PUBLIC auto encode(const CoderSource &source) -> typename Interface::StringType;

SP_PUBLIC void encode(std::basic_ostream<char> &stream, const CoderSource &source);
SP_PUBLIC void encode(const Callback<void(char)> &cb, const CoderSource &source);
SP_PUBLIC size_t encode(char *, size_t bsize, const CoderSource &source);

template <typename Interface>
SP_PUBLIC auto decode(const CoderSource &source) -> typename Interface::BytesType;

SP_PUBLIC void decode(std::basic_ostream<char> &stream, const CoderSource &source);
SP_PUBLIC void decode(const Callback<void(uint8_t)> &cb, const CoderSource &source);
SP_PUBLIC size_t decode(uint8_t *, size_t bsize, const CoderSource &source);

} // namespace stappler::base64


namespace STAPPLER_VERSIONIZED stappler::base64url {

SP_PUBLIC size_t encodeSize(size_t);
SP_PUBLIC size_t decodeSize(size_t);

template <typename Interface>
SP_PUBLIC auto encode(const CoderSource &source) -> typename Interface::StringType;

SP_PUBLIC void encode(std::basic_ostream<char> &stream, const CoderSource &source);
SP_PUBLIC void encode(const Callback<void(char)> &cb, const CoderSource &source);
SP_PUBLIC size_t encode(char *, size_t bsize, const CoderSource &source);


template <typename Interface>
SP_PUBLIC auto decode(const CoderSource &source) -> typename Interface::BytesType;

SP_PUBLIC void decode(std::basic_ostream<char> &stream, const CoderSource &source);
SP_PUBLIC void decode(const Callback<void(uint8_t)> &cb, const CoderSource &source);
SP_PUBLIC size_t decode(uint8_t *, size_t bsize, const CoderSource &source);

} // namespace stappler::base64url


namespace STAPPLER_VERSIONIZED stappler {

template <typename Container, typename StreamType>
inline void toStringStreamConcat(StreamType &stream, const Container &c) {
	for (auto &it : c) { stream << it; }
}

template <typename Container, typename Sep, typename StreamType>
inline void toStringStreamConcat(StreamType &stream, const Container &c, const Sep &s) {
	bool b = false;
	for (auto &it : c) {
		if (b) {
			stream << s;
		} else {
			b = true;
		}
		stream << it;
	}
}

template <typename Container, typename StringType>
inline auto toStringConcat(const Container &c) -> StringType {
	typename traits::SelectStringStream<StringType>::Type stream;
	toStringStreamConcat(stream, c);
	return stream.str();
}

template <typename Container, typename Sep, typename StringType>
inline auto toStringConcat(const Container &c, const Sep &s) -> StringType {
	typename traits::SelectStringStream<StringType>::Type stream;
	toStringStreamConcat(stream, c, s);
	return stream.str();
}

} // namespace STAPPLER_VERSIONIZED stappler


namespace STAPPLER_VERSIONIZED stappler::unicode { } // namespace stappler::unicode


namespace STAPPLER_VERSIONIZED stappler::string {

template <typename Interface>
auto toupper(const StringView &str) -> typename Interface::StringType {
	return StringTraits<Interface>::toupper(str);
}

template <typename Interface>
auto toupper(const WideStringView &str) -> typename Interface::WideStringType {
	return StringTraits<Interface>::toupper(str);
}

template <typename Interface>
auto tolower(const StringView &str) -> typename Interface::StringType {
	return StringTraits<Interface>::tolower(str);
}

template <typename Interface>
auto tolower(const WideStringView &str) -> typename Interface::WideStringType {
	return StringTraits<Interface>::tolower(str);
}

template <typename Interface>
auto totitle(const StringView &str) -> typename Interface::StringType {
	return StringTraits<Interface>::totitle(str);
}

template <typename Interface>
auto totitle(const WideStringView &str) -> typename Interface::WideStringType {
	return StringTraits<Interface>::totitle(str);
}

template <typename Interface>
inline auto urlencode(const StringView &data) -> typename Interface::StringType {
	return StringTraits<Interface>::urlencode(data);
}

template <typename Interface>
inline auto urldecode(const StringView &data) -> typename Interface::StringType {
	return StringTraits<Interface>::urldecode(data);
}

template <typename Interface>
inline auto toUtf16(const StringView &data) -> typename Interface::WideStringType {
	return StringTraits<Interface>::toUtf16(data);
}

template <typename Interface>
inline auto toUtf16(char32_t ch) -> typename Interface::WideStringType {
	return StringTraits<Interface>::toUtf16(ch);
}

template <typename Interface>
inline auto toUtf16Html(const StringView &data) -> typename Interface::WideStringType {
	return StringTraits<Interface>::toUtf16Html(data);
}

template <typename Interface>
inline auto toUtf8(const WideStringView &data) -> typename Interface::StringType {
	return StringTraits<Interface>::toUtf8(data);
}

template <typename Interface>
inline auto toUtf8(const StringViewBase<char32_t> &data) -> typename Interface::StringType {
	return StringTraits<Interface>::toUtf8(data);
}

template <typename Interface>
inline auto toUtf8(const wchar_t *buf, size_t size) -> typename Interface::StringType {
	if constexpr (sizeof(wchar_t) == 2) {
		return StringTraits<Interface>::toUtf8(WideStringView((const char16_t *)buf, size));
	} else if constexpr (sizeof(wchar_t) == 1) {
		// assume already unicode
		return StringView((const char *)buf, size).str<Interface>();
	} else if constexpr (sizeof(wchar_t) == 4) {
		return StringTraits<Interface>::toUtf8(
				StringViewBase<char32_t>((const char32_t *)buf, size));
	} else {
		static_assert(false, "Unknown type for wchar_t");
	}
}

template <typename Interface>
inline auto toUtf8(char16_t c) -> typename Interface::StringType {
	return StringTraits<Interface>::toUtf8(c);
}

template <typename Interface>
inline auto toUtf8(char32_t c) -> typename Interface::StringType {
	return StringTraits<Interface>::toUtf8(c);
}

template <typename Interface>
inline auto toKoi8r(const WideStringView &data) -> typename Interface::StringType {
	return StringTraits<Interface>::toKoi8r(data);
}

template <typename Interface>
inline auto decodeHtml(const StringView &data) -> typename Interface::StringType {
	return StringTraits<Interface>::decodeHtml(data);
}

template <typename T>
inline void split(const StringView &str, const StringView &delim, T &&callback) {
	StringView r(str);
	while (!r.empty()) {
		auto w = r.readUntilString(delim);
		if (r.is(delim)) {
			r += delim.size();
		}
		if (!w.empty()) {
			callback(w);
		}
	}
}

template <typename Interface>
template <typename T>
void StringTraits<Interface>::split(const String &str, const String &delim, T &&callback) {
	size_t start = 0;
	size_t pos = 0;
	for (pos = str.find(delim, start); pos != String::npos;
			start = pos + delim.length(), pos = str.find(delim, start)) {
		if (pos != start) {
			callback(CharReaderBase(str.data() + start, pos - start));
		}
	}
	if (start < str.length()) {
		callback(CharReaderBase(str.data() + start, str.size() - start));
	}
}

template <typename Interface>
auto StringTraits<Interface>::urlencode(const StringView &data) -> String {
	String ret;
	ret.reserve(data.size() * 2);
	for (auto &c : data) {
		if (isUrlencodeChar(c)) {
			ret.push_back('%');
			ret.append(base16::charToHex(c, true), 2);
		} else {
			ret.push_back(c);
		}
	}
	return ret;
}

template <typename Storage>
inline void urldecode(Storage &storage, const StringView &str) {
	using Value = typename Storage::value_type *;

	storage.reserve(str.size());

	StringView r(str);
	while (!r.empty()) {
		StringView tmp = r.readUntil<StringView::Chars<'%'>>();
		storage.insert(storage.end(), Value(tmp.data()), Value(tmp.data() + tmp.size()));

		if (r.is('%') && r > 2) {
			StringView hex(r.data() + 1, 2);
			hex.skipChars<StringView::CharGroup<CharGroupId::Hexadecimial>>();
			if (hex.empty()) {
				storage.push_back(base16::hexToChar(r[1], r[2]));
			} else {
				storage.insert(storage.end(), Value(r.data()), Value(r.data() + 3));
			}
			r += 3;
		} else if (!r.empty()) {
			storage.insert(storage.end(), Value(r.data()), Value(r.data() + r.size()));
			r.clear();
		}
	}
}

template <typename Interface>
auto StringTraits<Interface>::urldecode(const StringView &str) -> String {
	String ret;
	string::urldecode(ret, str);
	return ret;
}

template <typename Interface>
auto StringTraits<Interface>::toUtf16(char32_t ch) -> WideString {
	const auto size = sprt::unicode::getUtf16Length(ch);
	WideString utf16_str;
	utf16_str.reserve(size);

	unicode::utf16Encode(utf16_str, ch);

	return utf16_str;
}

template <typename Interface>
auto StringTraits<Interface>::toUtf16(const StringView &utf8_str) -> WideString {
	const auto size = sprt::unicode::getUtf16Length(utf8_str);
	WideString utf16_str;
	utf16_str.reserve(size);

	uint8_t offset = 0;
	auto ptr = utf8_str.data();
	auto len = utf8_str.size();
	auto end = ptr + utf8_str.size();
	while (ptr < end) {
		auto c = sprt::unicode::utf8Decode32(ptr, len, offset);
		unicode::utf16Encode(utf16_str, c);
		ptr += offset;
		len -= offset;
	}

	return utf16_str;
}

template <typename Interface>
auto StringTraits<Interface>::toUtf16Html(const StringView &utf8_str) -> WideString {
	const auto size = sprt::unicode::getUtf16HtmlLength(utf8_str);
	WideString utf16_str;
	utf16_str.reserve(size);

	uint8_t offset = 0;
	auto ptr = utf8_str.data();
	auto len = utf8_str.size();
	auto end = ptr + utf8_str.size();
	while (ptr < end) {
		auto c = sprt::unicode::utf8HtmlDecode32(ptr, len, offset);
		unicode::utf16Encode(utf16_str, c);
		ptr += offset;
		len -= offset;
	}

	return utf16_str;
}

template <typename Interface>
auto StringTraits<Interface>::toUtf8(const WideStringView &str) -> String {
	const auto size = sprt::unicode::getUtf8Length(str);
	String ret;
	ret.reserve(size);

	uint8_t offset;
	auto ptr = str.data();
	auto len = str.size();
	auto end = ptr + str.size();
	while (ptr < end) {
		auto c = sprt::unicode::utf16Decode32(ptr, len, offset);
		unicode::utf8Encode(ret, c);
		ptr += offset;
		len -= offset;
	}

	return ret;
}

template <typename Interface>
auto StringTraits<Interface>::toUtf8(const StringViewBase<char32_t> &str) -> String {
	const auto size = sprt::unicode::getUtf8Length(str);
	String ret;
	ret.reserve(size);

	for (auto &c : str) { unicode::utf8Encode(ret, c); }

	return ret;
}

template <typename Interface>
auto StringTraits<Interface>::toUtf8(char16_t c) -> String {
	String ret;
	ret.reserve(sprt::unicode::utf8EncodeLength(c));
	unicode::utf8Encode(ret, c);
	return ret;
}

template <typename Interface>
auto StringTraits<Interface>::toUtf8(char32_t c) -> String {
	String ret;
	ret.reserve(sprt::unicode::utf8EncodeLength(c));
	unicode::utf8Encode(ret, c);
	return ret;
}

template <typename Interface>
auto StringTraits<Interface>::toKoi8r(const WideStringView &str) -> String {
	String ret;
	ret.reserve(str.size());
	auto ptr = str.data();
	auto end = ptr + str.size();
	while (ptr < end) { ret.push_back(charToKoi8r(*ptr++)); }
	return ret;
}

template <typename Interface>
auto StringTraits<Interface>::tolower(const WideStringView &str) -> WideString {
	return platform::tolower<Interface>(str);
}

template <typename Interface>
auto StringTraits<Interface>::toupper(const WideStringView &str) -> WideString {
	return platform::toupper<Interface>(str);
}

template <typename Interface>
auto StringTraits<Interface>::totitle(const WideStringView &str) -> WideString {
	return platform::totitle<Interface>(str);
}

template <typename Interface>
auto StringTraits<Interface>::tolower(const StringView &str) -> String {
	return platform::tolower<Interface>(str);
}

template <typename Interface>
auto StringTraits<Interface>::toupper(const StringView &str) -> String {
	return platform::toupper<Interface>(str);
}

template <typename Interface>
auto StringTraits<Interface>::totitle(const StringView &str) -> String {
	return platform::totitle<Interface>(str);
}

template <typename Interface>
auto StringTraits<Interface>::decodeHtml(const StringView &utf8_str) -> String {
	const auto size = sprt::unicode::getUtf8HtmlLength(utf8_str);
	String result_str;
	result_str.reserve(size);

	uint8_t offset = 0;
	auto ptr = utf8_str.data();
	auto len = utf8_str.size();
	auto end = ptr + utf8_str.size();
	while (ptr < end) {
		if (*ptr == '&') {
			auto c = sprt::unicode::utf8HtmlDecode32(ptr, len, offset);
			unicode::utf8Encode(result_str, c);
			ptr += offset;
			len -= offset;
		} else {
			result_str.emplace_back(*ptr);
			++ptr;
			--len;
		}
	}

	return result_str;
}

template <typename Interface>
bool StringTraits<Interface>::isUrlencodeChar(char c) {
	if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') || c == '-'
			|| c == '_' || c == '~' || c == '.') {
		return false;
	} else {
		return true;
	}
}

} // namespace stappler::string

namespace STAPPLER_VERSIONIZED stappler::base64 {

SP_PUBLIC auto __encode_pool(const CoderSource &source) ->
		typename memory::PoolInterface::StringType;
SP_PUBLIC auto __encode_std(const CoderSource &source) ->
		typename memory::StandartInterface::StringType;

SP_PUBLIC auto __decode_pool(const CoderSource &source) ->
		typename memory::PoolInterface::BytesType;
SP_PUBLIC auto __decode_std(const CoderSource &source) ->
		typename memory::StandartInterface::BytesType;

template <>
inline auto decode<memory::PoolInterface>(const CoderSource &source) ->
		typename memory::PoolInterface::BytesType {
	return __decode_pool(source);
}

template <>
inline auto decode<memory::StandartInterface>(const CoderSource &source) ->
		typename memory::StandartInterface::BytesType {
	return __decode_std(source);
}

} // namespace stappler::base64

namespace STAPPLER_VERSIONIZED stappler::base64url {

inline size_t encodeSize(size_t l) { return base64::encodeSize(l); }
inline size_t decodeSize(size_t l) { return base64::decodeSize(l); }

SP_PUBLIC auto __encode_pool(const CoderSource &source) ->
		typename memory::PoolInterface::StringType;
SP_PUBLIC auto __encode_std(const CoderSource &source) ->
		typename memory::StandartInterface::StringType;

template <typename Interface>
SP_PUBLIC inline auto decode(const CoderSource &source) -> typename Interface::BytesType {
	return base64::decode<Interface>(source);
}

inline void decode(std::basic_ostream<char> &stream, const CoderSource &source) {
	base64::decode(stream, source);
}

inline void decode(const Callback<void(uint8_t)> &cb, const CoderSource &source) {
	base64::decode(cb, source);
}

inline size_t decode(uint8_t *buf, size_t bsize, const CoderSource &source) {
	return base64::decode(buf, bsize, source);
	;
}

} // namespace stappler::base64url


namespace STAPPLER_VERSIONIZED stappler::mem_pool {

using String = stappler::memory::string;
using WideString = stappler::memory::u16string;
using StringStream = stappler::memory::ostringstream;
using Interface = stappler::memory::PoolInterface;

template <typename... Args>
inline String toString(Args &&...args) {
	return string::toString<Interface>(std::forward<Args>(args)...);
}

} // namespace stappler::mem_pool

namespace STAPPLER_VERSIONIZED stappler::mem_std {

using String = std::string;
using WideString = std::u16string;
using StringStream = std::stringstream;
using Interface = stappler::memory::StandartInterface;

template <typename... Args>
inline String toString(Args &&...args) {
	return string::toString<Interface>(std::forward<Args>(args)...);
}

} // namespace stappler::mem_std

#endif /* STAPPLER_CORE_STRING_SPSTRING_H_ */
