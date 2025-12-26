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

// Component to store additional Node's data (to reduce Node's memory footprint)
// Most nodes does not need this data to be stored within it
//
// When you use setTag, setName or setValue, this component will be added into Node
struct NodeData {
	static ComponentId Id;

	uint64_t tag = InvalidTag;
	String name;
	Value value;
};

ComponentId NodeData::Id;

void ActionStorage::addAction(Rc<Action> &&a) { actionToStart.emplace_back(move(a)); }

void ActionStorage::removeAction(Action *a) {
	auto it = std::find(actionToStart.begin(), actionToStart.end(), a);
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

	if (_running) {
		child->handleEnter(_scene);
		child->handleLayout(this);
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

	auto it = std::find(_children.begin(), _children.end(), child);
	if (it != _children.end()) {
		if (_running) {
			child->handleExit();
		}

		if (cleanup) {
			child->cleanup();
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
	for (const auto &child : _children) {
		if (_running) {
			child->handleExit();
		}

		if (cleanup) {
			child->cleanup();
		}
		// set parent nil at the end
		child->setParent(nullptr);
	}

	_children.clear();
}

void Node::reorderChild(Node *child, ZOrder localZOrder) {
	XLASSERT(child != nullptr, "Child must be non-nil");
	_reorderChildDirty = true;
	child->setLocalZOrder(localZOrder);
}

bool Node::sortAllChildren() {
	bool ret = false;
	if (_reorderChildDirty && !_children.empty()) {
		std::sort(std::begin(_children), std::end(_children), [&](const Node *l, const Node *r) {
			return l->getLocalZOrder() < r->getLocalZOrder();
		});
		handleReorderChildDirty();
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
	if (auto d = getComponent<NodeData>()) {
		return d->name;
	}
	return StringView();
}

void Node::setName(StringView str) {
	setOrUpdateComponent<NodeData>([&](NodeData *data) {
		if (data->name != str) {
			data->name = str.str<Interface>();
			return true;
		}
		return false;
	});
}

const Value &Node::getDataValue() const {
	if (auto d = getComponent<NodeData>()) {
		return d->value;
	}
	return Value::Null;
}

void Node::setDataValue(Value &&val) {
	setOrUpdateComponent<NodeData>([&](NodeData *data) {
		data->value = sp::move(val);
		return true;
	});
}

uint64_t Node::getTag() const {
	if (auto d = getComponent<NodeData>()) {
		return d->tag;
	}
	return InvalidTag;
}

void Node::setTag(uint64_t tag) {
	setOrUpdateComponent<NodeData>([&](NodeData *data) {
		if (data->tag != tag) {
			data->tag = tag;
			return true;
		}
		return false;
	});
}


void Node::setEventFlags(NodeEventFlags flags) { _eventFlags = flags; }

bool Node::addSystemItem(System *com) {
	XLASSERT(com != nullptr, "Argument must be non-nil");
	XLASSERT(com->getOwner() == nullptr, "System already added. It can't be added again");

	_systems.push_back(com);

	com->handleAdded(this);

	if (this->isRunning() && hasFlag(com->getSystemFlags(), SystemFlags::HandleSceneEvents)) {
		com->handleEnter(_scene);
	}

	return true;
}

bool Node::removeSystem(System *com) {
	if (_systems.empty()) {
		return false;
	}

	for (auto iter = _systems.begin(); iter != _systems.end(); ++iter) {
		if ((*iter) == com) {
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

void Node::handleContentSizeDirty() {
	auto tmpSystems = _systems;
	for (auto &it : tmpSystems) {
		if (hasFlag(it->getSystemFlags(), SystemFlags::HandleNodeEvents)) {
			it->handleContentSizeDirty();
		}
	}

	auto tmp = _children;
	for (auto &it : tmp) { it->handleLayout(this); }
}

void Node::handleComponentsDirty() {
	auto tmpSystems = _systems;
	for (auto &it : tmpSystems) {
		if (hasFlag(it->getSystemFlags(), SystemFlags::HandleComponents)) {
			it->handleComponentsDirty();
		}
	}
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

	auto density = std::min(std::min(scale.x, scale.y), scale.z);
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

void Node::handleLayout(Node *parent) {
	auto tmpSystems = _systems;
	for (auto &it : tmpSystems) {
		if (hasFlag(it->getSystemFlags(), SystemFlags::HandleNodeEvents)) {
			it->handleLayout(parent);
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

	for (auto &child : _children) { child->cleanup(); }

	this->removeAllSystems();
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

bool Node::visitGeometry(FrameInfo &info, NodeVisitFlags parentFlags) {
	VisitInfo visitInfo;

	visitInfo.visitNodesBelow = [](const VisitInfo &visitInfo, SpanView<Rc<Node>> nodes) {
		for (auto &it : nodes) { it->visitGeometry(*visitInfo.frameInfo, visitInfo.flags); }
	};

	visitInfo.visitNodesAbove = [](const VisitInfo &visitInfo, SpanView<Rc<Node>> nodes) {
		for (auto &it : nodes) { it->visitGeometry(*visitInfo.frameInfo, visitInfo.flags); }
	};

	visitInfo.node = this;

	return wrapVisit(info, parentFlags, visitInfo, false);
}

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

void Node::setComponentsDirtyCallback(Function<void()> &&cb) {
	makeDefaultCallbackSystem()->setComponentsDirtyCallback(
			[cb = sp::move(cb)](CallbackSystem *) { cb(); });
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

Mat4 Node::transform(const Mat4 &parentTransform) {
	return parentTransform * this->getNodeToParentTransform();
}

NodeVisitFlags Node::processParentFlags(FrameInfo &info, NodeVisitFlags parentFlags) {
	NodeVisitFlags flags = parentFlags;

	if (_transformDirty
			|| (hasFlag(_eventFlags, NodeEventFlags::HandleParentTransform)
					&& hasFlag(parentFlags, NodeVisitFlags::TransformDirty))) {
		handleTransformDirty(info.modelTransformStack.back());
	}

	if ((flags & NodeVisitFlags::GlobalTransformDirtyMask) != NodeVisitFlags::None
			|| _transformDirty || _contentSizeDirty) {
		_modelViewTransform = this->transform(info.modelTransformStack.back());

		handleGlobalTransformDirty(info.modelTransformStack.back());
	}

	if (_transformDirty) {
		_transformDirty = false;
		flags |= NodeVisitFlags::TransformDirty;
	}

	if (_contentSizeDirty
			|| (hasFlag(_eventFlags, NodeEventFlags::HandleParentContentSize)
					&& hasFlag(parentFlags, NodeVisitFlags::ContentSizeDirty))) {
		handleContentSizeDirty();
		_contentSizeDirty = false;
		flags |= NodeVisitFlags::ContentSizeDirty;
	}

	if (_componentsDirty
			|| (hasFlag(_eventFlags, NodeEventFlags::HandleComponents)
					&& hasFlag(parentFlags, NodeVisitFlags::ComponentsDirty))) {
		handleComponentsDirty();
		_componentsDirty = false;
		flags |= NodeVisitFlags::ComponentsDirty;
	}

	return flags;
}

void Node::visitSelf(FrameInfo &info, NodeVisitFlags flags, bool visibleByCamera) {
	for (auto &it : _systems) {
		if (hasFlag(it->getSystemFlags(), SystemFlags::HandleVisitSelf)) {
			it->handleVisitSelf(info, this, flags);
		}
	}

	// self draw
	if (visibleByCamera) {
		this->draw(info, flags);
	}
}

bool Node::wrapVisit(FrameInfo &info, NodeVisitFlags parentFlags, const VisitInfo &visitInfo,
		bool useContext) {
	if (!_visible) {
		return false;
	}

	bool hasFrameContext = false;
	if (useContext && _frameContext) {
		if (_parent && _parent->getFrameContext() != _frameContext) {
			info.pushContext(_frameContext);
			hasFrameContext = true;
		}
	}

	NodeVisitFlags flags = processParentFlags(info, parentFlags);

	if (!_running || !_visible) {
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
		info.depthStack.push_back(std::max(info.depthStack.back(), _depthIndex));
	}

	memory::vector< memory::vector<Rc<System>> * > systems;

	for (auto &it : _systems) {
		if (it->isEnabled() && hasFlag(it->getSystemFlags(), SystemFlags::AddToFrameStack)
				&& it->getFrameTag() != InvalidTag) {
			systems.emplace_back(info.pushSystem(it));
		}
		if (hasFlag(it->getSystemFlags(), SystemFlags::HandleVisitControl)) {
			visitInfo.visitableSystems.emplace_back(it);
		}
	}

	size_t i = 0;

	visitInfo.flags = flags;
	visitInfo.frameInfo = &info;
	visitInfo.visibleByCamera = visibleByCamera;

	if (!_reorderChildDirty && hasFlag(_eventFlags, NodeEventFlags::HandleParentReorderChild)
			&& hasFlag(parentFlags, NodeVisitFlags::ReorderChildDirty)) {
		handleReorderChildDirty();
	} else if (sortAllChildren()) {
		visitInfo.flags |= NodeVisitFlags::ReorderChildDirty;
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
			val = std::max(it->getMaxDepthIndex(), val);
		}
	}
	return val;
}

} // namespace stappler::xenolith
