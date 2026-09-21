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

#include "XLUiDockSystem.h"

#include "XLInheritedStyle.h" // isInlineRtl
#include "XLUiDockSplitter.h"
#include "XLUiLayoutSystem.h"
#include "XLUiStyleSystem.h"
#include "XLSelectionSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

ComponentId DockFrameComponent::Id;

uint64_t DockSystem::SystemFrameTag = System::GetNextSystemId();

bool DockSystem::init() { return init(Rc<PanelRegistry>::create()); }

bool DockSystem::init(Rc<PanelRegistry> &&registry) {
	if (!registry) {
		return false;
	}
	if (!System::init()) {
		return false;
	}

	_registry = sp::move(registry);

	_systemPriority = DockDefaultPriority;

	// HandleChildNodeEvents: panel content growing in a frame; AddToFrameStack: publish the dock to
	// its subtree; HandleMeasure: a fit-content ancestor can size around the dock
	setSystemFlags(SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents
			| SystemFlags::HandleNodeEvents | SystemFlags::HandleLayoutChildren
			| SystemFlags::HandleMeasure | SystemFlags::HandleChildNodeEvents
			| SystemFlags::AddToFrameStack | SystemFlags::HandleVisitSelf);
	setFrameTag(SystemFrameTag);
	return true;
}

void DockSystem::handleAdded(Node *owner) {
	System::handleAdded(owner);

	sprt_passert(owner->getSystemByType<LayoutSystem>() == nullptr,
			"DockSystem owns its children's geometry: the dock root must not carry a LayoutSystem");

	// A CSS width/height on a frame becomes a MeasureComponent hint instead of a committed size,
	// and `display: flex` on the root cannot add a second geometry writer.
	owner->setComponent<SystemManagedLayout>();

	// one drop target for the whole dock; frames are resolved by walking the split tree
	setDropTarget(owner,
			DropTargetSlots{
				.accept = [this](const DragEvent &event) { return handleDragAccept(event); },
				.enter = [this](const DragEvent &event) { handleDragEnter(event); },
				.over = [this](const DragEvent &event) { handleDragOver(event); },
				.leave = [this](const DragEvent &event) { handleDragLeave(event); },
				.drop = [this](const DragEvent &event,
								DragActions action) { return handleDragDrop(event, action); },
			});

	_registry->addHost(this);

	syncNodes();
	invalidateLayout();
}

void DockSystem::detachPanelsUnder(const Set<Node *> &roots) {
	if (roots.empty()) {
		return;
	}

	// Detach registry-owned panel nodes from subtrees about to be cleaned, without cleanup:
	// Node::cleanup() would destroy their systems while the registry still holds the nodes. A plain
	// detach fires handleExit, and re-parenting replays handleEnter. Panels sit in a frame's body,
	// so walk the whole ancestor chain; only nodes under `roots` are touched, as the registry may
	// be shared.
	_registry->foreachContent([&](StringView, Node *node) {
		for (auto p = node->getParent(); p != nullptr; p = p->getParent()) {
			if (roots.find(p) != roots.end()) {
				node->removeFromParent(false);
				return;
			}
		}
	});
}

void DockSystem::handleRemoved() {
	if (_owner) {
		detachPanelsUnder(Set<Node *>{_owner});

		_tree.each([&](DockTreeNode &n) {
			if (n.node) {
				n.node->removeFromParent(true);
				n.node = nullptr;
			}
		});
	}

	// release all claims; the panels keep their content in the registry
	_registry->releaseHost(this);

	System::handleRemoved();
}

DockSystem *DockSystem::findForNode(Node *node) {
	for (auto p = node; p != nullptr; p = p->getParent()) {
		if (auto sys = p->getSystemByType<DockSystem>()) {
			return sys;
		}
	}
	return nullptr;
}

// --- registry --------------------------------------------------------------

void DockSystem::registerPanel(DockPanelDescriptor &&desc) {
	_registry->registerPanel(sp::move(desc));
}

void DockSystem::unregisterPanel(StringView id) { _registry->unregisterPanel(id); }

// --- structure -------------------------------------------------------------

bool DockSystem::setLayout(const DockLayoutSpec &spec) {
	if (!_tree.build(spec)) {
		return false;
	}

	// drop panel ids the registry does not know; not fatal
	_tree.each([&](DockTreeNode &n) {
		if (!n.isLeaf()) {
			return;
		}
		Vector<String> kept;
		kept.reserve(n.panels.size());
		for (auto &id : n.panels) {
			if (getPanelDescriptor(id)) {
				kept.emplace_back(id);
			} else {
				log::source().warn("ui::DockSystem", "unknown panel '", id, "' in the layout");
			}
		}
		n.panels = sp::move(kept);
		n.active = n.panels.empty() ? 0 : sprt::min(n.active, n.panels.size() - 1);
	});

	syncNodes();
	invalidateLayout();
	return true;
}

DockNodeHandle DockSystem::findFrameByName(StringView name) const {
	return _tree.findFrameByName(name);
}

DockNodeHandle DockSystem::findFrameForPanel(StringView panelId) const {
	return _tree.findFrameForPanel(panelId);
}

DockFrame *DockSystem::getFrameNode(DockNodeHandle h) const {
	auto n = _tree.get(h);
	if (!n || !n->isLeaf()) {
		return nullptr;
	}
	return static_cast<DockFrame *>(n->node.get());
}

SpanView<String> DockSystem::getPanelsInFrame(DockNodeHandle h) const {
	auto n = _tree.get(h);
	return (n && n->isLeaf()) ? SpanView<String>(n->panels) : SpanView<String>();
}

// --- panels ----------------------------------------------------------------

bool DockSystem::isPanelOpen(StringView id) const { return !_tree.findFrameForPanel(id).empty(); }

bool DockSystem::openPanel(StringView id, DockNodeHandle target, size_t index) {
	auto desc = getPanelDescriptor(id);
	if (!desc) {
		log::source().error("ui::DockSystem", "openPanel: unknown panel '", id, "'");
		return false;
	}

	if (auto current = _tree.findFrameForPanel(id); !current.empty()) {
		// already parked somewhere: honour an explicit target, otherwise just bring it forward
		return target.empty() ? activatePanel(id) : movePanel(id, target, index);
	}

	if (target.empty()) {
		target = _tree.findFrameByName(desc->defaultFrame);
	}
	if (target.empty()) {
		target = _tree.findLargestLeaf();
	}

	auto leaf = _tree.get(target);
	if (!leaf || !leaf->isLeaf()) {
		log::source().error("ui::DockSystem", "openPanel: no frame to park '", id, "' in");
		return false;
	}

	const size_t at = sprt::min(index, leaf->panels.size());
	leaf->panels.emplace(leaf->panels.begin() + at, id.str<Interface>());
	leaf->active = at;

	updateFrameContent(*leaf);
	invalidateLayout();

	if (_panelOpenedCallback) {
		_panelOpenedCallback(id);
	}
	return true;
}

bool DockSystem::closePanel(StringView id) { return takePanelOut(id, true); }

void DockSystem::releasePanel(StringView id) {
	// same as a close, but not reported: the panel is moving to another container, and the registry
	// hands the node over
	takePanelOut(id, false);
}

bool DockSystem::takePanelOut(StringView id, bool notify) {
	auto h = _tree.findFrameForPanel(id);
	auto leaf = _tree.get(h);
	if (!leaf) {
		return false;
	}

	auto it = sprt::find(leaf->panels.begin(), leaf->panels.end(), id);
	if (it == leaf->panels.end()) {
		return false;
	}

	const size_t removed = size_t(it - leaf->panels.begin());
	leaf->panels.erase(it);
	if (leaf->panels.empty()) {
		leaf->active = 0;
	} else if (leaf->active >= leaf->panels.size()) {
		leaf->active = leaf->panels.size() - 1;
	} else if (leaf->active > removed) {
		--leaf->active;
	}

	updateFrameContent(*leaf);

	// an emptied frame folds away and its sibling takes the space, unless it is Permanent
	if (leaf->panels.empty()) {
		_tree.collapseLeaf(h);
		syncNodes();
	}
	invalidateLayout();

	if (notify && _panelClosedCallback) {
		_panelClosedCallback(id);
	}
	return true;
}

bool DockSystem::activatePanel(StringView id) {
	auto h = _tree.findFrameForPanel(id);
	auto leaf = _tree.get(h);
	if (!leaf) {
		return false;
	}

	auto it = sprt::find(leaf->panels.begin(), leaf->panels.end(), id);
	if (it == leaf->panels.end()) {
		return false;
	}

	const size_t index = size_t(it - leaf->panels.begin());
	if (leaf->active == index) {
		return true;
	}
	leaf->active = index;
	updateFrameContent(*leaf);
	invalidateLayout();

	if (_panelActivatedCallback) {
		_panelActivatedCallback(id);
	}
	return true;
}

void DockSystem::handlePanelTapped(StringView id) {
	if (_panelTapCallback) {
		_panelTapCallback(id);
	}
}

bool DockSystem::movePanel(StringView id, DockNodeHandle target, size_t index) {
	auto targetLeaf = _tree.get(target);
	if (!targetLeaf || !targetLeaf->isLeaf()) {
		return false;
	}
	if (!getPanelDescriptor(id)) {
		return false;
	}

	const auto source = _tree.findFrameForPanel(id);
	if (source == target) {
		// a reorder inside one frame: take it out first, then insert at the requested slot
		auto &leaf = _tree.at(target);
		auto it = sprt::find(leaf.panels.begin(), leaf.panels.end(), id);
		if (it == leaf.panels.end()) {
			return false;
		}
		leaf.panels.erase(it);
		const size_t at = sprt::min(index, leaf.panels.size());
		leaf.panels.emplace(leaf.panels.begin() + at, id.str<Interface>());
		leaf.active = at;
		updateFrameContent(leaf);
		invalidateLayout();
		return true;
	}

	if (auto sourceLeaf = _tree.get(source)) {
		auto it = sprt::find(sourceLeaf->panels.begin(), sourceLeaf->panels.end(), id);
		if (it != sourceLeaf->panels.end()) {
			sourceLeaf->panels.erase(it);
			if (!sourceLeaf->panels.empty() && sourceLeaf->active >= sourceLeaf->panels.size()) {
				sourceLeaf->active = sourceLeaf->panels.size() - 1;
			}
			updateFrameContent(*sourceLeaf);
		}
	}

	// re-read: the source may be the target's sibling, and the collapse below can move it
	auto &leaf = _tree.at(target);
	const size_t at = sprt::min(index, leaf.panels.size());
	leaf.panels.emplace(leaf.panels.begin() + at, id.str<Interface>());
	leaf.active = at;
	updateFrameContent(leaf);

	if (auto sourceLeaf = _tree.get(source); sourceLeaf && sourceLeaf->panels.empty()) {
		_tree.collapseLeaf(source);
		syncNodes();
	}
	invalidateLayout();
	return true;
}

// --- frames ----------------------------------------------------------------

DockNodeHandle DockSystem::splitFrame(DockNodeHandle frame, DockAxis axis, bool firstIsNew,
		const DockFrameParams &params, float ratio) {
	auto source = _tree.get(frame);
	if (!source || !source->isLeaf()) {
		return DockNodeHandle();
	}

	// a new frame inherits the source's flags and tab side unless the caller set its own; the name
	// is never inherited
	auto next = params;
	if (next.minSize == Size2::ZERO && next.flags == DockFrameFlags::Default) {
		next.flags = source->params.flags;
		next.tabBarSide = source->params.tabBarSide;
	}

	auto created = _tree.splitLeaf(frame, axis, firstIsNew, sp::move(next), ratio);
	if (created.empty()) {
		return created;
	}

	syncNodes();
	invalidateLayout();
	return created;
}

DockNodeHandle DockSystem::splitFrameWithPanel(DockNodeHandle frame, DockAxis axis, bool firstIsNew,
		StringView panelId, float ratio) {
	auto created = splitFrame(frame, axis, firstIsNew, DockFrameParams(), ratio);
	if (created.empty()) {
		return created;
	}
	if (!panelId.empty()) {
		movePanel(panelId, created);
	}
	return created;
}

bool DockSystem::closeFrame(DockNodeHandle h) {
	auto leaf = _tree.get(h);
	if (!leaf || !leaf->isLeaf()) {
		return false;
	}

	// close what is parked here first; the last one to go folds the place away
	auto panels = leaf->panels;
	for (auto &id : panels) { closePanel(id); }

	if (_tree.isValid(h)) {
		if (!_tree.collapseLeaf(h)) {
			return false;
		}
		syncNodes();
		invalidateLayout();
	}
	return true;
}

bool DockSystem::setFrameParams(DockNodeHandle h, const DockFrameParams &params) {
	auto leaf = _tree.get(h);
	if (!leaf || !leaf->isLeaf()) {
		return false;
	}
	if (leaf->params == params) {
		return true;
	}

	leaf->params = params;

	if (auto frame = static_cast<DockFrame *>(leaf->node.get())) {
		frame->setParams(params);
		// tabs too: the close affordance follows DockFrameFlags::AllowClose
		updateFrameTabs(*leaf);
	}

	invalidateLayout();
	return true;
}

bool DockSystem::setFrameCollapsed(DockNodeHandle h, bool value) {
	auto leaf = _tree.get(h);
	if (!leaf || !leaf->isLeaf()) {
		return false;
	}
	if (leaf->collapsed == value) {
		return true;
	}

	leaf->collapsed = value;

	if (auto frame = static_cast<DockFrame *>(leaf->node.get())) {
		frame->setCollapsed(value);
	}

	invalidateLayout();
	return true;
}

bool DockSystem::isFrameCollapsed(DockNodeHandle h) const {
	auto leaf = _tree.get(h);
	return leaf && leaf->isLeaf() && leaf->collapsed;
}

// --- persistence -----------------------------------------------------------

Value DockSystem::save() const { return _tree.save(); }

bool DockSystem::restore(const Value &value) {
	if (!_tree.restore(value,
				[this](StringView id) { return getPanelDescriptor(id) != nullptr; })) {
		return false;
	}

	// if every panel was dropped, keep one default frame rather than none
	if (_tree.empty()) {
		_tree.build(DockLayoutSpec::leaf(Vector<String>()));
	}

	syncNodes();

	// panels absent from the file stay closed, except OpenByDefault ones
	for (auto &it : _registry->getPanelDescriptors()) {
		if (!hasFlag(it.second.flags, DockPanelFlags::OpenByDefault)) {
			continue;
		}
		if (isPanelOpen(it.first)) {
			continue;
		}
		// held by another host: do not take it; with a shared registry that panel is not ours
		if (auto host = _registry->getHost(it.first); host != nullptr && host != this) {
			continue;
		}
		openPanel(it.first);
	}

	invalidateLayout();
	return true;
}

// --- parameters ------------------------------------------------------------

void DockSystem::setSplitterThickness(float value) {
	value = sprt::max(value, 0.0f);
	if (value != _splitterThickness) {
		_splitterThickness = value;
		invalidateLayout();
	}
}

void DockSystem::setOverflowPolicy(DockOverflowPolicy policy) {
	if (policy != _overflowPolicy) {
		_overflowPolicy = policy;
		invalidateLayout();
	}
}

void DockSystem::setLayoutChangedCallback(LayoutChangedCallback &&cb) {
	_layoutChangedCallback = sp::move(cb);
}

void DockSystem::setPanelOpenedCallback(PanelCallback &&cb) { _panelOpenedCallback = sp::move(cb); }

void DockSystem::setPanelClosedCallback(PanelCallback &&cb) { _panelClosedCallback = sp::move(cb); }

void DockSystem::setPanelActivatedCallback(PanelCallback &&cb) {
	_panelActivatedCallback = sp::move(cb);
}

void DockSystem::setPanelTapCallback(PanelCallback &&cb) { _panelTapCallback = sp::move(cb); }

// --- resizing --------------------------------------------------------------

bool DockSystem::canResize(DockNodeHandle h) const {
	auto split = _tree.get(h);
	if (!split || !split->isSplit()) {
		return false;
	}

	// a frame that forbids resizing freezes every divider touching it
	const auto allowsResize = [&](DockNodeHandle child) {
		auto n = _tree.get(child);
		if (!n) {
			return false;
		}
		return !n->isLeaf() || hasFlag(n->params.flags, DockFrameFlags::AllowResize);
	};
	return allowsResize(split->first) && allowsResize(split->second);
}

void DockSystem::updateSplitterDrag(DockNodeHandle h, const Vec2 &delta) {
	auto split = _tree.get(h);
	if (!split || !split->isSplit() || !canResize(h)) {
		return;
	}

	const bool horizontal = (split->axis == DockAxis::Horizontal);

	// Y points up and `first` of a vertical split is the top child: dragging down (negative
	// delta.y) makes it taller, so the vertical axis is inverted.
	const float travel = horizontal ? delta.x : -delta.y;

	const float extent = horizontal ? split->rect.size.width : split->rect.size.height;
	const float usable = sprt::max(extent - _splitterThickness, 0.0f);
	const float minA = _tree.minAlongAxis(split->first, split->axis);
	const float minB = _tree.minAlongAxis(split->second, split->axis);
	if (usable <= 0.0f || minA + minB >= usable) {
		return; // nothing to give: both children are already at their floor
	}

	const float free = usable - minA - minB;

	// Derive the current position from the ratio, as distribute does, not from the committed rect:
	// several deltas within one frame must accumulate before the next placement pass.
	const float current = minA + free * sprt::clamp(split->ratio, 0.0f, 1.0f);
	const float target = sprt::clamp(current + travel, minA, usable - minB);

	const float ratio = (free > 0.0f) ? ((target - minA) / free) : 0.5f;

	if (ratio != split->ratio) {
		split->ratio = ratio;
		invalidateLayout();
	}
}

bool DockSystem::setSplitRatio(DockNodeHandle h, float ratio) {
	auto split = _tree.get(h);
	if (!split || !split->isSplit()) {
		return false;
	}
	ratio = sprt::clamp(ratio, 0.0f, 1.0f);
	if (ratio != split->ratio) {
		split->ratio = ratio;
		invalidateLayout();
	}
	return true;
}

// --- dragging a tab --------------------------------------------------------

void DockSystem::setEdgeDropBand(float value) { _edgeDropBand = sprt::max(value, 0.0f); }

DockDropTarget DockSystem::hitTest(const Vec2 &rootLocal, StringView draggedPanelId) const {
	DockDropTarget target;

	auto handle = _tree.findLeafAt(rootLocal);
	auto leaf = _tree.get(handle);
	if (!leaf || !leaf->isLeaf()) {
		return target;
	}
	if (!hasFlag(leaf->params.flags, DockFrameFlags::AllowDrop)) {
		return target; // this place refuses panels: no zone at all, not even the middle
	}
	target.frame = handle;

	auto frame = static_cast<DockFrame *>(leaf->node.get());
	const Vec2 frameLocal = rootLocal - leaf->rect.origin;

	// A frame's only panel dragged over its own frame offers no zone: appending, reordering or
	// splitting (which would collapse the emptied half back) changes nothing.
	if (handle == _tree.findFrameForPanel(draggedPanelId) && leaf->panels.size() == 1) {
		return target;
	}

	// 1. the tab strip wins over everything else in the frame
	Rect strip;
	if (frame && frame->getTabBar()) {
		strip = frame->getTabBarRect();
		if (strip.containsPoint(frameLocal)) {
			auto bar = frame->getTabBar();
			target.kind = DockDropTarget::Kind::TabStrip;
			target.tabIndex = bar->indexForPosition(frameLocal - strip.origin);
			target.highlight = bar->caretRectForIndex(target.tabIndex);
			target.highlight.origin += strip.origin + leaf->rect.origin;
			return target;
		}
	}

	// 2. the four edge bands of the body mean "split the frame this way"
	Rect body = leaf->rect;
	if (strip.size.width > 0.0f && strip.size.height > 0.0f) {
		// carve the strip out of the body; which edge it sits on decides where
		switch (leaf->params.tabBarSide) {
		case DockTabBarSide::Top: body.size.height -= strip.size.height; break;
		case DockTabBarSide::Bottom:
			body.origin.y += strip.size.height;
			body.size.height -= strip.size.height;
			break;
		case DockTabBarSide::Left:
			body.origin.x += strip.size.width;
			body.size.width -= strip.size.width;
			break;
		case DockTabBarSide::Right: body.size.width -= strip.size.width; break;
		}
	}

	{
		// Edges on a disallowed axis are excluded from the nearest-edge comparison, so a pointer
		// near them falls through to the center zone instead of resolving to nothing.
		const bool splitH = allowsSplitAxis(leaf->params.flags, DockAxis::Horizontal);
		const bool splitV = allowsSplitAxis(leaf->params.flags, DockAxis::Vertical);

		const float band =
				sprt::min(_edgeDropBand, 0.25f * sprt::min(body.size.width, body.size.height));
		if (band > 0.0f && (splitH || splitV)) {
			constexpr float Never = maxOf<float>();

			const float dxLeft = splitH ? rootLocal.x - body.origin.x : Never;
			const float dxRight = splitH ? body.getMaxX() - rootLocal.x : Never;
			const float dyBottom = splitV ? rootLocal.y - body.origin.y : Never;
			const float dyTop = splitV ? body.getMaxY() - rootLocal.y : Never;

			// the closest allowed edge wins, so a corner resolves to one zone
			const float nearest = sprt::min(sprt::min(dxLeft, dxRight), sprt::min(dyBottom, dyTop));
			if (nearest < band) {
				if (nearest == dxLeft) {
					target.kind = DockDropTarget::Kind::SplitLeft;
					target.highlight = Rect(body.origin.x, body.origin.y, body.size.width / 2.0f,
							body.size.height);
				} else if (nearest == dxRight) {
					target.kind = DockDropTarget::Kind::SplitRight;
					target.highlight = Rect(body.origin.x + body.size.width / 2.0f, body.origin.y,
							body.size.width / 2.0f, body.size.height);
				} else if (nearest == dyTop) {
					target.kind = DockDropTarget::Kind::SplitTop;
					target.highlight = Rect(body.origin.x, body.origin.y + body.size.height / 2.0f,
							body.size.width, body.size.height / 2.0f);
				} else {
					target.kind = DockDropTarget::Kind::SplitBottom;
					target.highlight = Rect(body.origin.x, body.origin.y, body.size.width,
							body.size.height / 2.0f);
				}
				return target;
			}
		}
	}

	// 3. the middle: park it here as another tab
	target.kind = DockDropTarget::Kind::Center;
	target.highlight = body;
	return target;
}

DockPanelPayload *DockSystem::payloadOf(const DragEvent &event) {
	if (!event.data || !event.data->isLocal(DockPanelPayload::TypeName)) {
		return nullptr; // somebody else's drag; this dock has nothing to say about it
	}
	return dynamic_cast<DockPanelPayload *>(event.data->getLocal());
}

DragResponse DockSystem::handleDragAccept(const DragEvent &event) {
	auto payload = payloadOf(event);
	if (!payload) {
		return DragResponse();
	}

	// `event.location` is already in the owner's space, which is the tree's space
	auto target = hitTest(event.location, payload->panelId);
	if (target.kind == DockDropTarget::Kind::None) {
		return DragResponse(); // no zone here; whatever is under the dock may still take it
	}

	// panels are always moved, never copied
	return DragResponse{event.allowed & DragActions::Move};
}

void DockSystem::handleDragEnter(const DragEvent &event) {
	if (!_owner || _indicator) {
		return;
	}

	_indicator = Rc<DockDropIndicator>::create();
	_owner->addChild(_indicator, IndicatorZOrder);
	handleDragOver(event);
}

void DockSystem::handleDragOver(const DragEvent &event) {
	auto payload = payloadOf(event);
	if (!payload || !_indicator) {
		return;
	}
	_indicator->setTarget(hitTest(event.location, payload->panelId));
}

void DockSystem::handleDragLeave(const DragEvent &) {
	if (_indicator) {
		_indicator->removeFromParent(true);
		_indicator = nullptr;
	}
}

bool DockSystem::handleDragDrop(const DragEvent &event, DragActions) {
	auto payload = payloadOf(event);
	if (!payload) {
		return false;
	}

	// read everything first: applying the drop can collapse the source frame, invalidating its
	// handle and destroying the tab that delivered the drag
	const auto target = hitTest(event.location, payload->panelId);
	const auto panelId = payload->panelId;
	const auto source = payload->source;
	const bool fromHere = (payload->host == this);

	if (target.kind == DockDropTarget::Kind::None) {
		return false;
	}

	// Dropping a frame's only panel back into that frame is a no-op. Check `fromHere` first:
	// handles index this tree's arena only, so another dock's handle may coincidentally match.
	if (fromHere && target.frame == source && !target.isSplit()) {
		if (getPanelsInFrame(source).size() == 1) {
			return false;
		}
	}

	// A panel held elsewhere is taken over by the registry during updateFrameContent's acquire; one
	// not open here before is reported as opened.
	const bool arriving = !fromHere && !isPanelOpen(panelId);

	bool applied = false;
	switch (target.kind) {
	case DockDropTarget::Kind::None: return false;
	case DockDropTarget::Kind::Center:
		applied = movePanel(panelId, target.frame, maxOf<size_t>());
		break;
	case DockDropTarget::Kind::TabStrip:
		applied = movePanel(panelId, target.frame, target.tabIndex);
		break;
	default:
		// split zone: subdivide the target and park the panel in the new frame via the public API
		applied = !splitFrameWithPanel(target.frame, target.getAxis(), target.isFirst(), panelId)
						   .empty();
		break;
	}

	if (applied && arriving && _panelOpenedCallback) {
		_panelOpenedCallback(panelId);
	}
	return applied;
}

// --- the placement pass ----------------------------------------------------

void DockSystem::invalidateLayout() {
	if (_owner) {
		_owner->markLayoutChildrenDirty();
	}
	if (_layoutChangedCallback) {
		_layoutChangedCallback();
	}
}

void DockSystem::handleChildContentSizeDirty(Node *child) {
	System::handleChildContentSizeDirty(child);
	if (_inPlacement) {
		// our own commit talking back to us; see apply()
		return;
	}
	if (_owner) {
		_owner->markLayoutChildrenDirty();
	}
}

void DockSystem::handleLayoutChildren() {
	System::handleLayoutChildren();
	apply();
}

Size2 DockSystem::measureLeaf(const DockTreeNode &leaf) const {
	// One panel is visible at a time, so the frame fits the largest minimum, not the sum. A
	// collapsed frame ignores panel minimums entirely, so its minimum drops to the strip.
	Size2 content;
	if (!leaf.collapsed) {
		for (auto &id : leaf.panels) {
			if (auto desc = getPanelDescriptor(id)) {
				content.width = sprt::max(content.width, desc->minSize.width);
				content.height = sprt::max(content.height, desc->minSize.height);
			}
		}
	}

	/* The strip adds to one axis and floors the other, measured the same way as its
	`flex-basis: fit-content` inside the frame - which is `measureItem` and not `measureNode`,
	because a stylesheet may pin the strip (`dock-tab-bar.vertical { min-width: 54px }`) and a
	floor computed from the bare measurement would be the floor of a strip nobody draws. The
	frame's own direction says which axis the clamp is on: a side strip is an item of a row. */
	auto frame = static_cast<const DockFrame *>(leaf.node.get());
	if (frame && frame->getTabBar()) {
		const bool sideStrip = leaf.params.tabBarSide == DockTabBarSide::Left
				|| leaf.params.tabBarSide == DockTabBarSide::Right;
		const Size2 strip = LayoutSystem::measureItem(frame->getTabBar(),
				MeasureConstraints{MeasureMode::MaxContent}, sideStrip);
		switch (leaf.params.tabBarSide) {
		case DockTabBarSide::Top:
		case DockTabBarSide::Bottom:
			content.width = sprt::max(content.width, strip.width);
			content.height += strip.height;
			break;
		case DockTabBarSide::Left:
		case DockTabBarSide::Right:
			content.width += strip.width;
			content.height = sprt::max(content.height, strip.height);
			break;
		}
	}
	return content;
}

void DockSystem::apply() {
	if (!_owner || _inPlacement) {
		return;
	}

	// setContentSize notifies the parent synchronously into handleChildContentSizeDirty; without
	// this guard every placement would schedule another one.
	_inPlacement = true;

	_tree.updateMinimums([this](const DockTreeNode &n) { return measureLeaf(n); },
			_splitterThickness);
	// mirrors with the interface direction: `first` is the inline start; see DockTree::distribute
	_tree.distribute(Rect(Vec2::ZERO, _owner->getContentSize()), _overflowPolicy,
			_splitterThickness, isInlineRtl(_owner));
	commitGeometry();

	_inPlacement = false;
}

bool DockSystem::handleMeasure(const MeasureConstraints &constraints, Size2 &result) {
	if (_tree.empty()) {
		return false;
	}

	// Pure: only the minimum pass runs here. A dock fills whatever it gets, so its natural size is
	// the tree's minimum.
	_tree.updateMinimums([this](const DockTreeNode &n) { return measureLeaf(n); },
			_splitterThickness);

	result = _tree.getRootMinSize();
	result.width = sprt::min(result.width, constraints.maxWidth);
	result.height = sprt::min(result.height, constraints.maxHeight);
	return true;
}

void DockSystem::commitGeometry() {
	_tree.each([&](DockTreeNode &n) {
		if (!n.node) {
			return;
		}
		const Rect &rect = n.isSplit() ? n.splitterRect : n.rect;
		n.node->setContentSize(rect.size);
		n.node->setPosition(rect.origin);
	});
}

// --- scene nodes -----------------------------------------------------------

Node *DockSystem::acquireContent(StringView panelId) {
	// Builds on first show and takes the panel from another host if needed. The eviction can
	// re-enter through releasePanel, so callers read what they need before calling this.
	return _registry->acquireContent(panelId, this);
}

void DockSystem::updateFrameTabs(DockTreeNode &leaf) {
	auto frame = static_cast<DockFrame *>(leaf.node.get());
	if (!frame || !frame->getTabBar()) {
		return;
	}
	auto bar = frame->getTabBar();

	// reuse existing tabs: rebuilding one would drop its hover state and any drag in flight on it
	Vector<DockTab *> next;
	next.reserve(leaf.panels.size());

	for (size_t i = 0; i < leaf.panels.size(); ++i) {
		auto &id = leaf.panels[i];

		DockTab *tab = nullptr;
		for (auto &it : bar->getTabs()) {
			if (it->getPanelId() == id) {
				tab = it;
				break;
			}
		}
		if (!tab) {
			auto created = Rc<DockTab>::create(this, leaf.self, id);
			// parent it before `created` goes out of scope: `next` holds raw pointers
			bar->addChild(created, ZOrder(1));
			if (auto desc = getPanelDescriptor(id)) {
				created->setString(
						desc->title.empty() ? StringView(desc->id) : StringView(desc->title));
				created->setIcon(desc->icon);
				created->setClosable(hasFlag(desc->flags, DockPanelFlags::Closable)
						&& hasFlag(leaf.params.flags, DockFrameFlags::AllowClose));
			}
			tab = created;
		}

		// re-stamp the frame handle: a drop may have carried the tab into another frame
		tab->setFrame(leaf.self);
		tab->setActive(i == leaf.active);
		next.emplace_back(tab);
	}

	bar->setTabs(next);
}

void DockSystem::updateFrameContent(DockTreeNode &leaf) {
	auto frame = static_cast<DockFrame *>(leaf.node.get());
	if (!frame) {
		return;
	}

	updateFrameTabs(leaf);

	auto body = frame->getBody();
	StringView activeId;
	if (!leaf.panels.empty() && leaf.active < leaf.panels.size()) {
		activeId = leaf.panels[leaf.active];
	}

	auto content = activeId.empty() ? nullptr : acquireContent(activeId);

	// Detach everything else without cleanup: the nodes stay alive in the registry and keep their
	// state, and cleanup() would destroy their systems. handleExit/handleEnter pause and resume
	// them.
	auto children = body->getChildren();
	for (auto &it : Vector<Rc<Node>>(children.begin(), children.end())) {
		if (it.get() != content) {
			it->removeFromParent(false);
		}
	}

	if (content && content->getParent() != body) {
		content->removeFromParent(false);
		body->addChild(content);
		// fill the body; a panel that wants less says so with CSS on its own node
		LayoutSystem::setItem(content,
				FlexItemInfo{
					.grow = 1.0f,
					.shrink = 1.0f,
					.basis = 0.0f,
				});
	}
}

void DockSystem::syncNodes() {
	if (!_owner) {
		return;
	}

	// Sweep first: a released slot drops its node reference, but the node stays a child of the
	// root, so orphans are found from the children side and removed before new nodes are built.
	Set<Node *> live;
	_tree.each([&](DockTreeNode &n) {
		if (n.node) {
			live.emplace(n.node.get());
		}
	});

	Vector<Rc<Node>> orphans;
	for (auto &child : _owner->getChildren()) {
		if (live.find(child) != live.end()) {
			continue;
		}
		// only our own nodes: the root may carry application overlays
		if (child->getComponent<DockFrameComponent>()
				|| dynamic_cast<DockSplitter *>(child.get())) {
			orphans.emplace_back(child);
		}
	}
	// detach registry-owned panels from orphan frames before cleaning them; see detachPanelsUnder
	Set<Node *> dead;
	for (auto &it : orphans) {
		dead.emplace(it.get());
	}
	detachPanelsUnder(dead);

	for (auto &it : orphans) { it->removeFromParent(true); }

	_tree.each([&](DockTreeNode &n) {
		if (n.node) {
			return;
		}
		if (n.isLeaf()) {
			auto frame = Rc<DockFrame>::create(n.params, n.self);
			// the collapsed flag comes from the tree
			frame->setCollapsed(n.collapsed);
			if (_framesSelectable) {
				setNodeSelectable(frame, true);
			}
			n.node = frame;
			_owner->addChild(frame, FrameZOrder);
			updateFrameContent(n);
		} else {
			auto splitter = Rc<DockSplitter>::create(this, n.self, n.axis);
			n.node = splitter;
			_owner->addChild(splitter, SplitterZOrder);
		}
	});
}

void DockSystem::setFramesSelectable(bool value) {
	if (_framesSelectable == value) {
		return;
	}
	_framesSelectable = value;

	_tree.each([&](DockTreeNode &n) {
		if (n.node && n.isLeaf()) {
			setNodeSelectable(n.node, value);
		}
	});
}

void DockSystem::handleVisitSelf(FrameInfo &info, Node *node, NodeVisitFlags flags) {
	System::handleVisitSelf(info, node, flags);
	updateCurrentFrame();
}

void DockSystem::updateCurrentFrame() {
	DockFrame *current = nullptr;
	if (auto selection = SelectionSystem::findForNode(_owner)) {
		for (auto &it : selection->getChain()) {
			if (it->getComponent<DockFrameComponent>()) {
				// the deepest frame decides, whichever dock it belongs to
				if (it->getParent() == _owner) {
					current = static_cast<DockFrame *>(it.get());
				}
				break;
			}
		}
	}

	if (current && (current->isCollapsed() || !current->isRunning())) {
		current = nullptr;
	}

	if (_currentFrame.get() == current) {
		return;
	}

	if (_currentFrame) {
		_currentFrame->setCurrent(false);
	}
	_currentFrame = current;
	if (_currentFrame) {
		_currentFrame->setCurrent(true);
	}
}

} // namespace stappler::xenolith::ui
