/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#include "SPDataModel.h"

namespace STAPPLER_VERSIONIZED stappler::data {

// --- Model::Node --------------------------------------------------------------------------------

Model::Node::~Node() { }

bool Model::Node::init(Model *model, ItemId id, Kind kind) {
	_model = model;
	_id = id;
	_kind = kind;
	return true;
}

size_t Model::Node::getChildIndex() const {
	if (!_parent) {
		return maxOf<size_t>();
	}

	const auto &children = _parent->_children;
	for (size_t i = 0; i < children.size(); ++i) {
		if (children[i].get() == this) {
			return i;
		}
	}

	return maxOf<size_t>();
}

void Model::Node::setChildsCallback(ChildsCallback &&cb) {
	_childsCallback = sp::move(cb);
	// Installing a callback is what turns a category from "these are all the children there are"
	// into "ask me when you need them", so the state has to follow the callback both ways.
	_childsState = _childsCallback ? ChildsState::Pending : ChildsState::Empty;
}

bool Model::Node::requestChilds(Function<void()> &&onComplete) {
	switch (_childsState) {
	case ChildsState::Empty:
	case ChildsState::Loaded:
		if (onComplete) {
			onComplete();
		}
		return _childsState == ChildsState::Loaded;
	case ChildsState::Loading:
		if (onComplete) {
			_childsComplete.emplace_back(sp::move(onComplete));
		}
		return true;
	case ChildsState::Pending: break;
	}

	if (onComplete) {
		_childsComplete.emplace_back(sp::move(onComplete));
	}
	_childsState = ChildsState::Loading;

	// The completion may outlive every other reference to this node — a fetch that answers after the
	// view that asked for it went away — so it carries one of its own. resetChilds() is the escape
	// hatch for the completion that will never fire.
	auto linkId = sprt::retain(this);
	_childsCallback(this, [this, linkId, generation = _childsGeneration] {
		/* A completion from a load that has since been thrown away is dropped here rather than at
		its source, because only the node knows it was reset.

		Without this, an ANSWER THAT ARRIVES LATE corrupts the state machine: resetChilds() put the
		node back to Pending, a second load is already in flight, and the first one's completion
		would mark it Loaded and fire the second load's waiters over children that are not there
		yet. A loader that answers inline can never see it; one that hops to a worker sees it the
		first time a branch is refreshed while it is still listing. The reference is still released,
		so a dropped completion is not a leak. */
		if (generation != _childsGeneration) {
			sprt::release(this, linkId);
			return;
		}

		_childsState = ChildsState::Loaded;

		// Moved out before they run: a completion is free to ask for more children, and the vector
		// it would append to must not be the one being walked.
		auto complete = sp::move(_childsComplete);
		_childsComplete.clear();

		if (_model) {
			_model->setDirty(Flags(Update::ChildsLoaded));
		}
		for (auto &it : complete) { it(); }

		sprt::release(this, linkId);
	});

	return true;
}

void Model::Node::resetChilds() {
	if (!_childsCallback) {
		return;
	}

	_childsComplete.clear();
	_childsState = ChildsState::Pending;

	// Retires whatever is in flight: see the guard in requestChilds().
	++_childsGeneration;

	if (_model) {
		for (auto &it : _children) {
			_model->unindexSubtree(it);
			it->_parent = nullptr;
		}
	}
	_children.clear();

	if (_model) {
		_model->setDirty(Flags(Update::Structure));
	}
}

void Model::Node::setSpanCallback(SpanCallback &&cb) { _spanCallback = sp::move(cb); }

size_t Model::Node::getSpanData(const BatchCallback &cb, uint64_t first, size_t size) {
	if (_kind != Kind::Span || !_spanCallback || !cb || first >= _spanCount) {
		return 0;
	}

	// Clamped rather than refused: a view asking for a window that runs past the end of a span that
	// shrank under it is ordinary, and the rows past the end simply do not exist any more.
	const auto available = size_t(_spanCount - first);
	if (size > available) {
		size = available;
	}
	if (size == 0) {
		return 0;
	}

	_spanCallback(cb, first, size);
	return size;
}

// --- Model --------------------------------------------------------------------------------------

Model::~Model() { }

bool Model::init() { return init(Value()); }

bool Model::init(Value &&rootData) {
	_root = Rc<Node>::create(this, allocateId(), Kind::Category);
	if (!_root) {
		return false;
	}

	_root->_data = sp::move(rootData);
	_index.emplace(_root->getId(), _root.get());
	return true;
}

auto Model::getNode(ItemId id) const -> Node * {
	auto it = _index.find(id);
	return it != _index.end() ? it->second : nullptr;
}

auto Model::resolveParent(Node *parent) const -> Node * {
	if (!parent) {
		return _root;
	}
	// Only a category can hold children. Answering null rather than asserting keeps a caller that
	// walks a heterogeneous list from having to pre-check every node.
	return parent->isCategory() && parent->getModel() == this ? parent : nullptr;
}

bool Model::isAncestorOf(const Node *ancestor, const Node *node) {
	for (auto it = node; it; it = it->getParent()) {
		if (it == ancestor) {
			return true;
		}
	}
	return false;
}

void Model::indexSubtree(Node *node) {
	_index.emplace(node->getId(), node);
	for (auto &it : node->_children) { indexSubtree(it); }
}

void Model::unindexSubtree(Node *node) {
	_index.erase(node->getId());
	for (auto &it : node->_children) { unindexSubtree(it); }
}

void Model::attach(Node *node, Node *parent, size_t index) {
	auto &children = parent->_children;
	if (index > children.size()) {
		index = children.size();
	}
	node->_parent = parent;
	children.emplace(children.begin() + index, node);
}

// The caller MUST already hold an Rc on `node`: the vector slot being erased here is usually the
// last reference, and everything a caller does with the node afterwards would be a use-after-free.
sprt::pair<Model::Node *, size_t> Model::detach(Node *node) {
	auto parent = node->_parent;
	if (!parent) {
		return sprt::pair<Node *, size_t>(nullptr, maxOf<size_t>());
	}

	auto &children = parent->_children;
	for (size_t i = 0; i < children.size(); ++i) {
		if (children[i].get() == node) {
			children.erase(children.begin() + i);
			node->_parent = nullptr;
			return sprt::pair<Node *, size_t>(parent, i);
		}
	}

	return sprt::pair<Node *, size_t>(nullptr, maxOf<size_t>());
}

// True when the node is still part of this model's tree. Used by every deferred completion: an
// answer that arrives after its subject was removed must not resurrect it, and an id is never
// reused, so this cannot mistake a different element for the one that was asked about.
bool Model::isLive(const Node *node) const { return node && getNode(node->getId()) == node; }

auto Model::emplaceItem(Node *parent, size_t index, Value &&data, Ref *object) -> Node * {
	auto cat = resolveParent(parent);
	if (!cat) {
		return nullptr;
	}

	auto node = Rc<Node>::create(this, allocateId(), Kind::Item);
	if (!node) {
		return nullptr;
	}

	node->_data = sp::move(data);
	node->_object = object;

	attach(node, cat, index);
	_index.emplace(node->getId(), node.get());
	setDirty(Flags(Update::Structure));
	return node.get();
}

auto Model::emplaceCategory(Node *parent, size_t index, Value &&data, Ref *object) -> Node * {
	auto cat = resolveParent(parent);
	if (!cat) {
		return nullptr;
	}

	auto node = Rc<Node>::create(this, allocateId(), Kind::Category);
	if (!node) {
		return nullptr;
	}

	node->_data = sp::move(data);
	node->_object = object;

	attach(node, cat, index);
	_index.emplace(node->getId(), node.get());
	setDirty(Flags(Update::Structure));
	return node.get();
}

auto Model::emplaceSpan(Node *parent, size_t index, size_t count, SpanCallback &&cb) -> Node * {
	auto cat = resolveParent(parent);
	if (!cat) {
		return nullptr;
	}

	auto node = Rc<Node>::create(this, allocateId(), Kind::Span);
	if (!node) {
		return nullptr;
	}

	node->_spanCount = count;
	node->_spanCallback = sp::move(cb);

	attach(node, cat, index);
	_index.emplace(node->getId(), node.get());
	setDirty(Flags(Update::Structure));
	return node.get();
}

void Model::applyMove(Node *node, Node *dst, size_t index) {
	Rc<Node> ref(node);
	detach(node);
	attach(node, dst, index);
	setDirty(Flags(Update::Structure));
}

void Model::applyRemove(Node *node) {
	Rc<Node> ref(node);
	// Unindexed before it is detached, while the subtree is still walkable from here.
	unindexSubtree(node);
	detach(node);
	setDirty(Flags(Update::Structure));
}

bool Model::moveNode(Node *node, Node *dstParent, size_t index) {
	if (!node || node == _root || node->getModel() != this || !node->getParent()) {
		return false;
	}

	auto cat = resolveParent(dstParent);
	if (!cat) {
		return false;
	}

	/* A node may not become a child of its own descendant. The result would not be a tree: the cycle
	would be detached from the root, unreachable, and kept alive forever by its own refcounts, and
	every walk that entered it would not terminate. */
	if (isAncestorOf(node, cat)) {
		return false;
	}

	if (_slots.canMove && !_slots.canMove(node, cat, index)) {
		return false;
	}

	if (!_slots.performMove) {
		applyMove(node, cat, index);
		return true;
	}

	Rc<Model> self(this);
	Rc<Node> ref(node);
	Rc<Node> dst(cat);

	if (_policy == MutationPolicy::Confirmed) {
		_slots.performMove(node, cat, index, [self, ref, dst, index](Status st) {
			if (!sprt::status::isSuccessful(st) || !self->isLive(ref) || !self->isLive(dst)) {
				return;
			}
			self->applyMove(ref, dst, index);
		});
		return true;
	}

	// Optimistic: the model moves now, because a row that snapped back for the duration of a file
	// copy and then jumped forward again reads as a bug rather than as progress.
	auto origin = detach(node);
	Rc<Node> origParent(origin.first);
	const auto origIndex = origin.second;

	attach(node, cat, index);
	setDirty(Flags(Update::Structure));

	auto pending = Rc<PendingMutation>::alloc();
	pending->node = ref;
	pending->parent = origParent;
	pending->index = origIndex;
	_pending.emplace_back(pending);

	_slots.performMove(node, cat, index, [self, pending](Status st) {
		self->finishPending(pending, sprt::status::isSuccessful(st));
	});
	return true;
}

bool Model::removeNode(Node *node) {
	if (!node || node == _root || node->getModel() != this || !node->getParent()) {
		return false;
	}

	if (_slots.canRemove && !_slots.canRemove(node)) {
		return false;
	}

	if (!_slots.performRemove) {
		applyRemove(node);
		return true;
	}

	Rc<Model> self(this);
	Rc<Node> ref(node);

	if (_policy == MutationPolicy::Confirmed) {
		_slots.performRemove(node, [self, ref](Status st) {
			if (!sprt::status::isSuccessful(st) || !self->isLive(ref)) {
				return;
			}
			self->applyRemove(ref);
		});
		return true;
	}

	auto pending = Rc<PendingMutation>::alloc();
	pending->node = ref;
	pending->parent = node->getParent();
	pending->index = node->getChildIndex();
	pending->remove = true;
	_pending.emplace_back(pending);

	applyRemove(node);

	_slots.performRemove(node, [self, pending](Status st) {
		self->finishPending(pending, sprt::status::isSuccessful(st));
	});
	return true;
}

void Model::finishPending(PendingMutation *mutation, bool success) {
	// Held across the erase: the vector usually owns the last reference, and everything below reads
	// the mutation's fields.
	Rc<PendingMutation> hold(mutation);

	for (auto it = _pending.begin(); it != _pending.end(); ++it) {
		if (it->get() == mutation) {
			_pending.erase(it);
			break;
		}
	}

	if (success) {
		return;
	}

	auto parent = mutation->parent.get();
	if (!isLive(parent)) {
		// The place it came from is gone, so there is nowhere to put it back. Leaving it out is the
		// only consistent answer; the alternative would be re-attaching it to a detached subtree.
		return;
	}

	if (mutation->remove) {
		attach(mutation->node, parent, mutation->index);
		indexSubtree(mutation->node);
	} else if (isLive(mutation->node)) {
		applyMove(mutation->node, parent, mutation->index);
		return;
	}

	setDirty(Flags(Update::Structure));
}

void Model::clearChildren(Node *parent) {
	auto cat = resolveParent(parent);
	if (!cat || cat->_children.empty()) {
		return;
	}

	// Unindexed while the subtree is still walkable from here, then dropped in one go — the vector
	// holds the last reference to each child, so clearing it is what frees them.
	for (auto &it : cat->_children) {
		unindexSubtree(it);
		it->_parent = nullptr;
	}
	cat->_children.clear();

	setDirty(Flags(Update::Structure));
}

void Model::sortChildren(Node *parent, const Function<bool(const Node *, const Node *)> &cmp) {
	auto cat = resolveParent(parent);
	if (!cat || !cmp) {
		return;
	}

	// Reordering the vector leaves every id where it was, so a payload, an expansion or a selection
	// keyed by identity survives the sort. That is the difference between this and rebuilding the
	// children in the new order.
	sprt::sort(cat->_children.begin(), cat->_children.end(),
			[&](const Rc<Node> &l, const Rc<Node> &r) { return cmp(l.get(), r.get()); });

	setDirty(Flags(Update::Structure));
}

void Model::setNodeData(Node *node, Value &&value) {
	if (!node || node->getModel() != this) {
		return;
	}

	node->_data = sp::move(value);
	++node->_revision;
	setDirty(Flags(Update::Data));
}

void Model::setNodeObject(Node *node, Ref *object) {
	if (!node || node->getModel() != this) {
		return;
	}

	node->_object = object;
	++node->_revision;
	setDirty(Flags(Update::Data));
}

void Model::setSpanCount(Node *node, size_t count) {
	if (!node || node->getModel() != this || node->getKind() != Kind::Span
			|| node->_spanCount == count) {
		return;
	}

	node->_spanCount = count;
	setDirty(Flags(Update::Structure));
}

void Model::setSlots(Slots &&slots) { _slots = sp::move(slots); }

void Model::setDirty(Flags flags) { Subscription::setDirty(flags); }

} // namespace stappler::data
