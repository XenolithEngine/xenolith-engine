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
#include "SPValid.h"
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
#include "glsl/include/XL2dGlslParticleSim.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::vk {

// TransferSrc: a resized emitter copies its particles out of the old buffer
static constexpr auto s_particleBufferUsage = core::BufferUsage::ShaderDeviceAddress
		| core::BufferUsage::StorageBuffer | core::BufferUsage::TransferSrc;

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

void ParticleEmitterAttachmentHandle::finalize(FrameQueue &queue, bool successful) {
	if (!successful && _data) {
		_data->invalidate(_frameEmitters);
	}
	BufferAttachmentHandle::finalize(queue, successful);
}

static void ParticlePersistentData_initParticles(uint8_t *ptr, uint32_t first, uint32_t count,
		const ParticleSystemData *s) {
	::__sprt_memset(ptr, 0, count * sizeof(ParticleData));

	auto particle = reinterpret_cast<ParticleData *>(ptr);

	if (s->hasSeed) {
		// Derived from the index alone, so a grown buffer gets the same tail as a fresh one
		for (uint32_t i = 0; i < count; ++i) {
			glsl::particleSeedRng(particle->rng, s->seed, first + i);
			++particle;
		}
	} else {
		Vector<glsl::pcg16_state_t> randomdata;
		randomdata.resize(count);
		valid::makeRandomBytes(reinterpret_cast<uint8_t *>(randomdata.data()),
				randomdata.size() * sizeof(glsl::pcg16_state_t));

		for (uint32_t i = 0; i < count; ++i) {
			// Note - do not set values directly, use GLSL-compatible function
			glsl::pcg16_srandom_r(particle->rng, randomdata[i].state, randomdata[i].inc);
			++particle;
		}
	}
}

static uint32_t ParticlePersistentData_phaseSeed(const ParticleSystemData *s) {
	if (s->hasSeed) {
		return s->seed;
	}

	uint32_t seed = 0;
	valid::makeRandomBytes(reinterpret_cast<uint8_t *>(&seed), sizeof(seed));
	return seed;
}

static void ParticlePersistentData_addStaging(DeviceMemoryPool *pool,
		Vector<ParticlePersistentData::StagingData> &staging, Buffer *target, VkDeviceSize size,
		VkDeviceSize offset, const Callback<void(uint8_t *, VkDeviceSize)> &cb) {
	auto stage = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			BufferInfo(core::ForceBufferUsage(core::BufferUsage::TransferSrc), size));

	stage->map(cb, DeviceMemoryAccess::Flush);

	staging.emplace_back(ParticlePersistentData::StagingData{
		pool,
		move(stage),
		0,
		target,
		offset,
		size,
	});
}

Rc<Buffer> ParticlePersistentData::allocate(DeviceMemoryPool *pool, core::BufferUsage usage,
		VkDeviceSize size) {
	auto alloc = pool->getAllocator();
	auto buffer = alloc->preallocate(BufferInfo(usage, size));
	Buffer *buffers[] = {buffer.get()};
	alloc->emplaceObjects(AllocationUsage::DeviceLocal, SpanView<Image *>(), makeSpanView(buffers));
	return buffer;
}

void ParticlePersistentData::update(DeviceMemoryPool *pool, FrameContextHandle2d *ctx,
		Vector<FrameEmitter> &frameEmitters, Vector<StagingData> &staging) {
	auto client = ctx->client.get();

	auto it = _emitters.begin();
	while (it != _emitters.end()) {
		if (it->second.client == client && it->second.windowId == ctx->windowId) {
			auto v = ctx->particleEmitters.find(it->first);
			if (v == ctx->particleEmitters.end() || v->second.system->data.count == 0) {
				it = _emitters.erase(it);
				continue;
			}
		}
		++it;
	}

	uint32_t vertexOffset = 0;
	for (auto &info : ctx->particleEmitters) {
		auto s = info.second.system.get();
		if (s->data.count == 0) {
			continue;
		}

		auto eIt = _emitters.find(info.first);
		if (eIt == _emitters.end()) {
			EmitterData e;
			e.id = info.first;
			e.clock = ctx->clock;
			e.emitter = allocate(pool, core::BufferUsage::ShaderDeviceAddress,
					sizeof(ParticleEmitterData));
			e.particles =
					allocate(pool, s_particleBufferUsage, sizeof(ParticleData) * s->data.count);
			e.count = s->data.count;
			e.phaseSeed = ParticlePersistentData_phaseSeed(s);
			e.systemId = s->systemId;
			e.paramsGeneration = s->paramsGeneration;
			e.restartGeneration = s->restartGeneration;
			e.defaultSize = info.second.defaultSize;

			++_particleAllocations;
			log::source().debug("vk::ParticlePass", "emitter ", e.id, ": spawn, count ", e.count,
					", particle buffers allocated: ", _particleAllocations);

			eIt = _emitters.emplace(info.first, move(e)).first;
		} else {
			auto &e = eIt->second;
			const bool restart =
					e.systemId != s->systemId || e.restartGeneration != s->restartGeneration;

			if (e.count != s->data.count) {
				auto particles =
						allocate(pool, s_particleBufferUsage, sizeof(ParticleData) * s->data.count);

				auto persist = sprt::min(sprt::min(e.count, s->data.count), e.initStart);
				if (restart) {
					persist = 0;
				}

				if (persist > 0) {
					staging.emplace_back(StagingData{
						nullptr,
						e.particles,
						0,
						particles,
						0,
						persist * sizeof(ParticleData),
					});
				}

				++_particleAllocations;
				log::source().debug("vk::ParticlePass", "emitter ", e.id, ": resize ", e.count,
						"->", s->data.count,
						", particle buffers allocated: ", _particleAllocations);

				e.particles = move(particles);
				e.initStart = persist;
				e.count = s->data.count;
				e.needsEmitterUpload = true;
			} else if (restart) {
				e.initStart = 0;
				log::source().debug("vk::ParticlePass", "emitter ", e.id, ": restart");
			}

			if (restart) {
				e.frame = 0;
				e.cycle = 0;
				e.clock = ctx->clock;
				e.phaseSeed = ParticlePersistentData_phaseSeed(s);
			}

			if (e.systemId != s->systemId || e.paramsGeneration != s->paramsGeneration
					|| e.defaultSize != info.second.defaultSize) {
				if (!e.needsEmitterUpload) {
					log::source().debug("vk::ParticlePass", "emitter ", e.id, ": params");
				}
				e.needsEmitterUpload = true;
			}

			e.systemId = s->systemId;
			e.paramsGeneration = s->paramsGeneration;
			e.restartGeneration = s->restartGeneration;
			e.defaultSize = info.second.defaultSize;
		}

		auto &e = eIt->second;
		e.client = client;
		e.windowId = ctx->windowId;
		e.systemData = s;

		FrameEmitter frame;
		frame.uploads = e.needsEmitterUpload || e.initStart < e.count;

		writeUploads(pool, e, info.second, staging);

		auto framesInGen = glsl::particleCycleFrames(s->data);

		// a shortened cycle leaves the position past its end
		glsl::particleAdvanceFrame(e.frame, e.cycle, framesInGen, 0);

		frame.id = e.id;
		frame.emitter = e.emitter;
		frame.particles = e.particles;
		frame.extraData = e.extraData;
		frame.systemData = s;
		frame.renderInfo = &info.second;
		frame.framesInGen = framesInGen;
		frame.genframe = e.frame;
		frame.cycle = e.cycle;
		frame.seed = e.phaseSeed;
		frame.vertexOffset = vertexOffset;

		// a frame built before the latest one (a screenshot, another window) simulates nothing
		frame.nframes = glsl::particleAdvanceClock(e.clock, ctx->clock, s->data.frameInterval,
				info.second.maxFramesPerCall);

		glsl::particleAdvanceFrame(e.frame, e.cycle, framesInGen, frame.nframes);

		vertexOffset += e.count * 6;
		frameEmitters.emplace_back(move(frame));
	}
}

void ParticlePersistentData::writeUploads(DeviceMemoryPool *pool, EmitterData &e,
		const ParticleSystemRenderInfo &info, Vector<StagingData> &staging) {
	auto s = e.systemData.get();

	if (e.needsEmitterUpload) {
		auto extraSize = s->getExtraDataSize();
		if (!e.extraData || e.extraData->getSize() != extraSize) {
			e.extraData = allocate(pool, core::BufferUsage::ShaderDeviceAddress, extraSize);
		}

		ParticleEmitterData data = s->data;
		data.emissionData = UVec2::convertFromPacked(e.extraData->getDeviceAddress());
		if (data.sizeValue == 0.0f) {
			ParticleSystemData::writeParticleSize(data, info.defaultSize);
		}

		ParticlePersistentData_addStaging(pool, staging, e.extraData, extraSize, 0,
				[&](uint8_t *ptr, VkDeviceSize size) { s->writeExtraData(ptr, size, data); });

		ParticlePersistentData_addStaging(pool, staging, e.emitter, sizeof(ParticleEmitterData), 0,
				[&](uint8_t *ptr, VkDeviceSize size) {
			::__sprt_memcpy(ptr, &data, sizeof(ParticleEmitterData));
		});

		e.needsEmitterUpload = false;
	}

	if (e.initStart < e.count) {
		auto first = e.initStart;
		auto n = e.count - first;
		glsl::pcg16_state_t firstRng;
		ParticlePersistentData_addStaging(pool, staging, e.particles, n * sizeof(ParticleData),
				first * sizeof(ParticleData), [&](uint8_t *ptr, VkDeviceSize size) {
			ParticlePersistentData_initParticles(ptr, first, n, s);
			firstRng = reinterpret_cast<ParticleData *>(ptr)->rng;
		});

		log::source().debug("vk::ParticlePass", "emitter ", e.id, ": init particles ", first, "..",
				e.count, s->hasSeed ? " seeded" : "", ", rng[", first, "] = ", firstRng.state, ":",
				firstRng.inc);

		e.initStart = e.count;
	}
}

void ParticlePersistentData::invalidate(SpanView<FrameEmitter> emitters) {
	for (auto &it : emitters) {
		if (!it.uploads) {
			continue;
		}

		auto eIt = _emitters.find(it.id);
		if (eIt != _emitters.end() && eIt->second.particles == it.particles) {
			eIt->second.needsEmitterUpload = true;
			eIt->second.initStart = 0;
		}
	}
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
	auto dFrame = q.getFrame().get_cast<DeviceFrameHandle>();

	// The frame can be gone by now: with dependencies, submitInput defers this callback keeping
	// only the FrameQueue, and FrameQueue::tryReleaseFrame() drops _frame once everything finalized
	// (common during shutdown).
	if (!d || !dFrame) {
		complete(false);
		return;
	}

	// Without dynamic indexing the pass never runs, and uploads staged for it would be lost
	auto dev = static_cast<xenolith::vk::Device *>(dFrame->getDevice());
	if (!dev->hasDynamicIndexedBuffers()) {
		complete(true);
		return;
	}

	auto ctx = static_cast<FrameContextHandle2d *>(d);
	auto pool = dFrame->getMemPool(nullptr);

	_data->update(pool, ctx, handle._frameEmitters, handle._staging);

	if (!handle._frameEmitters.empty()) {
		size_t nVertexes = 0;
		for (auto &it : handle._frameEmitters) { nVertexes += it.systemData->data.count; }

		handle._vertices = pool->spawn(AllocationUsage::DeviceLocal,
				BufferInfo(core::BufferUsage::ShaderDeviceAddress, core::BufferUsage::TransferDst,
						sizeof(Vertex) * nVertexes * 6));

		handle._commands = pool->spawn(AllocationUsage::DeviceLocal,
				BufferInfo(core::BufferUsage::ShaderDeviceAddress,
						core::BufferUsage::IndirectBuffer, core::BufferUsage::TransferDst,
						sizeof(ParticleIndirectCommand) * handle._frameEmitters.size()));

		handle._data = _data;

		uint32_t index = 0;
		for (auto &it : handle._frameEmitters) {
			auto fIt = ctx->particleEmitters.find(it.id);
			fIt->second.index = index;
			handle._emittersIndexes.emplace(it.id, &fIt->second);
			handle.addBufferView(it.particles);
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
				mem_pool::Vector<SpecializationConstant>{
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
		auto *dev = static_cast<xenolith::vk::Device *>(queue.getFrame()->getDevice());
		if (!dev->hasDynamicIndexedBuffers()) {
			return false;
		}

		auto fHandle = queue.getAttachment(_emitters);
		auto aHandle = fHandle->handle.get_cast<ParticleEmitterAttachmentHandle>();

		return aHandle->hasInput();
	});

	if (!QueuePass::init(passBuilder)) {
		return false;
	}

	return true;
}

void ParticlePass::prepare(core::Device &dev) {
	auto &vkDev = static_cast<xenolith::vk::Device &>(dev);
	if (vkDev.hasDynamicIndexedBuffers()) {
		return;
	}

	log::source().info("vk::ParticlePass",
			"shaderStorageBufferArrayDynamicIndexing unsupported; "
			"ParticleUpdateComp pipeline disabled");

	for (auto &subpass : _data->subpasses) {
		const_cast<core::SubpassData *>(subpass)->computePipelines.clear();
	}
	for (auto &layout : _data->pipelineLayouts) {
		const_cast<core::PipelineLayoutData *>(layout)->computePipelines.clear();
		if (layout->defaultFamily) {
			const_cast<core::PipelineFamilyData *>(layout->defaultFamily)->computePipelines.clear();
		}
		for (auto &family : layout->families) {
			const_cast<core::PipelineFamilyData *>(family)->computePipelines.clear();
		}
	}
	if (auto *q = const_cast<core::QueueData *>(_data->queue)) {
		q->computePipelines.erase(UpdatePipelineName);
		q->programs.erase("ParticleUpdateComp");
	}
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

	auto aHandle = fHandle->handle.get_cast<ParticleEmitterAttachmentHandle>();
	auto emitters = aHandle->getFrameEmitters();

	auto transferIndirectBuffer = memPool->spawn(AllocationUsage::DeviceLocalHostVisible,
			BufferInfo(core::ForceBufferUsage(core::BufferUsage::TransferSrc),
					aHandle->getCommands()->getSize()));

	// Every emitter draws its own range: six vertices per particle, dead ones degenerate
	transferIndirectBuffer->map([&](uint8_t *buf, VkDeviceSize bufSize) {
		auto target = reinterpret_cast<ParticleIndirectCommand *>(buf);
		for (auto &e : emitters) {
			target->vertexCount = e.systemData->data.count * 6;
			target->instanceCount = 1;
			target->firstVertex = e.vertexOffset;
			target->firstInstance = 0;
			++target;
		}
	}, DeviceMemoryAccess::Flush);

	auto frameData = memPool->spawn(AllocationUsage::DeviceLocalHostVisible,
			BufferInfo(core::ForceBufferUsage(core::BufferUsage::ShaderDeviceAddress),
					sizeof(ParticleFrameData) * emitters.size()));

	Vector<BufferMemoryBarrier> barriers;

	auto indirectBuffer = aHandle->getCommands();

	buf.cmdCopyBuffer(transferIndirectBuffer, indirectBuffer);

	barriers.emplace_back(BufferMemoryBarrier(indirectBuffer, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT));

	for (auto &it : aHandle->getStaging()) {
		buf.cmdCopyBuffer(it.source, it.target, it.sourceOffset, it.targetOffset, it.size);
		barriers.emplace_back(BufferMemoryBarrier(it.target, VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, it.targetOffset, it.size));
	}

	buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
			barriers);

	buf.cmdGlobalBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT);

	buf.cmdBindPipelineWithDescriptors((*pipelineIt), 0);

	frameData->map([&](uint8_t *ptr, VkDeviceSize) {
		auto target = reinterpret_cast<ParticleFrameData *>(ptr);
		uint32_t bufferIndex = 0;
		for (auto &e : emitters) {
			::__sprt_memset(target, 0, sizeof(ParticleFrameData));
			target->emitterPointer = UVec2::convertFromPacked(buf.bindBufferAddress(e.emitter));
			target->vertexOffset = e.vertexOffset;
			target->particleBufferIndex = bufferIndex;
			target->materialIndex = e.renderInfo->material | e.renderInfo->transform << 16;
			target->framesInGen = e.framesInGen;
			target->genframe = e.genframe;
			target->nframes = e.nframes;
			target->cycle = e.cycle;
			target->seed = e.seed;
			target->dt = e.systemData->data.dt;

			auto &m = e.renderInfo->nodeToScene.m;
			target->transformX = Vec4(m[0], m[4], m[12], 0.0f);
			target->transformY = Vec4(m[1], m[5], m[13], 0.0f);
			target->transformRotation = sprt::atan2(m[1], m[0]);
			target->transformScale = sprt::sqrt(sprt::fabs(m[0] * m[5] - m[4] * m[1]));

			// the extra data is reached through the emitter data, keep it alive with the commands
			buf.bindBufferAddress(e.extraData);

			++target;
			++bufferIndex;
		}
	}, DeviceMemoryAccess::Flush);

	ParticleConstantData pcb;
	pcb.frameDataPointer = UVec2::convertFromPacked(buf.bindBufferAddress(frameData));
	pcb.outVerticesPointer =
			UVec2::convertFromPacked(buf.bindBufferAddress(aHandle->getVertices()));
	pcb.padding12 = 0;

	uint32_t emitterIndex = 0;
	for (auto &e : emitters) {
		pcb.emitterIndex = emitterIndex;

		buf.cmdPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0,
				BytesView((const uint8_t *)&pcb, sizeof(ParticleConstantData)));
		buf.cmdDispatchPipeline(*pipelineIt, e.systemData->data.count);

		++emitterIndex;
	}
}

} // namespace stappler::xenolith::basic2d::vk
