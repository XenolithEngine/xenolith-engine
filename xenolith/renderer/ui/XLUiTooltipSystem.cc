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
#include "XLScene.h"

#include <cmath>

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

uint64_t TooltipSystem::Id = System::GetNextSystemId();

// The stock hint's metrics. Deliberately not stylesheet-driven defaults: a scene with no
// StyleSystem (tests/auxui is one) still has to get a readable hint, and on the native path the
// hint is a scene of its own that the parent window's sheet does not reach.
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

// --- TooltipTarget ---------------------------------------------------------------------------

bool TooltipTarget::init(TooltipInfo &&info) {
	if (!InputListener::init()) {
		return false;
	}
	_info = sp::move(info);
	return setup();
}

bool TooltipTarget::init(StringView text) {
	if (!InputListener::init()) {
		return false;
	}
	_info.text = text.str<Interface>();
	return setup();
}

bool TooltipTarget::setup() {
	// onlyFocused = false is required, not a preference: a hint inside a Popup would never appear
	// otherwise, because a popup window never carries WindowState::Focused.
	addMouseOverRecognizer([this](const GestureData &data) {
		switch (data.event) {
		case GestureEvent::Began:
			_hovered = true;
			if (auto *system = acquireSystem()) {
				system->handleTargetHover(this, data.location());
			}
			break;
		case GestureEvent::Moved:
			// Every move restarts the dwell - this is what makes the delay "the pointer stopped"
			// rather than "the pointer arrived".
			if (_hovered) {
				if (auto *system = acquireSystem()) {
					system->handleTargetHover(this, data.location());
				}
			}
			break;
		case GestureEvent::Ended:
		case GestureEvent::Cancelled:
			_hovered = false;
			if (_system) {
				_system->handleTargetLeave(this);
			}
			break;
		default: break;
		}
		return true;
	}, InputMouseOverInfo{_info.hoverPadding, false});

	return true;
}

void TooltipTarget::handleExit() {
	_hovered = false;
	if (_system) {
		// Tell it before we go: a dwell running for a node that is leaving the scene would fire on
		// a target with no owner.
		_system->handleTargetGone(this);
		_system = nullptr;
	}
	InputListener::handleExit();
}

TooltipSystem *TooltipTarget::acquireSystem() {
	if (!_system) {
		_system = TooltipSystem::acquireForNode(getOwner());
	}
	return _system;
}

void TooltipTarget::setInfo(TooltipInfo &&info) {
	_info = sp::move(info);
	setTouchPadding(_info.hoverPadding);
	if (_system) {
		_system->handleTargetChanged(this);
	}
}

void TooltipTarget::setText(StringView text) {
	if (_info.text == text) {
		return;
	}
	_info.text = text.str<Interface>();
	if (_system) {
		_system->handleTargetChanged(this);
	}
}

void TooltipTarget::setFactory(TooltipFactory &&factory) {
	_info.factory = sp::move(factory);
	if (_system) {
		_system->handleTargetChanged(this);
	}
}

void TooltipTarget::setPlacement(const TooltipPlacement &placement) {
	_info.placement = placement;
	if (_system) {
		_system->handleTargetChanged(this);
	}
}

void TooltipTarget::clearPlacement() {
	_info.placement.reset();
	if (_system) {
		_system->handleTargetChanged(this);
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

	log::source().warn("TooltipSystem",
			"acquireForNode: the node is not in a scene with a content node");
	return nullptr;
}

bool TooltipSystem::init() {
	if (!System::init()) {
		return false;
	}

	_frameTag = TooltipSystem::Id;

	// Owner and scene events for the lifetime, and nothing else: no visit, no node events, no
	// update tick. A pointer at rest is reported to us by the targets, and the dwell action is what
	// keeps frames coming while it runs.
	_systemFlags = SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents;

	_defaultFactory = [](NotNull<SubWindow> surface, const TooltipRequest &request) {
		return buildDefaultTooltip(surface, request);
	};
	return true;
}

void TooltipSystem::handleAdded(Node *owner) {
	System::handleAdded(owner);
	updateDismissListener();
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

void TooltipSystem::handleTargetHover(NotNull<TooltipTarget> target, Vec2 pointerWorld) {
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

void TooltipSystem::handleTargetLeave(NotNull<TooltipTarget> target) {
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

void TooltipSystem::handleTargetGone(NotNull<TooltipTarget> target) {
	if (_pending == target.get()) {
		cancelDelay();
	}
	if (_shown == target.get()) {
		hide();
	}
}

void TooltipSystem::handleTargetChanged(NotNull<TooltipTarget> target) {
	if (_shown != target.get()) {
		return;
	}

	// Rebuild in place: the hint is describing something that just changed under it.
	if (target->isHovered()) {
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
	auto *target = _pending;
	_pending = nullptr;

	if (!target || !target->isHovered() || !target->getOwner()) {
		return;
	}

	present(target, _pointer);
}

bool TooltipSystem::showFor(NotNull<TooltipTarget> target, Vec2 pointerWorld) {
	cancelDelay();
	_pointer = pointerWorld;
	return present(target, pointerWorld);
}

bool TooltipSystem::present(NotNull<TooltipTarget> target, Vec2 pointerWorld) {
	auto node = target->getOwner();
	auto session = getSession();
	if (!node || !session) {
		return false;
	}

	const auto &info = target->getInfo();
	const auto &placement = placementFor(target);

	auto size = info.size;
	if (size == Extent2::ZERO) {
		size = measureDefaultTooltip(info.text, _config);
	} else {
		size = sprt::window::clampWindowExtent(size, _config.minExtent, _config.maxExtent);
	}

	TooltipRequest request;
	request.target = node;
	request.source = target;
	request.text = info.text;
	request.data = &info.data;
	request.nodeWorldRect = getTargetWorldRect(target);
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

	// Everything the builder reads is OWNED by the closure, and the target is held by Rc. On the
	// native path the builder does not run until the subwindow's scene is presented, by which time
	// the widget that asked for the hint may be long gone - a TooltipRequest pointing into its
	// TooltipInfo would be reading freed memory. Same reason DragSession holds its source by Rc.
	config.content = [factory = sp::move(factory), source = Rc<TooltipTarget>(target),
							 text = info.text, data = info.data, rect = request.nodeWorldRect,
							 pointer = pointerWorld, size](NotNull<SubWindow> surface) mutable {
		TooltipRequest req;
		req.source = source;
		req.target = source->getOwner();
		req.text = text;
		req.data = &data;
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

	// The target's identity keys the slot: re-presenting the same target refreshes rather than
	// flaps, and a different target replaces.
	auto key = toString(reinterpret_cast<uintptr_t>(target.get()), "-", info.text);

	_tip = session->showTip(sp::move(config), key, hideDelay);
	_shown = _tip ? target.get() : nullptr;
	return _shown != nullptr;
}

void TooltipSystem::hide() {
	_shown = nullptr;
	if (auto tip = sp::move(_tip)) {
		_tip = nullptr;
		tip->dismiss();
	}
}

const TooltipPlacement &TooltipSystem::placementFor(NotNull<TooltipTarget> target) const {
	const auto &own = target->getPlacement();
	return own ? *own : _config.placement;
}

Rect TooltipSystem::getTargetWorldRect(NotNull<TooltipTarget> target) const {
	auto node = target->getOwner();
	if (!node) {
		return Rect::ZERO;
	}

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
