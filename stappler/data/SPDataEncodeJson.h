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

#ifndef STAPPLER_DATA_SPDATAENCODEJSON_H_
#define STAPPLER_DATA_SPDATAENCODEJSON_H_

#include "SPDataValue.h"

#include <sprt/runtime/utils/base64.h>

#if MODULE_STAPPLER_FILESYSTEM
#include "SPFilesystem.h"
#endif

namespace STAPPLER_VERSIONIZED stappler::data::json {

template <typename StringType>
inline void encodeString(const Callback<void(StringView)> &stream, const StringType &str) {
	stream << '"';
	for (auto &i : str) {
		switch (i) {
		case '\n': stream << "\\n"; break;
		case '\r': stream << "\\r"; break;
		case '\t': stream << "\\t"; break;
		case '\f': stream << "\\f"; break;
		case '\b': stream << "\\b"; break;
		case '\\': stream << "\\\\"; break;
		case '\"': stream << "\\\""; break;
		case ' ': stream << ' '; break;
		default:
			if (i >= 0 && i <= 0x20) {
				stream << "\\u00" << base16::charToHex(i, true);
			} else {
				stream << i;
			}
			break;
		}
	}
	stream << '"';
}

inline void encodeDouble(const Callback<void(StringView)> &stream, double value) {
	if (sprt::isnan(value)) {
		stream << "NaN";
	} else if (value == sprt::Infinity<double>) {
		stream << "Infinity";
	} else if (value == -sprt::Infinity<double>) {
		stream << "-Infinity";
	} else {
		stream << value;
	}
}

template <typename Interface>
struct RawEncoder : public Interface::AllocBaseType {
	using InterfaceType = Interface;
	using ValueType = ValueTemplate<Interface>;

	inline RawEncoder(const Callback<void(StringView)> *s) : stream(s) { }

	inline void writeData(const char *data, size_t size) { (*stream) << StringView(data, size); }

	inline void writeData(const char *data) { writeData(data, sprt::strlen(data)); }

	inline void writeChar(char c) { (*stream) << c; }

	inline void write(nullptr_t) { writeData("null", "null"_len); }
	inline void write(bool value) { writeData((value) ? "true" : "false"); }
	inline void write(int64_t value) { (*stream) << value; }
	inline void write(double value) { encodeDouble(*stream, value); }

	inline void write(const typename ValueType::StringType &str) { encodeString(*stream, str); }

	inline void write(const typename ValueType::BytesType &data) {
		(*stream) << '"' << "BASE64:";
		sprt::base64url::encode(data.data(), data.size(),
				[&](const char *str, size_t len) { (*stream) << StringView(str, len); });
		(*stream) << '"';
	}
	inline void onBeginArray(const typename ValueType::ArrayType &arr) { (*stream) << '['; }
	inline void onEndArray(const typename ValueType::ArrayType &arr) { (*stream) << ']'; }
	inline void onBeginDict(const typename ValueType::DictionaryType &dict) { (*stream) << '{'; }
	inline void onEndDict(const typename ValueType::DictionaryType &dict) { (*stream) << '}'; }
	inline void onKey(const typename ValueType::StringType &str) {
		write(str);
		(*stream) << ':';
	}
	inline void onNextValue() { (*stream) << ','; }

	const Callback<void(StringView)> *stream = nullptr;
};

template <typename Interface>
struct PrettyEncoder : public Interface::AllocBaseType {
	using InterfaceType = Interface;
	using ValueType = ValueTemplate<Interface>;

	PrettyEncoder(const Callback<void(StringView)> *s, bool tM = false)
	: timeMarkers(tM), stream(s) { }

	void write(nullptr_t) {
		(*stream) << "null";
		offsetted = false;
	}
	void write(bool value) {
		(*stream) << ((value) ? "true" : "false");
		offsetted = false;
	}
	void write(int64_t value) {
		(*stream) << value;
		offsetted = false;
		if (timeMarkers
				&& (lastKey.find("time") != maxOf<size_t>()
						|| lastKey.find("Time") != maxOf<size_t>()
						|| lastKey.find("TIME") != maxOf<size_t>()
						|| lastKey.find("date") != maxOf<size_t>()
						|| lastKey.find("Date") != maxOf<size_t>())
				&& (value > 1'000'000'000'000'000 && value < 10'000'000'000'000'000)) {
			(*stream) << " /* "
					  << Time::microseconds(value).toHttp<typename Interface::StringType>()
					  << " */";
		}
	}
	void write(double value) {
		encodeDouble(*stream, value);
		offsetted = false;
	}

	void write(const typename ValueType::StringType &str) {
		encodeString(*stream, str);
		offsetted = false;
	}

	void write(const typename ValueType::BytesType &data) {
		(*stream) << '"' << "BASE64:";
		sprt::base64url::encode(data.data(), data.size(),
				[&](const char *str, size_t len) { (*stream) << StringView(str, len); });
		(*stream) << '"';
		offsetted = false;
	}

	bool isObjectArray(const typename ValueType::ArrayType &arr) {
		for (auto &it : arr) {
			if (!it.isDictionary()) {
				return false;
			}
		}
		return true;
	}

	void onBeginArray(const typename ValueType::ArrayType &arr) {
		(*stream) << '[';
		if (!isObjectArray(arr)) {
			++depth;
			bstack.push_back(false);
			offsetted = false;
		} else {
			bstack.push_back(true);
		}
	}

	void onEndArray(const typename ValueType::ArrayType &arr) {
		if (!bstack.empty()) {
			if (!bstack.back()) {
				--depth;
				(*stream) << '\n';
				for (size_t i = 0; i < depth; i++) { (*stream) << '\t'; }
			}
			bstack.pop_back();
		} else {
			--depth;
			(*stream) << '\n';
			for (size_t i = 0; i < depth; i++) { (*stream) << '\t'; }
		}
		(*stream) << ']';
		popComplex = true;
	}

	void onBeginDict(const typename ValueType::DictionaryType &dict) {
		lastKey = StringView();
		(*stream) << '{';
		++depth;
	}

	void onEndDict(const typename ValueType::DictionaryType &dict) {
		lastKey = StringView();
		--depth;
		(*stream) << '\n';
		for (size_t i = 0; i < depth; i++) { (*stream) << '\t'; }
		(*stream) << '}';
		popComplex = true;
	}

	void onKey(const typename ValueType::StringType &str) {
		lastKey = str;
		(*stream) << '\n';
		for (size_t i = 0; i < depth; i++) { (*stream) << '\t'; }
		write(str);
		offsetted = true;
		(*stream) << ':' << ' ';
	}

	void onNextValue() {
		lastKey = StringView();
		(*stream) << ',';
	}

	void onValue(const ValueType &val) {
		if (depth > 0) {
			if (popComplex && (val.isArray() || val.isDictionary())) {
				(*stream) << ' ';
			} else {
				if (!offsetted) {
					(*stream) << '\n';
					for (size_t i = 0; i < depth; i++) { (*stream) << '\t'; }
					offsetted = true;
				}
			}
			popComplex = false;
		}
	}

	size_t depth = 0;
	bool popComplex = false;
	bool offsetted = false;
	bool timeMarkers = false;
	const Callback<void(StringView)> *stream = nullptr;
	StringView lastKey;
	typename Interface::template ArrayType<bool> bstack;
};

template <typename Interface>
struct GitEncoder : public Interface::AllocBaseType {
	using InterfaceType = Interface;
	using ValueType = ValueTemplate<Interface>;

	inline GitEncoder(const Callback<void(StringView)> *s) : stream(s) { }

	// The scalars are RawEncoder's, byte for byte: inline is inline, and `encodeString` is reused
	// as it stands. Its rules - non-ASCII kept literal, only the controls escaped - are part of
	// this format rather than something to tidy up on the way past.
	void write(nullptr_t) { (*stream) << "null"; }
	void write(bool value) { (*stream) << ((value) ? "true" : "false"); }
	void write(int64_t value) { (*stream) << value; }
	void write(double value) { encodeDouble(*stream, value); }

	void write(const typename ValueType::StringType &str) { encodeString(*stream, str); }

	void write(const typename ValueType::BytesType &data) {
		(*stream) << '"' << "BASE64:";
		sprt::base64url::encode(data.data(), data.size(),
				[&](const char *str, size_t len) { (*stream) << StringView(str, len); });
		(*stream) << '"';
	}

	// The one rule that decides the layout. Local on purpose: it asks about the immediate children
	// and nothing deeper, so the answer for a container never depends on how far down the tree
	// goes.
	static bool isBlock(const typename ValueType::ArrayType &arr) {
		for (auto &it : arr) {
			if (it.isArray() || it.isDictionary()) {
				return true;
			}
		}
		return false;
	}

	static bool isBlock(const typename ValueType::DictionaryType &dict) {
		for (auto &it : dict) {
			if (it.second.isArray() || it.second.isDictionary()) {
				return true;
			}
		}
		return false;
	}

	bool inBlock() const { return !bstack.empty() && bstack.back(); }

	void newline() {
		(*stream) << '\n';
		for (size_t i = 0; i < depth; i++) { (*stream) << '\t'; }
	}

	// Printed before every child of the enclosing container - and the newline and indent come
	// BEFORE the comma, which is the entire format in one line of code.
	void prefix() {
		if (inBlock()) {
			newline();
		}
		if (comma) {
			(*stream) << ',' << ' ';
			comma = false;
		}
	}

	void open(char c, bool block) {
		(*stream) << c;
		bstack.push_back(block);
		if (block) {
			++depth;
		}
	}

	void close(char c) {
		bool block = false;
		if (!bstack.empty()) {
			block = bstack.back();
			bstack.pop_back();
		}
		if (block) {
			--depth;
			newline();
		}
		(*stream) << c;
		// A container never leaves a comma owing: the one that separates it from its next sibling
		// is set by the traversal afterwards, not carried out of here.
		comma = false;
	}

	void onBeginArray(const typename ValueType::ArrayType &arr) { open('[', isBlock(arr)); }
	void onEndArray(const typename ValueType::ArrayType &) { close(']'); }
	void onBeginDict(const typename ValueType::DictionaryType &dict) { open('{', isBlock(dict)); }
	void onEndDict(const typename ValueType::DictionaryType &) { close('}'); }

	void onKey(const typename ValueType::StringType &str) {
		prefix();
		encodeString(*stream, str);
		(*stream) << ':' << ' ';
		keyed = true; // consumed by the onValue that the traversal makes next
	}

	void onNextValue() { comma = true; }

	void onValue(const ValueType &) {
		if (keyed) {
			keyed = false; // a value behind a key: onKey already printed the prefix
			return;
		}
		if (bstack.empty()) {
			return; // the root has no prefix at all
		}
		prefix(); // an element of an array
	}

	/* `comma` and `keyed` are plain flags rather than stacks, and that is only correct because of
	how the traversal calls us: `onNextValue()` is always followed immediately by the `onKey` or
	`onValue` OF THE SAME CONTAINER, and `onKey` is always followed immediately by the value's own
	`onValue` (ValueTemplate::encode, SPDataValue.h). Neither flag can outlive a container
	boundary. If that loop ever changes, these two go wrong silently - which is why it is written
	down here rather than left to be rediscovered.

	`onKeyValuePair` and `onArrayValue` are deliberately NOT declared: StreamTraits would prefer
	them to `onKey`/the recursive encode, and every line prefix above would quietly stop being
	printed - leaving valid JSON with no newlines in it. */
	size_t depth = 0;
	bool comma = false;
	bool keyed = false;
	const Callback<void(StringView)> *stream = nullptr;
	typename Interface::template ArrayType<bool> bstack;
};

/* Which layout a JSON writer produces.

It replaced a pair of bools (`pretty`, `timeMarkers`) that could spell a state which does not
exist - `pretty = false, timeMarkers = true` - and had no room for a third layout. The bool
overloads are kept below for callers written against them. */
enum class Style : uint8_t {
	Compact, // no whitespace at all
	Pretty, // a readable dump, and only that: its rules are deliberately not pinned by a check
	PrettyTime, // Pretty plus a /* http-date */ comment on integers that look like timestamps.
	// NOT valid JSON - nothing reads it back, so it is a dump and never a document
	Git, // line-oriented and byte-stable: the form that goes into version control (GitEncoder)
};

inline Style styleOf(bool pretty, bool timeMarkers) {
	return pretty ? (timeMarkers ? Style::PrettyTime : Style::Pretty) : Style::Compact;
}

// The one place a style turns into an encoder, so that the string, the bytes, the callback and the
// file cannot drift apart - `Style::Git`'s closing newline is printed here for exactly that reason
template <typename Interface>
inline void write(const Callback<void(StringView)> &stream, const ValueTemplate<Interface> &val,
		Style style) {
	switch (style) {
	case Style::Compact: {
		RawEncoder<Interface> encoder(&stream);
		val.encode(encoder);
		break;
	}
	case Style::Pretty: {
		PrettyEncoder<Interface> encoder(&stream, false);
		val.encode(encoder);
		break;
	}
	case Style::PrettyTime: {
		PrettyEncoder<Interface> encoder(&stream, true);
		val.encode(encoder);
		break;
	}
	case Style::Git: {
		GitEncoder<Interface> encoder(&stream);
		val.encode(encoder);
		stream << '\n';
		break;
	}
	}
}

template <typename Interface>
inline auto write(const ValueTemplate<Interface> &val, Style style) ->
		typename Interface::StringType {
	typename Interface::StringType stream;
	write<Interface>([&](StringView str) { stream.append(str.data(), str.size()); }, val, style);
	return stream;
}

template <typename Interface>
inline void write(const Callback<void(StringView)> &stream, const ValueTemplate<Interface> &val,
		bool pretty, bool timeMarkers = false) {
	write<Interface>(stream, val, styleOf(pretty, timeMarkers));
}

template <typename Interface>
inline auto write(const ValueTemplate<Interface> &val, bool pretty = false,
		bool timeMarkers = false) -> typename Interface::StringType {
	return write<Interface>(val, styleOf(pretty, timeMarkers));
}

#if MODULE_STAPPLER_FILESYSTEM
template <typename Interface>
bool save(const ValueTemplate<Interface> &val, const FileInfo &info, Style style) {
	if (auto f = filesystem::File::open(info, filesystem::OpenFlags::Override)) {
		write([&](StringView str) { f.write((const uint8_t *)str.data(), str.size()); }, val,
				style);
		f.flush();
		f.close();
		return true;
	}
	return false;
}

template <typename Interface>
bool save(const ValueTemplate<Interface> &val, const FileInfo &info, bool pretty,
		bool timeMarkers = false) {
	// `timeMarkers` used to be accepted here and then dropped on the way into write(), so
	// PrettyTime saved as Pretty. Passing one Style instead of rebuilding a pair of bools is what
	// makes that class of mistake unavailable rather than merely fixed.
	return save<Interface>(val, info, styleOf(pretty, timeMarkers));
}
#endif

template <typename Interface>
auto toString(const ValueTemplate<Interface> &data, Style style) -> typename Interface::StringType {
	return write<Interface>(data, style);
}

template <typename Interface>
auto toString(const ValueTemplate<Interface> &data, bool pretty) -> typename Interface::StringType {
	return write<Interface>(data, styleOf(pretty, false));
}

} // namespace stappler::data::json

#endif /* STAPPLER_DATA_SPDATAENCODEJSON_H_ */
