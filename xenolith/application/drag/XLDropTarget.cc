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
#include "XLDragSystem.h"
#include "XLFrameContext.h"
#include "XLNode.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

bool DropTarget::init() {
	if (!System::init()) {
		return false;
	}

	// Owner events for the lifetime, scene events for handleExit, visit-self for the per-frame
	// registration. Nothing else: this system reads no node state and lays out nothing
	_systemFlags = SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents
			| SystemFlags::HandleVisitSelf;
	return true;
}

bool DropTarget::init(DropTargetSlots &&slots) {
	if (!init()) {
		return false;
	}

	_slots = sp::move(slots);
	return true;
}

void DropTarget::handleExit() {
	// The node is leaving the scene with a drag possibly hovering it. Tell the system so the
	// `leave` slot still fires - the drag itself carries on, a target vanishing is ordinary
	if (_current) {
		if (auto drag = DragSystem::findForNode(_owner)) {
			drag->handleTargetGone(this);
		}
		_current = false;
	}
	System::handleExit();
}

void DropTarget::handleVisitSelf(FrameInfo &info, Node *node, NodeVisitFlags flags) {
	System::handleVisitSelf(info, node, flags);

	if (!_enabled) {
		return;
	}

	// The nearest DragSystem above us on the frame stack. Absent when nobody installed one, which
	// is not an error: the node simply is not a drop target in that scene
	auto drag = info.getSystem<DragSystem>(DragSystem::Id);
	if (!drag) {
		return;
	}

	// Exactly the rect that was drawn - the transform stack already holds it, so there is nothing
	// to re-derive and nothing to get out of sync with
	auto rect = TransformRect(Rect(Vec2(0, 0), node->getContentSize()),
			info.modelTransformStack.back());

	if (_padding > 0.0f) {
		rect.origin.x -= _padding;
		rect.origin.y -= _padding;
		rect.size.width += _padding * 2.0f;
		rect.size.height += _padding * 2.0f;
	}

	drag->addTarget(this, rect);
}

void DropTarget::setSlots(DropTargetSlots &&slots) { _slots = sp::move(slots); }

DragResponse DropTarget::handleDragAccept(const DragEvent &event) {
	if (_slots.accept) {
		return _slots.accept(event);
	}
	// No predicate means no acceptance. An unconfigured target is inert, not permissive
	return DragResponse();
}

void DropTarget::handleDragEnter(const DragEvent &event) {
	_current = true;
	if (_slots.enter) {
		_slots.enter(event);
	}
}

void DropTarget::handleDragOver(const DragEvent &event) {
	if (_slots.over) {
		_slots.over(event);
	}
}

void DropTarget::handleDragLeave(const DragEvent &event) {
	_current = false;
	if (_slots.leave) {
		_slots.leave(event);
	}
}

bool DropTarget::handleDragDrop(const DragEvent &event, DragActions action) {
	if (_slots.drop) {
		return _slots.drop(event, action);
	}
	return false;
}

} // namespace stappler::xenolith
