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

#include "XLNode.h"

#include "XLInputListener.h"
#include "XLScene.h"
#include "XLDirector.h"
#include "XLScheduler.h"
#include "XLActionManager.h"
#include "XLFrameContext.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

ComponentId NodeIdentity::Id;
ComponentId MeasureComponent::Id;
ComponentId VisibilityComponent::Id;

void ActionStorage::addAction(Rc<Action> &&a) { actionToStart.emplace_back(move(a)); }

void ActionStorage::removeAction(Action *a) {
	auto it = sprt::find(actionToStart.begin(), actionToStart.end(), a);
	if (it != actionToStart.end()) {
		actionToStart.erase(it);
	}
}

void ActionStorage::removeAllActions() { actionToStart.clear(); }

void ActionStorage::removeActionByTag(uint32_t tag) {
	auto it = actionToStart.begin();
	while (it != actionToStart.end()) {
		if (it->get()->getTag() == tag) {
			actionToStart.erase(it);
			return;
		}
		++it;
	}
}

void ActionStorage::removeAllActionsByTag(uint32_t tag) {
	auto it = actionToStart.begin();
	while (it != actionToStart.end()) {
		if (it->get()) {
			if (it->get()->getTag() == tag) {
				it = actionToStart.erase(it);
				continue;
			}
		}
		++it;
	}
}

Action *ActionStorage::getActionByTag(uint32_t tag) {
	for (auto &it : actionToStart) {
		if (it->getTag() == tag) {
			return it;
		}
	}
	return nullptr;
}

uint64_t DataIdentity::allocate() {
	// data sets are also built on worker threads (VectorCanvas, deferred Label), so this must be
	// atomic; 0 is reserved for "no identity"
	static sprt::atomic<uint64_t> s_dataIdentity(1);
	return s_dataIdentity.fetch_add(1);
}

String MaterialInfo::description() const {
	StringStream stream;

	stream << "{" << images[0] << "," << images[1] << "," << images[2] << "," << images[3] << "},"
		   << "{" << samplers[0] << "," << samplers[1] << "," << samplers[2] << "," << samplers[3]
		   << "},"
		   << "{" << colorModes[0].toInt() << "," << colorModes[1].toInt() << ","
		   << colorModes[2].toInt() << "," << colorModes[3].toInt() << "},"
		   << "," << pipeline.description();

	return stream.str();
}

bool MaterialInfo::hasImage(uint64_t id) const {
	for (auto &it : images) {
		if (it == id) {
			return true;
		}
	}
	return false;
}

bool Node::isParent(Node *parent, Node *node) {
	if (!node) {
		return false;
	}
	auto p = node->getParent();
	while (p) {
		if (p == parent) {
			return true;
		}
		p = p->getParent();
	}
	return false;
}

Mat4 Node::getChainNodeToParentTransform(Node *parent, Node *node, bool withParent) {
	if (!isParent(parent, node)) {
		return Mat4::IDENTITY;
	}

	Mat4 ret = node->getNodeToParentTransform();
	auto p = node->getParent();
	for (; p != parent; p = p->getParent()) { ret = ret * p->getNodeToParentTransform(); }
	if (withParent && p == parent) {
		ret = ret * p->getNodeToParentTransform();
	}
	return ret;
}

Mat4 Node::getChainParentToNodeTransform(Node *parent, Node *node, bool withParent) {
	if (!isParent(parent, node)) {
		return Mat4::IDENTITY;
	}

	Mat4 ret = node->getParentToNodeTransform();
	auto p = node->getParent();
	for (; p != parent; p = p->getParent()) { ret = p->getParentToNodeTransform() * ret; }
	if (withParent && p == parent) {
		ret = p->getParentToNodeTransform() * ret;
	}
	return ret;
}

Node::Node() { }

Node::~Node() {
	for (auto &child : _children) { child->_parent = nullptr; }

	// stopAllActions();

	XLASSERT(!_running,
			"Node still marked as running on node destruction! Was base class onExit() called in "
			"derived class onExit() implementations?");
}

bool Node::init() { return true; }

void Node::setLocalZOrder(ZOrder z) {
	if (_zOrder == z) {
		return;
	}

	_zOrder = z;
	if (_parent) {
		_parent->reorderChild(this, z);
	}
}

void Node::setScale(float scale) {
	if (_scale.x == scale && _scale.x == scale && _scale.x == scale) {
		return;
	}

	_scale.x = _scale.y = _scale.z = scale;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setScale(const Vec2 &scale) {
	if (_scale.x == scale.x && _scale.y == scale.y) {
		return;
	}

	_scale.x = scale.x;
	_scale.y = scale.y;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setScale(const Vec3 &scale) {
	if (_scale == scale) {
		return;
	}

	_scale = scale;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setScaleX(float scaleX) {
	if (_scale.x == scaleX) {
		return;
	}

	_scale.x = scaleX;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setScaleY(float scaleY) {
	if (_scale.y == scaleY) {
		return;
	}

	_scale.y = scaleY;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setScaleZ(float scaleZ) {
	if (_scale.z == scaleZ) {
		return;
	}

	_scale.z = scaleZ;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setPosition(const Vec2 &position) {
	if (_position.x == position.x && _position.y == position.y) {
		return;
	}

	_position.x = position.x;
	_position.y = position.y;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setPosition(const Vec3 &position) {
	if (_position == position) {
		return;
	}

	_position = position;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setPositionX(float value) {
	if (_position.x == value) {
		return;
	}

	_position.x = value;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setPositionY(float value) {
	if (_position.y == value) {
		return;
	}

	_position.y = value;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setPositionZ(float value) {
	if (_position.z == value) {
		return;
	}

	_position.z = value;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setSkewX(float skewX) {
	if (_skew.x == skewX) {
		return;
	}

	_skew.x = skewX;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setSkewY(float skewY) {
	if (_skew.y == skewY) {
		return;
	}

	_skew.y = skewY;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setAnchorPoint(const Vec2 &point) {
	if (point == _anchorPoint) {
		return;
	}

	_anchorPoint = point;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::setContentSize(const Size2 &size) {
	if (size == _contentSize) {
		return;
	}

	_contentSize = size;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = _contentSizeDirty = true;

	if (_parent) {
		_parent->notifyChildContentSizeDirty(this);
	}
}

void Node::setVisible(bool visible) {
	if (visible == _visible) {
		return;
	}
	_visible = visible;
	if (_visible) {
		_contentSizeDirty = _transformInverseDirty = _transformCacheDirty = _transformDirty = true;
	}
}

void Node::setRotation(float rotation) {
	if (_rotation.z == rotation && _rotation.x == 0 && _rotation.y == 0) {
		return;
	}

	_rotation = Vec3(0.0f, 0.0f, rotation);
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
	_rotationQuat = Quaternion(_rotation);
}

void Node::setRotation(const Vec3 &rotation) {
	if (_rotation == rotation) {
		return;
	}

	_rotation = rotation;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
	_rotationQuat = Quaternion(_rotation);
}

void Node::setRotation(const Quaternion &quat) {
	if (_rotationQuat == quat) {
		return;
	}

	_rotationQuat = quat;
	_rotation = _rotationQuat.toEulerAngles();
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::addChildNode(Node *child) { addChildNode(child, child->_zOrder, InvalidTag); }

void Node::addChildNode(Node *child, ZOrder localZOrder) {
	addChildNode(child, localZOrder, InvalidTag);
}

void Node::addChildNode(Node *child, ZOrder localZOrder, uint64_t tag) {
	XLASSERT(child != nullptr, "Argument must be non-nil");
	XLASSERT(child->_parent == nullptr, "child already added. It can't be added again");

	if constexpr (config::NodePreallocateChilds > 1) {
		if (_children.empty()) {
			_children.reserve(config::NodePreallocateChilds);
		}
	}

	_reorderChildDirty = true;
	_children.push_back(child);
	child->setLocalZOrder(localZOrder);
	if (tag != InvalidTag) {
		child->setTag(tag);
	}
	child->setParent(this);

	// pull the child subtree's ancestor-components listeners into this chain
	if (child->_ancestorComponentsListeners) {
		adjustAncestorComponentsListeners(int32_t(child->_ancestorComponentsListeners));
	}

	if (_running) {
		child->handleEnter(_scene);
		child->handleLayoutInParent(this);
	}

	if (_cascadeColorEnabled) {
		updateCascadeColor();
	}

	if (_cascadeOpacityEnabled) {
		updateCascadeOpacity();
	}
}

Node *Node::getChildByTag(uint64_t tag) const {
	XLASSERT(tag != InvalidTag, "Invalid tag");
	for (const auto &child : _children) {
		if (child && child->getTag() == tag) {
			return child;
		}
	}
	return nullptr;
}

void Node::setParent(Node *parent) {
	if (parent == _parent) {
		return;
	}
	_parent = parent;
	_transformInverseDirty = _transformCacheDirty = _transformDirty = true;
}

void Node::removeFromParent(bool cleanup) {
	if (_parent != nullptr) {
		_parent->removeChild(this, cleanup);
	}
}

void Node::removeChild(Node *child, bool cleanup) {
	// explicit nil handling
	if (_children.empty()) {
		return;
	}

	auto it = sprt::find(_children.begin(), _children.end(), child);
	if (it != _children.end()) {
		if (_running) {
			child->handleExit();
		}

		if (cleanup) {
			child->cleanup();
		}

		// release the child subtree's remaining ancestor-components listeners from this chain,
		// before detaching (cleanup already decremented any removed systems while still linked)
		if (child->_ancestorComponentsListeners) {
			adjustAncestorComponentsListeners(-int32_t(child->_ancestorComponentsListeners));
		}

		// set parent nil at the end
		child->setParent(nullptr);
		_children.erase(it);
	}
}

void Node::removeChildByTag(uint64_t tag, bool cleanup) {
	XLASSERT(tag != InvalidTag, "Invalid tag");

	Node *child = this->getChildByTag(tag);
	if (child == nullptr) {
		log::source().warn("Node", "removeChildByTag(tag = ", tag, "): child not found!");
	} else {
		this->removeChild(child, cleanup);
	}
}

void Node::removeAllChildren(bool cleanup) {
	auto childs = sp::move(_children);
	_children.clear();

	for (const auto &child : childs) {
		if (_running) {
			child->handleExit();
		}

		if (cleanup) {
			child->cleanup();
		}

		// release the child subtree's remaining ancestor-components listeners from this chain
		if (child->_ancestorComponentsListeners) {
			adjustAncestorComponentsListeners(-int32_t(child->_ancestorComponentsListeners));
		}

		// set parent nil at the end
		child->setParent(nullptr);
	}
}

void Node::reorderChild(Node *child, ZOrder localZOrder) {
	XLASSERT(child != nullptr, "Child must be non-nil");
	_reorderChildDirty = true;
	child->setLocalZOrder(localZOrder);
}

bool Node::sortAllChildren() {
	bool ret = false;
	if (_reorderChildDirty && !_children.empty()) {
		sprt::sort(sprt::begin(_children), sprt::end(_children), [&](const Node *l, const Node *r) {
			return l->getLocalZOrder() < r->getLocalZOrder();
		});
		ret = true;
	}
	_reorderChildDirty = false;
	return ret;
}

void Node::runActionObject(Action *action) {
	XLASSERT(action != nullptr, "Argument must be non-nil");

	if (_actionManager) {
		_actionManager->addAction(action, this, !_running);
	} else {
		if (!_actionStorage) {
			_actionStorage = Rc<ActionStorage>::alloc();
		}

		_actionStorage->addAction(action);
	}
}

void Node::runActionObject(Action *action, uint32_t tag) {
	if (action) {
		action->setTag(tag);
	}
	runActionObject(action);
}

void Node::stopAllActions() {
	if (_actionManager) {
		_actionManager->removeAllActionsFromTarget(this);
	} else if (_actionStorage) {
		_actionStorage->removeAllActions();
	}
}

void Node::stopAction(Action *action) {
	if (_actionManager) {
		_actionManager->removeAction(action);
	} else if (_actionStorage) {
		_actionStorage->removeAction(action);
	}
}

void Node::stopActionByTag(uint32_t tag) {
	XLASSERT(tag != Action::INVALID_TAG, "Invalid tag");
	if (_actionManager) {
		_actionManager->removeActionByTag(tag, this);
	} else if (_actionStorage) {
		_actionStorage->removeActionByTag(tag);
	}
}

void Node::stopAllActionsByTag(uint32_t tag) {
	XLASSERT(tag != Action::INVALID_TAG, "Invalid tag");
	if (_actionManager) {
		_actionManager->removeAllActionsByTag(tag, this);
	} else if (_actionStorage) {
		_actionStorage->removeAllActionsByTag(tag);
	}
}

Action *Node::getActionByTag(uint32_t tag) {
	XLASSERT(tag != Action::INVALID_TAG, "Invalid tag");
	if (_actionManager) {
		return _actionManager->getActionByTag(tag, this);
	} else if (_actionStorage) {
		return _actionStorage->getActionByTag(tag);
	}
	return nullptr;
}

size_t Node::getNumberOfRunningActions() const {
	if (_actionManager) {
		return _actionManager->getNumberOfRunningActionsInTarget(this);
	} else if (_actionStorage) {
		return _actionStorage->actionToStart.size();
	}
	return 0;
}

StringView Node::getName() const {
	if (auto d = getComponent<NodeIdentity>()) {
		return d->name;
	}
	return StringView();
}

void Node::setName(StringView str) {
	setOrUpdateComponent<NodeIdentity>([&](NodeIdentity *data) {
		if (data->name != str) {
			data->name = str.str<Interface>();
			return true;
		}
		return false;
	});
}

StringView Node::getType() const {
	if (auto d = getComponent<NodeIdentity>()) {
		return d->type;
	}
	return StringView();
}

void Node::setType(StringView str) {
	setOrUpdateComponent<NodeIdentity>([&](NodeIdentity *data) {
		if (data->type != str) {
			data->type = str.str<Interface>();
			return true;
		}
		return false;
	});
}

void Node::addStyleClass(StringView cl) {
	setOrUpdateComponent<NodeIdentity>([&](NodeIdentity *d) {
		auto it = d->classes.find(cl);
		if (it == d->classes.end()) {
			d->classes.emplace(cl.str<Interface>());
			return true;
		}
		return false;
	});
}

void Node::removeStyleClass(StringView cl) {
	updateComponent<NodeIdentity>([&](NodeIdentity *d) {
		auto it = d->classes.find(cl);
		if (it != d->classes.end()) {
			d->classes.erase(it);
			return true;
		}
		return false;
	});
}

void Node::toggleStyleClass(StringView cl) {
	setOrUpdateComponent<NodeIdentity>([&](NodeIdentity *d) {
		auto it = d->classes.find(cl);
		if (it == d->classes.end()) {
			d->classes.emplace(cl.str<Interface>());
		} else {
			d->classes.erase(it);
		}
		return true;
	});
}

bool Node::hasStyleClass(StringView cl) const {
	if (auto d = getComponent<NodeIdentity>()) {
		auto it = d->classes.find(cl);
		return it != d->classes.end();
	}
	return false;
}

const HashSet<String, sprt::hash<void>> *Node::getStyleClasses() const {
	if (auto d = getComponent<NodeIdentity>()) {
		return &d->classes;
	}
	return nullptr;
}

const Value &Node::getDataValue() const {
	if (auto d = getComponent<NodeIdentity>()) {
		return d->value;
	}
	return Value::Null;
}

void Node::setDataValue(Value &&val) {
	setOrUpdateComponent<NodeIdentity>([&](NodeIdentity *data) {
		data->value = sp::move(val);
		return true;
	});
}

uint64_t Node::getTag() const {
	if (auto d = getComponent<NodeIdentity>()) {
		return d->tag;
	}
	return InvalidTag;
}

void Node::setTag(uint64_t tag) {
	setOrUpdateComponent<NodeIdentity>([&](NodeIdentity *data) {
		if (data->tag != tag) {
			data->tag = tag;
			return true;
		}
		return false;
	});
}


void Node::setEventFlags(NodeEventFlags flags) {
	_eventFlags = flags;
	if (_parent) {
		_parent->notifyChildContentSizeDirty(this);
	}
}

bool Node::addSystemItem(System *com) { return addSystemItem(com, com->getSystemPriority()); }

bool Node::addSystemItem(System *com, uint32_t priority) {
	XLASSERT(com != nullptr, "Argument must be non-nil");
	XLASSERT(com->getOwner() == nullptr, "System already added. It can't be added again");

	com->setSystemPriority(priority);

	// keep _systems sorted by ascending priority: lower priority is dispatched earlier.
	// stable — a new system is inserted after existing systems of equal priority (add order)
	size_t pos = _systems.size();
	for (size_t i = 0; i < _systems.size(); ++i) {
		if (_systems[i]->getSystemPriority() > priority) {
			pos = i;
			break;
		}
	}
	_systems.insert(_systems.begin() + pos, com);

	com->handleAdded(this);

	if (this->isRunning() && hasFlag(com->getSystemFlags(), SystemFlags::HandleSceneEvents)) {
		com->handleEnter(_scene);
	}

	return true;
}

void Node::updateSystemPriority(System *com) {
	auto it = sprt::find(_systems.begin(), _systems.end(), com);
	if (it == _systems.end()) {
		return;
	}

	const size_t idx = size_t(it - _systems.begin());
	const uint32_t priority = com->getSystemPriority();

	// nothing to do if the current position already keeps _systems sorted
	const bool ordered = (idx == 0 || _systems[idx - 1]->getSystemPriority() <= priority)
			&& (idx + 1 == _systems.size() || _systems[idx + 1]->getSystemPriority() >= priority);
	if (ordered) {
		return;
	}

	// hold a reference across the erase, then re-insert at the sorted position
	Rc<System> sys = *it;
	_systems.erase(it);

	size_t pos = _systems.size();
	for (size_t i = 0; i < _systems.size(); ++i) {
		if (_systems[i]->getSystemPriority() > priority) {
			pos = i;
			break;
		}
	}
	_systems.insert(_systems.begin() + pos, sp::move(sys));
}

bool Node::removeSystem(System *com) {
	if (_systems.empty()) {
		return false;
	}

	for (auto iter = _systems.begin(); iter != _systems.end(); ++iter) {
		if ((*iter) == com) {
			if (com->isAncestorComponentsCounted()) {
				adjustAncestorComponentsListeners(-1);
				com->clearAncestorComponentsCounted();
			}

			if (this->isRunning()
					&& hasFlag(com->getSystemFlags(), SystemFlags::HandleSceneEvents)) {
				com->handleExit();
			}

			com->handleRemoved();

			_systems.erase(iter);
			return true;
		}
	}
	return false;
}

bool Node::removeSystemByTag(uint64_t tag) {
	if (_systems.empty()) {
		return false;
	}

	for (auto iter = _systems.begin(); iter != _systems.end(); ++iter) {
		if ((*iter)->getFrameTag() == tag) {
			auto com = (*iter);
			if (com->isAncestorComponentsCounted()) {
				adjustAncestorComponentsListeners(-1);
				com->clearAncestorComponentsCounted();
			}
			if (this->isRunning()
					&& hasFlag(com->getSystemFlags(), SystemFlags::HandleSceneEvents)) {
				com->handleExit();
			}
			if (hasFlag(com->getSystemFlags(), SystemFlags::HandleOwnerEvents)) {
				com->handleRemoved();
			}
			_systems.erase(iter);
			return true;
		}
	}
	return false;
}

bool Node::removeAllSystemByTag(uint64_t tag) {
	if (_systems.empty()) {
		return false;
	}

	auto iter = _systems.begin();
	while (iter != _systems.end()) {
		if ((*iter)->getFrameTag() == tag) {
			auto com = (*iter);
			if (com->isAncestorComponentsCounted()) {
				adjustAncestorComponentsListeners(-1);
				com->clearAncestorComponentsCounted();
			}
			if (this->isRunning()
					&& hasFlag(com->getSystemFlags(), SystemFlags::HandleSceneEvents)) {
				com->handleExit();
			}
			if (hasFlag(com->getSystemFlags(), SystemFlags::HandleOwnerEvents)) {
				com->handleRemoved();
			}
			iter = _systems.erase(iter);
		} else {
			++iter;
		}
	}
	return false;
}

void Node::removeAllSystems() {
	auto tmp = sp::move(_systems);
	_systems.clear();

	for (auto iter : tmp) {
		if (iter->isAncestorComponentsCounted()) {
			adjustAncestorComponentsListeners(-1);
			iter->clearAncestorComponentsCounted();
		}
		if (this->isRunning() && hasFlag(iter->getSystemFlags(), SystemFlags::HandleSceneEvents)) {
			iter->handleExit();
		}
		if (hasFlag(iter->getSystemFlags(), SystemFlags::HandleOwnerEvents)) {
			iter->handleRemoved();
		}
	}
}

void Node::handleEnter(Scene *scene) {
	_scene = scene;
	_director = scene->getDirector();

	if (!_frameContext && _parent) {
		_frameContext = _parent->getFrameContext();
	} else if (_frameContext) {
		_frameContext->onEnter(scene);
	}

	if (_scheduler != _director->getScheduler()) {
		if (_scheduler) {
			_scheduler->unschedule(this);
		}
		_scheduler = _director->getScheduler();
	}

	if (_actionManager != _director->getActionManager()) {
		if (_actionManager) {
			_actionManager->removeAllActionsFromTarget(this);
		}
		_actionManager = _director->getActionManager();

		if (_actionStorage) {
			for (auto &it : _actionStorage->actionToStart) { runActionObject(it); }
			_actionStorage = nullptr;
		}
	}

	auto tmSystems = _systems;
	for (auto &it : tmSystems) {
		if (hasFlag(it->getSystemFlags(), SystemFlags::HandleSceneEvents)) {
			it->handleEnter(scene);
		}
	}

	auto childs = _children;
	for (auto &child : childs) { child->handleEnter(scene); }

	if (_scheduled) {
		_scheduler->scheduleUpdate(this, 0, _paused);
	}

	_running = true;
	this->resume();
}

void Node::handleExit() {
	// In reverse order from onEnter()

	this->pause();
	_running = false;

	if (_scheduled) {
		_scheduler->unschedule(this);
		_scheduled = true; // -re-enable after restart;
	}

	auto childs = _children;
	for (auto &child : childs) { child->handleExit(); }

	auto tmpSystems = _systems;
	for (auto &it : tmpSystems) {
		if (hasFlag(it->getSystemFlags(), SystemFlags::HandleSceneEvents)) {
			it->handleExit();
		}
	}

	if (_frameContext && _parent && _parent->getFrameContext() != _frameContext) {
		_frameContext->onExit();
	} else {
		_frameContext = nullptr;
	}

	// prevent node destruction until update is ended
	_director->autorelease(this);

	_scene = nullptr;
	_director = nullptr;
}

void Node::handleMeasure() {
	// fix the node's own size via the HandleMeasure protocol (see LayoutSystem::measureNode /
	// dispatchLayoutApplied, but applied to self). Must not change components.
	// Constraints: treat the currently-assigned box as available (a non-zero dimension
	// constrains wrapping); unconstrained axes fall back to maxOf<float>()
	MeasureConstraints c;
	if (_contentSize.width > 0.0f) {
		c.maxWidth = _contentSize.width;
	}
	if (_contentSize.height > 0.0f) {
		c.maxHeight = _contentSize.height;
	}

	auto tmpSystems = _systems;
	bool measured = false;
	for (auto &it : tmpSystems) {
		if (it->isEnabled() && hasFlag(it->getSystemFlags(), SystemFlags::HandleMeasure)) {
			Size2 result;
			if (it->handleMeasure(c, result)) {
				setContentSize(result);
				measured = true;
				break;
			}
		}
	}

	// fallback: use the precomputed size stored in a MeasureComponent, if present. A per-axis value
	// < 0 means "unspecified" (the style resolver only fills the axes CSS gave), so keep the current
	// size on those axes rather than committing a negative size
	if (!measured) {
		if (auto mc = getComponent<MeasureComponent>()) {
			Size2 cs = _contentSize;
			const Size2 req = mc->measure(c);
			if (req.width >= 0.0f) {
				cs.width = req.width;
			}
			if (req.height >= 0.0f) {
				cs.height = req.height;
			}
			setContentSize(cs);
		}
	}

	for (auto &it : tmpSystems) {
		if (it->isEnabled() && hasFlag(it->getSystemFlags(), SystemFlags::HandleMeasure)) {
			it->handleLayoutApplied(_contentSize);
		}
	}
}

// Deliver a descendant event to the nearest opted-in ancestor system on each frame-stack tag.
// The node's own systems are not on the stack yet during its phase processing (pushed later in
// wrapVisit), so only strict ancestors are visited - exactly the intended bubble-up semantics
template <typename Fn>
static void notifyStackChildEvent(FrameInfo &info, SystemFlags flag, const Fn &fn) {
	for (auto &it : info.systemStack) {
		if (it.second.empty()) {
			continue;
		}
		auto &sys = it.second.back();
		if (sys->isEnabled() && hasFlag(sys->getSystemFlags(), flag)) {
			fn(sys.get());
		}
	}
}

void Node::handleComponentsDirty(FrameInfo &info, const ComponentMask &mask) {
	handleComponentsDirty(mask);
	notifyStackChildEvent(info, SystemFlags::HandleChildComponents,
			[&](System *sys) { sys->handleChildComponentsDirty(this, mask); });
}

void Node::handleMeasure(FrameInfo &info) {
	handleMeasure();
	notifyStackChildEvent(info, SystemFlags::HandleChildMeasure,
			[&](System *sys) { sys->handleChildMeasure(this); });
}

void Node::handleContentSizeDirty(FrameInfo &info) {
	handleContentSizeDirty();
	notifyStackChildEvent(info, SystemFlags::HandleChildNodeEvents,
			[&](System *sys) { sys->handleChildContentSizeDirty(this); });
}

void Node::handleLayoutChildren(FrameInfo &info) {
	handleLayoutChildren();
	notifyStackChildEvent(info, SystemFlags::HandleChildLayoutChildren,
			[&](System *sys) { sys->handleChildLayoutChildren(this); });
}

void Node::handleContentSizeDirty() {
	auto tmpSystems = _systems;
	for (auto &it : tmpSystems) {
		if (hasFlag(it->getSystemFlags(), SystemFlags::HandleNodeEvents)) {
			it->handleContentSizeDirty();
		}
	}

	auto tmp = _children;
	for (auto &it : tmp) { it->handleLayoutInParent(this); }
}

void Node::handleComponentsDirty(const ComponentMask &mask) {
	auto tmpSystems = _systems;
	for (auto &it : tmpSystems) {
		if (hasFlag(it->getSystemFlags(), SystemFlags::HandleComponents)) {
			it->handleComponentsDirty(mask);
		}
	}
}

void Node::handleAncestorComponentsDirty() {
	auto tmpSystems = _systems;
	for (auto &it : tmpSystems) {
		if (it->isEnabled()
				&& hasFlag(it->getSystemFlags(), SystemFlags::HandleAncestorComponents)) {
			it->handleComponentsDirty(ComponentMask());
		}
	}
}

void Node::adjustAncestorComponentsListeners(int32_t delta) {
	if (delta == 0) {
		return;
	}
	for (Node *n = this; n; n = n->_parent) {
		XLASSERT(delta >= 0 || n->_ancestorComponentsListeners >= uint32_t(-delta),
				"ancestor-components listener counter underflow");
		n->_ancestorComponentsListeners =
				uint32_t(int32_t(n->_ancestorComponentsListeners) + delta);
	}
}

void Node::setWantsAncestorComponents(bool b) {
	if (b == _wantsAncestorComponents) {
		return;
	}
	_wantsAncestorComponents = b;
	adjustAncestorComponentsListeners(b ? 1 : -1);
}

void Node::handleTransformDirty(const Mat4 &parentTransform) {
	auto tmpSystems = _systems;
	for (auto &it : tmpSystems) {
		if (hasFlag(it->getSystemFlags(), SystemFlags::HandleNodeEvents)) {
			it->handleTransformDirty(parentTransform);
		}
	}
}

void Node::handleGlobalTransformDirty(const Mat4 &parentTransform) {
	Vec3 scale;
	parentTransform.decompose(&scale, nullptr, nullptr);

	if (_scale.x != 1.f) {
		scale.x *= _scale.x;
	}
	if (_scale.y != 1.f) {
		scale.y *= _scale.y;
	}
	if (_scale.z != 1.f) {
		scale.z *= _scale.z;
	}

	auto density = sprt::min(sprt::min(scale.x, scale.y), scale.z);
	if (density != _inputDensity) {
		_inputDensity = density;
	}
}

void Node::handleReorderChildDirty() {
	auto tmpSystems = _systems;
	for (auto &it : tmpSystems) {
		if (hasFlag(it->getSystemFlags(), SystemFlags::HandleNodeEvents)) {
			it->handleReorderChildDirty();
		}
	}
}

void Node::handleLayoutInParent(Node *parent) {
	auto tmpSystems = _systems;
	for (auto &it : tmpSystems) {
		if (hasFlag(it->getSystemFlags(), SystemFlags::HandleNodeEvents)) {
			it->handleLayoutInParent(parent);
		}
	}
}

void Node::handleLayoutChildren() {
	auto tmpSystems = _systems;
	for (auto &it : tmpSystems) {
		if (it->isEnabled() && hasFlag(it->getSystemFlags(), SystemFlags::HandleLayoutChildren)) {
			it->handleLayoutChildren();
		}
	}
}

void Node::notifyChildContentSizeDirty(Node *child) {
	auto tmpSystems = _systems;
	for (auto &it : tmpSystems) {
		if (hasFlag(it->getSystemFlags(), SystemFlags::HandleChildNodeEvents)) {
			it->handleChildContentSizeDirty(child);
		}
	}
}

void Node::cleanup() {
	if (_actionManager) {
		this->stopAllActions();
	}
	if (_scheduler) {
		this->unscheduleUpdate();
	}

	auto childs = _children;
	for (auto &child : childs) { child->cleanup(); }

	this->removeAllSystems();
	this->removeAllComponents();
}

Rect Node::getBoundingBox() const {
	Rect rect(0, 0, _contentSize.width, _contentSize.height);
	return TransformRect(rect, getNodeToParentTransform());
}

void Node::resume() {
	if (_paused) {
		_paused = false;
		if (_running && _scheduled) {
			_scheduler->resume(this);
			_actionManager->resumeTarget(this);
		}
	}
}

void Node::pause() {
	if (!_paused) {
		if (_running && _scheduled) {
			_actionManager->pauseTarget(this);
			_scheduler->pause(this);
		}
		_paused = true;
	}
}

void Node::update(const UpdateTime &time) { }

const Mat4 &Node::getNodeToParentTransform() const {
	if (_transformCacheDirty) {
		// Translate values
		float x = _position.x;
		float y = _position.y;
		float z = _position.z;

		bool needsSkewMatrix = (_skew.x || _skew.y);

		Vec2 anchorPointInPoints(_contentSize.width * _anchorPoint.x,
				_contentSize.height * _anchorPoint.y);
		Vec2 anchorPoint(anchorPointInPoints.x * _scale.x, anchorPointInPoints.y * _scale.y);

		// caculate real position
		if (!needsSkewMatrix && anchorPointInPoints != Vec2::ZERO) {
			x += -anchorPoint.x;
			y += -anchorPoint.y;
		}

		// Build Transform Matrix = translation * rotation * scale
		Mat4 translation;
		//move to anchor point first, then rotate
		Mat4::createTranslation(x + anchorPoint.x, y + anchorPoint.y, z, &translation);

		Mat4::createRotation(_rotationQuat, &_transform);

		_transform = translation * _transform;
		//move by (-anchorPoint.x, -anchorPoint.y, 0) after rotation
		_transform.translate(-anchorPoint.x, -anchorPoint.y, 0);

		if (_scale.x != 1.f) {
			_transform.m[0] *= _scale.x;
			_transform.m[1] *= _scale.x;
			_transform.m[2] *= _scale.x;
		}
		if (_scale.y != 1.f) {
			_transform.m[4] *= _scale.y;
			_transform.m[5] *= _scale.y;
			_transform.m[6] *= _scale.y;
		}
		if (_scale.z != 1.f) {
			_transform.m[8] *= _scale.z;
			_transform.m[9] *= _scale.z;
			_transform.m[10] *= _scale.z;
		}

		// If skew is needed, apply skew and then anchor point
		if (needsSkewMatrix) {
			Mat4 skewMatrix(1, (float)tanf(_skew.y), 0, 0, (float)tanf(_skew.x), 1, 0, 0, 0, 0, 1,
					0, 0, 0, 0, 1);

			_transform = _transform * skewMatrix;

			// adjust anchor point
			if (anchorPointInPoints != Vec2::ZERO) {
				_transform.m[12] += _transform.m[0] * -anchorPointInPoints.x
						+ _transform.m[4] * -anchorPointInPoints.y;
				_transform.m[13] += _transform.m[1] * -anchorPointInPoints.x
						+ _transform.m[5] * -anchorPointInPoints.y;
			}
		}

		_transformCacheDirty = false;
	}

	return _transform;
}

void Node::setNodeToParentTransform(const Mat4 &transform) {
	_transform = transform;
	_transformCacheDirty = false;
	_transformDirty = true;
}

const Mat4 &Node::getParentToNodeTransform() const {
	if (_transformInverseDirty) {
		_inverse = getNodeToParentTransform().getInversed();
		_transformInverseDirty = false;
	}

	return _inverse;
}

Mat4 Node::getNodeToWorldTransform() const {
	Mat4 t(this->getNodeToParentTransform());

	for (Node *p = _parent; p != nullptr; p = p->getParent()) {
		t = p->getNodeToParentTransform() * t;
	}

	return t;
}

Mat4 Node::getWorldToNodeTransform() const { return getNodeToWorldTransform().getInversed(); }

Vec2 Node::convertToNodeSpace(const Vec2 &worldPoint) const {
	Mat4 tmp = getWorldToNodeTransform();
	return tmp.transformPoint(worldPoint);
}

Vec2 Node::convertToWorldSpace(const Vec2 &nodePoint) const {
	Mat4 tmp = getNodeToWorldTransform();
	return tmp.transformPoint(nodePoint);
}

Vec2 Node::convertToNodeSpaceAR(const Vec2 &worldPoint) const {
	Vec2 nodePoint(convertToNodeSpace(worldPoint));
	return nodePoint
			- Vec2(_contentSize.width * _anchorPoint.x, _contentSize.height * _anchorPoint.y);
}

Vec2 Node::convertToWorldSpaceAR(const Vec2 &nodePoint) const {
	return convertToWorldSpace(nodePoint
			+ Vec2(_contentSize.width * _anchorPoint.x, _contentSize.height * _anchorPoint.y));
}

void Node::setCascadeOpacityEnabled(bool cascadeOpacityEnabled) {
	if (_cascadeOpacityEnabled == cascadeOpacityEnabled) {
		return;
	}

	_cascadeOpacityEnabled = cascadeOpacityEnabled;
	if (_cascadeOpacityEnabled) {
		updateCascadeOpacity();
	} else {
		disableCascadeOpacity();
	}
}

void Node::setCascadeColorEnabled(bool cascadeColorEnabled) {
	if (_cascadeColorEnabled == cascadeColorEnabled) {
		return;
	}

	_cascadeColorEnabled = cascadeColorEnabled;
	if (_cascadeColorEnabled) {
		updateCascadeColor();
	} else {
		disableCascadeColor();
	}
}

void Node::setOpacity(float opacity) {
	_displayedColor.a = _realColor.a = opacity;
	updateCascadeOpacity();
}

void Node::setOpacity(OpacityValue value) { setOpacity(value.get() / 255.0f); }

void Node::updateDisplayedOpacity(float parentOpacity) {
	_displayedColor.a = _realColor.a * parentOpacity;

	updateColor();

	if (_cascadeOpacityEnabled) {
		for (const auto &child : _children) { child->updateDisplayedOpacity(_displayedColor.a); }
	}
}

void Node::setColor(const Color4F &color, bool withOpacity) {
	if (withOpacity && _realColor.a != color.a) {
		_displayedColor = _realColor = color;

		updateCascadeColor();
		updateCascadeOpacity();
	} else {
		_realColor = Color4F(color.r, color.g, color.b, _realColor.a);
		_displayedColor = Color4F(color.r, color.g, color.b, _displayedColor.a);

		updateCascadeColor();
	}
}

void Node::updateDisplayedColor(const Color4F &parentColor) {
	_displayedColor.r = _realColor.r * parentColor.r;
	_displayedColor.g = _realColor.g * parentColor.g;
	_displayedColor.b = _realColor.b * parentColor.b;
	updateColor();

	if (_cascadeColorEnabled) {
		for (const auto &child : _children) { child->updateDisplayedColor(_displayedColor); }
	}
}


void Node::updateCascadeOpacity() {
	float parentOpacity = 1.0f;

	if (_parent != nullptr && _parent->isCascadeOpacityEnabled()) {
		parentOpacity = _parent->getDisplayedOpacity();
	}

	updateDisplayedOpacity(parentOpacity);
}

void Node::disableCascadeOpacity() {
	_displayedColor.a = _realColor.a;

	for (const auto &child : _children) { child->updateDisplayedOpacity(1.0f); }
}

void Node::updateCascadeColor() {
	Color4F parentColor = Color4F::WHITE;
	if (_parent && _parent->isCascadeColorEnabled()) {
		parentColor = _parent->getDisplayedColor();
	}

	updateDisplayedColor(parentColor);
}

void Node::disableCascadeColor() {
	for (const auto &child : _children) { child->updateDisplayedColor(Color4F::WHITE); }
}

void Node::draw(FrameInfo &info, NodeVisitFlags flags) { }

bool Node::visitDraw(FrameInfo &info, NodeVisitFlags parentFlags) {
	VisitInfo visitInfo;

	visitInfo.visitBegin = [](const VisitInfo &visitInfo) {
		for (auto &it : visitInfo.visitableSystems) { it->handleVisitBegin(*visitInfo.frameInfo); }
	};

	visitInfo.visitNodesBelow = [](const VisitInfo &visitInfo, SpanView<Rc<Node>> nodes) {
		for (auto &it : visitInfo.visitableSystems) {
			it->handleVisitNodesBelow(*visitInfo.frameInfo, nodes, visitInfo.flags);
		}

		for (auto &it : nodes) { it->visitDraw(*visitInfo.frameInfo, visitInfo.flags); }
	};

	visitInfo.visitSelf = [](const VisitInfo &visitInfo, Node *node) {
		node->visitSelf(*visitInfo.frameInfo, visitInfo.flags, visitInfo.visibleByCamera);
	};

	visitInfo.visitNodesAbove = [](const VisitInfo &visitInfo, SpanView<Rc<Node>> nodes) {
		for (auto &it : visitInfo.visitableSystems) {
			it->handleVisitNodesAbove(*visitInfo.frameInfo, nodes, visitInfo.flags);
		}

		for (auto &it : nodes) { it->visitDraw(*visitInfo.frameInfo, visitInfo.flags); }
	};

	visitInfo.visitEnd = [](const VisitInfo &visitInfo) {
		for (auto &it : visitInfo.visitableSystems) { it->handleVisitEnd(*visitInfo.frameInfo); }
	};

	visitInfo.node = this;

	return wrapVisit(info, parentFlags, visitInfo, true);
}

void Node::scheduleUpdate() {
	if (!_scheduled) {
		_scheduled = true;
		if (_running) {
			_scheduler->scheduleUpdate(this, 0, _paused);
		}
	}
}

void Node::unscheduleUpdate() {
	if (_scheduled) {
		if (_running) {
			_scheduler->unschedule(this);
		}
		_scheduled = false;
	}
}

bool Node::isTouched(const Vec2 &location, float padding) {
	Vec2 point = convertToNodeSpace(location);
	return isTouchedNodeSpace(point, padding);
}

bool Node::isTouchedNodeSpace(const Vec2 &point, float padding) {
	if (!isVisible()) {
		return false;
	}

	const Size2 &size = getContentSize();
	if (point.x > -padding && point.y > -padding && point.x < size.width + padding
			&& point.y < size.height + padding) {
		return true;
	} else {
		return false;
	}
}

void Node::setEnterCallback(Function<void(Scene *)> &&cb) {
	makeDefaultCallbackSystem()->setEnterCallback(
			[cb = sp::move(cb)](CallbackSystem *, Scene *scene) { cb(scene); });
}

void Node::setExitCallback(Function<void()> &&cb) {
	makeDefaultCallbackSystem()->setExitCallback([cb = sp::move(cb)](CallbackSystem *) { cb(); });
}

void Node::setContentSizeDirtyCallback(Function<void()> &&cb) {
	makeDefaultCallbackSystem()->setContentSizeDirtyCallback(
			[cb = sp::move(cb)](CallbackSystem *) { cb(); });
}

void Node::setComponentsDirtyCallback(Function<void(const ComponentMask &mask)> &&cb) {
	makeDefaultCallbackSystem()->setComponentsDirtyCallback(
			[cb = sp::move(cb)](CallbackSystem *, const ComponentMask &mask) { cb(mask); });
}

void Node::setTransformDirtyCallback(Function<void(const Mat4 &)> &&cb) {
	makeDefaultCallbackSystem()->setTransformDirtyCallback(
			[cb = sp::move(cb)](CallbackSystem *, const Mat4 &m) { cb(m); });
}

void Node::setReorderChildDirtyCallback(Function<void()> &&cb) {
	makeDefaultCallbackSystem()->setReorderChildDirtyCallback(
			[cb = sp::move(cb)](CallbackSystem *) { cb(); });
}

void Node::setLayoutCallback(Function<void(Node *)> &&cb) {
	makeDefaultCallbackSystem()->setLayoutCallback(
			[cb = sp::move(cb)](CallbackSystem *, Node *node) { cb(node); });
}

void Node::setMeasureCallback(Function<bool(const MeasureConstraints &, Size2 &)> &&cb) {
	makeDefaultCallbackSystem()->setMeasureCallback(
			[cb = sp::move(cb)](CallbackSystem *, const MeasureConstraints &c, Size2 &result) {
		return cb(c, result);
	});
}

void Node::setLayoutAppliedCallback(Function<void(const Size2 &)> &&cb) {
	makeDefaultCallbackSystem()->setLayoutAppliedCallback(
			[cb = sp::move(cb)](CallbackSystem *, const Size2 &size) { cb(size); });
}

Mat4 Node::transform(const Mat4 &parentTransform) {
	return parentTransform * this->getNodeToParentTransform();
}

NodeVisitFlags Node::processParentFlags(FrameInfo &info, NodeVisitFlags parentFlags) {
	NodeVisitFlags flags = parentFlags;
	const Mat4 &parentWorld = info.modelTransformStack.back();

	// Phase 1: components. Handlers may change node structure AND the node's own components.
	// If a handler re-dirties components, repeat handleComponentsDirty (bounded to 12 iterations)
	const bool ownComponentsDirty = _componentsDirty;
	const bool ancestorComponentsDirty = hasFlag(parentFlags, NodeVisitFlags::ComponentsDirty);
	if (ancestorComponentsDirty) {
		handleAncestorComponentsDirty(); // may set _componentsDirty
	}
	for (int guard = 0; _componentsDirty;) {
		auto mask = _componentsDirtyMask;
		resetComponentsDirty();
		handleComponentsDirty(info, mask); // may re-dirty components / change structure
		if (++guard >= 12) {
			if (_componentsDirty) {
				log::source().warn("Node",
						"handleComponentsDirty did not converge in 12 iterations");
				_componentsDirty = false; // do not clear dirty mask
			}
			break;
		}
	}

	if (ownComponentsDirty || ancestorComponentsDirty) {
		// propagate downward only into subtrees that actually contain a listener; otherwise
		// strip the flag so listener-less subtrees are skipped entirely
		if (_ancestorComponentsListeners > 0) {
			flags |= NodeVisitFlags::ComponentsDirty;
		} else {
			flags &= ~NodeVisitFlags::ComponentsDirty;
		}
	}

	// Phase 2: measure - fix the node's size. Runs before the transform phase so a measure-induced
	// setContentSize (which re-dirties _contentSizeDirty/_transformDirty) is visible to the transform
	// notifications below. Must not change components. Feeds phase 4 and the matrix rebuild
	if (_measureDirty) {
		_measureDirty = false;
		handleMeasure(info);
	}

	// Phase 3: transform notifications - the node's position is fixed. The world matrix itself
	// is (re)built in phase 5, once the size is final (the matrix depends on content size)
	if (_transformDirty
			|| (hasFlag(_eventFlags, NodeEventFlags::HandleParentTransform)
					&& hasFlag(parentFlags, NodeVisitFlags::TransformDirty))) {
		handleTransformDirty(parentWorld);
	}
	if ((flags & NodeVisitFlags::GlobalTransformDirtyMask) != NodeVisitFlags::None
			|| _transformDirty || _contentSizeDirty) {
		handleGlobalTransformDirty(parentWorld);
	}

	// Phase 4: content size - the node's size is now fixed
	if (_contentSizeDirty
			|| (hasFlag(_eventFlags, NodeEventFlags::HandleParentContentSize)
					&& hasFlag(parentFlags, NodeVisitFlags::ContentSizeDirty))) {
		handleContentSizeDirty(info);
		_contentSizeDirty = false;
		flags |= NodeVisitFlags::ContentSizeDirty;
		_layoutChildrenDirty = true; // own size changed -> re-lay-out children
	}

	// Phase 5: build the world matrix with the final size (anchor offset depends on content
	// size), and publish TransformDirty to children if the transform changed this visit
	if (_transformDirty
			|| (flags & NodeVisitFlags::GlobalTransformDirtyMask) != NodeVisitFlags::None) {
		_modelViewTransform = this->transform(parentWorld);
	}
	if (_transformDirty) {
		_transformDirty = false;
		flags |= NodeVisitFlags::TransformDirty;
	}

	return flags;
}

void Node::visitSelf(FrameInfo &info, NodeVisitFlags flags, bool visibleByCamera) {
	auto tmpSystems = _systems;
	for (auto &it : tmpSystems) {
		if (hasFlag(it->getSystemFlags(), SystemFlags::HandleVisitSelf)) {
			it->handleVisitSelf(info, this, flags);
		}
	}

	// self draw
	if (visibleByCamera) {
		this->draw(info, flags);
	}
}

bool Node::isEffectivelyVisible() const {
	if (!_visible) {
		return false;
	}
	if (auto c = getComponent<VisibilityComponent>()) {
		return c->visible();
	}
	return true;
}

bool Node::isDisplayed() const {
	if (!_visible) {
		return false;
	}
	if (auto c = getComponent<VisibilityComponent>()) {
		return !c->displayNone;
	}
	return true;
}

bool Node::wrapVisit(FrameInfo &info, NodeVisitFlags parentFlags, const VisitInfo &visitInfo,
		bool useContext) {
	if (!_visible) {
		return false;
	}

	bool reorderWasDirty = false;
	bool hasFrameContext = false;
	if (useContext && _frameContext) {
		if (_parent && _parent->getFrameContext() != _frameContext) {
			info.pushContext(_frameContext);
			hasFrameContext = true;
		}
	}

	NodeVisitFlags flags = processParentFlags(info, parentFlags);

	// Style-driven visibility (VisibilityComponent) cuts the visit HERE, after the node's own
	// data phases, not at the top like the explicit _visible flag: nothing below runs (no draw,
	// no children, no input), yet the hidden node keeps processing its components each frame,
	// so the styling protocol can still reach it and un-hide it (class change, CSS reload).
	if (!_running || !isEffectivelyVisible()) {
		if (hasFrameContext) {
			info.popContext();
		}
		return false;
	}

	auto order = getLocalZOrder();

	bool visibleByCamera = true;

	info.modelTransformStack.push_back(_modelViewTransform);
	if (order != ZOrderTransparent) {
		info.zPath.push_back(order);
	}

	if (_depthIndex > 0.0f) {
		info.depthStack.push_back(sprt::max(info.depthStack.back(), _depthIndex));
	}

	size_t i = 0;

	visitInfo.flags = flags;
	visitInfo.frameInfo = &info;
	visitInfo.visibleByCamera = visibleByCamera;

	if (sortAllChildren()) {
		reorderWasDirty = true;
	}

	// handleReorderChildDirty не должен менять геометрию и компоненты - запускаем его последним
	if (reorderWasDirty
			|| (hasFlag(_eventFlags, NodeEventFlags::HandleParentReorderChild)
					&& hasFlag(parentFlags, NodeVisitFlags::ReorderChildDirty))) {
		handleReorderChildDirty();
		visitInfo.flags |= NodeVisitFlags::ReorderChildDirty;
		_layoutChildrenDirty = true; // child order changed -> re-lay-out children
	}

	// Phase 6: lay out children with this node's own size and child order now fixed
	if (_layoutChildrenDirty) {
		_layoutChildrenDirty = false;
		handleLayoutChildren(info);
	}

	// End of node-local processing.
	// Publish AddToFrameStack systems onto info.systemStack AFTER this node's own phases ran, so a
	// node only ever sees ANCESTOR systems on the stack (never its own) - this is what makes the
	// child-event dispatch in handle{Measure,ContentSizeDirty,LayoutChildren}(FrameInfo&) bubble up.
	// The stack stays live while children are visited below, and is popped once they are done

	mem_pool::Vector< mem_pool::Vector<Rc<System>> * > systems;

	auto tmpSystems = _systems;
	for (auto &it : tmpSystems) {
		if (it->isEnabled() && hasFlag(it->getSystemFlags(), SystemFlags::AddToFrameStack)
				&& it->getFrameTag() != InvalidTag) {
			systems.emplace_back(info.pushSystem(it));
		}
		if (hasFlag(it->getSystemFlags(), SystemFlags::HandleVisitControl)) {
			visitInfo.visitableSystems.emplace_back(it);
		}
	}

	if (visitInfo.visitBegin) {
		visitInfo.visitBegin(visitInfo);
	}

	if (!_children.empty()) {
		auto c = _children;

		auto t = c.data();

		// draw children zOrder < 0
		for (; i < c.size(); i++) {
			auto node = c.at(i);
			if (node && node->_zOrder >= ZOrder(0)) {
				break;
			}
		}

		if (visitInfo.visitNodesBelow) {
			visitInfo.visitNodesBelow(visitInfo, SpanView<Rc<Node>>(t, t + i));
		}

		if (visitInfo.visitSelf) {
			visitInfo.visitSelf(visitInfo, this);
		}

		if (visitInfo.visitNodesAbove) {
			visitInfo.visitNodesAbove(visitInfo, SpanView<Rc<Node>>(t + i, t + c.size()));
		}

	} else {
		if (visitInfo.visitNodesBelow) {
			visitInfo.visitNodesBelow(visitInfo, SpanView<Rc<Node>>());
		}

		if (visitInfo.visitSelf) {
			visitInfo.visitSelf(visitInfo, this);
		}

		if (visitInfo.visitNodesAbove) {
			visitInfo.visitNodesAbove(visitInfo, SpanView<Rc<Node>>());
		}
	}

	if (visitInfo.visitEnd) {
		visitInfo.visitEnd(visitInfo);
	}

	for (auto &it : systems) { info.popSystem(it); }

	if (_depthIndex > 0.0f) {
		info.depthStack.pop_back();
	}

	if (order != ZOrderTransparent) {
		info.zPath.pop_back();
	}
	info.modelTransformStack.pop_back();

	if (hasFrameContext) {
		info.popContext();
	}

	return true;
}

CallbackSystem *Node::makeDefaultCallbackSystem() {
	auto system = getSystemByType<CallbackSystem>(DefaultCallbackSystemTag);
	if (!system) {
		system = addSystem(Rc<CallbackSystem>::create());
		system->setFrameTag(DefaultCallbackSystemTag);
	}
	return system;
}

float Node::getMaxDepthIndex() const {
	float val = _depthIndex;
	for (auto &it : _children) {
		if (it->isVisible()) {
			val = sprt::max(it->getMaxDepthIndex(), val);
		}
	}
	return val;
}

} // namespace stappler::xenolith
