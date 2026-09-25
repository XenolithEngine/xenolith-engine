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

#include "XLUiTooltipSystem.h"

#include "XLUiPanel.h"
#include "XL2dLabel.h"
#include "XL2dSceneLayout.h"
#include "XLAction.h"
#include "XLAppWindow.h"
#include "XLDirector.h"
#include "XLInputDispatcher.h"
#include "XLScene.h"

#include <cmath>

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

uint64_t TooltipSystem::Id = System::GetNextSystemId();

// The stock hint's metrics, not stylesheet defaults: a scene without a StyleSystem still gets a
// readable hint. A sheet in scope styles the hint on either path.
static constexpr float kTipHeight = 34.0f;
static constexpr float kTipFontSize = 13.0f;
static constexpr float kTipPadding = 12.0f;
static constexpr float kTipMinWidth = 120.0f;

Extent2 TooltipSystem::measureDefaultTooltip(StringView text, const TooltipConfig &config) {
	// Rough advance-width estimate: the hint is built before it is measured, and is clamped.
	const float textWidth = float(text.size()) * kTipFontSize * 0.58f;

	auto extent =
			Extent2(uint32_t(std::lround(sprt::max(kTipMinWidth, textWidth + kTipPadding * 2.0f))),
					uint32_t(std::lround(kTipHeight)));

	if (text.empty()) {
		extent = config.defaultSize;
	}

	return sprt::window::clampWindowExtent(extent, config.minExtent, config.maxExtent);
}

Rc<basic2d::SceneLayout2d> TooltipSystem::buildDefaultTooltip(NotNull<SubWindow>,
		const TooltipRequest &request) {
	const auto size = Size2(float(request.size.width), float(request.size.height));

	auto layout = Rc<basic2d::SceneLayout2d>::create();
	layout->setContentSize(size);

	// The stock hint's name, used by tools and tests; openOverlay supplies it only when a builder
	// left none.
	layout->setName("aux-tip");

	auto bg = layout->addChild(Rc<Panel>::create());
	bg->setAnchorPoint(Anchor::BottomLeft);
	bg->setPosition(Vec2::ZERO);
	bg->setContentSize(size);
	// Typed and classed for stylesheets; coloured here so it is readable without one.
	bg->setType("tooltip");
	bg->addStyleClass("xl-ui-tooltip");
	bg->setColor(Color(0x10'1014));

	auto label = layout->addChild(Rc<basic2d::Label>::create());
	label->setString(request.text);
	label->setFontSize(uint16_t(kTipFontSize));
	label->setColor(Color::White);
	label->setType("label");
	label->addStyleClass("xl-ui-tooltip-label");
	label->setAnchorPoint(Anchor::Middle);
	label->setPosition(Vec2(size.width / 2.0f, size.height / 2.0f));

	return layout;
}

// --- TooltipComponent -------------------------------------------------------------------------

ComponentId TooltipComponent::Id;

/* Ensures the scene has a coordinator. A hint is often declared before the widget is in a scene,
and a component cannot observe its later entry, so acquisition is deferred to a one-shot system
that removes itself once done. */
static void Tooltip_acquireSystem(NotNull<Node> node) {
	if (node->getScene()) {
		TooltipSystem::acquireForNode(node);
		return;
	}

	static constexpr uint64_t AnchorTag = "XLUiTooltipAnchor"_tag;
	for (auto &it : node->getSystems()) {
		if (it->getFrameTag() == AnchorTag) {
			return; // already waiting
		}
	}

	auto anchor = Rc<CallbackSystem>::create();
	anchor->setFrameTag(AnchorTag);

	// On the first visit, not on entering: Node::handleEnter sets `_running` at its end, so a
	// system added from a descendant's entry would never get handleEnter, its tick or listener.
	anchor->setVisitSelfCallback([](CallbackSystem *self, FrameInfo &, Node *, NodeVisitFlags) {
		auto owner = self->getOwner();
		if (!owner) {
			return;
		}
		TooltipSystem::acquireForNode(owner);

		// Node::visitSelf iterates a copy of the system list, which also keeps this object alive,
		// so removing ourselves here is safe.
		owner->removeSystem(self);
	});
	node->addSystem(sp::move(anchor));
}

static const TooltipComponent *Tooltip_attach(NotNull<Node> node,
		const Callback<void(NotNull<TooltipComponent>)> &fill) {
	auto ret = node->setOrUpdateComponent<TooltipComponent>([&](NotNull<TooltipComponent> comp) {
		fill(comp);
		return true;
	});

	// The flag and the component form one declaration: the visit reads the flag, the hover
	// resolution reads the component.
	node->addHitTestFlags(HitTestFlags::Tooltip);

	Tooltip_acquireSystem(node);

	// A hint currently up for this node is rebuilt.
	if (auto system = TooltipSystem::findForNode(node)) {
		system->handleNodeChanged(node);
	}
	return ret;
}

const TooltipComponent *setTooltip(NotNull<Node> node, TooltipInfo &&info) {
	return Tooltip_attach(node,
			[&](NotNull<TooltipComponent> comp) { comp->info = sp::move(info); });
}

const TooltipComponent *setTooltip(NotNull<Node> node, StringView text) {
	return Tooltip_attach(node,
			[&](NotNull<TooltipComponent> comp) { comp->info.text = text.str<Interface>(); });
}

const TooltipComponent *getTooltip(NotNull<Node> node) {
	return node->getComponent<TooltipComponent>();
}

void setTooltipText(NotNull<Node> node, StringView text) {
	auto changed = node->updateComponent<TooltipComponent>([&](NotNull<TooltipComponent> comp) {
		if (comp->info.text == text) {
			return false;
		}
		comp->info.text = text.str<Interface>();
		return true;
	});
	if (changed) {
		if (auto system = TooltipSystem::findForNode(node)) {
			system->handleNodeChanged(node);
		}
	}
}

void setTooltipEnabled(NotNull<Node> node, bool value) {
	node->updateComponent<TooltipComponent>([&](NotNull<TooltipComponent> comp) {
		if (comp->enabled == value) {
			return false;
		}
		comp->enabled = value;
		return true;
	});
	if (auto system = TooltipSystem::findForNode(node)) {
		system->handleNodeChanged(node);
	}
}

void removeTooltip(NotNull<Node> node) {
	if (node->removeComponent<TooltipComponent>()) {
		node->removeHitTestFlags(HitTestFlags::Tooltip);
		if (auto system = TooltipSystem::findForNode(node)) {
			system->handleNodeChanged(node);
		}
	}
}

// --- TooltipSystem ---------------------------------------------------------------------------

TooltipSystem *TooltipSystem::findForNode(Node *node) {
	while (node) {
		if (auto *tips = node->getSystemByType<TooltipSystem>()) {
			return tips;
		}
		node = node->getParent();
	}
	return nullptr;
}

TooltipSystem *TooltipSystem::acquireForNode(Node *node) {
	if (auto *tips = findForNode(node)) {
		return tips;
	}

	// None installed: put one on the scene content so widgets need no application setup.
	if (node) {
		if (auto scene = node->getScene()) {
			if (auto content = scene->getContent()) {
				return content->addSystem(Rc<TooltipSystem>::create());
			}
		}
	}

	// Quiet: a hint is often declared before the widget is in a scene; Tooltip_acquireSystem
	// retries on entry.
	return nullptr;
}

bool TooltipSystem::init() {
	if (!System::init()) {
		return false;
	}

	_frameTag = TooltipSystem::Id;

	// Owner and scene events, and visit control only as the last chance to attach the listeners
	// (see handleVisitBegin). The update tick notices a node sliding away under a still pointer.
	_systemFlags = SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents
			| SystemFlags::HandleVisitControl;

	_defaultFactory = [](NotNull<SubWindow> surface, const TooltipRequest &request) {
		return buildDefaultTooltip(surface, request);
	};
	return true;
}

void TooltipSystem::handleAdded(Node *owner) {
	System::handleAdded(owner);

	_hoverListener = Rc<InputListener>::create(HoverListenerPriority);
	_hoverListener->addMoveRecognizer([this](const GestureData &data) {
		if (data.input) {
			_pointer = data.input->currentLocation;
			_hasPointer = true;
			resolveHover(_pointer, true);
		}
		// Never consumed: this listener only watches and must not take a MouseMove from others.
		return false;
	});

	updateHoverListener();
	updateDismissListener();

	// The tick must exist before the pointer stops, so a hint appears in an otherwise idle scene.
	scheduleUpdate();
}

void TooltipSystem::handleRemoved() {
	cancelDelay();
	hide();

	for (auto listener : {&_hoverListener, &_dismissListener}) {
		if (*listener) {
			if (_owner) {
				_owner->removeSystem(listener->get());
			}
			*listener = nullptr;
		}
	}

	_hovered = nullptr;
	System::handleRemoved();
}

void TooltipSystem::handleEnter(Scene *scene) {
	System::handleEnter(scene);

	// Node::handleEnter sets `_running` after its children entered, and acquireForNode may run from
	// a descendant's handleEnter; a system added then never gets handleEnter (Node::addSystemItem),
	// and an InputListener that never entered refuses events. So attachment is retried here.
	updateHoverListener();
	updateDismissListener();
}

void TooltipSystem::handleVisitBegin(FrameInfo &info) {
	System::handleVisitBegin(info);

	// The last, always-working chance to attach the listeners; see handleEnter.
	updateHoverListener();
	updateDismissListener();
}

void TooltipSystem::updateHoverListener() {
	if (_owner && _hoverListener && !_hoverListener->getOwner() && _owner->isRunning()) {
		_owner->addSystem(_hoverListener);
	}
}

void TooltipSystem::update(const UpdateTime &time) {
	System::update(time);

	if (!_hasPointer) {
		return;
	}

	auto dispatcher = getDispatcher();
	if (!dispatcher) {
		return;
	}

	// A scene with no hint pays one flag test per frame.
	if (!hasFlag(dispatcher->getHitTestMask(), HitTestFlags::Tooltip)) {
		return;
	}

	// The pointer has not moved (no dwell restart), but what is under it may have.
	resolveHover(_pointer, false);
}

InputDispatcher *TooltipSystem::getDispatcher() const {
	auto owner = getOwner();
	auto director = owner ? owner->getDirector() : nullptr;
	return director ? director->getInputDispatcher() : nullptr;
}

void TooltipSystem::handleExit() {
	// An overlay hint lives in the scene being torn down; take it down with it.
	cancelDelay();
	hide();
	System::handleExit();
}

void TooltipSystem::setConfig(const TooltipConfig &config) {
	_config = config;
	updateDismissListener();
}

void TooltipSystem::setHoverDelay(TimeInterval value) { _config.hoverDelay = value; }

void TooltipSystem::setPlacement(const TooltipPlacement &value) { _config.placement = value; }

void TooltipSystem::setMode(TooltipMode value) { _config.mode = value; }

void TooltipSystem::setDefaultFactory(TooltipFactory &&factory) {
	if (factory) {
		_defaultFactory = sp::move(factory);
	} else {
		_defaultFactory = [](NotNull<SubWindow> surface, const TooltipRequest &request) {
			return buildDefaultTooltip(surface, request);
		};
	}
}

bool TooltipSystem::isVisible() const { return _tip && _tip->isOpen(); }

core::RenderServerChannel *TooltipSystem::getWindow() const {
	return getSubWindowParent(getOwner());
}

SubWindowSession *TooltipSystem::getSession() const {
	auto window = getWindow();
	return window ? SubWindowSession::get(window) : nullptr;
}

void TooltipSystem::updateDismissListener() {
	auto owner = getOwner();
	if (!owner) {
		return;
	}

	if (!_config.hideOnInput) {
		if (_dismissListener) {
			owner->removeSystem(_dismissListener.get());
			_dismissListener = nullptr;
		}
		return;
	}

	if (_dismissListener) {
		return;
	}

	// Post-scene band: widgets under the pointer already had the event. Swallows nothing; it only
	// notices user input.
	_dismissListener = owner->addSystem(Rc<InputListener>::create(DismissListenerPriority));
	_dismissListener->addTouchRecognizer([this](const GestureData &data) {
		if (data.event == GestureEvent::Began) {
			cancelDelay();
			hide();
		}
		return false;
	});

	InputKeyMask allKeys;
	allKeys.set();
	_dismissListener->addKeyRecognizer([this](const GestureData &data) {
		if (data.event == GestureEvent::Began) {
			cancelDelay();
			hide();
		}
		return false;
	}, InputKeyInfo(sp::move(allKeys)));
}

// --- hover state machine -----------------------------------------------------------------------

void TooltipSystem::resolveHover(const Vec2 &pointerWorld, bool fromMove) {
	auto dispatcher = getDispatcher();
	if (!dispatcher) {
		return;
	}

	// A pointer outside the window rests on nothing; without this gate (WindowState::Pointer) a
	// window the pointer left would keep its last hover.
	if (!hasFlag(dispatcher->getWindowState(), WindowState::Pointer)) {
		if (_hovered) {
			auto prev = sp::move(_hovered);
			_hovered = nullptr;
			handleTargetLeave(prev);
		}
		return;
	}

	Node *found = nullptr;
	dispatcher->foreachHitTest(HitTestFlags::Tooltip,
			[&](const InputListenerStorage::HitTestRec &rec) {
		auto comp = getTooltip(rec.node);
		if (!comp || !comp->enabled) {
			return true;
		}
		// The hover padding is per node, so the registry hands over records rather than answers.
		if (!rec.contains(pointerWorld, comp->info.hoverPadding)) {
			return true;
		}
		found = rec.node;
		return false;
	});

	if (found == _hovered) {
		// Same node. A real move restarts the dwell; a per-frame re-resolution must not, or the
		// delay would be rearmed forever.
		if (found && fromMove) {
			handleTargetHover(found, pointerWorld);
		}
		return;
	}

	if (_hovered) {
		auto prev = sp::move(_hovered);
		_hovered = nullptr;
		handleTargetLeave(prev);
	}

	_hovered = found;
	if (found) {
		handleTargetHover(found, pointerWorld);
	}
}

void TooltipSystem::handleTargetHover(NotNull<Node> target, Vec2 pointerWorld) {
	_pointer = pointerWorld;

	if (_shown == target.get()) {
		// Already up for this target: refresh the hide timer instead of restarting the dwell, which
		// would rebuild the hint on every movement.
		if (auto *session = getSession()) {
			session->refreshTip(_config.hideDelay);
		}
		return;
	}

	_pending = target;
	armDelay();
}

void TooltipSystem::handleTargetLeave(NotNull<Node> target) {
	if (_pending == target.get()) {
		cancelDelay();
	}

	if (_shown != target.get()) {
		return;
	}

	// Under Native the leave is false (the tip window took the pointer); the hide timer closes it.
	if (_config.hideOnLeave && _config.mode != TooltipMode::Native) {
		hide();
	}
}

void TooltipSystem::handleTargetGone(NotNull<Node> target) {
	if (_pending == target.get()) {
		cancelDelay();
	}
	if (_shown == target.get()) {
		hide();
	}
}

void TooltipSystem::handleNodeChanged(NotNull<Node> target) {
	if (_shown != target.get()) {
		return;
	}

	// Rebuild in place. Hover is taken from the resolution, not from the node.
	auto comp = getTooltip(target);
	if (_hovered == target.get() && comp && comp->enabled) {
		present(target, _pointer);
	} else {
		hide();
	}
}

void TooltipSystem::armDelay() {
	auto owner = getOwner();
	if (!owner || !_pending) {
		return;
	}

	owner->stopAllActionsByTag(DelayActionTag);

	if (!_config.hoverDelay) {
		fire();
		return;
	}

	// Rc, not `this`: the ActionManager holds the action, and the system may be removed from its
	// owner while it runs.
	owner->runAction(Rc<Sequence>::create(_config.hoverDelay,
							 [self = Rc<TooltipSystem>(this)] { self->fire(); }),
			DelayActionTag);
}

void TooltipSystem::cancelDelay() {
	_pending = nullptr;
	if (auto owner = getOwner()) {
		owner->stopAllActionsByTag(DelayActionTag);
	}
}

void TooltipSystem::fire() {
	auto target = sp::move(_pending);
	_pending = nullptr;

	// Still under the pointer and in the scene: a list can scroll a row away during the dwell.
	if (!target || _hovered != target || !target->isRunning()) {
		return;
	}

	present(target, _pointer);
}

bool TooltipSystem::showFor(NotNull<Node> target, Vec2 pointerWorld) {
	cancelDelay();
	_pointer = pointerWorld;
	return present(target, pointerWorld);
}

bool TooltipSystem::present(NotNull<Node> node, Vec2 pointerWorld) {
	auto session = getSession();
	auto comp = getTooltip(node);
	if (!session || !comp) {
		return false;
	}

	const auto &info = comp->info;
	const auto &placement = placementFor(node);

	auto size = info.size;
	if (size == Extent2::ZERO) {
		size = measureDefaultTooltip(info.text, _config);
	} else {
		size = sprt::window::clampWindowExtent(size, _config.minExtent, _config.maxExtent);
	}

	TooltipRequest request;
	request.target = node;
	request.info = &info;
	request.text = info.text;
	request.data = &info.data;
	request.nodeWorldRect = getTargetWorldRect(node);
	request.pointer = pointerWorld;
	request.size = size;

	auto factory = info.factory ? info.factory : _defaultFactory;
	if (!factory) {
		return false;
	}

	SubWindow::Config config;
	config.placement = makePlacement(request, placement);
	config.size = size;
	config.minExtent = _config.minExtent;
	config.maxExtent = _config.maxExtent;
	config.flags = _config.flags;
	config.title = _config.title;
	config.idPrefix = _config.idPrefix;
	config.preferNative = _config.mode == TooltipMode::Native;

	/* The closure owns everything the builder reads, the node by Rc and the info by copy: on the
	native path the builder runs when the subwindow's scene is presented, possibly after the widget
	and its TooltipComponent are gone. `req.info` points into the copy. */
	config.content = [factory = sp::move(factory), source = Rc<Node>(node.get()), info = info,
							 rect = request.nodeWorldRect, pointer = pointerWorld,
							 size](NotNull<SubWindow> surface) mutable {
		TooltipRequest req;
		req.target = source;
		req.info = &info;
		req.text = info.text;
		req.data = &info.data;
		req.nodeWorldRect = rect;
		req.pointer = pointer;
		req.size = size;
		return factory(surface, req);
	};

	auto hideDelay = _config.hideDelay;
	if (config.preferNative && !hideDelay) {
		// A native tip cannot rely on a leave, so a zero hide delay is replaced; see
		// TooltipConfig::hideOnLeave.
		hideDelay = SubWindowSession::DefaultHideDelay;
	}

	// Keyed by node identity and text: the same node refreshes, a different one replaces.
	auto key = toString(reinterpret_cast<uintptr_t>(node.get()), "-", info.text);

	_tip = session->showTip(sp::move(config), key, hideDelay);
	_shown = _tip ? node.get() : nullptr;
	return _shown != nullptr;
}

void TooltipSystem::hide() {
	_shown = nullptr;
	if (auto tip = sp::move(_tip)) {
		_tip = nullptr;
		tip->dismiss();
	}
}

const TooltipPlacement &TooltipSystem::placementFor(NotNull<Node> node) const {
	auto comp = getTooltip(node);
	const auto &own = comp ? comp->info.placement : sprt::optional<TooltipPlacement>();
	return own ? *own : _config.placement;
}

Rect TooltipSystem::getTargetWorldRect(NotNull<Node> node) const {

	// Four corners, not origin+size: the node may be rotated or scaled. Scene space (physical
	// pixels, see TooltipRequest::nodeWorldRect); placement uses ui::placementAnchorRect instead.
	const auto size = node->getContentSize();
	const Vec2 corners[4] = {
		node->convertToWorldSpace(Vec2::ZERO),
		node->convertToWorldSpace(Vec2(size.width, 0.0f)),
		node->convertToWorldSpace(Vec2(size.width, size.height)),
		node->convertToWorldSpace(Vec2(0.0f, size.height)),
	};

	Vec2 min = corners[0];
	Vec2 max = corners[0];
	for (size_t i = 1; i < 4; ++i) {
		min.x = sprt::min(min.x, corners[i].x);
		min.y = sprt::min(min.y, corners[i].y);
		max.x = sprt::max(max.x, corners[i].x);
		max.y = sprt::max(max.y, corners[i].y);
	}

	return Rect(min.x, min.y, max.x - min.x, max.y - min.y);
}

sprt::window::WindowPlacement TooltipSystem::makePlacement(const TooltipRequest &request,
		const TooltipPlacement &placement) const {
	sprt::window::WindowPlacement ret;
	ret.anchor = placement.anchor;
	ret.gravity = placement.gravity;
	ret.offset = placement.offset;
	ret.adjustment = placement.adjustment;

	Node *inScene = request.target ? request.target : getOwner();
	if (!inScene) {
		return ret;
	}

	/* The anchor box comes from ui::placementAnchorRect, not `request.nodeWorldRect`: that rect is
	in scene space (physical pixels), while WindowPlacement is in the window's logical points. */
	if (placement.anchorMode == TooltipAnchorMode::Pointer) {
		ret.anchorRect = placementAnchorPoint(inScene, request.pointer);
	} else {
		ret.anchorRect = placementAnchorRect(inScene);
	}

	return ret;
}

} // namespace stappler::xenolith::ui
