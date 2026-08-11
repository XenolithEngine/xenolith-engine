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

#ifndef XENOLITH_RENDERER_UI_DOCK_XLUIDOCKTABBAR_H_
#define XENOLITH_RENDERER_UI_DOCK_XLUIDOCKTABBAR_H_

#include "XLUiDockTab.h"
#include "XLUiPanel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// The strip of tabs along one edge of a frame.
//
// It runs its own LayoutSystem in flex - a row for a Top/Bottom strip, a column for a Left/Right
// one - with every tab sized by its own content. That is also what makes the strip self-sizing:
// the frame gives it `flex-basis: fit-content`, so the height (or width) it reports back is the
// height the tabs actually need, and the SAME measurement is what floors the frame's minimum.
//
// Overflow is deliberately not scrolled in this version: too many tabs simply make the frame's
// minimum grow, which the placement pass then has to honour.
//
// CSS type "dock-tab-bar", plus the class `horizontal` or `vertical`.
class SP_PUBLIC DockTabBar : public Panel {
public:
	virtual ~DockTabBar() = default;

	virtual bool init(DockTabBarSide);

	DockTabBarSide getSide() const { return _side; }
	virtual void setSide(DockTabBarSide);

	bool isVertical() const {
		return _side == DockTabBarSide::Left || _side == DockTabBarSide::Right;
	}

	SpanView<DockTab *> getTabs() const { return _tabs; }

	// Bring the strip in line with a frame's panel list, reusing the tabs that are already there:
	// a tab whose panel is still parked here keeps its node, so switching tabs or reordering them
	// does not rebuild the strip.
	virtual void setTabs(SpanView<DockTab *>);

	// index the strip would insert at for a point in its own coordinate space, and the caret to
	// draw for that index
	size_t indexForPosition(const Vec2 &) const;
	Rect caretRectForIndex(size_t) const;

protected:
	using Panel::init;

	DockTabBarSide _side = DockTabBarSide::Top;
	Vector<DockTab *> _tabs;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_DOCK_XLUIDOCKTABBAR_H_
