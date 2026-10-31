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

// Images one frame of the 3d pass can sample; the pass's descriptor array has this many slots
static constexpr uint32_t MaxTextures = 64;
static constexpr uint32_t NoTexture = XL_GLSL_3D_NO_TEXTURE;

class Camera;
class Mesh3d;
class View3d;
class FrameContext3d;
class SceneContent3d;
class Scene3d;

// What a draw looks like: a colour, optionally times a texture tiled `uvScale` times per unit of
// the mesh's texture coordinates
struct SP_PUBLIC Material3d {
	Color4F color = Color4F::WHITE;
	Rc<Texture> texture;
	float uvScale = 1.0f;
};

// A mesh where `transform` puts it, looking like `material`
struct SP_PUBLIC Draw3d {
	Rc<Mesh3d> mesh;
	Mat4 transform = Mat4::IDENTITY;
	Material3d material;
};

// What one View3d hands the pass for a frame
struct SP_PUBLIC ViewInput3d {
	Rect viewport; // in the scene's pixels, y up, as Node::getWorldBoundingBox gives it
	ViewData3d data;
	Vector<Draw3d> draws;
};

} // namespace stappler::xenolith::basic3d

#endif /* XENOLITH_RENDERER_BASIC3D_XL3D_H_ */
