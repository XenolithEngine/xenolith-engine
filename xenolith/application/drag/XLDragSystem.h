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

/** One drag, from begin to exactly one terminal event.

Between begin and the drop the session only reads; structural changes happen inside the target's
`drop` slot, which may destroy the source. Every ending (drop, cancel, Escape, source or system
leaving the scene) goes through `finish()`, which runs at most once; the system detaches the session
before calling it, so re-entry during the drop is a no-op. */
class SP_PUBLIC DragSession : public Ref {
public:
	virtual ~DragSession() = default;

	virtual bool init(NotNull<DragSystem>, DragOffer &&, Rc<Ref> &&source, uint32_t inputEventId);

	DragData *getData() const { return _data; }
	Ref *getSource() const { return _source; }
	// The node the drag is currently over, or null. Its DropTargetComponent is what answered
	Node *getTarget() const { return _target; }
	Node *getDecorator() const { return _decorator; }

	// Install (or replace) the node that follows the pointer, for DragOffer::decoratorDeferred.
	// Ignored once the drag has finished.
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

	// The input event id of the Begin that started this drag; 0 for a drag begun from code.
	// Reserved for a Wayland start_drag serial or an X11 grab timestamp
	uint32_t getInputEventId() const { return _inputEventId; }

	// Always false for now (no external drags). A target limited to in-process data should check it
	bool isExternal() const { return false; }

	bool isFinished() const { return _finished; }

protected:
	friend class DragSystem;

	virtual void update(const Vec2 &world, InputModifier);

	// The single funnel. `performDrop` false means cancel. Runs its body at most once
	virtual void finish(bool performDrop);

	// The current target left the scene, or stopped being one: leave fires, the drag continues
	virtual void handleTargetGone(NotNull<Node>);

	// Park the node on the decorator parent, on the Overlay level
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

Lives on `SceneContent` (`acquireForNode` installs it there): the full-window node reachable from
every descendant. Nesting is not allowed - `findForNode` would give widgets below a second system a
different drag. Public calls use `findForNode`, since gesture and command callbacks run outside a
visit. Drop targets are read from the window's hit-test registry (see HitTestFlags), not a roster.

During a drag the pointer is over the target, so the cursor is set by this system's own
InputListener on its owner: disabled when idle, no recognizers, at a negative priority that puts it
in the dispatcher's post-scene band. The window applies the last non-Undefined cursor it gets. */
class SP_PUBLIC DragSystem : public System {
public:
	static uint64_t Id;

	// Orders the decorator within the Overlay level, below WindowDecorations (ZOrder::max() - 1).
	// Must not be shared: sortAllChildren is unstable, so equal ZOrders permute between frames
	static constexpr ZOrder DecoratorZOrder = ZOrder::max() - ZOrder(16);

	// Applied after every widget's cursor; SceneContent's own listener sits at -1
	static constexpr int32_t CursorListenerPriority = -0x4000;

	// Travel before a press becomes a drag; must stay below the tap tolerance (TapDistanceAllowed)
	static constexpr float DefaultDragThreshold = 8.0f;

	// Walks the parent chain. Use this everywhere except inside a visit
	static DragSystem *findForNode(Node *);

	// findForNode, installing one on the scene's content node if there is none
	static DragSystem *acquireForNode(Node *);

	virtual ~DragSystem() = default;

	virtual bool init() override;

	virtual void handleAdded(Node *) override;
	virtual void handleRemoved() override;
	virtual void handleExit() override;

	virtual void update(const UpdateTime &) override;

	// Starts a drag. Null if one is already in flight, if the offer allows no action, or if it
	// asks to go external (no OS path yet). `source` is retained for the whole drag
	virtual DragSession *beginDrag(DragOffer &&, Rc<Ref> &&source, uint32_t inputEventId = 0);

	// `worldLocation` is world (screen) space, physical pixels; an absolute position, not a delta
	virtual void updateDrag(const Vec2 &worldLocation, InputModifier = InputModifier::None);

	/* Re-resolve the target at the current position, for when the scene moved under a still
	pointer (e.g. an auto-scrolling list). Call only on frames where something moved, and not from
	a visit hook: it can fire handleDragLeave/handleDragEnter, which may mutate the scene. */
	virtual void refreshDrag();

	virtual void commitDrag();

	// `source` guards against a stale abort: only that source's own drag is cancelled
	virtual void cancelDrag(Ref *source = nullptr);

	DragSession *getSession() const { return _session; }
	bool isDragging() const { return _session != nullptr; }

	// A target left the scene. If it is the current one it gets its `leave`; the drag goes on
	void handleTargetGone(NotNull<Node>);

	// How many drop targets the committed frame registered in the hit-test registry
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
