/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#include "XLCommon.h"
#include "XL3dMesh.h"
#include "XL3dMeshPack.h"

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
	// the material's flags, second texture and params, the view's screen and lights, at these
	// offsets
	check(sizeof(DrawData3d) == 240 && offsetof(DrawData3d, flags) == 152
					&& offsetof(DrawData3d, textureIdx2) == 156
					&& offsetof(DrawData3d, params) == 160
					&& offsetof(DrawData3d, textureIdx3) == 224,
			"mesh: a draw's record is 240 bytes, its second texture at 156, its params at 160, its "
			"third texture at 224",
			toString(sizeof(DrawData3d)));
	check(sizeof(ViewData3d) == 144 && offsetof(ViewData3d, screen) == 112
					&& offsetof(ViewData3d, firstLight) == 128
					&& offsetof(ViewData3d, lightCount) == 132,
			"mesh: a view's record is 144 bytes, its screen at 112, its lights at 128",
			toString(sizeof(ViewData3d)));
	check(sizeof(LightData3d) == 32 && offsetof(LightData3d, color) == 16,
			"mesh: a light's record is 32 bytes", toString(sizeof(LightData3d)));
	check(sizeof(Constants3d) == 40 && offsetof(Constants3d, lightPointer) == 24
					&& offsetof(Constants3d, drawIndex) == 32,
			"mesh: the push constants are 40 bytes, the lights' pointer at 24",
			toString(sizeof(Constants3d)));

	// a pack lays its meshes one after another, indexes unchanged
	auto packBox = Mesh3d::makeBox(Vec3(1.0f, 1.0f, 1.0f));
	auto packPlane = Mesh3d::makePlane(4.0f, 2.0f);
	auto layout = layoutPack(Vector<Rc<Mesh3d>>{packBox, packPlane, packBox});
	auto &r = layout.ranges;
	auto vertexAt = [&](size_t i) {
		return reinterpret_cast<const Vertex3d *>(layout.vertexes.data())[i];
	};
	auto indexAt = [&](size_t i) {
		return reinterpret_cast<const uint32_t *>(layout.indexes.data())[i];
	};
	check(r.size() == 3 && r[0].vertexOffset == 0 && r[0].firstIndex == 0
					&& r[1].vertexOffset == 24 && r[1].firstIndex == 36 && r[2].vertexOffset == 28
					&& r[2].firstIndex == 42 && r[1].vertexCount == 4 && r[1].indexCount == 6
					&& layout.vertexes.size() == 52 * sizeof(Vertex3d)
					&& layout.indexes.size() == 78 * sizeof(uint32_t),
			"mesh: a pack lays its meshes one after another");
	check(vertexAt(25).pos == packPlane->getVertexes()[1].pos
					&& indexAt(40) == packPlane->getIndexes()[4] && indexAt(50) == packBox->getIndexes()[8],
			"mesh: their data as it is, indexes left relative to the mesh's own vertexes");

	Material3d material;
	check(material.pipeline == MeshPipeline && material.flags == 0 && !material.texture2
					&& !material.texture3
					&& material.params[0] == Vec4::ZERO && material.params[1] == Vec4::ZERO
					&& material.params[2] == Vec4::ZERO && material.params[3] == Vec4::ZERO,
			"mesh: a material draws with the mesh pipeline, no flags, no second or third texture "
			"and no params by default");
}

} // namespace stappler::xenolith::basic3d
