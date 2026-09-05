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

#include "XLUiDockTree.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

DockLayoutSpec DockLayoutSpec::leaf(Vector<String> &&panels, DockFrameParams &&params) {
	DockLayoutSpec spec;
	spec.isSplit = false;
	spec.params = sp::move(params);
	spec.panels = sp::move(panels);
	return spec;
}

DockLayoutSpec DockLayoutSpec::hsplit(float ratio, DockLayoutSpec &&left, DockLayoutSpec &&right) {
	DockLayoutSpec spec;
	spec.isSplit = true;
	spec.axis = DockAxis::Horizontal;
	spec.ratio = ratio;
	spec.children.emplace_back(sp::move(left));
	spec.children.emplace_back(sp::move(right));
	return spec;
}

DockLayoutSpec DockLayoutSpec::vsplit(float ratio, DockLayoutSpec &&top, DockLayoutSpec &&bottom) {
	DockLayoutSpec spec;
	spec.isSplit = true;
	spec.axis = DockAxis::Vertical;
	spec.ratio = ratio;
	spec.children.emplace_back(sp::move(top));
	spec.children.emplace_back(sp::move(bottom));
	return spec;
}

void DockTree::clear() {
	_nodes.clear();
	_free.clear();
	_root = DockNodeHandle();
}

void DockTree::setRoot(DockNodeHandle h) {
	_root = h;
	if (auto n = get(h)) {
		n->parent = DockNodeHandle();
	}
}

bool DockTree::isValid(DockNodeHandle h) const {
	if (h.empty() || h.index >= _nodes.size()) {
		return false;
	}
	auto &n = _nodes[h.index];
	return n.kind != DockTreeNode::Kind::Free && n.generation == h.generation;
}

DockTreeNode *DockTree::get(DockNodeHandle h) { return isValid(h) ? &_nodes[h.index] : nullptr; }

const DockTreeNode *DockTree::get(DockNodeHandle h) const {
	return isValid(h) ? &_nodes[h.index] : nullptr;
}

DockTreeNode &DockTree::at(DockNodeHandle h) {
	sprt_passert(isValid(h), "DockTree: stale handle");
	return _nodes[h.index];
}

const DockTreeNode &DockTree::at(DockNodeHandle h) const {
	sprt_passert(isValid(h), "DockTree: stale handle");
	return _nodes[h.index];
}

DockNodeHandle DockTree::allocate() {
	DockNodeHandle h;
	if (!_free.empty()) {
		h.index = _free.back();
		_free.pop_back();
		// the slot was reset on release; only the generation carries over
		h.generation = _nodes[h.index].generation;
	} else {
		h.index = uint32_t(_nodes.size());
		h.generation = 1;
		_nodes.emplace_back();
		_nodes.back().generation = 1;
	}
	_nodes[h.index].self = h;
	return h;
}

void DockTree::release(DockNodeHandle h) {
	if (!isValid(h)) {
		return;
	}
	auto &n = _nodes[h.index];
	const uint32_t nextGeneration = n.generation + 1;

	n = DockTreeNode();
	n.kind = DockTreeNode::Kind::Free;
	n.generation = nextGeneration;

	_free.emplace_back(h.index);
}

void DockTree::releaseSubtree(DockNodeHandle h) {
	auto n = get(h);
	if (!n) {
		return;
	}
	if (n->isSplit()) {
		const auto first = n->first;
		const auto second = n->second;
		releaseSubtree(first);
		releaseSubtree(second);
	}
	release(h);
}

void DockTree::replaceChild(DockNodeHandle parent, DockNodeHandle oldChild,
		DockNodeHandle newChild) {
	auto p = get(parent);
	if (!p || !p->isSplit()) {
		return;
	}
	if (p->first == oldChild) {
		p->first = newChild;
	} else if (p->second == oldChild) {
		p->second = newChild;
	}
	if (auto c = get(newChild)) {
		c->parent = parent;
	}
}

DockNodeHandle DockTree::makeLeaf(DockFrameParams &&params, Vector<String> &&panels,
		size_t active) {
	auto h = allocate();
	auto &n = _nodes[h.index];
	n.kind = DockTreeNode::Kind::Leaf;
	n.params = sp::move(params);
	n.panels = sp::move(panels);
	n.active = n.panels.empty() ? 0 : sprt::min(active, n.panels.size() - 1);
	return h;
}

DockNodeHandle DockTree::makeSplit(DockAxis axis, float ratio, DockNodeHandle first,
		DockNodeHandle second) {
	if (!isValid(first) || !isValid(second)) {
		return DockNodeHandle();
	}

	auto h = allocate();
	auto &n = _nodes[h.index];
	n.kind = DockTreeNode::Kind::Split;
	n.axis = axis;
	n.ratio = sprt::clamp(ratio, 0.0f, 1.0f);
	n.first = first;
	n.second = second;

	_nodes[first.index].parent = h;
	_nodes[second.index].parent = h;
	return h;
}

bool DockTree::validateSpec(const DockLayoutSpec &spec) {
	if (!spec.isSplit) {
		return true;
	}
	if (spec.children.size() != 2) {
		return false;
	}
	return validateSpec(spec.children[0]) && validateSpec(spec.children[1]);
}

DockNodeHandle DockTree::buildSpec(const DockLayoutSpec &spec, DockNodeHandle parent) {
	DockNodeHandle h;
	if (spec.isSplit) {
		auto first = buildSpec(spec.children[0], DockNodeHandle());
		auto second = buildSpec(spec.children[1], DockNodeHandle());
		h = makeSplit(spec.axis, spec.ratio, first, second);
	} else {
		auto params = spec.params;
		auto panels = spec.panels;
		h = makeLeaf(sp::move(params), sp::move(panels), spec.active);
		if (auto n = get(h)) {
			n->collapsed = spec.collapsed;
		}
	}
	if (auto n = get(h)) {
		n->parent = parent;
	}
	return h;
}

bool DockTree::build(const DockLayoutSpec &spec) {
	if (!validateSpec(spec)) {
		log::source().error("DockTree",
				"malformed layout spec: a split needs exactly two children");
		return false;
	}

	clear();
	_root = buildSpec(spec, DockNodeHandle());
	return !_root.empty();
}

DockNodeHandle DockTree::splitLeaf(DockNodeHandle leaf, DockAxis axis, bool firstIsNew,
		DockFrameParams &&params, float ratio) {
	auto target = get(leaf);
	if (!target || !target->isLeaf()) {
		return DockNodeHandle();
	}
	if (!hasFlag(target->params.flags, DockFrameFlags::AllowSplit)) {
		return DockNodeHandle();
	}

	const auto parent = target->parent;
	const bool wasRoot = (leaf == _root);

	auto created = makeLeaf(sp::move(params), Vector<String>());

	// `target` and every other pointer into the arena may have been invalidated by the allocation
	auto split = firstIsNew ? makeSplit(axis, ratio, created, leaf)
							: makeSplit(axis, ratio, leaf, created);
	if (split.empty()) {
		release(created);
		return DockNodeHandle();
	}

	if (wasRoot) {
		setRoot(split);
	} else {
		replaceChild(parent, leaf, split);
		at(split).parent = parent;
	}
	return created;
}

bool DockTree::collapseLeaf(DockNodeHandle leaf) {
	auto target = get(leaf);
	if (!target || !target->isLeaf()) {
		return false;
	}
	if (hasFlag(target->params.flags, DockFrameFlags::Permanent) || !target->panels.empty()) {
		return false;
	}
	if (leaf == _root) {
		return false; // the last parking place stays, empty or not
	}

	const auto parentHandle = target->parent;
	auto parent = get(parentHandle);
	if (!parent || !parent->isSplit()) {
		return false;
	}

	const auto sibling = (parent->first == leaf) ? parent->second : parent->first;
	const auto grandparent = parent->parent;
	const bool parentWasRoot = (parentHandle == _root);

	release(leaf);
	release(parentHandle);

	if (parentWasRoot) {
		setRoot(sibling);
	} else {
		replaceChild(grandparent, parentHandle, sibling);
	}
	return true;
}

DockNodeHandle DockTree::findLeafAt(const Vec2 &point) const {
	auto h = _root;
	while (auto n = get(h)) {
		if (n->isLeaf()) {
			return n->rect.containsPoint(point) ? h : DockNodeHandle();
		}
		// the divider band belongs to neither child: pick the side the point is on, so a drop
		// exactly over a splitter still resolves to a frame instead of nothing
		auto first = get(n->first);
		if (!first) {
			return DockNodeHandle();
		}
		if (n->axis == DockAxis::Horizontal) {
			h = (point.x < first->rect.getMaxX()) ? n->first : n->second;
		} else {
			// Vertical: `first` is the TOP child, so it owns the higher Y
			h = (point.y >= first->rect.origin.y) ? n->first : n->second;
		}
	}
	return DockNodeHandle();
}

DockNodeHandle DockTree::findFrameByName(StringView name) const {
	if (name.empty()) {
		return DockNodeHandle();
	}
	DockNodeHandle result;
	each([&](const DockTreeNode &n) {
		if (result.empty() && n.isLeaf() && n.params.name == name) {
			result = n.self;
		}
	});
	return result;
}

DockNodeHandle DockTree::findFrameForPanel(StringView panelId) const {
	DockNodeHandle result;
	each([&](const DockTreeNode &n) {
		if (!result.empty() || !n.isLeaf()) {
			return;
		}
		for (auto &it : n.panels) {
			if (it == panelId) {
				result = n.self;
				return;
			}
		}
	});
	return result;
}

DockNodeHandle DockTree::findLargestLeaf() const {
	DockNodeHandle result;
	float bestArea = -1.0f;
	each([&](const DockTreeNode &n) {
		if (!n.isLeaf()) {
			return;
		}
		const float area = n.rect.size.width * n.rect.size.height;
		if (area > bestArea) {
			bestArea = area;
			result = n.self;
		}
	});
	return result;
}

size_t DockTree::getLeafCount() const {
	size_t count = 0;
	each([&](const DockTreeNode &n) {
		if (n.isLeaf()) {
			++count;
		}
	});
	return count;
}

void DockTree::updateMinimumsAt(DockNodeHandle h, const MeasureLeaf &measure, float thickness) {
	auto n = get(h);
	if (!n) {
		return;
	}

	if (n->isLeaf()) {
		const bool collapsed = n->collapsed;
		const Size2 content = measure ? measure(*n) : Size2::ZERO;
		// re-read: the callback runs arbitrary code and may have touched the arena
		n = get(h);
		if (collapsed) {
			/* THE DECLARED FLOOR IS DROPPED TOO, and that is the point rather than an oversight.

			`params.minSize` is what the application reserved for the place's CONTENT - a sidebar
			says 250pt because a tree needs 250pt. A shut rail is showing no content, so honouring
			that floor would leave the divider stuck exactly where it was and the collapse would do
			nothing visible. What is left is the strip, which measureLeaf still reports. */
			n->minSize = content;
			return;
		}
		n->minSize = Size2(sprt::max(n->params.minSize.width, content.width),
				sprt::max(n->params.minSize.height, content.height));
		return;
	}

	const auto first = n->first;
	const auto second = n->second;
	const auto axis = n->axis;
	updateMinimumsAt(first, measure, thickness);
	updateMinimumsAt(second, measure, thickness);

	n = get(h);
	auto a = get(first);
	auto b = get(second);
	if (!a || !b) {
		n->minSize = Size2::ZERO;
		return;
	}

	// The divider is part of the split's own cost: leaving it out would let the tree claim it fits
	// into less than it does, and distribute() would then hand out negative extents.
	if (axis == DockAxis::Horizontal) {
		n->minSize.width = a->minSize.width + b->minSize.width + thickness;
		n->minSize.height = sprt::max(a->minSize.height, b->minSize.height);
	} else {
		n->minSize.width = sprt::max(a->minSize.width, b->minSize.width);
		n->minSize.height = a->minSize.height + b->minSize.height + thickness;
	}
}

void DockTree::updateMinimums(const MeasureLeaf &measure, float thickness) {
	updateMinimumsAt(_root, measure, thickness);
}

Size2 DockTree::getRootMinSize() const {
	auto n = get(_root);
	return n ? n->minSize : Size2::ZERO;
}

float DockTree::minAlongAxis(DockNodeHandle h, DockAxis axis) const {
	auto n = get(h);
	if (!n) {
		return 0.0f;
	}
	return (axis == DockAxis::Horizontal) ? n->minSize.width : n->minSize.height;
}

void DockTree::distributeAt(DockNodeHandle h, const Rect &rect, DockOverflowPolicy policy,
		float thickness) {
	auto n = get(h);
	if (!n) {
		return;
	}

	n->rect = rect;
	if (n->isLeaf()) {
		return;
	}

	const bool horizontal = (n->axis == DockAxis::Horizontal);
	const auto first = n->first;
	const auto second = n->second;
	const float ratio = sprt::clamp(n->ratio, 0.0f, 1.0f);

	const float extent = horizontal ? rect.size.width : rect.size.height;
	const float usable = sprt::max(extent - thickness, 0.0f);

	float minA = minAlongAxis(first, n->axis);
	float minB = minAlongAxis(second, n->axis);

	float a = 0.0f;
	float b = 0.0f;
	if (minA + minB > usable) {
		if (policy == DockOverflowPolicy::Scale) {
			// everything stays visible and inside the root; the layout snaps back exactly once
			// the root grows past the tree's minimum again
			const float total = minA + minB;
			const float k = (total > 0.0f) ? (usable / total) : 0.0f;
			a = minA * k;
			b = usable - a;
		} else {
			// Clip: both minimums are honoured and the tail runs outside the rect
			a = minA;
			b = minB;
		}
	} else {
		const float free = usable - minA - minB;
		a = minA + free * ratio;
		b = usable - a;
	}

	a = sprt::max(a, 0.0f);
	b = sprt::max(b, 0.0f);

	Rect firstRect;
	Rect splitRect;
	Rect secondRect;
	if (horizontal) {
		// `first` is the left child; X simply increases
		firstRect = Rect(rect.origin.x, rect.origin.y, a, rect.size.height);
		splitRect = Rect(rect.origin.x + a, rect.origin.y, thickness, rect.size.height);
		secondRect = Rect(rect.origin.x + a + thickness, rect.origin.y, b, rect.size.height);
	} else {
		// Y points up, so `first` - the TOP child - starts at the high end of the rect
		firstRect = Rect(rect.origin.x, rect.origin.y + b + thickness, rect.size.width, a);
		splitRect = Rect(rect.origin.x, rect.origin.y + b, rect.size.width, thickness);
		secondRect = Rect(rect.origin.x, rect.origin.y, rect.size.width, b);
	}

	// `n` still points into the arena here - distribute never allocates, so nothing can move -
	// but the recursion below rewrites the children, so the divider band is stored first
	n->splitterRect = splitRect;

	distributeAt(first, firstRect, policy, thickness);
	distributeAt(second, secondRect, policy, thickness);
}

void DockTree::distribute(const Rect &available, DockOverflowPolicy policy, float thickness) {
	distributeAt(_root, available, policy, thickness);
}

void DockTree::each(const Callback<void(DockTreeNode &)> &cb) {
	for (auto &it : _nodes) {
		if (it.kind != DockTreeNode::Kind::Free) {
			cb(it);
		}
	}
}

void DockTree::each(const Callback<void(const DockTreeNode &)> &cb) const {
	for (auto &it : _nodes) {
		if (it.kind != DockTreeNode::Kind::Free) {
			cb(it);
		}
	}
}

void DockTree::eachInOrderAt(DockNodeHandle h,
		const Callback<void(const DockTreeNode &)> &cb) const {
	auto n = get(h);
	if (!n) {
		return;
	}
	cb(*n);
	if (n->isSplit()) {
		eachInOrderAt(n->first, cb);
		eachInOrderAt(n->second, cb);
	}
}

void DockTree::eachInOrder(const Callback<void(const DockTreeNode &)> &cb) const {
	eachInOrderAt(_root, cb);
}

// --- persistence -----------------------------------------------------------

namespace {

StringView DockTree_axisName(DockAxis axis) {
	return (axis == DockAxis::Horizontal) ? StringView("h") : StringView("v");
}

StringView DockTree_sideName(DockTabBarSide side) {
	switch (side) {
	case DockTabBarSide::Top: return StringView("top");
	case DockTabBarSide::Bottom: return StringView("bottom");
	case DockTabBarSide::Left: return StringView("left");
	case DockTabBarSide::Right: return StringView("right");
	}
	return StringView("top");
}

DockTabBarSide DockTree_readSide(StringView name) {
	if (name == "bottom") {
		return DockTabBarSide::Bottom;
	} else if (name == "left") {
		return DockTabBarSide::Left;
	} else if (name == "right") {
		return DockTabBarSide::Right;
	}
	return DockTabBarSide::Top;
}

} // namespace

Value DockTree::saveNode(DockNodeHandle h) const {
	Value ret;
	auto n = get(h);
	if (!n) {
		return ret;
	}

	if (n->isSplit()) {
		ret.setString("split", "type");
		// a string, not the enum's integer: the format has to survive a reordering of DockAxis
		ret.setString(DockTree_axisName(n->axis), "axis");
		ret.setDouble(n->ratio, "ratio");
		ret.setValue(saveNode(n->first), "first");
		ret.setValue(saveNode(n->second), "second");
		return ret;
	}

	ret.setString("frame", "type");
	if (!n->params.name.empty()) {
		ret.setString(n->params.name, "name");
	}
	// the frame's DECLARED floor only; the propagated one is recomputed on restore
	ret.setValue(Value{Value(n->params.minSize.width), Value(n->params.minSize.height)}, "min");
	ret.setInteger(toInt(n->params.flags), "flags");
	ret.setString(DockTree_sideName(n->params.tabBarSide), "tabBar");

	Value panels;
	for (auto &id : n->panels) { panels.addString(id); }
	ret.setValue(sp::move(panels), "panels");
	ret.setInteger(n->active, "active");
	// Written only when it is TRUE - a file that says nothing about a frame's shut-ness describes an
	// open one, which is what every layout written before this existed meant.
	if (n->collapsed) {
		ret.setBool(true, "collapsed");
	}
	return ret;
}

Value DockTree::save() const {
	Value ret;
	ret.setInteger(SaveVersion, "version");
	ret.setValue(saveNode(_root), "root");
	return ret;
}

bool DockTree::readSpec(const Value &src, DockLayoutSpec &out,
		const Callback<bool(StringView)> &isPanelKnown) {
	const auto type = src.getString("type");

	if (type == "split") {
		if (!src.hasValue("first") || !src.hasValue("second")) {
			log::source().error("DockTree", "restore: a split needs both children");
			return false;
		}
		out.isSplit = true;
		out.axis = (src.getString("axis") == "v") ? DockAxis::Vertical : DockAxis::Horizontal;
		out.ratio = sprt::clamp(float(src.getDouble("ratio", 0.5)), 0.0f, 1.0f);
		out.children.resize(2);
		return readSpec(src.getValue("first"), out.children[0], isPanelKnown)
				&& readSpec(src.getValue("second"), out.children[1], isPanelKnown);
	}

	if (type != "frame") {
		log::source().error("DockTree", "restore: unknown node type '", type, "'");
		return false;
	}

	out.isSplit = false;
	out.params.name = src.getString("name");
	out.params.flags = DockFrameFlags(src.getInteger("flags", toInt(DockFrameFlags::Default)));
	out.params.tabBarSide = DockTree_readSide(src.getString("tabBar"));

	const auto &min = src.getValue("min");
	if (min.isArray() && min.size() >= 2) {
		out.params.minSize = Size2(float(min.getDouble(0)), float(min.getDouble(1)));
	}

	const auto &panels = src.getValue("panels");
	if (panels.isArray()) {
		for (auto &it : panels.asArray()) {
			auto id = it.getString();
			if (id.empty()) {
				continue;
			}
			if (!isPanelKnown || !isPanelKnown(id)) {
				// a downgraded application, or a feature that was removed: not fatal
				log::source().warn("DockTree", "restore: unknown panel '", id, "', dropped");
				continue;
			}
			bool duplicate = false;
			for (auto &kept : out.panels) {
				if (kept == id) {
					duplicate = true;
					break;
				}
			}
			if (duplicate) {
				log::source().warn("DockTree", "restore: panel '", id, "' appears twice, dropped");
				continue;
			}
			out.panels.emplace_back(id);
		}
	}

	const auto active = size_t(sprt::max(src.getInteger("active", 0), int64_t(0)));
	out.active = out.panels.empty() ? 0 : sprt::min(active, out.panels.size() - 1);
	out.collapsed = src.getBool("collapsed");
	return true;
}

void DockTree::pruneEmptyLeaves() {
	// A leaf can end up empty because every panel it held was dropped as unknown. Collapsing is
	// iterative on purpose: merging one leaf away can leave its former parent's sibling as the
	// only child, and the pass has to see the tree that results, not the one it started with.
	bool changed = true;
	while (changed) {
		changed = false;
		DockNodeHandle victim;
		each([&](DockTreeNode &n) {
			if (victim.empty() && n.isLeaf() && n.panels.empty()
					&& !hasFlag(n.params.flags, DockFrameFlags::Permanent)) {
				victim = n.self;
			}
		});
		if (!victim.empty()) {
			changed = collapseLeaf(victim);
		}
	}
}

bool DockTree::restore(const Value &value, const Callback<bool(StringView)> &isPanelKnown) {
	const auto version = value.getInteger("version", 0);
	if (version <= 0 || version > SaveVersion) {
		log::source().error("DockTree", "restore: unsupported layout version ", version);
		return false;
	}
	if (!value.hasValue("root")) {
		log::source().error("DockTree", "restore: no root node");
		return false;
	}

	// Build a spec first and validate it whole. Nothing here touches the live tree, so a malformed
	// save leaves the layout on screen exactly as it was.
	DockLayoutSpec spec;
	if (!readSpec(value.getValue("root"), spec, isPanelKnown)) {
		return false;
	}
	if (!validateSpec(spec)) {
		return false;
	}

	if (!build(spec)) {
		return false;
	}
	pruneEmptyLeaves();
	return true;
}

} // namespace stappler::xenolith::ui
