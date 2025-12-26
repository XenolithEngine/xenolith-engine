#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_samplerless_texture_functions : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "SPGlslInit.h"
#include "XL2dGlslVertexData.h"

layout (constant_id = 0) const int SAMPLERS_ARRAY_SIZE = 2;
layout (constant_id = 1) const int IMAGES_ARRAY_SIZE = 128;
layout (constant_id = 2) const int IMAGE_TYPE = 0;

layout (set = 0, binding = 0) uniform sampler immutableSamplers[SAMPLERS_ARRAY_SIZE];
layout (set = 0, binding = 1) uniform texture2D images2d[IMAGES_ARRAY_SIZE];
layout (set = 0, binding = 1) uniform texture2DArray images2dArray[IMAGES_ARRAY_SIZE];
layout (set = 0, binding = 1) uniform texture3D images3d[IMAGES_ARRAY_SIZE];

#include "XL2dGlslGradient.h"

layout (location = 0) in vec4 fragColor;
layout (location = 1) in vec4 fragTexCoord;
layout (location = 2) in vec4 shadowColor;
layout (location = 3) in vec4 outlineColor;
layout (location = 4) in float outlineOffset;
layout (location = 5) in vec2 fragPosition;
layout (location = 6) in flat uvec4 tex;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec4 outShadow;

layout (std430, push_constant) uniform pcb {
	VertexConstantData pushConstants;
};

vec4 getSample(in sampler2D s, vec2 coord) {
	return texture(s, coord);
}

#define SAMPLER2D(image, sampler) sampler2D( images2d[image], immutableSamplers[sampler] )
#define SAMPLER2DARR(image, sampler) sampler2DArray( images2dArray[image], immutableSamplers[sampler] )
#define SAMPLER3D(image, sampler) sampler3D( images3d[image], immutableSamplers[sampler] )

#define SAMPLE_PC 0

#if SAMPLE_PC == 0
#define SAMPLER2D_PC SAMPLER2D(pushConstants.imageIdx, pushConstants.samplerIdx)
#define SAMPLER2DARR_PC SAMPLER2DARR(pushConstants.imageIdx, pushConstants.samplerIdx)
#define SAMPLER3D_PC SAMPLER3D(pushConstants.imageIdx, pushConstants.samplerIdx)
#endif

#if SAMPLE_PC == 1
#define SAMPLER2D_PC SAMPLER2D(tex.x, tex.y)
#define SAMPLER2DARR_PC SAMPLER2DARR(tex.x, tex.y)
#define SAMPLER3D_PC SAMPLER3D(tex.x, tex.y)
#endif

#if SAMPLE_PC == 2
#define SAMPLER2D_PC SAMPLER2D(texImage, texSampler)
#define SAMPLER2DARR_PC SAMPLER2DARR(texImage, texSampler)
#define SAMPLER3D_PC SAMPLER3D(texImage, texSampler)
#endif

#if SAMPLE_PC == 3
#define SAMPLER2D_PC SAMPLER2D(materials[pushConstants.materialIdx].samplerImageIdx & 0xFFFF, 0)
#define SAMPLER2DARR_PC SAMPLER2DARR(materials[pushConstants.materialIdx].samplerImageIdx & 0xFFFF, 0)
#define SAMPLER3D_PC SAMPLER3D(materials[pushConstants.materialIdx].samplerImageIdx & 0xFFFF, 0)
#endif

vec2 getTextureSize_pc() {
	vec2 size;
	if (IMAGE_TYPE == 1) {
		size = textureSize(images2dArray[tex.x], 0).xy;
	} else if (IMAGE_TYPE == 2) {
		size = textureSize(images3d[tex.x], 0).xy;
	} else {
		size = textureSize(images2d[tex.x], 0);
	}
	return size;
}

float getOutlineSample(in vec2 coord, float initColor, float z) {
	const uint nsamples = min(4, max(uint(ceil(outlineOffset)), 2));

	const vec2 offset = outlineOffset / getTextureSize_pc();
	const vec2 step = offset / float(nsamples - 1);
	const vec2 origin = coord - offset / 2.0;

	float accum = initColor;

	for (uint i = 0; i < nsamples; ++ i) {
		for (uint j = 0; j < nsamples; ++ j) {
			if (IMAGE_TYPE == 1) {
				accum += texture(SAMPLER2DARR_PC, vec3(origin + vec2(step.x * i, step.y * j), z)).a;
			} else if (IMAGE_TYPE == 2) {
				accum += texture(SAMPLER3D_PC, vec3(origin + vec2(step.x * i, step.y * j), z)).a;
			} else {
				accum += texture(SAMPLER2D_PC, origin + vec2(step.x * i, step.y * j)).a;
			}
		}
	}

	const float result = (0.5 - abs(0.5 - accum / (nsamples * nsamples + 1))) * 2.0 - 1.0;

	return -(result * result * result * result - 1);
}

void main() {
	vec4 textureColor;
	if (IMAGE_TYPE == 1) {
		textureColor = texture(SAMPLER2DARR_PC, fragTexCoord.xyz);
	} else if (IMAGE_TYPE == 2) {
		textureColor = texture(SAMPLER3D_PC, fragTexCoord.xyz);
	} else {
		textureColor = texture(SAMPLER2D_PC, fragTexCoord.xy);
	}

	if (outlineOffset > 0.0) {
		float outlineSample = getOutlineSample(fragTexCoord.xy, textureColor.a, fragTexCoord.z);

		outColor = outlineColor * outlineSample + textureColor * (1.0 - outlineSample);
	} else {
		outColor = fragColor * textureColor;
	}

	/*if (pushConstants.gradientCount > 0) {
		outColor = applyGradient(outColor, pushConstants.gradientOffset, pushConstants.gradientCount);
	}*/

	outShadow = shadowColor;
}
