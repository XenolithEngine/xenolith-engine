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

// basic3d::MaxTextures; every slot is written, the unused ones with an empty image
layout (set = 0, binding = 0) uniform sampler2D textures[64];

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
layout (location = 1) in vec2 fragTexCoord;
layout (location = 2) in flat uint fragDraw;

layout (location = 0) out vec4 outColor;

void main() {
	DrawData3d d = DrawBuffer(constants.drawPointer).draws[fragDraw];
	ViewData3d view = ViewBuffer(constants.viewPointer).view;

	vec4 base = d.color;
	if (d.textureIdx != XL_GLSL_3D_NO_TEXTURE) {
		base *= texture(textures[nonuniformEXT(d.textureIdx)], fragTexCoord);
	}

	vec3 n = normalize(fragNormal);
	float lambert = max(dot(n, view.sunDirection.xyz), 0.0);
	vec3 light = view.ambient.rgb + view.sunColor.rgb * lambert;

	outColor = vec4(base.rgb * light, base.a);
}
