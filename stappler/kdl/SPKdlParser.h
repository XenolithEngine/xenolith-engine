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

#ifndef STAPPLER_KDL_SPKDLPARSER_H_
#define STAPPLER_KDL_SPKDLPARSER_H_

#include "SPKdlValue.h"

namespace STAPPLER_VERSIONIZED stappler::kdl {

// A value as the parser hands it over. The views are valid until the handler returns.
struct Scalar {
	ValueType type = ValueType::Null;
	bool boolean = false;
	bool hasAnnotation = false;
	StringView annotation;
	StringView text;
	StringView literal;
	size_t offset = 0;
};

struct ParseError {
	size_t offset = 0;
	Location location;
	mem_std::String message;
};

// Events of a parse, in document order; slashdashed parts produce none.
class SP_PUBLIC ParserHandler {
public:
	virtual ~ParserHandler() = default;

	virtual void handleBeginNode(const Scalar &name) = 0;
	virtual void handleArgument(const Scalar &value) = 0;
	virtual void handleProperty(const Scalar &key, const Scalar &value) = 0;
	virtual void handleEndNode() = 0;
};

// Line starts of a text, for turning byte offsets into a Location.
class SP_PUBLIC LineIndex {
public:
	explicit LineIndex(StringView source);

	Location locate(size_t offset) const;

private:
	StringView _source;
	mem_std::Vector<size_t> _starts;
};

// Parses a KDL v2 document. On failure the handler has seen a prefix of the document, and `err`
// tells where and why.
SP_PUBLIC Status parse(StringView source, ParserHandler &handler, ParseError *err = nullptr);

template <typename Interface>
class DocumentBuilder final : public ParserHandler {
public:
	using Document = DocumentTemplate<Interface>;
	using Node = NodeTemplate<Interface>;
	using Value = ValueTemplate<Interface>;
	using Property = PropertyTemplate<Interface>;

	DocumentBuilder(StringView source, Document &doc) : _index(source), _doc(&doc) { }

	void handleBeginNode(const Scalar &name) override {
		auto &list = _stack.empty() ? _doc->nodes : _stack.back()->children;
		auto &node = list.emplace_back();
		node.name = name.text.str<Interface>();
		node.hasAnnotation = name.hasAnnotation;
		node.annotation = name.annotation.str<Interface>();
		node.location = _index.locate(name.offset);
		_stack.emplace_back(&node);
	}

	void handleArgument(const Scalar &value) override {
		fill(_stack.back()->arguments.emplace_back(), value);
	}

	void handleProperty(const Scalar &key, const Scalar &value) override {
		auto &prop = _stack.back()->properties.emplace_back();
		prop.name = key.text.str<Interface>();
		prop.location = _index.locate(key.offset);
		fill(prop.value, value);
	}

	void handleEndNode() override { _stack.pop_back(); }

private:
	void fill(Value &out, const Scalar &value) {
		out.type = value.type;
		out.boolean = value.boolean;
		out.hasAnnotation = value.hasAnnotation;
		out.annotation = value.annotation.str<Interface>();
		out.text = value.text.str<Interface>();
		out.literal = value.literal.str<Interface>();
		out.location = _index.locate(value.offset);
	}

	LineIndex _index;
	Document *_doc = nullptr;

	// A child is placed in its parent's vector only after the parent stops growing, so the pointers
	// stay valid: siblings are appended after the previous sibling has ended.
	mem_std::Vector<Node *> _stack;
};

template <typename Interface>
Status read(StringView source, DocumentTemplate<Interface> &doc, ParseError *err = nullptr) {
	DocumentBuilder<Interface> builder(source, doc);
	return parse(source, builder, err);
}

} // namespace stappler::kdl

#endif /* STAPPLER_KDL_SPKDLPARSER_H_ */
