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

#ifndef XENOLITH_RENDERER_UI_DOCK_XLUIPANELHANDLE_H_
#define XENOLITH_RENDERER_UI_DOCK_XLUIPANELHANDLE_H_

#include "XLUiPanelHost.h"
#include "XLDragSystem.h"
#include "XLUiButton.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/** The grab point of a parked panel, whatever it is parked in: a dock tab, an accordion header.

It is a Button, so the whole tap/hover/active machinery - and with it `:hover` and `:active` in CSS -
comes for free. What it adds is the drag that pulls the panel out of wherever it currently is, which
is why the panel id and the host are on it rather than just a caption.

WHY THIS IS A BASE CLASS AND NOT A HELPER FUNCTION. Building the DragOffer is the obvious thing to
share and it is the smaller half. The other half is the part that is easy to get wrong, and getting
it wrong is silent:

 - THE THRESHOLD. The drag only begins after DefaultDragThreshold points of travel, which is past the
   tap tolerance, so a click never starts one - and handleLeftTap still refuses while _dragging,
   because the release would otherwise be a second action on the same press;

 - THE CAPTURE. The pointer leaves this node on the first frame of the drag, and the dispatcher
   freezes an event chain's listener set at Begin. Without setExclusive() the recognizer stops
   receiving Move a few pixels in and the drag dies silently;

 - THE ABORT. This node is routinely destroyed by the very drop that ends the drag - that is what a
   move IS. handleExit cancels a drag that outlived its own handle.

The swipe rides the listener the Button already owns rather than a second InputListener of its own:
a second one would sit between the pointer and this one.

Two seams for a subclass: canBeginDragAt() decides WHERE on this node a drag may start (a dock tab
says anywhere; an accordion header says only on its grip), and updatePanelDragOffer() adds whatever
the host needs to recognise where the panel came from. */
class SP_PUBLIC PanelHandle : public Button {
public:
	virtual ~PanelHandle() = default;

	virtual bool init(NotNull<PanelHost>, StringView panelId);

	virtual void handleExit() override;

	StringView getPanelId() const { return _panelId; }
	PanelHost *getPanelHost() const { return _host; }

	bool isDragging() const { return _dragging; }

protected:
	using Button::init;

	// May a press at this WORLD point start a drag? The default is the whole node, which is what a
	// tab wants; a widget that also does something else with a press narrows it to a grip, which a
	// world point answers directly - `_grip->isTouched(p)`, the same test TreeView's row uses to
	// keep a tap on its expander from counting as a tap on the row.
	virtual bool canBeginDragAt(const Vec2 &worldLocation) const { return true; }

	// Stamp whatever the host needs onto the payload and the offer the base has just filled in. A
	// dock tab records the frame it was sitting in; a linear container records its index.
	virtual void updatePanelDragOffer(DragOffer &, DockPanelPayload &) { }

	bool handleDragBegin(const GestureSwipe &);
	void handleDrag(const GestureSwipe &);
	void handleDragEnd(bool cancelled);

	PanelHost *_host = nullptr; // non-owning: the host outlives every node it created

	// the general drag coordinator, acquired when a drag starts; non-owning for the same reason
	DragSystem *_drag = nullptr;

	String _panelId;
	bool _dragging = false;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_DOCK_XLUIPANELHANDLE_H_
