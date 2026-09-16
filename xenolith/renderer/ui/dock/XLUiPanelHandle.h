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

/** The grab point of a parked panel: a dock tab, an accordion header. A Button that also drags
the panel out of its host.

 - the drag starts after DefaultDragThreshold points, past the tap tolerance, and handleLeftTap
   refuses while _dragging;
 - setExclusive() is required once dragging: the pointer leaves the node and the listener set is
   frozen at Begin, so without it Move events stop arriving;
 - the drop usually destroys this node; handleExit cancels a drag that outlives its handle.

The swipe uses the Button's own listener. Subclasses override canBeginDragAt() to limit where a drag
may start and updatePanelDragOffer() to record the panel's origin. */
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

	// Whether a press at this world point may start a drag; the whole node by default.
	virtual bool canBeginDragAt(const Vec2 &worldLocation) const { return true; }

	// Add host-specific origin data to the payload and offer after the base filled them in.
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
