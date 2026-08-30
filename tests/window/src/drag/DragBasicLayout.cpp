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

#include "drag/DragBasicLayout.h"
#include "XLDropTarget.h"
#include "XLScene.h"
#include "XLSceneContent.h"
#include "XL2dLayer.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

// Two stacks and one invisible node. `Refusing` sits exactly on top of `B` so a refusal has to be
// transparent for B to ever receive anything; `High` sits inside `Low` so the topmost must win.
// The ZOrders are deliberately all distinct - sortAllChildren is unstable, and equal orders would
// make "topmost" a coin flip between frames.
static constexpr auto TargetA = Rect(60.0f, 60.0f, 200.0f, 100.0f);
static constexpr auto TargetB = Rect(320.0f, 60.0f, 200.0f, 100.0f);
static constexpr auto Refusing = Rect(320.0f, 60.0f, 200.0f, 100.0f);
static constexpr auto OverlapLow = Rect(60.0f, 220.0f, 200.0f, 120.0f);
static constexpr auto OverlapHigh = Rect(100.0f, 250.0f, 100.0f, 60.0f);
static constexpr auto Hidden = Rect(320.0f, 220.0f, 200.0f, 120.0f);

} // namespace

bool DragBasicLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	_targetA = addTarget("target-a", TargetA, ZOrder(1), &_a, true, [this] {
		if (_destroySource && _source) {
			// The drop destroys the source, which is what a Move normally does. The session holds
			// it by Rc precisely so this does not pull the ground out from under itself
			_sourceDestroyed = true;
			auto node = _source;
			_source = nullptr;
			node->removeFromParent(true);
		}
	});
	_targetB = addTarget("target-b", TargetB, ZOrder(1), &_b);
	_refusing = addTarget("refusing", Refusing, ZOrder(3), &_refused, false);
	_overlapLow = addTarget("overlap-low", OverlapLow, ZOrder(1), &_low);
	_overlapHigh = addTarget("overlap-high", OverlapHigh, ZOrder(5), &_high);

	_hidden = addTarget("hidden", Hidden, ZOrder(1), &_hiddenCounters);
	_hidden->setVisible(false);

	_source = addChild(Rc<basic2d::Layer>::create(Color::Red_500), ZOrder(0));
	_source->setName("source");
	_source->setAnchorPoint(Anchor::BottomLeft);
	_source->setPosition(Vec2(600.0f, 60.0f));
	_source->setContentSize(Size2(40.0f, 40.0f));

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.6f), [this] { runPhase1(); },
			Rc<DelayTime>::create(0.3f), [this] { runPhase2(); }, Rc<DelayTime>::create(0.3f),
			[this] { runPhase3(); }, Rc<DelayTime>::create(0.3f), [this] { runPhase4(); },
			Rc<DelayTime>::create(0.3f), [this] { runPhase5(); }));
	return true;
}

void DragBasicLayout::handleEnter(Scene *scene) {
	TestLayout::handleEnter(scene);

	// NOT in init(): a node has no scene until it is added to one, and acquireForNode installs the
	// system on the scene's content node. This is the same reason FormInputListener finds its form
	// in handleEnter rather than in its constructor
	_drag = DragSystem::acquireForNode(this);
}

void DragBasicLayout::handleExit() {
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

Node *DragBasicLayout::addTarget(StringView name, const Rect &rect, ZOrder z, Counters *counters,
		bool accept, Function<void()> &&extraOnDrop) {
	auto node = addChild(Rc<basic2d::Layer>::create(Color::Teal_700), z);
	node->setName(name);
	node->setAnchorPoint(Anchor::BottomLeft);
	node->setPosition(rect.origin);
	node->setContentSize(rect.size);

	setDropTarget(node,
			DropTargetSlots{
				.accept = [counters, accept](const DragEvent &event) -> DragResponse {
		++counters->accepts;
		return accept ? DragResponse{event.allowed} : DragResponse();
	},
				.enter = [counters](const DragEvent &) { ++counters->enters; },
				.over = [counters](const DragEvent &) { ++counters->overs; },
				.leave = [counters](const DragEvent &) { ++counters->leaves; },
				.drop =
						[counters, extra = sp::move(extraOnDrop)](const DragEvent &, DragActions) {
		++counters->drops;
		if (extra) {
			extra();
		}
		return true;
	},
			});

	return node;
}

Vec2 DragBasicLayout::world(const Rect &rect) const {
	return const_cast<DragBasicLayout *>(this)->convertToWorldSpace(
			Vec2(rect.getMidX(), rect.getMidY()));
}

void DragBasicLayout::beginDrag(Node *source) {
	DragOffer offer;
	offer.local = source;
	offer.localType = String("test/node");
	offer.label = String("test payload");
	offer.allowedActions = DragActions::Move;
	offer.defaultAction = DragActions::Move;
	offer.decorator = []() -> Rc<Node> {
		auto node = Rc<basic2d::Layer>::create(Color::Amber_500);
		node->setAnchorPoint(Anchor::Middle);
		node->setContentSize(Size2(40.0f, 24.0f));
		return node;
	};
	offer.completion = [this](DragActions action) {
		++_completions;
		_lastCompletion = action;
	};

	_drag->beginDrag(sp::move(offer), Rc<Ref>(source));
}

void DragBasicLayout::expect(bool cond, StringView phase, StringView what) {
	++_checks;
	if (!cond) {
		++_failures;
		log::source().error("DragBasicTest", phase, ": ", what);
	}
}

void DragBasicLayout::expectEq(StringView phase, StringView what, size_t actual, size_t expected) {
	++_checks;
	if (actual != expected) {
		++_failures;
		log::source().error("DragBasicTest", phase, ": ", what, " is ", actual, ", expected ",
				expected);
	}
}

void DragBasicLayout::runPhase1() {
	expect(_drag != nullptr, "phase1", "no drag system was acquired");
	if (!_drag) {
		return;
	}

	// The hidden target is not merely refused - it was never visited, so it is not in the roster
	// at all. Five of the six are
	expectEq("phase1", "roster size", _drag->getTargetCount(), 5);

	beginDrag(_source);

	auto session = _drag->getSession();
	expect(session != nullptr, "phase1", "the drag did not start");
	if (!session) {
		return;
	}

	auto decorator = session->getDecorator();
	expect(decorator != nullptr, "phase1", "no decorator was built");
	if (decorator) {
		auto content = getScene() ? getScene()->getContent() : nullptr;
		expect(decorator->getParent() == content, "phase1",
				"the decorator is not a child of the scene content");
		expect(decorator->getLocalZOrder() == DragSystem::DecoratorZOrder, "phase1",
				"the decorator is in the wrong ZOrder band");
	}

	_drag->updateDrag(world(TargetA));
	expectEq("phase1", "A entered", _a.enters, 1);
	expectEq("phase1", "A over", _a.overs, 1);
	expectEq("phase1", "A left", _a.leaves, 0);
	expectEq("phase1", "B entered", _b.enters, 0);
	expect(session->getTarget() != nullptr, "phase1", "the drag has no current target over A");
	expect(session->getResolvedAction() == DragActions::Move, "phase1",
			"the resolved action is not Move");
}

void DragBasicLayout::runPhase2() {
	if (!_drag) {
		return;
	}

	// Leaving A and entering B closes A's bracket exactly once...
	_drag->updateDrag(world(TargetB));
	expectEq("phase2", "A left", _a.leaves, 1);
	expectEq("phase2", "A re-entered", _a.enters, 1);
	expectEq("phase2", "B entered", _b.enters, 1);

	// ...and the refusing node on top of B was asked and declined, without ever becoming current.
	// A refusal has to be transparent, or nothing could ever be dropped under a decorative overlay
	expect(_refused.accepts > 0, "phase2", "the refusing target was never asked");
	expectEq("phase2", "the refusing target entered", _refused.enters, 0);

	// staying inside a target is `over`, not another `enter`
	_drag->updateDrag(world(TargetB) + Vec2(10.0f, 0.0f));
	expectEq("phase2", "B entered again while staying inside", _b.enters, 1);
	expectEq("phase2", "B over", _b.overs, 2);

	// of two overlapping targets the one drawn on top takes it - and the covered one is not even
	// asked, because the search stops at the first acceptance
	_drag->updateDrag(world(OverlapHigh));
	expectEq("phase2", "the top target entered", _high.enters, 1);
	expectEq("phase2", "the covered target entered", _low.enters, 0);
	expectEq("phase2", "the covered target was asked", _low.accepts, 0);
	expectEq("phase2", "B left", _b.leaves, 1);

	// over an invisible node there is nothing at all
	_drag->updateDrag(world(Hidden));
	expectEq("phase2", "the hidden target was asked", _hiddenCounters.accepts, 0);
	expectEq("phase2", "the top target left", _high.leaves, 1);
	expect(_drag->getSession()->getTarget() == nullptr, "phase2",
			"an invisible node acted as a drop target");
	expect(_drag->getSession()->getResolvedAction() == DragActions::None, "phase2",
			"an action resolved with no target under the pointer");
}

void DragBasicLayout::runPhase3() {
	if (!_drag) {
		return;
	}

	auto session = _drag->getSession();
	Rc<Node> decorator = session ? session->getDecorator() : nullptr;

	_drag->updateDrag(world(TargetA));
	expectEq("phase3", "A entered", _a.enters, 2);

	_drag->commitDrag();

	expectEq("phase3", "A dropped", _a.drops, 1);
	expectEq("phase3", "A left", _a.leaves, 2);
	expectEq("phase3", "completions", _completions, 1);
	expect(_lastCompletion == DragActions::Move, "phase3", "the completion did not report Move");
	expect(_drag->getSession() == nullptr, "phase3", "the session outlived the drop");
	expect(!_drag->isDragging(), "phase3", "the system still reports a drag in flight");

	if (decorator) {
		expect(decorator->getParent() == nullptr, "phase3", "the decorator is still in the scene");
	}

	// a second commit has nothing to commit, and must not fire anything a second time
	_drag->commitDrag();
	expectEq("phase3", "completions after a second commit", _completions, 1);
	expectEq("phase3", "A dropped after a second commit", _a.drops, 1);
}

void DragBasicLayout::runPhase4() {
	if (!_drag) {
		return;
	}

	beginDrag(_source);
	expect(_drag->isDragging(), "phase4", "the second drag did not start");

	_drag->updateDrag(world(TargetA));
	expectEq("phase4", "A entered", _a.enters, 3);

	_drag->cancelDrag();

	// cancellation still closes the bracket, and still reports - with None, so a Move source
	// knows to keep its original
	expectEq("phase4", "A left", _a.leaves, 3);
	expectEq("phase4", "A dropped on cancel", _a.drops, 1);
	expectEq("phase4", "completions", _completions, 2);
	expect(_lastCompletion == DragActions::None, "phase4",
			"a cancelled drag reported an action anyway");

	// a cancel with a foreign source must not touch someone else's drag
	beginDrag(_source);
	_drag->cancelDrag(_targetB);
	expect(_drag->isDragging(), "phase4", "a foreign source cancelled this drag");
	_drag->cancelDrag();
	expect(!_drag->isDragging(), "phase4", "the drag survived its own cancel");
	expectEq("phase4", "completions after the guarded cancel", _completions, 3);
}

void DragBasicLayout::runPhase5() {
	if (!_drag) {
		return;
	}

	// The drop destroys the source node. Nothing here may touch freed memory: the session holds
	// the source by Rc, and the system detaches the session before the drop runs, so the
	// handleExit that follows finds no drag in flight instead of re-entering a half-finished one
	_destroySource = true;

	beginDrag(_source);
	_drag->updateDrag(world(TargetA));
	_drag->commitDrag();

	expect(_sourceDestroyed, "phase5", "the drop did not destroy the source");
	expectEq("phase5", "A dropped", _a.drops, 2);
	expectEq("phase5", "completions", _completions, 4);
	expect(_lastCompletion == DragActions::Move, "phase5", "the completion did not report Move");
	expect(_drag->getSession() == nullptr, "phase5", "the session outlived a destructive drop");

	log::source().warn("DragBasicTest", "SUMMARY: ", _checks, " checks, ", _failures, " failures");
}

void DragBasicLayout::registerCommands() {
	TestLayout::registerCommands();

	addCommand("targets", "Dump the committed drop-target roster", [this](Value &&) {
		Value result;
		result.setInteger(int64_t(_drag ? _drag->getTargetCount() : 0), "count");
		result.setBool(_drag && _drag->isDragging(), "dragging");
		return result;
	});
}

} // namespace stappler::xenolith::app
