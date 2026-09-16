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

#include "SPTessTypes.h"
#include "SPLog.h"

namespace STAPPLER_VERSIONIZED stappler::geom {

static constexpr VerboseFlag TessTypesVerbose = VerboseFlag::None;
static constexpr bool IntersectDebug = false;
static constexpr bool DictDebug = false;

bool EdgeDictNode::operator<(const EdgeDictNode &other) const {
	if (value.y == other.value.y) {
		return edge->direction < other.edge->direction;
	} else {
		return value.y < other.value.y;
	}
}

bool EdgeDictNode::operator<(const Edge &other) const {
	auto &left = other.getLeftVec();
	if (value.y == left.y) {
		return edge->direction < other.direction; // dst.y
	} else {
		return value.y < left.y;
	}
}

bool EdgeDictNode::operator<(const Vec2 &other) const { return value.y < other.y; }

bool EdgeDictNode::operator<=(const EdgeDictNode &other) const {
	if (value.y == other.value.y) {
		return value.w == other.value.w || edge->direction < other.edge->direction; // dst.y
	} else {
		return value.y < other.value.y;
	}
}

bool EdgeDictNode::operator==(const EdgeDictNode &other) const {
	return value.y == other.value.y && value.w == other.value.w;
}

void Vertex::insertBefore(HalfEdge *eOrig) {
	_edge = eOrig;

	/* fix other edges on this vertex loop */
	HalfEdge *e = eOrig;
	do {
		e->setOrigin(this);
		e = e->_originNext;
	} while (e != eOrig);
}

void Vertex::removeFromList(Vertex *newOrg) {
	HalfEdge *e, *eStart = _edge;
	e = eStart;
	do {
		e->setOrigin(newOrg);
		e = e->_originNext;
	} while (e != eStart);
}

void Vertex::foreach (const Callback<void(const HalfEdge &)> &cb) const {
	auto e = _edge;
	do {
		cb(*e);
		e = e->_originNext;
	} while (e != _edge);
}

void Vertex::relocate(const Vec2 &vec) {
	_origin = vec;
	auto e = _edge;
	do {
		e->origin = vec;
		e = e->_originNext;
	} while (e != _edge);
}

void FaceEdge::foreach (const Callback<void(const FaceEdge &)> &cb) const {
	auto e = this;
	do {
		cb(*e);
		e = e->_next;
	} while (e != this);
}

void HalfEdge::splitEdgeLoops(HalfEdge *eOrg, HalfEdge *eNew, Vertex *v) {
	eNew->sym()->copyOrigin(eOrg->sym());
	eOrg->sym()->setOrigin(v);
	eNew->setOrigin(v);

	HalfEdge *a = eOrg, *b = eOrg->sym(), // original edge
								*c = eNew, *d = eNew->sym(), // new edge
												   *e = eOrg->_leftNext, // next edge in left loop
														   *g = b->_originNext,
			 *h = g->sym(); // prev edge in right loop

	e->_originNext = d;
	d->_originNext = g; // vertex cycle around dest vertex;
	c->_originNext = b;
	b->_originNext = c; // cycle around new vertex;
	a->_leftNext = c;
	c->_leftNext = e; // left face loop
	h->_leftNext = d;
	d->_leftNext = b; // right face loop
	c->_winding = a->_winding;
	d->_winding = b->_winding;
	c->_realWinding = a->_realWinding;
	d->_realWinding = b->_realWinding;
}

void HalfEdge::joinEdgeLoops(HalfEdge *eOrg, HalfEdge *oPrev) {
	// connect eOrg into vertex
	HalfEdge *a = eOrg, *b = eOrg->sym(), // original edge
								*e = oPrev, // next edge in left loop
										*g = oPrev->_originNext,
			 *h = g->sym(); // prev edge in right loop

	e->_originNext = b;
	b->_originNext = g; // cycle around new vertex;
	a->_leftNext = e;
	h->_leftNext = b; // left and right loops
}

HalfEdge *HalfEdge::sym() const { return (HalfEdge *)((char *)this - sizeof(HalfEdge) * isRight); }

uint32_t HalfEdge::getIndex() const { return ((uintptr_t)this >> 5) % 1'024; }

void HalfEdge::setOrigin(const Vertex *v) {
	origin = v->_origin;
	vertex = v->_uniqueIdx;
}

void HalfEdge::copyOrigin(const HalfEdge *e) {
	origin = e->origin;
	vertex = e->vertex;
}

HalfEdge *HalfEdge::getOriginNext() const { return _originNext; }

HalfEdge *HalfEdge::getOriginPrev() const { return sym()->_leftNext; }

HalfEdge *HalfEdge::getDestinationNext() const { return sym()->_originNext->sym(); }

HalfEdge *HalfEdge::getDestinationPrev() const { return _leftNext->sym(); }

HalfEdge *HalfEdge::getLeftLoopNext() const { return _leftNext; }

HalfEdge *HalfEdge::getLeftLoopPrev() const { return _originNext->sym(); }

HalfEdge *HalfEdge::getRightLoopNext() const { return sym()->_leftNext->sym(); }

HalfEdge *HalfEdge::getRightLoopPrev() const { return sym()->_originNext; }

const Vec2 &HalfEdge::getOrgVec() const { return origin; }

const Vec2 &HalfEdge::getDstVec() const { return sym()->origin; }

float HalfEdge::getLength() const { return origin.distance(sym()->origin); }

Edge *HalfEdge::getEdge() const { return (Edge *)((char *)this - sizeof(HalfEdge) * edgeOffset); }

bool HalfEdge::goesLeft() const {
	return ((Edge *)((char *)this - sizeof(HalfEdge) * edgeOffset))->inverted
			!= static_cast<bool>(edgeOffset);
}

bool HalfEdge::goesRight() const {
	return ((Edge *)((char *)this - sizeof(HalfEdge) * edgeOffset))->inverted
			== static_cast<bool>(edgeOffset);
}

void HalfEdge::foreachOnFace(const Callback<void(HalfEdge &)> &cb) {
	auto e = this;
	do {
		cb(*e);
		e = e->_leftNext;
	} while (e != this);
}

void HalfEdge::foreachOnVertex(const Callback<void(HalfEdge &)> &cb) {
	auto e = this;
	do {
		cb(*e);
		e = e->_originNext;
	} while (e != this);
}

void HalfEdge::foreachOnFace(const Callback<void(const HalfEdge &)> &cb) const {
	auto e = this;
	do {
		cb(*e);
		e = e->_leftNext;
	} while (e != this);
}

void HalfEdge::foreachOnVertex(const Callback<void(const HalfEdge &)> &cb) const {
	auto e = this;
	do {
		cb(*e);
		e = e->_originNext;
	} while (e != this);
}

float HalfEdge::getDirection() const { return getEdge()->direction; }

Edge::Edge() {
	left.isRight = -1;
	left.edgeOffset = 0;
	left._originNext = &left;
	left._leftNext = &right;
	right.isRight = 1;
	right.edgeOffset = 1;
	right._originNext = &right;
	right._leftNext = &left;
}

const Vec2 &Edge::getLeftVec() const { return inverted ? right.getOrgVec() : left.getOrgVec(); }

const Vec2 &Edge::getRightVec() const { return inverted ? left.getOrgVec() : right.getOrgVec(); }

const Vec2 &Edge::getOrgVec() const { return left.origin; }

const Vec2 &Edge::getDstVec() const { return right.origin; }

uint32_t Edge::getLeftOrg() const { return inverted ? right.vertex : left.vertex; }

uint32_t Edge::getRightOrg() const { return inverted ? left.vertex : right.vertex; }

void Edge::updateInfo() {
	if (sprt::isnan(direction)) {
		inverted = !EdgeGoesRight(&left);
		direction = EdgeDirection(getRightVec() - getLeftVec());
	}
};

int16_t Edge::getLeftWinding() const { return inverted ? right._realWinding : left._realWinding; }

int16_t Edge::getRightWinding() const { return inverted ? left._realWinding : right._realWinding; }

ObjectAllocator::ObjectAllocator(memory::pool_t *pool) : _pool(pool), _vertexes(pool) {
	_vertexes.reserve(VertexSetPrealloc);
}

Edge *ObjectAllocator::allocEdge() {
	Edge *edge = nullptr;
	if (!_freeEdges) {
		preallocateEdges(EdgeAllocBatch);
	}

	auto node = _freeEdges;
	_freeEdges = (Edge *)node->node;
	edge = new (node) Edge();

	return edge;
}

Vertex *ObjectAllocator::allocVertex() {
	Vertex *vertex = nullptr;
	if (!_freeVertexes) {
		preallocateVertexes(VertexAllocBatch);
	}

	auto node = _freeVertexes;
	_freeVertexes = (Vertex *)node->_edge;
	vertex = new (node) Vertex();
	vertex->_uniqueIdx = uint32_t(_vertexes.size());

	_vertexes.emplace_back(vertex);

	return vertex;
}

FaceEdge *ObjectAllocator::allocFaceEdge() {
	FaceEdge *face = nullptr;
	if (!_freeFaces) {
		preallocateFaceEdges(VertexAllocBatch);
	}

	auto node = _freeFaces;
	_freeFaces = node->_next;
	face = new (node) FaceEdge();
	return face;
}

void ObjectAllocator::releaseEdge(Edge *eDel) {
	removeEdgeFromVec(_edgesOfInterests, &eDel->left);
	removeEdgeFromVec(_edgesOfInterests, &eDel->right);
	removeEdgeFromVec(_faceEdges, &eDel->left);
	removeEdgeFromVec(_faceEdges, &eDel->right);

	auto lVertex = _vertexes[eDel->left.vertex];
	if (lVertex && lVertex->_edge == &eDel->left) {
		lVertex->_edge = eDel->left._originNext;
	}

	auto rVertex = _vertexes[eDel->right.vertex];
	if (rVertex && rVertex->_edge == &eDel->right) {
		rVertex->_edge = eDel->right._originNext;
	}

	if (eDel->node) {
		const_cast<EdgeDictNode *>(eDel->node)->edge = nullptr;
	}

	eDel->~Edge();

	eDel->node = (EdgeDictNode *)_freeEdges;
	eDel->invalidated = true;
	_freeEdges = eDel;
}

void ObjectAllocator::releaseVertex(uint32_t vDelId, uint32_t vNewId) {
	auto it1 = _vertexes[vDelId];
	auto it2 = _vertexes[vNewId];

	if (it1 && it2) {
		auto vDel = it1;

		vDel->removeFromList(it2);
		vDel->~Vertex();
		_vertexes[vDelId] = nullptr;

		vDel->_edge = (HalfEdge *)_freeVertexes;
		_freeVertexes = vDel;
	}
}

void ObjectAllocator::releaseVertex(Vertex *vDel) {
	if (vDel) {
		if constexpr (TessTypesVerbose != VerboseFlag::None) {
			sprt::cout << "releaseVertex: " << vDel->_uniqueIdx << ": " << vDel->_exportIdx << "\n";
		}
		if (vDel->_exportIdx != maxOf<uint32_t>()) {
			_exportVertexes[vDel->_exportIdx] = nullptr;
		}

		_vertexes[vDel->_uniqueIdx] = nullptr;
		vDel->~Vertex();

		vDel->_edge = (HalfEdge *)_freeVertexes;
		_freeVertexes = vDel;
	}
}

void ObjectAllocator::trimVertexes() {
	size_t offset = 0;
	for (auto it = _vertexes.rbegin(); it != _vertexes.rend(); ++it) {
		if (*it == nullptr) {
			++offset;
		} else {
			break;
		}
	}

	if (offset > 0) {
		_vertexes.resize(_vertexes.size() - offset);
	}
}

void ObjectAllocator::preallocateVertexes(uint32_t n) {
	if (auto vertsMem = (Vertex *)memory::pool::palloc(_pool, sizeof(Vertex) * n)) {
		for (uint32_t i = 0; i < n; ++i) {
			auto mem = vertsMem + i;
			mem->_edge = (HalfEdge *)(mem + 1);
		}

		Vertex *vtmp = _freeVertexes;
		_freeVertexes = vertsMem;
		(vertsMem + n - 1)->_edge = (HalfEdge *)vtmp;
	}

	_vertexes.reserve(n);
	_exportVertexes.reserve(n);
}

void ObjectAllocator::preallocateEdges(uint32_t n) {
	if (auto edgesMem = (Edge *)memory::pool::palloc(_pool, sizeof(Edge) * n)) {
		for (uint32_t i = 0; i < n; ++i) {
			auto mem = edgesMem + i;
			mem->node = (EdgeDictNode *)(mem + 1);
		}

		Edge *etmp = _freeEdges;
		_freeEdges = edgesMem;
		(edgesMem + n - 1)->node = (EdgeDictNode *)(etmp);
	}
}

void ObjectAllocator::preallocateFaceEdges(uint32_t n) {
	if (auto edgesMem = (FaceEdge *)memory::pool::palloc(_pool, sizeof(FaceEdge) * n)) {
		for (uint32_t i = 0; i < n; ++i) {
			auto mem = edgesMem + i;
			mem->_next = (FaceEdge *)(mem + 1);
		}

		FaceEdge *etmp = _freeFaces;
		_freeFaces = edgesMem;
		(edgesMem + n - 1)->_next = (FaceEdge *)(etmp);
	}
}

void ObjectAllocator::removeEdgeFromVec(sprt::__pool_vector<HalfEdge *> &vec, HalfEdge *e) {
	auto eOIt = sprt::find(vec.begin(), vec.end(), e);
	if (eOIt != vec.end()) {
		*eOIt = nullptr;
	}
}

VertexPriorityQueue::Heap::Heap(memory::pool_t *p, uint32_t s) : max(s == 0 ? 1 : s), pool(p) {
	// reserve at least index 1 (the sentinel written below); s==0 (empty vertex
	// set) would otherwise allocate a single slot yet write nodes[1]/handles[1].
	// Keeping max >= 1 also lets the `max <<= 1` growth in insert() actually grow.
	nodes = (Node *)memory::pool::palloc(pool, (max + 1) * sizeof(Node));
	handles = (Elem *)memory::pool::palloc(pool, (max + 1) * sizeof(Elem));

	nodes[1].handle = 1; /* so that Minimum() returns NULL */
	handles[1].key = nullptr;
}

VertexPriorityQueue::Heap::~Heap() {
	memory::pool::free(pool, nodes, (max + 1) * sizeof(Node));
	memory::pool::free(pool, handles, (max + 1) * sizeof(Elem));
}

void VertexPriorityQueue::Heap::init() {
	/* This method of building a heap is O(n), rather than O(n lg n). */

	for (uint32_t i = size; i >= 1; --i) { floatDown(i); }

	initialized = true;
}

/* returns INV_HANDLE iff out of memory */
VertexPriorityQueue::Handle VertexPriorityQueue::Heap::insert(Key keyNew) {
	uint32_t curr;
	Handle free;

	curr = ++size;
	if ((curr * 2) > max) {
		Node *saveNodes = nodes;
		Elem *saveHandles = handles;

		// If the heap overflows, double its size.
		auto tmpSize = max;

		max <<= 1;

		nodes = (Node *)memory::pool::palloc(pool, ((max + 1) * sizeof(Node)));
		if (nodes != nullptr) {
			sprt::memcpy(nodes, saveNodes, ((tmpSize + 1) * sizeof(Node)));
		}

		handles = (Elem *)memory::pool::palloc(pool, ((max + 1) * sizeof(Elem)));
		if (handles != nullptr) {
			sprt::memcpy(handles, saveHandles, (size_t)((tmpSize + 1) * sizeof(Elem)));
		}

		memory::pool::free(pool, saveNodes, (tmpSize + 1) * sizeof(Node));
		memory::pool::free(pool, saveHandles, (tmpSize + 1) * sizeof(Elem));
	}

	if (freeList == 0) {
		free = curr;
	} else {
		free = freeList;
		freeList = handles[free].node;
	}

	nodes[curr].handle = free;
	handles[free].node = curr;
	handles[free].key = keyNew;

	if (initialized) {
		floatUp(curr);
	}
	sprt_passert(free != InvalidHandle, "pqHeapInsert");
	return free;
}

/* really pqHeapExtractMin */
VertexPriorityQueue::Key VertexPriorityQueue::Heap::extractMin() {
	Node *n = nodes;
	Elem *h = handles;
	Handle hMin = n[1].handle;
	Key min = h[hMin].key;

	if (size > 0) {
		n[1].handle = n[size].handle;
		h[n[1].handle].node = 1;

		h[hMin].key = NULL;
		h[hMin].node = freeList;
		freeList = hMin;

		if (--size > 0) {
			floatDown(1);
		}
	}
	if (min) {
		min->_queueHandle = maxOf<QueueHandle>();
	}
	return min;
}

/* really pqHeapDelete */
void VertexPriorityQueue::Heap::remove(Handle hCurr) {
	Node *n = nodes;
	Elem *h = handles;
	uint32_t curr;

	sprt_passert(hCurr >= 1 && hCurr <= Handle(max) && h[hCurr].key != nullptr, "pqHeapDelete");

	curr = h[hCurr].node;
	n[curr].handle = n[size].handle;
	h[n[curr].handle].node = curr;

	if (curr <= --size) {
		if (curr <= 1 || VertLeq(h[n[curr >> 1].handle].key, h[n[curr].handle].key)) {
			floatDown(curr);
		} else {
			floatUp(curr);
		}
	}
	h[hCurr].key = NULL;
	h[hCurr].node = freeList;
	freeList = hCurr;
}


void VertexPriorityQueue::Heap::floatDown(int curr) {
	Node *n = nodes;
	Elem *h = handles;
	Handle hCurr, hChild;
	uint32_t child;

	hCurr = n[curr].handle;
	for (;;) {
		child = curr << 1;
		if (child < size && VertLeq(h[n[child + 1].handle].key, h[n[child].handle].key)) {
			++child;
		}

		sprt_passert(child <= this->max, "FloatDown");

		hChild = n[child].handle;
		if (child > size || VertLeq(h[hCurr].key, h[hChild].key)) {
			n[curr].handle = hCurr;
			h[hCurr].node = curr;
			break;
		}
		n[curr].handle = hChild;
		h[hChild].node = curr;
		curr = child;
	}
}

void VertexPriorityQueue::Heap::floatUp(int curr) {
	Node *n = nodes;
	Elem *h = handles;
	Handle hCurr, hParent;
	uint32_t parent;

	hCurr = n[curr].handle;
	for (;;) {
		parent = curr >> 1;
		hParent = n[parent].handle;
		if (parent == 0 || VertLeq(h[hParent].key, h[hCurr].key)) {
			n[curr].handle = hCurr;
			h[hCurr].node = curr;
			break;
		}
		n[curr].handle = hParent;
		h[hParent].node = curr;
		curr = parent;
	}
}

VertexPriorityQueue::VertexPriorityQueue(memory::pool_t *p,
		const sprt::__pool_vector<Vertex *> &vec)
: heap(p, uint32_t(vec.size())), max(uint32_t(vec.size())), pool(p) {
	keys = (Key *)memory::pool::palloc(p, max * sizeof(Key));

	for (auto &v : vec) {
		if (v) {
			v->_queueHandle = insert(v);
			if (v->_queueHandle == InvalidHandle) {
				return;
			}
		}
	}

	if (init()) {
		initialized = true;
	}
}

VertexPriorityQueue::~VertexPriorityQueue() { memory::pool::free(pool, keys, max * sizeof(Key)); }


#define LT(x, y)     (! VertLeq(y,x))
#define GT(x, y)     (! VertLeq(x,y))
#define KeySwap(a, b)   if(1){Key *tmp = *a; *a = *b; *b = tmp;}else

bool VertexPriorityQueue::init() {
	Key **p, **r, **i, **j, *piv;
	struct {
		Key **p, **r;
	} Stack[50], *top = Stack;
	unsigned int seed = 2'016'473'283;

	/* Create an array of indirect pointers to the keys, so that we
	 * the handles we have returned are still valid. */
	/*
	 pq->order = (PQkey **)memAlloc( (size_t)
	 (pq->size * sizeof(pq->order[0])) );
	 */
	order = (Key **)memory::pool::palloc(pool, size_t((size + 1) * sizeof(Key *)));

	p = order;
	r = p + size - 1;
	for (piv = keys, i = p; i <= r; ++piv, ++i) { *i = piv; }

	/* Sort the indirect pointers in descending order,
	 * using randomized Quicksort */
	top->p = p;
	top->r = r;
	++top;
	while (--top >= Stack) {
		p = top->p;
		r = top->r;
		while (r > p + 10) {
			seed = seed * 1'539'415'821 + 1;
			i = p + seed % (r - p + 1);
			piv = *i;
			*i = *p;
			*p = piv;
			i = p - 1;
			j = r + 1;
			do {
				do { ++i; } while (GT(**i, *piv));
				do { --j; } while (LT(**j, *piv));
				KeySwap(i, j);
			} while (i < j);
			KeySwap(i, j); /* Undo last swap */
			if (i - p < r - j) {
				top->p = j + 1;
				top->r = r;
				++top;
				r = i - 1;
			} else {
				top->p = p;
				top->r = i - 1;
				++top;
				p = j + 1;
			}
		}
		/* Insertion sort small lists */
		for (i = p + 1; i <= r; ++i) {
			piv = *i;
			for (j = i; j > p && LT(**(j - 1), *piv); --j) { *j = *(j - 1); }
			*j = piv;
		}
	}
	max = size;
	initialized = true;

	heap.init();

#ifndef NDEBUG
	p = order;
	r = p + size - 1;
	for (i = p; i < r; ++i) { sprt_passert(VertLeq(**(i + 1), **i), "pqInit"); }
#endif

	return 1;
}

#undef LT
#undef GT
#undef KeySwap

bool VertexPriorityQueue::empty() const { return size == 0 && heap.empty(); }

VertexPriorityQueue::Handle VertexPriorityQueue::insert(Key keyNew) {
	int curr;

	if (initialized) {
		return heap.insert(keyNew);
	}
	curr = size;
	if (++size >= max) {
		Key *saveKey = keys;
		// If the heap overflows, double its size.
		auto tmpSize = max;
		max <<= 1;
		keys = (Key *)memory::pool::palloc(pool, max * sizeof(Key));
		if (keys) {
			sprt::memcpy(keys, saveKey, (size_t)(tmpSize * sizeof(Key)));
		}

		memory::pool::free(pool, saveKey, tmpSize * sizeof(Key));
	}
	sprt_passert(curr != InvalidHandle, "pqInsert");
	keys[curr] = keyNew;

	/* Negative handles index the sorted array. */
	return -(curr + 1);
}

void VertexPriorityQueue::remove(Handle curr) {
	if (curr >= 0) {
		heap.remove(curr);
		return;
	}
	curr = -(curr + 1);
	sprt_passert(curr < Handle(max) && keys[curr] != nullptr, "pqDelete");

	keys[curr] = nullptr;
	while (size > 0 && *(order[size - 1]) == nullptr) { --size; }
}

VertexPriorityQueue::Key VertexPriorityQueue::extractMin() {
	Key sortMin, heapMin;

	if (size == 0) {
		return heap.extractMin();
	}
	sortMin = *(order[size - 1]);
	if (!heap.empty()) {
		heapMin = heap.getMin();
		if (VertLeq(heapMin, sortMin)) {
			return heap.extractMin();
		}
	}
	do { --size; } while (size > 0 && *(order[size - 1]) == NULL);
	sortMin->_queueHandle = maxOf<QueueHandle>();
	return sortMin;
}

VertexPriorityQueue::Key VertexPriorityQueue::getMin() const {
	Key sortMin, heapMin;

	if (size == 0) {
		return heap.getMin();
	}
	sortMin = *(order[size - 1]);
	if (!heap.empty()) {
		heapMin = heap.getMin();
		if (VertLeq(heapMin, sortMin)) {
			return heapMin;
		}
	}
	return sortMin;
}

EdgeDict::EdgeDict(memory::pool_t *p, uint32_t size) : nodes(p), freeNodes(p), pool(p) {
	nodes.reserve(size);
	freeNodes.reserve(size);
}

void EdgeDict::refresh(const EdgeDictNode &cn) const {
	if (cn.stamp == serial) {
		return;
	}
	auto &n = const_cast<EdgeDictNode &>(cn);
	n.stamp = serial;

	// The three branches `update` used to run for everybody, unchanged. Only WHEN they run moved.
	if (n.edge && n.edge->getRightOrg() == eventVertex) {
		n.value.x = n.value.z;
		n.value.y = n.value.w;
	} else if (n.horizontal) {
		const float tValue = (event.x - n.org.x) / (n.norm.x);
		n.value.x = n.org.x + n.norm.x * tValue;
		n.value.y = n.org.y + n.norm.y * tValue;
	} else {
		const float sValue = (event.y - n.org.y) / (n.norm.y);
		n.value.x = n.org.x + n.norm.x * sValue;
		n.value.y = n.org.y + n.norm.y * sValue;
	}
}

size_t EdgeDict::lowerBound(float y) const {
	// Plain binary search over the value the last `update` left. Tombstones take part: they hold
	// a value from that same update, so including them keeps the array a sorted sequence.
	size_t lo = 0, hi = nodes.size();
	while (lo < hi) {
		const size_t mid = lo + (hi - lo) / 2;
		refresh(*nodes[mid]);
		if (nodes[mid]->value.y < y) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}
	return lo;
}

EdgeDictNode *EdgeDict::acquireNode() {
	if (!freeNodes.empty()) {
		auto ret = freeNodes.back();
		freeNodes.pop_back();
		return ret;
	}
	return new (memory::pool::palloc(pool, sizeof(EdgeDictNode))) EdgeDictNode();
}

const EdgeDictNode *EdgeDict::push(Edge *edge, int16_t windingAbove) {
	if constexpr (DictDebug) {
		sprt::cout << "\t\tDict push: " << *edge << "\n";
	}

	sprt_passert(edge, "edge should be defined");

	auto &dst = edge->getDstVec();
	auto &org = edge->getOrgVec();

	Vec2 norm;
	Vec4 value;
	if (org == event) {
		norm = dst - event;
		value = Vec4(event.x, event.y, dst.x, dst.y);
	} else if (dst == event) {
		norm = org - event;
		value = Vec4(event.x, event.y, org.x, org.y);
	} else {
		sprt::cout << "Fail to add edge: " << *edge << " for " << event << "\n";
		return nullptr;
	}

	/* The structure this replaced was a SET, and that is load-bearing.
	
	`nodes.emplace` on a set does not insert when an equivalent element is already there - it
	returns the one that is. Equivalent under its comparator means the same crossing height AND the
	same direction, so two edges that reach the sweepline at the same point at the same angle
	shared a single entry, and the second one's `Edge::node` pointed at the first one's.
	
	Whether that was intended is a separate question; the algorithm is built on it. A vector that
	inserted both instead produced a dictionary the sweep could not finish - four hundred crossing
	wires died at event 1 955 of 104 337. */
	size_t pos = lowerBound(value.y);
	while (pos < nodes.size() && (refresh(*nodes[pos]), nodes[pos]->value.y == value.y)
			&& nodes[pos]->edge && nodes[pos]->edge->direction < edge->direction) {
		++pos;
	}

	if (pos < nodes.size() && nodes[pos]->edge && nodes[pos]->value.y == value.y
			&& nodes[pos]->edge->direction == edge->direction) {
		return nodes[pos]; // equivalent entry already present - the set would have kept it
	}

	auto node = acquireNode();
	*node = EdgeDictNode{event, norm, value, edge, windingAbove,
		sprt::abs(norm.x) > sprt::Epsilon<float>, false};
	node->stamp = serial; // built from the current event, so it is already up to date

	/* Where it goes, and whether anything has to move to let it in.
	
	The order is by `value.y`, tie-broken by direction - the tree's `operator<` in two lines,
	because a flat array needs the comparison at the insertion point rather than as a type trait.
	
	A tombstone AT that point takes the new edge without anything moving, which is the common case
	on a sweepline: an edge retires and another enters at nearly the same height. Failing that the
	tail shifts, and shifting eight-byte words over contiguous memory is what this structure was
	chosen for. */
	if (pos < nodes.size() && nodes[pos]->dead) {
		freeNodes.emplace_back(nodes[pos]);
		nodes[pos] = node;
	} else if (pos > 0 && nodes[pos - 1]->dead) {
		freeNodes.emplace_back(nodes[pos - 1]);
		nodes[pos - 1] = node;
	} else {
		nodes.emplace(nodes.begin() + pos, node);
	}

	return node;
}

void EdgeDict::pop(const EdgeDictNode *node) {
	if constexpr (DictDebug) {
		sprt::cout << "\t\tDict pop: " << *node->edge << "\n";
	}

	// A tombstone, in place: nothing moves and no search has to be redone. The entry keeps the
	// value it died with, which is the value everything else in the array also holds, so the
	// ordering the searches rely on is untouched. `update` sweeps it out on the next event.
	auto slot = const_cast<EdgeDictNode *>(node);
	if (slot->dead) {
		return;
	}
	if (slot->edge) {
		slot->edge->node = nullptr;
	}
	slot->dead = true;

	// Removed for real, not tombstoned. `getEdgeBelow` walks DOWN from a bound and returns
	// whatever it lands on without asking whether it is alive - the callers were written against a
	// structure where a popped entry was gone - so leaving one behind hands the winding an edge
	// that is not there. The retirement inside `update` is where tombstones pay off; `pop` is rare
	// and can afford a memmove.
	for (size_t i = 0; i < nodes.size(); ++i) {
		if (nodes[i] == slot) {
			nodes.erase(nodes.begin() + i);
			freeNodes.emplace_back(slot);
			return;
		}
	}
}

void EdgeDict::update(Vertex *v, float tolerance) {
	event = v->_origin;
	eventVertex = v->_uniqueIdx;
	++serial;

	/* One pass, and what it does now is RETIRE - nothing else.
	
	The parameter test that used to justify a division per node is a range test: `t` lies in
	[0, 1] exactly when the sweep's x lies between the edge's own two x's, whichever way round
	they are. Same for the vertical case in y. So this loop is two comparisons a node, and the
	crossing point itself is worked out by `refresh` when something actually looks at it.
	
	The survivors are written back over the array as it goes, which is also where tombstones -
	`pop`'s and this pass's own - are swept out. */
	size_t write = 0;
	const size_t count = nodes.size();

	for (size_t read = 0; read < count; ++read) {
		auto n = nodes[read];

		if (n->dead || !n->edge) {
			n->dead = true;
			freeNodes.emplace_back(n);
			continue;
		}

		bool retire = false;

		if (n->edge->getRightOrg() == v->_uniqueIdx) {
			// Ends at this very vertex: not retired, and `refresh` snaps it to its destination.
		} else if (n->horizontal) {
			const float lo = sprt::min(n->org.x, n->value.z);
			const float hi = sprt::max(n->org.x, n->value.z);
			retire = event.x < lo || event.x > hi;
		} else {
			const float lo = sprt::min(n->org.y, n->value.w);
			const float hi = sprt::max(n->org.y, n->value.w);
			retire = event.y < lo || event.y > hi;
		}

		/* The degenerate case, gated so it costs nothing for the nodes it cannot apply to.
		
		It asks whether the edge has collapsed onto its own destination, and it can only be true
		where the sweep has reached that destination's x - so the x is compared first and the
		value is brought up to date only for the handful of edges that end here. */
		if (!retire && event.x == n->value.z) {
			refresh(*n);
			auto curr = n->current();
			auto dst = n->dst();
			if (curr.x == dst.x && sprt::abs(curr.y - dst.y) < tolerance && n->value.y < event.y) {
				retire = true;
			}
		}

		if (retire) {
			n->edge->node = nullptr;
			n->dead = true;
			freeNodes.emplace_back(n);
			continue;
		}

		nodes[write++] = n;
	}

	nodes.resize(write);
}

const EdgeDictNode *EdgeDict::checkForIntersects(Vertex *v, Vec2 &intersectPoint,
		IntersectionEvent &ev, float tolerance) const {
	if (nodes.empty()) {
		return nullptr;
	}

	auto &org = v->_origin;

	if constexpr (IntersectDebug) {
		sprt::cout << "\t\t\t\tcheckForIntersects: " << *v << "\n";
	}

	/* Only the edges whose sweepline crossing is level with the event can match.
	
	The test below is `VertEq(nCurr, org)` - both coordinates within `tolerance` - and the
	dictionary is ORDERED BY `value.y`, which is exactly `nCurr.y`. So every candidate lies in the
	band [org.y - tolerance, org.y + tolerance], and the rest of the dictionary cannot contain one
	however large it is.
	
	This is a filter, not a heuristic: the nodes skipped are nodes the old loop would have visited
	and rejected on the same comparison. What changes is only how many are looked at - a full walk
	of every open edge on every event, which at a few hundred open edges is where the sweep spent
	almost all of its time.
	
	`lower_bound` on a Vec2 compares `value.y < other.y` (EdgeDictNode::operator<), so it lands on
	the first node at or above the bottom of the band. */
	const float bandTop = org.y + tolerance;

	for (size_t i = lowerBound(org.y - tolerance); i < nodes.size(); ++i) {
		auto &n = *nodes[i];
		refresh(n);
		if (n.value.y > bandTop) {
			break;
		}
		if (n.dead) {
			continue; // a tombstone is not an answer
		}
		auto nCurr = n.current();
		auto nDst = n.dst();

		if constexpr (IntersectDebug) {
			sprt::cout << "\t\t\t\t\t: " << *n.edge << "\n";
		}

		if (VertEq(nCurr, org, tolerance) && !VertEq(n.org, org, tolerance)) {
			if (VertEq(nCurr, nDst, tolerance)) {
				continue; // no intersection, just line end
			}
			intersectPoint = event;
			ev = IntersectionEvent::EventIsIntersection;
			return &n;
		}
	}

	return nullptr;
}

/* How far out from the insertion point a new edge is checked against the open ones.
 
THE SWEEPLINE INVARIANT says two: an edge entering the status structure can only cross the edges
immediately above and below it, because to reach any other it would first have to cross those. That
is Bentley-Ottmann, and it is what turns this walk from "every open edge" into "a couple".

It is stated here as a NUMBER rather than assumed, because the invariant holds for exact arithmetic
and this is float. `update()` recomputes every edge's crossing of the sweepline on every event, and
two edges whose crossings land within an ulp of each other can be ordered either way - so the true
neighbour can sit a place or two off where the ordering says it is. The window is the slack for
that, and its value is a measured one.

EIGHT, and the number is measured rather than argued. Two is what the invariant says, and two is
wrong here: on the icon corpus it changes four icons out of eight thousand six hundred, and on a
graph of four hundred crossing wires it loses a third of the sweep's events outright. Three is
already clean on every icon. Eight is where the WIRES stop differing too - the event count matches
the exhaustive scan exactly (104 337 either way), while three and four are off by a handful.

So eight is the smallest window at which both corpora agree with checking every open edge, and it
still costs a tenth of what checking every open edge costs. It is not a proof - the exhaustive scan
is the only thing that is - so the day a shape comes out wrong, raising this is the first thing to
try, and if raising it fixes the shape then this number is what was wrong rather than the
geometry. */
static constexpr uint32_t DictNeighbourWindow = 8;

const EdgeDictNode *EdgeDict::checkForIntersects(HalfEdge *edge, Vec2 &intersectPoint,
		IntersectionEvent &ev, float tolerance) const {
	namespace simd = sprt::geom::simd;

	if (nodes.empty()) {
		return nullptr;
	}

	auto &org = edge->getOrgVec(); // == event
	auto &dst = edge->getDstVec();

	auto simdVec1 = simd::load(org.x, org.y, dst.x, dst.y);

	if constexpr (IntersectDebug) {
		sprt::cout << "\t\t\t\tcheckForIntersects: " << *edge << "\n";
	}

	// The y-band both ends of every test below live in - see the reject inside `test`.
	const float edgeLo = sprt::min(org.y, dst.y) - tolerance;
	const float edgeHi = sprt::max(org.y, dst.y) + tolerance;

	const EdgeDictNode *hit = nullptr;

	// One node against this edge. The body is the one that walked the whole dictionary; what
	// changed is only which nodes reach it.
	const auto test = [&](const EdgeDictNode &n) -> bool {
		refresh(n);
		const float nodeLo = sprt::min(n.value.y, n.value.w);
		const float nodeHi = sprt::max(n.value.y, n.value.w);
		if (nodeHi < edgeLo || nodeLo > edgeHi) {
			return false;
		}

		auto nCurr = n.current();
		auto nDst = n.dst();

		// overlap check should be made in mergeVertexes
		// so, should never happen
		if (VertEq(n.org, org, tolerance) || VertEq(nDst, org, tolerance)) {
			return false; // common org, not interested
		} else if (VertEq(nCurr, org, tolerance)) {
			if (VertEq(nCurr, nDst, tolerance)) {
				return false; // no intersection, just line end
			}
			intersectPoint = event;
			ev = IntersectionEvent::EventIsIntersection;
			hit = &n;
			return true;
		}

		if (VertEq(dst, nDst, tolerance)) {
			return false; // common dst
		}

		simd::f32x4 intersect;
		if (simd::isVec2BboxIntersects(simdVec1, simd::load(&n.value.x), intersect)) {
			Vec4 isect;
			simd::store(&isect.x, intersect);
			if (VertEq(nCurr, nDst, tolerance)) {
				if (sprt::abs(isect.x) < tolerance) {
					if (sprt::abs(isect.y) < tolerance) {
						intersectPoint = nCurr;
						ev = IntersectionEvent::EdgeConnection1; // n ends on edge;
						hit = &n;
						return true;
					}
				} else {
					auto S = (nDst.x - org.x) / (isect.x);
					if (S >= 0.0f && S <= 1.0f) {
						auto y = org.y + S * isect.y;
						if (sprt::abs(nDst.y - y) <= tolerance) {
							intersectPoint = nCurr;
							ev = IntersectionEvent::EdgeConnection1; // n ends on edge;
							hit = &n;
							return true;
						}
					}
				}
				return false;
			}

			const float denom =
					isect.w * isect.x - isect.z * isect.y; // crossProduct2Vector(A, B, C, D);
			if (denom != 0.0f) {
				const auto CAx = org.x - n.value.x;
				const auto CAy = org.y - n.value.y;

				auto S = (CAy * isect.z - CAx * isect.w) / denom;
				auto T = (CAy * isect.x - CAx * isect.y) / denom;

				if (S >= 0.0f && S <= 1.0f && T >= 0.0f && T <= 1.0f) {
					intersectPoint = Vec2(org.x + S * isect.x, org.y + S * isect.y);
					auto eq2 = VertEq(intersectPoint, dst, tolerance);
					auto eq1 = VertEq(intersectPoint, nDst, tolerance);
					if (eq1 && eq1) {
						ev = IntersectionEvent::Merge;
					} else if (eq2) {
						ev = IntersectionEvent::EdgeConnection2; // edge ends on n;
					} else if (eq1) {
						intersectPoint = nDst;
						ev = IntersectionEvent::EdgeConnection1; // n ends on edge;
					} else {
						ev = IntersectionEvent::Regular;
					}
					hit = &n;
					return true;
				}
			}
		}
		return false;
	};

	/* Outward from where this edge sits in the ordering, alternating up and down.
	
	Alternating rather than up-then-down so that the nearest neighbour is reached first whichever
	side it is on: the old loop returned the LOWEST crossing in the dictionary, this one returns
	the NEAREST, and near the sweepline that is the one the algorithm is entitled to assume comes
	first. */
	const size_t mid = lowerBound(org.y);
	size_t up = mid;
	size_t down = mid;

	for (uint32_t step = 0; step < DictNeighbourWindow; ++step) {
		bool moved = false;
		while (up < nodes.size() && nodes[up]->dead) {
			++up; // step over the dead rather than counting them as a neighbour
		}
		if (up < nodes.size()) {
			auto &n = *nodes[up];
			++up;
			moved = true;
			if (test(n)) {
				return hit;
			}
		}
		while (down > 0 && nodes[down - 1]->dead) { --down; }
		if (down > 0) {
			--down;
			moved = true;
			if (test(*nodes[down])) {
				return hit;
			}
		}
		if (!moved) {
			break;
		}
	}

	return nullptr;
}

const EdgeDictNode *EdgeDict::getEdgeBelow(const Edge *e) const {
	if (nodes.empty()) {
		return nullptr;
	}

	// `lower_bound` over `EdgeDictNode::operator<(const Edge &)`: by y, tie-broken by direction.
	auto &left = e->getLeftVec();
	size_t lo = 0, hi = nodes.size();
	while (lo < hi) {
		const size_t mid = lo + (hi - lo) / 2;
		auto &n = *nodes[mid];
		refresh(n);
		const bool less = (n.value.y == left.y) ? (n.edge && n.edge->direction < e->direction)
												: (n.value.y < left.y);
		if (less) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}

	if (lo == 0) {
		return nullptr; // first edge in dict greater or equal then e, no edges below
	}

	// The walk down is the tree version's, condition for condition: it stopped at `begin()` and
	// returned whatever it landed on, tombstone or not, and the callers were written against
	// that. Adding a skip here changed which edge the winding was taken from.
	size_t i = lo - 1;
	refresh(*nodes[i]);
	while (i > 0 && nodes[i]->current() == event) {
		--i;
		refresh(*nodes[i]);
	}
	return nodes[i];
}

const EdgeDictNode *EdgeDict::getEdgeBelow(const Vec2 &vec, uint32_t vertex) const {
	if (nodes.empty()) {
		return nullptr;
	}

	const size_t lo = lowerBound(vec.y);
	if (lo == 0) {
		return nullptr; // first edge in dict greater or equal then e, no edges below
	}

	size_t i = lo - 1;
	refresh(*nodes[i]);
	while (i > 0 && nodes[i]->edge
			&& (nodes[i]->edge->getRightOrg() == vertex || nodes[i]->current() == vec)) {
		--i;
		refresh(*nodes[i]);
	}
	return nodes[i];
}

} // namespace stappler::geom
