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

#include "XLFocusWithin.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

ComponentId FocusWithinComponent::Id;

bool hasFocusWithin(const Node *node) {
	return node ? node->getComponent<FocusWithinComponent>() != nullptr : false;
}

static void retainFocusWithin(Node *node) {
	while (node) {
		node->setOrUpdateComponent<FocusWithinComponent>([](NotNull<FocusWithinComponent> c) {
			++c->counter;
			// Only the first retain changes what a selector sees, so only it dirties the node.
			return c->counter == 1;
		});
		node = node->getParent();
	}
}

static void releaseFocusWithin(Node *node) {
	while (node) {
		if (auto c = node->getComponent<FocusWithinComponent>()) {
			if (c->counter <= 1) {
				// Presence is the state: the last release removes the component.
				node->removeComponent<FocusWithinComponent>();
			} else {
				node->updateComponent<FocusWithinComponent>([](NotNull<FocusWithinComponent> c) {
					--c->counter;
					return false;
				});
			}
		}
		node = node->getParent();
	}
}

void updateFocusWithinChain(Node *from, Node *to) {
	if (from == to) {
		return;
	}

	// Retain before release: a shared ancestor goes 1 -> 2 -> 1 and keeps the component, so it is
	// not restyled (same order as the focus swap in FormSystem).
	if (to) {
		retainFocusWithin(to);
	}
	if (from) {
		releaseFocusWithin(from);
	}
}

} // namespace stappler::xenolith
