/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons whom the Software is
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

#include "XLCommon.h" // IWYU pragma: keep

#include "particles/ParticleTextures.h"
#include "XLDirector.h"
#include "XLResourceCache.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

static Bytes makeSoftCircleBitmap(uint32_t size) {
	Bytes ret;
	ret.resize(size_t(size) * size * 4);

	const float center = float(size) / 2.0f;
	for (uint32_t y = 0; y < size; ++y) {
		for (uint32_t x = 0; x < size; ++x) {
			const float dx = (float(x) + 0.5f - center) / center;
			const float dy = (float(y) + 0.5f - center) / center;
			const float d = sprt::sqrt(dx * dx + dy * dy);
			const float t = sprt::clamp(1.0f - d, 0.0f, 1.0f);
			const float a = t * t * (3.0f - 2.0f * t);

			auto px = ret.data() + (size_t(y) * size + x) * 4;
			px[0] = 255;
			px[1] = 255;
			px[2] = 255;
			px[3] = uint8_t(sprt::round(a * 255.0f));
		}
	}
	return ret;
}

static constexpr Color3B s_frameSheetColors[16] = {
	Color3B(255, 0, 0),
	Color3B(0, 255, 0),
	Color3B(0, 0, 255),
	Color3B(255, 255, 0),
	Color3B(255, 0, 255),
	Color3B(0, 255, 255),
	Color3B(255, 128, 0),
	Color3B(128, 0, 255),
	Color3B(0, 128, 255),
	Color3B(255, 0, 128),
	Color3B(128, 255, 0),
	Color3B(0, 255, 128),
	Color3B(128, 128, 255),
	Color3B(255, 128, 128),
	Color3B(128, 255, 128),
	Color3B(255, 255, 255),
};

Color3B getFrameSheetColor(uint32_t index) { return s_frameSheetColors[index % 16]; }

static Bytes makeFrameSheetBitmap(uint32_t cell) {
	const uint32_t size = cell * 4;
	Bytes ret;
	ret.resize(size_t(size) * size * 4);

	for (uint32_t y = 0; y < size; ++y) {
		for (uint32_t x = 0; x < size; ++x) {
			// the first row of the bitmap is the top of the image
			auto color = getFrameSheetColor((y / cell) * 4 + x / cell);
			auto px = ret.data() + (size_t(y) * size + x) * 4;
			px[0] = color.r;
			px[1] = color.g;
			px[2] = color.b;
			px[3] = 255;
		}
	}
	return ret;
}

// A white box with a one-texel soft edge
static Bytes makeSquareBitmap(uint32_t size) {
	Bytes ret;
	ret.resize(size_t(size) * size * 4);
	for (uint32_t y = 0; y < size; ++y) {
		for (uint32_t x = 0; x < size; ++x) {
			auto edge = sprt::min(sprt::min(x, y), sprt::min(size - 1 - x, size - 1 - y));
			auto px = ret.data() + (size_t(y) * size + x) * 4;
			px[0] = 255;
			px[1] = 255;
			px[2] = 255;
			px[3] = (edge == 0) ? 128 : 255;
		}
	}
	return ret;
}

// A soft ellipse along Y, so AlignWithVelocity stretches it along the motion
static Bytes makeSparkBitmap(uint32_t width, uint32_t height) {
	Bytes ret;
	ret.resize(size_t(width) * height * 4);
	for (uint32_t y = 0; y < height; ++y) {
		for (uint32_t x = 0; x < width; ++x) {
			const float dx = (float(x) + 0.5f - width / 2.0f) / (width / 2.0f);
			const float dy = (float(y) + 0.5f - height / 2.0f) / (height / 2.0f);
			const float t = sprt::clamp(1.0f - sprt::sqrt(dx * dx + dy * dy), 0.0f, 1.0f);
			auto px = ret.data() + (size_t(y) * width + x) * 4;
			px[0] = 255;
			px[1] = 255;
			px[2] = 255;
			px[3] = uint8_t(sprt::round(t * t * 255.0f));
		}
	}
	return ret;
}

static Rc<Texture> makeTexture(Director *director, StringView key, uint32_t width, uint32_t height,
		Function<Bytes()> &&make) {
	auto cache = director ? director->getResourceCache() : nullptr;
	if (!cache) {
		return nullptr;
	}

	return cache->addExternalImage(key,
			core::ImageInfo(Extent2(width, height), core::ImageFormat::R8G8B8A8_UNORM,
					core::ImageUsage::Sampled),
			[make = sp::move(make)](uint8_t *ptr, uint64_t len,
					const core::ImageData::DataCallback &cb) {
		auto bitmap = make();
		if (ptr) {
			::__sprt_memcpy(ptr, bitmap.data(), sprt::min(size_t(len), bitmap.size()));
		} else {
			cb(bitmap);
		}
	},
			TimeInterval::seconds(600), TemporaryResourceFlags::CompileWhenAdded);
}

Rc<Texture> makeSoftCircleTexture(Director *director, uint32_t size) {
	return makeTexture(director, toString("particles.soft-circle.", size), size, size,
			[size]() { return makeSoftCircleBitmap(size); });
}

Rc<Texture> makeFrameSheetTexture(Director *director, uint32_t cellSize) {
	return makeTexture(director, toString("particles.frame-sheet.", cellSize), cellSize * 4,
			cellSize * 4, [cellSize]() { return makeFrameSheetBitmap(cellSize); });
}

Rc<Texture> makeSquareTexture(Director *director, uint32_t size) {
	return makeTexture(director, toString("particles.square.", size), size, size,
			[size]() { return makeSquareBitmap(size); });
}

Rc<Texture> makeSparkTexture(Director *director, uint32_t width, uint32_t height) {
	return makeTexture(director, toString("particles.spark.", width, "x", height), width, height,
			[width, height]() { return makeSparkBitmap(width, height); });
}

} // namespace stappler::xenolith::examples
