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

#include "XLCoreQueueData.h"
#include "XLCoreQueuePass.h"
#include "SPIRV-Reflect/spirv_reflect.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

void QueueData::clear() {
	for (auto &it : programs) { it->program = nullptr; }

	for (auto &it : passes) {
		for (auto &subpass : it->subpasses) {
			for (auto &pipeline : subpass->graphicPipelines) { pipeline->pipeline = nullptr; }
			for (auto &pipeline : subpass->computePipelines) { pipeline->pipeline = nullptr; }
		}

		it->pass->invalidate();
		it->pass = nullptr;
		it->impl = nullptr;
	}

	for (auto &it : attachments) { it->attachment = nullptr; }

	for (auto &it : textureSets) {
		it->layout = nullptr;
		it->compiledSamplers.clear();
	}

	if (resource) {
		resource->clear();
		resource = nullptr;
	}
	linked.clear();
	compiled = false;

	if (releaseCallback) {
		releaseCallback();
		releaseCallback = nullptr;
	}
}


void ProgramData::inspect(SpanView<uint32_t> data) {
	SpvReflectShaderModule shader;

	spvReflectCreateShaderModule(data.size() * sizeof(uint32_t), data.data(), &shader);

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

	bindings.reserve(shader.descriptor_binding_count);
	for (auto &it : makeSpanView(shader.descriptor_bindings, shader.descriptor_binding_count)) {
		bindings.emplace_back(ProgramDescriptorBinding(
				{it.set, it.binding, DescriptorType(it.descriptor_type), it.count}));
	}

	constants.reserve(shader.push_constant_block_count);
	for (auto &it : makeSpanView(shader.push_constant_blocks, shader.push_constant_block_count)) {
		constants.emplace_back(ProgramPushConstantBlock({it.absolute_offset, it.padded_size}));
	}

	entryPoints.reserve(shader.entry_point_count);
	for (auto &it : makeSpanView(shader.entry_points, shader.entry_point_count)) {
		entryPoints.emplace_back(ProgramEntryPointBlock({it.id, memory::string(it.name),
			it.local_size.x, it.local_size.y, it.local_size.z}));
	}

	spvReflectDestroyShaderModule(&shader);
}

SpecializationInfo::SpecializationInfo(const ProgramData *d) : data(d) { }

SpecializationInfo::SpecializationInfo(const ProgramData *d, SpanView<SpecializationConstant> c)
: data(d), constants(c.vec<memory::PoolInterface>()) { }

bool GraphicPipelineInfo::isSolid() const {
	if (material.getDepthInfo().writeEnabled || !material.getBlendInfo().enabled) {
		return true;
	}
	return false;
}

} // namespace stappler::xenolith::core
