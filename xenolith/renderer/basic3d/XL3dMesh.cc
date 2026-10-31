/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#include "XL3dMesh.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d {

static Vertex3d Mesh3d_vertex(const Vec3 &pos, const Vec3 &normal, const Vec3 &tangent, float u,
		float v) {
	Vertex3d ret;
	ret.pos = Vec4(pos.x, pos.y, pos.z, 1.0f);
	ret.normal = Vec4(normal.x, normal.y, normal.z, 0.0f);
	ret.tangent = Vec4(tangent.x, tangent.y, tangent.z, 1.0f);
	ret.attr = Vec4(u, v, 0.0f, 0.0f);
	return ret;
}

Rc<Mesh3d> Mesh3d::makeBox(const Vec3 &h) {
	struct Face {
		Vec3 normal;
		Vec3 tangent;
	};

	// each face's bitangent is cross(normal, tangent), so tangent, bitangent, normal is a right
	// frame and the corners below go counter-clockwise seen from outside
	const Face faces[] = {
		{Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f)},
		{Vec3(-1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)},
		{Vec3(0.0f, 1.0f, 0.0f), Vec3(-1.0f, 0.0f, 0.0f)},
		{Vec3(0.0f, -1.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f)},
		{Vec3(0.0f, 0.0f, 1.0f), Vec3(1.0f, 0.0f, 0.0f)},
		{Vec3(0.0f, 0.0f, -1.0f), Vec3(-1.0f, 0.0f, 0.0f)},
	};

	Vector<Vertex3d> vertexes;
	Vector<uint32_t> indexes;
	for (auto &f : faces) {
		Vec3 b;
		Vec3::cross(f.normal, f.tangent, &b);
		auto base = uint32_t(vertexes.size());
		const float corners[4][2] = {{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}};
		for (auto &c : corners) {
			auto p = f.normal + f.tangent * c[0] + b * c[1];
			vertexes.emplace_back(Mesh3d_vertex(Vec3(p.x * h.x, p.y * h.y, p.z * h.z), f.normal,
					f.tangent, (c[0] + 1.0f) * 0.5f, (c[1] + 1.0f) * 0.5f));
		}
		for (auto i : {0, 1, 2, 0, 2, 3}) { indexes.emplace_back(base + i); }
	}

	return Rc<Mesh3d>::create(sp::move(vertexes), sp::move(indexes));
}

Rc<Mesh3d> Mesh3d::makePlane(float size, float uvRepeat) {
	auto s = size * 0.5f;
	Vec3 n(0.0f, 0.0f, 1.0f);
	Vec3 t(1.0f, 0.0f, 0.0f);

	Vector<Vertex3d> vertexes{
		Mesh3d_vertex(Vec3(-s, -s, 0.0f), n, t, 0.0f, 0.0f),
		Mesh3d_vertex(Vec3(s, -s, 0.0f), n, t, uvRepeat, 0.0f),
		Mesh3d_vertex(Vec3(s, s, 0.0f), n, t, uvRepeat, uvRepeat),
		Mesh3d_vertex(Vec3(-s, s, 0.0f), n, t, 0.0f, uvRepeat),
	};
	Vector<uint32_t> indexes{0, 1, 2, 0, 2, 3};
	return Rc<Mesh3d>::create(sp::move(vertexes), sp::move(indexes));
}

Rc<Mesh3d> Mesh3d::makeTorus(float major, float minor, uint32_t segments, uint32_t sides) {
	segments = sprt::max(segments, uint32_t(3));
	sides = sprt::max(sides, uint32_t(3));

	const float twoPi = 6.28318530718f;

	// u goes around the ring, v around the tube; d/du x d/dv points outward, so the quad
	// (i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1) winds counter-clockwise seen from outside
	Vector<Vertex3d> vertexes;
	for (uint32_t i = 0; i <= segments; ++i) {
		auto u = twoPi * float(i) / float(segments);
		auto cu = cosf(u), su = sinf(u);
		for (uint32_t j = 0; j <= sides; ++j) {
			auto v = twoPi * float(j) / float(sides);
			auto cv = cosf(v), sv = sinf(v);
			Vec3 normal(cu * cv, su * cv, sv);
			Vec3 pos((major + minor * cv) * cu, (major + minor * cv) * su, minor * sv);
			Vec3 tangent(-su, cu, 0.0f);
			vertexes.emplace_back(Mesh3d_vertex(pos, normal, tangent,
					float(i) / float(segments) * 4.0f, float(j) / float(sides)));
		}
	}

	Vector<uint32_t> indexes;
	auto at = [&](uint32_t i, uint32_t j) { return i * (sides + 1) + j; };
	for (uint32_t i = 0; i < segments; ++i) {
		for (uint32_t j = 0; j < sides; ++j) {
			for (auto idx : {at(i, j), at(i + 1, j), at(i + 1, j + 1), at(i, j), at(i + 1, j + 1),
					 at(i, j + 1)}) {
				indexes.emplace_back(idx);
			}
		}
	}

	return Rc<Mesh3d>::create(sp::move(vertexes), sp::move(indexes));
}

bool Mesh3d::init(Vector<Vertex3d> &&vertexes, Vector<uint32_t> &&indexes) {
	_vertexes = sp::move(vertexes);
	_indexes = sp::move(indexes);

	_min = Vec3(maxOf<float>(), maxOf<float>(), maxOf<float>());
	_max = -_min;
	for (auto &it : _vertexes) {
		_min = Vec3(sprt::min(_min.x, it.pos.x), sprt::min(_min.y, it.pos.y),
				sprt::min(_min.z, it.pos.z));
		_max = Vec3(sprt::max(_max.x, it.pos.x), sprt::max(_max.y, it.pos.y),
				sprt::max(_max.z, it.pos.z));
	}
	if (_vertexes.empty()) {
		_min = _max = Vec3::ZERO;
	}
	return true;
}

} // namespace stappler::xenolith::basic3d
