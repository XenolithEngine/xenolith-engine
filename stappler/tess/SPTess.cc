/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "SPTess.h"
#include "SPLog.h"
#include "SPTessTypes.h"
#include "SPTessLine.h"
#include "SPTessSimd.hpp"

namespace STAPPLER_VERSIONIZED stappler::geom {

static constexpr VerboseFlag TessVerbose = VerboseFlag::None;
VerboseFlag TessVerboseInfo = TessVerbose;

struct Tesselator::Data : ObjectAllocator {
	// potential root face edges (connected to right non-convex angle)
	Vec2 _bmax, _bmin, _event;

	TessResult *_result = nullptr;
	EdgeDict *_edgeDict = nullptr;
	VertexPriorityQueue *_vertexQueue = nullptr;

	/* TWO tolerances, because the sweep asks "are these the same" about two different KINDS of
	number and one epsilon cannot serve both.

	`_mathTolerance` is dimensionless. `Edge::direction` lives in [-2, 2] and `EdgeAngle` in
	[0, 8) - both are ratios, both are O(1) whatever the scene is, and an absolute epsilon is
	exactly right for them.

	`_vertexTolerance` is the same question asked of COORDINATES, and coordinates have a scale.
	`Epsilon<float>` is one ulp AT 1.0; the distance between two adjacent representable floats
	grows with the exponent, and past |x| = 4 it already exceeds four of them. So a fixed epsilon
	stops meaning "the same point" and starts meaning "bit-identical" - and worse, a point the
	sweep COMPUTED (an intersection, a relocated vertex) lands a few ulp from the vertex it is
	supposed to coincide with, is declared distinct, and the code splits an edge that has no
	length. That is not a hypothetical: at coordinates of 1e5 a graph editor's wires failed with
	an intersection four ulp from its own event vertex.

	So this one is scaled to the data - see `updateVertexTolerance`. */
	float _mathTolerance = sprt::Epsilon<float> * 4.0f;
	float _vertexTolerance = sprt::Epsilon<float> * 4.0f;

	/* WHERE the input sits, taken from its first vertex and subtracted from every coordinate that
	enters, added back to every coordinate that leaves.

	A scaled tolerance (above) makes the sweep ASK the right question, but it cannot give back
	precision the arithmetic has already spent. An intersection of two edges at coordinates of 1e5
	is computed from differences of numbers whose own ulp is 0.008, so the answer can be wrong by
	that much however it is then compared - and the sweep's ORDER (`VertLeq`, which is exact by
	necessity) disagrees with the geometry. No epsilon repairs that; only better-conditioned
	numbers do.

	So the shape is tesselated around its own origin. A wire in an atlas a hundred thousand units
	wide is three hundred units long, and only its low bits ever said what it looked like; here
	its coordinates are its own size, and every bit of the mantissa is spent on the shape rather
	than on where the shape happens to be.

	The FIRST vertex rather than the centre of the box: the box is not known until the last vertex
	has arrived, and vertexes are consumed as they come. The centre would be one bit better and
	would cost a second pass over the whole mesh.

	TWO fields, because a caller may have done the subtraction itself. `LineDrawer` does - it has
	to flatten a bezier before any vertex exists, and the flattening wants the precision as much as
	the sweep does - and then says so with `setOutputOrigin`. In that case nothing is subtracted on
	the way in (it is already off) but the frame still has to go back on the way out. */
	Vec2 _inputOrigin; // subtracted from every coordinate pushed in
	Vec2 _outputOrigin; // added to every coordinate written out
	bool _hasNormalizeOrigin = false;

	Winding _winding = Winding::NonZero;
	float _boundaryOffset = 0.0f;
	float _boundaryInset = 0.0f;
	float _contentScale = 1.0f;
	uint32_t _nvertexes = 0;
	uint8_t _markValue = 1;

	RelocateRule _relocateRule = RelocateRule::Auto;

	bool _dryRun = false;
	bool _valid = true;

	// The fast path (SPTess.h). `_bypassEnabled` is a kill switch for tests and for bisecting; the
	// counter is what a test asserts on so it cannot pass vacuously.
	bool _bypassEnabled = true;
	uint32_t _bypassCount = 0;

	// Contours held back, in arrival order, and the latch that ends the holding.
	bool _bypassLatched = false;
	bool _replaying = false;
	sprt::__pool_vector<StrokeCandidate> _candidates;

	/* Rings of the candidates that were accepted, and how each one is cut up.

	A stroke's ribbon is a strip of trapezoids, two triangles per segment; a filled contour is
	convex and is a fan from its first vertex. The antialias skirt is the same code for both - it
	only ever looks at consecutive ring vertices. */
	struct AcceptedRing {
		SpanView<Vec2> ring;
		bool fan = false;
	};

	sprt::__pool_vector<AcceptedRing> _ribbons;

	// Filled contours offered before the sweep, in arrival order.
	sprt::__pool_vector<SpanView<Vec2>> _fills;
	// One corner of the fringe, kept between the two passes that need it.
	struct DisplacedCorner {
		Vec2 point;
		Vec2 norm;
		float value = 0.0f;
	};

	mem_std::Vector<DisplacedCorner> _displaceScratch;

	float _bypassFringe = 0.0f;
	uint32_t _bypassBase = 0;
	uint32_t _bypassVertexes = 0;
	uint32_t _bypassFaces = 0;

	// Stage B of the predicate, plus the ribbons of whatever survives it. Anything rejected is
	// replayed through the ordinary streaming path, in order, before the sweep starts.
	void resolveCandidates();
	void resolveFills(Tesselator *);

	Vertex *_eventVertex = nullptr;

	sprt::__pool_vector<Vertex *> _protectedVertexes;
	sprt::__pool_vector<HalfEdge *> _protectedEdges;

	Data(memory::pool_t *p);

	bool computeInterior();

	// Compute boundary face contour, also - split vertexes in subboundaries for antialiasing
	uint32_t computeBoundary();

	// Sets `_vertexTolerance` from the bounding box the input actually occupies. Called at the
	// start of every sweep, because the relocation rule can run one and then move the vertexes.
	void updateVertexTolerance();

	bool tessellateInterior();
	bool tessellateMonoRegion(HalfEdge *, uint8_t);
	bool sweepVertex(VertexPriorityQueue &pq, EdgeDict &dict, Vertex *v);
	HalfEdge *processIntersect(Vertex *, const EdgeDictNode *, HalfEdge *, Vec2 &,
			IntersectionEvent ev);
	HalfEdge *processIntersect(Vertex *, const EdgeDictNode *, Vec2 &, IntersectionEvent ev);

	Edge *makeEdgeLoop(const Vec2 &origin);

	Vertex *makeVertex(HalfEdge *eOrig);

	HalfEdge *pushVertex(HalfEdge *e, const Vec2 &, bool clockwise = false, bool returnNew = false);
	HalfEdge *connectEdges(HalfEdge *eOrg, HalfEdge *eDst);

	Vertex *splitEdge(HalfEdge *, const Vec2 &);
	Vertex *splitEdge(HalfEdge *, HalfEdge *eOrg2, const Vec2 &);

	HalfEdge *getFirstEdge(Vertex *org) const;
	bool mergeVertexes(Vertex *org, Vertex *merge);
	HalfEdge *removeEdge(HalfEdge *);

	HalfEdge *removeDegenerateEdges(HalfEdge *, uint32_t *nedges, bool safeRemove);
	bool removeDegenerateEdges(FaceEdge *, size_t &removed);

	bool processEdgeOverlap(Vertex *v, HalfEdge *e1, HalfEdge *e2);

	bool isDegenerateTriangle(HalfEdge *);

	uint32_t followBoundary(FaceEdge *, HalfEdge *, uint8_t);
	void splitVertex(HalfEdge *first, HalfEdge *last);
	void displaceBoundary(FaceEdge *);
};

Tesselator::~Tesselator() {
	if (_data) {
		auto pool = _data->_pool;
		_data->~Data();
		_data = nullptr;
		memory::pool::destroy(pool);
	}
}

bool Tesselator::init(memory::pool_t *pool) {
	auto p = memory::pool::create(pool);
	memory::context ctx(p);

	_data = new (p) Data(p);
	return true;
}

void Tesselator::setAntialiasValue(float value) {
	_data->_boundaryInset = _data->_boundaryOffset = value;
}

void Tesselator::setBoundariesTransform(float inset, float offset) {
	_data->_boundaryInset = inset;
	_data->_boundaryOffset = offset;
}

float Tesselator::getBoundaryInset() const { return _data->_boundaryInset; }

float Tesselator::getBoundaryOffset() const { return _data->_boundaryOffset; }

void Tesselator::setContentScale(float value) { _data->_contentScale = value; }

float Tesselator::getContentScale() const { return _data->_contentScale; }

void Tesselator::setRelocateRule(RelocateRule rule) { _data->_relocateRule = rule; }

Tesselator::RelocateRule Tesselator::getRelocateRule() const { return _data->_relocateRule; }

void Tesselator::setStrokeBypassEnabled(bool value) { _data->_bypassEnabled = value; }

bool Tesselator::isStrokeBypassEnabled() const { return _data->_bypassEnabled; }

uint32_t Tesselator::getStrokeBypassCount() const { return _data->_bypassCount; }

bool Tesselator::pushFillCandidate(SpanView<Vec2> pts) {
	if (!_data->_bypassEnabled || pts.size() < 3) {
		return false;
	}

	auto copy =
			reinterpret_cast<Vec2 *>(memory::pool::palloc(_data->_pool, sizeof(Vec2) * pts.size()));
	for (size_t i = 0; i < pts.size(); ++i) {
		copy[i] = pts[i];

		// The bounding box is what sets the vertex tolerance, and it has to see every contour -
		// including the ones that never reach the sweep - or the tolerance would depend on which
		// contours happened to be accepted.
		_data->_bmin =
				Vec2(sprt::min(_data->_bmin.x, pts[i].x), sprt::min(_data->_bmin.y, pts[i].y));
		_data->_bmax =
				Vec2(sprt::max(_data->_bmax.x, pts[i].x), sprt::max(_data->_bmax.y, pts[i].y));
	}

	_data->_fills.emplace_back(SpanView<Vec2>(copy, pts.size()));
	return true;
}

bool Tesselator::pushStrokeCandidate(const StrokeCandidate &cand) {
	if (!_data->_bypassEnabled || _data->_bypassLatched || !cand.replay) {
		return false;
	}

	auto copy = cand;

	// The caller's buffer is reused for the next contour, so the points have to be taken now.
	auto pts = reinterpret_cast<Vec2 *>(
			memory::pool::palloc(_data->_pool, sizeof(Vec2) * cand.points.size()));
	sprt::memcpy(pts, cand.points.data(), sizeof(Vec2) * cand.points.size());
	copy.points = SpanView<Vec2>(pts, cand.points.size());

	_data->_candidates.emplace_back(copy);
	return true;
}

void Tesselator::flushStrokeCandidates() {
	_data->_bypassLatched = true;

	/* RE-ENTRANT, and it has to be handled rather than assumed away.

	A replay goes through the ordinary streaming path, which calls `beginContour`, which comes
	straight back here. Latching alone does not stop it - the pending list is still non-empty - so
	the second entry would walk the same list again. Two hundred wires crashed the process with a
	stack overflow before this guard existed. */
	if (_data->_replaying || _data->_candidates.empty()) {
		return;
	}

	_data->_replaying = true;
	for (size_t i = 0; i < _data->_candidates.size(); ++i) {
		auto cand = _data->_candidates[i];
		cand.replay(this, cand);
	}
	_data->_candidates.clear();
	_data->_replaying = false;
}

void Tesselator::setWindingRule(Winding winding) { _data->_winding = winding; }

Winding Tesselator::getWindingRule() const { return _data->_winding; }

void Tesselator::setOutputOrigin(const Vec2 &origin) {
	// The caller's coordinates are already relative to `origin`, so there is nothing to take off
	// on the way in - only to put back on the way out.
	_data->_inputOrigin = Vec2::ZERO;
	_data->_outputOrigin = origin;
	_data->_hasNormalizeOrigin = true;
}

void Tesselator::preallocate(uint32_t n) {
	_data->preallocateVertexes(n);
	_data->preallocateEdges(n);
}

Tesselator::Cursor Tesselator::beginContour(bool clockwise) {
	// Anything that reaches the mesh directly ends the holding - see pushStrokeCandidate.
	flushStrokeCandidates();
	return Cursor{nullptr, nullptr, clockwise};
}

bool Tesselator::pushVertex(Cursor &cursor, const Vec2 &vertex) {
	// reject NaN (isValid) and also non-finite (Inf) coordinates: Inf would turn
	// into NaN inside the edge-angle math and corrupt the ordered-set comparators
	if (!vertex.isValid() || !sprt::isfinite(vertex.x) || !sprt::isfinite(vertex.y)) {
		return false;
	}

	if (!cursor.closed) {
		if (cursor.count == 0) {
			cursor.origin = vertex;
		}

		if constexpr (TessVerbose != VerboseFlag::None) {
			sprt::cout << "Push: " << vertex << "\n";
		}

		cursor.edge = _data->pushVertex(cursor.edge, vertex, cursor.isClockwise);
		++cursor.count;
		return true;
	}

	return false;
}

bool Tesselator::pushStrokeVertex(Cursor &cursor, const Vec2 &vertex, const Vec2 &offset) {
	if (!vertex.isValid() || !offset.isValid()) {
		return false;
	}

	if (!cursor.closed) {
		if (cursor.count == 0) {
			cursor.origin = vertex;
		}

		if constexpr (TessVerbose != VerboseFlag::None) {
			sprt::cout << "Push (stroke): " << vertex << ", " << offset << "\n";
		}

		if (!cursor.edge) {
			cursor.root = cursor.edge =
					_data->pushVertex(cursor.edge, vertex + offset, cursor.isClockwise);
			cursor.edge = _data->pushVertex(cursor.edge, vertex - offset, cursor.isClockwise);
		} else {
			_data->pushVertex(cursor.edge->getLeftLoopPrev(), vertex - offset, cursor.isClockwise);
			cursor.edge = _data->pushVertex(cursor.edge->getLeftLoopPrev(), vertex + offset,
					cursor.isClockwise, true);
		}

		++cursor.count;
		return true;
	}
	return false;
}

bool Tesselator::pushStrokeTop(Cursor &cursor, const Vec2 &vertex) {
	if (!vertex.isValid()) {
		return false;
	}

	if (!cursor.closed) {
		if (cursor.count == 0) {
			cursor.origin = vertex;
		}

		if constexpr (TessVerbose != VerboseFlag::None) {
			sprt::cout << "Push (stroke-top): " << vertex << "\n";
		}

		if (!cursor.edge) {
			cursor.root = cursor.edge = _data->pushVertex(cursor.edge, vertex, cursor.isClockwise);
		} else {
			cursor.edge = _data->pushVertex(cursor.edge->getLeftLoopPrev(), vertex,
					cursor.isClockwise, true);
		}

		++cursor.count;
		return true;
	}
	return false;
}

bool Tesselator::pushStrokeBottom(Cursor &cursor, const Vec2 &vertex) {
	if (!vertex.isValid()) {
		return false;
	}

	if (!cursor.closed) {
		if (cursor.count == 0) {
			cursor.origin = vertex;
		}

		if constexpr (TessVerbose != VerboseFlag::None) {
			sprt::cout << "Push (stroke-bottom): " << vertex << "\n";
		}

		if (!cursor.edge) {
			cursor.root = cursor.edge = _data->pushVertex(cursor.edge, vertex, cursor.isClockwise);
		} else {
			_data->pushVertex(cursor.edge->getLeftLoopPrev(), vertex, cursor.isClockwise);
		}

		++cursor.count;
		return true;
	}
	return false;
}

bool Tesselator::closeContour(Cursor &cursor) {
	if (cursor.closed) {
		return false;
	}

	cursor.closed = true;

	cursor.edge = _data->removeDegenerateEdges(cursor.edge, &cursor.count, true);

	if (cursor.edge) {
		if constexpr (TessVerbose != VerboseFlag::None) {
			sprt::cout << "Contour:\n";
			cursor.edge->foreachOnFace([&](HalfEdge &e) { sprt::cout << "\t" << e << "\n"; });
		}
		_data->trimVertexes();
		return true;
	} else {
		if constexpr (TessVerbose != VerboseFlag::None) {
			sprt::cout << "Fail to add empty contour\n";
		}
	}
	_data->trimVertexes();
	return false;
}

bool Tesselator::closeStrokeContour(Cursor &cursor) {
	if (cursor.closed) {
		return false;
	}

	cursor.closed = true;

	if (cursor.root) {
		_data->_vertexes[cursor.root->vertex]->relocate(cursor.edge->origin);
		_data->_vertexes[cursor.root->sym()->vertex]->relocate(
				cursor.edge->getLeftLoopPrev()->origin);
	}

	cursor.edge = _data->removeDegenerateEdges(cursor.edge, &cursor.count, true);

	if (cursor.edge) {
		if constexpr (TessVerbose != VerboseFlag::None) {
			sprt::cout << "Contour:\n";
			cursor.edge->foreachOnFace([&](HalfEdge &e) { sprt::cout << "\t" << e << "\n"; });
		}
		return true;
	} else {
		if constexpr (TessVerbose != VerboseFlag::None) {
			sprt::cout << "Fail to add empty contour\n";
		}
	}
	_data->trimVertexes();
	return false;
}

/* Does this ribbon clear itself, and do the ribbons clear each other?

Stage A settled the joins; what is left is whether two parts of the path that are far apart along
the contour end up near each other in the plane. Two ribbons closer than the stroke width overlap,
and an overlap is exactly what the NonZero sweep is for.

Brute force over segment pairs. The production case is a flattened cubic - a couple of dozen
segments - and the budget below turns a pathological contour into a fallback rather than into a
quadratic scan. */
static bool Tesselator_ribbonIsClear(const StrokeCandidate &cand, uint32_t &budget) {
	const auto &pts = cand.points;
	const size_t n = pts.size();
	// Two ribbons closer than the stroke width overlap; the fringe is added because one part's
	// alpha ramp lying over another part's is visible even where the interiors are not.
	const float clearance = cand.halfWidth * 2.0f + cand.fringe * 2.0f;
	const float clearSq = clearance * clearance;

	const auto segDistSq = [](const Vec2 &a0, const Vec2 &a1, const Vec2 &b0, const Vec2 &b1) {
		const auto pointSegSq = [](const Vec2 &p, const Vec2 &s0, const Vec2 &s1) {
			const auto d = s1 - s0;
			const float len2 = d.lengthSquared();
			float t = 0.0f;
			if (len2 > 0.0f) {
				t = sprt::clamp(Vec2::dot(p - s0, d) / len2, 0.0f, 1.0f);
			}
			return (p - (s0 + d * t)).lengthSquared();
		};
		return sprt::min(sprt::min(pointSegSq(a0, b0, b1), pointSegSq(a1, b0, b1)),
				sprt::min(pointSegSq(b0, a0, a1), pointSegSq(b1, a0, a1)));
	};

	/* A SWEEP OVER THE SEGMENTS' X-EXTENTS, not a scan over their pairs.

	The pairwise scan is the obvious way and it does not pay: measured on four hundred wires, it
	cost about nine microseconds a contour against a whole tesselation of twenty-one. A predicate
	that costs half of what it avoids is not an optimisation.

	Segments sorted by their left edge, with an active set holding only those whose right edge is
	still within the clearance of the current left edge. A wire is monotone or nearly so, which
	keeps the active set at two or three and the whole thing linear; a contour that folds back on
	itself grows the set, and the budget below turns that into a fallback rather than into the
	quadratic scan by another name. */
	const size_t segs = n - 1;

	mem_std::Vector<uint32_t> order;
	order.reserve(segs);
	for (uint32_t i = 0; i < segs; ++i) { order.emplace_back(i); }

	const auto loX = [&](uint32_t i) { return sprt::min(pts[i].x, pts[i + 1].x); };
	const auto hiX = [&](uint32_t i) { return sprt::max(pts[i].x, pts[i + 1].x); };

	sprt::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) { return loX(a) < loX(b); });

	mem_std::Vector<uint32_t> active;
	for (auto i : order) {
		const float left = loX(i) - clearance;

		// Retire everything that ends before this segment can reach.
		size_t w = 0;
		for (size_t k = 0; k < active.size(); ++k) {
			if (hiX(active[k]) >= left) {
				active[w++] = active[k];
			}
		}
		active.resize(w);

		for (auto j : active) {
			const uint32_t d = i > j ? i - j : j - i;
			if (d <= 1) {
				continue; // adjacent segments meet by construction; stage A judged their joint
			}

			if (budget == 0) {
				return false; // a contour this tangled belongs in the sweep anyway
			}
			--budget;

			if (segDistSq(pts[i], pts[i + 1], pts[j], pts[j + 1]) <= clearSq) {
				return false;
			}
		}

		active.emplace_back(i);
	}
	return true;
}

/* Convex, and simple by consequence: every turn the same way, and the total turning exactly one
revolution. The second half is what separates a convex polygon from a star that turns the same way
several times over. */
static bool Tesselator_isConvexRing(SpanView<Vec2> pts) {
	const size_t n = pts.size();
	if (n < 3) {
		return false;
	}

	int sign = 0;
	for (size_t i = 0; i < n; ++i) {
		const auto &a = pts[i];
		const auto &b = pts[(i + 1) % n];
		const auto &c = pts[(i + 2) % n];
		const float cross = Vec2::cross(b - a, c - b);

		if (cross > 0.0f) {
			if (sign < 0) {
				return false;
			}
			sign = 1;
		} else if (cross < 0.0f) {
			if (sign > 0) {
				return false;
			}
			sign = -1;
		} else {
			return false; // a collinear or repeated triple: the sweep merges these, the fan cannot
		}
	}
	return sign != 0;
}

void Tesselator::Data::resolveFills(Tesselator *self) {
	if (_fills.empty()) {
		return;
	}

	/* Every accepted contour must clear every other one, so one rejection does not spoil the rest -
	unlike the stroke's all-or-nothing latch, where the ordering of a partial demotion would be the
	problem. Here nothing is reordered: a rejected contour is streamed by the caller, and an
	accepted one never enters the mesh at all.

	TOUCHING COUNTS AS OVERLAP. Two rectangles that share a corner are one vertex to the sweep -
	`checkGrid` pins that at seventeen by seventeen for a sixteen-by-sixteen grid - and emitting
	them apart would silently double it. The clearance is therefore strict, and carries the fringe
	on top when there is one. */
	/* THE WINDING RULE DECIDES WHETHER A LONE CONTOUR IS FILLED AT ALL, and only two of the five
	answer "yes, whichever way it is wound". `Positive` and `Negative` fill one orientation and not
	the other; `AbsGeqTwo` fills neither. A fan emitted without asking would add area the path does
	not have - measured: it changed six hundred and sixty-two icons. */
	const bool relocateOk = _relocateRule == Tesselator::RelocateRule::Auto
			|| _relocateRule == Tesselator::RelocateRule::Never;

	if ((_winding != Winding::NonZero && _winding != Winding::EvenOdd) || !relocateOk) {
		for (auto &c : _fills) {
			auto cursor = self->beginContour();
			for (auto &p : c) { self->pushVertex(cursor, p); }
			self->closeContour(cursor);
		}
		_fills.clear();
		return;
	}

	const float clearance = _boundaryOffset + _boundaryInset * 0.5f;

	struct Box {
		Vec2 min, max;
	};

	mem_std::Vector<Box> boxes;
	boxes.reserve(_fills.size());
	for (auto &c : _fills) {
		Box b{c[0], c[0]};
		for (auto &p : c) {
			b.min.x = sprt::min(b.min.x, p.x);
			b.min.y = sprt::min(b.min.y, p.y);
			b.max.x = sprt::max(b.max.x, p.x);
			b.max.y = sprt::max(b.max.y, p.y);
		}
		boxes.emplace_back(b);
	}

	mem_std::Vector<bool> keep(_fills.size(), true);
	for (size_t i = 0; i < _fills.size(); ++i) {
		if (!Tesselator_isConvexRing(_fills[i])) {
			keep[i] = false;
		}
	}

	/* Pairwise separation, over a UNIFORM GRID rather than over the pairs.

	A sweep along one axis was the first attempt and it does nothing here: a column of rectangles
	shares its x-extent exactly, so every box in the column stays active at once and the scan is
	quadratic again. The case this exists for is forty thousand boxes on a lattice, which is
	precisely that shape.

	Cell size is the largest box plus the clearance, so a box touches at most four cells and two
	boxes that could be within the clearance always share one. */
	float cell = clearance;
	for (auto &b : boxes) {
		cell = sprt::max(cell, sprt::max(b.max.x - b.min.x, b.max.y - b.min.y));
	}
	cell = sprt::max(cell + clearance, sprt::Epsilon<float> * 1'024.0f);

	struct Entry {
		int64_t key;
		uint32_t idx;
	};

	mem_std::Vector<Entry> cells;
	cells.reserve(_fills.size() * 4);
	for (uint32_t i = 0; i < boxes.size(); ++i) {
		const int64_t x0 = int64_t(sprt::floor(boxes[i].min.x / cell));
		const int64_t x1 = int64_t(sprt::floor(boxes[i].max.x / cell));
		const int64_t y0 = int64_t(sprt::floor(boxes[i].min.y / cell));
		const int64_t y1 = int64_t(sprt::floor(boxes[i].max.y / cell));
		for (int64_t cx = x0; cx <= x1; ++cx) {
			for (int64_t cy = y0; cy <= y1; ++cy) {
				cells.emplace_back(Entry{(cx << 32) ^ (cy & 0xFFFF'FFFF), i});
			}
		}
	}

	sprt::sort(cells.begin(), cells.end(),
			[](const Entry &a, const Entry &b) { return a.key < b.key; });

	const auto testPair = [&](uint32_t i, uint32_t j) {
		if (i == j) {
			return;
		}
		const auto &a = boxes[i];
		const auto &b = boxes[j];
		const bool apart = a.max.x + clearance < b.min.x || b.max.x + clearance < a.min.x
				|| a.max.y + clearance < b.min.y || b.max.y + clearance < a.min.y;
		if (!apart) {
			keep[i] = false;
			keep[j] = false;
		}
	};

	// Every box against those in its own cell and in the eight around it.
	for (uint32_t i = 0; i < boxes.size(); ++i) {
		const int64_t cx = int64_t(sprt::floor(boxes[i].min.x / cell));
		const int64_t cy = int64_t(sprt::floor(boxes[i].min.y / cell));
		for (int64_t dx = -1; dx <= 1 && keep[i]; ++dx) {
			for (int64_t dy = -1; dy <= 1 && keep[i]; ++dy) {
				const int64_t k = ((cx + dx) << 32) ^ ((cy + dy) & 0xFFFF'FFFF);
				auto lo = sprt::lower_bound(cells.begin(), cells.end(), k,
						[](const Entry &e, int64_t v) { return e.key < v; });
				for (auto it = lo; it != cells.end() && it->key == k; ++it) {
					testPair(i, it->idx);
				}
			}
		}
	}

	uint32_t accepted = 0;
	for (size_t i = 0; i < _fills.size(); ++i) {
		auto &c = _fills[i];

		if (!keep[i]) {
			// Streamed here, in arrival order, exactly as the caller would have streamed it.
			auto cursor = self->beginContour();
			for (auto &p : c) { self->pushVertex(cursor, p); }
			self->closeContour(cursor);
			continue;
		}

		auto pts = reinterpret_cast<Vec2 *>(memory::pool::palloc(_pool, sizeof(Vec2) * c.size()));
		for (size_t k = 0; k < c.size(); ++k) { pts[k] = c[k] - _inputOrigin; }
		_ribbons.emplace_back(AcceptedRing{SpanView<Vec2>(pts, c.size()), true});

		const uint32_t n = uint32_t(c.size());
		_bypassVertexes += n;
		_bypassFaces += n - 2;
		if (_bypassFringe > 0.0f) {
			_bypassVertexes += n;
			_bypassFaces += n * 2;
		}
		++accepted;
	}

	_fills.clear();
	_bypassCount += accepted;
}

void Tesselator::Data::resolveCandidates() {
	if (_candidates.empty()) {
		return;
	}

	/* Stage B. The winding and relocation rules are read here and not earlier because every caller
	configures the tesselator AFTER running the LineDrawer.

	The fringe is a v1 exclusion: it is produced by walking the boundary of the swept mesh, and
	reproducing it for a ribbon is a separate piece of work. Until then an antialiased stroke falls
	back, which is correct and merely not yet fast. */
	bool ok = _winding == Winding::NonZero
			&& (_relocateRule == Tesselator::RelocateRule::Auto
					|| _relocateRule == Tesselator::RelocateRule::Never);

	for (auto &it : _candidates) { it.fringe = _bypassFringe; }

	uint32_t budget = 0;
	if (ok) {
		for (auto &it : _candidates) { budget += uint32_t(it.points.size()) * 16; }
		for (auto &it : _candidates) {
			if (!Tesselator_ribbonIsClear(it, budget)) {
				ok = false;
				break;
			}
		}
	}

	// More than one held contour would have to be checked against the others as well. One is the
	// production case (a path per wire), so v1 stops there rather than guessing at the rest.
	if (ok && _candidates.size() > 1) {
		ok = false;
	}

	if (!ok) {
		return; // the caller flushes: every candidate is replayed, in order
	}

	mem_std::Vector<Vec2> ring;
	for (auto &it : _candidates) {
		StrokeWriter::buildRibbon(it, ring);

		auto pts =
				reinterpret_cast<Vec2 *>(memory::pool::palloc(_pool, sizeof(Vec2) * ring.size()));
		for (size_t i = 0; i < ring.size(); ++i) {
			// Same frame as everything else in the mesh; see Data::pushVertex.
			pts[i] = ring[i] - _inputOrigin;
		}
		_ribbons.emplace_back(AcceptedRing{SpanView<Vec2>(pts, ring.size()), false});

		const uint32_t n = uint32_t(it.points.size());
		_bypassVertexes += n * 2;
		_bypassFaces += (n - 1) * 2;

		// The fringe: a displaced twin of every ring vertex, and a quad on every ring edge.
		if (_bypassFringe > 0.0f) {
			_bypassVertexes += n * 2;
			_bypassFaces += n * 4;
		}
	}

	_candidates.clear();
	_bypassCount = uint32_t(_ribbons.size());
}

bool Tesselator::prepare(TessResult &res) {
	_data->_result = &res;

	/* The fast path's block is reserved BEFORE the sweep's, and the sweep's base is pushed past it.

	It cannot go after the antialias block: that one is reserved as `E + SumS + 1` but `write`
	calls `exportExtraVertex` `S + 1` times PER boundary, so with more than one boundary the
	running counter overruns its reservation. That survives today only because the canvas callback
	grows its array on demand, and it is not something a second producer may build on.

	With no accepted candidates `_bypassVertexes` is zero and the line below is the assignment it
	has always been. */
	/* The fringe's reach, computed once because BOTH halves need it - and it is the `Never`
	arithmetic on purpose.

	Under `Auto` a boundary vertex is relocated only where the sweep split it, which is where the
	shape crossed itself, and a contour that reaches the fast path provably does not. So both
	allowed rules take the same branch of `displaceBoundary`: no inset, and the offset carries half
	of it.

	Computing this inside the stroke resolver was the first attempt, and it left every antialiased
	FILL without a skirt: that resolver returns early when there are no stroke candidates, which is
	every plain path. Six hundred and sixty-two icons noticed. */
	_data->_bypassFringe = 0.0f;
	if ((_data->_relocateRule == RelocateRule::Auto || _data->_relocateRule == RelocateRule::Never)
			&& (_data->_boundaryOffset > 0.0f || _data->_boundaryInset > 0.0f)) {
		_data->_bypassFringe = _data->_boundaryOffset + _data->_boundaryInset * 0.5f;
	}

	_data->resolveCandidates();
	flushStrokeCandidates(); // replays anything stage B rejected, in arrival order
	_data->resolveFills(this);

	_data->_bypassBase = res.nvertexes;
	res.nvertexes += _data->_bypassVertexes;
	res.nfaces += _data->_bypassFaces;

	_data->_vertexOffset = res.nvertexes;

	if ((_data->_relocateRule == RelocateRule::Monotonize)
			&& (_data->_boundaryOffset > 0.0f || _data->_boundaryInset > 0.0f)) {
		_data->_dryRun = true;
	}

	/* Nothing left for the sweep is a success, not a failure.

	When every contour of a path took the fast path the mesh is empty, and `computeInterior` on an
	empty mesh reports failure - which the callers read as "this path produced nothing" and skip
	`write` for. Forty thousand disjoint rectangles came out as zero triangles before this was
	here. */
	if (_data->_nvertexes == 0 && _data->_bypassVertexes > 0) {
		return true;
	}

	if (!_data->computeInterior()) {
		return false;
	}

	if (_data->_boundaryOffset > 0.0f || _data->_boundaryInset > 0.0f) {
		auto nBoundarySegments = _data->computeBoundary();

		if constexpr (TessVerbose != VerboseFlag::None) {
			for (auto &it : _data->_boundaries) {
				if (!it->_degenerate) {
					sprt::cout << "Boundary:\n";
					it->foreach ([&](const FaceEdge &edge) { sprt::cout << "\t" << edge << "\n"; });
				}
			}
		}

		if (_data->_relocateRule == RelocateRule::Monotonize) {
			for (auto &it : _data->_boundaries) {
				if (it->_degenerate) {
					continue;
				}
				auto e = it;
				do {
					_data->displaceBoundary(e);
					e = e->_next;
				} while (e != it);
			}

			_data->_dryRun = false;

			for (auto &it : _data->_vertexes) {
				if (!it) {
					continue;
				}

				if constexpr (TessVerbose != VerboseFlag::None) {
					sprt::cout << "Vertex: " << *it << "\n";
				}

				auto e = it->_edge;
				do {
					auto edge = e->getEdge();
					sprt_passert(!edge->invalidated,
							"Tess: failed: edge was invalidated but still in use");
					edge->direction = nan();
					edge->node = nullptr;
					e->origin = it->_origin;
					e->_realWinding = 0;
					e = e->_originNext;
				} while (e != it->_edge);
			}

			_data->computeInterior();
		}

		if (!_data->tessellateInterior()) {
			_data->_valid = false;
			_data->_result = nullptr;
			return false;
		}


		// allocate additional space for boundaries (vertexes and triangles)
		res.nvertexes += _data->_exportVertexes.size() + nBoundarySegments + 1;
		res.nfaces += _data->_faceEdges.size() + nBoundarySegments * 2;

		if (_data->_relocateRule == RelocateRule::DistanceField) {
			for (auto &it : _data->_boundaries) {
				res.nvertexes += it->_nextra;
				res.nfaces += it->_nextra;
			}
		}
		return true;
	} else {
		if (!_data->tessellateInterior()) {
			_data->_valid = false;
			_data->_result = nullptr;
			return false;
		}

		res.nvertexes += _data->_exportVertexes.size();
		res.nfaces += _data->_faceEdges.size();
		return true;
	}
	return false;
}

bool Tesselator::write(TessResult &res) {
	/* The accepted ribbons go out FIRST, and above the validity guard on purpose: their block was
	counted into `res.nvertexes` by `prepare`, so a sweep that failed afterwards must not leave it
	unwritten and the indexes shifted.

	Ring order is `top0, bottom0..bottomN-1, topN-1..top1` (SPTessLine.h), so the two chains index
	back out of it as below. Each segment is one convex trapezoid and splits either way; this takes
	the diagonal `top_s -> bottom_{s+1}` for both halves, which keeps the two triangles' winding
	consistent with `exportQuad`'s. */
	if (!_data->_ribbons.empty()) {
		uint32_t base = _data->_bypassBase;
		uint32_t triangle[3] = {0};

		const float fringe = _data->_bypassFringe;

		for (auto &accepted : _data->_ribbons) {
			auto &ring = accepted.ring;
			const uint32_t total = uint32_t(ring.size());
			const uint32_t n = total / 2;

			/* One corner of the fringe, transcribed from `displaceBoundary` on the branch a
			split-free contour takes: no relocation of the interior vertex, the whole reach going
			outward, and the same spike clamp - a corner sharper than a ratio of three keeps its
			place and fades in instead, which is what `_value` is. */
			const auto displace = [&](uint32_t k, bool flip, Vec2 &out, float &value, Vec2 &norm) {
				// `flip` swaps the two NEIGHBOURS, which is what reversing the ring means. It does
				// not move the vertex: displacing `total-1-k` and storing it at `k` was the first
				// attempt and it scrambles the skirt, which showed as three times the area.
				const Vec2 &a = ring[(k + total - 1) % total];
				const Vec2 &cur = ring[k];
				const Vec2 &c = ring[(k + 1) % total];
				const Vec2 &prev = flip ? c : a;
				const Vec2 &next = flip ? a : c;

				Vec4 r;
				getVertexNormal(&prev.x, &cur.x, &next.x, &r.x);

				value = 0.0f;
				if (sprt::isnan(r.y) || r.y > 3.0f) {
					value = 1.0f - 3.0f / r.y;
					r.y = 3.0f;
				}

				norm = -Vec2(r.z, r.w);

				const float offsetMod = copysign(r.y * fringe, r.x);
				out = Vec2(cur.x + r.z * offsetMod, cur.y + r.w * offsetMod);
			};

			/* WHICH WAY IS OUT is checked, not deduced.

			`displaceBoundary`'s sign comes from the cross product of the two edge directions, so
			it depends on how the ring is wound - and a fringe pointing inward is a dark halo
			inside the stroke: invisible at a glance and miserable to attribute. Take a vertex that
			is certainly on the hull (the lexicographic maximum), displace it, and see whether it
			moved away from the middle. */
			bool reversed = false;
			if (fringe > 0.0f) {
				Vec2 centroid;
				uint32_t hull = 0;
				for (uint32_t i = 0; i < total; ++i) {
					centroid += ring[i];
					if (ring[i].x > ring[hull].x
							|| (ring[i].x == ring[hull].x && ring[i].y > ring[hull].y)) {
						hull = i;
					}
				}
				centroid /= float(total);

				Vec2 out, norm;
				float value = 0.0f;
				displace(hull, false, out, value, norm);
				reversed = Vec2::dot(out - ring[hull], ring[hull] - centroid) < 0.0f;
			}

			/* Each corner's displacement is computed ONCE and kept: the interior vertex wants its
			normal and the fringe vertex wants the point, and both come out of the same
			`getVertexNormal`. Computing it twice was doubling the only arithmetic on this path. */
			auto &scratch = _data->_displaceScratch;
			if (fringe > 0.0f) {
				scratch.clear();
				scratch.resize(total);
				for (uint32_t i = 0; i < total; ++i) {
					displace(i, reversed, scratch[i].point, scratch[i].value, scratch[i].norm);
				}
			}

			for (uint32_t i = 0; i < total; ++i) {
				res.pushVertex(res.target, base + i, ring[i] + _data->_outputOrigin, 1.0f,
						fringe > 0.0f ? scratch[i].norm : Vec2::ZERO);
			}

			if (accepted.fan) {
				// Convex, so a fan from the first vertex covers it and every triangle is inside.
				for (uint32_t i = 1; i + 1 < total; ++i) {
					triangle[0] = base;
					triangle[1] = base + i;
					triangle[2] = base + i + 1;
					res.pushTriangle(res.target, triangle);
				}
			} else {
				const auto topIdx = [&](uint32_t i) { return i == 0 ? 0u : total - i; };
				const auto bottomIdx = [&](uint32_t i) { return 1u + i; };

				for (uint32_t seg = 0; seg + 1 < n; ++seg) {
					triangle[0] = base + topIdx(seg);
					triangle[1] = base + bottomIdx(seg);
					triangle[2] = base + topIdx(seg + 1);
					res.pushTriangle(res.target, triangle);

					triangle[0] = base + bottomIdx(seg);
					triangle[1] = base + bottomIdx(seg + 1);
					triangle[2] = base + topIdx(seg + 1);
					res.pushTriangle(res.target, triangle);
				}
			}

			base += total;

			if (fringe > 0.0f) {
				// The displaced twins, then a quad per ring edge between the twin and its original
				// - the same pair of triangles `exportQuad` emits, in the same order.
				for (uint32_t i = 0; i < total; ++i) {
					res.pushVertex(res.target, base + i, scratch[i].point + _data->_outputOrigin,
							scratch[i].value, scratch[i].norm);
				}

				for (uint32_t i = 0; i < total; ++i) {
					const uint32_t j = (i + 1) % total;
					const uint32_t tl = base + i, tr = base + j;
					const uint32_t bl = base - total + i, br = base - total + j;

					triangle[0] = tl;
					triangle[1] = bl;
					triangle[2] = tr;
					res.pushTriangle(res.target, triangle);

					triangle[0] = bl;
					triangle[1] = br;
					triangle[2] = tr;
					res.pushTriangle(res.target, triangle);
				}

				base += total;
			}
		}
	}

	if (!_data->_valid) {
		return false;
	}

	uint32_t triangle[3] = {0};

	auto exportQuad = [&, this](uint32_t tl, uint32_t tr, uint32_t bl, uint32_t br) {
		triangle[0] = _data->_vertexOffset + tl;
		triangle[1] = _data->_vertexOffset + bl;
		triangle[2] = _data->_vertexOffset + tr;

		res.pushTriangle(res.target, triangle);

		triangle[0] = _data->_vertexOffset + bl;
		triangle[1] = _data->_vertexOffset + br;
		triangle[2] = _data->_vertexOffset + tr;

		res.pushTriangle(res.target, triangle);
	};

	if (_data->_boundaryOffset > 0.0f || _data->_boundaryInset > 0.0f) {
		uint32_t tl, tr, bl, br, origin;

		uint32_t nexports = uint32_t(_data->_exportVertexes.size());

		auto exportExtraVertex = [&, this](FaceEdge *e) {
			auto originVertex = nexports;
			auto nextVertex = nexports;
			res.pushVertex(res.target, nexports + _data->_vertexOffset,
					e->_displaced + _data->_outputOrigin, e->_value,
					(e->_vertex->_origin - e->_displaced).getNormalized());
			++nexports;

			if (e->_nextra > 0) {
				auto incr = e->_angle / e->_nextra;
				float angle = -incr;
				for (uint16_t i = 0; i < e->_nextra; ++i) {
					Vec2 point = e->_displaced;
					point.rotate(e->_origin, angle);

					res.pushVertex(res.target, nexports + _data->_vertexOffset,
							point + _data->_outputOrigin, e->_value,
							(e->_vertex->_origin - point).getNormalized());
					nextVertex = nexports;

					triangle[0] = _data->_vertexOffset + e->_vertex->_exportIdx;
					triangle[1] = _data->_vertexOffset + nextVertex;
					triangle[2] = _data->_vertexOffset + originVertex;

					res.pushTriangle(res.target, triangle);

					originVertex = nexports;

					++nexports;
					angle -= incr;
				}
			}
		};

		for (auto &it : _data->_boundaries) {
			if (it->_degenerate) {
				continue;
			}

			auto e = it;

			bool shouldDisplace = true;
			if (_data->_relocateRule == RelocateRule::Monotonize) {
				// boundaries already relocated
				shouldDisplace = false;
			}

			if (shouldDisplace) {
				do {
					_data->displaceBoundary(e);
					e = e->_next;
				} while (e != it);
			}

			origin = nexports;
			e = e->_next;

			exportExtraVertex(e);

			do {
				// e and e->next should be ready
				tl = nexports - 1;
				tr = nexports;
				bl = e->_vertex->_exportIdx;
				br = e->_next->_vertex->_exportIdx;

				e = e->_next;

				exportExtraVertex(e);
				exportQuad(tl, tr, bl, br);
			} while (e != it);

			// export first edge
			tl = nexports - 1;
			tr = origin;
			bl = e->_vertex->_exportIdx;
			br = e->_next->_vertex->_exportIdx;
			exportQuad(tl, tr, bl, br);
		}
	}

	for (auto &it : _data->_exportVertexes) {
		if (it) {
			res.pushVertex(res.target, it->_exportIdx + _data->_vertexOffset,
					it->_origin + _data->_outputOrigin, 1.0f, it->_norm);
		}
	}

	auto mark = ++_data->_markValue;
	for (auto &it : _data->_faceEdges) {
		if (it && it->_mark != mark && isWindingInside(_data->_winding, it->_realWinding)) {
			uint32_t vertex = 0;
			bool valid = true;

			/* Bounded, and the bound is the whole point.
			
			A face here is a TRIANGLE or it is nothing - the emit below fires only at exactly
			three edges, so walking a fourth can change nothing except how long it takes. That
			turned out to matter: `foreachOnFace` follows `_leftNext` until it returns to where
			it started, and a face ring that does NOT close - what a mesh the sweep could not
			repair leaves behind - spins there forever. Three hundred crossing wires hung on
			exactly that, and a renderer that hangs is worse than one that drops a triangle.
			
			Four steps: one more than a triangle, which is enough to tell a triangle from
			something that is not one, and no more. The drop path below is the one this code
			already took for a stale vertex index. */
			auto faceEdge = it;
			do {
				auto &edge = *faceEdge;
				if (vertex < 3) {
					// bounds- and null-check the vertex in every build; on a stale
					// index drop this triangle (graceful degradation) instead of
					// reading out of bounds. DEBUG additionally aborts to surface the
					// tessellator bug during development.
					if (edge.vertex < _data->_vertexes.size()) {
						const auto v = _data->_vertexes[edge.vertex];
						if (v) {
							triangle[vertex] = v->_exportIdx + _data->_vertexOffset;
						} else {
#if DEBUG
							sprt::cout << "Invalid vertex: " << edge.vertex << "\n";
							::abort();
#endif
							valid = false;
						}
					} else {
#if DEBUG
						sprt::cout << "Invalid vertex index: " << edge.vertex << " of "
								   << _data->_vertexes.size() << "\n";
						::abort();
#endif
						valid = false;
					}
				}
				edge._mark = mark;
				++vertex;
				faceEdge = faceEdge->_leftNext;
			} while (faceEdge && faceEdge != it && vertex <= 3);

			// `faceEdge == it` is the ring having closed. Without it a broken ring that ran out
			// of steps would look exactly like a triangle whose third edge happened to be last.
			if (vertex == 3 && faceEdge == it && valid) {
				res.pushTriangle(res.target, triangle);
			}
		}
	}

	return true;
}

Tesselator::Data::Data(memory::pool_t *p) : ObjectAllocator(p) { }

void Tesselator::Data::updateVertexTolerance() {
	// The magnitude of the largest coordinate, not the size of the box: precision is decided by
	// the exponent of the number being compared, so a small shape far from the origin is the
	// dangerous case and a large shape around it is not.
	const float scale = sprt::max(sprt::max(sprt::abs(_bmin.x), sprt::abs(_bmax.x)),
			sprt::max(sprt::abs(_bmin.y), sprt::abs(_bmax.y)));

	// Never finer than the unscaled value: at coordinates below 1 the old number is already
	// coarser than an ulp, and loosening it there would merge things that are genuinely apart.
	_vertexTolerance = _mathTolerance * sprt::max(1.0f, scale);
}

bool Tesselator::Data::computeInterior() {
	bool result = true;

	updateVertexTolerance();

	_exportVertexes.clear();

	EdgeDict dict(_pool, 8);
	VertexPriorityQueue pq(_pool, _vertexes);

	_edgeDict = &dict;
	_vertexQueue = &pq;

	Vertex *v, *vNext;
	while ((v = pq.extractMin()) != nullptr) {
		for (;;) {
			vNext = pq.getMin();
			if (vNext == NULL || !VertEq(vNext, v, _vertexTolerance)) {
				break;
			}

			vNext = pq.extractMin();
			if (!mergeVertexes(v, vNext)) {
				log::source().error("geom::Tesselator", "Tesselation failed on mergeVertexes");
				result = false;
				break;
			}
		}

		dict.update(v, _vertexTolerance);

		if (!sweepVertex(pq, dict, v)) {
			log::source().error("geom::Tesselator", "Tesselation failed on sweepVertex");
			result = false;
			break;
		}
	}

	_edgeDict = nullptr;
	_vertexQueue = nullptr;

	return result;
}

uint32_t Tesselator::Data::computeBoundary() {
	_nvertexes = uint32_t(_vertexes.size()); // for new vertexes handling
	uint32_t nsegments = 0;
	auto mark = ++_markValue;

	for (auto &it : _edgesOfInterests) {
		if (!it) {
			continue;
		}
		auto e = it->getEdge();
		if (e->left._mark != mark) {
			if (!isWindingInside(_winding, e->left._realWinding)) {
				nsegments += followBoundary(nullptr, &e->left, mark);
			} else {
				e->left._mark = mark;
			}
		}
		if (e->right._mark != mark) {
			if (!isWindingInside(_winding, e->right._realWinding)) {
				nsegments += followBoundary(nullptr, &e->right, mark);
			} else {
				e->right._mark = mark;
			}
		}
	}

	for (auto &it : _boundaries) {
		size_t removed = 0;
		if (!removeDegenerateEdges(it, removed)) {
			it->_degenerate = true;
			nsegments -= removed;
		}
	}

	return nsegments;
}

bool Tesselator::Data::tessellateInterior() {
	auto mark = ++_markValue;

	for (auto &it : _edgesOfInterests) {
		if (!it) {
			continue;
		}
		auto e = it->getEdge();
		if (e->left._mark != mark) {
			if (isWindingInside(_winding, e->left._realWinding)) {
				if constexpr (TessVerbose != VerboseFlag::None) {
					uint32_t vertex = 0;
					sprt::cout << "Inside Face: \n";
					e->left.foreachOnFace([&](HalfEdge &edge) {
						sprt::cout << "\t" << vertex++ << "; " << edge << "\n";
					});
				}

				if (!tessellateMonoRegion(&e->left, mark)) {
					return false;
				}
			} else {
				e->left._mark = mark;
			}
		}
		if (e->right._mark != mark) {
			if (isWindingInside(_winding, e->right._realWinding)) {
				if constexpr (TessVerbose != VerboseFlag::None) {
					uint32_t vertex = 0;
					sprt::cout << "Inside Face: \n";
					e->right.foreachOnFace([&](HalfEdge &edge) {
						sprt::cout << "\t" << vertex++ << "; " << edge << "\n";
					});
				}

				if (!tessellateMonoRegion(&e->right, mark)) {
					return false;
				}
			} else {
				e->right._mark = mark;
			}
		}
	}
	return true;
}

bool Tesselator::Data::tessellateMonoRegion(HalfEdge *edge, uint8_t v) {
	if (edge->_leftNext->_leftNext == edge) {
		return true;
	}

	edge = removeDegenerateEdges(edge, nullptr, false);
	if (!edge) {
		return true;
	}

	HalfEdge *up = edge, *lo;

	/* All edges are oriented CCW around the boundary of the region.
	 * First, find the half-edge whose origin vertex is rightmost.
	 * Since the sweep goes from left to right, face->anEdge should
	 * be close to the edge we want.
	 */
	for (; VertLeq(up->getDstVec(), up->getOrgVec()); up = up->getLeftLoopPrev());
	for (; VertLeq(up->getOrgVec(), up->getDstVec()); up = up->getLeftLoopNext());
	lo = up->getLeftLoopPrev();

	if constexpr (TessVerbose == VerboseFlag::Full) {
		sprt::cout << "Start: Up: " << *up << "\n";
		sprt::cout << "Start: Lo: " << *lo << "\n";
	}

	up->_mark = v;
	lo->_mark = v;

	const Vec2 *v0, *v1, *v2;

	/* WHAT A REFUSED CUT MEANS, and what it cost to get this wrong twice.

	`connectEdges` refuses a cut whose two ends are the same vertex - a triangle with no area. The
	original code answered that by failing, which fails the whole path and loses the shape; an
	attempt to carry on instead broke out of the inner loop only, and six hundred crossing wires
	spun in the outer one forever.

	The middle answer - end the region, keep the triangles already produced, let the path live -
	was tried and is WRONG, in two ways that only appeared at scale, and both are recorded here so
	the idea is not had a third time:

	  * The partial triangulation leaves a boundary segment of zero length, and the bisector of
	    such a corner is NaN. It reached a vertex's own position. See displaceBoundary.
	  * The closing fan does not merely spin on a broken ring, it CREATES an edge per turn - so
	    what used to be a fast failure became a grind. Measured: at eight thousand boxes the window
	    stopped producing frames at all, while the same scene on the old behaviour drew every frame
	    with one shape missing and a line in the log.

	So a refused cut fails the region again, as it always did. What is kept from the attempt is the
	CEILING below: the loops are bounded, so the hang cannot come back either. A shape that cannot
	be triangulated is dropped, quickly, and says so. */

	/* The ceiling, and it is not a magic number.

	Each turn of either loop cuts one triangle off the region, and a polygon of V vertices yields
	exactly V - 2 of them - so no region can need more turns than the mesh has vertices. A walk that
	wants more is walking a ring that does not close.

	Stated directly rather than measured by walking the ring first: that pre-walk was one more pass
	over every region for a number this already knows. */
	const uint32_t regionLimit = _nvertexes + 2;

	// A counter EACH: the walk down the two chains and the fan that closes what is left are two
	// passes over the same region, and either may take up to its length. Sharing one budget
	// between them cut the second short and changed a hundred and eighty icons.
	uint32_t turns = 0;
	uint32_t fanTurns = 0;

	while (++turns < regionLimit && up->getLeftLoopNext() != lo) {
		if (VertLeq(up->getDstVec(), lo->getOrgVec())) {
			if constexpr (TessVerbose == VerboseFlag::Full) {
				sprt::cout << "Lo: " << *lo << "\n";
				sprt::cout << "Up: " << *up << "\n";
			}

			/* up->Dst is on the left.  It is safe to form triangles from lo->Org.
			 * The EdgeGoesLeft test guarantees progress even when some triangles
			 * are CW, given that the upper and lower chains are truly monotone.
			 */
			v0 = &lo->getOrgVec();
			v1 = &lo->getDstVec();
			v2 = &lo->getLeftLoopNext()->getDstVec();

			while (lo->getLeftLoopNext() != up // invariant is not reached
					&& (lo->getLeftLoopNext()->goesLeft()
							|| Vec2::isCounterClockwise(*v0, *v1, *v2))) {
				auto tempHalfEdge = connectEdges(lo->getLeftLoopNext(), lo);
				if (tempHalfEdge == nullptr) {
					return false;
				}

				lo = tempHalfEdge->sym();
				v0 = &lo->getOrgVec();
				v1 = &lo->getDstVec();
				v2 = &lo->getLeftLoopNext()->getDstVec();

				if (tempHalfEdge && !isDegenerateTriangle(tempHalfEdge)) {
					_faceEdges.emplace_back(tempHalfEdge);
				}
			}
			lo = lo->getLeftLoopPrev();
			lo->_mark = v;
		} else {
			if constexpr (TessVerbose == VerboseFlag::Full) {
				sprt::cout << "Up: " << *up << "\n";
				sprt::cout << "Lo: " << *lo << "\n";
			}

			v0 = &up->getDstVec();
			v1 = &up->getOrgVec();
			v2 = &up->getLeftLoopPrev()->getOrgVec();

			/* lo->Org is on the left.  We can make CCW triangles from up->Dst. */
			while (lo->getLeftLoopNext() != up
					&& (up->getLeftLoopPrev()->goesRight()
							|| !Vec2::isCounterClockwise(*v0, *v1, *v2))) {
				auto tempHalfEdge = connectEdges(up, up->getLeftLoopPrev());
				if (tempHalfEdge == nullptr) {
					return false;
				}

				up = tempHalfEdge->sym();
				v0 = &up->getDstVec();
				v1 = &up->getOrgVec();
				v2 = &up->getLeftLoopPrev()->getOrgVec();

				if (tempHalfEdge && !isDegenerateTriangle(tempHalfEdge)) {
					_faceEdges.emplace_back(tempHalfEdge);
				}
			}
			up = up->getLeftLoopNext();
			up->_mark = v;
		}
	}

	/* Now lo->Org == up->Dst == the leftmost vertex.  The remaining region
	 * can be tessellated in a fan from this leftmost vertex.
	 */
	// The closing fan, and the same rules: a refused cut fails it, and the ceiling ends it.
	while (++fanTurns < regionLimit && lo->getLeftLoopNext()->getLeftLoopNext() != up) {
		auto tempHalfEdge = connectEdges(lo->getLeftLoopNext(), lo);
		if (tempHalfEdge == nullptr) {
			return false;
		}
		if (tempHalfEdge && !isDegenerateTriangle(tempHalfEdge)) {
			_faceEdges.emplace_back(tempHalfEdge);
		}
		lo = tempHalfEdge->sym();
		lo->_mark = v;
	}

	if (lo && !isDegenerateTriangle(lo)) {
		_faceEdges.emplace_back(lo);
	}
	return true;
}

bool Tesselator::Data::sweepVertex(VertexPriorityQueue &pq, EdgeDict &dict, Vertex *v) {
	auto doConnectEdges = [&, this](HalfEdge *source, HalfEdge *target) {
		if constexpr (TessVerbose != VerboseFlag::None) {
			sprt::cout << "\t\tConnect: \n\t\t\t" << *source << "\n\t\t\t" << *target << "\n";
		}
		auto eNew = connectEdges(source->getLeftLoopPrev(), target);
		if (eNew) {
			_edgesOfInterests.emplace_back(eNew);
		}
		return eNew;
	};

	auto onVertex = [&, this](VertexType type, Edge *fullEdge, HalfEdge *e, HalfEdge *eNext) {
		if (_dryRun) {
			return;
		}
		auto ePrev = e->getLeftLoopPrev();
		auto ePrevEdge = ePrev->getEdge();
		switch (type) {
		case VertexType::Start:
			// 1. Insert e(i) in T and set helper(e, i) to v(i).
			if (!fullEdge->node) {
				fullEdge->node = dict.push(fullEdge, e->_realWinding);
			}
			fullEdge->node->helper = Helper{e, eNext, type};
			break;
		case VertexType::End:
			// 1. if helper(e, i-1) is a merge vertex
			// 2. 	then Insert the diagonal connecting v(i) to helper(e, i~1) in T.
			// 3. Delete e(i-1) from T.
			if (auto dictNode = ePrevEdge->node) {
				if (dictNode->helper.type == VertexType::Merge) {
					doConnectEdges(e, dictNode->helper.e1);
				}

				// dict.pop(dictNode);
				// ePrev->getEdge()->node = nullptr;
			}
			break;
		case VertexType::Split:
			// 1. Search in T to find the edge e(j) directly left of v(i)
			// 2. Insert the diagonal connecting v(i) to helper(e, j) in D.
			// 3. helper(e, j) <— v(i)
			// 4. Insert e(i) in T and set helper(e, i) to v(i)
			if constexpr (TessVerbose == VerboseFlag::Full) {
				sprt::cout << "\t\te: " << *e << "\n";
			}
			if (auto edgeBelow = dict.getEdgeBelow(e->origin, e->vertex)) {
				if constexpr (TessVerbose != VerboseFlag::None) {
					sprt::cout << "\t\tedgeBelow: " << *edgeBelow << "\n";
				}
				if (edgeBelow->helper.e1) {
					auto tmpE = doConnectEdges(e, edgeBelow->helper.e1);
					edgeBelow->helper = Helper{tmpE, eNext, type};
				}
			}
			if (!fullEdge->node) {
				fullEdge->node = dict.push(fullEdge, e->_realWinding);
			}
			fullEdge->node->helper = Helper{e, eNext, type};
			break;
		case VertexType::Merge:
			// 1. if helper(e, i-1) is a merge vertex
			// 2. 	then Insert the diagonal connecting v, to helper(e, i-1) in D.
			// 3. Delete e(i - 1) from T.
			if constexpr (TessVerbose == VerboseFlag::Full) {
				sprt::cout << "\t\tePrevEdge: " << *ePrevEdge << "\n";
			}
			if (auto dictNode = ePrevEdge->node) {
				if (dictNode->helper.type == VertexType::Merge) {
					doConnectEdges(e, dictNode->helper.e1);
					dictNode->helper.type = VertexType::RegularTop;
				}

				// dict.pop(dictNode);
				// ePrev->getEdge()->node = nullptr;
			}

			// 4. Search in T to find the edge e(j) directly left of v(i)
			// 5. if helper(e, j) is a merge vertex
			// 6. 	then Insert the diagonal connecting v, to helper(e, j) in D.
			// 7. helper(e, j) <— v(i)
			if (auto edgeBelow = dict.getEdgeBelow(e->origin, e->vertex)) {
				if constexpr (TessVerbose != VerboseFlag::None) {
					sprt::cout << "\t\tedgeBelow: " << *edgeBelow << "\n";
				}
				if (edgeBelow->helper.type == VertexType::Merge) {
					e = doConnectEdges(e, edgeBelow->helper.e1);
				}
				edgeBelow->helper = Helper{e, eNext, type};
			}
			break;
		case VertexType::RegularBottom: // boundary above vertex
			// 2. if helper(e, i-1) is a merge vertex
			// 3. 	then Insert the diagonal connecting v, to helper(e, i-1) in D
			// 4. Delete e(i-1) from T.
			// 5. Insert e(i) in T and set helper(e, i) to v(i)
			if constexpr (TessVerbose == VerboseFlag::Full) {
				sprt::cout << "\t\tePrevEdge: " << *ePrevEdge << "\n";
			}
			if (auto dictNode = ePrevEdge->node) {
				if (dictNode->helper.type == VertexType::Merge) {
					doConnectEdges(e, dictNode->helper.e1);
				}

				dict.pop(dictNode);
				ePrevEdge->node = nullptr;
			}
			if (!fullEdge->node) {
				fullEdge->node = dict.push(fullEdge, e->_realWinding);
			}
			fullEdge->node->helper = Helper{e, eNext, type};
			break;
		case VertexType::RegularTop: // boundary below vertex
			// 6. Search in T to find the edge e(j) directly left of v(i)
			// 7. if helper(e, j) is a merge vertex
			// 8. 	then Insert the diagonal connecting v(i) to helper(e, j) in D.
			// 9. helper(e, j) <- v(i)
			if (auto edgeBelow = dict.getEdgeBelow(e->origin, e->vertex)) {
				if (edgeBelow->helper.type == VertexType::Merge) {
					e = doConnectEdges(e, edgeBelow->helper.e1);
				}
				edgeBelow->helper = Helper{e, eNext, type};
			}
			break;
		}
	};

	if constexpr (TessVerbose != VerboseFlag::None) {
		sprt::cout << "Sweep event: " << v->_uniqueIdx << ": " << v->_origin << "\n";
	}

	_event = v->_origin;

	Vec2 tmp;
	IntersectionEvent event;

	// first - process intersections
	// Intersection can split some edge in dictionary with event vertex,
	// so, event vertex will no longer be valid for iteration
	do {
		if (auto node = dict.checkForIntersects(v, tmp, event, _vertexTolerance)) {
			if (processIntersect(v, node, tmp, event) == nullptr) {
				return false;
			}
		}
	} while (0);

	VertexType type;
	HalfEdge *e = v->_edge, *eEnd = v->_edge, *eNext = nullptr;
	Edge *fullEdge = nullptr;

	_eventVertex = v;

	do {
		e->getEdge()->updateInfo();
		fullEdge = e->getEdge();
		if (e->goesRight()) {
			// push outcoming edge
			if (auto node = dict.checkForIntersects(e, tmp, event, _vertexTolerance)) {
				// edges in dictionary should remains valid
				// intersections preserves left subedge, and no
				// intersection points can be at the left of sweep line
				if (processIntersect(v, node, e, tmp, event)) {
					if (!_eventVertex) {
						return false;
					}
					e = v->_edge;
				}
			}
		}
		e = e->_originNext;
	} while (e && e != v->_edge);

	if (!e) {
		return false;
	}

	// rotate to first left non-convex angle counterclockwise
	// its critical for correct winding calculations
	eEnd = e = getFirstEdge(v);

	do {
		fullEdge = e->getEdge();

		// save original next to prevent new edges processing
		// new edges always added between e and eNext around origin
		eNext = e->_originNext;

		if (e->goesRight()) {
			if (e->_originNext->goesRight()) {
				if (AngleIsConvex(e, e->_originNext)) {
					// winding can be taken from edge below bottom (next) edge
					// or 0 if there is no edges below
					auto edgeBelow = dict.getEdgeBelow(e->_originNext->getEdge());
					if (!edgeBelow) {
						e->_realWinding = e->_originNext->_realWinding = 0;
					} else {
						e->_realWinding = e->_originNext->sym()->_realWinding =
								edgeBelow->windingAbove;
					}

					if constexpr (TessVerbose != VerboseFlag::None) {
						sprt::cout << "\tright-convex: " << e << " " << e->getDstVec() << " - "
								   << e->getOrgVec() << " - " << e->_originNext->getDstVec()
								   << " = " << e->_realWinding;
					}

					type = VertexType::Split;
					if (isWindingInside(_winding, e->_realWinding)) {
						if constexpr (TessVerbose != VerboseFlag::None) {
							sprt::cout << "; Split\n";
						}
						onVertex(VertexType::Split, fullEdge, e, e->_originNext);
					} else {
						if constexpr (TessVerbose != VerboseFlag::None) {
							sprt::cout << "\n";
						}
					}
				} else {
					_edgesOfInterests.emplace_back(e);

					e->_realWinding = e->_originNext->sym()->_realWinding =
							e->sym()->_realWinding + e->sym()->_winding;

					if constexpr (TessVerbose != VerboseFlag::None) {
						sprt::cout << "\tright: " << e << " " << e->getDstVec() << " - "
								   << e->getOrgVec() << " - " << e->_originNext->getDstVec()
								   << " = " << e->_realWinding << "(" << e->sym()->_realWinding
								   << "+" << e->sym()->_winding << ")";
					}

					type = VertexType::Start;
					if (isWindingInside(_winding, e->_realWinding)) {
						if constexpr (TessVerbose != VerboseFlag::None) {
							sprt::cout << "; Start\n";
						}
						onVertex(VertexType::Start, fullEdge, e, e->_originNext);
					} else {
						if constexpr (TessVerbose != VerboseFlag::None) {
							sprt::cout << "\n";
						}
					}
				}
			} else {
				// right-to-left
				e->_realWinding = e->_originNext->sym()->_realWinding;

				if constexpr (TessVerbose != VerboseFlag::None) {
					sprt::cout << "\tright-to-left: " << e << " " << e->getDstVec() << " - "
							   << e->getOrgVec() << " - " << e->_originNext->getDstVec() << " = "
							   << e->_realWinding << "(" << e->_originNext->sym()->_realWinding
							   << ":" << e->_originNext->_realWinding << ")";
				}

				type = VertexType::RegularBottom;
				if (isWindingInside(_winding, e->_realWinding)) {
					if constexpr (TessVerbose != VerboseFlag::None) {
						sprt::cout << "; RegularBottom\n";
					}
					onVertex(VertexType::RegularBottom, fullEdge, e, e->_originNext);
				} else {
					if constexpr (TessVerbose != VerboseFlag::None) {
						sprt::cout << "\n";
					}
				}
			}

			// sprt::cout << "\t\tpush edge" << fullEdge->getLeftVec() << " - " << fullEdge->getRightVec()
			//		<< " winding: " << e->_realWinding << "\n";

			// push outcoming edge
			if (!fullEdge->node) {
				fullEdge->node = dict.push(fullEdge, e->_realWinding);
				if (isWindingInside(_winding, e->_realWinding)) {
					fullEdge->node->helper = Helper{e, e->_originNext, type};
				}
			}
		} else {
			// remove incoming edge
			if (e->_originNext->goesRight()) {
				// left-to-right
				e->_originNext->sym()->_realWinding = e->_realWinding;

				if constexpr (TessVerbose != VerboseFlag::None) {
					sprt::cout << "\tleft-to-right: " << e << " " << e->getDstVec() << " - "
							   << e->getOrgVec() << " - " << e->_originNext->getDstVec() << " = "
							   << e->_realWinding;
				}

				type = VertexType::RegularTop;
				if (isWindingInside(_winding, e->_realWinding)) {
					if constexpr (TessVerbose != VerboseFlag::None) {
						sprt::cout << "; RegularTop\n";
					}
					onVertex(VertexType::RegularTop, fullEdge, e, e->_originNext);
				} else {
					if constexpr (TessVerbose != VerboseFlag::None) {
						sprt::cout << "\n";
					}
				}

			} else {
				if (AngleIsConvex(e, e->_originNext)) {
					if constexpr (TessVerbose != VerboseFlag::None) {
						sprt::cout << "\tleft-convex: " << e << " " << e->getDstVec() << " - "
								   << e->getOrgVec() << " - " << e->_originNext->getDstVec()
								   << " = " << e->_realWinding;
					}

					type = VertexType::Merge;
					if (isWindingInside(_winding, e->_realWinding)) {
						if constexpr (TessVerbose != VerboseFlag::None) {
							sprt::cout << "; Merge\n";
						}
						onVertex(VertexType::Merge, fullEdge, e, e->_originNext);
					} else {
						if constexpr (TessVerbose != VerboseFlag::None) {
							sprt::cout << "\n";
						}
					}

				} else {
					if constexpr (TessVerbose != VerboseFlag::None) {
						sprt::cout << "\tleft: " << e << " " << e->getDstVec() << " - "
								   << e->getOrgVec() << " - " << e->_originNext->getDstVec()
								   << " = " << e->_realWinding;
					}

					type = VertexType::End;
					if (isWindingInside(_winding, e->_realWinding)) {
						if constexpr (TessVerbose != VerboseFlag::None) {
							sprt::cout << "; End\n";
						}
						onVertex(VertexType::End, fullEdge, e, e->_originNext);
					} else {
						if constexpr (TessVerbose != VerboseFlag::None) {
							sprt::cout << "\n";
						}
					}
				}
			}

			if (fullEdge->node) {
				if (fullEdge->node->helper.type != VertexType::Merge) {
					dict.pop(fullEdge->node);
					fullEdge->node = nullptr;
				}
			}
		}
		e = eNext;
	} while (e != eEnd);

	_eventVertex = nullptr;

	v->_exportIdx = uint32_t(_exportVertexes.size());
	_exportVertexes.emplace_back(v);
	return true;
}

HalfEdge *Tesselator::Data::processIntersect(Vertex *v, const EdgeDictNode *edge1, HalfEdge *edge2,
		Vec2 &intersect, IntersectionEvent ev) {
	if constexpr (TessVerbose != VerboseFlag::None) {
		if (edge2) {
			sprt::cout << "Intersect: " << edge1->org << " - " << edge1->dst() << "  X  "
					   << edge2->getOrgVec() << " - " << edge2->getDstVec() << " = " << intersect
					   << ": " << ev << "\n";
		} else {
			sprt::cout << "Intersect: " << edge1->org << " - " << edge1->dst() << "  X  "
					   << v->_origin << " = " << intersect << ": " << ev << "\n";
		}
	}

	auto fixDictEdge = [&](const EdgeDictNode *e) {
		e->edge->direction = nan();
		e->edge->updateInfo();
		auto &org = e->edge->getOrgVec();
		auto &dst = e->edge->getDstVec();
		auto tmp = const_cast<EdgeDictNode *>(e);
		if (e->edge->inverted) {
			tmp->norm = org - dst;
			tmp->value.z = org.x;
			tmp->value.w = org.y;
			tmp->horizontal = sprt::abs(tmp->norm.x) > sprt::Epsilon<float>;
		} else {
			tmp->norm = dst - org;
			tmp->value.z = dst.x;
			tmp->value.w = dst.y;
			tmp->horizontal = sprt::abs(tmp->norm.x) > sprt::Epsilon<float>;
		}
	};

	auto checkRecursive = [&, this](HalfEdge *e) {
		if (auto node = _edgeDict->checkForIntersects(e, intersect, ev, _vertexTolerance)) {
			processIntersect(v, node, e, intersect, ev);
		}
	};

	Vertex *vertex = nullptr;

	switch (ev) {
	case IntersectionEvent::Regular:
		// split both edge1 and edge2, recursive check on new edge2 segments
		vertex = splitEdge(edge1->edge->inverted ? &edge1->edge->right : &edge1->edge->left, edge2,
				intersect);
		if constexpr (TessVerbose != VerboseFlag::None) {
			sprt::cout << "\tVertex: " << *vertex << "\n";
		}
		fixDictEdge(edge1);
		checkRecursive(edge2);
		_vertexQueue->insert(vertex);
		break;
	case IntersectionEvent::EventIsIntersection:
		// two cases: edges overlap or edge2 starts on edge1
		// in either cases we just split edge1, then merge vertexes
		// if edges is overlapping - it will be processed when new edge1 segment checked for intersections
		// edge2 can be null here
		vertex = splitEdge(edge1->edge->getPostitive(), intersect);
		fixDictEdge(edge1);
		if (!mergeVertexes(v, vertex)) {
			log::source().error("geom::Tesselator",
					"Tesselation failed on processIntersect: "
					"IntersectionEvent::EventIsIntersection");
			releaseVertex(v);
			return nullptr;
		}
		break;
	case IntersectionEvent::EdgeConnection1:
		// Edge2 ends somewhere on Edge1
		// split Edge1, next segment will be checked on next sweep event
		vertex = splitEdge(edge2->getEdge()->getPostitive(), intersect);
		if (!mergeVertexes(_vertexes[edge1->edge->getNegative()->vertex], vertex)) {
			log::source().error("geom::Tesselator",
					"Tesselation failed on processIntersect: IntersectionEvent::EdgeConnection1");
			releaseVertex(_vertexes[edge1->edge->getNegative()->vertex]);
			return nullptr;
		}
		break;
	case IntersectionEvent::EdgeConnection2:
		// Edge1 ends somewhere on Edge2
		// split Edge2, perform recursive checks on new segment
		vertex = splitEdge(edge1->edge->getPostitive(), intersect);
		fixDictEdge(edge1);
		if (!mergeVertexes(_vertexes[edge2->getEdge()->getNegative()->vertex], vertex)) {
			log::source().error("geom::Tesselator",
					"Tesselation failed on processIntersect: IntersectionEvent::EdgeConnection2");
			releaseVertex(_vertexes[edge2->getEdge()->getNegative()->vertex]);
			return nullptr;
		}
		break;
	case IntersectionEvent::Merge: return nullptr; break;
	}

	return edge2;
}

HalfEdge *Tesselator::Data::processIntersect(Vertex *v, const EdgeDictNode *edge1, Vec2 &intersect,
		IntersectionEvent ev) {
	if constexpr (TessVerbose != VerboseFlag::None) {
		sprt::cout << "Intersect: " << edge1->org << " - " << edge1->dst() << "  X  " << v->_origin
				   << " = " << intersect << ": " << ev << "\n";
	}

	auto fixDictEdge = [&](const EdgeDictNode *e) {
		e->edge->direction = nan();
		e->edge->updateInfo();
		auto &org = e->edge->getOrgVec();
		auto &dst = e->edge->getDstVec();
		auto tmp = const_cast<EdgeDictNode *>(e);
		if (e->edge->inverted) {
			tmp->norm = org - dst;
			tmp->value.z = org.x;
			tmp->value.w = org.y;
			tmp->horizontal = sprt::abs(tmp->norm.x) > sprt::Epsilon<float>;
		} else {
			tmp->norm = dst - org;
			tmp->value.z = dst.x;
			tmp->value.w = dst.y;
			tmp->horizontal = sprt::abs(tmp->norm.x) > sprt::Epsilon<float>;
		}
	};

	Vertex *vertex = nullptr;

	switch (ev) {
	case IntersectionEvent::EventIsIntersection:
		// two cases: edges overlap or edge2 starts on edge1
		// in either cases we just split edge1, then merge vertexes
		// if edges is overlapping - it will be processed when new edge1 segment checked for intersections
		// edge2 can be null here
		vertex = splitEdge(edge1->edge->getPostitive(), intersect);
		fixDictEdge(edge1);
		if (!mergeVertexes(v, vertex)) {
			log::source().error("geom::Tesselator",
					"Tesselation failed on processIntersect: "
					"IntersectionEvent::EventIsIntersection");
			releaseVertex(v);
			return nullptr;
		}
		break;
	default: return nullptr;
	}

	return edge1->edge ? edge1->edge->getPostitive() : nullptr;
}

Edge *Tesselator::Data::makeEdgeLoop(const Vec2 &origin) {
	Edge *edge = allocEdge();

	makeVertex(&edge->left)->_origin = origin;
	edge->right.copyOrigin(&edge->left);

	edge->left.origin = edge->right.origin = origin;
	edge->left._leftNext = &edge->left;
	edge->left._originNext = &edge->right;
	edge->right._leftNext = &edge->right;
	edge->right._originNext = &edge->left;

	return edge;
}

Vertex *Tesselator::Data::makeVertex(HalfEdge *eOrig) {
	Vertex *vNew = allocVertex();
	vNew->insertBefore(eOrig);
	return vNew;
}

HalfEdge *Tesselator::Data::pushVertex(HalfEdge *e, const Vec2 &vertex, bool clockwise,
		bool returnNew) {
	// Every coordinate that reaches the mesh comes through here - `Tesselator::pushVertex`,
	// `pushStrokeVertex`, `pushStrokeTop` and `pushStrokeBottom` all funnel into it - so this is
	// the one place the frame has to be taken off. Points the sweep computes later (splits,
	// intersections, displacements) are already in it, being arithmetic on numbers that are.
	if (!_hasNormalizeOrigin) {
		_inputOrigin = _outputOrigin = vertex;
		_hasNormalizeOrigin = true;
	}

	const Vec2 origin = vertex - _inputOrigin;

	if (!e) {
		/* Make a self-loop (one vertex, one edge). */
		auto edge = makeEdgeLoop(origin);

		edge->left._winding = (clockwise ? -1 : 1);
		edge->right._winding = (clockwise ? 1 : -1);
		e = &edge->left;
	} else {
		// split primary edge

		Edge *eNew = allocEdge(); // make new edge pair
		Vertex *v =
				makeVertex(&eNew->left); // make _sym as origin, because _leftNext will be clockwise
		v->_origin = origin;

		HalfEdge::splitEdgeLoops(e, &eNew->left, v);

		if (returnNew) {
			e = &eNew->left;
		}
	}

	if (origin.x < _bmin.x) {
		_bmin.x = origin.x;
	}
	if (origin.x > _bmax.x) {
		_bmax.x = origin.x;
	}
	if (origin.y < _bmin.y) {
		_bmin.y = origin.y;
	}
	if (origin.y > _bmax.y) {
		_bmax.y = origin.y;
	}

	++_nvertexes;

	return e;
}

HalfEdge *Tesselator::Data::connectEdges(HalfEdge *eOrg, HalfEdge *eDst) {
	if (eOrg->sym()->vertex == eDst->vertex) {
		if constexpr (TessVerbose == VerboseFlag::General) {
			sprt::cout << "ERROR: connectEdges on same vertex:\n\t" << *eOrg << "\n\t"
					   << *eOrg->sym() << "\n\t" << *eDst << "\n";
		}
		log::source().error("geom::Tesselator", "Tesselation failed on connectEdges");
		return nullptr;
	}

	// for triangle cut - eDst->lnext = eOrg
	Edge *edge = allocEdge();
	HalfEdge *eNew = &edge->left; // make new edge pair
	HalfEdge *eNewSym = eNew->sym();
	HalfEdge *ePrev = eDst->_originNext->sym();
	HalfEdge *eNext = eOrg->_leftNext;

	eNew->_realWinding = eNewSym->_realWinding = eOrg->_realWinding;

	eNew->copyOrigin(eOrg->sym());
	eNew->sym()->copyOrigin(eDst);

	ePrev->_leftNext = eNewSym;
	eNewSym->_leftNext = eNext; // external left chain
	eNew->_leftNext = eDst;
	eOrg->_leftNext = eNew; // internal left chain

	eNew->_originNext = eOrg->sym();
	eNext->_originNext = eNew; // org vertex chain
	eNewSym->_originNext = ePrev->sym();
	eDst->_originNext = eNewSym; // dst vertex chain

	if constexpr (TessVerbose != VerboseFlag::None) {
		sprt::cout << "\t\t\tConnected: " << *eNew << "\n";
	}

	edge->updateInfo();

	return eNew;
}

Vertex *Tesselator::Data::splitEdge(HalfEdge *eOrg1, const Vec2 &vec) {
	Vertex *v = nullptr;

	if constexpr (TessVerbose != VerboseFlag::None) {
		sprt::cout << "SplitEdge:\n\t" << *eOrg1 << "\n";
	}

	HalfEdge *eNew = &allocEdge()->left; // make new edge pair
	v = makeVertex(eNew); // make _sym as origin, because _leftNext will be clockwise
	v->_origin = vec;

	auto v2 = _vertexes[eOrg1->sym()->vertex];

	HalfEdge::splitEdgeLoops(eOrg1, eNew, v);

	if (v2->_edge == eOrg1->sym()) {
		v2->_edge = eNew->sym();
	}

	eNew->getEdge()->direction = nan();
	eNew->getEdge()->updateInfo();

	if constexpr (TessVerbose != VerboseFlag::None) {
		sprt::cout << "\t" << *eOrg1 << "\n\t" << *eNew << "\n";
	}

	return v;
}

Vertex *Tesselator::Data::splitEdge(HalfEdge *eOrg1, HalfEdge *eOrg2, const Vec2 &vec2) {
	Vertex *v = nullptr;
	HalfEdge *oPrevOrg = nullptr;
	HalfEdge *oPrevNew = nullptr;

	const Edge *fullEdge1 = eOrg1->getEdge();
	const Edge *fullEdge2 = eOrg2->getEdge();

	// swap edges if eOrg2 will be upper then eOrg1
	if (fullEdge2->direction > fullEdge1->direction) {
		auto tmp = eOrg2;
		eOrg2 = eOrg1;
		eOrg1 = tmp;
	}

	do {
		// split primary edge
		HalfEdge *eNew = &allocEdge()->left; // make new edge pair
		v = makeVertex(eNew); // make _sym as origin, because _leftNext will be clockwise
		v->_origin = vec2;

		auto v2 = _vertexes[eOrg1->sym()->vertex];

		HalfEdge::splitEdgeLoops(eOrg1, eNew, v);

		if (v2->_edge == eOrg1->sym()) {
			v2->_edge = eNew->sym();
		}

		oPrevOrg = eNew;
		oPrevNew = eOrg1->sym();

		eNew->getEdge()->updateInfo();
	} while (0);

	do {
		auto v2 = _vertexes[eOrg2->sym()->vertex];

		// split and join secondary edge
		HalfEdge *eNew = &allocEdge()->left; // make new edge pair

		HalfEdge::splitEdgeLoops(eOrg2, eNew, v);
		HalfEdge::joinEdgeLoops(eOrg2, oPrevOrg);
		HalfEdge::joinEdgeLoops(eNew->sym(), oPrevNew);

		if (v2->_edge == eOrg2->sym()) {
			v2->_edge = eNew->sym();
		}

		eNew->getEdge()->direction = nan();
		eNew->getEdge()->updateInfo();
	} while (0);

	return v;
}

// rotate to first left non-convex angle counterclockwise
HalfEdge *Tesselator::Data::getFirstEdge(Vertex *v) const {
	auto e = v->_edge;
	do {
		if (e->goesRight()) {
			if (e->_originNext->goesRight()) {
				if (AngleIsConvex(e, e->_originNext)) {
					// convex right angle is solution
					return e;
				} else {
					// non-convex right angle, skip
				}
			} else {
				// right-to-left angle, next angle is solution
				return e->_originNext;
			}
		} else {
			if (e->_originNext->goesLeft()) {
				if (AngleIsConvex(e, e->_originNext)) {
					// convex left angle, next angle is solution
					return e->_originNext;
				} else {
					// non-convex left angle, skip
				}
			} else {
				// left-to-right angle, skip
			}
		}
		e = e->_originNext;
	} while (e != v->_edge);
	return e;
}

static bool Tesselator_checkConnectivity(HalfEdge *eOrg) {
	if constexpr (TessVerbose != VerboseFlag::None) {
		auto eOrgTmp = eOrg;
		auto n = 0;
		while (n < 100) {
			eOrgTmp = eOrgTmp->_originNext;
			if (eOrgTmp == eOrg) {
				break;
			}
			++n;
		}

		if (n >= 100) {
			return false;
		}
	}

	return true;
}

bool Tesselator::Data::mergeVertexes(Vertex *org, Vertex *merge) {
	if (sprt::find(_protectedVertexes.begin(), _protectedVertexes.end(), org)
			!= _protectedVertexes.end()) {
		return true;
	}

	if (sprt::find(_protectedVertexes.begin(), _protectedVertexes.end(), merge)
			!= _protectedVertexes.end()) {
		return true;
	}

	if constexpr (TessVerbose != VerboseFlag::None) {
		sprt::cout << TessVerbose << "Merge:\n\t" << *org << "\n";
		org->foreach ([&](const HalfEdge &e) { sprt::cout << "\t\t" << e << "\n"; });

		sprt::cout << "\t" << *merge << "\n";
		merge->foreach ([&](const HalfEdge &e) { sprt::cout << "\t\t" << e << "\n"; });
	}

	auto insertNext = [&](HalfEdge *l, HalfEdge *r) {
		auto lNext = l->_originNext;

		if (r->_originNext != r) {
			auto rOriginPrev = r->getOriginPrev();
			auto rLeftPrev = r->getLeftLoopPrev();

			rOriginPrev->_originNext = r->_originNext;
			rLeftPrev->_leftNext = rOriginPrev;
		}

		r->_originNext = lNext;
		r->sym()->_leftNext = l;
		lNext->sym()->_leftNext = r;
		l->_originNext = r;
		return r;
	};

	auto mergeEdges = [&](HalfEdge *eOrg, HalfEdge *eMerge) {
		if (eOrg->_leftNext->sym() == eMerge) {
			if constexpr (TessVerbose != VerboseFlag::None) {
				sprt::cout << "Merge next (auto):\n\t" << *eOrg << "\n\t" << *eMerge << "\n";
			}

			return insertNext(eOrg, eMerge);
		} else if (eMerge->_leftNext->sym() == eOrg) {
			if constexpr (TessVerbose != VerboseFlag::None) {
				sprt::cout << "Merge prev (auto):\n\t" << *eOrg << "\n\t" << *eMerge << "\n";
			}

			insertNext(eOrg->getOriginPrev(), eMerge);
			return eOrg;
		} else {
			auto eOrgCcw = Vec2::isCounterClockwise(org->_origin, eOrg->getDstVec(),
					eOrg->_leftNext->getDstVec());
			auto eMergeCcw = Vec2::isCounterClockwise(org->_origin, eMerge->getDstVec(),
					eMerge->_leftNext->getDstVec());
			if (eOrgCcw == eMergeCcw) {
				if (eOrg->goesRight() && eMerge->goesRight()) {
					if (VertLeq(eOrg->getDstVec(), eMerge->getDstVec())) {
						if constexpr (TessVerbose != VerboseFlag::None) {
							sprt::cout << "Merge prev (direct):\n\t" << *eOrg << "\n\t" << *eMerge
									   << "\n";
						}

						insertNext(eOrg->getOriginPrev(), eMerge);
						return eOrg;
					} else {
						if constexpr (TessVerbose != VerboseFlag::None) {
							sprt::cout << "Merge next (direct):\n\t" << *eOrg << "\n\t" << *eMerge
									   << "\n";
						}

						return insertNext(eOrg, eMerge);
					}
				} else {
					if (VertLeq(eOrg->getDstVec(), eMerge->getDstVec())) {
						if constexpr (TessVerbose != VerboseFlag::None) {
							sprt::cout << "Merge next (reverse):\n\t" << *eOrg << "\n\t" << *eMerge
									   << "\n";
						}

						return insertNext(eOrg, eMerge);
					} else {
						if constexpr (TessVerbose != VerboseFlag::None) {
							sprt::cout << "Merge prev (reverse):\n\t" << *eOrg << "\n\t" << *eMerge
									   << "\n";
						}

						insertNext(eOrg->getOriginPrev(), eMerge);
						return eOrg;
					}
				}
			} else if (eOrgCcw) {
				if constexpr (TessVerbose != VerboseFlag::None) {
					sprt::cout << "Merge prev (ccw):\n\t" << *eOrg << "\n\t" << *eMerge << "\n";
				}

				auto r = insertNext(eOrg, eMerge);
				return r;
			} else {
				if constexpr (TessVerbose != VerboseFlag::None) {
					sprt::cout << "Merge next (ccw):\n\t" << *eOrg << "\n\t" << *eMerge << "\n";
				}

				insertNext(eOrg->getOriginPrev(), eMerge);
				return eOrg;
			}
		}
	};

	auto eOrg = org->_edge;
	auto eMerge = merge->_edge;
	auto eMergeEnd = eMerge;

	float lA = EdgeAngle(eOrg->getNormVec(), eOrg->getOriginNext()->getNormVec());
	if (sprt::isnan(lA)) {
		return false;
	}

	// merge common edges, if any
	do {
		auto eMergeNext = eMerge->_originNext;

		if (eMerge->sym()->vertex == org->_uniqueIdx && eMergeNext->_originNext == eMerge) {
			org->_edge = removeEdge(eMerge);
			releaseVertex(merge);
			if constexpr (TessVerbose == VerboseFlag::Full) {
				sprt::cout << TessVerbose << "Out:\n\t" << *org << "\n";
			}
			return true;
		}

		eMerge = eMergeNext;
	} while (eMerge != eMergeEnd);

	if (!Tesselator_checkConnectivity(org->_edge)) {
		log::source().error("geom::Tesselator", "Pizdets");
	}

	do {
		auto eMergeNext = eMerge->_originNext;
		// control infinite loop with max rotation angle metric
		float totalAngle = 0;

		do {
			if constexpr (TessVerbose != VerboseFlag::None) {
				sprt::cout << "eMerge: " << *eMerge << "\n";
			}
			auto rA = EdgeAngle(eOrg->getNormVec(), eMerge->getNormVec());
			if (sprt::isnan(rA)) {
				return false;
			}

			totalAngle += rA;
			if (EdgeAngleIsBelowTolerance(rA, _mathTolerance)) {
				auto tmpOrg = mergeEdges(eOrg, eMerge);

				if (!Tesselator_checkConnectivity(org->_edge)) {
					log::source().error("geom::Tesselator", "Pizdets");
				}

				eMerge->origin = eOrg->origin;
				eMerge->vertex = eOrg->vertex;
				eOrg = tmpOrg;
				lA = EdgeAngle(eOrg->getNormVec(), eOrg->getOriginNext()->getNormVec());
				if (sprt::isnan(lA)) {
					return false;
				}
				break;
			} else if (rA < lA) {
				if constexpr (TessVerbose != VerboseFlag::None) {
					sprt::cout << "Insert next:\n\t" << *eOrg << "\n\t" << *eMerge << "\n";
				}

				auto tmpOrg = insertNext(eOrg, eMerge);
				if (!Tesselator_checkConnectivity(org->_edge)) {
					log::source().error("geom::Tesselator", "Pizdets");
				}

				eMerge->origin = eOrg->origin;
				eMerge->vertex = eOrg->vertex;
				eOrg = tmpOrg;
				lA = EdgeAngle(eOrg->getNormVec(), eOrg->getOriginNext()->getNormVec());
				if (sprt::isnan(lA)) {
					return false;
				}
				break;
			} else {
				eOrg = eOrg->_originNext;
				lA = EdgeAngle(eOrg->getNormVec(), eOrg->getOriginNext()->getNormVec());
				if (sprt::isnan(lA)) {
					return false;
				}
			}
		} while (totalAngle < 32.0f);

		if (totalAngle >= 32.0f) {
			return false;
		}

		if (eMerge == eMergeNext) {
			break;
		}
		eMerge = eMergeNext;
	} while (eMerge != eMergeEnd);

	if (!Tesselator_checkConnectivity(org->_edge)) {
		log::source().error("geom::Tesselator", "Pizdets");
	}

	if (merge->_queueHandle != maxOf<QueueHandle>()) {
		_vertexQueue->remove(merge->_queueHandle);
		merge->_queueHandle = maxOf<QueueHandle>();
	}

	releaseVertex(merge);

	// remove degenerates

	// remove ears - edge cycles on same vertex
	auto eOrgEnd = eOrg = org->_edge;

	if (!Tesselator_checkConnectivity(eOrg)) {
		log::source().error("geom::Tesselator", "Pizdets");
	}

	do {
		if constexpr (TessVerbose != VerboseFlag::None) {
			sprt::cout << TessVerbose << "\t\tRemoveEars: " << *eOrg << "\n";
		}

		auto eOrgNext = eOrg->_originNext;

		if (eOrg->_leftNext->sym() == eOrg->_originNext
				&& eOrg->_originNext->_leftNext->sym() == eOrg) {
			auto eOrgJoin = eOrgNext;

			if constexpr (TessVerbose != VerboseFlag::None) {
				sprt::cout << TessVerbose << "\t\t\t: " << *eOrg << "\n";
				sprt::cout << TessVerbose << "\t\t\t: " << *eOrgJoin << "\n";
			}
			eOrgNext = eOrgJoin->_originNext;

			auto orgPrev = eOrg->getOriginPrev();
			auto orgLeftPrev = eOrg->getLeftLoopPrev();
			auto joinLeftPrev = eOrgJoin->getLeftLoopPrev();

			orgPrev->_originNext = eOrgJoin->_originNext;
			orgLeftPrev->_leftNext = eOrg->_leftNext->_leftNext;
			joinLeftPrev->_leftNext = eOrgJoin->_leftNext->_leftNext;

			auto vertex = _vertexes[eOrg->_leftNext->vertex];

			auto orgEdge = eOrg->getEdge();
			if (orgEdge->node) {
				_edgeDict->pop(orgEdge->node);
				orgEdge->node = nullptr;
			}
			releaseEdge(orgEdge);

			auto joinEdge = eOrgJoin->getEdge();
			if (joinEdge->node) {
				_edgeDict->pop(joinEdge->node);
				joinEdge->node = nullptr;
			}
			releaseEdge(joinEdge);

			// we can not touch vertexes, that was already exported
			if (VertLeq(_event, vertex->_origin)) {
				if (vertex->_queueHandle != maxOf<QueueHandle>()) {
					_vertexQueue->remove(vertex->_queueHandle);
					vertex->_queueHandle = maxOf<QueueHandle>();
				}
				if (vertex == _eventVertex) {
					_eventVertex = nullptr;
				}
			}

			releaseVertex(vertex);

			if (eOrg == eOrgEnd || eOrgJoin == eOrgEnd) {
				eOrgEnd = org->_edge = eOrgNext->getOriginPrev();
			}

			if (eOrg == eOrgEnd || eOrgJoin == eOrgEnd || eOrg == eOrgNext) {
				// origin vertex is empty
				if (org == _eventVertex) {
					_eventVertex = nullptr;
				}

				org->_edge = nullptr;
				log::source().error("geom::Tesselator", "Pizdets");
				return false;
			}
		}

		eOrg = eOrgNext;
	} while (eOrg != eOrgEnd);

	if constexpr (TessVerbose != VerboseFlag::None) {
		sprt::cout << "\tResult (pre): " << eOrg->vertex << "\n";
		org->foreach ([&](const HalfEdge &e) {
			sprt::cout << "\t\t" << e << "\n";
			if constexpr (TessVerbose == VerboseFlag::Full) {
				e.foreachOnFace([&](const HalfEdge &e) { sprt::cout << "\t\t\t" << e << "\n"; });
			}
		});
	}

	// process overlaps
	_protectedVertexes.emplace_back(org);


	bool overlapProcessed = false;
	while (!overlapProcessed) {
		eOrgEnd = eOrg = org->_edge;
		if constexpr (TessVerbose != VerboseFlag::None) {
			sprt::cout << "Start overlap processing: " << eOrg->vertex << " ("
					   << _protectedVertexes.size() << "): " << *eOrg << "\n";
		}

		do {
			auto eOrgNext = eOrg->_originNext;

			float a = EdgeAngle(eOrg->getNormVec(), eOrgNext->getNormVec());
			if (sprt::isnan(a)) {
				return false;
			}
			if (EdgeAngleIsBelowTolerance(a, _mathTolerance)) {
				auto eOrgJoin = eOrgNext;

				eOrgNext = eOrgJoin->_originNext;

				if (processEdgeOverlap(org, eOrg, eOrgJoin)) {
					eOrgEnd = eOrg = org->_edge;
					eOrg = eOrg->_originNext;
					break;
				} else {
					if (eOrgJoin == eOrgEnd) {
						overlapProcessed = true;
						break;
					}
				}
			}

			eOrg = eOrgNext;
		} while (eOrg != eOrgEnd);

		if (eOrg == eOrgEnd) {
			overlapProcessed = true;
		}
	}

	// remove loops
	eOrgEnd = eOrg = org->_edge;
	do {
		auto eOrgNext = eOrg->_originNext;

		if (eOrg->_leftNext->_leftNext == eOrg) {
			auto next = eOrg->_leftNext->sym();
			if (next == eOrgNext) {
				if (org->_edge == eOrg || eOrgEnd == eOrg) {
					eOrgEnd = org->_edge = eOrgNext;
				}

				auto eOrgPrev = eOrg->getOriginPrev();
				auto eOrgSym = eOrg->sym();
				auto eOrgSymPrev = eOrgSym->getLeftLoopPrev();
				auto eOrgSymOrgPrev = eOrgSym->getOriginPrev();
				auto eNextSym = next->sym();

				if (next->_winding != eOrg->_winding) {
					next->_winding += eOrg->_winding;
				}
				if (eNextSym->_winding != eOrgSym->_winding) {
					eNextSym->_winding += eOrgSym->_winding;
				}

				if constexpr (TessVerbose != VerboseFlag::None) {
					sprt::cout << "Remove loop: " << eOrg->vertex << " ("
							   << _protectedVertexes.size() << "):\n" "\t" << *eOrg << "\n\t"
							   << *eOrg->_leftNext << "\n";
				}

				eOrgSymPrev->_leftNext = eNextSym;
				eNextSym->_leftNext = eOrgSym->_leftNext;

				eOrgPrev->_originNext = eOrg->_originNext;
				eOrgSymOrgPrev->_originNext = eOrgSym->_originNext;

				_vertexes[eOrgSymOrgPrev->vertex]->_edge = eOrgSym->_originNext;

				if constexpr (TessVerbose != VerboseFlag::None) {
					_vertexes[eOrgSymOrgPrev->vertex]->foreach ([&](const HalfEdge &e) {
						sprt::cout << "\tVertex " << eOrgSymOrgPrev->vertex << ": " << e << "\n";
					});
				}

				auto joinEdge = eOrg->getEdge();
				if (joinEdge->node) {
					_edgeDict->pop(joinEdge->node);
					joinEdge->node = nullptr;
				}
				releaseEdge(joinEdge);
			}
		}

		eOrg = eOrgNext;
	} while (eOrg != eOrgEnd);

	if constexpr (TessVerbose != VerboseFlag::None) {
		sprt::cout << "\tResult (post): " << eOrg->vertex << "\n";
		org->foreach ([&](const HalfEdge &e) {
			sprt::cout << "\t\t" << e << "\n";
			if constexpr (TessVerbose == VerboseFlag::Full) {
				e.foreachOnFace([&](const HalfEdge &e) { sprt::cout << "\t\t\t" << e << "\n"; });
			}
		});
	}

	_protectedVertexes.pop_back();
	return true;
}

HalfEdge *Tesselator::Data::removeEdge(HalfEdge *e) {
	auto eSym = e->sym();

	auto eLeftPrev = e->getLeftLoopPrev();
	auto eSymLeftPrev = eSym->getLeftLoopPrev();
	auto eOriginPrev = e->getOriginPrev();
	auto eSymOriginPrev = eSym->getOriginPrev();

	e->_originNext->origin = e->_leftNext->origin;
	e->_originNext->vertex = e->_leftNext->vertex;

	e->_originNext->getEdge()->direction = nan();
	e->_originNext->getEdge()->updateInfo();

	eLeftPrev->_leftNext = e->_leftNext;
	eSymLeftPrev->_leftNext = eSym->_leftNext;

	eOriginPrev->_originNext = eSym->_originNext;
	eSymOriginPrev->_originNext = e->_originNext;

	releaseEdge(e->getEdge());

	return eSymOriginPrev->_originNext;
}

HalfEdge *Tesselator::Data::removeDegenerateEdges(HalfEdge *e, uint32_t *nedges, bool safeRemove) {
	while (e && !e->_mark) {
		auto eLnext = e->_leftNext;

		auto edge = e->getEdge();
		auto edgeNext = eLnext->getEdge();

		edge->updateInfo();
		edgeNext->updateInfo();

		while (VertEq(e->getOrgVec(), e->getDstVec(), _vertexTolerance)
				&& e->_leftNext->_leftNext != e) {
			if constexpr (TessVerbose != VerboseFlag::None) {
				sprt::cout << "Remove degenerate: " << *e << "\n";
			}

			auto vertex = _vertexes[e->sym()->vertex];
			auto merge = _vertexes[e->vertex];

			auto tmp = e;
			e = eLnext;
			eLnext = e->_leftNext;

			vertex->_edge = removeEdge(tmp);

			if (safeRemove) {
				releaseVertex(merge);
			}

			if (nedges) {
				--*nedges;
			}

			edge = e->getEdge();
			edgeNext = eLnext->getEdge();

			edge->updateInfo();
			edgeNext->updateInfo();
		}

		if (eLnext->_leftNext == e) {
			// Degenerate contour (one or two edges)

			if (eLnext != e) {
				if (safeRemove) {
					releaseVertex(_vertexes[eLnext->vertex]);
					releaseVertex(_vertexes[eLnext->sym()->vertex]);
				}
				releaseEdge(eLnext->getEdge());
				if (nedges) {
					--*nedges;
				}
			}
			if (safeRemove) {
				releaseVertex(_vertexes[e->vertex]);
				releaseVertex(_vertexes[e->sym()->vertex]);
			}
			releaseEdge(e->getEdge());
			if (nedges) {
				--*nedges;
			}
			return nullptr; // last edge destroyed
		}

		// check and remove tail-like structs
		if (FloatEq(edge->direction, edgeNext->direction, _mathTolerance)) {
			if (safeRemove) {
				HalfEdge *tmp = eLnext;

				// we need to recheck e for another degenerate cases
				e = e->getLeftLoopPrev();

				if (safeRemove) {
					auto vertex = _vertexes[tmp->sym()->vertex];
					auto merge = _vertexes[tmp->vertex];

					vertex->_edge = removeEdge(tmp);
					releaseVertex(merge);
				} else {
					auto vertex = _vertexes[tmp->sym()->vertex];
					vertex->_edge = removeEdge(tmp);
				}

				if (nedges) {
					--*nedges;
				}
			} else if (eLnext->_leftNext->_leftNext == e) {
				return nullptr;
			}
		}
		e->_mark = 1;
		e = e->_leftNext;
	};

	return e;
}

bool Tesselator::Data::processEdgeOverlap(Vertex *org, HalfEdge *e1, HalfEdge *e2) {
	if (sprt::find(_protectedEdges.begin(), _protectedEdges.end(), e1) != _protectedEdges.end()) {
		return false;
	}

	if (sprt::find(_protectedEdges.begin(), _protectedEdges.end(), e2) != _protectedEdges.end()) {
		return false;
	}


	if constexpr (TessVerbose != VerboseFlag::None) {
		sprt::cout << "processEdgeOverlap:\n\t" << *e1 << "\n\t" << *e2 << "\n";
	}

	if (e1->goesLeft()) {
		if (!VertLeq(e2->getDstVec(), e1->getDstVec())) {
			sprt::swap(e1, e2); // split e2
		}
	} else {
		if (!VertLeq(e1->getDstVec(), e2->getDstVec())) {
			sprt::swap(e1, e2); // split e2
		}
	}

	Vertex *vMerge;
	if (!VertEq(e1->getDstVec(), e2->getDstVec(), _vertexTolerance)) {
		vMerge = splitEdge(e2, e1->getDstVec());
	} else {
		vMerge = _vertexes[e2->sym()->vertex];
	}

	if constexpr (TessVerbose != VerboseFlag::None) {
		sprt::cout << "Overlap: " << *e2 << "\n";
	}

	auto vOrgIdx = e1->sym()->vertex;
	auto vOrg = _vertexes[vOrgIdx];

	_protectedEdges.emplace_back(e2->sym());
	_protectedEdges.emplace_back(e1->sym());

	bool result = false;

	do {
		if (vOrg != vMerge) {
			if (sprt::find(_protectedVertexes.begin(), _protectedVertexes.end(), vOrg)
					!= _protectedVertexes.end()) {
				break;
			}

			if (sprt::find(_protectedVertexes.begin(), _protectedVertexes.end(), vMerge)
					!= _protectedVertexes.end()) {
				break;
			}

			result = mergeVertexes(vOrg, vMerge);
		}
	} while (0);

	_protectedEdges.pop_back();
	_protectedEdges.pop_back();

	return result;
}

bool Tesselator::Data::isDegenerateTriangle(HalfEdge *e) {
	if (e->_leftNext->_leftNext == e) {
		return true;
	}

	auto eEnd = e;

	do {
		auto eLnext = e->_leftNext;

		auto edge = e->getEdge();
		auto edgeNext = eLnext->getEdge();

		edge->updateInfo();
		edgeNext->updateInfo();

		// check and remove tail-like structs
		if (FloatEq(edge->direction, edgeNext->direction, _mathTolerance)) {
			return true;
		}
		e = eLnext;
	} while (e != eEnd);

	return false;
}

bool Tesselator::Data::removeDegenerateEdges(FaceEdge *e, size_t &removed) {
	if (e->_next->_next == e) {
		return true;
	}

	auto eEnd = e;

	do {
		auto eLnext = e->_next;

		while (VertEq(e->_vertex, eLnext->_vertex, _vertexTolerance) && e->_next->_next != e) {
			eLnext = e->_next->_next;

			if (eEnd == e->_next) {
				eEnd = eLnext;
			}

			e->_next = e->_next->_next;
			++removed;
		}

		if (eLnext->_next == e) {
			if (eLnext != e) {
				++removed;
			}
			++removed;
			return false; // last edge destroyed
		}

		// check and remove tail-like structs
		if (FloatEq(e->_direction, eLnext->_direction, _mathTolerance)) {
			if (eLnext->_next->_next == e) {
				removed += 3;
				return false;
			}
		}
		e = eLnext;
	} while (e != eEnd);

	return true;
}

uint32_t Tesselator::Data::followBoundary(FaceEdge *face, HalfEdge *e, uint8_t mark) {
	auto findNext = [&, this](HalfEdge *eNext) {
		if (eNext->_originNext->_originNext == eNext) {
			// simple vertex
			return eNext;
		} else {
			// find next boundary in opposite direction to separate subboundaries
			auto prev = eNext->_originNext;
			while (isWindingInside(_winding, prev->_realWinding) && prev != eNext) {
				prev = prev->_originNext;
			}
			if (prev != eNext) {
				splitVertex(eNext, prev);
			}
			return prev;
		}
		return eNext;
	};

	uint32_t nsegments = 0;
	// assume left loop is outside
	while (e->_mark != mark) {
		auto target = e->_leftNext;
		auto eNext = findNext(target);

		if (!face) {
			face = allocFaceEdge();
			_boundaries.emplace_back(face);
			face->_next = face;
		} else {
			auto tmp = allocFaceEdge();
			tmp->_next = face->_next;
			face->_next = tmp;
			face = tmp;
		}

		++nsegments;
		face->_vertex = _vertexes[e->vertex];
		face->_displaced = face->_origin = e->origin;
		face->_direction = e->getEdge()->direction;

		if (target != eNext) {
			face->_splitVertex = true;
		} else if (face->_vertex->_uniqueIdx >= _nvertexes) {
		}

		e->_mark = mark;
		e = eNext;
	}
	return nsegments;
}

void Tesselator::Data::splitVertex(HalfEdge *first, HalfEdge *last) {
	// create new vertex for first->orgNext -> last
	auto org = _vertexes[first->vertex];
	auto vertex = allocVertex();

	auto front = first->_originNext;
	auto back = last->_originNext;

	first->getLeftLoopPrev()->_leftNext = last;
	first->_originNext = back;

	last->getLeftLoopPrev()->_leftNext = first;
	last->_originNext = front;

	org->_edge = front;
	vertex->_edge = first;
	vertex->_origin = front->origin;

	auto e = first;
	do {
		e->vertex = vertex->_uniqueIdx;
		e = e->_originNext;
	} while (e != first);

	if (org->_exportIdx != maxOf<uint32_t>()) {
		vertex->_exportIdx = uint32_t(_exportVertexes.size());
		_exportVertexes.emplace_back(vertex);
	}
}

void Tesselator::Data::displaceBoundary(FaceEdge *edge) {
	auto &v0 = edge->_origin;
	auto &v1 = edge->_next->_origin;
	auto &v2 = edge->_next->_next->_origin;

	// use optimized combined direction/normal func
	Vec4 result;
	getVertexNormal(&v0.x, &v1.x, &v2.x, &result.x);

	float offsetValue = _boundaryOffset;
	float insetValue = _boundaryInset;

	bool shouldRelocate = false;
	switch (_relocateRule) {
	case RelocateRule::Never:
		// do not inset, increase offset
		offsetValue += _boundaryInset * 0.5f;
		insetValue = 0.0f;
		break;
	case RelocateRule::Always:
	case RelocateRule::Monotonize:
	case RelocateRule::DistanceField: shouldRelocate = true; break;
	case RelocateRule::Auto:
		if (edge->_next->_splitVertex) {
			shouldRelocate = true;
		} else {
			// do not inset, increase offset
			offsetValue += _boundaryInset * 0.5f;
			insetValue = 0.0f;
		}
		break;
	}

	edge->_next->_norm = edge->_next->_vertex->_norm = -Vec2(result.z, result.w);

	if (result.x < -0.0f && _relocateRule == RelocateRule::DistanceField) {
		auto a0 = v0 - v1;
		auto a2 = v2 - v1;

		auto cross = Vec2::cross(a0, a2);
		auto dot = Vec2::dot(a0, a2);
		auto angle = M_PI - atan2f(cross, dot);
		auto length = offsetValue * angle * _contentScale;

		uint16_t minVertexes = static_cast<uint16_t>(sprt::floor(angle / M_PI_4));
		uint16_t vertexes = static_cast<uint16_t>(sprt::floor(length / 4.0f));

		auto perp = Vec2(v1 - v0).getPerp();
		perp.normalize();
		edge->_next->_displaced = v1 + perp * offsetValue;

		auto rperp = Vec2(v1 - v2).getRPerp();
		rperp.normalize();
		edge->_next->_rperp = v1 + rperp * offsetValue;

		edge->_next->_nextra = sprt::max(minVertexes, vertexes);
		edge->_next->_value = 0.0f;
		edge->_next->_angle = angle;
	} else {
		if (sprt::isnan(result.y) || result.y > 3.0f) {
			edge->_next->_value = 1.0f - 3.0f / result.y;
			result.y = 3.0f;
		}

		const float offsetMod = copysign(result.y * offsetValue, result.x);

		edge->_next->_displaced = Vec2(v1.x + result.z * offsetMod, v1.y + result.w * offsetMod);
	}

	if (shouldRelocate) {
		const float insetMod = copysign(result.y * insetValue, result.x);
		if (edge->_next->_vertex) {
			edge->_next->_vertex->relocate(
					Vec2(v1.x - result.z * insetMod, v1.y - result.w * insetMod));
			sprt_passert(!sprt::isnan(edge->_next->_vertex->_origin.x)
							&& !sprt::isnan(edge->_next->_vertex->_origin.y),
					"Tess: displaced vertex is NaN");
		}
	}
}

} // namespace stappler::geom
