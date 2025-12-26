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

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::vk {

enum class PseudoSdfSpecialization {
	Solid = 0,
	Sdf = 1,
	Backread = 2
};

bool ShadowPass::makeRenderQueue(Queue::Builder &builder, RenderQueueInfo &info) {
	using namespace core;

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
				ImageInfo(info.extent, core::ForceImageUsage(core::ImageUsage::ColorAttachment),
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
		return Rc<VertexAttachment>::create(builder, _materials);
	});

	_lightsData = queueBuilder.addAttachemnt(FrameContext2d::LightDataAttachmentName,
			[](AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsInput();
		return Rc<ShadowLightDataAttachment>::create(builder);
	});

	auto colorAttachment = passBuilder.addAttachment(_output);
	auto shadowAttachment = passBuilder.addAttachment(_shadow);
	auto sdfAttachment = passBuilder.addAttachment(_sdf);
	auto sdfDepthAttachment = passBuilder.addAttachment(_depthSdf);
	auto depth2dAttachment = passBuilder.addAttachment(_depth2d);

	_particles = info.particleAttachment;

	passBuilder.addAttachment(_vertexes);
	passBuilder.addAttachment(_lightsData);
	passBuilder.addAttachment(_materials);
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
	auto currentExtent = fb->getExtent();

	auto drawFullscreen = [&](const core::GraphicPipelineData *pipeline) {
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

	VertexPassHandle::prepareMaterialCommands(materials, buf);

	uint32_t subpassIdx = 0;

	if (_shadowData->getLightsCount()) {
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

		ImageMemoryBarrier inImageBarriers[] = {
			ImageMemoryBarrier(static_cast<Image *>(_sdfImage->getImage()->getImage().get()),
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
					VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL)};

		buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
						| VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				VK_DEPENDENCY_BY_REGION_BIT, inImageBarriers);

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

		subpassIdx = buf.cmdNextSubpass();

		// shadow drawing pipeline
		auto pipeline = _data->subpasses[subpassIdx]->graphicPipelines.get(
				StringView(ShadowPass::ShadowPipeline));
		buf.cmdBindPipelineWithDescriptors(pipeline);
		buf.cmdPushConstants(VK_SHADER_STAGE_FRAGMENT_BIT, 0,
				BytesView(reinterpret_cast<const uint8_t *>(&pcb.shadowDataPointer),
						sizeof(uint64_t)));
		drawFullscreen(pipeline);

	} else {
		subpassIdx = buf.cmdNextSubpass();
		subpassIdx = buf.cmdNextSubpass();
	}

	auto input = _shadowData->getFrameInput();
	if (input->decorations.borderRadius > 0.0f) {
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
		buf.cmdBindPipelineWithDescriptors(pipeline);
		buf.cmdPushConstants(VK_SHADER_STAGE_FRAGMENT_BIT, 0,
				BytesView(reinterpret_cast<const uint8_t *>(&data), sizeof(FrameClipperData)));
		drawFullscreen(pipeline);
	}
}

} // namespace stappler::xenolith::basic2d::vk
