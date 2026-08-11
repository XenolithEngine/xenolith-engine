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

#ifndef XENOLITH_RENDERER_UI_DOCK_XLUIDOCKDRAGVISUALS_H_
#define XENOLITH_RENDERER_UI_DOCK_XLUIDOCKDRAGVISUALS_H_

#include "XLUiDockTab.h"
#include "XLUiPanel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// What follows the pointer while a panel is being dragged: the panel's icon and title on a small
// surface. It carries NO InputListener at all - one would put it between the pointer and the tab
// that owns the drag.
//
// CSS type "dock-drag-ghost".
class SP_PUBLIC DockDragGhost : public Panel {
public:
	virtual ~DockDragGhost() = default;

	virtual bool init(const DockPanelDescriptor &);

protected:
	using Panel::init;

	basic2d::Label *_label = nullptr;
	basic2d::IconSprite *_icon = nullptr;
};

// The highlight showing where the panel would land. One rectangle, moved and resized to whatever
// the hit test reports, hidden when there is nowhere to drop.
//
// CSS type "dock-drop-indicator", with a class per zone (`center`, `split`, `caret`) so the three
// cases can be told apart visually.
class SP_PUBLIC DockDropIndicator : public Panel {
public:
	virtual ~DockDropIndicator() = default;

	virtual bool init() override;

	virtual void setTarget(const DockDropTarget &);

protected:
	void setZoneClass(StringView);

	String _zone;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_DOCK_XLUIDOCKDRAGVISUALS_H_
