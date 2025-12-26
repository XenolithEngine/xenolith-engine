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

#ifndef XENOLITH_APPLICATION_RESOURCES_XLCURVEBUFFER_H_
#define XENOLITH_APPLICATION_RESOURCES_XLCURVEBUFFER_H_

#include "XLInterpolation.h"
#include "XLLinearGradient.h"
#include "SPBitmap.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

enum class CurveBufferType {
	Float,
	Vec2,
	Vec3,
	Vec4,
};

class CurveBuffer : public Ref {
public:
	virtual ~CurveBuffer() = default;

	struct Interpolation {
		interpolation::Type type = interpolation::Type::QuadEaseInOut;
		SpanView<float> params;
	};

	struct RenderInfo {
		Extent2 size;
		UVec2 zero;
		UVec2 one;

		uint8_t background = 0;
		uint8_t controls = 0;
		uint8_t component = 0;
		uint8_t componentBackground = 0;
	};

	// fill values with 0-1 interval
	bool init(uint32_t npoints, const Callback<float(float)> &);
	bool init(uint32_t npoints, const Callback<Vec2(float)> &);
	bool init(uint32_t npoints, const Callback<Vec3(float)> &);
	bool init(uint32_t npoints, const Callback<Vec4(float)> &);

	bool init(uint32_t npoints, SpanView<GradientStep>);

	bool init(uint32_t npoints, interpolation::Type, SpanView<float> params);

	bool init(uint32_t npoints, const Interpolation &);
	bool init(uint32_t npoints, const std::array<Interpolation, 2> &);
	bool init(uint32_t npoints, const std::array<Interpolation, 3> &);
	bool init(uint32_t npoints, const std::array<Interpolation, 4> &);

	float getFloat(float) const;
	Vec2 getVec2(float) const;
	Vec3 getVec3(float) const;
	Vec4 getVec4(float) const;

	uint64_t getId() const { return _id; }

	size_t getSize() const;

	uint32_t getElementSize() const; // in floats

	const float *getData() const;

	CurveBufferType getType() const { return _type; }

	float getMin() const { return _min; }
	float getMax() const { return _max; }

	Bitmap renderComponent(const RenderInfo &, uint8_t component);

protected:
	uint64_t _id = 0;
	CurveBufferType _type = CurveBufferType::Float;
	Vector<float> _data;
	float _min = 0.0f;
	float _max = 1.0f;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_RESOURCES_XLCURVEBUFFER_H_ */
