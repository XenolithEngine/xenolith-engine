/**
Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#ifndef STAPPLER_DATA_SPDATAENCODEKDL_H_
#define STAPPLER_DATA_SPDATAENCODEKDL_H_

#include "SPDataValue.h"
#include "SPKdlEmitter.h"

#include <sprt/runtime/utils/base64.h>

#if MODULE_STAPPLER_FILESYSTEM
#include "SPFilesystem.h"
#endif

// A value as a KDL v2 document: JiK 4.0.0 with the entries of the root dictionary as top-level
// nodes. The mapping is described in docs/usage/data/value.adoc; the output is the canonical form
// of stappler_kdl's emitter.

namespace STAPPLER_VERSIONIZED stappler::data::kdl {

template <typename Interface>
struct KdlEncoder {
	using ValueType = ValueTemplate<Interface>;
	using Type = typename ValueType::Type;
	using ArrayType = typename ValueType::ArrayType;
	using DictionaryType = typename ValueType::DictionaryType;

	static bool isContainer(const ValueType &v) {
		return v.getType() == Type::ARRAY || v.getType() == Type::DICTIONARY;
	}

	// Strings and keys go into the text as they are, so they have to be UTF-8.
	static bool isWritable(const ValueType &v) {
		switch (v.getType()) {
		case Type::CHARSTRING: return sprt::unicode::isValidUtf8(stappler::kdl::viewOf(v.getString()));
		case Type::ARRAY:
			for (auto &it : v.asArray()) {
				if (!isWritable(it)) {
					return false;
				}
			}
			break;
		case Type::DICTIONARY:
			for (auto &it : v.asDict()) {
				if (!sprt::unicode::isValidUtf8(stappler::kdl::viewOf(it.first))
						|| !isWritable(it.second)) {
					return false;
				}
			}
			break;
		default: break;
		}
		return true;
	}

	const Callback<void(StringView)> *stream;

	void indent(uint32_t depth) {
		for (uint32_t i = 0; i < depth; ++i) { *stream << "    "; }
	}

	void writeName(StringView name, StringView annotation) {
		if (!annotation.empty()) {
			*stream << "(" << annotation << ")";
		}
		stappler::kdl::writeString(*stream, name);
	}

	// A double keeps a point or an exponent, so that it reads back as a double.
	void writeDouble(double value) {
		if (sprt::isnan(value)) {
			*stream << "#nan";
		} else if (value == sprt::Infinity<double>) {
			*stream << "#inf";
		} else if (value == -sprt::Infinity<double>) {
			*stream << "#-inf";
		} else {
			char buf[sprt::DOUBLE_MAX_DIGITS];
			auto len = sprt::dtoa(value, buf, sprt::DOUBLE_MAX_DIGITS);
			StringView text(buf + sprt::DOUBLE_MAX_DIGITS - len, len);

			// The canonical exponent of stappler_kdl: `E`, and always a sign.
			auto mantissa = text.readUntil<StringView::Chars<'e', 'E'>>();
			*stream << mantissa;
			bool integral = true;
			for (auto c : mantissa) {
				if (c == '.') {
					integral = false;
					break;
				}
			}
			if (integral && text.empty()) {
				*stream << ".0";
			}
			if (!text.empty()) {
				++text;
				*stream << "E";
				if (!text.is('-') && !text.is('+')) {
					*stream << "+";
				}
				*stream << text;
			}
		}
	}

	void writeScalar(const ValueType &v) {
		switch (v.getType()) {
		case Type::BOOLEAN: *stream << (v.asBool() ? "#true" : "#false"); break;
		case Type::INTEGER: *stream << v.asInteger(); break;
		case Type::DOUBLE: writeDouble(v.asDouble()); break;
		case Type::CHARSTRING: stappler::kdl::writeString(*stream, stappler::kdl::viewOf(v.getString())); break;
		case Type::BYTESTRING:
			*stream << "(base64)";
			stappler::kdl::writeString(*stream,
					stappler::kdl::viewOf(base64::encode<Interface>(v.getBytes())));
			break;
		default: *stream << "#null"; break;
		}
	}

	void writeNode(StringView name, const ValueType &v, uint32_t depth) {
		indent(depth);
		switch (v.getType()) {
		case Type::ARRAY: writeArray(name, v.asArray(), depth); break;
		case Type::DICTIONARY: writeDict(name, v.asDict(), depth); break;
		default:
			writeName(name, StringView());
			*stream << " ";
			writeScalar(v);
			*stream << "\n";
			break;
		}
	}

	void writeArray(StringView name, const ArrayType &arr, uint32_t depth) {
		bool scalars = true;
		for (auto &it : arr) {
			if (isContainer(it)) {
				scalars = false;
				break;
			}
		}
		if (scalars) {
			writeName(name, arr.size() < 2 ? StringView("array") : StringView());
			for (auto &it : arr) {
				*stream << " ";
				writeScalar(it);
			}
			*stream << "\n";
			return;
		}
		writeName(name, StringView());
		*stream << " {\n";
		for (auto &it : arr) { writeNode("-", it, depth + 1); }
		indent(depth);
		*stream << "}\n";
	}

	void writeDict(StringView name, const DictionaryType &dict, uint32_t depth) {
		bool hasScalars = false;
		bool hasChildren = false;
		bool onlyDash = true;
		for (auto &it : dict) {
			if (isContainer(it.second)) {
				hasChildren = true;
				onlyDash = onlyDash && stappler::kdl::viewOf(it.first) == "-";
			} else {
				hasScalars = true;
			}
		}

		// Without properties, children named only `-` would read as an array.
		writeName(name, (!hasScalars && onlyDash) ? StringView("object") : StringView());
		for (auto &it : dict) {
			if (!isContainer(it.second)) {
				*stream << " ";
				stappler::kdl::writeString(*stream, stappler::kdl::viewOf(it.first));
				*stream << "=";
				writeScalar(it.second);
			}
		}
		if (hasChildren) {
			*stream << " {\n";
			for (auto &it : dict) {
				if (isContainer(it.second)) {
					writeNode(stappler::kdl::viewOf(it.first), it.second, depth + 1);
				}
			}
			indent(depth);
			*stream << "}";
		}
		*stream << "\n";
	}
};

// Writes nothing and answers false when the value cannot be a document: the root is not a
// dictionary, or a string or a key is not UTF-8.
template <typename Interface>
inline bool write(const Callback<void(StringView)> &stream, const ValueTemplate<Interface> &val) {
	if (!val.isDictionary() || !KdlEncoder<Interface>::isWritable(val)) {
		return false;
	}
	KdlEncoder<Interface> encoder{&stream};
	for (auto &it : val.asDict()) {
		encoder.writeNode(stappler::kdl::viewOf(it.first), it.second, 0);
	}
	return true;
}

// Empty when the value cannot be written, and for an empty dictionary.
template <typename Interface>
inline auto write(const ValueTemplate<Interface> &val) -> typename Interface::StringType {
	typename Interface::StringType ret;
	if (!data::kdl::write<Interface>([&](StringView str) { ret.append(str.data(), str.size()); },
				val)) {
		return typename Interface::StringType();
	}
	return ret;
}

#if MODULE_STAPPLER_FILESYSTEM
// A value that cannot be written leaves the file as it was.
template <typename Interface>
bool save(const ValueTemplate<Interface> &val, const FileInfo &info) {
	if (!val.isDictionary() || !KdlEncoder<Interface>::isWritable(val)) {
		return false;
	}
	if (auto f = filesystem::File::open(info, filesystem::OpenFlags::Override)) {
		data::kdl::write<Interface>(
				[&](StringView str) { f.write((const uint8_t *)str.data(), str.size()); }, val);
		f.flush();
		f.close();
		return true;
	}
	return false;
}
#endif

} // namespace stappler::data::kdl

#endif /* STAPPLER_DATA_SPDATAENCODEKDL_H_ */
