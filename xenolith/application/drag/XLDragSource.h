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

A press becomes a drag after `threshold` points of travel (below the tap tolerance). On start the
source captures the pointer with setExclusive(), since the dispatcher freezes an event chain's
listeners at Begin. The drag survives the source node leaving the scene. The offer builder returns
false to refuse a drag. */
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

	// The drag this source has in flight, or null (e.g. after the drop); for deferred decorators
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

	// The owner left the scene mid-drag, so the recognizer teardown is not taken as a release
	bool _detached = false;
};

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_DRAG_XLDRAGSOURCE_H_
