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

#include "XLCompositorBlend.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::compositor {

// round(x / 255) for x in [0, 255 * 255]
static inline uint32_t Blend_divide255(uint32_t x) {
	auto t = x + 128;
	return (t + (t >> 8)) >> 8;
}

static inline bool Blend_isSupported(raster::PixelFormat format) {
	return format == raster::PixelFormat::RGBA8888 || format == raster::PixelFormat::BGRA8888;
}

// The part of the clip a source covers, in target pixels; empty when it covers nothing.
static URect Blend_coverage(const BlendSource &source, const URect &clip) {
	auto x0 = sprt::max(int64_t(source.origin.x), int64_t(clip.x));
	auto y0 = sprt::max(int64_t(source.origin.y), int64_t(clip.y));
	auto x1 = sprt::min(int64_t(source.origin.x) + source.src.width, int64_t(clip.x) + clip.width);
	auto y1 =
			sprt::min(int64_t(source.origin.y) + source.src.height, int64_t(clip.y) + clip.height);
	if (x0 >= x1 || y0 >= y1) {
		return URect();
	}
	return URect(uint32_t(x0), uint32_t(y0), uint32_t(x1 - x0), uint32_t(y1 - y0));
}

static void Blend_source(const raster::Target &target, const BlendSource &source,
		const URect &area) {
	// Byte order is the same for both formats except red and blue, which trade places.
	const bool swap = source.format != target.format;
	const uint32_t srcR = swap ? 2 : 0;
	const uint32_t srcB = swap ? 0 : 2;

	const bool opaque = source.blend == PlaneBlend::Opaque;
	const uint32_t planeAlpha = source.alpha;

	for (uint32_t y = area.y; y < area.y + area.height; ++y) {
		auto dst = target.pixels + size_t(y) * target.stride + size_t(area.x) * 4;
		auto src = source.pixels + size_t(source.src.y + (y - source.origin.y)) * source.stride
				+ size_t(source.src.x + (area.x - source.origin.x)) * 4;

		if (opaque && planeAlpha == 255 && !swap) {
			sprt::memcpy(dst, src, size_t(area.width) * 4);
			continue;
		}

		for (uint32_t x = 0; x < area.width; ++x, dst += 4, src += 4) {
			uint32_t r = src[srcR];
			uint32_t g = src[1];
			uint32_t b = src[srcB];
			uint32_t a = opaque ? 255 : src[3];

			if (planeAlpha != 255) {
				r = Blend_divide255(r * planeAlpha);
				g = Blend_divide255(g * planeAlpha);
				b = Blend_divide255(b * planeAlpha);
				a = Blend_divide255(a * planeAlpha);
			}

			if (a == 255) {
				dst[0] = uint8_t(r);
				dst[1] = uint8_t(g);
				dst[2] = uint8_t(b);
				dst[3] = 255;
				continue;
			}

			const uint32_t inverse = 255 - a;
			dst[0] = uint8_t(sprt::min(255U, r + Blend_divide255(dst[0] * inverse)));
			dst[1] = uint8_t(sprt::min(255U, g + Blend_divide255(dst[1] * inverse)));
			dst[2] = uint8_t(sprt::min(255U, b + Blend_divide255(dst[2] * inverse)));
			dst[3] = uint8_t(sprt::min(255U, a + Blend_divide255(dst[3] * inverse)));
		}
	}
}

void blendPlanes(const raster::Target &target, SpanView<BlendSource> sources,
		const Color4F &background, const URect &clip) {
	if (target.empty() || !Blend_isSupported(target.format)) {
		return;
	}

	auto area = raster::intersectRects(clip, URect(0, 0, target.width, target.height));
	if (area.width == 0 || area.height == 0) {
		return;
	}

	// Everything below an opaque plane that covers the whole area is hidden: start from it.
	size_t first = 0;
	bool covered = false;
	for (size_t i = sources.size(); i > 0; --i) {
		auto &it = sources[i - 1];
		if (!it.pixels || !Blend_isSupported(it.format) || it.blend != PlaneBlend::Opaque
				|| it.alpha != 255) {
			continue;
		}
		auto c = Blend_coverage(it, area);
		if (c.x == area.x && c.y == area.y && c.width == area.width && c.height == area.height) {
			first = i - 1;
			covered = true;
			break;
		}
	}

	if (!covered) {
		raster::fillRect(target, area,
				Color4F(background.r * background.a, background.g * background.a,
						background.b * background.a, background.a));
	}

	for (size_t i = first; i < sources.size(); ++i) {
		auto &it = sources[i];
		if (!it.pixels || !Blend_isSupported(it.format) || it.alpha == 0) {
			continue;
		}
		auto c = Blend_coverage(it, area);
		if (c.width > 0 && c.height > 0) {
			Blend_source(target, it, c);
		}
	}
}

} // namespace stappler::xenolith::compositor
