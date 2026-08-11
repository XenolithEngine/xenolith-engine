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
	DropTarget *getTarget() const { return _target; }
	Node *getDecorator() const { return _decorator; }

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

	// The current target left the scene: leave fires, the drag continues
	virtual void handleTargetGone(NotNull<DropTarget>);

	DragEvent makeEvent(DropTarget *) const;
	void setTarget(DropTarget *, DragActions resolved);
	void updateDecorator();
	void teardown();

	DragSystem *_system = nullptr;
	DragOffer _offer;

	Rc<DragData> _data;

	// Rc, not raw: the source is routinely destroyed by the very drop that ends this drag
	Rc<Ref> _source;
	Rc<DropTarget> _target;
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
`Scene::addChild` is protected it is also the only place the decorator could be parked. Nesting is
asserted against: the frame stack hands a descendant the NEAREST system with this tag, so a second
DragSystem deeper in the tree would silently take half the drop targets with it.

HOW IT IS REACHED. Two ways, and the distinction matters. `DropTarget` registers through the frame
stack, because it runs inside a visit. Everything public here runs OUTSIDE one - a gesture callback,
a command handler - and the frame stack is dead by then, so those use `findForNode`, which walks
the parent chain. Same split as FormSystem.

CURSOR. During a drag the pointer is over the TARGET, not the source, so nothing the source owns
can set the cursor. Instead this system keeps its own InputListener on its owner - disabled when
idle, no recognizers, nothing but a window layer - at a deeply negative priority. Negative-priority
listeners land in the dispatcher's post-scene band, which is walked last, and the window applies the
LAST non-Undefined cursor it is handed. So this one layer covers the window and outranks every
widget under the pointer, for exactly as long as the drag lasts. */
class SP_PUBLIC DragSystem : public System {
public:
	static uint64_t Id;

	// Above ordinary content and above the basic2d overlay stack, but strictly below
	// WindowDecorations at ZOrder::max() - 1: a drag ghost must not paint over the title bar.
	// A band of its own, shared with nothing - sortAllChildren is unstable, so equal ZOrders
	// permute between frames
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

	virtual void handleVisitBegin(FrameInfo &) override;
	virtual void handleVisitEnd(FrameInfo &) override;

	// Starts a drag. Null if one is already in flight, if the offer allows no action, or if it
	// asks to go external (v1 has no OS path). `source` is retained for the whole drag
	virtual DragSession *beginDrag(DragOffer &&, Rc<Ref> &&source, uint32_t inputEventId = 0);

	// `worldLocation` is world (screen) space, physical pixels - what an input event carries.
	// Never accumulate deltas: this is a position, and the drag is a fixed point of it
	virtual void updateDrag(const Vec2 &worldLocation, InputModifier = InputModifier::None);

	virtual void commitDrag();

	// `source` guards against a stale abort: a source leaving the scene cancels only its OWN drag
	virtual void cancelDrag(Ref *source = nullptr);

	DragSession *getSession() const { return _session; }
	bool isDragging() const { return _session != nullptr; }

	// Registration point for DropTarget, meaningful only during a visit
	void addTarget(NotNull<DropTarget>, const Rect &worldRect);

	// A target left the scene. If it is the current one it gets its `leave`, and the drag goes on:
	// a target disappearing mid-drag is ordinary, not a reason to abort
	void handleTargetGone(NotNull<DropTarget>);

	// Size of the committed roster - the targets that were visited last frame
	size_t getTargetCount() const { return _targets.size(); }

	InputListener *getCursorListener() const { return _cursorListener; }

protected:
	friend class DragSession;

	struct TargetRec {
		Rc<DropTarget> target;
		Rect worldRect;
	};

	void setCursor(WindowCursor);

	// Written during the visit, read from input callbacks one frame later. NOT pool-allocated:
	// it outlives the frame that filled it
	Vector<TargetRec> _targets;
	Vector<TargetRec> _pendingTargets;

	Rc<DragSession> _session;
	Rc<InputListener> _cursorListener;
};

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_DRAG_XLDRAGSYSTEM_H_
