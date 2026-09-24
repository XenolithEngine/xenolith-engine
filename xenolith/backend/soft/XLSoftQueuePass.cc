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
#include <sprt/runtime/platform.h>

#include <cmath>
#include <cstdlib>

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft {

/* RGA2 blit (weak). */
extern "C" __attribute__((weak)) int rga2_blit(uintptr_t dst, uint32_t dst_stride, uint32_t dst_swap,
		int dx, int dy, int dw, int dh, uintptr_t src, uint32_t src_stride, uint32_t src_swap,
		int sx, int sy, int sw, int sh);

/* Scanout mapping for RGA video, or 0 if the fb cannot take direct writes. */
extern "C" __attribute__((weak)) uintptr_t xenolith_soft_scanout_fb(uint32_t *stride);

/* Last frame went to scanout via RGA; present() skips the shadow copy. A composed frame clears it;
 * empty draw lists leave it. Atomic: present() need not run on the thread that ran the pass. */
static sprt::atomic<bool> s_scanoutDirectSticky{false};

/* A direct-scanout frame never touched the pass's own image, so the damage tracker's snapshot of
 * that image is a frame behind what is on screen. Left set, the next composed frame would present
 * a shadow whose video area still holds whatever was there before the blits. One full repaint on
 * the way back closes that; on every backend without a scanout mapping the flag is never set. */
static sprt::atomic<bool> s_scanoutDirectPending{false};

namespace {

struct RgaBlitInfo {
	int32_t dx = 0, dy = 0, dw = 0, dh = 0;
	int32_t sx = 0, sy = 0, sw = 0, sh = 0;
	uint32_t srcStride = 0;
	const uint8_t *srcPixels = nullptr;
};

// Fullscreen video sprite: one opaque axis-aligned nearest-sampled
// unswizzled white RGBA8 quad (6 indexes / 4 vertices).
bool findRgaBlit(const raster::DrawList &list, RgaBlitInfo &out) {
	if (list.entries.size() != 1 || list.indexes.size() < 6 || list.vertexes.size() < 4) {
		return false;
	}
	const auto &entry = list.entries.front();
	if (entry.type != raster::DrawEntry::Triangles) {
		return false;
	}
	const auto &cmd = list.commands[entry.index];
	if (cmd.kind != raster::TextureKind::Texture2D || cmd.indexCount != 6) {
		return false;
	}
	if (cmd.blend != raster::BlendMode::Solid && cmd.blend != raster::BlendMode::Transparent) {
		return false;
	}
	if (cmd.sampler.filter != raster::Filter::Nearest) {
		return false;
	}
	const auto &tex = list.textures[cmd.texture];
	// BGRA only: rga2-lite rejects channel-swap on scaled paths.
	if (tex.format != raster::PixelFormat::BGRA8888 || tex.width < 2 || tex.height < 2) {
		return false;
	}
	for (auto &c : tex.swizzle) {
		if (c != raster::ComponentMapping::Identity) {
			return false;
		}
	}

	uint32_t idx[6];
	for (size_t i = 0; i < 6; ++i) {
		idx[i] = list.indexes[cmd.firstIndex + i];
	}

	float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
	float minU = 1e9f, maxU = -1e9f, minV = 1e9f, maxV = -1e9f;
	for (auto i : idx) {
		const auto &v = list.vertexes[i];
		if (v.x < minX) { minX = v.x; }
		if (v.x > maxX) { maxX = v.x; }
		if (v.y < minY) { minY = v.y; }
		if (v.y > maxY) { maxY = v.y; }
		if (v.u < minU) { minU = v.u; }
		if (v.u > maxU) { maxU = v.u; }
		if (v.v < minV) { minV = v.v; }
		if (v.v > maxV) { maxV = v.v; }
		if (v.color.r < 0.999f || v.color.g < 0.999f || v.color.b < 0.999f || v.color.a < 0.999f) {
			return false;
		}
	}
	for (auto i : idx) {
		const auto &v = list.vertexes[i];
		if ((fabsf(v.x - minX) > 0.01f && fabsf(v.x - maxX) > 0.01f)
				|| (fabsf(v.y - minY) > 0.01f && fabsf(v.y - maxY) > 0.01f)) {
			return false;
		}
	}

	out.dx = int32_t(minX + 0.5f);
	out.dy = int32_t(minY + 0.5f);
	out.dw = int32_t(maxX + 0.5f) - out.dx;
	out.dh = int32_t(maxY + 0.5f) - out.dy;
	out.sx = int32_t(minU * float(tex.width) + 0.5f);
	out.sy = int32_t(minV * float(tex.height) + 0.5f);
	out.sw = int32_t((maxU - minU) * float(tex.width) + 0.5f);
	out.sh = int32_t((maxV - minV) * float(tex.height) + 0.5f);
	out.srcStride = tex.stride;
	out.srcPixels = tex.pixels;
	return out.dw > 0 && out.dh > 0 && out.sw > 0 && out.sh > 0;
}

// XL_SOFT_RGA=0 forces the CPU path.
bool tryRgaVideoBlit(CommandBuffer &buf, const raster::Target &target,
		const Vector<URect> &areas, const Color4F &clearColor) {
	static const bool enabled = [] {
		auto e = ::getenv("XL_SOFT_RGA");
		return !e || ::atoi(e) != 0;
	}();
	if (!enabled || !rga2_blit) {
		return false;
	}
	if (target.format != getRasterFormat(core::ImageFormat::B8G8R8A8_UNORM)) {
		return false;
	}

	RgaBlitInfo b;
	if (!findRgaBlit(buf.getDrawList(), b)) {
		return false;
	}

	uint32_t x0 = target.width, y0 = target.height, x1 = 0, y1 = 0;
	for (auto &a : areas) {
		if (a.x < x0) { x0 = a.x; }
		if (a.y < y0) { y0 = a.y; }
		if (a.x + a.width > x1) { x1 = a.x + a.width; }
		if (a.y + a.height > y1) { y1 = a.y + a.height; }
	}

	// The sprite must span the damage vertically, or the rows it does not cover would keep the
	// previous frame while the rest advances.
	if (b.dy > int32_t(y0) || b.dy + b.dh < int32_t(y1)) {
		return false;
	}

	// Clip the blit to the damage bounding box, mapping back to source px.
	int32_t right = b.dx + b.dw;
	int32_t bx0 = b.dx > int32_t(x0) ? b.dx : int32_t(x0);
	int32_t bx1 = right < int32_t(x1) ? right : int32_t(x1);
	if (bx1 <= bx0) {
		return false;
	}

	// Every rejection is behind us: nothing above has written a pixel, so a decline cannot leave
	// half a frame in the scanout.
	uint32_t fbStride = 0;
	uintptr_t fbPixels = xenolith_soft_scanout_fb ? xenolith_soft_scanout_fb(&fbStride) : 0;
	raster::Target dst = target;
	if (fbPixels) {
		dst.pixels = reinterpret_cast<uint8_t *>(fbPixels);
		dst.stride = fbStride;
	}

	// Fill the letterbox strips, blit the intersection.
	if (b.dx > int32_t(x0)) {
		uint32_t w = uint32_t(b.dx) - x0;
		raster::fillRect(dst, URect(x0, y0, w, y1 - y0), clearColor);
	}
	if (right < int32_t(x1)) {
		uint32_t rx = uint32_t(right > int32_t(x0) ? right : int32_t(x0));
		raster::fillRect(dst, URect(rx, y0, x1 - rx, y1 - y0), clearColor);
	}

	int32_t bw = bx1 - bx0;
	float scaleX = float(b.sw) / float(b.dw);
	int32_t isx = b.sx + int32_t(float(bx0 - b.dx) * scaleX + 0.5f);
	int32_t isw = int32_t(float(bw) * scaleX + 0.5f);
	if (isw < 1) { isw = 1; }
	b.dx = bx0; b.dw = bw; b.sx = isx; b.sw = isw;

	bool ok = rga2_blit(uintptr_t(dst.pixels), uint32_t(dst.stride), 2, b.dx, b.dy, b.dw, b.dh,
					  uintptr_t(b.srcPixels), b.srcStride, 2, b.sx, b.sy, b.sw, b.sh)
			== 0;
	// Only a blit that actually reached the scanout lets present() skip the shadow copy. On failure
	// the caller clears the flag and present() copies the shadow over whatever landed here.
	if (ok && fbPixels) {
		s_scanoutDirectSticky.store(true);
		s_scanoutDirectPending.store(true);
	}
	return ok;
}


}

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

/* XL_SOFT_SWEEP: walk the rasterizer through a list of configurations inside one run, one step at
a time, and report each step on its own lines (soft::sweep).

It exists for the board. A configuration read from the environment at start-up is one image and
one boot per measurement point, and on the Pi 4 each of those is an update cycle; here one boot
covers them all, back to back, on the same scene.

	XL_SOFT_SWEEP=W:M[:R]   W frames of warm-up per step (run, not counted), M frames measured,
	                        R passes over the whole list (default 1). Unset or 0 = off.
	XL_SOFT_SWEEP_STEPS     the list, steps separated by spaces or commas, each MODE/TILE/THREADS:
	                          MODE     full (f)   - every frame repaints the whole surface
	                                   damage (d) - what the damage tracker says, frame skipping
	                                                included
	                          TILE     off, W (square) or WxH; 0 in either place is "do not cut
	                                   that way", so 0x64 is full-width strips
	                          THREADS  N, or A-B for one step per count; capped by what the pool
	                                   can supply
	                        Default: FrameSweep_defaultSteps. Short forms exist because of Embox:
	                        its shell cuts `export NAME=VALUE` at 64 characters, which leaves the
	                        list about 37 - "d/64/1-4,f/512/1-4" is eight steps.

`full` does not bypass the damage tracker, it overrules it: the tracker is still asked, so its
per-image snapshot stays current and the next `damage` step starts from a correct baseline rather
than a stale one (a stale one can report "unchanged" for a region the image no longer holds). Only
the area handed to the rasterizer is widened.

The period is runPass to runPass, so a frame the tracker skipped still counts - with nothing
rasterized in it.

Every span is taken in ticks of platform::clock(ClockType::Hardware) - the cycle or system counter -
because the system clock is not always fine enough for them: on Embox it advances once per
millisecond, and a stage of a millisecond or two timed by it is mostly quantization. The step's
whole measured window is timed both ways, and the ratio converts ticks to microseconds; over a
window of seconds the system clock's millisecond is noise. tick_mhz= reports that ratio, which is
also a check: 54 on a Pi 4. After the last pass the sweep reports `done` and steps aside: the pass returns to
SP_RASTER_TILE / SP_RASTER_THREADS and to the tracker's own decisions. */
namespace {

struct FrameSweepStep {
	bool full = false;
	raster::TilingInfo tiling;
};

struct FrameSweepAcc {
	uint64_t frames = 0;
	uint64_t skipped = 0;
	uint64_t periodMicros = 0; // the measured window by the system clock...
	uint64_t periodTicks = 0; // ...and by the hardware counter; the ratio converts the rest
	uint64_t recordTicks = 0;
	uint64_t clearTicks = 0;
	uint64_t rasterTicks = 0;
	uint64_t damagePixels = 0;
	uint64_t regions = 0;
	uint64_t tiles = 0;
	uint64_t workers = 0;
	uint64_t busyTicks = 0;
	uint64_t maxBusyTicks = 0;
	uint64_t maxStartTicks = 0;
	raster::FillStats raster; // the draw alone
	raster::FillStats clear; // the load op
};

struct FrameSweep {
	bool enabled = false;
	bool done = false;
	uint32_t warmup = 0;
	uint32_t measure = 0;
	uint32_t passes = 1;
	Vector<FrameSweepStep> steps;

	uint32_t pass = 0;
	uint32_t step = 0;
	uint32_t frameInStep = 0;
	bool current = false; // the frame in flight is measured
	bool previous = false; // the one before it was
	Time last;
	uint64_t lastTicks = 0;
	FrameSweepAcc acc;
};

static uint64_t FrameSweep_ticks() {
#if SPRT_EMBOX_USER
	// Not readable at EL0 on Embox; see drawTiled. Coarse, and tick_mhz= will say 1.
	return sprt::platform::clock(sprt::platform::ClockType::Monotonic);
#else
	return sprt::platform::clock(sprt::platform::ClockType::Hardware);
#endif
}

// What the question needs: a solid baseline, the same with square tiles at one thread (the price
// of cutting, nothing else), the thread counts at the default size, the sizes around it, and
// strips (cut rows only: nothing splits a span). Then the same under the damage tracker, where the
// regions are small and threads have little to share.
static const char *FrameSweep_defaultSteps =
		"full/off/1 full/256/1 full/256/2 full/256/3 full/256/4 "
		"full/128/1 full/128/4 full/64/1 full/64/4 full/512/1 full/512/4 full/0x64/1 full/0x64/4 "
		"damage/off/1 damage/256/1 damage/256/2 damage/256/4 damage/64/1 damage/64/4";

static uint32_t FrameSweep_readUint(const char *&p) {
	uint32_t v = 0;
	while (*p >= '0' && *p <= '9') {
		v = v * 10 + uint32_t(*p - '0');
		++p;
	}
	return v;
}

static bool FrameSweep_prefix(const char *&c, const char *prefix) {
	size_t i = 0;
	for (; prefix[i]; ++i) {
		if (c[i] != prefix[i]) {
			return false;
		}
	}
	c += i;
	return true;
}

// One entry of the list, which may stand for several steps (a thread range). Appends them to
// `out`; false leaves `out` as it was.
static bool FrameSweep_parseStep(const char *p, const char *end, Vector<FrameSweepStep> &out) {
	char buf[48];
	auto len = size_t(end - p);
	if (len == 0 || len >= sizeof(buf)) {
		return false;
	}
	memcpy(buf, p, len);
	buf[len] = 0;

	FrameSweepStep step;
	const char *c = buf;
	if (FrameSweep_prefix(c, "full/") || FrameSweep_prefix(c, "f/")) {
		step.full = true;
	} else if (FrameSweep_prefix(c, "damage/") || FrameSweep_prefix(c, "d/")) {
		step.full = false;
	} else {
		return false;
	}

	if (FrameSweep_prefix(c, "off/")) {
		step.tiling.width = step.tiling.height = 0;
	} else {
		auto w = FrameSweep_readUint(c);
		auto h = w;
		if (*c == 'x' || *c == 'X') {
			++c;
			h = FrameSweep_readUint(c);
		}
		if (*c != '/' || (w == 0 && h == 0)) {
			return false;
		}
		step.tiling.width = w;
		step.tiling.height = h;
		++c;
	}

	auto first = FrameSweep_readUint(c);
	auto last = first;
	if (*c == '-') {
		++c;
		last = FrameSweep_readUint(c);
	}
	if (*c != 0 || first == 0 || last < first) {
		return false;
	}

	step.tiling.timed = true;
	for (auto threads = first; threads <= last; ++threads) {
		step.tiling.threads = threads;
		out.emplace_back(step);
	}
	return true;
}

static FrameSweep FrameSweep_load() {
	FrameSweep s;
	auto env = ::getenv("XL_SOFT_SWEEP");
	if (!env || !*env || StringView(env) == "0") {
		return s;
	}

	const char *p = env;
	s.warmup = FrameSweep_readUint(p);
	if (*p == ':') {
		++p;
		s.measure = FrameSweep_readUint(p);
	}
	if (*p == ':') {
		++p;
		s.passes = FrameSweep_readUint(p);
	}
	if (*p != 0 || s.measure == 0 || s.passes == 0) {
		log::source().error("soft::sweep", "XL_SOFT_SWEEP=", StringView(env),
				" is not W:M[:R] with M and R above zero; the sweep is off");
		return s;
	}

	const char *list = ::getenv("XL_SOFT_SWEEP_STEPS");
	if (!list || !*list) {
		list = FrameSweep_defaultSteps;
	}

	for (const char *it = list; *it;) {
		while (*it == ' ' || *it == ',') {
			++it;
		}
		auto end = it;
		while (*end && *end != ' ' && *end != ',') {
			++end;
		}
		if (end == it) {
			break;
		}
		if (!FrameSweep_parseStep(it, end, s.steps)) {
			// Not skipped: a list with a hole in it measures something else than was asked, and
			// the report would not say so.
			log::source().error("soft::sweep", "step '", StringView(it, size_t(end - it)),
					"' is not MODE/TILE/THREADS; the sweep is off");
			return FrameSweep();
		}
		it = end;
	}

	if (s.steps.empty()) {
		return FrameSweep();
	}

	s.enabled = true;
	log::source().debug("soft::sweep", "start: ", s.steps.size(), " steps x ", s.passes,
			" pass(es), ", s.warmup, " warm-up + ", s.measure, " measured frames each; kernels=",
			raster::getActiveKernelSetName());
	return s;
}

static FrameSweep &FrameSweep_get() {
	static FrameSweep s = FrameSweep_load();
	return s;
}

// "off", or WxH as it was cut - so a square asked for as "256" reads back as "256x256".
static String FrameSweep_tileName(const raster::TilingInfo &tiling) {
	if (!tiling.tiled()) {
		return String("off");
	}
	return toString(tiling.width, "x", tiling.height);
}

static void FrameSweep_report(const FrameSweep &s, Extent2 surface) {
	auto &a = s.acc;
	auto &st = s.steps[s.step];
	const double n = double(sprt::max(a.frames, uint64_t(1)));
	const double usPerTick =
			a.periodTicks ? double(a.periodMicros) / double(a.periodTicks) : 1.0;
	auto per = [&](uint64_t v) { return double(v) / n; };
	auto perT = [&](uint64_t ticks) { return double(ticks) * usPerTick / n; };
	auto rate = [&](uint64_t px, uint64_t ticks) {
		return ticks ? double(px) / (double(ticks) * usPerTick) : 0.0;
	};

	// One key=value line per aspect, all tagged with the same step so they can be read apart:
	//   step   - where the time went (us/frame); ran= is how many threads the pool actually
	//            supplied, against the threads= asked for
	//   fill   - pixels written per frame and per microsecond of the stage that wrote them
	//            (Mpx/s): raster= is the draw, clear= the load op, area= the damage per raster-us
	//   ops    - the rasterizer's own work per frame, which is what tiling multiplies
	//   pool   - the workers: busy is summed over them, longest is the slowest, start is how late
	//            the last one took its first tile
	log::source().debug("soft::sweep", "step p=", s.pass + 1, " i=", s.step + 1, "/",
			s.steps.size(), " name=", st.full ? "full/" : "damage/", FrameSweep_tileName(st.tiling),
			"/", st.tiling.threads, " threads=", st.tiling.threads,
			" ran=", per(a.workers), " frames=", a.frames, " skipped=", a.skipped, " period=",
			perT(a.periodTicks), " record=", perT(a.recordTicks), " clear=", perT(a.clearTicks),
			" raster=", perT(a.rasterTicks), " surface=",
			uint64_t(surface.width) * uint64_t(surface.height), " tick_mhz=",
			usPerTick > 0.0 ? 1.0 / usPerTick : 0.0);
	log::source().debug("soft::sweep", "fill i=", s.step + 1, " damage=", per(a.damagePixels),
			" regions=", per(a.regions), " tiles=", per(a.tiles), " raster_px=",
			per(a.raster.total()), " span_px=", per(a.raster.spanPixels), " glyph_px=",
			per(a.raster.glyphPixels), " rect_px=", per(a.raster.fillPixels), " clear_px=",
			per(a.clear.total()), " raster_mpxs=", rate(a.raster.total(), a.rasterTicks),
			" clear_mpxs=", rate(a.clear.total(), a.clearTicks), " area_mpxs=",
			rate(a.damagePixels, a.rasterTicks));
	log::source().debug("soft::sweep", "ops i=", s.step + 1, " passes=",
			per(a.raster.ops.passes), " entries=", per(a.raster.ops.entries), " commands=",
			per(a.raster.ops.commands), " triangles=", per(a.raster.ops.triangles), " setups=",
			per(a.raster.ops.setups), " rows=", per(a.raster.ops.rows), " spans=",
			per(a.raster.ops.spans), " glyphs=", per(a.raster.ops.glyphs), " rects=",
			per(a.raster.ops.rects));
	log::source().debug("soft::sweep", "pool i=", s.step + 1, " busy=", perT(a.busyTicks),
			" longest=", perT(a.maxBusyTicks), " start=", perT(a.maxStartTicks));
}

// Called once per runPass, before anything else. Closes the frame before it, reports and advances
// when a step is complete, and returns the step this frame runs under - or nullptr when the sweep
// is off or over.
static const FrameSweepStep *FrameSweep_begin(Extent2 surface) {
	auto &s = FrameSweep_get();
	if (!s.enabled || s.done) {
		return nullptr;
	}

	auto now = Time::now();
	auto nowTicks = FrameSweep_ticks();
	if (s.previous && s.last != Time()) {
		s.acc.periodMicros += (now - s.last).toMicros();
		s.acc.periodTicks += nowTicks - s.lastTicks;
		++s.acc.frames;
	}
	s.last = now;
	s.lastTicks = nowTicks;

	if (s.acc.frames >= s.measure) {
		FrameSweep_report(s, surface);
		s.acc = FrameSweepAcc();
		s.frameInStep = 0;
		if (++s.step >= s.steps.size()) {
			s.step = 0;
			if (++s.pass >= s.passes) {
				s.done = true;
				s.previous = s.current = false;
				log::source().debug("soft::sweep", "done");
				return nullptr;
			}
		}
	}

	s.current = s.frameInStep >= s.warmup;
	s.previous = s.current;
	++s.frameInStep;
	return &s.steps[s.step];
}

static FrameSweepAcc *FrameSweep_acc() {
	auto &s = FrameSweep_get();
	return (s.enabled && !s.done && s.current) ? &s.acc : nullptr;
}

static bool FrameSweep_full() {
	auto &s = FrameSweep_get();
	return s.enabled && !s.done && s.steps[s.step].full;
}

} // namespace

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

	// See s_scanoutDirectPending: the previous frame bypassed this image entirely.
	if (s_scanoutDirectPending.exchange(false)) {
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

	// A `full` sweep step: the tracker has committed its snapshot for this image, which is all it
	// was asked for. `areas` still holds the whole surface.
	if (FrameSweep_full()) {
		return true;
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

		auto sweepStep = FrameSweep_begin(_frameSurface);
		auto sweepAcc = FrameSweep_acc();

		Vector<URect> redrawAreas;
		if (!computeRedrawArea(q, target, redrawAreas)) {
			// the image already holds this frame; leave every pixel untouched
			if (sweepAcc) {
				++sweepAcc->skipped;
			}
			return true;
		}

		if (redrawAreas.empty()) {
			if (sweepAcc) {
				++sweepAcc->skipped;
			}
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

		// Record first: an empty draw list must not clear the previous frame
		// (dynamic-image mid-rebind used to publish a black frame).
		{
			FrameStageTimer timer(FrameStage::Record);
			auto recordStarted = sweepAcc ? FrameSweep_ticks() : 0;
			recordSubpass(q, *subpass, *buf);
			if (sweepAcc) {
				sweepAcc->recordTicks += FrameSweep_ticks() - recordStarted;
			}
		}

		if (buf->getDrawList().empty()) {
			return true;
		}

		auto clearAttachment =
				static_cast<core::ImageAttachment *>(out->pass->attachment->attachment.get());
		if (tryRgaVideoBlit(*buf, target, redrawAreas, clearAttachment->getClearColor())) {
			return true;
		}

		// Composed into the shadow: present() must copy again.
		s_scanoutDirectSticky.store(false);

		// Load op. Clear is the only one that touches memory, and only inside the damaged regions:
		// outside them the image keeps the previous frame, which is exactly what makes the partial
		// redraw correct rather than merely cheaper.
		// The clear writes real pixels and belongs in the same budget as the draw - on a frame
		// whose damage is the whole surface it is the single largest writer.
		raster::FillStats clearFill;
		if (out->pass->loadOp == core::AttachmentLoadOp::Clear) {
			FrameStageTimer timer(FrameStage::Clear);
			auto clearStarted = sweepAcc ? FrameSweep_ticks() : 0;
			auto imgAttachment =
					static_cast<core::ImageAttachment *>(out->pass->attachment->attachment.get());
			for (auto &it : redrawAreas) {
				raster::fillRect(target, it, imgAttachment->getClearColor(), &clearFill);
			}
			if (sweepAcc) {
				sweepAcc->clearTicks += FrameSweep_ticks() - clearStarted;
			}
		}

		// The command list is built once; rasterization repeats per tile of each region. Tiling and
		// thread count come from SP_RASTER_TILE / SP_RASTER_THREADS (untiled, single-threaded by
		// default).
		raster::TilingStats tiling;
		auto started = Time::now();
		auto startedTicks = sweepAcc ? FrameSweep_ticks() : 0;
		raster::drawTiled(target, buf->getDrawList(), redrawAreas,
				sweepStep ? sweepStep->tiling : raster::getDefaultTiling(), &tiling);
		auto elapsed = Time::now() - started;

		if (sweepAcc) {
			sweepAcc->rasterTicks += FrameSweep_ticks() - startedTicks;
			sweepAcc->regions += redrawAreas.size();
			for (auto &it : redrawAreas) {
				sweepAcc->damagePixels += uint64_t(it.width) * uint64_t(it.height);
			}
			sweepAcc->tiles += tiling.tiles;
			sweepAcc->workers += tiling.workers;
			sweepAcc->busyTicks += tiling.busyTicks;
			sweepAcc->maxBusyTicks += tiling.maxBusyTicks;
			sweepAcc->maxStartTicks += tiling.maxStartTicks;
			sweepAcc->raster.add(tiling.fill);
			sweepAcc->clear.add(clearFill);
		}

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

// Read by the embox swapchain's present(): the last frame already reached the scanout, so the
// shadow copy can be skipped (a cache clean is still needed).
extern "C" int xenolith_soft_rga_direct_presented(void) {
	return stappler::xenolith::soft::s_scanoutDirectSticky.load() ? 1 : 0;
}
