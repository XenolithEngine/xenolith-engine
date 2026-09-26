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


#include "XLCompositorVkPass.h"

#if MODULE_XENOLITH_RENDERER_COMPOSITOR_VK

#include "XLVkDevice.h"
#include "XLVkRenderPass.h"
#include "XLVkObject.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreFrameHandle.h"
#include "XLCoreQueueData.h"
#include "glsl/include/XLCompositorGlslData.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::compositor::vk {

namespace shaders {

#include "xl_compositor_quad.vert.h"
#include "xl_compositor_plane.frag.h"
#include "xl_compositor_solid.frag.h"

static SpanView<uint32_t> QuadVert(reinterpret_cast<const uint32_t *>(xl_compositor_quad_vert),
		sizeof(xl_compositor_quad_vert) / sizeof(uint32_t));
static SpanView<uint32_t> PlaneFrag(reinterpret_cast<const uint32_t *>(xl_compositor_plane_frag),
		sizeof(xl_compositor_plane_frag) / sizeof(uint32_t));
static SpanView<uint32_t> SolidFrag(reinterpret_cast<const uint32_t *>(xl_compositor_solid_frag),
		sizeof(xl_compositor_solid_frag) / sizeof(uint32_t));

} // namespace shaders

static constexpr StringView CompositorPass_Solid = "Solid";
static constexpr StringView CompositorPass_Opaque = "Opaque";
static constexpr StringView CompositorPass_Premultiplied = "Premultiplied";

// XL_WM_PROFILE=1: what each composed frame drew.
static bool CompositorVkPass_isProfiling() {
	static const bool s_profile = [] {
		auto value = ::getenv("XL_WM_PROFILE");
		return value && StringView(value) != "0";
	}();
	return s_profile;
}

bool PlaneSetHandle::writeDescriptor(const core::QueuePassHandle &,
		core::DescriptorImageInfo &info) {
	auto &view = info.index < _views.size() ? _views[info.index] : _empty;
	if (!view || !_sampler) {
		return false;
	}
	info.imageView = view;
	info.sampler = _sampler;
	info.layout = core::AttachmentLayout::ShaderReadOnlyOptimal;
	return true;
}

uint32_t PlaneSetHandle::enumerateDirtyDescriptors(const PassHandle &,
		const core::PipelineDescriptor &, const core::DescriptorBinding &,
		const Callback<void(uint32_t)> &cb) const {
	for (uint32_t i = 0; i < CompositorPass::MaxPlanes; ++i) { cb(i); }
	return CompositorPass::MaxPlanes;
}

void PlaneSetHandle::setImages(Vector<Rc<core::ImageView>> &&views, Rc<core::ImageView> &&empty,
		Rc<core::Sampler> &&sampler) {
	_views = sp::move(views);
	_empty = sp::move(empty);
	_sampler = sp::move(sampler);
}

bool CompositorPassHandle::prepare(core::FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = static_cast<CompositorPass *>(_queuePass.get());

	auto &constraints = q.getFrame()->getFrameConstraints();
	_extent = Extent2(constraints.extent.width, constraints.extent.height);
	if (auto output = static_cast<xvk::ImageAttachmentHandle *>(
				getAttachmentHandle(pass->getOutput()))) {
		if (auto image = output->getImage()) {
			auto extent = image->getInfo().extent;
			_extent = Extent2(extent.width, extent.height);
		}
	}

	if (auto planes = getAttachmentHandle(pass->getPlaneSet())) {
		if (auto input = dynamic_cast<PlaneSetInput *>(planes->getInput())) {
			collectPlanes(*input, _extent);
		}
	}

	_onLocked = sp::move(cb);
	_pendingLocks = uint32_t(_planes.size());
	_lockDeferred = false;

	// A frame being copied (a screenshot moves its layout) is sampled only after the copy.
	auto loop = q.getLoop();
	for (auto &it : _planes) {
		it.frame->lock(core::PlaneFrameAccess::Shared, loop,
				[this, handle = Rc<CompositorPassHandle>(this), queue = Rc<core::FrameQueue>(&q)](
						Rc<core::PlaneFrameLock> &&lock) {
			_locks.emplace_back(sp::move(lock));
			if (--_pendingLocks == 0 && _lockDeferred) {
				if (prepareLocked(*queue)) {
					if (auto cb = sp::move(_onLocked)) {
						cb(true);
					}
				}
			}
		});
	}

	if (_pendingLocks == 0) {
		return prepareLocked(q);
	}

	_lockDeferred = true;
	return false;
}

bool CompositorPassHandle::prepareLocked(core::FrameQueue &q) {
	auto pass = static_cast<CompositorPass *>(_queuePass.get());
	auto device = static_cast<xvk::Device *>(q.getFrame()->getDevice());

	Vector<Rc<core::ImageView>> views;
	for (auto &it : _planes) {
		auto image = it.frame->getImage();
		auto slots = it.frame->getSlotTable();
		if (slots) {
			it.view = slots->getView(it.frame->getSlot());
		}
		if (!it.view || it.view->getImage() != image) {
			it.view = device->makeImageView(image,
					image->getViewInfo(core::ImageViewInfo(image->getInfo())));
			if (slots && it.view) {
				slots->setView(it.frame->getSlot(), Rc<core::ImageView>(it.view));
			}
		}
		views.emplace_back(it.view);
	}

	Rc<core::ImageView> empty;
	if (auto emptyImage = _data->queue->emptyImage) {
		if (!emptyImage->views.empty()) {
			empty = emptyImage->views.front()->view;
		}
	}

	if (auto planes = static_cast<PlaneSetHandle *>(getAttachmentHandle(pass->getPlaneSet()))) {
		planes->setImages(sp::move(views), sp::move(empty),
				Rc<core::Sampler>(pass->acquireSampler(*device)));
	}

	return xvk::QueuePassHandle::prepare(q, [this](bool success) {
		if (auto cb = sp::move(_onLocked)) {
			cb(success);
		}
	});
}

void CompositorPassHandle::collectPlanes(const PlaneSetInput &input, const Extent2 &output) {
	_background = input.background;
	_planes.clear();

	for (auto &it : input.planes) {
		if (_planes.size() >= CompositorPass::MaxPlanes) {
			break;
		}

		auto image = it.frame ? dynamic_cast<xvk::Image *>(it.frame->getImage()) : nullptr;
		if (!image || it.alpha <= 0.0f) {
			continue;
		}

		// A frame drawn before a resize shows what it has, as on the soft pass.
		auto &extent = image->getInfo().extent;
		if (it.src.x >= extent.width || it.src.y >= extent.height) {
			continue;
		}
		int64_t width = sprt::min(it.src.width, extent.width - it.src.x);
		int64_t height = sprt::min(it.src.height, extent.height - it.src.y);

		// 1:1, clipped to the output; the source origin moves with the clip.
		auto x0 = sprt::max(int64_t(it.dst.x), int64_t(0));
		auto y0 = sprt::max(int64_t(it.dst.y), int64_t(0));
		auto x1 = sprt::min(int64_t(it.dst.x) + width, int64_t(output.width));
		auto y1 = sprt::min(int64_t(it.dst.y) + height, int64_t(output.height));
		if (x0 >= x1 || y0 >= y1) {
			continue;
		}

		Plane plane;
		plane.frame = it.frame;
		plane.dst = URect(uint32_t(x0), uint32_t(y0), uint32_t(x1 - x0), uint32_t(y1 - y0));
		plane.srcOrigin =
				UVec2(uint32_t(it.src.x + (x0 - it.dst.x)), uint32_t(it.src.y + (y0 - it.dst.y)));
		plane.alpha = it.alpha;
		plane.blend = it.blend;
		_planes.emplace_back(sp::move(plane));
	}
}

Vector<const core::CommandBuffer *> CompositorPassHandle::doPrepareCommands(
		core::FrameHandle &handle) {
	auto pass = _data->impl.cast<xvk::RenderPass>().get();
	auto &subpass = _data->subpasses.front();

	auto getPipeline = [&](StringView name) -> const core::GraphicPipelineData * {
		auto it = subpass->graphicPipelines.find(name);
		return it != subpass->graphicPipelines.end() ? *it : nullptr;
	};

	auto solid = getPipeline(CompositorPass_Solid);
	auto opaque = getPipeline(CompositorPass_Opaque);
	auto premultiplied = getPipeline(CompositorPass_Premultiplied);
	if (!solid || !opaque || !premultiplied) {
		return Vector<const core::CommandBuffer *>();
	}

	// What is below an opaque plane that covers the whole output is not drawn at all.
	size_t first = 0;
	bool covered = false;
	for (size_t i = _planes.size(); i > 0; --i) {
		auto &it = _planes[i - 1];
		if (it.blend == PlaneBlend::Opaque && it.alpha >= 1.0f && it.dst.x == 0 && it.dst.y == 0
				&& it.dst.width == _extent.width && it.dst.height == _extent.height) {
			first = i - 1;
			covered = true;
			break;
		}
	}

	for (auto &it : _planes) { autorelease(it.view); }

	auto buf = _pool->recordBuffer(*_device, Vector<Rc<xvk::DescriptorPool>>(_descriptors),
			[&, this](xvk::CommandBuffer &buf) {
		pass->perform(*this, buf, [&, this] {
			VkViewport viewport{0.0f, 0.0f, float(_extent.width), float(_extent.height), 0.0f,
				1.0f};
			buf.cmdSetViewport(0, makeSpanView(&viewport, 1));

			glsl::CompositorPlaneData data;
			sprt::memset(&data, 0, sizeof(data));
			data.extent = UVec2(_extent.width, _extent.height);

			auto push = [&] {
				buf.cmdPushConstants(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
						BytesView(reinterpret_cast<const uint8_t *>(&data), sizeof(data)));
			};

			auto scissor = [&](const URect &rect) {
				VkRect2D r{{int32_t(rect.x), int32_t(rect.y)}, {rect.width, rect.height}};
				buf.cmdSetScissor(0, makeSpanView(&r, 1));
			};

			if (!covered) {
				auto &bg = _background;
				data.dst = UVec4(0, 0, _extent.width, _extent.height);
				data.color = Vec4(bg.r * bg.a, bg.g * bg.a, bg.b * bg.a, bg.a);
				scissor(URect(0, 0, _extent.width, _extent.height));
				buf.cmdBindPipelineWithDescriptors(solid);
				push();
				buf.cmdDraw(6, 1, 0, 0);
			}

			for (size_t i = first; i < _planes.size(); ++i) {
				auto &it = _planes[i];
				auto isOpaque = it.blend == PlaneBlend::Opaque;
				data.dst = UVec4(it.dst.x, it.dst.y, it.dst.width, it.dst.height);
				data.srcOrigin = it.srcOrigin;
				data.alpha = it.alpha;
				data.opaque = isOpaque ? 1 : 0;
				data.index = uint32_t(i);
				scissor(it.dst);
				buf.cmdBindPipelineWithDescriptors(
						(isOpaque && it.alpha >= 1.0f) ? opaque : premultiplied);
				push();
				buf.cmdDraw(6, 1, 0, 0);
			}
		}, true);
		return true;
	});

	if (CompositorVkPass_isProfiling()) {
		log::source().debug("compositor::vk", "planes=", _planes.size() - first,
				" background=", !covered);
	}

	return Vector<const core::CommandBuffer *>{buf};
}

void CompositorPassHandle::doComplete(core::FrameQueue &q, Function<void(bool)> &&func,
		bool success) {
	// The device is done with the planes' images: a waiting copy may take them now.
	_locks.clear();
	xvk::QueuePassHandle::doComplete(q, sp::move(func), success);
}

bool CompositorPass::makeRenderQueue(core::Queue::Builder &builder,
		const CompositorQueueInfo &info) {
	using namespace core;

	// Every frame is composed whole: the planes change under it, not the other way around.
	builder.setDamageFlags(QueueDamageFlags::None);
	builder.setApi(InstanceApi::Vulkan);

	builder.addPass("CompositorPass", PassType::Graphics, RenderOrderingHighest,
			[&](QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		return Rc<CompositorPass>::create(builder, passBuilder, info);
	});

	return true;
}

PlaneCaps CompositorPass::getCaps() {
	PlaneCaps caps;
	caps.scaling = false;
	caps.planeAlpha = true;
	caps.premultiplied = true;
	caps.maxPlanes = MaxPlanes;
	return caps;
}

bool CompositorPass::init(core::Queue::Builder &queueBuilder, core::QueuePassBuilder &passBuilder,
		const CompositorQueueInfo &info) {
	using namespace core;

	_output =
			queueBuilder.addAttachemnt("Output", [&](AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsOutput();

		// No clear: the pass writes every pixel, the background included.
		return Rc<xvk::ImageAttachment>::create(builder,
				ImageInfo(info.extent, core::ForceImageUsage(core::ImageUsage::ColorAttachment),
						info.target->getCommonFormat()),
				core::ImageAttachment::AttachmentInfo{
					.initialLayout = AttachmentLayout::Undefined,
					.finalLayout = AttachmentLayout::PresentSrc,
					.clearOnLoad = false,
					.clearColor = info.background,
				});
	});

	_planeSet = queueBuilder.addAttachemnt(PlaneSetAttachmentName,
			[](AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsInput();
		builder.setInputValidationCallback([](const AttachmentInputData *data) {
			return dynamic_cast<const PlaneSetInput *>(data) != nullptr;
		});
		return Rc<PlaneSetVkAttachment>::create(builder);
	});

	auto colorAttachment = passBuilder.addAttachment(_output);
	auto planeSet = passBuilder.addAttachment(_planeSet);

	auto layout = passBuilder.addDescriptorLayout("CompositorLayout",
			[&](PipelineLayoutBuilder &layoutBuilder) {
		layoutBuilder.addSet([&](DescriptorSetBuilder &setBuilder) {
			setBuilder.addDescriptorArray(planeSet, MaxPlanes, DescriptorType::CombinedImageSampler,
					AttachmentLayout::ShaderReadOnlyOptimal);
		});
	});

	passBuilder.addSubpass([&](SubpassBuilder &subpassBuilder) {
		auto quadVert = queueBuilder.addProgramByRef("Compositor_QuadVert", shaders::QuadVert);
		auto planeFrag = queueBuilder.addProgramByRef("Compositor_PlaneFrag", shaders::PlaneFrag);
		auto solidFrag = queueBuilder.addProgramByRef("Compositor_SolidFrag", shaders::SolidFrag);

		// No blending, every channel written: an empty PipelineMaterialInfo would write none.
		auto replace = PipelineMaterialInfo(BlendInfo());

		subpassBuilder.addGraphicPipeline(CompositorPass_Solid, layout->defaultFamily,
				Vector<SpecializationInfo>{SpecializationInfo(quadVert),
					SpecializationInfo(solidFrag)},
				DynamicState::Default, replace);

		subpassBuilder.addGraphicPipeline(CompositorPass_Opaque, layout->defaultFamily,
				Vector<SpecializationInfo>{SpecializationInfo(quadVert),
					SpecializationInfo(planeFrag)},
				DynamicState::Default, replace);

		subpassBuilder.addGraphicPipeline(CompositorPass_Premultiplied, layout->defaultFamily,
				Vector<SpecializationInfo>{SpecializationInfo(quadVert),
					SpecializationInfo(planeFrag)},
				DynamicState::Default,
				BlendInfo(BlendFactor::One, BlendFactor::OneMinusSrcAlpha, BlendOp::Add,
						BlendFactor::One, BlendFactor::OneMinusSrcAlpha, BlendOp::Add));

		subpassBuilder.addColor(colorAttachment,
				AttachmentDependencyInfo{
					PipelineStage::ColorAttachmentOutput,
					AccessType::ColorAttachmentWrite,
					PipelineStage::ColorAttachmentOutput,
					AccessType::ColorAttachmentWrite,
					FrameRenderPassState::Submitted,
				},
				AttachmentLayout::ColorAttachmentOptimal);
	});

	return xvk::QueuePass::init(passBuilder);
}

const Rc<core::Sampler> &CompositorPass::acquireSampler(xvk::Device &device) {
	if (!_sampler) {
		_sampler = Rc<xvk::Sampler>::create(device,
				core::SamplerInfo{
					.magFilter = core::Filter::Nearest,
					.minFilter = core::Filter::Nearest,
					.addressModeU = core::SamplerAddressMode::ClampToEdge,
					.addressModeV = core::SamplerAddressMode::ClampToEdge,
					.addressModeW = core::SamplerAddressMode::ClampToEdge,
				});
	}
	return _sampler;
}

} // namespace stappler::xenolith::compositor::vk

#endif
