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
#include "XLSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class DragSystem;

/** Makes the node it is added to a place a drag can be dropped.

    node->addSystem(Rc<DropTarget>::create(DropTargetSlots{
        .accept = [](const DragEvent &e) {
            return e.data->isLocal("my/thing") ? DragResponse{e.requested} : DragResponse{};
        },
        .drop = [this](const DragEvent &e, DragActions) { return take(e.data->getLocal()); },
    }));

HOW IT IS FOUND. Not by a registry and not by walking the scene: the target REGISTERS ITSELF, once
per frame, from inside its owner's visit, into the nearest `DragSystem` on the frame stack. Three
things fall out of that and none of them are otherwise cheap to get right:

- the world rect comes from the visit's own transform stack, so it is exactly the rect that was
  drawn, with no re-derivation;
- registration order is visit order is PAINT order, so the drag system finds the topmost target by
  walking its roster backwards - the same trick InputListenerStorage uses;
- a node that is not visited is not registered. An invisible subtree, a `display: none` one, a
  detached one - all of them stop being drop targets for free, with no bookkeeping and no stale
  entries.

The roster is therefore one frame old when a pointer event reads it. That is not a compromise: it
is exactly the staleness ordinary input already has, since the dispatcher resolves every event
against the previously committed listener storage.

WHAT THE SLOTS MAY DO. `accept` is a predicate and is called during hit testing - for candidates
that may never become the current target, possibly several times per frame. It must be pure. All
visual feedback belongs in `enter`/`over`/`leave`, which fire only for the current target and are
bracketed exactly. See DropTargetSlots. */
class SP_PUBLIC DropTarget : public System {
public:
	virtual ~DropTarget() = default;

	virtual bool init() override;
	virtual bool init(DropTargetSlots &&);

	virtual void handleExit() override;
	virtual void handleVisitSelf(FrameInfo &, Node *, NodeVisitFlags flags) override;

	virtual void setSlots(DropTargetSlots &&);
	const DropTargetSlots &getSlots() const { return _slots; }

	// Inflates the registered world rect on every side, in world units. The same idea as
	// InputListener::setTouchPadding: a thin target is hard to hit
	void setPadding(float value) { _padding = value; }
	float getPadding() const { return _padding; }

	// True while this target is the one the live drag is over
	bool isCurrent() const { return _current; }

protected:
	friend class DragSession;
	friend class DragSystem;

	// Dispatch points, so a subclass can override behaviour instead of filling slots. The base
	// implementations just call the corresponding slot
	virtual DragResponse handleDragAccept(const DragEvent &);
	virtual void handleDragEnter(const DragEvent &);
	virtual void handleDragOver(const DragEvent &);
	virtual void handleDragLeave(const DragEvent &);
	virtual bool handleDragDrop(const DragEvent &, DragActions);

	DropTargetSlots _slots;
	float _padding = 0.0f;
	bool _current = false;
};

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_DRAG_XLDROPTARGET_H_
