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

#ifndef STAPPLER_TESS_SPTESSTYPES_H_
#define STAPPLER_TESS_SPTESSTYPES_H_

#include <sprt/runtime/geom/vec4.h>
#include <sprt/runtime/log.h>

#include "SPTess.h"

namespace STAPPLER_VERSIONIZED stappler::geom {

using sprt::geom::Vec4;

struct Vertex;
struct FaceEdge;
struct HalfEdge;
struct Edge;

using QueueHandle = int32_t;

static constexpr uint32_t VertexSetPrealloc = 64;
static constexpr uint32_t EdgeSetPrealloc = 64;
static constexpr uint32_t VertexAllocBatch = 32;
static constexpr uint32_t EdgeAllocBatch = 32;

extern VerboseFlag TessVerboseInfo;

enum class VertexType {
	Start, // right non-convex angle
	End, // left non-convex angle
	Split, // right convex angle
	Merge, // left convex angle
	RegularTop, // boundary below vertex
	RegularBottom, // boundary above vertex
};

struct Helper {
	HalfEdge *e1 = nullptr;
	HalfEdge *e2 = nullptr;
	VertexType type = VertexType::Start;
};

struct EdgeDictNode {
	Vec2 org;
	Vec2 norm;
	mutable Vec4 value; // value, dst
	Edge *edge = nullptr;
	int16_t windingAbove = 0;
	bool horizontal = false;

	/* A tombstone: this entry is out of the dictionary but still occupies its place.
	
	A FLAG rather than `edge = nullptr`, and the distinction is not cosmetic. The structure this
	replaced erased the node from a set, which left the node object itself untouched in the pool -
	so a pointer taken before the erase still read a live `edge` afterwards, and the algorithm
	relies on that: `processIntersect` ends with `edge1->edge ? ... : nullptr`, and `edge1` is
	routinely a node the merge just popped. Nulling `edge` turned that into a failure - two hundred
	and twenty crossing wires stopped tesselating - while the old code read the stale pointer and
	carried on. Marking dead without touching `edge` keeps that behaviour exactly. */
	bool dead = false;

	mutable Helper helper;

	// Which event `value` was last computed for. See EdgeDict::refresh: the crossing of the
	// sweepline is worked out when somebody looks at it, not for everybody on every event.
	mutable uint32_t stamp = 0;

	Vec2 current() const { return Vec2(value.x, value.y); }
	Vec2 dst() const { return Vec2(value.z, value.w); }
	float dstX() const { return value.z; }
	float dstY() const { return value.w; }

	bool operator<(const EdgeDictNode &other) const;
	bool operator<(const Edge &other) const;
	bool operator<(const Vec2 &other) const;
	bool operator<=(const EdgeDictNode &other) const;
	bool operator==(const EdgeDictNode &other) const;
};

struct Vertex {
	HalfEdge *_edge = nullptr; /* a half-edge with this origin */
	Vec2 _origin;
	Vec2 _norm;
	uint32_t _uniqueIdx = maxOf<uint32_t>(); /* to allow identify unique vertices */
	QueueHandle _queueHandle = maxOf<QueueHandle>(); /* to allow deletion from priority queue */
	uint32_t _exportIdx = maxOf<uint32_t>();

	void insertBefore(HalfEdge *eOrig);
	void removeFromList(Vertex *newOrg);

	void foreach (const Callback<void(const HalfEdge &)> &) const;

	void relocate(const Vec2 &);
};

struct FaceEdge : memory::AllocPool {
	FaceEdge *_next = nullptr;
	Vertex *_vertex = nullptr;
	Vec2 _origin;
	Vec2 _displaced;
	Vec2 _rperp; // secondary boundary vertex for DF
	Vec2 _norm; // edge negative (pointing into object) normal direction
	float _value = 0.0f;
	float _direction = 0.0f;
	float _angle = 0.0f;
	uint16_t _nextra = 0;
	bool _splitVertex = false;
	bool _degenerate = false;

	void foreach (const Callback<void(const FaceEdge &)> &) const;
};

struct HalfEdge {
	HalfEdge *_originNext = nullptr; /* next edge CCW around origin */
	HalfEdge *_leftNext = nullptr; /* next edge CCW around left face */
	Vec2 origin;
	uint32_t vertex = maxOf<
			uint32_t>(); // normally, we should not access vertex directly to improve data locality
	int16_t _realWinding = 0;
	int16_t isRight	   : 2 = 0; // -1 or 1
	int16_t edgeOffset : 2 = 0; // 0 or 1
	int16_t _winding   : 2 =
			0; /* change in winding number when crossing from the right face to the left face */
	int16_t _mark : 10 = 0;

	static void splitEdgeLoops(HalfEdge *eOrg, HalfEdge *eNew, Vertex *v);
	static void joinEdgeLoops(HalfEdge *eOrg, HalfEdge *oPrev);

	HalfEdge *sym() const; // uses `this` pointer and isLeft to find Sym edge

	uint32_t getIndex() const;

	void setOrigin(const Vertex *);
	void copyOrigin(const HalfEdge *);

	HalfEdge *getOriginNext() const;
	HalfEdge *getOriginPrev() const;

	HalfEdge *getDestinationNext() const;
	HalfEdge *getDestinationPrev() const;

	HalfEdge *getLeftLoopNext() const;
	HalfEdge *getLeftLoopPrev() const;

	HalfEdge *getRightLoopNext() const;
	HalfEdge *getRightLoopPrev() const;

	const Vec2 &getOrgVec() const;
	const Vec2 &getDstVec() const;
	Vec2 getNormVec() const { return getDstVec() - getOrgVec(); }

	float getLength() const;

	Edge *getEdge() const;

	// edge info should be updated
	bool goesLeft() const; // right edge goes left
	bool goesRight() const; // left edge goes right

	void foreachOnFace(const Callback<void(HalfEdge &)> &);
	void foreachOnVertex(const Callback<void(HalfEdge &)> &);

	void foreachOnFace(const Callback<void(const HalfEdge &)> &) const;
	void foreachOnVertex(const Callback<void(const HalfEdge &)> &) const;

	float getDirection() const;
};

struct Edge {
	HalfEdge left;
	HalfEdge right;
	const EdgeDictNode *node = nullptr;
	float direction = nan();
	bool inverted = false; // inverted means left edge goes right
	bool invalidated = false;

	Edge();

	const Vec2 &getLeftVec() const;
	const Vec2 &getRightVec() const;

	const Vec2 &getOrgVec() const;
	const Vec2 &getDstVec() const;

	uint32_t getLeftOrg() const;
	uint32_t getRightOrg() const;

	void updateInfo();

	int16_t getLeftWinding() const;
	int16_t getRightWinding() const;

	// halfedge in positive sweep direction
	HalfEdge *getPostitive() { return (inverted) ? &right : &left; }

	// halfedge in negative sweep direction
	HalfEdge *getNegative() { return (inverted) ? &left : &right; }
};

struct ObjectAllocator : public memory::AllocPool {
	memory::pool_t *_pool = nullptr;

	Vertex *_freeVertexes = nullptr;
	Edge *_freeEdges = nullptr;
	FaceEdge *_freeFaces = nullptr;

	sprt::__pool_vector<Vertex *> _vertexes;
	sprt::__pool_vector<Vertex *> _exportVertexes;
	sprt::__pool_vector<HalfEdge *> _edgesOfInterests;
	sprt::__pool_vector<HalfEdge *> _faceEdges;

	sprt::__pool_vector<FaceEdge *> _boundaries;

	uint32_t _vertexOffset = 0;

	ObjectAllocator(memory::pool_t *pool);

	Edge *allocEdge();
	Vertex *allocVertex();
	FaceEdge *allocFaceEdge();

	void releaseEdge(Edge *);
	void releaseVertex(uint32_t, uint32_t);
	void releaseVertex(Vertex *);
	void trimVertexes();

	void preallocateVertexes(uint32_t n);
	void preallocateEdges(uint32_t n);
	void preallocateFaceEdges(uint32_t n);

	void removeEdgeFromVec(sprt::__pool_vector<HalfEdge *> &, HalfEdge *);
};

struct VertexPriorityQueue {
	using Handle = QueueHandle;
	using Key = Vertex *;

	static constexpr Handle InvalidHandle = maxOf<Handle>();

	struct Node {
		Handle handle;
	};

	struct Elem {
		Key key;
		Handle node;
	};

	struct Heap {
		Node *nodes = nullptr;
		Elem *handles = nullptr;
		uint32_t size = 0, max = 0;
		Handle freeList = 0;
		bool initialized = false;
		memory::pool_t *pool;

		Heap(memory::pool_t *, uint32_t);
		~Heap();

		void init();
		bool empty() const { return size == 0; }
		Key getMin() const { return handles[nodes[1].handle].key; }

		Handle insert(Key keyNew);
		Key extractMin();
		void remove(Handle hCurr);

		void floatDown(int curr);
		void floatUp(int curr);
	};

	Heap heap;
	Key *keys = nullptr;
	Key **order = nullptr;
	uint32_t size = 0, max = 0;
	bool initialized = false;
	memory::pool_t *pool = nullptr;
	Handle freeList = 0;

	VertexPriorityQueue(memory::pool_t *, const sprt::__pool_vector<Vertex *> &);
	~VertexPriorityQueue();

	bool init();

	bool empty() const;

	Handle insert(Key);
	void remove(Handle);

	Key extractMin();
	Key getMin() const;
};

enum class IntersectionEvent {
	Regular,
	EventIsIntersection, // intersection directly on event point, new edge should split old one
	EdgeConnection1, // connection, ends on old edge
	EdgeConnection2, // connection, ends on new edge
	Merge, // both edges ends in same place
};

/* The sweepline's status: every edge the sweep is currently between the ends of, in vertical order.

A FLAT ARRAY OF POINTERS, not a tree, and the shape is what the measurement asked for. `update`
walks the whole structure on every event - a million events by a few hundred open edges - and at
six nanoseconds a node that was a cache miss per node, not arithmetic: replacing the division in
that loop with a multiply moved it four percent. A red-black tree pays three pointers and a
rebalance per node; an array pays a prefetch.

STABLE NODES. `Edge::node` holds a pointer into here and is dereferenced long after it was taken,
so a node may never move. The nodes are allocated one at a time from the pool and only their
ADDRESSES live in the array - so reordering the array costs a memmove of eight-byte words and
invalidates nothing.

TOMBSTONES. A retiring edge is marked `dead` and stays where it is: deletion moves nothing.
Every search skips them, and `update` - which already walks everything - compacts them out as it
goes, so the removal costs nothing on top of a pass that exists anyway.

That leaves one question, and it is the reason tombstones are safe here: does a dead entry break
the ordering the searches binary-search on? It does not. Values are recomputed for every node in
one place, `update`, once per event - so every value in the array, live or dead, was computed at
the same sweep position, and a tombstone sits exactly where it sat when it died. The array stays
sorted; the dead are simply not answers.

Slots freed by a compaction are reused by the NEXT event's pushes, never the same one: a node
handed out during an event is dereferenced during that event, and handing the same address to a
second edge before it is done would be an aliasing bug that no test could be relied on to find. */
struct EdgeDict {
	Vec2 event;

	// Sweep order. Entries marked `dead` are tombstones - see above.
	sprt::__pool_vector<EdgeDictNode *> nodes;

	// Compacted out by `update`, handed back out by `push` from the next event onward.
	sprt::__pool_vector<EdgeDictNode *> freeNodes;

	memory::pool_t *pool = nullptr;

	// Bumped once per event; a node whose stamp differs holds a value from an older sweep
	// position. Starts at one so a freshly allocated node (stamp zero) is always stale.
	uint32_t serial = 1;
	uint32_t eventVertex = 0;

	EdgeDict(memory::pool_t *, uint32_t size);

	const EdgeDictNode *push(Edge *, int16_t windingAbove);
	void pop(const EdgeDictNode *);

	/* Brings one node's crossing of the sweepline up to the current event, if it is not already.
	
	`update` used to do this for EVERY open edge on every event - a few hundred nodes times a
	million events - and the overwhelming majority of those values were never read: after the
	neighbour window, one event looks at a couple of dozen entries. So the work moved to the read.
	
	What made that possible is that the RETIREMENT test, which is why `update` had to touch every
	node in the first place, turns out not to need the value at all: `t = (event.x - org.x) /
	norm.x` lies in [0, 1] exactly when `event.x` lies between `org.x` and `dst.x`, which is two
	comparisons and no division. */
	void refresh(const EdgeDictNode &) const;

	// Index of the first entry whose value is not below `y`, tombstones included: they carry the
	// value they died with, which is from the same update as everyone else's.
	size_t lowerBound(float y) const;

	EdgeDictNode *acquireNode();

	void update(Vertex *, float tolerance);

	const EdgeDictNode *checkForIntersects(Vertex *, Vec2 &, IntersectionEvent &,
			float tolerance) const;
	const EdgeDictNode *checkForIntersects(HalfEdge *, Vec2 &, IntersectionEvent &,
			float tolerance) const;

	// find edge directly below edge (used for region winding)
	const EdgeDictNode *getEdgeBelow(const Edge *) const;

	// find edge directly below point (used for monotonize algo)
	const EdgeDictNode *getEdgeBelow(const Vec2 &, uint32_t vertex) const;
};

SP_ATTR_OPTIMIZE_INLINE_FN static inline bool VertLeq(const Vec2 &u, const Vec2 &v) {
	return ((u.x < v.x) || (u.x == v.x && u.y <= v.y));
}

SP_ATTR_OPTIMIZE_INLINE_FN static inline bool VertLeq(const Vertex *u, const Vertex *v) {
	return ((u->_origin.x < v->_origin.x)
			|| (u->_origin.x == v->_origin.x && u->_origin.y <= v->_origin.y));
}

SP_ATTR_OPTIMIZE_INLINE_FN static inline bool VertEq(const Vec2 &u, const Vec2 &v,
		float tolerance) {
	return u.fuzzyEquals(v, tolerance);
}

SP_ATTR_OPTIMIZE_INLINE_FN static inline bool FloatEq(float u, float v, float tolerance) {
	return u - tolerance <= v && v <= u + tolerance;
}

SP_ATTR_OPTIMIZE_INLINE_FN static inline bool VertEq(const Vertex *u, const Vertex *v,
		float tolerance) {
	return VertEq(u->_origin, v->_origin, tolerance);
}

SP_ATTR_OPTIMIZE_INLINE_FN static inline bool EdgeGoesRight(const HalfEdge *e) {
	return VertLeq(e->origin, e->sym()->origin);
}

SP_ATTR_OPTIMIZE_INLINE_FN static inline bool EdgeGoesLeft(const HalfEdge *e) {
	return !VertLeq(e->origin, e->sym()->origin);
}

SP_ATTR_OPTIMIZE_INLINE_FN static inline bool AngleIsConvex(const HalfEdge *a, const HalfEdge *b) {
	return a->getEdge()->direction > b->getEdge()->direction;
}


// fast synthetic tg|ctg function, returns range [-2.0, 2.0f]
// which monotonically grows with angle between vec and 0x as argument;
// norm.x assumed to be positive
SP_ATTR_OPTIMIZE_INLINE_FN static inline float EdgeDirection(const Vec2 &norm) {
	if (norm.y >= 0) {
		return (norm.x > norm.y) ? (norm.y / norm.x) : (2.0f - norm.x / norm.y);
	} else {
		return (norm.x > -norm.y) ? (norm.y / norm.x) : (-2.0f - norm.x / norm.y);
	}
}

// same method, map full angle with positive x axis to [0.0f, 8.0f)
SP_ATTR_OPTIMIZE_INLINE_FN static inline float EdgeAngle(const Vec2 &norm) {
	if (norm.x >= 0 && norm.y >= 0) {
		// [0.0, 2.0]
		return (norm.x > norm.y) ? (norm.y / norm.x) : (2.0f - norm.x / norm.y);
	} else if (norm.x < 0 && norm.y >= 0) {
		// (2.0, 4.0]
		return (-norm.x > norm.y) ? (4.0 + norm.y / norm.x) : (2.0f - norm.x / norm.y);
	} else if (norm.x < 0 && norm.y < 0) {
		// (4.0, 6.0)
		return (norm.x < norm.y) ? (4.0 + norm.y / norm.x) : (6.0f - norm.x / norm.y);
	} else {
		// [6.0, 8.0)
		return (norm.x > -norm.y) ? (8.0 + norm.y / norm.x) : (6.0f - norm.x / norm.y);
	}
}

SP_ATTR_OPTIMIZE_INLINE_FN static inline float EdgeAngle(const Vec2 &from, const Vec2 &to) {
	if (from == to) {
		return 8.0f;
	}

	auto fromA = EdgeAngle(from);
	auto toA = EdgeAngle(to);

	if (sprt::isnan(fromA) || sprt::isnan(toA)) {
		sprt::oslog::vperror(__SPRT_LOCATION, "Tess", "EdgeAngle (NaN): ", from, " ", to);
		return sprt::NaN<float>;
	}

	if (fromA <= toA) {
		return toA - fromA;
	} else {
		return 8.0 - (fromA - toA);
	}
}

SP_ATTR_OPTIMIZE_INLINE_FN static inline bool EdgeAngleIsBelowTolerance(float A, float tolerance) {
	return A < tolerance || 8.0f - A < tolerance;
}

inline bool isWindingInside(Winding w, int16_t n) {
	switch (w) {
	case Winding::EvenOdd: return (n & 1); break;
	case Winding::NonZero: return (n != 0); break;
	case Winding::Positive: return (n > 0); break;
	case Winding::Negative: return (n < 0); break;
	case Winding::AbsGeqTwo: return (n >= 2) || (n <= -2); break;
	}
	return false;
}

} // namespace stappler::geom

namespace sprt {

template <>
struct io_traits<STAPPLER_VERSIONIZED_NAMESPACE::geom::IntersectionEvent> {
	using IntersectionEvent = STAPPLER_VERSIONIZED_NAMESPACE::geom::IntersectionEvent;

	template <io_character CharType>
	static void encode(const callback<void(StringViewBase<CharType>)> &os,
			const IntersectionEvent &ev) {
		switch (ev) {
		case IntersectionEvent::Regular: os << "Regular"; break;
		case IntersectionEvent::EventIsIntersection: os << "EventIsIntersection"; break;
		case IntersectionEvent::EdgeConnection1: os << "EdgeConnection1"; break;
		case IntersectionEvent::EdgeConnection2: os << "EdgeConnection2"; break;
		case IntersectionEvent::Merge: os << "Merge"; break;
		}
	}
};

template <>
struct io_traits<STAPPLER_VERSIONIZED_NAMESPACE::geom::Vertex> {
	using VerboseFlag = STAPPLER_VERSIONIZED_NAMESPACE::geom::VerboseFlag;
	using Vertex = STAPPLER_VERSIONIZED_NAMESPACE::geom::Vertex;
	using HalfEdge = STAPPLER_VERSIONIZED_NAMESPACE::geom::HalfEdge;

	template <io_character CharType>
	static void encode(const callback<void(StringViewBase<CharType>)> &out, const Vertex &v) {
		switch (STAPPLER_VERSIONIZED_NAMESPACE::geom::TessVerboseInfo) {
		case VerboseFlag::None: out << "Vertex (" << v._uniqueIdx << ") : " << v._origin; break;
		case VerboseFlag::General: out << "Vertex (" << v._uniqueIdx << ") : " << v._origin; break;
		case VerboseFlag::Full:
			out << "Vertex (" << v._uniqueIdx << ") : " << v._origin << "\n";
			v.foreach ([&](const HalfEdge &e) {
				auto orgVec = e.origin;
				auto dstVec = e.sym()->origin;
				uint32_t orgIdx = e.vertex;
				uint32_t dstIdx = e.sym()->vertex;

				out << "\tEdge (" << e.getIndex() << ":" << e.sym()->getIndex() << ") : " << orgVec
					<< " - " << dstVec << "\n";
				out << "\t\tDir: (" << e.getIndex() << "; org: " << orgIdx
					<< "; left: " << e._leftNext->getIndex()
					<< "; ccw: " << e._originNext->getIndex() << ")\n";
				out << "\t\tSym: (" << e.sym()->getIndex() << "; org: " << dstIdx
					<< "; left: " << e.sym()->_leftNext->getIndex()
					<< "; ccw: " << e.sym()->_originNext->getIndex() << ")\n";
			});
			break;
		}
	}
};

template <>
struct io_traits<STAPPLER_VERSIONIZED_NAMESPACE::geom::HalfEdge> {
	using VerboseFlag = STAPPLER_VERSIONIZED_NAMESPACE::geom::VerboseFlag;
	using Vertex = STAPPLER_VERSIONIZED_NAMESPACE::geom::Vertex;
	using HalfEdge = STAPPLER_VERSIONIZED_NAMESPACE::geom::HalfEdge;

	template <io_character CharType>
	static void encode(const callback<void(StringViewBase<CharType>)> &out, const HalfEdge &e) {

		auto orgVec = e.origin;
		auto dstVec = e.sym()->origin;
		uint32_t orgIdx = e.vertex;
		uint32_t dstIdx = e.sym()->vertex;

		switch (STAPPLER_VERSIONIZED_NAMESPACE::geom::TessVerboseInfo) {
		case VerboseFlag::None:
			out << "Edge (" << e.getIndex() << ":" << e.sym()->getIndex() << ") : " << orgVec
				<< " - " << dstVec << "; " << e.vertex << " - " << e.sym()->vertex;
			break;
		case VerboseFlag::General:
			out << "Edge (" << e.getIndex() << ":" << e.sym()->getIndex() << ") : " << orgVec
				<< " - " << dstVec << "; " << e.vertex << " - " << e.sym()->vertex
				<< " winding: " << e._realWinding << ":" << e._winding << ";";
			if (e.goesLeft()) {
				out << " goes left; ";
			} else if (e.goesRight()) {
				out << " goes right; ";
			} else {
				out << " unknown direction; ";
			}
			out << (void *)&e;
			break;
		case VerboseFlag::Full:
			out << "Edge (" << e.getIndex() << ":" << e.sym()->getIndex() << ") : " << orgVec
				<< " - " << dstVec << "; " << e.vertex << " - " << e.sym()->vertex
				<< " winding: " << e._realWinding << ":" << e._winding << ";\n";
			out << "\tDir: (" << e.getIndex() << "; org: " << orgIdx
				<< "; left: " << e._leftNext->getIndex() << "; ccw: " << e._originNext->getIndex()
				<< ")";
			if (e.goesLeft()) {
				out << " goes left;";
			} else if (e.goesRight()) {
				out << " goes right;";
			} else {
				out << " unknown direction;";
			}
			out << "\n";
			out << "\tSym: (" << e.sym()->getIndex() << "; org: " << dstIdx
				<< "; left: " << e.sym()->_leftNext->getIndex()
				<< "; ccw: " << e.sym()->_originNext->getIndex() << ")";
			if (e.sym()->goesLeft()) {
				out << " goes left; ";
			} else if (e.sym()->goesRight()) {
				out << " goes right; ";
			} else {
				out << " unknown direction; ";
			}
			out << (void *)&e << "\n";
			break;
		}
	}
};

template <>
struct io_traits<STAPPLER_VERSIONIZED_NAMESPACE::geom::FaceEdge> {
	using VerboseFlag = STAPPLER_VERSIONIZED_NAMESPACE::geom::VerboseFlag;
	using FaceEdge = STAPPLER_VERSIONIZED_NAMESPACE::geom::FaceEdge;

	template <io_character CharType>
	static void encode(const callback<void(StringViewBase<CharType>)> &out, const FaceEdge &e) {
		auto orgVec = e._vertex->_origin;
		auto dstVec = e._next->_vertex->_origin;
		uint32_t orgIdx = e._vertex->_uniqueIdx;
		uint32_t dstIdx = e._next->_vertex->_uniqueIdx;
		out << "FaceEdge (" << orgIdx << " - " << dstIdx << ") : " << orgVec << " - " << dstVec
			<< ";";
	}
};

template <>
struct io_traits<STAPPLER_VERSIONIZED_NAMESPACE::geom::EdgeDictNode> {
	using VerboseFlag = STAPPLER_VERSIONIZED_NAMESPACE::geom::VerboseFlag;
	using EdgeDictNode = STAPPLER_VERSIONIZED_NAMESPACE::geom::EdgeDictNode;

	template <io_character CharType>
	static void encode(const callback<void(StringViewBase<CharType>)> &out, const EdgeDictNode &e) {
		out << "EdgeDictNode(" << e.org << "; " << e.dst() << "; cur: " << e.current() << ");";
	}
};

template <>
struct io_traits<STAPPLER_VERSIONIZED_NAMESPACE::geom::Edge> {
	using VerboseFlag = STAPPLER_VERSIONIZED_NAMESPACE::geom::VerboseFlag;
	using Edge = STAPPLER_VERSIONIZED_NAMESPACE::geom::Edge;

	template <io_character CharType>
	static void encode(const callback<void(StringViewBase<CharType>)> &out, const Edge &e) {
		if (e.inverted) {
			out << e.right;
		} else {
			out << e.left;
		}
	}
};

} // namespace sprt

#endif /* STAPPLER_TESS_SPTESSTYPES_H_ */
