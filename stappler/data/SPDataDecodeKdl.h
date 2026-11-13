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

#ifndef STAPPLER_DATA_SPDATADECODEKDL_H_
#define STAPPLER_DATA_SPDATADECODEKDL_H_

#include "SPDataValue.h"
#include "SPKdlParser.h"

// A KDL v2 document as a value, by the mapping of SPDataEncodeKdl.h. A hand-written document reads
// the same way; what the mapping cannot tell apart is an error with its place in the text.

namespace STAPPLER_VERSIONIZED stappler::data::kdl {

using ParseError = stappler::kdl::ParseError;
using Location = stappler::kdl::Location;

template <typename Interface>
struct KdlDecoder {
	using ValueType = ValueTemplate<Interface>;
	using Type = typename ValueType::Type;
	using Node = stappler::kdl::NodeTemplate<memory::StandardInterface>;
	using Scalar = stappler::kdl::ValueTemplate<memory::StandardInterface>;
	using Property = stappler::kdl::PropertyTemplate<memory::StandardInterface>;
	using NodeList = memory::StandardInterface::VectorType<Node>;
	using PropertyList = memory::StandardInterface::VectorType<Property>;

	static bool isBase64(StringView text) {
		for (auto c : text) {
			if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
						|| c == '+' || c == '/' || c == '=')) {
				return false;
			}
		}
		return true;
	}

	ParseError *err = nullptr;

	template <typename... Args>
	bool fail(Location loc, Args &&...args) {
		if (err) {
			err->offset = 0;
			err->location = loc;
			err->message = mem_std::toString(sprt::forward<Args>(args)...);
		}
		return false;
	}

	bool readScalar(const Scalar &s, ValueType &out) {
		if (s.hasAnnotation) {
			auto annotation = s.getAnnotation();
			if (annotation == "base64" && s.isString()) {
				if (!isBase64(s.getString())) {
					return fail(s.location, "invalid base64 string");
				}
				out = ValueType(base64::decode<Interface>(CoderSource(s.getString())));
				return true;
			}
			return fail(s.location, "unknown value annotation (", annotation, ")");
		}
		switch (s.type) {
		case stappler::kdl::ValueType::Null: out = ValueType(); break;
		case stappler::kdl::ValueType::Bool: out = ValueType(s.boolean); break;
		case stappler::kdl::ValueType::String: out = ValueType(s.getString()); break;
		case stappler::kdl::ValueType::Integer:
			if (auto v = s.getInteger()) {
				out = ValueType(v.get());
			} else {
				return fail(s.location, "integer ", stappler::kdl::viewOf(s.literal),
						" does not fit in 64 bits");
			}
			break;
		case stappler::kdl::ValueType::Float:
		case stappler::kdl::ValueType::Inf:
		case stappler::kdl::ValueType::NegInf:
		case stappler::kdl::ValueType::NaN:
			if (auto v = s.getDouble()) {
				out = ValueType(v.get());
			} else {
				return fail(s.location, "number ", stappler::kdl::viewOf(s.literal),
						" is not a double");
			}
			break;
		}
		return true;
	}

	static bool isArrayNode(const Node &n) {
		if (!n.properties.empty()) {
			return false;
		}
		if (!n.arguments.empty()) {
			return n.arguments.size() > 1 || !n.children.empty();
		}
		if (n.children.empty()) {
			return false;
		}
		for (auto &it : n.children) {
			if (stappler::kdl::viewOf(it.name) != "-") {
				return false;
			}
		}
		return true;
	}

	bool readNode(const Node &n, ValueType &out) {
		auto annotation = n.getAnnotation();
		if (n.hasAnnotation && annotation != "array" && annotation != "object") {
			return fail(n.location, "unknown node annotation (", annotation, ")");
		}

		if (annotation == "object") {
			if (!n.arguments.empty()) {
				return fail(n.arguments.front().location, "an object has no arguments");
			}
			return readEntries(n.properties, n.children, out);
		}

		if (annotation != "array") {
			if (!n.arguments.empty() && !n.properties.empty()) {
				return fail(n.location, "a node with arguments has no properties");
			}
			if (n.arguments.size() == 1 && n.children.empty()) {
				return readScalar(n.arguments.front(), out);
			}
			if (n.arguments.empty() && n.properties.empty() && n.children.empty()) {
				return fail(n.location, "node ", stappler::kdl::viewOf(n.name),
						" has no value: an empty array is (array), an empty object is (object)");
			}
			if (!isArrayNode(n)) {
				return readEntries(n.properties, n.children, out);
			}
		}

		if (!n.properties.empty()) {
			return fail(n.properties.front().location, "an array has no properties");
		}
		out = ValueType(Type::ARRAY);
		for (auto &it : n.arguments) {
			ValueType v;
			if (!readScalar(it, v)) {
				return false;
			}
			out.addValue(sp::move(v));
		}
		for (auto &it : n.children) {
			if (stappler::kdl::viewOf(it.name) != "-") {
				return fail(it.location, "an array element is a node named -, not ",
						stappler::kdl::viewOf(it.name));
			}
			ValueType v;
			if (!readNode(it, v)) {
				return false;
			}
			out.addValue(sp::move(v));
		}
		return true;
	}

	// Properties repeat by the rule of KDL, the rightmost one wins; a child repeats nothing.
	bool readEntries(const PropertyList &props, const NodeList &children, ValueType &out) {
		out = ValueType(Type::DICTIONARY);
		for (auto &it : props) {
			ValueType v;
			if (!readScalar(it.value, v)) {
				return false;
			}
			out.setValue(sp::move(v), stappler::kdl::viewOf(it.name));
		}
		for (auto &it : children) {
			auto key = stappler::kdl::viewOf(it.name);
			if (out.hasValue(key)) {
				return fail(it.location, "duplicate key ", key);
			}
			ValueType v;
			if (!readNode(it, v)) {
				return false;
			}
			out.setValue(sp::move(v), key);
		}
		return true;
	}
};

// The root is always a dictionary: an empty document is an empty one.
template <typename Interface>
Status read(StringView source, ValueTemplate<Interface> &out, ParseError *err = nullptr) {
	stappler::kdl::DocumentTemplate<memory::StandardInterface> doc;
	auto st = stappler::kdl::read(source, doc, err);
	if (st != Status::Ok) {
		return st;
	}
	KdlDecoder<Interface> decoder{err};
	ValueTemplate<Interface> ret;
	if (!decoder.readEntries(typename KdlDecoder<Interface>::PropertyList(), doc.nodes, ret)) {
		return Status::ErrorInvalidArguemnt;
	}
	out = sp::move(ret);
	return Status::Ok;
}

// An empty value when the text is not a document of this mapping.
template <typename Interface>
auto read(StringView source, ParseError *err = nullptr) -> ValueTemplate<Interface> {
	ValueTemplate<Interface> ret;
	if (data::kdl::read<Interface>(source, ret, err) != Status::Ok) {
		return ValueTemplate<Interface>();
	}
	return ret;
}

} // namespace stappler::data::kdl

#endif /* STAPPLER_DATA_SPDATADECODEKDL_H_ */
