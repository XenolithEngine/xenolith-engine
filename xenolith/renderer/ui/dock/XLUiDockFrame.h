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

#ifndef XENOLITH_RENDERER_UI_DOCK_XLUIDOCKFRAME_H_
#define XENOLITH_RENDERER_UI_DOCK_XLUIDOCKFRAME_H_

#include "XLUiDockTypes.h"
#include "XLUiDockTabBar.h"
#include "XLUiPanel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// One parking place, as a scene node: a tab strip and a body that hosts the active panel.
//
// It is a FLAT child of the dock root - it neither contains nor is contained by any other frame,
// whatever the split tree says. DockSystem writes its position and content size; the frame owns
// only what is inside it.
//
// Inside, the strip-then-body arrangement is one flex column (a row for a Left/Right strip), run
// by an ordinary LayoutSystem. The frame carries SystemManagedLayout so a stylesheet cannot
// reconfigure that layout out from under it - see the header of that marker for the full reason.
//
// CSS type "dock-frame"; the body is "dock-frame-body".
class SP_PUBLIC DockFrame : public Panel {
public:
	virtual ~DockFrame() = default;

	virtual bool init(const DockFrameParams &, DockNodeHandle);

	DockNodeHandle getHandle() const { return _handle; }

	const DockFrameParams &getParams() const { return _params; }
	virtual void setParams(const DockFrameParams &);

	// where the active panel's node is parented
	Node *getBody() const { return _body; }

	/* SHUT TO THE TAB STRIP: the body stops being displayed and the strip is all that is left.

	`display: none` and not `setVisible(false)`, through a VisibilityComponent: an invisible box is
	still a box the flex run reserves room for, and what is wanted here is for the body to take no
	room at all so the strip becomes the frame's whole width. The class `collapsed` goes on the frame
	so a stylesheet can say what a shut place looks like.

	It says nothing about the tree - DockSystem::setFrameCollapsed writes that and then tells this. */
	virtual void setCollapsed(bool);
	bool isCollapsed() const { return _collapsed; }

	DockTabBar *getTabBar() const { return _tabBar; }

	// The strip's rect in THIS frame's coordinate space, as of the last layout. A drop test works
	// in the dock root's space and offsets this by the frame's own rect, rather than converting
	// through the scene graph.
	Rect getTabBarRect() const;

protected:
	using Panel::init;

	// keeps the flex run pointing the way the tab bar side demands
	void updateFlow();

	DockNodeHandle _handle;
	DockFrameParams _params;
	DockTabBar *_tabBar = nullptr;
	Node *_body = nullptr;
	bool _collapsed = false;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_DOCK_XLUIDOCKFRAME_H_
