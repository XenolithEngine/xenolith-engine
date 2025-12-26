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

#include "XLAction.h"
#include "XLNode.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

void Action::invalidate() {
	if (_target) {
		stop();
	}
}

void Action::stop() { _target = nullptr; }

bool Action::isDone() const { return _target == nullptr; }

SP_COVERAGE_TRIVIAL
void Action::step(float dt) { log::source().warn("Action", "[step]: override me"); }

SP_COVERAGE_TRIVIAL
void Action::update(float time) { log::source().warn("Action", "[update]: override me"); }

void Action::startWithTarget(Node *aTarget) { _target = aTarget; }

void Action::setContainer(Node *container) { _container = container; }

void Action::setTarget(Node *target) { _target = target; }

bool ActionInstant::init(bool runOnce) {
	_duration = 0.0f;
	_runOnce = runOnce;
	return true;
}

void ActionInstant::step(float dt) {
	if (!_performed || !_runOnce) {
		update(1.0f);
		_performed = true;
	}
}

void Show::update(float time) { _target->setVisible(true); }

void Hide::update(float time) { _target->setVisible(false); }

void ToggleVisibility::update(float time) { _target->setVisible(!_target->isVisible()); }

bool RemoveSelf::init(bool isNeedCleanUp, bool runOnce) {
	if (!ActionInstant::init(runOnce)) {
		return false;
	}

	_isNeedCleanUp = isNeedCleanUp;
	return true;
}

void RemoveSelf::update(float time) { _target->removeFromParent(_isNeedCleanUp); }

bool Place::init(const Vec2 &pos, bool runOnce) {
	if (!ActionInstant::init(runOnce)) {
		return false;
	}

	_position = pos;
	return true;
}

void Place::update(float time) { _target->setPosition(_position); }

bool CallFunc::init(Function<void()> &&func, bool runOnce) {
	if (!ActionInstant::init(runOnce)) {
		return false;
	}

	_callback = sp::move(func);
	return true;
}

void CallFunc::update(float time) { _callback(); }

bool ActionInterval::init(float duration) {
	_duration = duration;

	// prevent division by 0
	// This comparison could be in step:, but it might decrease the performance
	// by 3% in heavy based action games.
	_duration = std::max(_duration, FLT_EPSILON);
	if (_duration == 0) {
		_duration = FLT_EPSILON;
	}

	_elapsed = 0;
	_firstTick = true;
	return true;
}

void ActionInterval::stop() {
	_elapsed = _duration;
	Action::stop();
}

bool ActionInterval::isDone() const { return _elapsed >= _duration; }

void ActionInterval::step(float dt) {
	if (_firstTick) {
		_firstTick = false;
		_elapsed = 0;
	} else {
		_elapsed += dt;
	}

	this->update(math::clamp(_elapsed / _duration, 0.0f, 1.0f));
}

void ActionInterval::startWithTarget(Node *target) {
	Action::startWithTarget(target);
	_elapsed = 0.0f;
	_firstTick = true;
}

void ActionInterval::setDuration(float duration) { _duration = std::max(_duration, FLT_EPSILON); }

bool Speed::init(Rc<ActionInterval> &&action, float speed) {
	XLASSERT(action != nullptr, "action must not be NULL");
	setInnerAction(move(action));
	_speed = speed;
	return true;
}

void Speed::setInnerAction(Rc<ActionInterval> &&action) {
	if (_innerAction != action) {
		_innerAction = action;
	}
}

void Speed::startWithTarget(Node *target) {
	Action::startWithTarget(target);
	_innerAction->startWithTarget(target);
	setDuration(_innerAction->getDuration() * _speed);
}

void Speed::stop() {
	_innerAction->stop();
	Action::stop();
}

void Speed::step(float dt) { _innerAction->step(dt * _speed); }

bool Speed::isDone() const { return _innerAction->isDone(); }

void Sequence::stop(void) {
	if (_prevTime < 1.0f && _currentIdx < _actions.size()) {
		bool finalizeInstants = false;
		auto front = _actions.begin() + _currentIdx;
		auto end = _actions.end();

		front->action->stop();
		finalizeInstants =
				(_prevTime - std::numeric_limits<float>::epsilon()) >= front->maxThreshold;
		++front;
		++_currentIdx;

		if (finalizeInstants) {
			while (front != end && front->threshold <= std::numeric_limits<float>::epsilon()) {
				front->action->startWithTarget(_target);
				front->action->update(1.0);
				front->action->stop();

				++front;
				++_currentIdx;
			}
		}

		// do not update any non-instant actions, just start-stop
		while (front != end) {
			front->action->startWithTarget(_target);
			front->action->stop();

			++front;
			++_currentIdx;
		}

		_prevTime = 1.0f;
	}
	ActionInterval::stop();
}

void Sequence::update(float t) {
	auto front = _actions.begin() + _currentIdx;
	auto end = _actions.end();

	auto dt = t - _prevTime;

	// assume monotonical progress

	while (front != end && dt != 0) {
		// process instants
		if (front->threshold <= std::numeric_limits<float>::epsilon()) {
			do {
				front->action->startWithTarget(_target);
				front->action->update(1.0);
				front->action->stop();

				++front;
				++_currentIdx;
			} while (front != end && front->threshold == 0.0f);

			// start next non-instant
			if (front == end) {
				_prevTime = t;
				return;
			} else {
				front->action->startWithTarget(_target);
				front->action->update(0.0);
			}
		}

		auto timeFromActionStart = t - front->minThreshold;
		auto actionRelativeTime = timeFromActionStart / front->threshold;

		if (actionRelativeTime >= 1.0f - std::numeric_limits<float>::epsilon() || t == 1.0f) {
			front->action->update(1.0f);
			dt = t - front->maxThreshold;
			front->action->stop();

			++front;
			++_currentIdx;

			// start next non-instant
			if (front == end) {
				_prevTime = t;
				return;
			} else if (front->threshold > std::numeric_limits<float>::epsilon()) {
				front->action->startWithTarget(_target);
				front->action->update(0.0);
			}
		} else {
			front->action->update(actionRelativeTime);
			dt = 0.0f;
			break;
		}
	}

	auto tmp = front;
	while (front != end && front->threshold <= std::numeric_limits<float>::epsilon()) {
		front->action->startWithTarget(_target);
		front->action->update(1.0);
		front->action->stop();

		++front;
		++_currentIdx;
	}

	if (front != end && tmp != front) {
		front->action->startWithTarget(_target);
		front->action->update(0.0);
	}

	_prevTime = t;
}

void Sequence::startWithTarget(Node *target) {
	ActionInterval::startWithTarget(target);
	float threshold = 0.0f;
	for (auto &it : _actions) {
		it.minThreshold = threshold;
		it.threshold = it.action->getDuration() / _duration;
		threshold += it.threshold;
		it.maxThreshold = threshold;
	}

	// start first action if it's not instant
	if (_actions.front().threshold != 0.0f) {
		_actions.front().action->startWithTarget(_target);
	}

	_prevTime = 0.0f;
	_currentIdx = 0;
}

bool Sequence::reserve(size_t s) {
	_actions.reserve(s);
	return true;
}

bool Sequence::addAction(Function<void()> &&cb) {
	auto a = Rc<CallFunc>::create(sp::move(cb));
	return addAction(a.get());
}

bool Sequence::addAction(float time) {
	auto a = Rc<DelayTime>::create(time);
	return addAction(a.get());
}

bool Sequence::addAction(TimeInterval ival) {
	auto a = Rc<DelayTime>::create(ival.toFloatSeconds());
	return addAction(a.get());
}

bool Sequence::addAction(Action *a) {
	_duration += a->getDuration();
	_actions.emplace_back(ActionData{a});
	return true;
}

void Spawn::stop(void) {
	if (_prevTime < 1.0f) {
		for (auto &it : _actions) {
			if (it.threshold >= _prevTime) {
				it.action->stop();
			}
		}
		_prevTime = 1.0f;
	}
	ActionInterval::stop();
}

void Spawn::update(float t) {
	for (auto &it : _actions) {
		if (t >= it.threshold && _prevTime < it.threshold) {
			it.action->update(1.0f);
			it.action->stop();
		} else if (t < it.threshold) {
			it.action->update(t * it.threshold);
		}
	}

	_prevTime = t;
}

void Spawn::startWithTarget(Node *target) {
	ActionInterval::startWithTarget(target);
	for (auto &it : _actions) {
		it.threshold = it.action->getDuration() / _duration - std::numeric_limits<float>::epsilon();
		it.action->startWithTarget(target);
	}

	_prevTime = -std::numeric_limits<float>::epsilon() * 2;
}

bool Spawn::reserve(size_t s) {
	_actions.reserve(s);
	return true;
}

bool Spawn::addAction(Function<void()> &&cb) {
	auto a = Rc<CallFunc>::create(sp::move(cb));
	return addAction(a.get());
}

bool Spawn::addAction(float time) {
	auto a = Rc<DelayTime>::create(time);
	return addAction(a.get());
}

bool Spawn::addAction(Action *a) {
	_duration = std::max(_duration, a->getDuration());
	_actions.emplace_back(ActionData{a});
	return true;
}

bool Repeat::init(Rc<ActionInterval> &&action, uint32_t times) {
	float d = action->getDuration() * times;

	if (ActionInterval::init(d)) {
		_times = times;
		setInnerAction(move(action));
		_actionInstant = dynamic_cast<ActionInstant *>(action.get()) ? true : false;
		if (_actionInstant) {
			_times -= 1;
		}
		_total = 0;
		return true;
	}

	return false;
}

void Repeat::setInnerAction(Rc<ActionInterval> &&action) {
	if (_innerAction != action) {
		_innerAction = move(action);
	}
}

void Repeat::stop() {
	_innerAction->stop();
	ActionInterval::stop();
}

void Repeat::update(float dt) {
	if (dt >= _nextDt) {
		while (dt > _nextDt && _total < _times) {
			_innerAction->update(1.0f);
			++_total;

			_innerAction->stop();
			_innerAction->startWithTarget(_target);
			_nextDt = _innerAction->getDuration() / _duration * (_total + 1);
		}

		if (dt >= 1.0f && _total < _times) {
			++_total;
		}

		// don't set an instant action back or update it, it has no use because it has no duration
		if (!_actionInstant) {
			if (_total == _times) {
				_innerAction->update(1);
				_innerAction->stop();
			} else {
				_innerAction->update(dt - (_nextDt - _innerAction->getDuration() / _duration));
			}
		}
	} else {
		_innerAction->update(fmodf(dt * _times, 1.0f));
	}
}

void Repeat::startWithTarget(Node *target) {
	_total = 0;
	_nextDt = _innerAction->getDuration() / _duration;
	ActionInterval::startWithTarget(target);
	_innerAction->startWithTarget(target);
}

bool Repeat::isDone() const { return _total == _times; }

bool RepeatForever::init(ActionInterval *action) {
	_innerAction = action;
	return true;
}

void RepeatForever::startWithTarget(Node *target) {
	ActionInterval::startWithTarget(target);
	_innerAction->startWithTarget(target);
}

void RepeatForever::step(float dt) {
	_innerAction->step(dt);
	if (_innerAction->isDone()) {
		float diff = _innerAction->getElapsed() - _innerAction->getDuration();
		if (diff > _innerAction->getDuration()) {
			diff = fmodf(diff, _innerAction->getDuration());
		}
		_innerAction->startWithTarget(_target);
		_innerAction->step(0.0f);
		_innerAction->step(diff);
	}
}

bool RepeatForever::isDone() const { return false; }

void DelayTime::update(float time) { }

bool TintTo::init(float duration, const Color4F &to, ColorMask mask) {
	if (!ActionInterval::init(duration)) {
		return false;
	}

	_to = to;
	_mask = mask;

	return true;
}

void TintTo::startWithTarget(Node *target) {
	ActionInterval::startWithTarget(target);
	if (target) {
		_from = target->getColor();
		_to.setUnmasked(_from, _mask);
	}
}

void TintTo::update(float time) { _target->setColor(progress(_from, _to, time), true); }

bool ActionProgress::init(float duration, UpdateCallback &&update, StartCallback &&start,
		StopCallback &&stop) {
	return init(duration, 0.0f, 1.0f, sp::move(update), sp::move(start), sp::move(stop));
}

bool ActionProgress::init(float duration, float targetProgress, UpdateCallback &&update,
		StartCallback &&start, StopCallback &&stop) {
	return init(duration, 0.0f, targetProgress, sp::move(update), sp::move(start), sp::move(stop));
}

bool ActionProgress::init(float duration, float sourceProgress, float targetProgress,
		UpdateCallback &&update, StartCallback &&start, StopCallback &&stop) {
	if (!ActionInterval::init(duration)) {
		return false;
	}

	_sourceProgress = sourceProgress;
	_targetProgress = targetProgress;
	_onUpdate = sp::move(update);
	_onStart = sp::move(start);
	_onStop = sp::move(stop);

	return true;
}

void ActionProgress::startWithTarget(Node *t) {
	ActionInterval::startWithTarget(t);
	_stopped = false;
	if (_onStart) {
		_onStart();
	}
}

void ActionProgress::update(float time) {
	if (_onUpdate) {
		_onUpdate(_sourceProgress + (_targetProgress - _sourceProgress) * time);
	}
}

void ActionProgress::stop() {
	if (!_stopped && _onStop) {
		_onStop();
	}
	_stopped = true;
	ActionInterval::stop();
}

bool MoveTo::init(float duration, const Vec2 &position) {
	if (!ActionInterval::init(duration)) {
		return false;
	}

	_endPosition = Vec3(position.x, position.y, nan());
	return true;
}

bool MoveTo::init(float duration, const Vec3 &position) {
	if (!ActionInterval::init(duration)) {
		return false;
	}

	_endPosition = position;
	return true;
}

void MoveTo::startWithTarget(Node *target) {
	ActionInterval::startWithTarget(target);
	_startPosition = target->getPosition();
	if (isnan(_endPosition.z)) {
		_endPosition.z = _startPosition.z;
	}
}

void MoveTo::update(float time) {
	_target->setPosition(progress(_startPosition, _endPosition, time));
}

bool ScaleTo::init(float duration, float scale) {
	if (!ActionInterval::init(duration)) {
		return false;
	}

	_endScale = Vec3(scale, scale, scale);
	return true;
}

bool ScaleTo::init(float duration, const Vec3 &scale) {
	if (!ActionInterval::init(duration)) {
		return false;
	}

	_endScale = scale;
	return true;
}

void ScaleTo::startWithTarget(Node *target) {
	ActionInterval::startWithTarget(target);
	_startScale = target->getScale();
}

void ScaleTo::update(float time) { _target->setScale(progress(_startScale, _endScale, time)); }

bool ResizeTo::init(float duration, const Size2 &size) {
	if (!ActionInterval::init(duration)) {
		return false;
	}

	_endSize = size;
	return true;
}

void ResizeTo::startWithTarget(Node *target) {
	ActionInterval::startWithTarget(target);
	_startSize = target->getContentSize();
}

void ResizeTo::update(float time) { _target->setContentSize(progress(_startSize, _endSize, time)); }

bool FadeTo::init(float duration, float target) {
	if (!ActionInterval::init(duration)) {
		return false;
	}

	_endOpacity = target;
	return true;
}

void FadeTo::startWithTarget(Node *target) {
	ActionInterval::startWithTarget(target);
	_startOpacity = target->getOpacity();
}

void FadeTo::update(float time) { _target->setOpacity(progress(_startOpacity, _endOpacity, time)); }

bool RenderContinuously::init() {
	_innerAction = Rc<RepeatForever>::create(Rc<DelayTime>::create(1.0f));

	if (_innerAction) {
		return ActionInterval::init(_innerAction->getDuration());
	}
	return false;
}

bool RenderContinuously::init(float duration) {
	_innerAction = Rc<DelayTime>::create(1.0f);

	if (_innerAction) {
		return ActionInterval::init(_innerAction->getDuration());
	}
	return false;
}

void RenderContinuously::step(float dt) {
	ActionInterval::step(dt);
	_innerAction->step(dt);
}

void RenderContinuously::startWithTarget(Node *target) {
	ActionInterval::startWithTarget(target);
	_innerAction->startWithTarget(target);
}

void RenderContinuously::update(float time) {
	// do nothing
}

bool RenderContinuously::isDone(void) const { return _innerAction->isDone(); }

void RenderContinuously::stop() {
	_innerAction->stop();
	ActionInterval::stop();
}


} // namespace stappler::xenolith
