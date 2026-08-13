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

#include <sprt/runtime/window/window_icon.h>

namespace sprt::window {

const WindowIconImage *WindowIcon::getBestImage(uint32_t size) const {
	const WindowIconImage *best = nullptr;
	const WindowIconImage *largest = nullptr;
	for (auto &it : images) {
		if (!it.isValid()) {
			continue;
		}
		if (!largest || it.extent.width > largest->extent.width) {
			largest = &it;
		}
		// Smallest image at or above the request: upscaling is always worse than downscaling, and
		// the WM/OS does the final scale itself.
		if (it.extent.width >= size && (!best || it.extent.width < best->extent.width)) {
			best = &it;
		}
	}
	return best ? best : largest;
}

// Premultiply one straight-alpha channel. Rounded, not truncated: truncation drifts a 50%-alpha
// white down to 127 and shows up as a visible grey fringe once the WM composites the icon.
static inline uint8_t WindowIcon_premultiply(uint8_t c, uint8_t a) {
	return uint8_t((uint32_t(c) * uint32_t(a) + 127) / 255);
}

void packIconArgbPremultiplied(const WindowIconImage &img, uint32_t *out) {
	auto count = size_t(img.extent.width) * size_t(img.extent.height);
	auto src = img.data.data();
	for (size_t i = 0; i < count; ++i, src += 4) {
		auto a = src[3];
		out[i] = (uint32_t(a) << 24) | (uint32_t(WindowIcon_premultiply(src[0], a)) << 16)
				| (uint32_t(WindowIcon_premultiply(src[1], a)) << 8)
				| uint32_t(WindowIcon_premultiply(src[2], a));
	}
}

void packIconBgraPremultipliedBottomUp(const WindowIconImage &img, uint8_t *out) {
	auto width = size_t(img.extent.width);
	auto height = size_t(img.extent.height);
	auto stride = width * 4;
	for (size_t y = 0; y < height; ++y) {
		auto src = img.data.data() + y * stride;
		// Bottom-up: source row 0 is the top, and a DDB's row 0 is the bottom.
		auto dst = out + (height - 1 - y) * stride;
		for (size_t x = 0; x < width; ++x, src += 4, dst += 4) {
			auto a = src[3];
			dst[0] = WindowIcon_premultiply(src[2], a);
			dst[1] = WindowIcon_premultiply(src[1], a);
			dst[2] = WindowIcon_premultiply(src[0], a);
			dst[3] = a;
		}
	}
}

} // namespace sprt::window
