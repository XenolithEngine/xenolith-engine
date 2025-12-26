/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#include "XLCoreObject.h"
#include "XLCoreInfo.h"
#include "XLCoreDevice.h"
#include "XLCoreDeviceQueue.h"
#include "SPIRV-Reflect/spirv_reflect.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

Object::~Object() { invalidate(); }

bool Object::init(Device &dev, ClearCallback cb, ObjectType type, ObjectHandle handle,
		void *extra) {
	_object.device = &dev;
	_object.callback = cb;
	_object.type = type;
	_object.handle = handle;
	_object.ptr = extra;
	if (_object.handle.get()) {
		_object.device->addObject(this);
	}
	return true;
}

void Object::invalidate() {
	if (_object.callback) {
		if (_object.handle.get()) {
			_object.device->removeObject(this);
		}
		if (_object.callback) {
			_object.callback(_object.device, _object.type, _object.handle, _object.ptr);
		}
		_object.callback = nullptr;
		_object.device = nullptr;
		_object.handle = ObjectHandle::zero();
	}
}

static std::atomic<uint64_t> s_RenderPassImplCurrentIndex = 1;

bool RenderPass::init(Device &dev, ClearCallback cb, ObjectType type, ObjectHandle ptr, void *p) {
	if (Object::init(dev, cb, type, ptr, p)) {
		_index = s_RenderPassImplCurrentIndex.fetch_add(1);
		return true;
	}
	return false;
}

uint64_t Framebuffer::getViewHash(SpanView<Rc<ImageView>> views) {
	Vector<uint64_t> ids;
	ids.reserve(views.size());
	for (auto &it : views) { ids.emplace_back(it->getIndex()); }
	return getViewHash(ids);
}

uint64_t Framebuffer::getViewHash(SpanView<uint64_t> ids) {
	return hash::hash64((const char *)ids.data(), ids.size() * sizeof(uint64_t));
}

uint64_t Framebuffer::getHash() const {
	return hash::hash64((const char *)_viewIds.data(), _viewIds.size() * sizeof(uint64_t));
}

inline uint32_t hash(uint32_t k, uint32_t capacity) {
	k ^= k >> 16;
	k *= 0x85eb'ca6b;
	k ^= k >> 13;
	k *= 0xc2b2'ae35;
	k ^= k >> 16;
	return k & (capacity - 1);
}

bool DataAtlas::init(Type t, uint32_t count, uint32_t objectSize, Extent2 imageSize) {
	_type = t;
	_objectSize = objectSize;
	_imageExtent = imageSize;
	_data.reserve(count * objectSize);
	return true;
}

void DataAtlas::compile() {
	auto bufferObjectSize = sizeof(uint32_t) * 2 + _objectSize;
	auto bufferObjectCount = math::npot(uint32_t(_intNames.size()));

	Bytes dataStorage;
	dataStorage.resize(bufferObjectCount * bufferObjectSize, uint8_t(0xFFU));

	auto bufferData = dataStorage.data();

	for (auto &it : _intNames) {
		auto objectKey = uint32_t(it.first);
		uint32_t slot = hash(objectKey, bufferObjectCount);

		while (true) {
			auto targetObject = bufferData + slot * bufferObjectSize;
			uint32_t prev = *reinterpret_cast<uint32_t *>(targetObject);
			if (prev == 0xffff'ffffU || prev == objectKey) {
				memcpy(targetObject, &objectKey, sizeof(uint32_t));
				memcpy(targetObject + sizeof(uint32_t), &it.second, sizeof(uint32_t));
				memcpy(targetObject + sizeof(uint32_t) * 2,
						_data.data() + _objectSize * uint32_t(it.second), _objectSize);
				break;
			}
			slot = (slot + 1) & (bufferObjectCount - 1);
		}
	}

	_bufferData = sp::move(dataStorage);
}

uint32_t DataAtlas::getHash(uint32_t id) const {
	if (!_bufferData.empty()) {
		auto bufferObjectSize = sizeof(uint32_t) * 2 + _objectSize;
		auto size = uint32_t(_bufferData.size() / bufferObjectSize);
		return hash(id, size);
	}
	return maxOf<uint32_t>();
}

const uint8_t *DataAtlas::getObjectByName(uint32_t id) const {
	if (!_bufferData.empty()) {
		auto bufferObjectSize = sizeof(uint32_t) * 2 + _objectSize;
		auto size = uint32_t(_bufferData.size() / bufferObjectSize);
		uint32_t slot = hash(id, size);

		auto bufferData = _bufferData.data();

		while (true) {
			uint32_t prev =
					*reinterpret_cast<const uint32_t *>(bufferData + slot * bufferObjectSize);
			if (prev == id) {
				return bufferData + slot * bufferObjectSize + sizeof(uint32_t) * 2;
			} else if (prev == 0xffff'ffffU) {
				break;
			}
			slot = (slot + 1) & (size - 1);
		}
	}

	auto it = _intNames.find(id);
	if (it != _intNames.end()) {
		if (it->second < _data.size() / _objectSize) {
			return _data.data() + _objectSize * it->second;
		}
	}
	return nullptr;
}

const uint8_t *DataAtlas::getObjectByName(StringView str) const {
	auto it = _stringNames.find(str.str<Interface>());
	if (it != _stringNames.end()) {
		if (it->second < _data.size() / _objectSize) {
			return _data.data() + _objectSize * it->second;
		}
	}
	return nullptr;
}

const uint8_t *DataAtlas::getObjectByOrder(uint32_t order) const {
	if (order < _data.size() / _objectSize) {
		return _data.data() + _objectSize * order;
	}
	return nullptr;
}

void DataAtlas::addObject(uint32_t id, void *data) {
	auto off = _data.size();
	_data.resize(off + _objectSize);
	memcpy(_data.data() + off, data, _objectSize);

	_intNames.emplace(id, off / _objectSize);
}

void DataAtlas::addObject(StringView name, void *data) {
	auto off = _data.size();
	_data.resize(off + _objectSize);
	memcpy(_data.data() + off, data, _objectSize);

	_stringNames.emplace(name.str<Interface>(), off / _objectSize);
}

void DataAtlas::setBuffer(Rc<BufferObject> &&index) { _buffer = move(index); }

static std::atomic<uint64_t> s_ImageViewCurrentIndex = 1;

ImageObject::~ImageObject() { }

bool ImageObject::init(Device &dev, ClearCallback cb, ObjectType type, ObjectHandle ptr, void *p) {
	if (Object::init(dev, cb, type, ptr, p)) {
		_index = s_ImageViewCurrentIndex.fetch_add(1);
		return true;
	}
	return false;
}
bool ImageObject::init(Device &dev, ClearCallback cb, ObjectType type, ObjectHandle ptr, void *p,
		uint64_t idx) {
	if (Object::init(dev, cb, type, ptr, p)) {
		_index = idx;
		return true;
	}
	return false;
}

ImageViewInfo ImageObject::getViewInfo(const ImageViewInfo &info) const {
	return _info.getViewInfo(info);
}

ImageAspects ImageObject::getAspects() const {
	switch (core::getImagePixelFormat(_info.format)) {
	case core::PixelFormat::D: return ImageAspects::Depth; break;
	case core::PixelFormat::DS: return ImageAspects::Depth | ImageAspects::Stencil; break;
	case core::PixelFormat::S: return ImageAspects::Stencil; break;
	default: return ImageAspects::Color; break;
	}
	return ImageAspects::None;
}

ImageView::~ImageView() {
	if (_releaseCallback) {
		_releaseCallback();
		_releaseCallback = nullptr;
	}
}

bool ImageView::init(Device &dev, ClearCallback cb, ObjectType type, ObjectHandle ptr, void *p) {
	if (Object::init(dev, cb, type, ptr, p)) {
		_index = s_ImageViewCurrentIndex.fetch_add(1);
		return true;
	}
	return false;
}

void ImageView::setReleaseCallback(Function<void()> &&cb) { _releaseCallback = sp::move(cb); }

void ImageView::runReleaseCallback() {
	if (_releaseCallback) {
		auto cb = sp::move(_releaseCallback);
		_releaseCallback = nullptr;
		cb();
	}
}

Extent3 ImageView::getExtent() const { return _image->getInfo().extent; }

uint32_t ImageView::getLayerCount() const { return _info.layerCount.get(); }

Extent3 ImageView::getFramebufferExtent() const {
	return Extent3(_image->getInfo().extent.width, _image->getInfo().extent.height,
			getLayerCount());
}

void CommandBuffer::bindImage(ImageObject *image) {
	if (image) {
		_images.emplace(image);
	}
}

void CommandBuffer::bindBuffer(BufferObject *buffer) {
	if (buffer) {
		_buffers.emplace(buffer);
	}
}

void CommandBuffer::bindFramebuffer(Framebuffer *fb) {
	if (fb) {
		_framebuffers.emplace(fb);
	}
}

String Shader::inspectShader(SpanView<uint32_t> data) {
	SpvReflectShaderModule shader;

	spvReflectCreateShaderModule(data.size() * sizeof(uint32_t), data.data(), &shader);

	ProgramStage stage = ProgramStage::None;
	switch (shader.spirv_execution_model) {
	case SpvExecutionModelVertex: stage = ProgramStage::Vertex; break;
	case SpvExecutionModelTessellationControl: stage = ProgramStage::TesselationControl; break;
	case SpvExecutionModelTessellationEvaluation:
		stage = ProgramStage::TesselationEvaluation;
		break;
	case SpvExecutionModelGeometry: stage = ProgramStage::Geometry; break;
	case SpvExecutionModelFragment: stage = ProgramStage::Fragment; break;
	case SpvExecutionModelGLCompute: stage = ProgramStage::Compute; break;
	case SpvExecutionModelKernel: stage = ProgramStage::Compute; break;
	case SpvExecutionModelTaskNV: stage = ProgramStage::Task; break;
	case SpvExecutionModelMeshNV: stage = ProgramStage::Mesh; break;
	case SpvExecutionModelRayGenerationKHR: stage = ProgramStage::RayGen; break;
	case SpvExecutionModelIntersectionKHR: stage = ProgramStage::Intersection; break;
	case SpvExecutionModelAnyHitKHR: stage = ProgramStage::AnyHit; break;
	case SpvExecutionModelClosestHitKHR: stage = ProgramStage::ClosestHit; break;
	case SpvExecutionModelMissKHR: stage = ProgramStage::MissHit; break;
	case SpvExecutionModelCallableKHR: stage = ProgramStage::Callable; break;
	default: break;
	}

	StringStream d;
	auto out = makeCallback(d);

	out << "[" << stage << "]\n";

	for (auto &it : makeSpanView(shader.descriptor_bindings, shader.descriptor_binding_count)) {
		out << "\tBinging: [" << it.set << ":" << it.binding << "] "
			<< getDescriptorTypeName(DescriptorType(it.descriptor_type)) << "\n";
	}

	for (auto &it : makeSpanView(shader.push_constant_blocks, shader.push_constant_block_count)) {
		out << "\tPushConstant: [" << it.absolute_offset << " - " << it.padded_size << "]\n";
	}

	spvReflectDestroyShaderModule(&shader);

	return d.str();
}

String Shader::inspect(SpanView<uint32_t> data) { return inspectShader(data); }

void Semaphore::setSignaled(bool value) { _signaled = value; }

void Semaphore::setWaited(bool value) { _waited = value; }

void Semaphore::setInUse(bool value, uint64_t timeline) {
	if (timeline == _timeline) {
		_inUse = value;
	}
}

bool Semaphore::reset() {
	if (_signaled == _waited) {
		_signaled = false;
		_waited = false;
		_inUse = false;
		++_timeline;
		return true;
	}
	return false;
}

Fence::~Fence() { doRelease(nullptr, false); }

void Fence::clear() {
	if (_releaseFn) {
		_releaseFn = nullptr;
	}
	if (_scheduleFn) {
		_scheduleFn = nullptr;
	}
}

void Fence::setFrame(uint64_t f) { _frame = f; }

void Fence::setFrame(Function<bool()> &&schedule, Function<void()> &&release, uint64_t f) {
	_frame = f;
	_scheduleFn = sp::move(schedule);
	_releaseFn = sp::move(release);
}

void Fence::setScheduleCallback(Function<bool()> &&schedule) { _scheduleFn = sp::move(schedule); }

void Fence::setReleaseCallback(Function<bool()> &&release) { _releaseFn = sp::move(release); }

void Fence::bindQueries(NotNull<QueryPool> q) { _queries.emplace_back(q); }

void Fence::setArmed(DeviceQueue &q) {
	std::unique_lock<Mutex> lock(_mutex);
	_state = Armed;
	_queue = &q;
	_queue->retainFence(*this);
	_armedTime = sp::platform::clock(ClockType::Monotonic);
}

void Fence::setArmed() {
	std::unique_lock<Mutex> lock(_mutex);
	_state = Armed;
	_armedTime = sp::platform::clock(ClockType::Monotonic);
}

void Fence::setTag(StringView tag) { _tag = tag; }

void Fence::addQueryCallback(Function<void(bool, SpanView<Rc<QueryPool>>)> &&cb, Ref *ref,
		StringView tag) {
	addRelease([this, cb = sp::move(cb)](bool success) { cb(success, _queries); }, ref, tag);
}

void Fence::addRelease(Function<void(bool)> &&cb, Ref *ref, StringView tag) {
	std::unique_lock<Mutex> lock(_mutex);
	_release.emplace_back(ReleaseHandle({sp::move(cb), ref, tag}));
}

bool Fence::schedule(Loop &loop) {
	std::unique_lock<Mutex> lock(_mutex);
	if (_state != Armed) {
		lock.unlock();
		if (_releaseFn) {
			loop.performOnThread([this, loop = &loop] {
				doRelease(loop, false);

				if (_releaseFn) {
					auto releaseFn = sp::move(_releaseFn);
					_releaseFn = nullptr;
					_scheduleFn = nullptr;
					releaseFn();
				} else {
					_scheduleFn = nullptr;
				}
			}, this, true);
		} else {
			doRelease(&loop, false);
			_scheduleFn = nullptr;
		}
		return false;
	} else {
		lock.unlock();

		if (check(loop, true)) {
			// fence was released
			_scheduleFn = nullptr;
			return false;
		}
	}

	if (!_scheduleFn) {
		return false;
	}

	auto scheduleFn = sp::move(_scheduleFn);
	_scheduleFn = nullptr;

	if (lock.owns_lock()) {
		lock.unlock();
	}

	return scheduleFn();
}

bool Fence::check(Loop &loop, bool lockfree) {
	std::unique_lock<Mutex> lock(_mutex);
	if (_state != Armed) {
		return true;
	}

	Status status = doCheckFence(lockfree);

	switch (status) {
	case Status::Ok:
		_state = Signaled;
		// XL_VKAPI_LOG("Fence [", _frame, "] ", _tag, ": signaled: ", sp::platform::clock(ClockType::Monotonic) - _armedTime);
		lock.unlock();
		setSignaled(loop);
		return true;
		break;
	case Status::Suspended:
	case Status::Declined:
		_state = Armed;
		if (sp::platform::clock(ClockType::Monotonic) - _armedTime > 1'000'000) {
			lock.unlock();
			/*if (_queue) {
				XL_VKAPI_LOG("Fence [", _queue->getFrameIndex(), "] Fence is possibly broken: ", _tag);
			} else {
				XL_VKAPI_LOG("Fence [", _frame, "] Fence is possibly broken: ", _tag);
			}*/
			return check(loop, false);
		}
		return false;
	default: break;
	}
	return false;
}

void Fence::autorelease(Rc<Ref> &&ref) { _autorelease.emplace_back(move(ref)); }

void Fence::setSignaled(Loop &loop) {
	_state = Signaled;
	if (loop.isOnThisThread()) {
		doRelease(&loop, true);
		scheduleReset(loop);
	} else {
		scheduleReleaseReset(loop, true);
	}
}

void Fence::scheduleReset(Loop &loop) {
	if (!loop.isRunning()) {
		_releaseFn = nullptr;
		_scheduleFn = nullptr;
		_autorelease.clear();
		_queries.clear();
	}
	if (_releaseFn) {
		loop.performInQueue(Rc<thread::Task>::create([this](const thread::Task &) {
			doResetFence();
			return true;
		}, [this](const thread::Task &, bool success) {
			auto releaseFn = sp::move(_releaseFn);
			_releaseFn = nullptr;
			releaseFn();
		}, this));
	} else {
		doResetFence();
	}
}

void Fence::scheduleReleaseReset(Loop &loop, bool s) {
	if (_releaseFn) {
		loop.performInQueue(Rc<thread::Task>::create([this](const thread::Task &) {
			doResetFence();
			return true;
		}, [this, s, loop = &loop](const thread::Task &, bool success) {
			doRelease(loop, s);

			auto releaseFn = sp::move(_releaseFn);
			_releaseFn = nullptr;
			releaseFn();
		}, this));
	} else {
		doResetFence();
		doRelease(&loop, s);
	}
}

void Fence::doRelease(Loop *loop, bool success) {
	if (_queue) {
		_queue->releaseFence(*this);
		_queue = nullptr;
	}

	auto autorelease = sp::move(_autorelease);
	_autorelease.clear();

	if (!_release.empty()) {
		XL_PROFILE_BEGIN(total, "vk::Fence::reset", "total", 250);
		for (auto &it : _release) {
			if (it.callback) {
				XL_PROFILE_BEGIN(fence, "vk::Fence::reset", it.tag, 250);
				it.callback(success);
				XL_PROFILE_END(fence);
			}
		}
		XL_PROFILE_END(total);
		_release.clear();
	}

	if (loop) {
		for (auto &it : _queries) { _object.device->releaseQueryPool(*loop, move(it)); }
	}
	_queries.clear();

	_tag = StringView();
	autorelease.clear();
}

} // namespace stappler::xenolith::core
