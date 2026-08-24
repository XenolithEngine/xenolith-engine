/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/

#include "TessRaster.h"
#include "TessBench.h"

#include "SPBitmap.h"
#include "SPFilesystem.h"

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

namespace STAPPLER_VERSIONIZED stappler::tessbench {

using namespace stappler::xenolith;

// ---- image comparison ---------------------------------------------------------------------------

RasterImage::Diff RasterImage::compare(const RasterImage &other, uint32_t tolerance) const {
	Diff d;
	if (width != other.width || height != other.height || data.size() != other.data.size()) {
		d.sizeMismatch = true;
		return d;
	}
	for (size_t i = 0; i + 3 < data.size(); i += 4) {
		uint32_t worst = 0;
		for (uint32_t k = 0; k < 4; ++k) {
			const auto a = uint32_t(data[i + k]);
			const auto b = uint32_t(other.data[i + k]);
			worst = sprt::max(worst, a > b ? a - b : b - a);
		}
		if (worst > tolerance) {
			++d.pixels;
		}
		d.maxDelta = sprt::max(d.maxDelta, worst);
	}
	return d;
}

RasterImage::Digest RasterImage::digest() const {
	Digest d;
	if (empty() || width == 0 || height == 0) {
		return d;
	}

	// Intensity from the red channel: the geometry is drawn white on black, so red IS coverage.
	for (size_t i = 0; i + 3 < data.size(); i += 4) {
		const auto v = uint32_t(data[i]);
		if (v) {
			++d.nonZero;
			d.coverage += v;
		}
	}

	// 8x8 mean-thresholded hash. Cells rather than samples so a one-pixel shift moves a bit only
	// when it moves the cell's average, which is what "the picture moved" means at this size.
	uint64_t cell[64] = {};
	uint64_t count[64] = {};
	for (uint32_t y = 0; y < height; ++y) {
		const uint32_t cy = sprt::min(uint32_t(7), y * 8 / height);
		for (uint32_t x = 0; x < width; ++x) {
			const uint32_t cx = sprt::min(uint32_t(7), x * 8 / width);
			cell[cy * 8 + cx] += data[(y * width + x) * 4];
			++count[cy * 8 + cx];
		}
	}

	uint64_t total = 0;
	for (uint32_t i = 0; i < 64; ++i) {
		cell[i] = count[i] ? cell[i] / count[i] : 0;
		total += cell[i];
	}
	const uint64_t mean = total / 64;
	for (uint32_t i = 0; i < 64; ++i) {
		if (cell[i] > mean) {
			d.signature |= (uint64_t(1) << i);
		}
	}
	return d;
}

mem_std::String RasterImage::Digest::encode() const {
	// The signature in TWO halves. It is a full 64 bits and the reader is signed - a hash with the
	// top bit set came back as something else entirely, and every icon whose hash happened to have
	// it read as a different picture. Halves keep the text plain and the round-trip exact.
	return mem_std::toString(nonZero, " ", coverage, " ", uint32_t(signature >> 32), " ",
			uint32_t(signature & 0xFFFF'FFFFu));
}

bool RasterImage::Digest::decode(StringView str, Digest &out) {
	const auto next = [&]() -> uint64_t {
		str.skipChars<StringView::WhiteSpace>();
		return uint64_t(str.readInteger(10).get(0));
	};
	out = Digest();
	out.nonZero = uint32_t(next());
	out.coverage = next();
	const auto hi = uint32_t(next());
	const auto lo = uint32_t(next());
	out.signature = (uint64_t(hi) << 32) | uint64_t(lo);
	return true;
}

uint32_t RasterImage::Digest::distance(const Digest &other) const {
	uint64_t x = signature ^ other.signature;
	uint32_t n = 0;
	while (x) {
		n += uint32_t(x & 1);
		x >>= 1;
	}
	return n;
}

bool RasterImage::writePng(StringView path) const {
	if (empty()) {
		return false;
	}
	bitmap::BitmapTemplate<mem_std::Interface> bmp(data.data(), width, height,
			bitmap::PixelFormat::RGBA8888, bitmap::AlphaFormat::Unpremultiplied);
	return bmp.save(bitmap::FileFormat::Png, FileInfo{path});
}

bool RasterImage::readPng(StringView path, RasterImage &out) {
	bitmap::BitmapTemplate<mem_std::Interface> bmp;
	if (!bmp.loadData(filesystem::readIntoMemory<mem_std::Interface>(FileInfo{path}))) {
		return false;
	}
	bmp.convert(bitmap::PixelFormat::RGBA8888);
	out.width = bmp.width();
	out.height = bmp.height();
	out.data.assign(bmp.dataPtr(), bmp.dataPtr() + bmp.data().size());
	return true;
}

// ---- the engine, brought up with no window -------------------------------------------------------

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

// Runs the looper until `ready` says so. A bench is synchronous by nature - it asks one question at
// a time and waits for the answer - and the engine is not, so this is where the two meet. Bounded,
// because a bench that hangs on a lost callback is worse than one that fails.
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
		log::source().error("tessbench", "no looper");
		return false;
	}

	auto instanceInfo = Rc<core::InstanceInfo>::alloc();
	instanceInfo->api = core::InstanceApi::Software;
	s_ctx.instance = soft::platform::createInstance(sp::move(instanceInfo));
	if (!s_ctx.instance) {
		log::source().error("tessbench", "no software instance");
		return false;
	}

	auto loopInfo = Rc<core::LoopInfo>::alloc();
	s_ctx.loop = s_ctx.instance->makeLoop(s_ctx.looper, sp::move(loopInfo));
	if (!s_ctx.loop) {
		log::source().error("tessbench", "no loop");
		return false;
	}

	// The queue: one flat pass, the extent of the target, and nothing else. `makeRenderQueue` is
	// the same call `Scene2d` makes for the software backend - the bench does not describe a
	// pipeline of its own, so what it rasterizes is what the engine rasterizes.
	auto builder = core::Queue::Builder("tessbench");
	basic2d::soft::FlatPass::RenderQueueInfo info{
		s_ctx.loop.get(),
		extent,
		// OPAQUE black, and the geometry is white: what comes out is a coverage map.
		//
		// A transparent background would be the obvious choice and is the wrong one - the flat
		// pipeline blends SrcAlpha/OneMinusSrcAlpha, so drawing onto zero alpha leaves zero alpha
		// and the whole image reads as empty however much was drawn on it. Coverage is also the
		// thing worth comparing: the antialias fringe IS a coverage ramp, and a golden that holds
		// it as a grey level compares exactly what the tesselator decides.
		Color4F(0.0f, 0.0f, 0.0f, 1.0f),
		core::QueueDamageFlags::None,
	};
	if (!basic2d::soft::FlatPass::makeRenderQueue(builder, info)) {
		log::source().error("tessbench", "failed to build the flat queue");
		return false;
	}

	s_ctx.queue = Rc<core::Queue>::create(sp::move(builder));
	if (!s_ctx.queue) {
		log::source().error("tessbench", "failed to create the queue");
		return false;
	}

	// The loop has to be RUNNING before anything is submitted to it. Without an application
	// nobody does this for us, and a frame handed to a stopped loop simply never starts - which
	// looks exactly like a frame that hangs.
	s_ctx.loop->run();

	// Compiling the queue is asynchronous even here, and there is no application to run the
	// looper for us: this is a CLI thread, so it pumps its own.
	bool compiled = false, done = false;
	s_ctx.loop->compileQueue(s_ctx.queue, [&](bool ok) {
		compiled = ok;
		done = true;
	});
	if (!pump([&] { return done; })) {
		log::source().error("tessbench", "queue compilation did not finish");
		return false;
	}
	if (!compiled) {
		log::source().error("tessbench", "queue compilation failed");
		return false;
	}

	s_ctx.vertexes = s_ctx.queue->getAttachment(basic2d::FrameContext2d::VertexAttachmentName);
	if (!s_ctx.vertexes) {
		log::source().error("tessbench", "no vertex attachment in the queue");
		return false;
	}
	s_ctx.output = s_ctx.queue->getPresentImageOutput();
	if (!s_ctx.output) {
		s_ctx.output = s_ctx.queue->getTransferImageOutput();
	}
	/* The material.
	
	The flat queue declares four fallback materials of its own (`FlatPass::makeMaterialSubpass`),
	so the bench does not have to build a material set - it only has to name one. The transparent
	one, because the antialias fringe is a per-vertex alpha and a solid pipeline would throw it
	away, which would rasterize the fill and call it the icon.
	
	Their real ids are assigned when the queue compiles, so this can only be asked now. */
	if (auto matAttachment =
					s_ctx.queue->getAttachment(basic2d::FrameContext2d::MaterialAttachmentName)) {
		auto ma = static_cast<core::MaterialAttachment *>(matAttachment->attachment.get());
		// TWO of the four are transparent, and they differ only in their texture: one samples the
		// queue's empty image and one its solid image. A mesh with no texture coordinates must
		// have the SOLID one - sampling the empty image returns zero alpha, and the frame then
		// completes perfectly while drawing nothing, which is exactly what it did.
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
		log::source().error("tessbench", "no material in the compiled queue");
		return false;
	}
	if (!s_ctx.output) {
		log::source().error("tessbench", "no image output in the queue");
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

RasterImage rasterIcon(StringView name, bool antialias) {
	RasterImage out;
	if (!s_ctx.ready) {
		return out;
	}

	// The geometry: the same call the golden uses, so a raster and a digest can never describe
	// different tesselations of the same icon.
	RawMesh raw;
	auto res = tessellateIcon(name, antialias, &raw);
	if (res.digest.failed || raw.indexes.empty()) {
		return out;
	}

	auto mesh = Rc<basic2d::VertexData>::alloc();
	mesh->data.resize(raw.vertexes.size());
	for (size_t i = 0; i < raw.vertexes.size(); ++i) {
		// White with the tesselator's own per-vertex intensity in alpha - the antialias fringe IS
		// that alpha, and dropping it would rasterize the fill and call it the icon.
		mesh->data[i] = basic2d::Vertex{Vec4(raw.vertexes[i], 0.0f, 1.0f),
			Vec4(1.0f, 1.0f, 1.0f, raw.values[i]), Vec2(0.0f, 0.0f), 0, 0};
	}
	mesh->indexes.assign(raw.indexes.begin(), raw.indexes.end());

	core::FrameConstraints constraints;
	constraints.extent = s_ctx.extent;
	constraints.density = 1.0f;

	auto req = Rc<core::FrameRequest>::create(s_ctx.queue, constraints);
	if (!req) {
		return out;
	}

	auto handle = Rc<basic2d::FrameContextHandle2d>::alloc();
	handle->commands = Rc<basic2d::CommandList>::create(req->getPool());

	/* The transform, and it has to do the whole job.
	
	`FrameConstraints::transform` is what a window would put here, and there is no window - so it
	stays identity and the shader multiplies the vertex by this matrix and nothing else. That means
	this matrix is the projection: the icon is authored in a 24-unit box with y down, and what the
	rasterizer wants is clip space, -1..1 with y up. So: scale into [0,2], flip y, shift to [-1,1]. */
	Mat4 view;
	view.translate(-1.0f, 1.0f, 0.0f);
	view.scale(2.0f / 24.0f, -2.0f / 24.0f, 1.0f);

	basic2d::CmdInfo info;
	info.material = s_ctx.material;
	info.renderingLevel = RenderingLevel::Transparent;
	info.depthValue = 0.0f;

	handle->commands->pushVertexArray(sp::move(mesh), view, sp::move(info));

	req->addInput(s_ctx.vertexes, Rc<core::AttachmentInputData>(handle));

	/* The queue declares attachments for lights and particle emitters even though the flat pass
	renders neither - `FrameContext2d::initWithQueue` requires them to exist by name, and the soft
	backend serves them with an attachment that accepts input and drops it. They still have to be
	FED: a frame waits for input on every attachment that declares it, and one left unfed does not
	fail, it waits - which is indistinguishable from a hang. */
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
				auto view = softImage->getView();
				out.width = s_ctx.extent.width;
				out.height = s_ctx.extent.height;
				out.data.assign(view.data(), view.data() + view.size());
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
		log::source().error("tessbench", "raster failed: frameRan=", frameRan, " frameOk=", frameOk,
				" outputCalled=", outputCalled, " done=", done, " material=", s_ctx.material,
				" verts=", raw.vertexes.size(), " idx=", raw.indexes.size());
		out.data.clear();
	}
	return out;
}

#else

bool rasterInit(Extent2) {
	log::source().error("tessbench", "built without the software backend");
	return false;
}
void rasterFinalize() { }
RasterImage rasterIcon(StringView, bool) { return RasterImage(); }

#endif

} // namespace stappler::tessbench
