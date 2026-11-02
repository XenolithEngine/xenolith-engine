/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#ifndef XENOLITH_RENDERER_BASIC3D_XL3D_H_
#define XENOLITH_RENDERER_BASIC3D_XL3D_H_

#include "XL2d.h"
#include "XLTexture.h"
#include "glsl/include/XL3dGlslData.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d {

using Vertex3d = glsl::Vertex3d;
using DrawData3d = glsl::DrawData3d;
using ViewData3d = glsl::ViewData3d;
using Constants3d = glsl::Constants3d;
using LightData3d = glsl::LightData3d;

// Images one frame of the 3d pass can sample; the pass's descriptor array has this many slots
static constexpr uint32_t MaxTextures = 64;

// Layered images (a texture with more than one array layer) one frame can sample: they have an
// array of slots of their own, sampler2DArray at set 0, binding 1
static constexpr uint32_t MaxTextureArrays = 16;
static constexpr uint32_t NoTexture = XL_GLSL_3D_NO_TEXTURE;

// Point lights one view hands a frame; the rest are dropped, with a warning
static constexpr uint32_t MaxLights = 64;

class Camera;
class Mesh3d;
class MeshPack3d;
class View3d;
class FrameContext3d;
class SceneContent3d;
class Scene3d;

// The stock pipeline of a material: the mesh shader, texture times Lambert
static constexpr uint32_t MeshPipeline = 0;

/* A pipeline an app adds to the 3d pass (Scene3d::init): its own vertex and fragment shaders over
the pass's interface - the push constants Constants3d, the buffers they point to, the texture
array at set 0, binding 0 and the layered one at binding 1 (see XL3dGlslData.h). Depth test Less with writes, back faces culled,
counter-clockwise front faces, as the mesh pipeline. A material picks it by its place in the list
plus one. The SPIR-V is referenced, not copied: it lives as long as the app. */
struct SP_PUBLIC Pipeline3dInfo {
	String name;
	SpanView<uint32_t> vert;
	SpanView<uint32_t> frag;
};

// What a draw looks like: a colour, optionally times a texture tiled `uvScale` times per unit of
// the mesh's texture coordinates. An app pipeline reads `texture2`, `texture3`, `flags` and
// `params` as it likes; the mesh pipeline ignores them.
// A texture with array layers takes a slot of the layered array instead (MaxTextureArrays), and
// its index in DrawData3d is into that array: an app pipeline samples it as a sampler2DArray. The
// mesh pipeline has no use for one and draws such a material as its colour, with a warning.
struct SP_PUBLIC Material3d {
	Color4F color = Color4F::WHITE;
	Rc<Texture> texture;
	float uvScale = 1.0f;
	uint32_t pipeline = MeshPipeline; // MeshPipeline, or the index of an app pipeline plus one
	uint32_t flags = 0;
	Vec4 params[4] = {Vec4::ZERO, Vec4::ZERO, Vec4::ZERO, Vec4::ZERO};
	Rc<Texture> texture2;
	Rc<Texture> texture3;
};

// A point light in the world: its colour times its intensity falls off as (1 - (d / radius)^2)^2
struct SP_PUBLIC Light3d {
	Vec3 position;
	float radius = 1.0f;
	Color4F color = Color4F::WHITE;
};

// Where a draw's mesh is in GPU memory, when its pack is loaded: taken by the view on the app
// thread for the frame, so the pass reads no pack while an app moves meshes between them
struct SP_PUBLIC DrawGpu3d {
	Rc<core::BufferObject> vertexes;
	Rc<core::BufferObject> indexes;
	uint32_t firstIndex = 0;
	int32_t vertexOffset = 0;
};

// A mesh where `transform` puts it, looking like `material`
struct SP_PUBLIC Draw3d {
	Rc<Mesh3d> mesh;
	Mat4 transform = Mat4::IDENTITY;
	Material3d material;
	DrawGpu3d gpu; // filled by View3d::draw
};

// What one View3d hands the pass for a frame
struct SP_PUBLIC ViewInput3d {
	Rect viewport; // in the scene's pixels, y up, as Node::getWorldBoundingBox gives it
	ViewData3d data;
	Vector<Draw3d> draws;
	Vector<Light3d> lights; // up to MaxLights
};

} // namespace stappler::xenolith::basic3d

#endif /* XENOLITH_RENDERER_BASIC3D_XL3D_H_ */
