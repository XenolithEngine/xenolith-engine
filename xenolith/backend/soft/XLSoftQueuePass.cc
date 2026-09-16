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

#include "XLSoftQueuePass.h"
#include "XLSoftObject.h"
#include "XLSoftLoop.h"

#include "XLCoreFrameQueue.h"
#include "XLCoreFrameHandle.h"
#include "XLCoreFrameRequest.h"
#include "XLCoreSwapchain.h"

#include <sprt/cxx/atomic>

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft {

bool RenderPass::init(Device &dev, const core::QueuePassData &data) {
	return core::Object::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle, void *) { },
			core::ObjectType::RenderPass, core::ObjectHandle::zero());
}

bool CommandBuffer::init(Device &dev) {
	_device = &dev;
	return true;
}

void CommandBuffer::setScissor(const URect &rect) {
	// Clamp once, here, so no kernel has to defend against a scissor that leaves the target.
	auto left = sprt::min(rect.x, _target.width);
	auto top = sprt::min(rect.y, _target.height);
	auto right = sprt::min(uint32_t(rect.x + rect.width), _target.width);
	auto bottom = sprt::min(uint32_t(rect.y + rect.height), _target.height);

	if (left >= right || top >= bottom) {
		_scissor = URect{0, 0, 0, 0};
		return;
	}

	_scissor = URect{left, top, right - left, bottom - top};
}

void QueuePassHandle::recordSubpass(core::FrameQueue &q, const core::SubpassData &subpass,
		CommandBuffer &buf) {
	if (subpass.commandsCallback) {
		subpass.commandsCallback(q, subpass, buf);
	}
}

URect QueuePassHandle::rotateScissor(const core::FrameConstraints &constraints,
		const URect &scissor) {
	// Y flip first: scene space grows upwards, the target downwards.
	int32_t x = int32_t(scissor.x);
	int32_t y = int32_t(constraints.extent.height - scissor.y - scissor.height);
	uint32_t width = scissor.width;
	uint32_t height = scissor.height;

	switch (core::getPureTransform(constraints.transform)) {
	case core::SurfaceTransformFlags::Rotate90:
		y = int32_t(scissor.x);
		x = int32_t(scissor.y);
		sprt::swap(width, height);
		break;
	case core::SurfaceTransformFlags::Rotate180: y = int32_t(scissor.y); break;
	case core::SurfaceTransformFlags::Rotate270:
		y = int32_t(constraints.extent.height - scissor.x - scissor.width);
		x = int32_t(constraints.extent.width - scissor.y - scissor.height);
		sprt::swap(width, height);
		break;
	default: break;
	}

	if (x < 0) {
		width = (uint32_t(-x) < width) ? width - uint32_t(-x) : 0;
		x = 0;
	}

	if (y < 0) {
		height = (uint32_t(-y) < height) ? height - uint32_t(-y) : 0;
		y = 0;
	}

	return URect{uint32_t(x), uint32_t(y), width, height};
}

// The bounding rectangle of the redraw regions. Used as the recording scissor and reported in the
// damage log for comparison with the separate regions.
static URect QueuePassHandle_boundingRect(SpanView<URect> areas) {
	if (areas.empty()) {
		return URect{0, 0, 0, 0};
	}
	auto x0 = areas.front().x, y0 = areas.front().y;
	auto x1 = x0 + areas.front().width, y1 = y0 + areas.front().height;
	for (auto &it : areas) {
		x0 = sprt::min(x0, it.x);
		y0 = sprt::min(y0, it.y);
		x1 = sprt::max(x1, it.x + it.width);
		y1 = sprt::max(y1, it.y + it.height);
	}
	return URect{x0, y0, x1 - x0, y1 - y0};
}

bool QueuePassHandle::computeRedrawArea(core::FrameQueue &q, const raster::Target &target,
		Vector<URect> &areas) {
	areas.clear();
	areas.emplace_back(URect{0, 0, target.width, target.height});

	// XL_SOFT_DAMAGE_LOG=1 reports what each frame decided. Without it there is no way to tell a
	// working partial redraw from a silently disabled one - the picture is identical either way.
	static const bool damageLog = [] {
		auto value = ::getenv("XL_SOFT_DAMAGE_LOG");
		return value && StringView(value) != "0";
	}();

	// XL_SOFT_FORCE_FULL_REDRAW=1 repaints the whole surface every frame, for benchmarks (a static
	// scene otherwise skips its frames). Disables damage tracking; not for production.
	static const bool forceFull = [] {
		auto value = ::getenv("XL_SOFT_FORCE_FULL_REDRAW");
		return value && StringView(value) != "0";
	}();

	if (forceFull) {
		return true;
	}

	if (!hasFlag(_data->queue->damage, core::QueueDamageFlags::PartialRedraw)) {
		if (damageLog) {
			log::source().debug("soft::QueuePassHandle",
					"damage: full repaint, the queue did not ask for partial redraw");
		}
		return true;
	}

	// Only the presented image carries a per-index snapshot of what it holds.
	core::ImageStorage *image = nullptr;
	for (auto &it : _data->attachments) {
		if (it->finalLayout != core::AttachmentLayout::PresentSrc) {
			continue;
		}
		if (auto aData = q.getAttachment(it->attachment)) {
			if (auto img = aData->image.get()) {
				if (img->isSwapchainImage()) {
					image = img;
				}
			}
		}
		break;
	}

	if (!image) {
		if (damageLog) {
			log::source().debug("soft::QueuePassHandle",
					"damage: full repaint, no presented swapchain attachment");
		}
		return true;
	}

	auto swapchainImage = static_cast<core::SwapchainImage *>(image);
	auto swapchain = swapchainImage->getSwapchain();
	if (!swapchain) {
		if (damageLog) {
			log::source().debug("soft::QueuePassHandle",
					"damage: full repaint, the image has no swapchain");
		}
		return true;
	}

	auto request = q.getFrame()->getRequest();

	Vector<URect> damage;
	const auto extent = Extent2(target.width, target.height);
	if (!swapchain->getDamage().computeRedrawArea(uint32_t(image->getImageIndex()),
				request->getDamageState().get(), extent, damage)) {
		if (damageLog) {
			auto state = request->getDamageState().get();
			log::source().debug("soft::QueuePassHandle", "damage: full repaint (state=",
					state ? "present" : "absent", ", full=", state ? state->full : false,
					", entries=", state ? state->entries.size() : 0, ", image=",
					image->getImageIndex(), ")");
		}
		return true; // the whole surface
	}

	if (damage.empty()) {
		// This image already holds exactly what the frame wants to draw. With the queue opted into
		// frame skipping there is nothing to do at all - not a cheaper frame, no frame.
		if (hasFlag(_data->queue->damage, core::QueueDamageFlags::SkipEmptyFrames)) {
			request->setRedrawSkipped(true);
			if (damageLog) {
				log::source().debug("soft::QueuePassHandle", "damage: frame skipped, nothing "
															"changed");
			}
			return false;
		}
		return true;
	}

	// Keep the regions apart (the tracker already merged them to at most MaxRects), but make them
	// pairwise disjoint: each region is a separate rasterization pass, and an overlap would blend
	// transparent commands twice. The tracker's one-pixel padding makes neighbours touch.
	areas.clear();
	for (auto &it : damage) {
		auto rect = it;
		bool merged = true;
		while (merged) {
			merged = false;
			for (size_t i = 0; i < areas.size(); ++i) {
				auto &existing = areas[i];
				if (raster::intersectRects(existing, rect).width == 0) {
					continue;
				}
				auto x0 = sprt::min(existing.x, rect.x);
				auto y0 = sprt::min(existing.y, rect.y);
				auto x1 = sprt::max(existing.x + existing.width, rect.x + rect.width);
				auto y1 = sprt::max(existing.y + existing.height, rect.y + rect.height);
				rect = URect{x0, y0, x1 - x0, y1 - y0};
				areas.erase(areas.begin() + i);
				merged = true;
				break;
			}
		}
		areas.emplace_back(rect);
	}

	if (damageLog) {
		uint64_t full = sprt::max(uint64_t(target.width) * uint64_t(target.height), uint64_t(1));
		uint64_t part = 0;
		for (auto &it : areas) { part += uint64_t(it.width) * uint64_t(it.height); }

		auto box = QueuePassHandle_boundingRect(areas);
		uint64_t boxArea = uint64_t(box.width) * uint64_t(box.height);

		log::source().debug("soft::QueuePassHandle", "damage: repainting ", areas.size(),
				" region(s), ", (part * 100) / full, "% of the surface (their bounding box would "
												  "have been ",
				(boxArea * 100) / full, "%)");
	}

	return true;
}

/* Frame budget counters: cumulative, so every report is a running average over the whole run.
Atomic because `present` may run on a different thread than the pass. */
static sprt::atomic<uint64_t> s_budgetStage[toInt(FrameStage::Count)] = {};
static sprt::atomic<uint64_t> s_budgetFrames{0};
static sprt::atomic<uint64_t> s_budgetPeriod{0};

// The reporting interval, resolved once. Same grammar as XL_SOFT_PROFILE: N = every N frames,
// unset or 0 = off, anything unparseable = 60.
static uint64_t FrameBudget_interval() {
	static const uint64_t value = [] () -> uint64_t {
		auto env = ::getenv("XL_SOFT_BUDGET");
		if (!env) {
			return 0;
		}
		auto str = StringView(env);
		if (str == "0") {
			return 0;
		}
		auto n = str.readInteger(10).get(0);
		return n > 0 ? uint64_t(n) : 60;
	}();
	return value;
}

bool isFrameBudgetEnabled() { return FrameBudget_interval() != 0; }

void addFrameStageTime(FrameStage stage, uint64_t micros) {
	if (stage < FrameStage::Count) {
		s_budgetStage[toInt(stage)].fetch_add(micros);
	}
}

// When the last present returned. Zero until the first one, so the first frame of a run
// contributes nothing.
static Time s_budgetPresented;

void openFrameBudget() {
	if (FrameBudget_interval() == 0 || s_budgetPresented == Time()) {
		return;
	}
	addFrameStageTime(FrameStage::Wait, (Time::now() - s_budgetPresented).toMicros());
}

void closeFrameBudget() {
	auto interval = FrameBudget_interval();
	if (interval == 0) {
		return;
	}

	// The period is present-to-present.
	auto &previous = s_budgetPresented;
	auto now = Time::now();
	auto frames = s_budgetFrames.fetch_add(1) + 1;
	if (previous != Time()) {
		s_budgetPeriod.fetch_add((now - previous).toMicros());
	}
	previous = now;

	if (frames % interval != 0) {
		return;
	}

	uint64_t stage[toInt(FrameStage::Count)];
	uint64_t accounted = 0;
	for (uint32_t i = 0; i < toInt(FrameStage::Count); ++i) {
		stage[i] = s_budgetStage[i].load();
		accounted += stage[i];
	}

	auto period = s_budgetPeriod.load();

	// `other` can come out negative on the first reports (stages and period have not yet covered
	// the same frames), so clamp it instead of wrapping.
	auto other = period > accounted ? period - accounted : 0;

	// Percentages of the period, not of the accounted total.
	auto pct = [&] (uint64_t v) { return period ? double(v) * 100.0 / double(period) : 0.0; };
	auto per = [&] (uint64_t v) { return double(v) / double(frames); };

	log::source().debug("soft::budget", "frames=", frames, " period=", per(period),
			"us/frame (", period ? 1'000'000.0 * double(frames) / double(period) : 0.0, " fps)");
	log::source().debug("soft::budget", "  wait=", per(stage[toInt(FrameStage::Wait)]), "us ",
			pct(stage[toInt(FrameStage::Wait)]), "%",
			" vertex=", per(stage[toInt(FrameStage::Vertex)]), "us ",
			pct(stage[toInt(FrameStage::Vertex)]), "%",
			" record=", per(stage[toInt(FrameStage::Record)]), "us ",
			pct(stage[toInt(FrameStage::Record)]), "%",
			" clear=", per(stage[toInt(FrameStage::Clear)]), "us ",
			pct(stage[toInt(FrameStage::Clear)]), "%");
	log::source().debug("soft::budget", "  raster=", per(stage[toInt(FrameStage::Raster)]), "us ",
			pct(stage[toInt(FrameStage::Raster)]), "%",
			" present=", per(stage[toInt(FrameStage::Present)]), "us ",
			pct(stage[toInt(FrameStage::Present)]), "%",
			" other=", per(other), "us ", pct(other), "%");
}

// XL_SOFT_PROFILE reports what the rasterizer costs: it times raster::draw only, around the whole
// tile fork and join. Runs on the loop thread only, so the counters need no synchronization.
static void QueuePassHandle_profileFrame(TimeInterval elapsed, SpanView<URect> areas,
		const raster::TilingStats &tiling, Extent2 surface) {
	// XL_SOFT_PROFILE=N reports every N frames; =1 is every frame, unset or =0 is off. Counters are
	// cumulative: every line is the running average over the whole run.
	static const uint64_t reportEvery = [] () -> uint64_t {
		auto value = ::getenv("XL_SOFT_PROFILE");
		if (!value) {
			return 0;
		}
		auto str = StringView(value);
		if (str == "0") {
			return 0;
		}
		auto n = str.readInteger(10).get(0);
		return n > 0 ? uint64_t(n) : 60;
	}();

	if (reportEvery == 0) {
		return;
	}

	static uint64_t frames = 0;
	static uint64_t micros = 0;
	static uint64_t pixels = 0;
	static uint64_t regions = 0;
	static uint64_t tileCount = 0;
	static uint64_t workerCount = 0;
	static uint64_t surfacePixels = 0;
	static raster::FillStats fill;

	++frames;
	micros += elapsed.toMicros();
	regions += areas.size();
	tileCount += tiling.tiles;
	workerCount += tiling.workers;
	surfacePixels += uint64_t(surface.width) * uint64_t(surface.height);
	fill.add(tiling.fill);
	for (auto &it : areas) { pixels += uint64_t(it.width) * uint64_t(it.height); }

	if (frames % reportEvery != 0) {
		return;
	}

	// Mpx/s compares kernel sets independently of the damage size. kernels=, threads= and
	// tiles/frame= report what actually ran, not what was requested, so fallbacks are visible.
	auto usec = sprt::max(micros, uint64_t(1));
	log::source().debug("soft::profile", "kernels=", raster::getActiveKernelSetName(),
			" threads=", double(workerCount) / double(frames), " frames=", frames,
			" regions/frame=", double(regions) / double(frames),
			" tiles/frame=", double(tileCount) / double(frames), " px/frame=", pixels / frames,
			" us/frame=", double(micros) / double(frames), " Mpx/s=", double(pixels) / double(usec));

	//   surface  - the window.
	//   damage   - what the tracker handed the rasterizer.
	//   filled   - what the kernels wrote; above damage is overdraw.
	//
	// damage/surface tells a full repaint apart; filled/damage is the work inside the repaint.
	auto denom = sprt::max(pixels, uint64_t(1));
	log::source().debug("soft::profile", "fill: surface/frame=", surfacePixels / frames,
			" damage/frame=", pixels / frames, " filled/frame=", fill.total() / frames,
			" (span=", fill.spanPixels / frames, " glyph=", fill.glyphPixels / frames,
			" rect=", fill.fillPixels / frames, ")",
			" damage/surface=",
			double(pixels) / double(sprt::max(surfacePixels, uint64_t(1))),
			" filled/damage=", double(fill.total()) / double(denom));
}

bool QueuePassHandle::runPass(core::FrameQueue &q) {
	auto getViewForAttachment =
			[&](const core::AttachmentSubpassData *desc) -> Rc<core::ImageView> {
		auto aIt = _queueData->attachmentMap.find(desc->pass->attachment);
		if (aIt == _queueData->attachmentMap.end() || !aIt->second->image) {
			return nullptr;
		}

		auto imgAttachment =
				static_cast<core::ImageAttachment *>(desc->pass->attachment->attachment.get());
		auto viewInfo = imgAttachment->getImageViewInfo(aIt->second->image->getInfo(), *desc->pass);
		return aIt->second->image->getView(viewInfo);
	};

	for (auto &subpass : _data->subpasses) {
		if (subpass->outputImages.empty()) {
			log::source().error("soft::QueuePassHandle", "Subpass has no colour output: ",
					subpass->key);
			return false;
		}

		// MRT is out of scope: the flat contract writes exactly one colour attachment, and
		// quietly rasterizing into the first of several would be worse than refusing.
		if (subpass->outputImages.size() > 1) {
			log::source().error("soft::QueuePassHandle",
					"Multiple colour outputs are not supported: ", subpass->key);
			return false;
		}

		auto out = subpass->outputImages.front();
		auto view = getViewForAttachment(out);
		if (!view) {
			log::source().error("soft::QueuePassHandle", "No image view for attachment: ",
					out->key);
			return false;
		}

		auto image = view->getImage().get_cast<Image>();
		if (!image) {
			log::source().error("soft::QueuePassHandle", "Attachment is not a software image: ",
					out->key);
			return false;
		}

		auto &info = image->getInfo();

		raster::Target target;
		target.pixels = image->getData();
		target.width = info.extent.width;
		target.height = info.extent.height;
		target.stride = image->getStride();
		target.format = getRasterFormat(info.format);

		if (target.empty() || raster::getPixelSize(target.format) == 0) {
			log::source().error("soft::QueuePassHandle", "Attachment is not rasterizable: ",
					out->key, " (format ", core::getImageFormatName(info.format), ")");
			return false;
		}

		_frameFill = raster::FillStats();
		_frameSurface = Extent2(target.width, target.height);

		Vector<URect> redrawAreas;
		if (!computeRedrawArea(q, target, redrawAreas)) {
			// the image already holds this frame; leave every pixel untouched
			return true;
		}

		if (redrawAreas.empty()) {
			return true;
		}

		auto buf = Rc<CommandBuffer>::create(*_device);
		if (!buf) {
			return false;
		}

		buf->setTarget(target);

		// The base scissor is the bounding box of the damage: it bounds the work done while
		// *recording* (clipping, span setup), which is per command and not per region. Each region
		// then narrows it further at draw time.
		buf->setScissor(QueuePassHandle_boundingRect(redrawAreas));

		// Load op: clear only inside the damaged regions; outside them the image keeps the previous
		// frame. The clear counts toward the fill stats like the draw.
		raster::FillStats clearFill;
		if (out->pass->loadOp == core::AttachmentLoadOp::Clear) {
			FrameStageTimer timer(FrameStage::Clear);
			auto imgAttachment =
					static_cast<core::ImageAttachment *>(out->pass->attachment->attachment.get());
			for (auto &it : redrawAreas) {
				raster::fillRect(target, it, imgAttachment->getClearColor(), &clearFill);
			}
		}

		{
			FrameStageTimer timer(FrameStage::Record);
			recordSubpass(q, *subpass, *buf);
		}

		// The command list is built once; rasterization repeats per tile of each region. Tiling and
		// thread count come from SP_RASTER_TILE / SP_RASTER_THREADS (untiled, single-threaded by
		// default).
		raster::TilingStats tiling;
		auto started = Time::now();
		raster::drawTiled(target, buf->getDrawList(), redrawAreas, raster::getDefaultTiling(),
				&tiling);
		auto elapsed = Time::now() - started;
		tiling.fill.add(clearFill);
		QueuePassHandle_profileFrame(elapsed, redrawAreas, tiling,
				Extent2(target.width, target.height));

		// The same span, charged to the budget independently of the profile.
		if (isFrameBudgetEnabled()) {
			addFrameStageTime(FrameStage::Raster, elapsed.toMicros());
		}

		_frameFill.add(tiling.fill);
		handlePassRasterized(q);
	}

	return true;
}

bool QueuePassHandle::prepare(core::FrameQueue &q, Function<void(bool)> &&cb) {
	_device = static_cast<Device *>(q.getFrame()->getDevice());
	_softLoop = static_cast<Loop *>(q.getFrame()->getLoop());

	return core::QueuePassHandle::prepare(q, sp::move(cb));
}

void QueuePassHandle::submit(core::FrameQueue &q, Rc<core::FrameSync> &&sync,
		Function<void(bool)> &&onSubmited, Function<void(bool)> &&onComplete) {
	// Rasterization is synchronous: by the time the pass returns, the pixels are written. The
	// fence is acquired anyway because the frame graph drives completion through it.
	auto success = runPass(q);

	_fence = _loop->acquireFence(core::FenceType::Default);
	if (!_fence) {
		onSubmited(false);
		return;
	}

	_fence->setTag(getName());
	_fence->addRelease([this, guard = Rc<core::FrameQueue>(&q),
							   onComplete = sp::move(onComplete)](bool fenceSuccess) {
		for (auto &it : _data->completeCallbacks) { it(*guard, *_data, fenceSuccess); }
		onComplete(fenceSuccess);
	}, this, "soft::QueuePassHandle::submit");

	// No device queue armed this fence, so arm it by hand; otherwise core::Fence::check skips it
	// and the release callbacks that complete the pass never run.
	_fence->setArmed();

	for (auto &it : _data->submittedCallbacks) { it(q, *_data, success); }

	onSubmited(success);

	auto fence = move(_fence);
	_fence = nullptr;
	fence->schedule(*_loop);
}

} // namespace stappler::xenolith::soft
