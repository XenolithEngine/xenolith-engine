/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#include "XL2dVkVertexPass.h"
#include "XL2dVertexPlan.h"
#include "XLCoreAttachment.h"
#include "XLCoreEnum.h"
#include "XLCoreFrameHandle.h"
#include "XLCoreFrameQueue.h"
#include "XLCorePresentationFrame.h"
#include "XLDirector.h"
#include "XLVkDeviceQueue.h"
#include "XLVkRenderPass.h"
#include "XLVkTextureSet.h"
#include "XLVkPipeline.h"
#include "XL2dFrameContext.h"
#include "XL2dDamage.h"
#include "XLLinearGradient.h"
#include "backend/vk/XL2dVkParticlePass.h"
#include "glsl/include/XL2dGlslVertexData.h"
#include <vulkan/vulkan_core.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::vk {

struct VertexMaterialVertexProcessor;

// The frame's draw plan itself is backend-neutral and lives in basic2d::VertexPlan. What is left
// here is the Vulkan half: device buffers to write that plan into, and the frame plumbing.
struct VertexMaterialVertexProcessor : public Ref {
	using WriteTarget = VertexWriteTarget;

	core::FrameConstraints _constraints;
	bool _persistentMapping = false;
	Rc<core::FrameCache> _cache;
	Rc<Device> _device;
	Rc<DeviceMemoryPool> _devMemPool;

	// what the plan reads from the frame and what it produces
	VertexPlanContext _plan;

	uint32_t shadowsCmds = 0;

	uint64_t _time = 0;

	Rc<Buffer> _indexes;
	Rc<Buffer> _vertexes;
	Rc<Buffer> _transforms;

	VertexAttachmentHandle *_attachment = nullptr;
	Rc<FrameContextHandle2d> _input;
	Function<void(bool)> _callback;
	DrawStat _drawStat;

	// Damage is collected by the plan's own command walk; the collector lives here because it
	// outlives the plan (the frame request keeps its finalized state).
	DamageCollector _damage;
	Rc<core::FrameRequest> _request;

	VertexMaterialVertexProcessor(VertexAttachmentHandle *, Rc<FrameContextHandle2d> &&,
			Function<void(bool)> &&cb);

	void run(core::FrameHandle &frame);

	bool loadVertexes();

	void finalize(VertexPlan *plan);

#if XL_FRAME_ACCOUNT
	uint64_t _walkTime = 0;
	uint64_t _bufferTime = 0;
	uint64_t _fillTime = 0;
	uint64_t _uploadTime = 0;
	uint64_t _queueWaitTime = 0;
#endif
};

VertexMaterialVertexProcessor::VertexMaterialVertexProcessor(VertexAttachmentHandle *a,
		Rc<FrameContextHandle2d> &&input, Function<void(bool)> &&cb)
: _attachment(a), _input(sp::move(input)), _callback(sp::move(cb)) {
	_time = sp::platform::clock(ClockType::Monotonic);
}

void VertexMaterialVertexProcessor::run(core::FrameHandle &frame) {
	_constraints = frame.getFrameConstraints();
	_request = frame.getRequest();
	_persistentMapping = frame.isPersistentMapping();
	_cache = frame.getLoop()->getFrameCache();
	_device = static_cast<Device *>(frame.getDevice());
	_devMemPool = static_cast<DeviceFrameHandle &>(frame).getMemPool(this);

	frame.performInQueue([this](core::FrameHandle &handle) {
		if (!loadVertexes()) {
			_callback(false);
		}
	}, this, "VertexMaterialAttachmentHandle::submitInput");
}

bool VertexMaterialVertexProcessor::loadVertexes() {
#if XL_FRAME_ACCOUNT
	// Everything before this point is queue latency, not work: the constructor stamped `_time` when
	// the input was submitted and this body runs when a worker picks the task up.
	_queueWaitTime = (sp::platform::clock(ClockType::Monotonic) - _time) * 1'000;
#endif
	auto pool = memory::pool::create(memory::pool::acquire());
	auto ret = mem_pool::perform([&] {
		_drawStat.cachedFramebuffers = uint32_t(_cache->getFramebuffersCount());
		_drawStat.cachedImages = uint32_t(_cache->getImagesCount());
		_drawStat.cachedImageViews = uint32_t(_cache->getImageViewsCount());
		_drawStat.materials = uint32_t(_attachment->getMaterialSet()->getMaterials().size());

		auto plan = new (pool) VertexPlan;
		plan->surfaceExtent = _constraints.extent;
		plan->transform = _constraints.transform;
		plan->hasGpuSideAtlases = _device->hasBufferDeviceAddresses();
		plan->flatOrder = _attachment->isFlatOrder();
		plan->pool = pool;

		_plan.input = _input;
		_plan.materialSet = _attachment->getMaterialSet();
		_plan.damage = &_damage;
		_plan.collectDamage = _attachment->isDamageTracked();
		if (_plan.collectDamage) {
			_damage.init(_input, _constraints);
		}

		auto shadowExtent = _input->lights.getShadowExtent(_constraints.getScreenSize());
		auto shadowSize = _input->lights.getShadowSize(_constraints.getScreenSize());

		plan->shadowSize = Vec2(shadowSize.width / float(shadowExtent.width),
				shadowSize.height / float(shadowExtent.height));

#if XL_FRAME_ACCOUNT
		// One clock read at each boundary, so the four phases add up to the stage instead of being
		// four independent measurements of overlapping things.
		auto phaseMark = sp::platform::nanoclock(ClockType::Monotonic);
#endif

		auto cmd = _input->commands->getFirst();
		while (cmd) {
			plan->pushCommand(_plan, cmd);
			cmd = cmd->next;
		}

#if XL_FRAME_ACCOUNT
		{
			auto now = sp::platform::nanoclock(ClockType::Monotonic);
			_walkTime = now - phaseMark;
			phaseMark = now;
		}
#endif

		// create buffers
		_indexes = _devMemPool->spawn(AllocationUsage::DeviceLocalHostVisible,
				BufferInfo(StringView("IndexBuffer"), core::BufferUsage::IndexBuffer,
						(plan->globalWritePlan.indexes + 12) * sizeof(uint32_t)));

		_vertexes = _devMemPool->spawn(AllocationUsage::DeviceLocalHostVisible,
				BufferInfo(StringView("VertexBuffer"), core::BufferUsage::StorageBuffer,
						core::BufferUsage::ShaderDeviceAddress,
						(plan->globalWritePlan.vertexes + 8) * sizeof(Vertex)));

		_transforms = _devMemPool->spawn(AllocationUsage::DeviceLocalHostVisible,
				BufferInfo(StringView("TransformBuffer"), core::BufferUsage::StorageBuffer,
						core::BufferUsage::ShaderDeviceAddress,
						(_input->commands->getPredefinedTransforms()
								+ plan->globalWritePlan.transforms + 1)
								* sizeof(TransformData)));

		if (!_vertexes || !_indexes || !_transforms) {
			delete plan;
			return false;
		}

#if XL_FRAME_ACCOUNT
		{
			auto now = sp::platform::nanoclock(ClockType::Monotonic);
			_bufferTime = now - phaseMark;
			phaseMark = now;
		}
#endif

		Bytes vertexData, indexData, transformData, instanceData;

		WriteTarget writeTarget;
		writeTarget.transtormOffset = _input->commands->getPredefinedTransforms();

		if (_persistentMapping) {
			// do not invalidate regions
			writeTarget.vertexes = _vertexes->getPersistentMappedRegion(false);
			writeTarget.indexes = _indexes->getPersistentMappedRegion(false);
			writeTarget.transform = reinterpret_cast<TransformData *>(
					_transforms->getPersistentMappedRegion(false));
		} else {
			vertexData.resize(_vertexes->getSize());
			indexData.resize(_indexes->getSize());
			transformData.resize(_transforms->getSize());

			writeTarget.vertexes = vertexData.data();
			writeTarget.indexes = indexData.data();
			writeTarget.transform = reinterpret_cast<TransformData *>(transformData.data());
		}

		if (plan->isEmpty()) {
			plan->pushInitial(writeTarget);
		} else {
			plan->updatePathsDepth();

			// write initial full screen quad
			plan->pushAll(_plan, writeTarget);
		}

#if XL_FRAME_ACCOUNT
		// The write plus the spans; the plan splits the two for itself.
		_fillTime = sp::platform::nanoclock(ClockType::Monotonic) - phaseMark;
		phaseMark = sp::platform::nanoclock(ClockType::Monotonic);
#endif

		if (_persistentMapping) {
			_vertexes->flushMappedRegion();
			_indexes->flushMappedRegion();
			_transforms->flushMappedRegion();
		} else {
			_vertexes->setData(vertexData);
			_indexes->setData(indexData);
			_transforms->setData(transformData);
		}

#if XL_FRAME_ACCOUNT
		_uploadTime = sp::platform::nanoclock(ClockType::Monotonic) - phaseMark;
#endif

		finalize(plan);
		delete plan;
		return true;
	}, pool);
	memory::pool::destroy(pool);
	return ret;
}

void VertexMaterialVertexProcessor::finalize(VertexPlan *plan) {
	auto t = sp::platform::clock(ClockType::Monotonic);
	_drawStat.vertexes = plan->globalWritePlan.vertexes - plan->excludeVertexes;
	_drawStat.triangles = (plan->globalWritePlan.indexes - plan->excludeIndexes) / 3;
	_drawStat.zPaths = uint32_t(plan->paths.size());
	_drawStat.drawCalls = uint32_t(_plan.materialSpans.size() + _plan.overlaySpans.size());
	_drawStat.solidCmds = _plan.solidCmds;
	_drawStat.surfaceCmds = _plan.surfaceCmds;
	_drawStat.transparentCmds = _plan.transparentCmds;
	_drawStat.shadowsCmds = shadowsCmds;
	_drawStat.vertexInputTime = uint32_t(t - _time);
#if XL_FRAME_ACCOUNT
	/* The PRESENTATION frame's order, not the FrameHandle's.

	They are two different counters and they do not agree - measured, one apart - so a reader that
	took one from here and the other from the completion bookkeeping would be comparing two
	numbering schemes and calling the mismatch an error. The presentation frame is the one the
	engine's own timing block names, so it is the one that goes here. */
	if (auto pf = _request ? _request->getPresentationFrame() : nullptr) {
		_drawStat.frameOrder = pf->getFrameOrder();
	}
	_drawStat.deferredWorkTime = plan->deferredWorkTime;
	_drawStat.deferredWaitTime = plan->deferredWaitTime;
	_drawStat.deferredCount = plan->deferredCount;
	_drawStat.deferredWaited = plan->deferredWaited;

	_drawStat.walkTime = _walkTime;
	_drawStat.bufferTime = _bufferTime;
	_drawStat.uploadTime = _uploadTime;
	_drawStat.writeTime = plan->writeTime;
	_drawStat.spanTime = plan->spanTime;
	_drawStat.damageTime = plan->damageTime;
	_drawStat.planTime = plan->planTime;
	_drawStat.queueWaitTime = _queueWaitTime;
	_drawStat.fillTime = _fillTime;
#endif
	if (_input->client) {
		_input->client->pushDrawStat(_drawStat);
	}

	// Publish before the attachment reports readiness, so the loop thread sees a fully written
	// state by the time it records the pass or presents.
	auto damageState = _plan.collectDamage ? _damage.finalize() : Rc<core::FrameDamageState>();
	if (_request) {
		_request->setDamageState(Rc<core::FrameDamageState>(damageState));
	}

	_attachment->loadData(sp::move(_input), sp::move(_indexes), sp::move(_vertexes),
			sp::move(_transforms), sp::move(_plan.materialSpans), sp::move(_plan.overlaySpans),
			sp::move(_plan.shadowSolidSpans), sp::move(_plan.shadowSdfSpans), plan->maxShadowValue,
			sp::move(damageState));

	_callback(true);
}

bool VertexAttachment::init(AttachmentBuilder &builder, const AttachmentData *m, bool flatOrder,
		bool damageTracked) {
	if (core::GenericAttachment::init(builder)) {
		_materials = m;
		_flatOrder = flatOrder;
		_damageTracked = damageTracked;
		return true;
	}
	return false;
}

auto VertexAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<VertexAttachmentHandle>::create(this, handle);
}

bool VertexAttachmentHandle::setup(FrameQueue &handle, Function<void(bool)> &&cb) {
	if (auto materials = handle.getAttachment(
				(static_cast<VertexAttachment *>(_attachment.get()))->getMaterials())) {
		_materials = static_cast<const MaterialAttachmentHandle *>(materials->handle.get());
	}
	return true;
}

void VertexAttachmentHandle::submitInput(FrameQueue &q, Rc<core::AttachmentInputData> &&data,
		Function<void(bool)> &&cb) {
	auto d = data.cast<FrameContextHandle2d>();
	if (!d || q.isFinalized()) {
		cb(false);
		return;
	}

	q.getFrame()->waitForDependencies(data->waitDependencies,
			[this, d = sp::move(d), cb = sp::move(cb)](FrameHandle &handle, bool success) mutable {
		if (!success || !handle.isValidFlag()) {
			cb(false);
			return;
		}

		_materialSet = _materials->getSet();

		handle.getPool()->perform([&] {
			auto proc = Rc<VertexMaterialVertexProcessor>::alloc(this, sp::move(d), sp::move(cb));
			proc->run(handle);
		});
	});
}

bool VertexAttachmentHandle::empty() const { return !_indexes || !_vertexes || !_transforms; }

void VertexAttachmentHandle::loadData(Rc<FrameContextHandle2d> &&data, Rc<Buffer> &&indexes,
		Rc<Buffer> &&vertexes, Rc<Buffer> &&transforms, Vector<VertexSpan> &&spans,
		Vector<VertexSpan> &&overlaySpans, Vector<VertexSpan> &&shadowSolidSpans,
		Vector<VertexSpan> &&shadowSdfSpans, float maxShadowValue,
		Rc<core::FrameDamageState> &&damage) {
	_damage = sp::move(damage);
	_commands = move(data);
	_indexes = move(indexes);
	_vertexes = move(vertexes);
	_transforms = move(transforms);
	_spans = sp::move(spans);
	_overlaySpans = sp::move(overlaySpans);
	_shadowSolidSpans = sp::move(shadowSolidSpans);
	_shadowSdfSpans = sp::move(shadowSdfSpans);

	_maxShadowValue = maxShadowValue;
}

const Rc<FrameContextHandle2d> &VertexAttachmentHandle::getCommands() const { return _commands; }

core::ImageFormat VertexPass::selectDepthFormat(SpanView<core::ImageFormat> formats) {
	core::ImageFormat ret = core::ImageFormat::Undefined;

	uint32_t score = 0;

	auto selectWithScore = [&](core::ImageFormat fmt, uint32_t sc) {
		if (score < sc) {
			ret = fmt;
			score = sc;
		}
	};

	for (auto &it : formats) {
		switch (it) {
		case core::ImageFormat::D16_UNORM: selectWithScore(it, 12); break;
		case core::ImageFormat::X8_D24_UNORM_PACK32: selectWithScore(it, 7); break;
		case core::ImageFormat::D32_SFLOAT: selectWithScore(it, 9); break;
		case core::ImageFormat::S8_UINT: break;
		case core::ImageFormat::D16_UNORM_S8_UINT: selectWithScore(it, 11); break;
		case core::ImageFormat::D24_UNORM_S8_UINT: selectWithScore(it, 10); break;
		case core::ImageFormat::D32_SFLOAT_S8_UINT: selectWithScore(it, 8); break;
		default: break;
		}
	}

	return ret;
}

auto VertexPass::makeFrameHandle(const FrameQueue &handle) -> Rc<QueuePassHandle> {
	return Rc<VertexPassHandle>::create(*this, handle);
}

bool VertexPassHandle::prepare(FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = static_cast<VertexPass *>(_queuePass.get());

	if (auto materialBuffer = q.getAttachment(pass->getMaterials())) {
		_materialBuffer =
				static_cast<const MaterialAttachmentHandle *>(materialBuffer->handle.get());
	}

	if (auto vertexBuffer = q.getAttachment(pass->getVertexes())) {
		_vertexBuffer = static_cast<const VertexAttachmentHandle *>(vertexBuffer->handle.get());
	}

	if (auto particleBuffer = q.getAttachment(pass->getParticles())) {
		_particles =
				static_cast<const ParticleEmitterAttachmentHandle *>(particleBuffer->handle.get());
	}

	/* A frame that was asked for no cutout leaves these null, and recordFrameCapture then records
	nothing - which is the whole cost of an idle capture.

	An input with no regions is the ordinary case, not an error: FrameContext2d submits one on every
	frame because an input attachment that is not fed wedges the frame waiting for it. */
	_captureInput = nullptr;
	_captureSource = nullptr;
	_captureSourcePresented = false;

	if (auto capture = q.getAttachment(pass->getCapture())) {
		auto input = dynamic_cast<const core::FrameCaptureInput *>(capture->handle->getInput());
		if (input && !input->regions.empty()) {
			_captureInput = input;
		}
	}

	if (_captureInput) {
		if (auto output = q.getAttachment(pass->getOutput())) {
			if (auto storage = output->image.get()) {
				_captureSource = static_cast<Image *>(storage->getImage().get());
				_captureSourcePresented = storage->isSwapchainImage();
			}
		}
		if (!_captureSource) {
			// Nothing to copy out of. The frame still completes, and the input's completion still
			// runs - the targets are simply told the capture did not happen.
			_captureInput = nullptr;
		}
	}

	return QueuePassHandle::prepare(q, sp::move(cb));
}

Vector<const core::CommandBuffer *> VertexPassHandle::doPrepareCommands(FrameHandle &handle) {
	CommandBufferInfo info;

	auto queue = _device->getQueueFamily(_pool->getFamilyIdx());
	if (queue->timestampValidBits > 0 && _data->acquireTimestamps > 0) {
		info.timestampQueries = _data->acquireTimestamps;
	}

	auto buf = _pool->recordBuffer(*_device, Vector<Rc<DescriptorPool>>(_descriptors),
			[&, this](CommandBuffer &buf) {
		auto materials = _materialBuffer->getSet().get();

		Vector<ImageMemoryBarrier> outputImageBarriers;
		Vector<BufferMemoryBarrier> outputBufferBarriers;

		doFinalizeTransfer(materials, outputImageBarriers, outputBufferBarriers);

		if (!outputBufferBarriers.empty() && !outputImageBarriers.empty()) {
			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
					VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, outputBufferBarriers,
					outputImageBarriers);
		}

		prepareRenderPass(buf);

		_data->impl.cast<RenderPass>()->perform(*this, buf,
				[&, this] { prepareMaterialCommands(materials, buf); }, true);

		finalizeRenderPass(buf);
		return true;
	}, move(info));

	return Vector<const core::CommandBuffer *>{buf};
}

void VertexPassHandle::doProcessQueries(FrameQueue &, SpanView<Rc<core::QueryPool>> queries) {
	for (auto &q : queries) {
		if (q->getInfo().type == core::QueryType::Timestamp) {
			uint64_t begin = 0;
			uint64_t end = 0;
			q.get_cast<QueryPool>()->getResults(*_device,
					[&](SpanView<uint64_t> values, uint32_t tag) {
				if (tag == TimestampBeginTag) {
					begin = values.front();
				} else if (tag == TimestampEndTag) {
					end = values.front();
				}
			});
			if (begin && end && begin < end) {
				auto nticks = end - begin;
				auto mksec = nticks
						* _device->getInfo().properties.device10.properties.limits.timestampPeriod
						/ 1000.0f;
				_queueData->deviceTime = static_cast<uint64_t>(sprt::ceil(mksec));
			}
		}
	}
}

void VertexPassHandle::prepareRenderPass(CommandBuffer &buf) {
	buf.cmdWriteTimestamp(core::PipelineStage::TopOfPipe, TimestampBeginTag);
}

void VertexPassHandle::prepareMaterialCommands(core::MaterialSet *materials, CommandBuffer &buf) {
	drawSpans(materials, buf, _vertexBuffer->getVertexData());
}

/* Record one set of spans.

Split out of prepareMaterialCommands so it can be run twice over two disjoint sets: the content, and
then - after the frame has been copied out - the Overlay level. Nothing here depends on the iteration
index or on the previous span, and the spans carry absolute firstIndex/vertexOffset/firstInstance, so
two runs produce exactly the pixels one run over the concatenation would. */
void VertexPassHandle::drawSpans(core::MaterialSet *materials, CommandBuffer &buf,
		SpanView<VertexSpan> spans) {
	auto commands = _vertexBuffer->getCommands();
	auto pass = static_cast<RenderPass *>(_data->impl.get());

	// bind global indexes
	if (_vertexBuffer->getIndexes()) {
		buf.cmdBindIndexBuffer(_vertexBuffer->getIndexes(), 0, VK_INDEX_TYPE_UINT32);
	}

	if (_vertexBuffer->empty() || !_vertexBuffer->getIndexes() || !_vertexBuffer->getVertexes()) {
		return;
	}

	clearDynamicState(buf);

	uint32_t boundTextureSetIndex = maxOf<uint32_t>();

	VertexConstantData pcb;
	pcb.vertexPointer =
			UVec2::convertFromPacked(buf.bindBufferAddress(_vertexBuffer->getVertexes().get()));
	pcb.transformPointer =
			UVec2::convertFromPacked(buf.bindBufferAddress(_vertexBuffer->getTransforms().get()));

	// Use commented code to debug drawing command-by-command
	//static size_t ctrl = 0;

	//size_t i = 0;
	//size_t min = 0;
	//size_t max = (ctrl ++) % (spans.size());
	auto drawSpan = [&](const VertexSpan &materialVertexSpan) {
		//++ i;
		//if (i != max) {
		//	continue;
		//}
		//if (i < min) {
		//
		//continue;
		//}
		//if (i >= max) {
		//	break;
		//}

		//++ i;

		auto material = materials->getMaterialById(materialVertexSpan.material);
		if (!material) {
			return;
		}
		if (material->getImages().empty()) {
			stappler::log::source().error("MaterialRenderPassHandle", "Material ",
					materialVertexSpan.material, " has no images");
			return;
		}

		pcb.materialPointer =
				UVec2::convertFromPacked(buf.bindBufferAddress(material->getBuffer()));
		pcb.imageIdx = material->getImages().front().descriptor;
		pcb.samplerIdx = material->getImages().front().sampler;
		pcb.gradientOffset = materialVertexSpan.gradientOffset;
		pcb.gradientCount = materialVertexSpan.gradientCount;
		//pcb.outlineOffset = materialVertexSpan.outlineOffset;

		if (auto a = material->getAtlas()) {
			// Without BDA the font path keeps CPU-only DataAtlas (no GPU buffer).
			if (auto atlasBuf = a->getBuffer()) {
				pcb.atlasPointer = UVec2::convertFromPacked(buf.bindBufferAddress(atlasBuf));
			}
		}

		auto textureSetIndex = material->getLayoutIndex();

		auto pipeline = material->getPipeline();
		if (!pipeline || !pipeline->pipeline) {
			stappler::log::source().error("MaterialRenderPassHandle", "Material ",
					materialVertexSpan.material, " has no pipeline");
			return;
		}
		buf.cmdBindPipelineWithDescriptors(pipeline);

		if (textureSetIndex != boundTextureSetIndex) {
			auto l = materials->getLayout(textureSetIndex);
			if (l && l->set) {
				auto s = static_cast<TextureSet *>(l->set.get());
				auto set = s->getSet();

				// rebind texture set at last index
				buf.cmdBindDescriptorSets(static_cast<RenderPass *>(_data->impl.get()),
						makeSpanView(&set, 1), pipeline->layout->sets.size());
				boundTextureSetIndex = textureSetIndex;
			} else {
				stappler::log::source().error("MaterialRenderPassHandle",
						"Invalid textureSetlayout: ", textureSetIndex);
				return;
			}
		}

		applyDynamicState(commands, buf, materialVertexSpan.state);

		if (materialVertexSpan.particleSystemId > 0) {
			if (!_particles) {
				return;
			}

			auto particleVertexes = _particles->getVertices();

			auto emitterRenderInfo =
					_particles->getEmitterRenderInfo(materialVertexSpan.particleSystemId);

			if (particleVertexes && emitterRenderInfo) {
				VertexConstantData pcbParticle = pcb;

				pcbParticle.vertexPointer =
						UVec2::convertFromPacked(particleVertexes->getDeviceAddress());

				buf.cmdPushConstants(pass->getPipelineLayout(0),
						VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
						BytesView(reinterpret_cast<const uint8_t *>(&pcbParticle),
								sizeof(VertexConstantData)));

				buf.cmdDrawIndirect(_particles->getCommands(),
						emitterRenderInfo->index * sizeof(ParticleIndirectCommand), 1,
						sizeof(ParticleIndirectCommand));
			}
		} else {
			buf.cmdPushConstants(pass->getPipelineLayout(0),
					VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
					BytesView(reinterpret_cast<const uint8_t *>(&pcb), sizeof(VertexConstantData)));

			buf.cmdDrawIndexed(materialVertexSpan.indexCount, // indexCount
					materialVertexSpan.instanceCount, // instanceCount
					materialVertexSpan.firstIndex, // firstIndex
					materialVertexSpan.vertexOffset, // vertexOffset
					materialVertexSpan.firstInstance // uint32_t  firstInstance
			);
		}
	};

	for (auto &materialVertexSpan : spans) { drawSpan(materialVertexSpan); }
}

void VertexPassHandle::finalizeRenderPass(CommandBuffer &buf) {
	buf.cmdWriteTimestamp(core::PipelineStage::BottomOfPipe, TimestampEndTag);

	// The order here is the whole point of the Overlay level: the frame is copied out first, and only
	// then does the overlay draw on top of it. So a cutout can never contain the overlay - a drag
	// ghost cannot photograph itself - and that holds however the ghost was created and whenever.
	recordFrameCapture(buf);
	recordOverlayPass(buf);
}

void VertexPassHandle::recordOverlayPass(CommandBuffer &buf) {
	// isRedrawSkipped: the target image already holds this exact frame and RenderPass::perform
	// recorded nothing at all, not even the barriers - so the image is in a layout this pass has not
	// established, and there is by definition nothing new to draw over it.
	if (isRedrawSkipped()) {
		return;
	}

	auto spans = _vertexBuffer ? _vertexBuffer->getOverlayData() : SpanView<VertexSpan>();
	if (spans.empty()) {
		return;
	}

	auto pass = _data->impl.cast<RenderPass>();
	if (!pass->getRenderPass(RenderPassVariant::Default)) {
		return;
	}

	// Same render area as the content pass: with a partial redraw everything outside the damaged
	// rectangle is unchanged, overlay geometry included - the damage collector walks the overlay
	// commands like any other, so anything the overlay moved is already inside it.
	const VkRect2D *renderArea = nullptr;
	VkRect2D area;
	if (hasPartialRedrawArea(area)) {
		renderArea = &area;
	}

	auto variant = pass->usesAlternativeAttachments(*this) ? RenderPassVariant::OverlayOffscreen
														   : RenderPassVariant::Overlay;
	if (!pass->getRenderPass(variant)) {
		return;
	}

	buf.cmdBeginRenderPass(pass, static_cast<Framebuffer *>(getFramebuffer()),
			VK_SUBPASS_CONTENTS_INLINE, variant, renderArea);

	recordOverlaySubpasses(buf, spans);

	buf.cmdEndRenderPass();
}

void VertexPassHandle::recordOverlaySubpasses(CommandBuffer &buf, SpanView<VertexSpan> spans) {
	drawSpans(_materialBuffer->getSet().get(), buf, spans);
}

void VertexPassHandle::recordFrameCapture(CommandBuffer &buf) {
	if (!_captureInput || !_captureSource) {
		return;
	}

	/* The layout the pass left the source in. RenderPass::perform has already written its output
	barriers by the time this runs, so this is what the image is in right now.

	Restoring it afterwards is not tidiness: the next frame's partial redraw begins its render pass
	with initialLayout = PRESENT_SRC and loadOp = LOAD (see XLVkRenderPass.cc, Variant::Load), so a
	presented image left in TRANSFER_SRC would make the next frame load from a layout the image is
	not in. */
	const VkImageLayout sourceLayout = _captureSourcePresented
			? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
			: VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

	Vector<ImageMemoryBarrier> barriers;
	barriers.reserve(_captureInput->regions.size() + 1);

	if (sourceLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
		barriers.emplace_back(ImageMemoryBarrier(_captureSource,
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, sourceLayout,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
	}

	for (auto &it : _captureInput->regions) {
		// UNDEFINED rather than the SHADER_READ_ONLY the target actually holds: the copy overwrites
		// every pixel of it, so there is nothing to preserve. A target is written exactly once and
		// nothing samples it before its capture completes, so there is no reader to race with.
		barriers.emplace_back(ImageMemoryBarrier(static_cast<Image *>(it.target.get()),
				VkAccessFlags(0), VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
	}

	buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT, 0, barriers);

	for (auto &it : _captureInput->regions) {
		VkImageCopy copy{};
		copy.srcSubresource = VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		copy.srcOffset = VkOffset3D{int32_t(it.src.x), int32_t(it.src.y), 0};
		copy.dstSubresource = VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		copy.dstOffset = VkOffset3D{0, 0, 0};
		copy.extent = VkExtent3D{it.src.width, it.src.height, 1};

		buf.cmdCopyImage(_captureSource, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				static_cast<Image *>(it.target.get()), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copy);
	}

	barriers.clear();

	if (sourceLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
		barriers.emplace_back(ImageMemoryBarrier(_captureSource, VK_ACCESS_TRANSFER_READ_BIT,
				VkAccessFlags(0), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, sourceLayout));
	}

	for (auto &it : _captureInput->regions) {
		barriers.emplace_back(ImageMemoryBarrier(static_cast<Image *>(it.target.get()),
				VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
	}

	buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
			barriers);
}

void VertexPassHandle::clearDynamicState(CommandBuffer &buf) {
	auto currentExtent = getFramebuffer()->getExtent();

	VkViewport viewport{
		0.0f,
		0.0f,
		float(currentExtent.width),
		float(currentExtent.height),
		0.0f,
		1.0f,
	};
	buf.cmdSetViewport(0, makeSpanView(&viewport, 1));

	VkRect2D scissorRect{{0, 0}, {currentExtent.width, currentExtent.height}};
	buf.cmdSetScissor(0, makeSpanView(&scissorRect, 1));

	_dynamicStateId = maxOf<StateId>();
	_dynamicState = DrawStateValues();
}

void VertexPassHandle::applyDynamicState(const FrameContextHandle2d *commands, CommandBuffer &buf,
		uint32_t stateId) {
	if (stateId == _dynamicStateId) {
		return;
	}

	auto currentExtent = getFramebuffer()->getExtent();
	auto state = commands->getState(stateId);
	//log::source().verbose("VertexPassHandle", (void *)this, " enable state: ", stateId, " ", (void *)state);
	if (!state) {
		if (_dynamicState.isScissorEnabled()) {
			_dynamicState.enabled &= ~(core::DynamicState::Scissor);
			VkRect2D scissorRect{{0, 0}, {currentExtent.width, currentExtent.height}};
			buf.cmdSetScissor(0, makeSpanView(&scissorRect, 1));
		}
	} else {
		if (state->isScissorEnabled()) {
			if (_dynamicState.isScissorEnabled()) {
				if (_dynamicState.scissor != state->scissor) {
					auto scissorRect = rotateScissor(_constraints, state->scissor);
					buf.cmdSetScissor(0, makeSpanView(&scissorRect, 1));
					_dynamicState.scissor = state->scissor;
				}
			} else {
				_dynamicState.enabled |= core::DynamicState::Scissor;
				auto scissorRect = rotateScissor(_constraints, state->scissor);
				buf.cmdSetScissor(0, makeSpanView(&scissorRect, 1));
				_dynamicState.scissor = state->scissor;
			}
		} else {
			if (_dynamicState.isScissorEnabled()) {
				_dynamicState.enabled &= ~(core::DynamicState::Scissor);
				VkRect2D scissorRect{{0, 0}, {currentExtent.width, currentExtent.height}};
				buf.cmdSetScissor(0, makeSpanView(&scissorRect, 1));
			}
		}
	}

	_dynamicStateId = stateId;
}

} // namespace stappler::xenolith::basic2d::vk
