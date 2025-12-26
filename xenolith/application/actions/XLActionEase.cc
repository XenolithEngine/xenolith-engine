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

#include "XLActionEase.h"
#include "XLInterpolation.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

bool ActionEaseCommon::init(ActionInterval *action) {
	XLASSERT(action != nullptr, "");

	if (ActionInterval::init(action->getDuration())) {
		_inner = action;
		return true;
	}

	return false;
}

void ActionEaseCommon::startWithTarget(Node *target) {
	ActionInterval::startWithTarget(target);
	_inner->startWithTarget(_target);
}

void ActionEaseCommon::stop(void) {
	_inner->stop();
	ActionInterval::stop();
}

bool EaseActionTyped::init(ActionInterval *action, Type t, SpanView<float> params) {
	if (ActionEaseCommon::init(action)) {
		_type = t;
		_params = params.vec<Interface>();
		return true;
	}

	return false;
}

bool EaseActionTyped::init(ActionInterval *action, Type t, float rate) {
	if (ActionEaseCommon::init(action)) {
		_type = t;
		_params.emplace_back(rate);
		return true;
	}

	return false;
}

bool EaseActionTyped::init(ActionInterval *action, const EaseBezierInfo &info) {
	if (ActionEaseCommon::init(action)) {
		_type = Type::Bezierat;
		_params.emplace_back(info.v0.x);
		_params.emplace_back(info.v0.y);
		_params.emplace_back(info.v1.x);
		_params.emplace_back(info.v1.y);
		return true;
	}

	return false;
}

void EaseActionTyped::update(float time) {
	_inner->update(interpolation::interpolateTo(time, _type, _params));
}

void EaseActionTyped::setParams(SpanView<float> params) { _params = params.vec<Interface>(); }

SpanView<float> EaseActionTyped::getParams() const { return _params; }

} // namespace stappler::xenolith
