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
			// Only the first one changes what a selector sees; the rest must not re-dirty the
			// node, or a focus move inside a panel would restyle everything above it.
			return c->counter == 1;
		});
		node = node->getParent();
	}
}

static void releaseFocusWithin(Node *node) {
	while (node) {
		if (auto c = node->getComponent<FocusWithinComponent>()) {
			if (c->counter <= 1) {
				// Presence is the state, so the last release takes the component away rather than
				// leaving a zero behind for a matcher to read.
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

	// Retain BEFORE release: a shared ancestor of the two chains goes 1 -> 2 -> 1 and never loses
	// the component, so neither it nor anything under it is restyled for a move that did not leave
	// it. The same order, and the same reason, as the focus-in-then-out swap in FormSystem.
	if (to) {
		retainFocusWithin(to);
	}
	if (from) {
		releaseFocusWithin(from);
	}
}

} // namespace stappler::xenolith
