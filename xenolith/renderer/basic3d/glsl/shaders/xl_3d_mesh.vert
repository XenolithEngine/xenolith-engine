#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require

#ifndef SP_GLSL
#define SP_GLSL
#endif

#include "sprt_glsl.h"
#include "XL3dGlslData.h"

layout(std430, buffer_reference, buffer_reference_align = 16) readonly buffer VertexBuffer {
	Vertex3d vertices[];
};

layout(std430, buffer_reference, buffer_reference_align = 16) readonly buffer DrawBuffer {
	DrawData3d draws[];
};

layout(std430, buffer_reference, buffer_reference_align = 16) readonly buffer ViewBuffer {
	ViewData3d view;
};

layout (std430, push_constant) uniform pcb {
	Constants3d constants;
};

layout (location = 0) out vec3 fragNormal;
layout (location = 1) out vec2 fragTexCoord;
layout (location = 2) out flat uint fragDraw;

void main() {
	// gl_VertexIndex includes the draw's vertexOffset
	Vertex3d v = VertexBuffer(constants.vertexPointer).vertices[gl_VertexIndex];
	DrawData3d d = DrawBuffer(constants.drawPointer).draws[constants.drawIndex];

	fragNormal = (d.normal * vec4(v.normal.xyz, 0.0)).xyz;
	fragTexCoord = v.attr.xy * d.uvScale;
	fragDraw = constants.drawIndex;

	gl_Position = ViewBuffer(constants.viewPointer).view.viewProj * (d.model * vec4(v.pos.xyz, 1.0));
}
