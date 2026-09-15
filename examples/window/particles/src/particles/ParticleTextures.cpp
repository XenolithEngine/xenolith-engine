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

Rc<Texture> makeSoftCircleTexture(Director *director, uint32_t size) {
	auto cache = director ? director->getResourceCache() : nullptr;
	if (!cache) {
		return nullptr;
	}

	return cache->addExternalImage(toString("particles.soft-circle.", size),
			core::ImageInfo(Extent2(size, size), core::ImageFormat::R8G8B8A8_UNORM,
					core::ImageUsage::Sampled),
			[size](uint8_t *ptr, uint64_t len, const core::ImageData::DataCallback &cb) {
		auto bitmap = makeSoftCircleBitmap(size);
		if (ptr) {
			::__sprt_memcpy(ptr, bitmap.data(), sprt::min(size_t(len), bitmap.size()));
		} else {
			cb(bitmap);
		}
	}, TimeInterval(), TemporaryResourceFlags::CompileWhenAdded);
}

} // namespace stappler::xenolith::examples
