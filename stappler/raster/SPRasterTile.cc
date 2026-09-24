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

#include "SPRasterKernel.h"

#include <sprt/runtime/dispatch/looper.h>
#include <sprt/runtime/dispatch/task.h>
#include <sprt/runtime/thread/qtimeline.h>

// Cutting a region into tiles, and handing the tiles to a thread pool.
//
// Neither half touches the pixel loops: a tile is nothing but a smaller `clip`, and `draw` already
// took one. What the rasterizer gains is locality - a run across the full width of a 1080p surface
// has evicted its own texture before the next row starts - and what the frame gains is the other
// cores. They are separate effects and are switched separately, because a measurement that turns
// both on at once cannot attribute either.

namespace STAPPLER_VERSIONIZED stappler::raster {

// A cache line, on every target this builds for. Not the vector width: an unaligned load costs
// almost nothing on x86, while a load that straddles two lines costs a second access - and two
// threads writing into one line costs far more than that.
static constexpr uint32_t Tile_alignBytes = 64;

void makeTileGrid(const URect &region, const TilingInfo &tiling, uint32_t pixelSize,
		const Callback<void(const URect &)> &cb) {
	if (region.width == 0 || region.height == 0) {
		return;
	}

	const uint32_t stepPx = pixelSize > 0 ? sprt::max(1U, Tile_alignBytes / pixelSize) : 1U;

	uint32_t tileWidth = tiling.width > 0 ? tiling.width : region.width;
	uint32_t tileHeight = tiling.height > 0 ? tiling.height : region.height;

	// Down to a whole number of cache lines, never up: the requested width is an upper bound, and
	// rounding it up would silently hand back tiles larger than were asked for. Below one line
	// there is nothing to align to and the request is honoured as it stands - which is what makes
	// a one-pixel tiling usable as a test.
	if (tileWidth >= stepPx) {
		tileWidth -= tileWidth % stepPx;
	}
	if (tileWidth == 0) {
		tileWidth = 1;
	}
	if (tileHeight == 0) {
		tileHeight = 1;
	}

	const uint32_t xEnd = region.x + region.width;
	const uint32_t yEnd = region.y + region.height;

	for (uint32_t y = region.y; y < yEnd;) {
		const uint32_t ny = sprt::min(y + tileHeight, yEnd);
		for (uint32_t x = region.x; x < xEnd;) {
			uint32_t nx = x + tileWidth;

			// Cut on the alignment grid whenever that boundary falls inside the tile. The first
			// tile of a row absorbs the misalignment and every one after it starts on a line.
			if (tileWidth >= stepPx) {
				const uint32_t snapped = nx - (nx % stepPx);
				if (snapped > x && snapped < nx) {
					nx = snapped;
				}
			}

			nx = sprt::min(nx, xEnd);
			cb(URect{x, y, nx - x, ny - y});
			x = nx;
		}
		y = ny;
	}
}

static uint32_t Tile_readUint(const char *&p) {
	uint32_t value = 0;
	while (*p >= '0' && *p <= '9') {
		value = value * 10 + uint32_t(*p - '0');
		++p;
	}
	return value;
}

const TilingInfo &getDefaultTiling() {
	static const TilingInfo s_tiling = [] {
		TilingInfo info;
		info.width = 256;
		info.height = 256;
		info.threads = 0; // as many as the pool has

		if (auto value = ::getenv("SP_RASTER_TILE")) {
			auto str = StringView(value);
			if (str == "off" || str == "0") {
				info.width = 0;
				info.height = 0;
			} else {
				const char *p = value;
				auto w = Tile_readUint(p);
				auto h = w;
				if (*p == 'x' || *p == 'X') {
					++p;
					h = Tile_readUint(p);
				}

				// Not a silent fallback, for the same reason SP_RASTER_KERNELS is not: a typo
				// would otherwise read as "tiling did not help", which is hard to un-conclude.
				if (w == 0 && h == 0) {
					log::source().error("raster", "SP_RASTER_TILE=", str,
							" is not WxH, W or off; tiling stays off");
				} else {
					info.width = w;
					info.height = h;
				}
			}
		}

		if (auto value = ::getenv("SP_RASTER_THREADS")) {
			const char *p = value;
			auto n = Tile_readUint(p);
			if (n == 0) {
				log::source().error("raster", "SP_RASTER_THREADS=", StringView(value),
						" is not a positive count; staying single-threaded");
			} else {
				info.threads = n;
			}
		}

		return info;
	}();
	return s_tiling;
}

// The tiles of every region, clipped to the target.
static void Tile_collect(const Target &target, SpanView<URect> regions, const TilingInfo &tiling,
		Vector<URect> &tiles) {
	const auto bounds = URect{0, 0, target.width, target.height};
	const auto pixelSize = getPixelSize(target.format);

	for (auto &region : regions) {
		auto clipped = intersectRects(region, bounds);
		if (clipped.width == 0 || clipped.height == 0) {
			continue;
		}
		makeTileGrid(clipped, tiling, pixelSize,
				[&](const URect &tile) { tiles.emplace_back(tile); });
	}
}

// The loop every worker runs: take the next tile until none is left. Workers are greedy, one task
// each, rather than one task per tile: tiles differ in cost by more than an order of magnitude, so
// a static split would leave threads idle.
static uint32_t Tile_drawShare(const Target &target, const DrawList *list, SpanView<URect> tiles,
		sprt::atomic<uint32_t> &next, const Color4F *clear, FillStats *fill) {
	uint32_t drawn = 0;
	for (;;) {
		auto index = next.fetch_add(1);
		if (index >= tiles.size()) {
			break;
		}
		if (clear) {
			fillRect(target, tiles[index], *clear, fill);
		}
		if (list) {
			drawn += draw(target, *list, tiles[index], fill);
		}
	}
	return drawn;
}

uint32_t drawTiled(const Target &target, const DrawList &list, SpanView<URect> regions,
		const TilingInfo &tiling, TilingStats *stats) {
	if (stats) {
		*stats = TilingStats{};
	}

	if (target.empty() || list.empty() || regions.empty()) {
		return 0;
	}

	Vector<URect> tiles;
	Tile_collect(target, regions, tiling, tiles);

	if (stats) {
		stats->tiles = uint32_t(tiles.size());
	}

	if (tiles.empty()) {
		return 0;
	}

	// Resolve the kernel table here rather than letting a worker be the first to ask. It is a lazy
	// static that allocates and logs on its first call: N threads arriving at once would serialize
	// on the guard, and the line saying which set is in use would come from whichever one won.
	getKernels();

	// The calling thread is one of the workers, so the pool only has to supply the rest.
	auto looper = sprt::dispatch::Looper::getIfExists();
	const uint32_t available = looper ? uint32_t(looper->getWorkersCount()) + 1 : 1;

	// Without a looper there is nothing to fan out to and the loop runs here: that is the unit
	// test, and it is also wasm, where hardware_concurrency() is 1 by construction.
	uint32_t workers = tiling.threads > 0 ? sprt::min(tiling.threads, available) : available;
	workers = sprt::min(workers, uint32_t(tiles.size()));

	if (workers <= 1) {
		if (stats) {
			stats->workers = 1;
		}
		sprt::atomic<uint32_t> nextTile{0};
		return Tile_drawShare(target, &list, tiles, nextTile, nullptr,
				stats ? &stats->fill : nullptr);
	}

	// Holding a pool worker for the whole rasterization is safe because of when this runs, not by
	// luck: the vertex stage and the font work are joined before the command list is recorded, so
	// the pool has nothing else to do inside a frame. drawTiledAsync breaks that assumption, and
	// pays for it with latency only: a worker never waits on anything.
	sprt::atomic<uint32_t> nextTile{0};
	sprt::atomic<uint32_t> drawn{0};
	sprt::qtimeline finished;

	// One add per worker at the end of its run, not one per tile: the counters are a diagnostic
	// and must not put a contended cache line in the middle of the pixel loops.
	sprt::atomic<uint64_t> spanPixels{0};
	sprt::atomic<uint64_t> glyphPixels{0};
	sprt::atomic<uint64_t> fillPixels{0};

	// Captured by reference on purpose: the calling thread does not return until every worker has
	// signalled, so everything here outlives them.
	auto body = [&] {
		FillStats localFill;
		drawn.fetch_add(Tile_drawShare(target, &list, tiles, nextTile, nullptr,
				stats ? &localFill : nullptr));
		if (stats) {
			spanPixels.fetch_add(localFill.spanPixels);
			glyphPixels.fetch_add(localFill.glyphPixels);
			fillPixels.fetch_add(localFill.fillPixels);
		}
	};

	uint32_t posted = 0;
	for (uint32_t i = 1; i < workers; ++i) {
		auto st = looper->performAsync([&] {
			body();
			finished.signal(1);
		});
		if (sprt::status::isSuccessful(st)) {
			++posted;
		}
	}

	// The calling thread takes tiles too - it would otherwise stand and wait - and it drains
	// whatever the pool did not get to, so a task that never ran cannot leave a tile unpainted.
	body();

	if (posted > 0) {
		finished.wait(posted);
	}

	if (stats) {
		stats->workers = posted + 1;
		stats->fill.spanPixels = spanPixels.load();
		stats->fill.glyphPixels = glyphPixels.load();
		stats->fill.fillPixels = fillPixels.load();
	}

	return drawn.load();
}

Rc<TiledDrawJob> drawTiledAsync(TiledDrawRequest &&req, TiledDrawCallback &&complete, Ref *owner) {
	auto job = Rc<TiledDrawJob>::alloc();
	job->_target = req.target;
	job->_list = (req.list && !req.list->empty()) ? req.list : nullptr;
	job->_clear = req.clear;
	job->_clearColor = req.clearColor;
	job->_collectStats = req.collectStats;

	if (!req.target.empty() && (job->_list || job->_clear)) {
		Tile_collect(req.target, req.regions, req.tiling, job->_tiles);
	}

	if (job->_tiles.empty()) {
		complete(true, 0, job->getStats());
		return nullptr;
	}

	getKernels();

	auto looper = sprt::dispatch::Looper::getIfExists();
	const uint32_t pool = looper ? uint32_t(looper->getWorkersCount()) : 0;

	uint32_t workers = req.tiling.threads > 0 ? sprt::min(req.tiling.threads, pool) : pool;
	workers = sprt::min(workers, uint32_t(job->_tiles.size()));

	if (workers == 0) {
		job->_workers = 1;
		job->drawShare();
		complete(true, job->_drawn.load(), job->getStats());
		return nullptr;
	}

	job->_complete = sp::move(complete);
	job->_workers = workers;
	job->_pending.store(workers);

	for (uint32_t i = 0; i < workers; ++i) {
		auto task = Rc<sprt::dispatch::Task>::create([job](const sprt::dispatch::Task &) {
			job->drawShare();
			return true;
		}, [job](const sprt::dispatch::Task &, bool executed) {
			job->handleWorkerComplete(executed);
		}, owner);

		auto st = looper->performAsync(sp::move(task));
		if (!sprt::status::isSuccessful(st) && st != Status::Declined) {
			// A pool that refuses outright sends no completion: draw its share here instead.
			job->drawShare();
			job->handleWorkerComplete(true);
		}
	}

	return job;
}

bool TiledDrawJob::isDrawn() const { return _finished.try_wait(_workers); }

void TiledDrawJob::waitDrawn() { _finished.wait(_workers); }

void TiledDrawJob::drawShare() {
	FillStats fill;
	_drawn.fetch_add(Tile_drawShare(_target, _list, _tiles, _nextTile,
			_clear ? &_clearColor : nullptr, _collectStats ? &fill : nullptr));
	if (_collectStats) {
		_spanPixels.fetch_add(fill.spanPixels);
		_glyphPixels.fetch_add(fill.glyphPixels);
		_fillPixels.fetch_add(fill.fillPixels);
	}
	_finished.signal(1);
}

void TiledDrawJob::handleWorkerComplete(bool executed) {
	if (!executed) {
		_finished.signal(1);
	}

	if (_pending.fetch_sub(1) == 1) {
		// A worker that ran at all ran until the tiles were exhausted, so tiles are left over only
		// when every worker was dropped unrun.
		auto success = _nextTile.load() >= _tiles.size();
		auto complete = sp::move(_complete);
		_complete = nullptr;
		complete(success, _drawn.load(), getStats());
	}
}

TilingStats TiledDrawJob::getStats() const {
	TilingStats stats;
	stats.tiles = uint32_t(_tiles.size());
	stats.workers = _workers;
	stats.fill.spanPixels = _spanPixels.load();
	stats.fill.glyphPixels = _glyphPixels.load();
	stats.fill.fillPixels = _fillPixels.load();
	return stats;
}

} // namespace stappler::raster
