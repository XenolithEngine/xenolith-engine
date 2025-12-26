/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#ifndef CORE_VG_SPVECTORPATHDATA_H_
#define CORE_VG_SPVECTORPATHDATA_H_

#include "SPColor.h"
#include "SPGeometry.h"
#include "SPMat4.h"
#include "SPTessLine.h"
#include "SPMemory.h"

namespace STAPPLER_VERSIONIZED stappler::vg {

using namespace geom;

union SP_PUBLIC CommandData {
	struct {
		float x;
		float y;
	} p;
	struct {
		float v;
		bool a;
		bool b;
	} f;

	CommandData(float x, float y) { p.x = x; p.y = y; }
	CommandData(float r, bool a, bool b) { f.v = r; f.a = a; f.b = b; }
	CommandData() { p = {0.0f, 0.0f}; }
};

enum class Command : uint8_t { // use hint to decode data from `_points` vector
	MoveTo, // (x, y)
	LineTo, // (x, y)
	QuadTo, // (x1, y1) (x2, y2)
	CubicTo, // (x1, y1) (x2, y2) (x3, y3)
	ArcTo, // (rx, ry), (x, y), (rotation, largeFlag, sweepFlag)
	ClosePath, // nothing
};

struct SP_PUBLIC PathParams {
	Mat4 transform;
	Color4B fillColor = Color4B(255, 255, 255, 255);
	Color4B strokeColor = Color4B(255, 255, 255, 255);
	DrawFlags style = DrawFlags::Fill;
	float strokeWidth = 1.0f;

	Winding winding = Winding::NonZero;
	LineCup lineCup = LineCup::Butt;
	LineJoin lineJoin = LineJoin::Miter;
	float miterLimit = 4.0f;
	bool isAntialiased = true;
};

struct PathWriter;

template <typename Interface>
struct SP_PUBLIC PathData : Interface::AllocBaseType {
	template <typename Value>
	using Vector = typename Interface::template VectorType<Value>;

	Vector<CommandData> points;
	Vector<Command> commands;
	Vector<Vec2> uv;

	PathParams params;

	PathData() = default;
	PathData(const PathData &) = default;
	PathData &operator=(const PathData &) = default;

	PathData(PathData &&) = default;
	PathData &operator=(PathData &&) = default;

	void clear();

	PathWriter getWriter();

	template <typename OutInterface>
	auto encode() const -> typename OutInterface::BytesType;

	template <typename OutInterface>
	auto toString(bool newline = false) const -> typename OutInterface::StringType;
};

struct SP_PUBLIC PathWriter {
	VectorAdapter<CommandData> points;
	VectorAdapter<Command> commands;
	VectorAdapter<Vec2> uvPoints;

	PathWriter(PathData<mem_std::Interface> &);
	PathWriter(PathData<mem_pool::Interface> &);

	PathWriter() = default;
	PathWriter(const PathWriter &) = default;
	PathWriter &operator=(const PathWriter &) = default;

	PathWriter(PathWriter &&) = default;
	PathWriter &operator=(PathWriter &&) = default;

	explicit operator bool () const;

	bool empty() const;

	void reserve(size_t);

	bool readFromPathString(StringView);
	bool readFromFileContent(StringView);
	bool readFromFile(const FileInfo &);
	bool readFromBytes(BytesView);

	PathWriter &moveTo(float x, float y, float u = nan(), float v = nan());
	PathWriter &moveTo(const Vec2 &point, const Vec2 &uv = Vec2(nan(), nan()));

	PathWriter &lineTo(float x, float y, float u = nan(), float v = nan());
	PathWriter &lineTo(const Vec2 &point, const Vec2 &uv = Vec2(nan(), nan()));

	PathWriter &quadTo(float x1, float y1, float x2, float y2, float u = nan(), float v = nan());
	PathWriter &quadTo(const Vec2& p1, const Vec2& p2, const Vec2 &uv = Vec2(nan(), nan()));

	PathWriter &cubicTo(float x1, float y1, float x2, float y2, float x3, float y3, float u = nan(), float v = nan());
	PathWriter &cubicTo(const Vec2& p1, const Vec2& p2, const Vec2& p3, const Vec2 &uv = Vec2(nan(), nan()));

	PathWriter &arcTo(float rx, float ry, float rotation, bool largeFlag, bool sweepFlag, float x, float y, float u = nan(), float v = nan());
	PathWriter &arcTo(const Vec2 & r, float rotation, bool largeFlag, bool sweepFlag, const Vec2 &target, const Vec2 &uv = Vec2(nan(), nan()));
	PathWriter &closePath();

	PathWriter &addRect(const Rect& rect);
	PathWriter &addRect(const Rect& rect, float rx, float ry);
	PathWriter &addRect(float x, float y, float width, float height);
	PathWriter &addOval(const Rect& oval);
	PathWriter &addCircle(float x, float y, float radius);
	PathWriter &addEllipse(float x, float y, float rx, float ry);
	PathWriter &addArc(const Rect& oval, float startAngleInRadians, float sweepAngleInRadians);
	PathWriter &addRect(float x, float y, float width, float height, float rx, float ry);

	bool addPath(const PathData<memory::StandartInterface> &);
	bool addPath(const PathData<memory::PoolInterface> &);
	bool addPath(BytesView);
	bool addPath(StringView);
};


}

#endif /* CORE_VG_SPVECTORPATHDATA_H_ */
