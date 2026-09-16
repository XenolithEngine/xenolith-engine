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

#include "XLRemoteObject.h"
#include "XLRemoteSerialize.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

static core::ObjectHandle idToHandle(uint64_t id) {
#if (XL_USE_64_BIT_PTR_DEFINES == 1)
	return core::ObjectHandle(reinterpret_cast<void *>(uintptr_t(id)));
#else
	return core::ObjectHandle(id);
#endif
}

static uint64_t handleToId(core::ObjectHandle h) {
#if (XL_USE_64_BIT_PTR_DEFINES == 1)
	return uint64_t(reinterpret_cast<uintptr_t>(h.get()));
#else
	return uint64_t(h.get());
#endif
}

// Set up the ObjectData of a thin handle: server id in `handle`, no device/callback so that
// Object::invalidate() (run from ~Object) is a no-op.
static void initRemote(core::ObjectData &obj, core::ObjectType type, uint64_t id) {
	obj.type = type;
	obj.device = nullptr;
	obj.callback = nullptr;
	obj.handle = idToHandle(id);
	obj.ptr = nullptr;
}

uint64_t getRemoteObjectId(const core::Object &obj) {
	return handleToId(obj.getObjectData().handle);
}

// vtable emission for the out-of-line dtors: safe without exceptions enabled.
__SPRT_PUSH_ALLOW_CXXABI_ALLOC

Image::~Image() = default;
Buffer::~Buffer() = default;
ImageView::~ImageView() = default;
Sampler::~Sampler() = default;
Shader::~Shader() = default;
GraphicPipeline::~GraphicPipeline() = default;
ComputePipeline::~ComputePipeline() = default;
RenderPass::~RenderPass() = default;
TextureSetLayout::~TextureSetLayout() = default;

__SPRT_POP_ALLOW_CXXABI_ALLOC

bool Image::init(uint64_t id, const core::ImageInfoData &info) {
	initRemote(_object, core::ObjectType::Image, id);
	_info = info;
	_index = id;
	return true;
}

bool Buffer::init(uint64_t id, const core::BufferInfo &info) {
	initRemote(_object, core::ObjectType::Buffer, id);
	_info = info;
	return true;
}

bool ImageView::init(uint64_t id, const Rc<core::ImageObject> &image,
		const core::ImageViewInfo &info) {
	initRemote(_object, core::ObjectType::ImageView, id);
	_info = info;
	_image = image;
	_index = id;
	return true;
}

bool Sampler::init(uint64_t id, const core::SamplerInfo &info) {
	initRemote(_object, core::ObjectType::Sampler, id);
	_info = info;
	return true;
}

bool Shader::init(uint64_t id, core::ProgramStage stage) {
	initRemote(_object, core::ObjectType::ShaderModule, id);
	_stage = stage;
	return true;
}

bool GraphicPipeline::init(uint64_t id) {
	initRemote(_object, core::ObjectType::Pipeline, id);
	return true;
}

bool ComputePipeline::init(uint64_t id, uint32_t localX, uint32_t localY, uint32_t localZ) {
	initRemote(_object, core::ObjectType::Pipeline, id);
	_localX = localX;
	_localY = localY;
	_localZ = localZ;
	return true;
}

bool RenderPass::init(uint64_t id, core::PassType type, uint64_t index) {
	initRemote(_object, core::ObjectType::RenderPass, id);
	_type = type;
	_index = index;
	return true;
}

bool TextureSetLayout::init(uint64_t id, uint32_t imageCount, uint32_t samplersCount) {
	initRemote(_object, core::ObjectType::DescriptorSetLayout, id);
	_imageCount = imageCount;
	_samplersCount = samplersCount;
	return true;
}

// --- ObjectRegistry (server) ---

ObjectRegistry::~ObjectRegistry() { }

void ObjectRegistry::shareWindow(core::RenderServerChannel *obj, SpanView<core::Queue *> q,
		const HashMap<const core::MaterialAttachment *, Rc<core::MaterialSet>> &materials) {
	auto exportQueues = [&](SharedWindowInfo &info) {
		Vector<uint64_t> queues;
		for (auto &it : q) {
			if (auto id = share(it)) {
				queues.emplace_back(id);

				for (auto &mIt : materials) {
					if (mIt.first->getData()->queue->queue == it) {
						attachMaterials(mIt.second);
					}
				}
			}
		}

		info.queues = queues;
	};

	if (!obj) {
		return;
	}
	auto it = _windowByPtr.find(obj);
	if (it != _windowByPtr.end()) {
		auto vIt = _windowById.find(it->second);
		if (vIt == _windowById.end()) {
			return;
		}
		exportQueues(vIt->second);
		return;
	}
	auto id = allocateId();
	_windowByPtr.emplace(obj, id);

	auto vIt = _windowById.emplace(id, SharedWindowInfo{obj}).first;
	exportQueues(vIt->second);
}

uint64_t ObjectRegistry::share(core::RenderServerChannel *obj) {
	if (!obj) {
		return 0;
	}
	auto it = _windowByPtr.find(obj);
	if (it != _windowByPtr.end()) {
		return it->second;
	}
	auto id = allocateId();
	_windowByPtr.emplace(obj, id);
	_windowById.emplace(id, obj);
	return id;
}

uint64_t ObjectRegistry::share(core::Queue *obj) {
	if (!obj) {
		return 0;
	}
	auto it = _queueByPtr.find(obj);
	if (it != _queueByPtr.end()) {
		return it->second;
	}
	auto id = allocateId();
	_queueByPtr.emplace(obj, id);
	_queueById.emplace(id, SharedQueueInfo{Rc<core::Queue>(obj)});
	return id;
}

uint64_t ObjectRegistry::share(core::Object *obj) {
	if (!obj) {
		return 0;
	}
	auto it = _objectByPtr.find(obj);
	if (it != _objectByPtr.end()) {
		return it->second;
	}
	auto id = allocateId();
	_objectByPtr.emplace(obj, id);
	_objectById.emplace(id, Rc<core::Object>(obj));
	if (_queueScope) {
		auto qIt = _queueById.find(_queueScope);
		if (qIt != _queueById.end()) {
			qIt->second.objects.emplace_back(id);
		}
	}
	return id;
}

ObjectRegistry::QueueScope::QueueScope(ObjectRegistry &r, uint64_t queueId) : registry(r) {
	previous = registry._queueScope;
	registry._queueScope = queueId;
}

ObjectRegistry::QueueScope::~QueueScope() { registry._queueScope = previous; }

void ObjectRegistry::pinObject(core::Object *obj, uint64_t id) {
	if (!obj || !id) {
		return;
	}
	// Detach whatever object currently holds `id`.
	auto idIt = _objectById.find(id);
	if (idIt != _objectById.end()) {
		if (idIt->second.get() == obj) {
			return; // already pinned to this object
		}
		_objectByPtr.erase(idIt->second.get());
	}
	// Detach any other id this object previously held.
	auto ptrIt = _objectByPtr.find(obj);
	if (ptrIt != _objectByPtr.end()) {
		_objectById.erase(ptrIt->second);
		_objectByPtr.erase(ptrIt);
	}
	_objectByPtr.emplace(obj, id);
	_objectById.erase(id);
	_objectById.emplace(id, Rc<core::Object>(obj));
}

uint64_t ObjectRegistry::attachMaterials(NotNull<core::MaterialSet> set) {
	auto v = get(set->getOwner()->getData()->queue->queue);
	if (v == 0) {
		return 0;
	}

	auto vIt = _queueById.find(v);
	if (vIt == _queueById.end()) {
		return 0;
	}

	auto aIt = vIt->second.materials.find(set->getOwner());
	if (aIt != vIt->second.materials.end()) {
		aIt->second = set;
	} else {
		vIt->second.materials.emplace(set->getOwner(), set.get());
	}
	return v;
}

uint64_t ObjectRegistry::get(core::RenderServerChannel *obj) const {
	if (!obj) {
		return 0;
	}
	auto it = _windowByPtr.find(obj);
	if (it != _windowByPtr.end()) {
		return it->second;
	}
	return 0;
}

uint64_t ObjectRegistry::get(core::Queue *obj) const {
	if (!obj) {
		return 0;
	}
	auto it = _queueByPtr.find(obj);
	if (it != _queueByPtr.end()) {
		return it->second;
	}
	return 0;
}

uint64_t ObjectRegistry::get(core::Object *obj) const {
	if (!obj) {
		return 0;
	}
	auto it = _objectByPtr.find(obj);
	if (it != _objectByPtr.end()) {
		return it->second;
	}
	return 0;
}

void ObjectRegistry::drop(core::RenderServerChannel *window) {
	auto it = _windowByPtr.find(window);
	if (it == _windowByPtr.end()) {
		return;
	}

	// Take the queue ids before the window entry goes: they are what has to be reconsidered.
	auto queues = sp::move(_windowById.find(it->second)->second.queues);
	_windowById.erase(it->second);
	_windowByPtr.erase(it);

	for (auto queueId : queues) {
		bool used = false;
		for (auto &wIt : _windowById) {
			for (auto q : wIt.second.queues) { used = used || q == queueId; }
		}
		if (used) {
			continue;
		}
		auto qIt = _queueById.find(queueId);
		if (qIt == _queueById.end()) {
			continue;
		}

		// The objects the queue's encoding minted go with it, unless another queue shared the same
		// object (two windows of one application can reference one image).
		for (auto objectId : qIt->second.objects) {
			bool shared = false;
			for (auto &other : _queueById) {
				if (other.first == queueId) {
					continue;
				}
				for (auto o : other.second.objects) { shared = shared || o == objectId; }
			}
			if (shared) {
				continue;
			}
			auto oIt = _objectById.find(objectId);
			if (oIt != _objectById.end()) {
				_objectByPtr.erase(oIt->second.get());
				_objectById.erase(oIt);
			}
		}

		_queueByPtr.erase(qIt->second.queue.get());
		_queueById.erase(qIt);
	}
}

void ObjectRegistry::drop(core::Queue *queue) {
	auto it = _queueByPtr.find(queue);
	if (it != _queueByPtr.end()) {
		_queueById.erase(it->second);
		_queueByPtr.erase(it);
	}
}

void ObjectRegistry::drop(core::Object *obj) {
	auto it = _objectByPtr.find(obj);
	if (it != _objectByPtr.end()) {
		_objectById.erase(it->second);
		_objectByPtr.erase(it);
	}
}

core::Object *ObjectRegistry::resolveObject(uint64_t id) const {
	auto it = _objectById.find(id);
	return (it != _objectById.end()) ? it->second : nullptr;
}

const ObjectRegistry::SharedQueueInfo *ObjectRegistry::resolveQueue(uint64_t id) const {
	auto it = _queueById.find(id);
	return (it != _queueById.end()) ? &it->second : nullptr;
}

core::RenderServerChannel *ObjectRegistry::resolveWindow(uint64_t id) const {
	auto it = _windowById.find(id);
	return (it != _windowById.end()) ? it->second.window : nullptr;
}

bool ObjectRegistry::isWindowVisible(uint64_t windowId, uint64_t session) const {
	auto it = _windowById.find(windowId);
	if (it == _windowById.end() || session == 0) {
		return false;
	}
	if (it->second.ownerSession != 0) {
		return it->second.ownerSession == session;
	}
	return it->second.assignedSession == 0 || it->second.assignedSession == session;
}

bool ObjectRegistry::isQueueVisible(uint64_t queueId, uint64_t session) const {
	for (auto &it : _windowById) {
		for (auto q : it.second.queues) {
			if (q == queueId && isWindowVisible(it.first, session)) {
				return true;
			}
		}
	}
	return false;
}

bool ObjectRegistry::claimWindow(uint64_t windowId, uint64_t session) {
	if (!isWindowVisible(windowId, session)) {
		return false;
	}
	auto &info = _windowById.find(windowId)->second;
	for (auto &it : _windowById) {
		if (it.first == windowId || it.second.ownerSession == 0
				|| it.second.ownerSession == session) {
			continue;
		}
		for (auto q : it.second.queues) {
			for (auto own : info.queues) {
				if (q == own) {
					return false;
				}
			}
		}
	}
	info.ownerSession = session;
	return true;
}

bool ObjectRegistry::assignWindow(uint64_t windowId, uint64_t session) {
	auto it = _windowById.find(windowId);
	if (it == _windowById.end()) {
		return false;
	}
	it->second.assignedSession = session;
	return true;
}

bool ObjectRegistry::setWindowCreator(uint64_t windowId, uint64_t session, uint32_t serial) {
	auto it = _windowById.find(windowId);
	if (it == _windowById.end()) {
		return false;
	}
	it->second.creatorSession = session;
	it->second.creatorSerial = serial;
	return true;
}

Vector<uint64_t> ObjectRegistry::releaseSession(uint64_t session) {
	Vector<uint64_t> ret;
	if (session == 0) {
		return ret;
	}
	for (auto &it : _windowById) {
		if (it.second.assignedSession == session) {
			it.second.assignedSession = 0;
		}
		if (it.second.creatorSession == session) {
			it.second.creatorSession = 0;
			it.second.creatorSerial = 0;
		}
		if (it.second.ownerSession == session) {
			it.second.ownerSession = 0;
			ret.emplace_back(it.first);
		}
	}
	return ret;
}

void ObjectRegistry::clear() {
	_objectByPtr.clear();
	_objectById.clear();
	_queueByPtr.clear();
	_queueById.clear();
	_windowByPtr.clear();
	_windowById.clear();
	_next = 1;
}

// --- ObjectFactory (client) ---

core::Object *ObjectFactory::resolveObject(uint64_t id) const {
	auto it = _objectById.find(id);
	return (it != _objectById.end()) ? it->second : nullptr;
}

core::Queue *ObjectFactory::resolveQueue(uint64_t id) const {
	auto it = _queueById.find(id);
	return (it != _queueById.end()) ? it->second.get() : nullptr;
}

core::Queue *ObjectFactory::makeQueue(uint64_t id, core::Queue &queue, BytesView data) {
	if (id == 0) {
		return nullptr;
	}
	if (auto c = resolveQueue(id)) {
		return static_cast<core::Queue *>(c);
	}
	auto q = QueueCodec::decodeQueue(queue, data, *this);
	if (q) {
		_queueById.emplace(id, &queue);
		return &queue;
	}
	return nullptr;
}

core::ImageObject *ObjectFactory::makeImage(uint64_t id, const core::ImageInfoData &info) {
	if (id == 0) {
		return nullptr;
	}
	if (auto c = resolveObject(id)) {
		return static_cast<core::ImageObject *>(c);
	}
	Rc<core::ImageObject> obj = Rc<Image>::create(id, info);
	_objectById.emplace(id, obj);
	return obj;
}

core::BufferObject *ObjectFactory::makeBuffer(uint64_t id, const core::BufferInfo &info) {
	if (id == 0) {
		return nullptr;
	}
	if (auto c = resolveObject(id)) {
		return static_cast<core::BufferObject *>(c);
	}
	Rc<core::BufferObject> obj = Rc<Buffer>::create(id, info);
	_objectById.emplace(id, obj);
	return obj;
}

core::ImageView *ObjectFactory::makeImageView(uint64_t id, const Rc<core::ImageObject> &image,
		const core::ImageViewInfo &info) {
	if (id == 0) {
		return nullptr;
	}
	if (auto c = resolveObject(id)) {
		return static_cast<core::ImageView *>(c);
	}
	Rc<core::ImageView> obj = Rc<ImageView>::create(id, image, info);
	_objectById.emplace(id, obj);
	return obj;
}

core::Sampler *ObjectFactory::makeSampler(uint64_t id, const core::SamplerInfo &info) {
	if (id == 0) {
		return nullptr;
	}
	if (auto c = resolveObject(id)) {
		return static_cast<core::Sampler *>(c);
	}
	Rc<core::Sampler> obj = Rc<Sampler>::create(id, info);
	_objectById.emplace(id, obj);
	return obj;
}

core::Shader *ObjectFactory::makeShader(uint64_t id, core::ProgramStage stage) {
	if (id == 0) {
		return nullptr;
	}
	if (auto c = resolveObject(id)) {
		return static_cast<core::Shader *>(c);
	}
	Rc<core::Shader> obj = Rc<Shader>::create(id, stage);
	_objectById.emplace(id, obj);
	return obj;
}

core::GraphicPipeline *ObjectFactory::makeGraphicPipeline(uint64_t id) {
	if (id == 0) {
		return nullptr;
	}
	if (auto c = resolveObject(id)) {
		return static_cast<core::GraphicPipeline *>(c);
	}
	Rc<core::GraphicPipeline> obj = Rc<GraphicPipeline>::create(id);
	_objectById.emplace(id, obj);
	return obj;
}

core::ComputePipeline *ObjectFactory::makeComputePipeline(uint64_t id, uint32_t x, uint32_t y,
		uint32_t z) {
	if (id == 0) {
		return nullptr;
	}
	if (auto c = resolveObject(id)) {
		return static_cast<core::ComputePipeline *>(c);
	}
	Rc<core::ComputePipeline> obj = Rc<ComputePipeline>::create(id, x, y, z);
	_objectById.emplace(id, obj);
	return obj;
}

core::RenderPass *ObjectFactory::makeRenderPass(uint64_t id, core::PassType type, uint64_t index) {
	if (id == 0) {
		return nullptr;
	}
	if (auto c = resolveObject(id)) {
		return static_cast<core::RenderPass *>(c);
	}
	Rc<core::RenderPass> obj = Rc<RenderPass>::create(id, type, index);
	_objectById.emplace(id, obj);
	return obj;
}

core::TextureSetLayout *ObjectFactory::makeTextureSetLayout(uint64_t id, uint32_t imageCount,
		uint32_t samplersCount) {
	if (id == 0) {
		return nullptr;
	}
	if (auto c = resolveObject(id)) {
		return static_cast<core::TextureSetLayout *>(c);
	}
	Rc<core::TextureSetLayout> obj = Rc<TextureSetLayout>::create(id, imageCount, samplersCount);
	_objectById.emplace(id, obj);
	return obj;
}

void ObjectFactory::clear() { _objectById.clear(); }

} // namespace stappler::xenolith::remote
