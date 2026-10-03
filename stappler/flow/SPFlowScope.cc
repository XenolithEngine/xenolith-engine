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

// The half of loop bodies that belongs to the graph: working out which nodes make up one. A scope
// static and an activation is dynamic; this file decides the static half once, at build time,
// because the run makes no structural decisions and because working it out per iteration
// would mean walking the graph on every turn of every loop. The rules, in order:
//   1. Walk the exec edges from the entry points. An edge out of a pin its operation declared as
//      opening a scope puts everything it reaches into a new scope, a child of the one the source
//      is in. Every other edge keeps the scope it came from.
//   2. Nodes nothing told to run - the eager ones - take the deepest scope among the nodes that
//      feed them, to a fixed point. A node that reads the loop's item belongs to the loop; one
//      that reads nothing from inside it does not, and is computed once.
//   3. Refuse what has no answer: a node reached as part of two different bodies (scope-conflict),
//      and an edge leading out of a body (scope-escape).
// Rule 3's second half is a decision rather than an oversight: an accumulator written by the body
// and read after the loop cannot be a data edge, because the reader would have to name one of many
// activations and there is no honest answer to which. It goes in the scene, or in a local of the
// node that opens the loop.

namespace STAPPLER_VERSIONIZED stappler::flow {

SpanView<uint32_t> RuntimeGraph::getScopeNodes(uint32_t scope) const {
	if (scope >= _scopes.size()) {
		return SpanView<uint32_t>();
	}
	auto &s = _scopes[scope];
	return SpanView<uint32_t>(_scopeNodes.data() + s.nodeBegin, s.nodeCount);
}

bool RuntimeGraph::isScopeWithin(uint32_t scope, uint32_t ancestor) const {
	while (scope != InvalidIndex) {
		if (scope == ancestor) {
			return true;
		}
		if (scope >= _scopes.size()) {
			return false;
		}
		scope = _scopes[scope].parent;
	}
	return false;
}

void RuntimeGraph::assignScopes(DiagReport &report) {
	auto nodeCount = uint32_t(_nodes.size());

	_scopes.clear();
	_scopeNodes.clear();

	// Scope 0 is the graph. It exists even for an empty graph, so that "the scope of a node" is
	// always a question with an answer.
	_scopes.emplace_back(RuntimeScope());

	if (nodeCount == 0) {
		return;
	}

	mem_std::Vector<uint32_t> scopeOf(nodeCount, InvalidIndex);

	// The scope a given (opener, pin) opens - created on first use so that a body reached twice
	// through the same pin is one body and not two.
	auto childScope = [&](uint32_t opener, uint32_t pin) -> uint32_t {
		for (uint32_t i = 1; i < uint32_t(_scopes.size()); ++i) {
			if (_scopes[i].opener == opener && _scopes[i].execPin == pin) {
				return i;
			}
		}
		RuntimeScope scope;
		scope.opener = opener;
		scope.execPin = pin;
		scope.parent = scopeOf[opener];
		scope.depth = _scopes[scope.parent].depth + 1;
		scope.kind = _nodes[opener].op->getScopeKind();
		_scopes.emplace_back(scope);
		return uint32_t(_scopes.size()) - 1;
	};

	// Is `node` the node that opens `scope` or any scope enclosing it? An exec edge back to one of
	// those is a loop closing, or a nested body handing control to the loop outside it - both
	// legal, and neither a reason to re-scope the target.
	auto isEnclosingOpener = [&](uint32_t node, uint32_t scope) {
		while (scope != InvalidIndex && scope != 0) {
			if (_scopes[scope].opener == node) {
				return true;
			}
			scope = _scopes[scope].parent;
		}
		return false;
	};

	// The parallel scope `scope` is, or the nearest one enclosing it; InvalidIndex for none.
	auto parallelOf = [&](uint32_t scope) {
		while (scope != InvalidIndex && scope != 0) {
			if (_scopes[scope].kind == ScopeKind::Parallel) {
				return scope;
			}
			scope = _scopes[scope].parent;
		}
		return InvalidIndex;
	};

	auto isBarrier = [&](uint32_t node) { return _nodes[node].op && _nodes[node].op->joinsScope(); };

	// A barrier already refused as the second one of a body: nothing more is said about it.
	mem_std::Vector<uint8_t> refusedBarrier(nodeCount, uint8_t(0));

	// The exec walk. A barrier is the exception to "an edge keeps the scope it came from": an edge
	// out of a parallel body into it closes the block, and the barrier sits in the body's parent.

	mem_std::Vector<uint32_t> stack;
	for (auto entry : _entryNodes) {
		if (scopeOf[entry] == InvalidIndex) {
			scopeOf[entry] = 0;
			stack.emplace_back(entry);
		}
	}

	while (!stack.empty()) {
		auto n = stack.back();
		stack.pop_back();
		auto here = scopeOf[n];
		auto &node = _nodes[n];
		if (!node.op) {
			continue;
		}

		for (auto e : getExecOutEdges(n)) {
			auto &edge = _execEdges[e];
			auto target = edge.dstNode;
			auto want = node.op->opensScope(edge.srcPin) ? childScope(n, edge.srcPin) : here;
			if (want == here && isBarrier(target) && parallelOf(here) != InvalidIndex
					&& _scopes[here].kind != ScopeKind::Parallel) {
				continue; // a nested loop's body closing the block; refused in step 3
			}
			if (want == here && here != 0 && isBarrier(target)
					&& _scopes[here].kind == ScopeKind::Parallel) {
				want = _scopes[here].parent;
				auto &block = _scopes[here];
				if (block.barrier == InvalidIndex) {
					block.barrier = target;
				} else if (block.barrier != target) {
					report.reportEdge(DiagSeverity::Error, DiagCode::ParallelUnpaired, node.id,
							node.op->getExecOut()[edge.srcPin], _nodes[target].id, StringView(),
							DiagText(DiagDetail::ParallelTwoBarriers));
					refusedBarrier[target] = 1;
					continue;
				}
			}

			if (scopeOf[target] == InvalidIndex) {
				scopeOf[target] = want;
				stack.emplace_back(target);
				continue;
			}
			if (scopeOf[target] == want || isEnclosingOpener(target, want)) {
				continue;
			}
			report.reportEdge(DiagSeverity::Error, DiagCode::ScopeConflict, node.id,
					node.op->getExecOut()[edge.srcPin], _nodes[target].id, StringView(),
					DiagText(DiagDetail::ScopeConflict));
		}
	}

	// The eager ones, by where their values come from. The scope a producer contributes is not
	// always the scope it is in: a node that opens a loop sits outside the body, but the value it
	// hands out - the item of this iteration - is produced once per turn, so whatever is computed
	// from it belongs to the body. Anything else would give such a node one record for the whole
	// loop, and it would run once, on the first item.
	auto contributedScope = [&](uint32_t producer) {
		for (uint32_t i = 1; i < uint32_t(_scopes.size()); ++i) {
			if (_scopes[i].opener == producer) {
				return i;
			}
		}
		return scopeOf[producer];
	};

	bool changed = true;
	while (changed) {
		changed = false;
		for (uint32_t n = 0; n < nodeCount; ++n) {
			if (scopeOf[n] != InvalidIndex || !_nodes[n].op) {
				continue;
			}
			// A node with an exec input that nothing reaches can never fire; validation has already
			// warned about it. Give it the root scope so that everything has one.
			uint32_t deepest = InvalidIndex;
			bool waiting = false;
			for (auto e : getDataInEdges(n)) {
				auto src = _dataEdges[e].srcNode;
				if (scopeOf[src] == InvalidIndex) {
					waiting = true;
					break;
				}
				auto from = contributedScope(src);
				// A collector reads the values of all branches, once, after the block: it belongs
				// to the block's parent, not to the body its value comes from.
				if (_dataEdges[e].dstPin == _nodes[n].op->getBranchPin()
						&& _scopes[from].kind == ScopeKind::Parallel) {
					from = _scopes[from].parent;
				}
				if (deepest == InvalidIndex || _scopes[from].depth > _scopes[deepest].depth) {
					deepest = from;
				}
			}
			if (waiting) {
				continue; // its producers are not placed yet; another pass will get to it
			}
			scopeOf[n] = deepest == InvalidIndex ? 0 : deepest;
			changed = true;
		}
	}

	for (uint32_t n = 0; n < nodeCount; ++n) {
		if (scopeOf[n] == InvalidIndex) {
			scopeOf[n] = 0; // unreachable, or waiting on a producer that is itself unreachable
		}
		_nodes[n].scope = scopeOf[n];
		_nodes[n].opensScope = InvalidIndex;
	}
	for (uint32_t i = 1; i < uint32_t(_scopes.size()); ++i) {
		_nodes[_scopes[i].opener].opensScope = i;
	}

	// Edges that leave a body.

	for (auto &edge : _dataEdges) {
		auto from = scopeOf[edge.srcNode];
		auto to = scopeOf[edge.dstNode];
		auto &src = _nodes[edge.srcNode];
		auto &dst = _nodes[edge.dstNode];
		auto locate = [&](DiagCode code, DiagDetail key) {
			report.reportEdge(DiagSeverity::Error, code, src.id,
					src.op->getDataOut()[edge.srcPin].name, dst.id,
					dst.op->getDataIn()[edge.dstPin].name, DiagText(key));
		};

		if (edge.dstPin == dst.op->getBranchPin()) {
			// A collector's input comes out of a parallel body - the body's nodes or the fan-out's
			// own per-branch outputs - into the block's parent, and from nowhere else.
			auto body = contributedScope(edge.srcNode);
			if (body == 0 || _scopes[body].kind != ScopeKind::Parallel) {
				locate(DiagCode::ParallelUnpaired, DiagDetail::ParallelCollectorNoBlock);
			} else if (to != _scopes[body].parent) {
				locate(DiagCode::ParallelEscape, DiagDetail::ParallelEscapeValue);
			}
			continue;
		}

		// What a fan-out hands out is one value per branch, and only its body can read one.
		auto opened = src.opensScope != InvalidIndex ? src.opensScope : contributedScope(edge.srcNode);
		if (opened != from && _scopes[opened].kind == ScopeKind::Parallel
				&& !isScopeWithin(to, opened)) {
			locate(DiagCode::ParallelEscape, DiagDetail::ParallelEscapeValue);
			continue;
		}

		if (isScopeWithin(to, from)) {
			continue; // same scope, or the consumer is deeper: it reads an enclosing value
		}
		auto block = parallelOf(from);
		if (block != InvalidIndex && !isScopeWithin(to, block)) {
			locate(DiagCode::ParallelEscape, DiagDetail::ParallelEscapeValue);
		} else {
			locate(DiagCode::ScopeEscape, DiagDetail::ScopeEscapeValue);
		}
	}

	for (auto &edge : _execEdges) {
		auto from = scopeOf[edge.srcNode];
		auto to = scopeOf[edge.dstNode];
		auto &src = _nodes[edge.srcNode];
		auto block = parallelOf(from);
		if (isBarrier(edge.dstNode)) {
			if (refusedBarrier[edge.dstNode]) {
				continue;
			}
			if (block != InvalidIndex && from == block && _scopes[block].barrier == edge.dstNode
					&& to == _scopes[block].parent) {
				continue; // the body closing into its barrier
			}
			// Into a barrier from anywhere but its own body: the block would close on a token that
			// is not the end of a branch.
			report.reportEdge(DiagSeverity::Error, DiagCode::ParallelEscape, src.id,
					src.op ? src.op->getExecOut()[edge.srcPin] : StringView(),
					_nodes[edge.dstNode].id, StringView(),
					DiagText(DiagDetail::ParallelBarrierEntry));
			continue;
		}
		if (to == from) {
			continue;
		}
		if (src.op && src.op->opensScope(edge.srcPin) && _scopes[to].opener == edge.srcNode
				&& _scopes[to].execPin == edge.srcPin) {
			continue; // the edge that opens the body
		}
		if (isEnclosingOpener(edge.dstNode, from)
				&& (block == InvalidIndex || isScopeWithin(to, block))) {
			continue; // the loop closing, or a nested body handing back to an outer one
		}
		const bool parallel = block != InvalidIndex && !isScopeWithin(to, block);
		report.reportEdge(DiagSeverity::Error,
				parallel ? DiagCode::ParallelEscape : DiagCode::ScopeEscape, src.id,
				src.op ? src.op->getExecOut()[edge.srcPin] : StringView(), _nodes[edge.dstNode].id,
				StringView(),
				DiagText(parallel ? DiagDetail::ParallelEscapeExec : DiagDetail::ScopeEscapeExec));
	}

	// Pairs: a block closes into exactly one barrier, a barrier closes exactly one block.

	for (uint32_t i = 1; i < uint32_t(_scopes.size()); ++i) {
		auto &scope = _scopes[i];
		if (scope.kind == ScopeKind::Parallel && scope.barrier == InvalidIndex) {
			report.reportNode(DiagSeverity::Error, DiagCode::ParallelUnpaired,
					_nodes[scope.opener].id, DiagText(DiagDetail::ParallelNoBarrier));
		}
	}
	for (uint32_t n = 0; n < nodeCount; ++n) {
		if (!isBarrier(n) || refusedBarrier[n]) {
			continue;
		}
		uint32_t blocks = 0;
		for (uint32_t i = 1; i < uint32_t(_scopes.size()); ++i) {
			if (_scopes[i].barrier == n) {
				++blocks;
			}
		}
		if (blocks != 1) {
			report.reportNode(DiagSeverity::Error, DiagCode::ParallelUnpaired, _nodes[n].id,
					DiagText(blocks == 0 ? DiagDetail::ParallelBarrierNoBlock
										 : DiagDetail::ParallelTwoBlocks));
		}
	}

	// The node list of each scope, in index order.

	_scopeNodes.reserve(nodeCount);
	for (uint32_t s = 0; s < uint32_t(_scopes.size()); ++s) {
		_scopes[s].nodeBegin = uint32_t(_scopeNodes.size());
		for (uint32_t n = 0; n < nodeCount; ++n) {
			if (scopeOf[n] == s) {
				// The node's slot in its own scope, taken here because this is the loop that
				// decides the order. The interpreter turns (activation, slot) into a record address
				// without searching for anything, so this number is the whole of what makes that
				// possible.
				_nodes[n].slotInScope = uint32_t(_scopeNodes.size()) - _scopes[s].nodeBegin;
				_scopeNodes.emplace_back(n);
			}
		}
		_scopes[s].nodeCount = uint32_t(_scopeNodes.size()) - _scopes[s].nodeBegin;
	}
}

} // namespace stappler::flow
