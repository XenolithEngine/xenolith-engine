/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef STAPPLER_TESS_SPTESSLINE_H_
#define STAPPLER_TESS_SPTESSLINE_H_

#include "SPTess.h"

namespace STAPPLER_VERSIONIZED stappler::geom {

enum class LineCup {
	Butt,
	Round,
	Square
};

enum class LineJoin {
	Miter,
	Round,
	Bevel
};

enum class DrawFlags : uint32_t {
	None = 0,
	Fill = 1 << 0,
	Stroke = 1 << 1,
	FillAndStroke = Fill | Stroke,
	PseudoSdf = 1 << 2,
	UV = 1 << 3
};

SP_DEFINE_ENUM_AS_MASK(DrawFlags)

using DrawStyle = DrawFlags;

// Helper class, that transform lines in SVG notation (bezier2/3, arcs) into series of segments,
// then output this segments to contour in tesselator
struct SP_PUBLIC LineDrawer {
	// `e` defines relative error in terms of maximum allowed distance between the point,
	// where line should be in perfect implementation, and the segment in output
	// For perfect VG quality, it should be around 0.75 of screen pixel
	LineDrawer(float e, Rc<Tesselator> &&tessFill, Rc<Tesselator> &&tessStroke,
			Rc<Tesselator> &&tessSdf, float lineWidth = 1.0f, LineJoin = LineJoin::Miter,
			LineCup = LineCup::Butt);

	void drawBegin(float x, float y);
	void drawLine(float x, float y);
	void drawQuadBezier(float x1, float y1, float x2, float y2);
	void drawCubicBezier(float x1, float y1, float x2, float y2, float x3, float y3);
	void drawArc(float rx, float ry, float angle, bool largeArc, bool sweep, float x, float y);
	void drawClose(bool closed);

	void push(float x, float y);
	void pushStroke(const Vec2 &v0, const Vec2 &v1, const Vec2 &v2);

	struct BufferNode {
		BufferNode *next;
		BufferNode *prev;
		Vec2 point;
	};

	DrawStyle style = DrawStyle::None;
	LineJoin lineJoin = LineJoin::Miter;
	LineCup lineCup = LineCup::Butt;
	float distanceError = 0.0f;
	float angularError = 0.0f;
	float strokeWidth = 0.0f;
	size_t count = 0;
	Vec2 origin[2];
	BufferNode buffer[3];
	BufferNode *target = nullptr;

	Rc<Tesselator> fill;
	Tesselator::Cursor fillCursor;

	Rc<Tesselator> stroke;
	Tesselator::Cursor strokeCursor;

	Rc<Tesselator> sdf;
	Tesselator::Cursor sdfCursor;

	float _miterLimit = 4.0f;
};

} // namespace stappler::geom

#endif /* STAPPLER_TESS_SPTESSLINE_H_ */
