/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#ifndef XENOLITH_RENDERER_BASIC3D_GLSL_INCLUDE_XL3DGLSLDATA_H_
#define XENOLITH_RENDERER_BASIC3D_GLSL_INCLUDE_XL3DGLSLDATA_H_

#include "sprt_glsl.h"

#ifndef SP_GLSL
namespace STAPPLER_VERSIONIZED stappler::glsl {

using namespace sprt::glsl;

#endif

// DrawData3d::textureIdx of a draw with no texture: its colour alone
#define XL_GLSL_3D_NO_TEXTURE 0xFFFFFFFFu

// One vertex, 64 bytes, read by gl_VertexIndex through a buffer reference
struct Vertex3d {
	vec4 pos; // xyz - position in the model, w - 1
	vec4 normal; // xyz - unit normal, w - free
	vec4 tangent; // xyz - unit tangent, w - the bitangent's sign
	vec4 attr; // xy - texture coordinates, zw - free
};

// One draw: where the model is and what it looks like
struct DrawData3d {
	mat4 model;
	mat4 normal; // the inverse transpose of the model's rotation and scale, for normals
	vec4 color;
	uint textureIdx; // into the pass's texture array, or XL_GLSL_3D_NO_TEXTURE
	float uvScale;
	uint flags; // the material's, for an app pipeline
	uint padding;
	vec4 params[2]; // the material's, for an app pipeline
};

// One view: its camera and its light
struct ViewData3d {
	mat4 viewProj; // depth in [0, 1], clip y flipped for Vulkan
	vec4 sunDirection; // xyz - toward the sun, unit length
	vec4 sunColor; // rgb - colour times intensity
	vec4 ambient; // rgb - colour times intensity
	vec4 screen; // xy - the viewport's size in pixels, z - pixels per point, w - free
};

struct Constants3d {
	uvec2 viewPointer; // 0-8, this view's ViewData3d
	uvec2 drawPointer; // 8-16, the frame's DrawData3d array
	uvec2 vertexPointer; // 16-24, the frame's Vertex3d array
	uint drawIndex; // 24-28
	uint padding; // 28-32
};

#ifndef SP_GLSL
} // namespace stappler::glsl
#endif

#endif /* XENOLITH_RENDERER_BASIC3D_GLSL_INCLUDE_XL3DGLSLDATA_H_ */
