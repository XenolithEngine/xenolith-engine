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

#ifndef TESTS_WINDOW_SRC_DOCK_DOCKTABSLAYOUT_H_
#define TESTS_WINDOW_SRC_DOCK_DOCKTABSLAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiDockSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Tabs, lazily built panel content, and the drop zones a drag resolves to.
//
// The property that matters most here is that a panel's NODE is built once and then survives
// everything: switching to another tab, being reordered, being dragged into a different frame,
// even the source frame collapsing behind it. A panel that rebuilt itself on every move would
// lose its scroll position, its selection and its half-typed text, and none of that would be
// visible in a layout dump - hence a per-panel build counter.
//
// The rest is the zone model: the tab strip beats the body, an edge band of the body beats its
// middle, a frame that refuses drops offers no zone at all, and dropping a lone panel back into
// its own frame is not a drop.
class DockTabsLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	void expect(bool cond, StringView phase, StringView what);
	void expectNear(StringView phase, StringView what, float actual, float expected);

	void runPhase1();
	void runPhase2();
	void runPhase3();
	void runPhase4();
	void runPhase5();

	size_t buildCount(StringView id) const;
	Vector<String> panelsOf(ui::DockNodeHandle) const;

	Node *_root = nullptr;
	ui::DockSystem *_dock = nullptr;

	ui::DockNodeHandle _left;
	ui::DockNodeHandle _right;

	Map<String, size_t> _builds;
	Map<String, Rc<Node>> _built;

	size_t _checks = 0;
	size_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_DOCK_DOCKTABSLAYOUT_H_
