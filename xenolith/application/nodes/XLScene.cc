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

#include "XLScene.h"

#include "XLFrameContext.h"
#include "XLInputDispatcher.h"
#include "XLDirector.h"
#include "XLSceneContent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

static void Scene_findUnresolvedInputs(SpanView<core::AttachmentData *> queue,
		const mem_pool::Set<const core::AttachmentData *> &resolved) {
	for (auto &it : queue) {
		auto iit = resolved.find(it);
		if (iit == resolved.end()) {
			log::source().warn("Scene", "No input defined for attachment: ", it->key);
		}
	}
}

Scene::~Scene() {
	slog().debug("Scene", "~Scene");
	_queue = nullptr;
}

bool Scene::init(Queue::Builder &&builder, const core::FrameConstraints &constraints) {
	if (!Node::init()) {
		return false;
	}

	setLocalZOrder(ZOrderTransparent);

	_queue = makeQueue(move(builder));

	setFrameConstraints(_constraints);

	return true;
}

bool Scene::init(Rc<Queue> &&queue, const core::FrameConstraints &constraints) {
	if (!queue || !Node::init()) {
		return false;
	}

	setLocalZOrder(ZOrderTransparent);

	_queue = sp::move(queue);
	_ownsQueue = false;

	setFrameConstraints(_constraints);

	return true;
}

void Scene::renderRequest(const Rc<core::FrameRequestProxy> &req, sprt::PoolRef *pool) {
	if (!_director) {
		return;
	}

	Size2 scaledExtent(_constraints.extent.width / _constraints.density,
			_constraints.extent.height / _constraints.density);

	FrameInfo info;
	info.request = req;
	info.pool = pool;

	render(info);

	if (info.resolvedInputs.size() != _queue->getInputAttachments().size()) {
		Scene_findUnresolvedInputs(_queue->getInputAttachments(), info.resolvedInputs);
	}
}

void Scene::render(FrameInfo &info) {
	info.director = _director;
	info.scene = this;
	info.zPath.reserve(8);

	info.viewProjectionStack.reserve(2);
	info.viewProjectionStack.push_back(_director->getGeneralProjection());

	info.modelTransformStack.reserve(8);
	info.modelTransformStack.push_back(Mat4::IDENTITY);

	info.depthStack.reserve(4);
	info.depthStack.push_back(0.0f);

	auto eventDispatcher = _director->getInputDispatcher();

	info.input = eventDispatcher->acquireNewStorage();

	// Published for the duration of the visit only: a node attached while a frame is in flight
	// reaches it through getFrameInfo() and catches up on the phases the visit has already passed.
	_frameInfo = &info;

	visitDraw(info, NodeVisitFlags::None);

	_frameInfo = nullptr;

	eventDispatcher->commitStorage(_director->getRenderServer(), move(info.input));
}

void Scene::handleEnter(Scene *scene) { Node::handleEnter(scene); }

void Scene::handleExit() { Node::handleExit(); }

void Scene::handleContentSizeDirty() {
	Node::handleContentSizeDirty();

	setAnchorPoint(Anchor::Middle);
	setPosition(Vec2((_contentSize * _constraints.density) / 2.0f));

	updateContentNode(_content);

#if DEBUG
	log::source().info("Scene", "ContentSize: ", _contentSize, " density: ", _constraints.density);
#endif
}

void Scene::setContent(SceneContent *content) {
	if (_content) {
		_content->removeFromParent(true);
	}
	_content = nullptr;
	if (content) {
		_content = addChild(content);
		updateContentNode(_content);
	}
}

void Scene::handlePresented(Director *dir) {
	_director = dir;
	if (getContentSize() == Size2::ZERO) {
		setContentSize(Size2(_constraints.getScreenSize()) / _constraints.density);
	}

	// Only for a queue this scene built. A shared queue's internal resource is registered by its
	// owner (QueueCache) for as long as the cache entry lives; registering it again here would let
	// whichever scene finishes first erase it for everyone (ResourceCache is name-keyed, no
	// refcount).
	if (_ownsQueue) {
		if (auto res = _queue->getInternalResource()) {
			auto cache = dir->getResourceCache();
			if (cache) {
				cache->addResource(res);
			} else {
				log::source().error("Director", "ResourceCache is not loaded");
			}
		}
	}

	handleEnter(this);
}

void Scene::handleFinished(Director *dir) {
	handleExit();

	if (_director == dir) {
		if (_ownsQueue) {
			if (auto res = _queue->getInternalResource()) {
				auto cache = dir->getResourceCache();
				if (cache) {
					cache->removeResource(res->getName());
				}
			}
		}

		_director = nullptr;
	}
}

void Scene::setFrameConstraints(const core::FrameConstraints &constraints) {
	if (_constraints != constraints) {
		auto size = Size2(constraints.getScreenSize());

		_constraints = constraints;

		setContentSize(size / _constraints.density);
		setScale(_constraints.density);
		_contentSizeDirty = true;

		setPosition(Vec2((_contentSize * _constraints.density) / 2.0f));

		updateContentNode(_content);
	}
}

void Scene::handleWindowGeometryChanged(const sprt::window::WindowGeometry &) {
	// Nothing for a scene graph to do by default - the size half of this already arrived as
	// FrameConstraints, and the position half means nothing to the layout. Present so a subclass
	// has somewhere to put what it does care about.
}

Size2 Scene::getContentSize() const { return _content ? _content->getContentSize() : _contentSize; }

void Scene::setClipContent(bool value) {
	if (_content) {
		if (isClipContent() != value) {
			if (value) {
				_content->enableScissor();
			} else {
				_content->disableScissor();
			}
		}
	}
}

bool Scene::isClipContent() const { return _content ? _content->isScissorEnabled() : false; }

auto Scene::makeQueue(Queue::Builder &&builder) -> Rc<Queue> {
	// No begin/end/attach/detach callbacks are installed here any more. They existed only to pin
	// this Scene's refcount for the duration of a frame, and because they captured `this` they made
	// the queue permanently scene-specific - two scenes could never share one. The pin now rides on
	// the FrameRequest itself (Director::acquireFrame -> FrameRequestProxy::setSceneRef), which has
	// exactly the same lifetime and leaves the queue free of any scene identity.
	//
	// Queue::Builder::set*Callback remain in the API for queues that are not scene-backed.
	return Rc<Queue>::create(move(builder));
}

void Scene::updateContentNode(SceneContent *content) {
	if (content) {
		content->setPosition(Vec2(0.0f, 0.0f));
		content->setContentSize(Size2(_contentSize.width, _contentSize.height));
		content->setAnchorPoint(Anchor::BottomLeft);
	}
}

} // namespace stappler::xenolith
