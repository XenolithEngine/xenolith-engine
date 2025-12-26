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

#ifndef XENOLITH_CORE_XLCOREOBJECT_H_
#define XENOLITH_CORE_XLCOREOBJECT_H_

#include "XLCoreInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

class TextureSet;
class ImageView;
class BufferObject;
class Loop;
class Device;
class Attachment;
class Queue;
class CommandPool;
class QueryPool;
class DeviceQueue;

struct GraphicPipelineInfo;
struct GraphicPipelineData;
struct ComputePipelineInfo;
struct ComputePipelineData;
struct ProgramData;
struct QueuePassData;
struct SubpassData;
struct PipelineDescriptor;

struct SP_PUBLIC ObjectData {
	using ClearCallback = void (*)(Device *, ObjectType, ObjectHandle, void *);

	ObjectType type;
	Device *device = nullptr;
	ClearCallback callback = nullptr;
	ObjectHandle handle;
	void *ptr = nullptr;
};

class SP_PUBLIC Object : public NamedRef {
public:
	using ObjectHandle = core::ObjectHandle;
	using ClearCallback = void (*)(Device *, ObjectType, ObjectHandle, void *);

	virtual ~Object();

	virtual bool init(Device &, ClearCallback, ObjectType, ObjectHandle ptr, void * = nullptr);
	void invalidate();

	const ObjectData &getObjectData() const { return _object; }

	virtual void setName(StringView str) { _name = str.str<Interface>(); }
	virtual StringView getName() const override { return _name; }

protected:
	ObjectData _object;
	String _name;
};


class SP_PUBLIC GraphicPipeline : public Object {
public:
	using PipelineInfo = core::GraphicPipelineInfo;
	using PipelineData = core::GraphicPipelineData;
	using SubpassData = core::SubpassData;
	using Queue = core::Queue;

	virtual ~GraphicPipeline() { }
};


class SP_PUBLIC ComputePipeline : public Object {
public:
	using PipelineInfo = core::ComputePipelineInfo;
	using PipelineData = core::ComputePipelineData;
	using SubpassData = core::SubpassData;
	using Queue = core::Queue;

	virtual ~ComputePipeline() { }

	uint32_t getLocalX() const { return _localX; }
	uint32_t getLocalY() const { return _localY; }
	uint32_t getLocalZ() const { return _localZ; }

protected:
	uint32_t _localX = 0;
	uint32_t _localY = 0;
	uint32_t _localZ = 0;
};


class SP_PUBLIC Shader : public Object {
public:
	using ProgramData = core::ProgramData;
	using DescriptorType = core::DescriptorType;

	static String inspectShader(SpanView<uint32_t>);

	virtual ~Shader() { }

	virtual ProgramStage getStage() const { return _stage; }

protected:
	String inspect(SpanView<uint32_t>);

	ProgramStage _stage = ProgramStage::None;
};


class SP_PUBLIC RenderPass : public Object {
public:
	using QueuePassData = core::QueuePassData;
	using Attachment = core::Attachment;
	using PipelineDescriptor = core::PipelineDescriptor;
	using DescriptorType = core::DescriptorType;

	virtual ~RenderPass() { }

	virtual bool init(Device &, ClearCallback, ObjectType, ObjectHandle, void *) override;

	uint64_t getIndex() const { return _index; }
	PassType getType() const { return _type; }

protected:
	uint64_t _index = 1; // 0 stays as special value
	PassType _type = PassType::Generic;
};

class SP_PUBLIC Framebuffer : public Object {
public:
	static uint64_t getViewHash(SpanView<Rc<ImageView>>);
	static uint64_t getViewHash(SpanView<uint64_t>);

	virtual ~Framebuffer() { }

	const Extent2 &getExtent() const { return _extent; }
	uint32_t getLayerCount() const { return _layerCount; }
	Extent3 getFramebufferExtent() const { return Extent3(_extent, _layerCount); }
	const Vector<uint64_t> &getViewIds() const { return _viewIds; }
	const Rc<RenderPass> &getRenderPass() const { return _renderPass; }

	uint64_t getHash() const;

protected:
	Extent2 _extent;
	uint32_t _layerCount;
	Vector<uint64_t> _viewIds;
	Rc<RenderPass> _renderPass;
	Vector<Rc<ImageView>> _imageViews;
};

class SP_PUBLIC DataAtlas : public Ref {
public:
	enum Type {
		ImageAtlas,
		MeshAtlas,
		Custom,
	};

	virtual ~DataAtlas() = default;

	bool init(Type, uint32_t count, uint32_t objectSize, Extent2 = Extent2(0, 0));
	void compile();

	uint32_t getHash(uint32_t) const;

	const uint8_t *getObjectByName(uint32_t) const;
	const uint8_t *getObjectByName(StringView) const;
	const uint8_t *getObjectByOrder(uint32_t) const;

	void addObject(uint32_t, void *);
	void addObject(StringView, void *);

	Type getType() const { return _type; }
	uint32_t getObjectSize() const { return _objectSize; }
	Extent2 getImageExtent() const { return _imageExtent; }

	uint32_t getObjectsCount() const { return uint32_t(_intNames.size() + _stringNames.size()); }

	BytesView getData() const { return _data; }
	BytesView getBufferData() const { return _bufferData; }

	void setBuffer(Rc<BufferObject> &&);
	const Rc<BufferObject> &getBuffer() const { return _buffer; }

protected:
	Type _type = Type::Custom;
	uint32_t _objectSize;
	Extent2 _imageExtent;
	HashMap<uint32_t, uint32_t> _intNames;
	HashMap<String, uint32_t> _stringNames;
	Bytes _data;
	Bytes _bufferData;
	Rc<BufferObject> _buffer;
};

class SP_PUBLIC ImageObject : public Object {
public:
	virtual ~ImageObject();

	virtual bool init(Device &, ClearCallback, ObjectType, ObjectHandle ptr, void *p) override;
	virtual bool init(Device &, ClearCallback, ObjectType, ObjectHandle ptr, void *p, uint64_t idx);

	const ImageInfoData &getInfo() const { return _info; }
	uint64_t getIndex() const { return _index; }
	const Rc<DataAtlas> &getAtlas() const { return _atlas; }

	ImageViewInfo getViewInfo(const ImageViewInfo &) const;

	ImageAspects getAspects() const;

protected:
	ImageInfoData _info;
	Rc<DataAtlas> _atlas;

	uint64_t _index = 1; // 0 stays as special value
};

class SP_PUBLIC ImageView : public Object {
public:
	virtual ~ImageView();

	virtual bool init(Device &, ClearCallback, ObjectType, ObjectHandle ptr,
			void *p = nullptr) override;

	void setReleaseCallback(Function<void()> &&);
	void runReleaseCallback();

	const Rc<ImageObject> &getImage() const { return _image; }
	const ImageViewInfo &getInfo() const { return _info; }

	void setLocation(uint32_t set, uint32_t desc) {
		_set = set;
		_descriptor = desc;
	}

	uint32_t getSet() const { return _set; }
	uint32_t getDescriptor() const { return _descriptor; }
	uint64_t getIndex() const { return _index; }

	Extent3 getExtent() const;
	uint32_t getLayerCount() const;

	Extent3 getFramebufferExtent() const;

protected:
	ImageViewInfo _info;
	Rc<ImageObject> _image;

	uint32_t _set = 0;
	uint32_t _descriptor = 0;

	// all ImageViews are atomically indexed for descriptor caching purpose
	uint64_t _index = 1; // 0 stays as special value
	Function<void()> _releaseCallback;
};

class SP_PUBLIC BufferObject : public Object {
public:
	virtual ~BufferObject() { }

	const BufferInfo &getInfo() const { return _info; }
	uint64_t getSize() const { return _info.size; }

	void setLocation(uint32_t set, uint32_t desc) {
		_set = set;
		_descriptor = desc;
	}

	uint32_t getSet() const { return _set; }
	uint32_t getDescriptor() const { return _descriptor; }

	uint64_t getDeviceAddress() const { return _deviceAddress; }

protected:
	BufferInfo _info;

	uint32_t _set = 0;
	uint32_t _descriptor = 0;
	uint64_t _deviceAddress = 0;
};


class SP_PUBLIC Sampler : public Object {
public:
	virtual ~Sampler() { }

	const SamplerInfo &getInfo() const { return _info; }

	void setIndex(uint32_t idx) { _index = idx; }
	uint32_t getIndex() const { return _index; }

protected:
	uint32_t _index = 0;
	SamplerInfo _info;
};

class SP_PUBLIC CommandBuffer : public Ref {
public:
	virtual ~CommandBuffer() = default;

	virtual void bindImage(ImageObject *);
	virtual void bindBuffer(BufferObject *);
	virtual void bindFramebuffer(Framebuffer *);

	SpanView<Rc<QueryPool>> getQueryPools() const { return _queryPools; }

	const CommandPool *getCommandPool() const { return _pool; }

protected:
	const CommandPool *_pool = nullptr;
	uint32_t _currentSubpass = 0;
	bool _withinRenderpass = false;

	Set<Rc<ImageObject>> _images;
	Set<Rc<BufferObject>> _buffers;
	Set<Rc<Framebuffer>> _framebuffers;
	Vector<Rc<QueryPool>> _queryPools;
};

struct SP_PUBLIC MaterialImageSlot {
	Rc<ImageView> image;
	uint32_t refCount = 0;
};

struct SP_PUBLIC MaterialLayout {
	Vector<MaterialImageSlot> imageSlots;
	uint32_t usedImageSlots = 0;

	Rc<TextureSet> set;
};

class SP_PUBLIC Semaphore : public Object {
public:
	virtual ~Semaphore() { }

	SemaphoreType getType() const { return _type; }

	void setSignaled(bool value);
	bool isSignaled() const { return _signaled; }

	void setWaited(bool value);
	bool isWaited() const { return _waited; }

	void setInUse(bool value, uint64_t timeline);
	bool isInUse() const { return _inUse; }

	uint64_t getTimeline() const { return _timeline; }

	virtual bool reset();

protected:
	SemaphoreType _type = SemaphoreType::Default;
	uint64_t _timeline = 0;
	bool _signaled = false;
	bool _waited = false;
	bool _inUse = false;
};

/* Fence wrapper
 *
 * usage pattern:
 * - store handles in common storage
 * - pop one before running signal function
 * - associate resources with Fence
 * - run function, that signals VkFence
 * - schedule spinner on check()
 * - release resources when VkFence is signaled
 * - push Fence back into storage when Fence is signaled
 * - storage should reset() Fence on push
 */
class SP_PUBLIC Fence : public Object {
public:
	enum State {
		Disabled,
		Armed,
		Signaled
	};

	virtual ~Fence();

	void clear();

	FenceType getType() const { return _type; }

	void setFrame(uint64_t f);
	void setFrame(Function<bool()> &&schedule, Function<void()> &&release, uint64_t f);
	uint64_t getFrame() const { return _frame; }

	void setScheduleCallback(Function<bool()> &&schedule);
	void setReleaseCallback(Function<bool()> &&release);

	uint64_t getArmedTime() const { return _armedTime; }

	bool isArmed() const { return _state == Armed; }

	void bindQueries(NotNull<QueryPool>);

	void setArmed(DeviceQueue &);
	void setArmed();

	void setTag(StringView);
	StringView getTag() const { return _tag; }

	void addQueryCallback(Function<void(bool, SpanView<Rc<QueryPool>>)> &&, Ref *, StringView tag);

	// function will be called and ref will be released on fence's signal
	void addRelease(Function<void(bool)> &&, Ref *, StringView tag);

	bool schedule(Loop &);

	bool check(Loop &, bool lockfree = true);
	// void reset(Loop &, Function<void(Rc<Fence> &&)> &&);

	void autorelease(Rc<Ref> &&);

protected:
	using Object::init;

	void setSignaled(Loop &loop);

	void scheduleReset(Loop &);
	void scheduleReleaseReset(Loop &, bool s);
	void doRelease(Loop *loop, bool success);

	virtual Status doCheckFence(bool lockfree) = 0;
	virtual void doResetFence() = 0;

	struct ReleaseHandle {
		Function<void(bool)> callback;
		Rc<Ref> ref;
		StringView tag;
	};

	uint64_t _frame = 0;
	FenceType _type = FenceType::Default;
	State _state = Disabled;
	Vector<ReleaseHandle> _release;
	Mutex _mutex;
	DeviceQueue *_queue = nullptr;
	uint64_t _armedTime = 0;
	StringView _tag;

	Function<bool()> _scheduleFn;
	Function<void()> _releaseFn;
	Vector<Rc<Ref>> _autorelease;
	Vector<Rc<QueryPool>> _queries;
};

} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCOREOBJECT_H_ */
