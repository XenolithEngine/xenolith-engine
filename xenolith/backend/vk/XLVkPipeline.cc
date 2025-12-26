/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "XLVkPipeline.h"
#include "SPLog.h"
#include "XLVkDevice.h"
#include "XLVkRenderPass.h"
#include "XLVkTextureSet.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

static BytesView Shader_emplaceConstant(Bytes &data, BytesView constant) {
	auto originalSize = data.size();
	auto constantSize = constant.size();
	data.resize(originalSize + constantSize);
	memcpy(data.data() + originalSize, constant.data(), constantSize);
	return BytesView(data.data() + originalSize, constantSize);
}

static BytesView Shader_emplaceConstant(Bytes &data, float constant) {
	return Shader_emplaceConstant(data, BytesView((const uint8_t *)&constant, sizeof(float)));
}

static BytesView Shader_emplaceConstant(Bytes &data, int constant) {
	return Shader_emplaceConstant(data, BytesView((const uint8_t *)&constant, sizeof(int)));
}

bool Shader::init(Device &dev, const ProgramData &data) {
	_stage = data.stage;
	_name = data.key.str<Interface>();

	if (!data.data.empty()) {
		return setup(dev, data, data.data);
	} else if (data.callback != nullptr) {
		bool ret = false;
		data.callback(dev,
				[&, this](SpanView<uint32_t> shaderData) { ret = setup(dev, data, shaderData); });
		return ret;
	}

	return false;
}

bool Shader::setup(Device &dev, const ProgramData &programData, SpanView<uint32_t> data) {
	VkShaderModuleCreateInfo createInfo{};
	sanitizeVkStruct(createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = data.size() * sizeof(uint32_t);
	createInfo.flags = 0;
	createInfo.pCode = data.data();

	if (dev.getTable()->vkCreateShaderModule(dev.getDevice(), &createInfo, nullptr, &_shaderModule)
			== VK_SUCCESS) {
		return core::Shader::init(dev,
				[](core::Device *dev, core::ObjectType, ObjectHandle ptr, void *) {
			auto d = ((Device *)dev);
			d->getTable()->vkDestroyShaderModule(d->getDevice(), (VkShaderModule)ptr.get(),
					nullptr);
		}, core::ObjectType::ShaderModule, ObjectHandle(_shaderModule));
	}
	return false;
}

bool GraphicPipeline::comparePipelineOrdering(const PipelineInfo &l, const PipelineInfo &r) {
	if (l.material.getDepthInfo().writeEnabled != r.material.getDepthInfo().writeEnabled) {
		if (l.material.getDepthInfo().writeEnabled) {
			return true; // pipelines with depth write comes first
		}
		return false;
	} else if (l.material.getBlendInfo().enabled != r.material.getBlendInfo().enabled) {
		if (!l.material.getBlendInfo().enabled) {
			return true; // pipelines without blending comes first
		}
		return false;
	} else {
		return &l < &r;
	}
}

bool GraphicPipeline::init(Device &dev, const PipelineData &params, const SubpassData &pass,
		const Queue &queue) {
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	sanitizeVkStruct(vertexInputInfo);
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.pNext = nullptr;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	sanitizeVkStruct(inputAssembly);
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.pNext = nullptr;
	inputAssembly.flags = 0;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport{};
	sanitizeVkStruct(viewport);
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = 0.0f;
	viewport.height = 0.0f;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	sanitizeVkStruct(scissor);
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = 0;
	scissor.extent.height = 0;

	VkPipelineViewportStateCreateInfo viewportState{};
	sanitizeVkStruct(viewportState);
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.pNext = nullptr;
	viewportState.flags = 0;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	sanitizeVkStruct(rasterizer);
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.pNext = nullptr;
	rasterizer.flags = 0;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;

	if (params.material.getLineWidth() == 0.0f || !dev.hasNonSolidFillMode()) {
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
	} else if (params.material.getLineWidth() > 0.0f) {
		rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
		rasterizer.lineWidth = params.material.getLineWidth();
	} else if (params.material.getLineWidth() < 0.0f) {
		rasterizer.polygonMode = VK_POLYGON_MODE_POINT;
		rasterizer.lineWidth = -params.material.getLineWidth();
	}

	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	VkPipelineMultisampleStateCreateInfo multisampling{};
	sanitizeVkStruct(multisampling);
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.pNext = nullptr;
	multisampling.flags = 0;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = nullptr; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	Vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
	for (size_t i = 0; i < pass.outputImages.size(); ++i) {
		core::BlendInfo blend;
		if (pass.outputImages[i]->blendInfo.enabled) {
			blend = pass.outputImages[i]->blendInfo;
		} else {
			blend = params.material.getBlendInfo();
		}

		colorBlendAttachments.emplace_back(VkPipelineColorBlendAttachmentState{
			blend.isEnabled() ? VK_TRUE : VK_FALSE,
			VkBlendFactor(blend.srcColor),
			VkBlendFactor(blend.dstColor),
			VkBlendOp(blend.opColor),
			VkBlendFactor(blend.srcAlpha),
			VkBlendFactor(blend.dstAlpha),
			VkBlendOp(blend.opAlpha),
			VkColorComponentFlags(blend.writeMask),
		});
	}

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	sanitizeVkStruct(colorBlending);
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.pNext = nullptr;
	colorBlending.flags = 0;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlending.attachmentCount = uint32_t(colorBlendAttachments.size());
	colorBlending.pAttachments = colorBlendAttachments.data();
	colorBlending.blendConstants[0] = 0.0f; // Optional
	colorBlending.blendConstants[1] = 0.0f; // Optional
	colorBlending.blendConstants[2] = 0.0f; // Optional
	colorBlending.blendConstants[3] = 0.0f; // Optional

	Vector<VkDynamicState> dynamicStates;

	if ((params.dynamicState & core::DynamicState::Viewport) != core::DynamicState::None) {
		dynamicStates.emplace_back(VK_DYNAMIC_STATE_VIEWPORT);
	}

	if ((params.dynamicState & core::DynamicState::Scissor) != core::DynamicState::None) {
		dynamicStates.emplace_back(VK_DYNAMIC_STATE_SCISSOR);
	}

	VkPipelineDynamicStateCreateInfo dynamicState{};
	sanitizeVkStruct(dynamicState);
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.pNext = nullptr;
	dynamicState.flags = 0;
	dynamicState.dynamicStateCount = uint32_t(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	Vector<VkPipelineShaderStageCreateInfo> shaderStages;
	shaderStages.reserve(params.shaders.size());

	struct SpecInfo {
		VkSpecializationInfo specInfo;
		Vector<VkSpecializationMapEntry> entries;
		Bytes data;
	};

	Vector<SpecInfo> specs;
	size_t constants = 0;
	for (auto &shader : params.shaders) {
		if (!shader.constants.empty()) {
			++constants;
		}
	}
	specs.reserve(constants);

	for (auto &shader : params.shaders) {
		VkPipelineShaderStageCreateInfo info;
		sanitizeVkStruct(info);
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		info.pNext = nullptr;
		info.flags = 0;
		info.stage = VkShaderStageFlagBits(shader.data->stage);
		info.module = shader.data->program.cast<Shader>()->getModule();
		info.pName = shader.data->entryPoints.front().name.data();

		if (!dev.getInfo().features.device10.features.shaderSampledImageArrayDynamicIndexing) {
			for (auto &it : shader.data->entryPoints) {
				if (StringView(it.name).ends_with("_static")) {
					info.pName = it.name.data();
				}
			}
		}

		if (!shader.constants.empty()) {
			auto &spec = specs.emplace_back();
			spec.entries.reserve(shader.constants.size());
			spec.data.reserve(sizeof(uint32_t) * shader.constants.size());
			uint32_t idx = 0;
			for (auto &it : shader.constants) {
				VkSpecializationMapEntry &entry = spec.entries.emplace_back();
				entry.constantID = idx;
				entry.offset = uint32_t(spec.data.size());

				BytesView data;
				switch (it.type) {
				case core::SpecializationConstant::Int:
					data = Shader_emplaceConstant(spec.data, it.intValue);
					entry.size = data.size();
					break;
				case core::SpecializationConstant::Float:
					data = Shader_emplaceConstant(spec.data, it.floatValue);
					entry.size = data.size();
					break;
				case core::SpecializationConstant::Callback: {
					auto val = (*it.function)(dev, *params.layout);
					switch (val.type) {
					case core::SpecializationConstant::Int:
						data = Shader_emplaceConstant(spec.data, val.intValue);
						entry.size = data.size();
						break;
					case core::SpecializationConstant::Float:
						data = Shader_emplaceConstant(spec.data, val.floatValue);
						entry.size = data.size();
						break;
					default:
						log::source().error("GraphicPipeline",
										"Invalid SpecializationConstant::Callback result, callback "
										"should " "not return another callback");
						break;
					}
				}
				}
				++idx;
			}

			spec.specInfo.mapEntryCount = uint32_t(spec.entries.size());
			spec.specInfo.pMapEntries = spec.entries.data();
			spec.specInfo.dataSize = spec.data.size();
			spec.specInfo.pData = spec.data.data();
			info.pSpecializationInfo = &spec.specInfo;
		} else {
			info.pSpecializationInfo = nullptr;
		}
		shaderStages.emplace_back(info);
	}

	VkPipelineDepthStencilStateCreateInfo depthState;
	sanitizeVkStruct(depthState);
	depthState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthState.pNext = nullptr;
	depthState.flags = 0;
	if (pass.depthStencil) {
		auto a = (core::ImageAttachment *)pass.depthStencil->pass->attachment->attachment.get();
		if (a && core::isDepthFormat(a->getImageInfo().format)) {
			auto &depth = params.material.getDepthInfo();
			auto &bounds = params.material.getDepthBounds();
			depthState.depthTestEnable = depth.testEnabled ? VK_TRUE : VK_FALSE;
			depthState.depthWriteEnable = depth.writeEnabled ? VK_TRUE : VK_FALSE;
			depthState.depthCompareOp = VkCompareOp(depth.compare);

			depthState.depthBoundsTestEnable = bounds.enabled ? VK_TRUE : VK_FALSE;
			depthState.minDepthBounds = bounds.min;
			depthState.maxDepthBounds = bounds.max;
		} else {
			depthState.depthTestEnable = VK_FALSE;
			depthState.depthWriteEnable = VK_FALSE;
			depthState.depthCompareOp = VkCompareOp(0);

			depthState.depthBoundsTestEnable = VK_FALSE;
			depthState.minDepthBounds = 0.0f;
			depthState.maxDepthBounds = 0.0f;
		}

		if (a && core::isStencilFormat(a->getImageInfo().format)) {
			auto &front = params.material.getStencilInfoFront();
			auto &back = params.material.getStencilInfoBack();

			depthState.stencilTestEnable = params.material.isStancilEnabled() ? VK_TRUE : VK_FALSE;

			depthState.front.failOp = VkStencilOp(front.fail);
			depthState.front.passOp = VkStencilOp(front.pass);
			depthState.front.depthFailOp = VkStencilOp(front.depthFail);
			depthState.front.compareOp = VkCompareOp(front.compare);
			depthState.front.compareMask = front.compareMask;
			depthState.front.writeMask = front.writeMask;
			depthState.front.reference = front.reference;

			depthState.back.failOp = VkStencilOp(back.fail);
			depthState.back.passOp = VkStencilOp(back.pass);
			depthState.back.depthFailOp = VkStencilOp(back.depthFail);
			depthState.back.compareOp = VkCompareOp(back.compare);
			depthState.back.compareMask = back.compareMask;
			depthState.back.writeMask = back.writeMask;
			depthState.back.reference = back.reference;
		} else {
			depthState.stencilTestEnable = VK_FALSE;

			depthState.front.failOp = VK_STENCIL_OP_KEEP;
			depthState.front.passOp = VK_STENCIL_OP_KEEP;
			depthState.front.depthFailOp = VK_STENCIL_OP_KEEP;
			depthState.front.compareOp = VK_COMPARE_OP_NEVER;
			depthState.front.compareMask = 0;
			depthState.front.writeMask = 0;
			depthState.front.reference = 0;

			depthState.back.failOp = VK_STENCIL_OP_KEEP;
			depthState.back.passOp = VK_STENCIL_OP_KEEP;
			depthState.back.depthFailOp = VK_STENCIL_OP_KEEP;
			depthState.back.compareOp = VK_COMPARE_OP_NEVER;
			depthState.back.compareMask = 0;
			depthState.back.writeMask = 0;
			depthState.back.reference = 0;
		}
	}

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	sanitizeVkStruct(pipelineInfo);
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = nullptr;
	pipelineInfo.flags = 0;
	pipelineInfo.stageCount = uint32_t(shaderStages.size());
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = pass.depthStencil ? &depthState : nullptr; // Optional
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = (dynamicStates.size() > 0) ? &dynamicState : nullptr; // Optional

	pipelineInfo.layout = pass.pass->impl.cast<RenderPass>()
								  ->getPipelineLayout(params.layout->index)
								  ->getLayout();
	pipelineInfo.renderPass = pass.pass->impl.cast<RenderPass>()->getRenderPass();
	pipelineInfo.subpass = pass.index;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	auto err = dev.getTable()->vkCreateGraphicsPipelines(dev.getDevice(), VK_NULL_HANDLE, 1,
			&pipelineInfo, nullptr, &_pipeline);
	if (err == VK_SUCCESS) {
		_name = params.key.str<Interface>();
		return core::GraphicPipeline::init(dev,
				[](core::Device *dev, core::ObjectType, ObjectHandle ptr, void *) {
			auto d = ((Device *)dev);
			d->getTable()->vkDestroyPipeline(d->getDevice(), (VkPipeline)ptr.get(), nullptr);
		}, core::ObjectType::Pipeline, ObjectHandle(_pipeline));
	}
	return false;
}

bool ComputePipeline::init(Device &dev, const PipelineData &params, const SubpassData &pass,
		const Queue &) {
	VkComputePipelineCreateInfo pipelineInfo;
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = nullptr;
	pipelineInfo.flags = 0;
	pipelineInfo.layout = pass.pass->impl.cast<RenderPass>()
								  ->getPipelineLayout(params.layout->index)
								  ->getLayout();
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = 0;

	struct SpecInfo {
		VkSpecializationInfo specInfo;
		Vector<VkSpecializationMapEntry> entries;
		Bytes data;
	};

	Vector<SpecInfo> specs;
	pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipelineInfo.stage.pNext = nullptr;
	pipelineInfo.stage.flags = 0;
	pipelineInfo.stage.stage = VkShaderStageFlagBits(params.shader.data->stage);
	pipelineInfo.stage.module = params.shader.data->program.cast<Shader>()->getModule();
	pipelineInfo.stage.pName = params.shader.data->entryPoints.front().name.data();
	pipelineInfo.stage.pSpecializationInfo = nullptr;

	if (!dev.getInfo().features.device10.features.shaderSampledImageArrayDynamicIndexing) {
		for (auto &it : params.shader.data->entryPoints) {
			if (StringView(it.name).ends_with("_static")) {
				pipelineInfo.stage.pName = it.name.data();
			}
		}
	}

	for (auto &it : params.shader.data->entryPoints) {
		if (it.name == pipelineInfo.stage.pName) {
			_localX = it.localX;
			_localY = it.localY;
			_localZ = it.localZ;
		}
	}

	if (!params.shader.constants.empty()) {
		auto &spec = specs.emplace_back();
		spec.entries.reserve(params.shader.constants.size());
		spec.data.reserve(sizeof(uint32_t) * params.shader.constants.size());
		uint32_t idx = 0;
		for (auto &it : params.shader.constants) {
			VkSpecializationMapEntry &entry = spec.entries.emplace_back();
			entry.constantID = idx;
			entry.offset = uint32_t(spec.data.size());

			BytesView data;
			switch (it.type) {
			case core::SpecializationConstant::Int:
				data = Shader_emplaceConstant(spec.data, it.intValue);
				entry.size = data.size();
				break;
			case core::SpecializationConstant::Float:
				data = Shader_emplaceConstant(spec.data, it.floatValue);
				entry.size = data.size();
				break;
			case core::SpecializationConstant::Callback: {
				auto val = (*it.function)(dev, *params.layout);
				switch (val.type) {
				case core::SpecializationConstant::Int:
					data = Shader_emplaceConstant(spec.data, val.intValue);
					entry.size = data.size();
					break;
				case core::SpecializationConstant::Float:
					data = Shader_emplaceConstant(spec.data, val.floatValue);
					entry.size = data.size();
					break;
				default:
					log::source().error("GraphicPipeline",
										"Invalid SpecializationConstant::Callback result, callback "
										"should " "not return another callback");
					break;
				}
			}
			}
			++idx;
		}

		spec.specInfo.mapEntryCount = uint32_t(spec.entries.size());
		spec.specInfo.pMapEntries = spec.entries.data();
		spec.specInfo.dataSize = spec.data.size();
		spec.specInfo.pData = spec.data.data();
		pipelineInfo.stage.pSpecializationInfo = &spec.specInfo;
	} else {
		pipelineInfo.stage.pSpecializationInfo = nullptr;
	}

	auto err = dev.getTable()->vkCreateComputePipelines(dev.getDevice(), VK_NULL_HANDLE, 1,
			&pipelineInfo, nullptr, &_pipeline);
	if (err == VK_SUCCESS) {
		_name = params.key.str<Interface>();
		return core::ComputePipeline::init(dev,
				[](core::Device *dev, core::ObjectType, ObjectHandle ptr, void *) {
			auto d = ((Device *)dev);
			d->getTable()->vkDestroyPipeline(d->getDevice(), (VkPipeline)ptr.get(), nullptr);
		}, core::ObjectType::Pipeline, ObjectHandle(_pipeline));
	}
	return false;
}

} // namespace stappler::xenolith::vk
