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

IT CARRIES DATA AND NOTHING ELSE. There is no object here with a lifecycle, no visit hook and no
input: being a drop target is a fact about a node, and a fact belongs in a Component. What used to be
a Ref-derived System with a virtual visit hook on every target is now one bit in
Node::getHitTestFlags(), and the node publishes itself into the frame's hit-test registry like
everything else that wants to be found under a pointer.

HOW IT IS FOUND. Not by a registry of the drag system's own and not by walking the scene: the NODE
registers itself, once per frame, from inside its own visit (see HitTestFlags). Three things fall out
of that and none of them are otherwise cheap to get right:

- the world rect comes from the visit's own transform, so it is exactly the rect that was drawn, with
  no re-derivation - and a rotated target is hit where it was drawn, not across its bounding box;
- registration order is visit order is PAINT order, so the drag finds the topmost target by walking
  the registry backwards;
- a node that is not visited is not registered. An invisible subtree, a `display: none` one, a
  detached one - all of them stop being drop targets for free, with no bookkeeping and no stale
  entries.

The registry is therefore one frame old when a pointer event reads it. That is not a compromise: it
is exactly the staleness ordinary input already has, since the dispatcher resolves every event
against the previously committed listener storage.

WHAT THE SLOTS MAY DO. `accept` is a predicate and is called during hit testing - for candidates that
may never become the current target, possibly several times per frame. It must be pure. All visual
feedback belongs in `enter`/`over`/`leave`, which fire only for the current target and are bracketed
exactly. See DropTargetSlots. */
struct SP_PUBLIC DropTargetComponent {
	static ComponentId Id;

	DropTargetSlots slots;

	// Inflates the hit test on every side, in world units. The same idea as
	// InputListener::setTouchPadding: a thin target is hard to hit. It lives here rather than in the
	// registry record because how far outside itself a target reaches is a property of the TARGET,
	// not of the frame it was drawn in
	float padding = 0.0f;

	// A disabled target is not found. Cheaper and clearer than removing and re-adding the component
	// around a mode the node moves in and out of
	bool enabled = true;
};

// Attaches a drop target to `node`, or replaces the slots of the one it already has, and marks the
// node as a participant in the hit-test registry. The only supported way in: the flag is a cache of
// this component's presence, and setting one without the other makes a node that wins a hit test and
// then offers nothing
SP_PUBLIC const DropTargetComponent *setDropTarget(NotNull<Node>, DropTargetSlots &&);

SP_PUBLIC const DropTargetComponent *getDropTarget(NotNull<Node>);

SP_PUBLIC void setDropTargetEnabled(NotNull<Node>, bool);
SP_PUBLIC void setDropTargetPadding(NotNull<Node>, float);

// Removes both the component and the flag. A drag hovering the node when this runs gets its `leave`
// on the next frame, the same way it would if the node had left the scene
SP_PUBLIC void removeDropTarget(NotNull<Node>);

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_DRAG_XLDROPTARGET_H_
