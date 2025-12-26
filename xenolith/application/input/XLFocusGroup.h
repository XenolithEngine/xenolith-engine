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

#ifndef XENOLITH_APPLICATION_INPUT_XLFOCUSGROUP_H_
#define XENOLITH_APPLICATION_INPUT_XLFOCUSGROUP_H_

#include "XLSystem.h"
#include "XLGestureRecognizer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class SP_PUBLIC FocusGroup : public System {
public:
	enum class Flags : uint32_t {
		None = 0,

		// Только один слушатель может получать события группы
		SingleFocus = 1 << 0,

		// Если флаг установлен, эксклюзивная группа будет распространять события во вложенные группы
		Propagate = 1 << 1,

		// Эксклюзивные группы исключают получение событий другими группами: если оно может быть получено эксклюзивной группой.
		// Группы, вложенные в эксклюзивные, могут также получать события, если установлен флаг Propagate.
		// Важно: события не перехватываются, если ни один обработчик в группе их не обрабатывает
		Exclusive = 1 << 2,
	};

	using EventMask = std::bitset<toInt(InputEventName::Max)>;

	static uint64_t Id;

	virtual ~FocusGroup() = default;

	virtual bool init() override;

	// Check if Event is in group's event mask
	virtual bool canHandleEvent(const InputEvent &);

	// Check if event should be delivered to listener, called after `canHandleEvent`
	// It works like a filter on literners in this group
	// Normally, focus can allow only one listener for event
	virtual bool canHandleEventWithListener(const InputEvent &, NotNull<InputListener>);

	// For exclusive focus groups, the group with the highest priority is selected,
	// the first one encountered on the scene graph among groups with the same priority.
	virtual uint32_t getPriority() const;

	virtual bool isParentGroup(FocusGroup *) const;

	virtual void setEventMask(EventMask &&);

	virtual void setFlags(Flags);
	virtual Flags getFlags() const { return _flags; }

	virtual bool setFocus(InputListener *);

protected:
	friend class InputDispatcher;

	virtual void updateWithListeners(SpanView<InputListener *>);

	bool _exclusive = false;
	uint32_t _priority = 0;
	Flags _flags = Flags::None;
	EventMask _eventMask;

	// We store only Id to avoid dangling pointers
	uint64_t _focusedListener = 0;
	uint64_t _nextListener = 0;
	Vector<uint64_t> _listeners;
};

SP_DEFINE_ENUM_AS_MASK(FocusGroup::Flags)

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_INPUT_XLFOCUSGROUP_H_
