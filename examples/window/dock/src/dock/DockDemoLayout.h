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
#include "XL2dLabel.h"
#include "XLUiDockSystem.h"
#include "XLUiButton.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

/* The demo's stylesheet, for whoever installs it.

IT GOES ON THE SCENE CONTENT, not on this layout. A tab's hint is a ui::SubWindow, and an in-scene
one is pushed onto the SceneContent as an overlay - a SIBLING of this layout rather than a
descendant. A ui::StyleSystem installed one level lower styles every tab in the window and none of
the hints that pop out of them, which come up as unstyled white boxes.

On the content, one recursive ui::StyleResolver covers the layout and every overlay pushed beside
it, `:root` is the content, and the custom properties declared there are inherited by both. */
StringView getDockDemoStylesheet();

/** An IDE-shaped arrangement of parked panels, and the two kinds of tab strip it can park them in.

WHAT IS ON SCREEN. Three frames from one split tree: a sidebar whose strip runs down its LEFT edge,
and beside it an editor over a bottom band. The sidebar's strip is the SAME DockTabBar as the other
two, turned on its side - and the stylesheet then narrows its tabs to square icons and hides their
titles, which is the whole of the difference between an icon rail and a row of labelled tabs.

WHAT THERE IS TO DO WITH IT, all of it drag and drop the dock runs by itself:

  * DRAG A TAB ONTO ANOTHER FRAME'S BODY and it becomes a tab of that frame. Drag it onto the
    frame's tab strip and it lands at the caret;
  * DRAG A TAB ONTO A FRAME'S EDGE - the outer ~48pt of the body - and the frame is SPLIT, the
    dragged panel taking the side you dropped on. A layout is built by dragging, not declared;
  * DRAG A TAB BETWEEN THE TWO KINDS OF STRIP. A panel dragged into the rail arrives as a square
    icon with its title in a hint; dragged back out it is a labelled tab again. Nothing about the
    panel changed - only which strip its tab now lives in;
  * DRAG A DIVIDER to re-proportion two frames.

A panel's node is built at most once, on first show, and kept across every one of those moves -
the demo counts the builder calls so that claim can be read off the screen rather than believed.

DRIVING IT WITHOUT A MOUSE. Every action is also a `dock.*` inspector command, and `dock.hittest` /
`dock.drop` resolve and commit a drop point without an input event at all, so the drag-to-split
behaviour above is scriptable headless. runSelfCheck() asserts the structural claims synchronously
and prints "N checks, M failures". */
class DockDemoLayout : public basic2d::SceneLayout2d {
public:
	virtual ~DockDemoLayout() = default;

	virtual bool init() override;

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;
	virtual void handleContentSizeDirty() override;

protected:
	// --- construction ----------------------------------------------------
	void buildControlBar();
	ui::Button *makeControl(StringView label, Function<void()> &&action);

	// Describe every panel to the registry. Called once: a descriptor outlives every layout the
	// tree is ever given, which is exactly why the two are separate.
	void registerPanels();

	// (Re)build the declared split tree. This is what Reset does, and what the self-check restores
	// after it has rearranged things.
	void applyLayout();

	// --- operations, shared by the buttons and the commands ---------------

	// Turn one frame's strip: Left/Right make an icon rail, Top/Bottom a row of labelled tabs.
	bool setFrameSide(StringView frameName, ui::DockTabBarSide);

	// Commit what a drop at a resolved target would do, through the same public operations the
	// dock's own drop slot uses. `dock.drop` is this, and so is every drag that ends on screen.
	bool commitDrop(StringView panelId, const ui::DockDropTarget &);

	// One line of live state.
	void refreshStatus(StringView lastAction = StringView());

	// How many times a panel's builder ran (0 when it has never been shown).
	size_t buildCount(StringView id) const;

	// --- inspector -------------------------------------------------------
	void registerCommands();

	/* One `dock.*` command. The handler reads its arguments through a CONST reference: a script
	sends whatever it likes, and the non-const data::Value getters assert on a key that is not
	there rather than answering an empty value. */
	void addCommand(StringView name, StringView description,
			Function<Value(const Value &)> &&handler);

	// The whole tree as a Value: every slot in depth-first order, and for a frame the tabs as they
	// are actually rendered - kind, size and all, which is how a headless run sees an icon rail.
	Value encodeTree() const;
	ui::DockNodeHandle resolveFrame(const Value &args) const;

	// --- self-check ------------------------------------------------------

	/* Restore the declared layout and run the checks against it, one pass later.

	BOTH HALVES MATTER. The reset is what makes the button mean the same thing the tenth time it is
	pressed as the first - every claim below is about the DECLARED arrangement, not about whatever
	the demo has been dragged into. The delay is because a mutation writes the tree immediately
	while the RECTS follow on the next layout pass, and one of the checks asks the hit test where a
	frame's edge is. */
	void runSelfCheck();

	/* The claims of the class comment, asserted with no further frame needed: the declared frames
	exist and hold what the spec put in them, the sidebar's strip is vertical and its tabs are
	marked as such, a panel's builder never runs twice, a panel carried between the two kinds of
	strip is not rebuilt, an edge drop resolves to a split zone and splitting it adds exactly one
	frame, and save/restore round-trips. Prints "N checks, M failures". */
	void runChecks();
	void expect(bool, StringView message);

	Node *_background = nullptr;

	// The flex column everything hangs from. A child rather than this node: a recursive
	// StyleResolver re-resolves its DESCENDANTS and never its own owner, so `#demo-root
	// { display: flex }` written for the node carrying the resolver is a rule that never runs.
	Node *_root = nullptr;

	Node *_controlBar = nullptr;
	basic2d::Label *_statusLabel = nullptr;

	// The dock's owner. It carries no layout of its own - DockSystem writes every frame's geometry
	// itself - so it is a flex ITEM of `#demo-root` and never a flex container.
	Node *_dockRoot = nullptr;
	ui::DockSystem *_dock = nullptr;

	// builder call counts, keyed by panel id: lazy content made visible
	Map<String, size_t> _builds;

	// which panel the Close button last closed, so it can reopen it on the next click
	String _lastClosed;
	String _lastAction;

	// Held so handleExit can take the commands down: a lambda that captured a destroyed layout is
	// a dangling call from the inspector socket.
	Scene *_inspectorScene = nullptr;
	Vector<String> _inspectorCommands;

	size_t _checks = 0;
	size_t _failures = 0;

	// The lazy builder has TWO claims, and only one of them survives being exercised: "never built
	// twice" holds forever, "not built yet" only until something has shown the panel once.
	bool _checked = false;
};

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_DOCK_SRC_DOCK_DOCKDEMOLAYOUT_H_
