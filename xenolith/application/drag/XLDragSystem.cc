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
#include "XLInputDispatcher.h"
#include "director/XLDirector.h"
#include "XLAppThread.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

uint64_t DragSystem::Id = System::GetNextSystemId();

// --- DragSession -----------------------------------------------------------

bool DragSession::init(NotNull<DragSystem> system, DragOffer &&offer, Rc<Ref> &&source,
		uint32_t inputEventId) {
	_system = system;
	_offer = sp::move(offer);
	_source = sp::move(source);
	_inputEventId = inputEventId;

	// The clipboard half is built even for an in-process drag, so a target using
	// getTypes()/encode() works unchanged for external drags
	auto clipboard = _offer.takeClipboardData(_source);

	_data = Rc<DragData>::create(sp::move(clipboard), Rc<Ref>(_offer.local), _offer.localType);
	if (!_data) {
		return false;
	}

	// Hold the factory result in an Rc before parking it, or its only reference dies
	_decoratorParent = _offer.decoratorParent ? _offer.decoratorParent : system->getOwner();
	if (_offer.decorator && !_offer.decoratorDeferred && _decoratorParent) {
		Rc<Node> node = _offer.decorator();
		if (node) {
			installDecorator(sp::move(node));
		}
	}

	return true;
}

bool DragSession::init(NotNull<DragSystem> system, NotNull<sprt::window::DropOffer> offer,
		NotNull<AppThread> app, DragActions preferred) {
	_system = system;
	_external = offer.get();
	_offer.allowedActions = offer->getAllowedActions();
	setExternalPreferred(preferred);

	_data = Rc<DragData>::create(offer, app);
	return _data != nullptr;
}

void DragSession::setExternalPreferred(DragActions preferred) {
	// Without a preference from the OS, a drop from outside copies
	auto want = pickAction(preferred & _offer.allowedActions);
	_offer.defaultAction = (want != DragActions::None) ? want : DragActions::Copy;
}

void DragSession::setDecorator(Rc<Node> &&node) {
	// A late decorator after the drag ended would park a node nothing removes
	if (_finished || !_decoratorParent) {
		return;
	}

	if (_decorator) {
		_decorator->removeFromParent(true);
		_decorator = nullptr;
	}

	if (node) {
		installDecorator(sp::move(node));
		// Nothing else moves the decorator until the next pointer event
		updateDecorator();
	}
}

void DragSession::installDecorator(Rc<Node> &&node) {
	/* The Overlay level draws after the frame is captured, so a ghost made of captured pixels never
	captures itself. DecoratorZOrder orders it among other overlay nodes. */
	node->setOverlay(true);
	_decoratorParent->addChild(node, DragSystem::DecoratorZOrder);
	_decorator = sp::move(node);
}

DragEvent DragSession::makeEvent(Node *target) const {
	DragEvent ev;
	ev.session = const_cast<DragSession *>(this);
	ev.data = _data;
	ev.target = target;
	ev.worldLocation = _world;
	// Through the transform the target was drawn with, not convertToNodeSpace: the hit test
	// answered against the drawn frame, and the live tree may have moved since
	ev.location = target ? target->getModelToNodeTransform().transformPoint(_world) : _world;
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

	// For an external drag the OS has already turned the modifiers into its preference
	_preferred = modifiersToActions(_external ? InputModifier::None : mods, _offer.allowedActions,
			_offer.defaultAction);

	Node *next = nullptr;
	DragActions resolved = DragActions::None;
	auto cursorOverride = WindowCursor::Undefined;

	// Topmost first: registration order is paint order, walked backwards
	if (auto dispatcher = _system->getDispatcher()) {
		dispatcher->foreachHitTest(HitTestFlags::DropTarget,
				[&, this](const InputListenerStorage::HitTestRec &rec) {
			auto comp = getDropTarget(rec.node);
			if (!comp || !comp->enabled) {
				return true;
			}
			// The padding is the target's own, so the registry hands over records, not answers
			if (!rec.contains(world, comp->padding)) {
				return true;
			}

			auto response =
					comp->slots.accept ? comp->slots.accept(makeEvent(rec.node)) : DragResponse();

			// The modifier's preference wins when the target accepts it; otherwise any action both
			// sides allow (a Copy-only target still takes a Shift drag)
			auto common = response.accepted & _offer.allowedActions;
			auto action = hasFlag(common, _preferred) ? _preferred : pickAction(common);
			if (action == DragActions::None) {
				return true; // this one refuses; keep looking at whatever is under it
			}

			next = rec.node;
			resolved = action;
			cursorOverride = response.cursor;
			return false;
		});
	}

	setTarget(next, resolved);

	if (_target) {
		if (auto comp = getDropTarget(_target)) {
			if (comp->slots.over) {
				comp->slots.over(makeEvent(_target));
			}
		}
	}

	updateDecorator();

	if (_external) {
		_external->status(_resolved);
		return;
	}

	// No target shows Grabbing; a refused action shows NoDrop via actionToCursor
	auto cursor = (cursorOverride != WindowCursor::Undefined)
			? cursorOverride
			: (_target ? actionToCursor(_resolved) : WindowCursor::Grabbing);
	if (cursor != _cursor) {
		_cursor = cursor;
		_system->setCursor(cursor);
	}
}

void DragSession::setTarget(Node *next, DragActions resolved) {
	if (next == _target) {
		_resolved = resolved;
		return;
	}

	if (_target) {
		if (auto comp = getDropTarget(_target)) {
			if (comp->slots.leave) {
				comp->slots.leave(makeEvent(_target));
			}
		}
	}

	_target = next;
	_resolved = resolved;

	if (_target) {
		if (auto comp = getDropTarget(_target)) {
			if (comp->slots.enter) {
				comp->slots.enter(makeEvent(_target));
			}
		}
	}
}

void DragSession::handleTargetGone(NotNull<Node> target) {
	if (_target != target.get()) {
		return;
	}

	// `leave` still fires to close the enter/leave bracket; the drag continues
	if (auto comp = getDropTarget(_target)) {
		if (comp->slots.leave) {
			comp->slots.leave(makeEvent(_target));
		}
	}
	_target = nullptr;
	_resolved = DragActions::None;
}

void DragSession::updateDecorator() {
	if (!_decorator || !_decoratorParent) {
		return;
	}
	// into the parent's space, not always the system's owner - see DragOffer::decoratorParent
	_decorator->setPosition(_decoratorParent->convertToNodeSpace(_world) + _offer.decoratorOffset);
}

void DragSession::teardown() {
	if (_decorator) {
		_decorator->removeFromParent(true);
		_decorator = nullptr;
	}
	_decoratorParent = nullptr;
	if (_system && !_external) {
		_system->setCursor(WindowCursor::Undefined);
	}
}

void DragSession::finish(bool performDrop) {
	if (_finished) {
		return;
	}
	_finished = true;

	// Snapshot first: the drop may destroy the source, the target and their subtree, so no member
	// may be read after it runs
	Rc<Node> target = _target;
	auto resolved = _resolved;
	auto completion = sp::move(_offer.completion);

	teardown();

	bool dropped = false;
	if (target) {
		auto ev = makeEvent(target);

		/* `leave` before `drop`, so the highlight is down during the structural change and
		enter/leave stay bracketed. The component is looked up again for the drop, since a slot
		may take the target apart; the Rc above keeps the node alive. */
		if (auto comp = getDropTarget(target)) {
			if (comp->slots.leave) {
				comp->slots.leave(ev);
			}
		}

		if (performDrop && resolved != DragActions::None) {
			if (auto comp = getDropTarget(target)) {
				if (comp->slots.drop) {
					dropped = comp->slots.drop(ev, resolved);
				}
			}
		}
	}

	_target = nullptr;

	if (_external) {
		if (!performDrop) {
			// Cancelled from this side while the OS still thinks the drop is welcome
			_external->status(DragActions::None);
		}
		_data->settle(dropped ? resolved : DragActions::None);
	}

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

	// Nobody installed one: put it on the scene content
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

	// No visit hooks: drop targets publish themselves into the window's hit-test registry
	_systemFlags = SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents;
	return true;
}

void DragSystem::handleAdded(Node *owner) {
	System::handleAdded(owner);

	// findForNode returns the nearest system, so a nested one would split the scene
	sprt_passert(findForNode(owner->getParent()) == nullptr, "DragSystem must not be nested");

	// The cursor layer; disabled while idle
	_cursorListener = Rc<InputListener>::create(CursorListenerPriority);
	_cursorListener->setEnabled(false);
	owner->addSystem(_cursorListener);

	// Watches for a press ending outside its chain; see update()
	scheduleUpdate();
}

void DragSystem::handleRemoved() {
	cancelDrag();

	if (_cursorListener) {
		if (_owner) {
			_owner->removeSystem(_cursorListener);
		}
		_cursorListener = nullptr;
	}

	System::handleRemoved();
}

void DragSystem::handleExit() {
	// The whole subtree is leaving the scene; there is nowhere left to drop
	cancelDrag();

	System::handleExit();
}

void DragSystem::update(const UpdateTime &time) {
	System::update(time);

	/* A DropTargetComponent has no lifecycle, so a target leaving the scene or losing its component
	is detected here. Only "gone" is checked, not "no longer under the pointer" (refreshDrag). */
	if (_session) {
		if (auto target = _session->getTarget()) {
			if (!target->isRunning() || !getDropTarget(target)) {
				_session->handleTargetGone(target);
			}
		}
	}

	/* A source that left the scene mid-drag is inert, so the release is detected by the input chain
	that began the drag ending. Commit, not cancel: a chain cancelled by the pointer leaving the
	window has no target, so the commit drops nothing. */
	if (!_session || !_owner) {
		return;
	}

	// A drag begun from code has no input chain to watch
	if (_session->getInputEventId() == 0) {
		return;
	}

	auto director = _owner->getDirector();
	auto dispatcher = director ? director->getInputDispatcher() : nullptr;
	if (!dispatcher || dispatcher->isEventActive(_session->getInputEventId())) {
		return;
	}

	commitDrag();
}

InputDispatcher *DragSystem::getDispatcher() const {
	auto director = _owner ? _owner->getDirector() : nullptr;
	return director ? director->getInputDispatcher() : nullptr;
}

size_t DragSystem::getTargetCount() const {
	size_t count = 0;
	if (auto dispatcher = getDispatcher()) {
		dispatcher->foreachHitTest(HitTestFlags::DropTarget,
				[&](const InputListenerStorage::HitTestRec &) {
			++count;
			return true;
		});
	}
	return count;
}

void DragSystem::handleTargetGone(NotNull<Node> target) {
	if (_session) {
		_session->handleTargetGone(target);
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

void DragSystem::refreshDrag() {
	if (_session) {
		_session->update(_session->getWorldLocation(), _session->getModifiers());
	}
}

void DragSystem::commitDrag() {
	if (!_session) {
		return;
	}

	// Detach before finishing: anything re-entering during the drop must find no drag in flight
	auto session = sp::move(_session);
	_session = nullptr;
	session->finish(true);
}

void DragSystem::handleExternalDrop(const sprt::window::DropEvent &ev) {
	auto offer = ev.offer.get();
	if (!offer) {
		return;
	}

	bool own = _session && _session->getExternalOffer() == offer;

	// The OS runs one drag at a time: a new one means the old one ended without telling us
	if (!own && _session && _session->isExternal() && ev.phase == sprt::window::DropPhase::Enter) {
		cancelDrag();
	}

	switch (ev.phase) {
	case sprt::window::DropPhase::Enter:
	case sprt::window::DropPhase::Motion:
		if (!own) {
			// A Motion with no Enter is a drag that was already over the window when the scene
			// appeared: start it here
			auto director = _owner ? _owner->getDirector() : nullptr;
			auto app = director ? director->getApplication() : nullptr;
			if (_session || !app || offer->getAllowedActions() == DragActions::None) {
				offer->refuse(ev.phase);
				return;
			}

			auto session = Rc<DragSession>::create(this, offer, app, ev.preferred);
			if (!session) {
				offer->refuse(ev.phase);
				return;
			}
			_session = sp::move(session);
		}
		_session->setExternalPreferred(ev.preferred);
		_session->update(ev.location, ev.modifiers);
		break;
	case sprt::window::DropPhase::Leave:
		if (own) {
			cancelDrag();
		}
		break;
	case sprt::window::DropPhase::Drop:
		if (!own) {
			offer->refuse(ev.phase);
			return;
		}
		_session->setExternalPreferred(ev.preferred);
		_session->update(ev.location, ev.modifiers);
		commitDrag();
		break;
	}
}

void DragSystem::cancelDrag(Ref *source) {
	if (!_session) {
		return;
	}

	if (source && _session->getSource() != source) {
		return; // another source's teardown
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
