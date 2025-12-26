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

#ifndef XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKPARTICLEPASS_H_
#define XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKPARTICLEPASS_H_

#include "XL2dVkMaterial.h"
#include "XL2dCommandList.h"
#include "XLCoreDevice.h"

#if MODULE_XENOLITH_BACKEND_VK

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::vk {

class ParticleEmitterAttachmentHandle;

class SP_PUBLIC ParticlePersistentData : public Ref {
public:
	struct EmitterData {
		uint64_t id = 0;
		mutable uint64_t clock = 0;
		mutable uint64_t frame = 0;
		Rc<Buffer> emitter;
		Rc<Buffer> particles;
		Rc<Buffer> emissionData;
		Vector<Rc<Buffer>> extraData;
		Rc<ParticleSystemData> systemData;
	};

	struct StagingData {
		Rc<DeviceMemoryPool> memPool;
		Rc<Buffer> source;
		VkDeviceSize sourceOffset = 0;

		Rc<Buffer> target;
		VkDeviceSize targetOffset = 0;
		VkDeviceSize size = 0;
	};

	virtual ~ParticlePersistentData() = default;

	Vector<uint64_t> updateEmitters(DeviceMemoryPool *,
			const memory::map<uint64_t, ParticleSystemRenderInfo> &data, uint64_t clock);

	void updateEmitter(DeviceMemoryPool *, EmitterData &, ParticleSystemData *);
	void addEmitter(DeviceMemoryPool *, uint64_t id, ParticleSystemData *, uint64_t clock);

	const Map<uint64_t, EmitterData> &getEmitters() const { return _emitters; }

	SpanView<StagingData> getStaging() const { return _staging; }

	void clearStaging();

protected:
	EmitterData spawnEmitter(DeviceMemoryPool *, uint64_t id, ParticleSystemData *,
			uint32_t initParticles, uint64_t clock);

	mutable Vector<StagingData> _staging;
	Map<uint64_t, EmitterData> _emitters;
};

class SP_PUBLIC ParticleEmitterAttachment : public BufferAttachment {
public:
	virtual ~ParticleEmitterAttachment() = default;

	virtual bool init(AttachmentBuilder &builder);

	ParticlePersistentData *getData() const { return _data; }

protected:
	void handleInput(FrameQueue &, ParticleEmitterAttachmentHandle &, core::AttachmentInputData *,
			Function<void(bool)> &&);

	Rc<ParticlePersistentData> _data;
};

class SP_PUBLIC ParticleEmitterAttachmentHandle : public BufferAttachmentHandle {
public:
	virtual ~ParticleEmitterAttachmentHandle() = default;

	Buffer *getVertices() const { return _vertices; }
	Buffer *getCommands() const { return _commands; }

	const ParticleSystemRenderInfo *getEmitterRenderInfo(uint64_t) const;

	virtual uint32_t enumerateDirtyDescriptors(const PassHandle &, const PipelineDescriptor &,
			const core::DescriptorBinding &, const Callback<void(uint32_t)> &) const override;

	virtual void enumerateAttachmentObjects(
			const Callback<void(core::Object *, const core::SubresourceRangeInfo &)> &) override;

	bool hasInput() const { return !_emittersIndexes.empty(); }

protected:
	friend class ParticleEmitterAttachment;

	Rc<Buffer> _vertices;
	Rc<Buffer> _commands;

	Rc<ParticlePersistentData> _data;
	Map<uint64_t, const ParticleSystemRenderInfo *> _emittersIndexes;
};

class SP_PUBLIC ParticlePass : public QueuePass {
public:
	using AttachmentHandle = core::AttachmentHandle;

	static constexpr StringView UpdatePipelineName = "ParticleUpdateComp";

	virtual ~ParticlePass() = default;

	virtual bool init(Queue::Builder &queueBuilder, QueuePassBuilder &passBuilder,
			const AttachmentData *);

	const AttachmentData *getEmitters() const { return _emitters; }

protected:
	void recordCommandBuffer(const core::SubpassData &, core::FrameQueue &, core::CommandBuffer &);

	const AttachmentData *_emitters = nullptr;
};

} // namespace stappler::xenolith::basic2d::vk

#endif

#endif /* XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKPARTICLEPASS_H_ */
