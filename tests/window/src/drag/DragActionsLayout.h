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

#ifndef TESTS_WINDOW_SRC_DRAG_DRAGACTIONSLAYOUT_H_
#define TESTS_WINDOW_SRC_DRAG_DRAGACTIONSLAYOUT_H_

#include "app/TestLayout.h"
#include "XLDragSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Action negotiation: who decides what a drop actually DOES, and what the cursor says about it.
//
// The rule under test is a three-way agreement, and the interesting half is that the modifier only
// states a PREFERENCE. The source publishes a mask of what it can do, the modifier picks one of
// them, the target answers with a subset - and if the target cannot do the preferred one but can
// do another, that other one happens. Collapsing preference and demand into a single value is the
// obvious simplification and it is wrong: it makes a Copy-only target refuse any drag the user
// happens to be holding Shift over.
//
// The cursor is the visible half of the same state, and it has its own trap: during a drag the
// pointer is over the target, so nothing the source owns can set it. It comes from a window-wide
// layer the drag system owns, at a priority low enough to be applied last.
class DragActionsLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;

protected:
	virtual void registerCommands() override;

	void expect(bool cond, StringView phase, StringView what);
	void expectAction(StringView phase, StringView what, DragActions actual, DragActions expected);
	void expectCursor(StringView phase, StringView what, WindowCursor expected);

	Node *addTarget(StringView name, const Rect &, DragActions accepted);
	Vec2 world(const Rect &) const;

	bool beginDrag(DragActions allowed, DragActions dflt);
	WindowCursor currentCursor() const;

	void runPhase1();
	void runPhase2();
	void runPhase3();
	void runPhase4();

	// the node wired for the REAL input path: a swipe on it starts a drag through DragSource
	Node *_handle = nullptr;

	DragSystem *_drag = nullptr;

	size_t _completions = 0;
	DragActions _lastCompletion = DragActions::None;

	size_t _checks = 0;
	size_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_DRAG_DRAGACTIONSLAYOUT_H_
