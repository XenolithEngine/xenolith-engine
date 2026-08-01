#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_samplerless_texture_functions : enable
#extension GL_EXT_nonuniform_qualifier : enable

// Lightweight counterpart of xl_2d_material.frag for the flat render queue:
// a single color output (the flat subpass has no Shadow attachment) and no outline sampling.

// clang-format off

#include "sprt_glsl.h"
#include "XL2dGlslVertexData.h"

layout (constant_id = 0) const int SAMPLERS_ARRAY_SIZE = 2;
layout (constant_id = 1) const int IMAGES_ARRAY_SIZE = 128;
layout (constant_id = 2) const int IMAGE_TYPE = 0;

layout (set = 0, binding = 0) uniform sampler immutableSamplers[SAMPLERS_ARRAY_SIZE];
layout (set = 0, binding = 1) uniform texture2D images2d[IMAGES_ARRAY_SIZE];
layout (set = 0, binding = 1) uniform texture2DArray images2dArray[IMAGES_ARRAY_SIZE];
layout (set = 0, binding = 1) uniform texture3D images3d[IMAGES_ARRAY_SIZE];

layout (location = 0) in vec4 fragColor;
layout (location = 1) in vec4 fragTexCoord;

layout (location = 0) out vec4 outColor;

layout (std430, push_constant) uniform pcb {
	VertexConstantData pushConstants;
};

// IMAGES_ARRAY_SIZE is specialized to 1 when the device lacks
// shaderSampledImageArrayDynamicIndexing (e.g. V3DV). Dynamic indices then
// become nir_tex_src_texture_offset and assert in v3d_tex.c — force [0].
#define TEX_IDX(image) ((IMAGES_ARRAY_SIZE == 1) ? 0 : (image))
#define SMP_IDX(sampler) ((IMAGES_ARRAY_SIZE == 1) ? 0 : (sampler))
#define SAMPLER2D(image, sampler) sampler2D( images2d[TEX_IDX(image)], immutableSamplers[SMP_IDX(sampler)] )
#define SAMPLER2DARR(image, sampler) sampler2DArray( images2dArray[TEX_IDX(image)], immutableSamplers[SMP_IDX(sampler)] )
#define SAMPLER3D(image, sampler) sampler3D( images3d[TEX_IDX(image)], immutableSamplers[SMP_IDX(sampler)] )

#define SAMPLER2D_PC SAMPLER2D(pushConstants.imageIdx, pushConstants.samplerIdx)
#define SAMPLER2DARR_PC SAMPLER2DARR(pushConstants.imageIdx, pushConstants.samplerIdx)
#define SAMPLER3D_PC SAMPLER3D(pushConstants.imageIdx, pushConstants.samplerIdx)

void main() {
	vec4 textureColor;
	if (IMAGE_TYPE == 1) {
		textureColor = texture(SAMPLER2DARR_PC, fragTexCoord.xyz);
	} else if (IMAGE_TYPE == 2) {
		textureColor = texture(SAMPLER3D_PC, fragTexCoord.xyz);
	} else {
		textureColor = texture(SAMPLER2D_PC, fragTexCoord.xy);
	}

	outColor = fragColor * textureColor;
}
