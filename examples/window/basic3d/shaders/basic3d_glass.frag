#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require
#extension GL_EXT_nonuniform_qualifier : enable

#ifndef SP_GLSL
#define SP_GLSL
#endif

#include "sprt_glsl.h"
#include "XL3dGlslData.h"

// A translucent pipeline: a tinted glass, as clear as the depth behind it is near. The opaque draws'
// depth comes as an input attachment; the camera is orthographic, so its difference times the
// depth range (params[0].x) is how far the view goes through the glass. Its colour is params[1],
// its alpha 1 - exp(-params[0].y * that distance).

layout (set = 0, binding = 0) uniform sampler2D textures[64];
layout (set = 0, binding = 1) uniform sampler2DArray textureArrays[16];
layout (input_attachment_index = 0, set = 0, binding = 2) uniform subpassInput sceneDepth;

layout(std430, buffer_reference, buffer_reference_align = 16) readonly buffer DrawBuffer {
	DrawData3d draws[];
};

layout (std430, push_constant) uniform pcb {
	Constants3d constants;
};

layout (location = 0) in vec3 fragNormal;
layout (location = 1) in vec3 fragWorld;
layout (location = 2) in flat uint fragDraw;

layout (location = 0) out vec4 outColor;

void main() {
	DrawData3d d = DrawBuffer(constants.drawPointer).draws[fragDraw];

	float through = max(subpassLoad(sceneDepth).r - gl_FragCoord.z, 0.0) * d.params[0].x;
	outColor = vec4(d.params[1].rgb, 1.0 - exp(-d.params[0].y * through));
}
