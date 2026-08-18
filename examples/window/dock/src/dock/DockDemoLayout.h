/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#ifndef EXAMPLES_WINDOW_DOCK_SRC_DOCK_DOCKDEMOLAYOUT_H_
#define EXAMPLES_WINDOW_DOCK_SRC_DOCK_DOCKDEMOLAYOUT_H_

#include "XL2dSceneLayout.h"
#include "XLUiDockSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

// A live demonstration of ui::DockFrame (and the DockSystem that owns it).
//
// What it shows, and how to read it:
//   * Several NAMED frames parked by one split tree - "sidebar" carries its tab strip on the LEFT
//     (DockTabBarSide::Left) while the others use the default TOP, so two DockFrame orientations
//     are visible at once. Each frame's declared name is also its CSS #id, which the stylesheet
//     below uses to give one parking place a distinct fill.
//   * A control bar whose buttons call the public DockSystem API (activate / move / split / close /
//     reset) - the same operations a drag-and-drop would commit through, so what you click is
//     exactly what the dock does on its own. Each one reports itself in the status line via the
//     system's callbacks.
//   * Panels are lazy: their content node is built once, on first show, and kept across moves, so
//     switching tabs or dragging a panel never rebuilds it (the self-check counts builder calls).
class DockDemoLayout : public basic2d::SceneLayout2d {
public:
	virtual ~DockDemoLayout() = default;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	// Register the panels, build the initial split tree and wire the callbacks. Called by init(),
	// and again (after a reset) to restore the declared layout.
	void buildDock();

	// The control bar: one button per public operation it wants to demonstrate, plus the status
	// label on its left. Created once; safe to call from anywhere in this class.
	void makeControlBar();

	// One styled button appended to the bar. Returns it so a caller can keep a reference.
	ui::Button *makeControl(StringView label, Function<void()> &&action);

	// One line of live state for the status strip, recomputed from the tree and callbacks.
	void refreshStatus(StringView lastAction = StringView());

	// How many times a panel's builder ran (0 when it has never been shown).
	size_t buildCount(StringView id) const;

	// --- self-check ------------------------------------------------------
	// Structural assertions that hold synchronously (no frame needed): the declared frames exist,
	// their panels are parked where setLayout put them, only the active panel was built, moving a
	// panel carries its node rather than rebuilding it, and split/save/restore preserve the tree.
	// Prints "N checks, M failures" on completion.
	void runSelfCheck();

	ui::DockSystem *_dock = nullptr;

	Node *_background = nullptr; // full-bleed backdrop behind everything
	Node *_dockRoot = nullptr;   // flat owner: every DockFrame is its direct child (no layout of its own)
	Node *_controlBarRow = nullptr; // the top strip hosting the status label and control buttons
	basic2d::Label *_statusLabel = nullptr;

	// builder call counts, keyed by panel id - the point of lazy content made visible
	Map<String, size_t> _builds;

	// which panel the Close button last closed, so it can reopen it on the next click
	String _lastClosed;

	size_t _checks = 0;
	size_t _failures = 0;
	bool _selfCheckDone = false;
};

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_DOCK_SRC_DOCK_DOCKDEMOLAYOUT_H_
