/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUX_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUX_H_

#include <sprt/runtime/window/input.h>
#include <sprt/runtime/window/window_info.h>
#include <sprt/c/__sprt_math.h>

#if SPRT_LINUX

namespace sprt::window {

SPRT_API SpanView<StringView> getCursorNames(WindowCursor);

SPRT_API InputKeyCode getKeysymCode(uint32_t sym);

// Generates linear bitmap <size> with shadow data
template <typename Callback>
static void makeShadowVector(const Callback &cb, uint32_t size) {
	static_assert(sprt::is_invocable_v<Callback, uint32_t, float>,
			"Callback<void(uint32_t, float)> expected");

	const float sigma = __sprt_sqrtf((size * size) / (-2.0f * __sprt_logf(1.0f / 255.0f)));
	const float sigma_v = -1.0f / (2.0f * sigma * sigma);

	for (uint32_t j = 0; j < size; j++) { cb(j, __sprt_expf((j * j) * sigma_v)); }
}

// Generates bitmap <width, width> with shadow, possibly rounded for inset radius
template <typename Callback>
inline void makeShadowCorner(const Callback &cb, uint32_t width, uint32_t inset) {
	static_assert(sprt::is_invocable_v<Callback, uint32_t, uint32_t, float>,
			"Callback<void(uint32_t, uint32_t, float)> expected");

	auto size = width - inset;

	const float sigma = __sprt_sqrtf((size * size) / (-2.0f * __sprt_logf(1.0f / 255.0f)));
	const float sigma_v = -1.0f / (2.0f * sigma * sigma);

	for (uint32_t i = 0; i < width; i++) {
		for (uint32_t j = 0; j < width; j++) {
			float dist = __sprt_sqrtf((i * i) + (j * j));
			float tmp = 0.0f;
			if (dist <= inset) {
				tmp = 1.0f;
			} else if (dist > size + inset) {
				tmp = 0.0f;
			} else {
				dist = dist - inset;
				tmp = __sprt_expf((dist * dist) * sigma_v);
			}

			cb(i, j, tmp);
		}
	}
}

// Generates bitmap <size, size> with rounded corner
template <typename Callback>
static void makeRoundedCorners(const Callback &cb, uint32_t size) {
	static_assert(sprt::is_invocable_v<Callback, uint32_t, uint32_t, float>,
			"Callback<void(uint32_t, uint32_t, float)> expected");
	for (uint32_t i = 0; i < size; i++) {
		for (uint32_t j = 0; j < size; j++) {
			auto u = size - i - 1;
			auto v = size - j - 1;
			float dist = __sprt_sqrtf((u * u) + (v * v));
			if (dist >= size) {
				cb(i, j, 0.0f);
			} else {
				cb(i, j, 1.0f);
			}
		}
	}
}

} // namespace sprt::window

#endif

#endif /* CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUX_H_ */
