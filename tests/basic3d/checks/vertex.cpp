/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#include "XLCommon.h"
#include "XL3dMesh.h"
#include "XL3dMeshPack.h"
#include "Basic3dTestData.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d {

using test::check;
using test::near;

static bool nearVec(const Vec3 &a, const Vec3 &b, float eps) {
	return near(a.x, b.x, eps) && near(a.y, b.y, eps) && near(a.z, b.z, eps);
}

// What xlkdl gpu makes of a vertex (glsl/basic3d-test.kdl): its format, its packed fields, and a
// mesh of it, alone and in a pack with meshes of Vertex3d
void performVertexTests() {
	using checks::Packed3dTestVertex;

	auto &format = Packed3dTestVertex::Format;
	check(format.stride == 32 && format.align == 16 && format.positionOffset == 0
					&& StringView(format.name) == "Packed3dTest",
			"vertex: the format is the record's std430 stride, alignment and position",
			toString(format.stride, " ", format.align, " ", format.positionOffset));
	check(Vertex3dFormat.stride == 64 && Vertex3dFormat.positionOffset == 0,
			"vertex: Vertex3d's format is 64 bytes");

	// the packed fields come back within what they keep
	bool octs = true;
	for (auto &n : {Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f), Vec3(0.0f, -1.0f, 0.0f),
			 Vec3(0.6f, -0.48f, 0.64f), Vec3(-0.36f, 0.48f, -0.8f)}) {
		octs = octs && nearVec(unpackOct16(packOct16(n)), n, 1.0e-3f);
	}
	check(octs, "vertex: a unit vector survives oct16 within 1e-3, both hemispheres");
	auto uv = unpackHalf2(packHalf2(Vec2(0.25f, -3.5f)));
	check(uv == Vec2(0.25f, -3.5f), "vertex: halves keep what they can hold exactly");
	auto c = unpackUnorm8x4(packUnorm8x4(Vec4(0.0f, 1.0f, 0.5f, 2.0f)));
	check(near(c.x, 0.0f, 1.0e-6f) && near(c.y, 1.0f, 1.0e-6f) && near(c.z, 128.0f / 255.0f, 1.0e-6f)
					&& near(c.w, 1.0f, 1.0e-6f),
			"vertex: unorm8x4 rounds to the nearest of 255 steps and clamps");

	Packed3dTestVertex v{
		.pos = Vec3(1.0f, -2.0f, 3.0f),
		.normal = Vec3(0.0f, 0.6f, 0.8f),
		.uv = Vec2(0.5f, 0.75f),
		.color = Vec4(1.0f, 0.0f, 0.0f, 1.0f),
		.layer = 2.0f,
	};
	auto p = v.pack();
	check(p.pos == v.pos && p.normal == packOct16(v.normal) && p.uv == packHalf2(v.uv)
					&& p.color == 0xFF0000FFu && p.layer == 2.0f && p.padding == 0,
			"vertex: pack() packs each field by its type");

	// a mesh of the format: its bytes, their count and bounds; no Vertex3d to read
	auto a = v;
	auto b = v;
	b.pos = Vec3(-4.0f, 5.0f, -6.0f);
	auto mesh = Mesh3d::make<Packed3dTestVertex>(Vector<Packed3dTestVertex>{a, b, a},
			Vector<uint32_t>{0, 1, 2});
	check(mesh && mesh->getFormat() == &format && mesh->getVertexCount() == 3
					&& mesh->getVertexData().size() == 96 && mesh->getVertexes().empty(),
			"vertex: a mesh of the format holds its packed vertexes");
	if (!mesh) {
		return;
	}
	check(mesh->getMin() == Vec3(-4.0f, -2.0f, -6.0f) && mesh->getMax() == Vec3(1.0f, 5.0f, 3.0f),
			"vertex: its bounds by the format's position");
	check(!Rc<Mesh3d>::create(&format, Bytes(40), 2, Vector<uint32_t>{0, 1}),
			"vertex: bytes that are not the vertexes' are refused");

	// a pack of both formats: each mesh's vertexes from a 16-byte boundary, as they are
	auto box = Mesh3d::makeBox(Vec3(1.0f, 1.0f, 1.0f));
	auto layout = layoutPack(Vector<Rc<Mesh3d>>{box, mesh, box});
	auto &r = layout.ranges;
	check(r.size() == 3 && r[0].vertexByteOffset == 0 && r[1].vertexByteOffset == 24 * 64
					&& r[2].vertexByteOffset == 24 * 64 + 96 && r[1].vertexCount == 3
					&& r[1].firstIndex == 36 && r[2].firstIndex == 39
					&& layout.vertexes.size() == 48 * 64 + 96,
			"vertex: a pack lays meshes of both formats one after another",
			toString(r.size() == 3 ? r[2].vertexByteOffset : 0));
	check(BytesView(layout.vertexes).sub(r[1].vertexByteOffset, 96) == mesh->getVertexData(),
			"vertex: the packed vertexes as they are");
}

} // namespace stappler::xenolith::basic3d
