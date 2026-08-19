/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#include "SPTessLine.h"
#include "SPTessSimd.hpp"

#include <sprt/runtime/geom/vec4.h>

namespace STAPPLER_VERSIONIZED stappler::geom {

using sprt::geom::Vec4;

// Beyond this many dashes on one contour the pattern stops being a visual feature and starts
// being a way to hang the tesselator; the rest of the contour is drawn solid instead.
static constexpr uint32_t getMaxDashCount() { return 8192; }

void DashWriter::init(StrokeWriter *writer, SpanView<float> pattern, float offset) {
	_writer = writer;
	_pattern = nullptr;
	_count = 0;
	_offset = offset;

	if (pattern.empty()) {
		return;
	}

	float period = 0.0f;
	for (auto &it : pattern) {
		// A negative entry invalidates the whole list (SVG), and a NaN would poison every
		// comparison downstream.
		if (it < 0.0f || sprt::isnan(it)) {
			return;
		}
		period += it;
	}

	// An all-zero pattern has no period to advance along: the splitting loop would never
	// consume any of the segment it is walking.
	if (period <= sprt::Epsilon<float>) {
		return;
	}

	_pattern = pattern.data();
	_count = uint32_t(pattern.size());
}

void DashWriter::resetPhase() {
	_idx = 0;
	_on = true;
	_remain = lengthAt(0);

	if (_offset == 0.0f || sprt::isnan(_offset)) {
		return;
	}

	const uint32_t logical = logicalCount();
	float period = 0.0f;
	for (uint32_t i = 0; i < logical; ++i) { period += lengthAt(i); }

	float offset = fmodf(_offset, period);
	if (offset < 0.0f) {
		offset += period;
	}

	// Bounded by construction: the period is non-zero, so each full turn of the pattern
	// consumes it - but zero-length entries make individual steps free, hence the explicit cap.
	uint32_t guard = logical * 2 + 2;
	while (offset > 0.0f && guard-- > 0) {
		if (offset >= _remain) {
			offset -= _remain;
			advance();
		} else {
			_remain -= offset;
			offset = 0.0f;
		}
	}
}

void DashWriter::advance() {
	_idx = (_idx + 1) % logicalCount();
	_on = (_idx % 2) == 0;
	_remain = lengthAt(_idx);
}

void DashWriter::startDash(const Vec2 &p) {
	if (_dashOpen) {
		return;
	}

	if (_deferring) {
		_firstDash.clear();
		_firstDash.emplace_back(p);
	} else {
		_writer->begin();
		_writer->lineTo(p);
	}
	_dashOpen = true;
}

void DashWriter::addPoint(const Vec2 &p) {
	if (!_dashOpen) {
		return;
	}

	if (_deferring) {
		_firstDash.emplace_back(p);
	} else {
		_writer->lineTo(p);
	}
}

void DashWriter::endDash() {
	if (!_dashOpen) {
		return;
	}

	if (_deferring) {
		// Hold the first dash back: whether it is a dash of its own or the tail of the last one
		// is not known until the contour ends. Every dash after it goes straight through.
		_deferring = false;
		_firstDashDone = true;
	} else {
		_writer->end(false);
	}

	_dashOpen = false;
	++_emitted;
}

void DashWriter::begin() {
	if (!isActive()) {
		_writer->begin();
		return;
	}

	resetPhase();
	_firstDash.clear();
	_hasLast = false;
	_deferring = true;
	_firstDashDone = false;
	_dashOpen = false;
	_emitted = 0;
}

void DashWriter::lineTo(const Vec2 &p) {
	if (!isActive()) {
		_writer->lineTo(p);
		return;
	}

	if (!_hasLast) {
		_origin = _last = p;
		_hasLast = true;
		if (_on) {
			startDash(p);
		}
		return;
	}

	const float len = _last.distance(p);
	if (len < getCloseControlDistance()) {
		return;
	}

	if (_emitted >= getMaxDashCount()) {
		// Guard tripped: keep drawing, but stop cutting.
		addPoint(p);
		_last = p;
		return;
	}

	float t = 0.0f;
	uint32_t guard = logicalCount() * 2 + 2;
	while (len - t > _remain) {
		t += _remain;

		const auto q = _last + (p - _last) * (t / len);
		if (_on) {
			addPoint(q);
			endDash();
		} else {
			startDash(q);
		}

		advance();

		if (_remain > 0.0f) {
			guard = logicalCount() * 2 + 2;
		} else if (guard-- == 0) {
			break; // a run of zero-length entries that never consumes the segment
		}

		if (_emitted >= getMaxDashCount()) {
			stappler::log::source().warn("LineDrawer", "dash pattern exceeded ", getMaxDashCount(),
					" dashes on one contour, drawing the rest solid");
			break;
		}
	}

	_remain -= (len - t);
	if (_on) {
		addPoint(p);
	}
	_last = p;
}

// Emits whatever the first dash turned out to be, once the contour's shape is known.
void DashWriter::flushDeferred(bool closed) {
	if (_firstDash.empty()) {
		return;
	}

	_writer->begin();
	for (auto &it : _firstDash) { _writer->lineTo(it); }
	_writer->end(closed);
	_firstDash.clear();
}

void DashWriter::end(bool closed) {
	if (!isActive()) {
		_writer->end(closed);
		return;
	}

	// A ClosePath does not arrive as a point, so the segment back to the start is walked here.
	if (closed && _hasLast && !_last.fuzzyEquals(_origin, getCloseControlDistance())) {
		lineTo(_origin);
	}

	if (_dashOpen && _deferring) {
		// The pattern never switched off: the contour is one dash, and a closed one can be a
		// closed ribbon just like an undashed stroke.
		_dashOpen = false;
		_deferring = false;
		flushDeferred(closed);
	} else if (closed && _dashOpen && _firstDashDone) {
		// Sew the seam: the dash running through the start point continues into the first one,
		// so the join at the moveTo looks like any other join instead of two caps meeting.
		for (auto &it : _firstDash) { _writer->lineTo(it); }
		_writer->end(false);
		_dashOpen = false;
		_firstDash.clear();
	} else {
		if (_dashOpen) {
			_writer->end(false);
			_dashOpen = false;
		}
		flushDeferred(false);
	}

	_firstDashDone = false;
	_hasLast = false;
}

void StrokeWriter::init(Tesselator *tess, const StrokeConfig &cfg, float distanceError) {
	_tess = tess;
	_halfWidth = cfg.lineWidth / 2.0f;
	_miterLimit = cfg.miterLimit;
	_lineJoin = cfg.lineJoin;
	_lineCup = cfg.lineCup;

	// Same chord-error criterion the arc flattener uses (see drawArcBegin): how far a segment of
	// the polygon may sag from the true circle before it needs splitting. Computed once here
	// rather than per cap - a dashed line emits two of them per dash.
	//
	// The tolerance is NOT `distanceError` alone. That one is deliberately loosened for wide
	// strokes (draw_approx_err_sq(e * log2(w))) because an error along the centre line is hidden
	// by the width of the ribbon - but a cap arc has a radius of only half that width, so the
	// same tolerance would show as a visibly polygonal end. Cap it at a fraction of the radius.
	if (_lineCup == LineCup::Round && _halfWidth > sprt::Epsilon<float>) {
		const float tolerance = sprt::min(sqrtf(distanceError), _halfWidth * 0.05f);
		const float err = (_halfWidth - tolerance) / _halfWidth;

		_capSegments = uint32_t(ceilf(float(M_PI * 2.0) / (acosf(err) * 2.0f)));
		_capSegments = sprt::clamp(_capSegments, uint32_t(12), uint32_t(64));

		// An even count puts a vertex on both ends of every diameter, so a cap reaches exactly
		// half a width past the end point rather than falling short by the sagitta.
		if (_capSegments % 2) {
			++_capSegments;
		}
	}
}

// How far past the end point the ribbon reaches. A square cap is exactly the line extended by
// half its width, so it needs no geometry of its own; butt and round do not move the end point.
Vec2 StrokeWriter::capOffset(const Vec2 &dir) const {
	if (_lineCup != LineCup::Square) {
		return Vec2::ZERO;
	}

	auto norm = dir;
	norm.normalize();
	return norm * _halfWidth;
}

// A round cap is emitted as a separate closed contour rather than woven into the ribbon.
// The stroke tesselator already runs NonZero, so a disc overlapping the end of the ribbon merges
// with it and no seam survives - and unlike splicing an arc into the half-edge loop, this cannot
// turn the ribbon inside out if the traversal order is misjudged.
//
// The winding direction is NOT free: under NonZero a contour wound against the ribbon cancels it
// exactly where the two overlap, and the whole stroke disappears. The ribbon runs clockwise in
// this coordinate system, so the disc has to as well - hence the negated angle.
void StrokeWriter::emitRoundCap(const Vec2 &p) {
	if (_lineCup != LineCup::Round || _halfWidth <= sprt::Epsilon<float>) {
		return;
	}

	auto cursor = _tess->beginContour();
	const float step = float(M_PI * 2.0) / float(_capSegments);
	for (uint32_t i = 0; i < _capSegments; ++i) {
		const float a = -step * float(i);
		_tess->pushVertex(cursor, Vec2(p.x + cosf(a) * _halfWidth, p.y + sinf(a) * _halfWidth));
	}
	_tess->closeContour(cursor);
}

// A zero-length dash: there is no direction to offset along, so the ribbon degenerates to
// nothing. Round and square caps still have an extent, and that extent is the dot of a dotted
// line; with a butt cap SVG says there is nothing to draw.
void StrokeWriter::emitDot(const Vec2 &p) {
	switch (_lineCup) {
	case LineCup::Butt: break;
	case LineCup::Round: emitRoundCap(p); break;
	case LineCup::Square: {
		// Wound clockwise, to match emitRoundCap - see the note there.
		auto cursor = _tess->beginContour();
		_tess->pushVertex(cursor, Vec2(p.x - _halfWidth, p.y - _halfWidth));
		_tess->pushVertex(cursor, Vec2(p.x - _halfWidth, p.y + _halfWidth));
		_tess->pushVertex(cursor, Vec2(p.x + _halfWidth, p.y + _halfWidth));
		_tess->pushVertex(cursor, Vec2(p.x + _halfWidth, p.y - _halfWidth));
		_tess->closeContour(cursor);
		break;
	}
	}
}

void StrokeWriter::begin() {
	if (!_tess) {
		return;
	}

	_cursor = _tess->beginContour();
	_count = 0;
	_open = true;
}

void StrokeWriter::lineTo(const Vec2 &p) {
	if (!_tess) {
		return;
	}

	// A repeated point has no direction, so it would put a zero-length segment into the ribbon
	// and a normalize() of (0,0) into the join math. Curve flattening emits these, and so does
	// a zero-length dash - which is what a dotted pattern is made of.
	if (_count > 0 && p.fuzzyEquals(_cur, getCloseControlDistance())) {
		return;
	}

	if (_count < 2) {
		_origin[_count] = p;
	}

	// The join at a vertex needs the segment on either side of it, so a point can only be
	// emitted once its successor is known - the ribbon runs two points behind the input.
	if (_count > 1) {
		pushStroke(_prev, _cur, p);
	}

	_prev = _cur;
	_cur = p;
	++_count;
}

void StrokeWriter::end(bool closed) {
	if (!_tess || _count == 0) {
		return;
	}

	if (closed && _count > 2) {
		if (_cur.fuzzyEquals(_origin[0], getCloseControlDistance())) {
			pushStroke(_prev, _origin[0], _origin[1]);
		} else {
			pushStroke(_prev, _cur, _origin[0]);
			pushStroke(_cur, _origin[0], _origin[1]);
		}

		_tess->closeStrokeContour(_cursor);
	} else if (_count > 1) {
		auto norm = _cur - _prev;
		norm.normalize();
		auto perp = norm.getRPerp();
		perp.negate();

		// A two-point contour never reaches pushStroke - that needs a vertex with a segment on
		// either side - so the ribbon has not been opened yet and the start pair is still owed.
		// Every dash is exactly such a contour, so this is the common case once dashes are on.
		if (!_cursor.edge) {
			_tess->pushStrokeVertex(_cursor, _prev - capOffset(_cur - _prev), perp * _halfWidth);
		}

		_tess->pushStrokeVertex(_cursor, _cur + capOffset(_cur - _prev), perp * _halfWidth);

		emitRoundCap(_origin[0]);
		emitRoundCap(_cur);
	} else {
		emitDot(_cur);
	}

	_count = 0;
	_open = false;
}

void StrokeWriter::pushStroke(const Vec2 &v0, const Vec2 &v1, const Vec2 &v2) {
	Vec4 result;
	getVertexNormal(&v0.x, &v1.x, &v2.x, &result.x);

	float mod = copysign(result.y * _halfWidth, result.x);
	if (!_cursor.edge) {
		auto norm = v1 - v0;
		norm.normalize();
		auto perp = norm.getRPerp();
		perp.negate();

		_tess->pushStrokeVertex(_cursor, v0 - capOffset(v1 - v0), perp * _halfWidth);
	}

	if (sprt::abs(result.y) < _miterLimit) {
		_tess->pushStrokeVertex(_cursor, v1, Vec2(result.z * mod, result.w * mod));
	} else {
		auto l0 = v1.distanceSquared(v0);
		auto l2 = v1.distanceSquared(v2);

		float qSquared;
		if (l0 > l2) {
			qSquared = l2 / (result.y * result.y - 1);
		} else {
			qSquared = l0 / (result.y * result.y - 1);
		}

		float inverseMiterLimitSq = result.y * result.y * qSquared;
		float offsetLengthSq = mod * mod;

		if (offsetLengthSq > inverseMiterLimitSq) {
			mod = copysign(sqrt(inverseMiterLimitSq), result.x);
		}

		if (mod > 0.0f) {
			do {
				auto norm = v1 - v0;
				norm.normalize();
				auto perp = norm.getRPerp();
				_tess->pushStrokeBottom(_cursor, v1 + perp * _halfWidth);
			} while (0);

			do {
				auto norm = v2 - v1;
				norm.normalize();
				auto perp = norm.getRPerp();
				_tess->pushStrokeBottom(_cursor, v1 + perp * _halfWidth);
			} while (0);

			_tess->pushStrokeTop(_cursor, v1 + Vec2(result.z * mod, result.w * mod));
		} else {
			_tess->pushStrokeBottom(_cursor, v1 - Vec2(result.z * mod, result.w * mod));

			do {
				auto norm = v1 - v0;
				norm.normalize();
				auto perp = norm.getRPerp();
				_tess->pushStrokeTop(_cursor, v1 - perp * _halfWidth);
			} while (0);

			do {
				auto norm = v2 - v1;
				norm.normalize();
				auto perp = norm.getRPerp();
				_tess->pushStrokeTop(_cursor, v1 - perp * _halfWidth);
			} while (0);
		}
	}
}

} // namespace stappler::geom
