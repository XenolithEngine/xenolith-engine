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

#include "XLCompositorSoftPass.h"

#if MODULE_XENOLITH_RENDERER_COMPOSITOR_SOFT

#include "XLSoftObject.h"
#include "XLSoftDevice.h"
#include "XLCoreFrameQueue.h"

#include <sprt/runtime/dispatch/looper.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::compositor::soft {

bool CompositorPass::makeRenderQueue(core::Queue::Builder &builder,
		const CompositorQueueInfo &info) {
	using namespace core;

	// Every frame is composed whole: the planes change under it, not the other way around.
	builder.setDamageFlags(QueueDamageFlags::None);
	builder.setApi(InstanceApi::Software);

	builder.addPass("CompositorPass", PassType::Graphics, RenderOrderingHighest,
			[&](QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		return Rc<CompositorPass>::create(builder, passBuilder, info);
	});

	return true;
}

bool CompositorPass::init(core::Queue::Builder &queueBuilder, core::QueuePassBuilder &passBuilder,
		const CompositorQueueInfo &info) {
	using namespace core;

	_output =
			queueBuilder.addAttachemnt("Output", [&](AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsOutput();

		// No clear: the pass writes every pixel, the background included.
		return Rc<sf::ImageAttachment>::create(builder,
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
		return Rc<PlaneSetAttachment>::create(builder);
	});

	auto colorAttachment = passBuilder.addAttachment(_output);
	passBuilder.addAttachment(_planeSet);

	passBuilder.addSubpass([&](SubpassBuilder &subpassBuilder) {
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

	return core::QueuePass::init(passBuilder);
}

bool CompositorPassHandle::prepare(core::FrameQueue &q, Function<void(bool)> &&cb) {
	if (auto planes = static_cast<CompositorPass *>(_queuePass.get())->getPlaneSet()) {
		_planeSet = getAttachmentHandle(planes);
	}
	return sf::QueuePassHandle::prepare(q, sp::move(cb));
}

// XL_WM_PROFILE=1: what each composed frame cost, and how the pool took it.
static bool CompositorPass_isProfiling() {
	static const bool s_profile = [] {
		auto value = ::getenv("XL_WM_PROFILE");
		return value && StringView(value) != "0";
	}();
	return s_profile;
}

void CompositorPassHandle::submit(core::FrameQueue &q, Rc<core::FrameSync> &&,
		Function<void(bool)> &&onSubmited, Function<void(bool)> &&onComplete) {
	raster::Target target;
	if (_data->subpasses.empty() || !resolveOutputTarget(*_data->subpasses.front(), target)) {
		finishSubmit(q, false, sp::move(onSubmited), sp::move(onComplete));
		return;
	}

	markShadowComposed();

	Rc<PlaneSetInput> input;
	if (_planeSet) {
		input = static_cast<PlaneSetInput *>(_planeSet->getInput());
	}

	Color4F background = Color4F::BLACK;
	Vector<BlendSource> sources;
	if (input) {
		background = input->background;
		for (auto &it : input->planes) {
			auto image = dynamic_cast<sf::Image *>(it.frame ? it.frame->getImage() : nullptr);
			if (!image) {
				continue;
			}

			// A frame drawn before a resize shows what it has.
			auto &extent = image->getInfo().extent;
			if (it.src.x >= extent.width || it.src.y >= extent.height) {
				continue;
			}

			BlendSource source;
			source.pixels = image->getData();
			source.stride = image->getStride();
			source.format = sf::getRasterFormat(image->getInfo().format);
			source.src = URect(it.src.x, it.src.y, sprt::min(it.src.width, extent.width - it.src.x),
					sprt::min(it.src.height, extent.height - it.src.y));
			source.origin = IVec2(it.dst.x, it.dst.y);
			source.alpha = uint8_t(sprt::clamp(int32_t(it.alpha * 255.0f + 0.5f), 0, 255));
			source.blend = it.blend;
			sources.emplace_back(source);
		}
	}

	// Bands of whole rows, a few per worker: blending costs about the same everywhere, but a band
	// under an opaque plane is a row copy and one under several planes is not.
	auto looper = sprt::dispatch::Looper::getIfExists();
	auto workers = looper ? sprt::max(uint32_t(looper->getWorkersCount()), 1U) : 1U;

	raster::TiledDrawRequest req;
	req.target = target;
	req.regions.emplace_back(URect(0, 0, target.width, target.height));
	req.tiling.width = 0;
	req.tiling.height = sprt::max(8U, (target.height + workers * 4 - 1) / (workers * 4));
	req.tiling.threads = 0;
	req.tileCallback = [input, sources = sp::move(sources),
							   background](const raster::Target &target, const URect &tile) {
		blendPlanes(target, sources, background, tile);
	};

	auto planeCount = input ? input->planes.size() : 0;
	auto started = sp::platform::clock(ClockType::Monotonic);
	auto job = raster::drawTiledAsync(sp::move(req),
			[this, queue = Rc<core::FrameQueue>(&q), onSubmited = sp::move(onSubmited),
					onComplete = sp::move(onComplete), started,
					planeCount](bool success, uint32_t, const raster::TilingStats &stats) mutable {
		if (CompositorPass_isProfiling()) {
			log::source().debug("compositor::soft", "planes=", planeCount, " tiles=", stats.tiles,
					" workers=", stats.workers,
					" us=", sp::platform::clock(ClockType::Monotonic) - started);
		}
		finishSubmit(*queue, success, sp::move(onSubmited), sp::move(onComplete));
	},
			this);

	if (job) {
		_device->addRasterJob(sp::move(job));
	}
}

} // namespace stappler::xenolith::compositor::soft

#endif
