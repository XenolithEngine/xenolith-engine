/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#include "XL2dActionAcceleratedMove.h"
#include "XLNode.h"

#define XL_ACCELERATED_LOG(...)
//#define XL_ACCELERATED_LOG(...) stappler::log::format("ActionAcceleratedMove", __VA_ARGS__)

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

Rc<ActionInterval> ActionAcceleratedMove::createBounce(float acceleration, Vec2 from, Vec2 to,
		Vec2 velocity, float bounceAcceleration, Function<void(Node *)> &&callback) {
	Vec2 diff = to - from;
	float distance = diff.length();

	if (std::fabs(distance) < std::numeric_limits<float>::epsilon()) {
		return Rc<DelayTime>::create(0.0f);
	}

	Vec2 normal = diff.getNormalized();
	normal.normalize();

	Vec2 velProject = velocity.project(normal);

	float startSpeed;
	if (std::fabs(normal.getAngle(velProject)) < M_PI_2) {
		startSpeed = velProject.length();
	} else {
		startSpeed = -velProject.length();
	}

	return ActionAcceleratedMove::createBounce(acceleration, from, to, startSpeed,
			bounceAcceleration, sp::move(callback));
}

Rc<ActionInterval> ActionAcceleratedMove::createBounce(float acceleration, Vec2 from, Vec2 to,
		float velocity, float bounceAcceleration, Function<void(Node *)> &&callback) {
	Vec2 diff = to - from;
	float distance = diff.length();

	if (std::fabs(distance) < std::numeric_limits<float>::epsilon()) {
		return Rc<DelayTime>::create(0.0f);
	}

	Vec2 normal = diff.getNormalized();
	normal.normalize();

	float startSpeed = velocity;

	if (startSpeed == 0) {
		float duration = std::sqrt(distance / acceleration);

		auto a = ActionAcceleratedMove::createWithDuration(duration, normal, from, startSpeed,
				acceleration);
		auto b = ActionAcceleratedMove::createWithDuration(duration, normal, a->getEndPosition(),
				a->getEndVelocity(), -acceleration);

		a->setCallback(Function<void(Node *)>(callback));
		b->setCallback(sp::move(callback));

		return Rc<Sequence>::create(a, b);
	} else {
		float t = startSpeed / acceleration;
		float result = (startSpeed * t) - (acceleration * t * t * 0.5);

		if (startSpeed > 0 && distance > result) {
			float pseudoDistance = distance + acceleration * t * t * 0.5;
			float pseudoDuration = std::sqrt(pseudoDistance / acceleration);

			auto a = ActionAcceleratedMove::createAccelerationTo(normal, from, startSpeed,
					acceleration * pseudoDuration, acceleration);
			auto b = ActionAcceleratedMove::createWithDuration(pseudoDuration, normal,
					a->getEndPosition(), a->getEndVelocity(), -acceleration);

			a->setCallback(Function<void(Node *)>(callback));
			b->setCallback(sp::move(callback));

			return Rc<Sequence>::create(a, b);
		} else if (startSpeed > 0 && distance <= result) {
			if (bounceAcceleration == 0) {
				acceleration = -acceleration;
				float pseudoDistance = distance - result; // < 0
				float pseudoDuration = std::sqrt(pseudoDistance / acceleration); // > 0

				auto a = ActionAcceleratedMove::createAccelerationTo(normal, from, startSpeed,
						acceleration * pseudoDuration, acceleration);
				auto b = ActionAcceleratedMove::createWithDuration(pseudoDuration, normal,
						a->getEndPosition(), a->getEndVelocity(), -acceleration);

				a->setCallback(Function<void(Node *)>(callback));
				b->setCallback(sp::move(callback));

				return Rc<Sequence>::create(a, b);
			} else {
				auto a0 = ActionAcceleratedMove::createAccelerationTo(from, to, startSpeed,
						-acceleration);
				auto a1 = ActionAcceleratedMove::createDecceleration(normal, a0->getEndPosition(),
						a0->getEndVelocity(), bounceAcceleration);

				Vec2 tmpFrom = a1->getEndPosition();
				diff = to - tmpFrom;
				distance = diff.length();
				normal = diff.getNormalized();
				float duration = std::sqrt(distance / acceleration);

				auto a = ActionAcceleratedMove::createWithDuration(duration, normal, tmpFrom, 0,
						acceleration);
				auto b = ActionAcceleratedMove::createWithDuration(duration, normal,
						a->getEndPosition(), a->getEndVelocity(), -acceleration);

				a0->setCallback(Function<void(Node *)>(callback));
				a1->setCallback(Function<void(Node *)>(callback));
				a->setCallback(Function<void(Node *)>(callback));
				b->setCallback(sp::move(callback));

				return Rc<Sequence>::create(a0, a1, a, b);
			}
		} else {
			float pseudoDistance = 0;

			if (bounceAcceleration) {
				t = startSpeed / bounceAcceleration;
				pseudoDistance =
						distance + std::fabs((startSpeed * t) - bounceAcceleration * t * t * 0.5);
			} else {
				pseudoDistance =
						distance + std::fabs((startSpeed * t) - acceleration * t * t * 0.5);
			}

			float pseudoDuration = std::sqrt(pseudoDistance / acceleration);

			Rc<ActionAcceleratedMove> a1;
			Rc<ActionAcceleratedMove> a2;
			if (bounceAcceleration) {
				a1 = ActionAcceleratedMove::createDecceleration(-normal, from, -startSpeed,
						bounceAcceleration);
				a2 = ActionAcceleratedMove::createAccelerationTo(normal, a1->getEndPosition(), 0,
						acceleration * pseudoDuration, acceleration);
			} else {
				a2 = ActionAcceleratedMove::createAccelerationTo(normal, from, startSpeed,
						acceleration * pseudoDuration, acceleration);
			}

			auto b = ActionAcceleratedMove::createDecceleration(normal, a2->getEndPosition(),
					a2->getEndVelocity(), acceleration);

			if (a1) {
				a2->setCallback(Function<void(Node *)>(callback));
				b->setCallback(Function<void(Node *)>(callback));
				a1->setCallback(sp::move(callback));
				return Rc<Sequence>::create(a1, a2, b);
			} else {
				a2->setCallback(Function<void(Node *)>(callback));
				b->setCallback(sp::move(callback));
				return Rc<Sequence>::create(a2, b);
			}
		}
	}
}

Rc<ActionInterval> ActionAcceleratedMove::createFreeBounce(float acceleration, Vec2 from, Vec2 to,
		Vec2 velocity, float bounceAcceleration, Function<void(Node *)> &&callback) {

	Vec2 diff = to - from;
	float distance = diff.length();
	Vec2 normal = diff.getNormalized();
	Vec2 velProject = velocity.project(normal);

	float startSpeed;
	if (std::fabs(normal.getAngle(velProject)) < M_PI_2) {
		startSpeed = velProject.length();
	} else {
		startSpeed = -velProject.length();
	}

	if (startSpeed < 0) {
		return createBounce(acceleration, from, to, velocity, bounceAcceleration,
				sp::move(callback));
	} else {
		float duration = startSpeed / acceleration;
		float deccelerationPath = startSpeed * duration - acceleration * duration * duration * 0.5;
		if (deccelerationPath < distance) {
			auto a = createWithDuration(duration, normal, from, startSpeed, -acceleration);
			a->setCallback(sp::move(callback));
			return a;
		} else {
			return createBounce(acceleration, from, to, velocity, bounceAcceleration,
					sp::move(callback));
		}
	}
}

Rc<ActionInterval> ActionAcceleratedMove::createWithBounds(float acceleration, Vec2 from,
		Vec2 velocity, const Rect &bounds, Function<void(Node *)> &&callback) {
	Vec2 normal = velocity.getNormalized();

	float angle = normal.getAngle();

	float start, end, pos, v, t, dist;
	Vec2 x, y, z, a, b, i;

	if (bounds.size.width == 0 && bounds.size.height == 0) {
		return nullptr;
	}

	if (bounds.size.width == 0) {
		start = bounds.origin.y;
		end = bounds.origin.y + bounds.size.height;
		pos = from.y;

		v = velocity.y;
		t = std::fabs(v) / std::fabs(acceleration);
		dist = std::fabs(v) * t - std::fabs(acceleration) * t * t * 0.5;

		if (velocity.y > 0) {
			if (std::fabs(pos - end) < std::numeric_limits<float>::epsilon()) {
				return Rc<DelayTime>::create(0.0f);
			} else if (pos + dist < end) {
				return createDecceleration(Vec2(0.0f, 1.0f), from, velocity.y, -acceleration,
						sp::move(callback));
			} else {
				return createAccelerationTo(from, Vec2(from.x, end), v, -acceleration,
						sp::move(callback));
			}
		} else {
			if (std::fabs(pos - start) < std::numeric_limits<float>::epsilon()) {
				return Rc<DelayTime>::create(0.0f);
			} else if (pos - dist > start) {
				return createDecceleration(Vec2(0.0f, -1.0f), from, velocity.y, -acceleration,
						sp::move(callback));
			} else {
				return createAccelerationTo(from, Vec2(from.x, start), std::fabs(v), -acceleration,
						sp::move(callback));
			}
		}
	}

	if (bounds.size.height == 0) {
		start = bounds.origin.x;
		end = bounds.origin.x + bounds.size.width;
		pos = from.x;

		v = std::fabs(velocity.x);
		t = v / std::fabs(acceleration);
		dist = v * t - std::fabs(acceleration) * t * t * 0.5;

		if (velocity.x > 0) {
			if (std::fabs(pos - end) < std::numeric_limits<float>::epsilon()) {
				return Rc<DelayTime>::create(0.0f);
			} else if (pos + dist < end) {
				return createDecceleration(Vec2(1, 0), from, v, -acceleration, sp::move(callback));
			} else {
				return createAccelerationTo(from, Vec2(end, from.y), v, -acceleration,
						sp::move(callback));
			}
		} else {
			if (std::fabs(pos - start) < std::numeric_limits<float>::epsilon()) {
				return nullptr;
			} else if (pos - dist > start) {
				return createDecceleration(Vec2(-1, 0), from, v, -acceleration, sp::move(callback));
			} else {
				return createAccelerationTo(from, Vec2(start, from.y), v, -acceleration,
						sp::move(callback));
			}
		}
	}

	if (angle < -M_PI_2) {
		x = Vec2(bounds.getMinX(), bounds.getMaxY());
		y = Vec2(bounds.getMinX(), bounds.getMinY());
		z = Vec2(bounds.getMaxX(), bounds.getMinY());

		a = (normal.x == 0) ? from : Vec2::getIntersectPoint(from, from + normal, x, y);
		b = (normal.y == 0) ? from : Vec2::getIntersectPoint(from, from + normal, y, z);
	} else if (angle < 0) {
		x = Vec2(bounds.getMinX(), bounds.getMinY());
		y = Vec2(bounds.getMaxX(), bounds.getMinY());
		z = Vec2(bounds.getMaxX(), bounds.getMaxY());

		a = (normal.x == 0) ? from : Vec2::getIntersectPoint(from, from + normal, x, y);
		b = (normal.y == 0) ? from : Vec2::getIntersectPoint(from, from + normal, y, z);
	} else if (angle < M_PI_2) {
		x = Vec2(bounds.getMaxX(), bounds.getMinY());
		y = Vec2(bounds.getMaxX(), bounds.getMaxY());
		z = Vec2(bounds.getMinX(), bounds.getMaxY());

		a = (normal.y == 0) ? from : Vec2::getIntersectPoint(from, from + normal, x, y);
		b = (normal.x == 0) ? from : Vec2::getIntersectPoint(from, from + normal, y, z);
	} else {
		x = Vec2(bounds.getMaxX(), bounds.getMaxY());
		y = Vec2(bounds.getMinX(), bounds.getMaxY());
		z = Vec2(bounds.getMinX(), bounds.getMinY());

		a = (normal.y == 0) ? from : Vec2::getIntersectPoint(from, from + normal, x, y);
		b = (normal.x == 0) ? from : Vec2::getIntersectPoint(from, from + normal, y, z);
	}

	float a_len = from.distance(a);
	float b_len = from.distance(b);
	float i_len, s_len;

	if (a_len < b_len) {
		i = a;
		i_len = a_len;
		s_len = b_len;
	} else {
		i = b;
		i_len = b_len;
		s_len = a_len;
	}

	v = velocity.length();
	t = v / acceleration;
	float path = v * t - acceleration * t * t * 0.5;

	if (path < i_len) {
		return createDecceleration(normal, from, v, acceleration, sp::move(callback));
	} else {
		auto a1 = createAccelerationTo(from, i, v, -acceleration);

		if (s_len > 0) {
			a1->setCallback(Function<void(Node *)>(callback));
			Vec2 diff = y - i;
			if (diff.length() < std::numeric_limits<float>::epsilon()) {
				return a1;
			}
			Vec2 newNormal = diff.getNormalized();

			i_len = diff.length();

			acceleration = (normal * acceleration).project(newNormal).length();
			if (acceleration == 0.0f) {
				return a1;
			}
			v = (normal * a1->getEndVelocity()).project(newNormal).length();
			t = v / acceleration;
			path = v * t - acceleration * t * t * 0.5;

			if (path < i_len) {
				auto a2 = createDecceleration(newNormal, i, v, acceleration);
				a2->setCallback(sp::move(callback));
				return Rc<Sequence>::create(a1, a2);
			} else {
				auto a2 = createAccelerationTo(i, y, v, -acceleration);
				a2->setCallback(sp::move(callback));
				return Rc<Sequence>::create(a1, a2);
			}
		} else {
			if (a1) {
				a1->setCallback(sp::move(callback));
			}
			return a1;
		}
	}
}

Vec2 ActionAcceleratedMove::computeEndPoint() {
	return _startPoint
			+ (_normalPoint
					* ((_startVelocity * _accDuration)
							+ (_acceleration * _accDuration * _accDuration * 0.5)));
}

Vec2 ActionAcceleratedMove::computeNormalPoint() {
	return (_endPoint - _startPoint).getNormalized();
}

float ActionAcceleratedMove::computeEndVelocity() {
	return _startVelocity + _acceleration * _accDuration;
}

Rc<ActionAcceleratedMove> ActionAcceleratedMove::createDecceleration(Vec2 normal, Vec2 startPoint,
		float startVelocity, float acceleration, Function<void(Node *)> &&callback) {
	auto pRet = Rc<ActionAcceleratedMove>::alloc();
	if (pRet->initDecceleration(normal, startPoint, startVelocity, acceleration,
				sp::move(callback))) {
		return pRet;
	}
	return nullptr;
}

Rc<ActionAcceleratedMove> ActionAcceleratedMove::createDecceleration(Vec2 startPoint, Vec2 endPoint,
		float acceleration, Function<void(Node *)> &&callback) {
	auto pRet = Rc<ActionAcceleratedMove>::alloc();
	if (pRet->initDecceleration(startPoint, endPoint, acceleration, sp::move(callback))) {
		return pRet;
	}
	return nullptr;
}

Rc<ActionAcceleratedMove> ActionAcceleratedMove::createAccelerationTo(Vec2 normal, Vec2 startPoint,
		float startVelocity, float endVelocity, float acceleration,
		Function<void(Node *)> &&callback) {
	auto pRet = Rc<ActionAcceleratedMove>::alloc();
	if (pRet->initAccelerationTo(normal, startPoint, startVelocity, endVelocity, acceleration,
				sp::move(callback))) {
		return pRet;
	}
	return nullptr;
}

Rc<ActionAcceleratedMove> ActionAcceleratedMove::createAccelerationTo(Vec2 startPoint,
		Vec2 endPoint, float startVelocity, float acceleration, Function<void(Node *)> &&callback) {
	auto pRet = Rc<ActionAcceleratedMove>::alloc();
	if (pRet->initAccelerationTo(startPoint, endPoint, startVelocity, acceleration,
				sp::move(callback))) {
		return pRet;
	}
	return nullptr;
}

Rc<ActionAcceleratedMove> ActionAcceleratedMove::createWithDuration(float duration, Vec2 normal,
		Vec2 startPoint, float startVelocity, float acceleration,
		Function<void(Node *)> &&callback) {
	auto pRet = Rc<ActionAcceleratedMove>::alloc();
	if (pRet->initWithDuration(duration, normal, startPoint, startVelocity, acceleration,
				sp::move(callback))) {
		return pRet;
	}
	return nullptr;
}

bool ActionAcceleratedMove::initDecceleration(Vec2 normal, Vec2 startPoint, float startVelocity,
		float acceleration, Function<void(Node *)> &&callback) {
	acceleration = std::fabs(acceleration);
	startVelocity = std::fabs(startVelocity);
	if (startVelocity < 0 || acceleration < 0) {
		XL_ACCELERATED_LOG("Deceleration failed: velocity:%f acceleration:%f", startVelocity,
				acceleration);
		return false;
	}

	_accDuration = startVelocity / acceleration;

	if (!ActionInterval::init(_accDuration)) {
		return false;
	}

	_acceleration = -acceleration;
	_startVelocity = startVelocity;
	_endVelocity = 0;

	_normalPoint = normal;
	_startPoint = startPoint;

	_endPoint = computeEndPoint();

	XL_ACCELERATED_LOG(
			"%s %d Acceleration:%f velocity:%f->%f duration:%f position:(%f %f)->(%f %f)",
			__FUNCTION__, __LINE__, _acceleration, _startVelocity, _endVelocity, _accDuration,
			_startPoint.x, _startPoint.y, _endPoint.x, _endPoint.y);

	if (isnan(_endPoint.x) || isnan(_endPoint.y) || isnan(_accDuration)) {
		XL_ACCELERATED_LOG("Failed!");
		return false;
	}

	_callback = sp::move(callback);
	return true;
}

bool ActionAcceleratedMove::initDecceleration(Vec2 startPoint, Vec2 endPoint, float acceleration,
		Function<void(Node *)> &&callback) {
	float distance = startPoint.distance(endPoint);
	acceleration = std::fabs(acceleration);

	if (std::fabs(distance) < std::numeric_limits<float>::epsilon()) {
		_accDuration = 0.0f;
	} else {
		_accDuration = std::sqrt((distance * 2.0f) / acceleration);
	}

	if (!ActionInterval::init(_accDuration)) {
		return false;
	}

	_acceleration = -acceleration;
	_startVelocity = _accDuration * acceleration;
	_endVelocity = 0;

	_startPoint = startPoint;
	_endPoint = endPoint;
	_normalPoint = computeNormalPoint();

	XL_ACCELERATED_LOG(
			"%s %d Acceleration:%f velocity:%f->%f duration:%f position:(%f %f)->(%f %f)",
			__FUNCTION__, __LINE__, _acceleration, _startVelocity, _endVelocity, _accDuration,
			_startPoint.x, _startPoint.y, _endPoint.x, _endPoint.y);

	if (isnan(_endPoint.x) || isnan(_endPoint.y) || isnan(_accDuration)) {
		XL_ACCELERATED_LOG("Failed!");
		return false;
	}

	_callback = sp::move(callback);
	return true;
}

bool ActionAcceleratedMove::initAccelerationTo(Vec2 normal, Vec2 startPoint, float startVelocity,
		float endVelocity, float acceleration, Function<void(Node *)> &&callback) {

	_accDuration = (endVelocity - startVelocity) / acceleration;

	if (_accDuration < 0) {
		XL_ACCELERATED_LOG("AccelerationTo failed: velocity:(%f:%f) acceleration:%f", startVelocity,
				endVelocity, acceleration);
		return false;
	}

	if (!ActionInterval::init(_accDuration)) {
		return false;
	}

	_acceleration = acceleration;
	_startVelocity = startVelocity;
	_endVelocity = endVelocity;

	_normalPoint = normal;
	_startPoint = startPoint;

	_endPoint = computeEndPoint();

	XL_ACCELERATED_LOG(
			"%s %d Acceleration:%f velocity:%f->%f duration:%f position:(%f %f)->(%f %f)",
			__FUNCTION__, __LINE__, _acceleration, _startVelocity, _endVelocity, _accDuration,
			_startPoint.x, _startPoint.y, _endPoint.x, _endPoint.y);

	if (isnan(_endPoint.x) || isnan(_endPoint.y) || isnan(_accDuration)) {
		XL_ACCELERATED_LOG("Failed!");
		return false;
	}

	_callback = sp::move(callback);
	return true;
}

bool ActionAcceleratedMove::initAccelerationTo(Vec2 startPoint, Vec2 endPoint, float startVelocity,
		float acceleration, Function<void(Node *)> &&callback) {

	float distance = -endPoint.distance(startPoint);
	float d = startVelocity * startVelocity - 2 * acceleration * distance;

	if (std::fabs(distance) < std::numeric_limits<float>::epsilon()) {
		XL_ACCELERATED_LOG("zero distance");
	}

	if (d < 0) {
		XL_ACCELERATED_LOG("AccelerationTo failed: acceleration:%f velocity:%f D:%f", acceleration,
				startVelocity, d);
		return false;
	}

	float t1 = (-startVelocity + std::sqrt(d)) / acceleration;
	float t2 = (-startVelocity - std::sqrt(d)) / acceleration;

	if (distance != 0.0f) {
		_accDuration = t1 < 0 ? t2 : (t2 < 0 ? t1 : std::min(t1, t2));
		if (isnan(_accDuration)) {
			_accDuration = 0.0f;
		}
	} else {
		_accDuration = 0.0f;
	}

	if (_accDuration < 0) {
		if (d < 0) {
			XL_ACCELERATED_LOG("AccelerationTo failed: acceleration:%f velocity:%f duration:%f",
					acceleration, startVelocity, _accDuration);
			return false;
		}
	}

	if (!ActionInterval::init(_accDuration)) {
		return false;
	}

	_startPoint = startPoint;
	_endPoint = endPoint;
	_normalPoint = computeNormalPoint();

	_acceleration = acceleration;
	_startVelocity = startVelocity;
	_endVelocity = computeEndVelocity();

	XL_ACCELERATED_LOG(
			"%s %d Acceleration:%f velocity:%f->%f duration:%f position:(%f %f)->(%f %f)",
			__FUNCTION__, __LINE__, _acceleration, _startVelocity, _endVelocity, _accDuration,
			_startPoint.x, _startPoint.y, _endPoint.x, _endPoint.y);

	if (isnan(_endPoint.x) || isnan(_endPoint.y) || isnan(_accDuration)) {
		XL_ACCELERATED_LOG("Failed!");
		return false;
	}

	_callback = sp::move(callback);
	return true;
}

bool ActionAcceleratedMove::initWithDuration(float duration, Vec2 normal, Vec2 startPoint,
		float startVelocity, float acceleration, Function<void(Node *)> &&callback) {

	_accDuration = duration;

	if (!ActionInterval::init(_accDuration)) {
		return false;
	}

	_normalPoint = normal;
	_startPoint = startPoint;

	_acceleration = acceleration;
	_startVelocity = startVelocity;

	_endVelocity = computeEndVelocity();
	_endPoint = computeEndPoint();

	XL_ACCELERATED_LOG(
			"%s %d Acceleration:%f velocity:%f->%f duration:%f position:(%f %f)->(%f %f)",
			__FUNCTION__, __LINE__, _acceleration, _startVelocity, _endVelocity, _accDuration,
			_startPoint.x, _startPoint.y, _endPoint.x, _endPoint.y);

	if (isnan(_endPoint.x) || isnan(_endPoint.y) || isnan(_accDuration)) {
		XL_ACCELERATED_LOG("Failed!");
		return false;
	}

	_callback = sp::move(callback);
	return true;
}

float ActionAcceleratedMove::getDuration() const { return _accDuration; }

Vec2 ActionAcceleratedMove::getPosition(float timePercent) const {
	float t = timePercent * _accDuration;
	return _startPoint + _normalPoint * ((_startVelocity * t) + (_acceleration * t * t * 0.5));
}

float ActionAcceleratedMove::getCurrentVelocity() const {
	return _startVelocity + _acceleration * _elapsed;
}

void ActionAcceleratedMove::startWithTarget(Node *target) {
	ActionInterval::startWithTarget(target);
	XL_ACCELERATED_LOG("Acceleration:%f velocity:%f->%f duration:%f position:(%f %f)->(%f %f)",
			_acceleration, _startVelocity, _endVelocity, _accDuration, _startPoint.x, _startPoint.y,
			_endPoint.x, _endPoint.y);
}

void ActionAcceleratedMove::update(float t) {
	if (_target) {
		Vec2 pos = getPosition(t);
		_target->setPosition(pos);
		if (_callback) {
			_callback(_target);
		}
	}
}

void ActionAcceleratedMove::setCallback(Function<void(Node *)> &&callback) {
	_callback = sp::move(callback);
}

} // namespace stappler::xenolith::basic2d
