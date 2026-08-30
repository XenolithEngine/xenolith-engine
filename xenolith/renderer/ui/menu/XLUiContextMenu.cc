/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#include "XLUiContextMenu.h"

#include "XLAppWindow.h"
#include "XLDirector.h"
#include "XLFrameContext.h"
#include "XLNode.h"
#include "XLScene.h"
#include "XLSceneContent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

uint64_t ContextMenuSystem::Id = System::GetNextSystemId();

// --- ContextMenuTarget -------------------------------------------------------------------------

bool ContextMenuTarget::init() {
	if (!System::init()) {
		return false;
	}

	// Owner events for the lifetime, scene events for handleExit, visit-self for the per-frame
	// registration. Nothing else: this system reads no node state, lays out nothing and, above all,
	// listens to nothing
	_systemFlags = SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents
			| SystemFlags::HandleVisitSelf;
	return true;
}

bool ContextMenuTarget::init(Rc<MenuSource> &&source) {
	if (!init()) {
		return false;
	}

	_source = sp::move(source);
	return true;
}

bool ContextMenuTarget::init(Builder &&builder) {
	if (!init()) {
		return false;
	}

	_builder = sp::move(builder);
	return true;
}

void ContextMenuTarget::handleVisitSelf(FrameInfo &info, Node *node, NodeVisitFlags flags) {
	System::handleVisitSelf(info, node, flags);

	if (!_enabled) {
		return;
	}

	// The nearest ContextMenuSystem above us on the frame stack. Absent when nobody installed one,
	// which is not an error: the node simply has no menu in that scene
	auto menus = info.getSystem<ContextMenuSystem>(ContextMenuSystem::Id);
	if (!menus) {
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

	menus->addTarget(this, rect);
}

void ContextMenuTarget::setSource(Rc<MenuSource> &&source) { _source = sp::move(source); }

void ContextMenuTarget::setBuilder(Builder &&builder) { _builder = sp::move(builder); }

Rc<MenuSource> ContextMenuTarget::resolve(const ContextMenuRequest &request) {
	// The builder decides first and may decline; the fixed source is what it falls back to. A
	// target with neither offers nothing, which blocks rather than falls through - see the header
	if (_builder) {
		if (auto source = _builder(request)) {
			return source;
		}
	}
	return _source;
}

// --- ContextMenuSystem -------------------------------------------------------------------------

ContextMenuSystem *ContextMenuSystem::findForNode(Node *node) {
	while (node) {
		if (auto menus = node->getSystemByType<ContextMenuSystem>()) {
			return menus;
		}
		node = node->getParent();
	}
	return nullptr;
}

ContextMenuSystem *ContextMenuSystem::acquireForNode(Node *node) {
	if (auto menus = findForNode(node)) {
		return menus;
	}

	// Nobody installed one. Put it where it belongs rather than making every widget demand that the
	// application arrange a context-menu system before it can carry a menu
	if (node) {
		if (auto scene = node->getScene()) {
			if (auto content = scene->getContent()) {
				return content->addSystem(Rc<ContextMenuSystem>::create());
			}
		}
	}

	log::source().warn("ContextMenuSystem",
			"acquireForNode: the node is not in a scene with a content node");
	return nullptr;
}

bool ContextMenuSystem::init() {
	if (!System::init()) {
		return false;
	}

	_frameTag = ContextMenuSystem::Id;

	// Owner + scene events for the lifetime; visit control for the roster brackets; the frame stack
	// so descendants can find us during their own visit. No visit-self, no node events, no update
	// tick - the listener is what wakes this system up
	_systemFlags = SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents
			| SystemFlags::HandleVisitControl | SystemFlags::AddToFrameStack;
	return true;
}

void ContextMenuSystem::handleAdded(Node *owner) {
	System::handleAdded(owner);

	// getSystem<T>(tag) hands a descendant the NEAREST system with this tag, so a second one deeper
	// in the tree would quietly take every target below it out of this roster
	sprt_passert(findForNode(owner->getParent()) == nullptr,
			"ContextMenuSystem must not be nested");

	_listener = Rc<InputListener>::create(ListenerPriority);

	/* The mouse: a TAP of the right button, which is press-and-release without moving past the tap
	tolerance. Not a press recognizer - that would fire while the button is still down and take
	right-button DRAGGING away from everything that uses it (ui::CanvasView pans with one). */
	_listener->addTapRecognizer([this](const GestureTap &tap) {
		if (tap.event != GestureEvent::Activated) {
			return false;
		}
		// True only when a menu actually opened. It reports Processed either way and swallows
		// nothing - this listener is the last one asked, so there is nobody left to keep it from
		return openAt(tap.location(), false,
				tap.input ? tap.input->data.input.modifiers : InputModifier::None);
	}, InputTapInfo{makeButtonMask({InputMouseButton::MouseRight}), 1});

	/* The touchscreen: a finger held still. The button mask cannot express "a finger" -
	InputMouseButton::Touch IS MouseLeft - so what the event was made by is read off the event
	itself, exactly as scroll inertia reads it. Without that check a mouse held down for half a
	second would open a menu. */
	_listener->addPressRecognizer(
			[this](const GesturePress &press) {
		// The press has to be ACCEPTED for the hold to be timed at all: a callback that declines
		// Began is never asked again, and the recognizer never reaches Activated
		if (press.event != GestureEvent::Activated) {
			return true;
		}

		// Declining here cancels the hold, which is what a mouse held down deserves: it is not a
		// context-menu gesture, and there is nothing more to wait for
		if (!press.input || !hasFlag(press.input->data.input.modifiers, InputModifier::Touch)) {
			return false;
		}
		return openAt(press.location(), true, press.input->data.input.modifiers);
	},
			/* NOT InputPressFlags::Capture, which is the default. Capturing would make this
			listener - which every press in the window reaches, since it is the last one asked -
			take the pointer exclusively from whatever widget was actually being pressed. The hold
			is timed just as well without it, and a widget that captures for itself (a drag
			starting) cancels this one, which is exactly the suppression that should happen. */
			InputPressInfo{makeButtonMask({InputMouseButton::Touch}), _longPress,
				InputPressFlags::None});

	/* The other listener: the one that takes an open menu down, and SPENDS the click doing it.

	It sits in the pre-scene band, so it is asked before every widget, and it swallows the press -
	Processed becomes Captured, which cancels everyone else for that whole chain. A click outside an
	open menu therefore dismisses it and reaches nothing; the next one is an ordinary click. That is
	what every desktop menu does, and it is the only arrangement that can: a dismiss listener asked
	last would be told about a press the widget under the pointer had already acted on.

	It is disabled whenever no menu is open, so it registers nothing and costs nothing - the same
	arrangement as DragSystem's cursor layer.

	On a real window system a native popup holds a pointer grab and the parent window never sees the
	press at all, so none of this runs there; it is for the in-scene overlay path, and for headless,
	where the emulated window manager delivers to the parent. */
	_dismissListener = Rc<InputListener>::create(DismissListenerPriority);
	_dismissListener->setEnabled(false);
	_dismissListener->setSwallowEvent(InputEventName::Begin);
	_dismissListener->addTouchRecognizer(
			[this](const GestureData &data) {
		if (data.event == GestureEvent::Began) {
			close();
		}
		// True for every event of the chain, not just the Began: the swallow turned that Began into
		// a capture, so this listener owns the rest of the press and has to keep accepting it
		return true;
	},
			// Every button, including the right one: a right click while a menu is up closes it and
			// does not open the next one. One rule for "a click outside is spent on the dismissal"
			// is easier to predict than one rule per button
			InputTouchInfo{makeButtonMask({InputMouseButton::MouseLeft,
				InputMouseButton::MouseRight, InputMouseButton::MouseMiddle})});

	_listener->setEnabled(_enabled);
	attachListener();
}

void ContextMenuSystem::handleEnter(Scene *scene) {
	System::handleEnter(scene);
	attachListener();
}

void ContextMenuSystem::attachListener() {
	/* Attached when the owner is RUNNING, which is not the same moment as when it exists.

	Node::handleEnter sets `_running` at its very end, after its children have entered - and
	acquireForNode is reached from a descendant's handleEnter, which is inside that window. A system
	added to a node that is not running yet is never handed handleEnter (see Node::addSystemItem),
	and an InputListener that never entered refuses every event before any filter of ours runs. So
	the attachment is retried: from our own handleEnter, and from the first visit, by which time
	everything above is certainly running. */
	if (!_owner || !_owner->isRunning()) {
		return;
	}

	if (_listener && !_listener->getOwner()) {
		_owner->addSystem(_listener);
	}
	if (_dismissListener && !_dismissListener->getOwner()) {
		_owner->addSystem(_dismissListener);
	}
}

void ContextMenuSystem::handleRemoved() {
	close();

	for (auto listener : {&_listener, &_dismissListener}) {
		if (*listener) {
			if (_owner) {
				_owner->removeSystem(listener->get());
			}
			*listener = nullptr;
		}
	}

	_targets.clear();
	_pendingTargets.clear();

	System::handleRemoved();
}

void ContextMenuSystem::handleExit() {
	// The scene is being torn down with a menu open on it. Take it along rather than leaving a
	// surface parented to a content node on its way out
	close();
	_targets.clear();
	_pendingTargets.clear();

	System::handleExit();
}

void ContextMenuSystem::handleVisitBegin(FrameInfo &info) {
	System::handleVisitBegin(info);

	// The last chance for the listener to join, and the one that always works: by the first visit
	// everything above this system is running. See attachListener
	attachListener();

	_pendingTargets.clear();
}

void ContextMenuSystem::handleVisitEnd(FrameInfo &info) {
	// Everything below has registered by now. Swap rather than assign: the outgoing vector becomes
	// next frame's scratch and keeps its capacity
	_targets.swap(_pendingTargets);
	System::handleVisitEnd(info);
}

void ContextMenuSystem::addTarget(NotNull<ContextMenuTarget> target, const Rect &worldRect) {
	_pendingTargets.emplace_back(TargetRec{Rc<ContextMenuTarget>(target.get()), worldRect});
}

void ContextMenuSystem::setMenuConfigCallback(Function<void(MenuConfig &)> &&cb) {
	_configCallback = sp::move(cb);
}

void ContextMenuSystem::setEnabled(bool value) {
	if (_enabled == value) {
		return;
	}

	System::setEnabled(value);

	if (_listener) {
		_listener->setEnabled(value);
	}
	if (!value) {
		close();
	}
	updateDismissListener();
}

bool ContextMenuSystem::isMenuOpen() const { return _menu && _menu->isOpen(); }

ContextMenuTarget *ContextMenuSystem::findTarget(const Vec2 &worldLocation) const {
	// Backwards: the roster was filled in visit order, which is paint order, so the last entry
	// containing the point is the topmost one drawn there
	for (size_t i = _targets.size(); i > 0; --i) {
		auto &rec = _targets[i - 1];
		if (!rec.target->isEnabled() || !rec.target->getOwner()) {
			continue;
		}
		if (rec.worldRect.containsPoint(worldLocation)) {
			return rec.target.get();
		}
	}
	return nullptr;
}

AppWindow *ContextMenuSystem::getAppWindow() const {
	auto owner = getOwner();
	auto scene = owner ? owner->getScene() : nullptr;
	auto director = scene ? scene->getDirector() : nullptr;
	auto server = director ? director->getRenderServer() : nullptr;
	return server ? dynamic_cast<AppWindow *>(server) : nullptr;
}

bool ContextMenuSystem::openAt(const Vec2 &worldLocation, bool fromTouch, InputModifier mods) {
	auto owner = getOwner();
	if (!owner) {
		return false;
	}

	auto target = findTarget(worldLocation);
	if (!target) {
		return false;
	}

	auto node = target->getOwner();
	if (!node) {
		return false;
	}

	ContextMenuRequest request;
	request.worldLocation = worldLocation;
	// In the DECLARING node's space, so a view can ask which of its rows this was without
	// converting anything itself
	request.location = node->convertToNodeSpace(worldLocation);
	request.modifiers = mods;
	request.fromTouch = fromTouch;

	auto source = target->resolve(request);

	// Remembered even when the answer was "nothing": what a test and a caller both want to know is
	// which target ANSWERED, not which one happened to have a menu
	_currentTarget = target;

	if (!source || source->countVisible() == 0) {
		return false;
	}

	auto window = getAppWindow();
	if (!window) {
		return false;
	}

	// One menu at a time. Closing the previous one first, rather than letting two chains coexist:
	// the second one's outside-tap would take down the wrong chain
	close();

	MenuConfig config;
	config.idPrefix = String("context");

	/* The generation is what keeps a REOPENING from erasing itself.

	`onClose` may arrive after the next menu has already been opened - closing one and opening
	another is one gesture, and a right click while a menu is up is the ordinary way to do it - and
	a callback that nulled the handle unconditionally would then throw away the handle of the menu
	that is on screen. */
	const auto generation = ++_generation;
	config.onClose = [this, generation] {
		if (_generation == generation) {
			_menu = nullptr;
			// However it went away - an item chosen, Escape, the surface closed - nothing outside
			// this scene should be swallowed any more
			updateDismissListener();
		}
	};

	// The application's say - the stylesheet above all, since a native popup is a scene of its own
	// and does not inherit the parent window's ui::StyleSystem
	if (_configCallback) {
		_configCallback(config);
	}

	// `owner` is the SceneContent, which is the space placementForPoint wants to convert through
	_menu = openMenu(window, placementForPoint(owner, owner->convertToNodeSpace(worldLocation)),
			source, sp::move(config));

	updateDismissListener();
	return _menu != nullptr;
}

void ContextMenuSystem::updateDismissListener() {
	if (!_dismissListener) {
		return;
	}

	// The HANDLE, not isOpen(): a surface is not open the instant openMenu returns - on the native
	// path the window is still being created - and a menu that swallowed nothing for its first
	// frames would let exactly the fastest click through
	_dismissListener->setEnabled(_enabled && _menu != nullptr);
}

void ContextMenuSystem::close() {
	if (auto menu = sp::move(_menu)) {
		_menu = nullptr;
		menu->dismiss();
	}
	updateDismissListener();
}

// --- the short way in --------------------------------------------------------------------------

static ContextMenuTarget *ContextMenu_attach(NotNull<Node> node, Rc<ContextMenuTarget> &&target) {
	// A second target on one node would register twice and shadow itself; replacing is what a
	// caller means by setting a menu on a node that already has one
	if (auto prev = node->getSystemByType<ContextMenuTarget>()) {
		node->removeSystem(prev);
	}

	auto ret = node->addSystem(sp::move(target));

	// The coordinator has to exist before the first press, and nothing else would create it. Not in
	// the target's handleAdded: the parent chain is not complete there
	ContextMenuSystem::acquireForNode(node);
	return ret;
}

ContextMenuTarget *setContextMenu(NotNull<Node> node, Rc<MenuSource> &&source) {
	return ContextMenu_attach(node, Rc<ContextMenuTarget>::create(sp::move(source)));
}

ContextMenuTarget *setContextMenu(NotNull<Node> node, ContextMenuTarget::Builder &&builder) {
	return ContextMenu_attach(node, Rc<ContextMenuTarget>::create(sp::move(builder)));
}

} // namespace stappler::xenolith::ui
