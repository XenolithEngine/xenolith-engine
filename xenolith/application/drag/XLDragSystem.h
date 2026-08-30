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

#ifndef XENOLITH_APPLICATION_DRAG_XLDRAGSYSTEM_H_
#define XENOLITH_APPLICATION_DRAG_XLDRAGSYSTEM_H_

#include "XLDragTypes.h"
#include "XLDropTarget.h"
#include "XLInputListener.h"
#include "XLSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class DragSystem;
class InputDispatcher;

/** One drag, from the moment it begins to exactly one terminal event.

THE INVARIANT WORTH KEEPING: between begin and the drop, the session only READS. Every structural
change happens in one shot inside the target's `drop` slot. Without that rule a drop that removes
the source node - which is the normal case, not an exotic one: it is what moving a dock panel does -
would destroy the very object delivering the drag events, halfway through delivering them.

The second half of the same guarantee is `finish()`: every way this can end (drop, cancel, Escape,
the source leaving the scene, the whole system leaving the scene) funnels through it, and it runs
its body at most once. The system detaches the session from itself BEFORE calling it, so anything
that re-enters during the drop finds no drag in flight and becomes a no-op instead of recursing. */
class SP_PUBLIC DragSession : public Ref {
public:
	virtual ~DragSession() = default;

	virtual bool init(NotNull<DragSystem>, DragOffer &&, Rc<Ref> &&source, uint32_t inputEventId);

	DragData *getData() const { return _data; }
	Ref *getSource() const { return _source; }
	// The node the drag is currently over, or null. Its DropTargetComponent is what answered
	Node *getTarget() const { return _target; }
	Node *getDecorator() const { return _decorator; }

	// Install (or replace) the node that follows the pointer. For a source that set
	// DragOffer::decoratorDeferred and had nothing to show yet at beginDrag. Ignored once the drag
	// has finished, so a capture that lands late is a no-op rather than a leak.
	void setDecorator(Rc<Node> &&);

	DragActions getAllowedActions() const { return _offer.allowedActions; }

	// The single action the modifiers ask for right now, clamped to what the source allows. A
	// preference: a target that cannot do it may still accept something else
	DragActions getPreferredAction() const { return _preferred; }

	// The single action a drop right here and now would perform; None when there is nowhere
	// to drop
	DragActions getResolvedAction() const { return _resolved; }

	const Vec2 &getWorldLocation() const { return _world; }
	InputModifier getModifiers() const { return _modifiers; }

	// The input event id of the Begin that started this drag. Unused in v1; it is what a Wayland
	// start_drag serial and an X11 grab timestamp will have to be derived from
	uint32_t getInputEventId() const { return _inputEventId; }

	// Always false in v1. A target that can only handle in-process data should branch on it now,
	// so it keeps working unchanged once external drags exist
	bool isExternal() const { return false; }

	bool isFinished() const { return _finished; }

protected:
	friend class DragSystem;

	virtual void update(const Vec2 &world, InputModifier);

	// The single funnel. `performDrop` false means cancel. Runs its body at most once
	virtual void finish(bool performDrop);

	// The current target left the scene, or stopped being one: leave fires, the drag continues
	virtual void handleTargetGone(NotNull<Node>);

	// Park the node on the decorator parent, on the Overlay level. See the definition for why the
	// level rather than the ZOrder is what matters.
	void installDecorator(Rc<Node> &&);

	DragEvent makeEvent(Node *target) const;
	void setTarget(Node *, DragActions resolved);
	void updateDecorator();
	void teardown();

	DragSystem *_system = nullptr;
	DragOffer _offer;

	Rc<DragData> _data;

	// Rc, not raw: the source is routinely destroyed by the very drop that ends this drag
	Rc<Ref> _source;
	Rc<Node> _target;
	Rc<Node> _decorator;
	Rc<Node> _decoratorParent;

	Vec2 _world;
	InputModifier _modifiers = InputModifier::None;
	DragActions _preferred = DragActions::None;
	DragActions _resolved = DragActions::None;
	WindowCursor _cursor = WindowCursor::Undefined;
	uint32_t _inputEventId = 0;
	bool _finished = false;
};

/** The drag coordinator. One per scene, on the SceneContent.

    auto drag = DragSystem::acquireForNode(this);
    drag->beginDrag(DragOffer{ ... }, this, swipe.getId());

WHERE IT LIVES. On `SceneContent`, and `acquireForNode` puts it there if nobody did. That node is
the only one at this layer that is both full-window and reachable from every descendant, and since
`Scene::addChild` is protected it is also the only place the decorator could be parked. A second one
deeper in the tree is no longer a silent trap - targets are the registry's, not any system's - but
`findForNode` would hand the widgets below it a different drag from the ones above, so don't.

HOW IT IS REACHED. `findForNode` walks the parent chain, and everything public here uses it: a
gesture callback and a command handler both run outside any visit. Drop TARGETS do not come through
here at all - a node publishes itself into the window's hit-test registry (see HitTestFlags) and this
system reads that registry, so there is no roster here to keep in step with the scene.

CURSOR. During a drag the pointer is over the TARGET, not the source, so nothing the source owns
can set the cursor. Instead this system keeps its own InputListener on its owner - disabled when
idle, no recognizers, nothing but a window layer - at a deeply negative priority. Negative-priority
listeners land in the dispatcher's post-scene band, which is walked last, and the window applies the
LAST non-Undefined cursor it is handed. So this one layer covers the window and outranks every
widget under the pointer, for exactly as long as the drag lasts. */
class SP_PUBLIC DragSystem : public System {
public:
	static uint64_t Id;

	// The decorator is put on the Overlay LEVEL (see DragSession::installDecorator), which is what
	// puts it above ordinary content and, crucially, after the frame has been captured. This ZOrder
	// is what orders it against the other things on that level - WindowDecorations at
	// ZOrder::max() - 1, which a drag ghost must not paint over. A band of its own, shared with
	// nothing: sortAllChildren is unstable, so equal ZOrders permute between frames
	static constexpr ZOrder DecoratorZOrder = ZOrder::max() - ZOrder(16);

	// Deeply negative so the drag's cursor layer is applied after every widget's; see the class
	// comment. SceneContent's own listener sits at -1
	static constexpr int32_t CursorListenerPriority = -0x4000;

	// How far a pointer must travel before a press becomes a drag. Above the tap tolerance
	// (TapDistanceAllowed = 12) would make a drag impossible to start without first cancelling a
	// tap; below it, a click would start a drag
	static constexpr float DefaultDragThreshold = 8.0f;

	// Walks the parent chain. Use this everywhere except inside a visit
	static DragSystem *findForNode(Node *);

	// findForNode, and if there is none, installs one on the scene's content node. This is what
	// lets a widget start a drag without the application having arranged anything
	static DragSystem *acquireForNode(Node *);

	virtual ~DragSystem() = default;

	virtual bool init() override;

	virtual void handleAdded(Node *) override;
	virtual void handleRemoved() override;
	virtual void handleExit() override;

	virtual void update(const UpdateTime &) override;

	// Starts a drag. Null if one is already in flight, if the offer allows no action, or if it
	// asks to go external (v1 has no OS path). `source` is retained for the whole drag
	virtual DragSession *beginDrag(DragOffer &&, Rc<Ref> &&source, uint32_t inputEventId = 0);

	// `worldLocation` is world (screen) space, physical pixels - what an input event carries.
	// Never accumulate deltas: this is a position, and the drag is a fixed point of it
	virtual void updateDrag(const Vec2 &worldLocation, InputModifier = InputModifier::None);

	/* Re-resolve what the drag is over, at the position it is already at.

	For when the SCENE moved under a pointer that did not: a list auto-scrolling at its edge, a
	panel animating into place. Drag events arrive only on pointer motion - DragSession::update is
	called from DragSource::handleDragMove and nowhere else - so after such a move everything the
	drag decided is about a target that has slid away, and nothing will say so.

	Call it only on a frame where something actually moved. Calling it every frame regardless is the
	thing this exists to avoid: it would make handleDragOver a 60Hz event for every drag everywhere
	in order to fix a case that arises only while a scroller is scrolling itself.

	Not from a visit hook: this can fire handleDragLeave/handleDragEnter, and a target is entitled
	to mutate the scene in those. */
	virtual void refreshDrag();

	virtual void commitDrag();

	// `source` guards against a stale abort: a source leaving the scene cancels only its OWN drag
	virtual void cancelDrag(Ref *source = nullptr);

	DragSession *getSession() const { return _session; }
	bool isDragging() const { return _session != nullptr; }

	// A target left the scene. If it is the current one it gets its `leave`, and the drag goes on:
	// a target disappearing mid-drag is ordinary, not a reason to abort
	void handleTargetGone(NotNull<Node>);

	// How many drop targets the committed frame registered. Answered by the hit-test registry, which
	// is the only place that knows: this system keeps no roster of its own
	size_t getTargetCount() const;

	InputListener *getCursorListener() const { return _cursorListener; }

protected:
	friend class DragSession;

	void setCursor(WindowCursor);

	// The window's input dispatcher, which owns the hit-test registry. Null outside a scene
	InputDispatcher *getDispatcher() const;

	Rc<DragSession> _session;
	Rc<InputListener> _cursorListener;
};

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_DRAG_XLDRAGSYSTEM_H_
