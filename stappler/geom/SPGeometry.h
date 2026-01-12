/**
Copyright (c) 2010-2012 cocos2d-x.org
Copyright (c) 2013-2014 Chukong Technologies
Copyright (c) 2017-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
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

#ifndef CORE_GEOM_SPGEOMETRY_H_
#define CORE_GEOM_SPGEOMETRY_H_

#include "SPVec2.h"
#include "SPVec3.h"

namespace STAPPLER_VERSIONIZED stappler::geom {

struct SP_PUBLIC Metric {
	enum Units : uint8_t {
		Percent,
		Px,
		Em,
		Rem,
		Auto,
		Dpi,
		Dppx,
		Contain, // only for background-size
		Cover, // only for background-size
		Vw,
		Vh,
		VMin,
		VMax
	};

	inline bool isAuto() const { return metric == Units::Auto; }

	inline bool isFixed() const {
		switch (metric) {
		case Units::Px:
		case Units::Em:
		case Units::Rem:
		case Units::Vw:
		case Units::Vh:
		case Units::VMin:
		case Units::VMax: return true; break;
		default: break;
		}
		return false;
	}

	float value = 0.0f;
	Units metric = Units::Auto;

	Metric(float v, Units m) : value(v), metric(m) { }

	Metric() = default;

	bool readStyleValue(StringView r, bool resolutionMetric, bool allowEmptyMetric);

	constexpr bool operator==(const Metric &other) const = default;
	constexpr bool operator!=(const Metric &other) const = default;
};

struct SP_PUBLIC Size2 {
	static const Size2 ZERO;

	float width = 0.0f;
	float height = 0.0f;

	constexpr Size2() = default;
	constexpr Size2(float w, float h) : width(w), height(h) { }
	constexpr Size2(const sprt::Size2 &s) : width(s.width), height(s.height) { }

	template <typename Functor>
	constexpr Size2(const Size2 &v, const Functor &f) : width(f(v.width)), height(f(v.height)) { }

	constexpr Size2(const Size2 &other) = default;
	constexpr explicit Size2(const Vec2 &point) : width(point.x), height(point.y) { }

	constexpr operator Vec2() const { return Vec2(width, height); }

	constexpr operator sprt::Size2() const { return sprt::Size2(width, height); }

	constexpr Size2 &operator=(const Size2 &other) = default;
	constexpr Size2 &operator=(const Vec2 &point) {
		this->width = point.x;
		this->height = point.y;
		return *this;
	}

	constexpr Size2 operator+(const Size2 &right) const {
		return Size2(this->width + right.width, this->height + right.height);
	}
	constexpr Size2 operator-(const Size2 &right) const {
		return Size2(this->width - right.width, this->height - right.height);
	}
	constexpr Size2 operator*(float a) const { return Size2(this->width * a, this->height * a); }
	constexpr Size2 operator/(float a) const { return Size2(this->width / a, this->height / a); }

	constexpr void setSize(float w, float h) { }

	constexpr bool fuzzyEquals(const Size2 &target, float var = sprt::Epsilon<float>) const {
		return (std::fabs(this->width - target.width) < var)
				&& (std::fabs(this->height - target.height) < var);
	}

	constexpr auto operator<=>(const Size2 &) const = default;
};


struct SP_PUBLIC Size3 {
	static const Size3 ZERO;

	float width = 0.0f;
	float height = 0.0f;
	float depth = 0.0f;

	constexpr Size3() = default;
	constexpr Size3(float w, float h, float d) : width(w), height(h), depth(d) { }

	template <typename Functor>
	constexpr Size3(const Size3 &v, const Functor &f)
	: width(f(v.width)), height(f(v.height)), depth(f(v.depth)) { }

	constexpr Size3(const Size3 &other) = default;
	constexpr explicit Size3(const Vec3 &point)
	: width(point.x), height(point.y), depth(point.z) { }

	constexpr operator Vec3() const { return Vec3(width, height, depth); }

	constexpr Size3 &operator=(const Size3 &other) = default;
	constexpr Size3 &operator=(const Vec3 &point) {
		width = point.x;
		height = point.y;
		depth = point.z;
		return *this;
	}

	constexpr Size3 operator+(const Size3 &right) const {
		Size3 ret(*this);
		ret.width += right.width;
		ret.height += right.height;
		ret.depth += right.depth;
		return ret;
	}
	constexpr Size3 operator-(const Size3 &right) const {
		Size3 ret(*this);
		ret.width -= right.width;
		ret.height -= right.height;
		ret.depth -= right.depth;
		return ret;
	}
	constexpr Size3 operator*(float a) const {
		Size3 ret(*this);
		ret.width *= a;
		ret.height *= a;
		ret.depth *= a;
		return ret;
	}
	constexpr Size3 operator/(float a) const {
		Size3 ret(*this);
		ret.width /= a;
		ret.height /= a;
		ret.depth /= a;
		return ret;
	}

	constexpr bool fuzzyEquals(const Size3 &target, float var = sprt::Epsilon<float>) const {
		return (std::fabs(this->width - target.width) < var)
				&& (std::fabs(this->height - target.height) < var)
				&& (std::fabs(this->depth - target.depth) < var);
	}

	constexpr auto operator<=>(const Size3 &) const = default;
};


struct SP_PUBLIC Extent2 {
	static const Extent2 ZERO;

	uint32_t width = 0;
	uint32_t height = 0;

	constexpr Extent2() = default;
	constexpr Extent2(uint32_t w, uint32_t h) : width(w), height(h) { }
	constexpr Extent2(const sprt::Extent2 &e) : width(e.width), height(e.height) { }

	constexpr Extent2(const Extent2 &other) = default;
	constexpr Extent2 &operator=(const Extent2 &other) = default;

	constexpr explicit Extent2(const Size2 &size) : width(size.width), height(size.height) { }
	constexpr explicit Extent2(const Vec2 &point) : width(point.x), height(point.y) { }

	constexpr operator sprt::Extent2() const { return sprt::Extent2(width, height); }

	constexpr Extent2 &operator=(const Size2 &size) {
		width = size.width;
		height = size.width;
		return *this;
	}
	constexpr Extent2 &operator=(const Vec2 &other) {
		width = other.x;
		height = other.y;
		return *this;
	}

	constexpr auto operator<=>(const Extent2 &) const = default;

	constexpr operator Size2() const { return Size2(width, height); }
};


struct SP_PUBLIC Extent3 {
	static const Extent3 ZERO;

	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t depth = 0;

	constexpr Extent3() = default;
	constexpr Extent3(uint32_t w, uint32_t h, uint32_t d) : width(w), height(h), depth(d) { }
	constexpr Extent3(Extent2 e, uint32_t d) : width(e.width), height(e.height), depth(d) { }
	constexpr Extent3(const sprt::Extent3 &e) : width(e.width), height(e.height), depth(e.depth) { }

	constexpr Extent3(const Extent3 &other) = default;
	constexpr Extent3 &operator=(const Extent3 &other) = default;

	constexpr Extent3(const Extent2 &other) : width(other.width), height(other.height), depth(1) { }
	constexpr Extent3 &operator=(const Extent2 &other) {
		width = other.width;
		height = other.height;
		depth = 1;
		return *this;
	}

	constexpr explicit Extent3(const Size3 &size)
	: width(size.width), height(size.height), depth(size.depth) { }
	constexpr explicit Extent3(const Vec3 &point)
	: width(point.x), height(point.y), depth(point.z) { }

	constexpr operator sprt::Extent3() const { return sprt::Extent3(width, height, depth); }

	constexpr Extent3 &operator=(const Size3 &size) {
		width = size.width;
		height = size.width;
		depth = size.depth;
		return *this;
	}
	constexpr Extent3 &operator=(const Vec3 &other) {
		width = other.x;
		height = other.y;
		depth = other.z;
		return *this;
	}

	constexpr auto operator<=>(const Extent3 &) const = default;

	constexpr operator Size3() const { return Size3(width, height, depth); }
};


struct SP_PUBLIC Rect {
	static const Rect ZERO;

	Vec2 origin;
	Size2 size;

	constexpr Rect() = default;
	constexpr Rect(float x, float y, float width, float height)
	: origin(x, y), size(width, height) { }
	constexpr Rect(const Vec2 &o, const Size2 &s) : origin(o), size(s) { }

	constexpr Rect(const sprt::Rect &r) : origin(r.origin), size(r.size) { }

	template <typename Functor>
	constexpr Rect(const Rect &v, const Functor &f)
	: origin(Vec2(v.origin, f)), size(Size2(v.size, f)) { }

	constexpr Rect(const Rect &other) = default;

	constexpr Rect &operator=(const Rect &other) = default;

	constexpr operator sprt::Rect() const { return sprt::Rect{origin, size}; }

	constexpr float getMaxX() const { return origin.x + size.width; }
	constexpr float getMidX() const { return origin.x + size.width / 2.0f; }
	constexpr float getMinX() const { return origin.x; }
	constexpr float getMaxY() const { return origin.y + size.height; }
	constexpr float getMidY() const { return origin.y + size.height / 2.0f; }
	constexpr float getMinY() const { return origin.y; }

	constexpr bool equals(const Rect &rect) const {
		return (origin == rect.origin) && (size == rect.size);
	}

	bool containsPoint(const Vec2 &point, float padding = 0.0f) const;
	bool intersectsRect(const Rect &rect) const;
	bool intersectsCircle(const Vec2 &center, float radius) const;

	/** Get the min rect which can contain this and rect. */
	Rect unionWithRect(const Rect &rect) const;

	/** Compute the min rect which can contain this and rect, assign it to this. */
	void merge(const Rect &rect);

	constexpr auto operator<=>(const Rect &) const = default;
};

struct SP_PUBLIC UVec2 {
	static constexpr size_t DIMENSIONS = 2;

	static UVec2 convertFromPacked(uint64_t v) {
		UVec2 ret;
		memcpy(&ret, &v, sizeof(uint64_t));
		return ret;
	}

	uint32_t x;
	uint32_t y;

	constexpr auto operator<=>(const UVec2 &) const = default;
};

struct SP_PUBLIC IVec2 {
	static constexpr size_t DIMENSIONS = 2;

	int32_t x;
	int32_t y;

	constexpr auto operator<=>(const IVec2 &) const = default;
};

struct SP_PUBLIC UVec3 {
	static constexpr size_t DIMENSIONS = 3;

	uint32_t x;
	uint32_t y;
	uint32_t z;

	constexpr auto operator<=>(const UVec3 &) const = default;
};

struct SP_PUBLIC IVec3 {
	static constexpr size_t DIMENSIONS = 2;

	int32_t x;
	int32_t y;
	int32_t z;

	constexpr auto operator<=>(const IVec3 &) const = default;
};

struct SP_PUBLIC UVec4 {
	static constexpr size_t DIMENSIONS = 4;

	uint32_t x;
	uint32_t y;
	uint32_t z;
	uint32_t w;

	constexpr auto operator<=>(const UVec4 &) const = default;
};

struct SP_PUBLIC IVec4 {
	static constexpr size_t DIMENSIONS = 4;

	int32_t x;
	int32_t y;
	int32_t z;
	int32_t w;

	constexpr auto operator<=>(const IVec4 &) const = default;
};

struct SP_PUBLIC URect {
	uint32_t x = 0;
	uint32_t y = 0;
	uint32_t width = 0;
	uint32_t height = 0;

	URect() = default;
	URect(const UVec2 &origin, const Extent2 &size)
	: x(origin.x), y(origin.y), width(size.width), height(size.height) { }
	URect(uint32_t x, uint32_t y, uint32_t w, uint32_t h) : x(x), y(y), width(w), height(h) { }

	explicit URect(const Rect &rect)
	: x(std::ceil(rect.origin.x))
	, y(std::ceil(rect.origin.y))
	, width(std::floor(rect.size.width))
	, height(std::floor(rect.size.height)) { }

	constexpr UVec2 origin() const { return UVec2{x, y}; }

	constexpr float getMaxX() const { return x + width; }
	constexpr float getMidX() const { return x + width / 2.0f; }
	constexpr float getMinX() const { return x; }
	constexpr float getMaxY() const { return y + height; }
	constexpr float getMidY() const { return y + height / 2.0f; }
	constexpr float getMinY() const { return y; }

	bool containsPoint(const UVec2 &point) const;
	bool intersectsRect(const URect &rect) const;

	constexpr auto operator<=>(const URect &) const = default;
};

struct SP_PUBLIC IRect {
	int32_t x = 0;
	int32_t y = 0;
	uint32_t width = 0;
	uint32_t height = 0;

	IRect() = default;
	IRect(const IVec2 &origin, const Extent2 &size)
	: x(origin.x), y(origin.y), width(size.width), height(size.height) { }
	IRect(int32_t x, int32_t y, uint32_t w, uint32_t h) : x(x), y(y), width(w), height(h) { }

	explicit IRect(const Rect &rect)
	: x(std::ceil(rect.origin.x))
	, y(std::ceil(rect.origin.y))
	, width(std::floor(rect.size.width))
	, height(std::floor(rect.size.height)) { }

	constexpr IVec2 origin() const { return IVec2{x, y}; }

	constexpr float getMaxX() const { return x + width; }
	constexpr float getMidX() const { return x + width / 2.0f; }
	constexpr float getMinX() const { return x; }
	constexpr float getMaxY() const { return y + height; }
	constexpr float getMidY() const { return y + height / 2.0f; }
	constexpr float getMinY() const { return y; }

	bool containsPoint(const IVec2 &point) const;
	bool intersectsRect(const IRect &rect) const;

	constexpr auto operator<=>(const IRect &) const = default;
};

constexpr Size2 Size2::ZERO(0.0f, 0.0f);
constexpr Size3 Size3::ZERO = Size3(0.0f, 0.0f, 0.0f);
constexpr Extent2 Extent2::ZERO = Extent2(0, 0);
constexpr Extent3 Extent3::ZERO = Extent3(0, 0, 0);
constexpr Rect Rect::ZERO = Rect(0.0f, 0.0f, 0.0f, 0.0f);

SP_PUBLIC Rect TransformRect(const Rect &rect, const Mat4 &transform);

inline const CallbackStream &operator<<(const CallbackStream &stream, const Rect &obj) {
	stream << "Rect(x:" << obj.origin.x << " y:" << obj.origin.y << " width:" << obj.size.width
		   << " height:" << obj.size.height << ");";
	return stream;
}

inline std::ostream &operator<<(std::ostream &stream, const Rect &obj) {
	stream << "Rect(x:" << obj.origin.x << " y:" << obj.origin.y << " width:" << obj.size.width
		   << " height:" << obj.size.height << ");";
	return stream;
}

inline const CallbackStream &operator<<(const CallbackStream &stream, const URect &obj) {
	stream << "URect(x:" << obj.x << " y:" << obj.y << " width:" << obj.width
		   << " height:" << obj.height << ");";
	return stream;
}

inline std::ostream &operator<<(std::ostream &stream, const URect &obj) {
	stream << "URect(x:" << obj.x << " y:" << obj.y << " width:" << obj.width
		   << " height:" << obj.height << ");";
	return stream;
}

inline const CallbackStream &operator<<(const CallbackStream &stream, const Size2 &obj) {
	stream << "Size2(width:" << obj.width << " height:" << obj.height << ");";
	return stream;
}

inline std::ostream &operator<<(std::ostream &stream, const Size2 &obj) {
	stream << "Size2(width:" << obj.width << " height:" << obj.height << ");";
	return stream;
}

inline const CallbackStream &operator<<(const CallbackStream &stream, const Size3 &obj) {
	stream << "Size3(width:" << obj.width << " height:" << obj.height << " depth:" << obj.depth
		   << ");";
	return stream;
}

inline std::ostream &operator<<(std::ostream &stream, const Size3 &obj) {
	stream << "Size3(width:" << obj.width << " height:" << obj.height << " depth:" << obj.depth
		   << ");";
	return stream;
}

inline const CallbackStream &operator<<(const CallbackStream &stream, const Extent2 &obj) {
	stream << "Extent2(width:" << obj.width << " height:" << obj.height << ");";
	return stream;
}

inline std::ostream &operator<<(std::ostream &stream, const Extent2 &obj) {
	stream << "Extent2(width:" << obj.width << " height:" << obj.height << ");";
	return stream;
}

inline const CallbackStream &operator<<(const CallbackStream &stream, const Extent3 &obj) {
	stream << "Extent3(width:" << obj.width << " height:" << obj.height << " depth:" << obj.depth
		   << ");";
	return stream;
}

inline std::ostream &operator<<(std::ostream &stream, const Extent3 &obj) {
	stream << "Extent3(width:" << obj.width << " height:" << obj.height << " depth:" << obj.depth
		   << ");";
	return stream;
}

inline const CallbackStream &operator<<(const CallbackStream &stream, const UVec2 &obj) {
	stream << "UVec2(x:" << obj.x << " y:" << obj.y << ");";
	return stream;
}

inline std::ostream &operator<<(std::ostream &stream, const UVec2 &obj) {
	stream << "UVec2(x:" << obj.x << " y:" << obj.y << ");";
	return stream;
}

inline const CallbackStream &operator<<(const CallbackStream &stream, const UVec3 &obj) {
	stream << "UVec3(x:" << obj.x << " y:" << obj.y << " z:" << obj.z << ");";
	return stream;
}

inline std::ostream &operator<<(std::ostream &stream, const UVec3 &obj) {
	stream << "UVec3(x:" << obj.x << " y:" << obj.y << " z:" << obj.z << ");";
	return stream;
}

} // namespace stappler::geom

#endif /* CORE_GEOM_SPGEOMETRY_H_ */
