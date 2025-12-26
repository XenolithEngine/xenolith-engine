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

#ifndef XENOLITH_APPLICATION_RESOURCES_XLLINEARGRADIENT_H_
#define XENOLITH_APPLICATION_RESOURCES_XLLINEARGRADIENT_H_

#include "XLCommon.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

struct SP_PUBLIC GradientStep {
	float value = 0.0f;
	float factor = 0.0f;
	Color4F color = Color4F::WHITE;

	GradientStep() = default;
	GradientStep(float v, Color4F c) : value(v), color(c) { }
	GradientStep(float v, float f, Color4F c) : value(v), factor(f), color(c) { }

	GradientStep(const GradientStep &) = default;
	GradientStep &operator=(const GradientStep &) = default;
};

struct SP_PUBLIC LinearGradientData : public Ref {
	virtual ~LinearGradientData() = default;

	Vec2 start;
	Vec2 end;
	Vector<GradientStep> steps;

	LinearGradientData() = default;
	LinearGradientData(const LinearGradientData &);
	LinearGradientData(LinearGradientData &&);
};

class SP_PUBLIC LinearGradient : public Ref {
public:
	virtual ~LinearGradient() = default;

	// start and end in node space
	bool init(const Vec2 &start, const Vec2 &end, Vector<GradientStep> &&);
	bool init(const Vec2 &origin, float angle, float distance, Vector<GradientStep> &&);

	bool updateWithData(const Vec2 &start, const Vec2 &end, Vector<GradientStep> &&);
	bool updateWithData(const Vec2 &origin, float angle, float distance, Vector<GradientStep> &&);

	const Vec2 &getStart() const;
	const Vec2 &getEnd() const;

	const Vector<GradientStep> &getSteps() const;

	// pop data, mark for copy on write
	// user should not modify data
	Rc<LinearGradientData> pop();

	// duplicate data, user can modify new data
	Rc<LinearGradientData> dup();

protected:
	void copy();

	bool _copyOnWrite = false;
	Rc<LinearGradientData> _data;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLLINEARGRADIENT_H_ */
