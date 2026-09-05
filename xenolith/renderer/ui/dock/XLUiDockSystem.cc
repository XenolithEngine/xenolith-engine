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
#include "XLUiDockSplitter.h"
#include "XLUiLayoutSystem.h"
#include "XLUiStyleSystem.h"

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

	// HandleChildNodeEvents catches a panel's content growing inside a frame; AddToFrameStack
	// publishes the dock to its own subtree; HandleMeasure lets a fit-content ancestor size around
	// the whole dock (see handleMeasure for why the answer is the tree's minimum)
	setSystemFlags(SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents
			| SystemFlags::HandleNodeEvents | SystemFlags::HandleLayoutChildren
			| SystemFlags::HandleMeasure | SystemFlags::HandleChildNodeEvents
			| SystemFlags::AddToFrameStack);
	setFrameTag(SystemFrameTag);
	return true;
}

void DockSystem::handleAdded(Node *owner) {
	System::handleAdded(owner);

	sprt_passert(owner->getSystemByType<LayoutSystem>() == nullptr,
			"DockSystem owns its children's geometry: the dock root must not carry a LayoutSystem");

	// Claim ownership of the children's ContentSize towards the style resolver: a CSS width/height
	// on a frame becomes an intrinsic hint in a MeasureComponent instead of a committed size that
	// would fight this system every frame. It also keeps `display:flex` on the root from adding a
	// second writer of the children's geometry.
	owner->setComponent<SystemManagedLayout>();

	// How the dock receives dragged panels. One target for the whole dock, not one per frame: a
	// frame is resolved by walking the split tree, which is both cheaper than a scene traversal
	// and immune to whatever the parked panels' own input listeners are doing
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

	// A panel node the registry keeps alive may be parked inside a subtree that is about to be
	// cleaned. Node::cleanup() recurses into children and would destroy its systems (a Label's
	// EventListener among them), leaving a dangling system pointer on a node the registry - and
	// whoever re-parents it next - still holds. So take it out first, WITHOUT cleanup: a plain
	// detach fires handleExit, which is exactly how a system pauses while its node is out of the
	// scene, and re-entry replays handleEnter.
	//
	// It attaches to a frame's BODY, one level below the frame itself, so the whole ancestor chain
	// has to be walked rather than just the direct parent. And with a registry that may be SHARED
	// with another container, the walk is what keeps this from tearing that container's live panels
	// out from under it: only what is inside `roots` is ours to detach.
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

	// Give up every claim: the panels this dock was holding are now parked nowhere, and they keep
	// their content, so re-opening one anywhere brings back exactly what was there.
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

	// drop what the registry does not know: a layout naming a panel this build of the application
	// no longer has must not be fatal
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
	// Structurally identical to a close - the frame folds away just the same - but NOT reported as
	// one: the panel is moving to another container, and an application that treats `closed` as
	// "the user is done with this" would act on something that did not happen. The node is not
	// touched here at all; the registry hands it to the new host.
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

	// an emptied place folds away and its sibling takes the space, unless it was declared to stay
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

	// re-read: nothing reallocated above, but the source may have been the target's sibling and
	// the collapse below can move it
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

	// a new place inherits the constraints of the one it was carved out of, unless the caller
	// named its own; only the name is never inherited - it identifies one place, not a kind
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

// --- persistence -----------------------------------------------------------

Value DockSystem::save() const { return _tree.save(); }

bool DockSystem::restore(const Value &value) {
	if (!_tree.restore(value,
				[this](StringView id) { return getPanelDescriptor(id) != nullptr; })) {
		return false;
	}

	// A tree with nothing left in it - every panel it named is gone from this build - would leave
	// the dock with no parking place at all. One default frame is a better answer than none.
	if (_tree.empty()) {
		_tree.build(DockLayoutSpec::leaf(Vector<String>()));
	}

	syncNodes();

	// Panels the file did not mention stay closed, EXCEPT the ones declared OpenByDefault: those
	// are how a panel introduced by a newer build of the application still shows up.
	for (auto &it : _registry->getPanelDescriptors()) {
		if (!hasFlag(it.second.flags, DockPanelFlags::OpenByDefault)) {
			continue;
		}
		if (isPanelOpen(it.first)) {
			continue;
		}
		// Somebody ELSE is holding it. Opening it here would take it away from them, which is the
		// opposite of what "open this by default" asks for: a shared registry makes a dock's saved
		// layout one half of an arrangement, and the other half's panels are not this one's to
		// claim. A container that wants it back restores its own half.
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

// --- resizing --------------------------------------------------------------

bool DockSystem::canResize(DockNodeHandle h) const {
	auto split = _tree.get(h);
	if (!split || !split->isSplit()) {
		return false;
	}

	// a place that forbids resizing freezes every divider touching it, whichever side it is on
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

	// Y points up while `first` of a vertical split is the TOP child. Dragging the divider DOWN is
	// a NEGATIVE delta.y and makes the top child TALLER, so the vertical axis is inverted.
	const float travel = horizontal ? delta.x : -delta.y;

	const float extent = horizontal ? split->rect.size.width : split->rect.size.height;
	const float usable = sprt::max(extent - _splitterThickness, 0.0f);
	const float minA = _tree.minAlongAxis(split->first, split->axis);
	const float minB = _tree.minAlongAxis(split->second, split->axis);
	if (usable <= 0.0f || minA + minB >= usable) {
		return; // nothing to give: both children are already at their floor
	}

	const float free = usable - minA - minB;

	// Where the first child stands NOW, derived from the ratio with the same formula distribute
	// uses - not read back from its committed rect. Reading the rect would make the result depend
	// on whether a placement pass has run since the previous delta, so a burst of deltas inside
	// one frame would lose all but the last. In ratio space the drag is a true fixed point of the
	// pass: any number of small deltas land exactly where one big one does.
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

	// A panel that is the ONLY occupant of this very frame has nowhere to go inside it. Appending
	// it or reordering it changes nothing; even splitting the frame off changes nothing, because
	// carrying the lone panel into the new half empties the old one and collapses it straight
	// back. So the whole frame offers no zone at all, and the indicator stays hidden while the
	// panel is dragged around its own place.
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

	if (hasFlag(leaf->params.flags, DockFrameFlags::AllowSplit)) {
		const float band =
				sprt::min(_edgeDropBand, 0.25f * sprt::min(body.size.width, body.size.height));
		if (band > 0.0f) {
			const float dxLeft = rootLocal.x - body.origin.x;
			const float dxRight = body.getMaxX() - rootLocal.x;
			const float dyBottom = rootLocal.y - body.origin.y;
			const float dyTop = body.getMaxY() - rootLocal.y;

			// the closest edge wins, so a corner resolves to one zone rather than to neither
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

	// `event.location` is already in the owner's space, which is the space the tree is computed
	// in - so this is the rootLocal hitTest wants, with no conversion of our own
	auto target = hitTest(event.location, payload->panelId);
	if (target.kind == DockDropTarget::Kind::None) {
		return DragResponse(); // no zone here; whatever is under the dock may still take it
	}

	// A panel is moved between frames, never copied: there is one node and one identity
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

	// Read everything the drop needs BEFORE anything mutates: applying it can collapse the source
	// frame, which invalidates its handle and destroys the tab that delivered the drag
	const auto target = hitTest(event.location, payload->panelId);
	const auto panelId = payload->panelId;
	const auto source = payload->source;
	const bool fromHere = (payload->host == this);

	if (target.kind == DockDropTarget::Kind::None) {
		return false;
	}

	// Dropping the only panel of a frame back into that same frame changes nothing.
	//
	// `fromHere` is not a shortcut, it is what makes the comparison mean anything: a DockNodeHandle
	// is an index into ONE tree's arena, so the handle of another dock's frame can equal one of ours
	// by coincidence and this would refuse a perfectly good drop. A panel arriving from anywhere
	// else has no no-op case here at all - wherever it lands is somewhere it was not.
	if (fromHere && target.frame == source && !target.isSplit()) {
		if (getPanelsInFrame(source).size() == 1) {
			return false;
		}
	}

	// A panel another container is holding is taken from it by the registry as part of handing over
	// the node, on the acquire that updateFrameContent does at the end of every path below. So there
	// is nothing to negotiate here - only the arrival to report, which for a panel that was not open
	// in this dock a moment ago is an open rather than a move.
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
		// A split zone: subdivide the target and park the panel in the new place. Both go through
		// the same public operations an application would call, so a drop can never reach a code
		// path the API does not already expose - and the whole thing stays drivable from a test.
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
	// Only ONE panel is visible at a time, so the place has to fit the LARGEST of them - not their
	// sum. This is where a panel's declared minimum strengthens the frame's, and through the
	// bottom-up pass in DockTree, every split above it.
	Size2 content;
	for (auto &id : leaf.panels) {
		if (auto desc = getPanelDescriptor(id)) {
			content.width = sprt::max(content.width, desc->minSize.width);
			content.height = sprt::max(content.height, desc->minSize.height);
		}
	}

	// The tab strip eats one axis outright and floors the other. Its natural size comes from the
	// SAME measurement protocol that gives it `flex-basis: fit-content` inside the frame, so the
	// strip cannot end up smaller than the size the frame reserved for it.
	auto frame = static_cast<const DockFrame *>(leaf.node.get());
	if (frame && frame->getTabBar()) {
		const Size2 strip = LayoutSystem::measureNode(frame->getTabBar(),
				MeasureConstraints{MeasureMode::MaxContent});
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

	// Node::setContentSize notifies the parent SYNCHRONOUSLY, which lands right back in
	// handleChildContentSizeDirty above and would mark us dirty again - an unconditional relayout
	// on every frame, forever. The same guard LayoutSystem needs, for the same reason.
	_inPlacement = true;

	_tree.updateMinimums([this](const DockTreeNode &n) { return measureLeaf(n); },
			_splitterThickness);
	_tree.distribute(Rect(Vec2::ZERO, _owner->getContentSize()), _overflowPolicy,
			_splitterThickness);
	commitGeometry();

	_inPlacement = false;
}

bool DockSystem::handleMeasure(const MeasureConstraints &constraints, Size2 &result) {
	if (_tree.empty()) {
		return false;
	}

	// Pure: only the first of the three passes runs here. A dock has no "preferred larger" size -
	// it always fills whatever it is given - so its natural size IS the tree's minimum.
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
	// Builds it on first show, and - when another container was holding it - takes it from there:
	// the registry evicts the previous host before handing the node over, so this dock never has to
	// ask who had it. That eviction can re-enter this system through releasePanel, which is why the
	// callers of this read everything they need before calling it.
	return _registry->acquireContent(panelId, this);
}

void DockSystem::updateFrameTabs(DockTreeNode &leaf) {
	auto frame = static_cast<DockFrame *>(leaf.node.get());
	if (!frame || !frame->getTabBar()) {
		return;
	}
	auto bar = frame->getTabBar();

	// Reuse the tab a panel already has, wherever in the strip it was: a reorder, an activation or
	// a panel arriving beside it must not destroy and rebuild a tab - that would drop the hover
	// state and, worse, the drag that is quite possibly in flight on it right now.
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
			// Parent it BEFORE the local Rc goes out of scope: `next` holds raw pointers, so
			// letting the only reference die at the end of this block would leave every entry
			// dangling. The strip is the tab's owner from its first moment; setTabs below only
			// reorders what is already there.
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

		// the frame handle is re-stamped every time: a tab can be carried into another place by a
		// drop, and the handle is how it reports where it now lives
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

	// take out whatever else is in there; the node itself stays alive in _content, so a panel that
	// is only being switched away from - or moved to another frame - keeps its state. Detach
	// WITHOUT cleanup: these are live nodes we own and re-parent below, and Node::cleanup() would
	// destroy their systems (a Label's EventListener among them), which handleEnter then reads as
	// freed memory on the next present. A plain detach fires handleExit, which is exactly how a
	// system pauses while its node leaves the scene; re-entry replays it through handleEnter.
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

	// First the sweep, then the build. A slot that was released - a frame collapsed by the last
	// panel leaving it, a split merged away by a drop - dropped its reference to the node, but the
	// root still holds one, so the node would otherwise stay in the scene: drawn, hit-tested, and
	// frozen at whatever rect it had when its slot died. Nothing in the tree would ever mention it
	// again, which is exactly why the sweep has to work from the other direction.
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
		// only our own nodes: the root may well carry an overlay the application put there
		if (child->getComponent<DockFrameComponent>()
				|| dynamic_cast<DockSplitter *>(child.get())) {
			orphans.emplace_back(child);
		}
	}
	// A panel node the registry keeps alive may still be parented INSIDE an orphan frame that is
	// about to be cleaned. Take those out first - see detachPanelsUnder for why a plain detach, and
	// why the whole ancestor chain has to be walked. Only then may the orphan be cleaned safely.
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

} // namespace stappler::xenolith::ui
