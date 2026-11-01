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

// Stripes across the world's x, half a unit wide, in the material's two params colours; with
// flag 1 the first colour alone. A second texture, when the material has one, takes the place of
// the second colour, laid over the world's xy; with flag 2 the material's texture does, a layered
// one, its layer params[2].x

layout (set = 0, binding = 0) uniform sampler2D textures[64];
layout (set = 0, binding = 1) uniform sampler2DArray textureArrays[16];

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
	if (second && d.textureIdx2 != XL_GLSL_3D_NO_TEXTURE) {
		base = texture(textures[nonuniformEXT(d.textureIdx2)], fragWorld.xy * 0.5);
	}
	if (second && (d.flags & 2u) != 0u && d.textureIdx != XL_GLSL_3D_NO_TEXTURE) {
		base = texture(textureArrays[nonuniformEXT(d.textureIdx)],
				vec3(fragWorld.xy * 0.5, d.params[2].x));
	}

	vec3 n = normalize(fragNormal);
	float lambert = max(dot(n, view.sunDirection.xyz), 0.0);
	vec3 light = view.ambient.rgb + view.sunColor.rgb * lambert;

	outColor = vec4(base.rgb * light, base.a);
}
