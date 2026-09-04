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

#include "XLUiPanelHandle.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

class DockSystem;

// One tab in a frame's strip: the icon and title of a parked panel, plus an optional close button.
//
// It is a PanelHandle, so the drag that pulls the panel out of this frame - threshold, pointer
// capture, abort on exit - is the same one an accordion header uses, and so is everything a Button
// brings with it. What a TAB adds is the frame it belongs to: the whole of the tab is a grab point
// (there is nothing else a press on it could mean), and the frame handle is what a drop reads to
// recognise a move that would change nothing.
//
// CSS type "dock-tab"; the style class `active` is on the one showing, and the close affordance is
// "dock-tab-close".
class SP_PUBLIC DockTab : public PanelHandle {
public:
	virtual ~DockTab() = default;

	virtual bool init(NotNull<DockSystem>, DockNodeHandle frame, StringView panelId);

	DockNodeHandle getFrame() const { return _frame; }
	void setFrame(DockNodeHandle handle) { _frame = handle; }

	// The title, which is also what the tab OFFERS AS A HINT: a strip narrowed to icons is a
	// stylesheet's decision (`dock-tab.vertical > label { display: none }`), and the hint is what
	// keeps the title readable after it.
	virtual void setString(StringView) override;

	virtual void setActive(bool);
	bool isActive() const { return _active; }

	// mirrors DockPanelFlags::Closable; hides the close affordance when off
	virtual void setClosable(bool);

protected:
	using PanelHandle::init;

	virtual bool handleLeftTap() override;

	// the frame this tab sits in travels with the panel; see DockPanelPayload
	virtual void updatePanelDragOffer(DragOffer &, DockPanelPayload &) override;

	DockNodeHandle _frame;
	Button *_close = nullptr;
	bool _active = false;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_DOCK_XLUIDOCKTAB_H_
