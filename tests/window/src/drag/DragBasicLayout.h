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

#ifndef TESTS_WINDOW_SRC_DRAG_DRAGBASICLAYOUT_H_
#define TESTS_WINDOW_SRC_DRAG_DRAGBASICLAYOUT_H_

#include "app/TestLayout.h"
#include "XLDragSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// The drag protocol itself: the roster, the enter/over/leave bracket, topmost-wins, and the
// exactly-once guarantee at the end.
//
// Four of these are properties nothing else in the engine checks, and each has a specific way of
// going wrong:
//
// - a target is registered by being VISITED, so an invisible one must be invisible to a drag too;
// - registration order is paint order, so of two overlapping targets the one drawn on top must be
//   the one that receives - and the two must have distinct ZOrders, because sortAllChildren is
//   unstable and equal orders permute between frames;
// - `enter` and `leave` are a bracket in EVERY path out, including cancellation;
// - the drop routinely destroys the source node. That is the normal case for a move, and the
//   session has to survive its own source disappearing halfway through.
class DragBasicLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;

protected:
	// Per-target counters. `accepts` is bumped from the acceptance predicate, which is called
	// during hit testing and may run for targets that never become current - it is here to prove
	// exactly that
	struct Counters {
		size_t accepts = 0;
		size_t enters = 0;
		size_t overs = 0;
		size_t leaves = 0;
		size_t drops = 0;
	};

	virtual void registerCommands() override;

	void expect(bool cond, StringView phase, StringView what);
	void expectEq(StringView phase, StringView what, size_t actual, size_t expected);

	Node *addTarget(StringView name, const Rect &, ZOrder, Counters *, bool accept = true,
			Function<void()> &&extraOnDrop = nullptr);

	// Centre of a target's declared rect, in world space. Derived from THIS node's transform, not
	// the target's: the hidden one is never visited, so its own transform is not to be trusted
	Vec2 world(const Rect &) const;

	void beginDrag(Node *source);

	void runPhase1();
	void runPhase2();
	void runPhase3();
	void runPhase4();
	void runPhase5();

	Node *_targetA = nullptr;
	Node *_targetB = nullptr;
	Node *_overlapLow = nullptr;
	Node *_overlapHigh = nullptr;
	Node *_hidden = nullptr;
	Node *_refusing = nullptr;

	Counters _a;
	Counters _b;
	Counters _low;
	Counters _high;
	Counters _hiddenCounters;
	Counters _refused;

	// the node a drag carries; phase 5 replaces it with one the drop destroys
	Rc<Node> _source;

	DragSystem *_drag = nullptr;

	size_t _completions = 0;
	DragActions _lastCompletion = DragActions::None;

	// phase 5 arms this, and the drop then removes the source node from the scene - the case the
	// session's Rc on the source exists for
	bool _destroySource = false;
	bool _sourceDestroyed = false;

	size_t _checks = 0;
	size_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_DRAG_DRAGBASICLAYOUT_H_
