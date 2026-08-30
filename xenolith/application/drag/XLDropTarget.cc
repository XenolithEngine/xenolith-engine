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

#include "XLDropTarget.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

ComponentId DropTargetComponent::Id;

const DropTargetComponent *setDropTarget(NotNull<Node> node, DropTargetSlots &&slots) {
	auto ret =
			node->setOrUpdateComponent<DropTargetComponent>([&](NotNull<DropTargetComponent> comp) {
		comp->slots = sp::move(slots);
		return true;
	});

	// The flag and the component are one declaration: the visit reads the flag, the hit test reads
	// the component, and neither is any use without the other
	node->addHitTestFlags(HitTestFlags::DropTarget);
	return ret;
}

const DropTargetComponent *getDropTarget(NotNull<Node> node) {
	return node->getComponent<DropTargetComponent>();
}

void setDropTargetEnabled(NotNull<Node> node, bool value) {
	node->updateComponent<DropTargetComponent>([&](NotNull<DropTargetComponent> comp) {
		if (comp->enabled == value) {
			return false;
		}
		comp->enabled = value;
		return true;
	});
}

void setDropTargetPadding(NotNull<Node> node, float value) {
	node->updateComponent<DropTargetComponent>([&](NotNull<DropTargetComponent> comp) {
		if (comp->padding == value) {
			return false;
		}
		comp->padding = value;
		return true;
	});
}

void removeDropTarget(NotNull<Node> node) {
	if (node->removeComponent<DropTargetComponent>()) {
		node->removeHitTestFlags(HitTestFlags::DropTarget);
	}
}

} // namespace stappler::xenolith
