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

#ifndef STAPPLER_KDL_SPKDLEMITTER_H_
#define STAPPLER_KDL_SPKDLEMITTER_H_

#include "SPKdlValue.h"

namespace STAPPLER_VERSIONIZED stappler::kdl {

// The canonical form is the one of the KDL test suite: no comments, 4-space indents, properties
// deduplicated and sorted, strings bare where they can be and quoted otherwise, numbers in their
// canonical decimal text, children only when there are any.

using OutputCallback = Callback<void(StringView)>;

// Whether the text can be written as a bare identifier string.
SP_PUBLIC bool isIdentifier(StringView);

SP_PUBLIC void writeQuoted(const OutputCallback &, StringView);

// Bare when it can be, quoted otherwise.
SP_PUBLIC void writeString(const OutputCallback &, StringView);

SP_PUBLIC void writeScalar(const OutputCallback &, ValueType, bool boolean, StringView text);

template <typename Interface>
void writeValue(const OutputCallback &out, const ValueTemplate<Interface> &value) {
	if (value.hasAnnotation) {
		out << "(";
		writeString(out, viewOf(value.annotation));
		out << ")";
	}
	writeScalar(out, value.type, value.boolean, viewOf(value.text));
}

template <typename Interface>
void writeNode(const OutputCallback &out, const NodeTemplate<Interface> &node, uint32_t depth = 0) {
	for (uint32_t i = 0; i < depth; ++i) { out << "    "; }
	if (node.hasAnnotation) {
		out << "(";
		writeString(out, viewOf(node.annotation));
		out << ")";
	}
	writeString(out, viewOf(node.name));
	for (auto &arg : node.arguments) {
		out << " ";
		writeValue(out, arg);
	}

	mem_std::Vector<const PropertyTemplate<Interface> *> props;
	for (auto &prop : node.properties) {
		if (node.getProperty(viewOf(prop.name)) == &prop) {
			props.emplace_back(&prop);
		}
	}
	sprt::sort(props.begin(), props.end(),
			[](const PropertyTemplate<Interface> *l, const PropertyTemplate<Interface> *r) {
		return viewOf(l->name) < viewOf(r->name);
	});
	for (auto prop : props) {
		out << " ";
		writeString(out, viewOf(prop->name));
		out << "=";
		writeValue(out, prop->value);
	}

	if (!node.children.empty()) {
		out << " {\n";
		for (auto &child : node.children) { writeNode(out, child, depth + 1); }
		for (uint32_t i = 0; i < depth; ++i) { out << "    "; }
		out << "}";
	}
	out << "\n";
}

template <typename Interface>
void write(const OutputCallback &out, const DocumentTemplate<Interface> &doc) {
	for (auto &node : doc.nodes) { writeNode(out, node); }
}

} // namespace stappler::kdl

#endif /* STAPPLER_KDL_SPKDLEMITTER_H_ */
