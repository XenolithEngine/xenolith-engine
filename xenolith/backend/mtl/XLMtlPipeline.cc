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

#include "XLMtlPipeline.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::mtl {

MTLPixelFormat getMTLPixelFormat(core::ImageFormat fmt) {
	switch (fmt) {
	case core::ImageFormat::R8_UNORM: return MTLPixelFormatR8Unorm; break;
	case core::ImageFormat::R8G8_UNORM: return MTLPixelFormatRG8Unorm; break;
	case core::ImageFormat::R8G8B8A8_UNORM: return MTLPixelFormatRGBA8Unorm; break;
	case core::ImageFormat::R8G8B8A8_SRGB: return MTLPixelFormatRGBA8Unorm_sRGB; break;
	case core::ImageFormat::B8G8R8A8_UNORM: return MTLPixelFormatBGRA8Unorm; break;
	case core::ImageFormat::B8G8R8A8_SRGB: return MTLPixelFormatBGRA8Unorm_sRGB; break;
	case core::ImageFormat::R16G16B16A16_SFLOAT: return MTLPixelFormatRGBA16Float; break;
	case core::ImageFormat::R32G32B32A32_SFLOAT: return MTLPixelFormatRGBA32Float; break;
	case core::ImageFormat::R16_SFLOAT: return MTLPixelFormatR16Float; break;
	case core::ImageFormat::R32_SFLOAT: return MTLPixelFormatR32Float; break;
	case core::ImageFormat::D16_UNORM: return MTLPixelFormatDepth16Unorm; break;
	case core::ImageFormat::D32_SFLOAT: return MTLPixelFormatDepth32Float; break;
	case core::ImageFormat::D32_SFLOAT_S8_UINT:
		return MTLPixelFormatDepth32Float_Stencil8;
		break;
	default:
		log::source().error("mtl", "Unmapped ImageFormat: ", core::getImageFormatName(fmt));
		break;
	}
	return MTLPixelFormatInvalid;
}

MTLBlendFactor getMTLBlendFactor(core::BlendFactor factor) {
	switch (factor) {
	case core::BlendFactor::Zero: return MTLBlendFactorZero; break;
	case core::BlendFactor::One: return MTLBlendFactorOne; break;
	case core::BlendFactor::SrcColor: return MTLBlendFactorSourceColor; break;
	case core::BlendFactor::OneMinusSrcColor: return MTLBlendFactorOneMinusSourceColor; break;
	case core::BlendFactor::DstColor: return MTLBlendFactorDestinationColor; break;
	case core::BlendFactor::OneMinusDstColor:
		return MTLBlendFactorOneMinusDestinationColor;
		break;
	case core::BlendFactor::SrcAlpha: return MTLBlendFactorSourceAlpha; break;
	case core::BlendFactor::OneMinusSrcAlpha: return MTLBlendFactorOneMinusSourceAlpha; break;
	case core::BlendFactor::DstAlpha: return MTLBlendFactorDestinationAlpha; break;
	case core::BlendFactor::OneMinusDstAlpha:
		return MTLBlendFactorOneMinusDestinationAlpha;
		break;
	}
	return MTLBlendFactorZero;
}

MTLBlendOperation getMTLBlendOperation(core::BlendOp op) {
	switch (op) {
	case core::BlendOp::Add: return MTLBlendOperationAdd; break;
	case core::BlendOp::Subtract: return MTLBlendOperationSubtract; break;
	case core::BlendOp::ReverseSubtract: return MTLBlendOperationReverseSubtract; break;
	case core::BlendOp::Min: return MTLBlendOperationMin; break;
	case core::BlendOp::Max: return MTLBlendOperationMax; break;
	}
	return MTLBlendOperationAdd;
}

MTLCompareFunction getMTLCompareFunction(core::CompareOp op) {
	switch (op) {
	case core::CompareOp::Never: return MTLCompareFunctionNever; break;
	case core::CompareOp::Less: return MTLCompareFunctionLess; break;
	case core::CompareOp::Equal: return MTLCompareFunctionEqual; break;
	case core::CompareOp::LessOrEqual: return MTLCompareFunctionLessEqual; break;
	case core::CompareOp::Greater: return MTLCompareFunctionGreater; break;
	case core::CompareOp::NotEqual: return MTLCompareFunctionNotEqual; break;
	case core::CompareOp::GreaterOrEqual: return MTLCompareFunctionGreaterEqual; break;
	case core::CompareOp::Always: return MTLCompareFunctionAlways; break;
	}
	return MTLCompareFunctionNever;
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
		// the backend performs no SPIR-V translation: shared GLSL-based queues
		// must not be compiled here, provide an MSL variant of the program
		log::source().error("mtl::Shader",
				"SPIR-V shader modules are not supported by the Metal backend, "
				"MSL source expected: ",
				_name);
		return false;
	}

	// ProgramData::data carries UTF-8 MSL text padded to uint32_t words,
	// trim the zero padding
	auto code = reinterpret_cast<const char *>(data.data());
	size_t codeSize = data.size() * sizeof(uint32_t);
	while (codeSize > 0 && code[codeSize - 1] == 0) { --codeSize; }

	@autoreleasepool {
		NSString *source = [[NSString alloc] initWithBytes:code
													length:codeSize
												  encoding:NSUTF8StringEncoding];
		if (!source) {
			log::source().error("mtl::Shader", "Shader source is not valid UTF-8: ", _name);
			return false;
		}

		MTLCompileOptions *options = [[MTLCompileOptions alloc] init];

		NSError *error = nil;
		id<MTLLibrary> library = [dev.getDevice() newLibraryWithSource:source
															   options:options
																 error:&error];
		if (!library) {
			log::source().error("mtl::Shader", "Fail to compile MSL library: ", _name, ": ",
					error ? StringView(error.localizedDescription.UTF8String) : StringView());
			return false;
		}

		id<MTLFunction> function =
				[library newFunctionWithName:[NSString stringWithUTF8String:ShaderEntryPointName]];
		if (!function) {
			log::source().error("mtl::Shader", "Entry point '", ShaderEntryPointName,
					"' not found in program: ", _name);
			return false;
		}

		_library = retainHandle(library);
		_function = retainHandle(function);
	}

	return core::Shader::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle ptr, void *fn) {
		releaseHandle((void *)ptr.get());
		releaseHandle(fn);
	}, core::ObjectType::ShaderModule, core::ObjectHandle(_library), _function);
}

bool GraphicPipeline::init(Device &dev, const PipelineData &data) {
	_name = data.key.str<Interface>();

	id<MTLFunction> vertexFn = nil;
	id<MTLFunction> fragmentFn = nil;

	for (auto &it : data.shaders) {
		if (!it.data || !it.data->program) {
			log::source().error("mtl::GraphicPipeline", _name,
					": program is not compiled: ", it.data ? it.data->key : StringView("<null>"));
			return false;
		}
		auto fn = static_cast<Shader *>(it.data->program.get())->getFunction();
		switch (it.data->stage) {
		case core::ProgramStage::Vertex: vertexFn = fn; break;
		case core::ProgramStage::Fragment: fragmentFn = fn; break;
		default:
			log::source().error("mtl::GraphicPipeline", _name,
					": unsupported shader stage in graphic pipeline");
			return false;
			break;
		}
	}

	if (!vertexFn) {
		log::source().error("mtl::GraphicPipeline", _name, ": no vertex shader");
		return false;
	}

	@autoreleasepool {
		MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor alloc] init];
		desc.label = [NSString stringWithUTF8String:_name.data()];
		desc.vertexFunction = vertexFn;
		desc.fragmentFunction = fragmentFn;

		// color targets from subpass output attachments; blend state comes
		// from the PIPELINE's material info (Solid vs Transparent pipelines
		// differ exactly here), not from the subpass attachment defaults
		auto &blendInfo = data.material.getBlendInfo();
		NSUInteger targetIndex = 0;
		for (auto &out : data.subpass->outputImages) {
			if (out->pass->attachment->type != core::AttachmentType::Image) {
				continue;
			}
			auto attachment = out->pass->attachment->attachment.get_cast<core::ImageAttachment>();
			if (!attachment) {
				continue;
			}

			MTLRenderPipelineColorAttachmentDescriptor *target =
					desc.colorAttachments[targetIndex];
			target.pixelFormat = getMTLPixelFormat(attachment->getImageInfo().format);
			if (blendInfo.enabled) {
				target.blendingEnabled = YES;
				target.sourceRGBBlendFactor =
						getMTLBlendFactor(core::BlendFactor(blendInfo.srcColor));
				target.destinationRGBBlendFactor =
						getMTLBlendFactor(core::BlendFactor(blendInfo.dstColor));
				target.rgbBlendOperation = getMTLBlendOperation(core::BlendOp(blendInfo.opColor));
				target.sourceAlphaBlendFactor =
						getMTLBlendFactor(core::BlendFactor(blendInfo.srcAlpha));
				target.destinationAlphaBlendFactor =
						getMTLBlendFactor(core::BlendFactor(blendInfo.dstAlpha));
				target.alphaBlendOperation =
						getMTLBlendOperation(core::BlendOp(blendInfo.opAlpha));
			}
			++targetIndex;
		}

		// depth-stencil: the pixel format on the pipeline, test/write state as
		// a separate immutable object bound alongside it at record time
		if (data.subpass->depthStencil
				&& data.subpass->depthStencil->pass->attachment->type
						== core::AttachmentType::Image) {
			auto attachment = data.subpass->depthStencil->pass->attachment->attachment
									  .get_cast<core::ImageAttachment>();
			if (attachment) {
				auto &depthInfo = data.material.getDepthInfo();
				desc.depthAttachmentPixelFormat =
						getMTLPixelFormat(attachment->getImageInfo().format);

				MTLDepthStencilDescriptor *depthDesc = [[MTLDepthStencilDescriptor alloc] init];
				depthDesc.depthWriteEnabled = depthInfo.writeEnabled ? YES : NO;
				depthDesc.depthCompareFunction = depthInfo.testEnabled
						? getMTLCompareFunction(core::CompareOp(depthInfo.compare))
						: MTLCompareFunctionAlways;

				id<MTLDepthStencilState> depthState =
						[dev.getDevice() newDepthStencilStateWithDescriptor:depthDesc];
				if (!depthState) {
					log::source().error("mtl::GraphicPipeline", _name,
							": fail to create depth-stencil state");
					return false;
				}
				_depthStencil = retainHandle(depthState);
			}
		}

		NSError *error = nil;
		id<MTLRenderPipelineState> pipeline =
				[dev.getDevice() newRenderPipelineStateWithDescriptor:desc error:&error];
		if (!pipeline) {
			releaseHandle(_depthStencil);
			_depthStencil = nullptr;
			log::source().error("mtl::GraphicPipeline", "Fail to create pipeline: ", _name, ": ",
					error ? StringView(error.localizedDescription.UTF8String) : StringView());
			return false;
		}

		_pipeline = retainHandle(pipeline);
	}

	return core::GraphicPipeline::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle ptr, void *depthStencil) {
		releaseHandle((void *)ptr.get());
		releaseHandle(depthStencil);
	}, core::ObjectType::Pipeline, core::ObjectHandle(_pipeline), _depthStencil);
}

bool ComputePipeline::init(Device &dev, const PipelineData &data) {
	_name = data.key.str<Interface>();

	id<MTLFunction> computeFn = nil;
	if (data.shader.data && data.shader.data->program
			&& data.shader.data->stage == core::ProgramStage::Compute) {
		computeFn = static_cast<Shader *>(data.shader.data->program.get())->getFunction();
	}

	if (!computeFn) {
		log::source().error("mtl::ComputePipeline", _name, ": no compute shader");
		return false;
	}

	@autoreleasepool {
		NSError *error = nil;
		id<MTLComputePipelineState> pipeline =
				[dev.getDevice() newComputePipelineStateWithFunction:computeFn error:&error];
		if (!pipeline) {
			log::source().error("mtl::ComputePipeline", _name,
					": fail to create compute pipeline: ",
					error ? StringView(error.localizedDescription.UTF8String) : StringView());
			return false;
		}

		_pipeline = retainHandle(pipeline);
	}

	return core::ComputePipeline::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle ptr, void *) {
		releaseHandle((void *)ptr.get());
	}, core::ObjectType::Pipeline, core::ObjectHandle(_pipeline));
}

} // namespace stappler::xenolith::mtl
