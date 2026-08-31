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

#ifndef XENOLITH_APPLICATION_DRAG_XLDRAGSOURCE_H_
#define XENOLITH_APPLICATION_DRAG_XLDRAGSOURCE_H_

#include "XLDragSystem.h"
#include "XLInputListener.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

/** Makes the node it is added to draggable.

    node->addSystem(Rc<DragSource>::create([this](DragOffer &offer) {
        offer.local = _item;
        offer.localType = StringView("my/item");
        offer.allowedActions = DragActions::Move | DragActions::Copy;
        offer.decorator = [this] { return makeGhost(); };
        offer.completion = [this](DragActions a) { if (a == DragActions::Move) { detach(); } };
        return true;
    }));

It is an InputListener, not a plain System, and that is not an implementation detail: the three
things a source has to get right are all listener-level, and all three are easy to get wrong.

- THE THRESHOLD. A press only becomes a drag after DefaultDragThreshold points of travel, which is
  below the tap tolerance, so an ordinary click never starts one.

- THE CAPTURE. The pointer leaves the source on the first frame of the drag, and the dispatcher
  freezes the candidate list for an event chain at Begin - it can only shrink after that, never
  re-target. So without setExclusive() the source stops receiving Move the moment the pointer
  crosses its own edge, and the drag dies silently a few pixels in. That call is made here, once.

- THE ABORT. The source node can leave the scene while the button is still down; in fact the drop
  that ends the drag routinely destroys it. handleExit cancels this source's own drag - and only
  its own, guarded by identity, so an unrelated node's teardown does not abort someone else's.

The offer builder returns false to refuse: an item that happens not to be movable right now simply
does not start a drag. */
class SP_PUBLIC DragSource : public InputListener {
public:
	// Fills the offer for one drag. False refuses to start
	using OfferBuilder = Function<bool(DragOffer &)>;

	virtual ~DragSource() = default;

	virtual bool init(OfferBuilder &&, float threshold = DragSystem::DefaultDragThreshold);

	virtual void handleExit() override;

	virtual void setOfferBuilder(OfferBuilder &&);

	bool isDragging() const { return _dragging; }

	// The system this source last started a drag with; null when idle
	DragSystem *getDragSystem() const { return _drag; }

	// The drag this source has in flight, or null when it has none. What a source with a deferred
	// decorator calls once it finally has something to show - and null is the ordinary answer for
	// anything that arrives after the drop.
	DragSession *getSession() const;

protected:
	using InputListener::init;

	virtual bool handleDragBegin(const GestureSwipe &);
	virtual void handleDragMove(const GestureSwipe &);
	virtual void handleDragEnd(bool cancelled);

	OfferBuilder _builder;

	// raw: the system lives on the scene content, which outlives any source; cleared on exit
	DragSystem *_drag = nullptr;
	bool _dragging = false;

	// The owner left the scene while this drag was in flight. Kept so the recognizer teardown that
	// follows is not mistaken for the user releasing the pointer - see handleExit.
	bool _detached = false;
};

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_DRAG_XLDRAGSOURCE_H_
