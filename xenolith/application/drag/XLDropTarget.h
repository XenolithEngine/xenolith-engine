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

#ifndef XENOLITH_APPLICATION_DRAG_XLDROPTARGET_H_
#define XENOLITH_APPLICATION_DRAG_XLDROPTARGET_H_

#include "XLDragTypes.h"
#include "XLNode.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

/** Makes the node it is attached to a place a drag can be dropped.

    setDropTarget(node, DropTargetSlots{
        .accept = [](const DragEvent &e) {
            return e.data->isLocal("my/thing") ? DragResponse{e.allowed} : DragResponse{};
        },
        .drop = [this](const DragEvent &e, DragActions) { return take(e.data->getLocal()); },
    });

A plain data component: no lifecycle, visit hook or input. The node has HitTestFlags::DropTarget and
registers itself into the frame's hit-test registry during its own visit, so the rect is the drawn
one (rotation included), registration order is paint order (topmost last), and unvisited nodes are
never targets. The registry is one frame old when a pointer event reads it, like ordinary input.
See DropTargetSlots for what the slots may do. */
struct SP_PUBLIC DropTargetComponent {
	static ComponentId Id;

	DropTargetSlots slots;

	// Inflates the hit test on every side, in world units, like InputListener::setTouchPadding
	float padding = 0.0f;

	// A disabled target is not found
	bool enabled = true;
};

// Attaches a drop target to `node` (or replaces its slots) and sets the hit-test flag. The only
// supported way in: the flag must match the component's presence
SP_PUBLIC const DropTargetComponent *setDropTarget(NotNull<Node>, DropTargetSlots &&);

SP_PUBLIC const DropTargetComponent *getDropTarget(NotNull<Node>);

SP_PUBLIC void setDropTargetEnabled(NotNull<Node>, bool);
SP_PUBLIC void setDropTargetPadding(NotNull<Node>, float);

// Removes both the component and the flag. A drag hovering the node gets its `leave` next frame
SP_PUBLIC void removeDropTarget(NotNull<Node>);

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_DRAG_XLDROPTARGET_H_
