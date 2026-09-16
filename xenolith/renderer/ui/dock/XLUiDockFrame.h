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
// A flat child of the dock root; DockSystem writes its position and content size. Inside, the
// strip and body are a flex column (a row for a Left/Right strip) run by a LayoutSystem; the frame
// carries SystemManagedLayout so a stylesheet cannot reconfigure that layout.
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

	/* Collapse to the tab strip: the body gets `display: none` (not setVisible, which would still
	take room) and the frame gets the class `collapsed`. Does not touch the tree;
	DockSystem::setFrameCollapsed does and then calls this. */
	virtual void setCollapsed(bool);
	bool isCollapsed() const { return _collapsed; }

	DockTabBar *getTabBar() const { return _tabBar; }

	// The strip's rect in this frame's space, as of the last layout.
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
