/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#include "XLCommon.h"
#include "XL3dMesh.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d {

using test::check;
using test::near;

static Vec3 xyz(const Vec4 &v) { return Vec3(v.x, v.y, v.z); }

// Unit normals; every triangle facing the way its vertexes' normals do (counter-clockwise seen
// from outside); every vertex inside the bounds
static void checkMesh(const Mesh3d &mesh, StringView name) {
	auto vertexes = mesh.getVertexes();
	auto indexes = mesh.getIndexes();

	bool unit = true;
	bool inside = true;
	for (auto &v : vertexes) {
		unit = unit && near(xyz(v.normal).length(), 1.0f, 1.0e-4f);
		inside = inside && v.pos.x >= mesh.getMin().x && v.pos.x <= mesh.getMax().x
				&& v.pos.y >= mesh.getMin().y && v.pos.y <= mesh.getMax().y
				&& v.pos.z >= mesh.getMin().z && v.pos.z <= mesh.getMax().z;
	}
	check(unit, toString("mesh: ", name, " normals are unit length"));
	check(inside, toString("mesh: ", name, " bounds hold every vertex"));

	bool outward = indexes.size() % 3 == 0;
	for (size_t i = 0; i + 2 < indexes.size() && outward; i += 3) {
		auto &a = vertexes[indexes[i]];
		auto &b = vertexes[indexes[i + 1]];
		auto &c = vertexes[indexes[i + 2]];
		Vec3 face;
		Vec3::cross(xyz(b.pos) - xyz(a.pos), xyz(c.pos) - xyz(a.pos), &face);
		auto n = xyz(a.normal) + xyz(b.normal) + xyz(c.normal);
		outward = Vec3::dot(face, n) > 0.0f;
	}
	check(outward, toString("mesh: ", name, " faces wind counter-clockwise from outside"));
}

void performMeshTests() {
	auto torus = Mesh3d::makeTorus(2.0f, 0.5f, 24, 12);
	check(torus->getVertexes().size() == 25 * 13 && torus->getIndexes().size() == 24 * 12 * 6,
			"mesh: torus counts", toString(torus->getVertexes().size(), " ",
					torus->getIndexes().size()));
	check(near(torus->getMax().x, 2.5f, 1.0e-4f) && near(torus->getMax().z, 0.5f, 1.0e-4f)
					&& near(torus->getMin().z, -0.5f, 1.0e-4f),
			"mesh: torus bounds");
	checkMesh(*torus, "torus");

	auto box = Mesh3d::makeBox(Vec3(1.0f, 2.0f, 3.0f));
	check(box->getVertexes().size() == 24 && box->getIndexes().size() == 36, "mesh: box counts");
	check(near(box->getMax().y, 2.0f, 1.0e-5f) && near(box->getMin().z, -3.0f, 1.0e-5f),
			"mesh: box bounds");
	checkMesh(*box, "box");

	auto plane = Mesh3d::makePlane(10.0f, 4.0f);
	check(plane->getVertexes().size() == 4 && plane->getIndexes().size() == 6
					&& near(plane->getVertexes()[2].attr.x, 4.0f, 1.0e-5f)
					&& near(plane->getVertexes()[0].normal.z, 1.0f, 1.0e-5f),
			"mesh: plane counts, texture repeat and normal");
	checkMesh(*plane, "plane");

	// the records the shaders read by buffer reference, std430: an app pipeline's shaders read
	// the material's flags and params, and the view's screen, at these offsets
	check(sizeof(DrawData3d) == 192 && offsetof(DrawData3d, flags) == 152
					&& offsetof(DrawData3d, params) == 160,
			"mesh: a draw's record is 192 bytes, its params at 160", toString(sizeof(DrawData3d)));
	check(sizeof(ViewData3d) == 128 && offsetof(ViewData3d, screen) == 112,
			"mesh: a view's record is 128 bytes, its screen at 112", toString(sizeof(ViewData3d)));

	Material3d material;
	check(material.pipeline == MeshPipeline && material.flags == 0
					&& material.params[0] == Vec4::ZERO && material.params[1] == Vec4::ZERO,
			"mesh: a material draws with the mesh pipeline, no flags and no params by default");
}

} // namespace stappler::xenolith::basic3d
