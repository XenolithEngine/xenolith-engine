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

uint32_t drawTiled(const Target &target, const DrawList &list, SpanView<URect> regions,
		const TilingInfo &tiling, TilingStats *stats) {
	if (stats) {
		*stats = TilingStats{};
	}

	if (target.empty() || list.empty() || regions.empty()) {
		return 0;
	}

	const auto bounds = URect{0, 0, target.width, target.height};
	const auto pixelSize = getPixelSize(target.format);

	Vector<URect> tiles;
	for (auto &region : regions) {
		auto clipped = intersectRects(region, bounds);
		if (clipped.width == 0 || clipped.height == 0) {
			continue;
		}
		makeTileGrid(clipped, tiling, pixelSize,
				[&](const URect &tile) { tiles.emplace_back(tile); });
	}

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
		uint32_t drawn = 0;
		for (auto &tile : tiles) { drawn += draw(target, list, tile); }
		return drawn;
	}

	// Workers are greedy: one task per worker, each looping until the tile list is exhausted,
	// rather than one task per tile. Tiles differ in cost by more than an order of magnitude - an
	// empty corner against one holding the whole sprite - so a static split would leave threads
	// idle, and a task per tile would post a completion back to the looper for every one of them.
	//
	// Holding a pool worker for the whole rasterization is safe because of when this runs, not by
	// luck: the vertex stage and the font work are joined before the command list is recorded, so
	// the pool has nothing else to do inside a frame. Should work ever start arriving in parallel
	// with a frame, this is the assumption that has to be revisited - a greedy worker would keep
	// it waiting rather than interleave with it.
	sprt::atomic<uint32_t> nextTile{0};
	sprt::atomic<uint32_t> drawn{0};
	sprt::qtimeline finished;

	const uint32_t total = uint32_t(tiles.size());

	// Captured by reference on purpose: the calling thread does not return until every worker has
	// signalled, so everything here outlives them.
	auto body = [&] {
		uint32_t local = 0;
		for (;;) {
			auto index = nextTile.fetch_add(1);
			if (index >= total) {
				break;
			}
			local += draw(target, list, tiles[index]);
		}
		drawn.fetch_add(local);
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
	}

	return drawn.load();
}

} // namespace stappler::raster
