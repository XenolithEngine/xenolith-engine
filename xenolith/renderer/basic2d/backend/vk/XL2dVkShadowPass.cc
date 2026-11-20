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

#include "XL2dVkShadowPass.h"
#include "XL2dVkParticlePass.h"

#include "XLCoreDevice.h"
#include "XLCoreEnum.h"
#include "XLCoreQueueData.h"
#include "XLVkDevice.h"
#include "XLCoreFrameQueue.h"
#include "XL2dFrameContext.h"
#include "XLVkTextureSet.h"
#include "glsl/XL2dShaders.h"
#include "glsl/include/XL2dGlslBloom.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::vk {

enum class PseudoSdfSpecialization {
	Solid = 0,
	Sdf = 1,
	Backread = 2
};

bool ShadowPass::makeRenderQueue(Queue::Builder &builder, RenderQueueInfo &info) {
	using namespace core;

	builder.setDamageFlags(info.damage);
	builder.setApi(InstanceApi::Vulkan);
	builder.setTypeTag(toInt(QueueType::Default));

	const AttachmentData *particleEmitters = nullptr;

	particleEmitters = builder.addAttachemnt(FrameContext2d::ParticleEmittersAttachment,
			[](AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsInput();
		return Rc<ParticleEmitterAttachment>::create(builder);
	});

	builder.addPass("ParticlePass", PassType::Compute, RenderOrdering(1),
			[&](QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		return Rc<ParticlePass>::create(builder, passBuilder, particleEmitters);
	});

	builder.addPass("MaterialSwapchainPass", PassType::Graphics, RenderOrderingHighest,
			[&](QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		return Rc<ShadowPass>::create(builder, passBuilder,
				PassCreateInfo{
					.target = info.target,
					.extent = info.extent,
					.flags = info.flags | Flags::UsePseudoSdf,
					.backgroundColor = info.backgroundColor,
					.particleAttachment = particleEmitters,
					.damage = info.damage,
				});
	});

	return true;
}

bool ShadowPass::init(Queue::Builder &queueBuilder, QueuePassBuilder &passBuilder,
		const PassCreateInfo &info) {
	using namespace core;

	core::SamplerInfo samplers[] = {
		SamplerInfo{
			.magFilter = Filter::Nearest,
			.minFilter = Filter::Nearest,
			.addressModeU = SamplerAddressMode::Repeat,
			.addressModeV = SamplerAddressMode::Repeat,
			.addressModeW = SamplerAddressMode::Repeat,
		},
		SamplerInfo{
			.magFilter = Filter::Linear,
			.minFilter = Filter::Linear,
			.addressModeU = SamplerAddressMode::Repeat,
			.addressModeV = SamplerAddressMode::Repeat,
			.addressModeW = SamplerAddressMode::Repeat,
		},
		SamplerInfo{
			.magFilter = Filter::Linear,
			.minFilter = Filter::Linear,
			.addressModeU = SamplerAddressMode::ClampToEdge,
			.addressModeV = SamplerAddressMode::ClampToEdge,
			.addressModeW = SamplerAddressMode::ClampToEdge,
		},
	};

	auto texLayout = queueBuilder.addTextureSetLayout("General", samplers);

	_output =
			queueBuilder.addAttachemnt("Output", [&](AttachmentBuilder &builder) -> Rc<Attachment> {
		// swapchain output
		builder.defineAsOutput();

		return Rc<vk::ImageAttachment>::create(builder,
				ImageInfo(info.extent, core::ForceImageUsage(core::ImageUsage::ColorAttachment
								| core::ImageUsage::InputAttachment),
						info.target->getCommonFormat()),
				core::ImageAttachment::AttachmentInfo{
					.initialLayout = AttachmentLayout::Undefined,
					.finalLayout = AttachmentLayout::PresentSrc,
					.clearOnLoad = true,
					.clearColor = info.backgroundColor // Color4F::WHITE;
				});
	});

	_shadow =
			queueBuilder.addAttachemnt("Shadow", [&](AttachmentBuilder &builder) -> Rc<Attachment> {
		return Rc<vk::ImageAttachment>::create(builder,
				ImageInfo(info.extent,
						core::ForceImageUsage(core::ImageUsage::ColorAttachment
								| core::ImageUsage::InputAttachment),
						core::ImageFormat::R16_SFLOAT),
				core::ImageAttachment::AttachmentInfo{
					.initialLayout = AttachmentLayout::Undefined,
					.finalLayout = AttachmentLayout::ShaderReadOnlyOptimal,
					.clearOnLoad = true,
					.clearColor = Color4F(0.0f, 0.0f, 0.0f, 0.0f) // Color4F::BLACK;
				});
	});

	// The frame distortion pair's intermediate, Particle-52: the displaced frame values the
	// shift subpass writes for the distorted emitters' footprints alone
	_distort =
			queueBuilder.addAttachemnt("Distort", [&](AttachmentBuilder &builder) -> Rc<Attachment> {
		return Rc<vk::ImageAttachment>::create(builder,
				ImageInfo(info.extent,
						core::ForceImageUsage(core::ImageUsage::ColorAttachment
										| core::ImageUsage::InputAttachment),
						info.target->getCommonFormat()),
				core::ImageAttachment::AttachmentInfo{
					.initialLayout = AttachmentLayout::Undefined,
					.finalLayout = AttachmentLayout::ShaderReadOnlyOptimal,
					.clearOnLoad = true,
					.clearColor = Color4F(0.0f, 0.0f, 0.0f, 0.0f)
				});
	});

	// The HDR world's own accumulation, Vfx-2: the posted frame's emitters draw here instead
	// of the scene subpass, linear and unclamped, the alpha accumulating premultiplied over
	// the transparent black the clear leaves. The bright pass and the tonemap composite read
	// it same-pixel, the blur chain the pair below walks the levels
	_hdr = queueBuilder.addAttachemnt("ParticleHdr",
			[&](AttachmentBuilder &builder) -> Rc<Attachment> {
		return Rc<vk::ImageAttachment>::create(builder,
				ImageInfo(info.extent,
						core::ForceImageUsage(core::ImageUsage::ColorAttachment
										| core::ImageUsage::InputAttachment
										| core::ImageUsage::Sampled),
						core::ImageFormat::R16G16B16A16_SFLOAT),
				core::ImageAttachment::AttachmentInfo{
					.initialLayout = AttachmentLayout::Undefined,
					.finalLayout = AttachmentLayout::ShaderReadOnlyOptimal,
					.clearOnLoad = true,
					.clearColor = Color4F(0.0f, 0.0f, 0.0f, 0.0f)
				});
	});

	_bloomA = queueBuilder.addAttachemnt("ParticleBloomA",
			[&](AttachmentBuilder &builder) -> Rc<Attachment> {
		return Rc<vk::ImageAttachment>::create(builder,
				ImageInfo(info.extent,
						core::ForceImageUsage(core::ImageUsage::ColorAttachment
										| core::ImageUsage::InputAttachment
										| core::ImageUsage::Sampled),
						core::ImageFormat::R16G16B16A16_SFLOAT),
				core::ImageAttachment::AttachmentInfo{
					.initialLayout = AttachmentLayout::Undefined,
					.finalLayout = AttachmentLayout::ShaderReadOnlyOptimal,
					.clearOnLoad = true,
					.clearColor = Color4F(0.0f, 0.0f, 0.0f, 0.0f)
				});
	});

	_depth2d = queueBuilder.addAttachemnt("CommonDepth2d",
			[&](AttachmentBuilder &builder) -> Rc<Attachment> {
		return Rc<vk::ImageAttachment>::create(builder,
				ImageInfo(info.extent,
						core::ForceImageUsage(core::ImageUsage::DepthStencilAttachment),
						VertexPass::selectDepthFormat(
								info.target->getSupportedDepthStencilFormat())),
				core::ImageAttachment::AttachmentInfo{.initialLayout = AttachmentLayout::Undefined,
					.finalLayout = AttachmentLayout::DepthStencilAttachmentOptimal,
					.clearOnLoad = true,
					.clearColor = Color4F::WHITE});
	});

	_sdf = queueBuilder.addAttachemnt("Sdf", [&](AttachmentBuilder &builder) -> Rc<Attachment> {
		return Rc<vk::ImageAttachment>::create(builder,
				ImageInfo(info.extent,
						core::ForceImageUsage(core::ImageUsage::ColorAttachment
								| core::ImageUsage::InputAttachment),
						core::ImageFormat::R16G16B16A16_SFLOAT),
				core::ImageAttachment::AttachmentInfo{.initialLayout = AttachmentLayout::Undefined,
					.finalLayout = AttachmentLayout::ShaderReadOnlyOptimal,
					.clearOnLoad = true,
					.clearColor = Color4F(config::VGPseudoSdfMax, 0.0f, 0.0f, 0.0f)});
	});

	_depthSdf = queueBuilder.addAttachemnt("DepthSdf",
			[&](AttachmentBuilder &builder) -> Rc<Attachment> {
		return Rc<vk::ImageAttachment>::create(builder,
				ImageInfo(info.extent,
						core::ForceImageUsage(core::ImageUsage::DepthStencilAttachment),
						VertexPass::selectDepthFormat(
								info.target->getSupportedDepthStencilFormat())),
				core::ImageAttachment::AttachmentInfo{.initialLayout = AttachmentLayout::Undefined,
					.finalLayout = AttachmentLayout::DepthStencilAttachmentOptimal,
					.clearOnLoad = true,
					.clearColor = Color4F::WHITE});
	});

	_materials = queueBuilder.addAttachemnt(FrameContext2d::MaterialAttachmentName,
			[&](AttachmentBuilder &builder) -> Rc<Attachment> {
		return Rc<vk::MaterialAttachment>::create(builder, texLayout);
	});

	_vertexes = queueBuilder.addAttachemnt(FrameContext2d::VertexAttachmentName,
			[&, this](AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsInput();
		return Rc<VertexAttachment>::create(builder, _materials, false,
				hasFlag(info.damage, core::QueueDamageFlags::PresentHint));
	});

	_lightsData = queueBuilder.addAttachemnt(FrameContext2d::LightDataAttachmentName,
			[](AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsInput();
		return Rc<ShadowLightDataAttachment>::create(builder);
	});

	// Rectangles of the presented image to copy out after the render pass ends; see FlatPass, which
	// declares the same attachment for the same reason.
	_capture = queueBuilder.addAttachemnt(core::FrameCaptureAttachmentName,
			[](AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsInput();
		return Rc<core::FrameCaptureAttachment>::create(builder);
	});

	auto colorAttachment = passBuilder.addAttachment(_output);
	auto shadowAttachment = passBuilder.addAttachment(_shadow);
	auto sdfAttachment = passBuilder.addAttachment(_sdf);
	auto sdfDepthAttachment = passBuilder.addAttachment(_depthSdf);
	auto depth2dAttachment = passBuilder.addAttachment(_depth2d);
	auto distortAttachment = passBuilder.addAttachment(_distort,
			AttachmentDependencyInfo::make(PipelineStage::ColorAttachmentOutput
							| PipelineStage::FragmentShader,
					AccessType::ColorAttachmentWrite | AccessType::InputAttachmantRead));

	auto hdrAttachment = passBuilder.addAttachment(_hdr,
			AttachmentDependencyInfo::make(PipelineStage::ColorAttachmentOutput
							| PipelineStage::FragmentShader,
					AccessType::ColorAttachmentWrite | AccessType::InputAttachmantRead
							| AccessType::ShaderRead));

	auto bloomAAttachment = passBuilder.addAttachment(_bloomA,
			AttachmentDependencyInfo::make(PipelineStage::ColorAttachmentOutput
							| PipelineStage::FragmentShader,
					AccessType::ColorAttachmentWrite | AccessType::InputAttachmantRead
							| AccessType::ShaderRead));

	_particles = info.particleAttachment;

	passBuilder.addAttachment(_vertexes);
	passBuilder.addAttachment(_lightsData);
	passBuilder.addAttachment(_materials);
	passBuilder.addAttachment(_capture);
	passBuilder.addAttachment(info.particleAttachment,
			AttachmentDependencyInfo::make(PipelineStage::VertexShader
							| PipelineStage::DrawIndirect,
					AccessType::IndirectCommandRead | AccessType::ShaderRead));

	auto layout2d =
			passBuilder.addDescriptorLayout("Layout2d", [&](PipelineLayoutBuilder &layoutBuilder) {
		layoutBuilder.setTextureSetLayout(texLayout);
	});

	auto layoutSdf =
			passBuilder.addDescriptorLayout("LayoutSdf", [&](PipelineLayoutBuilder &layoutBuilder) {
		layoutBuilder.addSet([&](DescriptorSetBuilder &setBuilder) {
			setBuilder.addDescriptor(sdfAttachment, DescriptorType::InputAttachment,
					AttachmentLayout::General);
		});
	});

	auto layoutShadow =
			passBuilder.addDescriptorLayout("Layout2d", [&](PipelineLayoutBuilder &layoutBuilder) {
		// Vertex input attachment - per-frame vertex list
		layoutBuilder.addSet([&](DescriptorSetBuilder &setBuilder) {
			setBuilder.addDescriptor(shadowAttachment, DescriptorType::InputAttachment,
					AttachmentLayout::ShaderReadOnlyOptimal);
			setBuilder.addDescriptor(sdfAttachment, DescriptorType::InputAttachment,
					AttachmentLayout::ShaderReadOnlyOptimal);
		});
	});

	// The frame distortion pair's layout, Particle-52: the settled frame for the shift draw's
	// displaced fetch, the intermediate for the composite draw's same-pixel backread. One set
	// serves both halves; each fragment binds the one it reads
	auto layoutDistort =
			passBuilder.addDescriptorLayout("LayoutDistort", [&](PipelineLayoutBuilder &layoutBuilder) {
		layoutBuilder.addSet([&](DescriptorSetBuilder &setBuilder) {
			setBuilder.addDescriptor(colorAttachment, DescriptorType::SampledImage,
					AttachmentLayout::General);
			setBuilder.addDescriptor(distortAttachment, DescriptorType::InputAttachment,
					AttachmentLayout::General);
		});
	});

	// The HDR world chain's own layouts, Vfx-2: the bright draw reads the world same-pixel,
	// the blur's two walks read their own half of the pair as sampled images, the composite
	// reads the world and the last level same-pixel
	auto layoutBloomBright =
			passBuilder.addDescriptorLayout("LayoutBloomBright",
					[&](PipelineLayoutBuilder &layoutBuilder) {
		layoutBuilder.addSet([&](DescriptorSetBuilder &setBuilder) {
			setBuilder.addDescriptor(hdrAttachment, DescriptorType::InputAttachment,
					AttachmentLayout::General);
		});
	});

	auto layoutBloomComposite =
			passBuilder.addDescriptorLayout("LayoutBloomComposite",
					[&](PipelineLayoutBuilder &layoutBuilder) {
		layoutBuilder.addSet([&](DescriptorSetBuilder &setBuilder) {
			setBuilder.addDescriptor(hdrAttachment, DescriptorType::InputAttachment,
					AttachmentLayout::General);
			setBuilder.addDescriptor(bloomAAttachment, DescriptorType::SampledImage,
					AttachmentLayout::General);
		});
	});

	auto subpass2d = passBuilder.addSubpass([&, this](SubpassBuilder &subpassBuilder) {
		makeMaterialSubpass(queueBuilder, passBuilder, subpassBuilder, layout2d, colorAttachment,
				shadowAttachment, depth2dAttachment);
	});

	auto subpassSdf = passBuilder.addSubpass([&](SubpassBuilder &subpassBuilder) {
		subpassBuilder.addColor(sdfAttachment,
				AttachmentDependencyInfo{
					PipelineStage::ColorAttachmentOutput,
					AccessType::ColorAttachmentWrite,
					PipelineStage::ColorAttachmentOutput,
					AccessType::ColorAttachmentWrite,
					FrameRenderPassState::Submitted,
				},
				AttachmentLayout::General);

		subpassBuilder.setDepthStencil(sdfDepthAttachment,
				AttachmentDependencyInfo{
					PipelineStage::EarlyFragmentTest,
					AccessType::DepthStencilAttachmentRead
							| AccessType::DepthStencilAttachmentWrite,
					PipelineStage::LateFragmentTest,
					AccessType::DepthStencilAttachmentRead
							| AccessType::DepthStencilAttachmentWrite,
					FrameRenderPassState::Submitted,
				});

		subpassBuilder.addInput(sdfAttachment,
				AttachmentDependencyInfo{
					PipelineStage::FragmentShader,
					AccessType::ShaderRead,
					PipelineStage::FragmentShader,
					AccessType::ShaderWrite,
					FrameRenderPassState::Submitted,
				},
				AttachmentLayout::General);

		auto pseudoSdfVert = queueBuilder.addProgramByRef("PseudoSdfVert", shaders::PseudoSdfVert);
		auto pseudoSdfFrag = queueBuilder.addProgramByRef("PseudoSdfFrag", shaders::PseudoSdfFrag);

		subpassBuilder.addGraphicPipeline(ShadowPass::PseudoSdfSolidPipeline,
				layoutSdf->defaultFamily,
				Vector<SpecializationInfo>({
					SpecializationInfo(pseudoSdfVert,
							Vector<SpecializationConstant>{
								SpecializationConstant(toInt(PseudoSdfSpecialization::Solid))}),
					SpecializationInfo(pseudoSdfFrag,
							Vector<SpecializationConstant>{
								SpecializationConstant(toInt(PseudoSdfSpecialization::Solid))}),
				}),
				PipelineMaterialInfo({BlendInfo(BlendFactor::One, BlendFactor::One, BlendOp::Min,
											  ColorComponentFlags::R),
					DepthInfo(true, true, CompareOp::LessOrEqual)}));

		subpassBuilder.addGraphicPipeline(ShadowPass::PseudoSdfPipeline, layoutSdf->defaultFamily,
				Vector<SpecializationInfo>({
					SpecializationInfo(pseudoSdfVert,
							Vector<SpecializationConstant>{
								SpecializationConstant(toInt(PseudoSdfSpecialization::Sdf))}),
					SpecializationInfo(pseudoSdfFrag,
							Vector<SpecializationConstant>{
								SpecializationConstant(toInt(PseudoSdfSpecialization::Sdf))}),
				}),
				PipelineMaterialInfo({BlendInfo(BlendFactor::One, BlendFactor::One, BlendOp::Min,
											  ColorComponentFlags::R),
					DepthInfo(false, true, CompareOp::Less)}));

		subpassBuilder.addGraphicPipeline(ShadowPass::PseudoSdfBackreadPipeline,
				layoutSdf->defaultFamily,
				Vector<SpecializationInfo>({
					SpecializationInfo(pseudoSdfVert,
							Vector<SpecializationConstant>{
								SpecializationConstant(toInt(PseudoSdfSpecialization::Backread))}),
					SpecializationInfo(pseudoSdfFrag,
							Vector<SpecializationConstant>{
								SpecializationConstant(toInt(PseudoSdfSpecialization::Backread))}),
				}),
				PipelineMaterialInfo({BlendInfo(BlendFactor::One, BlendFactor::Zero, BlendOp::Add,
											  ColorComponentFlags::G | ColorComponentFlags::B
													  | ColorComponentFlags::A),
					DepthInfo(false, true, CompareOp::Less)}));
	});

	auto subpassShadows = passBuilder.addSubpass([&](SubpassBuilder &subpassBuilder) {
		subpassBuilder.addColor(colorAttachment,
				AttachmentDependencyInfo{
					PipelineStage::ColorAttachmentOutput,
					AccessType::ColorAttachmentWrite,
					PipelineStage::ColorAttachmentOutput,
					AccessType::ColorAttachmentWrite,
					FrameRenderPassState::Submitted,
				},
				AttachmentLayout::ColorAttachmentOptimal);

		subpassBuilder.addInput(shadowAttachment,
				AttachmentDependencyInfo{
					PipelineStage::FragmentShader,
					AccessType::ShaderRead,
					PipelineStage::FragmentShader,
					AccessType::ShaderRead,
					FrameRenderPassState::Submitted,
				},
				AttachmentLayout::ShaderReadOnlyOptimal);

		subpassBuilder.addInput(sdfAttachment,
				AttachmentDependencyInfo{
					PipelineStage::FragmentShader,
					AccessType::ShaderRead,
					PipelineStage::FragmentShader,
					AccessType::ShaderRead,
					FrameRenderPassState::Submitted,
				},
				AttachmentLayout::ShaderReadOnlyOptimal);

		auto shadowVert =
				queueBuilder.addProgramByRef("PseudoMergeVert", shaders::PseudoSdfShadowVert);
		auto shadowFrag =
				queueBuilder.addProgramByRef("PseudoMergeFrag", shaders::PseudoSdfShadowFrag);
		auto frameClipperFrag =
				queueBuilder.addProgramByRef("FrameClipperFrag", shaders::FrameClipperFrag);

		// clang-format off
		subpassBuilder.addGraphicPipeline(ShadowPass::ShadowPipeline, layoutShadow->defaultFamily,
			Vector<SpecializationInfo>({// no specialization required for vertex shader
				shadowVert,
				shadowFrag
			}),
			PipelineMaterialInfo({
				BlendInfo(BlendFactor::Zero, BlendFactor::SrcColor, BlendOp::Add,
					BlendFactor::Zero, BlendFactor::One, BlendOp::Add), DepthInfo()}));
		// clang-format on

		subpassBuilder.addGraphicPipeline(ShadowPass::FrameClipperPipeline,
				layoutShadow->defaultFamily,
				Vector<SpecializationInfo>({
					shadowVert,
					frameClipperFrag,
				}),
				PipelineMaterialInfo({
					BlendInfo(BlendFactor::SrcColor, BlendFactor::DstColor, BlendOp::Min),
					DepthInfo(),
				}));
	});

	// The frame distortion pair, Particle-52: a distorted emitter's draw leaves the scene
	// subpass for these two. The shift subpass fetches the frame the earlier subpasses settled
	// at the coordinate the particle's color names and writes the fetched values into the
	// intermediate with the particle's coverage as the alpha; the composite subpass reads the
	// same pixel of the intermediate through an input attachment - the SDF backread's own
	// shape - and blends it into the frame under the emitter's blend mode, one pipeline per
	// registered pair. Every draw is scissored to the emitter's own box - no fullscreen pass,
	// and the displaced fetches never read what their own subpass writes
	auto distortVert = queueBuilder.addProgramByRef("Loader_MaterialVertByRef", shaders::MaterialVert);
	auto distortShiftFrag =
			queueBuilder.addProgramByRef("Loader_DistortShiftFrag", shaders::ParticleDistortShiftFrag);
	auto distortCompositeFrag =
			queueBuilder.addProgramByRef("Loader_DistortCompositeFrag",
					shaders::ParticleDistortCompositeFrag);

	auto subpassDistortShift = passBuilder.addSubpass([&](SubpassBuilder &subpassBuilder) {
		subpassBuilder.addColor(distortAttachment,
				AttachmentDependencyInfo{
					PipelineStage::ColorAttachmentOutput,
					AccessType::ColorAttachmentWrite,
					PipelineStage::ColorAttachmentOutput,
					AccessType::ColorAttachmentWrite,
					FrameRenderPassState::Submitted,
				},
				AttachmentLayout::General);

		subpassBuilder.addInput(colorAttachment,
				AttachmentDependencyInfo{
					PipelineStage::FragmentShader,
					AccessType::ShaderRead,
					PipelineStage::FragmentShader,
					AccessType::ShaderRead,
					FrameRenderPassState::Submitted,
				},
				AttachmentLayout::General);

		subpassBuilder.addGraphicPipeline(DistortShiftPipeline, layoutDistort->defaultFamily,
				Vector<SpecializationInfo>({
					distortVert,
					distortShiftFrag,
				}),
				PipelineMaterialInfo({BlendInfo(), DepthInfo(), ImageViewType::ImageView2D,
						PipelineVariant::ParticleDistort}));
	});

	auto subpassDistortComposite = passBuilder.addSubpass([&](SubpassBuilder &subpassBuilder) {
		subpassBuilder.addColor(colorAttachment,
				AttachmentDependencyInfo{
					PipelineStage::ColorAttachmentOutput,
					AccessType::ColorAttachmentWrite,
					PipelineStage::ColorAttachmentOutput,
					AccessType::ColorAttachmentWrite,
					FrameRenderPassState::Submitted,
				},
				AttachmentLayout::General);

		subpassBuilder.addInput(distortAttachment,
				AttachmentDependencyInfo{
					PipelineStage::FragmentShader,
					AccessType::ShaderRead,
					PipelineStage::FragmentShader,
					AccessType::ShaderWrite,
					FrameRenderPassState::Submitted,
				},
				AttachmentLayout::General);

		// The blend is part of the material key the same way the scene subpass's twins are:
		// the four pipelines carry the alpha/add/sub/mul pairs the emitter's mode asks for -
		// alpha the transparent pair the sprite draw itself lands under - and the variant
		// keeps every existing material's match away from them
		subpassBuilder.addGraphicPipeline(DistortCompositePipelines[0], layoutDistort->defaultFamily,
				Vector<SpecializationInfo>({
					distortVert,
					distortCompositeFrag,
				}),
				PipelineMaterialInfo({BlendInfo(BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha,
											BlendOp::Add, BlendFactor::Zero, BlendFactor::One,
											BlendOp::Add),
						DepthInfo(), ImageViewType::ImageView2D,
						PipelineVariant::ParticleDistort}));

		subpassBuilder.addGraphicPipeline(DistortCompositePipelines[1], layoutDistort->defaultFamily,
				Vector<SpecializationInfo>({
					distortVert,
					distortCompositeFrag,
				}),
				PipelineMaterialInfo({BlendInfo(BlendFactor::SrcAlpha, BlendFactor::One, BlendOp::Add,
											BlendFactor::Zero, BlendFactor::One, BlendOp::Add),
						DepthInfo(), ImageViewType::ImageView2D,
						PipelineVariant::ParticleDistort}));

		subpassBuilder.addGraphicPipeline(DistortCompositePipelines[2], layoutDistort->defaultFamily,
				Vector<SpecializationInfo>({
					distortVert,
					distortCompositeFrag,
				}),
				PipelineMaterialInfo({BlendInfo(BlendFactor::SrcAlpha, BlendFactor::One,
											BlendOp::ReverseSubtract, BlendFactor::Zero,
											BlendFactor::One, BlendOp::Add),
						DepthInfo(), ImageViewType::ImageView2D,
						PipelineVariant::ParticleDistort}));

		subpassBuilder.addGraphicPipeline(DistortCompositePipelines[3], layoutDistort->defaultFamily,
				Vector<SpecializationInfo>({
					distortVert,
					distortCompositeFrag,
				}),
				PipelineMaterialInfo({BlendInfo(BlendFactor::DstColor, BlendFactor::OneMinusSrcAlpha,
											BlendOp::Add, BlendFactor::Zero, BlendFactor::One,
											BlendOp::Add),
						DepthInfo(), ImageViewType::ImageView2D,
						PipelineVariant::ParticleDistort}));
	});

	// The HDR world's own chain, Vfx-2: the world's emitter spans draw into the floating-point
	// target under the material twins, the bright pass lifts what sits above the threshold,
	// the three fixed levels of separable blur walk the pair, and the tonemap composite
	// returns the world to the frame as a premultiplied pair. A frame without the post walks
	// all of them empty, which is the old frame exactly
	auto subpassBloomWorld = passBuilder.addSubpass([&](SubpassBuilder &subpassBuilder) {
		subpassBuilder.addColor(hdrAttachment,
				AttachmentDependencyInfo{
					PipelineStage::ColorAttachmentOutput,
					AccessType::ColorAttachmentWrite,
					PipelineStage::ColorAttachmentOutput,
					AccessType::ColorAttachmentWrite,
					FrameRenderPassState::Submitted,
				},
				AttachmentLayout::General);

		makeBloomMaterialPipelines(queueBuilder, subpassBuilder, layout2d);
	});

	auto bloomVert = queueBuilder.addProgramByRef("Loader_BloomVertByRef", shaders::PseudoSdfShadowVert);

	auto subpassBloomBright = passBuilder.addSubpass([&](SubpassBuilder &subpassBuilder) {
		subpassBuilder.addColor(bloomAAttachment,
				AttachmentDependencyInfo{
					PipelineStage::ColorAttachmentOutput,
					AccessType::ColorAttachmentWrite,
					PipelineStage::ColorAttachmentOutput,
					AccessType::ColorAttachmentWrite,
					FrameRenderPassState::Submitted,
				},
				AttachmentLayout::General);

		subpassBuilder.addInput(hdrAttachment,
				AttachmentDependencyInfo{
					PipelineStage::FragmentShader,
					AccessType::InputAttachmantRead,
					PipelineStage::FragmentShader,
					AccessType::ShaderWrite,
					FrameRenderPassState::Submitted,
				},
				AttachmentLayout::General);

		auto bloomBrightFrag =
				queueBuilder.addProgramByRef("Loader_BloomBrightFrag", shaders::BloomBrightFrag);

		subpassBuilder.addGraphicPipeline(ShadowPass::BloomBrightPipeline,
				layoutBloomBright->defaultFamily,
				Vector<SpecializationInfo>({
					bloomVert,
					bloomBrightFrag,
				}),
				PipelineMaterialInfo({BlendInfo(), DepthInfo()}));
	});

	auto subpassBloomComposite = passBuilder.addSubpass([&](SubpassBuilder &subpassBuilder) {
		subpassBuilder.addColor(colorAttachment,
				AttachmentDependencyInfo{
					PipelineStage::ColorAttachmentOutput,
					AccessType::ColorAttachmentWrite,
					PipelineStage::ColorAttachmentOutput,
					AccessType::ColorAttachmentWrite,
					FrameRenderPassState::Submitted,
				},
				AttachmentLayout::General);

		subpassBuilder.addInput(hdrAttachment,
				AttachmentDependencyInfo{
					PipelineStage::FragmentShader,
					AccessType::InputAttachmantRead,
					PipelineStage::FragmentShader,
					AccessType::ShaderWrite,
					FrameRenderPassState::Submitted,
				},
				AttachmentLayout::General);

		subpassBuilder.addInput(bloomAAttachment,
				AttachmentDependencyInfo{
					PipelineStage::FragmentShader,
					AccessType::ShaderRead,
					PipelineStage::ColorAttachmentOutput,
					AccessType::ColorAttachmentWrite,
					FrameRenderPassState::Submitted,
				},
				AttachmentLayout::General);

		auto bloomCompositeFrag = queueBuilder.addProgramByRef("Loader_BloomCompositeFrag",
				shaders::BloomCompositeFrag);

		// the premultiplied pair: a pixel with no world in it is a no-op, so the scene the
		// earlier subpasses drew - the chrome among it - stands exactly what it was
		subpassBuilder.addGraphicPipeline(ShadowPass::BloomCompositePipeline,
				layoutBloomComposite->defaultFamily,
				Vector<SpecializationInfo>({
					bloomVert,
					bloomCompositeFrag,
				}),
				PipelineMaterialInfo({BlendInfo(BlendFactor::One, BlendFactor::OneMinusSrcAlpha,
											  BlendOp::Add, BlendFactor::Zero, BlendFactor::One,
											  BlendOp::Add),
						DepthInfo()}));
	});

	// the world subpass settles the target the bright pass lifts and the composite returns
	passBuilder.addSubpassDependency(subpassBloomWorld, PipelineStage::ColorAttachmentOutput,
			AccessType::ColorAttachmentWrite, subpassBloomBright,
			PipelineStage::FragmentShader, AccessType::InputAttachmantRead, true);


	passBuilder.addSubpassDependency(subpassBloomWorld, PipelineStage::ColorAttachmentOutput,
			AccessType::ColorAttachmentWrite, subpassBloomComposite,
			PipelineStage::FragmentShader, AccessType::InputAttachmantRead, true);

	// the bright pass's own write hands the bloom to the composite as a sampled read
	passBuilder.addSubpassDependency(subpassBloomBright, PipelineStage::ColorAttachmentOutput,
			AccessType::ColorAttachmentWrite, subpassBloomComposite,
			PipelineStage::FragmentShader, AccessType::ShaderRead, false);

	// the scene and the distortion pair wrote the frame the composite blends the world over
	passBuilder.addSubpassDependency(subpassDistortComposite, PipelineStage::ColorAttachmentOutput,
			AccessType::ColorAttachmentWrite, subpassBloomComposite,
			PipelineStage::FragmentShader | PipelineStage::ColorAttachmentOutput,
			AccessType::InputAttachmantRead | AccessType::ColorAttachmentWrite, true);

	passBuilder.addSubpassDependency(subpass2d, PipelineStage::LateFragmentTest,
			AccessType::DepthStencilAttachmentWrite, subpassShadows, PipelineStage::FragmentShader,
			AccessType::InputAttachmantRead, true);

	passBuilder.addSubpassDependency(subpass2d, PipelineStage::ColorAttachmentOutput,
			AccessType::ColorAttachmentWrite, subpassShadows,
			PipelineStage::FragmentShader | PipelineStage::ColorAttachmentOutput,
			AccessType::InputAttachmantRead | AccessType::ColorAttachmentWrite, true);

	passBuilder.addSubpassDependency(subpassSdf, PipelineStage::ColorAttachmentOutput,
			AccessType::ColorAttachmentWrite, subpassShadows,
			PipelineStage::FragmentShader | PipelineStage::ColorAttachmentOutput,
			AccessType::InputAttachmantRead | AccessType::ColorAttachmentWrite, true);

	// self-dependency to read from color output
	passBuilder.addSubpassDependency(subpassSdf, PipelineStage::ColorAttachmentOutput,
			AccessType::ColorAttachmentWrite, subpassSdf,
			PipelineStage::ColorAttachmentOutput | PipelineStage::FragmentShader,
			AccessType::ColorAttachmentWrite | AccessType::InputAttachmantRead, true);

	// the pair's dependencies: the scene and the shadow composite settle the frame before the
	// shift subpass reads it, and the intermediate settles before the same-pixel backread
	passBuilder.addSubpassDependency(subpass2d, PipelineStage::ColorAttachmentOutput,
			AccessType::ColorAttachmentWrite, subpassDistortShift,
			PipelineStage::FragmentShader | PipelineStage::ColorAttachmentOutput,
			AccessType::ShaderRead | AccessType::InputAttachmantRead
					| AccessType::ColorAttachmentWrite,
			true);

	passBuilder.addSubpassDependency(subpassShadows, PipelineStage::ColorAttachmentOutput,
			AccessType::ColorAttachmentWrite, subpassDistortShift,
			PipelineStage::FragmentShader | PipelineStage::ColorAttachmentOutput,
			AccessType::ShaderRead | AccessType::InputAttachmantRead
					| AccessType::ColorAttachmentWrite,
			true);

	passBuilder.addSubpassDependency(subpassDistortShift, PipelineStage::ColorAttachmentOutput,
			AccessType::ColorAttachmentWrite, subpassDistortComposite,
			PipelineStage::FragmentShader | PipelineStage::ColorAttachmentOutput,
			AccessType::InputAttachmantRead | AccessType::ColorAttachmentWrite, true);

	passBuilder.setAcquireTimestamps(2);

	if (!VertexPass::init(passBuilder)) {
		return false;
	}

	_flags = info.flags;
	return true;
}

auto ShadowPass::makeFrameHandle(const FrameQueue &handle) -> Rc<QueuePassHandle> {
	return Rc<ShadowPassHandle>::create(*this, handle);
}

// The HDR world subpass's material twins, Vfx-2: the scene subpass's own blend pairs and the
// mask path's particle fragment, the same shaders and the same texture set, declared against
// the floating-point target under the ParticlePost variant no material resolves to - the
// world's draws bind them by the blend mode and the mask the way the distortion pair binds
// its composite, never through a material
void ShadowPass::makeBloomMaterialPipelines(Queue::Builder &queueBuilder,
		core::SubpassBuilder &subpassBuilder, const core::PipelineLayoutData *layout2d) {
	using namespace core;

	auto bloomVert =
			queueBuilder.addProgramByRef("Loader_BloomMaterialVertByRef", shaders::MaterialVert);

	auto nsamplers = layout2d->textureSetLayout->samplers.size();

	auto materialSpecInfo = Vector<SpecializationInfo>({
		core::SpecializationInfo(bloomVert),
		core::SpecializationInfo(
			queueBuilder.addProgramByRef("Loader_BloomMaterialFragByRef", shaders::MaterialFrag),
			Vector<SpecializationConstant>{
				SpecializationConstant([nsamplers](const core::Device &dev,
											   const PipelineLayoutData &) -> SpecializationConstant {
					return uint32_t(nsamplers);
				}),
				SpecializationConstant([](const core::Device &dev,
									 const PipelineLayoutData &data) -> SpecializationConstant {
					auto l = data.textureSetLayout->layout.get_cast<vk::TextureSetLayout>()
									 ->getImageCount();
					return uint32_t(l);
				}),
				SpecializationConstant(0)
			}
		),
	});

	auto particleSpecInfo = Vector<SpecializationInfo>({
		core::SpecializationInfo(bloomVert),
		core::SpecializationInfo(queueBuilder.addProgramByRef("Loader_BloomParticleFragByRef",
										 shaders::ParticleFrag)),
	});

	// the blend pairs are the scene twins' own, the depth the target has none of: the world
	// draws in the span order the scene walks, the painter's order the transparent level keeps.
	// The alpha twin's ALPHA factors are Horizon-2's own: (One, OneMinusSrcAlpha), the coverage
	// the composite's veil reads accumulating over draws - the scene twins' (Zero, One) left
	// the world's alpha channel empty forever, so a posted alpha-blended puff never occluded
	// the frame beneath and the composite returned it as pure addition. The light modes - add,
	// sub, mul - keep the passthrough pair: light does not cover
	subpassBuilder.addGraphicPipeline(ShadowPass::BloomMaterialPipelines[0], layout2d->defaultFamily,
			materialSpecInfo,
			PipelineMaterialInfo({BlendInfo(BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha,
										 BlendOp::Add, BlendFactor::One,
										 BlendFactor::OneMinusSrcAlpha, BlendOp::Add),
					DepthInfo(), ImageViewType::ImageView2D,
					PipelineVariant::ParticlePost}));

	subpassBuilder.addGraphicPipeline(ShadowPass::BloomMaterialPipelines[1], layout2d->defaultFamily,
			materialSpecInfo,
			PipelineMaterialInfo({BlendInfo(BlendFactor::SrcAlpha, BlendFactor::One, BlendOp::Add,
										  BlendFactor::Zero, BlendFactor::One, BlendOp::Add),
					DepthInfo(), ImageViewType::ImageView2D,
					PipelineVariant::ParticlePost}));

	subpassBuilder.addGraphicPipeline(ShadowPass::BloomMaterialPipelines[2], layout2d->defaultFamily,
			materialSpecInfo,
			PipelineMaterialInfo({BlendInfo(BlendFactor::SrcAlpha, BlendFactor::One,
										  BlendOp::ReverseSubtract, BlendFactor::Zero,
										  BlendFactor::One, BlendOp::Add),
					DepthInfo(), ImageViewType::ImageView2D,
					PipelineVariant::ParticlePost}));

	subpassBuilder.addGraphicPipeline(ShadowPass::BloomMaterialPipelines[3], layout2d->defaultFamily,
			materialSpecInfo,
			PipelineMaterialInfo({BlendInfo(BlendFactor::DstColor, BlendFactor::OneMinusSrcAlpha,
										  BlendOp::Add, BlendFactor::Zero, BlendFactor::One,
										  BlendOp::Add),
					DepthInfo(), ImageViewType::ImageView2D,
					PipelineVariant::ParticlePost}));

	subpassBuilder.addGraphicPipeline(ShadowPass::BloomParticlePipelines[0], layout2d->defaultFamily,
			particleSpecInfo,
			PipelineMaterialInfo({BlendInfo(BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha,
										 BlendOp::Add, BlendFactor::One,
										 BlendFactor::OneMinusSrcAlpha, BlendOp::Add),
					DepthInfo(), ImageViewType::ImageView2D,
					PipelineVariant::ParticlePost}));

	subpassBuilder.addGraphicPipeline(ShadowPass::BloomParticlePipelines[1], layout2d->defaultFamily,
			particleSpecInfo,
			PipelineMaterialInfo({BlendInfo(BlendFactor::SrcAlpha, BlendFactor::One, BlendOp::Add,
										  BlendFactor::Zero, BlendFactor::One, BlendOp::Add),
					DepthInfo(), ImageViewType::ImageView2D,
					PipelineVariant::ParticlePost}));

	subpassBuilder.addGraphicPipeline(ShadowPass::BloomParticlePipelines[2], layout2d->defaultFamily,
			particleSpecInfo,
			PipelineMaterialInfo({BlendInfo(BlendFactor::SrcAlpha, BlendFactor::One,
										  BlendOp::ReverseSubtract, BlendFactor::Zero,
										  BlendFactor::One, BlendOp::Add),
					DepthInfo(), ImageViewType::ImageView2D,
					PipelineVariant::ParticlePost}));

	subpassBuilder.addGraphicPipeline(ShadowPass::BloomParticlePipelines[3], layout2d->defaultFamily,
			particleSpecInfo,
			PipelineMaterialInfo({BlendInfo(BlendFactor::DstColor, BlendFactor::OneMinusSrcAlpha,
										  BlendOp::Add, BlendFactor::Zero, BlendFactor::One,
										  BlendOp::Add),
					DepthInfo(), ImageViewType::ImageView2D,
					PipelineVariant::ParticlePost}));
}

void ShadowPass::makeMaterialSubpass(Queue::Builder &queueBuilder, QueuePassBuilder &passBuilder,
		core::SubpassBuilder &subpassBuilder, const core::PipelineLayoutData *layout2d,
		const core::AttachmentPassData *colorAttachment,
		const core::AttachmentPassData *shadowAttachment,
		const core::AttachmentPassData *depth2dAttachment) {
	using namespace core;

	// load shaders by ref - do not copy data into engine
	auto materialVert = queueBuilder.addProgram("Loader_MaterialVert",
			[](core::Device &dev, const core::ProgramData::DataCallback &cb) {
		cb(shaders::MaterialVert);
	});

	auto materialFrag = queueBuilder.addProgramByRef("Loader_MaterialFrag", shaders::MaterialFrag);

	auto nsamplers = layout2d->textureSetLayout->samplers.size();

	// clang-format off
	auto shaderSpecInfo = Vector<SpecializationInfo>({
		core::SpecializationInfo(
			materialVert
		),
		core::SpecializationInfo(
			materialFrag,
			Vector<SpecializationConstant>{
				SpecializationConstant([nsamplers](const core::Device &dev, const PipelineLayoutData &) -> SpecializationConstant {
					return uint32_t(nsamplers);
				}),
				SpecializationConstant([](const core::Device &dev, const PipelineLayoutData &data) -> SpecializationConstant {
					auto l = data.textureSetLayout->layout.get_cast<vk::TextureSetLayout>()->getImageCount();
					return uint32_t(l);
				}),
				SpecializationConstant(0)
			}
		)
	});
	// clang-format on

	// pipelines for material-besed rendering
	auto materialPipeline =
			subpassBuilder.addGraphicPipeline("Solid", layout2d->defaultFamily, shaderSpecInfo,
					PipelineMaterialInfo({BlendInfo(), DepthInfo(true, true, CompareOp::Less),
						ImageViewType::ImageView2D}));

	auto transparentPipeline = subpassBuilder.addGraphicPipeline("Transparent",
			layout2d->defaultFamily, shaderSpecInfo,
			PipelineMaterialInfo({BlendInfo(BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha,
										  BlendOp::Add, BlendFactor::Zero, BlendFactor::One,
										  BlendOp::Add),
				DepthInfo(false, true, CompareOp::LessOrEqual), ImageViewType::ImageView2D}));

	// The additive twin of the transparent pipeline - what a ParticleEmitter with
	// ParticleBlendMode::Add asks for. The blend is part of the material key, so no existing
	// request reaches this pipeline but an additive one.
	subpassBuilder.addGraphicPipeline("Add", layout2d->defaultFamily, shaderSpecInfo,
			PipelineMaterialInfo({BlendInfo(BlendFactor::SrcAlpha, BlendFactor::One, BlendOp::Add,
										  BlendFactor::Zero, BlendFactor::One, BlendOp::Add),
				DepthInfo(false, true, CompareOp::LessOrEqual), ImageViewType::ImageView2D}));

	// The subtractive and multiplying twins, what ParticleBlendMode::Sub and ParticleBlendMode::Mul
	// ask for: the reverse-subtract equation is the destination minus the alpha-weighted source,
	// and the (DstColor, OneMinusSrcAlpha) pair scales the destination by the source while the
	// transparent edge eases to one. The blend is part of the material key, so no request
	// reaches these but a sub or mul one.
	subpassBuilder.addGraphicPipeline("Sub", layout2d->defaultFamily, shaderSpecInfo,
			PipelineMaterialInfo({BlendInfo(BlendFactor::SrcAlpha, BlendFactor::One,
										  BlendOp::ReverseSubtract, BlendFactor::Zero,
										  BlendFactor::One, BlendOp::Add),
				DepthInfo(false, true, CompareOp::LessOrEqual), ImageViewType::ImageView2D}));

	subpassBuilder.addGraphicPipeline("Mul", layout2d->defaultFamily, shaderSpecInfo,
			PipelineMaterialInfo({BlendInfo(BlendFactor::DstColor, BlendFactor::OneMinusSrcAlpha,
										  BlendOp::Add, BlendFactor::Zero, BlendFactor::One,
										  BlendOp::Add),
				DepthInfo(false, true, CompareOp::LessOrEqual), ImageViewType::ImageView2D}));

	// pipeline for debugging - draw lines instead of triangles
	subpassBuilder.addGraphicPipeline("DebugTriangles", layout2d->defaultFamily, shaderSpecInfo,
			PipelineMaterialInfo(BlendInfo(BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha,
										 BlendOp::Add, BlendFactor::Zero, BlendFactor::One,
										 BlendOp::Add),
					DepthInfo(false, true, CompareOp::LessOrEqual), ImageViewType::ImageView2D,
					LineWidth(1.0f)));

	// clang-format off
	auto shaderTex2dArraySpecInfo = Vector<SpecializationInfo>({
		core::SpecializationInfo(
			materialVert
		),
		core::SpecializationInfo(
			materialFrag,
			Vector<SpecializationConstant>{
				SpecializationConstant([nsamplers](const core::Device &dev, const PipelineLayoutData &) -> SpecializationConstant {
					return uint32_t(nsamplers);
				}),
				SpecializationConstant([](const core::Device &dev, const PipelineLayoutData &data) -> SpecializationConstant {
					auto l = data.textureSetLayout->layout.get_cast<vk::TextureSetLayout>()->getImageCount();
					return uint32_t(l);
				}),
				SpecializationConstant(1)
			}
		)
	});
	// clang-format on

	// pipelines for material-besed rendering
	subpassBuilder.addGraphicPipeline("Solid_Tex2dArrayFrag", layout2d->defaultFamily,
			shaderTex2dArraySpecInfo,
			PipelineMaterialInfo({BlendInfo(), DepthInfo(true, true, CompareOp::Less),
				ImageViewType::ImageView2DArray}));

	subpassBuilder.addGraphicPipeline("Transparent_Tex2dArrayFrag", layout2d->defaultFamily,
			shaderTex2dArraySpecInfo,
			PipelineMaterialInfo({BlendInfo(BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha,
										  BlendOp::Add, BlendFactor::Zero, BlendFactor::One,
										  BlendOp::Add),
				DepthInfo(false, true, CompareOp::LessOrEqual), ImageViewType::ImageView2DArray}));

	// clang-format off
	auto shaderTex3dSpecInfo = Vector<SpecializationInfo>({
		core::SpecializationInfo(
			materialVert
		),
		core::SpecializationInfo(
			materialFrag,
			Vector<SpecializationConstant>{
				SpecializationConstant([nsamplers](const core::Device &dev, const PipelineLayoutData &) -> SpecializationConstant {
					return uint32_t(nsamplers);
				}),
				SpecializationConstant([](const core::Device &dev, const PipelineLayoutData &data) -> SpecializationConstant {
					auto l = data.textureSetLayout->layout.get_cast<vk::TextureSetLayout>()->getImageCount();
					return uint32_t(l);
				}),
				SpecializationConstant(2)
			}
		)
	});
	// clang-format on

	// pipelines for material-besed rendering
	subpassBuilder.addGraphicPipeline("Solid_Tex3dFrag", layout2d->defaultFamily,
			shaderTex3dSpecInfo,
			PipelineMaterialInfo({BlendInfo(), DepthInfo(true, true, CompareOp::Less),
				ImageViewType::ImageView3D}));

	subpassBuilder.addGraphicPipeline("Transparent_Tex3dFrag", layout2d->defaultFamily,
			shaderTex3dSpecInfo,
			PipelineMaterialInfo({BlendInfo(BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha,
										  BlendOp::Add, BlendFactor::Zero, BlendFactor::One,
										  BlendOp::Add),
				DepthInfo(false, true, CompareOp::LessOrEqual), ImageViewType::ImageView3D}));

	// The particle draw's own fragment - the quad's alpha from its UV, no texture sampled. The
	// variant keeps these pipelines from sharing one material lookup with their sampling twins,
	// so only a masked emitter's material request reaches them
	auto particleFrag = queueBuilder.addProgramByRef("Loader_ParticleFrag", shaders::ParticleFrag);

	auto particleSpecInfo = Vector<SpecializationInfo>({
		core::SpecializationInfo(materialVert),
		core::SpecializationInfo(particleFrag),
	});

	subpassBuilder.addGraphicPipeline("Solid_Particle", layout2d->defaultFamily,
			particleSpecInfo,
			PipelineMaterialInfo({BlendInfo(), DepthInfo(true, true, CompareOp::Less),
				ImageViewType::ImageView2D, PipelineVariant::ParticleQuadMask}));

	subpassBuilder.addGraphicPipeline("Transparent_Particle", layout2d->defaultFamily,
			particleSpecInfo,
			PipelineMaterialInfo({BlendInfo(BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha,
										 BlendOp::Add, BlendFactor::Zero, BlendFactor::One,
										 BlendOp::Add),
				DepthInfo(false, true, CompareOp::LessOrEqual), ImageViewType::ImageView2D,
				PipelineVariant::ParticleQuadMask}));

	// The masked draw's own additive twin, the same opened destination the base Add pipeline
	// above carries: a builtin-masked emitter can be additive too
	subpassBuilder.addGraphicPipeline("Add_Particle", layout2d->defaultFamily,
			particleSpecInfo,
			PipelineMaterialInfo({BlendInfo(BlendFactor::SrcAlpha, BlendFactor::One, BlendOp::Add,
										 BlendFactor::Zero, BlendFactor::One, BlendOp::Add),
				DepthInfo(false, true, CompareOp::LessOrEqual), ImageViewType::ImageView2D,
				PipelineVariant::ParticleQuadMask}));

	// The masked draw's subtractive and multiplying twins, the same equations the base Sub and
	// Mul pipelines above carry: a builtin-masked emitter can subtract and multiply too
	subpassBuilder.addGraphicPipeline("Sub_Particle", layout2d->defaultFamily,
			particleSpecInfo,
			PipelineMaterialInfo({BlendInfo(BlendFactor::SrcAlpha, BlendFactor::One,
										 BlendOp::ReverseSubtract, BlendFactor::Zero,
										 BlendFactor::One, BlendOp::Add),
				DepthInfo(false, true, CompareOp::LessOrEqual), ImageViewType::ImageView2D,
				PipelineVariant::ParticleQuadMask}));

	subpassBuilder.addGraphicPipeline("Mul_Particle", layout2d->defaultFamily,
			particleSpecInfo,
			PipelineMaterialInfo({BlendInfo(BlendFactor::DstColor, BlendFactor::OneMinusSrcAlpha,
										 BlendOp::Add, BlendFactor::Zero, BlendFactor::One,
										 BlendOp::Add),
				DepthInfo(false, true, CompareOp::LessOrEqual), ImageViewType::ImageView2D,
				PipelineVariant::ParticleQuadMask}));

	static_cast<MaterialAttachment *>(_materials->attachment.get())
			->addPredefinedMaterials(Vector<Rc<Material>>({
				Rc<Material>::create(Material::MaterialIdInitial, materialPipeline,
						layout2d->textureSetLayout->queue->emptyImage, ColorMode::IntensityChannel),
				Rc<Material>::create(Material::MaterialIdInitial, materialPipeline,
						layout2d->textureSetLayout->queue->solidImage, ColorMode::IntensityChannel),
				Rc<Material>::create(Material::MaterialIdInitial, transparentPipeline,
						layout2d->textureSetLayout->queue->emptyImage, ColorMode()),
				Rc<Material>::create(Material::MaterialIdInitial, transparentPipeline,
						layout2d->textureSetLayout->queue->solidImage, ColorMode()),
			}));

	subpassBuilder.addColor(colorAttachment,
			AttachmentDependencyInfo{
				PipelineStage::ColorAttachmentOutput,
				AccessType::ColorAttachmentWrite,
				PipelineStage::ColorAttachmentOutput,
				AccessType::ColorAttachmentWrite,
				FrameRenderPassState::Submitted,
			},
			AttachmentLayout::ColorAttachmentOptimal);

	subpassBuilder.addColor(shadowAttachment,
			AttachmentDependencyInfo{
				PipelineStage::ColorAttachmentOutput,
				AccessType::ColorAttachmentWrite,
				PipelineStage::ColorAttachmentOutput,
				AccessType::ColorAttachmentWrite,
				FrameRenderPassState::Submitted,
			},
			BlendInfo(BlendFactor::One, core::BlendFactor::One, core::BlendOp::Max));

	subpassBuilder.setDepthStencil(depth2dAttachment,
			AttachmentDependencyInfo{
				PipelineStage::EarlyFragmentTest,
				AccessType::DepthStencilAttachmentRead | AccessType::DepthStencilAttachmentWrite,
				PipelineStage::LateFragmentTest,
				AccessType::DepthStencilAttachmentRead | AccessType::DepthStencilAttachmentWrite,
				FrameRenderPassState::Submitted,
			});
}

bool ShadowPassHandle::prepare(FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = static_cast<ShadowPass *>(_queuePass.get());

	if (auto lightsBuffer = q.getAttachment(pass->getLightsData())) {
		auto lightsData =
				static_cast<ShadowLightDataAttachmentHandle *>(lightsBuffer->handle.get());
		_shadowData = lightsData;

		if (lightsData && lightsData->getLightsCount()) {
			if (auto vertexBuffer = q.getAttachment(pass->getVertexes())) {
				auto vertexHandle =
						static_cast<const VertexAttachmentHandle *>(vertexBuffer->handle.get());
				lightsData->allocateBuffer(static_cast<DeviceFrameHandle *>(q.getFrame().get()), 0,
						vertexHandle->getMaxShadowValue());
			}
		}
	}

	if (auto sdfImage = q.getAttachment(pass->getSdf())) {
		_sdfImage = static_cast<const ImageAttachmentHandle *>(sdfImage->handle.get());
	}

	return VertexPassHandle::prepare(q, sp::move(cb));
}

void ShadowPassHandle::prepareMaterialCommands(core::MaterialSet *materials, CommandBuffer &buf) {
	auto fb = getFramebuffer();
	if (!fb) {
		log::source().error("vk::ShadowPass", "prepareMaterialCommands: no framebuffer");
		VertexPassHandle::prepareMaterialCommands(materials, buf);
		buf.cmdNextSubpass();
		buf.cmdNextSubpass();
		return;
	}
	auto currentExtent = fb->getExtent();

	auto drawFullscreen = [&](const core::GraphicPipelineData *pipeline) {
		if (!pipeline || !pipeline->pipeline) {
			return;
		}
		auto viewport = VkViewport{0.0f, 0.0f, float(currentExtent.width),
			float(currentExtent.height), 0.0f, 1.0f};
		buf.cmdSetViewport(0, makeSpanView(&viewport, 1));

		auto scissorRect = VkRect2D{{0, 0}, {currentExtent.width, currentExtent.height}};
		buf.cmdSetScissor(0, makeSpanView(&scissorRect, 1));

		buf.cmdDrawIndexed(6, // indexCount
				1, // instanceCount
				6, // firstIndex
				0, // int32_t   vertexOffset
				0 // uint32_t  firstInstance
		);
	};

	// The post words the frame's own render infos carry, Vfx-2: any emitter with them moves
	// the whole world into the floating-point target - the document is this window's whole
	// world, and the scene subpass of a posted frame draws everything but the world
	_postOn = false;
	_postThreshold = 1.0f;
	_postStrength = 1.0f;
	if (_particles) {
		for (auto &span : _vertexBuffer->getVertexData()) {
			if (span.particleEmitterId == 0) {
				continue;
			}
			auto emitterRenderInfo = _particles->getEmitterRenderInfo(span.particleEmitterId);
			if (emitterRenderInfo && emitterRenderInfo->post != 0u) {
				_postOn = true;
				_postThreshold = emitterRenderInfo->bloomThreshold;
				_postStrength = emitterRenderInfo->bloomStrength;
				break;
			}
		}
	}

	// The scene subpass draws everything but the distorted spans - those leave for the
	// trailing pair, where the frame they displace is already settled - and a posted frame's
	// emitter spans leave for the world's own subpass after it
	drawSpans(materials, buf, _vertexBuffer->getVertexData(), DistortFilter::SkipDistort, _postOn);

	uint32_t subpassIdx = 0;

	// Always advance through SDF + Shadow subpasses to match the render pass.
	const bool haveLights =
			_shadowData && _shadowData->getBuffer() && _shadowData->getLightsCount();

	if (haveLights) {
		// sdf drawing pipeline
		subpassIdx = buf.cmdNextSubpass();
		auto commands = _vertexBuffer->getCommands();
		auto solidPipeline = _data->subpasses[subpassIdx]->graphicPipelines.get(
				StringView(ShadowPass::PseudoSdfSolidPipeline));
		auto sdfPipeline = _data->subpasses[subpassIdx]->graphicPipelines.get(
				StringView(ShadowPass::PseudoSdfPipeline));
		auto backreadPipeline = _data->subpasses[subpassIdx]->graphicPipelines.get(
				StringView(ShadowPass::PseudoSdfBackreadPipeline));

		PSDFConstantData pcb;
		pcb.vertexPointer =
				UVec2::convertFromPacked(buf.bindBufferAddress(_vertexBuffer->getVertexes().get()));
		pcb.transformPointer = UVec2::convertFromPacked(
				buf.bindBufferAddress(_vertexBuffer->getTransforms().get()));
		pcb.shadowDataPointer =
				UVec2::convertFromPacked(buf.bindBufferAddress(_shadowData->getBuffer().get()));
		pcb.pseudoSdfInset = config::VGPseudoSdfInset;
		pcb.pseudoSdfOffset = config::VGPseudoSdfOffset;
		pcb.pseudoSdfMax = config::VGPseudoSdfMax;

		if (solidPipeline && solidPipeline->pipeline) {
			buf.cmdBindPipelineWithDescriptors(solidPipeline);

			buf.cmdPushConstants(VK_SHADER_STAGE_VERTEX_BIT, 0,
					BytesView(reinterpret_cast<const uint8_t *>(&pcb), sizeof(PSDFConstantData)));

			clearDynamicState(buf);

			for (auto &materialVertexSpan : _vertexBuffer->getShadowSolidData()) {
				applyDynamicState(commands, buf, materialVertexSpan.state);

				buf.cmdDrawIndexed(materialVertexSpan.indexCount, // indexCount
						materialVertexSpan.instanceCount, // instanceCount
						materialVertexSpan.firstIndex, // firstIndex
						materialVertexSpan.vertexOffset, // int32_t   vertexOffset
						materialVertexSpan.firstInstance // uint32_t  firstInstance
				);
			}
		}

		if (sdfPipeline && sdfPipeline->pipeline) {
			buf.cmdBindPipelineWithDescriptors(sdfPipeline);

			clearDynamicState(buf);

			for (auto &materialVertexSpan : _vertexBuffer->getShadowSdfData()) {
				applyDynamicState(commands, buf, materialVertexSpan.state);

				buf.cmdDrawIndexed(materialVertexSpan.indexCount, // indexCount
						materialVertexSpan.instanceCount, // instanceCount
						materialVertexSpan.firstIndex, // firstIndex
						materialVertexSpan.vertexOffset, // int32_t   vertexOffset
						materialVertexSpan.firstInstance // uint32_t  firstInstance
				);
			}
		}

		if (_sdfImage && _sdfImage->getImage() && _sdfImage->getImage()->getImage()) {
			ImageMemoryBarrier inImageBarriers[] = {
				ImageMemoryBarrier(static_cast<Image *>(_sdfImage->getImage()->getImage().get()),
						VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
						VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
						VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL)};

			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
							| VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					VK_DEPENDENCY_BY_REGION_BIT, inImageBarriers);
		}

		if (backreadPipeline && backreadPipeline->pipeline) {
			buf.cmdBindPipelineWithDescriptors(backreadPipeline);

			clearDynamicState(buf);

			for (auto &materialVertexSpan : _vertexBuffer->getShadowSdfData()) {
				applyDynamicState(commands, buf, materialVertexSpan.state);

				buf.cmdDrawIndexed(materialVertexSpan.indexCount, // indexCount
						materialVertexSpan.instanceCount, // instanceCount
						materialVertexSpan.firstIndex, // firstIndex
						materialVertexSpan.vertexOffset, // int32_t   vertexOffset
						materialVertexSpan.firstInstance // uint32_t  firstInstance
				);
			}
		}

		subpassIdx = buf.cmdNextSubpass();

		// shadow drawing pipeline
		auto pipeline = _data->subpasses[subpassIdx]->graphicPipelines.get(
				StringView(ShadowPass::ShadowPipeline));
		if (pipeline && pipeline->pipeline) {
			buf.cmdBindPipelineWithDescriptors(pipeline);
			buf.cmdPushConstants(VK_SHADER_STAGE_FRAGMENT_BIT, 0,
					BytesView(reinterpret_cast<const uint8_t *>(&pcb.shadowDataPointer),
							sizeof(uint64_t)));
			drawFullscreen(pipeline);
		}

	} else {
		subpassIdx = buf.cmdNextSubpass();
		subpassIdx = buf.cmdNextSubpass();
	}

	const auto *input = _shadowData ? _shadowData->getFrameInput() : nullptr;
	if (input && input->decorations.borderRadius > 0.0f) {
		FrameClipperData data;
		data.frameSize = Vec2(_constraints.extent.width, _constraints.extent.height);
		data.radius = input->decorations.borderRadius * _constraints.surfaceDensity;
		data.shadow = input->decorations.shadowRadius * _constraints.surfaceDensity;

		const float sigma = sqrtf((data.shadow * data.shadow) / (-2.0f * logf(1.0f / 255.0f)));
		data.sigma = (-1.0f / (2.0f * sigma * sigma));
		data.value = input->decorations.shadowValue;
		data.offset = input->decorations.shadowOffset * _constraints.surfaceDensity;

		data.constraints = toInt(input->decorations.viewConstraints) | (1 << 4);
		if (input->decorations.drawUserShadows) {
			data.constraints |= (1 << 4);
		}

		auto pipeline = _data->subpasses[subpassIdx]->graphicPipelines.get(
				StringView(ShadowPass::FrameClipperPipeline));
		if (pipeline && pipeline->pipeline) {
			buf.cmdBindPipelineWithDescriptors(pipeline);
			buf.cmdPushConstants(VK_SHADER_STAGE_FRAGMENT_BIT, 0,
					BytesView(reinterpret_cast<const uint8_t *>(&data), sizeof(FrameClipperData)));
			drawFullscreen(pipeline);
		}
	}

	// The distortion pair's own draws
	recordDistortionSubpasses(buf, _vertexBuffer->getVertexData());

	// The HDR world's own draws, Vfx-2
	recordPostSubpasses(buf, _vertexBuffer->getVertexData());
}

void ShadowPassHandle::recordOverlaySubpasses(CommandBuffer &buf, SpanView<VertexSpan> spans) {
	// the overlay's scene subpass keeps everything but the distorted spans, same as the main
	// walk; the pair draws them after the SDF and shadow subpasses are walked empty. A posted
	// frame's emitter spans leave for the world's own subpass, which the overlay walks empty -
	// the drag ghost is chrome, and the composite the main instance drew has already returned
	// the world to the frame
	drawSpans(_materialBuffer->getSet().get(), buf, spans, DistortFilter::SkipDistort, _postOn);

	// The render pass has eight subpasses and an instance must walk all of them. The overlay
	// draws the scene and, below, the distorted spans; SDF and shadow subpasses are advanced
	// empty, as for a frame with no lights, and the HDR world's bright and composite the same
	// way - the drag ghost is chrome, and the world the main instance returned stands.
	buf.cmdNextSubpass();
	buf.cmdNextSubpass();

	recordDistortionSubpasses(buf, spans);

	buf.cmdNextSubpass();
	buf.cmdNextSubpass();
	buf.cmdNextSubpass();
}

void ShadowPassHandle::recordDistortionSubpasses(CommandBuffer &buf, SpanView<VertexSpan> spans) {
	auto commands = _vertexBuffer->getCommands();
	auto pass = static_cast<RenderPass *>(_data->impl.get());

	// the shift subpass: the settled frame fetched at the coordinate the particle color names
	auto subpassIdx = buf.cmdNextSubpass();
	auto shiftPipeline = _data->subpasses[subpassIdx]->graphicPipelines.get(
			StringView(ShadowPass::DistortShiftPipeline));

	// the composite subpass: the intermediate blended back, one pipeline per blend mode in
	// the ParticleBlendMode order alpha, add, sub, mul
	subpassIdx = buf.cmdNextSubpass();
	const core::GraphicPipelineData *compositePipelines[4] = {
		_data->subpasses[subpassIdx]->graphicPipelines.get(
				StringView(ShadowPass::DistortCompositePipelines[0])),
		_data->subpasses[subpassIdx]->graphicPipelines.get(
				StringView(ShadowPass::DistortCompositePipelines[1])),
		_data->subpasses[subpassIdx]->graphicPipelines.get(
				StringView(ShadowPass::DistortCompositePipelines[2])),
		_data->subpasses[subpassIdx]->graphicPipelines.get(
				StringView(ShadowPass::DistortCompositePipelines[3])),
	};

	if (!shiftPipeline || !shiftPipeline->pipeline) {
		return;
	}

	clearDynamicState(buf);

	auto particleVertexes = _particles ? _particles->getVertices() : nullptr;
	if (!particleVertexes) {
		return;
	}

	for (auto &materialVertexSpan : spans) {
		if (materialVertexSpan.particleEmitterId == 0) {
			continue;
		}
		auto emitterRenderInfo =
				_particles->getEmitterRenderInfo(materialVertexSpan.particleEmitterId);
		if (!emitterRenderInfo || emitterRenderInfo->distortion == 0u) {
			continue;
		}

		auto material = _materialBuffer->getSet()->getMaterialById(materialVertexSpan.material);
		if (!material) {
			continue;
		}

		applyDynamicState(commands, buf, materialVertexSpan.state);

		// the shift draw: the displaced fetch and the mask ride the particle push constants
		buf.cmdBindPipelineWithDescriptors(shiftPipeline);

		glsl::ParticleDrawConstantData pcbShift;
		pcbShift.base.vertexPointer =
				UVec2::convertFromPacked(particleVertexes->getDeviceAddress());
		pcbShift.base.transformPointer = UVec2::convertFromPacked(
				buf.bindBufferAddress(_vertexBuffer->getTransforms().get()));
		pcbShift.base.materialPointer =
				UVec2::convertFromPacked(buf.bindBufferAddress(material->getBuffer()));
		pcbShift.base.atlasPointer = UVec2(0, 0);
		if (auto a = material->getAtlas()) {
			if (auto atlasBuf = a->getBuffer()) {
				pcbShift.base.atlasPointer =
						UVec2::convertFromPacked(buf.bindBufferAddress(atlasBuf));
			}
		}
		pcbShift.base.imageIdx = material->getImages().front().descriptor;
		pcbShift.base.samplerIdx = material->getImages().front().sampler;
		pcbShift.base.gradientOffset = materialVertexSpan.gradientOffset;
		pcbShift.base.gradientCount = materialVertexSpan.gradientCount;
		// the mask word's own free tail carries the emitter's distortion scale, Horizon-2's H3:
		// the shift fragment reads its pixel reach there, the sixteen the fresh emitter carries
		// the old shader's own constant - one push word, no struct seat, no extra constant
		pcbShift.mask = glsl::vec4(float(emitterRenderInfo->maskShape),
				emitterRenderInfo->maskThickness, emitterRenderInfo->maskSoftness,
				emitterRenderInfo->distortionScale);
		buf.cmdPushConstants(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
				BytesView(reinterpret_cast<const uint8_t *>(&pcbShift),
						sizeof(glsl::ParticleDrawConstantData)));
		buf.cmdDrawIndirect(_particles->getCommands(),
				emitterRenderInfo->index * sizeof(ParticleIndirectCommand), 1,
				sizeof(ParticleIndirectCommand));

		// the composite draw: the emitter's blend mode picks the pipeline, the alpha the
		// intermediate carries is the coverage the blend factors weigh
		const auto mode = emitterRenderInfo->blendMode;
		if (mode < 4u) {
			if (auto composite = compositePipelines[mode]; composite && composite->pipeline) {
				buf.cmdBindPipelineWithDescriptors(composite);

				glsl::VertexConstantData pcbComposite;
				pcbComposite.vertexPointer =
						UVec2::convertFromPacked(particleVertexes->getDeviceAddress());
				pcbComposite.transformPointer = UVec2::convertFromPacked(
						buf.bindBufferAddress(_vertexBuffer->getTransforms().get()));
				pcbComposite.materialPointer =
						UVec2::convertFromPacked(buf.bindBufferAddress(material->getBuffer()));
				pcbComposite.atlasPointer = pcbShift.base.atlasPointer;
				pcbComposite.imageIdx = material->getImages().front().descriptor;
				pcbComposite.samplerIdx = material->getImages().front().sampler;
				pcbComposite.gradientOffset = materialVertexSpan.gradientOffset;
				pcbComposite.gradientCount = materialVertexSpan.gradientCount;
				buf.cmdPushConstants(VK_SHADER_STAGE_VERTEX_BIT, 0,
						BytesView(reinterpret_cast<const uint8_t *>(&pcbComposite),
								sizeof(glsl::VertexConstantData)));
				buf.cmdDrawIndirect(_particles->getCommands(),
						emitterRenderInfo->index * sizeof(ParticleIndirectCommand), 1,
						sizeof(ParticleIndirectCommand));
			}
		}
	}
}


// The HDR world's own recording, Vfx-2: the world's emitter spans into the floating-point
// target, the bright pass at the document's threshold, the tonemap composite at the
// document's strength - the composite walks the multi-scale kernel over the bright pass's
// own output itself. A frame without the post draws no world and every subpass is walked
// empty, which is the old frame exactly
void ShadowPassHandle::recordPostSubpasses(CommandBuffer &buf, SpanView<VertexSpan> spans) {
	auto commands = _vertexBuffer->getCommands();
	auto pass = static_cast<RenderPass *>(_data->impl.get());

	// the world subpass: the posted frame's emitter spans draw into the floating-point target
	// through the material twins, bound by the blend mode and the mask the way the distortion
	// pair binds its composite
	auto subpassIdx = buf.cmdNextSubpass();
	const core::GraphicPipelineData *materialPipelines[4] = {
		_data->subpasses[subpassIdx]->graphicPipelines.get(
				StringView(ShadowPass::BloomMaterialPipelines[0])),
		_data->subpasses[subpassIdx]->graphicPipelines.get(
				StringView(ShadowPass::BloomMaterialPipelines[1])),
		_data->subpasses[subpassIdx]->graphicPipelines.get(
				StringView(ShadowPass::BloomMaterialPipelines[2])),
		_data->subpasses[subpassIdx]->graphicPipelines.get(
				StringView(ShadowPass::BloomMaterialPipelines[3])),
	};
	const core::GraphicPipelineData *particlePipelines[4] = {
		_data->subpasses[subpassIdx]->graphicPipelines.get(
				StringView(ShadowPass::BloomParticlePipelines[0])),
		_data->subpasses[subpassIdx]->graphicPipelines.get(
				StringView(ShadowPass::BloomParticlePipelines[1])),
		_data->subpasses[subpassIdx]->graphicPipelines.get(
				StringView(ShadowPass::BloomParticlePipelines[2])),
		_data->subpasses[subpassIdx]->graphicPipelines.get(
				StringView(ShadowPass::BloomParticlePipelines[3])),
	};

	if (_postOn && _particles && _particles->getVertices()) {
		auto materials = _materialBuffer->getSet().get();
		auto particleVertexes = _particles->getVertices();

		clearDynamicState(buf);

		uint32_t boundTextureSetIndex = maxOf<uint32_t>();

		for (auto &materialVertexSpan : spans) {
			if (materialVertexSpan.particleEmitterId == 0) {
				continue;
			}
			auto emitterRenderInfo =
					_particles->getEmitterRenderInfo(materialVertexSpan.particleEmitterId);
			if (!emitterRenderInfo) {
				continue;
			}
			// Horizon-2, H4: a distorted emitter's draw belongs to the trailing pair alone -
			// the pair displaces the settled frame, and the same span drawn here as an ordinary
			// tinted quad was the bubble the lens showed beside its own displacement
			if (emitterRenderInfo->distortion != 0u) {
				continue;
			}
			// Horizon-2, H1: a subtracting emitter never enters the world - the float target's
			// blend write clamps the reverse-subtract's negative result to zero (probed on this
			// device: an additive quad under a full sub quad answers the empty world exactly),
			// so its darkness cannot live in the world's light. The scene subpass draws it - the
			// span the posted routing below leaves in the scene - and the sub it always carried
			// blackens the frame the same bytes the unposted frame always read
			if (emitterRenderInfo->blendMode == 2u) {
				continue;
			}

			auto material = materials->getMaterialById(materialVertexSpan.material);
			if (!material) {
				continue;
			}

			const auto mode = emitterRenderInfo->blendMode;
			if (mode >= 4u) {
				continue;
			}

			auto pipeline = emitterRenderInfo->maskShape != 0u ? particlePipelines[mode]
															   : materialPipelines[mode];
			if (!pipeline || !pipeline->pipeline) {
				continue;
			}

			applyDynamicState(commands, buf, materialVertexSpan.state);

			buf.cmdBindPipelineWithDescriptors(pipeline);

			// the material's own texture set, the same binding the scene's draws make
			auto textureSetIndex = material->getLayoutIndex();
			if (textureSetIndex != boundTextureSetIndex) {
				auto l = materials->getLayout(textureSetIndex);
				if (l && l->set) {
					auto s = static_cast<TextureSet *>(l->set.get());
					auto set = s->getSet();
					buf.cmdBindDescriptorSets(static_cast<RenderPass *>(_data->impl.get()),
							makeSpanView(&set, 1), pipeline->layout->sets.size());
					boundTextureSetIndex = textureSetIndex;
				} else {
					continue;
				}
			}

			glsl::VertexConstantData pcb;
			pcb.vertexPointer =
					UVec2::convertFromPacked(particleVertexes->getDeviceAddress());
			pcb.transformPointer = UVec2::convertFromPacked(
					buf.bindBufferAddress(_vertexBuffer->getTransforms().get()));
			pcb.materialPointer =
					UVec2::convertFromPacked(buf.bindBufferAddress(material->getBuffer()));
			pcb.atlasPointer = UVec2(0, 0);
			if (auto a = material->getAtlas()) {
				if (auto atlasBuf = a->getBuffer()) {
					pcb.atlasPointer = UVec2::convertFromPacked(buf.bindBufferAddress(atlasBuf));
				}
			}
			pcb.imageIdx = material->getImages().front().descriptor;
			pcb.samplerIdx = material->getImages().front().sampler;
			pcb.gradientOffset = materialVertexSpan.gradientOffset;
			pcb.gradientCount = materialVertexSpan.gradientCount;

			if (emitterRenderInfo->maskShape != 0u) {
				glsl::ParticleDrawConstantData pcbMasked;
				pcbMasked.base = pcb;
				pcbMasked.mask = glsl::vec4(float(emitterRenderInfo->maskShape),
						emitterRenderInfo->maskThickness, emitterRenderInfo->maskSoftness, 0.0f);
				buf.cmdPushConstants(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
						BytesView(reinterpret_cast<const uint8_t *>(&pcbMasked),
								sizeof(glsl::ParticleDrawConstantData)));
			} else {
				buf.cmdPushConstants(VK_SHADER_STAGE_VERTEX_BIT, 0,
						BytesView(reinterpret_cast<const uint8_t *>(&pcb),
								sizeof(glsl::VertexConstantData)));
			}

			buf.cmdDrawIndirect(_particles->getCommands(),
					emitterRenderInfo->index * sizeof(ParticleIndirectCommand), 1,
					sizeof(ParticleIndirectCommand));
		}
	}

	auto drawFullscreen = [&](const core::GraphicPipelineData *pipeline) {
		if (!pipeline || !pipeline->pipeline) {
			return;
		}
		auto currentExtent = getFramebuffer()->getExtent();
		auto viewport = VkViewport{0.0f, 0.0f, float(currentExtent.width),
			float(currentExtent.height), 0.0f, 1.0f};
		buf.cmdSetViewport(0, makeSpanView(&viewport, 1));

		auto scissorRect = VkRect2D{{0, 0}, {currentExtent.width, currentExtent.height}};
		buf.cmdSetScissor(0, makeSpanView(&scissorRect, 1));

		buf.cmdDrawIndexed(6, // indexCount
				1, // instanceCount
				6, // firstIndex
				0, // int32_t   vertexOffset
				0 // uint32_t  firstInstance
		);
	};

	// the bright pass: what sits above the threshold, the alpha scaled by the same factor
	subpassIdx = buf.cmdNextSubpass();
	auto brightPipeline = _data->subpasses[subpassIdx]->graphicPipelines.get(
			StringView(ShadowPass::BloomBrightPipeline));

	if (_postOn && brightPipeline && brightPipeline->pipeline) {
		buf.cmdBindPipelineWithDescriptors(brightPipeline);

		struct BrightPcb {
			float threshold;
		} brightPcb{_postThreshold};

		buf.cmdPushConstants(VK_SHADER_STAGE_FRAGMENT_BIT, 0,
				BytesView(reinterpret_cast<const uint8_t *>(&brightPcb), sizeof(BrightPcb)));
		drawFullscreen(brightPipeline);
	}

	// the tonemap composite: the multi-scale kernel over the bright pass's own output, the
	// strength the document named
	subpassIdx = buf.cmdNextSubpass();
	auto compositePipeline = _data->subpasses[subpassIdx]->graphicPipelines.get(
			StringView(ShadowPass::BloomCompositePipeline));

	if (_postOn && compositePipeline && compositePipeline->pipeline) {
		buf.cmdBindPipelineWithDescriptors(compositePipeline);

		struct CompositePcb {
			float strength;
			float pad[3];
		} compositePcb{_postStrength, {0.0f, 0.0f, 0.0f}};

		buf.cmdPushConstants(VK_SHADER_STAGE_FRAGMENT_BIT, 0,
				BytesView(reinterpret_cast<const uint8_t *>(&compositePcb),
						sizeof(CompositePcb)));
		drawFullscreen(compositePipeline);
	}
}

} // namespace stappler::xenolith::basic2d::vk
