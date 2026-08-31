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

#include "XLCommon.h"

#include "drag/DragActionsLayout.h"
#include "XLDragSource.h"
#include "XLDropTarget.h"
#include "XLWindowInfo.h"
#include "XL2dLayer.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

static constexpr auto AnyTarget = Rect(60.0f, 60.0f, 180.0f, 120.0f);
static constexpr auto CopyOnly = Rect(280.0f, 60.0f, 180.0f, 120.0f);
static constexpr auto MoveOnly = Rect(500.0f, 60.0f, 180.0f, 120.0f);
static constexpr auto Refusing = Rect(60.0f, 220.0f, 180.0f, 120.0f);
static constexpr auto Handle = Rect(500.0f, 260.0f, 120.0f, 60.0f);

static StringView actionName(DragActions action) {
	switch (action) {
	case DragActions::None: return StringView("none");
	case DragActions::Copy: return StringView("copy");
	case DragActions::Move: return StringView("move");
	case DragActions::Link: return StringView("link");
	default: break;
	}
	return StringView("mixed");
}

} // namespace

bool DragActionsLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	addTarget("any", AnyTarget, DragActions::All);
	addTarget("copy-only", CopyOnly, DragActions::Copy);
	addTarget("move-only", MoveOnly, DragActions::Move);
	addTarget("refusing", Refusing, DragActions::None);

	// The real input path. Everything the automated phases below drive through the public API,
	// this one node reaches through a swipe: threshold, pointer capture, modifiers off the event
	_handle = addChild(Rc<basic2d::Layer>::create(Color::Amber_600), ZOrder(1));
	_handle->setName("handle");
	_handle->setAnchorPoint(Anchor::BottomLeft);
	_handle->setPosition(Handle.origin);
	_handle->setContentSize(Handle.size);
	_handle->addSystem(Rc<DragSource>::create([this](DragOffer &offer) {
		offer.local = _handle;
		offer.localType = String("test/handle");
		offer.label = String("handle");
		offer.allowedActions = DragActions::All;
		offer.defaultAction = DragActions::Move;
		offer.decorator = []() -> Rc<Node> {
			auto node = Rc<basic2d::Layer>::create(Color::Amber_300);
			node->setAnchorPoint(Anchor::Middle);
			node->setContentSize(Size2(60.0f, 30.0f));
			return node;
		};
		offer.completion = [this](DragActions action) {
			++_completions;
			_lastCompletion = action;
		};
		return true;
	}));

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.6f), [this] { runPhase1(); },
			Rc<DelayTime>::create(0.3f), [this] { runPhase2(); }, Rc<DelayTime>::create(0.3f),
			[this] { runPhase3(); }, Rc<DelayTime>::create(0.3f), [this] { runPhase4(); }));
	return true;
}

void DragActionsLayout::handleEnter(Scene *scene) {
	TestLayout::handleEnter(scene);
	_drag = DragSystem::acquireForNode(this);
}

void DragActionsLayout::handleExit() {
	// The system lives on the scene CONTENT, not on this layout, so a session started here outlives
	// the layout that started it: switch away mid-sequence and the next stand finds a drag already
	// in flight, with the cursor still Grabbing. A programmatic drag has no input chain to end it
	// either - the one that watches a press chain does not apply - so ending it is this stand's own
	// job, exactly as committing it is
	if (_drag && _drag->isDragging()) {
		_drag->cancelDrag();
	}
	_drag = nullptr;

	TestLayout::handleExit();
}

Node *DragActionsLayout::addTarget(StringView name, const Rect &rect, DragActions accepted) {
	auto node = addChild(Rc<basic2d::Layer>::create(Color::Teal_700), ZOrder(1));
	node->setName(name);
	node->setAnchorPoint(Anchor::BottomLeft);
	node->setPosition(rect.origin);
	node->setContentSize(rect.size);

	setDropTarget(node,
			DropTargetSlots{
				.accept = [accepted](const DragEvent &event) -> DragResponse {
		// the canonical shape of an accept slot: what I can do, intersected with what the
		// source offers. Never a bare constant, or the target claims actions the source
		// never had
		return DragResponse{event.allowed & accepted};
	},
				.drop = [](const DragEvent &, DragActions) { return true; },
			});

	return node;
}

Vec2 DragActionsLayout::world(const Rect &rect) const {
	return const_cast<DragActionsLayout *>(this)->convertToWorldSpace(
			Vec2(rect.getMidX(), rect.getMidY()));
}

bool DragActionsLayout::beginDrag(DragActions allowed, DragActions dflt) {
	DragOffer offer;
	offer.local = this;
	offer.localType = String("test/thing");
	offer.allowedActions = allowed;
	offer.defaultAction = dflt;
	offer.completion = [this](DragActions action) {
		++_completions;
		_lastCompletion = action;
	};
	return _drag->beginDrag(sp::move(offer), Rc<Ref>(this)) != nullptr;
}

WindowCursor DragActionsLayout::currentCursor() const {
	auto listener = _drag ? _drag->getCursorListener() : nullptr;
	if (!listener || !listener->isEnabled()) {
		return WindowCursor::Undefined;
	}
	return listener->getCursor();
}

void DragActionsLayout::expect(bool cond, StringView phase, StringView what) {
	++_checks;
	if (!cond) {
		++_failures;
		log::source().error("DragActionsTest", phase, ": ", what);
	}
}

void DragActionsLayout::expectAction(StringView phase, StringView what, DragActions actual,
		DragActions expected) {
	++_checks;
	if (actual != expected) {
		++_failures;
		log::source().error("DragActionsTest", phase, ": ", what, " is ", actionName(actual),
				", expected ", actionName(expected));
	}
}

void DragActionsLayout::expectCursor(StringView phase, StringView what, WindowCursor expected) {
	++_checks;
	auto actual = currentCursor();
	if (actual != expected) {
		++_failures;
		log::source().error("DragActionsTest", phase, ": ", what, " is ",
				getWindowCursorName(actual), ", expected ", getWindowCursorName(expected));
	}
}

void DragActionsLayout::runPhase1() {
	expect(_drag != nullptr, "phase1", "no drag system was acquired");
	if (!_drag) {
		return;
	}

	// idle: the cursor layer is not registered at all, so no widget's cursor is overridden
	expectCursor("phase1", "the idle cursor", WindowCursor::Undefined);

	expect(beginDrag(DragActions::All, DragActions::Move), "phase1", "the drag did not start");

	auto session = _drag->getSession();
	if (!session) {
		return;
	}

	auto pos = world(AnyTarget);

	_drag->updateDrag(pos, InputModifier::None);
	expectAction("phase1", "the default action", session->getResolvedAction(), DragActions::Move);
	expectCursor("phase1", "the cursor over a Move", WindowCursor::Move);

	_drag->updateDrag(pos, InputModifier::Ctrl);
	expectAction("phase1", "Ctrl", session->getResolvedAction(), DragActions::Copy);
	expectCursor("phase1", "the cursor over a Copy", WindowCursor::Copy);

	_drag->updateDrag(pos, InputModifier::Shift);
	expectAction("phase1", "Shift", session->getResolvedAction(), DragActions::Move);

	_drag->updateDrag(pos, InputModifier::Ctrl | InputModifier::Shift);
	expectAction("phase1", "Ctrl+Shift", session->getResolvedAction(), DragActions::Link);
	expectCursor("phase1", "the cursor over a Link", WindowCursor::Alias);
}

void DragActionsLayout::runPhase2() {
	if (!_drag) {
		return;
	}

	auto session = _drag->getSession();
	if (!session) {
		expect(false, "phase2", "the drag from phase 1 is gone");
		return;
	}

	// A target that can only Copy, with the user asking for Move. The preference loses and the
	// drop still happens - as a Copy
	_drag->updateDrag(world(CopyOnly), InputModifier::Shift);
	expectAction("phase2", "the preferred action", session->getPreferredAction(),
			DragActions::Move);
	expectAction("phase2", "what a Copy-only target resolves to", session->getResolvedAction(),
			DragActions::Copy);
	expectCursor("phase2", "the cursor over a forced Copy", WindowCursor::Copy);

	// and symmetrically
	_drag->updateDrag(world(MoveOnly), InputModifier::Ctrl);
	expectAction("phase2", "what a Move-only target resolves to", session->getResolvedAction(),
			DragActions::Move);

	// a target that accepts nothing is not a target: no action, and the cursor says so
	_drag->updateDrag(world(Refusing), InputModifier::None);
	expect(session->getTarget() == nullptr, "phase2", "a target accepting nothing became current");
	expectAction("phase2", "the action over a refusing target", session->getResolvedAction(),
			DragActions::None);
	expectCursor("phase2", "the cursor with nowhere to drop", WindowCursor::Grabbing);

	// dropping where nothing is accepted is not a drop, and the source is told so
	_drag->commitDrag();
	expect(_completions == 1, "phase2", "the completion did not run exactly once");
	expectAction("phase2", "what a drop on nothing reports", _lastCompletion, DragActions::None);
	expectCursor("phase2", "the cursor after the drag", WindowCursor::Undefined);
}

void DragActionsLayout::runPhase3() {
	if (!_drag) {
		return;
	}

	// A source that can only Move. Ctrl asks for Copy, which this source cannot do - the drag
	// must fall back to what IS offered rather than refusing outright, or a stray modifier would
	// silently break it
	expect(beginDrag(DragActions::Move, DragActions::Move), "phase3", "the drag did not start");

	auto session = _drag->getSession();
	if (!session) {
		return;
	}

	_drag->updateDrag(world(AnyTarget), InputModifier::Ctrl);
	expectAction("phase3", "Ctrl on a Move-only source", session->getPreferredAction(),
			DragActions::Move);
	expectAction("phase3", "the resolved action", session->getResolvedAction(), DragActions::Move);

	// this source cannot Copy, so a Copy-only target has nothing in common with it
	_drag->updateDrag(world(CopyOnly), InputModifier::None);
	expect(session->getTarget() == nullptr, "phase3",
			"a Copy-only target accepted a Move-only source");

	_drag->updateDrag(world(MoveOnly), InputModifier::None);
	_drag->commitDrag();
	expect(_completions == 2, "phase3", "the completion did not run");
	expectAction("phase3", "the reported action", _lastCompletion, DragActions::Move);
}

void DragActionsLayout::runPhase4() {
	if (!_drag) {
		return;
	}

	// A source offering nothing cannot start a drag at all
	expect(!beginDrag(DragActions::None, DragActions::None), "phase4",
			"a drag with no allowed action started anyway");
	expect(!_drag->isDragging(), "phase4", "a refused drag left a session behind");

	// and one already in flight refuses a second
	expect(beginDrag(DragActions::All, DragActions::Copy), "phase4", "the drag did not start");
	expect(!beginDrag(DragActions::All, DragActions::Copy), "phase4",
			"a second concurrent drag started");
	_drag->cancelDrag();

	log::source().warn("DragActionsTest", "SUMMARY: ", _checks, " checks, ", _failures,
			" failures");
}

void DragActionsLayout::registerCommands() {
	TestLayout::registerCommands();

	// World-space centres, so a synthetic-input driver does not have to reconstruct the layout's
	// transform to aim at anything
	addCommand("points", "World-space centres of the handle and the targets", [this](Value &&) {
		Value result;
		auto put = [&](StringView name, const Rect &rect) {
			auto point = world(rect);
			Value value;
			value.setDouble(point.x, "x");
			value.setDouble(point.y, "y");
			result.setValue(sp::move(value), name);
		};
		put("handle", Handle);
		put("any", AnyTarget);
		put("copy-only", CopyOnly);
		put("move-only", MoveOnly);
		put("refusing", Refusing);
		return result;
	});

	addCommand("state", "Report the live drag: target, actions, cursor", [this](Value &&) {
		Value result;
		auto session = _drag ? _drag->getSession() : nullptr;
		result.setBool(session != nullptr, "dragging");
		result.setString(getWindowCursorName(currentCursor()), "cursor");
		if (session) {
			result.setString(actionName(session->getPreferredAction()), "preferred");
			result.setString(actionName(session->getResolvedAction()), "resolved");
			auto target = session->getTarget();
			result.setString(target ? target->getName() : StringView(), "target");
		}
		result.setInteger(int64_t(_completions), "completions");
		result.setString(actionName(_lastCompletion), "last");
		return result;
	});
}

} // namespace stappler::xenolith::app
