/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef XENOLITH_APPLICATION_NODES_XLNODE_H_
#define XENOLITH_APPLICATION_NODES_XLNODE_H_

#include "XLNodeInfo.h"
#include "XLSystem.h"
#include "XLComponent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class System;
class DynamicStateSystem;
class Scene;
class Scheduler;
class InputListener;
class Action;
class ActionManager;
class Director;
class FrameContext;

/* Internal and CSS identity of a scene-graph node: what selectors are matched against. */
struct SP_PUBLIC NodeIdentity {
	static ComponentId Id;

	uint64_t tag = InvalidTag;
	String type; // element/tag name ("label", "layer", "flex", ...)
	String name; // css #id
	HashSet<String, sprt::hash<void>> classes; // css .classes
	Value value;
};

struct SP_PUBLIC ActionStorage : public Ref {
	Vector<Rc<Action>> actionToStart;

	void addAction(Rc<Action> &&a);
	void removeAction(Action *a);
	void removeAllActions();
	void removeActionByTag(uint32_t);
	void removeAllActionsByTag(uint32_t);
	Action *getActionByTag(uint32_t);
};

class SP_PUBLIC Node : public Ref, public ComponentContainer {
public:
	/* Nodes with transparent zOrder will not be added into zPath */
	static constexpr ZOrder ZOrderTransparent = ZOrder::min();
	static constexpr ZOrder ZOrderMax = ZOrder::max();
	static constexpr ZOrder ZOrderMin = ZOrder::min() + ZOrder(1);
	static constexpr uint64_t DefaultCallbackSystemTag = InvalidTag - 1;

	static bool isParent(Node *parent, Node *node);
	static Mat4 getChainNodeToParentTransform(Node *parent, Node *node, bool withParent);
	static Mat4 getChainParentToNodeTransform(Node *parent, Node *node, bool withParent);

	Node();
	virtual ~Node();

	virtual bool init();

	virtual void setLocalZOrder(ZOrder localZOrder);
	virtual ZOrder getLocalZOrder() const { return _zOrder; }

	virtual void setScale(float scale);
	virtual void setScale(const Vec2 &);
	virtual void setScale(const Vec3 &);
	virtual void setScaleX(float scaleX);
	virtual void setScaleY(float scaleY);
	virtual void setScaleZ(float scaleZ);

	virtual Vec3 getScale() const { return _scale; }

	virtual void setPosition(const Vec2 &position);
	virtual void setPosition(const Vec3 &position);
	virtual void setPositionX(float);
	virtual void setPositionY(float);
	virtual void setPositionZ(float);

	virtual Vec3 getPosition() const { return _position; }

	virtual void setSkewX(float skewX);
	virtual void setSkewY(float skewY);

	virtual Vec2 getSkew() const { return _skew; }

	/**
	 * Sets the anchor point in percent.
	 *
	 * anchorPoint is the point around which all transformations and positioning manipulations take place.
	 * It's like a pin in the node where it is "attached" to its parent.
	 * The anchorPoint is normalized, like a percentage. (0,0) means the bottom-left corner and (1,1) means the top-right corner.
	 * But you can use values higher than (1,1) and lower than (0,0) too.
	 * The default anchorPoint is (0.5,0.5), so it starts in the center of the node.
	 * @note If node has a physics body, the anchor must be in the middle, you cann't change this to other value.
	 *
	 * @param anchorPoint   The anchor point of node.
	 */
	virtual void setAnchorPoint(const Vec2 &anchorPoint);
	virtual Vec2 getAnchorPoint() const { return _anchorPoint; }

	/**
	 * Sets the untransformed size of the node.
	 *
	 * The contentSize remains the same no matter the node is scaled or rotated.
	 * All nodes has a size. Layer and Scene has the same size of the screen.
	 *
	 * @param contentSize   The untransformed size of the node.
	 */
	virtual void setContentSize(const Size2 &contentSize);
	virtual Size2 getContentSize() const { return _contentSize; }

	// Force handleComponentsDirty processing on the next visit
	void markComponentsDirty() { _componentsDirty = true; }

	// Force handleContentSizeDirty processing on the next visit
	void markContentSizeDirty() { _contentSizeDirty = true; }

	// Opt into the measure phase: handleMeasure will run on the next visit to (re)fix the
	// node's own size via the SystemFlags::HandleMeasure protocol
	void markMeasureDirty() { _measureDirty = true; }

	// Request the layout-children phase on the next visit (a layout engine re-runs its pass
	// over the children, e.g. after a child's content size changed)
	void markLayoutChildrenDirty() { _layoutChildrenDirty = true; }

	virtual void setVisible(bool visible);
	virtual bool isVisible() const { return _visible; }

	// The visibility wrapVisit actually honors: the explicit setVisible flag combined with a
	// style-driven VisibilityComponent (display: none / visibility: hidden). When false, the
	// node reacts at visit exactly like setVisible(false) — the whole subtree is skipped.
	bool isEffectivelyVisible() const;

	// Whether the node occupies a layout box: false when explicitly invisible or hidden via
	// `display: none`; a `visibility: hidden` node stays displayed (keeps its box), matching
	// CSS semantics. Used by layout engines to decide which children to collapse.
	bool isDisplayed() const;

	virtual void setRotation(float rotationInRadians);
	virtual void setRotation(const Vec3 &rotationInRadians);
	virtual void setRotation(const Quaternion &quat);

	virtual float getRotation() const { return _rotation.z; }
	virtual Vec3 getRotation3D() const { return _rotation; }
	virtual Quaternion getRotationQuat() const { return _rotationQuat; }

	template <typename N, typename... Args>
	auto addChild(N *child, Args &&...args) -> N * {
		addChildNode(child, sprt::forward<Args>(args)...);
		return child;
	}

	template <typename N, typename... Args>
	auto addChild(const Rc<N> &child, Args &&...args) -> N * {
		addChildNode(child.get(), sprt::forward<Args>(args)...);
		return child.get();
	}

	virtual void addChildNode(Node *child);
	virtual void addChildNode(Node *child, ZOrder localZOrder);
	virtual void addChildNode(Node *child, ZOrder localZOrder, uint64_t tag);

	virtual Node *getChildByTag(uint64_t tag) const;

	virtual SpanView<Rc<Node>> getChildren() const { return _children; }
	virtual size_t getChildrenCount() const { return _children.size(); }

	/** Monotonic counter of changes to the child list: add, remove and reorder each bump it.
	 * Consumers whose result depends on a child's position among its siblings (CSS structural
	 * selectors such as `:nth-child`) use it to detect that their cached answer is stale.
	 * `sortAllChildren` does NOT bump it - the sort only applies a reorder already counted. */
	uint32_t getChildrenVersion() const { return _childrenVersion; }

	/** The child list changed (add / remove / reorder): bump the version and, while running,
	 * re-arm every remaining child's content-size phase. A sibling's position among its
	 * siblings is an input to its style (`:nth-child`) and to layout, and a plain add/remove
	 * moves nothing, so without this nudge the siblings would never signal and would keep
	 * answers computed for the old child list. */
	void markChildrenStructureDirty();

	virtual void setParent(Node *parent);
	virtual Node *getParent() const { return _parent; }

	virtual void removeFromParent(bool cleanup = true);
	virtual void removeChild(Node *child, bool cleanup = true);
	virtual void removeChildByTag(uint64_t tag, bool cleanup = true);
	virtual void removeAllChildren(bool cleanup = true);

	virtual void reorderChild(Node *child, ZOrder localZOrder);

	/**
	 * Sorts the children array once before drawing, instead of every time when a child is added or reordered.
	 * This appraoch can improves the performance massively.
	 * @note Don't call this manually unless a child added needs to be removed in the same frame.
	 */
	virtual bool sortAllChildren();

	template <typename A>
	auto runAction(A *action) -> A * {
		runActionObject(action);
		return action;
	}

	template <typename A>
	auto runAction(A *action, uint32_t tag) -> A * {
		runActionObject(action, tag);
		return action;
	}

	template <typename A>
	auto runAction(const Rc<A> &action) -> A * {
		runActionObject(action.get());
		return action.get();
	}

	template <typename A>
	auto runAction(const Rc<A> &action, uint32_t tag) -> A * {
		runActionObject(action.get(), tag);
		return action.get();
	}

	void runActionObject(Action *);
	void runActionObject(Action *, uint32_t tag);

	void stopAllActions();

	void stopAction(Action *action);
	void stopActionByTag(uint32_t tag);
	void stopAllActionsByTag(uint32_t tag);

	Action *getActionByTag(uint32_t tag);
	size_t getNumberOfRunningActions() const;

	template <typename C>
	auto addSystem(C *system) -> C * {
		if (addSystemItem(system)) {
			return system;
		}
		return nullptr;
	}

	template <typename C>
	auto addSystem(const Rc<C> &system) -> C * {
		if (addSystemItem(system.get())) {
			return system.get();
		}
		return nullptr;
	}

	// Add a system with an explicit dispatch priority (lower is dispatched earlier),
	// overriding the system's own default priority
	template <typename C>
	auto addSystem(C *system, uint32_t priority) -> C * {
		if (addSystemItem(system, priority)) {
			return system;
		}
		return nullptr;
	}

	template <typename C>
	auto addSystem(const Rc<C> &system, uint32_t priority) -> C * {
		if (addSystemItem(system.get(), priority)) {
			return system.get();
		}
		return nullptr;
	}

	// Adds the system using its own (default) priority for ordering
	virtual bool addSystemItem(System *);
	// Adds the system, assigning the given priority for ordering
	virtual bool addSystemItem(System *, uint32_t priority);

	// Re-sort an already-added system after its priority changed (called by
	// System::setSystemPriority; not intended for direct use)
	void updateSystemPriority(System *);
	virtual bool removeSystem(System *);
	virtual bool removeSystemByTag(uint64_t);
	virtual bool removeAllSystemByTag(uint64_t);
	virtual void removeAllSystems();

	template <typename T>
	T *getSystemByType() const;

	template <typename T>
	T *getSystemByType(uint64_t tag) const;

	SpanView<Rc<System>> getSystems() const { return _systems; }

	virtual StringView getName() const;
	virtual void setName(StringView str);

	virtual StringView getType() const;
	virtual void setType(StringView str);

	virtual void addStyleClass(StringView cl);
	virtual void removeStyleClass(StringView cl);
	virtual void toggleStyleClass(StringView cl);
	virtual bool hasStyleClass(StringView cl) const;
	virtual const HashSet<String, sprt::hash<void>> *getStyleClasses() const;

	virtual const Value &getDataValue() const;
	virtual void setDataValue(Value &&val);

	virtual uint64_t getTag() const;
	virtual void setTag(uint64_t tag);

	virtual bool isRunning() const { return _running; }

	virtual void setEventFlags(NodeEventFlags);
	virtual NodeEventFlags getEventFlags() const { return _eventFlags; }

	// Node was added to scene
	virtual void handleEnter(Scene *);

	// Node was removed from scene
	virtual void handleExit();

	// The node's own size is being fixed for this frame (phase 2, requires _measureDirty).
	// Runs the SystemFlags::HandleMeasure protocol and commits the result via setContentSize.
	// Must NOT change components. Opt-in: set via markMeasureDirty()
	virtual void handleMeasure();

	// New ContentSize applied for the node
	// There you can setup Node's appearance and layout it's subnodes
	// ContentSize processed after Measure/Transform, size is fixed here
	virtual void handleContentSizeDirty();

	// Some of node's components was updated
	// Components processed after ContentSize and Transform, be sure not to modify them here
	virtual void handleComponentsDirty(const ComponentMask &);

	// Some of an ancestor's components was updated. Dispatched to systems that opted in
	// via SystemFlags::HandleAncestorComponents; Node subclasses can override (calling base)
	// to react to ancestor component changes. Only reaches nodes whose subtree contains a
	// listener (see setWantsAncestorComponents / _ancestorComponentsListeners)
	virtual void handleAncestorComponentsDirty();

	// Register this node itself as an ancestor-components listener (for Node subclasses that
	// override handleAncestorComponentsDirty instead of attaching a System). Feeds the same
	// subtree counter that gates ancestor ComponentsDirty propagation
	void setWantsAncestorComponents(bool);
	bool getWantsAncestorComponents() const { return _wantsAncestorComponents; }

	// Apply `delta` to this node's ancestor-components listener counter and to all ancestors.
	// Called by owned Systems (when their HandleAncestorComponents/enabled state changes) and
	// by child attach/detach; not intended for direct use
	void adjustAncestorComponentsListeners(int32_t delta);

	// New Transform applied for the node
	// Node was repositioned or scaled within it's parent
	virtual void handleTransformDirty(const Mat4 &);

	// Node was repositioned or scaled within scene
	// There global parameters (like pixel density) can be recalculated
	// Called after `handleTransformDirty` if node's transform was also dirty.
	// The transform phase runs after Measure, so the node's size is already fixed here
	virtual void handleGlobalTransformDirty(const Mat4 &);

	// Children array was updated somehow
	// Called after all other processing
	virtual void handleReorderChildDirty();

	// Lay out this node's children (phase 6, requires _layoutChildrenDirty). Runs after child
	// reorder with this node's own size and child order fixed; dispatched to systems with
	// SystemFlags::HandleLayoutChildren (this is where a layout engine positions/sizes children)
	virtual void handleLayoutChildren();

	// Node should be positioned within parent (parent's content size changed)
	virtual void handleLayoutInParent(Node *);

	// Immediate direct-parent fallback for a child content-size change: called from the child's
	// setContentSize/setEventFlags and dispatched to THIS (parent) node's own systems flagged
	// SystemFlags::HandleChildNodeEvents. The primary channel is the frame stack - during a
	// descendant's visit, handleContentSizeDirty(FrameInfo&) delivers the event to the nearest
	// opted-in ancestor system (see handleContentSizeDirty(FrameInfo&) / SystemFlags::AddToFrameStack);
	// this method stays as a mutation-time notification for changes made outside the visit loop
	virtual void notifyChildContentSizeDirty(Node *child);

	virtual void cleanup();

	virtual Rect getBoundingBox() const;

	virtual void resume();
	virtual void pause();

	virtual void update(const UpdateTime &time);

	virtual const Mat4 &getNodeToParentTransform() const;

	virtual void setNodeToParentTransform(const Mat4 &transform);
	virtual const Mat4 &getParentToNodeTransform() const;

	virtual Mat4 getNodeToWorldTransform() const;
	virtual Mat4 getWorldToNodeTransform() const;

	Vec2 convertToNodeSpace(const Vec2 &worldPoint) const;
	Vec2 convertToWorldSpace(const Vec2 &nodePoint) const;
	Vec2 convertToNodeSpaceAR(const Vec2 &worldPoint) const;
	Vec2 convertToWorldSpaceAR(const Vec2 &nodePoint) const;

	virtual bool isCascadeOpacityEnabled() const { return _cascadeOpacityEnabled; }
	virtual bool isCascadeColorEnabled() const { return _cascadeColorEnabled; }

	virtual void setCascadeOpacityEnabled(bool cascadeOpacityEnabled);
	virtual void setCascadeColorEnabled(bool cascadeColorEnabled);

	virtual float getOpacity() const { return _realColor.a; }
	virtual float getDisplayedOpacity() const { return _displayedColor.a; }
	virtual void setOpacity(float opacity);
	virtual void setOpacity(OpacityValue);
	virtual void updateDisplayedOpacity(float parentOpacity);

	virtual Color4F getColor() const { return _realColor; }
	virtual Color4F getDisplayedColor() const { return _displayedColor; }
	virtual void setColor(const Color4F &color, bool withOpacity = false);
	virtual void updateDisplayedColor(const Color4F &parentColor);

	virtual void setDepthIndex(float value) { _depthIndex = value; }
	virtual float getDepthIndex() const { return _depthIndex; }

	virtual void draw(FrameInfo &, NodeVisitFlags flags);

	// visit on sorted nodes, push draw commands
	// on this step, we also process parent-to-child geometry changes
	virtual bool visitDraw(FrameInfo &, NodeVisitFlags parentFlags);

	virtual void visitSelf(FrameInfo &, NodeVisitFlags flags, bool visibleByCamera);

	void scheduleUpdate();
	void unscheduleUpdate();

	virtual bool isTouched(const Vec2 &location, float padding = 0.0f);
	virtual bool isTouchedNodeSpace(const Vec2 &location, float padding = 0.0f);

	// Callbacks bound with default CallbackSystem to reduce common node memory footprint.
	// System will be created when first callback attached, and marked with DefaultCallbackSystemTag
	// to separate it from user-defined systems
	virtual void setEnterCallback(Function<void(Scene *)> &&);
	virtual void setExitCallback(Function<void()> &&);
	virtual void setContentSizeDirtyCallback(Function<void()> &&);
	virtual void setComponentsDirtyCallback(Function<void(const ComponentMask &mask)> &&);
	virtual void setTransformDirtyCallback(Function<void(const Mat4 &)> &&);
	virtual void setReorderChildDirtyCallback(Function<void()> &&);
	virtual void setLayoutCallback(Function<void(Node *)> &&);

	// content measurement protocol (see System::handleMeasure): return true and
	// fill the Size2 to answer, return false to fall through to other systems
	virtual void setMeasureCallback(Function<bool(const MeasureConstraints &, Size2 &)> &&);

	// a layout engine committed the size to this node (see System::handleLayoutApplied)
	virtual void setLayoutAppliedCallback(Function<void(const Size2 &)> &&);

	float getInputDensity() const { return _inputDensity; }

	Scene *getScene() const { return _scene; }
	Director *getDirector() const { return _director; }
	Scheduler *getScheduler() const { return _scheduler; }
	ActionManager *getActionManager() const { return _actionManager; }
	FrameContext *getFrameContext() const { return _frameContext; }

	virtual float getMaxDepthIndex() const;

	// Recurse into parent tree to find node with specific component.
	// Node that have specific component will be returned to callback.
	// `depth` will be set to recursion depth where 0 - direct parent.
	// You should return false from Callback to stop recursion.
	// function returns true if node was found or false otherwise
	template <typename T>
	bool findParentWithComponent(
			const Callback<bool(NotNull<Node>, NotNull<const T>, uint32_t depth)> &) const;

	// Enumerate childrens to find nodes with specific components.
	// You can set maximal recursion depth for enumeration to walk through subchilds, 0 means only direct childs.
	// Nodes that have specific component will be returned to callback.
	// `depth` will be set to recursion depth where 0 - direct childs
	// You should return false from Callback to stop iterating.
	// function returns true if some nodes was found or false if nothing was found
	template <typename T>
	bool enumerateChildsWithComponent(
			const Callback<bool(NotNull<Node>, NotNull<const T>, uint32_t depth)> &,
			uint32_t maxDepth = 0);

protected:
	struct VisitInfo {
		void (*visitBegin)(const VisitInfo &) = nullptr;
		void (*visitNodesBelow)(const VisitInfo &, SpanView<Rc<Node>>) = nullptr;
		void (*visitSelf)(const VisitInfo &, Node *) = nullptr;
		void (*visitNodesAbove)(const VisitInfo &, SpanView<Rc<Node>>) = nullptr;
		void (*visitEnd)(const VisitInfo &) = nullptr;
		Node *node = nullptr;

		mutable NodeVisitFlags flags = NodeVisitFlags::None;
		mutable FrameInfo *frameInfo = nullptr;
		mutable bool visibleByCamera = true;
		mutable Vector<Rc<System>> visitableSystems;
	};

	void handleMeasure(FrameInfo &);
	void handleComponentsDirty(FrameInfo &, const ComponentMask &);
	void handleContentSizeDirty(FrameInfo &);
	void handleLayoutChildren(FrameInfo &);

	virtual void updateCascadeOpacity();
	virtual void disableCascadeOpacity();
	virtual void updateCascadeColor();
	virtual void disableCascadeColor();
	virtual void updateColor() { }

	Mat4 transform(const Mat4 &parentTransform);
	virtual NodeVisitFlags processParentFlags(FrameInfo &info, NodeVisitFlags parentFlags);

	virtual bool wrapVisit(FrameInfo &, NodeVisitFlags flags, const VisitInfo &, bool useContext);

	virtual CallbackSystem *makeDefaultCallbackSystem();

	template <typename T>
	bool enumerateChildsWithComponent(
			const Callback<bool(NotNull<Node>, NotNull<const T>, uint32_t depth)> &cb,
			uint32_t maxDepth, uint32_t depth, bool &shouldStop);

	bool _is3d = false;
	bool _running = false;
	bool _visible = true;
	bool _scheduled = false;
	bool _paused = false;

	bool _cascadeColorEnabled = false;
	bool _cascadeOpacityEnabled = true;

	// bumped on every add/remove/reorder of a child - see getChildrenVersion()
	uint32_t _childrenVersion = 0;

	bool _contentSizeDirty = true;
	bool _reorderChildDirty = true;
	bool _transformDirty = true;
	bool _measureDirty = false; // opt-in measure phase (see markMeasureDirty)
	bool _layoutChildrenDirty = true; // run handleLayoutChildren on next visit
	mutable bool _transformCacheDirty = true; // dynamic value
	mutable bool _transformInverseDirty = true; // dynamic value

	NodeEventFlags _eventFlags = NodeEventFlags::None;

	// This node's own opt-in as an ancestor-components listener (see setWantsAncestorComponents)
	bool _wantsAncestorComponents = false;

	// Number of active ancestor-components listeners in this node's subtree (self + all
	// descendants): enabled Systems with SystemFlags::HandleAncestorComponents plus nodes
	// with _wantsAncestorComponents. When > 0, ancestor ComponentsDirty is propagated into
	// this subtree during visit; when 0, the subtree is pruned
	uint32_t _ancestorComponentsListeners = 0;

	ZOrder _zOrder = ZOrder(0);

	Vec2 _skew;
	Vec2 _anchorPoint;
	Size2 _contentSize;

	Vec3 _position;
	Vec3 _scale = Vec3(1.0f, 1.0f, 1.0f);
	Vec3 _rotation;
	float _inputDensity = 1.0f;
	float _depthIndex = 0.0f;

	// to support HDR, we use float colors;
	Color4F _displayedColor = Color4F::WHITE;
	Color4F _realColor = Color4F::WHITE;

	Quaternion _rotationQuat;

	mutable Mat4 _transform = Mat4::IDENTITY;
	mutable Mat4 _inverse = Mat4::IDENTITY;
	Mat4 _modelViewTransform = Mat4::IDENTITY;

	Vector<Rc<Node>> _children;
	Node *_parent = nullptr;

	Vector<Rc<System>> _systems;

	Scene *_scene = nullptr;
	Director *_director = nullptr;
	Scheduler *_scheduler = nullptr;
	ActionManager *_actionManager = nullptr;
	FrameContext *_frameContext = nullptr;

	Rc<ActionStorage> _actionStorage;
};

template <typename T>
auto Node::getSystemByType() const -> T * {
	for (auto &it : _systems) {
		if (auto ret = dynamic_cast<T *>(it.get())) {
			return ret;
		}
	}
	return nullptr;
}

template <typename T>
auto Node::getSystemByType(uint64_t tag) const -> T * {
	for (auto &it : _systems) {
		if (it->getFrameTag() == tag) {
			if (auto ret = dynamic_cast<T *>(it.get())) {
				return ret;
			}
		}
	}
	return nullptr;
}

template <typename T>
bool Node::findParentWithComponent(
		const Callback<bool(NotNull<Node>, NotNull<const T>, uint32_t depth)> &cb) const {
	bool found = false;
	uint32_t depth = 0;
	auto parent = _parent;
	while (parent) {
		if (auto c = parent->getComponent<T>()) {
			found = true;
			if (!cb(parent, c, depth)) {
				return true;
			}
		}
		parent = parent->getParent();
		++depth;
	}
	return found;
}

template <typename T>
bool Node::enumerateChildsWithComponent(
		const Callback<bool(NotNull<Node>, NotNull<const T>, uint32_t depth)> &cb,
		uint32_t maxDepth) {
	bool shouldStop = false;
	return enumerateChildsWithComponent(cb, maxDepth, 0, shouldStop);
}

template <typename T>
bool Node::enumerateChildsWithComponent(
		const Callback<bool(NotNull<Node>, NotNull<const T>, uint32_t depth)> &cb,
		uint32_t maxDepth, uint32_t depth, bool &shouldStop) {
	bool found = false;
	for (auto &it : _children) {
		if (auto c = it->getComponent<T>()) {
			found = true;
			if (!cb(it, c, depth)) {
				shouldStop = true;
				return true;
			}
		}
		if (depth != maxDepth) {
			if (it->enumerateChildsWithComponent(cb, maxDepth, depth + 1, shouldStop, found)) {
				found = true;
			}
			if (shouldStop == true) {
				return true;
			}
		}
	}
	return found;
}

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_NODES_XLNODE_H_ */
