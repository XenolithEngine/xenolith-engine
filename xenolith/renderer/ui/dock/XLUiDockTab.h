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

#ifndef XENOLITH_RENDERER_UI_DOCK_XLUIDOCKTAB_H_
#define XENOLITH_RENDERER_UI_DOCK_XLUIDOCKTAB_H_

#include "XLUiDockTypes.h"
#include "XLDragSystem.h"
#include "XLUiButton.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

class DockSystem;

// One tab in a frame's strip: the icon and title of a parked panel, plus an optional close button.
//
// It is a Button, so the whole tap/hover/active machinery - and with it `:hover` and `:active` in
// CSS - comes for free. What it adds is the drag that pulls the panel out of this frame, which is
// why the panel id and not just a caption is on it.
//
// CSS type "dock-tab"; the style class `active` is on the one showing, and the close affordance is
// "dock-tab-close".
class SP_PUBLIC DockTab : public Button {
public:
	virtual ~DockTab() = default;

	virtual bool init(NotNull<DockSystem>, DockNodeHandle frame, StringView panelId);

	virtual void handleExit() override;

	StringView getPanelId() const { return _panelId; }
	DockNodeHandle getFrame() const { return _frame; }
	void setFrame(DockNodeHandle handle) { _frame = handle; }

	virtual void setActive(bool);
	bool isActive() const { return _active; }

	// mirrors DockPanelFlags::Closable; hides the close affordance when off
	virtual void setClosable(bool);

	bool isDragging() const { return _dragging; }

protected:
	using Button::init;

	virtual bool handleLeftTap() override;

	bool handleDragBegin(const GestureSwipe &);
	void handleDrag(const GestureSwipe &);
	void handleDragEnd(bool cancelled);

	DockSystem *_system = nullptr; // non-owning: the system outlives every node it created

	// the general drag coordinator, acquired when a drag starts; non-owning for the same reason
	DragSystem *_drag = nullptr;

	DockNodeHandle _frame;
	String _panelId;
	Button *_close = nullptr;
	bool _active = false;
	bool _dragging = false;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_DOCK_XLUIDOCKTAB_H_
