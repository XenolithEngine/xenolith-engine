/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#ifndef XENOLITH_RENDERER_BASIC3D_XL3DVERTEX_H_
#define XENOLITH_RENDERER_BASIC3D_XL3DVERTEX_H_

#include "XLCommon.h"

// What a vertex of a schema is to a mesh and the pass: its stride, its alignment and where its
// position is. The schema's header (xlkdl gpu, docs/usage/tools/xlkdl-gpu.adoc) describes each
// vertex with a VertexFormat3d and packs it with a builder of its own, by the functions below; this
// header includes nothing of basic3d, so a schema's header can include it.
namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d {

// A vertex's record: its std430 stride and alignment, and the offset of its position, a vec3 or a
// vec4 of floats. Generated as a constant, so a pipeline and a mesh compare their formats by
// address.
struct VertexFormat3d {
	const char *name = nullptr;
	uint32_t stride = 0;
	uint32_t align = 0;
	uint32_t positionOffset = 0;
};

// Vertex3d, the vertex of the mesh builders and of the mesh pipeline
SP_PUBLIC extern const VertexFormat3d Vertex3dFormat;

// The packed fields of a vertex, as GLSL unpacks them: oct16 a unit vector, octahedral, in two
// snorm16 (the preamble's xl3dUnpackOct16); half2 two halves (unpackHalf2x16); unorm8x4 four
// bytes from 0 to 1, x in the lowest (unpackUnorm4x8)
SP_PUBLIC uint32_t packOct16(const Vec3 &);
SP_PUBLIC Vec3 unpackOct16(uint32_t);
SP_PUBLIC uint32_t packHalf2(const Vec2 &);
SP_PUBLIC Vec2 unpackHalf2(uint32_t);
SP_PUBLIC uint32_t packUnorm8x4(const Vec4 &);
SP_PUBLIC Vec4 unpackUnorm8x4(uint32_t);

} // namespace stappler::xenolith::basic3d

#endif /* XENOLITH_RENDERER_BASIC3D_XL3DVERTEX_H_ */
