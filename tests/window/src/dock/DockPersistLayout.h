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

#ifndef TESTS_WINDOW_SRC_DOCK_DOCKPERSISTLAYOUT_H_
#define TESTS_WINDOW_SRC_DOCK_DOCKPERSISTLAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiDockSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Saving a dock layout and restoring it.
//
// The strong claim is a round trip: save, then rearrange the dock as a user would - split it,
// carry a panel across, drag a divider - then restore, and the tree must come back byte for byte.
// Comparing two save() outputs is what makes that checkable at all: it covers the shape, the
// ratios, the tab order and the active tab in one comparison, and it fails loudly if the format
// ever starts writing something derived.
//
// The rest is what a saved layout meets in the real world: a panel the file names and this build
// no longer ships, a panel this build has and the file never heard of, and a file that is simply
// broken - which must leave the live layout untouched rather than half-applied.
class DockPersistLayout : public TestLayout {
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

	Node *_root = nullptr;
	ui::DockSystem *_dock = nullptr;

	Value _saved;

	size_t _checks = 0;
	size_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_DOCK_DOCKPERSISTLAYOUT_H_
