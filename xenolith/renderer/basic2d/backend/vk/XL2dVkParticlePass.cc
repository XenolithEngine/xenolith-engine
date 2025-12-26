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

#include "XL2dVkParticlePass.h"
#include "SPMemory.h"
#include "SPPlatform.h"
#include "SPTime.h"
#include "XL2d.h"
#include "XL2dCommandList.h"
#include "XL2dConfig.h"
#include "XL2dParticleSystem.h"
#include "XL2dFrameContext.h"
#include "XLCoreAttachment.h"
#include "XLCoreEnum.h"
#include "XLCoreInfo.h"
#include "XLCoreQueueData.h"
#include "XLVk.h"
#include "XLVkAllocator.h"
#include "XLVkAttachment.h"
#include "XLVkDevice.h"
#include "XLVkDeviceQueue.h"
#include "glsl/XL2dShaders.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::vk {

static constexpr int ENABLE_FEEDBACK = 0;

const ParticleSystemRenderInfo *ParticleEmitterAttachmentHandle::getEmitterRenderInfo(
		uint64_t id) const {
	auto it = _emittersIndexes.find(id);
	if (it != _emittersIndexes.end()) {
		return it->second;
	}
	return nullptr;
}

uint32_t ParticleEmitterAttachmentHandle::enumerateDirtyDescriptors(const PassHandle &,
		const PipelineDescriptor &, const core::DescriptorBinding &binding,
		const Callback<void(uint32_t)> &cb) const {
	uint32_t ret = 0;
	uint32_t idx = 0;
	for (auto &it : _buffers) {
		if (it.dirty || it.buffer != binding.get(idx).data
				|| it.buffer->getObjectData().handle != binding.get(idx).object) {
			cb(idx);
			++ret;
		}
		++idx;
	}
	return ret;
}

void ParticleEmitterAttachmentHandle::enumerateAttachmentObjects(
		const Callback<void(core::Object *, const core::SubresourceRangeInfo &)> &cb) {
	using namespace core;
	if (_vertices) {
		cb(_vertices.get(), SubresourceRangeInfo(ObjectType::Buffer, 0, _vertices->getSize()));
	}
	if (_commands) {
		cb(_commands.get(), SubresourceRangeInfo(ObjectType::Buffer, 0, _commands->getSize()));
	}
}

Vector<uint64_t> ParticlePersistentData::updateEmitters(DeviceMemoryPool *pool,
		const memory::map<uint64_t, ParticleSystemRenderInfo> &data, uint64_t clock) {
	Vector<uint64_t> ret;
	auto it = _emitters.begin();
	while (it != _emitters.end()) {
		auto v = data.find(it->first);
		if (v == data.end() || v->second.system->data.count == 0) {
			it = _emitters.erase(it);
		} else {
			if (it->second.systemData != v->second.system) {
				emplace_ordered(ret, v->first);
				updateEmitter(pool, it->second, v->second.system);
			}
			++it;
		}
	}

	for (auto &it : data) {
		auto v = _emitters.find(it.first);
		if (v == _emitters.end()) {
			emplace_ordered(ret, it.first);
			if (it.second.system->data.count) {
				addEmitter(pool, it.first, it.second.system, clock);
			}
		}
	}

	return ret;
}

static void ParticlePersistentData_initParticles(uint8_t *ptr, uint32_t particleCount) {
	// initialize randomizer
	Vector<glsl::pcg16_state_t> randomdata;
	randomdata.resize(particleCount);

	// use hard-random from OS
	// to debug randomness - use static values
	stappler::platform::makeRandomBytes(reinterpret_cast<uint8_t *>(randomdata.data()),
			randomdata.size());

	auto particle = reinterpret_cast<ParticleData *>(ptr);

	for (size_t i = 0; i < particleCount; ++i) {
		auto &init = randomdata[i];

		// Note - do not set values directly, use GLSL-compatible function
		glsl::pcg16_srandom_r(particle->rng, init.state, init.inc);
		++particle;
	}
}

void ParticlePersistentData::updateEmitter(DeviceMemoryPool *pool, EmitterData &e,
		ParticleSystemData *s) {
	uint32_t patchCount = 0;
	uint32_t persistCount = 0;
	if (e.systemData->data.count < s->data.count) {
		patchCount = s->data.count - e.systemData->data.count;
		persistCount = e.systemData->data.count;
	} else {
		persistCount = s->data.count;
	}

	auto newEmitterData = spawnEmitter(pool, e.id, s, patchCount, e.clock);

	_staging.emplace_back(StagingData{
		nullptr,
		e.particles,
		0,
		newEmitterData.particles,
		0,
		persistCount * sizeof(ParticleData),
	});

	e = move(newEmitterData);
}

void ParticlePersistentData::addEmitter(DeviceMemoryPool *pool, uint64_t id, ParticleSystemData *s,
		uint64_t clock) {
	_emitters.emplace(id, spawnEmitter(pool, id, s, s->data.count, clock));
}

void ParticlePersistentData::clearStaging() { _staging.clear(); }

ParticlePersistentData::EmitterData ParticlePersistentData::spawnEmitter(DeviceMemoryPool *pool,
		uint64_t id, ParticleSystemData *s, uint32_t initParticles, uint64_t clock) {
	auto addStaging = [&](Buffer *buf, VkDeviceSize size, VkDeviceSize offset,
							  const Callback<void(uint8_t *, VkDeviceSize)> &cb) {
		auto stage = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
				BufferInfo(core::ForceBufferUsage(core::BufferUsage::TransferSrc), size));

		stage->map(cb, DeviceMemoryAccess::Flush);

		_staging.emplace_back(StagingData{
			pool,
			move(stage),
			0,
			buf,
			offset,
			size,
		});
	};

	auto alloc = pool->getAllocator();

	auto emitterBuffer = alloc->preallocate(
			BufferInfo(core::BufferUsage::ShaderDeviceAddress, sizeof(ParticleEmitterData)));
	auto particlesBuffer = alloc->preallocate(
			BufferInfo(core::BufferUsage::ShaderDeviceAddress | core::BufferUsage::StorageBuffer,
					sizeof(ParticleData) * s->data.count));

	Rc<Buffer> emissionData;
	if (s->data.emissionType == 0) {
		emissionData = alloc->preallocate(BufferInfo(core::BufferUsage::ShaderDeviceAddress,
				sizeof(ParticleEmissionPoints) + s->emissionPoints.size() * sizeof(Vec2)));
	}

	Buffer *buffers[] = {emitterBuffer.get(), particlesBuffer.get(), emissionData.get()};

	alloc->emplaceObjects(AllocationUsage::DeviceLocal, SpanView<Image *>(), makeSpanView(buffers));

	s->data.emissionData = UVec2::convertFromPacked(emissionData->getDeviceAddress());

	addStaging(emitterBuffer, emitterBuffer->getSize(), 0,
			[&](uint8_t *ptr, VkDeviceSize size) { ::__sprt_memcpy(ptr, &s->data, size); });

	if (initParticles > 0) {
		auto fullSize = particlesBuffer->getSize();
		auto patchSize = sizeof(ParticleData) * initParticles;
		auto offset = fullSize - patchSize;

		addStaging(particlesBuffer, patchSize, offset, [&](uint8_t *ptr, VkDeviceSize size) {
			ParticlePersistentData_initParticles(ptr, initParticles);
		});
	}

	addStaging(emissionData, emissionData->getSize(), 0, [&](uint8_t *ptr, VkDeviceSize size) {
		auto points = reinterpret_cast<ParticleEmissionPoints *>(ptr);
		points->count = static_cast<uint32_t>(s->emissionPoints.size());
		ptr += sizeof(ParticleEmissionPoints);
		memcpy(ptr, s->emissionPoints.data(), s->emissionPoints.size() * sizeof(Vec2));
	});

	return EmitterData{
		id,
		clock,
		0,
		move(emitterBuffer),
		move(particlesBuffer),
		move(emissionData),
		Vector<Rc<Buffer>>(),
		s,
	};
}

bool ParticleEmitterAttachment::init(AttachmentBuilder &builder) {
	if (!BufferAttachment::init(builder,
				BufferInfo(core::BufferUsage::StorageBuffer, core::PassType::Compute))) {
		return false;
	}

	_data = Rc<ParticlePersistentData>::alloc();

	builder.setInputValidationCallback([](const core::AttachmentInputData *) { return true; });

	_frameHandleCallback = [&](Attachment &a, const FrameQueue &q) -> Rc<AttachmentHandle> {
		return Rc<ParticleEmitterAttachmentHandle>::create(a, q);
	};

	builder.setInputSubmissionCallback(
			[&](FrameQueue &q, AttachmentHandle &handle, core::AttachmentInputData *d,
					Function<void(bool)> &&cb) {
		handleInput(q, static_cast<ParticleEmitterAttachmentHandle &>(handle), d, sp::move(cb));
	});

	return true;
}

void ParticleEmitterAttachment::handleInput(FrameQueue &q, ParticleEmitterAttachmentHandle &handle,
		core::AttachmentInputData *d, Function<void(bool)> &&complete) {
	auto ctx = static_cast<FrameContextHandle2d *>(d);
	auto dFrame = q.getFrame().get_cast<DeviceFrameHandle>();

	if (!d) {
		complete(false);
	}

	Vector<uint64_t> ids;

	ids = _data->updateEmitters(dFrame->getMemPool(nullptr), ctx->particleEmitters, ctx->clock);

	if (!_data->getEmitters().empty()) {
		auto alloc = dFrame->getMemPool(nullptr);

		size_t nVertexes = 0;

		for (auto &it : ctx->particleEmitters) { nVertexes += it.second.system->data.count; }

		auto vertexBuffer = alloc->spawn(AllocationUsage::DeviceLocal,
				BufferInfo(core::BufferUsage::ShaderDeviceAddress, core::BufferUsage::TransferDst,
						sizeof(Vertex) * nVertexes * 6));

		auto indirectBuffer = alloc->spawn(AllocationUsage::DeviceLocal,
				BufferInfo(core::BufferUsage::ShaderDeviceAddress,
						core::BufferUsage::IndirectBuffer, core::BufferUsage::TransferDst,
						sizeof(ParticleIndirectCommand) * ctx->particleEmitters.size()));

		handle._vertices = vertexBuffer;
		handle._commands = indirectBuffer;

		handle._data = _data;

		uint32_t index = 0;
		for (auto &it : _data->getEmitters()) {
			auto fIt = ctx->particleEmitters.find(it.first);
			if (fIt != ctx->particleEmitters.end()) {
				fIt->second.index = index;
				handle._emittersIndexes.emplace(it.first, &fIt->second);
			}

			handle.addBufferView(it.second.particles);

			++index;
		}
	}

	complete(true);
}

bool ParticlePass::init(Queue::Builder &queueBuilder, QueuePassBuilder &passBuilder,
		const AttachmentData *outVertexes) {
	using namespace core;

	_emitters = outVertexes;

	auto particlesData = passBuilder.addAttachment(_emitters,
			AttachmentDependencyInfo::make(PipelineStage::ComputeShader | PipelineStage::Transfer,
					AccessType::ShaderWrite | AccessType::TransferWrite));

	auto layout = passBuilder.addDescriptorLayout("ParticleLayout",
			[&](PipelineLayoutBuilder &layoutBuilder) {
		layoutBuilder.addSet([&](DescriptorSetBuilder &set) {
			set.addDescriptorArray(particlesData, config::ParticleBufferArraySize,
					DescriptorFlags::UpdateAfterBind | DescriptorFlags::PartiallyBound);
		});
	});

	// clang-format off
	passBuilder.addSubpass([&](SubpassBuilder &subpassBuilder) {
		auto particleUpdateComp =
				queueBuilder.addProgramByRef("ParticleUpdateComp", shaders::ParticleUpdateComp);

		subpassBuilder.addComputePipeline(UpdatePipelineName, layout->defaultFamily,
			SpecializationInfo(
				particleUpdateComp,
				memory::vector<SpecializationConstant>{
					SpecializationConstant(ENABLE_FEEDBACK),
					SpecializationConstant(config::ParticleBufferArraySize)
				}
			)
		);

		subpassBuilder.setCommandsCallback([this] (FrameQueue &frame, const SubpassData &subpass, core::CommandBuffer &buf) {
			recordCommandBuffer(subpass, frame, buf);
		});
	});
	// clang-format on

	passBuilder.setAvailabilityChecker([this](const FrameQueue &queue, const QueuePassData &) {
		auto fHandle = queue.getAttachment(_emitters);
		auto aHandle = fHandle->handle.get_cast<ParticleEmitterAttachmentHandle>();

		return aHandle->hasInput();
	});

	if (!QueuePass::init(passBuilder)) {
		return false;
	}

	return true;
}

void ParticlePass::recordCommandBuffer(const core::SubpassData &subpass, core::FrameQueue &queue,
		core::CommandBuffer &cbuf) {
	auto dFrame = queue.getFrame().get_cast<DeviceFrameHandle>();
	auto memPool = dFrame->getMemPool(nullptr);

	auto &buf = static_cast<vk::CommandBuffer &>(cbuf);
	auto pipelineIt = subpass.computePipelines.find(UpdatePipelineName);
	if (pipelineIt == subpass.computePipelines.end()) {
		return;
	}

	auto fHandle = queue.getAttachment(_emitters);
	auto pHandle = queue.getRenderPass(_data);

	auto attachment = _emitters->attachment.get_cast<ParticleEmitterAttachment>();
	auto aHandle = fHandle->handle.get_cast<ParticleEmitterAttachmentHandle>();
	auto ctx = static_cast<FrameContextHandle2d *>(aHandle->getInput());
	auto data = attachment->getData();

	auto transferIndirectBuffer = memPool->spawn(AllocationUsage::DeviceLocalHostVisible,
			BufferInfo(core::ForceBufferUsage(core::BufferUsage::TransferSrc),
					aHandle->getCommands()->getSize()));

	transferIndirectBuffer->map([&](uint8_t *buf, VkDeviceSize bufSize) {
		auto target = reinterpret_cast<ParticleIndirectCommand *>(buf);

		uint32_t vertexOffset = 0;

		for (auto &e : data->getEmitters()) {
			target->vertexCount = 0;
			target->instanceCount = 1;
			target->firstVertex = vertexOffset;
			target->firstInstance = 0;

			vertexOffset += e.second.systemData->data.count * 6;

			++target;
		}
	}, DeviceMemoryAccess::Flush);

	Vector<BufferMemoryBarrier> barriers;

	auto indirectBuffer = aHandle->getCommands();

	buf.cmdCopyBuffer(transferIndirectBuffer, indirectBuffer);
	//buf.cmdCopyBuffer(transferVertexBuffer, aHandle->getVertices());

	barriers.emplace_back(BufferMemoryBarrier(indirectBuffer, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT));

	for (auto it : data->getStaging()) {
		buf.cmdCopyBuffer(it.source, it.target, it.sourceOffset, it.targetOffset, it.size);
		barriers.emplace_back(BufferMemoryBarrier(it.target, VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, it.targetOffset, it.size));
	}

	data->clearStaging();

	buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
			barriers);

	buf.cmdGlobalBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT);

	buf.cmdBindPipelineWithDescriptors((*pipelineIt), 0);

	uint64_t vertexAddress = buf.bindBufferAddress(aHandle->getVertices());
	uint64_t commandsAddress = buf.bindBufferAddress(aHandle->getCommands());

	ParticleConstantData pcb;

	uint32_t bufferIndex = 0;
	for (auto &e : data->getEmitters()) {
		auto &d = e.second.systemData->data;
		auto renderInfo = aHandle->getEmitterRenderInfo(e.first);

		auto lifetime = d.lifetime.init + d.lifetime.rnd;
		auto framesInGen = uint32_t(
				std::floor(TimeInterval::floatSeconds(lifetime).toMicros() / d.frameInterval));

		auto dt = ctx->clock - e.second.clock;
		auto v = float(dt) / d.frameInterval;
		auto nframes = std::min(renderInfo->maxFramesPerCall, static_cast<uint32_t>(std::floor(v)));

		// corrent frame index if lifetime was decreased
		while (e.second.frame >= framesInGen) { e.second.frame -= framesInGen; }

		pcb.framesInGen = framesInGen;
		pcb.genframe = e.second.frame;
		pcb.gentime = float(pcb.genframe) / float(framesInGen);
		pcb.gendt = (1.0f / float(framesInGen));
		pcb.dt = d.dt;

		pcb.nframes = nframes;
		pcb.timeline = (1.0 - d.explosiveness) * lifetime / d.count;
		pcb.materialIndex = renderInfo->material | renderInfo->transform << 16;

		pcb.outVerticesPointer = UVec2::convertFromPacked(vertexAddress);
		pcb.outCommandPointer = UVec2::convertFromPacked(commandsAddress);
		pcb.emitterPointer = UVec2::convertFromPacked(e.second.emitter->getDeviceAddress());
		pcb.particleBufferIndex = bufferIndex;

		if constexpr (ENABLE_FEEDBACK) {
			auto feedback = memPool->spawn(AllocationUsage::DeviceLocalHostVisible,
					BufferInfo(core::ForceBufferUsage(core::BufferUsage::ShaderDeviceAddress),
							sizeof(ParticleFeedback)));

			feedback->map([&](uint8_t *buf, VkDeviceSize bufSize) { memset(buf, 0, bufSize); },
					DeviceMemoryAccess::Flush);

			pcb.feedbackPointer = UVec2::convertFromPacked(feedback->getDeviceAddress());

			pHandle->handle->getFence()->addRelease([feedback, framesInGen, nframes, pcb](bool) {
				feedback->map([&](uint8_t *buf, VkDeviceSize bufSize) {
					auto fb = (ParticleFeedback *)buf;

					log::source().debug("Particles", framesInGen, " ", nframes, " ", pcb.genframe,
							" ", pcb.gentime, " ", pcb.gendt, " emitted:", fb->emissionCount,
							" simulated:", fb->simulationCount, " skipped:", fb->skippedCount,
							" written: ", fb->written);
					if (fb->emissionCount > 0) {
						log::source().debug("Particles", "P: ", fb->emitted.currentLifetime, " ",
								fb->nframes);
					}
				}, DeviceMemoryAccess::Invalidate);
			}, feedback, SP_FUNC);
		}

		buf.cmdPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0,
				BytesView((const uint8_t *)&pcb, sizeof(ParticleConstantData)));
		buf.cmdDispatchPipeline(*pipelineIt, e.second.systemData->data.count);

		e.second.frame += nframes;
		e.second.clock += static_cast<uint64_t>(nframes) * d.frameInterval;

		while (e.second.frame >= framesInGen) { e.second.frame -= framesInGen; }

		++bufferIndex;
	}
}

} // namespace stappler::xenolith::basic2d::vk
