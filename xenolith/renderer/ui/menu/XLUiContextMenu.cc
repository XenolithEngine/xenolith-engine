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
#include "XLInputDispatcher.h"
#include "XLFrameContext.h"
#include "XLNode.h"
#include "XLScene.h"
#include "XLSceneContent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

uint64_t ContextMenuSystem::Id = System::GetNextSystemId();

// --- ContextMenuComponent ----------------------------------------------------------------------

ComponentId ContextMenuComponent::Id;

Rc<MenuSource> ContextMenuComponent::resolve(const ContextMenuRequest &request) const {
	// the builder decides first, the fixed source is the fallback; neither means nothing is
	// offered, which blocks (see the header)
	if (builder) {
		if (auto result = builder(request)) {
			return result;
		}
	}
	return source;
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

	// none installed: put one on the scene content
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

	// Visit events only to attach the listeners (see attachListener); targets live in the window's
	// hit-test registry
	_systemFlags = SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents
			| SystemFlags::HandleVisitControl;
	return true;
}

void ContextMenuSystem::handleAdded(Node *owner) {
	System::handleAdded(owner);

	// findForNode returns the nearest system, so a nested second one would split the scene
	sprt_passert(findForNode(owner->getParent()) == nullptr,
			"ContextMenuSystem must not be nested");

	_listener = Rc<InputListener>::create(ListenerPriority);

	/* Mouse: a right-button tap (press and release within tap tolerance), not a press, so
	right-button drags (ui::CanvasView panning) still work. */
	_listener->addTapRecognizer([this](const GestureTap &tap) {
		if (tap.event != GestureEvent::Activated) {
			return false;
		}
		// true only when a menu opened; swallows nothing
		return openAt(tap.location(), false,
				tap.input ? tap.input->data.input.modifiers : InputModifier::None);
	}, InputTapInfo{makeButtonMask({InputMouseButton::MouseRight}), 1});

	/* Touch: a long press. InputMouseButton::Touch equals MouseLeft, so the Touch modifier on the
	event tells a finger from a held mouse button. */
	_listener->addPressRecognizer(
			[this](const GesturePress &press) {
		// Began must be accepted, or the recognizer never reaches Activated
		if (press.event != GestureEvent::Activated) {
			return true;
		}

		// not a touch: declining cancels the hold
		if (!press.input || !hasFlag(press.input->data.input.modifiers, InputModifier::Touch)) {
			return false;
		}
		return openAt(press.location(), true, press.input->data.input.modifiers);
	},
			/* Not InputPressFlags::Capture (the default): this listener sees every press and must
			not take the pointer from the pressed widget. A widget that captures cancels this hold.
			*/
			InputPressInfo{makeButtonMask({InputMouseButton::Touch}), _longPress,
				InputPressFlags::None});

	/* Dismiss listener: pre-scene band, swallows the press (Captured cancels the rest of the
	chain), so a click outside an open menu only dismisses it. Enabled only while a menu is open.

	Native popups hold a pointer grab, so this serves the overlay path and headless, where the
	emulated window manager delivers to the parent. */
	_dismissListener = Rc<InputListener>::create(DismissListenerPriority);
	_dismissListener->setEnabled(false);
	_dismissListener->setSwallowEvent(InputEventName::Begin);
	_dismissListener->addTouchRecognizer(
			[this](const GestureData &data) {
		if (data.event == GestureEvent::Began) {
			close();
		}
		// accept every event of the chain: the swallowed Began made this listener its owner
		return true;
	},
			// every button: a right click while a menu is up closes it without opening another
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
	/* Attach only once the owner is running. acquireForNode is reached from a descendant's
	handleEnter, before Node::handleEnter sets `_running`; a system added then never gets
	handleEnter, and a listener that never entered refuses all events. Retried from handleEnter and
	the first visit. */
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

	System::handleRemoved();
}

void ContextMenuSystem::handleExit() {
	// scene torn down with a menu open: close it with the scene
	close();

	System::handleExit();
}

void ContextMenuSystem::handleVisitBegin(FrameInfo &info) {
	System::handleVisitBegin(info);

	// by the first visit everything above is running; see attachListener
	attachListener();
}

size_t ContextMenuSystem::getTargetCount() const {
	size_t count = 0;
	if (auto dispatcher = getDispatcher()) {
		dispatcher->foreachHitTest(HitTestFlags::ContextMenu,
				[&](const InputListenerStorage::HitTestRec &) {
			++count;
			return true;
		});
	}
	return count;
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

InputDispatcher *ContextMenuSystem::getDispatcher() const {
	auto owner = getOwner();
	auto director = owner ? owner->getDirector() : nullptr;
	return director ? director->getInputDispatcher() : nullptr;
}

Node *ContextMenuSystem::findTarget(const Vec2 &worldLocation) const {
	Node *found = nullptr;
	if (auto dispatcher = getDispatcher()) {
		// topmost first: walk backwards, registration order is paint order
		dispatcher->foreachHitTest(HitTestFlags::ContextMenu,
				[&](const InputListenerStorage::HitTestRec &rec) {
			auto comp = getContextMenu(rec.node);
			if (!comp || !comp->enabled) {
				return true;
			}
			if (!rec.contains(worldLocation, comp->padding)) {
				return true;
			}
			found = rec.node;
			return false;
		});
	}
	return found;
}

core::RenderServerChannel *ContextMenuSystem::getParentWindow() const {
	return getSubWindowParent(getOwner());
}

bool ContextMenuSystem::openAt(const Vec2 &worldLocation, bool fromTouch, InputModifier mods) {
	auto owner = getOwner();
	if (!owner) {
		return false;
	}

	auto node = findTarget(worldLocation);
	if (!node) {
		return false;
	}

	auto comp = getContextMenu(node);
	if (!comp) {
		return false;
	}

	ContextMenuRequest request;
	request.worldLocation = worldLocation;
	// in the declaring node's space, through the transform it was drawn with (as the hit test used)
	request.location = node->getModelToNodeTransform().transformPoint(worldLocation);
	request.modifiers = mods;
	request.fromTouch = fromTouch;

	auto source = comp->resolve(request);

	// remembered even when nothing is offered: callers want the target that answered
	_currentTarget = node;

	if (!source || source->countVisible() == 0) {
		return false;
	}

	auto window = getParentWindow();
	if (!window) {
		return false;
	}

	// one menu at a time: a second chain's outside tap would close the wrong one
	close();

	MenuConfig config;
	config.idPrefix = String("context");

	/* `onClose` of the previous menu may arrive after the next one opened (a right click while a
	menu is up); the generation keeps it from clearing the new handle. */
	const auto generation = ++_generation;
	config.onClose = [this, generation] {
		if (_generation == generation) {
			_menu = nullptr;
			// however the menu closed, stop swallowing presses
			updateDismissListener();
		}
	};

	// application customization; above all the stylesheet, which a native popup does not inherit
	if (_configCallback) {
		_configCallback(config);
	}

	// `owner` is the SceneContent, the space placementForPoint converts through
	_menu = openMenu(window, placementForPoint(owner, owner->convertToNodeSpace(worldLocation)),
			source, sp::move(config));

	updateDismissListener();
	return _menu != nullptr;
}

void ContextMenuSystem::updateDismissListener() {
	if (!_dismissListener) {
		return;
	}

	// the handle, not isOpen(): a native surface is not open yet when openMenu returns, and the
	// dismiss listener must already swallow clicks during those frames
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

static const ContextMenuComponent *ContextMenu_attach(NotNull<Node> node,
		const Callback<void(NotNull<ContextMenuComponent>)> &fill) {
	auto ret = node->setOrUpdateComponent<ContextMenuComponent>(
			[&](NotNull<ContextMenuComponent> comp) {
		// clear both so a fixed menu replaces a previous builder
		comp->source = nullptr;
		comp->builder = nullptr;
		fill(comp);
		return true;
	});

	// the visit reads the flag, the hit test reads the component
	node->addHitTestFlags(HitTestFlags::ContextMenu);

	// the coordinator must exist before the first press
	ContextMenuSystem::acquireForNode(node);
	return ret;
}

const ContextMenuComponent *setContextMenu(NotNull<Node> node, Rc<MenuSource> &&source) {
	return ContextMenu_attach(node,
			[&](NotNull<ContextMenuComponent> comp) { comp->source = sp::move(source); });
}

const ContextMenuComponent *setContextMenu(NotNull<Node> node,
		ContextMenuComponent::Builder &&builder) {
	return ContextMenu_attach(node,
			[&](NotNull<ContextMenuComponent> comp) { comp->builder = sp::move(builder); });
}

const ContextMenuComponent *getContextMenu(NotNull<Node> node) {
	return node->getComponent<ContextMenuComponent>();
}

void setContextMenuEnabled(NotNull<Node> node, bool value) {
	node->updateComponent<ContextMenuComponent>([&](NotNull<ContextMenuComponent> comp) {
		if (comp->enabled == value) {
			return false;
		}
		comp->enabled = value;
		return true;
	});
}

void removeContextMenu(NotNull<Node> node) {
	if (node->removeComponent<ContextMenuComponent>()) {
		node->removeHitTestFlags(HitTestFlags::ContextMenu);
	}
}

} // namespace stappler::xenolith::ui
