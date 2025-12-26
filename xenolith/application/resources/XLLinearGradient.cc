/**
 Copyright (c) 2024-2025 Stappler Team <admin@stappler.org>

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

#include "XLLinearGradient.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

LinearGradientData::LinearGradientData(const LinearGradientData &other)
: start(other.start), end(other.end), steps(other.steps) { }

LinearGradientData::LinearGradientData(LinearGradientData &&other)
: start(other.start), end(other.end), steps(sp::move(other.steps)) { }

bool LinearGradient::init(const Vec2 &start, const Vec2 &end, Vector<GradientStep> &&steps) {
	if (!updateWithData(start, end, sp::move(steps))) {
		return false;
	}

	return true;
}

bool LinearGradient::init(const Vec2 &origin, float angle, float distance,
		Vector<GradientStep> &&steps) {
	return init(origin, origin + Vec2::forAngle(angle) * distance, sp::move(steps));
}

bool LinearGradient::updateWithData(const Vec2 &start, const Vec2 &end,
		Vector<GradientStep> &&steps) {
	if (steps.empty()) {
		return false;
	}

	if (!_data) {
		_data = Rc<LinearGradientData>::alloc();
		_copyOnWrite = false;
	} else if (_copyOnWrite) {
		_data = Rc<LinearGradientData>::alloc(*_data);
	}

	_data->start = start;
	_data->end = end;
	_data->steps = sp::move(steps);

	std::sort(_data->steps.begin(), _data->steps.end(),
			[](const GradientStep &l, const GradientStep &r) { return l.value < r.value; });
	/*if (_data->steps.front().value > 0.0f) {
		_data->steps.insert(_data->steps.begin(), GradientStep{0.0f, _data->steps.front().color});
	}
	if (_data->steps.back().value < 1.0f) {
		_data->steps.emplace_back(GradientStep{1.0f, _data->steps.back().color});
	}*/

	return true;
}

bool LinearGradient::updateWithData(const Vec2 &origin, float angle, float distance,
		Vector<GradientStep> &&steps) {
	return updateWithData(origin, origin + Vec2::forAngle(angle) * distance, sp::move(steps));
}

const Vec2 &LinearGradient::getStart() const { return _data->start; }
const Vec2 &LinearGradient::getEnd() const { return _data->end; }

const Vector<GradientStep> &LinearGradient::getSteps() const { return _data->steps; }

Rc<LinearGradientData> LinearGradient::pop() {
	_copyOnWrite = true;
	return _data;
}

// duplicate data, user can modify new data
Rc<LinearGradientData> LinearGradient::dup() { return Rc<LinearGradientData>::alloc(*_data); }

void LinearGradient::copy() {
	_data = Rc<LinearGradientData>::alloc(*_data);
	_copyOnWrite = false;
}

} // namespace stappler::xenolith
