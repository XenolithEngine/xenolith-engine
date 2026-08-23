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
#include "SPMemory.h"

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

// Two points closer than this are the same point as far as closing a contour is concerned:
// an explicit ClosePath on a subpath that already ended on its start point must not add a
// zero-length segment.
constexpr float getCloseControlDistance() { return sprt::Epsilon<float> * 32; }

// Everything that describes the stroke itself, as opposed to the line being stroked.
// Grouped into a struct because the parameter list would otherwise be seven positional
// floats and enums, and because `VectorPath` already stores exactly this set.
struct SP_PUBLIC StrokeConfig {
	float lineWidth = 1.0f;
	LineJoin lineJoin = LineJoin::Miter;
	LineCup lineCup = LineCup::Butt;
	float miterLimit = 4.0f;

	// Dash pattern, in path units. Empty means a solid stroke.
	// NOTE: a view, it does not own the lengths - the storage must outlive the LineDrawer.
	SpanView<float> dashArray;
	float dashOffset = 0.0f;
};

// Builds the stroke of a single open or closed polyline.
//
// The stroke is not a mesh of per-segment quads: it is one closed offset polygon (a "ribbon"),
// fed to the general sweepline tesselator with a NonZero winding rule, which is what resolves
// the self-intersections on inner corners. `Tesselator::pushStrokeVertex` emits both sides of
// the ribbon at once, so the loop ends up ordered as
//
//     top0 -> bottom0 -> bottom1 -> ... -> bottomN -> topN -> topN-1 -> ... -> top1 -> top0
//
// which makes the `top0 -> bottom0` edge the start cap and `bottomN -> topN` the end cap.
//
// This is split out of LineDrawer because a dashed stroke has to start and end a ribbon many
// times over one contour, so the ribbon state has to be separately resettable.
struct SP_PUBLIC StrokeWriter {
	void init(Tesselator *, const StrokeConfig &, float distanceError);

	explicit operator bool() const { return _tess != nullptr; }

	void begin();
	void lineTo(const Vec2 &);
	void end(bool closed);

	bool isOpen() const { return _open; }

	void pushStroke(const Vec2 &v0, const Vec2 &v1, const Vec2 &v2);

	Vec2 capOffset(const Vec2 &dir) const;
	void emitRoundCap(const Vec2 &);
	void emitDot(const Vec2 &);

	Tesselator *_tess = nullptr;
	Tesselator::Cursor _cursor;

	float _halfWidth = 0.0f;
	float _miterLimit = 4.0f;
	LineJoin _lineJoin = LineJoin::Miter;
	LineCup _lineCup = LineCup::Butt;
	uint32_t _capSegments = 0;

	Vec2 _origin[2];
	Vec2 _prev;
	Vec2 _cur;
	size_t _count = 0;
	bool _open = false;
};

// Cuts the stream of points into dashes and feeds each one to StrokeWriter as its own ribbon.
//
// This runs on the flattened polyline, after curves have been subdivided, so it needs no notion
// of curves at all - and, crucially, no buffering of the contour: it walks segment by segment,
// splitting one wherever a dash boundary falls inside it. The one thing it does hold is the
// FIRST dash, which cannot be emitted until it is known whether the contour closes: on a closed
// contour the last dash has to flow into the first one across the start point, or the pattern
// would show a seam with two caps in it exactly at the moveTo.
//
// When the pattern is empty or degenerate the writer is inactive and forwards straight through,
// so the caller never has to branch on it.
struct SP_PUBLIC DashWriter {
	void init(StrokeWriter *, SpanView<float> pattern, float offset);

	bool isActive() const { return _count > 0; }

	void begin();
	void lineTo(const Vec2 &);
	void end(bool closed);

	// Pattern entries, indexed logically: an odd-length pattern repeats twice (SVG), so the
	// logical sequence is twice as long as the stored one and the on/off phase alternates
	// across the repeat.
	float lengthAt(uint32_t idx) const { return _pattern[idx % _count]; }
	uint32_t logicalCount() const { return (_count % 2) ? _count * 2 : _count; }

	void resetPhase();
	void advance();

	void startDash(const Vec2 &);
	void addPoint(const Vec2 &);
	void endDash();
	void flushDeferred(bool closed);

	StrokeWriter *_writer = nullptr;
	const float *_pattern = nullptr;
	uint32_t _count = 0;
	float _offset = 0.0f;

	uint32_t _idx = 0;
	float _remain = 0.0f;
	bool _on = true;

	Vec2 _last;
	Vec2 _origin;
	bool _hasLast = false;

	mem_std::Vector<Vec2> _firstDash;
	bool _deferring = false;
	bool _firstDashDone = false;
	bool _dashOpen = false;

	// Runaway guard: a fine pattern over a long path is O(length/period) ribbons, and the
	// sweepline is superlinear in the contours it has to merge.
	uint32_t _emitted = 0;
};

// Helper class, that transform lines in SVG notation (bezier2/3, arcs) into series of segments,
// then output this segments to contour in tesselator
struct SP_PUBLIC LineDrawer {
	// `e` defines relative error in terms of maximum allowed distance between the point,
	// where line should be in perfect implementation, and the segment in output
	// For perfect VG quality, it should be around 0.75 of screen pixel
	LineDrawer(float e, Rc<Tesselator> &&tessFill, Rc<Tesselator> &&tessStroke,
			Rc<Tesselator> &&tessSdf, const StrokeConfig & = StrokeConfig());

	void drawBegin(float x, float y);
	void drawLine(float x, float y);
	void drawQuadBezier(float x1, float y1, float x2, float y2);
	void drawCubicBezier(float x1, float y1, float x2, float y2, float x3, float y3);
	void drawArc(float rx, float ry, float angle, bool largeArc, bool sweep, float x, float y);
	void drawClose(bool closed);

	void push(float x, float y);

	struct BufferNode {
		BufferNode *next;
		BufferNode *prev;
		Vec2 point;
	};

	DrawStyle style = DrawStyle::None;
	float distanceError = 0.0f;
	float angularError = 0.0f;
	size_t count = 0;

	/* Where this drawer's coordinates are measured from - the first point it is ever given.
	Everything past the five entry points above is relative to it, and the tesselators are told
	about it so their output is not.

	The tesselator already normalizes on its own first vertex, so this is not about the sweep. It
	is about everything that happens BEFORE the sweep: a bezier is subdivided, an arc is turned
	into segments and a stroke is offset along its normals, all in whatever coordinates the caller
	used. A half-width of two units offset from coordinates of 1e6, where a float's own step is
	0.0625, is three percent wrong before the tesselator has seen anything - and there is no
	epsilon downstream that can put those bits back. Subtracting here is what keeps the mantissa
	spent on the shape. */
	Vec2 drawOrigin;
	bool hasDrawOrigin = false;

	Vec2 origin[2];
	BufferNode buffer[3];
	BufferNode *target = nullptr;

	Rc<Tesselator> fill;
	Tesselator::Cursor fillCursor;

	Rc<Tesselator> stroke;
	StrokeWriter strokeWriter;
	DashWriter dashWriter;

	Rc<Tesselator> sdf;
	Tesselator::Cursor sdfCursor;
};

} // namespace stappler::geom

#endif /* STAPPLER_TESS_SPTESSLINE_H_ */
