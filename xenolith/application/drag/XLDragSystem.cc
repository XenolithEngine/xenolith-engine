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

#include "XLDragSystem.h"
#include "XLNode.h"
#include "XLScene.h"
#include "XLSceneContent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

uint64_t DragSystem::Id = System::GetNextSystemId();

// --- DragSession -----------------------------------------------------------

bool DragSession::init(NotNull<DragSystem> system, DragOffer &&offer, Rc<Ref> &&source,
		uint32_t inputEventId) {
	_system = system;
	_offer = sp::move(offer);
	_source = sp::move(source);
	_inputEventId = inputEventId;

	// The clipboard half is built even for a purely in-process drag. It costs one allocation, and
	// it is what makes a target written against getTypes()/encode() work unchanged the day the
	// same drag arrives from another process
	auto clipboard = _offer.takeClipboardData(_source);

	_data = Rc<DragData>::create(sp::move(clipboard), Rc<Ref>(_offer.local), _offer.localType);
	if (!_data) {
		return false;
	}

	// Build, park, THEN keep. Calling the factory inline into addChild and holding the raw return
	// hands the scene a node whose only reference just died at the end of the full expression
	_decoratorParent = _offer.decoratorParent ? _offer.decoratorParent : system->getOwner();
	if (_offer.decorator && _decoratorParent) {
		Rc<Node> node = _offer.decorator();
		if (node) {
			_decoratorParent->addChild(node, DragSystem::DecoratorZOrder);
			_decorator = node;
		}
	}

	return true;
}

DragEvent DragSession::makeEvent(DropTarget *target) const {
	DragEvent ev;
	ev.session = const_cast<DragSession *>(this);
	ev.data = _data;
	ev.worldLocation = _world;
	ev.location = (target && target->getOwner()) ? target->getOwner()->convertToNodeSpace(_world)
												: _world;
	ev.allowed = _offer.allowedActions;
	ev.preferred = _preferred;
	ev.modifiers = _modifiers;
	return ev;
}

void DragSession::update(const Vec2 &world, InputModifier mods) {
	if (_finished || !_system) {
		return;
	}

	_world = world;
	_modifiers = mods;
	_preferred = modifiersToActions(mods, _offer.allowedActions, _offer.defaultAction);

	DropTarget *next = nullptr;
	DragActions resolved = DragActions::None;
	auto cursorOverride = WindowCursor::Undefined;

	// Backwards: the roster was filled in visit order, which is paint order, so the last entry
	// containing the point is the topmost one drawn there
	auto &targets = _system->_targets;
	for (size_t i = targets.size(); i > 0; --i) {
		auto &rec = targets[i - 1];
		if (!rec.target->isEnabled() || !rec.target->getOwner()) {
			continue;
		}
		if (!rec.worldRect.containsPoint(world)) {
			continue;
		}

		auto response = rec.target->handleDragAccept(makeEvent(rec.target));

		// The modifier's preference wins when the target can do it; otherwise whatever both
		// sides CAN agree on happens. That is what lets a Copy-only target take a drag the user
		// is holding Shift over, instead of silently refusing it
		auto common = response.accepted & _offer.allowedActions;
		auto action = hasFlag(common, _preferred) ? _preferred : pickAction(common);
		if (action == DragActions::None) {
			continue; // this one refuses; keep looking at whatever is under it
		}

		next = rec.target;
		resolved = action;
		cursorOverride = response.cursor;
		break;
	}

	setTarget(next, resolved);

	if (_target) {
		_target->handleDragOver(makeEvent(_target));
	}

	updateDecorator();

	// No target is not the same as a refused one: Grabbing says "still carrying", NoDrop says
	// "not here". actionToCursor answers the second case
	auto cursor = (cursorOverride != WindowCursor::Undefined)
			? cursorOverride
			: (_target ? actionToCursor(_resolved) : WindowCursor::Grabbing);
	if (cursor != _cursor) {
		_cursor = cursor;
		_system->setCursor(cursor);
	}
}

void DragSession::setTarget(DropTarget *next, DragActions resolved) {
	if (next == _target) {
		_resolved = resolved;
		return;
	}

	if (_target) {
		_target->handleDragLeave(makeEvent(_target));
	}

	_target = next;
	_resolved = resolved;

	if (_target) {
		_target->handleDragEnter(makeEvent(_target));
	}
}

void DragSession::handleTargetGone(NotNull<DropTarget> target) {
	if (_target != target.get()) {
		return;
	}

	// The node left the scene under the pointer. Its `leave` still fires - the bracket is a
	// promise - but the drag itself carries on looking for somewhere else to land
	_target->handleDragLeave(makeEvent(_target));
	_target = nullptr;
	_resolved = DragActions::None;
}

void DragSession::updateDecorator() {
	if (!_decorator || !_decoratorParent) {
		return;
	}
	// into the PARENT's space, which is not always the system's owner - see DragOffer::decoratorParent
	_decorator->setPosition(_decoratorParent->convertToNodeSpace(_world) + _offer.decoratorOffset);
}

void DragSession::teardown() {
	if (_decorator) {
		_decorator->removeFromParent(true);
		_decorator = nullptr;
	}
	_decoratorParent = nullptr;
	if (_system) {
		_system->setCursor(WindowCursor::Undefined);
	}
}

void DragSession::finish(bool performDrop) {
	if (_finished) {
		return;
	}
	_finished = true;

	// Snapshot first. The drop is allowed to destroy the source, the target and half the subtree
	// they live in - which is exactly what moving a docked panel does - so nothing may be read
	// out of a member after it runs
	Rc<DropTarget> target = _target;
	auto resolved = _resolved;
	auto completion = sp::move(_offer.completion);

	teardown();

	bool dropped = false;
	if (target) {
		auto ev = makeEvent(target);

		// leave BEFORE drop, so the target's highlight is already down while the structural
		// change happens - and so `enter` and `leave` stay an exact bracket in every path
		target->handleDragLeave(ev);

		if (performDrop && resolved != DragActions::None) {
			dropped = target->handleDragDrop(ev, resolved);
		}
	}

	_target = nullptr;

	if (completion) {
		completion(dropped ? resolved : DragActions::None);
	}
}

// --- DragSystem ------------------------------------------------------------

DragSystem *DragSystem::findForNode(Node *node) {
	while (node) {
		if (auto drag = node->getSystemByType<DragSystem>()) {
			return drag;
		}
		node = node->getParent();
	}
	return nullptr;
}

DragSystem *DragSystem::acquireForNode(Node *node) {
	if (auto drag = findForNode(node)) {
		return drag;
	}

	// Nobody installed one. Put it where it belongs rather than making every widget demand that
	// the application arrange a drag system before it can be dragged
	if (node) {
		if (auto scene = node->getScene()) {
			if (auto content = scene->getContent()) {
				return content->addSystem(Rc<DragSystem>::create());
			}
		}
	}

	slog().warn("DragSystem", "acquireForNode: the node is not in a scene with a content node");
	return nullptr;
}

bool DragSystem::init() {
	if (!System::init()) {
		return false;
	}

	_frameTag = DragSystem::Id;

	// Owner + scene events for the lifetime; visit control for the roster brackets; the frame
	// stack so descendants can find us during their own visit. No visit-self, no node events,
	// no update tick - a live pointer already keeps frames coming
	_systemFlags = SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents
			| SystemFlags::HandleVisitControl | SystemFlags::AddToFrameStack;
	return true;
}

void DragSystem::handleAdded(Node *owner) {
	System::handleAdded(owner);

	// getSystem<T>(tag) hands a descendant the NEAREST system with this tag, so a second one
	// deeper in the tree would quietly take every target below it out of this roster
	sprt_passert(findForNode(owner->getParent()) == nullptr, "DragSystem must not be nested");

	// The cursor layer. Idle it is disabled, so it registers nothing and costs nothing
	_cursorListener = Rc<InputListener>::create(CursorListenerPriority);
	_cursorListener->setEnabled(false);
	owner->addSystem(_cursorListener);
}

void DragSystem::handleRemoved() {
	cancelDrag();

	if (_cursorListener) {
		if (_owner) {
			_owner->removeSystem(_cursorListener);
		}
		_cursorListener = nullptr;
	}

	_targets.clear();
	_pendingTargets.clear();

	System::handleRemoved();
}

void DragSystem::handleExit() {
	// The whole subtree is leaving the scene; there is nowhere left to drop
	cancelDrag();
	_targets.clear();
	_pendingTargets.clear();

	System::handleExit();
}

void DragSystem::handleVisitBegin(FrameInfo &info) {
	System::handleVisitBegin(info);
	_pendingTargets.clear();
}

void DragSystem::handleVisitEnd(FrameInfo &info) {
	// Everything below has registered by now. Swap rather than assign: the outgoing vector
	// becomes next frame's scratch and keeps its capacity
	_targets.swap(_pendingTargets);
	System::handleVisitEnd(info);
}

void DragSystem::addTarget(NotNull<DropTarget> target, const Rect &worldRect) {
	_pendingTargets.emplace_back(TargetRec{Rc<DropTarget>(target.get()), worldRect});
}

void DragSystem::handleTargetGone(NotNull<DropTarget> target) {
	if (_session) {
		_session->handleTargetGone(target);
	}

	// Drop it from the committed roster too, or its last known rect stays hittable until the
	// next frame replaces the whole thing
	for (auto it = _targets.begin(); it != _targets.end(); ++it) {
		if (it->target == target.get()) {
			_targets.erase(it);
			break;
		}
	}
}

DragSession *DragSystem::beginDrag(DragOffer &&offer, Rc<Ref> &&source, uint32_t inputEventId) {
	if (!_owner || _session) {
		return nullptr; // one at a time
	}

	if (offer.allowedActions == DragActions::None) {
		return nullptr;
	}

	if (offer.externalPolicy != DragExternalPolicy::Never) {
		slog().warn("DragSystem",
				"externalPolicy other than Never is not implemented yet; refusing the drag");
		return nullptr;
	}

	auto session = Rc<DragSession>::create(this, sp::move(offer), sp::move(source), inputEventId);
	if (!session) {
		return nullptr;
	}

	_session = sp::move(session);
	setCursor(WindowCursor::Grabbing);
	return _session.get();
}

void DragSystem::updateDrag(const Vec2 &worldLocation, InputModifier mods) {
	if (_session) {
		_session->update(worldLocation, mods);
	}
}

void DragSystem::commitDrag() {
	if (!_session) {
		return;
	}

	// Detach BEFORE finishing. The drop mutates the scene, and anything that re-enters while it
	// does - a source's handleExit, a target's - must find no drag in flight
	auto session = sp::move(_session);
	_session = nullptr;
	session->finish(true);
}

void DragSystem::cancelDrag(Ref *source) {
	if (!_session) {
		return;
	}

	if (source && _session->getSource() != source) {
		return; // somebody else's teardown; not this drag's business
	}

	auto session = sp::move(_session);
	_session = nullptr;
	session->finish(false);
}

void DragSystem::setCursor(WindowCursor cursor) {
	if (!_cursorListener) {
		return;
	}

	if (cursor == WindowCursor::Undefined) {
		_cursorListener->setCursor(WindowCursor::Undefined);
		_cursorListener->setEnabled(false);
	} else {
		_cursorListener->setCursor(cursor);
		_cursorListener->setEnabled(true);
	}
}

} // namespace stappler::xenolith
