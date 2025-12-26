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

#ifndef XENOLITH_APPLICATION_DIRECTOR_XLSCHEDULER_H_
#define XENOLITH_APPLICATION_DIRECTOR_XLSCHEDULER_H_

#include "XLNodeInfo.h"
#include "SPPriorityList.h"
#include "SPSubscription.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

using SchedulerFunc = Function<void(const UpdateTime &)>;

struct SP_PUBLIC SchedulerCallback {
	SchedulerFunc callback = nullptr;
	bool paused = false;
	bool removed = false;

	SchedulerCallback() = default;

	SchedulerCallback(SchedulerFunc &&fn, bool p) : callback(sp::move(fn)), paused(p) { }
};

class SP_PUBLIC Scheduler : public Ref {
public:
	Scheduler();
	virtual ~Scheduler();

	bool init();

	void unschedule(const void *);
	void unscheduleAll();

	template <typename T>
	void scheduleUpdate(T *, int32_t p = 0, bool paused = false);

	void schedulePerFrame(SchedulerFunc &&callback, void *target, int32_t priority, bool paused);

	void update(const UpdateTime &);

	bool isPaused(void *) const;
	void resume(void *);
	void pause(void *);

	bool empty() const;

protected:
	struct ScheduledTemporary {
		SchedulerFunc callback;
		void *target;
		int32_t priority;
		bool paused;
	};

	bool _locked = false;
	const void *_currentTarget = nullptr;
	SchedulerCallback *_currentNode = nullptr;
	PriorityList<SchedulerCallback> _list;
	Vector<ScheduledTemporary> _tmp;
};

template <class T = Subscription>
class SP_PUBLIC SchedulerListener {
public:
	using Callback = Function<void(Subscription::Flags)>;

	SchedulerListener(Scheduler *s = nullptr, const Callback &cb = nullptr, T *sub = nullptr);
	~SchedulerListener();

	SchedulerListener(const SchedulerListener<T> &);
	SchedulerListener &operator=(const SchedulerListener<T> &);

	SchedulerListener(SchedulerListener<T> &&);
	SchedulerListener &operator=(SchedulerListener<T> &&);

	SchedulerListener &operator=(T *);

	inline operator T *() { return get(); }
	inline operator T *() const { return get(); }
	inline operator bool() const { return _binding; }

	inline T *operator->() { return get(); }
	inline const T *operator->() const { return get(); }

	void set(T *sub);
	T *get() const;

	void setScheduler(Scheduler *);
	Scheduler *getScheduler() const;

	void setCallback(const Callback &cb);
	const Callback &getCallback() const;

	void setDirty();
	void update(const UpdateTime &time);

	void check();

protected:
	void updateScheduler();
	void schedule();
	void unschedule();

	Scheduler *_scheduler = nullptr;
	Binding<T> _binding;
	Callback _callback;
	bool _dirty = false;
	bool _scheduled = false;
};

template <typename T, typename Enable = void>
class SP_PUBLIC SchedulerUpdate {
public:
	static void scheduleUpdate(Scheduler *scheduler, T *target, int32_t p, bool paused) {
		scheduler->schedulePerFrame([target](const UpdateTime &time) { target->update(time); },
				target, p, paused);
	}
};

template <class T>
class SP_PUBLIC SchedulerUpdate<T, typename std::enable_if<std::is_base_of<Ref, T>::value>::type> {
public:
	static void scheduleUpdate(Scheduler *scheduler, T *t, int32_t p, bool paused) {
		auto ref = static_cast<Ref *>(t);
		auto target = Rc<Ref>(ref);

		scheduler->schedulePerFrame([target = move(target)](const UpdateTime &time) {
			((T *)target.get())->update(time);
		}, t, p, paused);
	}
};

template <typename T>
void Scheduler::scheduleUpdate(T *target, int32_t p, bool paused) {
	SchedulerUpdate<T>::scheduleUpdate(this, target, p, paused);
}


template <class T>
SchedulerListener<T>::SchedulerListener(Scheduler *s, const Callback &cb, T *sub)
: _scheduler(s), _binding(sub), _callback(cb) {
	static_assert(std::is_convertible<T *, Subscription *>::value,
			"Invalid Type for DataListener<T>!");
	updateScheduler();
}

template <class T>
SchedulerListener<T>::~SchedulerListener() {
	unschedule();
}

template <class T>
SchedulerListener<T>::SchedulerListener(const SchedulerListener<T> &other)
: _binding(other._binding), _callback(other._callback) {
	updateScheduler();
}

template <class T>
SchedulerListener<T> &SchedulerListener<T>::operator=(const SchedulerListener<T> &other) {
	_binding = other._binding;
	_callback = other._callback;
	updateScheduler();
	return *this;
}

template <class T>
SchedulerListener<T>::SchedulerListener(SchedulerListener<T> &&other)
: _binding(sp::move(other._binding)), _callback(sp::move(other._callback)) {
	other.updateScheduler();
	updateScheduler();
}

template <class T>
SchedulerListener<T> &SchedulerListener<T>::operator=(SchedulerListener<T> &&other) {
	_binding = sp::move(other._binding);
	_callback = sp::move(other._callback);
	other.updateScheduler();
	updateScheduler();
	return *this;
}

template <class T>
void SchedulerListener<T>::set(T *sub) {
	if (_binding != sub) {
		_binding = Binding<T>(sub);
		updateScheduler();
	}
}

template <class T>
SchedulerListener<T> &SchedulerListener<T>::operator=(T *sub) {
	set(sub);
	return *this;
}

template <class T>
T *SchedulerListener<T>::get() const {
	return _binding;
}

template <class T>
void SchedulerListener<T>::setScheduler(Scheduler *s) {
	unschedule();
	_scheduler = s;
	updateScheduler();
}

template <class T>
Scheduler *SchedulerListener<T>::getScheduler() const {
	return _scheduler;
}

template <class T>
void SchedulerListener<T>::setCallback(const Callback &cb) {
	_callback = cb;
}

template <class T>
const Function<void(Subscription::Flags)> &SchedulerListener<T>::getCallback() const {
	return _callback;
}

template <class T>
void SchedulerListener<T>::setDirty() {
	_dirty = true;
}

template <class T>
void SchedulerListener<T>::update(const UpdateTime &time) {
	if (_callback && _binding) {
		auto val = _binding.check();
		if (!val.empty() || _dirty) {
			_dirty = false;
			_callback(val);
		}
	}
}

template <class T>
void SchedulerListener<T>::check() {
	update(UpdateTime());
}

template <class T>
void SchedulerListener<T>::updateScheduler() {
	if (_binding && !_scheduled) {
		schedule();
	} else if (!_binding && _scheduled) {
		unschedule();
	}
}

template <class T>
void SchedulerListener<T>::schedule() {
	if (_scheduler && _binding && !_scheduled) {
		_scheduler->scheduleUpdate(this, 0, false);
		_scheduled = true;
	}
}

template <class T>
void SchedulerListener<T>::unschedule() {
	if (_scheduled) {
		_scheduler->unschedule(this);
		_scheduled = false;
	}
}

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_DIRECTOR_XLSCHEDULER_H_ */
