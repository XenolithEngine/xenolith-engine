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

#ifndef XENOLITH_RENDERER_UI_DOCK_XLUIDOCKSPLITTER_H_
#define XENOLITH_RENDERER_UI_DOCK_XLUIDOCKSPLITTER_H_

#include "XLUiDockTypes.h"
#include "XLUiPanel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

class DockSystem;

// The divider between the two children of one split, as its own flat node in the dock root.
//
// It is a node rather than something the frames draw for two reasons. Hit testing and the resize
// cursor come free from an InputListener - no rect arithmetic on every pointer move, no priority
// juggling against the frames' own listeners. And a frame that drew its own divider would have to
// know its neighbour and its parent split, which is exactly the coupling the flat design avoids.
//
// It sits in a ZOrder band ABOVE the frames. That is load-bearing, not cosmetic: sortAllChildren
// is not a stable sort, so at equal ZOrder a frame could end up over the divider and swallow the
// drag.
//
// CSS type "dock-splitter"; the style class `dragging` is on while it is being moved, and :hover
// works through the usual InteractiveComponent counter.
class SP_PUBLIC DockSplitter : public Panel {
public:
	// a 6pt band is hard to hit exactly, so the grab area reaches a little past the paint
	static constexpr float GrabPadding = 3.0f;

	virtual ~DockSplitter() = default;

	virtual bool init(NotNull<DockSystem>, DockNodeHandle, DockAxis);

	DockNodeHandle getHandle() const { return _handle; }
	DockAxis getAxis() const { return _axis; }

	bool isDragging() const { return _dragging; }

protected:
	using Panel::init;

	bool handleDragBegin();
	void handleDrag(const Vec2 &delta, float density);
	void handleDragEnd();

	DockSystem *_system = nullptr; // non-owning: the system outlives every node it created
	DockNodeHandle _handle;
	DockAxis _axis = DockAxis::Horizontal;
	InputListener *_listener = nullptr;
	bool _dragging = false;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_DOCK_XLUIDOCKSPLITTER_H_
