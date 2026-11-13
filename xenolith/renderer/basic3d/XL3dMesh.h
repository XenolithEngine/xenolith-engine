/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#ifndef XENOLITH_RENDERER_BASIC3D_XL3DMESH_H_
#define XENOLITH_RENDERER_BASIC3D_XL3DMESH_H_

#include "XL3d.h"
#include "XL3dMeshPack.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d {

/* Triangles over vertexes, immutable once made, shared between frames by Rc. Faces wind
counter-clockwise seen from outside, which is what the pass culls by.

The vertexes are of a format: Vertex3d (Vertex3dFormat) for the builders below and the mesh
pipeline, or a vertex of a schema, made from its builders (make) and read by a pipeline of that
format. */
class SP_PUBLIC Mesh3d : public Ref {
public:
	// a box centred on the origin
	static Rc<Mesh3d> makeBox(const Vec3 &halfExtent);

	// a square in the xy plane centred on the origin, facing +z; texture coordinates run from 0 to
	// `uvRepeat` across it
	static Rc<Mesh3d> makePlane(float size, float uvRepeat = 1.0f);

	// a torus around the z axis: `major` is the ring's radius, `minor` the tube's
	static Rc<Mesh3d> makeTorus(float major, float minor, uint32_t segments, uint32_t sides);

	// a mesh of a vertex of a schema: its builders, each packed into the record of its format
	template <typename Vertex>
	static Rc<Mesh3d> make(SpanView<Vertex> vertexes, Vector<uint32_t> &&indexes) {
		auto &format = Vertex::Format;
		Bytes data(vertexes.size() * format.stride);
		for (size_t i = 0; i < vertexes.size(); ++i) {
			auto packed = vertexes[i].pack();
			static_assert(sizeof(packed) == Vertex::Format.stride);
			sprt::memcpy(data.data() + i * format.stride, &packed, sizeof(packed));
		}
		return Rc<Mesh3d>::create(&format, sp::move(data), uint32_t(vertexes.size()),
				sp::move(indexes));
	}

	virtual ~Mesh3d() = default;

	bool init(Vector<Vertex3d> &&, Vector<uint32_t> &&);

	// `count` vertexes of `format`, its stride apart
	bool init(const VertexFormat3d *format, Bytes &&, uint32_t count, Vector<uint32_t> &&);

	const VertexFormat3d *getFormat() const { return _format; }
	BytesView getVertexData() const { return _vertexes; }
	uint32_t getVertexCount() const { return _vertexCount; }

	// the vertexes of a mesh of Vertex3dFormat; none for another format
	SpanView<Vertex3d> getVertexes() const;
	SpanView<uint32_t> getIndexes() const { return _indexes; }

	const Vec3 &getMin() const { return _min; }
	const Vec3 &getMax() const { return _max; }

	// the pack the mesh is in, if any, and its range there (XL3dMeshPack.h); set by the pack.
	// App thread only
	MeshPack3d *getPack() const { return _pack; }
	const MeshPackRange &getPackRange() const { return _packRange; }
	void setPack(MeshPack3d *, const MeshPackRange &);

protected:
	const VertexFormat3d *_format = &Vertex3dFormat;
	Bytes _vertexes;
	uint32_t _vertexCount = 0;
	Vector<uint32_t> _indexes;
	Vec3 _min;
	Vec3 _max;
	Rc<MeshPack3d> _pack;
	MeshPackRange _packRange;
};

} // namespace stappler::xenolith::basic3d

#endif /* XENOLITH_RENDERER_BASIC3D_XL3DMESH_H_ */
