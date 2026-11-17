/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/

#include "SpriteRaster.h"

#include "XL2d.h"
#include "XL2dCommandList.h"
#include "XL2dFrameContext.h"
#include "XLCoreFrameRequest.h"
#include "XLCoreImageStorage.h"
#include "XLCoreLoop.h"
#include "XLCoreQueue.h"

#if MODULE_XENOLITH_RENDERER_BASIC2D_SOFT
#include "XL2dSoftFlatPass.h"
#include "XLSoftObject.h"
#include "XLSoftPlatform.h"
#endif

#include <sprt/runtime/dispatch/looper.h>

namespace STAPPLER_VERSIONIZED stappler::spritebench {

using namespace stappler::xenolith;

#if MODULE_XENOLITH_RENDERER_BASIC2D_SOFT

namespace {

struct RasterContext {
	sprt::dispatch::Looper *looper = nullptr;
	Rc<core::Instance> instance;
	Rc<core::Loop> loop;
	Rc<core::Queue> queue;
	const core::AttachmentData *vertexes = nullptr;
	const core::AttachmentData *output = nullptr;
	core::MaterialId material = 0;
	Extent2 extent;
	bool ready = false;
};

RasterContext s_ctx;

// Runs the looper until `ready` says so, bounded: a check that hangs on a lost callback is worse
// than one that fails.
bool pump(const Callback<bool()> &ready, uint32_t millis = 20'000) {
	for (uint32_t i = 0; i < millis * 10; ++i) {
		if (ready()) {
			return true;
		}
		if (s_ctx.looper->poll() == 0) {
			sprt::platform::sleep(100);
		}
	}
	return ready();
}

} // namespace

bool rasterInit(Extent2 extent) {
	if (s_ctx.ready && s_ctx.extent == extent) {
		return true;
	}
	rasterFinalize();

	s_ctx.extent = extent;
	s_ctx.looper = sprt::dispatch::Looper::acquire();
	if (!s_ctx.looper) {
		log::source().error("spritebench", "no looper");
		return false;
	}

	auto instanceInfo = Rc<core::InstanceInfo>::alloc();
	instanceInfo->api = core::InstanceApi::Software;
	s_ctx.instance = soft::platform::createInstance(sp::move(instanceInfo));
	if (!s_ctx.instance) {
		log::source().error("spritebench", "no software instance");
		return false;
	}

	auto loopInfo = Rc<core::LoopInfo>::alloc();
	s_ctx.loop = s_ctx.instance->makeLoop(s_ctx.looper, sp::move(loopInfo));
	if (!s_ctx.loop) {
		log::source().error("spritebench", "no loop");
		return false;
	}

	// The same queue a Scene2d makes for the software backend - the bench describes no pipeline of
	// its own, so what it rasterizes is what the engine rasterizes. Opaque black behind white
	// geometry: the readback is a coverage map (see the tess bench for why not a transparent
	// background - SrcAlpha blending onto zero alpha reads as empty however much was drawn).
	auto builder = core::Queue::Builder("spritebench");
	basic2d::soft::FlatPass::RenderQueueInfo info{
		s_ctx.loop.get(),
		extent,
		Color4F(0.0f, 0.0f, 0.0f, 1.0f),
		core::QueueDamageFlags::None,
	};
	if (!basic2d::soft::FlatPass::makeRenderQueue(builder, info)) {
		log::source().error("spritebench", "failed to build the flat queue");
		return false;
	}

	s_ctx.queue = Rc<core::Queue>::create(sp::move(builder));
	if (!s_ctx.queue) {
		log::source().error("spritebench", "failed to create the queue");
		return false;
	}

	// The loop must run before anything is submitted; without an application nobody does this, and
	// a frame handed to a stopped loop never starts - which looks exactly like a hang.
	s_ctx.loop->run();

	bool compiled = false, done = false;
	s_ctx.loop->compileQueue(s_ctx.queue, [&](bool ok) {
		compiled = ok;
		done = true;
	});
	if (!pump([&] { return done; })) {
		log::source().error("spritebench", "queue compilation did not finish");
		return false;
	}
	if (!compiled) {
		log::source().error("spritebench", "queue compilation failed");
		return false;
	}

	s_ctx.vertexes = s_ctx.queue->getAttachment(basic2d::FrameContext2d::VertexAttachmentName);
	if (!s_ctx.vertexes) {
		log::source().error("spritebench", "no vertex attachment in the queue");
		return false;
	}
	s_ctx.output = s_ctx.queue->getPresentImageOutput();
	if (!s_ctx.output) {
		s_ctx.output = s_ctx.queue->getTransferImageOutput();
	}
	if (!s_ctx.output) {
		log::source().error("spritebench", "no image output in the queue");
		return false;
	}

	/* The material: the flat queue declares fallback materials of its own, and the bench only has
	to name one. The transparent SOLID one - a mesh whose texture coordinates nobody looks at must
	not sample the empty image, which returns zero alpha and draws nothing while the frame reports
	success. */
	if (auto matAttachment =
				s_ctx.queue->getAttachment(basic2d::FrameContext2d::MaterialAttachmentName)) {
		auto ma = static_cast<core::MaterialAttachment *>(matAttachment->attachment.get());
		const core::ImageData *solidImage = nullptr;
		if (auto resource = s_ctx.queue->getInternalResource()) {
			solidImage = resource->getImage(core::SolidTextureName);
		}
		if (auto set = ma->getMaterials()) {
			for (auto &it : set->getMaterials()) {
				auto m = it.second.get();
				if (!m || !m->getPipeline() || m->getPipeline()->isSolid()) {
					continue;
				}
				if (solidImage && !m->getImages().empty()
						&& m->getImages().front().image == solidImage) {
					s_ctx.material = it.first;
					break;
				}
			}
		}
	}
	if (!s_ctx.material) {
		log::source().error("spritebench", "no material in the compiled queue");
		return false;
	}

	s_ctx.ready = true;
	return true;
}

void rasterFinalize() {
	if (s_ctx.loop && s_ctx.loop->isRunning()) {
		s_ctx.loop->stop();
	}
	s_ctx.queue = nullptr;
	s_ctx.loop = nullptr;
	s_ctx.instance = nullptr;
	s_ctx.looper = nullptr;
	s_ctx.vertexes = nullptr;
	s_ctx.ready = false;
}

RasterImage rasterDraw(const Callback<void(const Mat4 &, core::MaterialId,
		basic2d::CommandList &)> &build) {
	RasterImage out;
	if (!s_ctx.ready) {
		return out;
	}

	core::FrameConstraints constraints;
	constraints.extent = s_ctx.extent;
	constraints.density = 1.0f;

	auto req = Rc<core::FrameRequest>::create(s_ctx.queue, constraints);
	if (!req) {
		return out;
	}

	auto handle = Rc<basic2d::FrameContextHandle2d>::alloc();
	handle->commands = Rc<basic2d::CommandList>::create(req->getPool());

	/* Vertexes are placed in pixels of the target, so this matrix is the whole projection: into
	[0,2], y flipped (vertex space is y up, the raster reads top-left down), shifted to [-1,1]. */
	Mat4 view;
	view.translate(-1.0f, 1.0f, 0.0f);
	view.scale(2.0f / float(s_ctx.extent.width), -2.0f / float(s_ctx.extent.height), 1.0f);

	// The check's own draws, each through the mesh-to-frame door a sprite command takes.
	build(view, s_ctx.material, *handle->commands);

	req->addInput(s_ctx.vertexes, Rc<core::AttachmentInputData>(handle));

	// Lights and emitters are declared by the queue and served with sink attachments; they still
	// have to be fed, or the frame waits on them - which is indistinguishable from a hang.
	for (auto name : {basic2d::FrameContext2d::LightDataAttachmentName,
				basic2d::FrameContext2d::ParticleEmittersAttachment}) {
		if (auto a = s_ctx.queue->getAttachment(name)) {
			req->addInput(a, Rc<core::AttachmentInputData>(handle));
		}
	}

	bool ok = false, done = false, outputCalled = false, frameRan = false, frameOk = false;
	req->setOutput(s_ctx.output, [&](core::FrameAttachmentData &data, bool success, Ref *) -> bool {
		outputCalled = true;
		if (success && data.image) {
			if (auto img = data.image->getImage()) {
				auto softImage = static_cast<soft::Image *>(img.get());
				auto v = softImage->getView();
				out.width = s_ctx.extent.width;
				out.height = s_ctx.extent.height;
				out.data.assign(v.data(), v.data() + v.size());
				ok = true;
			}
		}
		done = true;
		return true;
	});

	s_ctx.loop->runRenderQueue(sp::move(req), 0, [&](bool success) {
		frameRan = true;
		frameOk = success;
		if (!success) {
			done = true;
		}
	});

	pump([&] { return done; });
	if (!ok) {
		log::source().error("spritebench", "raster failed: frameRan=", frameRan, " frameOk=",
				frameOk, " outputCalled=", outputCalled, " done=", done, " material=",
				s_ctx.material);
		out.data.clear();
	}
	return out;
}

#else

bool rasterInit(Extent2) {
	log::source().error("spritebench", "built without the software backend");
	return false;
}
void rasterFinalize() { }
RasterImage rasterDraw(const Callback<void(const Mat4 &, core::MaterialId, basic2d::CommandList &)>) {
	return RasterImage();
}

#endif

} // namespace stappler::spritebench
