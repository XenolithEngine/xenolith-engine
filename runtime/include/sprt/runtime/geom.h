/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_GEOM_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_GEOM_H_

#include <sprt/runtime/math.h>

namespace sprt {

struct Vec2 {
	float x;
	float y;

	constexpr bool operator==(const Vec2 &) const = default;
	constexpr bool operator!=(const Vec2 &) const = default;
};

struct Size2 {
	float width;
	float height;

	constexpr bool operator==(const Size2 &) const = default;
	constexpr bool operator!=(const Size2 &) const = default;
};

struct Rect {
	Vec2 origin;
	Size2 size;

	constexpr bool operator==(const Rect &) const = default;
	constexpr bool operator!=(const Rect &) const = default;
};

struct Extent2 {
	uint32_t width;
	uint32_t height;

	constexpr bool operator==(const Extent2 &) const = default;
	constexpr bool operator!=(const Extent2 &) const = default;
};

struct Extent3 {
	uint32_t width;
	uint32_t height;
	uint32_t depth;

	constexpr bool operator==(const Extent3 &) const = default;
	constexpr bool operator!=(const Extent3 &) const = default;
};

struct IVec2 {
	int32_t x;
	int32_t y;

	constexpr bool operator==(const IVec2 &) const = default;
	constexpr bool operator!=(const IVec2 &) const = default;
};

struct IRect {
	int32_t x;
	int32_t y;
	uint32_t width = 0;
	uint32_t height = 0;

	constexpr bool operator==(const IRect &) const = default;
	constexpr bool operator!=(const IRect &) const = default;
};

struct Padding {
	float top = 0.0f;
	float right = 0.0f;
	float bottom = 0.0f;
	float left = 0.0f;

	constexpr bool operator==(const Padding &) const = default;
	constexpr bool operator!=(const Padding &) const = default;
};

struct Color3B {
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;

	constexpr bool operator==(const Color3B &) const = default;
	constexpr bool operator!=(const Color3B &) const = default;
};

struct Color4B {
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
	uint8_t a = 0;

	constexpr bool operator==(const Color4B &) const = default;
	constexpr bool operator!=(const Color4B &) const = default;
};

struct Color4S {
	uint16_t r = 0;
	uint16_t g = 0;
	uint16_t b = 0;
	uint16_t a = 0;

	constexpr bool operator==(const Color4S &) const = default;
	constexpr bool operator!=(const Color4S &) const = default;
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_GEOM_H_
