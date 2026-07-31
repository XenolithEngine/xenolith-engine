/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#include "XL2dVkFlatPass.h"

#include "XLCoreEnum.h"
#include "XLCoreQueueData.h"
#include "XL2dFrameContext.h"
#include "XLVkTextureSet.h"
#include "glsl/XL2dShaders.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::vk {

class IgnoredInputAttachmentHandle : public core::AttachmentHandle {
public:
	virtual ~IgnoredInputAttachmentHandle() = default;

	virtual void submitInput(core::FrameQueue &, Rc<core::AttachmentInputData> &&,
			Function<void(bool)> &&cb) override {
		cb(true);
	}
};

Rc<core::AttachmentHandle> IgnoredInputAttachment::makeFrameHandle(const core::FrameQueue &queue) {
	return Rc<IgnoredInputAttachmentHandle>::create(*this, queue);
}

bool FlatPass::makeRenderQueue(Queue::Builder &builder, RenderQueueInfo &info) {
	using namespace core;

	builder.setDamageFlags(info.damage);

	// A single graphics pass - no compute pass for particles.
	builder.addPass("MaterialSwapchainPass", PassType::Graphics, RenderOrderingHighest,
			[&](QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		return Rc<FlatPass>::create(builder, passBuilder, info);
	});

	return true;
}

bool FlatPass::init(Queue::Builder &queueBuilder, QueuePassBuilder &passBuilder,
		const RenderQueueInfo &info) {
	using namespace core;

	// Sampler order is baked into materials via MaterialInfo::samplers - keep it in sync with
	// ShadowPass::init.
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

	_output = queueBuilder.addAttachemnt("Output",
			[&](AttachmentBuilder &builder) -> Rc<Attachment> {
		// swapchain output
		builder.defineAsOutput();

		return Rc<vk::ImageAttachment>::create(builder,
				ImageInfo(info.extent, core::ForceImageUsage(core::ImageUsage::ColorAttachment),
						info.target->getCommonFormat()),
				core::ImageAttachment::AttachmentInfo{
					.initialLayout = AttachmentLayout::Undefined,
					.finalLayout = AttachmentLayout::PresentSrc,
					.clearOnLoad = true,
					.clearColor = info.backgroundColor,
				});
	});

	_materials = queueBuilder.addAttachemnt(FrameContext2d::MaterialAttachmentName,
			[&](AttachmentBuilder &builder) -> Rc<Attachment> {
		return Rc<vk::MaterialAttachment>::create(builder, texLayout);
	});

	_vertexes = queueBuilder.addAttachemnt(FrameContext2d::VertexAttachmentName,
			[&, this](AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsInput();
		// flat order: no depth buffer, so spans are emitted in painter's order.
		// damage tracking: the lightweight queue is where partial redraw will land, and the
		// present hint is useful on its own
		return Rc<VertexAttachment>::create(builder, _materials, true,
				hasFlag(info.damage, core::QueueDamageFlags::PresentHint));
	});

	// FrameContext2d submits lights and particle emitters unconditionally and refuses to
	// initialize when the attachments are missing - accept and drop them.
	auto lights = queueBuilder.addAttachemnt(FrameContext2d::LightDataAttachmentName,
			[](AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsInput();
		return Rc<IgnoredInputAttachment>::create(builder);
	});

	auto particles = queueBuilder.addAttachemnt(FrameContext2d::ParticleEmittersAttachment,
			[](AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsInput();
		return Rc<IgnoredInputAttachment>::create(builder);
	});

	auto colorAttachment = passBuilder.addAttachment(_output);

	passBuilder.addAttachment(_vertexes);
	passBuilder.addAttachment(_materials);
	passBuilder.addAttachment(lights);
	passBuilder.addAttachment(particles);

	// Must stay at index 0 - VertexPassHandle::prepareMaterialCommands pushes constants through
	// getPipelineLayout(0).
	auto layout2d =
			passBuilder.addDescriptorLayout("Layout2d", [&](PipelineLayoutBuilder &layoutBuilder) {
		layoutBuilder.setTextureSetLayout(texLayout);
	});

	passBuilder.addSubpass([&, this](SubpassBuilder &subpassBuilder) {
		makeMaterialSubpass(queueBuilder, subpassBuilder, layout2d, colorAttachment);
	});

	passBuilder.setAcquireTimestamps(2);

	return VertexPass::init(passBuilder);
}

auto FlatPass::makeFrameHandle(const FrameQueue &handle) -> Rc<QueuePassHandle> {
	return Rc<FlatPassHandle>::create(*this, handle);
}

void FlatPass::makeMaterialSubpass(Queue::Builder &queueBuilder,
		core::SubpassBuilder &subpassBuilder, const core::PipelineLayoutData *layout2d,
		const core::AttachmentPassData *colorAttachment) {
	using namespace core;

	auto flatVert = queueBuilder.addProgramByRef("Loader_FlatVert", shaders::FlatVert);
	auto flatFrag = queueBuilder.addProgramByRef("Loader_FlatFrag", shaders::FlatFrag);

	auto nsamplers = layout2d->textureSetLayout->samplers.size();

	auto makeSpecInfo = [&](uint32_t imageType) {
		// clang-format off
		return Vector<SpecializationInfo>({
			core::SpecializationInfo(
				flatVert
			),
			core::SpecializationInfo(
				flatFrag,
				Vector<SpecializationConstant>{
					SpecializationConstant([nsamplers](const core::Device &dev, const PipelineLayoutData &) -> SpecializationConstant {
						return uint32_t(nsamplers);
					}),
					SpecializationConstant([](const core::Device &dev, const PipelineLayoutData &data) -> SpecializationConstant {
						auto l = data.textureSetLayout->layout.get_cast<vk::TextureSetLayout>()->getImageCount();
						return uint32_t(l);
					}),
					SpecializationConstant(imageType)
				}
			)
		});
		// clang-format on
	};

	auto shaderSpecInfo = makeSpecInfo(0);

	// PipelineMaterialInfo (DepthInfo included) must stay byte-identical to ShadowPass: materials
	// are matched to pipelines by that struct's value, not by pipeline name, and Sprite bakes
	// DepthInfo into the material request. The depth state itself is dropped at pipeline creation
	// because the subpass has no depth attachment.
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

	auto shaderTex2dArraySpecInfo = makeSpecInfo(1);

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

	auto shaderTex3dSpecInfo = makeSpecInfo(2);

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

	// fallback materials for any Layer/Sprite that does not define its own
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
}

} // namespace stappler::xenolith::basic2d::vk
