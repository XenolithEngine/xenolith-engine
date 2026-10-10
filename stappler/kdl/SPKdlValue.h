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

#ifndef STAPPLER_KDL_SPKDLVALUE_H_
#define STAPPLER_KDL_SPKDLVALUE_H_

#include "SPMemory.h"

namespace STAPPLER_VERSIONIZED stappler::kdl {

enum class ValueType : uint8_t {
	Null,
	Bool,
	String,
	Integer,
	Float,
	Inf,
	NegInf,
	NaN,
};

// A view of a whole string: a string value may hold U+0000 from an escape, and the pointer
// constructors of StringView stop at the first NUL.
template <typename T>
inline StringView viewOf(const T &str) {
	StringView ret;
	ret.set(str.data(), str.size());
	return ret;
}

// One-based line and column (in codepoints) of a place in the source; zero is unknown.
struct Location {
	uint32_t line = 0;
	uint32_t column = 0;
};

// A number keeps its canonical decimal text, not a machine value: KDL numbers have no size limit,
// and a reader that wants an int64 or a double asks for one and is told when it does not fit.
template <typename Interface>
struct ValueTemplate : Interface::AllocBaseType {
	using StringType = typename Interface::StringType;

	ValueType type = ValueType::Null;
	bool boolean = false;
	bool hasAnnotation = false;

	// The type annotation, `(u8)` in `(u8)12`.
	StringType annotation;

	// A string's decoded text, or a number's canonical decimal form (`0x10` is `16`, `1e3` is
	// `1E+3`).
	StringType text;

	// A number as it was written, without the type annotation.
	StringType literal;

	Location location;

	bool isNull() const { return type == ValueType::Null; }
	bool isBool() const { return type == ValueType::Bool; }
	bool isString() const { return type == ValueType::String; }
	bool isInteger() const { return type == ValueType::Integer; }
	bool isNumber() const { return type >= ValueType::Integer; }

	StringView getString() const { return isString() ? viewOf(text) : StringView(); }
	StringView getAnnotation() const { return hasAnnotation ? viewOf(annotation) : StringView(); }

	sprt::Result<int64_t> getInteger() const;
	sprt::Result<uint64_t> getUnsigned() const;
	sprt::Result<double> getDouble() const;
};

template <typename Interface>
struct PropertyTemplate : Interface::AllocBaseType {
	typename Interface::StringType name;
	ValueTemplate<Interface> value;
	Location location;
};

template <typename Interface>
struct NodeTemplate : Interface::AllocBaseType {
	using StringType = typename Interface::StringType;
	using Value = ValueTemplate<Interface>;
	using Property = PropertyTemplate<Interface>;
	using Node = NodeTemplate<Interface>;

	StringType name;
	StringType annotation;
	bool hasAnnotation = false;
	Location location;

	typename Interface::template VectorType<Value> arguments;

	// In the order written, duplicates included; getProperty answers with the rightmost one.
	typename Interface::template VectorType<Property> properties;

	typename Interface::template VectorType<Node> children;

	StringView getAnnotation() const { return hasAnnotation ? viewOf(annotation) : StringView(); }

	const Property *getProperty(StringView key) const {
		for (auto it = properties.rbegin(); it != properties.rend(); ++it) {
			if (viewOf(it->name) == key) {
				return &(*it);
			}
		}
		return nullptr;
	}

	const Value *getPropertyValue(StringView key) const {
		auto prop = getProperty(key);
		return prop ? &prop->value : nullptr;
	}
};

template <typename Interface>
struct DocumentTemplate : Interface::AllocBaseType {
	typename Interface::template VectorType<NodeTemplate<Interface>> nodes;
};

namespace detail {

SP_PUBLIC sprt::Result<int64_t> readInteger(StringView canonical);
SP_PUBLIC sprt::Result<uint64_t> readUnsigned(StringView canonical);
SP_PUBLIC sprt::Result<double> readDouble(StringView canonical);

} // namespace detail

template <typename Interface>
sprt::Result<int64_t> ValueTemplate<Interface>::getInteger() const {
	return type == ValueType::Integer ? detail::readInteger(text) : sprt::Result<int64_t>();
}

template <typename Interface>
sprt::Result<uint64_t> ValueTemplate<Interface>::getUnsigned() const {
	return type == ValueType::Integer ? detail::readUnsigned(text) : sprt::Result<uint64_t>();
}

template <typename Interface>
sprt::Result<double> ValueTemplate<Interface>::getDouble() const {
	switch (type) {
	case ValueType::Integer:
	case ValueType::Float: return detail::readDouble(text);
	case ValueType::Inf: return sprt::Result<double>(sprt::Infinity<double>);
	case ValueType::NegInf: return sprt::Result<double>(-sprt::Infinity<double>);
	case ValueType::NaN: return sprt::Result<double>(sprt::NaN<double>);
	default: break;
	}
	return sprt::Result<double>();
}

} // namespace stappler::kdl

#endif /* STAPPLER_KDL_SPKDLVALUE_H_ */
