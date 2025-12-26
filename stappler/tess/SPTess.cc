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
#include "SPTessSimd.hpp"

namespace STAPPLER_VERSIONIZED stappler::geom {

static constexpr VerboseFlag TessVerbose = VerboseFlag::None;

struct Tesselator::Data : ObjectAllocator {
	// potential root face edges (connected to right non-convex angle)
	Vec2 _bmax, _bmin, _event;

	TessResult *_result = nullptr;
	EdgeDict *_edgeDict = nullptr;
	VertexPriorityQueue *_vertexQueue = nullptr;

	float _mathTolerance = std::numeric_limits<float>::epsilon() * 4.0f;

	Winding _winding = Winding::NonZero;
	float _boundaryOffset = 0.0f;
	float _boundaryInset = 0.0f;
	float _contentScale = 1.0f;
	uint32_t _nvertexes = 0;
	uint8_t _markValue = 1;

	RelocateRule _relocateRule = RelocateRule::Auto;

	bool _dryRun = false;
	bool _valid = true;

	Vertex *_eventVertex = nullptr;

	memory::vector<Vertex *> _protectedVertexes;
	memory::vector<HalfEdge *> _protectedEdges;

	Data(memory::pool_t *p);

	bool computeInterior();

	// Compute boundary face contour, also - split vertexes in subboundaries for antialiasing
	uint32_t computeBoundary();

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

void Tesselator::setWindingRule(Winding winding) { _data->_winding = winding; }

Winding Tesselator::getWindingRule() const { return _data->_winding; }

void Tesselator::preallocate(uint32_t n) {
	_data->preallocateVertexes(n);
	_data->preallocateEdges(n);
}

Tesselator::Cursor Tesselator::beginContour(bool clockwise) {
	return Cursor{nullptr, nullptr, clockwise};
}

bool Tesselator::pushVertex(Cursor &cursor, const Vec2 &vertex) {
	if (!vertex.isValid()) {
		return false;
	}

	if (!cursor.closed) {
		if (cursor.count == 0) {
			cursor.origin = vertex;
		}

		if constexpr (TessVerbose != VerboseFlag::None) {
			std::cout << std::setprecision(std::numeric_limits<float>::digits10 + 1)
					  << "Push: " << vertex << "\n";
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
			std::cout << std::setprecision(std::numeric_limits<float>::digits10 + 1)
					  << "Push (stroke): " << vertex << ", " << offset << "\n";
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
			std::cout << std::setprecision(std::numeric_limits<float>::digits10 + 1)
					  << "Push (stroke-top): " << vertex << "\n";
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
			std::cout << std::setprecision(std::numeric_limits<float>::digits10 + 1)
					  << "Push (stroke-bottom): " << vertex << "\n";
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
			std::cout << "Contour:\n";
			cursor.edge->foreachOnFace(
					[&](HalfEdge &e) { std::cout << TessVerbose << "\t" << e << "\n"; });
		}
		_data->trimVertexes();
		return true;
	} else {
		if constexpr (TessVerbose != VerboseFlag::None) {
			std::cout << "Fail to add empty contour\n";
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
			std::cout << "Contour:\n";
			cursor.edge->foreachOnFace(
					[&](HalfEdge &e) { std::cout << TessVerbose << "\t" << e << "\n"; });
		}
		return true;
	} else {
		if constexpr (TessVerbose != VerboseFlag::None) {
			std::cout << "Fail to add empty contour\n";
		}
	}
	_data->trimVertexes();
	return false;
}

bool Tesselator::prepare(TessResult &res) {
	_data->_result = &res;
	_data->_vertexOffset = res.nvertexes;

	if ((_data->_relocateRule == RelocateRule::Monotonize)
			&& (_data->_boundaryOffset > 0.0f || _data->_boundaryInset > 0.0f)) {
		_data->_dryRun = true;
	}

	if (!_data->computeInterior()) {
		return false;
	}

	if (_data->_boundaryOffset > 0.0f || _data->_boundaryInset > 0.0f) {
		auto nBoundarySegments = _data->computeBoundary();

		if constexpr (TessVerbose != VerboseFlag::None) {
			for (auto &it : _data->_boundaries) {
				if (!it->_degenerate) {
					std::cout << "Boundary:\n";
					it->foreach ([&](const FaceEdge &edge) { std::cout << "\t" << edge << "\n"; });
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
					std::cout << "Vertex: " << *it << "\n";
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
			res.pushVertex(res.target, nexports + _data->_vertexOffset, e->_displaced, e->_value,
					(e->_vertex->_origin - e->_displaced).getNormalized());
			++nexports;

			if (e->_nextra > 0) {
				auto incr = e->_angle / e->_nextra;
				float angle = -incr;
				for (uint16_t i = 0; i < e->_nextra; ++i) {
					Vec2 point = e->_displaced;
					point.rotate(e->_origin, angle);

					res.pushVertex(res.target, nexports + _data->_vertexOffset, point, e->_value,
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
			res.pushVertex(res.target, it->_exportIdx + _data->_vertexOffset, it->_origin, 1.0f,
					it->_norm);
		}
	}

	auto mark = ++_data->_markValue;
	for (auto &it : _data->_faceEdges) {
		if (it && it->_mark != mark && isWindingInside(_data->_winding, it->_realWinding)) {
			uint32_t vertex = 0;

			it->foreachOnFace([&, this](HalfEdge &edge) {
				if (vertex < 3) {
#if DEBUG
					if (_data->_vertexes.size() >= edge.vertex) {
						const auto v = _data->_vertexes[edge.vertex];
						if (v) {
							const auto qidx = v->_exportIdx;
							triangle[vertex] = qidx + _data->_vertexOffset;
						} else {
							std::cout << "Invalid vertex: " << edge.vertex << "\n";
							::abort();
						}
					} else {
						std::cout << "Invalid vertex index: " << edge.vertex << " of "
								  << _data->_vertexes.size() << "\n";
						::abort();
					}
#else
					triangle[vertex] =
							_data->_vertexes[edge.vertex]->_exportIdx + _data->_vertexOffset;
#endif
				}
				edge._mark = mark;
				++vertex;
			});

			if (vertex == 3) {
				res.pushTriangle(res.target, triangle);
			}
		}
	}

	return true;
}

Tesselator::Data::Data(memory::pool_t *p) : ObjectAllocator(p) { }

bool Tesselator::Data::computeInterior() {
	bool result = true;

	_exportVertexes.clear();

	EdgeDict dict(_pool, 8);
	VertexPriorityQueue pq(_pool, _vertexes);

	_edgeDict = &dict;
	_vertexQueue = &pq;

	Vertex *v, *vNext;
	while ((v = pq.extractMin()) != nullptr) {
		for (;;) {
			vNext = pq.getMin();
			if (vNext == NULL || !VertEq(vNext, v, _mathTolerance)) {
				break;
			}

			vNext = pq.extractMin();
			if (!mergeVertexes(v, vNext)) {
				log::source().error("geom::Tesselator", "Tesselation failed on mergeVertexes");
				result = false;
				break;
			}
		}

		dict.update(v, _mathTolerance);

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
					std::cout << "Inside Face: \n";
					e->left.foreachOnFace([&](HalfEdge &edge) {
						std::cout << "\t" << TessVerbose << vertex++ << "; " << edge << "\n";
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
					std::cout << "Inside Face: \n";
					e->right.foreachOnFace([&](HalfEdge &edge) {
						std::cout << "\t" << TessVerbose << vertex++ << "; " << edge << "\n";
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
		std::cout << "Start: Up: " << *up << "\n";
		std::cout << "Start: Lo: " << *lo << "\n";
	}

	up->_mark = v;
	lo->_mark = v;

	const Vec2 *v0, *v1, *v2;

	while (up->getLeftLoopNext() != lo) {
		if (VertLeq(up->getDstVec(), lo->getOrgVec())) {
			if constexpr (TessVerbose == VerboseFlag::Full) {
				std::cout << "Lo: " << *lo << "\n";
				std::cout << "Up: " << *up << "\n";
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
				std::cout << "Up: " << *up << "\n";
				std::cout << "Lo: " << *lo << "\n";
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
	while (lo->getLeftLoopNext()->getLeftLoopNext() != up) {
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
			std::cout << "\t\tConnect: \n\t\t\t" << *source << "\n\t\t\t" << *target << "\n";
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
				std::cout << "\t\te: " << *e << "\n";
			}
			if (auto edgeBelow = dict.getEdgeBelow(e->origin, e->vertex)) {
				if constexpr (TessVerbose != VerboseFlag::None) {
					std::cout << "\t\tedgeBelow: " << *edgeBelow << "\n";
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
				std::cout << "\t\tePrevEdge: " << *ePrevEdge << "\n";
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
					std::cout << "\t\tedgeBelow: " << *edgeBelow << "\n";
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
				std::cout << "\t\tePrevEdge: " << *ePrevEdge << "\n";
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
		std::cout << "Sweep event: " << v->_uniqueIdx << ": " << v->_origin << "\n";
	}

	_event = v->_origin;

	Vec2 tmp;
	IntersectionEvent event;

	// first - process intersections
	// Intersection can split some edge in dictionary with event vertex,
	// so, event vertex will no longer be valid for iteration
	do {
		if (auto node = dict.checkForIntersects(v, tmp, event, _mathTolerance)) {
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
			if (auto node = dict.checkForIntersects(e, tmp, event, _mathTolerance)) {
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
						std::cout << "\tright-convex: " << e << " " << e->getDstVec() << " - "
								  << e->getOrgVec() << " - " << e->_originNext->getDstVec() << " = "
								  << e->_realWinding;
					}

					type = VertexType::Split;
					if (isWindingInside(_winding, e->_realWinding)) {
						if constexpr (TessVerbose != VerboseFlag::None) {
							std::cout << "; Split\n";
						}
						onVertex(VertexType::Split, fullEdge, e, e->_originNext);
					} else {
						if constexpr (TessVerbose != VerboseFlag::None) {
							std::cout << "\n";
						}
					}
				} else {
					_edgesOfInterests.emplace_back(e);

					e->_realWinding = e->_originNext->sym()->_realWinding =
							e->sym()->_realWinding + e->sym()->_winding;

					if constexpr (TessVerbose != VerboseFlag::None) {
						std::cout << "\tright: " << e << " " << e->getDstVec() << " - "
								  << e->getOrgVec() << " - " << e->_originNext->getDstVec() << " = "
								  << e->_realWinding << "(" << e->sym()->_realWinding << "+"
								  << e->sym()->_winding << ")";
					}

					type = VertexType::Start;
					if (isWindingInside(_winding, e->_realWinding)) {
						if constexpr (TessVerbose != VerboseFlag::None) {
							std::cout << "; Start\n";
						}
						onVertex(VertexType::Start, fullEdge, e, e->_originNext);
					} else {
						if constexpr (TessVerbose != VerboseFlag::None) {
							std::cout << "\n";
						}
					}
				}
			} else {
				// right-to-left
				e->_realWinding = e->_originNext->sym()->_realWinding;

				if constexpr (TessVerbose != VerboseFlag::None) {
					std::cout << "\tright-to-left: " << e << " " << e->getDstVec() << " - "
							  << e->getOrgVec() << " - " << e->_originNext->getDstVec() << " = "
							  << e->_realWinding << "(" << e->_originNext->sym()->_realWinding
							  << ":" << e->_originNext->_realWinding << ")";
				}

				type = VertexType::RegularBottom;
				if (isWindingInside(_winding, e->_realWinding)) {
					if constexpr (TessVerbose != VerboseFlag::None) {
						std::cout << "; RegularBottom\n";
					}
					onVertex(VertexType::RegularBottom, fullEdge, e, e->_originNext);
				} else {
					if constexpr (TessVerbose != VerboseFlag::None) {
						std::cout << "\n";
					}
				}
			}

			// std::cout << "\t\tpush edge" << fullEdge->getLeftVec() << " - " << fullEdge->getRightVec()
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
					std::cout << "\tleft-to-right: " << e << " " << e->getDstVec() << " - "
							  << e->getOrgVec() << " - " << e->_originNext->getDstVec() << " = "
							  << e->_realWinding;
				}

				type = VertexType::RegularTop;
				if (isWindingInside(_winding, e->_realWinding)) {
					if constexpr (TessVerbose != VerboseFlag::None) {
						std::cout << "; RegularTop\n";
					}
					onVertex(VertexType::RegularTop, fullEdge, e, e->_originNext);
				} else {
					if constexpr (TessVerbose != VerboseFlag::None) {
						std::cout << "\n";
					}
				}

			} else {
				if (AngleIsConvex(e, e->_originNext)) {
					if constexpr (TessVerbose != VerboseFlag::None) {
						std::cout << "\tleft-convex: " << e << " " << e->getDstVec() << " - "
								  << e->getOrgVec() << " - " << e->_originNext->getDstVec() << " = "
								  << e->_realWinding;
					}

					type = VertexType::Merge;
					if (isWindingInside(_winding, e->_realWinding)) {
						if constexpr (TessVerbose != VerboseFlag::None) {
							std::cout << "; Merge\n";
						}
						onVertex(VertexType::Merge, fullEdge, e, e->_originNext);
					} else {
						if constexpr (TessVerbose != VerboseFlag::None) {
							std::cout << "\n";
						}
					}

				} else {
					if constexpr (TessVerbose != VerboseFlag::None) {
						std::cout << "\tleft: " << e << " " << e->getDstVec() << " - "
								  << e->getOrgVec() << " - " << e->_originNext->getDstVec() << " = "
								  << e->_realWinding;
					}

					type = VertexType::End;
					if (isWindingInside(_winding, e->_realWinding)) {
						if constexpr (TessVerbose != VerboseFlag::None) {
							std::cout << "; End\n";
						}
						onVertex(VertexType::End, fullEdge, e, e->_originNext);
					} else {
						if constexpr (TessVerbose != VerboseFlag::None) {
							std::cout << "\n";
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
			std::cout << "Intersect: " << edge1->org << " - " << edge1->dst() << "  X  "
					  << edge2->getOrgVec() << " - " << edge2->getDstVec() << " = " << intersect
					  << ": " << ev << "\n";
		} else {
			std::cout << "Intersect: " << edge1->org << " - " << edge1->dst() << "  X  "
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
			tmp->horizontal = std::abs(tmp->norm.x) > std::numeric_limits<float>::epsilon();
		} else {
			tmp->norm = dst - org;
			tmp->value.z = dst.x;
			tmp->value.w = dst.y;
			tmp->horizontal = std::abs(tmp->norm.x) > std::numeric_limits<float>::epsilon();
		}
	};

	auto checkRecursive = [&, this](HalfEdge *e) {
		if (auto node = _edgeDict->checkForIntersects(e, intersect, ev, _mathTolerance)) {
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
			std::cout << "\tVertex: " << *vertex << "\n";
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
		std::cout << "Intersect: " << edge1->org << " - " << edge1->dst() << "  X  " << v->_origin
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
			tmp->horizontal = std::abs(tmp->norm.x) > std::numeric_limits<float>::epsilon();
		} else {
			tmp->norm = dst - org;
			tmp->value.z = dst.x;
			tmp->value.w = dst.y;
			tmp->horizontal = std::abs(tmp->norm.x) > std::numeric_limits<float>::epsilon();
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

HalfEdge *Tesselator::Data::pushVertex(HalfEdge *e, const Vec2 &origin, bool clockwise,
		bool returnNew) {
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
			std::cout << "ERROR: connectEdges on same vertex:\n\t" << *eOrg << "\n\t"
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
		std::cout << "\t\t\tConnected: " << *eNew << "\n";
	}

	edge->updateInfo();

	return eNew;
}

Vertex *Tesselator::Data::splitEdge(HalfEdge *eOrg1, const Vec2 &vec) {
	Vertex *v = nullptr;

	if constexpr (TessVerbose != VerboseFlag::None) {
		std::cout << "SplitEdge:\n\t" << *eOrg1 << "\n";
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
		std::cout << "\t" << *eOrg1 << "\n\t" << *eNew << "\n";
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
	if (std::find(_protectedVertexes.begin(), _protectedVertexes.end(), org)
			!= _protectedVertexes.end()) {
		return true;
	}

	if (std::find(_protectedVertexes.begin(), _protectedVertexes.end(), merge)
			!= _protectedVertexes.end()) {
		return true;
	}

	if constexpr (TessVerbose != VerboseFlag::None) {
		std::cout << TessVerbose << "Merge:\n\t" << *org << "\n";
		org->foreach ([&](const HalfEdge &e) { std::cout << "\t\t" << e << "\n"; });

		std::cout << "\t" << *merge << "\n";
		merge->foreach ([&](const HalfEdge &e) { std::cout << "\t\t" << e << "\n"; });
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
				std::cout << "Merge next (auto):\n\t" << *eOrg << "\n\t" << *eMerge << "\n";
			}

			return insertNext(eOrg, eMerge);
		} else if (eMerge->_leftNext->sym() == eOrg) {
			if constexpr (TessVerbose != VerboseFlag::None) {
				std::cout << "Merge prev (auto):\n\t" << *eOrg << "\n\t" << *eMerge << "\n";
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
							std::cout << "Merge prev (direct):\n\t" << *eOrg << "\n\t" << *eMerge
									  << "\n";
						}

						insertNext(eOrg->getOriginPrev(), eMerge);
						return eOrg;
					} else {
						if constexpr (TessVerbose != VerboseFlag::None) {
							std::cout << "Merge next (direct):\n\t" << *eOrg << "\n\t" << *eMerge
									  << "\n";
						}

						return insertNext(eOrg, eMerge);
					}
				} else {
					if (VertLeq(eOrg->getDstVec(), eMerge->getDstVec())) {
						if constexpr (TessVerbose != VerboseFlag::None) {
							std::cout << "Merge next (reverse):\n\t" << *eOrg << "\n\t" << *eMerge
									  << "\n";
						}

						return insertNext(eOrg, eMerge);
					} else {
						if constexpr (TessVerbose != VerboseFlag::None) {
							std::cout << "Merge prev (reverse):\n\t" << *eOrg << "\n\t" << *eMerge
									  << "\n";
						}

						insertNext(eOrg->getOriginPrev(), eMerge);
						return eOrg;
					}
				}
			} else if (eOrgCcw) {
				if constexpr (TessVerbose != VerboseFlag::None) {
					std::cout << "Merge prev (ccw):\n\t" << *eOrg << "\n\t" << *eMerge << "\n";
				}

				auto r = insertNext(eOrg, eMerge);
				return r;
			} else {
				if constexpr (TessVerbose != VerboseFlag::None) {
					std::cout << "Merge next (ccw):\n\t" << *eOrg << "\n\t" << *eMerge << "\n";
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
	if (isnan(lA)) {
		return false;
	}

	// merge common edges, if any
	do {
		auto eMergeNext = eMerge->_originNext;

		if (eMerge->sym()->vertex == org->_uniqueIdx && eMergeNext->_originNext == eMerge) {
			org->_edge = removeEdge(eMerge);
			releaseVertex(merge);
			if constexpr (TessVerbose == VerboseFlag::Full) {
				std::cout << TessVerbose << "Out:\n\t" << *org << "\n";
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
				std::cout << "eMerge: " << *eMerge << "\n";
			}
			auto rA = EdgeAngle(eOrg->getNormVec(), eMerge->getNormVec());
			if (isnan(rA)) {
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
				if (isnan(lA)) {
					return false;
				}
				break;
			} else if (rA < lA) {
				if constexpr (TessVerbose != VerboseFlag::None) {
					std::cout << "Insert next:\n\t" << *eOrg << "\n\t" << *eMerge << "\n";
				}

				auto tmpOrg = insertNext(eOrg, eMerge);
				if (!Tesselator_checkConnectivity(org->_edge)) {
					log::source().error("geom::Tesselator", "Pizdets");
				}

				eMerge->origin = eOrg->origin;
				eMerge->vertex = eOrg->vertex;
				eOrg = tmpOrg;
				lA = EdgeAngle(eOrg->getNormVec(), eOrg->getOriginNext()->getNormVec());
				if (isnan(lA)) {
					return false;
				}
				break;
			} else {
				eOrg = eOrg->_originNext;
				lA = EdgeAngle(eOrg->getNormVec(), eOrg->getOriginNext()->getNormVec());
				if (isnan(lA)) {
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
			std::cout << TessVerbose << "\t\tRemoveEars: " << *eOrg << "\n";
		}

		auto eOrgNext = eOrg->_originNext;

		if (eOrg->_leftNext->sym() == eOrg->_originNext
				&& eOrg->_originNext->_leftNext->sym() == eOrg) {
			auto eOrgJoin = eOrgNext;

			if constexpr (TessVerbose != VerboseFlag::None) {
				std::cout << TessVerbose << "\t\t\t: " << *eOrg << "\n";
				std::cout << TessVerbose << "\t\t\t: " << *eOrgJoin << "\n";
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
		std::cout << "\tResult (pre): " << eOrg->vertex << "\n";
		org->foreach ([&](const HalfEdge &e) {
			std::cout << "\t\t" << e << "\n";
			if constexpr (TessVerbose == VerboseFlag::Full) {
				e.foreachOnFace([&](const HalfEdge &e) { std::cout << "\t\t\t" << e << "\n"; });
			}
		});
	}

	// process overlaps
	_protectedVertexes.emplace_back(org);


	bool overlapProcessed = false;
	while (!overlapProcessed) {
		eOrgEnd = eOrg = org->_edge;
		if constexpr (TessVerbose != VerboseFlag::None) {
			std::cout << "Start overlap processing: " << eOrg->vertex << " ("
					  << _protectedVertexes.size() << "): " << *eOrg << "\n";
		}

		do {
			auto eOrgNext = eOrg->_originNext;

			float a = EdgeAngle(eOrg->getNormVec(), eOrgNext->getNormVec());
			if (isnan(a)) {
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
					std::cout << "Remove loop: " << eOrg->vertex << " ("
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
						std::cout << "\tVertex " << eOrgSymOrgPrev->vertex << ": " << e << "\n";
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
		std::cout << "\tResult (post): " << eOrg->vertex << "\n";
		org->foreach ([&](const HalfEdge &e) {
			std::cout << "\t\t" << e << "\n";
			if constexpr (TessVerbose == VerboseFlag::Full) {
				e.foreachOnFace([&](const HalfEdge &e) { std::cout << "\t\t\t" << e << "\n"; });
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

		while (VertEq(e->getOrgVec(), e->getDstVec(), _mathTolerance)
				&& e->_leftNext->_leftNext != e) {
			if constexpr (TessVerbose != VerboseFlag::None) {
				std::cout << "Remove degenerate: " << *e << "\n";
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
	if (std::find(_protectedEdges.begin(), _protectedEdges.end(), e1) != _protectedEdges.end()) {
		return false;
	}

	if (std::find(_protectedEdges.begin(), _protectedEdges.end(), e2) != _protectedEdges.end()) {
		return false;
	}


	if constexpr (TessVerbose != VerboseFlag::None) {
		std::cout << "processEdgeOverlap:\n\t" << *e1 << "\n\t" << *e2 << "\n";
	}

	if (e1->goesLeft()) {
		if (!VertLeq(e2->getDstVec(), e1->getDstVec())) {
			std::swap(e1, e2); // split e2
		}
	} else {
		if (!VertLeq(e1->getDstVec(), e2->getDstVec())) {
			std::swap(e1, e2); // split e2
		}
	}

	Vertex *vMerge;
	if (!VertEq(e1->getDstVec(), e2->getDstVec(), _mathTolerance)) {
		vMerge = splitEdge(e2, e1->getDstVec());
	} else {
		vMerge = _vertexes[e2->sym()->vertex];
	}

	if constexpr (TessVerbose != VerboseFlag::None) {
		std::cout << "Overlap: " << *e2 << "\n";
	}

	auto vOrgIdx = e1->sym()->vertex;
	auto vOrg = _vertexes[vOrgIdx];

	_protectedEdges.emplace_back(e2->sym());
	_protectedEdges.emplace_back(e1->sym());

	bool result = false;

	do {
		if (vOrg != vMerge) {
			if (std::find(_protectedVertexes.begin(), _protectedVertexes.end(), vOrg)
					!= _protectedVertexes.end()) {
				break;
			}

			if (std::find(_protectedVertexes.begin(), _protectedVertexes.end(), vMerge)
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

		while (VertEq(e->_vertex, eLnext->_vertex, _mathTolerance) && e->_next->_next != e) {
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

		uint16_t minVertexes = static_cast<uint16_t>(std::floor(angle / M_PI_4));
		uint16_t vertexes = static_cast<uint16_t>(std::floor(length / 4.0f));

		auto perp = Vec2(v1 - v0).getPerp();
		perp.normalize();
		edge->_next->_displaced = v1 + perp * offsetValue;

		auto rperp = Vec2(v1 - v2).getRPerp();
		rperp.normalize();
		edge->_next->_rperp = v1 + rperp * offsetValue;

		edge->_next->_nextra = std::max(minVertexes, vertexes);
		edge->_next->_value = 0.0f;
		edge->_next->_angle = angle;
	} else {
		if (std::isnan(result.y) || result.y > 3.0f) {
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
			sprt_passert(!std::isnan(edge->_next->_vertex->_origin.x)
							&& !std::isnan(edge->_next->_vertex->_origin.y),
					"Tess: displaced vertex is NaN");
		}
	}
}

} // namespace stappler::geom
