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

#include "SPDocNode.h"

namespace STAPPLER_VERSIONIZED stappler::document {

Node::Node() { }

Node::Node(StringView htmlName) : _htmlName(htmlName.str<Interface>()) { }

Node::Node(StringView htmlName, WideString &&value)
: _htmlName(htmlName.str<Interface>()), _value(move(value)) { }

Node *Node::pushNode(Node *node) {
	propagateValue();
	_nodes.emplace_back(node);
	node->_parent = this;
	return _nodes.back();
}

void Node::setAttribute(StringView name, StringView value) {
	if (isStringCaseEqual(name, "id")) {
		_htmlId = value.str<Interface>();
	} else if (isStringCaseEqual(name, "style")) {
		// do nothing
		return;
	} else if (isStringCaseEqual(name, "x-type")) {
		_xType = value.str<Interface>();
		string::apply_tolower_c(_xType);
	} else if (isStringCaseEqual(name, "class")) {
		value.split<StringView::WhiteSpace>([&](const StringView &c) {
			auto &it = _classes.emplace_back(c.str<Interface>());
			string::apply_tolower_c(it);
		});
	} else if (isStringCaseEqual(name, "x-refs")) {
		_autoRefs = (isStringCaseEqual(value, "auto"));
	}
	auto key = name.str<Interface>();
	string::apply_tolower_c(key);
	_attributes.emplace(move(key), value.str<Interface>());
}

void Node::pushValue(StringView str) { pushValue(string::toUtf16Html<Interface>(str)); }

void Node::pushValue(WideString &&str) {
	auto n = new (memory::pool::acquire()) Node(StringView("__value__"), sp::move(str));
	_nodes.emplace_back(n);
}

void Node::finalize() { }

void Node::setNodeId(NodeId id) { _nodeId = id; }
NodeId Node::getNodeId() const { return _nodeId; }

const StyleList &Node::getStyle() const { return _style; }

StyleList &Node::getStyle() { return _style; }

auto Node::getNodes() const -> const Vector<Node *> & { return _nodes; }

auto Node::getAttributes() const -> const Map<String, String> & { return _attributes; }

auto Node::getClasses() const -> const Vector<String> & { return _classes; }

StringView Node::getAttribute(StringView key) const {
	auto it = _attributes.find(key);
	if (it != _attributes.end()) {
		return it->second;
	}
	return StringView();
}

bool Node::empty() const { return _value.empty() && _nodes.empty(); }

bool Node::hasValue() const { return !_value.empty(); }

void Node::foreach (const ForeachIter &onNode) {
	onNode(*this, 0);
	foreach (onNode, 0);
}

void Node::foreach (const ForeachConstIter &onNode) const {
	onNode(*this, 0);
	foreach (onNode, 0);
}

size_t Node::getChildIndex(const Node &node) const {
	size_t idx = 0;
	for (auto &it : _nodes) {
		if (it == &node) {
			return idx;
		}
		++idx;
	}
	return 0;
}

auto Node::getValueRecursive() const -> WideString {
	WideString ret;
	foreach ([&](const Node &n, size_t level) {
		if (n.hasValue()) {
			ret += n.getValue().str<Interface>();
		}
	});
	return ret;
}

void Node::propagateValue() {
	if (!_value.empty()) {
		auto n = new (memory::pool::acquire()) Node(StringView("__value__"), sp::move(_value));
		_nodes.emplace_back(n);
		_value.clear();
	}
}

void Node::foreach (const ForeachIter &onNode, size_t level) {
	for (auto &it : _nodes) {
		onNode(*it, level + 1);
		it->foreach (onNode, level + 1);
	}
}

void Node::foreach (const ForeachConstIter &onNode, size_t level) const {
	for (auto &it : _nodes) {
		const Node *n = it;
		onNode(*n, level + 1);
		n->foreach (onNode, level + 1);
	}
}

} // namespace stappler::document
