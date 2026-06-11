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

void ObjectRegistry::shareWindow(core::RenderServerChannel *obj, SpanView<core::Queue *> q) {
	auto exportQueues = [&](SharedWindowInfo &info) {
		Vector<uint64_t> queues;
		for (auto &it : q) {
			if (auto id = getId(it)) {
				queues.emplace_back(id);
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

uint64_t ObjectRegistry::getId(core::RenderServerChannel *obj) {
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

uint64_t ObjectRegistry::getId(core::Queue *obj) {
	if (!obj) {
		return 0;
	}
	auto it = _queueByPtr.find(obj);
	if (it != _queueByPtr.end()) {
		return it->second;
	}
	auto id = allocateId();
	_queueByPtr.emplace(obj, id);
	_queueById.emplace(id, Rc<core::Queue>(obj));
	return id;
}

uint64_t ObjectRegistry::getId(core::Object *obj) {
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
	return id;
}

void ObjectRegistry::drop(core::RenderServerChannel *window) {
	auto it = _windowByPtr.find(window);
	if (it != _windowByPtr.end()) {
		_windowById.erase(it->second);
		_windowByPtr.erase(it);
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

core::Queue *ObjectRegistry::resolveQueue(uint64_t id) const {
	auto it = _queueById.find(id);
	return (it != _queueById.end()) ? it->second : nullptr;
}

core::RenderServerChannel *ObjectRegistry::resolveWindow(uint64_t id) const {
	auto it = _windowById.find(id);
	return (it != _windowById.end()) ? it->second.window : nullptr;
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
