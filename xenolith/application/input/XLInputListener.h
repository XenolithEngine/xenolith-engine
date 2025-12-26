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

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Node;
class Scene;
class GestureRecognizer;
class FocusGroup;

class SP_PUBLIC InputListener : public System {
public:
	using EventMask = std::bitset<toInt(InputEventName::Max)>;
	using ButtonMask = std::bitset<toInt(InputMouseButton::Max)>;
	using KeyMask = std::bitset<toInt(InputKeyCode::Max)>;

	template <typename T>
	using InputCallback = Function<bool(const T &)>;

	using DefaultEventFilter = std::function<bool(const InputEvent &)>;
	using EventFilter = Function<bool(const InputEvent &, const DefaultEventFilter &)>;

	virtual ~InputListener() = default;

	bool init(int32_t priority = 0);

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;
	virtual void handleVisitSelf(FrameInfo &, Node *, NodeVisitFlags flags) override;

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

	// try to set focus on this listener
	bool setFocused();
	bool isFocused() const;

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

	void setWindowStateCallback(Function<bool(WindowState, WindowState)> &&);

	void clear();

	bool hasBackButtonRecognizer() const;

protected:
	friend class FocusGroup;

	void handleFocusIn(FocusGroup *);
	void handleFocusOut(FocusGroup *);

	bool shouldProcessEvent(const InputEvent &) const;
	bool _shouldProcessEvent(const InputEvent &) const; // default realization

	void addEventMask(const EventMask &);

	using EventCallback = std::variant<Function<bool()>, Function<bool(WindowState, WindowState)>>;

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

	Scene *_scene = nullptr;

	EventFilter _eventFilter;
	Vector<Rc<GestureRecognizer>> _recognizers;
	Map<InputEventName, EventCallback> _callbacks;
	Map<InputEventName, uint32_t> _retainedEvents;
	Function<void(bool)> _focusCallback;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_INPUT_XLINPUTLISTENER_H_ */
