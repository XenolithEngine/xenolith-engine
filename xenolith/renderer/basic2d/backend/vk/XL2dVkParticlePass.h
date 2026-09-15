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
#include "XLCoreRenderSession.h"

#if MODULE_XENOLITH_BACKEND_VK

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::vk {

class ParticleEmitterAttachmentHandle;

/* Particle state that outlives frames: one set of buffers per ParticleEmitter.

Only the GL loop thread touches it - ParticleEmitterAttachment::handleInput runs from
AttachmentHandle::submitInput and ParticleEmitterAttachmentHandle::finalize from the FrameQueue, both
on that thread. Commands are recorded on worker threads, and frames of one queue can overlap
(several windows, screenshot frames), so a frame records only its own snapshot kept in the
attachment handle. */
class SP_PUBLIC ParticlePersistentData : public Ref {
public:
	struct StagingData {
		Rc<DeviceMemoryPool> memPool;
		Rc<Buffer> source;
		VkDeviceSize sourceOffset = 0;

		Rc<Buffer> target;
		VkDeviceSize targetOffset = 0;
		VkDeviceSize size = 0;
	};

	// An emitter as one frame simulates it
	struct FrameEmitter {
		uint64_t id = 0;
		Rc<Buffer> emitter;
		Rc<Buffer> particles;
		Rc<Buffer> extraData;
		Rc<ParticleSystemData> systemData;
		const ParticleSystemRenderInfo *renderInfo = nullptr;
		uint32_t framesInGen = 1;
		uint32_t genframe = 0;
		uint32_t nframes = 0;
		uint32_t cycle = 0;
		uint32_t seed = 0;
		uint32_t vertexOffset = 0;
		bool uploads = false;
	};

	struct EmitterData {
		uint64_t id = 0;
		core::RenderClientChannel *client = nullptr;
		uint64_t windowId = 0;

		uint64_t clock = 0;
		uint32_t frame = 0; // position in the emission cycle
		uint32_t cycle = 0;
		uint32_t phaseSeed = 0; // the system's seed, or random per restart

		Rc<Buffer> emitter;
		Rc<Buffer> particles;
		Rc<Buffer> extraData;
		Rc<ParticleSystemData> systemData;

		uint32_t count = 0;
		uint64_t systemId = 0;
		uint64_t paramsGeneration = 0;
		uint64_t restartGeneration = 0;
		Size2 defaultSize;

		bool needsEmitterUpload = true;
		// Particles from this index to the end are reinitialized by the next frame
		uint32_t initStart = 0;
	};

	virtual ~ParticlePersistentData() = default;

	// Brings the buffers in line with the frame input and fills the frame's snapshot and uploads
	void update(DeviceMemoryPool *, FrameContextHandle2d *, Vector<FrameEmitter> &,
			Vector<StagingData> &);

	// A frame that did not complete may have lost its uploads: redo them from the CPU data
	void invalidate(SpanView<FrameEmitter>);

	const Map<uint64_t, EmitterData> &getEmitters() const { return _emitters; }

	uint64_t getParticleAllocations() const { return _particleAllocations; }

protected:
	Rc<Buffer> allocate(DeviceMemoryPool *, core::BufferUsage, VkDeviceSize);

	void writeUploads(DeviceMemoryPool *, EmitterData &, const ParticleSystemRenderInfo &,
			Vector<StagingData> &);

	Map<uint64_t, EmitterData> _emitters;
	uint64_t _particleAllocations = 0;
};

class SP_PUBLIC ParticleEmitterAttachment : public BufferAttachment {
public:
	virtual ~ParticleEmitterAttachment() = default;

	virtual bool init(AttachmentBuilder &builder) override;

	ParticlePersistentData *getData() const { return _data; }

	// Remote render session: the per-frame input this attachment consumes is a
	// FrameContextHandle2d.
	virtual Rc<core::AttachmentInputData> makeInputData(NotNull<core::RenderClientChannel> client,
			uint64_t windowId) const override {
		return makeFrameContextInput(client, windowId);
	}

protected:
	void handleInput(FrameQueue &, ParticleEmitterAttachmentHandle &, core::AttachmentInputData *,
			Function<void(bool)> &&);

	Rc<ParticlePersistentData> _data;
};

class SP_PUBLIC ParticleEmitterAttachmentHandle : public BufferAttachmentHandle {
public:
	using FrameEmitter = ParticlePersistentData::FrameEmitter;
	using StagingData = ParticlePersistentData::StagingData;

	virtual ~ParticleEmitterAttachmentHandle() = default;

	Buffer *getVertices() const { return _vertices; }
	Buffer *getCommands() const { return _commands; }

	const ParticleSystemRenderInfo *getEmitterRenderInfo(uint64_t) const;

	// Index in this span is the particle buffer index and the indirect command index
	SpanView<FrameEmitter> getFrameEmitters() const { return _frameEmitters; }
	SpanView<StagingData> getStaging() const { return _staging; }

	virtual uint32_t enumerateDirtyDescriptors(const PassHandle &, const PipelineDescriptor &,
			const core::DescriptorBinding &, const Callback<void(uint32_t)> &) const override;

	virtual void enumerateAttachmentObjects(
			const Callback<void(core::Object *, const core::SubresourceRangeInfo &)> &) override;

	virtual void finalize(FrameQueue &, bool successful) override;

	bool hasInput() const { return !_frameEmitters.empty(); }

protected:
	friend class ParticleEmitterAttachment;

	Rc<Buffer> _vertices;
	Rc<Buffer> _commands;

	Rc<ParticlePersistentData> _data;
	Vector<FrameEmitter> _frameEmitters;
	Vector<StagingData> _staging;
	Map<uint64_t, const ParticleSystemRenderInfo *> _emittersIndexes;
};

class SP_PUBLIC ParticlePass : public QueuePass {
public:
	using AttachmentHandle = core::AttachmentHandle;

	static constexpr StringView UpdatePipelineName = "ParticleUpdateComp";

	virtual ~ParticlePass() = default;

	virtual bool init(Queue::Builder &queueBuilder, QueuePassBuilder &passBuilder,
			const AttachmentData *);

	virtual void prepare(core::Device &) override;

	const AttachmentData *getEmitters() const { return _emitters; }

protected:
	void recordCommandBuffer(const core::SubpassData &, core::FrameQueue &, core::CommandBuffer &);

	const AttachmentData *_emitters = nullptr;
};

} // namespace stappler::xenolith::basic2d::vk

#endif

#endif /* XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKPARTICLEPASS_H_ */
