/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#ifndef XENOLITH_RENDERER_BASIC3D_XL3DMESH_H_
#define XENOLITH_RENDERER_BASIC3D_XL3DMESH_H_

#include "XL3d.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d {

/* Triangles over vertexes, immutable once made, shared between frames by Rc. Faces wind
counter-clockwise seen from outside, which is what the pass culls by. */
class SP_PUBLIC Mesh3d : public Ref {
public:
	// a box centred on the origin
	static Rc<Mesh3d> makeBox(const Vec3 &halfExtent);

	// a square in the xy plane centred on the origin, facing +z; texture coordinates run from 0 to
	// `uvRepeat` across it
	static Rc<Mesh3d> makePlane(float size, float uvRepeat = 1.0f);

	// a torus around the z axis: `major` is the ring's radius, `minor` the tube's
	static Rc<Mesh3d> makeTorus(float major, float minor, uint32_t segments, uint32_t sides);

	virtual ~Mesh3d() = default;

	bool init(Vector<Vertex3d> &&, Vector<uint32_t> &&);

	SpanView<Vertex3d> getVertexes() const { return _vertexes; }
	SpanView<uint32_t> getIndexes() const { return _indexes; }

	const Vec3 &getMin() const { return _min; }
	const Vec3 &getMax() const { return _max; }

protected:
	Vector<Vertex3d> _vertexes;
	Vector<uint32_t> _indexes;
	Vec3 _min;
	Vec3 _max;
};

} // namespace stappler::xenolith::basic3d

#endif /* XENOLITH_RENDERER_BASIC3D_XL3DMESH_H_ */
