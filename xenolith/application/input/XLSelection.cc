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

#include "XLSelection.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

ComponentId SelectionComponent::Id;

bool hasSelectionWithin(const Node *node) {
	if (!node) {
		return false;
	}
	auto c = node->getComponent<SelectionComponent>();
	return c && c->withinCounter > 0;
}

bool isNodeSelected(const Node *node) {
	if (!node) {
		return false;
	}
	auto c = node->getComponent<SelectionComponent>();
	return c && c->selected;
}

void buildSelectionChain(Node *anchor, Vector<Rc<Node>> &out) {
	out.clear();
	for (auto node = anchor; node != nullptr; node = node->getParent()) { out.emplace_back(node); }
}

static void retainSelectionWithin(Node *node) {
	node->setOrUpdateComponent<SelectionComponent>([](NotNull<SelectionComponent> c) {
		++c->withinCounter;
		// Only the first one changes what a selector sees; the rest must not re-dirty the node, or
		// a selection moving between two rows would restyle everything above them.
		return c->withinCounter == 1;
	});
}

static void releaseSelectionWithin(Node *node) {
	auto c = node->getComponent<SelectionComponent>();
	if (!c) {
		return;
	}

	if (c->withinCounter <= 1 && !c->selected) {
		// Presence is the state, so the last release takes the component away rather than leaving a
		// zero behind for a matcher to read. NOT when the node is itself selected: that bit is the
		// other half of this component and outlives the chain.
		node->removeComponent<SelectionComponent>();
	} else {
		node->updateComponent<SelectionComponent>([](NotNull<SelectionComponent> c) {
			const auto before = c->withinCounter;
			--c->withinCounter;
			// Re-style only when the ANSWER changes, which for the counter is the 1 -> 0 edge. A
			// node kept alive by `selected` still stops matching `:selection-within` there, so that
			// edge has to dirty even though the component stays.
			return before == 1;
		});
	}
}

void updateSelectionChain(SpanView<Rc<Node>> from, SpanView<Rc<Node>> to) {
	// Retain BEFORE release: a shared ancestor of the two chains goes 1 -> 2 -> 1 and never loses
	// the component, so neither it nor anything under it is restyled for a move that did not leave
	// it. The same order, and the same reason, as updateFocusWithinChain.
	for (auto &node : to) { retainSelectionWithin(node); }
	for (auto &node : from) { releaseSelectionWithin(node); }
}

void setNodeSelected(Node *node, bool value) {
	if (!node) {
		return;
	}

	if (value) {
		node->setOrUpdateComponent<SelectionComponent>([](NotNull<SelectionComponent> c) {
			if (c->selected) {
				return false;
			}
			c->selected = true;
			return true;
		});
		return;
	}

	auto c = node->getComponent<SelectionComponent>();
	if (!c || !c->selected) {
		return;
	}
	if (c->withinCounter <= 0) {
		// Nothing else is holding the component up - the chain was released first, or this node
		// never had one. Take it away rather than leave a false behind for a matcher to read.
		node->removeComponent<SelectionComponent>();
		return;
	}
	node->updateComponent<SelectionComponent>([](NotNull<SelectionComponent> c) {
		c->selected = false;
		return true;
	});
}

} // namespace stappler::xenolith
