/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#include "XLWgpuPipeline.h"
#include "XLWgpuQueuePass.h"
#include "XLCoreAttachment.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::webgpu {

WGPUTextureFormat getWGPUFormat(core::ImageFormat fmt) {
	switch (fmt) {
	case core::ImageFormat::R8_UNORM: return WGPUTextureFormat_R8Unorm; break;
	case core::ImageFormat::R8G8_UNORM: return WGPUTextureFormat_RG8Unorm; break;
	case core::ImageFormat::R8G8B8A8_UNORM: return WGPUTextureFormat_RGBA8Unorm; break;
	case core::ImageFormat::R8G8B8A8_SRGB: return WGPUTextureFormat_RGBA8UnormSrgb; break;
	case core::ImageFormat::B8G8R8A8_UNORM: return WGPUTextureFormat_BGRA8Unorm; break;
	case core::ImageFormat::B8G8R8A8_SRGB: return WGPUTextureFormat_BGRA8UnormSrgb; break;
	case core::ImageFormat::R16G16B16A16_SFLOAT: return WGPUTextureFormat_RGBA16Float; break;
	case core::ImageFormat::R32G32B32A32_SFLOAT: return WGPUTextureFormat_RGBA32Float; break;
	case core::ImageFormat::R16_SFLOAT: return WGPUTextureFormat_R16Float; break;
	case core::ImageFormat::R32_SFLOAT: return WGPUTextureFormat_R32Float; break;
	case core::ImageFormat::D16_UNORM: return WGPUTextureFormat_Depth16Unorm; break;
	case core::ImageFormat::D24_UNORM_S8_UINT: return WGPUTextureFormat_Depth24PlusStencil8; break;
	case core::ImageFormat::D32_SFLOAT: return WGPUTextureFormat_Depth32Float; break;
	case core::ImageFormat::D32_SFLOAT_S8_UINT:
		return WGPUTextureFormat_Depth32FloatStencil8;
		break;
	default:
		log::source().error("webgpu", "Unmapped ImageFormat: ", core::getImageFormatName(fmt));
		break;
	}
	return WGPUTextureFormat_Undefined;
}

WGPUBlendFactor getWGPUBlendFactor(core::BlendFactor factor) {
	switch (factor) {
	case core::BlendFactor::Zero: return WGPUBlendFactor_Zero; break;
	case core::BlendFactor::One: return WGPUBlendFactor_One; break;
	case core::BlendFactor::SrcColor: return WGPUBlendFactor_Src; break;
	case core::BlendFactor::OneMinusSrcColor: return WGPUBlendFactor_OneMinusSrc; break;
	case core::BlendFactor::DstColor: return WGPUBlendFactor_Dst; break;
	case core::BlendFactor::OneMinusDstColor: return WGPUBlendFactor_OneMinusDst; break;
	case core::BlendFactor::SrcAlpha: return WGPUBlendFactor_SrcAlpha; break;
	case core::BlendFactor::OneMinusSrcAlpha: return WGPUBlendFactor_OneMinusSrcAlpha; break;
	case core::BlendFactor::DstAlpha: return WGPUBlendFactor_DstAlpha; break;
	case core::BlendFactor::OneMinusDstAlpha: return WGPUBlendFactor_OneMinusDstAlpha; break;
	}
	return WGPUBlendFactor_Undefined;
}

WGPUBlendOperation getWGPUBlendOperation(core::BlendOp op) {
	switch (op) {
	case core::BlendOp::Add: return WGPUBlendOperation_Add; break;
	case core::BlendOp::Subtract: return WGPUBlendOperation_Subtract; break;
	case core::BlendOp::ReverseSubtract: return WGPUBlendOperation_ReverseSubtract; break;
	case core::BlendOp::Min: return WGPUBlendOperation_Min; break;
	case core::BlendOp::Max: return WGPUBlendOperation_Max; break;
	}
	return WGPUBlendOperation_Undefined;
}

WGPUCompareFunction getWGPUCompareFunction(core::CompareOp op) {
	switch (op) {
	case core::CompareOp::Never: return WGPUCompareFunction_Never; break;
	case core::CompareOp::Less: return WGPUCompareFunction_Less; break;
	case core::CompareOp::Equal: return WGPUCompareFunction_Equal; break;
	case core::CompareOp::LessOrEqual: return WGPUCompareFunction_LessEqual; break;
	case core::CompareOp::Greater: return WGPUCompareFunction_Greater; break;
	case core::CompareOp::NotEqual: return WGPUCompareFunction_NotEqual; break;
	case core::CompareOp::GreaterOrEqual: return WGPUCompareFunction_GreaterEqual; break;
	case core::CompareOp::Always: return WGPUCompareFunction_Always; break;
	}
	return WGPUCompareFunction_Undefined;
}

bool Shader::init(Device &dev, const core::ProgramData &data) {
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

bool Shader::setup(Device &dev, const core::ProgramData &programData, SpanView<uint32_t> data) {
	if (!data.empty() && data[0] == 0x0723'0203) {
		// SPIR-V program (e.g. from a shared GLSL-based queue): use the native
		// extension, naga translates it internally; a browser build accepts
		// WGSL only - such queues must not be compiled there
		if (!dev.getBackendFeatures().spirvShaders) {
			log::source().error("webgpu::Shader",
					"SPIR-V shader modules are not supported by this device: ", _name);
			return false;
		}

#if XL_WGPU_NATIVE_API
		WGPUShaderModuleDescriptorSpirV spirvDesc;
		spirvDesc.label = WGPUStringView{_name.data(), _name.size()};
		spirvDesc.sourceSize = uint32_t(data.size());
		spirvDesc.source = data.data();

		_shaderModule = wgpuDeviceCreateShaderModuleSpirV(dev.getDevice(), &spirvDesc);
		if (!_shaderModule) {
			log::source().error("webgpu::Shader",
					"Fail to create shader module from SPIR-V: ", _name);
			return false;
		}

		return core::Shader::init(dev,
				[](core::Device *, core::ObjectType, core::ObjectHandle ptr, void *) {
			wgpuShaderModuleRelease(reinterpret_cast<WGPUShaderModule>(ptr.get()));
		}, core::ObjectType::ShaderModule, core::ObjectHandle(_shaderModule));
#else
		return false;
#endif
	}

	// data is WGSL text packed into uint32_t words, trim word-alignment padding
	auto code = reinterpret_cast<const char *>(data.data());
	auto codeSize = data.size() * sizeof(uint32_t);
	while (codeSize > 0 && code[codeSize - 1] == '\0') { --codeSize; }

	WGPUShaderSourceWGSL wgslSource = WGPU_SHADER_SOURCE_WGSL_INIT;
	wgslSource.code = WGPUStringView{code, codeSize};

	WGPUShaderModuleDescriptor moduleDesc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
	moduleDesc.nextInChain = &wgslSource.chain;
	moduleDesc.label = WGPUStringView{_name.data(), _name.size()};

	_shaderModule = wgpuDeviceCreateShaderModule(dev.getDevice(), &moduleDesc);
	if (!_shaderModule) {
		log::source().error("webgpu::Shader", "Fail to create shader module: ", _name);
		return false;
	}

	return core::Shader::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle ptr, void *) {
		wgpuShaderModuleRelease(reinterpret_cast<WGPUShaderModule>(ptr.get()));
	}, core::ObjectType::ShaderModule, core::ObjectHandle(_shaderModule));
}

bool GraphicPipeline::init(Device &dev, const PipelineData &data) {
	_name = data.key.str<Interface>();

	WGPUShaderModule vertexModule = nullptr;
	WGPUShaderModule fragmentModule = nullptr;

	for (auto &it : data.shaders) {
		if (!it.data || !it.data->program) {
			log::source().error("webgpu::GraphicPipeline", _name, ": program is not compiled: ",
					it.data ? it.data->key : StringView("<null>"));
			return false;
		}
		auto module = static_cast<Shader *>(it.data->program.get())->getModule();
		switch (it.data->stage) {
		case core::ProgramStage::Vertex: vertexModule = module; break;
		case core::ProgramStage::Fragment: fragmentModule = module; break;
		default:
			log::source().error("webgpu::GraphicPipeline", _name,
					": unsupported shader stage in graphic pipeline");
			return false;
			break;
		}
	}

	if (!vertexModule) {
		log::source().error("webgpu::GraphicPipeline", _name, ": no vertex shader");
		return false;
	}

	WGPURenderPipelineDescriptor pipelineDesc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
	pipelineDesc.label = WGPUStringView{_name.data(), _name.size()};

	// compiled pass layouts (see webgpu::RenderPass::makeLayout)
	auto passImpl = data.subpass->pass->impl.get_cast<RenderPass>();
	if (passImpl && data.layout) {
		if (auto l = passImpl->getLayout(data.layout->index)) {
			pipelineDesc.layout = l->pipelineLayout;
		}
	}
	if (!pipelineDesc.layout) {
		log::source().error("webgpu::GraphicPipeline", _name, ": no compiled pipeline layout");
		return false;
	}

	pipelineDesc.vertex.module = vertexModule;
	pipelineDesc.vertex.entryPoint = WGPUStringView{"main", WGPU_STRLEN};

	pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
	pipelineDesc.primitive.cullMode = WGPUCullMode_None;

	// color targets from subpass output attachments
	Vector<WGPUColorTargetState> targets;
	Vector<WGPUBlendState> blendStates;
	blendStates.reserve(data.subpass->outputImages.size());

	for (auto &out : data.subpass->outputImages) {
		if (out->pass->attachment->type != core::AttachmentType::Image) {
			continue;
		}
		auto attachment = out->pass->attachment->attachment.get_cast<core::ImageAttachment>();
		if (!attachment) {
			continue;
		}

		WGPUColorTargetState target = WGPU_COLOR_TARGET_STATE_INIT;
		target.format = getWGPUFormat(attachment->getImageInfo().format);

		// blend state comes from the PIPELINE's material info (Solid vs
		// Transparent pipelines differ exactly here), not from the subpass
		// attachment defaults
		auto &blendInfo = data.material.getBlendInfo();
		if (blendInfo.enabled) {
			auto &blend = blendStates.emplace_back(WGPU_BLEND_STATE_INIT);
			blend.color.srcFactor = getWGPUBlendFactor(core::BlendFactor(blendInfo.srcColor));
			blend.color.dstFactor = getWGPUBlendFactor(core::BlendFactor(blendInfo.dstColor));
			blend.color.operation = getWGPUBlendOperation(core::BlendOp(blendInfo.opColor));
			blend.alpha.srcFactor = getWGPUBlendFactor(core::BlendFactor(blendInfo.srcAlpha));
			blend.alpha.dstFactor = getWGPUBlendFactor(core::BlendFactor(blendInfo.dstAlpha));
			blend.alpha.operation = getWGPUBlendOperation(core::BlendOp(blendInfo.opAlpha));
			target.blend = &blend;
		}

		targets.emplace_back(target);
	}

	WGPUFragmentState fragmentState = WGPU_FRAGMENT_STATE_INIT;
	if (fragmentModule) {
		fragmentState.module = fragmentModule;
		fragmentState.entryPoint = WGPUStringView{"main", WGPU_STRLEN};
		fragmentState.targetCount = targets.size();
		fragmentState.targets = targets.data();
		pipelineDesc.fragment = &fragmentState;
	}

	// depth-stencil from subpass depth attachment
	WGPUDepthStencilState depthState = WGPU_DEPTH_STENCIL_STATE_INIT;
	if (data.subpass->depthStencil
			&& data.subpass->depthStencil->pass->attachment->type
					== core::AttachmentType::Image) {
		auto attachment = data.subpass->depthStencil->pass->attachment->attachment
								  .get_cast<core::ImageAttachment>();
		if (attachment) {
			auto &depthInfo = data.material.getDepthInfo();
			depthState.format = getWGPUFormat(attachment->getImageInfo().format);
			depthState.depthWriteEnabled =
					depthInfo.writeEnabled ? WGPUOptionalBool_True : WGPUOptionalBool_False;
			depthState.depthCompare = depthInfo.testEnabled
					? getWGPUCompareFunction(core::CompareOp(depthInfo.compare))
					: WGPUCompareFunction_Always;
			pipelineDesc.depthStencil = &depthState;
		}
	}

	_pipeline = wgpuDeviceCreateRenderPipeline(dev.getDevice(), &pipelineDesc);
	if (!_pipeline) {
		log::source().error("webgpu::GraphicPipeline", "Fail to create pipeline: ", _name);
		return false;
	}

	return core::GraphicPipeline::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle ptr, void *) {
		wgpuRenderPipelineRelease(reinterpret_cast<WGPURenderPipeline>(ptr.get()));
	}, core::ObjectType::Pipeline, core::ObjectHandle(_pipeline));
}

bool ComputePipeline::init(Device &dev, const PipelineData &data) {
	_name = data.key.str<Interface>();

	if (!data.shader.data || !data.shader.data->program) {
		log::source().error("webgpu::ComputePipeline", _name, ": program is not compiled: ",
				data.shader.data ? data.shader.data->key : StringView("<null>"));
		return false;
	}

	if (data.shader.data->stage != core::ProgramStage::Compute) {
		log::source().error("webgpu::ComputePipeline", _name, ": not a compute shader");
		return false;
	}

	WGPUComputePipelineDescriptor pipelineDesc = WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;
	pipelineDesc.label = WGPUStringView{_name.data(), _name.size()};

	auto passImpl = data.subpass->pass->impl.get_cast<RenderPass>();
	if (passImpl && data.layout) {
		if (auto l = passImpl->getLayout(data.layout->index)) {
			pipelineDesc.layout = l->pipelineLayout;
		}
	}
	if (!pipelineDesc.layout) {
		log::source().error("webgpu::ComputePipeline", _name, ": no compiled pipeline layout");
		return false;
	}

	pipelineDesc.compute.module =
			static_cast<Shader *>(data.shader.data->program.get())->getModule();
	pipelineDesc.compute.entryPoint = WGPUStringView{"main", WGPU_STRLEN};

	_pipeline = wgpuDeviceCreateComputePipeline(dev.getDevice(), &pipelineDesc);
	if (!_pipeline) {
		log::source().error("webgpu::ComputePipeline", "Fail to create pipeline: ", _name);
		return false;
	}

	return core::ComputePipeline::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle ptr, void *) {
		wgpuComputePipelineRelease(reinterpret_cast<WGPUComputePipeline>(ptr.get()));
	}, core::ObjectType::Pipeline, core::ObjectHandle(_pipeline));
}

} // namespace stappler::xenolith::webgpu
