/**
 Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
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

#ifndef CORE_DOCUMENT_SPDOCUMENTNODE_H_
#define CORE_DOCUMENT_SPDOCUMENTNODE_H_

#include "SPDocStyle.h"
#include "SPDocument.h"

namespace STAPPLER_VERSIONIZED stappler::document {

class SP_PUBLIC Node : public memory::PoolInterface::AllocBaseType {
public:
	using Interface = memory::PoolInterface;

	template <typename T, typename V>
	using Map = memory::PoolInterface::MapType<T, V>;

	template <typename T>
	using Vector = memory::PoolInterface::VectorType<T>;

	using String = memory::PoolInterface::StringType;
	using WideString = memory::PoolInterface::WideStringType;

	using ForeachIter = Callback<void(Node &, size_t level)>;
	using ForeachConstIter = Callback<void(const Node &, size_t level)>;

	Node();
	Node(Node &&) = default;
	Node &operator=(Node &&) = default;

	Node(const Node &) = default;
	Node &operator=(const Node &) = default;

	Node(StringView htmlName);
	Node(StringView htmlName, WideString &&value);

	Node *pushNode(Node *);

	void setAttribute(StringView name, StringView value);

	void pushValue(StringView str);
	void pushValue(WideString &&str);

	void finalize();

	// void pushStyle(const Vector<StyleParameter> &, const MediaQueryId &);
	// void pushStyle(const StyleList &);

	Node *getParent() const { return _parent; }

	void setNodeId(NodeId id);
	NodeId getNodeId() const;

	const StyleList &getStyle() const;
	StyleList &getStyle();

	StringView getHtmlId() const { return _htmlId; }
	StringView getHtmlName() const { return _htmlName; }
	StringView getXType() const { return _xType; }
	WideStringView getValue() const { return _value; }

	const Vector<Node *> &getNodes() const;
	const Map<String, String> &getAttributes() const;
	const Vector<String> &getClasses() const;
	StringView getAttribute(StringView) const;

	bool empty() const;
	bool hasValue() const;

	explicit operator bool() const { return !empty(); }

	void foreach (const ForeachIter &);
	void foreach (const ForeachConstIter &) const;

	size_t getChildIndex(const Node &) const;

	WideString getValueRecursive() const;

protected:
	void propagateValue();
	void foreach (const ForeachIter &, size_t level);
	void foreach (const ForeachConstIter &, size_t level) const;

	bool _autoRefs = false;

	NodeId _nodeId = NodeIdNone;
	Node *_parent = nullptr;
	String _htmlId;
	String _htmlName;
	String _xType;
	WideString _value;

	StyleList _style;
	Vector<Node *> _nodes;
	Vector<String> _classes;
	Map<String, String> _attributes;
};
} // namespace stappler::document

#endif /* CORE_DOCUMENT_SPDOCUMENTNODE_H_ */
