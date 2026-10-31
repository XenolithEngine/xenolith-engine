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

// Stripes across the world's x, half a unit wide, in the material's two params colours; with
// flag 1 the first colour alone

layout(std430, buffer_reference, buffer_reference_align = 16) readonly buffer DrawBuffer {
	DrawData3d draws[];
};

layout(std430, buffer_reference, buffer_reference_align = 16) readonly buffer ViewBuffer {
	ViewData3d view;
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
	ViewData3d view = ViewBuffer(constants.viewPointer).view;

	bool second = (d.flags & 1u) == 0u && fract(fragWorld.x) >= 0.5;
	vec4 base = second ? d.params[1] : d.params[0];

	vec3 n = normalize(fragNormal);
	float lambert = max(dot(n, view.sunDirection.xyz), 0.0);
	vec3 light = view.ambient.rgb + view.sunColor.rgb * lambert;

	outColor = vec4(base.rgb * light, base.a);
}
