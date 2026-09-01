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

#ifndef XENOLITH_APPLICATION_INPUT_XLINPUTLISTENER_H_
#define XENOLITH_APPLICATION_INPUT_XLINPUTLISTENER_H_

#include "XLSystem.h"
#include "XLNodeInfo.h"
#include "XLGestureRecognizer.h"
#include "XLHotkey.h"

#include <sprt/cxx/variant>

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Node;
class Scene;
class GestureRecognizer;
class FocusGroup;

class SP_PUBLIC InputListener : public System {
public:
	using EventMask = sprt::bitset<toInt(InputEventName::Max)>;
	using ButtonMask = sprt::bitset<toInt(InputMouseButton::Max)>;
	using KeyMask = sprt::bitset<toInt(InputKeyCode::Max)>;

	template <typename T>
	using InputCallback = Function<bool(const T &)>;

	using DefaultEventFilter = mem_std::Function<bool(const InputEvent &)>;
	using EventFilter = Function<bool(const InputEvent &, const DefaultEventFilter &)>;

	virtual ~InputListener() = default;

	bool init(int32_t priority = 0);

	virtual void handleAdded(Node *) override;
	virtual void handleRemoved() override;
	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;
	virtual void handleVisitSelf(FrameInfo &, Node *, NodeVisitFlags flags) override;
	virtual void handleTransformDirty(const Mat4 &) override;
	virtual void settlePointerState() override;

	virtual void update(const UpdateTime &) override;

	// Unique listener id; always > 0
	uint64_t getId() const;

	void setCursor(WindowCursor);
	WindowCursor getCursor() const { return _windowLayer.cursor; }

	void setLayerFlags(WindowLayerFlags);
	WindowLayerFlags getLayerFlags() const { return _windowLayer.flags; }

	void setOwner(Node *pOwner);
	Node *getOwner() const { return _owner; }

	void setPriority(int32_t);
	int32_t getPriority() const { return _priority; }

	void setOpacityFilter(float value) { _opacityFilter = value; }
	float getOpacityFilter() const { return _opacityFilter; }

	void setTouchPadding(float value) { _touchPadding = value; }
	float getTouchPadding() const { return _touchPadding; }

	// For all currently active events (pointer/touch or keyboard) with this listener,
	// make this listener exclusive responder. All other listeners will receive Cancel events
	void setExclusive();

	// For all currently active pointer/touch events with specific id and this listener,
	// make this listener exclusive responder. All other listeners will receive Cancel events
	void setExclusiveForTouch(uint32_t eventId);

	// Event swallow means that for eny event with this name, InputEventState::Processed will become
	// InputEventState::Captured.
	// In other words, any event in swallow mask can be declined or processed exclusively
	//
	// Note that this listener can be not the first listener, that recevies this event. In this case,
	// previous listener will receive cancel event.
	void setSwallowEvents(EventMask &&);
	void setSwallowEvents(const EventMask &);
	void setSwallowAllEvents();
	void setSwallowEvent(InputEventName);

	void clearSwallowAllEvents();
	void clearSwallowEvent(InputEventName);
	void clearSwallowEvents(const EventMask &);

	bool isSwallowAllEvents() const;
	bool isSwallowAllEvents(const EventMask &) const;
	bool isSwallowAnyEvents(const EventMask &) const;
	bool isSwallowEvent(InputEventName) const;

	void setTouchFilter(const EventFilter &);

	bool shouldSwallowEvent(const InputEvent &) const;
	bool canHandleEvent(const InputEvent &event) const;
	InputEventState handleEvent(const InputEvent &event);

	void updatePointerState();

	// try to set focus on this listener
	bool setFocused();
	bool isFocused() const;

	// Fired from handleFocusIn/handleFocusOut. A subclass should override those instead; this is
	// for the listeners that are not subclassed
	void setFocusCallback(Function<void(bool)> &&);

	FocusGroup *getFocusGroup() const;

	GestureRecognizer *addTouchRecognizer(InputCallback<GestureData> &&,
			InputTouchInfo && = InputTouchInfo());
	GestureRecognizer *addTapRecognizer(InputCallback<GestureTap> &&,
			InputTapInfo && = InputTapInfo());
	GestureRecognizer *addPressRecognizer(InputCallback<GesturePress> &&,
			InputPressInfo && = InputPressInfo());
	GestureRecognizer *addSwipeRecognizer(InputCallback<GestureSwipe> &&,
			InputSwipeInfo && = InputSwipeInfo());
	GestureRecognizer *addPinchRecognizer(InputCallback<GesturePinch> &&,
			InputPinchInfo && = InputPinchInfo());
	GestureRecognizer *addScrollRecognizer(InputCallback<GestureScroll> &&,
			InputScrollInfo && = InputScrollInfo());
	GestureRecognizer *addMoveRecognizer(InputCallback<GestureData> &&,
			InputMoveInfo && = InputMoveInfo());
	GestureRecognizer *addMouseOverRecognizer(InputCallback<GestureData> &&,
			InputMouseOverInfo && = InputMouseOverInfo());

	GestureKeyRecognizer *addKeyRecognizer(InputCallback<GestureData> &&,
			InputKeyInfo && = InputKeyInfo());

	/* Subscribe to a global hotkey (see XLHotkey.h). Return true from the callback to consume the
	   key: the dispatcher stops the walk there and the ordinary key route never runs.

	   Unlike a key recognizer, this needs no key mask and is NOT hit-tested against the pointer
	   position — the dispatcher calls handleHotkey directly, bypassing canHandleEvent and the
	   touch filter. What still applies is the focus group: see HotkeyFlags::FocusedOnly. */
	void addHotkey(HotkeyId, HotkeyCallback &&, HotkeyFlags = HotkeyFlags::None);
	void removeHotkey(HotkeyId);
	bool hasHotkey(HotkeyId) const;

	// Any of `ids` this listener is subscribed to and is eligible for under `ctx` - see
	// HotkeyContext, which carries what each HotkeyFlags value is tested against
	bool canHandleHotkey(SpanView<HotkeyId> ids, const HotkeyContext &ctx) const;

	// Delivers the first matching binding; returns true when the callback consumed the hotkey
	bool handleHotkey(SpanView<HotkeyId> ids, const InputEvent &, const HotkeyContext &ctx);

	void setWindowStateCallback(Function<bool(WindowState, WindowState)> &&);

	void clear();

	bool hasBackButtonRecognizer() const;

protected:
	friend class FocusGroup;

	// Stamps _visitGeneration when the storage this listener registered into is committed
	friend class InputDispatcher;

	virtual void handleFocusIn(FocusGroup *);
	virtual void handleFocusOut(FocusGroup *);

	bool shouldProcessEvent(const InputEvent &) const;
	bool _shouldProcessEvent(const InputEvent &) const; // default realization

	void addEventMask(const EventMask &);

	struct HotkeyBinding {
		HotkeyCallback callback;
		HotkeyFlags flags = HotkeyFlags::None;
	};

	// True when this binding is eligible for the current delivery pass
	bool isHotkeyEligible(const HotkeyBinding &, const HotkeyContext &) const;

	using EventCallback = sprt::variant<Function<bool()>, Function<bool(WindowState, WindowState)>>;

	GestureRecognizer *addRecognizer(GestureRecognizer *);

	void retainEvent(core::InputEventName);
	void releaseEvent(core::InputEventName);
	void makeDelay();

	int32_t _priority = 0; // 0 - scene graph
	uint64_t _id = 0;
	EventMask _eventMask;
	EventMask _swallowEvents;
	WindowLayer _windowLayer;

	// Set if listener is in focus
	FocusGroup *_focusGroup = nullptr;

	float _touchPadding = 0.0f;
	float _opacityFilter = 0.0f;
	bool _hasFocus = false;

	bool _visitScissorEnabled = false;
	URect _visitScissor;

	/* Which committed frame this listener was last drawn in - stamped by InputDispatcher at commit.

	It is what stands in for the old walk up the parent chain asking every ancestor whether it is
	visible: a listener whose owner was not visited never registered, so it is not in the committed
	storage and this does not match. Cheaper than the walk, and it answers about the frame the event
	is actually being resolved against rather than about the tree as it is right now. A listener
	reached OUTSIDE that walk - an active gesture chain holds the ones it captured - is exactly the
	case that needs asking. */
	uint64_t _visitGeneration = 0;

	// The owner's opacity as of that frame, for _opacityFilter
	float _visitOpacity = 1.0f;

	// Whether any recognizer here keeps state derived from a hit test against the owner
	// (GestureRecognizer::requiresGeometryUpdate) - if none does, there is nothing to settle
	bool _geometryRecognizers = false;

	Scene *_scene = nullptr;

	EventFilter _eventFilter;
	Vector<Rc<GestureRecognizer>> _recognizers;
	Map<InputEventName, EventCallback> _callbacks;
	Map<InputEventName, uint32_t> _retainedEvents;
	Map<HotkeyId, HotkeyBinding> _hotkeys;
	Function<void(bool)> _focusCallback;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_INPUT_XLINPUTLISTENER_H_ */
