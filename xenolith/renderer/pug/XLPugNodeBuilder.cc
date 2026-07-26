/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#include "XLPugNodeBuilder.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::pugui {

NodeBuilder::NodeBuilder(NotNull<Node> root, BuilderConfig &&config)
: _root(root.get()), _config(move(config)) {
	if (!_config.registry) {
		_config.registry = Registry::createDefault();
	}
	if (_config.styleSheet) {
		_root->addSystem(Rc<StyleSheetSystem>::create(Rc<StyleSheet>(_config.styleSheet)));
	}
}

bool NodeBuilder::pushNode(StringView tag) {
	if (!_valid) {
		return false;
	}

	// the parent node can be materialized now - its attribute list is complete
	if (!_stack.empty() && !materializeTop()) {
		return false;
	}

	auto &pending = _stack.emplace_back();
	pending.tag = tag.str<memory::StandartInterface>();
	pending.factory = _config.registry->get(tag);
	if (!pending.factory) {
		onError(toString("pug: unknown tag '", tag, "', falling back to 'node'"));
		pending.factory = _config.registry->get("node");
	}
	pending.attrs = Value(Value::Type::DICTIONARY);
	return pending.factory != nullptr;
}

bool NodeBuilder::popNode() {
	if (!_valid || _stack.empty()) {
		onError("pug: unbalanced node close");
		_valid = false;
		return false;
	}

	Node *node = _stack.back().node ? _stack.back().node.get() : materializeTop();
	if (!node) {
		return false;
	}

	auto &pending = _stack.back();
	if (!pending.text.empty() && pending.factory && pending.factory->applyText) {
		StringView text(pending.text);
		if (_config.trimText) {
			text.trimChars<StringView::WhiteSpace>();
		}
		auto ctx = makeContext(node);
		pending.factory->applyText(ctx, node, text);
	}

	_stack.pop_back();
	return true;
}

bool NodeBuilder::setAttribute(StringView name, const spug::Value &value, bool) {
	if (!_valid || _stack.empty()) {
		onError("pug: attribute without an open node");
		return false;
	}

	auto &pending = _stack.back();
	// convert the pool-backed value into std memory immediately
	Value converted(value);
	if (pending.node) {
		// late attribute (should not happen with well-formed templates) - apply directly
		auto ctx = makeContext(pending.node);
		if (!pending.factory || !pending.factory->applyAttribute
				|| !pending.factory->applyAttribute(ctx, pending.node, name, converted)) {
			Registry::applyGenericAttribute(ctx, pending.node, name, converted);
		}
	} else {
		pending.attrs.setValue(move(converted), name.str<memory::StandartInterface>());
	}
	return true;
}

bool NodeBuilder::pushString(StringView str) {
	if (!_valid) {
		return false;
	}
	if (_stack.empty()) {
		// text without a node - nowhere to put it, report and drop
		onError(toString("pug: dropping top-level text: '", str, "'"));
		return true;
	}

	if (!_stack.back().node && !materializeTop()) {
		return false;
	}

	_stack.back().text.append(str.data(), str.size());
	return true;
}

void NodeBuilder::onError(StringView err) {
	if (_config.onError) {
		_config.onError(err);
	} else {
		log::source().warn("pugui", err);
	}
}

Node *NodeBuilder::materialize(Pending &pending, Node *parent) {
	if (!pending.factory || !pending.factory->create) {
		_valid = false;
		return nullptr;
	}

	auto ctx = makeContext(parent);
	auto node = pending.factory->create(ctx, pending.tag, pending.attrs);
	if (!node) {
		onError(toString("pug: factory for '", pending.tag, "' returned no node"));
		_valid = false;
		return nullptr;
	}

	for (auto &it : pending.attrs.asDict()) {
		StringView name(it.first);
		if (pending.factory->applyAttribute
				&& pending.factory->applyAttribute(ctx, node, name, it.second)) {
			continue;
		}
		if (!Registry::applyGenericAttribute(ctx, node, name, it.second)) {
			onError(toString("pug: unknown attribute '", name, "' on '", pending.tag, "'"));
		}
	}

	if (_config.styleSheet || _config.enableStyles) {
		// css element type; #id/classes were filled by the attribute pass above
		node->setType(pending.tag);
		node->addSystem(Rc<StyleApplier>::create());
	}

	pending.node = parent->addChild(node);
	return pending.node;
}

Node *NodeBuilder::materializeTop() {
	auto &pending = _stack.back();
	if (pending.node) {
		return pending.node;
	}

	Node *parent = _root;
	if (_stack.size() > 1) {
		auto &up = _stack[_stack.size() - 2];
		parent = up.node; // parents materialize before children are pushed
	}
	if (!parent) {
		_valid = false;
		return nullptr;
	}
	return materialize(pending, parent);
}

BuilderContext NodeBuilder::makeContext(Node *parent) {
	BuilderContext ctx;
	ctx.builder = this;
	ctx.parent = parent;
	ctx.resolveHandler = _config.resolveHandler ? &_config.resolveHandler : nullptr;
	ctx.onError = _config.onError ? &_config.onError : nullptr;
	return ctx;
}

bool runTemplate(const spug::Template &tpl, spug::Context &ctx, NodeBuilder &builder) {
	return tpl.run(ctx, builder) && builder.isValid();
}

} // namespace stappler::xenolith::pugui
