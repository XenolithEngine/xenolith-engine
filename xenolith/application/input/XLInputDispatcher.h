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
#include "XLFocusGroup.h"
#include "XLInputListener.h"
#include "XLTextInputManager.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class DirectorWindow;

class SP_PUBLIC InputListenerStorage : public PoolRef {
public:
	struct Rec {
		Rc<InputListener> listener;
		Rc<FocusGroup> focus;
		WindowLayer layer;
		uint32_t order = 0;
	};

	virtual ~InputListenerStorage();

	InputListenerStorage(PoolRef *);

	void clear();
	void reserve(const InputListenerStorage *);

	void addListener(NotNull<InputListener>, FocusGroup *, WindowLayer &&);

	void sort();

	template <typename Callback>
	bool foreachListener(const Callback &, FocusGroup *);

	template <typename Callback>
	bool foreachFocusGroup(const Callback &, FocusGroup *parentGroup);

	SpanView<Rec *> getFocusGroupListener(FocusGroup *) const;

protected:
	memory::vector<Rec> *_preSceneEvents = nullptr;
	memory::vector<Rec> *_sceneEvents = nullptr; // in reverse order
	memory::vector<Rec> *_postSceneEvents = nullptr;
	memory::map<FocusGroup *, memory::vector<Rec *>> *_focus = nullptr;
	uint32_t _order = 0;
};

class SP_PUBLIC InputDispatcher : public Ref {
public:
	virtual ~InputDispatcher() = default;

	bool init(PoolRef *, WindowState state);

	void update(const UpdateTime &time);

	Rc<InputListenerStorage> acquireNewStorage();
	void commitStorage(AppWindow *, Rc<InputListenerStorage> &&);

	void handleInputEvent(const InputEventData &);

	Vector<InputEventData> getActiveEvents() const;

	void setListenerExclusive(const InputListener *l);
	void setListenerExclusiveForTouch(const InputListener *l, uint32_t);
	void setListenerExclusiveForKey(const InputListener *l, InputKeyCode);

	WindowState getWindowState() const { return _windowState; }
	bool hasActiveInput() const;

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

	void cancelTouchEvents(float x, float y, InputModifier mods);
	void cancelKeyEvents(float x, float y, InputModifier mods);

	uint64_t _currentTime = 0;
	HashMap<uint32_t, EventHandlersInfo> _activeEvents;
	HashMap<InputKeyCode, EventHandlersInfo> _activeKeys;
	HashMap<uint32_t, EventHandlersInfo> _activeKeySyms;
	Rc<InputListenerStorage> _events;
	Rc<InputListenerStorage> _tmpEvents;
	Rc<PoolRef> _pool;

	Vec2 _pointerLocation = Vec2::ZERO;
	WindowState _windowState = WindowState::None;
};

template <typename Callback>
bool InputListenerStorage::foreachListener(const Callback &cb, FocusGroup *focus) {
	static_assert(std::is_invocable_v<Callback, const Rec &>, "Invalid callback type");

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

	memory::vector<Rec>::reverse_iterator it, end;
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
	static_assert(std::is_invocable_v<Callback, NotNull<FocusGroup>, SpanView<Rec *>>,
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
