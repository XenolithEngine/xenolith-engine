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

#ifndef TESTS_WINDOW_SRC_DOCK_ACCORDIONLAYOUT_H_
#define TESTS_WINDOW_SRC_DOCK_ACCORDIONLAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiDockSystem.h"
#include "XLUiAccordionView.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// A dock and an accordion standing side by side over ONE ui::PanelRegistry, which is the whole
// point of the arrangement: a panel dragged from one into the other must arrive as the SAME node.
//
// What this asserts, in the order it becomes possible to assert it:
//
//  - a panel's builder runs at most once, however many times it crosses between the two. That is
//    the property the shared registry exists for and the one that never shows up in a layout dump -
//    a rebuilt panel looks identical and has silently lost its scroll position and its selection;
//
//  - a panel is parked in exactly ONE place. Opening it on one side must take it off the other,
//    because there is one node and it cannot be in two parents;
//
//  - the zone rules of both sides, resolved with no drag in flight: the accordion's insertion index
//    and the dock's own hitTest for a panel it does not hold;
//
//  - the collapse and the two sizing policies, including that a collapsed section gives its panel
//    back rather than hiding it;
//
//  - a save/restore round trip over the PAIR, which is where the two halves have to agree;
//
//  - and the teardown: removing the dock while the accordion is still live must not touch the
//    accordion's panels. That one is silent without a test - the symptom is an empty section much
//    later, with nothing to connect it to.
class AccordionLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	void expect(bool cond, StringView phase, StringView what);

	void runPhase1();
	void runPhase2();
	void runPhase3();
	void runPhase4();
	void runPhase5();
	void runPhase6();

	size_t buildCount(StringView id) const;

	Node *_root = nullptr;
	Node *_dockRoot = nullptr;
	ui::DockSystem *_dock = nullptr;
	ui::AccordionView *_accordion = nullptr;
	Rc<ui::PanelRegistry> _registry;

	ui::DockNodeHandle _main;

	Map<String, size_t> _builds;
	Map<String, Rc<Node>> _built;

	size_t _checks = 0;
	size_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_DOCK_ACCORDIONLAYOUT_H_
