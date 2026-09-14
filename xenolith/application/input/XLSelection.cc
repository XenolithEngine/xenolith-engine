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
		// Only the first retain changes what a selector sees, so only it dirties the node.
		return c->withinCounter == 1;
	});
}

static void releaseSelectionWithin(Node *node) {
	auto c = node->getComponent<SelectionComponent>();
	if (!c) {
		return;
	}

	if (c->withinCounter <= 1 && !c->selected) {
		// Presence is the state: the last release removes the component, unless the node is
		// itself selected.
		node->removeComponent<SelectionComponent>();
	} else {
		node->updateComponent<SelectionComponent>([](NotNull<SelectionComponent> c) {
			const auto before = c->withinCounter;
			--c->withinCounter;
			// The 1 -> 0 edge changes `:selection-within` even when `selected` keeps the component.
			return before == 1;
		});
	}
}

void updateSelectionChain(SpanView<Rc<Node>> from, SpanView<Rc<Node>> to) {
	// Retain before release, so a shared ancestor is not restyled (as in updateFocusWithinChain).
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
		// No chain holds the component: remove it rather than leave `false` behind.
		node->removeComponent<SelectionComponent>();
		return;
	}
	node->updateComponent<SelectionComponent>([](NotNull<SelectionComponent> c) {
		c->selected = false;
		return true;
	});
}

} // namespace stappler::xenolith
