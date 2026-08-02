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

// Offscreen render smoke test for the Metal backend: compiles a queue with
// native MSL shaders through mtl::Loop, runs one frame through the frame
// graph and verifies the rendered triangle by pixel readback.

#include "SPCommon.h"
#include "SPBitmap.h"
#include "XLMtlInstance.h"
#include "XLMtlDevice.h"
#include "XLMtlLoop.h"
#include "XLMtlPipeline.h"
#include "XLMtlObject.h"
#include "XLMtlQueuePass.h"
#include "XLMtlTextureSet.h"
#include "XLMtlMaterial.h"
#include "XLCoreQueue.h"
#include "XLCoreResource.h"
#include "XLCoreQueuePass.h"
#include "XLCoreAttachment.h"
#include "XLMtlPlatform.h"
#include "XLMtlPresentation.h"
#include "XLMtlFontQueue.h"
#include "XL2dMtlVertexPass.h"
#include "XL2dFrameContext.h"
#include "XLCoreFrameRequest.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreImageStorage.h"
#include "XLCoreMaterial.h"
#include "XLCoreDynamicImage.h"
#include "XLCorePresentationEngine.h"
#include "XLCorePresentationFrame.h"

using namespace stappler;
using namespace stappler::xenolith;

static constexpr uint32_t RenderSize = 256;

// entry point is `main0`: `main` is reserved in MSL (see mtl::Shader)
static constexpr auto s_vertMsl = StringView(R"msl(
#include <metal_stdlib>
using namespace metal;

vertex float4 main0(uint idx [[vertex_id]]) {
	float2 positions[3] = {
		float2(-0.75, -0.75),
		float2(0.75, -0.75),
		float2(0.0, 0.75),
	};
	return float4(positions[idx], 0.0, 1.0);
}
)msl");

static constexpr auto s_fragMsl = StringView(R"msl(
#include <metal_stdlib>
using namespace metal;

fragment float4 main0() {
	return float4(1.0, 0.5, 0.0, 1.0);
}
)msl");

static constexpr auto s_fullscreenVertMsl = StringView(R"msl(
#include <metal_stdlib>
using namespace metal;

vertex float4 main0(uint idx [[vertex_id]]) {
	float2 positions[3] = {
		float2(-1.0, -1.0),
		float2(3.0, -1.0),
		float2(-1.0, 3.0),
	};
	return float4(positions[idx], 0.0, 1.0);
}
)msl");

// compute kernel: gradient + checkerboard into a storage texture; the logical
// 8x8 group size lives in the dispatch call (MSL has no in-shader equivalent
// of @workgroup_size)
static constexpr auto s_patternCompMsl = StringView(R"msl(
#include <metal_stdlib>
using namespace metal;

struct Params {
	uint cellShift;
};

kernel void main0(uint2 id [[thread_position_in_grid]],
		texture2d<float, access::write> image [[texture(0)]],
		constant Params &params [[buffer(0)]]) {
	if (id.x >= image.get_width() || id.y >= image.get_height()) {
		return;
	}
	float fx = float(id.x) / 255.0;
	float fy = float(id.y) / 255.0;
	float checker = float(((id.x >> params.cellShift) + (id.y >> params.cellShift)) % 2u);
	image.write(float4(fx, fy, checker, 1.0), uint2(id.xy));
}
)msl");

// samples the compute-produced pattern into the color output
static constexpr auto s_resolveFragMsl = StringView(R"msl(
#include <metal_stdlib>
using namespace metal;

fragment float4 main0(float4 pos [[position]],
		texture2d<float> tex [[texture(0)]],
		sampler samp [[sampler(0)]]) {
	return tex.sample(samp, pos.xy / 256.0, level(0.0));
}
)msl");

// texture set argument buffer: the MSL struct must match the engine's layout
// contract (samplers first, then textures - see mtl::TextureSet); the uniform
// buffer descriptor takes [[buffer(0)]], the set itself [[buffer(30)]]
static constexpr auto s_texArrayFragMsl = StringView(R"msl(
#include <metal_stdlib>
using namespace metal;

struct Params {
	uint index;
};

struct TextureSetArgs {
	array<sampler, 2> samplers;
	array<texture2d<float>, 8> textures;
};

fragment float4 main0(float4 pos [[position]],
		constant Params &params [[buffer(0)]],
		constant TextureSetArgs &texSet [[buffer(30)]]) {
	return texSet.textures[params.index].sample(texSet.samplers[0], pos.xy / 256.0);
}
)msl");

// pack MSL text into ProgramData's uint32_t words
static Vector<uint32_t> packMsl(StringView code) {
	Vector<uint32_t> ret;
	ret.resize((code.size() + sizeof(uint32_t) - 1) / sizeof(uint32_t), 0);
	sprt::memcpy(ret.data(), code.data(), code.size());
	return ret;
}

static bool checkPixel(const uint8_t *pixel, uint8_t r, uint8_t g, uint8_t b, int tolerance) {
	return sprt::abs(int(pixel[0]) - int(r)) <= tolerance
			&& sprt::abs(int(pixel[1]) - int(g)) <= tolerance
			&& sprt::abs(int(pixel[2]) - int(b)) <= tolerance;
}

static Rc<core::Queue> makeTriangleQueue() {
	core::Queue::Builder builder("OffscreenQueue");

	core::ProgramInfo vertInfo;
	vertInfo.stage = core::ProgramStage::Vertex;

	core::ProgramInfo fragInfo;
	fragInfo.stage = core::ProgramStage::Fragment;

	auto vertData = packMsl(s_vertMsl);
	auto fragData = packMsl(s_fragMsl);

	auto vertProg = builder.addProgram("TriangleVert", vertData, &vertInfo);
	auto fragProg = builder.addProgram("TriangleFrag", fragData, &fragInfo);

	auto attachment = builder.addAttachemnt("Output",
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		attachmentBuilder.defineAsOutput();
		return Rc<mtl::ImageAttachment>::create(attachmentBuilder,
				core::ImageInfo(Extent2(RenderSize, RenderSize), core::ImageFormat::R8G8B8A8_UNORM,
						core::ImageHints::FixedSize,
						core::ImageUsage::ColorAttachment | core::ImageUsage::TransferSrc),
				core::ImageAttachment::AttachmentInfo{
					.initialLayout = core::AttachmentLayout::Undefined,
					.finalLayout = core::AttachmentLayout::TransferSrcOptimal,
					.clearOnLoad = true,
					.clearColor = Color4F(0.1f, 0.2f, 0.3f, 1.0f),
				});
	});

	builder.addPass("OffscreenPass", core::PassType::Graphics, core::RenderOrdering(0),
			[&](core::QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		auto colorAttachment = passBuilder.addAttachment(attachment);

		auto layout = passBuilder.addDescriptorLayout("EmptyLayout",
				[](core::PipelineLayoutBuilder &) { });

		passBuilder.addSubpass([&](core::SubpassBuilder &subpassBuilder) {
			subpassBuilder.addColor(colorAttachment,
					core::AttachmentDependencyInfo{
						core::PipelineStage::ColorAttachmentOutput,
						core::AccessType::ColorAttachmentWrite,
						core::PipelineStage::ColorAttachmentOutput,
						core::AccessType::ColorAttachmentWrite,
						core::FrameRenderPassState::Submitted,
					},
					core::AttachmentLayout::ColorAttachmentOptimal);

			subpassBuilder.addGraphicPipeline("TrianglePipeline", layout->defaultFamily,
					Vector<core::SpecializationInfo>({
						core::SpecializationInfo(vertProg),
						core::SpecializationInfo(fragProg),
					}),
					core::PipelineMaterialInfo());

			subpassBuilder.setCommandsCallback(
					[](core::FrameQueue &frameQueue, const core::SubpassData &subpass,
							core::CommandBuffer &commands) {
				auto &buf = static_cast<mtl::CommandBuffer &>(commands);
				if (auto pipeline = subpass.graphicPipelines.get("TrianglePipeline")) {
					buf.cmdBindPipeline(pipeline);
					buf.cmdDraw(3);
				}
			});
		});

		return Rc<mtl::QueuePass>::create(passBuilder);
	});

	return Rc<core::Queue>::create(move(builder));
}

// two passes: compute writes pattern (parameters from uniform buffer)
// into storage texture, then graphics pass samples it into the output
static Rc<core::Queue> makeComputeQueue() {
	core::Queue::Builder builder("ComputeQueue");

	core::ProgramInfo compInfo;
	compInfo.stage = core::ProgramStage::Compute;

	core::ProgramInfo vertInfo;
	vertInfo.stage = core::ProgramStage::Vertex;

	core::ProgramInfo fragInfo;
	fragInfo.stage = core::ProgramStage::Fragment;

	auto compData = packMsl(s_patternCompMsl);
	auto vertData = packMsl(s_fullscreenVertMsl);
	auto fragData = packMsl(s_resolveFragMsl);

	auto compProg = builder.addProgram("PatternComp", compData, &compInfo);
	auto vertProg = builder.addProgram("ResolveVert", vertData, &vertInfo);
	auto fragProg = builder.addProgram("ResolveFrag", fragData, &fragInfo);

	// uniform parameters as static buffer in queue's internal resource
	static const uint32_t s_params[4] = {5, 0, 0, 0}; // cellShift = 5 -> 32px cells

	auto paramsData = builder.addBuffer("ParamsData",
			core::BufferInfo(core::BufferUsage::UniformBuffer, core::PassType::Compute,
					uint64_t(sizeof(s_params))),
			BytesView(reinterpret_cast<const uint8_t *>(s_params), sizeof(s_params)));

	auto paramsAttachment = builder.addAttachemnt("Params",
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		return Rc<mtl::BufferAttachment>::create(attachmentBuilder, paramsData);
	});

	auto patternAttachment = builder.addAttachemnt("Pattern",
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		return Rc<mtl::ImageAttachment>::create(attachmentBuilder,
				core::ImageInfo(Extent2(RenderSize, RenderSize), core::ImageFormat::R8G8B8A8_UNORM,
						core::ImageHints::FixedSize,
						core::ImageUsage::Storage | core::ImageUsage::Sampled),
				core::ImageAttachment::AttachmentInfo{
					.initialLayout = core::AttachmentLayout::Undefined,
					.finalLayout = core::AttachmentLayout::ShaderReadOnlyOptimal,
					.clearOnLoad = false,
					.clearColor = Color4F::BLACK,
				});
	});

	auto outputAttachment = builder.addAttachemnt("Output",
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		attachmentBuilder.defineAsOutput();
		return Rc<mtl::ImageAttachment>::create(attachmentBuilder,
				core::ImageInfo(Extent2(RenderSize, RenderSize), core::ImageFormat::R8G8B8A8_UNORM,
						core::ImageHints::FixedSize,
						core::ImageUsage::ColorAttachment | core::ImageUsage::TransferSrc),
				core::ImageAttachment::AttachmentInfo{
					.initialLayout = core::AttachmentLayout::Undefined,
					.finalLayout = core::AttachmentLayout::TransferSrcOptimal,
					.clearOnLoad = true,
					.clearColor = Color4F::BLACK,
				});
	});

	builder.addPass("PatternPass", core::PassType::Compute, core::RenderOrdering(0),
			[&](core::QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		auto patternAtt = passBuilder.addAttachment(patternAttachment);
		auto paramsAtt = passBuilder.addAttachment(paramsAttachment);

		auto layout = passBuilder.addDescriptorLayout("PatternLayout",
				[&](core::PipelineLayoutBuilder &layoutBuilder) {
			layoutBuilder.addSet([&](core::DescriptorSetBuilder &setBuilder) {
				setBuilder.addDescriptor(patternAtt, core::DescriptorType::StorageImage,
						core::AttachmentLayout::General);
				setBuilder.addDescriptor(paramsAtt, core::DescriptorType::UniformBuffer);
			});
		});

		passBuilder.addSubpass([&](core::SubpassBuilder &subpassBuilder) {
			subpassBuilder.addComputePipeline("PatternPipeline", layout->defaultFamily,
					core::SpecializationInfo(compProg));

			subpassBuilder.setCommandsCallback(
					[](core::FrameQueue &frameQueue, const core::SubpassData &subpass,
							core::CommandBuffer &commands) {
				auto &buf = static_cast<mtl::CommandBuffer &>(commands);
				if (auto pipeline = subpass.computePipelines.get("PatternPipeline")) {
					buf.cmdBindPipeline(pipeline);
					// 8x8 logical group size, matches the kernel's dispatch math
					buf.cmdDispatch(RenderSize / 8, RenderSize / 8, 1, 8, 8, 1);
				}
			});
		});

		return Rc<mtl::QueuePass>::create(passBuilder);
	});

	builder.addPass("ResolvePass", core::PassType::Graphics, core::RenderOrdering(1),
			[&](core::QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		auto patternAtt = passBuilder.addAttachment(patternAttachment);
		auto colorAtt = passBuilder.addAttachment(outputAttachment);

		auto layout = passBuilder.addDescriptorLayout("ResolveLayout",
				[&](core::PipelineLayoutBuilder &layoutBuilder) {
			layoutBuilder.addSet([&](core::DescriptorSetBuilder &setBuilder) {
				setBuilder.addDescriptor(patternAtt, core::DescriptorType::SampledImage,
						core::AttachmentLayout::ShaderReadOnlyOptimal);
				setBuilder.addSampler("PatternSampler",
						core::SamplerInfo{.magFilter = core::Filter::Nearest,
							.minFilter = core::Filter::Nearest});
			});
		});

		passBuilder.addSubpass([&](core::SubpassBuilder &subpassBuilder) {
			subpassBuilder.addColor(colorAtt,
					core::AttachmentDependencyInfo{
						core::PipelineStage::ColorAttachmentOutput,
						core::AccessType::ColorAttachmentWrite,
						core::PipelineStage::ColorAttachmentOutput,
						core::AccessType::ColorAttachmentWrite,
						core::FrameRenderPassState::Submitted,
					},
					core::AttachmentLayout::ColorAttachmentOptimal);

			subpassBuilder.addGraphicPipeline("ResolvePipeline", layout->defaultFamily,
					Vector<core::SpecializationInfo>({
						core::SpecializationInfo(vertProg),
						core::SpecializationInfo(fragProg),
					}),
					core::PipelineMaterialInfo());

			subpassBuilder.setCommandsCallback(
					[](core::FrameQueue &frameQueue, const core::SubpassData &subpass,
							core::CommandBuffer &commands) {
				auto &buf = static_cast<mtl::CommandBuffer &>(commands);
				if (auto pipeline = subpass.graphicPipelines.get("ResolvePipeline")) {
					buf.cmdBindPipeline(pipeline);
					buf.cmdDraw(3);
				}
			});
		});

		return Rc<mtl::QueuePass>::create(passBuilder);
	});

	return Rc<core::Queue>::create(move(builder));
}

// TextureSet demo: static red/green textures in a texture set (argument
// buffer), fragment shader picks one by index from the uniform buffer
static Rc<core::TextureSet> s_textureSet;

static Rc<core::Queue> makeTextureSetQueue() {
	core::Queue::Builder builder("TextureSetQueue");

	core::ProgramInfo vertInfo;
	vertInfo.stage = core::ProgramStage::Vertex;

	core::ProgramInfo fragInfo;
	fragInfo.stage = core::ProgramStage::Fragment;

	auto vertData = packMsl(s_fullscreenVertMsl);
	auto fragData = packMsl(s_texArrayFragMsl);

	auto vertProg = builder.addProgram("TexArrayVert", vertData, &vertInfo);
	auto fragProg = builder.addProgram("TexArrayFrag", fragData, &fragInfo);

	// static 1x1 textures
	static const uint8_t s_red[4] = {255, 0, 0, 255};
	static const uint8_t s_green[4] = {0, 255, 0, 255};

	auto redImage = builder.addBitmapImageByRef("RedImage",
			core::ImageInfo(Extent2(1, 1), core::ImageFormat::R8G8B8A8_UNORM,
					core::ImageUsage::Sampled),
			BytesView(s_red, sizeof(s_red)));
	builder.addImageView(redImage, core::ImageViewInfo());

	auto greenImage = builder.addBitmapImageByRef("GreenImage",
			core::ImageInfo(Extent2(1, 1), core::ImageFormat::R8G8B8A8_UNORM,
					core::ImageUsage::Sampled),
			BytesView(s_green, sizeof(s_green)));
	builder.addImageView(greenImage, core::ImageViewInfo());

	// texture set layout: 2 samplers, 8 images (matches the MSL argument struct)
	core::SamplerInfo samplers[2];
	samplers[0] = core::SamplerInfo{.magFilter = core::Filter::Nearest,
		.minFilter = core::Filter::Nearest};
	samplers[1] =
			core::SamplerInfo{.magFilter = core::Filter::Linear, .minFilter = core::Filter::Linear};

	auto texLayout = builder.addTextureSetLayout("TexSet", makeSpanView(samplers, 2), 8,
			xenolith::config::MaxBufferArrayObjects, 8);

	// texture index for the shader
	static const uint32_t s_texParams[4] = {1, 0, 0, 0}; // green

	auto paramsData = builder.addBuffer("TexParamsData",
			core::BufferInfo(core::BufferUsage::UniformBuffer, uint64_t(sizeof(s_texParams))),
			BytesView(reinterpret_cast<const uint8_t *>(s_texParams), sizeof(s_texParams)));

	auto paramsAttachment = builder.addAttachemnt("Params",
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		return Rc<mtl::BufferAttachment>::create(attachmentBuilder, paramsData);
	});

	auto outputAttachment = builder.addAttachemnt("Output",
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		attachmentBuilder.defineAsOutput();
		return Rc<mtl::ImageAttachment>::create(attachmentBuilder,
				core::ImageInfo(Extent2(RenderSize, RenderSize), core::ImageFormat::R8G8B8A8_UNORM,
						core::ImageHints::FixedSize,
						core::ImageUsage::ColorAttachment | core::ImageUsage::TransferSrc),
				core::ImageAttachment::AttachmentInfo{
					.initialLayout = core::AttachmentLayout::Undefined,
					.finalLayout = core::AttachmentLayout::TransferSrcOptimal,
					.clearOnLoad = true,
					.clearColor = Color4F::BLACK,
				});
	});

	builder.addPass("TexArrayPass", core::PassType::Graphics, core::RenderOrdering(0),
			[&](core::QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		auto colorAtt = passBuilder.addAttachment(outputAttachment);
		auto paramsAtt = passBuilder.addAttachment(paramsAttachment);

		auto layout = passBuilder.addDescriptorLayout("TexArrayLayout",
				[&](core::PipelineLayoutBuilder &layoutBuilder) {
			layoutBuilder.addSet([&](core::DescriptorSetBuilder &setBuilder) {
				setBuilder.addDescriptor(paramsAtt, core::DescriptorType::UniformBuffer);
			});
			layoutBuilder.setTextureSetLayout(texLayout);
		});

		passBuilder.addSubpass([&](core::SubpassBuilder &subpassBuilder) {
			subpassBuilder.addColor(colorAtt,
					core::AttachmentDependencyInfo{
						core::PipelineStage::ColorAttachmentOutput,
						core::AccessType::ColorAttachmentWrite,
						core::PipelineStage::ColorAttachmentOutput,
						core::AccessType::ColorAttachmentWrite,
						core::FrameRenderPassState::Submitted,
					},
					core::AttachmentLayout::ColorAttachmentOptimal);

			subpassBuilder.addGraphicPipeline("TexArrayPipeline", layout->defaultFamily,
					Vector<core::SpecializationInfo>({
						core::SpecializationInfo(vertProg),
						core::SpecializationInfo(fragProg),
					}),
					core::PipelineMaterialInfo());

			subpassBuilder.setCommandsCallback(
					[](core::FrameQueue &frameQueue, const core::SubpassData &subpass,
							core::CommandBuffer &commands) {
				auto &buf = static_cast<mtl::CommandBuffer &>(commands);
				if (auto pipeline = subpass.graphicPipelines.get("TexArrayPipeline")) {
					buf.cmdBindPipeline(pipeline);
					buf.cmdBindTextureSet(pipeline->layout, s_textureSet.get());
					buf.cmdDraw(3);
				}
			});
		});

		return Rc<mtl::QueuePass>::create(passBuilder);
	});

	return Rc<core::Queue>::create(move(builder));
}

// Material system demo: two predefined materials (red/green textures),
// compiled through the material machinery; fragment samples the green
// material's texture set slot
static Rc<core::TextureSet> s_materialTextureSet;
static const core::MaterialAttachment *s_materialAttachment = nullptr;

class TestMaterialAttachment : public mtl::MaterialAttachment {
public:
	virtual ~TestMaterialAttachment() = default;

	// simplified material data: [ samplerImageIdx, setIdx, 0, 0 ]
	virtual Bytes getMaterialData(NotNull<core::Material> m) const override {
		Bytes ret;
		ret.resize(16);
		auto &images = m->getImages();
		if (!images.empty()) {
			auto &image = images.front();
			uint32_t data[4] = {image.descriptor | (uint32_t(image.sampler) << 16), image.set, 0,
				0};
			sprt::memcpy(ret.data(), data, sizeof(data));
		}
		return ret;
	}
};

static Rc<core::Queue> makeMaterialQueue() {
	core::Queue::Builder builder("MaterialQueue");

	core::ProgramInfo vertInfo;
	vertInfo.stage = core::ProgramStage::Vertex;

	core::ProgramInfo fragInfo;
	fragInfo.stage = core::ProgramStage::Fragment;

	auto vertData = packMsl(s_fullscreenVertMsl);
	auto fragData = packMsl(s_texArrayFragMsl);

	auto vertProg = builder.addProgram("MaterialVert", vertData, &vertInfo);
	auto fragProg = builder.addProgram("MaterialFrag", fragData, &fragInfo);

	static const uint8_t s_red[4] = {255, 0, 0, 255};
	static const uint8_t s_green[4] = {0, 255, 0, 255};

	auto redImage = builder.addBitmapImageByRef("RedImage",
			core::ImageInfo(Extent2(1, 1), core::ImageFormat::R8G8B8A8_UNORM,
					core::ImageUsage::Sampled),
			BytesView(s_red, sizeof(s_red)));
	auto greenImage = builder.addBitmapImageByRef("GreenImage",
			core::ImageInfo(Extent2(1, 1), core::ImageFormat::R8G8B8A8_UNORM,
					core::ImageUsage::Sampled),
			BytesView(s_green, sizeof(s_green)));

	core::SamplerInfo samplers[2];
	samplers[0] = core::SamplerInfo{.magFilter = core::Filter::Nearest,
		.minFilter = core::Filter::Nearest};
	samplers[1] =
			core::SamplerInfo{.magFilter = core::Filter::Linear, .minFilter = core::Filter::Linear};

	auto texLayout = builder.addTextureSetLayout("TexSet", makeSpanView(samplers, 2), 8,
			xenolith::config::MaxBufferArrayObjects, 8);

	const core::MaterialAttachment *materialAttachment = nullptr;
	auto materialsAtt = builder.addAttachemnt("Materials",
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		auto a = Rc<TestMaterialAttachment>::create(attachmentBuilder, texLayout);
		materialAttachment = a.get();
		return a;
	});
	s_materialAttachment = materialAttachment;

	// texture index for the shader, updated after materials compilation
	static const uint32_t s_matParams[4] = {0, 0, 0, 0};

	auto paramsData = builder.addBuffer("MatParamsData",
			core::BufferInfo(core::BufferUsage::UniformBuffer | core::BufferUsage::TransferDst,
					uint64_t(sizeof(s_matParams))),
			BytesView(reinterpret_cast<const uint8_t *>(s_matParams), sizeof(s_matParams)));

	auto paramsAttachment = builder.addAttachemnt("Params",
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		return Rc<mtl::BufferAttachment>::create(attachmentBuilder, paramsData);
	});

	auto outputAttachment = builder.addAttachemnt("Output",
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		attachmentBuilder.defineAsOutput();
		return Rc<mtl::ImageAttachment>::create(attachmentBuilder,
				core::ImageInfo(Extent2(RenderSize, RenderSize), core::ImageFormat::R8G8B8A8_UNORM,
						core::ImageHints::FixedSize,
						core::ImageUsage::ColorAttachment | core::ImageUsage::TransferSrc),
				core::ImageAttachment::AttachmentInfo{
					.initialLayout = core::AttachmentLayout::Undefined,
					.finalLayout = core::AttachmentLayout::TransferSrcOptimal,
					.clearOnLoad = true,
					.clearColor = Color4F::BLACK,
				});
	});

	builder.addPass("MaterialPass", core::PassType::Graphics, core::RenderOrdering(0),
			[&](core::QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		auto colorAtt = passBuilder.addAttachment(outputAttachment);
		auto paramsAtt = passBuilder.addAttachment(paramsAttachment);
		passBuilder.addAttachment(materialsAtt);

		auto layout = passBuilder.addDescriptorLayout("MaterialLayout",
				[&](core::PipelineLayoutBuilder &layoutBuilder) {
			layoutBuilder.addSet([&](core::DescriptorSetBuilder &setBuilder) {
				setBuilder.addDescriptor(paramsAtt, core::DescriptorType::UniformBuffer);
			});
			layoutBuilder.setTextureSetLayout(texLayout);
		});

		passBuilder.addSubpass([&](core::SubpassBuilder &subpassBuilder) {
			subpassBuilder.addColor(colorAtt,
					core::AttachmentDependencyInfo{
						core::PipelineStage::ColorAttachmentOutput,
						core::AccessType::ColorAttachmentWrite,
						core::PipelineStage::ColorAttachmentOutput,
						core::AccessType::ColorAttachmentWrite,
						core::FrameRenderPassState::Submitted,
					},
					core::AttachmentLayout::ColorAttachmentOptimal);

			auto pipeline =
					subpassBuilder.addGraphicPipeline("MaterialPipeline", layout->defaultFamily,
							Vector<core::SpecializationInfo>({
								core::SpecializationInfo(vertProg),
								core::SpecializationInfo(fragProg),
							}),
							core::PipelineMaterialInfo());

			// predefined materials, compiled with the queue
			const_cast<core::MaterialAttachment *>(materialAttachment)
					->addPredefinedMaterials(Vector<Rc<core::Material>>({
						Rc<core::Material>::create(core::MaterialId(1), pipeline, redImage),
						Rc<core::Material>::create(core::MaterialId(2), pipeline, greenImage),
					}));

			subpassBuilder.setCommandsCallback(
					[](core::FrameQueue &frameQueue, const core::SubpassData &subpass,
							core::CommandBuffer &commands) {
				auto &buf = static_cast<mtl::CommandBuffer &>(commands);
				if (auto pipeline = subpass.graphicPipelines.get("MaterialPipeline")) {
					buf.cmdBindPipeline(pipeline);
					buf.cmdBindTextureSet(pipeline->layout, s_materialTextureSet.get());
					buf.cmdDraw(3);
				}
			});
		});

		return Rc<mtl::QueuePass>::create(passBuilder);
	});

	return Rc<core::Queue>::create(move(builder));
}

// 2d renderer slice: material quads through basic2d::mtl::MaterialVertexPass
// (command list -> storage buffers -> spans), with a scissored quad on top
static Rc<core::Queue> makeBasic2dQueue() {
	core::Queue::Builder builder("Basic2dQueue");

	core::ProgramInfo vertInfo;
	vertInfo.stage = core::ProgramStage::Vertex;

	core::ProgramInfo fragInfo;
	fragInfo.stage = core::ProgramStage::Fragment;

	auto vertData = packMsl(basic2d::mtl::getMaterialVertexShader());
	// the argument buffer struct must match the layout below (2 samplers, 8 images)
	auto fragSource = basic2d::mtl::getMaterialFragmentShader(2, 8);
	auto fragData = packMsl(fragSource);

	auto vertProg = builder.addProgram("Material2dVert", vertData, &vertInfo);
	auto fragProg = builder.addProgram("Material2dFrag", fragData, &fragInfo);

	static const uint8_t s_red[4] = {255, 0, 0, 255};
	static const uint8_t s_green[4] = {0, 255, 0, 255};

	auto redImage = builder.addBitmapImageByRef("RedImage",
			core::ImageInfo(Extent2(1, 1), core::ImageFormat::R8G8B8A8_UNORM,
					core::ImageUsage::Sampled),
			BytesView(s_red, sizeof(s_red)));
	auto greenImage = builder.addBitmapImageByRef("GreenImage",
			core::ImageInfo(Extent2(1, 1), core::ImageFormat::R8G8B8A8_UNORM,
					core::ImageUsage::Sampled),
			BytesView(s_green, sizeof(s_green)));

	core::SamplerInfo samplers[2];
	samplers[0] = core::SamplerInfo{.magFilter = core::Filter::Nearest,
		.minFilter = core::Filter::Nearest};
	samplers[1] =
			core::SamplerInfo{.magFilter = core::Filter::Linear, .minFilter = core::Filter::Linear};

	auto texLayout = builder.addTextureSetLayout("TexSet", makeSpanView(samplers, 2), 8,
			xenolith::config::MaxBufferArrayObjects, 8);

	const core::MaterialAttachment *materialAttachment = nullptr;
	auto materialsAtt = builder.addAttachemnt("Materials",
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		auto a = Rc<TestMaterialAttachment>::create(attachmentBuilder, texLayout);
		materialAttachment = a.get();
		return a;
	});
	s_materialAttachment = materialAttachment;

	auto vertexesAtt = builder.addAttachemnt("Vertexes",
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		attachmentBuilder.defineAsInput();
		return Rc<basic2d::mtl::VertexAttachment>::create(attachmentBuilder, materialsAtt);
	});

	auto outputAttachment = builder.addAttachemnt("Output",
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		attachmentBuilder.defineAsOutput();
		return Rc<mtl::ImageAttachment>::create(attachmentBuilder,
				core::ImageInfo(Extent2(RenderSize, RenderSize), core::ImageFormat::R8G8B8A8_UNORM,
						core::ImageHints::FixedSize,
						core::ImageUsage::ColorAttachment | core::ImageUsage::TransferSrc),
				core::ImageAttachment::AttachmentInfo{
					.initialLayout = core::AttachmentLayout::Undefined,
					.finalLayout = core::AttachmentLayout::TransferSrcOptimal,
					.clearOnLoad = true,
					.clearColor = Color4F::BLACK,
				});
	});

	builder.addPass("Vertex2dPass", core::PassType::Graphics, core::RenderOrdering(0),
			[&](core::QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		auto colorAtt = passBuilder.addAttachment(outputAttachment);
		auto vertexesPassAtt = passBuilder.addAttachment(vertexesAtt);
		passBuilder.addAttachment(materialsAtt);

		auto layout = passBuilder.addDescriptorLayout("Vertex2dLayout",
				[&](core::PipelineLayoutBuilder &layoutBuilder) {
			layoutBuilder.addSet([&](core::DescriptorSetBuilder &setBuilder) {
				// vertices, transforms, spans, combined data atlases
				setBuilder.addDescriptor(vertexesPassAtt, core::DescriptorType::StorageBuffer);
				setBuilder.addDescriptor(vertexesPassAtt, core::DescriptorType::StorageBuffer);
				setBuilder.addDescriptor(vertexesPassAtt, core::DescriptorType::StorageBuffer);
				setBuilder.addDescriptor(vertexesPassAtt, core::DescriptorType::StorageBuffer);
			});
			layoutBuilder.setTextureSetLayout(texLayout);
		});

		passBuilder.addSubpass([&](core::SubpassBuilder &subpassBuilder) {
			subpassBuilder.addColor(colorAtt,
					core::AttachmentDependencyInfo{
						core::PipelineStage::ColorAttachmentOutput,
						core::AccessType::ColorAttachmentWrite,
						core::PipelineStage::ColorAttachmentOutput,
						core::AccessType::ColorAttachmentWrite,
						core::FrameRenderPassState::Submitted,
					},
					core::AttachmentLayout::ColorAttachmentOptimal);

			auto pipeline =
					subpassBuilder.addGraphicPipeline("Material2dPipeline", layout->defaultFamily,
							Vector<core::SpecializationInfo>({
								core::SpecializationInfo(vertProg),
								core::SpecializationInfo(fragProg),
							}),
							core::PipelineMaterialInfo());

			const_cast<core::MaterialAttachment *>(materialAttachment)
					->addPredefinedMaterials(Vector<Rc<core::Material>>({
						Rc<core::Material>::create(core::MaterialId(1), pipeline, redImage),
						Rc<core::Material>::create(core::MaterialId(2), pipeline, greenImage),
					}));
		});

		return Rc<basic2d::mtl::MaterialVertexPass>::create(passBuilder, vertexesAtt, materialsAtt);
	});

	return Rc<core::Queue>::create(move(builder));
}

static Rc<basic2d::VertexData> makeQuad(float x0, float y0, float x1, float y1) {
	auto data = Rc<basic2d::VertexData>::alloc();
	data->data.resize(4);
	auto set = [&](size_t i, float x, float y, float u, float v) {
		data->data[i].pos = Vec4(x, y, 0.0f, 1.0f);
		data->data[i].color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
		data->data[i].tex = Vec2(u, v);
		data->data[i].material = 0;
		data->data[i].object = 0;
	};
	set(0, x0, y0, 0.0f, 0.0f);
	set(1, x1, y0, 1.0f, 0.0f);
	set(2, x1, y1, 1.0f, 1.0f);
	set(3, x0, y1, 0.0f, 1.0f);
	data->indexes = Vector<uint32_t>{0, 1, 2, 2, 3, 0};
	return data;
}

// present-capable triangle queue: output attachment with PresentSrc final
// layout, format matches the swapchain
static Rc<core::Queue> makePresentQueue(core::ImageFormat format) {
	core::Queue::Builder builder("PresentQueue");

	core::ProgramInfo vertInfo;
	vertInfo.stage = core::ProgramStage::Vertex;

	core::ProgramInfo fragInfo;
	fragInfo.stage = core::ProgramStage::Fragment;

	auto vertData = packMsl(s_vertMsl);
	auto fragData = packMsl(s_fragMsl);

	auto vertProg = builder.addProgram("TriangleVert", vertData, &vertInfo);
	auto fragProg = builder.addProgram("TriangleFrag", fragData, &fragInfo);

	auto attachment = builder.addAttachemnt("Output",
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		attachmentBuilder.defineAsOutput();
		return Rc<mtl::ImageAttachment>::create(attachmentBuilder,
				core::ImageInfo(Extent2(RenderSize, RenderSize), format,
						core::ImageUsage::ColorAttachment),
				core::ImageAttachment::AttachmentInfo{
					.initialLayout = core::AttachmentLayout::Undefined,
					.finalLayout = core::AttachmentLayout::PresentSrc,
					.clearOnLoad = true,
					.clearColor = Color4F(0.1f, 0.2f, 0.3f, 1.0f),
				});
	});

	builder.addPass("PresentPass", core::PassType::Graphics, core::RenderOrdering(0),
			[&](core::QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		auto colorAttachment = passBuilder.addAttachment(attachment);

		auto layout = passBuilder.addDescriptorLayout("EmptyLayout",
				[](core::PipelineLayoutBuilder &) { });

		passBuilder.addSubpass([&](core::SubpassBuilder &subpassBuilder) {
			subpassBuilder.addColor(colorAttachment,
					core::AttachmentDependencyInfo{
						core::PipelineStage::ColorAttachmentOutput,
						core::AccessType::ColorAttachmentWrite,
						core::PipelineStage::ColorAttachmentOutput,
						core::AccessType::ColorAttachmentWrite,
						core::FrameRenderPassState::Submitted,
					},
					core::AttachmentLayout::ColorAttachmentOptimal);

			subpassBuilder.addGraphicPipeline("TrianglePipeline", layout->defaultFamily,
					Vector<core::SpecializationInfo>({
						core::SpecializationInfo(vertProg),
						core::SpecializationInfo(fragProg),
					}),
					core::PipelineMaterialInfo());

			subpassBuilder.setCommandsCallback(
					[](core::FrameQueue &frameQueue, const core::SubpassData &subpass,
							core::CommandBuffer &commands) {
				auto &buf = static_cast<mtl::CommandBuffer &>(commands);
				if (auto pipeline = subpass.graphicPipelines.get("TrianglePipeline")) {
					buf.cmdBindPipeline(pipeline);
					buf.cmdDraw(3);
				}
			});
		});

		return Rc<mtl::QueuePass>::create(passBuilder);
	});

	return Rc<core::Queue>::create(move(builder));
}

// presentation window over a standalone (headless) CAMetalLayer: nextDrawable
// and presentDrawable work without a backing view, so the full swapchain
// cycle runs without opening a window
class TestPresentationWindow : public Ref, public core::PresentationWindow {
public:
	TestPresentationWindow(void *layer, mtl::Loop *loop, Extent2 extent)
	: _layer(layer), _loop(loop), _extent(extent) { }

	virtual core::ImageInfo getSwapchainImageInfo(const core::SwapchainConfig &cfg) const override {
		core::ImageInfo info;
		info.format = cfg.imageFormat;
		info.imageType = core::ImageType::Image2D;
		info.extent = Extent3(cfg.extent.width, cfg.extent.height, 1);
		info.arrayLayers = core::ArrayLayers(1);
		info.usage = core::ImageUsage::ColorAttachment;
		return info;
	}

	virtual core::ImageViewInfo getSwapchainImageViewInfo(
			const core::ImageInfo &image) const override {
		core::ImageViewInfo info;
		info.type = core::ImageViewType::ImageView2D;
		return image.getViewInfo(info);
	}

	virtual core::SurfaceInfo getSurfaceOptions(const core::Device &dev,
			NotNull<core::Surface> surface) const override {
		auto info =
				surface->getSurfaceOptions(dev, core::FullScreenExclusiveMode::Default, nullptr);
		info.currentExtent = _extent;
		info.minImageExtent = _extent;
		info.maxImageExtent = _extent;
		return info;
	}

	virtual core::SwapchainConfig selectConfig(const core::SurfaceInfo &info, bool) override {
		core::SwapchainConfig cfg;
		cfg.extent = info.currentExtent;
		cfg.imageCount = sprt::max(info.minImageCount, 2U);
		cfg.transfer = false;
		cfg.alpha = core::CompositeAlphaFlags::Opaque;

		cfg.imageFormat = info.formats.front().first;
		for (auto &it : info.formats) {
			if (it.first == core::ImageFormat::B8G8R8A8_UNORM) {
				cfg.imageFormat = it.first;
				break;
			}
		}

		cfg.presentMode = info.presentModes.front();
		for (auto &it : info.presentModes) {
			if (it == core::PresentMode::Fifo) {
				cfg.presentMode = it;
				break;
			}
		}

		_selectedFormat = cfg.imageFormat;
		return cfg;
	}

	virtual void acquireFrameData(NotNull<core::PresentationFrame> frame,
			Function<void(NotNull<core::PresentationFrame>)> &&cb) override {
		if (!_queue) {
			_queue = makePresentQueue(_selectedFormat);
			bool ok = false;
			_loop->compileQueue(_queue, [&](bool success) { ok = success; });
			if (!ok) {
				sprt::cerr << "Fail to compile present queue\n";
				_queue = nullptr;
				frame->invalidate();
				return;
			}
		}
		frame->getRequest()->setQueue(_queue);
		cb(frame);
	}

	virtual void handleFrameReady(NotNull<core::PresentationFrame>) override { }

	virtual void handleFramePresented(NotNull<core::PresentationFrame>) override {
		++presentedFrames;
	}

	virtual void handleSwapchainUpdated(const core::FrameConstraints &) override { }

	virtual Rc<core::Surface> makeSurface(NotNull<core::Instance> cinstance) override {
		return Rc<mtl::Surface>::create(static_cast<mtl::Instance *>(cinstance.get()), _layer);
	}

	virtual core::FrameConstraints exportConstraints(uint64_t &serial) const override {
		core::FrameConstraints c;
		c.extent = Extent3(_extent.width, _extent.height, 1);
		return c;
	}

	virtual void setFrameOrder(uint64_t value) override { _frameOrder = value; }

	uint32_t presentedFrames = 0;

protected:
	void *_layer = nullptr;
	mtl::Loop *_loop = nullptr;
	Rc<core::Queue> _queue;
	Extent2 _extent;
	uint64_t _frameOrder = 0;
	core::ImageFormat _selectedFormat = core::ImageFormat::B8G8R8A8_UNORM;
};

// compile queue, run one frame through the frame graph via the Loop, read
// back the output attachment (Loop::captureImage) and verify its content
static int runOffscreenQueue(sprt::dispatch::Looper *looper, mtl::Loop *mtlLoop,
		const Rc<core::Queue> &queue, StringView pngFile,
		const Function<bool(const uint8_t *data, uint64_t bytesPerRow)> &verify,
		const Function<bool(const Rc<core::Queue> &)> &onCompiled = nullptr,
		const Function<void(core::FrameRequest &)> &onRequest = nullptr) {
	bool compiled = false;
	mtlLoop->compileQueue(queue, [&](bool success) {
		sprt::cout << queue->getName().data() << " compiled: " << (success ? "true" : "false")
				   << "\n";
		compiled = success;
	});

	if (!compiled) {
		return -7;
	}

	if (onCompiled && !onCompiled(queue)) {
		return -15;
	}

	int renderResult = -8;
	bool captureComplete = false;
	bool frameComplete = false;

	auto outputAttachment = queue->getAttachment("Output");
	if (!outputAttachment) {
		sprt::cerr << "Output attachment not found\n";
		return -9;
	}

	auto req = Rc<core::FrameRequest>::create(queue);
	if (onRequest) {
		onRequest(*req);
	}
	req->setOutput(outputAttachment,
			[&](core::FrameAttachmentData &data, bool success, Ref *) -> bool {
		if (!success || !data.image) {
			sprt::cerr << "Frame output failed\n";
			renderResult = -10;
			captureComplete = true;
			return true;
		}

		mtlLoop->captureImage(
				[&, pngFile = pngFile.str<mem_std::Interface>()](const core::ImageInfoData &info,
						BytesView view) {
			if (view.empty()) {
				sprt::cerr << "Readback failed\n";
				renderResult = -11;
				captureComplete = true;
				return;
			}

			const uint64_t bytesPerRow =
					uint64_t(info.extent.width) * core::getFormatBlockSize(info.format);

			renderResult = verify(view.data(), bytesPerRow) ? 0 : -6;

			Bitmap bmp(view.data(), info.extent.width, info.extent.height,
					bitmap::PixelFormat::RGBA8888);
			if (bmp.save(FileInfo(pngFile))) {
				sprt::cout << "Saved: " << pngFile << "\n";
			}

			captureComplete = true;
		},
				data.image->getImage(), core::AttachmentLayout::TransferSrcOptimal);
		return true;
	});

	mtlLoop->runRenderQueue(move(req), 0, [&](bool success) { frameComplete = true; });

	// pump the looper until the frame completes and the readback is delivered
	uint32_t attempts = 10'000;
	while ((!captureComplete || !frameComplete) && attempts > 0) {
		looper->wait(TimeInterval::milliseconds(1));
		--attempts;
	}

	if (!captureComplete || !frameComplete) {
		sprt::cerr << "Frame timed out\n";
		renderResult = -12;
	}

	return renderResult;
}

int main(int argc, const char *argv[]) {
	return perform_main(argc, argv, [&]() -> int {
		auto info = Rc<core::InstanceInfo>::alloc();
		info->api = core::InstanceApi::Metal;

		auto instance = core::Instance::create(move(info));
		if (!instance) {
			sprt::cerr << "Fail to create Metal instance\n";
			return -1;
		}

		auto mtlInstance = instance.get_cast<mtl::Instance>();

		sprt::cout << "Metal devices: " << mtlInstance->getDeviceCount() << "\n";

		size_t i = 0;
		for (auto &it : mtlInstance->getDevices()) {
			sprt::cout << "[" << i << "] " << it.name << " (registryID: " << it.registryID
					   << ", unifiedMemory: " << (it.unifiedMemory ? "true" : "false") << ")\n";
			++i;
		}

		if (mtlInstance->getDeviceCount() == 0) {
			return 1;
		}

		// create loop + device on this thread's looper
		auto looper = sprt::dispatch::Looper::acquire();

		auto loop = instance->makeLoop(looper, Rc<core::LoopInfo>::alloc());
		if (!loop) {
			sprt::cerr << "Fail to create loop\n";
			return -2;
		}

		auto mtlLoop = static_cast<mtl::Loop *>(loop.get());
		auto device = mtlLoop->getDevice();
		if (!device) {
			sprt::cerr << "Fail to create device\n";
			return -3;
		}

		mtlLoop->run();

		sprt::cout << "Device: " << device->getDeviceData().name << "\n";

		// graphics: triangle over clear color
		auto triangleResult = runOffscreenQueue(looper, mtlLoop, makeTriangleQueue(),
				"offscreen.png", [&](const uint8_t *data, uint64_t bytesPerRow) {
			auto pixelAt = [&](uint32_t x, uint32_t y) { return data + y * bytesPerRow + x * 4; };

			auto center = pixelAt(RenderSize / 2, RenderSize / 2);
			auto corner = pixelAt(4, 4);

			bool centerOk = checkPixel(center, 255, 128, 0, 4);
			bool cornerOk = checkPixel(corner, 26, 51, 77, 4);

			sprt::cout << "Triangle center: [" << int(center[0]) << ", " << int(center[1]) << ", "
					   << int(center[2]) << "] " << (centerOk ? "OK" : "FAILED") << "\n";
			sprt::cout << "Triangle corner: [" << int(corner[0]) << ", " << int(corner[1]) << ", "
					   << int(corner[2]) << "] " << (cornerOk ? "OK" : "FAILED") << "\n";

			return centerOk && cornerOk;
		});

		sprt::cout << "Triangle queue: " << (triangleResult == 0 ? "OK" : "FAILED") << "\n";

		// compute: gradient + checkerboard via storage image, resolved by a
		// graphics pass in the same queue
		auto computeResult = runOffscreenQueue(looper, mtlLoop, makeComputeQueue(), "compute.png",
				[&](const uint8_t *data, uint64_t bytesPerRow) {
			auto pixelAt = [&](uint32_t x, uint32_t y) { return data + y * bytesPerRow + x * 4; };

			// r = x, g = y, b = checkerboard with 32px cells
			auto p1 = pixelAt(10, 20); // checker ((0 + 0) % 2) == 0
			auto p2 = pixelAt(200, 100); // checker ((6 + 3) % 2) == 1
			auto p3 = pixelAt(250, 250); // checker ((7 + 7) % 2) == 0

			bool ok1 = checkPixel(p1, 10, 20, 0, 2);
			bool ok2 = checkPixel(p2, 200, 100, 255, 2);
			bool ok3 = checkPixel(p3, 250, 250, 0, 2);

			sprt::cout << "Compute p1: [" << int(p1[0]) << ", " << int(p1[1]) << ", " << int(p1[2])
					   << "] " << (ok1 ? "OK" : "FAILED") << "\n";
			sprt::cout << "Compute p2: [" << int(p2[0]) << ", " << int(p2[1]) << ", " << int(p2[2])
					   << "] " << (ok2 ? "OK" : "FAILED") << "\n";
			sprt::cout << "Compute p3: [" << int(p3[0]) << ", " << int(p3[1]) << ", " << int(p3[2])
					   << "] " << (ok3 ? "OK" : "FAILED") << "\n";

			return ok1 && ok2 && ok3;
		});

		sprt::cout << "Compute queue: " << (computeResult == 0 ? "OK" : "FAILED") << "\n";

		// texture set: argument buffer with red/green textures, shader picks
		// green by index from the uniform buffer
		auto textureSetResult = runOffscreenQueue(looper, mtlLoop, makeTextureSetQueue(),
				"texset.png", [&](const uint8_t *data, uint64_t bytesPerRow) {
			auto pixelAt = [&](uint32_t x, uint32_t y) { return data + y * bytesPerRow + x * 4; };

			auto center = pixelAt(RenderSize / 2, RenderSize / 2);

			bool centerOk = checkPixel(center, 0, 255, 0, 2);

			sprt::cout << "TextureSet center: [" << int(center[0]) << ", " << int(center[1]) << ", "
					   << int(center[2]) << "] " << (centerOk ? "OK" : "FAILED") << "\n";

			return centerOk;
		}, [&](const Rc<core::Queue> &queue) {
			auto texLayoutData = queue->getTextureSetLayouts().get("TexSet");
			if (!texLayoutData || !texLayoutData->layout) {
				sprt::cerr << "Texture set layout was not compiled\n";
				return false;
			}

			auto redImage = queue->getInternalResource()->getImage("RedImage");
			auto greenImage = queue->getInternalResource()->getImage("GreenImage");
			if (!redImage || redImage->views.empty() || !redImage->views.front()->view
					|| !greenImage || greenImage->views.empty()
					|| !greenImage->views.front()->view) {
				sprt::cerr << "Static images were not compiled\n";
				return false;
			}

			core::MaterialLayout matLayout;
			matLayout.imageSlots.resize(2);
			matLayout.imageSlots[0].image = redImage->views.front()->view;
			matLayout.imageSlots[0].refCount = 1;
			matLayout.imageSlots[1].image = greenImage->views.front()->view;
			matLayout.imageSlots[1].refCount = 1;
			matLayout.usedImageSlots = 2;

			s_textureSet = texLayoutData->layout->acquireSet(*device);
			if (!s_textureSet) {
				sprt::cerr << "Fail to acquire texture set\n";
				return false;
			}

			s_textureSet->write(matLayout);
			return true;
		});

		s_textureSet = nullptr;
		sprt::cout << "TextureSet queue: " << (textureSetResult == 0 ? "OK" : "FAILED") << "\n";

		// material system: predefined red/green materials, render green one
		auto materialQueue = makeMaterialQueue();
		auto materialResult = runOffscreenQueue(looper, mtlLoop, materialQueue, "material.png",
				[&](const uint8_t *data, uint64_t bytesPerRow) {
			auto pixelAt = [&](uint32_t x, uint32_t y) { return data + y * bytesPerRow + x * 4; };

			auto center = pixelAt(RenderSize / 2, RenderSize / 2);

			bool centerOk = checkPixel(center, 0, 255, 0, 2);

			sprt::cout << "Material center: [" << int(center[0]) << ", " << int(center[1]) << ", "
					   << int(center[2]) << "] " << (centerOk ? "OK" : "FAILED") << "\n";

			return centerOk;
		}, [&](const Rc<core::Queue> &queue) {
			auto materials = s_materialAttachment->getMaterials();
			if (!materials) {
				sprt::cerr << "Materials were not compiled with the queue\n";
				return false;
			}

			auto material = materials->getMaterialById(core::MaterialId(2));
			if (!material) {
				sprt::cerr << "Material 2 not found in set\n";
				return false;
			}

			if (!material->getBuffer()) {
				sprt::cerr << "Material data buffer was not allocated\n";
				return false;
			}

			auto &image = material->getImages().front();
			sprt::cout << "Material 2: set=" << image.set << " descriptor=" << image.descriptor
					   << " buffer=" << (material->getBuffer() ? "allocated" : "missing") << "\n";

			auto layout = materials->getLayout(material->getLayoutIndex());
			if (!layout || !layout->set) {
				sprt::cerr << "Material layout has no texture set\n";
				return false;
			}

			s_materialTextureSet = layout->set;

			// point the shader at the green material's slot
			auto paramsData = queue->getInternalResource()->getBuffer("MatParamsData");
			if (!paramsData || !paramsData->buffer) {
				sprt::cerr << "Params buffer was not compiled\n";
				return false;
			}

			uint32_t index = image.descriptor;
			paramsData->buffer.get_cast<mtl::Buffer>()->setData(
					BytesView(reinterpret_cast<const uint8_t *>(&index), sizeof(index)));
			return true;
		});

		// runtime material update path (compileMaterials)
		if (materialResult == 0) {
			const core::GraphicPipelineData *pipelineData = nullptr;
			for (auto &pass : materialQueue->getPasses()) {
				for (auto &subpass : pass->subpasses) {
					if (auto p = subpass->graphicPipelines.get("MaterialPipeline")) {
						pipelineData = p;
					}
				}
			}

			auto redData = materialQueue->getInternalResource()->getImage("RedImage");

			bool updateDone = false;
			auto input = Rc<core::MaterialInputData>::alloc();
			input->attachment = s_materialAttachment;
			input->materialsToAddOrUpdate.emplace_back(
					Rc<core::Material>::create(core::MaterialId(3), pipelineData, redData));
			input->callback = [&] { updateDone = true; };

			mtlLoop->compileMaterials(move(input));

			uint32_t attempts = 5'000;
			while (!updateDone && attempts > 0) {
				looper->wait(TimeInterval::milliseconds(1));
				--attempts;
			}

			auto m3 = updateDone
					? s_materialAttachment->getMaterials()->getMaterialById(core::MaterialId(3))
					: nullptr;
			bool updateOk = m3 && m3->getBuffer();
			sprt::cout << "Material update: " << (updateOk ? "OK (material 3 compiled)" : "FAILED")
					   << "\n";
			if (!updateOk) {
				materialResult = -16;
			}
		}

		s_materialTextureSet = nullptr;
		s_materialAttachment = nullptr;
		sprt::cout << "Material queue: " << (materialResult == 0 ? "OK" : "FAILED") << "\n";

		// 2d renderer slice: two material quads + a scissored quad on top
		auto basic2dResult = runOffscreenQueue(looper, mtlLoop, makeBasic2dQueue(), "basic2d.png",
				[&](const uint8_t *data, uint64_t bytesPerRow) {
			auto pixelAt = [&](uint32_t x, uint32_t y) { return data + y * bytesPerRow + x * 4; };

			auto left = pixelAt(RenderSize / 4, RenderSize / 2);
			auto right = pixelAt((RenderSize * 3) / 4, RenderSize / 2);
			auto corner = pixelAt(2, RenderSize - 3);

			// scissored bar (engine Y-down projection puts NDC +0.9 at the
			// bottom rows): left half drawn, right half clipped away
			auto barIn = pixelAt(64, 243);
			auto barOut = pixelAt(192, 243);

			bool leftOk = checkPixel(left, 255, 0, 0, 2);
			bool rightOk = checkPixel(right, 0, 255, 0, 2);
			bool cornerOk = checkPixel(corner, 0, 0, 0, 2);
			bool barInOk = checkPixel(barIn, 255, 0, 0, 2);
			bool barOutOk = checkPixel(barOut, 0, 0, 0, 2);

			sprt::cout << "Basic2d left: [" << int(left[0]) << ", " << int(left[1]) << ", "
					   << int(left[2]) << "] " << (leftOk ? "OK" : "FAILED") << "\n";
			sprt::cout << "Basic2d right: [" << int(right[0]) << ", " << int(right[1]) << ", "
					   << int(right[2]) << "] " << (rightOk ? "OK" : "FAILED") << "\n";
			sprt::cout << "Basic2d corner: [" << int(corner[0]) << ", " << int(corner[1]) << ", "
					   << int(corner[2]) << "] " << (cornerOk ? "OK" : "FAILED") << "\n";
			sprt::cout << "Basic2d scissor-in: [" << int(barIn[0]) << ", " << int(barIn[1]) << ", "
					   << int(barIn[2]) << "] " << (barInOk ? "OK" : "FAILED") << "\n";
			sprt::cout << "Basic2d scissor-out: [" << int(barOut[0]) << ", " << int(barOut[1])
					   << ", " << int(barOut[2]) << "] " << (barOutOk ? "OK" : "FAILED") << "\n";

			return leftOk && rightOk && cornerOk && barInOk && barOutOk;
		}, nullptr, [&](core::FrameRequest &req) {
			auto contextHandle = Rc<basic2d::FrameContextHandle2d>::alloc();
			contextHandle->clock = 0;

			auto poolRef = Rc<sprt::PoolRef>::alloc();
			contextHandle->commands = Rc<basic2d::CommandList>::create(poolRef);

			basic2d::CmdInfo redInfo;
			redInfo.material = core::MaterialId(1);
			contextHandle->commands->pushVertexArray(makeQuad(-0.9f, -0.8f, -0.1f, 0.8f),
					Mat4::IDENTITY, sp::move(redInfo));

			basic2d::CmdInfo greenInfo;
			greenInfo.material = core::MaterialId(2);
			contextHandle->commands->pushVertexArray(makeQuad(0.1f, -0.8f, 0.9f, 0.8f),
					Mat4::IDENTITY, sp::move(greenInfo));

			// red bar across the top, scissored to the left half of the target
			basic2d::CmdInfo barInfo;
			barInfo.material = core::MaterialId(1);
			barInfo.state = contextHandle->addState(DrawStateValues{core::DynamicState::Scissor,
				URect{}, URect{0, 0, RenderSize / 2, RenderSize}, nullptr});
			contextHandle->commands->pushVertexArray(makeQuad(-0.8f, 0.85f, 0.8f, 0.95f),
					Mat4::IDENTITY, sp::move(barInfo));

			auto vertexes = req.getQueue()->getAttachment("Vertexes");
			req.addInput(vertexes, move(contextHandle));
		});

		sprt::cout << "Basic2d queue: " << (basic2dResult == 0 ? "OK" : "FAILED") << "\n";
		s_materialAttachment = nullptr;

		// font atlas queue: underline-only path (no freetype required)
		int fontResult = -20;
		{
			auto fontQueue = Rc<mtl::FontQueue>::create("FontQueue");

			bool fontCompiled = false;
			mtlLoop->compileQueue(fontQueue, [&](bool success) { fontCompiled = success; });

			auto dynImage =
					Rc<core::DynamicImage>::create([](core::DynamicImage::Builder &builder) {
				static const uint8_t s_white[1] = {255};
				builder.setImageByRef("FontAtlas",
						core::ImageInfo(Extent2(1, 1), core::ImageFormat::R8_UNORM,
								core::ImageUsage::Sampled | core::ImageUsage::TransferDst),
						BytesView(s_white, 1));
				return true;
			});

			bool imageCompiled = false;
			mtlLoop->compileImage(dynImage, [&](bool success) { imageCompiled = success; });

			if (fontCompiled && imageCompiled && dynImage->getInstance()) {
				bool outputCalled = false;
				bool outputValid = false;
				bool frameComplete = false;

				auto input = Rc<xenolith::font::RenderFontInput>::alloc();
				input->queue = looper;
				input->image = dynImage;
				input->output = [&](const core::ImageInfoData &info, BytesView data) {
					outputCalled = true;
					// single white underline pixel packed into the atlas
					outputValid = info.format == core::ImageFormat::R8_UNORM
							&& info.extent.width >= 1 && info.extent.height >= 1 && !data.empty()
							&& data[0] == 255;
				};

				auto req = Rc<core::FrameRequest>::create(fontQueue,
						core::FrameConstraints{Extent3(RenderSize, RenderSize, 1)});
				req->addInput(fontQueue->getAttachment(), Rc<core::AttachmentInputData>(input));

				mtlLoop->runRenderQueue(move(req), 0,
						[&](bool success) { frameComplete = success; });

				uint32_t attempts = 5'000;
				while (!frameComplete && attempts > 0) {
					looper->wait(TimeInterval::milliseconds(1));
					--attempts;
				}

				auto instance = dynImage->getInstance();
				bool instanceUpdated = instance && instance->gen > 1 && instance->data.atlas;

				sprt::cout << "Font atlas output: "
						   << (outputCalled && outputValid ? "OK" : "FAILED")
						   << "; instance: " << (instanceUpdated ? "OK" : "FAILED") << "\n";

				fontResult =
						(frameComplete && outputCalled && outputValid && instanceUpdated) ? 0 : -21;

				dynImage->finalize();
			} else {
				sprt::cerr << "Font queue/image compilation failed\n";
			}

			sprt::cout << "Font queue: " << (fontResult == 0 ? "OK" : "FAILED") << "\n";
		}

		// presentation via PresentationEngine + headless CAMetalLayer swapchain:
		// run the full acquire -> render -> present cycle for a number of
		// frames and verify they are presented
		int presentResult = 0;
		{
			auto layerHandle = mtl::platform::createOffscreenLayer();
			auto window = Rc<TestPresentationWindow>::alloc(layerHandle, mtlLoop,
					Extent2(RenderSize, RenderSize));

			auto engine =
					mtlLoop->makePresentationEngine(window.get(), core::PresentationOptions());
			if (engine && engine->run()) {
				engine->scheduleNextImage();

				constexpr uint32_t TargetFrames = 20;
				uint32_t attempts = 20'000;
				while (window->presentedFrames < TargetFrames && attempts > 0) {
					engine->setReadyForNextFrame();
					looper->wait(TimeInterval::milliseconds(1));
					--attempts;
				}

				sprt::cout << "Presented frames: " << window->presentedFrames << "\n";
				presentResult = window->presentedFrames >= TargetFrames ? 0 : -13;

				engine->end();
				looper->poll();
			} else {
				sprt::cerr << "Fail to run presentation engine\n";
				presentResult = -14;
			}

			engine = nullptr;
			looper->poll();
			mtl::platform::releaseLayerHandle(layerHandle);

			sprt::cout << "Present queue: " << (presentResult == 0 ? "OK" : "FAILED") << "\n";
		}

		mtlLoop->waitIdle();
		mtlLoop->stop();
		looper->poll();

		if (triangleResult != 0) {
			return triangleResult;
		}
		if (computeResult != 0) {
			return computeResult;
		}
		if (textureSetResult != 0) {
			return textureSetResult;
		}
		if (materialResult != 0) {
			return materialResult;
		}
		if (basic2dResult != 0) {
			return basic2dResult;
		}
		if (fontResult != 0) {
			return fontResult;
		}
		return presentResult;
	});
}
