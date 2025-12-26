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
#include "XLCoreAttachment.h"
#include "XLCoreEnum.h"
#include "XLCoreFrameHandle.h"
#include "XLCoreFrameQueue.h"
#include "XLDirector.h"
#include "XLVkDeviceQueue.h"
#include "XLVkRenderPass.h"
#include "XLVkTextureSet.h"
#include "XLVkPipeline.h"
#include "XL2dFrameContext.h"
#include "XLLinearGradient.h"
#include "backend/vk/XL2dVkParticlePass.h"
#include "glsl/include/XL2dGlslVertexData.h"
#include <vulkan/vulkan_core.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::vk {

struct VertexMaterialVertexProcessor;

struct VertexMaterialWriteTarget {
	TransformData *transform = nullptr;
	uint8_t *vertexes = nullptr;
	uint8_t *indexes = nullptr;

	uint32_t vertexOffset = 0;
	uint32_t indexOffset = 0;
	uint32_t transtormOffset = 0;
};

struct VertexMaterialDynamicData : public InterfaceObject<memory::PoolInterface>,
								   public memory::PoolInterface::AllocBaseType {
	using VertexProcessor = VertexMaterialVertexProcessor;
	using WriteTarget = VertexMaterialWriteTarget;

	struct VertexDataPlanInfo : public memory::PoolInterface::AllocBaseType {
		VertexDataPlanInfo *next = nullptr;
		SpanView<InstanceVertexData> vertexes;
		SpanView<ZOrder> zOrder;
		float depthValue = 0.0f;

		uint32_t vertexOffset = 0;
		uint32_t vertexCount = 0;
		uint32_t transformOffset = 0;
		uint32_t transformCount = 0;
	};

	struct StatePlanInfo {
		const StateData *stateData = nullptr;

		VertexDataPlanInfo *instanced = nullptr;
		VertexDataPlanInfo *packed = nullptr;

		Vector<const CmdParticleEmitter *> particles;

		uint32_t gradientStart = 0;
		uint32_t gradientCount = 0;
	};

	struct MaterialWritePlan {
		const core::Material *material = nullptr;
		Rc<core::DataAtlas> atlas;
		uint32_t vertexes = 0;
		uint32_t indexes = 0;
		uint32_t transforms = 0;
		uint32_t instances = 0;
		Map<StateId, StatePlanInfo> states;
	};

	Map<SpanView<ZOrder>, float, ZOrderLess> paths;

	// fill write plan
	MaterialWritePlan globalWritePlan;

	// write plan for objects, that do depth-write and can be drawn out of order
	Map<core::MaterialId, MaterialWritePlan> solidWritePlan;

	// write plan for objects without depth-write, that can be drawn out of order
	Map<core::MaterialId, MaterialWritePlan> surfaceWritePlan;

	// write plan for transparent objects, that should be drawn in order
	Map<SpanView<ZOrder>, Map<core::MaterialId, MaterialWritePlan>, ZOrderLess>
			transparentWritePlan;

	Extent3 surfaceExtent;
	core::SurfaceTransformFlags transform = core::SurfaceTransformFlags::Identity;
	Vec2 shadowSize = Vec2(1.0f, 1.0f);
	bool hasGpuSideAtlases = false;

	uint32_t excludeVertexes = 0;
	uint32_t excludeIndexes = 0;
	float maxShadowValue = 0.0f;

	memory::pool_t *pool = nullptr;

	StatePlanInfo *acquireStatePlan(FrameContextHandle2d *input, const core::Material *material,
			Map<core::MaterialId, MaterialWritePlan> &writePlan, const CmdInfo *c);

	void emplaceWritePlan(FrameContextHandle2d *input, const core::Material *material,
			Map<core::MaterialId, MaterialWritePlan> &writePlan, const Command *c,
			const CmdInfo *cmd, SpanView<InstanceVertexData> vertexes);

	void applyNormalized(SpanView<InstanceVertexData> &vertexes, const CmdDeferred *cmd);
	void pushVertexData(VertexProcessor *, const Command *c, const CmdVertexArray *cmd);
	void pushDeferred(VertexProcessor *, const Command *c, const CmdDeferred *cmd);
	void pushParticleEmitter(VertexProcessor *, const Command *c, const CmdParticleEmitter *cmd);

	void updatePathsDepth();

	void pushInitial(WriteTarget &writeTarget);
	void pushPlanVertexes(WriteTarget &writeTarget,
			Map<core::MaterialId, MaterialWritePlan> &writePlan);
	void drawWritePlan(VertexProcessor *processor, WriteTarget &writeTarget,
			Map<core::MaterialId, MaterialWritePlan> &writePlan);
	void pushAll(VertexProcessor *, WriteTarget &writeTarget);
};

struct VertexMaterialVertexProcessor : public Ref {
	using DynamicData = VertexMaterialDynamicData;
	using WriteTarget = VertexMaterialWriteTarget;

	uint32_t solidCmds = 0;
	uint32_t surfaceCmds = 0;
	uint32_t transparentCmds = 0;
	uint32_t shadowsCmds = 0;

	Vector<VertexSpan> materialSpans;
	Vector<VertexSpan> shadowSolidSpans;
	Vector<VertexSpan> shadowSdfSpans;

	uint64_t _time = 0;

	Rc<Buffer> _indexes;
	Rc<Buffer> _vertexes;
	Rc<Buffer> _transforms;

	VertexAttachmentHandle *_attachment = nullptr;
	Rc<FrameContextHandle2d> _input;
	Function<void(bool)> _callback;
	DrawStat _drawStat;

	VertexMaterialVertexProcessor(VertexAttachmentHandle *, Rc<FrameContextHandle2d> &&,
			Function<void(bool)> &&cb);

	void run(core::FrameHandle &frame);

	bool loadVertexes(core::FrameHandle &frame);

	void finalize(DynamicData *data);
};

VertexMaterialVertexProcessor::VertexMaterialVertexProcessor(VertexAttachmentHandle *a,
		Rc<FrameContextHandle2d> &&input, Function<void(bool)> &&cb)
: _attachment(a), _input(sp::move(input)), _callback(sp::move(cb)) {
	_time = sp::platform::clock(ClockType::Monotonic);
}

void VertexMaterialVertexProcessor::run(core::FrameHandle &frame) {
	frame.performInQueue([this](core::FrameHandle &handle) {
		if (!loadVertexes(handle)) {
			_callback(false);
		}
	}, this, "VertexMaterialAttachmentHandle::submitInput");
}

bool VertexMaterialVertexProcessor::loadVertexes(core::FrameHandle &fhandle) {
	auto handle = dynamic_cast<DeviceFrameHandle *>(&fhandle);
	if (!handle) {
		return false;
	}

	auto pool = memory::pool::create(memory::pool::acquire());
	auto ret = mem_pool::perform([&] {
		auto cache = handle->getLoop()->getFrameCache();

		_drawStat.cachedFramebuffers = uint32_t(cache->getFramebuffersCount());
		_drawStat.cachedImages = uint32_t(cache->getImagesCount());
		_drawStat.cachedImageViews = uint32_t(cache->getImageViewsCount());
		_drawStat.materials = uint32_t(_attachment->getMaterialSet()->getMaterials().size());

		auto dynamicData = new (pool) DynamicData;
		dynamicData->surfaceExtent = fhandle.getFrameConstraints().extent;
		dynamicData->transform = fhandle.getFrameConstraints().transform;
		dynamicData->hasGpuSideAtlases =
				handle->getAllocator()->getDevice()->hasDynamicIndexedBuffers();
		dynamicData->pool = pool;

		auto shadowExtent =
				_input->lights.getShadowExtent(fhandle.getFrameConstraints().getScreenSize());
		auto shadowSize =
				_input->lights.getShadowSize(fhandle.getFrameConstraints().getScreenSize());

		dynamicData->shadowSize = Vec2(shadowSize.width / float(shadowExtent.width),
				shadowSize.height / float(shadowExtent.height));

		auto cmd = _input->commands->getFirst();
		while (cmd) {
			switch (cmd->type) {
			case CommandType::CommandGroup: break;
			case CommandType::VertexArray:
				dynamicData->pushVertexData(this, cmd,
						reinterpret_cast<const CmdVertexArray *>(cmd->data));
				break;
			case CommandType::Deferred:
				dynamicData->pushDeferred(this, cmd,
						reinterpret_cast<const CmdDeferred *>(cmd->data));
				break;
			case CommandType::ParticleEmitter:
				dynamicData->pushParticleEmitter(this, cmd,
						reinterpret_cast<const CmdParticleEmitter *>(cmd->data));

				break;
			}
			cmd = cmd->next;
		}

		auto devFrame = static_cast<DeviceFrameHandle *>(handle);
		auto devPool = devFrame->getMemPool(this);

		// create buffers
		_indexes = devPool->spawn(AllocationUsage::DeviceLocalHostVisible,
				BufferInfo(StringView("IndexBuffer"), core::BufferUsage::IndexBuffer,
						(dynamicData->globalWritePlan.indexes + 12) * sizeof(uint32_t)));

		_vertexes = devPool->spawn(AllocationUsage::DeviceLocalHostVisible,
				BufferInfo(StringView("VertexBuffer"), core::BufferUsage::StorageBuffer,
						core::BufferUsage::ShaderDeviceAddress,
						(dynamicData->globalWritePlan.vertexes + 8) * sizeof(Vertex)));

		_transforms = devPool->spawn(AllocationUsage::DeviceLocalHostVisible,
				BufferInfo(StringView("TransformBuffer"), core::BufferUsage::StorageBuffer,
						core::BufferUsage::ShaderDeviceAddress,
						(_input->commands->getPredefinedTransforms()
								+ dynamicData->globalWritePlan.transforms + 1)
								* sizeof(TransformData)));

		if (!_vertexes || !_indexes || !_transforms) {
			delete dynamicData;
			return false;
		}

		Bytes vertexData, indexData, transformData, instanceData;

		WriteTarget writeTarget;
		writeTarget.transtormOffset = _input->commands->getPredefinedTransforms();

		if (fhandle.isPersistentMapping()) {
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

		if (dynamicData->globalWritePlan.vertexes == 0
				|| dynamicData->globalWritePlan.indexes == 0) {
			dynamicData->pushInitial(writeTarget);
		} else {
			dynamicData->updatePathsDepth();

			// write initial full screen quad
			dynamicData->pushAll(this, writeTarget);
		}

		if (fhandle.isPersistentMapping()) {
			_vertexes->flushMappedRegion();
			_indexes->flushMappedRegion();
			_transforms->flushMappedRegion();
		} else {
			_vertexes->setData(vertexData);
			_indexes->setData(indexData);
			_transforms->setData(transformData);
		}

		finalize(dynamicData);
		delete dynamicData;
		return true;
	}, pool);
	memory::pool::destroy(pool);
	return ret;
}

VertexMaterialDynamicData::StatePlanInfo *VertexMaterialDynamicData::acquireStatePlan(
		FrameContextHandle2d *input, const core::Material *material,
		Map<core::MaterialId, MaterialWritePlan> &writePlan, const CmdInfo *cmd) {
	auto materialIt = writePlan.find(cmd->material);
	if (materialIt == writePlan.end()) {
		if (material) {
			materialIt = writePlan.emplace(cmd->material, MaterialWritePlan()).first;
			materialIt->second.material = material;
			if (auto atlas = materialIt->second.material->getAtlas()) {
				materialIt->second.atlas = atlas;
			}
		}
	}

	if (materialIt != writePlan.end() && materialIt->second.material) {
		auto stateIt = materialIt->second.states.find(cmd->state);
		if (stateIt == materialIt->second.states.end()) {
			stateIt = materialIt->second.states.emplace(cmd->state, StatePlanInfo()).first;

			if (cmd->state != StateIdNone) {
				auto state = input->getState(cmd->state);
				if (state) {
					auto stateData =
							dynamic_cast<StateData *>(state->data ? state->data.get() : nullptr);

					if (stateData) {
						stateIt->second.stateData = stateData;
						if (stateData->gradient) {
							globalWritePlan.vertexes += stateData->gradient->steps.size() + 2;
						}
					}
				}
			}
		}

		auto pathsIt = paths.find(cmd->zPath);
		if (pathsIt == paths.end()) {
			paths.emplace(cmd->zPath, 0.0f);
		}

		return &stateIt->second;
	}
	return nullptr;
}
void VertexMaterialDynamicData::emplaceWritePlan(FrameContextHandle2d *input,
		const core::Material *material, Map<core::MaterialId, MaterialWritePlan> &writePlan,
		const Command *c, const CmdInfo *cmd, SpanView<InstanceVertexData> vertexes) {
	auto statePlan = acquireStatePlan(input, material, writePlan, cmd);

	if (statePlan) {
		InstanceVertexData *packedStart = const_cast<InstanceVertexData *>(vertexes.data());
		size_t packedCommands = 0;

		for (auto &vIt : vertexes) {
			// count data objects
			globalWritePlan.vertexes += vIt.data->data.size();
			globalWritePlan.indexes += vIt.data->indexes.size();

			if (vIt.sdfIndexes > 0) {
				globalWritePlan.indexes += (vIt.sdfIndexes + vIt.fillIndexes);
			}

			if ((c->flags & CommandFlags::DoNotCount) != CommandFlags::None) {
				excludeVertexes += vIt.data->data.size();
				excludeIndexes += vIt.data->indexes.size();
			}

			maxShadowValue = std::max(maxShadowValue, cmd->depthValue);

			bool drawAsInstances = false;
			if (vIt.instances.size() > 1) {
				drawAsInstances = true;
			}

			// pack non-instanced blocks
			if (drawAsInstances) {
				globalWritePlan.transforms += vIt.instances.size();

				if (packedCommands > 0) {
					// write packed blocks
					auto vertexData = new (pool) VertexDataPlanInfo;
					vertexData->next = statePlan->packed;
					vertexData->vertexes = makeSpanView(packedStart, packedCommands);
					vertexData->zOrder = cmd->zPath;
					vertexData->depthValue = cmd->depthValue;
					statePlan->packed = vertexData;
				}

				auto vertexData = new (pool) VertexDataPlanInfo;
				vertexData->next = statePlan->instanced;
				vertexData->vertexes = makeSpanView(&vIt, 1);
				vertexData->zOrder = cmd->zPath;
				vertexData->depthValue = cmd->depthValue;
				statePlan->instanced = vertexData;

				packedCommands = 0;
				packedStart = const_cast<InstanceVertexData *>(&vIt + 1);
			} else {
				++globalWritePlan.transforms;
				++packedCommands;
			}
		}

		if (packedCommands > 0) {
			// write packed blocks
			auto vertexData = new (pool) VertexDataPlanInfo;
			vertexData->next = statePlan->packed;
			vertexData->vertexes = makeSpanView(packedStart, packedCommands);
			vertexData->zOrder = cmd->zPath;
			vertexData->depthValue = cmd->depthValue;
			statePlan->packed = vertexData;
		}
	}
}

void VertexMaterialDynamicData::pushVertexData(VertexProcessor *processor, const Command *c,
		const CmdVertexArray *cmd) {
	auto material = processor->_attachment->getMaterialSet()->getMaterialById(cmd->material);
	if (!material) {
		return;
	}
	if (material->getPipeline()->isSolid()) {
		emplaceWritePlan(processor->_input, material, solidWritePlan, c, cmd, cmd->vertexes);
	} else if (cmd->renderingLevel == RenderingLevel::Surface) {
		emplaceWritePlan(processor->_input, material, surfaceWritePlan, c, cmd, cmd->vertexes);
	} else {
		auto v = transparentWritePlan.find(cmd->zPath);
		if (v == transparentWritePlan.end()) {
			v = transparentWritePlan.emplace(cmd->zPath, Map<core::MaterialId, MaterialWritePlan>())
						.first;
		}
		emplaceWritePlan(processor->_input, material, v->second, c, cmd, cmd->vertexes);
	}
};

void VertexMaterialDynamicData::applyNormalized(SpanView<InstanceVertexData> &vertexes,
		const CmdDeferred *cmd) {
	// apply transforms;
	if (cmd->normalized) {
		for (auto &it : vertexes) {
			if (it.instances.size() > 0) {
				const_cast<SpanView<TransformData> &>(it.instances) = it.instances.pdup();
			} else {
				TransformData instance;
				instance.transform = Mat4::IDENTITY;
				const_cast<SpanView<TransformData> &>(it.instances) =
						makeSpanView(&instance, 1).pdup();
			}
			for (auto &inst : it.instances) {
				auto modelTransform = cmd->modelTransform * inst.transform;

				Mat4 newMV;
				newMV.m[12] = std::floor(modelTransform.m[12]);
				newMV.m[13] = std::floor(modelTransform.m[13]);
				newMV.m[14] = std::floor(modelTransform.m[14]);

				const_cast<TransformData &>(inst).transform = cmd->viewTransform * newMV;
			}
		}
	} else {
		for (auto &it : vertexes) {
			if (it.instances.size() > 0) {
				const_cast<SpanView<TransformData> &>(it.instances) = it.instances.pdup();
			} else {
				TransformData instance;
				instance.transform = Mat4::IDENTITY;
				const_cast<SpanView<TransformData> &>(it.instances) =
						makeSpanView(&instance, 1).pdup();
			}
			for (auto &inst : it.instances) {
				const_cast<TransformData &>(inst).transform =
						cmd->viewTransform * cmd->modelTransform * inst.transform;
			}
		}
	}
}

void VertexMaterialDynamicData::pushDeferred(VertexProcessor *processor, const Command *c,
		const CmdDeferred *cmd) {
	auto material = processor->_attachment->getMaterialSet()->getMaterialById(cmd->material);
	if (!material) {
		return;
	}

	if (!cmd->deferred->isWaitOnReady()) {
		if (!cmd->deferred->isReady()) {
			return;
		}
	}

	SpanView<InstanceVertexData> storedVertexes;

	cmd->deferred->acquireResult(
			[&](SpanView<InstanceVertexData> vertexes, DeferredVertexResult::Flags flags) {
		auto v = vertexes.pdup();
		applyNormalized(v, cmd);
		storedVertexes = v;
	});

	if (cmd->renderingLevel == RenderingLevel::Solid) {
		emplaceWritePlan(processor->_input, material, solidWritePlan, c, cmd, storedVertexes);
	} else if (cmd->renderingLevel == RenderingLevel::Surface) {
		emplaceWritePlan(processor->_input, material, surfaceWritePlan, c, cmd, storedVertexes);
	} else {
		auto v = transparentWritePlan.find(cmd->zPath);
		if (v == transparentWritePlan.end()) {
			v = transparentWritePlan.emplace(cmd->zPath, Map<core::MaterialId, MaterialWritePlan>())
						.first;
		}
		emplaceWritePlan(processor->_input, material, v->second, c, cmd, storedVertexes);
	}
}

void VertexMaterialDynamicData::pushParticleEmitter(VertexProcessor *processor, const Command *c,
		const CmdParticleEmitter *cmd) {
	auto material = processor->_attachment->getMaterialSet()->getMaterialById(cmd->material);
	if (!material) {
		return;
	}

	auto emplacePlan = [&](Map<core::MaterialId, MaterialWritePlan> &writePlan) {
		auto statePlan = acquireStatePlan(processor->_input, material, writePlan, cmd);
		if (statePlan) {
			statePlan->particles.emplace_back(cmd);
		}
	};

	if (material->getPipeline()->isSolid()) {
		emplacePlan(solidWritePlan);
	} else if (cmd->renderingLevel == RenderingLevel::Surface) {
		emplacePlan(surfaceWritePlan);
	} else {
		auto v = transparentWritePlan.find(cmd->zPath);
		if (v == transparentWritePlan.end()) {
			v = transparentWritePlan.emplace(cmd->zPath, Map<core::MaterialId, MaterialWritePlan>())
						.first;
		}
		emplacePlan(v->second);
	}
}

void VertexMaterialDynamicData::updatePathsDepth() {
	float depthScale = 1.0f / float(paths.size() + 1);
	float depthOffset = 1.0f - depthScale;
	for (auto &it : paths) {
		it.second = depthOffset;
		depthOffset -= depthScale;
	}
}

void VertexMaterialDynamicData::pushInitial(WriteTarget &writeTarget) {
	if (writeTarget.transform) {
		TransformData nullTransforml;
		nullTransforml.offset = Vec4::ZERO;
		memcpy(writeTarget.transform, &nullTransforml, sizeof(TransformData));
		++writeTarget.transtormOffset;
	}

	if (writeTarget.indexes) {
		Vector<uint32_t> indexes{0, 2, 1, 0, 3, 2, 4, 6, 5, 4, 7, 6};
		memcpy(writeTarget.indexes, indexes.data(), indexes.size() * sizeof(uint32_t));
		writeTarget.indexOffset += indexes.size();
	}

	if (writeTarget.vertexes) {
		Vector<Vertex> vertexes{// full screen quad data
			Vertex{Vec4(-1.0f, -1.0f, 0.0f, 1.0f), Vec4::ONE, Vec2::ZERO, 0, 0},
			Vertex{Vec4(-1.0f, 1.0f, 0.0f, 1.0f), Vec4::ONE, Vec2::UNIT_Y, 0, 0},
			Vertex{Vec4(1.0f, 1.0f, 0.0f, 1.0f), Vec4::ONE, Vec2::ONE, 0, 0},
			Vertex{Vec4(1.0f, -1.0f, 0.0f, 1.0f), Vec4::ONE, Vec2::UNIT_X, 0, 0},

			// shadow quad data
			Vertex{Vec4(-1.0f, -1.0f, 0.0f, 1.0f), Vec4::ONE, Vec2(0.0f, 1.0f - shadowSize.y), 0,
				0},
			Vertex{Vec4(-1.0f, 1.0f, 0.0f, 1.0f), Vec4::ONE, Vec2(0.0f, 1.0f), 0, 0},
			Vertex{Vec4(1.0f, 1.0f, 0.0f, 1.0f), Vec4::ONE, Vec2(shadowSize.x, 1.0f), 0, 0},
			Vertex{Vec4(1.0f, -1.0f, 0.0f, 1.0f), Vec4::ONE,
				Vec2(shadowSize.x, 1.0f - shadowSize.y), 0, 0}};

		switch (core::getPureTransform(transform)) {
		case core::SurfaceTransformFlags::Rotate90:
			vertexes[0].tex = Vec2::UNIT_Y;
			vertexes[1].tex = Vec2::ONE;
			vertexes[2].tex = Vec2::UNIT_X;
			vertexes[3].tex = Vec2::ZERO;
			vertexes[4].tex = Vec2(0.0f, shadowSize.y);
			vertexes[5].tex = shadowSize;
			vertexes[6].tex = Vec2(shadowSize.x, 0.0f);
			vertexes[7].tex = Vec2::ZERO;
			break;
		case core::SurfaceTransformFlags::Rotate180:
			vertexes[0].tex = Vec2::ONE;
			vertexes[1].tex = Vec2::UNIT_X;
			vertexes[2].tex = Vec2::ZERO;
			vertexes[3].tex = Vec2::UNIT_Y;
			vertexes[4].tex = shadowSize;
			vertexes[5].tex = Vec2(shadowSize.x, 0.0f);
			vertexes[6].tex = Vec2::ZERO;
			vertexes[7].tex = Vec2(0.0f, shadowSize.y);
			break;
		case core::SurfaceTransformFlags::Rotate270:
			vertexes[0].tex = Vec2::UNIT_X;
			vertexes[1].tex = Vec2::ZERO;
			vertexes[2].tex = Vec2::UNIT_Y;
			vertexes[3].tex = Vec2::ONE;
			vertexes[4].tex = Vec2(shadowSize.x, 0.0f);
			vertexes[5].tex = Vec2::ZERO;
			vertexes[6].tex = Vec2(0.0f, shadowSize.y);
			vertexes[7].tex = shadowSize;
			break;
		default: break;
		}

		memcpy(writeTarget.vertexes, vertexes.data(), vertexes.size() * sizeof(Vertex));
		writeTarget.vertexOffset += vertexes.size();
	}
}

void VertexMaterialDynamicData::pushPlanVertexes(WriteTarget &writeTarget,
		Map<core::MaterialId, MaterialWritePlan> &writePlan) {
	auto pushVertexes = [&](core::MaterialId materialId, const MaterialWritePlan &plan,
								uint32_t transform, const InstanceVertexData &vertexes) {
		auto target = reinterpret_cast<Vertex *>(writeTarget.vertexes) + writeTarget.vertexOffset;
		memcpy(target, vertexes.data->data.data(), vertexes.data->data.size() * sizeof(Vertex));

		size_t idx = 0;
		if (plan.atlas) {
			if (hasGpuSideAtlases) {
				for (; idx < vertexes.data->data.size(); ++idx) {
					target[idx].material = materialId | transform << 16;
				}
			} else {
				auto ext = plan.atlas->getImageExtent();
				float atlasScaleX = 1.0f / ext.width;
				float atlasScaleY = 1.0f / ext.height;

				for (; idx < vertexes.data->data.size(); ++idx) {
					auto &t = target[idx];
					t.material = materialId | transform << 16;

					struct AtlasData {
						Vec2 pos;
						Vec2 tex;
					};

					if (auto d = reinterpret_cast<const AtlasData *>(
								plan.atlas->getObjectByName(t.object))) {
						t.pos += Vec4(d->pos.x, d->pos.y, 0, 0);
						t.tex = d->tex;
						t.object = 0;
					} else {
#if DEBUG
						log::source().warn("VertexMaterialDrawPlan", "Object not found: ", t.object,
								" ", string::toUtf8<Interface>(char16_t(t.object)));
#endif
						auto anchor = font::CharId::getAnchorForChar(t.object);
						switch (anchor) {
						case font::CharAnchor::BottomLeft:
							t.tex = Vec2(1.0f - atlasScaleX, 0.0f);
							break;
						case font::CharAnchor::TopLeft:
							t.tex = Vec2(1.0f - atlasScaleX, 0.0f + atlasScaleY);
							break;
						case font::CharAnchor::TopRight:
							t.tex = Vec2(1.0f, 0.0f + atlasScaleY);
							break;
						case font::CharAnchor::BottomRight: t.tex = Vec2(1.0f, 0.0f); break;
						}
					}
				}
			}
		} else {
			for (; idx < vertexes.data->data.size(); ++idx) {
				//target[idx].pos = transform.transform * target[idx].pos * transform.mask + transform.offset;
				target[idx].material = materialId | transform << 16;
			}
		}

		writeTarget.vertexOffset += vertexes.data->data.size();
	};

	auto writeTransform = [&](const TransformData &inst, float zOffset, float depthValue,
								  const StateData *stateData, uint32_t preTransform) -> uint32_t {
		auto ret = preTransform ? preTransform : writeTarget.transtormOffset;
		auto instanceTarget = writeTarget.transform + ret;
		memcpy(instanceTarget, &inst, sizeof(TransformData));
		instanceTarget->offset.z = zOffset;
		instanceTarget->shadowValue = depthValue;
		if (stateData) {
			instanceTarget->outlineColor = stateData->outlineColor;
			instanceTarget->outlineOffset = stateData->outlineOffset;
		} else {
			instanceTarget->outlineOffset = 0.0f;
		}

		if (!preTransform) {
			++writeTarget.transtormOffset;
		}
		return ret;
	};

	auto pushVertexList = [&](core::MaterialId mId, MaterialWritePlan &plan,
								  const StatePlanInfo &state, VertexDataPlanInfo *packedInstance,
								  bool instances) {
		while (packedInstance) {
			packedInstance->vertexOffset = writeTarget.vertexOffset;

			// used as firstInstance for instanced drawing to access transform array
			packedInstance->transformOffset = writeTarget.transtormOffset;

			float zOffset = 0.0f;
			float depthValue = 0.0f;
			auto pathIt = paths.find(packedInstance->zOrder);
			if (pathIt != paths.end()) {
				zOffset = pathIt->second;
			}

			if (packedInstance->depthValue > 0.0f) {
				auto f16 = halffloat::encode(packedInstance->depthValue);
				auto value = halffloat::decode(f16);
				depthValue = value;
			}

			for (auto &iit : packedInstance->vertexes) {
				if (instances) {
					for (auto &inst : iit.instances) {
						writeTransform(inst, zOffset, depthValue, state.stateData, 0);
					}

					pushVertexes(mId, plan, 0, iit);
				} else {
					auto transform = writeTransform(iit.instances.front(), zOffset, depthValue,
							state.stateData, 0);
					pushVertexes(mId, plan, transform, iit);
				}
			}

			packedInstance->vertexCount = writeTarget.vertexOffset - packedInstance->vertexOffset;
			packedInstance->transformCount =
					writeTarget.transtormOffset - packedInstance->transformOffset;

			packedInstance = packedInstance->next;
		}
	};

	for (auto &plan : writePlan) {
		for (auto &state : plan.second.states) {
			// write gradient vertexes (2 + n: start, end, anchors)
			if (state.second.stateData && state.second.stateData->gradient) {
				auto target =
						reinterpret_cast<Vertex *>(writeTarget.vertexes) + writeTarget.vertexOffset;

				Vec2 start =
						state.second.stateData->transform * state.second.stateData->gradient->start;
				Vec2 end =
						state.second.stateData->transform * state.second.stateData->gradient->end;

				start.y = surfaceExtent.height - start.y;
				end.y = surfaceExtent.height - end.y;

				Vec2 norm = end - start;

				float d = norm.y * norm.y / (norm.x * norm.x + norm.y * norm.y);

				Vec2 axisAngle;
				if (std::abs(norm.y) > std::abs(norm.x)) {
					axisAngle.x = std::copysign(norm.length(), norm.y);
					axisAngle.y = d;
				} else {
					axisAngle.x = std::copysign(norm.length(), norm.x);
					axisAngle.y = d;
				}

				target->pos = Vec4(start, 0.0f, 0.0f);
				target->tex = axisAngle;
				++target;

				target->pos = Vec4(end, 1.0f, 0.0f);
				target->tex = axisAngle;
				++target;

				for (auto &it : state.second.stateData->gradient->steps) {
					target->pos = Vec4(math::lerp(start, end, it.value), it.value, it.factor);
					target->tex = axisAngle;
					target->color = Vec4(it.color.r, it.color.g, it.color.b, it.color.a);
					++target;
				}

				state.second.gradientStart = writeTarget.vertexOffset;
				state.second.gradientCount =
						uint32_t(state.second.stateData->gradient->steps.size());

				writeTarget.vertexOffset += state.second.stateData->gradient->steps.size() + 2;
			}

			pushVertexList(plan.first, plan.second, state.second, state.second.instanced, true);
			pushVertexList(plan.first, plan.second, state.second, state.second.packed, false);

			for (auto &it : state.second.particles) {
				TransformData inst(it->transform);

				float zOffset = 0.0f;
				float depthValue = 0.0f;
				auto pathIt = paths.find(it->zPath);
				if (pathIt != paths.end()) {
					zOffset = pathIt->second;
				}

				if (it->depthValue > 0.0f) {
					auto f16 = halffloat::encode(it->depthValue);
					auto value = halffloat::decode(f16);
					depthValue = value;
				}

				writeTransform(inst, zOffset, depthValue, state.second.stateData,
						it->transformIndex);
			}
		}
	}
}

void VertexMaterialDynamicData::drawWritePlan(VertexProcessor *processor, WriteTarget &writeTarget,
		Map<core::MaterialId, MaterialWritePlan> &writePlan) {
	// optimize draw order, minimize switching pipeline, textureSet and descriptors
	Vector<const Pair<const core::MaterialId, MaterialWritePlan> *> drawOrder;

	// optimize pipeline switching strategy
	for (auto &it : writePlan) {
		if (drawOrder.empty()) {
			drawOrder.emplace_back(&it);
		} else {
			auto lb = std::lower_bound(drawOrder.begin(), drawOrder.end(), &it,
					[](const Pair<const core::MaterialId, MaterialWritePlan> *l,
							const Pair<const core::MaterialId, MaterialWritePlan> *r) {
				if (l->second.material->getPipeline() != l->second.material->getPipeline()) {
					return GraphicPipeline::comparePipelineOrdering(
							*l->second.material->getPipeline(), *r->second.material->getPipeline());
				} else if (l->second.material->getLayoutIndex()
						!= r->second.material->getLayoutIndex()) {
					return l->second.material->getLayoutIndex()
							< r->second.material->getLayoutIndex();
				} else {
					return l->first < r->first;
				}
			});
			if (lb == drawOrder.end()) {
				drawOrder.emplace_back(&it);
			} else {
				drawOrder.emplace(lb, &it);
			}
		}
	}

	auto writeIndexes = [](uint32_t *indexTarget, const uint32_t *indexSource, uint32_t indexCount,
								uint32_t vertexOffset) {
		if (vertexOffset == 0) {
			memcpy(indexTarget, indexSource, indexCount * sizeof(uint32_t));
		} else {
			for (size_t i = 0; i < indexCount; ++i) {
				*(indexTarget++) = *(indexSource++) + vertexOffset;
			}
		}
		return indexCount;
	};

	enum StatePlanPhase {
		StatePlanGeneral,
		StatePlanShadowSolid,
		StatePlanShadowVolumes
	};

	auto processStatePlanIndexes = [&](const InstanceVertexData &vertexes, StatePlanPhase phase,
										   uint32_t localVertexOffset) {
		switch (phase) {
		case StatePlanGeneral:
			writeTarget.indexOffset += writeIndexes(
					reinterpret_cast<uint32_t *>(writeTarget.indexes) + writeTarget.indexOffset,
					vertexes.data->indexes.data(),
					uint32_t(vertexes.data->indexes.size() - vertexes.sdfIndexes),
					localVertexOffset);
			break;
		case StatePlanShadowSolid:
			if (vertexes.sdfIndexes > 0 && vertexes.fillIndexes > 0) {
				writeTarget.indexOffset += writeIndexes(
						reinterpret_cast<uint32_t *>(writeTarget.indexes) + writeTarget.indexOffset,
						vertexes.data->indexes.data(), vertexes.fillIndexes, localVertexOffset);
			}
			break;
		case StatePlanShadowVolumes:
			if (vertexes.sdfIndexes > 0) {
				writeTarget.indexOffset += writeIndexes(
						reinterpret_cast<uint32_t *>(writeTarget.indexes) + writeTarget.indexOffset,
						vertexes.data->indexes.data() + vertexes.fillIndexes
								+ vertexes.strokeIndexes,
						vertexes.sdfIndexes, localVertexOffset);
			}
			break;
		}
	};

	auto processStatePlan = [&](core::MaterialId materialId, StateId stateId,
									const StatePlanInfo &statePlan, StatePlanPhase phase,
									mem_std::Vector<VertexSpan> &target) {
		size_t localVertexOffset = 0;
		auto materialIndexes = writeTarget.indexOffset;

		auto packedInstance = statePlan.instanced;
		while (packedInstance) {
			for (auto &vertexes : packedInstance->vertexes) {
				processStatePlanIndexes(vertexes, phase, 0);
				if (writeTarget.indexOffset > materialIndexes) {
					target.emplace_back(VertexSpan{.material = materialId,
						.indexCount = writeTarget.indexOffset - materialIndexes,
						.instanceCount = packedInstance->transformCount,
						.firstIndex = materialIndexes,
						.vertexOffset = packedInstance->vertexOffset,
						.firstInstance = packedInstance->transformOffset,
						.state = stateId,
						.gradientOffset = statePlan.gradientStart,
						.gradientCount = statePlan.gradientCount,
						.outlineOffset =
								(statePlan.stateData ? statePlan.stateData->outlineOffset : 0.0f)});
				}
				materialIndexes = writeTarget.indexOffset;
			}
			packedInstance = packedInstance->next;
		}

		materialIndexes = writeTarget.indexOffset;
		packedInstance = statePlan.packed;
		while (packedInstance) {
			for (auto &vertexes : packedInstance->vertexes) {
				processStatePlanIndexes(vertexes, phase, uint32_t(localVertexOffset));
				localVertexOffset += vertexes.data->data.size();
			}
			packedInstance = packedInstance->next;
		}

		if (writeTarget.indexOffset > materialIndexes) {
			target.emplace_back(VertexSpan{.material = materialId,
				.indexCount = writeTarget.indexOffset - materialIndexes,
				.instanceCount = 1,
				.firstIndex = materialIndexes,
				.vertexOffset = statePlan.packed->vertexOffset,
				.firstInstance = 0,
				.state = stateId,
				.gradientOffset = statePlan.gradientStart,
				.gradientCount = statePlan.gradientCount,
				.outlineOffset =
						(statePlan.stateData ? statePlan.stateData->outlineOffset : 0.0f)});
		}

		// do not draw shadows for a particles for now
		if (phase == StatePlanPhase::StatePlanGeneral) {
			for (auto &it : statePlan.particles) {
				target.emplace_back(VertexSpan{.material = materialId,
					.indexCount = 0,
					.instanceCount = 1,
					.firstIndex = 0,
					.vertexOffset = 0,
					.firstInstance = 0,
					.state = stateId,
					.gradientOffset = statePlan.gradientStart,
					.gradientCount = statePlan.gradientCount,
					.outlineOffset =
							(statePlan.stateData ? statePlan.stateData->outlineOffset : 0.0f),
					.particleSystemId = it->id});
			}
		}
	};

	// General drawing
	for (auto &it : drawOrder) {
		for (auto &state : it->second.states) {
			processStatePlan(it->first, state.first, state.second, StatePlanGeneral,
					processor->materialSpans);
		}
	}

	// Shadow solids
	for (auto &it : drawOrder) {
		for (auto &state : it->second.states) {
			processStatePlan(it->first, state.first, state.second, StatePlanShadowSolid,
					processor->shadowSolidSpans);
		}
	}

	// Shadow volumes
	for (auto &it : drawOrder) {
		for (auto &state : it->second.states) {
			processStatePlan(it->first, state.first, state.second, StatePlanShadowVolumes,
					processor->shadowSdfSpans);
		}
	}
}

void VertexMaterialDynamicData::pushAll(VertexProcessor *processor, WriteTarget &writeTarget) {
	pushInitial(writeTarget);
	pushPlanVertexes(writeTarget, solidWritePlan);
	pushPlanVertexes(writeTarget, surfaceWritePlan);
	for (auto &it : transparentWritePlan) { pushPlanVertexes(writeTarget, it.second); }

	uint32_t counter = 0;
	drawWritePlan(processor, writeTarget, solidWritePlan);

	processor->solidCmds = uint32_t(processor->materialSpans.size() - counter);
	counter = uint32_t(processor->materialSpans.size());

	drawWritePlan(processor, writeTarget, surfaceWritePlan);

	processor->surfaceCmds = uint32_t(processor->materialSpans.size() - counter);
	counter = uint32_t(processor->materialSpans.size());

	for (auto &it : transparentWritePlan) { drawWritePlan(processor, writeTarget, it.second); }

	processor->transparentCmds = uint32_t(processor->materialSpans.size() - counter);
}

void VertexMaterialVertexProcessor::finalize(DynamicData *data) {
	auto t = sp::platform::clock(ClockType::Monotonic);
	_drawStat.vertexes = data->globalWritePlan.vertexes - data->excludeVertexes;
	_drawStat.triangles = (data->globalWritePlan.indexes - data->excludeIndexes) / 3;
	_drawStat.zPaths = uint32_t(data->paths.size());
	_drawStat.drawCalls = uint32_t(materialSpans.size());
	_drawStat.solidCmds = solidCmds;
	_drawStat.surfaceCmds = surfaceCmds;
	_drawStat.transparentCmds = transparentCmds;
	_drawStat.shadowsCmds = shadowsCmds;
	_drawStat.vertexInputTime = uint32_t(t - _time);
	_input->director->pushDrawStat(_drawStat);

	_attachment->loadData(sp::move(_input), sp::move(_indexes), sp::move(_vertexes),
			sp::move(_transforms), sp::move(materialSpans), sp::move(shadowSolidSpans),
			sp::move(shadowSdfSpans), data->maxShadowValue);

	_callback(true);
}

bool VertexAttachment::init(AttachmentBuilder &builder, const AttachmentData *m) {
	if (core::GenericAttachment::init(builder)) {
		_materials = m;
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
		Vector<VertexSpan> &&shadowSolidSpans, Vector<VertexSpan> &&shadowSdfSpans,
		float maxShadowValue) {
	_commands = move(data);
	_indexes = move(indexes);
	_vertexes = move(vertexes);
	_transforms = move(transforms);
	_spans = sp::move(spans);
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
				_queueData->deviceTime = static_cast<uint64_t>(std::ceil(mksec));
			}
		}
	}
}

void VertexPassHandle::prepareRenderPass(CommandBuffer &buf) {
	buf.cmdWriteTimestamp(core::PipelineStage::TopOfPipe, TimestampBeginTag);
}

void VertexPassHandle::prepareMaterialCommands(core::MaterialSet *materials, CommandBuffer &buf) {
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

	auto spans = _vertexBuffer->getVertexData();

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

		pcb.materialPointer =
				UVec2::convertFromPacked(buf.bindBufferAddress(material->getBuffer()));
		pcb.imageIdx = material->getImages().front().descriptor;
		pcb.samplerIdx = material->getImages().front().sampler;
		pcb.gradientOffset = materialVertexSpan.gradientOffset;
		pcb.gradientCount = materialVertexSpan.gradientCount;
		//pcb.outlineOffset = materialVertexSpan.outlineOffset;

		if (auto a = material->getAtlas()) {
			pcb.atlasPointer =
					UVec2::convertFromPacked(buf.bindBufferAddress(a->getBuffer().get()));
		}

		auto textureSetIndex = material->getLayoutIndex();

		auto pipeline = material->getPipeline();
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
