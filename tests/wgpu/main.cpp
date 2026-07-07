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

#include "SPCommon.h"
#include "SPBitmap.h"
#include "XLWgpuInstance.h"
#include "XLWgpuLoop.h"
#include "XLWgpuPipeline.h"
#include "XLWgpuObject.h"
#include "XLWgpuQueuePass.h"
#include "XLWgpuPresentation.h"
#include "XLWgpuTextureSet.h"
#include "XLWgpuMaterial.h"
#include "XL2dWgpuVertexPass.h"
#include "XLWgpuFontQueue.h"
#include "XLCoreDynamicImage.h"
#include "XLCoreQueue.h"
#include "XLCoreResource.h"
#include "XLCoreQueuePass.h"
#include "XLCoreAttachment.h"
#include "XLCoreFrameRequest.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreImageStorage.h"
#include "XLCorePresentationFrame.h"

#include <xcb/xcb.h>

using namespace stappler;
using namespace stappler::xenolith;

static constexpr uint32_t RenderSize = 256;

static constexpr auto s_vertWgsl = StringView(R"wgsl(
@vertex
fn main(@builtin(vertex_index) idx : u32) -> @builtin(position) vec4<f32> {
	var positions = array<vec2<f32>, 3>(
		vec2<f32>(-0.75, -0.75),
		vec2<f32>(0.75, -0.75),
		vec2<f32>(0.0, 0.75));
	return vec4<f32>(positions[idx], 0.0, 1.0);
}
)wgsl");

static constexpr auto s_fragWgsl = StringView(R"wgsl(
@fragment
fn main() -> @location(0) vec4<f32> {
	return vec4<f32>(1.0, 0.5, 0.0, 1.0);
}
)wgsl");

static constexpr auto s_compWgsl = StringView(R"wgsl(
struct Params {
	cellShift : u32,
};

@group(0) @binding(0) var image : texture_storage_2d<rgba8unorm, write>;
@group(0) @binding(1) var<uniform> params : Params;

@compute @workgroup_size(8, 8)
fn main(@builtin(global_invocation_id) id : vec3<u32>) {
	let size = textureDimensions(image);
	if (id.x >= size.x || id.y >= size.y) {
		return;
	}
	let fx = f32(id.x) / 255.0;
	let fy = f32(id.y) / 255.0;
	let checker = f32(((id.x >> params.cellShift) + (id.y >> params.cellShift)) % 2u);
	textureStore(image, vec2<i32>(id.xy), vec4<f32>(fx, fy, checker, 1.0));
}
)wgsl");

static constexpr auto s_resolveVertWgsl = StringView(R"wgsl(
@vertex
fn main(@builtin(vertex_index) idx : u32) -> @builtin(position) vec4<f32> {
	var positions = array<vec2<f32>, 3>(
		vec2<f32>(-1.0, -1.0),
		vec2<f32>(3.0, -1.0),
		vec2<f32>(-1.0, 3.0));
	return vec4<f32>(positions[idx], 0.0, 1.0);
}
)wgsl");

static constexpr auto s_resolveFragWgsl = StringView(R"wgsl(
@group(0) @binding(0) var tex : texture_2d<f32>;
@group(0) @binding(1) var samp : sampler;

@fragment
fn main(@builtin(position) pos : vec4<f32>) -> @location(0) vec4<f32> {
	return textureSampleLevel(tex, samp, pos.xy / 256.0, 0.0);
}
)wgsl");

// pack WGSL text into ProgramData's uint32_t words
static Vector<uint32_t> packWgsl(StringView code) {
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

	auto vertData = packWgsl(s_vertWgsl);
	auto fragData = packWgsl(s_fragWgsl);

	auto vertProg = builder.addProgram("TriangleVert", vertData, &vertInfo);
	auto fragProg = builder.addProgram("TriangleFrag", fragData, &fragInfo);

	auto attachment = builder.addAttachemnt("Output",
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		attachmentBuilder.defineAsOutput();
		return Rc<webgpu::ImageAttachment>::create(attachmentBuilder,
				core::ImageInfo(Extent2(RenderSize, RenderSize),
						core::ImageFormat::R8G8B8A8_UNORM, core::ImageHints::FixedSize,
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

			subpassBuilder.setCommandsCallback([](core::FrameQueue &frameQueue,
					const core::SubpassData &subpass, core::CommandBuffer &commands) {
				auto &buf = static_cast<webgpu::CommandBuffer &>(commands);
				if (auto pipeline = subpass.graphicPipelines.get("TrianglePipeline")) {
					buf.cmdBindPipeline(pipeline);
					buf.cmdDraw(3);
				}
			});
		});

		return Rc<webgpu::QueuePass>::create(passBuilder);
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

	auto compData = packWgsl(s_compWgsl);
	auto vertData = packWgsl(s_resolveVertWgsl);
	auto fragData = packWgsl(s_resolveFragWgsl);

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
		return Rc<webgpu::BufferAttachment>::create(attachmentBuilder, paramsData);
	});

	auto patternAttachment = builder.addAttachemnt("Pattern",
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		return Rc<webgpu::ImageAttachment>::create(attachmentBuilder,
				core::ImageInfo(Extent2(RenderSize, RenderSize),
						core::ImageFormat::R8G8B8A8_UNORM, core::ImageHints::FixedSize,
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
		return Rc<webgpu::ImageAttachment>::create(attachmentBuilder,
				core::ImageInfo(Extent2(RenderSize, RenderSize),
						core::ImageFormat::R8G8B8A8_UNORM, core::ImageHints::FixedSize,
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

			subpassBuilder.setCommandsCallback([](core::FrameQueue &frameQueue,
					const core::SubpassData &subpass, core::CommandBuffer &commands) {
				auto &buf = static_cast<webgpu::CommandBuffer &>(commands);
				if (auto pipeline = subpass.computePipelines.get("PatternPipeline")) {
					buf.cmdBindPipeline(pipeline);
					buf.cmdDispatch(RenderSize / 8, RenderSize / 8, 1);
				}
			});
		});

		return Rc<webgpu::QueuePass>::create(passBuilder);
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

			subpassBuilder.setCommandsCallback([](core::FrameQueue &frameQueue,
					const core::SubpassData &subpass, core::CommandBuffer &commands) {
				auto &buf = static_cast<webgpu::CommandBuffer &>(commands);
				if (auto pipeline = subpass.graphicPipelines.get("ResolvePipeline")) {
					buf.cmdBindPipeline(pipeline);
					buf.cmdDraw(3);
				}
			});
		});

		return Rc<webgpu::QueuePass>::create(passBuilder);
	});

	return Rc<core::Queue>::create(move(builder));
}

static constexpr auto s_texArrayFragWgsl = StringView(R"wgsl(
struct Params {
	index : u32,
};

@group(0) @binding(0) var<uniform> params : Params;
@group(1) @binding(0) var samplers : binding_array<sampler, 2>;
@group(1) @binding(1) var textures : binding_array<texture_2d<f32>, 8>;

@fragment
fn main(@builtin(position) pos : vec4<f32>) -> @location(0) vec4<f32> {
	return textureSampleLevel(textures[params.index], samplers[0], pos.xy / 256.0, 0.0);
}
)wgsl");

// TextureSet demo: static red/green textures in a texture set (binding
// arrays), fragment shader picks one by index from the uniform buffer
static Rc<core::TextureSet> s_textureSet;

static Rc<core::Queue> makeTextureSetQueue() {
	core::Queue::Builder builder("TextureSetQueue");

	core::ProgramInfo vertInfo;
	vertInfo.stage = core::ProgramStage::Vertex;

	core::ProgramInfo fragInfo;
	fragInfo.stage = core::ProgramStage::Fragment;

	auto vertData = packWgsl(s_resolveVertWgsl);
	auto fragData = packWgsl(s_texArrayFragWgsl);

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

	// texture set layout: 2 samplers, 8 images (matches WGSL binding_array sizes)
	core::SamplerInfo samplers[2];
	samplers[0] = core::SamplerInfo{.magFilter = core::Filter::Nearest,
		.minFilter = core::Filter::Nearest};
	samplers[1] = core::SamplerInfo{.magFilter = core::Filter::Linear,
		.minFilter = core::Filter::Linear};

	auto texLayout = builder.addTextureSetLayout("TexSet", makeSpanView(samplers, 2), 8,
			xenolith::config::MaxBufferArrayObjects, 8);

	// texture index for the shader
	static const uint32_t s_texParams[4] = {1, 0, 0, 0}; // green

	auto paramsData = builder.addBuffer("TexParamsData",
			core::BufferInfo(core::BufferUsage::UniformBuffer, uint64_t(sizeof(s_texParams))),
			BytesView(reinterpret_cast<const uint8_t *>(s_texParams), sizeof(s_texParams)));

	auto paramsAttachment = builder.addAttachemnt("Params",
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		return Rc<webgpu::BufferAttachment>::create(attachmentBuilder, paramsData);
	});

	auto outputAttachment = builder.addAttachemnt("Output",
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		attachmentBuilder.defineAsOutput();
		return Rc<webgpu::ImageAttachment>::create(attachmentBuilder,
				core::ImageInfo(Extent2(RenderSize, RenderSize),
						core::ImageFormat::R8G8B8A8_UNORM, core::ImageHints::FixedSize,
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

			subpassBuilder.setCommandsCallback([](core::FrameQueue &frameQueue,
					const core::SubpassData &subpass, core::CommandBuffer &commands) {
				auto &buf = static_cast<webgpu::CommandBuffer &>(commands);
				if (auto pipeline = subpass.graphicPipelines.get("TexArrayPipeline")) {
					buf.cmdBindPipeline(pipeline);
					buf.cmdBindTextureSet(pipeline->layout, s_textureSet.get());
					buf.cmdDraw(3);
				}
			});
		});

		return Rc<webgpu::QueuePass>::create(passBuilder);
	});

	return Rc<core::Queue>::create(move(builder));
}

// Material system demo: two predefined materials (red/green textures),
// compiled through the material machinery; fragment samples the green
// material's texture set slot
static Rc<core::TextureSet> s_materialTextureSet;
static const core::MaterialAttachment *s_materialAttachment = nullptr;

class TestMaterialAttachment : public webgpu::MaterialAttachment {
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

	auto vertData = packWgsl(s_resolveVertWgsl);
	auto fragData = packWgsl(s_texArrayFragWgsl);

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
	samplers[1] = core::SamplerInfo{.magFilter = core::Filter::Linear,
		.minFilter = core::Filter::Linear};

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
		return Rc<webgpu::BufferAttachment>::create(attachmentBuilder, paramsData);
	});

	auto outputAttachment = builder.addAttachemnt("Output",
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		attachmentBuilder.defineAsOutput();
		return Rc<webgpu::ImageAttachment>::create(attachmentBuilder,
				core::ImageInfo(Extent2(RenderSize, RenderSize),
						core::ImageFormat::R8G8B8A8_UNORM, core::ImageHints::FixedSize,
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

			auto pipeline = subpassBuilder.addGraphicPipeline("MaterialPipeline",
					layout->defaultFamily,
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

			subpassBuilder.setCommandsCallback([](core::FrameQueue &frameQueue,
					const core::SubpassData &subpass, core::CommandBuffer &commands) {
				auto &buf = static_cast<webgpu::CommandBuffer &>(commands);
				if (auto pipeline = subpass.graphicPipelines.get("MaterialPipeline")) {
					buf.cmdBindPipeline(pipeline);
					buf.cmdBindTextureSet(pipeline->layout, s_materialTextureSet.get());
					buf.cmdDraw(3);
				}
			});
		});

		return Rc<webgpu::QueuePass>::create(passBuilder);
	});

	return Rc<core::Queue>::create(move(builder));
}

// 2d renderer slice: two quads with red/green materials through the
// webgpu MaterialVertexPass (command list -> storage buffers -> spans)
static Rc<core::Queue> makeBasic2dQueue(webgpu::Device *device) {
	core::Queue::Builder builder("Basic2dQueue");

	core::ProgramInfo vertInfo;
	vertInfo.stage = core::ProgramStage::Vertex;

	core::ProgramInfo fragInfo;
	fragInfo.stage = core::ProgramStage::Fragment;

	auto vertData = packWgsl(basic2d::webgpu::getMaterialVertexShader());
	auto fragData = packWgsl(basic2d::webgpu::getMaterialFragmentShader(
			device->getBackendFeatures().textureBindingArrays));

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
	samplers[1] = core::SamplerInfo{.magFilter = core::Filter::Linear,
		.minFilter = core::Filter::Linear};

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
		return Rc<basic2d::webgpu::VertexAttachment>::create(attachmentBuilder, materialsAtt);
	});

	auto outputAttachment = builder.addAttachemnt("Output",
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		attachmentBuilder.defineAsOutput();
		return Rc<webgpu::ImageAttachment>::create(attachmentBuilder,
				core::ImageInfo(Extent2(RenderSize, RenderSize),
						core::ImageFormat::R8G8B8A8_UNORM, core::ImageHints::FixedSize,
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
				// vertices, transforms, spans, atlases - views of the vertex attachment
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

			auto pipeline = subpassBuilder.addGraphicPipeline("Material2dPipeline",
					layout->defaultFamily,
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

		return Rc<basic2d::webgpu::MaterialVertexPass>::create(passBuilder, vertexesAtt,
				materialsAtt);
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

	auto vertData = packWgsl(s_vertWgsl);
	auto fragData = packWgsl(s_fragWgsl);

	auto vertProg = builder.addProgram("TriangleVert", vertData, &vertInfo);
	auto fragProg = builder.addProgram("TriangleFrag", fragData, &fragInfo);

	auto attachment = builder.addAttachemnt("Output",
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		attachmentBuilder.defineAsOutput();
		return Rc<webgpu::ImageAttachment>::create(attachmentBuilder,
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

			subpassBuilder.setCommandsCallback([](core::FrameQueue &frameQueue,
					const core::SubpassData &subpass, core::CommandBuffer &commands) {
				auto &buf = static_cast<webgpu::CommandBuffer &>(commands);
				if (auto pipeline = subpass.graphicPipelines.get("TrianglePipeline")) {
					buf.cmdBindPipeline(pipeline);
					buf.cmdDraw(3);
				}
			});
		});

		return Rc<webgpu::QueuePass>::create(passBuilder);
	});

	return Rc<core::Queue>::create(move(builder));
}

// minimal xcb window over dlopen-ed libxcb (runtime convention: no direct link)
struct XcbWindow {
	sprt::Dso handle;
	xcb_connection_t *connection = nullptr;
	uint32_t window = 0;

	decltype(&xcb_connect) fnConnect = nullptr;
	decltype(&xcb_disconnect) fnDisconnect = nullptr;
	decltype(&xcb_get_setup) fnGetSetup = nullptr;
	decltype(&xcb_setup_roots_iterator) fnSetupRootsIterator = nullptr;
	decltype(&xcb_generate_id) fnGenerateId = nullptr;
	decltype(&xcb_create_window) fnCreateWindow = nullptr;
	decltype(&xcb_map_window) fnMapWindow = nullptr;
	decltype(&xcb_flush) fnFlush = nullptr;
	decltype(&xcb_poll_for_event) fnPollForEvent = nullptr;

	bool open(uint16_t width, uint16_t height) {
		handle = sprt::Dso("libxcb.so.1");
		if (!handle) {
			return false;
		}

		fnConnect = handle.sym<decltype(fnConnect)>("xcb_connect");
		fnDisconnect = handle.sym<decltype(fnDisconnect)>("xcb_disconnect");
		fnGetSetup = handle.sym<decltype(fnGetSetup)>("xcb_get_setup");
		fnSetupRootsIterator =
				handle.sym<decltype(fnSetupRootsIterator)>("xcb_setup_roots_iterator");
		fnGenerateId = handle.sym<decltype(fnGenerateId)>("xcb_generate_id");
		fnCreateWindow = handle.sym<decltype(fnCreateWindow)>("xcb_create_window");
		fnMapWindow = handle.sym<decltype(fnMapWindow)>("xcb_map_window");
		fnFlush = handle.sym<decltype(fnFlush)>("xcb_flush");
		fnPollForEvent = handle.sym<decltype(fnPollForEvent)>("xcb_poll_for_event");

		if (!fnConnect || !fnGetSetup || !fnSetupRootsIterator || !fnGenerateId || !fnCreateWindow
				|| !fnMapWindow || !fnFlush) {
			return false;
		}

		connection = fnConnect(nullptr, nullptr);
		if (!connection) {
			return false;
		}

		auto screen = fnSetupRootsIterator(fnGetSetup(connection)).data;

		window = fnGenerateId(connection);
		fnCreateWindow(connection, XCB_COPY_FROM_PARENT, window, screen->root, 0, 0, width, height,
				0, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, 0, nullptr);
		fnMapWindow(connection, window);
		fnFlush(connection);
		return true;
	}

	void pump() {
		if (connection && fnPollForEvent) {
			while (auto ev = fnPollForEvent(connection)) { ::free(ev); }
		}
	}

	void close() {
		if (connection && fnDisconnect) {
			fnDisconnect(connection);
			connection = nullptr;
		}
	}
};

// minimal PresentationWindow: fixed size, no input, frames counted;
// render queue is built lazily when the swapchain format is known
class TestPresentationWindow : public Ref, public core::PresentationWindow {
public:
	TestPresentationWindow(XcbWindow *xcb, webgpu::Loop *loop, Extent2 extent)
	: _xcb(xcb), _loop(loop), _extent(extent) { }

	virtual core::ImageInfo getSwapchainImageInfo(
			const core::SwapchainConfig &cfg) const override {
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
		auto info = surface->getSurfaceOptions(dev, core::FullScreenExclusiveMode::Default,
				nullptr);
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
		auto instance = static_cast<webgpu::Instance *>(cinstance.get());

		WGPUSurfaceSourceXCBWindow src = WGPU_SURFACE_SOURCE_XCB_WINDOW_INIT;
		src.connection = _xcb->connection;
		src.window = _xcb->window;

		WGPUSurfaceDescriptor desc = WGPU_SURFACE_DESCRIPTOR_INIT;
		desc.nextInChain = &src.chain;

		auto surface = wgpuInstanceCreateSurface(instance->getInstance(), &desc);
		if (!surface) {
			sprt::cerr << "Fail to create WGPUSurface\n";
			return nullptr;
		}

		return Rc<webgpu::Surface>::create(instance, surface);
	}

	virtual core::FrameConstraints exportConstraints(uint64_t &serial) const override {
		core::FrameConstraints c;
		c.extent = Extent3(_extent.width, _extent.height, 1);
		return c;
	}

	virtual void setFrameOrder(uint64_t value) override { _frameOrder = value; }

	uint32_t presentedFrames = 0;

protected:
	XcbWindow *_xcb = nullptr;
	webgpu::Loop *_loop = nullptr;
	Rc<core::Queue> _queue;
	Extent2 _extent;
	uint64_t _frameOrder = 0;
	core::ImageFormat _selectedFormat = core::ImageFormat::B8G8R8A8_UNORM;
};

// copy texture into a mapped buffer, run verifier, save PNG
static int readbackTexture(webgpu::Device *device, WGPUTexture texture, StringView pngFile,
		const Function<bool(const uint8_t *data, uint64_t bytesPerRow)> &verify) {
	auto wgpuDevice = device->getDevice();

	const uint64_t bytesPerRow = RenderSize * 4; // 1024, satisfies 256-byte alignment
	const uint64_t bufferSize = bytesPerRow * RenderSize;

	WGPUBufferDescriptor bufDesc = WGPU_BUFFER_DESCRIPTOR_INIT;
	bufDesc.label = WGPUStringView{"readback", WGPU_STRLEN};
	bufDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
	bufDesc.size = bufferSize;
	auto buffer = wgpuDeviceCreateBuffer(wgpuDevice, &bufDesc);

	auto encoder = wgpuDeviceCreateCommandEncoder(wgpuDevice, nullptr);

	WGPUTexelCopyTextureInfo copySrc;
	copySrc.texture = texture;
	copySrc.mipLevel = 0;
	copySrc.origin = WGPUOrigin3D{0, 0, 0};
	copySrc.aspect = WGPUTextureAspect_All;

	WGPUTexelCopyBufferInfo copyDst;
	copyDst.layout.offset = 0;
	copyDst.layout.bytesPerRow = uint32_t(bytesPerRow);
	copyDst.layout.rowsPerImage = RenderSize;
	copyDst.buffer = buffer;

	WGPUExtent3D copySize{RenderSize, RenderSize, 1};
	wgpuCommandEncoderCopyTextureToBuffer(encoder, &copySrc, &copyDst, &copySize);

	auto commands = wgpuCommandEncoderFinish(encoder, nullptr);
	wgpuCommandEncoderRelease(encoder);

	wgpuQueueSubmit(device->getQueue(), 1, &commands);
	wgpuCommandBufferRelease(commands);

	bool mapComplete = false;
	bool mapSuccess = false;

	WGPUBufferMapCallbackInfo mapCallback = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
	mapCallback.mode = WGPUCallbackMode_AllowProcessEvents;
	mapCallback.callback = [](WGPUMapAsyncStatus status, WGPUStringView message, void *userdata1,
			void *userdata2) {
		*reinterpret_cast<bool *>(userdata1) = true;
		*reinterpret_cast<bool *>(userdata2) = (status == WGPUMapAsyncStatus_Success);
	};
	mapCallback.userdata1 = &mapComplete;
	mapCallback.userdata2 = &mapSuccess;

	wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, bufferSize, mapCallback);

	while (!mapComplete) { wgpuDevicePoll(wgpuDevice, true, nullptr); }

	int result = 0;

	if (mapSuccess) {
		auto data = reinterpret_cast<const uint8_t *>(
				wgpuBufferGetConstMappedRange(buffer, 0, bufferSize));

		result = verify(data, bytesPerRow) ? 0 : -6;

		Bitmap bmp(data, RenderSize, RenderSize, bitmap::PixelFormat::RGBA8888);
		if (bmp.save(FileInfo(pngFile))) {
			sprt::cout << "Saved: " << pngFile.data() << "\n";
		}

		wgpuBufferUnmap(buffer);
	} else {
		sprt::cerr << "Fail to map readback buffer\n";
		result = -5;
	}

	wgpuBufferRelease(buffer);

	return result;
}

// compile queue, run one frame through the frame graph, read back the output
// attachment and verify its content
static int runOffscreenQueue(sprt::dispatch::Looper *looper, webgpu::Loop *wgpuLoop,
		const Rc<core::Queue> &queue, StringView pngFile,
		const Function<bool(const uint8_t *data, uint64_t bytesPerRow)> &verify,
		const Function<bool(const Rc<core::Queue> &)> &onCompiled = nullptr,
		const Function<void(core::FrameRequest &)> &onRequest = nullptr) {
	auto device = wgpuLoop->getDevice();

	bool compiled = false;
	wgpuLoop->compileQueue(queue, [&](bool success) {
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
	bool outputReceived = false;
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
		outputReceived = true;
		if (!success || !data.image) {
			sprt::cerr << "Frame output failed\n";
			renderResult = -10;
			return true;
		}

		auto image = data.image->getImage().get_cast<webgpu::Image>();
		if (!image) {
			sprt::cerr << "Invalid output image\n";
			renderResult = -11;
			return true;
		}

		renderResult = readbackTexture(device, image->getTexture(), pngFile, verify);
		return true;
	});

	wgpuLoop->runRenderQueue(move(req), 0, [&](bool success) { frameComplete = true; });

	// pump the looper until the frame completes
	uint32_t attempts = 10'000;
	while ((!outputReceived || !frameComplete) && attempts > 0) {
		looper->wait(TimeInterval::milliseconds(1));
		--attempts;
	}

	if (!outputReceived || !frameComplete) {
		sprt::cerr << "Frame timed out\n";
		renderResult = -12;
	}

	return renderResult;
}

int main(int argc, const char *argv[]) {
	return perform_main(argc, argv, [&]() -> int {
		auto info = Rc<core::InstanceInfo>::alloc();
		info->api = core::InstanceApi::WebGPU;

		auto instance = core::Instance::create(move(info));
		if (!instance) {
			sprt::cerr << "Fail to create WebGPU instance\n";
			return -1;
		}

		auto wgpuInstance = instance.get_cast<webgpu::Instance>();

		sprt::cout << "WebGPU adapters: " << wgpuInstance->getDeviceCount() << "\n";

		size_t i = 0;
		for (auto &it : wgpuInstance->getAdapters()) {
			sprt::cout << "[" << i << "] " << it.device.data() << " (" << it.description.data()
					   << ")\n"
					   << "\tbackend: " << webgpu::getBackendTypeName(it.backendType)
					   << "; type: " << webgpu::getAdapterTypeName(it.adapterType) << "\n";
			++i;
		}

		if (wgpuInstance->getDeviceCount() == 0) {
			return 1;
		}

		// create loop + device on this thread's looper
		auto looper = sprt::dispatch::Looper::acquire();

		auto loop = instance->makeLoop(looper, Rc<core::LoopInfo>::alloc());
		if (!loop) {
			sprt::cerr << "Fail to create loop\n";
			return -2;
		}

		auto wgpuLoop = static_cast<webgpu::Loop *>(loop.get());
		auto device = wgpuLoop->getDevice();
		if (!device) {
			sprt::cerr << "Fail to create device\n";
			return -3;
		}

		wgpuLoop->run();

		sprt::cout << "Device: " << device->getAdapterData().device.data()
				   << "; features: " << device->getFeatures().size() << "\n";

		// graphics: triangle over clear color
		auto triangleResult = runOffscreenQueue(looper, wgpuLoop, makeTriangleQueue(),
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

		// compute: gradient + checkerboard pattern via storage image descriptor
		auto computeResult = runOffscreenQueue(looper, wgpuLoop, makeComputeQueue(), "compute.png",
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

		// asynchronous GPU drain (browser-compatible waitIdle counterpart)
		{
			bool drained = false;
			wgpuLoop->drain([&] { drained = true; });

			uint32_t attempts = 5'000;
			while (!drained && attempts > 0) {
				looper->wait(TimeInterval::milliseconds(1));
				--attempts;
			}

			sprt::cout << "Drain queue: " << (drained ? "OK" : "FAILED") << "\n";
			if (!drained) {
				return -30;
			}
		}

		// texture set (binding arrays): fragment samples textures[1] -> green
		int textureSetResult = 0;
		if (device->getBackendFeatures().textureBindingArrays) {
			textureSetResult = runOffscreenQueue(looper, wgpuLoop, makeTextureSetQueue(),
					"texset.png", [&](const uint8_t *data, uint64_t bytesPerRow) {
				auto pixelAt =
						[&](uint32_t x, uint32_t y) { return data + y * bytesPerRow + x * 4; };

				auto center = pixelAt(RenderSize / 2, RenderSize / 2);
				auto corner = pixelAt(4, 4);

				bool centerOk = checkPixel(center, 0, 255, 0, 2);
				bool cornerOk = checkPixel(corner, 0, 255, 0, 2);

				sprt::cout << "TextureSet center: [" << int(center[0]) << ", " << int(center[1])
						   << ", " << int(center[2]) << "] " << (centerOk ? "OK" : "FAILED")
						   << "\n";
				sprt::cout << "TextureSet corner: [" << int(corner[0]) << ", " << int(corner[1])
						   << ", " << int(corner[2]) << "] " << (cornerOk ? "OK" : "FAILED")
						   << "\n";

				return centerOk && cornerOk;
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
			sprt::cout << "TextureSet queue: " << (textureSetResult == 0 ? "OK" : "FAILED")
					   << "\n";
		} else {
			sprt::cout << "TextureSet test skipped (no TextureBindingArray feature)\n";
		}

		// material system: predefined red/green materials, render green one
		int materialResult = 0;
		if (device->getBackendFeatures().textureBindingArrays) {
			auto materialQueue = makeMaterialQueue();
			materialResult = runOffscreenQueue(looper, wgpuLoop, materialQueue,
					"material.png", [&](const uint8_t *data, uint64_t bytesPerRow) {
				auto pixelAt =
						[&](uint32_t x, uint32_t y) { return data + y * bytesPerRow + x * 4; };

				auto center = pixelAt(RenderSize / 2, RenderSize / 2);

				bool centerOk = checkPixel(center, 0, 255, 0, 2);

				sprt::cout << "Material center: [" << int(center[0]) << ", " << int(center[1])
						   << ", " << int(center[2]) << "] " << (centerOk ? "OK" : "FAILED")
						   << "\n";

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
				sprt::cout << "Material 2: set=" << image.set
						   << " descriptor=" << image.descriptor << " buffer="
						   << (material->getBuffer() ? "allocated" : "missing") << "\n";

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
				wgpuQueueWriteBuffer(device->getQueue(),
						paramsData->buffer.get_cast<webgpu::Buffer>()->getBuffer(), 0, &index,
						sizeof(index));
				return true;
			});

			// runtime material update path
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

				wgpuLoop->compileMaterials(move(input));

				uint32_t attempts = 5'000;
				while (!updateDone && attempts > 0) {
					looper->wait(TimeInterval::milliseconds(1));
					--attempts;
				}

				auto m3 = updateDone
						? s_materialAttachment->getMaterials()->getMaterialById(
								  core::MaterialId(3))
						: nullptr;
				bool updateOk = m3 && m3->getBuffer();
				sprt::cout << "Material update: "
						   << (updateOk ? "OK (material 3 compiled)" : "FAILED") << "\n";
				if (!updateOk) {
					materialResult = -16;
				}
			}

			s_materialTextureSet = nullptr;
			s_materialAttachment = nullptr;
			sprt::cout << "Material queue: " << (materialResult == 0 ? "OK" : "FAILED") << "\n";
		} else {
			sprt::cout << "Material test skipped (no TextureBindingArray feature)\n";
		}

		// 2d renderer slice: command list with two material quads;
		// runs in both texture modes (bindless / bind group per material)
		int basic2dResult = 0;
		{
			auto basic2dQueue = makeBasic2dQueue(device);
			basic2dResult = runOffscreenQueue(looper, wgpuLoop, basic2dQueue, "basic2d.png",
					[&](const uint8_t *data, uint64_t bytesPerRow) {
				auto pixelAt =
						[&](uint32_t x, uint32_t y) { return data + y * bytesPerRow + x * 4; };

				// red quad is scissored to the image-bottom half, green has
				// no state and must be visible in both halves
				auto redTop = pixelAt(RenderSize / 4, RenderSize / 4);
				auto redBottom = pixelAt(RenderSize / 4, (RenderSize * 3) / 4);
				auto greenTop = pixelAt((RenderSize * 3) / 4, RenderSize / 4);
				auto greenMid = pixelAt((RenderSize * 3) / 4, RenderSize / 2);
				auto corner = pixelAt(2, 2);

				bool redTopOk = checkPixel(redTop, 0, 0, 0, 2); // clipped
				bool redBottomOk = checkPixel(redBottom, 255, 0, 0, 2);
				bool greenTopOk = checkPixel(greenTop, 0, 255, 0, 2);
				bool greenMidOk = checkPixel(greenMid, 0, 255, 0, 2);
				bool cornerOk = checkPixel(corner, 0, 0, 0, 2);

				sprt::cout << "Basic2d scissored-out red: [" << int(redTop[0]) << ", "
						   << int(redTop[1]) << ", " << int(redTop[2]) << "] "
						   << (redTopOk ? "OK" : "FAILED") << "\n";
				sprt::cout << "Basic2d visible red: [" << int(redBottom[0]) << ", "
						   << int(redBottom[1]) << ", " << int(redBottom[2]) << "] "
						   << (redBottomOk ? "OK" : "FAILED") << "\n";
				sprt::cout << "Basic2d unscissored green: [" << int(greenTop[0]) << ", "
						   << int(greenTop[1]) << ", " << int(greenTop[2]) << "] "
						   << ((greenTopOk && greenMidOk) ? "OK" : "FAILED") << "\n";
				sprt::cout << "Basic2d corner: [" << int(corner[0]) << ", " << int(corner[1])
						   << ", " << int(corner[2]) << "] " << (cornerOk ? "OK" : "FAILED")
						   << "\n";

				return redTopOk && redBottomOk && greenTopOk && greenMidOk && cornerOk;
			}, nullptr, [&](core::FrameRequest &req) {
				auto contextHandle = Rc<basic2d::FrameContextHandle2d>::alloc();
				contextHandle->clock = 0;

				auto poolRef = Rc<sprt::PoolRef>::alloc();
				contextHandle->commands = Rc<basic2d::CommandList>::create(poolRef);

				basic2d::CmdInfo redInfo;
				redInfo.material = core::MaterialId(1);
				// scissor to the scene-bottom half (bottom-left based rect,
				// the pass converts it to framebuffer coords like vk)
				DrawStateValues scissorState;
				scissorState.enabled = core::DynamicState::Scissor;
				scissorState.scissor = URect{0, 0, RenderSize, RenderSize / 2};
				redInfo.state = contextHandle->addState(scissorState);
				contextHandle->commands->pushVertexArray(makeQuad(-0.9f, -0.8f, -0.1f, 0.8f),
						Mat4::IDENTITY, sp::move(redInfo));

				basic2d::CmdInfo greenInfo;
				greenInfo.material = core::MaterialId(2);
				contextHandle->commands->pushVertexArray(makeQuad(0.1f, -0.8f, 0.9f, 0.8f),
						Mat4::IDENTITY, sp::move(greenInfo));

				auto vertexes = req.getQueue()->getAttachment("Vertexes");
				req.addInput(vertexes, move(contextHandle));
			});

			s_materialAttachment = nullptr;
			sprt::cout << "Basic2d queue: " << (basic2dResult == 0 ? "OK" : "FAILED") << "\n";
		}

		// font atlas queue: underline-only path (no freetype required)
		int fontResult = -20;
		{
			auto fontQueue = Rc<webgpu::FontQueue>::create("FontQueue");

			bool fontCompiled = false;
			wgpuLoop->compileQueue(fontQueue, [&](bool success) { fontCompiled = success; });

			auto dynImage = Rc<core::DynamicImage>::create(
					[](core::DynamicImage::Builder &builder) {
				static const uint8_t s_white[1] = {255};
				builder.setImageByRef("FontAtlas",
						core::ImageInfo(Extent2(1, 1), core::ImageFormat::R8_UNORM,
								core::ImageUsage::Sampled | core::ImageUsage::TransferDst),
						BytesView(s_white, 1));
				return true;
			});

			bool imageCompiled = false;
			wgpuLoop->compileImage(dynImage, [&](bool success) { imageCompiled = success; });

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
							&& info.extent.width >= 1 && info.extent.height >= 1
							&& !data.empty() && data[0] == 255;
				};

				auto req = Rc<core::FrameRequest>::create(fontQueue,
						core::FrameConstraints{Extent3(RenderSize, RenderSize, 1)});
				req->addInput(fontQueue->getAttachment(), Rc<core::AttachmentInputData>(input));

				wgpuLoop->runRenderQueue(move(req), 0,
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
						   << ", instance: " << (instanceUpdated ? "OK" : "FAILED")
						   << " (gen=" << (instance ? instance->gen : 0) << ")\n";

				fontResult = (frameComplete && outputCalled && outputValid && instanceUpdated)
						? 0
						: -21;

				// instance holds a reference cycle with the image, break it
				dynImage->finalize();
			} else {
				sprt::cerr << "Font queue/image compilation failed\n";
			}

			sprt::cout << "Font queue: " << (fontResult == 0 ? "OK" : "FAILED") << "\n";
		}

		// on-screen presentation via PresentationEngine + WGPUSurface
		int presentResult = 0;
		XcbWindow xcb;
		uint16_t winW = uint16_t(RenderSize * 2), winH = uint16_t(RenderSize * 2);
		if (const char *ws = ::getenv("XL_WINDOW_SIZE")) {
			winW = uint16_t(::atoi(ws));
			winH = uint16_t(winW * 3 / 4);
		}
		if (::getenv("DISPLAY") && xcb.open(winW, winH)) {
			auto window = Rc<TestPresentationWindow>::alloc(&xcb, wgpuLoop,
					Extent2(winW, winH));

			auto engine = wgpuLoop->makePresentationEngine(window.get(),
					core::PresentationOptions());
			if (engine && engine->run()) {
				engine->scheduleNextImage();

				constexpr uint32_t TargetFrames = 120;
				uint32_t attempts = 20'000;
				while (window->presentedFrames < TargetFrames && attempts > 0) {
					engine->setReadyForNextFrame();
					looper->wait(TimeInterval::milliseconds(1));
					xcb.pump();
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
			xcb.close();

			sprt::cout << "Present queue: " << (presentResult == 0 ? "OK" : "FAILED") << "\n";
		} else {
			sprt::cout << "Presentation test skipped (no DISPLAY)\n";
		}

		wgpuLoop->waitIdle();
		wgpuLoop->stop();

		// drain looper queue to let stop() complete
		looper->poll();

		loop = nullptr;
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
		if (fontResult != 0) {
			return fontResult;
		}
		if (basic2dResult != 0) {
			return basic2dResult;
		}
		return presentResult;
	});
}
