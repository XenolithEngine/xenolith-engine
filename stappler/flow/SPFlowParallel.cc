/**
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

// The static half of a parallel block: what may run while a block is in flight, and which accesses
// that makes a result depend on the order of branches or on the moment of delivery. The window of a
// block is the steps between its fan-out F and its barrier B, and a node is concurrent when some
// executor order runs it inside that window. The machine does not stop at an unready barrier, so
// whatever lies on the front under F when F runs may run before B does, and so may everything that
// work can start. The region is therefore built from the front's order, not from dominance - a
// token is permission to run every time it arrives, and a node that dominates F can run again while
// the block is in flight:
//   MustRan(F)  F, its dominators over token edges, and the data producers of both and of
//               the body's inputs from outside it: they have run, and only a token can run
//               them again.
//   spine(F)    backwards from F over token and data edges, inside F's chain of scopes; a
//               barrier, or a collector's input, is where another block was delivered.
//   seeds       (a) exec outputs of a spine node fired after the one leading to the spine
//                   (the canonical edge order is the order they go on the front), and what
//                   the earlier ones left unfinished - loops and blocks; none for an
//                   exclusive operation or one that opens a scope;
//               (b) what a producer that pushed a spine node pushed beside it;
//               (c) the entry points and the eager nodes of F's chain, unless in MustRan;
//               (d) the region of every block whose barrier is on the spine.
//   Conc(F)     the seeds closed over token edges, over whole foreign scopes (an opener runs
//               again when a turn closes), and over data edges into nodes not in MustRan.
// F in its own region is a second token arriving while the block is in flight (parallel-reentry).
// The region depends on the front's order, so front weights have to derive it again.

namespace STAPPLER_VERSIONIZED stappler::flow {

namespace {

struct NodeSet {
	mem_std::Vector<uint64_t> bits;

	void init(uint32_t count) { bits.assign((count + 63) / 64, uint64_t(0)); }
	bool test(uint32_t i) const { return ((bits[i >> 6] >> (i & 63)) & 1) != 0; }
	bool set(uint32_t i) {
		auto &w = bits[i >> 6];
		auto mask = uint64_t(1) << (i & 63);
		if ((w & mask) != 0) {
			return false;
		}
		w |= mask;
		return true;
	}
};


// Why a node is in a region: an exec edge, a data edge, or nothing nameable (an entry point, a
// foreign scope, another block's region).
static constexpr uint32_t ReasonData = uint32_t(1) << 31;
static constexpr uint32_t ReasonNone = InvalidIndex;

// One scene access of a node, from its contract and its operation's groups.
struct Access {
	uint32_t node = InvalidIndex;
	TypeId component = value::NullTypeId;
	StringView componentName;
	StringView field; // empty: whether the entity has the component
	SceneAccess access = SceneAccess::Read;
	uint32_t targetNode = InvalidIndex; // the producer feeding the target pin, if an edge does
	uint32_t targetPin = InvalidIndex;
};

class ParallelAnalysis {
public:
	ParallelAnalysis(const RuntimeGraph &g, const value::TypeRegistry *scene)
	: _g(g), _scene(scene), _count(g.getNodeCount()) { }

	void run(DiagReport *);
	void describe(mem_std::Value &) const;

	// Which blocks' paths are lifted to weight 1, decided one block at a time, each lift kept only
	// when the analysis under it still refuses nothing. The weight of every node.
	void decideWeights(DiagReport &, mem_std::Vector<uint8_t> &weights);

private:
	struct Block {
		uint32_t fanOut = InvalidIndex;
		uint32_t scope = InvalidIndex;
		uint32_t barrier = InvalidIndex;
		uint32_t entityPin = InvalidIndex;
		bool gpu = false;
		NodeSet body, mustRan, conc, after;
		NodeSet reach; // may hold a token in the window: see mayWait

		// The nodes that lead to the fan-out in its own activation, and whether they are lifted.
		NodeSet lift;
		bool liftKnown = false;
		bool lifted = false;
		mem_std::Vector<uint32_t> reason;
	};

	bool isTokenEdge(uint32_t edge) const;
	bool inChain(const Block &, uint32_t scope) const;
	bool dominates(uint32_t a, uint32_t b) const;
	bool isOwn(const Block &, const Access &) const;
	bool mayWait(const Block &, uint32_t node) const;

	// The block a node's values belong to: the one it opens, or the one whose body it is in.
	uint32_t blockOf(uint32_t node) const;

	void computeDominators();
	void collectAccesses();
	void prepare(Block &);
	bool computeRegion(Block &);
	void computeAfter(Block &);
	void check(Block &, DiagReport &);

	void computeAll();
	void resetRegions();
	bool liftConflict(const Block &, uint32_t &blocker) const;
	bool accessesConflict(uint32_t a, uint32_t b) const;
	void checkGpu(Block &, DiagReport &);
	void checkEnums(Block &, DiagReport &);

	bool conflictOnce(uint32_t a, uint32_t b);
	void reportReason(DiagReport &, DiagCode, uint32_t node, uint32_t reason, DiagDetail);

	const RuntimeGraph &_g;
	const value::TypeRegistry *_scene;
	uint32_t _count;

	mem_std::Vector<uint32_t> _idom; // over token edges; _count is the virtual root
	mem_std::Vector<Access> _accesses;
	mem_std::Vector<uint32_t> _accessBegin, _accessCount;
	mem_std::Vector<uint8_t> _dynamicRead, _dynamicWrite;
	mem_std::Vector<Block> _blocks;
	mem_std::Set<uint64_t> _reported;
	NodeSet _weighted; // the union of the lifted blocks' paths
};

bool ParallelAnalysis::isTokenEdge(uint32_t e) const {
	auto &edge = _g.getExecEdges()[e];
	auto &src = _g.getNodeAt(edge.srcNode);
	auto &dst = _g.getNodeAt(edge.dstNode);
	if (src.scope == dst.scope) {
		return true;
	}
	if (src.opensScope != InvalidIndex && src.op->opensScope(edge.srcPin)
			&& dst.scope == src.opensScope) {
		return true; // the edge that opens a body
	}
	auto &scope = _g.getScopeAt(src.scope);
	return scope.kind == ScopeKind::Parallel && scope.barrier == edge.dstNode;
}

bool ParallelAnalysis::inChain(const Block &b, uint32_t scope) const {
	return _g.isScopeWithin(_g.getNodeAt(b.fanOut).scope, scope);
}

bool ParallelAnalysis::dominates(uint32_t a, uint32_t b) const {
	if (_idom[b] == InvalidIndex) {
		return false;
	}
	while (b != _count) {
		if (b == a) {
			return true;
		}
		b = _idom[b];
		if (b == InvalidIndex) {
			return false;
		}
	}
	return false;
}

// Whether data arriving can run `node` inside the window: an eager node runs on data, a node with
// an exec input only when it holds a token - which it cannot when every token path to it passes the
// barrier, or an output an exclusive node on the spine did not fire.
bool ParallelAnalysis::mayWait(const Block &b, uint32_t node) const {
	auto &n = _g.getNodeAt(node);
	return !n.op || !n.op->hasExecIn() || b.reach.test(node);
}

uint32_t ParallelAnalysis::blockOf(uint32_t node) const {
	auto &n = _g.getNodeAt(node);
	auto scope = n.opensScope != InvalidIndex ? n.opensScope : n.scope;
	for (uint32_t k = 0; k < uint32_t(_blocks.size()); ++k) {
		if (_g.isScopeWithin(scope, _blocks[k].scope)) {
			return k;
		}
	}
	return InvalidIndex;
}

bool ParallelAnalysis::isOwn(const Block &b, const Access &a) const {
	return a.targetNode == b.fanOut && a.targetPin == b.entityPin;
}

// Cooper, Harvey and Kennedy over the token edges, from a virtual root that points at every entry.
void ParallelAnalysis::computeDominators() {
	_idom.assign(_count + 1, InvalidIndex);

	mem_std::Vector<uint32_t> order; // reverse postorder
	mem_std::Vector<uint32_t> number(_count + 1, InvalidIndex);
	{
		mem_std::Vector<uint8_t> seen(_count, uint8_t(0));
		mem_std::Vector<uint32_t> post;
		mem_std::Vector<std::pair<uint32_t, uint32_t>> stack;
		for (auto entry : _g.getEntryNodes()) {
			if (seen[entry]) {
				continue;
			}
			seen[entry] = 1;
			stack.emplace_back(entry, 0);
			while (!stack.empty()) {
				auto &top = stack.back();
				auto outs = _g.getExecOutEdges(top.first);
				if (top.second < outs.size()) {
					auto e = outs[top.second++];
					auto next = _g.getExecEdges()[e].dstNode;
					if (isTokenEdge(e) && !seen[next]) {
						seen[next] = 1;
						stack.emplace_back(next, 0);
					}
				} else {
					post.emplace_back(top.first);
					stack.pop_back();
				}
			}
		}
		order.emplace_back(_count);
		for (size_t i = post.size(); i > 0; --i) { order.emplace_back(post[i - 1]); }
		for (uint32_t i = 0; i < uint32_t(order.size()); ++i) { number[order[i]] = i; }
	}

	auto intersect = [&](uint32_t a, uint32_t b) {
		while (a != b) {
			while (number[a] > number[b]) { a = _idom[a]; }
			while (number[b] > number[a]) { b = _idom[b]; }
		}
		return a;
	};

	mem_std::Vector<uint8_t> isEntry(_count, uint8_t(0));
	for (auto entry : _g.getEntryNodes()) { isEntry[entry] = 1; }

	_idom[_count] = _count;
	bool changed = true;
	while (changed) {
		changed = false;
		for (uint32_t i = 1; i < uint32_t(order.size()); ++i) {
			auto n = order[i];
			uint32_t idom = isEntry[n] ? _count : InvalidIndex;
			for (auto e : _g.getExecInEdges(n)) {
				auto pred = _g.getExecEdges()[e].srcNode;
				if (!isTokenEdge(e) || number[pred] == InvalidIndex || _idom[pred] == InvalidIndex) {
					continue;
				}
				idom = idom == InvalidIndex ? pred : intersect(pred, idom);
			}
			if (idom != InvalidIndex && _idom[n] != idom) {
				_idom[n] = idom;
				changed = true;
			}
		}
	}
	_idom[_count] = InvalidIndex;
}

void ParallelAnalysis::collectAccesses() {
	_accessBegin.assign(_count, 0);
	_accessCount.assign(_count, 0);
	_dynamicRead.assign(_count, uint8_t(0));
	_dynamicWrite.assign(_count, uint8_t(0));

	for (uint32_t n = 0; n < _count; ++n) {
		auto &node = _g.getNodeAt(n);
		_accessBegin[n] = uint32_t(_accesses.size());
		if (!node.op) {
			continue;
		}
		auto groups = node.op->getSceneGroups();
		auto refs = node.op->getSceneRefs();
		auto pinGroups = uint32_t(groups.size() - refs.size());
		auto contract = _g.getSceneContract(n);

		uint32_t targetNode = InvalidIndex, targetPin = InvalidIndex;
		auto target = node.op->getTargetPin();
		if (target != NullPin) {
			for (auto e : _g.getDataInEdges(n)) {
				auto &edge = _g.getDataEdges()[e];
				if (edge.dstPin == target) {
					targetNode = edge.srcNode;
					targetPin = edge.srcPin;
				}
			}
		}

		for (uint32_t k = 0; k < uint32_t(contract.size()) && k < groups.size(); ++k) {
			auto &binding = contract[k];
			auto &group = groups[k];
			if (binding.dynamic) {
				(group.access == SceneAccess::Write ? _dynamicWrite : _dynamicRead)[n] = 1;
				continue;
			}
			if (binding.component.empty()) {
				continue;
			}
			Access a;
			a.node = n;
			a.component = binding.componentId;
			a.componentName = binding.component;
			a.field = binding.field;
			a.access = group.access;
			const bool targeted = k < pinGroups ? target != NullPin : refs[k - pinGroups].targeted;
			if (targeted) {
				a.targetNode = targetNode;
				a.targetPin = targetPin;
			}
			_accesses.emplace_back(a);
		}
		_accessCount[n] = uint32_t(_accesses.size()) - _accessBegin[n];
	}
}

void ParallelAnalysis::prepare(Block &b) {
	auto &fanOut = _g.getNodeAt(b.fanOut);
	fanOut.op->findDataOut(StringView("entity"), b.entityPin);

	uint32_t executors = 0;
	if (fanOut.op->findSetting(StringView("executors"), executors)) {
		if (auto value = _g.getSetting(b.fanOut, executors)) {
			for (auto &it : value->asArray()) {
				if (StringView(it.getString()) == StringView("gpu")) {
					b.gpu = true;
				}
			}
		}
	}

	b.body.init(_count);
	b.mustRan.init(_count);
	b.conc.init(_count);
	b.after.init(_count);
	b.reason.assign(_count, ReasonNone);

	for (uint32_t n = 0; n < _count; ++n) {
		if (_g.isScopeWithin(_g.getNodeAt(n).scope, b.scope)) {
			b.body.set(n);
		}
	}

	// MustRan: F, its dominators, and the producers of both and of the body's outside inputs.
	mem_std::Vector<uint32_t> work;
	auto mark = [&](uint32_t n) {
		if (!b.body.test(n) && b.mustRan.set(n)) {
			work.emplace_back(n);
		}
	};
	mark(b.fanOut);
	if (_idom[b.fanOut] != InvalidIndex) {
		for (auto d = _idom[b.fanOut]; d != InvalidIndex && d != _count; d = _idom[d]) { mark(d); }
	}
	for (uint32_t n = 0; n < _count; ++n) {
		if (!b.body.test(n)) {
			continue;
		}
		for (auto e : _g.getDataInEdges(n)) {
			auto src = _g.getDataEdges()[e].srcNode;
			if (!b.body.test(src)) {
				mark(src);
			}
		}
	}
	while (!work.empty()) {
		auto n = work.back();
		work.pop_back();
		for (auto e : _g.getDataInEdges(n)) { mark(_g.getDataEdges()[e].srcNode); }
	}
}

bool ParallelAnalysis::computeRegion(Block &b) {
	mem_std::Vector<uint32_t> work;
	bool grew = false;
	auto add = [&](uint32_t n, uint32_t reason) {
		if (b.body.test(n)) {
			return;
		}
		if (b.conc.set(n)) {
			b.reason[n] = reason;
			work.emplace_back(n);
			grew = true;
		}
	};

	// The spine.

	NodeSet spine;
	spine.init(_count);
	mem_std::Vector<uint32_t> delivered; // blocks whose barrier is on the spine
	{
		mem_std::Vector<uint32_t> stack;
		spine.set(b.fanOut);
		stack.emplace_back(b.fanOut);
		while (!stack.empty()) {
			auto n = stack.back();
			stack.pop_back();
			auto &node = _g.getNodeAt(n);
			if (node.op && node.op->joinsScope()) {
				for (uint32_t k = 0; k < uint32_t(_blocks.size()); ++k) {
					if (_blocks[k].barrier == n) {
						delivered.emplace_back(k);
					}
				}
			} else {
				for (auto e : _g.getExecInEdges(n)) {
					auto src = _g.getExecEdges()[e].srcNode;
					if (isTokenEdge(e) && inChain(b, _g.getNodeAt(src).scope) && spine.set(src)) {
						stack.emplace_back(src);
					}
				}
			}
			for (auto e : _g.getDataInEdges(n)) {
				auto &edge = _g.getDataEdges()[e];
				// A collector is pushed by its block's delivery, not by the value: the block is
				// delivered, as if its barrier were on the spine.
				if (node.op && edge.dstPin == node.op->getBranchPin()) {
					auto k = blockOf(edge.srcNode);
					if (k != InvalidIndex) {
						delivered.emplace_back(k);
					}
					continue;
				}
				if (inChain(b, _g.getNodeAt(edge.srcNode).scope) && spine.set(edge.srcNode)) {
					stack.emplace_back(edge.srcNode);
				}
			}
		}
	}

	// The path a lift raises: the spine in the fan-out's own scope, and what its body reads from
	// there.
	if (!b.liftKnown) {
		b.liftKnown = true;
		b.lift.init(_count);
		auto scope = _g.getNodeAt(b.fanOut).scope;
		for (uint32_t n = 0; n < _count; ++n) {
			if (_g.getNodeAt(n).scope == scope && (spine.test(n) || b.mustRan.test(n))) {
				b.lift.set(n);
			}
		}
	}

	// Token reachability without the barrier's outputs and without the outputs an exclusive spine
	// node did not take: what may hold a token while the block is in flight.
	{
		b.reach.init(_count);
		mem_std::Vector<uint32_t> stack;
		for (auto entry : _g.getEntryNodes()) {
			if (b.reach.set(entry)) {
				stack.emplace_back(entry);
			}
		}
		while (!stack.empty()) {
			auto n = stack.back();
			stack.pop_back();
			if (n == b.barrier) {
				continue;
			}
			auto &node = _g.getNodeAt(n);
			const bool exclusive = spine.test(n) && node.op
					&& (node.op->isExecExclusive() || node.op->getScopeExecOut() != 0);
			for (auto e : _g.getExecOutEdges(n)) {
				auto dst = _g.getExecEdges()[e].dstNode;
				if (!isTokenEdge(e) || (exclusive && !spine.test(dst))) {
					continue;
				}
				if (b.reach.set(dst)) {
					stack.emplace_back(dst);
				}
			}
		}
	}

	// The seeds.

	auto seedScope = [&](uint32_t scope) {
		for (uint32_t n = 0; n < _count; ++n) {
			if (_g.isScopeWithin(_g.getNodeAt(n).scope, scope)) {
				add(n, ReasonNone);
			}
		}
	};

	for (uint32_t p = 0; p < _count; ++p) {
		if (!spine.test(p)) {
			continue;
		}
		auto &node = _g.getNodeAt(p);
		if (!node.op) {
			continue;
		}

		// Under a lift, what used to drain before this node runs after it.
		const bool liftedHere = b.lifted && b.lift.test(p);

		// (a) the exec outputs fired beside the one that leads here
		if (!node.op->isExecExclusive() && node.op->getScopeExecOut() == 0) {
			auto outs = _g.getExecOutEdges(p);
			uint32_t kstar = InvalidIndex;
			for (uint32_t k = 0; k < uint32_t(outs.size()); ++k) {
				auto dst = _g.getExecEdges()[outs[k]].dstNode;
				if (isTokenEdge(outs[k]) && spine.test(dst)) {
					kstar = k;
					break;
				}
			}
			if (kstar != InvalidIndex) {
				for (uint32_t k = kstar + 1; k < uint32_t(outs.size()); ++k) {
					if (isTokenEdge(outs[k])) {
						add(_g.getExecEdges()[outs[k]].dstNode, outs[k]);
					}
				}
				// What the earlier outputs left unfinished: loops still turning, blocks in flight.
				NodeSet seen;
				seen.init(_count);
				mem_std::Vector<uint32_t> stack;
				for (uint32_t k = 0; k < kstar; ++k) {
					auto dst = _g.getExecEdges()[outs[k]].dstNode;
					if (!isTokenEdge(outs[k])) {
						continue;
					}
					if (liftedHere && !_weighted.test(dst)) {
						add(dst, outs[k]); // a sibling of weight 0 now waits under the lifted path
					} else if (seen.set(dst)) {
						stack.emplace_back(dst);
					}
				}
				while (!stack.empty()) {
					auto v = stack.back();
					stack.pop_back();
					auto &vn = _g.getNodeAt(v);
					if (vn.opensScope != InvalidIndex
							&& _g.getScopeAt(vn.opensScope).kind != ScopeKind::Function) {
						add(v, ReasonNone);
						seedScope(vn.opensScope);
						auto &opened = _g.getScopeAt(vn.opensScope);
						if (opened.barrier != InvalidIndex) {
							add(opened.barrier, ReasonNone);
						}
					}
					for (auto e : _g.getExecOutEdges(v)) {
						auto dst = _g.getExecEdges()[e].dstNode;
						if (isTokenEdge(e) && _g.getNodeAt(dst).scope == vn.scope && seen.set(dst)) {
							stack.emplace_back(dst);
						}
					}
				}
			}
		}

		// Under a lift, an eager consumer this node woke waits under the lifted path too.
		if (liftedHere) {
			for (auto x : _g.getDataOutEdges(p)) {
				auto dst = _g.getDataEdges()[x].dstNode;
				auto &dn = _g.getNodeAt(dst);
				if (dn.op && !dn.op->hasExecIn() && !spine.test(dst) && !b.mustRan.test(dst)
						&& !_weighted.test(dst)) {
					add(dst, x | ReasonData);
				}
			}
		}

		// (b) what a producer that pushed this node pushed beside it
		for (auto e : _g.getDataInEdges(p)) {
			auto d = _g.getDataEdges()[e].srcNode;
			if (dominates(d, p) || _g.getDataEdges()[e].dstPin == node.op->getBranchPin()) {
				continue;
			}
			for (auto x : _g.getExecOutEdges(d)) {
				if (isTokenEdge(x)) {
					add(_g.getExecEdges()[x].dstNode, x);
				}
			}
			for (auto x : _g.getDataOutEdges(d)) {
				auto dst = _g.getDataEdges()[x].dstNode;
				if (dst != p && !b.mustRan.test(dst) && mayWait(b, dst)) {
					add(dst, x | ReasonData);
				}
			}
		}
	}

	// (c) what the run or a turn put on the front first
	for (auto entry : _g.getEntryNodes()) {
		if (!b.mustRan.test(entry)) {
			add(entry, ReasonNone);
		}
	}
	for (uint32_t n = 0; n < _count; ++n) {
		auto &node = _g.getNodeAt(n);
		if (!node.op || node.op->hasExecIn() || node.scope == 0 || !inChain(b, node.scope)
				|| b.mustRan.test(n)) {
			continue;
		}
		bool local = false;
		for (auto e : _g.getDataInEdges(n)) {
			if (_g.getNodeAt(_g.getDataEdges()[e].srcNode).scope == node.scope) {
				local = true;
			}
		}
		if (!local) {
			add(n, ReasonNone);
		}
	}

	// (d) what was pending while an earlier block on the spine was in flight
	for (auto k : delivered) {
		auto &other = _blocks[k];
		if (&other == &b) {
			continue;
		}
		for (uint32_t n = 0; n < _count; ++n) {
			if (other.conc.test(n)) {
				add(n, other.reason[n]);
			}
		}
	}

	// The closure.

	mem_std::Vector<uint8_t> scopeDone(_g.getScopeCount(), uint8_t(0));
	while (!work.empty()) {
		auto x = work.back();
		work.pop_back();
		auto &node = _g.getNodeAt(x);

		// T1: tokens
		for (auto e : _g.getExecOutEdges(x)) {
			if (isTokenEdge(e)) {
				add(_g.getExecEdges()[e].dstNode, e);
			}
		}

		// T2: a node of a foreign body: the whole body, and its opener, which runs again when a
		// turn closes
		if (node.scope != 0 && !inChain(b, node.scope)) {
			auto top = node.scope;
			while (!inChain(b, _g.getScopeAt(top).parent)) { top = _g.getScopeAt(top).parent; }
			if (!scopeDone[top]) {
				scopeDone[top] = 1;
				seedScope(top);
				add(_g.getScopeAt(top).opener, ReasonNone);
			}
		}

		// T4: data, into nodes that have not certainly run already and may be waiting for it
		for (auto e : _g.getDataOutEdges(x)) {
			auto dst = _g.getDataEdges()[e].dstNode;
			if (!b.mustRan.test(dst) && mayWait(b, dst)) {
				add(dst, e | ReasonData);
			}
		}
	}

	return grew;
}

void ParallelAnalysis::computeAfter(Block &b) {
	mem_std::Vector<uint32_t> work;
	auto add = [&](uint32_t n) {
		if (!b.body.test(n) && !b.conc.test(n) && b.after.set(n)) {
			work.emplace_back(n);
		}
	};
	add(b.barrier);
	for (uint32_t n = 0; n < _count; ++n) {
		auto &node = _g.getNodeAt(n);
		auto pin = node.op ? node.op->getBranchPin() : NullPin;
		if (pin == NullPin) {
			continue;
		}
		for (auto e : _g.getDataInEdges(n)) {
			auto &edge = _g.getDataEdges()[e];
			if (edge.dstPin == pin && (b.body.test(edge.srcNode) || edge.srcNode == b.fanOut)) {
				add(n);
			}
		}
	}
	while (!work.empty()) {
		auto x = work.back();
		work.pop_back();
		for (auto e : _g.getExecOutEdges(x)) {
			if (isTokenEdge(e)) {
				add(_g.getExecEdges()[e].dstNode);
			}
		}
		for (auto e : _g.getDataOutEdges(x)) { add(_g.getDataEdges()[e].dstNode); }
	}
}

bool ParallelAnalysis::conflictOnce(uint32_t a, uint32_t b) {
	auto key = (uint64_t(sprt::min(a, b)) << 32) | uint64_t(sprt::max(a, b));
	return _reported.emplace(key).second;
}

void ParallelAnalysis::reportReason(DiagReport &report, DiagCode code, uint32_t node,
		uint32_t reason, DiagDetail key) {
	if (reason == ReasonNone) {
		report.reportNode(DiagSeverity::Error, code, _g.getNodeAt(node).id, DiagText(key));
	} else if ((reason & ReasonData) != 0) {
		auto &edge = _g.getDataEdges()[reason & ~ReasonData];
		auto &src = _g.getNodeAt(edge.srcNode);
		auto &dst = _g.getNodeAt(edge.dstNode);
		report.reportEdge(DiagSeverity::Error, code, src.id, src.op->getDataOut()[edge.srcPin].name,
				dst.id, dst.op->getDataIn()[edge.dstPin].name, DiagText(key));
	} else {
		auto &edge = _g.getExecEdges()[reason];
		auto &src = _g.getNodeAt(edge.srcNode);
		report.reportEdge(DiagSeverity::Error, code, src.id, src.op->getExecOut()[edge.srcPin],
				_g.getNodeAt(edge.dstNode).id, StringView(), DiagText(key));
	}
}

void ParallelAnalysis::check(Block &b, DiagReport &report) {
	auto idOf = [&](uint32_t n) { return int64_t(_g.getNodeAt(n).id); };

	// The body on its own.

	bool bodyWrites = false, bodyAccess = false;
	for (uint32_t n = 0; n < _count; ++n) {
		if (!b.body.test(n)) {
			continue;
		}
		auto &node = _g.getNodeAt(n);
		if (node.op->getScopeKind() == ScopeKind::Parallel) {
			report.reportNode(DiagSeverity::Error, DiagCode::ParallelNested, node.id,
					DiagText(DiagDetail::ParallelNested));
		}
		if (node.op->getParallel() == OpParallel::Serial) {
			report.reportNode(DiagSeverity::Error, DiagCode::ParallelSerialOp, node.id,
					DiagText(DiagDetail::ParallelSerialOp).name(node.op->getName()));
		}
		if (_dynamicRead[n] || _dynamicWrite[n]) {
			report.reportNode(DiagSeverity::Error, DiagCode::ParallelDynamicScene, node.id,
					DiagText(DiagDetail::ParallelDynamicBody));
		}
		for (uint32_t k = 0; k < _accessCount[n]; ++k) {
			auto &a = _accesses[_accessBegin[n] + k];
			bodyAccess = true;
			if (a.access != SceneAccess::Write) {
				continue;
			}
			bodyWrites = true;
			if (!a.field.empty() && !isOwn(b, a)) {
				report.reportNode(DiagSeverity::Error, DiagCode::ParallelForeignWrite, node.id,
						DiagText(DiagDetail::ParallelForeignWrite)
								.name(a.componentName)
								.name(a.field));
			}
		}
	}

	// Between branches: a field one branch writes on its own entity, another reads on someone
	// else's.
	for (uint32_t w = 0; w < _count; ++w) {
		if (!b.body.test(w)) {
			continue;
		}
		for (uint32_t i = 0; i < _accessCount[w]; ++i) {
			auto &write = _accesses[_accessBegin[w] + i];
			if (write.access != SceneAccess::Write || write.field.empty() || !isOwn(b, write)) {
				continue;
			}
			for (uint32_t r = 0; r < _count; ++r) {
				if (!b.body.test(r)) {
					continue;
				}
				for (uint32_t j = 0; j < _accessCount[r]; ++j) {
					auto &read = _accesses[_accessBegin[r] + j];
					if (read.access == SceneAccess::Read && read.component == write.component
							&& read.field == write.field && !isOwn(b, read) && conflictOnce(r, w)) {
						report.reportNode(DiagSeverity::Error, DiagCode::ParallelConflict,
								_g.getNodeAt(r).id,
								DiagText(DiagDetail::ParallelConflictBranches)
										.name(read.componentName)
										.name(read.field)
										.number(idOf(w)));
					}
				}
			}
		}
	}

	// The region.

	if (b.conc.test(b.fanOut)) {
		reportReason(report, DiagCode::ParallelReentry, b.fanOut, b.reason[b.fanOut],
				DiagDetail::ParallelReentry);
	}

	for (uint32_t n = 0; n < _count; ++n) {
		if (!b.body.test(n)) {
			continue;
		}
		for (auto e : _g.getDataInEdges(n)) {
			auto &edge = _g.getDataEdges()[e];
			// The fan-out's own outputs are the branch's; its running again is parallel-reentry.
			if (!b.body.test(edge.srcNode) && edge.srcNode != b.fanOut && b.conc.test(edge.srcNode)
					&& conflictOnce(edge.srcNode, n)) {
				reportReason(report, DiagCode::ParallelConflict, n, e | ReasonData,
						DiagDetail::ParallelConflictInput);
			}
		}
	}

	for (uint32_t x = 0; x < _count; ++x) {
		if (!b.conc.test(x)) {
			continue;
		}
		auto &node = _g.getNodeAt(x);
		if ((_dynamicWrite[x] && bodyAccess) || (_dynamicRead[x] && bodyWrites)) {
			if (conflictOnce(x, x)) {
				report.reportNode(DiagSeverity::Error, DiagCode::ParallelDynamicScene, node.id,
						DiagText(DiagDetail::ParallelDynamicConcurrent).number(idOf(b.fanOut)));
			}
		}
		for (uint32_t i = 0; i < _accessCount[x]; ++i) {
			auto &c = _accesses[_accessBegin[x] + i];
			for (uint32_t n = 0; n < _count; ++n) {
				if (!b.body.test(n)) {
					continue;
				}
				for (uint32_t j = 0; j < _accessCount[n]; ++j) {
					auto &a = _accesses[_accessBegin[n] + j];
					if (a.component != c.component) {
						continue;
					}
					bool conflict = false;
					auto key = DiagDetail::ParallelConflictConcurrent;
					if (c.field.empty()) {
						// Adding or removing a component moves the rows of a pool the block reads.
						conflict = c.access == SceneAccess::Write;
						key = DiagDetail::ParallelConflictStructure;
					} else if (a.field == c.field) {
						conflict = c.access == SceneAccess::Write || a.access == SceneAccess::Write;
					}
					if (conflict && conflictOnce(x, n)) {
						auto message = c.field.empty()
								? DiagText(key).name(c.componentName).number(idOf(n))
								: DiagText(key).name(c.componentName).name(c.field).number(idOf(n));
						report.reportNode(DiagSeverity::Error, DiagCode::ParallelConflict, node.id,
								message);
					}
				}
			}
		}
	}

	if (b.gpu) {
		checkGpu(b, report);
	}
	checkEnums(b, report);
}

void ParallelAnalysis::checkGpu(Block &b, DiagReport &report) {
	// The value pins of a node: what a shader would carry. A name, a family or a target is a
	// literal of the node or a row, not a value.
	auto isValueIn = [](const PinDesc &p) {
		switch (p.role) {
		case PinRole::ComponentName:
		case PinRole::ComponentNameOptional:
		case PinRole::FieldName:
		case PinRole::EnumFamily:
		case PinRole::EntityTarget:
		case PinRole::EntityName: return false;
		default: return true;
		}
	};

	// A conversion out of the GPU set's complement: one value in, outside the set, one value out,
	// inside it. Its input is where a segment's narrowing happens on the CPU.
	auto isNarrowing = [&](uint32_t n) {
		auto &op = *_g.getNodeAt(n).op;
		uint32_t ins = 0, outs = 0;
		VarType in = VarType::Nil, out = VarType::Nil;
		for (auto &p : op.getDataIn()) {
			if (isValueIn(p)) {
				++ins;
				in = p.type;
			}
		}
		for (auto &p : op.getDataOut()) {
			++outs;
			out = p.type;
		}
		return ins == 1 && outs == 1 && !isGpuValueType(in) && isGpuValueType(out);
	};

	// The value pin of a write to the branch's own entity: a widened value is committed on the CPU.
	auto isOwnWriteValue = [&](uint32_t n, uint32_t pin) {
		auto &op = *_g.getNodeAt(n).op;
		auto groups = op.getSceneGroups();
		for (uint32_t k = 0; k < _accessCount[n]; ++k) {
			auto &a = _accesses[_accessBegin[n] + k];
			if (a.access != SceneAccess::Write || !isOwn(b, a)) {
				continue;
			}
			for (auto &g : groups) {
				if (!g.valueIsOutput && g.valuePin == pin) {
					return true;
				}
			}
		}
		return false;
	};

	// The converse: one value in, inside the set, one value out, outside it. What it produces may
	// only be committed - to a field of the branch's own entity or to a collector.
	auto isWidening = [&](uint32_t n) {
		auto &op = *_g.getNodeAt(n).op;
		uint32_t ins = 0, outs = 0;
		VarType in = VarType::Nil, out = VarType::Nil;
		for (auto &p : op.getDataIn()) {
			if (isValueIn(p)) {
				++ins;
				in = p.type;
			}
		}
		for (auto &p : op.getDataOut()) {
			++outs;
			out = p.type;
		}
		return ins == 1 && outs == 1 && isGpuValueType(in) && !isGpuValueType(out);
	};

	auto consumerAccepts = [&](const RuntimeDataEdge &edge, bool entity) {
		auto &dst = _g.getNodeAt(edge.dstNode);
		auto &pin = dst.op->getDataIn()[edge.dstPin];
		if (entity) {
			return pin.role == PinRole::EntityTarget || pin.role == PinRole::BranchValue;
		}
		if (isNarrowing(edge.dstNode)) {
			return true;
		}
		return (pin.role == PinRole::BranchValue || isOwnWriteValue(edge.dstNode, edge.dstPin))
				&& isWidening(edge.srcNode);
	};

	// A committed input: a literal, a value of the set widened by the edge itself, or a widening.
	auto committedFrom = [&](uint32_t n, uint32_t pin) {
		for (auto e : _g.getDataInEdges(n)) {
			auto &edge = _g.getDataEdges()[e];
			if (edge.dstPin != pin) {
				continue;
			}
			auto &src = _g.getNodeAt(edge.srcNode);
			return isGpuValueType(src.op->getDataOut()[edge.srcPin].type) || isWidening(edge.srcNode);
		}
		return true;
	};

	auto typeFault = [&](uint32_t n, StringView pin, VarType type) {
		report.reportPin(DiagSeverity::Error, DiagCode::ParallelGpuType, _g.getNodeAt(n).id, pin,
				DiagText(DiagDetail::ParallelGpuType).type(type));
	};

	// The fan-out's entity is a row on the GPU: only a target may take it.
	for (auto e : _g.getDataOutEdges(b.fanOut)) {
		auto &edge = _g.getDataEdges()[e];
		if (edge.srcPin == b.entityPin && !consumerAccepts(edge, true)) {
			typeFault(edge.dstNode, _g.getNodeAt(edge.dstNode).op->getDataIn()[edge.dstPin].name,
					VarType::EntityRef);
		}
	}

	for (uint32_t n = 0; n < _count; ++n) {
		if (!b.body.test(n)) {
			continue;
		}
		auto &node = _g.getNodeAt(n);
		auto &op = *node.op;
		bool typesOk = true;

		if (node.opensScope != InvalidIndex) {
			report.reportNode(DiagSeverity::Error, DiagCode::ParallelGpuShape, node.id,
					DiagText(DiagDetail::ParallelGpuLoop));
		}
		for (auto e : _g.getExecOutEdges(n)) {
			auto &edge = _g.getExecEdges()[e];
			if (edge.backEdge) {
				report.reportEdge(DiagSeverity::Error, DiagCode::ParallelGpuShape, node.id,
						op.getExecOut()[edge.srcPin], _g.getNodeAt(edge.dstNode).id, StringView(),
						DiagText(DiagDetail::ParallelGpuCycle));
			}
		}

		auto ins = op.getDataIn();
		for (uint32_t p = 0; p < uint32_t(ins.size()); ++p) {
			if (!isValueIn(ins[p]) || isGpuValueType(ins[p].type)) {
				continue;
			}
			if (isNarrowing(n) || (isOwnWriteValue(n, p) && committedFrom(n, p))) {
				continue;
			}
			typesOk = false;
			typeFault(n, ins[p].name, ins[p].type);
		}

		auto outs = op.getDataOut();
		for (uint32_t p = 0; p < uint32_t(outs.size()); ++p) {
			if (isGpuValueType(outs[p].type)) {
				continue;
			}
			bool accepted = true;
			for (auto e : _g.getDataOutEdges(n)) {
				auto &edge = _g.getDataEdges()[e];
				if (edge.srcPin == p && !consumerAccepts(edge, false)) {
					accepted = false;
				}
			}
			if (!accepted) {
				typesOk = false;
				typeFault(n, outs[p].name, outs[p].type);
			}
		}

		if (node.localSchema) {
			auto fields = node.localSchema->getFields();
			for (size_t f = outs.size(); f < fields.size(); ++f) {
				if (!isGpuValueType(fields[f].type)) {
					typesOk = false;
					typeFault(n, fields[f].name, fields[f].type);
				}
			}
		}

		// A scene access through the target is a row the loader fills on the CPU, so an operation
		// that only reads or writes one needs no shader form of its own once its values fit.
		const bool rowAccess = op.getTargetPin() != NullPin
				&& (op.getParallel() == OpParallel::SceneRead
						|| op.getParallel() == OpParallel::SceneWrite);
		const auto form = classifyGpuOp(op);
		const bool lowerable = form != GpuForm::None
				&& ((op.getFlags() & OpFlags::ShaderForm) != OpFlags::None || (rowAccess && typesOk));
		if (!lowerable) {
			report.reportNode(DiagSeverity::Error, DiagCode::ParallelGpuOp, node.id,
					DiagText(DiagDetail::ParallelGpuOp).name(op.getName()));
		}
	}
}

void ParallelAnalysis::checkEnums(Block &b, DiagReport &report) {
	for (uint32_t n = 0; n < _count; ++n) {
		if (!b.body.test(n)) {
			continue;
		}
		auto &node = _g.getNodeAt(n);
		auto ins = node.op->getDataIn();
		auto outs = node.op->getDataOut();
		// An enum narrowed to Int32: one Enum input, one Int32 output.
		if (ins.size() != 1 || outs.size() != 1 || ins[0].type != VarType::Enum
				|| outs[0].type != VarType::Int32) {
			continue;
		}

		TypeId family = ins[0].subtypeId;
		for (auto e : _g.getDataInEdges(n)) {
			auto &edge = _g.getDataEdges()[e];
			auto &src = _g.getNodeAt(edge.srcNode);
			family = nodeDataOut(*src.op, edge.srcPin, src.family).subtypeId;
		}
		if (family == value::NullTypeId) {
			report.reportNode(DiagSeverity::Error, DiagCode::ParallelEnumRange, node.id,
					DiagText(DiagDetail::ParallelEnumNoFamily));
			continue;
		}
		if (!_scene) {
			continue;
		}
		auto type = _scene->getEnum(family);
		if (!type) {
			report.reportNode(DiagSeverity::Error, DiagCode::ParallelEnumRange, node.id,
					DiagText(DiagDetail::ParallelEnumNoFamily));
			continue;
		}
		for (auto &m : type->getMembers()) {
			if (m.value < int64_t(-2'147'483'647 - 1) || m.value > int64_t(2'147'483'647)) {
				report.reportNode(DiagSeverity::Error, DiagCode::ParallelEnumRange, node.id,
						DiagText(DiagDetail::ParallelEnumRange)
								.name(type->getName())
								.name(m.name)
								.number(int64_t(m.value)));
				break;
			}
		}
	}
}

void ParallelAnalysis::run(DiagReport *report) {
	for (uint32_t s = 1; s < _g.getScopeCount(); ++s) {
		auto &scope = _g.getScopeAt(s);
		if (scope.kind != ScopeKind::Parallel || scope.barrier == InvalidIndex) {
			continue;
		}
		Block b;
		b.fanOut = scope.opener;
		b.scope = s;
		b.barrier = scope.barrier;
		_blocks.emplace_back(sprt::move(b));
	}
	if (_blocks.empty()) {
		return;
	}
	// In node order, so that a refusal for a pair of blocks lands where the first one looks.
	sprt::sort(_blocks.begin(), _blocks.end(),
			[](const Block &a, const Block &b) { return a.fanOut < b.fanOut; });

	computeDominators();
	collectAccesses();
	_weighted.init(_count);
	for (auto &b : _blocks) {
		prepare(b);
		// A built graph describes itself under the lifts its weights record.
		b.lifted = _g.getNodeAt(b.fanOut).weight > 0;
	}
	for (uint32_t n = 0; n < _count; ++n) {
		if (_g.getNodeAt(n).weight > 0) {
			_weighted.set(n);
		}
	}
	computeAll();

	if (report) {
		for (auto &b : _blocks) { check(b, *report); }
	}
}

void ParallelAnalysis::resetRegions() {
	_reported.clear();
	for (auto &b : _blocks) {
		b.conc.init(_count);
		b.after.init(_count);
		b.reason.assign(_count, ReasonNone);
	}
}

void ParallelAnalysis::computeAll() {
	// (d) makes the regions depend on each other; they only grow, so this ends.
	bool grew = true;
	for (uint32_t round = 0; grew && round <= uint32_t(_blocks.size()); ++round) {
		grew = false;
		for (auto &b : _blocks) { grew = computeRegion(b) || grew; }
	}
	for (auto &b : _blocks) { computeAfter(b); }
}

bool ParallelAnalysis::accessesConflict(uint32_t x, uint32_t y) const {
	if (((_dynamicWrite[x] || _dynamicRead[x]) && _accessCount[y] > 0)
			|| ((_dynamicWrite[y] || _dynamicRead[y]) && _accessCount[x] > 0)) {
		return true;
	}
	for (uint32_t i = 0; i < _accessCount[x]; ++i) {
		auto &a = _accesses[_accessBegin[x] + i];
		for (uint32_t j = 0; j < _accessCount[y]; ++j) {
			auto &c = _accesses[_accessBegin[y] + j];
			if (a.component != c.component) {
				continue;
			}
			const bool aw = a.access == SceneAccess::Write;
			const bool cw = c.access == SceneAccess::Write;
			if ((a.field.empty() && aw) || (c.field.empty() && cw)) {
				return true; // adding or removing a component meets any access to it
			}
			if (!a.field.empty() && a.field == c.field && (aw || cw)) {
				return true;
			}
		}
	}
	return false;
}

// A lifted path runs before the work of weight 0 it used to follow: it must not touch what that
// work touches.
bool ParallelAnalysis::liftConflict(const Block &b, uint32_t &blocker) const {
	for (uint32_t x = 0; x < _count; ++x) {
		if (!b.conc.test(x) || _weighted.test(x)) {
			continue;
		}
		for (uint32_t l = 0; l < _count; ++l) {
			if (b.lift.test(l) && accessesConflict(l, x)) {
				blocker = x;
				return true;
			}
		}
	}
	return false;
}

void ParallelAnalysis::decideWeights(DiagReport &report, mem_std::Vector<uint8_t> &weights) {
	weights.assign(_count, uint8_t(0));
	for (uint32_t k = 0; k < uint32_t(_blocks.size()); ++k) {
		auto &candidate = _blocks[k];
		candidate.lifted = true;
		_weighted.init(_count);
		for (auto &b : _blocks) {
			if (b.lifted) {
				for (uint32_t n = 0; n < _count; ++n) {
					if (b.lift.test(n)) {
						_weighted.set(n);
					}
				}
			}
		}
		resetRegions();
		computeAll();

		value::DiagFirst scratch;
		DiagReport trial(&scratch);
		for (auto &b : _blocks) { check(b, trial); }
		uint32_t blocker = InvalidIndex;
		bool refused = trial.hasErrors() || liftConflict(candidate, blocker);
		if (refused && blocker == InvalidIndex && scratch.has()) {
			// A node locus or an edge locus: the first number is the node, or the edge's source.
			blocker = _g.findNode(NodeId(scratch.get().locusValue[0]));
		}
		if (refused) {
			candidate.lifted = false;
			auto text = DiagText(DiagDetail::ParallelPriorityDropped);
			if (blocker != InvalidIndex) {
				text.number(int64_t(_g.getNodeAt(blocker).id));
			} else {
				text.name(StringView("?"));
			}
			report.reportNode(DiagSeverity::Advice, DiagCode::ParallelPriorityDropped,
					_g.getNodeAt(candidate.fanOut).id, text);
		}
	}

	for (auto &b : _blocks) {
		if (!b.lifted) {
			continue;
		}
		for (uint32_t n = 0; n < _count; ++n) {
			if (b.lift.test(n)) {
				weights[n] = 1;
			}
		}
	}
}

void ParallelAnalysis::describe(mem_std::Value &out) const {
	out = mem_std::Value(mem_std::Value::Type::ARRAY);
	for (auto &b : _blocks) {
		mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
		entry.setInteger(int64_t(_g.getNodeAt(b.fanOut).id), "fanOut");
		entry.setInteger(int64_t(_g.getNodeAt(b.barrier).id), "barrier");
		if (b.gpu) {
			entry.setBool(true, "gpu");
		}
		auto list = [&](const NodeSet &set, StringView key) {
			auto &arr = entry.newArray(key);
			for (uint32_t n = 0; n < _count; ++n) {
				if (set.test(n)) {
					arr.addInteger(int64_t(_g.getNodeAt(n).id));
				}
			}
		};
		list(b.body, StringView("body"));
		list(b.conc, StringView("concurrent"));
		list(b.after, StringView("after"));
		out.addValue(sprt::move(entry));
	}
}

} // namespace

StringView getParallelFailureName(ParallelFailure f) {
	switch (f) {
	case ParallelFailure::CancelFrame: return StringView("cancelFrame");
	case ParallelFailure::Partial: return StringView("partial");
	case ParallelFailure::Nothing: return StringView("nothing");
	}
	return StringView("?");
}

SpanView<RuntimeBlockWrite> RuntimeGraph::getBlockWrites(uint32_t block) const {
	auto &b = _blocks[block];
	return SpanView<RuntimeBlockWrite>(_blockWrites.data() + b.writeBegin, b.writeCount);
}

SpanView<uint32_t> RuntimeGraph::getBlockCollectors(uint32_t block) const {
	auto &b = _blocks[block];
	return SpanView<uint32_t>(_blockCollectors.data() + b.collectorBegin, b.collectorCount);
}

SpanView<TypeId> RuntimeGraph::getBlockQuery(uint32_t block, bool without) const {
	auto &b = _blocks[block];
	return without ? SpanView<TypeId>(_blockQuery.data() + b.withoutBegin, b.withoutCount)
				   : SpanView<TypeId>(_blockQuery.data() + b.withBegin, b.withCount);
}

// What the machine needs to run a block, decided once: which scope a step's branch is, the branch
// header with a copy of every field the body writes on its own entity, the branch budgets, the
// collectors to wake at delivery and the query's component types.
void RuntimeGraph::deriveBlocks(DiagReport &report) {
	_blocks.clear();
	_blockWrites.clear();
	_blockCollectors.clear();
	_blockQuery.clear();

	for (uint32_t s = 0; s < uint32_t(_scopes.size()); ++s) {
		auto walk = s;
		while (walk != InvalidIndex && walk != 0 && _scopes[walk].kind != ScopeKind::Parallel) {
			walk = _scopes[walk].parent;
		}
		_scopes[s].branchScope = (walk != InvalidIndex && walk != 0) ? walk : InvalidIndex;
	}

	for (uint32_t s = 1; s < uint32_t(_scopes.size()); ++s) {
		auto &scope = _scopes[s];
		if (scope.kind != ScopeKind::Parallel || scope.barrier == InvalidIndex) {
			continue;
		}

		RuntimeBlock block;
		block.scope = s;
		block.fanOut = scope.opener;
		block.barrier = scope.barrier;
		auto &fanOut = _nodes[block.fanOut];
		fanOut.op->findDataOut(StringView("entity"), block.entityPin);
		fanOut.op->findDataOut(StringView("index"), block.indexPin);

		uint32_t index = 0;
		if (fanOut.op->findSetting(StringView("onFailure"), index)) {
			if (auto value = getSetting(block.fanOut, index)) {
				auto name = StringView(value->getString());
				if (name == getParallelFailureName(ParallelFailure::Partial)) {
					block.onFailure = ParallelFailure::Partial;
				} else if (name == getParallelFailureName(ParallelFailure::Nothing)) {
					block.onFailure = ParallelFailure::Nothing;
				}
			}
		}

		if (fanOut.op->findSetting(StringView("timeout"), index)) {
			if (auto value = getSetting(block.fanOut, index)) {
				block.timeoutMs = value->getInteger() > 0 ? uint32_t(value->getInteger()) : 0;
			}
		}

		uint32_t bodyNodes = 0;
		for (uint32_t n = 0; n < uint32_t(_nodes.size()); ++n) {
			if (isScopeWithin(_nodes[n].scope, s)) {
				++bodyNodes;
			}
		}
		block.maxSteps = bodyNodes * 4 + 16;
		block.maxActivations = bodyNodes * 8 + 64;

		// The fields a branch writes on its own entity, each once, in the order the body names
		// them.
		uint32_t cursor = BranchHeader::CopiesOffset;
		block.writeBegin = uint32_t(_blockWrites.size());
		for (uint32_t n = 0; n < uint32_t(_nodes.size()); ++n) {
			auto &node = _nodes[n];
			if (!node.op || !isScopeWithin(node.scope, s)) {
				continue;
			}
			auto target = node.op->getTargetPin();
			bool own = false;
			if (target != NullPin) {
				for (auto e : getDataInEdges(n)) {
					auto &edge = _dataEdges[e];
					if (edge.dstPin == target && edge.srcNode == block.fanOut
							&& edge.srcPin == block.entityPin) {
						own = true;
					}
				}
			}
			if (!own) {
				continue;
			}
			auto groups = node.op->getSceneGroups();
			auto contract = getSceneContract(n);
			for (uint32_t k = 0; k < uint32_t(contract.size()) && k < groups.size(); ++k) {
				auto &binding = contract[k];
				if (groups[k].access != SceneAccess::Write || binding.dynamic
						|| binding.field.empty() || binding.declared == VarType::Nil
						|| value::isContainerType(binding.declared)) {
					continue;
				}
				bool known = false;
				for (uint32_t w = block.writeBegin; w < uint32_t(_blockWrites.size()); ++w) {
					if (_blockWrites[w].componentId == binding.componentId
							&& _blockWrites[w].field == binding.field) {
						known = true;
					}
				}
				if (known) {
					continue;
				}
				RuntimeBlockWrite write;
				write.componentId = binding.componentId;
				write.component = binding.component;
				write.field = binding.field;
				write.type = binding.declared;
				write.subtypeId = binding.desc ? binding.desc->subtypeId : value::NullTypeId;
				write.flagOffset = cursor;
				write.valueOffset = value::alignUp(cursor + 1, value::getTypeAlign(write.type));
				cursor = write.valueOffset + value::getTypeSize(write.type);
				_blockWrites.emplace_back(write);
			}
		}
		block.writeCount = uint32_t(_blockWrites.size()) - block.writeBegin;
		scope.headerBytes = value::alignUp(cursor, 8);

		// The collectors a delivery wakes.
		block.collectorBegin = uint32_t(_blockCollectors.size());
		for (uint32_t n = 0; n < uint32_t(_nodes.size()); ++n) {
			auto &node = _nodes[n];
			auto pin = node.op ? node.op->getBranchPin() : NullPin;
			if (pin == NullPin) {
				continue;
			}
			for (auto e : getDataInEdges(n)) {
				auto &edge = _dataEdges[e];
				if (edge.dstPin == pin
						&& (edge.srcNode == block.fanOut || isScopeWithin(_nodes[edge.srcNode].scope, s))) {
					_blockCollectors.emplace_back(n);
				}
			}
		}
		block.collectorCount = uint32_t(_blockCollectors.size()) - block.collectorBegin;

		// The query, as types.
		auto query = [&](StringView key, uint32_t &begin, uint32_t &count) {
			begin = uint32_t(_blockQuery.size());
			count = 0;
			uint32_t setting = 0;
			if (!fanOut.op->findSetting(key, setting)) {
				return false;
			}
			if (auto value = getSetting(block.fanOut, setting)) {
				for (auto &item : value->asArray()) {
					_blockQuery.emplace_back(makeTypeId(StringView(item.getString())));
					++count;
				}
			}
			return true;
		};
		if (query(StringView("with"), block.withBegin, block.withCount) && block.withCount == 0) {
			report.reportSetting(DiagSeverity::Error, DiagCode::SettingInvalid, fanOut.id,
					StringView("with"),
					DiagText(DiagDetail::SettingInvalid)
							.name(StringView("with"))
							.phrase(DiagPhrase::SettingQueryEmpty));
		}
		query(StringView("without"), block.withoutBegin, block.withoutCount);

		scope.block = uint32_t(_blocks.size());
		_blocks.emplace_back(block);
	}
}

void RuntimeGraph::analyzeParallel(DiagReport &report, const value::TypeRegistry *scene) {
	ParallelAnalysis analysis(*this, scene);
	analysis.run(&report);
	if (report.hasErrors() || _blocks.empty()) {
		return;
	}
	mem_std::Vector<uint8_t> weights;
	analysis.decideWeights(report, weights);
	for (uint32_t n = 0; n < uint32_t(_nodes.size()); ++n) {
		_nodes[n].weight = weights[n];
	}
}

void RuntimeGraph::describeParallel(mem_std::Value &out) const {
	ParallelAnalysis analysis(*this, _sceneRegistry);
	analysis.run(nullptr);
	analysis.describe(out);
}

} // namespace stappler::flow
