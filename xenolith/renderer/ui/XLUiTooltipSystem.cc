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

// The stock hint's metrics. Deliberately not stylesheet-driven defaults: a scene with no
// StyleSystem (tests/auxui is one) still has to get a readable hint. A scene that HAS one styles
// the hint on either path - ui::openPopupSurface shares the sheet with a native surface too - so
// these are the floor, not the look.
static constexpr float kTipHeight = 34.0f;
static constexpr float kTipFontSize = 13.0f;
static constexpr float kTipPadding = 12.0f;
static constexpr float kTipMinWidth = 120.0f;

Extent2 TooltipSystem::measureDefaultTooltip(StringView text, const TooltipConfig &config) {
	// Rough advance-width estimate: the hint is built before it is measured, and the exact metrics
	// would need a font query for a box that is clamped anyway.
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

	// "aux-tip" is the stock hint's name, and what tools and tests look a tip up by. A factory of
	// your own may name its root something else - openOverlay only supplies this name when the
	// builder left none.
	layout->setName("aux-tip");

	auto bg = layout->addChild(Rc<Panel>::create());
	bg->setAnchorPoint(Anchor::BottomLeft);
	bg->setPosition(Vec2::ZERO);
	bg->setContentSize(size);
	// Typed and classed so a stylesheet can take it over; coloured here so one that never arrives
	// is not a black-on-black hint.
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

/* Make sure the scene has a coordinator, whenever that becomes possible.

A hint is routinely declared while a widget is being BUILT - `label->setString(...); setTooltip(label,
...)`, long before anything is added to a scene - and a component, unlike the listener this replaced,
cannot notice its own arrival there later. So when there is no scene yet the acquire is deferred to
one, with a one-shot system that removes itself the moment it has done its job. It is the only
per-node object left in this design, it exists for at most one scene entry, and nothing carries it
afterwards. */
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

	/* On the first VISIT, not on entering the scene.

	Node::handleEnter sets `_running` at its very end, after its children have entered, so a system
	added to the content node from inside a descendant's entry is never handed handleEnter - it
	would never run, never schedule its update tick and never attach its listener. By the first
	visit everything above is running and an ordinary addSystem does the right thing. */
	anchor->setVisitSelfCallback([](CallbackSystem *self, FrameInfo &, Node *, NodeVisitFlags) {
		auto owner = self->getOwner();
		if (!owner) {
			return;
		}
		TooltipSystem::acquireForNode(owner);

		// Node::visitSelf iterates a COPY of the system list, so removing ourselves from inside it
		// is safe - and the copy is what keeps this object alive until the loop is done
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

	// The flag and the component are one declaration: the visit reads the flag, the hover
	// resolution reads the component
	node->addHitTestFlags(HitTestFlags::Tooltip);

	Tooltip_acquireSystem(node);

	// A hint that is already up and describes this node is describing something that just changed
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

	// Nobody installed one. Put it where it belongs rather than making every widget demand that the
	// application arrange a tooltip system before it can carry a hint.
	if (node) {
		if (auto scene = node->getScene()) {
			if (auto content = scene->getContent()) {
				return content->addSystem(Rc<TooltipSystem>::create());
			}
		}
	}

	// Quiet: a node with no scene is the ordinary case for a hint declared while a widget is being
	// built, and Tooltip_acquireSystem retries on entry
	return nullptr;
}

bool TooltipSystem::init() {
	if (!System::init()) {
		return false;
	}

	_frameTag = TooltipSystem::Id;

	// Owner and scene events for the lifetime, and visit control for one thing only: the visit is
	// the last chance to attach the listeners (see handleVisitBegin). The update tick is scheduled
	// separately - it is what notices a node sliding out from under a still pointer, which used to
	// come from each target's own geometry updates.
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
		// Never consumed: this listener decides nothing and must not take a MouseMove away from
		// anything that does
		return false;
	});

	updateHoverListener();
	updateDismissListener();

	// A hint has to appear in a scene where nothing else is happening, so the tick has to exist
	// before the pointer stops moving
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

	// Node::handleEnter sets `_running` at its very end, after its children have entered - and
	// acquireForNode is reached from a descendant's handleEnter, inside that window. A system added
	// to a node that is not running yet is never handed handleEnter (see Node::addSystemItem), and
	// an InputListener that never entered refuses every event. So the attachment is retried here,
	// by which time everything above is certainly running. Same fix as ContextMenuSystem's.
	updateHoverListener();
	updateDismissListener();
}

void TooltipSystem::handleVisitBegin(FrameInfo &info) {
	System::handleVisitBegin(info);

	// The last chance for the listeners to join, and the one that always works.
	// Node::handleEnter sets `_running` at its very end, after its children have entered - and
	// acquireForNode is reached from a descendant's entry, which is inside that window. A system
	// added to a node that is not running yet is never handed handleEnter (see Node::addSystemItem),
	// and an InputListener that never entered refuses every event. By the first visit everything
	// above is certainly running. Same fix as ContextMenuSystem's.
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

	// A scene with no hint in it pays one flag test per frame and nothing else
	if (!hasFlag(dispatcher->getHitTestMask(), HitTestFlags::Tooltip)) {
		return;
	}

	// The pointer has not moved - so no dwell is restarted - but what is UNDER it may have
	resolveHover(_pointer, false);
}

InputDispatcher *TooltipSystem::getDispatcher() const {
	auto owner = getOwner();
	auto director = owner ? owner->getDirector() : nullptr;
	return director ? director->getInputDispatcher() : nullptr;
}

void TooltipSystem::handleExit() {
	// The scene is being torn down and an overlay hint lives in it. Take it with us rather than
	// leaving a node parented to a content node on its way out.
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

AppWindow *TooltipSystem::getWindow() const {
	auto owner = getOwner();
	auto scene = owner ? owner->getScene() : nullptr;
	auto director = scene ? scene->getDirector() : nullptr;
	auto server = director ? director->getRenderServer() : nullptr;
	return server ? dynamic_cast<AppWindow *>(server) : nullptr;
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

	// Post-scene band: every widget under the pointer has already had the event, and this swallows
	// nothing - it only notices that the user did something.
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

	/* A pointer that is not in the window is not resting on anything.

	The old per-node listeners got this from GestureMouseOverRecognizer, which gates on
	WindowState::Pointer; asked centrally it has to be gated here, or a window the pointer has left
	would keep whatever it was last over. */
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
		// The hover padding is the NODE's, which is why the registry hands over records rather than
		// answers: a thin target is hard to rest a pointer on, and how thin is its own business
		if (!rec.contains(pointerWorld, comp->info.hoverPadding)) {
			return true;
		}
		found = rec.node;
		return false;
	});

	if (found == _hovered) {
		// Same node. A real movement restarts the dwell; a re-resolution on a frame where nothing
		// moved must not, or the delay would be rearmed forever and the hint would never appear
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
		// Already up for this target. Restarting the dwell here would tear the hint down and
		// rebuild it on every pixel of movement; refresh the hide timer instead, which is what the
		// session does for a repeated tip.
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

	// Under Native the leave is not to be trusted: the tip window took the pointer off the parent,
	// so the target reports a leave it never had. The hide timer closes it there.
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

	// Rebuild in place: the hint is describing something that just changed under it. Still hovered
	// is asked of the resolution rather than of the node - "is the pointer on me" is not a fact a
	// node carries any more
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

	// Rc, not `this`: the action outlives nothing here, but the ActionManager holds it and the
	// system could be removed from its owner while it runs.
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

	// Still the node the pointer is on, and still in the scene: the dwell is half a second, and a
	// list can scroll a row out from under a still pointer in that time
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

	/* Everything the builder reads is OWNED by the closure, the node included, by Rc.

	On the native path the builder does not run until the subwindow's scene is presented, by which
	time the widget that asked for the hint may be long gone - and its TooltipComponent with it, so
	a TooltipRequest pointing into that component would be reading freed memory. The info is COPIED
	here for exactly that reason; `req.info` then points into the copy, which lives as long as the
	closure. Same reason DragSession holds its source by Rc. */
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
		// A native tip cannot rely on a leave, so a zero hide delay would be a hint that never goes
		// away. See TooltipConfig::hideOnLeave.
		hideDelay = SubWindowSession::DefaultHideDelay;
	}

	// The node's identity keys the slot: re-presenting the same node refreshes rather than flaps,
	// and a different node replaces.
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

	// Four corners, not origin+size: the node may be rotated or scaled, and the anchor rect the
	// placement wants is the axis-aligned box it actually occupies.
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
	auto owner = getOwner();
	const float sceneHeight = owner ? owner->getContentSize().height : 0.0f;

	// Scene nodes are Y-up; WindowPlacement is Y-down from the parent's content top-left. Flipping
	// the rect swaps which edge is "top", so the anchor rect is built from the flipped extremes
	// rather than by flipping its origin.
	Rect rect;
	if (placement.anchorMode == TooltipAnchorMode::Pointer) {
		rect = Rect(request.pointer.x, request.pointer.y, 0.0f, 0.0f);
	} else {
		rect = request.nodeWorldRect;
	}

	const float topYDown = sceneHeight - (rect.origin.y + rect.size.height);

	sprt::window::WindowPlacement ret;
	ret.anchorRect = IRect(int32_t(std::lround(rect.origin.x)), int32_t(std::lround(topYDown)),
			uint32_t(std::lround(rect.size.width)), uint32_t(std::lround(rect.size.height)));
	ret.anchor = placement.anchor;
	ret.gravity = placement.gravity;
	ret.offset = placement.offset;
	ret.adjustment = placement.adjustment;
	return ret;
}

} // namespace stappler::xenolith::ui
