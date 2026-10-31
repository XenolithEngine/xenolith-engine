/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#include "XLCommon.h"
#include "XLEntryPoint.h"
#include "XLAppThread.h"
#include "XL2dScene.h"
#include "XL2dSceneContent.h"
#include "XL2dLayer.h"
#include "XL2dLabel.h"
#include "XLCoreFrameHandle.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreLoop.h"
#include "XLCoreQueueData.h"
#include "XLVkAttachment.h"
#include "XLVkDeviceQueue.h"
#include "XLVkQueuePass.h"
#include "XLVkRenderPass.h"
#include "SPPlatform.h"

#include <sprt/c/__sprt_stdlib.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::pass3d {

namespace {

#include "pass3d_scene.vert.h"
#include "pass3d_scene.frag.h"

SpanView<uint32_t> SceneVert(reinterpret_cast<const uint32_t *>(pass3d_scene_vert),
		sizeof(pass3d_scene_vert) / sizeof(uint32_t));
SpanView<uint32_t> SceneFrag(reinterpret_cast<const uint32_t *>(pass3d_scene_frag),
		sizeof(pass3d_scene_frag) / sizeof(uint32_t));

constexpr StringView ScenePipelineName = "Pass3dScene";

// PASS3D_STILL=1 stops the spinning cube, so the check samples a fixed picture
bool s_still = false;

struct SceneConstants {
	float viewProj[16];
	float params[4];
};

// Orthographic camera at yaw 45 deg and elevation 30 deg (a 2:1 rhombus per ground cell), looking
// at the origin. Depth goes to [0, 1] as Vulkan expects; the runtime's createOrthographic gives the
// GL range. Column-major, as GLSL reads a mat4.
void makeViewProj(float *m, float width, float height, float pixelsPerUnit) {
	// to-camera direction (cos30 cos45, cos30 sin45, sin30), forward is its negation
	const float fx = -0.61237244f, fy = -0.61237244f, fz = -0.5f;
	// right = normalize(cross(forward, up)), up' = cross(right, forward)
	const float rx = -0.70710678f, ry = 0.70710678f, rz = 0.0f;
	const float ux = -0.35355339f, uy = -0.35355339f, uz = 0.8660254f;

	const float halfW = width / (2.0f * pixelsPerUnit);
	const float halfH = height / (2.0f * pixelsPerUnit);
	const float dist = 50.0f, zNear = 0.0f, zFar = 100.0f;
	const float depth = zFar - zNear;

	float rows[4][4] = {
		{rx / halfW, ry / halfW, rz / halfW, 0.0f},
		{-ux / halfH, -uy / halfH, -uz / halfH, 0.0f}, // Vulkan clip y points down
		{fx / depth, fy / depth, fz / depth, (dist - zNear) / depth},
		{0.0f, 0.0f, 0.0f, 1.0f},
	};

	for (uint32_t col = 0; col < 4; ++col) {
		for (uint32_t row = 0; row < 4; ++row) { m[col * 4 + row] = rows[row][col]; }
	}
}

core::ImageFormat selectDepthFormat(SpanView<core::ImageFormat> formats) {
	for (auto &it : formats) {
		if (it == core::ImageFormat::D32_SFLOAT) {
			return it;
		}
	}
	return formats.empty() ? core::ImageFormat::Undefined : formats.front();
}

void addScenePass(core::Loop *loop, core::Queue::Builder &builder, Extent2 extent,
		float density) {
	using namespace core;

	auto output = builder.getAttachment("Output");
	if (!output) {
		log::source().error("pass3d", "basic2d queue has no Output attachment");
		return;
	}

	auto depth = builder.addAttachemnt("Depth3d", [&](AttachmentBuilder &b) -> Rc<Attachment> {
		return Rc<vk::ImageAttachment>::create(b,
				ImageInfo(extent, ForceImageUsage(ImageUsage::DepthStencilAttachment),
						selectDepthFormat(loop->getSupportedDepthStencilFormat())),
				ImageAttachment::AttachmentInfo{.initialLayout = AttachmentLayout::Undefined,
					.finalLayout = AttachmentLayout::DepthStencilAttachmentOptimal,
					.clearOnLoad = true,
					.clearColor = Color4F::WHITE});
	});

	auto vert = builder.addProgramByRef("Pass3dSceneVert", SceneVert);
	auto frag = builder.addProgramByRef("Pass3dSceneFrag", SceneFrag);

	builder.addPass("Pass3d", PassType::Graphics, RenderOrdering(10),
			[&](QueuePassBuilder &passBuilder) -> Rc<QueuePass> {
		auto colorPass = passBuilder.addAttachment(output);
		auto depthPass = passBuilder.addAttachment(depth);

		auto layout = passBuilder.addDescriptorLayout("Pass3dLayout",
				[](PipelineLayoutBuilder &) { });

		passBuilder.addSubpass([&](SubpassBuilder &subpassBuilder) {
			subpassBuilder.addColor(colorPass,
					AttachmentDependencyInfo{
						PipelineStage::ColorAttachmentOutput,
						AccessType::ColorAttachmentWrite,
						PipelineStage::ColorAttachmentOutput,
						AccessType::ColorAttachmentWrite,
						FrameRenderPassState::Submitted,
					});

			subpassBuilder.setDepthStencil(depthPass,
					AttachmentDependencyInfo{
						PipelineStage::EarlyFragmentTest,
						AccessType::DepthStencilAttachmentRead
								| AccessType::DepthStencilAttachmentWrite,
						PipelineStage::LateFragmentTest,
						AccessType::DepthStencilAttachmentRead
								| AccessType::DepthStencilAttachmentWrite,
						FrameRenderPassState::Submitted,
					});

			subpassBuilder.addGraphicPipeline(ScenePipelineName, layout->defaultFamily,
					Vector<SpecializationInfo>({
						SpecializationInfo(vert),
						SpecializationInfo(frag),
					}),
					PipelineMaterialInfo({BlendInfo(), DepthInfo(true, true, CompareOp::Less)}));

			subpassBuilder.setCommandsCallback([density](FrameQueue &queue,
													   const SubpassData &subpass,
													   CommandBuffer &commands) {
				auto &buf = static_cast<vk::CommandBuffer &>(commands);
				auto pipeline = subpass.graphicPipelines.get(ScenePipelineName);
				if (!pipeline) {
					return;
				}

				auto extent = queue.getFrame()->getFrameConstraints().extent;

				VkViewport viewport{0.0f, 0.0f, float(extent.width), float(extent.height), 0.0f,
					1.0f};
				buf.cmdSetViewport(0, makeSpanView(&viewport, 1));

				VkRect2D scissor{{0, 0}, {extent.width, extent.height}};
				buf.cmdSetScissor(0, makeSpanView(&scissor, 1));

				// 64 points per unit: the scene keeps its size on a dense screen
				SceneConstants pcb;
				makeViewProj(pcb.viewProj, float(extent.width), float(extent.height),
						64.0f * density);
				pcb.params[0] = s_still
						? 0.0f
						: float(sp::platform::clock(ClockType::Monotonic) % 1'000'000'000)
								/ 1'000'000.0f;
				pcb.params[1] = pcb.params[2] = pcb.params[3] = 0.0f;

				buf.cmdBindPipelineWithDescriptors(pipeline);
				buf.cmdPushConstants(VK_SHADER_STAGE_VERTEX_BIT, 0,
						BytesView(reinterpret_cast<const uint8_t *>(&pcb), sizeof(SceneConstants)));
				buf.cmdDraw(36, 3, 0, 0);
			});
		});

		return Rc<vk::QueuePass>::create(passBuilder);
	});
}

// A semi-transparent band that keeps sliding over the 3d content; its motion keeps the window
// producing frames when nothing asks for them.
class SlidingBand : public basic2d::Layer {
public:
	virtual ~SlidingBand() = default;

	virtual bool init() override {
		if (!Layer::init(Color4F(1.0f, 1.0f, 1.0f, 0.6f))) {
			return false;
		}
		scheduleUpdate();
		return true;
	}

	virtual void update(const UpdateTime &time) override {
		Layer::update(time);
		if (auto parent = getParent()) {
			auto size = parent->getContentSize();
			float t = float(time.app % 4'000'000) / 4'000'000.0f;
			float k = t < 0.5f ? t * 2.0f : 2.0f - t * 2.0f;
			setPosition(Vec2((size.width - getContentSize().width) * k, size.height * 0.15f));
		}
	}
};

} // namespace

DEFINE_CONFIG_FUNCTION((ContextConfig &cfg) {
	if (!cfg.window) {
		cfg.window = Rc<sprt::window::WindowInfo>::alloc();
	}
	cfg.window->flags = sprt::window::WindowCreationFlags::Regular;
	cfg.window->minExtent = Extent2(640, 480);
});

class Pass3dScene : public basic2d::Scene2d {
public:
	virtual ~Pass3dScene() = default;

	virtual bool init(NotNull<AppThread> app, NotNull<core::RenderServerChannel> window,
			const core::FrameConstraints &constraints) override {
		s_still = __sprt_getenv("PASS3D_STILL") != nullptr;

		auto extent = Extent2(constraints.extent.width, constraints.extent.height);
		auto density = constraints.density;

		// PASS3D_NO3D=1 leaves the stock queue alone: the baseline to compare a run with
		if (!basic2d::Scene2d::init(app, window, [&](core::Queue::Builder &builder) {
			if (!__sprt_getenv("PASS3D_NO3D")) {
				addScenePass(app->getGlLoop(), builder, extent, density);
			}
		}, constraints)) {
			return false;
		}

		auto content = Rc<basic2d::SceneContent2d>::create();
		content->setDefaultLights();

		if (!s_still) {
			auto band = content->addChild(Rc<SlidingBand>::create());
			band->setContentSize(Size2(220.0f, 120.0f));
		}

		// Static, over the ground below the boxes: the check reads the ground through it, which it
		// can only do when the 2d pass loads what the 3d pass wrote
		auto veil = content->addChild(Rc<basic2d::Layer>::create(Color4F(1.0f, 1.0f, 1.0f, 0.5f)));
		veil->setContentSize(Size2(120.0f, 60.0f));
		veil->setPosition(Vec2(420.0f, 100.0f));

		auto label = content->addChild(Rc<basic2d::Label>::create(
				"pass3d: depth-tested 3d pass under basic2d UI"));
		label->setFontSize(20);
		label->setPosition(Vec2(24.0f, 24.0f));

		// static, far from everything that moves: outside any damage rectangle
		auto marker = content->addChild(Rc<basic2d::Label>::create("STATIC UI MARKER"));
		marker->setFontSize(28);
		marker->setAnchorPoint(Anchor::TopRight);
		marker->setPosition(Vec2(constraints.extent.width / density - 24.0f,
				constraints.extent.height / density - 24.0f));

		setContent(content);
		return true;
	}

protected:
	// PASS3D_DAMAGE=1 turns on partial redraw and frame skipping, which the Default queue leaves
	// off. Both follow the 2d content only: the 3d pass repaints the whole image, the 2d pass only
	// its damage, so the UI outside it is lost on screen.
	virtual void describeQueue(QueueInfo &info) override {
		basic2d::Scene2d::describeQueue(info);
		info.backgroundColor = Color4F(0.12f, 0.12f, 0.14f, 1.0f);
		if (__sprt_getenv("PASS3D_DAMAGE")) {
			info.damage = core::QueueDamageFlags::PresentHint
					| core::QueueDamageFlags::PartialRedraw | core::QueueDamageFlags::SkipEmptyFrames;
		} else {
			info.damage = core::QueueDamageFlags::None;
		}
	}
};

DEFINE_PRIMARY_SCENE_CLASS(Pass3dScene)

} // namespace stappler::xenolith::pass3d
