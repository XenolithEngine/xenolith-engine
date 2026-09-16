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

/** One committed frame's input targets: the listeners and the hit-test registry. Both are
filled by the same visit from what nodes drew, and events are resolved against the frame the
user was looking at when they acted. */
class SP_PUBLIC InputListenerStorage : public sprt::PoolRef {
public:
	struct Rec {
		Rc<InputListener> listener;
		Rc<FocusGroup> focus;
		WindowLayer layer;
		uint32_t order = 0;
	};

	/* One node's offer to be found under a point (see HitTestFlags). Rc because a hit-test callback
	may restructure the scene, including deleting this node; a removed node lives one more frame. */
	struct HitTestRec {
		Rc<Node> node;

		// The AABB of the drawn rect: a cheap reject before the exact test, not the answer
		Rect worldRect;

		// The clip the node was drawn under; a point outside it does not hit
		URect scissor;

		float opacity = 1.0f;
		HitTestFlags flags = HitTestFlags::None;
		uint32_t order = 0;
		bool scissorEnabled = false;

		// AABB, then scissor, then the node's own drawn geometry. `padding` is chosen by the
		// asker (hover and drop paddings differ for the same node)
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

	/* Every node offering any of `mask`, topmost first (reverse registration/paint order). The
	callback returns false to stop, true to look underneath; returns false when stopped.
	Containment is for the callback to decide with HitTestRec::contains and its own padding. */
	bool foreachHitTest(HitTestFlags mask, const Callback<bool(const HitTestRec &)> &) const;

	// Union of every registered node's flags, to skip the walk when nothing relevant registered
	HitTestFlags getHitTestMask() const { return _hitTestMask; }

	size_t getHitTestCount() const;

	/* The scene's selection chain as of this frame: the anchor, then every ancestor up to the root,
	deepest first. Empty when nothing is selected.

	Published during the visit, never read live: a hotkey callback along this chain may restructure
	the scene, so the walk holds `Rc`s from the committed frame. Written by
	SelectionSystem::handleVisitSelf; see XLSelectionSystem.h. */
	void setSelectionChain(SpanView<Rc<Node>>);

	SpanView<Rc<Node>> getSelectionChain() const { return *_selectionChain; }

	// Where `node` sits on the chain, 0 being the anchor; maxOf<size_t>() when it is not on it.
	// Sort key of the hotkey chain pass (deepest first)
	size_t getSelectionDepth(const Node *) const;

	// Which committed frame this is. Stamped on every listener at commit, so a listener reached
	// outside the walk (e.g. held by a gesture chain) can tell whether it is still drawn
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

	// In paint order, walked backwards; holds only nodes that opted in, so no spatial index
	mem_pool::Vector<HitTestRec> *_hitTest = nullptr;
	HitTestFlags _hitTestMask = HitTestFlags::None;

	// Deepest first. Rc: a hotkey callback may delete the node it was reached through
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

	// Whether the chain that began with this event id is still open (not released or cancelled),
	// for state tied to a press held outside that chain; see DragSystem::update.
	bool isEventActive(uint32_t id) const;

	const InputEvent *getPointerEvent() const {
		return _hasPointerEvent ? &_pointerEvent : nullptr;
	}

	/* "What is under this point", answered from the committed frame - see
	InputListenerStorage::foreachHitTest. Subsystems use this, not their own roster. */
	bool foreachHitTest(HitTestFlags mask,
			const Callback<bool(const InputListenerStorage::HitTestRec &)> &) const;

	// Union of the committed frame's hit-test flags; None when nothing registered (or before the
	// first frame)
	HitTestFlags getHitTestMask() const;

	/* The selection chain of the committed frame, deepest first. Asked here rather than of
	SelectionSystem, whose live selection may have moved since. Empty before the first frame. */
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

	   Returns true when a subscriber consumed the combination: the key never reaches the listener
	   storage, so no chain is opened and the matching release is a no-op. Returns false (also
	   for a hotkey nobody handled) and the key is dispatched normally. */
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

	// Monotonic and never reset; the committed storage carries the current value
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
