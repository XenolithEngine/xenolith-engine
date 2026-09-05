/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef XENOLITH_APPLICATION_INPUT_XLINPUTDISPATCHER_H_
#define XENOLITH_APPLICATION_INPUT_XLINPUTDISPATCHER_H_

#include "XLContextInfo.h"
#include "XLCoreRenderSession.h"
#include "XLFocusGroup.h"
#include "XLInputListener.h"
#include "XLTextInputManager.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class DirectorWindow;

/** Everything one committed frame says about where input can land: the listeners, and the hit-test
registry every "what is under this point" question is answered from.

Both halves are filled by the same visit, at the same moment, and by the same rule - a node
publishes what it DREW - so they cannot disagree about geometry, and both go stale together when the
next frame is committed. That staleness is the contract, not a compromise: an event is resolved
against the frame the user was looking at when they acted. */
class SP_PUBLIC InputListenerStorage : public sprt::PoolRef {
public:
	struct Rec {
		Rc<InputListener> listener;
		Rc<FocusGroup> focus;
		WindowLayer layer;
		uint32_t order = 0;
	};

	/* One node's offer to be found under a point (see HitTestFlags).

	Rc, like the Rc<InputListener> above it and for the same reason: a hit test hands the node to a
	callback that is entitled to restructure the scene, up to and including deleting the node it was
	just given. The cost is that a removed node outlives its removal by one committed frame. */
	struct HitTestRec {
		Rc<Node> node;

		// The AABB of the drawn rect. A cheap reject before the exact test, which is what makes a
		// long registry affordable; it is NOT the answer, since a rotated node covers less than its
		// own bounding box
		Rect worldRect;

		// The clip the node was drawn under. A point outside it is a point the user cannot see, so
		// it is not one they can hit either
		URect scissor;

		float opacity = 1.0f;
		HitTestFlags flags = HitTestFlags::None;
		uint32_t order = 0;
		bool scissorEnabled = false;

		// AABB, then scissor, then the node's own drawn geometry. `padding` is the ASKER'S, not the
		// record's: how far outside itself a target reaches is a property of what is being asked
		// (a hover padding and a drop padding are different numbers about the same node)
		bool contains(const Vec2 &world, float padding = 0.0f) const;
	};

	virtual ~InputListenerStorage();

	InputListenerStorage(PoolRef *);

	void clear();
	void reserve(const InputListenerStorage *);

	void addListener(NotNull<InputListener>, FocusGroup *, WindowLayer &&);

	// Registration point for a node with HitTestFlags; meaningful only during a visit. `scissor` is
	// null when the node was drawn unclipped
	void addHitTest(NotNull<Node>, const Mat4 &worldTransform, const Size2 &, HitTestFlags,
			float opacity, const URect *scissor);

	/* Every node offering any of `mask`, TOPMOST FIRST - registration order is visit order is paint
	order, so the walk runs backwards. The callback returns false to stop (it has its answer) or true
	to keep looking at whatever is underneath; this returns false when it was stopped.

	Containment is the CALLBACK's to decide, with HitTestRec::contains, because the padding belongs
	to the asker: how far outside itself a node reaches is a property of the question (a hover
	padding and a drop padding are different numbers about the same node), and a record whose own box
	misses the point may still be the answer to a question asked with one. */
	bool foreachHitTest(HitTestFlags mask, const Callback<bool(const HitTestRec &)> &) const;

	// Union of every registered node's flags. "Does this frame contain any tooltip at all" in one
	// test, so a system with nothing to do does not walk the registry to find that out
	HitTestFlags getHitTestMask() const { return _hitTestMask; }

	size_t getHitTestCount() const;

	/* The scene's selection chain as of this frame: the anchor, then every ancestor up to the root,
	DEEPEST FIRST. Empty when nothing is selected.

	PUBLISHED DURING THE VISIT, never read live - the same contract as the hit-test registry above,
	and for a sharper reason. A hotkey delivered along this chain reaches a callback that is entitled
	to restructure the scene, up to and including deleting the node it was just given, so the walk
	must be over `Rc`s taken from the frame the user was actually looking at when they pressed the
	key. Re-deriving it from getParent() at event time would resolve against a graph that may already
	differ, and would hold raw pointers across exactly the callback licensed to invalidate them.

	Written by SelectionSystem::handleVisitSelf; see XLSelectionSystem.h. */
	void setSelectionChain(SpanView<Rc<Node>>);

	SpanView<Rc<Node>> getSelectionChain() const { return *_selectionChain; }

	// Where `node` sits on the chain, 0 being the anchor; maxOf<size_t>() when it is not on it.
	// This is the sort key of the hotkey chain pass - deepest first is the delivery order
	size_t getSelectionDepth(const Node *) const;

	// Which committed frame this is. Stamped on every listener at commit, which is how a listener
	// reached outside the walk (an active gesture chain holds one) can tell whether it was still
	// being drawn when the event it is being offered arrived
	uint64_t getGeneration() const { return _generation; }

	void sort();

	template <typename Callback>
	bool foreachListener(const Callback &, FocusGroup *);

	template <typename Callback>
	bool foreachFocusGroup(const Callback &, FocusGroup *parentGroup);

	SpanView<Rec *> getFocusGroupListener(FocusGroup *) const;

protected:
	friend class InputDispatcher;

	mem_pool::Vector<Rec> *_preSceneEvents = nullptr;
	mem_pool::Vector<Rec> *_sceneEvents = nullptr; // in reverse order
	mem_pool::Vector<Rec> *_postSceneEvents = nullptr;
	mem_pool::Map<FocusGroup *, mem_pool::Vector<Rec *>> *_focus = nullptr;

	// In paint order, walked backwards. One vector and no spatial index: the registry holds only
	// nodes that opted in, an AABB reject is a handful of comparisons, and paint order IS the
	// semantics of the answer - an index would have to reconstruct it
	mem_pool::Vector<HitTestRec> *_hitTest = nullptr;
	HitTestFlags _hitTestMask = HitTestFlags::None;

	// Deepest first. Rc for the reason spelled out on setSelectionChain: a hotkey callback reached
	// along this may delete the very node it was reached through
	mem_pool::Vector<Rc<Node>> *_selectionChain = nullptr;

	uint64_t _generation = 0;
	uint32_t _order = 0;
};

class SP_PUBLIC InputDispatcher : public Ref {
public:
	virtual ~InputDispatcher() = default;

	bool init(sprt::PoolRef *, WindowState state);

	void update(const UpdateTime &time);

	Rc<InputListenerStorage> acquireNewStorage();
	void commitStorage(core::RenderServerChannel *, Rc<InputListenerStorage> &&);

	void handleInputEvent(const InputEventData &);

	Vector<InputEventData> getActiveEvents() const;

	void setListenerExclusive(const InputListener *l);
	void setListenerExclusiveForTouch(const InputListener *l, uint32_t);
	void setListenerExclusiveForKey(const InputListener *l, InputKeyCode);

	WindowState getWindowState() const { return _windowState; }
	bool hasActiveInput() const;

	// Whether the chain that began with this event id is still open - i.e. the pointer has not been
	// released or cancelled. For whoever holds something whose life is tied to a press but who is
	// no longer in that chain to be told when it ends; see DragSystem::update.
	bool isEventActive(uint32_t id) const;

	const InputEvent *getPointerEvent() const {
		return _hasPointerEvent ? &_pointerEvent : nullptr;
	}

	/* "What is under this point", answered from the committed frame - see
	InputListenerStorage::foreachHitTest, which this forwards to.

	The one way to ask. A subsystem that keeps a roster of its own is keeping a second copy of this
	one, filled by the same visit from the same numbers. */
	bool foreachHitTest(HitTestFlags mask,
			const Callback<bool(const InputListenerStorage::HitTestRec &)> &) const;

	// Union of the committed frame's hit-test flags; None when nothing registered (or before the
	// first frame)
	HitTestFlags getHitTestMask() const;

	/* The selection chain of the COMMITTED frame, deepest first - the anchor, then its ancestors.

	The one way to ask, and the reason it is asked here rather than of the SelectionSystem: by the
	time an event is dispatched the live selection may already have moved, and this has to answer
	for the frame the user was looking at when they acted. Empty before the first frame. */
	SpanView<Rc<Node>> getSelectionChain() const;

	// Which frame the events being dispatched right now are resolved against
	uint64_t getCommittedGeneration() const;

	// When Director connected to other window, we should update cached WindowState
	void resetWindowState(WindowState, bool propagate);

protected:
	InputEvent getEventInfo(const InputEventData &) const;
	void updateEventInfo(InputEvent &, const InputEventData &) const;

	struct EventHandlersInfo {
		InputEvent event;
		Vector<Rc<InputListener>> listeners;
		Rc<InputListener> exclusive;
		Vector<const InputListener *> processed;
		bool isKeyEvent = false;
		FocusGroup *exclusiveGroup = nullptr;

		void handle(bool removeOnFail);
		void clear(bool cancel);

		void setExclusive(const InputListener *);

		void addListenersFromStorage(NotNull<InputListenerStorage>);
	};

	void setListenerExclusive(EventHandlersInfo &, const InputListener *l) const;

	void clearKey(const InputEventData &);
	EventHandlersInfo *resetKey(const InputEventData &);
	void handleKey(const InputEventData &, bool clear);

	/* Global hotkeys, delivered ahead of the ordinary key route (see XLHotkey.h).

	   Returns true when a subscriber consumed the combination: the key then never reaches the
	   listener storage at all, so no chain is opened for it and the matching release is a no-op.
	   Returns false — including "matched a hotkey nobody handled" — and the key is dispatched
	   normally. */
	bool handleHotkey(const InputEventData &, bool repeated);

	// The Exclusive focus group that would scope this event, by the same rule
	// EventHandlersInfo::addListenersFromStorage uses. Null when no group claims it.
	FocusGroup *getExclusiveGroup(const InputEvent &) const;

	void cancelTouchEvents(float x, float y, InputModifier mods);
	void cancelKeyEvents(float x, float y, InputModifier mods);

	uint64_t _currentTime = 0;
	HashMap<uint32_t, EventHandlersInfo> _activeEvents;
	HashMap<InputKeyCode, EventHandlersInfo> _activeKeys;
	HashMap<uint32_t, EventHandlersInfo> _activeKeySyms;
	Rc<InputListenerStorage> _events;
	Rc<InputListenerStorage> _tmpEvents;

	// Ever-growing; the committed storage carries the current value. Never reset, so a stamp from
	// an old frame can never be mistaken for a current one
	uint64_t _generation = 0;
	Rc<sprt::PoolRef> _pool;

	// The last MouseMove, as the dispatcher itself saw it - see getPointerEvent()
	InputEvent _pointerEvent = InputEvent{};
	bool _hasPointerEvent = false;

	WindowState _windowState = WindowState::None;
};

template <typename Callback>
bool InputListenerStorage::foreachListener(const Callback &cb, FocusGroup *focus) {
	static_assert(sprt::is_invocable_v<Callback, const Rec &>, "Invalid callback type");

	if (focus && !hasFlag(focus->getFlags(), FocusGroup::Flags::Propagate)) {
		auto it = _focus->find(focus);
		if (it != _focus->end()) {
			for (auto &l : it->second) {
				if (!cb(*l)) {
					return false;
				}
			}
		}
		return true;
	}

	mem_pool::Vector<Rec>::reverse_iterator it, end;
	it = _preSceneEvents->rbegin();
	end = _preSceneEvents->rend();

	for (; it != end; ++it) {
		if (!focus || it->focus == focus
				|| (it->focus && hasFlag(focus->getFlags(), FocusGroup::Flags::Propagate)
						&& it->focus->isParentGroup(focus))) {
			if (!cb(*it)) {
				return false;
			}
		}
	}

	it = _sceneEvents->rbegin();
	end = _sceneEvents->rend();

	for (; it != end; ++it) {
		if (!focus || it->focus == focus
				|| (it->focus && hasFlag(focus->getFlags(), FocusGroup::Flags::Propagate)
						&& it->focus->isParentGroup(focus))) {
			if (!cb(*it)) {
				return false;
			}
		}
	}

	it = _postSceneEvents->rbegin();
	end = _postSceneEvents->rend();

	for (; it != end; ++it) {
		if (!focus || it->focus == focus
				|| (it->focus && hasFlag(focus->getFlags(), FocusGroup::Flags::Propagate)
						&& it->focus->isParentGroup(focus))) {
			if (!cb(*it)) {
				return false;
			}
		}
	}

	return true;
}

template <typename Callback>
bool InputListenerStorage::foreachFocusGroup(const Callback &cb, FocusGroup *parentGroup) {
	static_assert(sprt::is_invocable_v<Callback, NotNull<FocusGroup>, SpanView<Rec *>>,
			"Invalid callback type");

	for (auto &it : *_focus) {
		if (!parentGroup || it.first == parentGroup || it.first->isParentGroup(parentGroup)) {
			if (!cb(it.first, it.second)) {
				return false;
			}
		}
	}
	return true;
}

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_INPUT_XLINPUTDISPATCHER_H_ */
