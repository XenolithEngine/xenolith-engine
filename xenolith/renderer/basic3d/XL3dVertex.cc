/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#include "XL3dVertex.h"
#include "XL3d.h"

#include <sprt/runtime/utils/halffloat.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d {

const VertexFormat3d Vertex3dFormat{"Vertex3d", sizeof(Vertex3d), 16, 0};

static uint32_t XL3dVertex_snorm16(float v) {
	auto c = sprt::clamp(v, -1.0f, 1.0f);
	auto i = int32_t(c >= 0.0f ? c * 32767.0f + 0.5f : c * 32767.0f - 0.5f);
	return uint32_t(uint16_t(int16_t(i)));
}

static float XL3dVertex_unsnorm16(uint32_t v) {
	return sprt::clamp(float(int16_t(uint16_t(v))) / 32767.0f, -1.0f, 1.0f);
}

static float XL3dVertex_signNotZero(float v) { return v >= 0.0f ? 1.0f : -1.0f; }

uint32_t packOct16(const Vec3 &n) {
	auto l1 = sprt::abs(n.x) + sprt::abs(n.y) + sprt::abs(n.z);
	if (l1 <= 0.0f) {
		return XL3dVertex_snorm16(0.0f) | (XL3dVertex_snorm16(0.0f) << 16);
	}
	auto x = n.x / l1;
	auto y = n.y / l1;
	// the lower hemisphere folds over the diagonals
	if (n.z < 0.0f) {
		auto fx = (1.0f - sprt::abs(y)) * XL3dVertex_signNotZero(x);
		auto fy = (1.0f - sprt::abs(x)) * XL3dVertex_signNotZero(y);
		x = fx;
		y = fy;
	}
	return XL3dVertex_snorm16(x) | (XL3dVertex_snorm16(y) << 16);
}

Vec3 unpackOct16(uint32_t v) {
	Vec3 n(XL3dVertex_unsnorm16(v & 0xFFFF), XL3dVertex_unsnorm16(v >> 16), 0.0f);
	n.z = 1.0f - sprt::abs(n.x) - sprt::abs(n.y);
	auto t = sprt::max(-n.z, 0.0f);
	n.x += n.x >= 0.0f ? -t : t;
	n.y += n.y >= 0.0f ? -t : t;
	n.normalize();
	return n;
}

uint32_t packHalf2(const Vec2 &v) {
	return uint32_t(sprt::halffloat::encode(v.x)) | (uint32_t(sprt::halffloat::encode(v.y)) << 16);
}

Vec2 unpackHalf2(uint32_t v) {
	return Vec2(sprt::halffloat::decode(uint16_t(v & 0xFFFF)),
			sprt::halffloat::decode(uint16_t(v >> 16)));
}

uint32_t packUnorm8x4(const Vec4 &v) {
	auto b = [](float f) { return uint32_t(sprt::clamp(f, 0.0f, 1.0f) * 255.0f + 0.5f); };
	return b(v.x) | (b(v.y) << 8) | (b(v.z) << 16) | (b(v.w) << 24);
}

Vec4 unpackUnorm8x4(uint32_t v) {
	return Vec4(float(v & 0xFF) / 255.0f, float((v >> 8) & 0xFF) / 255.0f,
			float((v >> 16) & 0xFF) / 255.0f, float(v >> 24) / 255.0f);
}

} // namespace stappler::xenolith::basic3d
