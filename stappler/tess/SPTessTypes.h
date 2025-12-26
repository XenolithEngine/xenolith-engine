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

#include "SPVec4.h"
#include "SPTess.h"

namespace STAPPLER_VERSIONIZED stappler::geom {

struct Vertex;
struct FaceEdge;
struct HalfEdge;
struct Edge;

using QueueHandle = int32_t;

static constexpr uint32_t VertexSetPrealloc = 64;
static constexpr uint32_t EdgeSetPrealloc = 64;
static constexpr uint32_t VertexAllocBatch = 32;
static constexpr uint32_t EdgeAllocBatch = 32;

extern int TessVerboseInfo;

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

	mutable Helper helper;

	Vec2 current() const { return Vec2(value.x, value.y); }
	Vec2 dst() const { return Vec2(value.z, value.w); }
	float dstX() const { return value.z; }
	float dstY() const { return value.w; }

	bool operator < (const EdgeDictNode &other) const;
	bool operator < (const Edge &other) const;
	bool operator < (const Vec2 &other) const;
	bool operator <= (const EdgeDictNode &other) const;
	bool operator== (const EdgeDictNode &other) const;
};

struct Vertex {
	HalfEdge *_edge = nullptr;  /* a half-edge with this origin */
	Vec2 _origin;
	Vec2 _norm;
	uint32_t _uniqueIdx = maxOf<uint32_t>(); /* to allow identify unique vertices */
	QueueHandle _queueHandle = maxOf<QueueHandle>(); /* to allow deletion from priority queue */
	uint32_t _exportIdx = maxOf<uint32_t>();

	void insertBefore(HalfEdge *eOrig);
	void removeFromList(Vertex *newOrg);

	void foreach(const Callback<void(const HalfEdge &)> &) const;

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

	void foreach(const Callback<void(const FaceEdge &)> &) const;
};

struct HalfEdge {
	HalfEdge *_originNext = nullptr; /* next edge CCW around origin */
	HalfEdge *_leftNext = nullptr; /* next edge CCW around left face */
	Vec2 origin;
	uint32_t vertex = maxOf<uint32_t>(); // normally, we should not access vertex directly to improve data locality
	int16_t _realWinding = 0;
	int16_t isRight : 2 = 0; // -1 or 1
	int16_t edgeOffset : 2 = 0; // 0 or 1
	int16_t _winding : 2 = 0; /* change in winding number when crossing from the right face to the left face */
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

	memory::vector<Vertex *> _vertexes;
	memory::vector<Vertex *> _exportVertexes;
	memory::vector<HalfEdge *> _edgesOfInterests;
	memory::vector<HalfEdge *> _faceEdges;

	memory::vector<FaceEdge *> _boundaries;

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

	void removeEdgeFromVec(memory::vector<HalfEdge *> &, HalfEdge *);
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

	VertexPriorityQueue(memory::pool_t *, const memory::vector<Vertex *> &);
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

struct EdgeDict {
	Vec2 event;

	memory::set<EdgeDictNode> nodes;
	memory::pool_t *pool = nullptr;

	EdgeDict(memory::pool_t *, uint32_t size);

	const EdgeDictNode * push(Edge *, int16_t windingAbove);
	void pop(const EdgeDictNode *);

	void update(Vertex *, float tolerance);

	const EdgeDictNode * checkForIntersects(Vertex *, Vec2 &, IntersectionEvent &, float tolerance) const;
	const EdgeDictNode * checkForIntersects(HalfEdge *, Vec2 &, IntersectionEvent &, float tolerance) const;

	// find edge directly below edge (used for region winding)
	const EdgeDictNode * getEdgeBelow(const Edge *) const;

	// find edge directly below point (used for monotonize algo)
	const EdgeDictNode * getEdgeBelow(const Vec2 &, uint32_t vertex) const;
};

SP_ATTR_OPTIMIZE_INLINE_FN static inline bool VertLeq(const Vec2 &u, const Vec2 &v) {
	return ((u.x < v.x) || (u.x == v.x && u.y <= v.y));
}

SP_ATTR_OPTIMIZE_INLINE_FN static inline bool VertLeq(const Vertex *u, const Vertex *v) {
	return ((u->_origin.x < v->_origin.x) || (u->_origin.x == v->_origin.x && u->_origin.y <= v->_origin.y));
}

SP_ATTR_OPTIMIZE_INLINE_FN static inline bool VertEq(const Vec2 &u, const Vec2 &v, float tolerance) {
	return u.fuzzyEquals(v, tolerance);
}

SP_ATTR_OPTIMIZE_INLINE_FN static inline bool FloatEq(float u, float v, float tolerance) {
	return u - tolerance <= v && v <= u + tolerance;
}

SP_ATTR_OPTIMIZE_INLINE_FN static inline bool VertEq(const Vertex *u, const Vertex *v, float tolerance) {
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

	if (std::isnan(fromA) || std::isnan(toA)) {
		std::cerr << "EdgeAngle (NaN): " << from << " " << to << "\n";
		return nan();
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

std::ostream &operator<<(std::ostream &out, const Vertex &v);
std::ostream &operator<<(std::ostream &out, const HalfEdge &e);
std::ostream &operator<<(std::ostream &out, const FaceEdge &e);
std::ostream &operator<<(std::ostream &out, VerboseFlag e);
std::ostream &operator<<(std::ostream &out, const EdgeDictNode &e);
std::ostream &operator<<(std::ostream &out, const Edge &e);

inline std::ostream &
operator << (std::ostream & os, const IntersectionEvent & ev) {
	switch (ev) {
	case IntersectionEvent::Regular: os << "Regular"; break;
	case IntersectionEvent::EventIsIntersection: os << "EventIsIntersection"; break;
	case IntersectionEvent::EdgeConnection1: os << "EdgeConnection1"; break;
	case IntersectionEvent::EdgeConnection2: os << "EdgeConnection2"; break;
	case IntersectionEvent::Merge: os << "Merge"; break;
	}
	return os;
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

}

#endif /* STAPPLER_TESS_SPTESSTYPES_H_ */
