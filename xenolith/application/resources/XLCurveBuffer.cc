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

#include "XLCurveBuffer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

static std::atomic<uint64_t> s_curveBufferId = 1;

// fill values with 0-1 interval
bool CurveBuffer::init(uint32_t npoints, const Callback<float(float)> &cb) {
	_id = s_curveBufferId.fetch_add(1);
	_type = CurveBufferType::Float;
	_data.resize(npoints);

	auto d = _data.data();

	float val = 0.0f;
	float dx = 1.0f / npoints;
	for (uint32_t i = 0; i < npoints; ++i) {
		*d++ = cb(val);
		val += dx;
	}
	return true;
}

bool CurveBuffer::init(uint32_t npoints, const Callback<Vec2(float)> &cb) {
	_id = s_curveBufferId.fetch_add(1);
	_type = CurveBufferType::Vec2;
	_data.resize(npoints * 2);

	auto d = _data.data();

	float val = 0.0f;
	float dx = 1.0f / npoints;
	for (uint32_t i = 0; i < npoints; ++i) {
		auto v = cb(val);
		*d++ = v.x;
		*d++ = v.y;
		val += dx;
	}
	return true;
}

bool CurveBuffer::init(uint32_t npoints, const Callback<Vec3(float)> &cb) {
	_id = s_curveBufferId.fetch_add(1);
	_type = CurveBufferType::Vec3;
	_data.resize(npoints * 3);

	auto d = _data.data();

	float val = 0.0f;
	float dx = 1.0f / npoints;
	for (uint32_t i = 0; i < npoints; ++i) {
		auto v = cb(val);
		*d++ = v.x;
		*d++ = v.y;
		*d++ = v.z;
		val += dx;
	}
	return true;
}

bool CurveBuffer::init(uint32_t npoints, const Callback<Vec4(float)> &cb) {
	_id = s_curveBufferId.fetch_add(1);
	_type = CurveBufferType::Vec4;
	_data.resize(npoints * 4);

	auto d = _data.data();

	float val = 0.0f;
	float dx = 1.0f / npoints;
	for (uint32_t i = 0; i < npoints; ++i) {
		auto v = cb(val);
		*d++ = v.x;
		*d++ = v.y;
		*d++ = v.z;
		*d++ = v.w;
		val += dx;
	}
	return true;
}

bool CurveBuffer::init(uint32_t npoints, SpanView<GradientStep> steps) {
	_id = s_curveBufferId.fetch_add(1);
	_type = CurveBufferType::Vec4;
	_data.resize(npoints * 4);

	auto d = _data.data();

	float val = 0.0f;
	float dx = 1.0f / npoints;

	for (uint32_t n = 0; n < npoints; ++n) {
		auto gradColor = math::lerp(steps[0].color, steps[1].color,
				math::smoothstep(steps[0].value, steps[1].value, val));

		for (size_t i = 2; i < steps.size(); ++i) {
			gradColor = math::lerp(gradColor, steps[i].color,
					math::smoothstep(steps[i - 1].value, steps[i].value, val));
		}

		*d++ = gradColor.r;
		*d++ = gradColor.g;
		*d++ = gradColor.b;
		*d++ = gradColor.a;

		val += dx;
	}
	return true;
}

bool CurveBuffer::init(uint32_t npoints, interpolation::Type t, SpanView<float> params) {
	_id = s_curveBufferId.fetch_add(1);
	_type = CurveBufferType::Float;
	_data.resize(npoints);

	auto d = _data.data();

	float val = 0.0f;
	float dx = 1.0f / npoints;
	for (uint32_t i = 0; i < npoints; ++i) {
		*d++ = interpolation::interpolateTo(val, t, params);
		val += dx;
	}
	return true;
}

bool CurveBuffer::init(uint32_t npoints, const Interpolation &t) {
	_id = s_curveBufferId.fetch_add(1);
	_type = CurveBufferType::Float;
	_data.resize(npoints);

	auto d = _data.data();

	float val = 0.0f;
	float dx = 1.0f / npoints;
	for (uint32_t i = 0; i < npoints; ++i) {
		*d++ = interpolation::interpolateTo(val, t.type, t.params);
		val += dx;
	}
	return true;
}

bool CurveBuffer::init(uint32_t npoints, const std::array<Interpolation, 2> &t) {
	_id = s_curveBufferId.fetch_add(1);
	_type = CurveBufferType::Vec2;
	_data.resize(npoints * 2);

	auto d = _data.data();

	float val = 0.0f;
	float dx = 1.0f / npoints;
	for (uint32_t i = 0; i < npoints; ++i) {
		*d++ = interpolation::interpolateTo(val, t[0].type, t[0].params);
		*d++ = interpolation::interpolateTo(val, t[1].type, t[1].params);
		val += dx;
	}
	return true;
}

bool CurveBuffer::init(uint32_t npoints, const std::array<Interpolation, 3> &t) {
	_id = s_curveBufferId.fetch_add(1);
	_type = CurveBufferType::Vec3;
	_data.resize(npoints * 3);

	auto d = _data.data();

	float val = 0.0f;
	float dx = 1.0f / npoints;
	for (uint32_t i = 0; i < npoints; ++i) {
		*d++ = interpolation::interpolateTo(val, t[0].type, t[0].params);
		*d++ = interpolation::interpolateTo(val, t[1].type, t[1].params);
		*d++ = interpolation::interpolateTo(val, t[2].type, t[2].params);
		val += dx;
	}
	return true;
}

bool CurveBuffer::init(uint32_t npoints, const std::array<Interpolation, 4> &t) {
	_id = s_curveBufferId.fetch_add(1);
	_type = CurveBufferType::Vec4;
	_data.resize(npoints * 4);

	auto d = _data.data();

	float val = 0.0f;
	float dx = 1.0f / npoints;
	for (uint32_t i = 0; i < npoints; ++i) {
		*d++ = interpolation::interpolateTo(val, t[0].type, t[0].params);
		*d++ = interpolation::interpolateTo(val, t[1].type, t[1].params);
		*d++ = interpolation::interpolateTo(val, t[2].type, t[2].params);
		*d++ = interpolation::interpolateTo(val, t[3].type, t[3].params);
		val += dx;
	}
	return true;
}

template <typename Vec>
static auto CurveBuffer_getVec(const CurveBuffer &buf, float val) {
	auto eltSize = buf.getElementSize();
	auto size = buf.getSize();
	auto v = val * size;

	auto firstVal = math::clamp(std::floor(v), 0.0f, float(size - 1.0f));
	auto secondVal = math::clamp(std::ceil(v), 0.0f, float(size - 1.0f));

	auto data = buf.getData();

	if (firstVal == secondVal) {
		if (auto content = data + static_cast<uint32_t>(firstVal) * eltSize) {
			return Vec(makeSpanView(content, eltSize));
		}
	} else {
		auto x = math::smoothstep(firstVal, secondVal, val * size);

		auto firstD = data + static_cast<uint32_t>(firstVal) * eltSize;
		auto secondD = data + static_cast<uint32_t>(secondVal) * eltSize;

		if (firstD && secondD) {
			return math::lerp(Vec(makeSpanView(firstD, eltSize)),
					Vec(makeSpanView(secondD, eltSize)), x);
		}
	}
	return Vec::INVALID;
}

float CurveBuffer::getFloat(float val) const { return CurveBuffer_getVec<Vec1>(*this, val).x; }

Vec2 CurveBuffer::getVec2(float val) const {
	switch (_type) {
	case CurveBufferType::Float: {
		auto v = CurveBuffer_getVec<Vec1>(*this, val);
		return Vec2(v.x, v.x);
		break;
	}
	case CurveBufferType::Vec2: {
		auto v = CurveBuffer_getVec<Vec2>(*this, val);
		return Vec2(v.x, v.y);
		break;
	}
	case CurveBufferType::Vec3: {
		auto v = CurveBuffer_getVec<Vec3>(*this, val);
		return Vec2(v.x, v.y);
		break;
	}
	case CurveBufferType::Vec4: {
		auto v = CurveBuffer_getVec<Vec4>(*this, val);
		return Vec2(v.x, v.y);
		break;
	}
	}
	return Vec2::INVALID;
}

Vec3 CurveBuffer::getVec3(float val) const {
	switch (_type) {
	case CurveBufferType::Float: {
		auto v = CurveBuffer_getVec<Vec1>(*this, val);
		return Vec3(v.x, v.x, v.x);
		break;
	}
	case CurveBufferType::Vec2: {
		auto v = CurveBuffer_getVec<Vec2>(*this, val);
		return Vec3(v.x, v.y, 0.0f);
		break;
	}
	case CurveBufferType::Vec3: {
		auto v = CurveBuffer_getVec<Vec3>(*this, val);
		return Vec3(v.x, v.y, v.z);
		break;
	}
	case CurveBufferType::Vec4: {
		auto v = CurveBuffer_getVec<Vec4>(*this, val);
		return Vec3(v.x, v.y, v.z);
		break;
	}
	}
	return Vec3::INVALID;
}

Vec4 CurveBuffer::getVec4(float val) const {
	switch (_type) {
	case CurveBufferType::Float: {
		auto v = CurveBuffer_getVec<Vec1>(*this, val);
		return Vec4(v.x, v.x, v.x, v.x);
		break;
	}
	case CurveBufferType::Vec2: {
		auto v = CurveBuffer_getVec<Vec2>(*this, val);
		return Vec4(v.x, v.y, 0.0f, 0.0f);
		break;
	}
	case CurveBufferType::Vec3: {
		auto v = CurveBuffer_getVec<Vec3>(*this, val);
		return Vec4(v.x, v.y, v.z, 0.0f);
		break;
	}
	case CurveBufferType::Vec4: {
		auto v = CurveBuffer_getVec<Vec4>(*this, val);
		return Vec4(v.x, v.y, v.z, v.w);
		break;
	}
	}
	return Vec4::INVALID;
}

size_t CurveBuffer::getSize() const { return _data.size() / getElementSize(); }

uint32_t CurveBuffer::getElementSize() const {
	switch (_type) {
	case CurveBufferType::Float: return 1; break;
	case CurveBufferType::Vec2: return 2; break;
	case CurveBufferType::Vec3: return 3; break;
	case CurveBufferType::Vec4: return 4; break;
	}
	return 0;
}

const float *CurveBuffer::getData() const { return _data.data(); }

Bitmap CurveBuffer::renderComponent(const RenderInfo &info, uint8_t component) {
	if (info.zero.x > info.size.width || info.zero.y > info.size.height
			|| info.one.x > info.size.width || info.one.y > info.size.height
			|| info.zero.x > info.one.x || info.zero.y > info.one.y) {
		log::source().error("CurveBuffer", "Invalid RenderInfo format");
		return Bitmap();
	}

	auto ncomp = getElementSize();
	if (component >= ncomp) {
		log::source().error("CurveBuffer", "Invalid component index: ", uint32_t(component));
		return Bitmap();
	}

	bitmap::PixelFormat fmt = bitmap::PixelFormat::I8;

	Bitmap bmp;
	bmp.alloc(info.background, info.size.width, info.size.height, fmt);

	auto dataPtr = bmp.dataPtr();

	auto dataWidth = info.one.x - info.zero.x;
	auto dataHeight = info.one.y - info.zero.y;
	float value = 0.0f;
	float dx = 1.0f / dataWidth;

	for (uint32_t i = 0; i < info.size.width; ++i) {
		dataPtr[(info.size.width * (info.size.height - 1 - info.zero.y)) + i] = info.controls;
		dataPtr[(info.size.width * (info.size.height - 1 - info.one.y)) + i] = info.controls;
	}

	for (uint32_t j = 0; j < info.size.height; ++j) {
		dataPtr[(info.size.width * (info.size.height - 1 - j)) + info.zero.x] = info.controls;
		dataPtr[(info.size.width * (info.size.height - 1 - j)) + info.one.x] = info.controls;
	}

	for (uint32_t i = 0; i < dataWidth; ++i) {
		auto vec = getVec4(value) * dataHeight;
		float v = 0.0f;
		switch (component) {
		case 0: v = vec.x; break;
		case 1: v = vec.y; break;
		case 2: v = vec.z; break;
		case 3: v = vec.w; break;
		}

		uint32_t index = maxOf<uint32_t>();
		if (v > 0) {
			for (uint32_t j = 0; j < info.size.height; ++j) {
				auto targetCell = (info.size.width * (info.size.height - 1 - j)) + info.zero.x + i;

				if (j > info.zero.y && j < v + info.zero.y) {
					dataPtr[targetCell] = info.componentBackground;
					index = j;
				}
			}
		} else {
			for (uint32_t j = info.size.height; j != 0; --j) {
				auto targetCell =
						(info.size.width * (info.size.height - 1 - (j - 1))) + info.zero.x + i;

				if ((j - 1) < info.zero.y && (j - 1) > v + info.zero.y) {
					dataPtr[targetCell] = info.componentBackground;
					index = (j - 1);
				}
			}
		}

		if (index != maxOf<uint32_t>()) {
			auto targetCell = (info.size.width * (info.size.height - 1 - index)) + info.zero.x + i;
			dataPtr[targetCell] = info.component;
		}

		value += dx;
	}

	return bmp;
}

} // namespace stappler::xenolith
